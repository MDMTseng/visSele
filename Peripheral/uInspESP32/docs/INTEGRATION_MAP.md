# uInspESP32 整合地圖

這份文件記錄「要讓 uInspESP32 在產線上跑起來，會碰到哪些檔案、哪些坑」。
範圍不只韌體 —— 從閘門感測器到氣閥的完整路徑橫跨四個子系統，任何一段
理解錯了都會在調機階段變成難查的間歇性問題。

新人請先讀「資料路徑」和「已知地雷」兩節。

> 建立於 2026-07（兩台新機專案）。標註 `[已驗證]` 的是本次實機或建置確認過的，
> 標註 `[未驗證]` 的是從程式碼推論、尚未實測。

---

## 1. 這些東西住在哪裡

| 子系統 | 路徑 | 角色 |
|---|---|---|
| **韌體** | `Peripheral/uInspESP32/` | 轉盤、閘門偵測、相機/光源/氣閥時序 |
| 韌體（重寫版，**停擺**） | `Peripheral/uInspESP32_v2/` | 見 §3 |
| 舊機韌體 | `Peripheral/uInspMEGA/` | Arduino MEGA + W5500，**新機要取代的對象** |
| **視覺核心** | `InspectionCore/Core0_1/wiringPanel.cpp` | 6100+ 行，周邊連線 + 結果回送 |
| 相機層 | `InspectionCore/CameraLayer/CameraLayer_HikRobot_Camera.cpp` | HikRobot MVS 封裝 |
| **WebUI** | `UI/WebUI/src/script.jsx` | 周邊 API class 都在這裡 |
| WebUI 連線引導 | `UI/WebUI/src/comm/BPG_WS.js` | 讀 machine_setting.json、發起周邊連線 |
| 機台組態 | `InspectionCore/Core0_1/data/machine_setting.json` | 周邊的 port/IP |

**參考實作（不同 repo）**：`C:\Users\TRS001\Documents\workspace\xInsp\plugins\hikrobot_camera`
—— 較新專案的 HikRobot plugin，掉幀問題已經解決得相當完整，見 §6。

---

## 2. 資料路徑（完整一圈）

```
       料件經過閘門
            │
            ▼
  [ESP32] GateSensing()          ISR 內，每個 timer tick
            │                     判斷脈寬 minWidth<w<maxWidth
            ▼
       newPulseEvent()            配發 tid，推進 RBuf
            │
            ▼
       Run_ACTS(cur_pulse)        依 STAGE_PULSE_OFFSET 排程：
            │                     L1A → CAM1 → (SWITCH) → SEL1/SEL2
            ├──────────────────→  PIN_O_CAM1 拉高（觸發相機）
            │                            │
            │                            ▼
            │                     [HikRobot] Line0 收到邊緣 → 出圖
            │                            │
            │                            ▼
            │                     [Core] CameraLayer callback
            │                            │
            └─→ bTrigInfo{tid,usH,usL,Qs} │
                     │                    ▼
                     │            [Core] 檢測 → uInspStatus
                     │                    │
              [PerifChannel]              ▼
                     │            [Core] PerifSendThread
                     │                    │
                     └────────────────────┤ ⚠ 這裡目前斷了，見 §5.6
                                          ▼
                              report{tid,cat} / inspRep{...}
                                          │
                                          ▼
                        [ESP32] 物件到 SWITCH 位置 → SEL1/SEL2 動作
```

**關鍵時序基準**：一切以 stepper 脈衝計數 `SYS_STEP_COUNT` 為準，不是 wall-clock。
`kPerRevPulseCountHw = 2400*16`，ISR 每 16 個 sub-pulse 處理一次。

---

## 3. 韌體：用 v1，不要用 v2

`uInspESP32_v2/` 看起來比較新（模組化、有 HAL、有測試框架、有 `DOC/`），
README 也宣稱 "100% complete (11/11 stages)"。**這是不準確的。**

