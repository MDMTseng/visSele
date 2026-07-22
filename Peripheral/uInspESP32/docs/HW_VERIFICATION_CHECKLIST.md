# uInspESP32 硬體驗證清單

**這份清單裡的每一項都還沒被驗證過。** 分支 `ct/uinsp_2mach` 上所有改動都只做過
靜態驗證（編譯乾淨），**一次都沒有接過真的機器**。

按順序做。每一階段都是後面的前提——跳著做的話失敗了會分不清是誰的問題。

搭配 `INTEGRATION_MAP.md` 使用（§ 編號指向該文件）。

---

## 準備

```sh
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"   # 見 INTEGRATION_MAP §9
cd Peripheral/uInspESP32 && pio run --target upload
cd UI/WebUI && npm run build
export INSP_PERIF_LOG=1     # core 印出序列埠 round-trip
```

`machine_setting.json`（`InspectionCore/Core0_1/data/`，**不在版控裡**）：

```jsonc
"uInsp_peripheral_conn_info": {
  "machine_type": "uInspESP32",
  "uart_name": "COM?",
  "baudrate": 115200
  // cat_ok / cat_ng 先「不要填」—— 階段 4 之前分選必須保持關閉
}
```

---

### 用工具跑

```sh
pip install pyserial
cd Peripheral/uInspESP32/tools
python uinsp_test.py ports
python uinsp_test.py --port COM? all
```

`tools/uinsp_test.py` 直接對韌體下 JSON，不經過 core / WebUI，把階段 0~3
能自動化的部分自動化，其餘停下來問你。每次跑完產出 `uinsp_verify_report.md`。
細節見 `tools/README.md`。

---

## 階段 0 — 韌體單獨（不接 core）

用序列埠工具直接對板子下命令，115200。

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 0.1 | `{"type":"PING"}` | `{"type":"PONG"}` | ☐ |
| 0.2 | `{"type":"get_setup"}` | 回 `machine_id`、`cfg_from_nvs`、`stage_pulse_offset`、`pulse_minWidth/maxWidth` | ☐ |
| 0.3 | 確認 `cfg_from_nvs` | 全新板子應為 **false**（跑編譯預設值）| ☐ |
| 0.4 | `{"type":"set_setup","machine_id":"M1","persist":true}` | 回 `persisted:true` | ☐ |
| 0.5 | **斷電重開**，再 `get_setup` | `machine_id=="M1"` 且 `cfg_from_nvs==true` | ☐ |
| 0.6 | `{"type":"clear_saved_setup"}` → 斷電重開 → `get_setup` | `cfg_from_nvs==false`，offset 回到編譯預設 | ☐ |

> 0.5 是 NVS 持久化的關鍵驗證（commit `de99f98f`）。**沒過就別往下走**，
> 後面兩台機台的設定管理全靠它。

### 0.7 錯誤路徑（`535d92fb`）

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 0.7a | 進檢測模式、手動遮閘門製造一顆沒有結果的物件 | 進 `INSPECTION_MODE_ERROR`，**不當機**、不重開 | ☐ |
| 0.7b | 觀察錯誤發生瞬間的氣閥 | 立刻斷電，不等盤子降速完 | ☐ |
| 0.7c | `{"type":"clear_error"}` | 回到 IDLE | ☐ |

> 0.7a 特別重要：錯誤原本是從 ISR 直接呼叫 `pinMode`/`digitalWrite`。如果改動
> 有問題，症狀會是**當掉或重開**而不是乾淨地進錯誤態。

---

## 階段 0B — 光板診斷（不用接機構）★

**只要 ESP32 板 + USB 線。** `trig_phamton_pulse` 繞過閘門感測直接產生物件，
所以整條 tid 路徑（配發 tid → `bTrigInfo` → `report` → 氣閥輸出 → 錯誤處理）
都可以在桌上驗完。

```sh
python uinsp_test.py --port COM? bench --count 10 --cat 1
```

| # | 檢查 | 實際 |
|---|---|---|
| B.3 | timer ISR 真的在跳（`SYS_STEP_COUNT` 前進）| ☐ |
| B.5 | 每個假脈衝剛好一個 `bTrigInfo` | ☐ |
| B.6 | `tid` 嚴格 +1 | ☐ |
| B.8 | SEL 計數器增加正確筆數 | ☐ |
| **B.9** | **不存在的 tid → 停機**（安全網存在）| ☐ |
| **B.12** | **不回報的物件 → 乾淨停機，板子仍回應**（ISR 錯誤路徑）| ☐ |

