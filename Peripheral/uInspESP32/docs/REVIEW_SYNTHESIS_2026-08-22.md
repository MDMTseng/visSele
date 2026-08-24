# ESP32 四面向 review 的整合計畫 — 2026-08-22

四份獨立 review(韌體架構 / 遙測面 / 協定面 / WebUI 消費端)的整合。
這份是**執行順序**,不是四份報告的堆疊。原始發現的 file:line 都保留。

---

## 0. 先修我今天弄壞的兩件事

兩件都是同一天加的 `reset_stat_maximum` 造成的,而且**兩件都是同一個錯誤**:
我列出了「所有 `_max_` 欄位」,卻沒有問**誰在讀它們**。

### 0.1 清掉了別的命令才回報的峰值

```
LegacyFirmware.cpp:6526  REP_ACQLAT_MAX_US = 0;   // 但它由 get_spikes:5637 回報
LegacyFirmware.cpp:6533  ACT_LATE_MAX      = 0;   // 但它由 poll:6030 回報
```

帶旗標呼叫 `get_running_stat` 的人**看不到這兩個值**(不在回覆裡),卻把它們摧毀了。
正在用 `get_spikes` / `poll` 觀察的另一個讀取者會靜默失去資料 —— 這正是 opt-in 設計
在 `:6073-6077` 說要防止的 cross-reader theft。

**修法**:兩者從清單移除;若真要能重置,就得在同一個回覆裡報出來。

### 0.2 拿走了 WebUI 告警依賴的峰值

UI 的 SWITCH 截止線告警(`uInspESP32_UI.jsx:1375-1383`):

```js
alarm when  SWITCH_ms * 1000  <  lat.max_us * 1.5
```

吃的是 `report_latency.max_us`,也就是我清單裡的 `REP_LAT_MAX_US`。UI **不帶旗標**
(它只顯示),所以 UI 是受害者不是加害者 —— bench 工具才是那個 reset 的人。soak
每分鐘 reset 一次,操作員若同時在看畫面,告警可能因為峰值剛被拿走而不亮。

**修法**:回到「兩個版本」慣例 —— 終身高水位(永不重置,給 UI)+ 視窗峰值
(可重置,給量測工具)。這正是 `_env_`/`_max_` 當初想做的事,只是這次兩邊語意都對。

### 0.3 順帶:清單本身不對稱

`:6537` 清了 `ISR_SEG_MAX_CY[]` 但沒清 `ISR_WORST_SEG_CY[]`,而 `:6304` 是**同一個
迴圈成對輸出的**。reset 之後兩個陣列描述不同的時代,而沒有任何東西說明這件事。

`health.rbuf_peak`、`stack_hwm`、`min_heap` 也都是高水位卻不受旗標影響 —— 那可能是
對的(它們是終身指標),但必須寫明,否則名字看起來像會被清。

---

## 1. 一個被推翻的前提:4096 不是限制

我在派工時把「主機的 `line.size() < 4096` 是天花板」當成已知事實傳下去。**是錯的**:

- `wiringPanel.cpp:8061` 讀的是 `CONSOLE_LINE_MAX`,而 `:7947` 設的是 **1900**。
- 而且那是 `PerifConsoleThread` 的**輸入**路徑(開發 TCP console 讀你打的字),
  **從來不經手裝置→主機的回覆**。
- `UINSP_CAVEATS.md:3300-3308` 早就記錄了:裝置→core 的真正上限是 core 的
  `dataBuff[20480]`,loopback 實測 20479 bytes 都完整往返。

**真正的成本是線路時間,不是截斷。** 230400 baud 下一個 byte 約 43.4 µs,現在
約 2.9–3.1 kB 的回覆佔用 TX 約 **130 ms**,而且期間持有 `perif_tx_lock`,排在判定
寫入前面 —— 直接吃進 CAM→SWITCH 預算。`:5978-5994` 為 `poll` 存在所做的論證正是這個。

**代價已經付過一次**:`get_schema` 為了一個不存在的限制而從 `get_setup` 拆出去。