| | v1 (`uInspESP32/`) | v2 (`uInspESP32_v2/`) |
|---|---|---|
| 最後一次 commit | 2025-10-13（功能） | 2025-09-12（純文件） |
| `pio run` | **SUCCESS** `[已驗證]` | **FAILED** `[已驗證]` |
| Flash / RAM | 22.9% / 11.3% | 沒走到 link |
| 進入點 | `main.cpp` → `firmwareSetup/Loop`，單一清楚 | 見下 |

v2 的三個具體問題（皆 `[已驗證]`）：

1. **HAL 沒被接上。** `platformio.ini` 用 `src_dir = ./`，`SyncTask.cpp` 定義了
   強符號 `setup()/loop()`，蓋掉 `src/main.cpp` 的 `__attribute__((weak))` 版本。
   那條乾淨的 `src/main_hal.cpp`（`custom_hal_init/update`）**編進去了但沒人呼叫**。

2. **build 掛掉。** `build_src_filter = +<*> -<test/>` 沒生效，`test/*.cpp` 被編進韌體，
   而 `test/mock/MockHAL.hpp:10` include 了 `hal/ITransport.hpp` —— 這個檔案在
   `include/ITransport.hpp`，不在 `include/hal/`。**測試框架從未編譯成功過**，
   所以 README 說的 "Stage 9: Host-side testing framework ✅" 也是假的。

3. **ISR 時序退化。** v1 的 `GPIOLS32_SET(PIN)` 是 `GPIO.out_w1ts = 1<<PIN`（單一
   暫存器寫入）；v2 改成 `getHAL().gpio().writePin(...)` → virtual call →
   Arduino `digitalWrite()`。這用在 `IRAM_ATTR onTimer()` 裡，而相機觸發脈寬設定
   只有 2 個 pulse。

**v2 值得留著當 STM32 移植的草稿**（HAL 介面、`src/hal/stm32/`、mock），但不是生產基礎。

---

## 4. 協定

Serial 115200，JSON。命令帶 `id` 會在回應中回echo。

### Host → Device

| 命令 | 說明 |
|---|---|
| `get_setup` / `set_setup` | 讀寫機台設定；`set_setup` 可帶 `"persist":true` 存入 NVS |
| `save_setup` / `clear_saved_setup` | NVS 存檔 / 清除（見 §7） |
| `enter_insp_mode` / `exit_insp_mode` | 進出檢測模式 |
| `clear_error` / `clear_error_history` | 錯誤恢復 |
| **`report{tid,cat}`** | **回報檢測結果**，`cat` 值見下表 |
| `set_sel1_cd{count}` | SEL1 動作倒數，歸零則進錯誤態；`-1` 停用 |
| `stepper_enable` / `stepper_disable` | |
| `trig_phamton_pulse` | 注入假物件（測試用，注意拼字是 `phamton`） |
| `get_running_stat` / `reset_running_stat` | |
| `PING` | 回 `{"type":"PONG"}`，**不帶任何統計數據** |
| `RESET` | 協定層重置，見 §5.5 |

#### 分選機構的實體拓撲

理解 `cat` 值之前要先知道料件實際往哪裡跑：

```
震動盤 ──→ 轉盤 ──→ 閘門感測 ──→ 相機 ──→ 噴料站(SEL1/SEL2) ──→ 排出機構
  ▲                                              │                    │
  │                                        噴中 = 被挑出              │
  │                                                                   │
  └───────────────────── 沒噴到的料件被收集，放回震動盤 ←─────────────┘
```

**關鍵：不動作 = 回流，不是報廢。** 沒有被任何氣閥噴中的料件會流到末端的排出
機構，被收集起來重新倒回震動盤，下一圈再驗一次。

這讓 NA 成為**安全的預設值**：

- 掉幀 / 沒檢測到 → NA → 不噴 → 回流 → **下一圈重新檢測**
- 沒有料件損失，也沒有錯誤分類，代價只有產能

所以偶發掉幀是廉價的，不需要為此停機。這是 §6 選擇「掉幀降級成 NA」而不是
「停機」的實體依據。

