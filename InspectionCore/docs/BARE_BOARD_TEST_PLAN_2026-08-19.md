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

**已閉合**:`INSP_CAM_TS_SYNTH=1` 讓 core 直接回答板子的 trigger,校正因此
收斂 —— `state=101 (READY)`、`valid=true`、`offset_us` **精確等於**
設定的 `INSP_CAM_TS_OFFSET_US`(5000 和 800 各驗過一次)、`resid_us=0`。
在此之前每次都是跑 16 秒 CAL 然後掉進 112。

**還沒閉合**:零件通過 gate 進了 pipe(`registered`/`waiting` 會增加),
但 `heading` 全 0、SEL 計數不動。根因**不是**配對邏輯 —— 直接觀測發現:

```
餵 6 個 phantom  ->  cam_trig announcements: 0
```

**phantom 零件從來不觸發 CAM1**,所以沒有 cam_trig 宣告,core 沒有東西可回答,
物件就永遠停在 waiting。sync pulse 能成功是因為它走另一條路、自己會發 cam_trig
—— 那正好解釋了為什麼校正收斂而零件不動。

已排除的假設(不要再花時間):匹配窗(5000us 與 800us 行為完全相同)、物件註冊
時機(物件在 gate 時就進 RBuf,到 CAM1 才填 `cam_us`)、`t_us` 欄位缺失
(兩個 cam_trig 送出點都有帶)。

**2026-08-19 續測,把間隔拉到 8 秒之後:**

```
gate    {"in": 4, "out": 4, "pct": 100, "loss": "none"}   <- 4/4 全過
verdict {"in": 4, "out": 0, "unanswered": 0}
count   全部 0
```

距離過濾的假設**完全證實** —— 8 秒間隔下 gate 接受率 100%,先前的 0% 純粹是
盤沒動。校正同時維持(`state=101`、`valid=true`、`offset_us=800`)。

所以現在斷點很明確且只剩一個:**零件進得了 verdict 階段,但拿不到判定**。
`verdict in=4 / out=0`,而 `unanswered` 是 0 —— 板子不認為它們被漏答,
是還在等。core 那邊 `INSP_CAM_TS_SYNTH` 對每個 cam_trig 都回 report,sync
pulse 明顯配得上(校正就是從那學的),所以格式和傳輸都沒問題,差別在一般零件的
配對條件。

**2026-08-19 最終:迴圈閉合了,而擋住它的一直是我設錯的轉速。**

```
plate.freq = 15000        <- production 值,來自原始碼註解
state=101  freq_meas=15012  cam_sync valid=true offset_us=800
verdict {"in": 2, "out": 2, "pct": 100}
count   {"NA": 2}
```

零件走完整條路:gate → 註冊 → CAM1 觸發 → core 收到 `cam_trig` → 合成
report 回去 → **板子拿到判定並計數**。判定是 `NA` 而不是 SEL1/SEL3,因為
core 沒有真的檢驗 —— 那正是 `INSP_CAM_TS_SYNTH` 應有的行為。

## CAMSYNC 漂移估計:對著已知斜率量,這是有相機時做不到的

真機上 `cam_ts` 來自相機晶振、`cam_us` 來自板子晶振,兩者真實關係
沒人知道,所以估計器吐出的每個數字都只能照單全收 —— 「~83μs/s」這個數字本身
就是被測物產生的。

`INSP_CAM_TS_MULT` 讓 `cam_ts = t_us * m + offset`,只有一個時鐘,
真值變成算術:offset 在 T0 學成 `T0(m-1)`,之後殘差是 `(T-T0)(m-1)` ——
對學到的 offset 而言是貨真價實的斜率,不是階躍。所以 `slope_ppb` 有標準答案。

**工具**:`bareboard_up.mjs`(冷機 → 101)、`camsync_drift.mjs`(精度)、`camsync_lost.mjs`(停機路徑 A/B)。

### 三檔結果

| mult | 真值 | 收斂值 | 樣本 | 誤差 |
|---|---|---|---|---|
| 1.0 | 0 ppb | 0 ppb | 241 | 0 |
| 1.0000833 | 83300 ppb | 83398 | 181 | +0.1% |
| 1.0000833(重跑) | 83300 ppb | 83303 | 302 | +3 ppb |
| 1.0000833(清錯後再收斂) | 83300 ppb | 83675 | 121 | +0.5% |

三次獨立收斂,單調趨近,沒有一次偏離超過 0.5%。旁證:進 101 時
`offset_us=6289`,而 `t_us≈65.9s` 時 `65.9e6 × 83.3e-6 + 800 = 6289` —— 差 1μs。

### 漂移補償把 417μs 壓成 3μs

`resid_us` 和 `delta_last_us` 是不同的量,分清楚才看得懂:

- `resid_us` = 靜態 offset 下的誤差
- `delta_last_us` = `|pipe->cam_us - expectedCamUs(cam_ts)|`,**窗口實際檢查的量**,已含漂移補償