所以精簡回覆該用**延遲**當理由,不是截斷。需要更正的過期註解:
`LegacyFirmware.cpp:4277-4287`、`:7904`、`DEV_COMPLETE_CHECKLIST.md:487-490`、
`UINSP_CAVEATS.md:2523-2532`。

---

## 2. 執行順序

分四層。層內可並行,層間有依賴或風險門檻。

### 第一層 — 現在就做,零風險或近零風險

| # | 事項 | 位置 | 風險 |
|---|---|---|---|
| 1.1 | `pin_mode` 缺的 `else`(見 §3.1) | `LegacyFirmware.cpp:5501-5510` | 極低,一行 |
| 1.2 | 修 §0.1 / §0.2 / §0.3 的 reset 清單 | `:6519-6540` | 極低,無 schema 變更 |
| 1.3 | 更正 §1 的過期註解 | 四處文件 + 兩處註解 | 零 |
| 1.4 | 更新 `FIRMWARE_CONTRACT.md` 的過期行數(見 §4) | 文件 | 零 |
| 1.5 | `uInspESP32_v2` 加 DEPRECATED 橫幅 | `README.md:1`、`SyncTask.cpp:1` | 零 |
| 1.6 | 11 個非 volatile 共享變數各加一行方向約定註解 | `FIRMWARE_CONTRACT.md:747` 列出 | 零 |

1.1 和 1.2 需要燒錄,所以要等 soak 空檔。1.3–1.6 完全不碰編譯產物。

### 第二層 — 協定一致性,修的是「靜默說謊」

這一層的共同點:**裝置回報成功,但沒做**。而且主機從不讀回覆,所以全部無人察覺。

| # | 事項 | 位置 |
|---|---|---|
| 2.1 | `pin_on`/`pin_off` 的 `doRsp=rspAck=true` 移進欄位守衛內 | `:7042`、`:7066` |
| 2.2 | `set_gate_disable` 同上(`on` 是 int 而非 bool 時靜默不套用) | `:7333` |
| 2.3 | `JSON_SETIF_ABLE` 的型別閘:改成型別轉換,或在 `set_setup` 回覆列出「實際套用的鍵」 | `:9124`,約 40 個呼叫點 |
| 2.4 | `get_version` 的 `id` 寫死成 100446 → 走共同 tail | `Data_Layer_Protocol.cpp:178` |
| 2.5 | `peerVERSION[20]` 的無界 `strcpy` | `:5166` |
| 2.6 | 統一回覆信封:每個分支都有 `type`(約 18 個沒有) | tail `:7897` |
| 2.7 | `set_setup` 的 ack 拆成 `ack`(RAM 已套用)+ `persisted` | `:5809` |

**2.3 是這一層的結構性核心。** key 在 schema 裡 → `unknown_keys` 放行 → `ack:true`,
但值因為 JSON 數字型別和 C++ 變數型別不合而沒套用。`{"plate":{"freq":12}}`
(整數餵給 float)就會中。韌體**已經知道這個陷阱**,在 `match_tolerance_mm`
(`:9410-9411`)手工繞過並寫了註解,其餘約 40 處都還在。

今天真的踩到一次同類:燒錄後 `min_detect_sep_us` 回到預設值,`ack` 一切正常,
吞吐掉 30%,畫面上什麼都沒有。

### 第三層 — 讓主機開始讀回覆

**沒有這一層,第二層全部不可執行。** `wiringPanel.cpp` 裡 `"ack"` / `"err"` /
`cfg_crc` / `serial_error_locked` / `get_version` 各出現 **0 次**;`:1190` 自己寫了
「the common replies (PONG, acks) never reach cJSON」。

最小可用版本:

1. 用 `id` 對應請求與回覆
2. `ack:false` 記錄成事件(接上事件記錄設計)
3. 看到 `err:"serial_error_locked"` 自動觸發既有的 RESYNC 路徑
   (`wiringPanel.cpp:6425-6434`,目前只有 WebUI 手動能觸發)
4. 連線時檢查 `cfg_crc`

