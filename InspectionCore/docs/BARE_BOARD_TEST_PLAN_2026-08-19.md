# 裸板測試計畫 — 2026-08-19(10 小時)

**手邊有什麼**:ESP32 板一塊(COM3,新韌體含 P1 修正,NVS 完好)、Windows bench、
core、WebUI。**沒有相機、沒有轉盤。**

這份計畫只列**在這個條件下真的能跑**的東西。每一項都標了它的依據是「已實測」
還是「推測」——推測的項目請先花十分鐘證實再投入,不要照著跑一整個小時。

---

## 0. 為什麼今天能做的比昨天多

三件事在 2026-08-19 改變了:

1. **`INSP_PERIF_CONSOLE` 移植到 Windows**(`49db1c88`)。在此之前
   `PerifConsoleThread` 整個在 `#ifndef _WIN32` 裡,4099 永遠不開,而
   `Peripheral/uInspESP32/tools/TESTS.md` 那 38 個工具**全部**靠它說話。
2. **板子在線**,`cat_ng:1 cat_ok:3` 已宣告 —— `hist_wiring.mjs` 因此第一次
   真的斷言而不是 SKIP。
3. **乾跑模式可用**。`set_dry_run{on:true}` 讓 SEL 閥門不動作
   (`LegacyFirmware.cpp:3288/3324/3356` 都要 `DRY_RUN==false`)並保持 stepper
   通電。配上馬達沒接,設 `plate.freq` 只是輸出訊號到空腳位。

**已實測可行的 headless 啟動序列**(不需要瀏覽器):

```
!pd {"type":"CONNECT","uart_name":"COM3","baudrate":230400,
     "machine_type":"uInspESP32","cam_idx":1,"pairing":"timestamp",
     "cat_ng":1,"cat_ok":3}
{"type":"set_dry_run","on":true}
{"type":"set_setup","plate":{"freq":12}}      # 注意:巢狀,不是 top-level
{"type":"enter_insp_mode"}                    # -> State changed from 100 to 102
```

`plate.freq` 是 `set_setup` 的巢狀欄位,不是獨立指令 —— 這一點吃掉我半小時,
`{"type":"plate","freq":12}` 會被靜靜忽略。

### 但它到不了終點,而且原因今天無解

上面的序列讓板子從 100 走到 **102 (INSPECTION_MODE_CAL)**,然後**必然掉進
112 (INSPECTION_MODE_ERROR)**,`error_hist` 留下 **14 =
`CAM_CLOCK_CAL_FAILED`「clock calibration did not converge」**。

CAMSYNC CAL 要靠相機回傳的 pulse 才能收斂(`LegacyFirmware.cpp:4530-4554`),
沒有相機就沒有 pulse。這不是設定問題,今天繞不過去。

**直接後果:完整鏈路(phantom 零件 → verdict → SEL 決策)今天測不了。**
`phantom_feed.mjs` 送了 249 個 phantom 而 `count` 全部是 0,原因就是板子
根本不在 inspection mode。這也正是先前那個一直查不出的 state 112。

**不受影響的是 Track A 的其餘部分** —— 底下的指令面掃描、協定 fuzz、io_trace、
故障注入,沒有一個需要進 inspection mode。那才是今天的主戰場。

---

### 迴圈的現況:校正閉合了,零件還沒

**已閉合**: 讓 core 直接回答板子的 trigger,校正因此
收斂 —— 、、 **精確等於**設定的
(5000 和 800 各驗過一次)、。在此之前每次
都是跑 16 秒 CAL 然後掉 112。

**還沒閉合**:零件通過 gate 進了 pipe(/ 會增加),但
 全 0、SEL 計數不動。根因**不是**配對邏輯 —— 直接觀測發現:



**phantom 零件從來不觸發 CAM1**,所以沒有 cam_trig 宣告,core 沒有東西可回答,
物件就永遠停在 waiting。sync pulse 能成功是因為它走另一條路、自己會發 cam_trig。

排除掉的假設(不要再試):匹配窗(5000us 與 800us 行為相同)、物件註冊時機
(物件在 gate 時就進 RBuf,CAM1 時才填 )、 欄位缺失(兩個
cam_trig 送出點都有)。

下一步是查 phantom 到 CAM1 之間斷在哪 —— 
需要盤子轉到那個 pulse 位置,而  有讀數(14.33)表示 counter
在走。若 phantom 本來就不設計成會觸發相機,那就要改用會觸發的路徑
( / )或讓 phantom 也走 CAM1 階段。

---

## 1. 起手式(30 分鐘,先做,別跳過)

建立基準線,否則後面每個紅燈都要先花時間排除「本來就是紅的」。