> ⚠ **但 NA 率必須被監控。** 回流是無限的 —— 如果某顆料件每次都被判 NA
> （持續的相機問題、或該料件本身總是觸發某種失敗），它會**永遠在系統裡繞圈**，
> 吃掉產能卻永遠不產出。NA 率上升是「產能流失 + 可能有東西在無限回流」的訊號，
> 不是可以忽略的雜訊。`get_running_stat` 的 `NA_Count` 就是拿來看這個的。

#### `report` 的 `cat` 值（`Run_ACTS()` 的 SWITCH 分支）

| `cat` | 行為 | 料件去向 | 計數器 |
|---|---|---|---|
| `1` | 觸發 SEL1 | 噴出 → 挑出 | `SEL1_Count` |
| `2` | 觸發 SEL2 | 噴出 → 挑出 | `SEL2_Count` |
| `3` | **不觸發**（SEL3 未實作，見 §5.4）| **回流** ⚠ 靜默 | `SEL3_Count` |
| `0xFFFF` (65535) | **NA —— 不觸發** | **回流重驗** | `NA_Count` |
| `insp_status_SKIP` (-2100) | 不觸發 | 回流 | `SKIP_Count` |
| 其他 / 未設定 | **`OBJECT_HAS_NO_INSP_RESULT` → 停機** | — | — |

> **掉幀要回報 `cat=0xFFFF`（NA），不是 NG。**
> NG 的語意是「檢測過且不合格」；掉幀是「根本沒檢測到」。兩者混用會污染
> NG 統計、讓良率數據失真，真正的品質問題會被傳輸雜訊蓋掉。
> NA 走回流路徑，該料件下一圈會重新檢測。

> ⚠ **`cat=3` 是個陷阱**：它會加 `SEL3_Count` 但**不觸發任何氣閥**，所以料件
> 靜默回流。如果 core 端的映射可能產生 3，會變成「以為分選了、實際上沒有」。
> 在 SEL3 補完之前（§5.4），不要讓 `cat=3` 出現在回報路徑上。

### Device → Host（非同步）

| 訊息 | 說明 |
|---|---|
| **`bTrigInfo`** | `{tidx, usH, usL, tid, Qs}` —— 相機觸發時發出。`tid` 是物件識別碼、`Qs` 是韌體 RBuf 深度。**這是結果回報的關聯依據** |
| `systemInfo` | 狀態變化 |
| `dbg` | 除錯訊息 |

> ⚠ **README.md 說這個訊息叫 `bT`，是錯的。** 實際 `retdoc["type"]="bTrigInfo"`
> （`LegacyFirmware.cpp` 的 `TaskQ2CommInfo_Type::btrigInfo` 分支）。`[已驗證]`

### 關聯模型：tid vs timestamp

這是新舊機最根本的差異，**不能混用**：

- **uInspMEGA**：host 送 `inspRep{status, time_100us}`，韌體用 `PulseTimeSyncInfo`
  學一條「host µs ↔ 自己 pulse count」的線性映射，換算成目標 gate pulse，再去
  RBuf 找最接近的物件。整套 `SETUP_preBaseTime → ... → READY` 狀態機都是為此。
  失配 → 安靜地分錯槽。

- **uInspESP32**：韌體主動送 `bTrigInfo{tid}`，host 回 `report{tid,cat}`，精確比對。
  失配 → `INSP_RESULT_MATCHES_NO_OBJECT` → **停機**。

**tid 模型是刻意的設計**：`PulseTimeSyncInfo` 在 uInspESP32 裡是死碼，只在
`sysinfo` 初始化時出現一次，之後完全沒用到 `[已驗證]`。移植時留下的殘骸，別以為要用。

---

## 5. 已知地雷

### 5.1 死碼是 `GateSensing2()`，活的是 `GateSensing()`

直覺會以為 `GateSensing2` 是新版。**反了。** `onTimer()` ISR 呼叫的是
`GateSensing()`，`GateSensing2()` 沒有任何呼叫點。`[已驗證]`

這很重要，因為兩者行為不同（下兩條）。

### 5.2 物件位置參考點是「後緣」不是「中心」

`GateSensing()` 裡：

```cpp
uint32_t middle_pulse = gateInfo.start_pulse+(diff>>1);   // 算了…
newPulseEvent(gateInfo.start_pulse, gateInfo.end_pulse,
              SYS_STEP_COUNT,   // ← 傳的是「現在」= 後緣，不是 middle_pulse
              diff);
```