> **建議先把階段 0 + 0B 在桌上跑到全綠再上機。** 這樣機台上出狀況時，
> 韌體那一層已經被排除掉了，省下最難查的那種「不知道是哪一層」的時間。
>
> B.12 取代了下面 0.7 需要人工遮閘門的做法，而且更可靠。

---

## 階段 1 — 相機儀表（`c3ae4fd2`）

**只看 log，不要改任何設定。** 這階段的目的是把未知變成已知。

啟動 core，接上相機，看 log：

```
[trigger cfg after-open]        TriggerSelector    ret=? cur=?
[trigger cfg after-open]        TriggerMode        ret=? cur=?
[trigger cfg after-open]        TriggerSource      ret=? cur=?
event Line0RisingEdge: selector=? notify=? register=?
```

| # | 要記下的事 | 為什麼重要 | 實際 |
|---|---|---|---|
| 1.1 | `TriggerSelector` 的 `cur` 值 | 這份程式從來沒設過它 —— 韌體預設一直在決定「trigger mode on」到底套用在什麼上（INTEGRATION_MAP §6 caveat #4）| ☐ |
| 1.2 | `event Line0RisingEdge` 三個回傳值 | 非 0 = 這台韌體不支援該事件 | ☐ |
| 1.3 | 跑一段產線，看 `perif trig:` 那行 | `line0RisingEdges` 有沒有在增加 | ☐ |

### 判讀

- **`line0RisingEdges` 一直是 0** → 預期中，因為 `TriggerSource` 目前是
  `Anyway`（caveat #7 會讓事件靜音）。此時做 1.4。
- **`line0RisingEdges` 有在跳** → 意外的好消息，直接跳到 1.5。

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 1.4 | **暫時**把 `TriggerSource` 改成 `Line0`（`CameraLayer_HikRobot_Camera.cpp` 的 `TriggerMode(2)`），重跑 | 影像**仍然正常進來** 且 `line0RisingEdges` 開始跳 | ☐ |
| 1.5 | 比對 `line0RisingEdges` vs `line0FallingEdges` | **應該相等** | ☐ |

> ⚠ **1.4 是有風險的一步。** 如果觸發線實際接在 Line1/Line2，設定會成功但
> **完全收不到影像**。看到影像停了就是這個原因，改回 `Anyway` 即可。
>
> ⚠ **1.5 若 rising ≠ falling，先停下來查硬體。** 那代表輸入端有彈跳、或電壓
> 只是勉強跨過光耦門檻 —— 正是 3.3V GPIO 推 5-24V 輸入的典型症狀
> （INTEGRATION_MAP §6）。**這個問題軟體補不了**，而且它會讓後面所有配對
> 測試的結果不可信。

---

## 階段 2 — tid 配對（分選仍關閉）

`cat_ok`/`cat_ng` **保持未設**。core 啟動時應該看到：

```
perif cat_ok/cat_ng not set in conn_info -- every part will be reported NA
and recirculate. Sorting is OFF until both are declared.
```

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 2.1 | 確認上面那行有出現 | 分選確實是關的 | ☐ |
| 2.2 | 跑料，觀察氣閥 | **完全不動作**，所有料件回流 | ☐ |
| 2.3 | 看 `perif trig:` 的 `rx` | 隨料件數穩定增加 | ☐ |
| 2.4 | 看 `pending` | **穩定在 0 附近**，不持續累積 | ☐ |
| 2.5 | 韌體端 `get_running_stat` | `NA_Count` 增加，`SEL1/SEL2_Count` 為 0 | ☐ |
| 2.6 | 整場跑完，**不應出現** `INSPECTION_MODE_ERROR` | tid 配對正確 | ☐ |
| 2.7 | 不應出現 `frame with no pending trigger` | 反之代表配對失步 | ☐ |

> 2.4 的 `pending` 持續爬升 = 結果產生速度跟不上觸發，或有 `bTrigInfo` 沒被消化。
> 2.6 出現 `INSP_RESULT_MATCHES_NO_OBJECT` = tid 對不上，**這是整條路徑的核心假設破了**。