**第 3 點是這一層的立即價值**:一個畸形封包會讓韌體латch 進 `serial_error_locked`,
之後對除 `RESET` 外的每個命令都回 `ack:false` —— 而核心的 ping thread 會**永遠**
往一個鎖死的裝置送 ping。核心唯一不聽的地方,正好是裝置正在大喊的地方。

**`cfg_crc` 有一個陷阱**(協定 review 的 R6):它指紋的是**活的**設定,不是儲存的。
`set_setup` 不帶 `persist` 也會改變 `cfg_crc`,所以釘住 CRC 的呼叫者會以為板子設定
好了,而重開機會還原。要嘛加一個「RAM 與 NVS 不同」的欄位,要嘛 CRC 分成兩個。

### 第四層 — 遙測整合(和 WebUI 一起)

**不要在 soak 期間做,而且要一次做完命名。** 兩波改名的成本高於一波。

| # | 事項 | 省下 | 破壞性 |
|---|---|---|---|
| 4.1 | 四個物件改成條件輸出:`jog`(只在 `JOG_STATE!=0`)、刪 `cam_pcnt`、`reset_reason*`/`xtal_mhz` 移到 `get_setup`、`boot0_us`/`bootd[]` 只在未 established 時 | ~320 B | `cam_pcnt` 零消費者 |
| 4.2 | 消重複:`GATE_EDGES` 報 3 次、`GATE_ACCEPT` 報 3 次、`UNANSWERED`/`SKIP`/`NA` 各 2 次;`pct` 四個由主機算 | ~250–300 B | bench 工具約 7 處 |
| 4.3 | `health` 拆出 `get_isr_profile`(所有 `isr_*`/`cam1_pw_*`/`act_*`) | ~900 B | 集中在 `pw_bringup.mjs` |
| 4.4 | `cam_sync` 拆出 `get_cam_sync`,留約 10 個現場欄位 | ~500 B | `camsync_*.mjs` 9 處 |
| 4.5 | 命名統一:一個後綴一種語意(`_max`/`_hwm`/`_env`/裸/`_now`) | — | 全面,約 2 天 + 重新 baseline |

輪詢路徑 **2.9 kB → 約 1.4 kB**,`perif_tx_lock` 每次少持有約 65 ms。

**4.5 之前必須先做的 UI 前置工程**:UI 現在有 **7 條獨立輪詢路徑**,其中兩條各自在打
`get_running_stat`(設定面板 1 s、側邊 MINI 1 s 但會互相退讓),還有一條每 tick 打
核心 `GS perif_pairing` **沒有 in-flight 保護**。任何 reset 語意都要求**恰好一個
擁有者**,所以「收斂成單一 poller」是前置條件,不是加分項。

---

## 3. 單獨列出的高風險缺陷

### 3.1 `pin_mode` 的安全防護是假的

```cpp
if(cfgPersistDeny()!=NULL) {          // 不在 IDLE → 應拒絕
  retdoc["err"]=cfgPersistDeny();
  doRsp=true; rspAck=false;           // 只是設旗標
}
else { doRsp=true; }
                                       // ← 沒有 return,沒有 else 包住後半
if(doc["pin"].is<int>()==true) {
  pinMode(pin,PIN_Mode);               // 照樣執行
  rspAck=true;                         // 而且把 ack 蓋回 true
}
```

它上面的註解正好描述了它現在會做的事:

> Raw pin access takes any GPIO number the caller sends, and **SEL1 is 25 while
> STEPPER_EN is 13**. In READY that means **an arbitrary actuator fired at an
> arbitrary plate position, or the driver de-energised at speed**.

有人寫了防護、寫了註解說明危險,**但防護沒有生效**。而且 WebUI 的 passthrough
(`wiringPanel.cpp:6461-6491`)原封轉發,所以從面板送得到。

一行 `else {` 的事。**第一層第一項。**

### 3.2 UI 重算了板子已經知道的東西,而且錯過一次

```js
PULSES_PER_REV = 70400;  plate diameter = 240mm    // uInspESP32_UI.jsx:126-130
```