`middle_pulse` 是未使用變數。而第三個參數直接成為 `head->gate_pulse`，**所有
CAM/L/SEL 的 offset 都從它往後推**。

**後果：觸發時序隨物件大小漂移。** 現有 offset（`CAM1_on=654`）是在現有料件尺寸
下試出來的。混料或新機料件尺寸不同，誤差就會跑出來。

`GateSensing2()` 和 v2 的 `GateSensor` 都是對的（傳 `middle_pulse`）。改的話
現有 offset 會整體平移約 `diff/2`，**要重新調機** —— 建議做成可切換
（`gate_ref: "mid"|"trail"`），新機用 mid，舊機不動。

### 5.3 實際跑的閘門偵測沒有防彈跳

`GateSensing()` 是純邊緣比較 `if(new_Sense != gateInfo.cur_Sense)`，
定義好的 `DEBOUNCE_L_THRES`/`DEBOUNCE_H_THRES` **只在死掉的 `GateSensing2()` 裡用到**。

目前靠 `newPulseEvent()` 兩道後級門檻擋抖動：距離 `_PLAT_DIST_step(3500)`（3.5mm）
和時間 `SYS_MIN_PULSE_TIME_SEP_us`（預設 1/15 秒）。擋得住重複觸發，但擋不住
「單一物件被抖成兩段 → 都太窄 → 被 minWidth 濾掉 → **整顆漏檢**」。

### 5.4 SEL3 是半成品

`STAGE_PULSE_OFFSET` 有 `SEL3_on/off`、JSON 讀寫齊全、`SEL3_Count` 有在算 ——
但 **`ACT_SCH` 裡沒有 `ACT_SEL3` 佇列**，`Run_ACTS()` 的 `case 3:` 只做
`SEL3_Count++` 就 break，**氣閥不會動**。`[已驗證]`

要三分類就得補完；只要兩分類就把 JSON 欄位拿掉，別讓調機的人以為設了有用。

### 5.5 Core 在每次連線時送兩次 RESET

`wiringPanel.cpp` 的 PD CONNECT handler：

```cpp
perifCH->send_RESET();
perifCH->send_RESET();   // 兩次
perifCH->RESET();
```

`send_RESET()` 送的是字面的 `{"type":"RESET"}`（`Data_Layer_Protocol.hpp` 的
`RESET_PACKET`），韌體收到後呼叫 `handleResetCommand()`。`[已驗證]`

WebUI 的 PING 看門狗掉 2 次（約 6 秒）就重連 → 重新 CONNECT → 又兩次 RESET。
所以 `handleResetCommand()` 裡放的任何動作都會在**每次重連時執行兩遍**，設計時要當它是常態路徑，不是異常路徑。

### 5.6 ⚠ Core 送的結果協定 uInspESP32 聽不懂（**尚未解決**）

`sendResultTo_perifCH()` 送的是：

```cpp
"{\"type\":\"inspRep\",\"status\":%d,\"idx\":%d,\"count\":%d,\"time_100us\":%lu}"
```

`inspRep` 的 handler **只存在於 uInspMEGA**。uInspESP32 沒有這個 type。`[已驗證]`

而且 `PerifChannel::recv_jsonRaw_data` 是**一根啞管子** —— 把裝置回來的所有東西
原封不動包成 PD MESSAGE 丟給瀏覽器，core 自己不解析任何內容。所以
**`bTrigInfo` 到得了瀏覽器，到不了 core**，而 core 才是送結果的那一方。

**這是自動分選跑不起來的根本原因。** 解法見 §8。

### 5.7 machine_setting.json 的 key 名稱不能當機種標示碼

現有檔案：

```json
"uInsp_peripheral_conn_info1": { "ip": "192.168.2.43", "port": 5213 },
"SLID_peripheral_conn_info":   { "uart_name": "COM5", "baudrate": 230400 }
```