### 2.8 掉幀吸收（`3ffadfd1`）

沒有現成的注入掉幀手段（xInsp plugin 有 `debug_drop_every_n`，這邊沒有）。
可行的替代：

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 2.8a | 提高幀率/加大 ROI 直到 log 出現 `camera dropped N frame(s)` | 同時出現 `-> reporting NA` | ☐ |
| 2.8b | 掉幀之後繼續跑 | **不出現** `INSPECTION_MODE_ERROR`，`missed(NA)` 增加，`pending` 回到 0 | ☐ |

> 2.8b 是這次改動最重要的一項：掉幀後 FIFO 應該**自己對齊**。
> 如果掉幀後開始連續報 `INSP_RESULT_MATCHES_NO_OBJECT`，代表吸收邏輯沒生效。

---

## 階段 3 — 分選對應（⚠ 一次只開一個）

**這是唯一會讓料件被實際噴出的階段，錯了不會自己顯現**（噴出的料件不回流）。

先確認實體出口：

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 3.1 | 韌體 `{"type":"sel_act",...}` 手動打 SEL1 | 記下 **SEL1 噴到哪個桶** | ☐ |
| 3.2 | 手動打 SEL2 | 記下 **SEL2 噴到哪個桶** | ☐ |
| 3.3 | 據此決定 `cat_ok` / `cat_ng` | 良品桶 → `cat_ok` | ☐ |

> INTEGRATION_MAP §8 有個**未驗證的推論**：SEL1 可能是良品出口（因為
> `SEL1_ACT_COUNTDOWN` 數到 0 會停機，像批量計數）。**以 3.1/3.2 的實測為準，
> 不要採信那個推論。**

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 3.4 | 填入 `cat_ok`/`cat_ng`，重連 | log 出現 `perif sorting: OK->SEL? NG->SEL?` | ☐ |
| 3.5 | 放**已知良品**數顆 | 全部進良品桶 | ☐ |
| 3.6 | 放**已知不良品**數顆 | 全部進不良品桶 | ☐ |
| 3.7 | 混放 | 分類正確 | ☐ |

---

## 階段 4 — WebUI

| # | 動作 | 預期 | 實際 |
|---|---|---|---|
| 4.1 | 現有 SLID / CNC 設備仍可正常連線操作 | **重構沒打壞既有功能**（`d956ddd9`）| ☐ |
| 4.2 | uInspMEGA 若有機器可接，設定同步仍正常 | `resyncRequiresAck` 的 per-class 開關有效 | ☐ |
| 4.3 | `diagnoseComm` 按鈕 | 仍可量測延遲 | ☐ |

> 4.1/4.2 是回歸測試。`Perif_API_Base` 抽取時**刻意保留**了兩個看起來像 bug
> 的行為，就是為了不打壞這兩條（INTEGRATION_MAP §7）。

---

## 階段 5 — 待補功能的確認

這些是**已知沒做**的，跑一次確認影響範圍：

| # | 項目 | 確認 | 實際 |
|---|---|---|---|
| 5.1 | `cat=3` 靜默陷阱 | core 的映射**不會**產生 3（否則料件靜默回流）| ☐ |
| 5.2 | SEL3 | 確認新機是否需要三分類；不需要就移除 JSON 欄位 | ☐ |
| 5.3 | gate 參考點 | 量測不同尺寸料件的觸發位置是否漂移（INTEGRATION_MAP §5.2）| ☐ |
| 5.4 | 閘門防彈跳 | 觀察是否有單顆料件被切成兩段而漏檢（§5.3）| ☐ |
| 5.5 | RESET 行為 | 拔插序列埠觸發重連，確認氣閥動作可接受（§5.5，每次連線送兩次）| ☐ |

---

## 記錄格式

每項請記下**實際觀察值**而不只是打勾，尤其：

- 階段 1 的四個 `TriggerSelector/Mode/Source/Activation` 數值
- 階段 1.5 的 rising / falling 實際數字
- 階段 2 跑完時的 `rx / missed(NA) / pending` 三個數
- 階段 3.1/3.2 的實體出口對應

這些數字會決定接下來要不要做 strict mode 的完整版（`Line0RisingEdge` slot push
+ 過期掃描 + reaper），或者 `nFrameNum` 吸收就已經夠用。