| 步驟 | 指令 | 預期 |
|---|---|---|
| 1.1 | `node UI/WebUI/tools/webctl/suite_nohw.mjs` | 12 pass / 1 skip / 1 fail(fail 是 `qa/run.mjs` 帶著它自己那 4 個) |
| 1.2 | `node UI/WebUI/tools/webctl/qa/run.mjs` | 35 pass / 0 skip / 4 fail |
| 1.3 | 記下 `SYSTIME` 與 `get_running_stat` 的 `count`/`yield` | 之後每次比對用 |

**已實測。** 1.1 和 1.2 的數字是 2026-08-19 當天量到的。

---

## 2. Track A — ESP32 traffic(最大的未測面,建議 4 小時)

板子接受 **70 個指令**。目前有測到的不到十個。這是今天投資報酬率最高的一塊,
而且**完全不需要相機或轉盤**。

### A1. 指令面的一致性掃描(90 分鐘)— 已實測基礎

每個指令送三種形態:合法、缺參數、型別錯誤。斷言三件事:
- 一定有回覆(不會靜靜吞掉)
- `ack` 欄位存在且與實際結果相符
- 板子在之後仍然回應 `get_running_stat`(用 SYSTIME 確認沒重啟)

已知的 70 個:`abort alt aux_test bye cam center centre clear_error
clear_error_history clear_saved_setup clear_verdict_log comm_lost_backup
crash_test enter_insp_mode exit_insp_mode fault gate get_backup_stat
get_running_stat get_schema get_setup get_spikes get_state_names
get_verdict_log get_version get_width_hist io_on_level io_trace_arm
io_trace_dump io_trace_stop jog jog_arm jog_end light mode none pin_mode
pin_off pin_on pin_read ping plate poll prbs pushlog reboot_bootloader report
reset_latency_stat reset_running_stat save_setup sel_act set_dry_run
set_gate_disable set_setup skip_policy slow_and_stop slow_only
stage_pulse_center stage_pulse_offset stage_pulse_width_us stepper_disable
stepper_enable stop_only trig_cam_burst trig_cam_pulse trig_phantom_pulse
trig_phantom_train trig_report virt_pulse wdt_test`

**先跳過這幾個**,它們有實體或破壞性後果:`reboot_bootloader`、`save_setup`、
`clear_saved_setup`(會動 NVS)、`crash_test`、`wdt_test`(留到 A4)。

`get_schema` 的回覆可以拿來自動產生「哪些欄位該接受什麼型別」,不用手寫表。

### A2. 協定層 fuzz 打真板子(60 分鐘)— 已實測基礎

`Peripheral/uInspESP32/tools/test_data_layer_overflow.cpp` 已經在 host 上跑
400 trials。把同一組 payload 透過 console 送到**真板子**,用 SYSTIME 單調性
確認沒重啟。今天已用四種 P1 形狀驗過(含 4095 bytes 無 brace),板子存活。

要擴充的:超長行(console 的 line buffer 是 4096,送 5000 會怎樣)、
分片送達(一個 JSON 拆成多次 write)、NUL 夾在中間、CRC trailer 錯誤。

### A3. IO 時序驗證(60 分鐘)— **推測,先花十分鐘證實**

`io_trace_arm` / `io_trace_dump` / `io_trace_stop` 看起來能記錄腳位時序而
不需要看實體。若成立,可以驗證 `stage_pulse_offset` / `stage_pulse_width_us` /
`stage_pulse_center` 這三組設定真的反映在輸出時序上 —— 那是目前**完全沒有自動
測試**的一塊,而且它直接決定零件被吹到哪個料道。

配合 `trig_phantom_train` 產生一串零件,再 dump 時序來比對。

### A4. 故障注入與恢復(30 分鐘)— 破壞性,放最後

`crash_test`、`wdt_test`、`fault`。每一個之後確認:板子重啟、NVS 設定還在
(`machine_id`、`io_on_level`、`cfg_from_nvs`)、core 能重新連上。
**這會重啟板子,所以排在 Track A 最後。**

---

## 3. Track B — Core(建議 2.5 小時)

### B1. console 自己的協定面(45 分鐘)— 新程式碼,只驗過 happy path

我今天才移植它,而且移植過程就踩到一個真 bug(非阻塞 read 把閒置客戶端當斷線)。
它現在只驗證過正常路徑,該測:
- 超長行(>4096)、沒有換行就斷線、送一半就斷線
- 多個客戶端搶(程式碼說 one client at a time,舊的會被關掉)
- 客戶端讀很慢時 echo 會丟(設計如此)——確認它**丟的是 echo 而不是卡住 perif RX**
- `!xx` 注入未知的兩字母 TL

### B2. perif link 生命週期(45 分鐘)— 現在有真板子