`BPG_WS.js` 找的是 `info.uInsp_peripheral_conn_info` —— **沒有結尾的 `1`**。
對不起來，所以走 else 分支印 `[peripheral] uInsp not configured`。
**目前 uInspMEGA 根本沒被連上**，有人用加尾碼的方式把它停用了。`[已驗證]`

教訓：把語意藏在 key 名稱裡會悄悄失效。要區分機種請用**物件內的顯式欄位**：

```json
"uInsp_peripheral_conn_info": {
  "machine_type": "uInspESP32",     ← 這樣
  "uart_name": "COM6",
  "baudrate": 115200
}
```

好消息是管線已經通的：WebUI 把整個 conn_info 物件展開進 CONNECT 封包
（`{type:"CONNECT", ...connInfo}`），core 的 CONNECT handler 就是用
`JFetch_STRING(json, "uart_name")` 從裡面挖欄位 —— 多一個 `machine_type`
會免費地一路送到 core。

#### conn_info 完整欄位（`machine_setting.json`）

> ⚠ **`InspectionCore/Core0_1/data/` 是 gitignore 的**，這個檔案不進版控
> （每台機器的設定不同）。所以架新機時**沒有範本可以 clone**，欄位得照這裡填。

```jsonc
"uInsp_peripheral_conn_info": {
  "machine_type": "uInspESP32",   // 或 "uInspMEGA"；未設 = uInspMEGA（舊路徑）
  "cat_ok": 1,                    // 良品噴哪一個 SEL —— 見下方警告
  "cat_ng": 2,                    // 不良品噴哪一個 SEL
  "uart_name": "COM6",
  "baudrate": 115200
}
```

- **`cat_ok`/`cat_ng` 任一未設 → 所有料件回報 NA、全部回流、不噴任何東西。**
  這是刻意的安全預設：分選在出口確認之前是關閉的，寧可不分也不要分錯。
  core 啟動時會 `LOGE` 明確告知。
- **key 名稱不能有數字尾碼**（`..._info1` 會讓 `BPG_WS.js` 找不到而靜默停用）

### 5.8 README.md 的 Project Structure 是過時的

它列的 `src/config.h`、`gate_sensor.*`、`pipeline.*`、`state_machine.*`、
`stepper.*`、`lib/DataLayer/` **全部不存在**。實際上這些都在
`src/app/LegacyFirmware.cpp` 裡（2400+ 行），pin 定義在
`include/config/HardwareConfig.hpp`。

---

## 6. 相機層：掉幀處理

### 現況（visSele）

`CameraLayer_HikRobot_Camera.cpp` 有 `nFrameNum` 連號偵測，但註解直說：

```cpp
// Drop detection. MVS nFrameNum increments for every frame the sensor
// exposes regardless of whether it reaches us; a gap means frames were
// lost between the sensor and this callback.
// Diagnostic only -- nothing downstream depends on it.     ← 只印 log
```

而且有兩個組態問題 `[已驗證]`：

1. **`TriggerSource` 優先選 `"anyway"`（13）**，失敗才退到 Line0。
2. **從不設定 `TriggerSelector`**。

`frameInfo` 結構也沒有 `frameNum` 欄位（只有 timeStamp/offset/w/h/channelCount/pixelBits）。

### 參考解（xInsp plugin）

`xInsp\plugins\hikrobot_camera` 已經把這件事做完了，README 記錄了 13 條
MVS 踩坑筆記。直接相關的：

| # | 內容 |
|---|---|
| **4** | `TriggerMode=On` 在沒設 `TriggerSelector` 前是 no-op —— 你配到的是韌體預設的 selector（常是 `AcquisitionStart`），**不是**「一個邊緣一張圖」。要先設 `TriggerSelector=FrameBurstStart` |
| **7** | `TriggerSource="Anyway"` 會**讓 Line0RisingEdge 事件靜音**。visSele 正好選在這個模式 —— 就算加了事件註冊也不會觸發 |
| **10** | `Counter0` 在 HikRobot USB3 韌體上是 one-shot，第一個邊緣後就停。**不能拿來當邊緣計數器**，只能用 `Line0RisingEdge` 事件 |
| **11** | `FrameTrigger`/`FrameTriggerMiss` 事件在 MV-CE200 和 MV-CA050 上都被韌體拒絕 |
| **13** | 天真的 FIFO 填充會把掉幀歸因到 burst 尾端而非實際位置；要在填充前掃描過期 slot |

