# uInspESP32 併發與 thread safety 分析

> **這是靜態程式碼分析，不是實測結果。** 下面標 🔴 的問題是從程式碼推導出來的
> race condition，**尚未在硬體上重現**。但它們的失效模式都是「偶發、無法重現、
> 看起來像硬體問題」，所以在兩台新機投產前值得處理。
>
> 分析對象：`src/app/LegacyFirmware.cpp` @ `c1f0a7da`

---

## 1. 執行情境

| 情境 | 核心 | 內容 |
|---|---|---|
| **timer ISR** `onTimer()` | core 1 | `StepGo()` → `GateSensing()` → `Run_ACTS()`。`IRAM_ATTR` |
| **主迴圈** `firmwareLoop()` | core 1 | 序列埠收發、命令處理、佇列排空、頻率斜坡 |
| **AUX_task** ×5 | core 0 | 輔助命令（delay / io_ctrl / wait_for_enc）|

**ISR 和主迴圈在同一核心**，所以它們不是真正並行——ISR 會**搶佔**主迴圈。
這一點很重要：代表保護只需要關中斷，不需要跨核心 spinlock。

AUX_task 在另一核心，但它只碰 `AUX2CommInfoQ`，**而且有 semaphore 保護**
（`AUX2Comm_Lock`，`LegacyFirmware.cpp:1940/1971`）。那條路徑是對的。

**全檔案沒有任何 `portENTER_CRITICAL` / `noInterrupts()`**（只有 1995 行一個
被註解掉的）。

---

## 2. 🔴 核心問題：`RingBufIdxCounter::dataSize` 的讀改寫競爭

`include/util/RingBuf.hpp`：

```cpp
class RingBufIdxCounter {
  RB_Idx_Type headIdx;
  RB_Idx_Type tailIdx;
  RB_Idx_Type dataSize;      // ← 生產者和消費者「都會改」
  ...
  int consumeTail(){
    if(dataSize==0)return -1;
    ...
    dataSize--;              // ← 讀-改-寫，非 atomic
  }
  int pushHead(){
    if(dataSize==RBLen) return -1;
    ...
    dataSize++;              // ← 讀-改-寫，非 atomic
  }
};
```

**這破壞了 lock-free ring buffer 之所以安全的前提。** 標準的單生產者/單消費者
ring buffer 之所以不用鎖，正是因為**生產者只寫 head、消費者只寫 tail**，
長度是「推導」出來的。這份實作讓兩邊都寫同一個 `dataSize`。

沒有 `volatile`、沒有 atomic、沒有臨界區。

### 失效機制

`dataSize++` 在 Xtensa 上是 load → add → store 三個指令。中間被 ISR 打斷：

```
主迴圈 consumeTail():  load dataSize (=5)
                       ↓ ISR 進來
ISR    pushHead():     load dataSize (=5), store 6
                       ↓ ISR 返回
主迴圈 consumeTail():  store 4          ← ISR 的 +1 被吃掉了
```

`RB_Idx_Type` 是 `uint8_t` 也一樣——8 位元的 RMW 在 Xtensa 上仍是三個指令。

### 後果

計數會**單向漂移**，而且兩個方向都會咬人：

- **少算**（增量遺失）→ 佇列以為比實際空 → 覆寫還沒送出的資料 → **`bTrigInfo` 靜默遺失**
  → core 端配對失步 → `INSP_RESULT_MATCHES_NO_OBJECT` → 停機
- **多算**（減量遺失）→ 佇列以為比實際滿 → `getHead()` 回 NULL →
  **`INSP_CAM_TRIG_INFO_CANNOT_BE_SENT` → 無故停機**

典型症狀：**跑了幾小時後莫名其妙停機，查不出原因，重開就好。**

---

## 3. 哪些佇列跨越了 ISR / 主迴圈邊界