板子把 `pulses_per_rev` 和 `plate_diameter_mm` 當成可設定鍵回報
(`PerifAPI.js:738`),UI 卻寫死自己那份。程式碼註解記著這組數字曾經錯 17.3%。

同類:速率 EMA(`:1673-1709`)假設計數器累計且單調,**計數變小會被當成「有人
reset」而丟掉濾波狀態**。第四層若把任何計數改成視窗式,它不會報錯,會印出
**看起來合理的錯數字**。

### 3.3 `cam_sync` 和 `yield` UI 完全沒讀

grep 整個 `UI/WebUI/src` 零命中。板子算的配對健康度和三段漏斗,操作員一個字都看
不到。今天靠 `yield` 漏斗一眼看出「gate 不是瓶頸、到達率才是」—— 那個判斷操作員
做不到。

也沒有任何地方顯示**良率百分比**(只有原始計數和吞吐率),而 `getSel1Countdown()`
(`PerifAPI.js:812`)**沒有任何呼叫者** —— 操作員無法知道還剩多少配額就會停機。

---

## 4. 架構:先更新數字,再決定

`FIRMWARE_CONTRACT.md` 有一份**量測過的**模組邊界分析,結論(「rig 邊界目前不搬」)
的推理是對的,但**輸入過期了**:

| | 文件(08-08) | 實際(08-22) |
|---|---|---|
| `LegacyFirmware.cpp` | 6499 行 | **9735 行** |
| dispatch | 1801 行 / 44 命令 | **2826 行 / 55 命令** |

**兩週長 50%。** 所以最高 CP 值的一項不是重構,是**重新量、重新決定**,否則後面
每個決定都建立在錯的數字上。

### 不該做的

- **不要拆 gate / pipeline / ACT。** 那是一台共用狀態的即時機器(60 個 ISR 觸及的
  全域)。**v2 正是試了這個分解然後失敗的。**
- **不要為可讀性動 `Run_ACTS`(`:2962-3396`)或 `GateSensing`(`:3665-3894`)。**
  兩者都在 IRAM、都有 ISR 預算,而相機觸發脈衝只有 2 個 pulse 寬。
- **不要「整理」註解。** 約 2000 行的註解是量測記錄,`:3956-3990` 和 `:4009-4055`
  的設計理由不存在於其他任何地方。刪掉它們是最貴的化妝品改動。
- **不要拿效能賣 dispatch 重構。** `report` 排第 47 個分支,每份判定走 47 次
  `strcmp` —— 但那只有 3–8 µs,對比 13 ms 的電子延遲差兩個數量級。理由應該是
  「每個 handler 可獨立閱讀與測試」和「745 行 rig 子集可實體切除」。

### v2 的處置

**v2 是死的,而且它的 README 在說謊:**

- `pio run` 建置失敗
- README 宣稱「Stage 9: Host-side testing framework ✅」—— 測試框架**從未編譯過**
  (`MockHAL.hpp` include 的路徑不存在)
- `main.cpp` 把 `setup()`/`loop()` 宣告為 `weak`,而 `SyncTask.cpp` 強定義同名符號 ——
  **整條乾淨的 HAL 路徑編譯得過但永遠不會被呼叫**
- `Pipeline.cpp` 10 行、`Scheduler.cpp` 16 行

危險的不是它不能用,是**它有第二份 protocol 層,grep 起來和活的那份一樣** ——
未來有人搜符號會在兩棵樹都命中,而分不出哪棵是活的。

保留價值只有 `src/hal/stm32/` 當作 STM32 移植草稿。其餘加 DEPRECATED 橫幅。

---

## 5. 時機

- **soak 進行中**:只能做 1.3–1.6(純文件/註解,不碰編譯產物)。
- **soak 空檔**:1.1 + 1.2 一起燒(兩者都小、都已定位)。
- **第二層**:可以在第三層之前做,但**沒有第三層就沒有人會發現它們壞了** ——
  所以第二層的驗收必須靠 bench 工具主動檢查回覆,不能靠機器自己。
- **第四層**:需要 UI 的 poller 收斂當前置,且**必須在兩次 soak 之間**做完,
  因為它會改掉 soak 正在記錄的欄位名。