它的核心機制是 **strict mode**：`Line0RisingEdge` 事件為每個邊緣 push 一個空
slot，影像 FIFO 填入，沒填到的 slot 標記 `missed:true` 但**保留正確的 trigger_id**。
壓測結果（300 觸發 @ 30Hz、雙相機、25% 強制掉幀）：1:1 不變式 100% 維持。

**這比「偵測到錯位就停機」好得多 —— 掉幀降級成「一顆料件判 NA」，機台繼續跑。**

⚠ **掉幀一定要走 NA（`cat=0xFFFF`）而不是 NG。** 見 §4：NG 是「檢測過且不合格」，
掉幀是「沒檢測到」。判成 NG 會把 USB/驅動的傳輸問題混進良率統計，之後追品質
問題時會被雜訊帶偏。

而且因為 NA 不噴、料件回流重驗（§4 拓撲圖），**掉幀在這條產線上幾乎沒有成本**
—— 該顆料件下一圈會重新檢測。這是這個設計比「停機」好得多的實體原因：
偶發傳輸問題不該讓整台機器停下來，它自己會在下一圈解決。

代價是產能，所以 NA 率仍要監控（持續 NA 的料件會無限回流）。

### ⚠ 硬體：3.3V 推不動光耦

plugin README 的接線章節明確指出：

> cameras need 5–24 V on `Line0`; ESP32 GPIO at 3.3 V is usually below the opto
> threshold. Either level-shift or use an opto (6N137 / TLP2309).

uInspESP32 的 `PIN_O_CAM1`（GPIO 17）是**裸的 3.3V ESP32 GPIO**。現役機台若能跑，
可能是勉強跨過門檻 —— 那會是溫度/批次相關的**間歇性漏觸發**，極難查。

**兩台新機請在設計階段就加準位轉換或光耦，不要沿用。軟體補不了這個。**

---

## 7. 本次已完成的改動

分支 `ct/uinsp_2mach`（從 `5db4dc42`）。

| Commit | 內容 |
|---|---|
| `de99f98f` | NVS 持久化 + `machine_id`；ISR 內 `digitalRead` → 暫存器直讀；輸出 fail-safe 集中成 `ALL_OUTPUTS_SAFE()` |
| `535d92fb` | ISR 內的錯誤轉移延後到主迴圈（原本從 ISR 呼叫 `pinMode`/`digitalWrite`）|
| `d956ddd9` | WebUI 抽出 `Perif_API_Base` 去重；新增 `uInspESP32_API` |
| `d7b74f61` | **core 的 tid 結果路徑**：`bTrigInfo` tap、FIFO 配對、`report{tid,cat}`、`machine_type` 辨識 |
| `3ffadfd1` | **掉幀吸收**：`frameInfo.frameNum` 貫通、斷號 → 補送 NA 讓 FIFO 重新對齊 |

### core 的 tid 路徑（`d7b74f61`）

`PerifChannel` 現在會攔 `bTrigInfo` 存進 `perifTriggerQueue`（**仍原封不動轉發給
瀏覽器**，不影響既有 WebUI）。結果在 `InspResultAction_s` 產生時就 FIFO 配對
最舊的未認領 trigger —— 在這裡配而不是在送出執行緒配，是為了讓配對跟著
**影像順序**而不是寫入順序。

`machine_type` 未設或不認得 → 完全走舊的 `inspRep` 路徑，未 opt-in 的部署零影響。

**分選目前是刻意關閉的**：`cat_ok`/`cat_ng` 任一未設，`perif_status_to_cat()`
一律回 NA。整條管線可以端到端測試（tid 配對、不會 fault），但**證明不可能分錯槽**。
確認出口後填上設定即可啟用。

配不到 trigger 的影像**不送**：韌體對未知 tid 會 fault，捏造一個比沉默更糟；
而且「有影像沒 trigger」本身就代表配對已經失步了。