- `PD CONNECT` / `DISCONNECT` 反覆(注意:序列埠 CONNECT 會 DTR 重啟板子)
- 拔 USB → tx_fail → suspect → 插回 → 恢復
- **今天發現的時序陷阱**:core 在板子開機完成前(約 6 秒)連上去,link 永遠不同步,
  log 每 9 秒噴 `perif: link RESYNC requested`。這值得寫成一個自動測試,因為
  它看起來完全像韌體壞掉。

### B3. 既有的 core 測試重跑(30 分鐘)

`bpg_sweep.mjs --include-crashers`(35 cases)、`churn.mjs`、`fd_leak.mjs`、
`slow_client.mjs`、`doorbell.mjs`。這些今天都綠,重跑是為了確認 console 移植
沒有影響它們 —— 我改的 `wiringPanel.cpp` 是共用檔案。

### B4. 修 `link_fault.mjs` 還 slot(30 分鐘)

REGRESSION_TESTS trap 13:它 PD CONNECT 進真板子那個 slot 且不還回去,跑完
板子留在 SUSPECT。修好之後它就能進 `suite_nohw` 的 RUNNABLE。

---

## 4. Track C — WebUI(建議 2 小時)

### C1. 那三個間歇(60 分鐘)— trap 12,今天最大的信任問題

`doorbell`(同位置 1 FAIL 後 1 PASS)、`r6_inspection` T1(三跑 1 過 2 敗)、
`r7_inspbug` T1(2 敗),三者單獨跑都綠。**未試的線索**:
`ActionThrottle_type: 'express'` —— `ActionThrottle.js` 對它短路,試了就能斷定
節流有沒有牽涉其中。

重現需要完整長跑,不是單跑一個 suite —— 那正是它難追的原因。

### C2. 兩個把 dev 警告當 error 的 case(30 分鐘)

`r6_inspection` T6、`r10_smoke` S11。dev bundle 本來就會發 React 警告
(antd `Drawer visible` deprecated 是另一條),把它們計入 error 從設計上就不可能
穩定。要嘛過濾已知的,要嘛只在 prod build 上斷言。

順帶:那條 `React does not recognize the %s prop` 我今天**沒有溯源成功**
(webctld 記的是未格式化字串,注入 `console.error` 又活不過觸發 mount 的 reload)。
能確定不是 `data-*` hooks。

### C3. 板子在線才能測的 WebUI 路徑(30 分鐘)

`hist_wiring.mjs` 今天已過。還沒測的:uInsp 面板的即時計數、統計歷史的清空與
匯出、`cat_ok`/`cat_ng` 改動後 UI 是否跟上。

---

## 5. 不要在今天做的

- **相機相關**:`dv_bench`、`rc_hammer`、`soak`、`qwatch`、真實檢測。沒有相機。
- **完整檢測鏈路**:已證實走不到(CAM_CLOCK_CAL_FAILED,見 §0)。不要再試,
  也不要把 `phantom_feed` / `pulse_load` / `qwatch` 排進今天 —— 它們的計數
  永遠是 0。
- **轉盤實測**:任何要 `plate_freq_meas > 0` 的東西。
- **`InspectionCore/test_suite/`**:四層阻塞,最硬的是 10221 golden 不在 repo。
  見 AUDIT_BACKLOG_2026-08-18 那條。
- **燒 NVS**:`save_setup` / `clear_saved_setup`。現在的 NVS 是好的
  (`cfg_crc: 562041882`),弄壞了要重設 `io_on_level`(這台是 active-low)。

---

## 6. 收尾(30 分鐘)

- 重跑起手式的三項,和基準線比對
- 確認板子:SYSTIME 單調、`machine_id` 未變、`io_on_level` 全 0、`cfg_from_nvs: true`
- 新發現的坑追加到對應的 `*_CAVEATS.md`(按歸屬,不要全塞 Core 那份)
- 數字寫進 `REGRESSION_TESTS.md`,不要只寫「有測過」

---

## 附:今天已經確立的基準

| | 數字 | 來源 |
|---|---|---|
| `suite_nohw` | 12 pass / 1 skip / 1 fail,共 14 | 板子接上後 |
| `qa/run.mjs` | 35 pass / 0 skip / 4 fail / 187s | `e3311d09` 之後 |
| `--insp` leaf diff | 959 leaves,0 diff | 今天所有 core 改動之後仍相同 |
| P1 host fuzz | 400 trials × 6000 bytes,0 越界 | `ad02cbe9` |
| P1 真硬體 | 4 種形狀,SYSTIME 單調無重啟 | 燒錄後 |
| console 透傳 | `get_running_stat` 2513 bytes 單行,keys 到 `cam_sync` | `49db1c88` |
| headless rig 上限 | 100 → 102 → **112**,error 14 CAM_CLOCK_CAL_FAILED | 無相機的必然結果 |