同一塊板、同一個時鐘、同樣流量(5s 間隔),只切 `cam:{drift_comp}`:

| | resid | delta | 結果 |
|---|---|---|---|
| A on | 417 μs | **3–4 μs** | 40s 全程 101,零拒絕 |
| B off | 417 μs | **416 μs** | t+10s 停機 |

140 倍,而且是「跑得動」與「停機」的分界。

### 沒有斜率就沒有補償 —— 清完錯誤的機器最脆弱

`expectedCamUs()` 的守衛是 `if(DRIFT_COMP && slope_n && est_cam_us)`。
`clear_error` 之後 `slope_n` 歸零,旗標還是 true 但**補償實際上是空的**,
要重新餵幾分鐘才長回來。第一次跑 A/B 就栽在這裡:對照組直接停機,看起來像
「補償沒用」,其實是對照組根本沒開成。`camsync_lost.mjs` 現在會擋。

這也是營運性質,不只是測試陷阱:**剛從錯誤恢復的機器,有一段時間完全沒有漂移補償。**

### `match_window_us` 有 200μs 地板

設 50 或 120 都會被夾成 200(`LegacyFirmware.cpp:9289`)。註解說明了理由:
*"A window narrower than the measurement noise (~50us observed) can never match
anything and would stop the machine forever."* 未補償誤差在 1s 間隔下只有 84μs,
在地板底下 —— 所以撬不動窗口,得改用**間隔**當槓桿(誤差隨 gap 線性成長,5s 給 417μs)。

---

## 發現:`CAM_CLOCK_LOST`(13)在運轉中的機器上不可達

三次實驗都停在 **error 1 = `INSP_RESULT_MATCHES_NO_OBJECT`**,`rejected=1`、
`rebuilds=0`,13 一次都沒出現。這不是巧合,是結構性的:

```
gate():634        拒絕條件  nearest_delta >  TOL_US
:6563   byTs 設定  nearestDelta <= TOL_US
```

同一個變數、同一個門檻、互補。所以 **gate() 拒絕的 frame 必然 `byTs==NULL`**。
READY 期間 `bySync` 也是 NULL(sync pulse 只在 CAL/RECAL 發),於是
`tarP = (byTs != NULL) ? byTs : bySync` 是 NULL,同一輪走到 `:6777`
raise error 1 停機 —— 而 `gate()` 在 `:6595`,`fault_pending`
檢查在 `:6597`,都在 error 1 之前,但 `consec_reject` 才剛變成 1。

**機器還是停了,安全性質沒破。** 壞掉的是另外兩件事:

1. **診斷。** 操作員看到「a verdict arrived for no known object」,而不是
   「camera clock lost」。真正的原因是時鐘,錯誤碼指向配對。
   `CAMSYNC LOST` 那行 dbg_printf 也永遠不會印。

2. **遲滯。** `LOST_N=2` 的註解寫著 *"one is a lost frame or a stray, two in
   a row is the clock, and there is nothing to be gained by letting a machine
   that cannot place its frames keep sorting parts."* 這個「容忍單一雜訊 frame」
   的設計意圖從來沒有生效過 —— 第一個出界的 frame 就停機了。

這同時回答了「實際使用上沒有 error 1 過」那個疑問的反面:**一旦時鐘真的漂出窗口,
你會看到的是 error 1,永遠不會是 13。**

未處理,因為這是 firmware 行為改動而不是測試問題。要修的話最小改法是在
`:6777` 之前檢查「這個 frame 是否被 gate 以出界為由拒絕」,是的話讓
`consec_reject` 累積而不是立刻停 —— 但那等於放寬「單一無主 verdict 就停機」,
是個政策決定,不該由測試順手改掉。

---

### 迴圈完全閉合:60/60,零損失

```
gate    {"in": 60, "out": 60, "pct": 100, "loss": "none"}
verdict {"in": 60, "out": 60, "pct": 100, "unanswered": 0}
count   {"SEL3": 60}        <- cat_ok 的料道,分料決策也走到了
error_hist []
```

60 顆零件 18 秒內跑完 gate → CAM1 → cam_trig → 合成 report → 配對 → 計數,
**沒有相機、沒有轉盤**。

### 擋住它的是我對兩站宣告都回覆

板子在報 error 之前就印了完整診斷,一直都有,是我沒去看:

```
NOMATCH state=101 tid=1 cam_ts=... valid=1 nearest=1 nd=6608
        rb_real=1 rb_sync=0 syncOut=0
```

`rb_sync=0`、`syncOut=0` 一次否決了「校正脈衝造成孤兒判定」那整套推論。
`nearest=1 nd=6608` 說配對沒壞,只差 6.6ms,而 `TOL_US=5000`。

原因在擷取到的宣告裡:

```
cam_trig tid=1 cam=2 t_us=98673515 gate_pulse=482119
cam_trig tid=1 cam=1 t_us=98680146 gate_pulse=482119
```