### 掉幀吸收（`3ffadfd1`）

裸 FIFO 有個致命弱點：**掉一幀就永久錯位**，之後每顆料件都用鄰居的判定去分選 ——
正是 tid 協定想根除的那種失效。

不需要猜。`nFrameNum` 計的是感測器曝光了幾幀（不管有沒有送達），所以斷號
**就是精確的掉幀數**。斷號 N 就把排在前面的 N 個 tid 提前退成 NA，那些料件回流
重驗，FIFO 下一幀立刻對齊。

實作放在 core 而不是讓相機層產生佔位影像：`CameraLayer` 的契約是「push 一張影像
的 callback」，硬生出沒有影像的 frame 會動到每個消費端。而 core 本來就握有
trigger 佇列，兩個計數在這裡交會最自然。

`frameNum`/`frameNumValid` 預設 `0/false`，所以給不出感測器計數的驅動
（Aravis、BMP、MindVision）自動退出這個機制，行為不變。

> ⚠ **`nFrameNum` 只抓得到「傳輸掉幀」**（曝光了但沒送到）。抓不到
> **「感測器根本沒接受這個觸發」**（曝光中、ROI 太大讀不完）—— 那種情況
> `nFrameNum` 不會有斷號，因為那一幀從來沒被曝光。要抓那個得靠
> `Line0RisingEdge` 事件計數（xInsp plugin 的機制 #2/#4），仍未實作。

### NVS 持久化怎麼用

存的欄位：`STAGE_PULSE_OFFSET` 全部、`plateFreq`、`minDetectTimeSep_us`、
`pulse_minWidth/maxWidth`、`machine_id`。

```jsonc
{"type":"set_setup", "persist":true, "stage_pulse_offset":{...}}  // 設定並存檔
{"type":"save_setup"}                                              // 存當前值
{"type":"clear_saved_setup"}                                       // 清 NVS
```

三個設計決定：
- **整包 blob 而非逐欄位 key** —— 寫到一半斷電不會變成半台 A 半台 B 的 offset
- **`persist` 是 opt-in** —— 不帶就跟以前一樣純 RAM，調機時反覆試不會一直燒 flash
- **magic/version 不符就退回編譯預設值** —— 舊板子刷新韌體不會載入亂數

`get_setup` 會回 `machine_id` 和 `cfg_from_nvs`。**`cfg_from_nvs:false` 代表這塊板子
跑的是編譯預設值而不是自己的存檔** —— 在它開始把料件往錯的槽裡吹之前就該叫出來。

### WebUI API 結構

```
Perif_API_Base                傳輸層（連線/追蹤窗/PING/設定檔/延遲量測）
├── uInsp_API      → uInspMEGA     pulse_hz、res_count 吞吐率
├── uInspESP32_API → uInspESP32    ch 10027，本次新增
└── GenPerif_API   → 通用
    └── SLID_API   → 坡檢設備
```

重構時**刻意保留**兩處看起來像 bug 的行為：
- `resyncRequiresAck()` 是 per-class 開關，因為 **uInspMEGA 的 `get_setup` 回
  `"type":"get_setup_rsp"` 且完全沒有 `ack` 欄位** —— 無條件檢查會讓它每次 resync 都 bail out
- `GenPerif_API` 仍然把 PING 回應丟掉（原本就如此），因為 `SLID_API` 繼承它、是產線在跑的設備

---

## 8. 未完成 / 待決策

### 待實作

