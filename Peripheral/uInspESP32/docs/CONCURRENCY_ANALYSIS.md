# uInspESP32 併發與 thread safety 分析

> **這是靜態程式碼分析。** race condition 靠測試不可靠——「沒測到」不等於
> 「不存在」，而這類問題的失效模式正是「偶發、無法重現、看起來像硬體問題」。
> 所以下面的修正是**靠推理做的，不是靠重現做的**。
>
> 分析對象：`src/app/LegacyFirmware.cpp` @ `c1f0a7da`
>
> **狀態**：§2 §3 已修（見 §5.0）。§5.2（`STAGE_PULSE_OFFSET` 撕裂）已修。
> §4 其餘小項仍待處理。

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

## 5. 修法

### 5.0 ✅ 已修（`RingBuf.hpp` + `LegacyFirmware.cpp`）

**(a) 佇列索引加臨界區。** `pushHead()` / `consumeTail()` / `pullHead()` /
`clear()` 整段包進 `portENTER_CRITICAL_SAFE`，每個 RingBuf 實例一個 mux
（不用全域，避免無關佇列互相序列化）。`dataSize` 同時加上 `volatile`。

**判斷與空/滿檢查一起包進去**，不能只保護計數寫入——「檢查是否為空」和
「推進索引」如果是兩個各自原子的步驟，另一邊還是能插進中間。

**(b) 拆掉兩個生產者。** ISR 的相機觸發改用專屬佇列 `ISRTrigQ`，主迴圈保留
`TaskQ2CommInfoQ`。這是**結構上消除競爭，不是用鎖蓋住它**。

理由很關鍵：`TaskQ2CommInfoQ` 的多生產者問題**光靠鎖住計數器救不了**。
生產是三個步驟——`getHead()` 拿槽、填資料、`pushHead()` 推進——ISR 和主迴圈
可能拿到**同一個槽**，各自寫入，結果一則訊息被覆蓋、下一個槽則從未被寫入就
被送出去。

而且 `TaskQ2CommInfo` 裡有三個 `std::string`，所以「把整個 struct 原子地複製
進佇列」這種修法會**把 malloc 帶進 ISR**——比原本的 bug 更糟。`ISRTrigInfo`
刻意只放 POD。

拆開之後：

| 佇列 | 生產者 | 消費者 | 狀態 |
|---|---|---|---|
| `ISRTrigQ` (32 深) | ISR | 主迴圈 | ✅ 真正的 SPSC + 臨界區 |
| `TaskQ2CommInfoQ` (20 深) | 主迴圈 | 主迴圈 | ✅ 完全沒有併發 |
| `RBuf` (100 深) | ISR | 主迴圈 | ✅ SPSC + 臨界區 |
| `act_S.*` | ISR | ISR | ✅ 加上 `clear()` 已保護 |

主迴圈排空時 **`ISRTrigQ` 優先**——相機觸發是 host 等著給判定的東西，
不該排在除錯訊息後面。

成本：`Run_ACTS` 每個 tick 約 10 個臨界區，每個數十 cycle，在 240MHz / 2kHz
tick 下約佔 0.2–0.4% CPU。

> ⚠ **仍需實測 ISR 執行時間**。相機觸發脈寬只有 2 個 pulse
> （`INTEGRATION_MAP.md` §5.2），上機時值得用示波器確認觸發波形沒有變胖或抖動。

建置影響：RAM +624 B（mux + `ISRTrigQ`），Flash +0.1%。

---

### 5.2 ✅ 已修：`STAGE_PULSE_OFFSET` 雙緩衝

`STAGE_PULSE_OFFSET` 保留為主迴圈的工作副本（`set_setup` 逐欄位改、
`MachineConfig` 整份讀寫、`get_setup` 回報都不變）。ISR **改成透過 `SPO_active`
指標讀取**，它永遠指向一份「不在寫入中」的快照。

```cpp
static stagePulseOffset SPO_snap[2];
static volatile stagePulseOffset* volatile SPO_active = &SPO_snap[0];

void STAGE_PULSE_OFFSET_publish() {          // 主迴圈呼叫
  stagePulseOffset* inactive = (SPO_active==&SPO_snap[0]) ? &SPO_snap[1] : &SPO_snap[0];
  *inactive = STAGE_PULSE_OFFSET;            // 中斷開啟下複製到私有緩衝
  __asm__ __volatile__("" ::: "memory");     // 複製先於指標切換
  SPO_active = inactive;                     // 對齊指標寫入 = 原子
}
```

**為什麼不用臨界區**：寫入端要更新 15 個欄位（JSON 逐欄位取值）。若包在
`portENTER_CRITICAL` 裡會遮罩 step timer ISR 整段時間，高 plateFreq 下可能掉步。
雙緩衝讓寫入端在中斷開啟下操作私有緩衝，只有指標切換是原子的——**完全不遮罩中斷**。

呼叫點：`firmwareSetup()` 的 `MachineConfig::begin()` 之後（timer arm 前）、
以及每次 `setMachineSetup()` 結尾。

**刻意不追求「單一物件全生命週期一致」**：`ACT_PUSH_TASK` 在註冊當下就把
`gate_pulse+offset` 算進 `targetPulse`，所以 CAM/L/SWITCH 的 offset 在註冊時固化，
SEL 的 offset 則在 SWITCH 分支較晚讀取。註冊與 SWITCH 之間若改了設定，該顆物件
會拿到新 SEL + 舊 CAM——這是「SEL 晚讀」的固有性質，跟撕裂無關，而且只在調機
當下發生，無害。

成本：兩份 60-byte 快照（RAM +120B），ISR 讀取多一次指標載入。

### 5.4 加 `volatile`（部分已做）

`blockNewDetectedObject` 已加。剩 `minWidth`/`maxWidth`、`SYS_STEP_COUNT`、
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

1. ✅ 佇列競爭已按 §5.0 修掉，**靠推理而不是靠重現**。
2. ✅ 刪掉 `dbg_printf("sdksjldlskjd")`，直接回收頻寬。
3. **上機時用示波器確認相機觸發波形**（脈寬只有 2 個 pulse），確認新增的臨界區
   沒有讓 ISR 變胖或抖動。這是 §5.0 唯一需要實測的副作用。
4. `stress` 現在是**回歸測試**而不是探測工具：如果還在遠低於 64 顆/秒的地方
   出現 `INSP_CAM_TRIG_INFO_CANNOT_BE_SENT`，代表還有沒抓到的東西。
5. §5.2（`STAGE_PULSE_OFFSET` 撕裂）仍在。它只在**調機當下改參數**時才會踩到
   ——正常運轉不會寫入——所以風險比佇列競爭低，但調機時看到一顆料件時序異常
   要想到它。