| 佇列 | 生產者 | 消費者 | 狀態 |
|---|---|---|---|
| **`TaskQ2CommInfoQ`** (20 深) | **ISR** (`Run_ACTS` :597/:642)<br>**+ 主迴圈** (:455 狀態轉移、:1259 `recv_ERROR`、:1379 `trigCamPulse`) | 主迴圈 (:2161) | 🔴 **最嚴重** |
| **`RBuf`** (100 深) | ISR (`newPulseEvent`) | 主迴圈（清理 `consumeTail`）<br>+ 主迴圈（`report` 改 `insp_status`）| 🔴 |
| `act_S.*` (7 條) | ISR (`ActRegister` / SWITCH 分支) | ISR (`Run_ACTS`) | 🟡 ISR 內自洽，但 `RESET_ALL_PIPELINE_QUEUE()` 從主迴圈清空 |
| `ERROR_HIST` | 主迴圈 | 主迴圈 | 🟢 安全 |
| `AUX2CommInfoQ` | AUX_task | 主迴圈 | 🟢 有 semaphore |

### `TaskQ2CommInfoQ` 特別糟

它**有兩個生產者**（ISR 和主迴圈）加一個消費者。連 SPSC 都不是，所以就算把
`dataSize` 改成推導式也不夠——`headIdx` 本身也會被兩個生產者競爭。

### `RBuf` 的額外問題

`report` 命令在主迴圈裡走訪 `RBuf` 並修改 `pipe->insp_status`，同時 ISR 的
`Run_ACTS` 在 SWITCH 分支讀同一個欄位。`int32_t` 對齊寫入在 Xtensa 上是
atomic，所以**不會撕裂**，但沒有 `volatile`，編譯器理論上可以把它快取在暫存器裡。
`-O3` 之下這不是純理論問題。

---

## 4. 其他共享狀態

| 變數 | 寫 | 讀 | 風險 |
|---|---|---|---|
| **`STAGE_PULSE_OFFSET`** | 主迴圈（`set_setup`、`MachineConfig::begin`）| ISR (`Run_ACTS`) | 🔴 **15 個欄位的 struct 逐一更新**，ISR 可能讀到半舊半新——一顆料件用舊的 `CAM1_on` 配新的 `SEL1_on` |
| `SEL1_ACT_COUNTDOWN` | ISR (`--`) + 主迴圈 (`set_sel1_cd`) | 兩邊 | 🔴 RMW 競爭 |
| `SEL1_Count` … `NA_Count` | ISR (`++`) | 主迴圈 (`get_running_stat`) | 🟡 **`uint64_t` 在 32 位元 CPU 上是兩次存取** → 統計數字可能撕裂（只影響顯示，不影響分選）|
| `blockNewDetectedObject` | 主迴圈 | ISR | 🟡 `bool`，非 `volatile` |
| `minWidth` / `maxWidth` | 主迴圈 (`set_setup`) | ISR (`GateSensing`) | 🟡 `int` 對齊，非 `volatile` |
| `SYS_STEP_COUNT` | ISR | 主迴圈 | 🟡 `uint32_t` 對齊 atomic，非 `volatile` |
| `PENDING_ISR_ERROR` | ISR | 主迴圈 | 🟢 已宣告 `volatile`（`535d92fb`）|

> 順帶一提：commit `535d92fb` 把錯誤狀態轉移從 ISR 移到主迴圈，**順便消掉了
> 一條競爭路徑**——`ERROR_HIST` 和 `SYS_STATE_LIFECYCLE` 現在是主迴圈獨佔。
> 當初的動機是 ISR 裡不能呼叫 Arduino GPIO，併發只是附帶收穫。

---

## 5. 建議的修法

按「風險 / 成本」排序。**這些都還沒做**，而且我不建議在沒有硬體的情況下盲改
ISR 熱路徑——併發修補改壞的後果比原本的 bug 更難查。

### 5.1 佇列索引加臨界區（最小、最安全）

因為 ISR 和主迴圈同核心，關中斷就夠：

```cpp
// RingBuf.hpp
#include "freertos/FreeRTOS.h"
static portMUX_TYPE rb_mux = portMUX_INITIALIZER_UNLOCKED;

int consumeTail(){
  portENTER_CRITICAL_SAFE(&rb_mux);      // ISR/非 ISR 通用
  ...
  portEXIT_CRITICAL_SAFE(&rb_mux);
}
```