| 項目 | 說明 |
|---|---|
| ~~Core 的 tid 結果路徑~~ | ✅ `d7b74f61`（分選待設定啟用）|
| ~~`machine_type` 欄位~~ | ✅ `d7b74f61` |
| ~~掉幀吸收~~ | ✅ `3ffadfd1` —— `nFrameNum` 斷號 → 補送 N 個 NA，FIFO 下一幀就對齊 |
| **相機觸發組態** | `TriggerSelector`（caveat #4，目前沒設）+ `TriggerSource` 從 `Anyway` 改 `Line0`（caveat #7，目前的設定讓事件靜音）|
| **`Line0RisingEdge` 事件** | 第二個獨立邊緣計數，用來跟 ESP32 的 `tid` 互相驗證。`nFrameNum` 只抓得到「傳輸掉幀」，抓不到「感測器根本沒接受這個觸發」 |
| **實機驗證 tid 配對** | 上面全是靜態驗證，一次都還沒接過真機 |
| `gate_ref` 中心/後緣可切換 | §5.2，**切 mid 要重新調機** |
| 閘門防彈跳 | §5.3，建議門檻可設定、預設 0 = 現行行為 |
| SEL3 補完或移除 | §5.4 |
| uInspESP32 操作面板 | API 層有了，`rdxComponent.jsx` 還沒有對應 UI |
| 側邊選單項目 | `script.jsx` 的裝置清單還沒加 ESP32 機種 |

### 待決策

1. **`uInspStatus` → `cat` 的映射。** core 的 `uInspStatus` 值域是什麼，
   OK/NG 分別對到 `cat` 1 還是 2？**對不上會分錯槽。**
   （掉幀 → `cat=0xFFFF`(NA) 這條已確定，見 §4 表格。）

   `[未驗證，推論]` SEL1 可能是良品出口 —— 韌體有 `SEL1_ACT_COUNTDOWN` /
   `set_sel1_cd`，動作次數歸零就進錯誤態，這個「數到 N 就停」的語意比較像
   良品的批量計數，不像不良品。**請實機確認再定映射**，猜錯的話良品和不良品
   會整批互換，而且因為兩者都會被噴出、不會回流，錯了不會自己顯現。
2. **兩台新機的相機型號。** 沿用現役還是新採購？若新購，照 xInsp plugin 的
   verified 清單（MV-CE200-11UM / MV-CA050-12UC）可省一輪韌體事件支援度試錯。
3. ~~新機走 visSele 還是 xInsp？~~ **已決定：走 Core0_1（visSele 舊專案）**，
   所以 §6 的相機層要在 visSele 這邊補強，不是移植到 xInsp。
   xInsp 的 plugin 仍是最佳參考實作。

### 已確定

- 新機**取代** uInspMEGA，新程式碼可重新定義協定
- 舊機相容「看情況」，不是硬需求 —— 用 `machine_type` 分歧即可，成本很低
- **單相機**，`tidx` 恆為 1，一條 FIFO
- FIFO 配對可接受（當初用 timestamp 是因為不確定怎麼掌握 HikRobot 掉幀，
  而這個問題在 xInsp plugin 已經解決）
- **掉幀 / 未檢測 → NA（`cat=0xFFFF`），不是 NG** —— 不可污染良率統計

---

## 9. 驗證指令

```sh
# 韌體
cd Peripheral/uInspESP32 && pio run          # 應為 SUCCESS，Flash ~23% RAM ~11%
pio run --target upload
pio device monitor                            # 115200

# WebUI
cd UI/WebUI && npm run build                  # vite，約 5000 modules

# WebUI regress（本機跑不了）
# 需要 webctl daemon + dev app + core :4090，且 fixture 路徑寫死成 macOS 家目錄
```

### ⚠ MSYS2 GCC 要先設 PATH，否則錯誤訊息會騙你

從 git-bash 直接呼叫 `C:\msys64\mingw64\bin\c++.exe` 編譯時：

```
rc=1，完全沒有任何診斷輸出，也沒有產出 .o
```

看起來像編譯器壞了或程式碼有問題，**其實是 `cc1plus` 載入不到 MSYS2 的 DLL**。
`c++.exe --version` 會正常回應，所以很容易誤判。修法：

```sh
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
```

同一個根因也會讓 `cmake --build` 在 vcpkg 的 compiler detection 階段失敗
（`vcpkg was unable to detect the active compiler's information`）。

單檔語法檢查（不需要完整 link）：

```sh
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
# 從 build/win-mingw-ninja/compile_commands.json 撈出該檔的編譯指令來跑
```

除錯環境變數：`INSP_PERIF_LOG=1` 會讓 core 印出周邊的序列埠 round-trip 時間，
可用來判斷延遲是在序列埠/ESP32 那一段還是 WS/WebUI 那一段。