**同一顆零件、同一個 gate_pulse、兩站、相差 6631 μs。** core 自己的 pairing log
一直寫著「the firmware announces every object on both 1 and 2」,而
`stage_pulse_offset` 的 CAM1_on 9515 與 CAM2_on 9317 差 198 steps,在
30000 steps/s 下正是 6600 μs。板子只記一站的 `cam_us`,所以兩站都回,
必有一個永遠配不上 —— **不管什麼時候送**。

修法是照 conn_info 裡的 `cam_idx` 過濾:一台相機、一路 frame、一個 report。

### 死掉的假設(別再跑一次)

| 假設 | 怎麼被否決的 |
|---|---|
| 回覆太快,sync 還沒被消費 | 0ms 與 8ms 延遲的 `nd` 完全相同(6608) |
| 校正脈衝的回覆晚到、tombstone 溢出 | `rb_sync=0`,從頭到尾沒有 sync 涉入 |
| core 該認得 CAL_BIT 的 tid | 板子早就不用 tid 配對了 —— 「with the voting scheme gone the tid lookup went too」 |
| 該給合成回覆加速率上限 | core 就是要忠實回覆每一個 frame,速率不是 core 的事 |

**驗收標準要用「量化誤差等級」而不是「小於 TOL_US」。** `cam_ts` 是從板子
自己的 `t_us` 算的,`pipe->cam_us` 也是同一個 `time_us`,所以
`nd` 理論上就該是 0。用 5000 當及格線的話,6608 修成 3000 也會被誤判成修好。

---

### 迴圈會閉合,但回覆太快會讓板子停機

兩次獨立測試,**真正進到 verdict 的零件 100% 拿到判定**:

```
verdict {"in": 1, "out": 1, "pct": 100}    count {"NA": 1}
verdict {"in": 2, "out": 2, "pct": 100}    count {"NA": 2}
```

停機的不是它們,是**多餘的判定**:`error_hist [1]` =
`INSP_RESULT_MATCHES_NO_OBJECT`「a verdict arrived for no known object」。

孤兒判定只能來自 sync pulse。被 `blocked` 擋掉的零件在
`LegacyFirmware.cpp:2871` 就 return,**根本不會建立物件、不會發 cam_trig**,
所以它們不是來源。機制在 `6565`:

```cpp
if(syncOutstanding!=1) bySync=NULL;      // 同時有多個 sync 待處理 -> 配不到
```

`INSP_CAM_TS_SYNTH` 目前在 **RX 路徑上同步回覆**,幾乎零延遲。真實 core
要等曝光加影像處理(acquisition leg 量過約 8ms),所以 sync pulse 之間有足夠間
隔;合成回覆沒有,前一個還沒被消費下一個就到,`syncOutstanding` 就大於 1。

**證據是速率相關的**:8 秒間隔餵 4 顆全程無誤;1 秒間隔餵到第 4 顆就 error 1。

下一步(尚未實作):給合成回覆加一個延遲,讓它像真的一樣。不能在 RX 路徑上 sleep
——那會卡住 perif RX——所以需要一個小 queue 加一條送出執行緒,或掛在既有的
perif send thread 上。

**已排除**:對 sync pulse 不回覆(試過,`e4c19f8c` 已回退)。CAL 的
`cam_ts` 就是從那個回覆來的,擋掉它校正立刻失效
(`valid=false`、`state 112`)。

---

### 轉速:別再設錯

`plate.freq` 的單位不是轉/秒。原始碼註解給了 production 值:

```
LegacyFirmware.cpp:1218   600us of light at production plate_freq 15000
LegacyFirmware.cpp:1055   16 kHz at plate_freq 8000      <- tick = 2 x plate_freq
```

| plate.freq | steps/s | 轉速 | 邊緣速度 | 距離門檻 159 steps |
|---|---|---|---|---|
| **15000**(production) | 30,000 | 25.6 RPM | 321 mm/s | **5.3 ms** |
| 12(我一開始設的) | 24 | 0.02 RPM | 0.26 mm/s | 6.6 秒 |

我設 12 等於**把盤設成每 50 分鐘轉一圈**。先前所有「距離過濾擋掉全部零件」、
「phantom 不觸發 CAM1」、「要 6.4 秒才能餵一顆」的結論,原因**全部只是這個**。
實測 ~25 steps/s 和 12 的理論值 24 完全吻合,那個數字本來就該讓我起疑。

現在 20 顆 200ms 間隔:`gate in=20 out=2, loss="blocked"` —— 距離不再是
瓶頸(換成 blocked,那是 pipeline 深度),而通過的兩顆 100% 拿到判定。`stage_pulse_offset.CAM1_on=9515`
要求盤子轉到那個 pulse 位置,而 `plate_freq_meas` 有讀數(14.33)表示 counter
在走。若 phantom 本來就不設計成會觸發相機,那就改用會觸發的路徑
(`trig_cam_pulse` / `trig_cam_burst`)或讓 phantom 也走完 CAM1 階段。

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