成本：ISR 內多幾十個 cycle。`Run_ACTS` 每個 tick 最多動 7 條佇列，在
2 kHz tick 下可以接受，但**要實測 ISR 執行時間**。

> ⚠ 用單一全域 mux 會讓所有 RingBuf 互相序列化。若量到影響，改成每個實例
> 一個 mux。

### 5.2 `STAGE_PULSE_OFFSET` 雙緩衝

不要就地改。準備一份新的，用一個 `volatile` 指標一次切換：

```cpp
stagePulseOffset SPO_buf[2];
volatile stagePulseOffset* SPO_active = &SPO_buf[0];
// 更新：寫進非使用中的那份，最後一行才切指標
```

指標寫入是 atomic，ISR 要嘛整份看到舊的、要嘛整份看到新的。

### 5.3 `TaskQ2CommInfoQ` 拆成兩條

ISR 一條、主迴圈一條，主迴圈輪流排空。這樣就變成乾淨的 SPSC，
配合 5.1 之後完全安全，而且消掉「兩個生產者」這個結構性問題。

### 5.4 加 `volatile`

`blockNewDetectedObject`、`minWidth`/`maxWidth`、`SYS_STEP_COUNT`、
`pipeLineInfo::insp_status`。零成本，防編譯器把值快取在暫存器。

### 5.5 統計計數器降為 32 位元或加保護

`SEL1_Count` 等改成 `uint32_t`（4 億顆料件才會 wrap，夠用），或在讀取端接受
撕裂。目前只影響顯示。

---

## 6. 怎麼把它測出來

`tools/uinsp_test.py stress` 就是為此設計的：

```sh
python uinsp_test.py --port COM6 stress --max-hz 150 --dwell 5
python uinsp_test.py --port COM6 stress --max-hz 150 --no-report   # 只壓 announce 路徑
```

判讀：

- **在遠低於理論上限的速率出現 `INSP_CAM_TRIG_INFO_CANNOT_BE_SENT`**
  → 很可能就是 §2 的 `dataSize` 漂移（真的塞滿應該發生在特定可算的速率）
- **`bTrigInfo` 數量少於發出的假脈衝數，但沒有任何錯誤**
  → 增量遺失，資料被覆寫
- **同一速率重跑結果不一致** → 競爭的典型特徵

理論上限可以先算出來當對照：

| 限制 | 數值 |
|---|---|
| `TaskQ2CommInfoQ` 深度 | 20 |
| 每顆料件的 `bTrigInfo` 則數 | **2**（CAM1 + CAM2 各一）|
| 115200 8N1 有效頻寬 | ~11.5 kB/s |
| 一則 `bTrigInfo` 大小 | ~90 B → ~8 ms |
| **序列埠上限** | **~64 顆/秒**（每顆 2 則）|

> 🔴 **順帶發現一個純浪費**：`LegacyFirmware.cpp:2181` 每排空一則訊息就送一次
> `djrl.dbg_printf("sdksjldlskjd")` —— 殘留的除錯字串，會實際透過序列埠送出
> 一個 JSON 框。**在 115200 上等於把有效吞吐砍掉一大塊**，而且沒有任何用途。
> 這個可以直接刪，零風險。

---

## 7. 對兩台新機的實務建議

1. **先量再改。** 跑 `stress` 拿到基準數字，確認上面哪些是真的會發生的。
2. **刪掉 `dbg_printf("sdksjldlskjd")`**——零風險、直接回收頻寬。
3. **5.4（加 `volatile`）零風險，可以先做。**
4. **5.1 / 5.2 / 5.3 要配合硬體實測**，特別是 5.1 會影響 ISR 時序，而相機
   觸發脈寬只有 2 個 pulse（見 `INTEGRATION_MAP.md` §5.2）。
5. 如果產線目標速率遠低於 64 顆/秒（例如 15 顆/秒，也就是
   `SYS_MIN_PULSE_TIME_SEP_us` 的預設值），**這些 race 的觸發機率會低很多**，
   但不會是零——ISR 每秒仍然跳 2000 次，每次都有機會撞上主迴圈的 RMW。
