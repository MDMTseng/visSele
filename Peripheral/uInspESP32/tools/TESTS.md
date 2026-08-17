# uInsp 測試工具目錄

`README.md` 講的是 **不經 core** 的韌體直測（`uinsp_test.py`、`uinsp_panel.py`）。
這份講的是另外那一半：**接著 core 跑的整機工具**。它們全部透過 core 的
`INSP_PERIF_CONSOLE` port 對裝置說話，因為 **core 獨佔序列埠**，機器在跑的時候
沒有第二條路可以碰到裝置。

先讀「共用前置」和「共用陷阱」兩節。**那些陷阱每一條都真的害過人**，而且大多數
不會報錯，只會給你一個看起來合理的錯數字。

---

## 共用前置

```sh
cd InspectionCore/Core0_1
INSP_PERIF_CONSOLE=4099 ../build/mac-arm64/visSele
```

沒有這個環境變數，core 就不開 4099，所有工具都連不上（症狀是
`ConnectionRefusedError`，不是任何有意義的錯誤訊息）。

console 上的兩種語法：

| 送什麼 | 去哪裡 |
|---|---|
| `{"type":"get_running_stat"}` | 轉發給**裝置**（ESP32） |
| `!fi {...}` / `!pd {...}` | 注入一個 BPG 封包給 **core**，兩個字母是封包型別 |
| `?lat` | core 自己的分段延遲表（不經 BPG，直接印文字） |

裝置的連線資訊在 `data/machine_setting.json` 的 `uInspESP32_peripheral_conn_info`，
`real_parts.py` 匯出成 `CONN`，其他工具都 `from real_parts import CONN`。

---

## 共用陷阱

**1. console 只保留一個 client。** 第二條連線一接上，core 就把第一條**無聲關掉**。
一個程式裡開兩個連線 = 回覆隨機落在其中一條。（寫 `static_part_profile.py` 時踩到。）

**2. `!fi` 在 core 剛啟動時會把 core 打死。** console 在約 2 秒就開始收命令，但
`bpg_pi.camera` 要約 20 秒後才被指派，而 FI 處理器對 `camera->TriggerMode()`
**沒有 null check** → SIGSEGV。可穩定重現。等 30 秒再送，或用
`static_part_profile.py --core-wait`。

**3. `!ld` 不會載入 def。** 它讀檔、包成一個 `"FL"` 封包，而**整個 core 沒有任何
`FL` 處理器**——封包沒人接，引擎維持空的，然後它回你 `{"core":"LD injected"}`。
要載 def 只能用 `!fi {"deffile":...}`。（這個害整晚的延遲量測全部量在沒有檢驗的
空管線上：`match` 0.004ms、每顆 NA。）

**4. `get_running_stat` 已經在溢位邊緣。** 跑 30 秒後序列化到 2886/3072 bytes，
而計數器只會變大。撐爆時**不報錯，是安靜地掉欄位**，整包變成截斷的 JSON。長跑之後
讀不到 `free_heap` 就是這個。新欄位請另開命令（`get_spikes` / `get_schema`）。

**5. `reset_running_stat` 會摧毀時鐘模型。** 它連帶呼叫 `CAM_SYNC.reset()`，代價是
約 10 秒配不上的報告（會看到 state 112 / err [1]）。**要清延遲計數器請用
`reset_latency_stat`**，它只碰延遲相關的量。任何用 `reset_running_stat` 做的 A/B
都是無效的——第二臂的 max 只可能往上爬。

**6. 開序列埠會重開機。** 送 `!pd {"type":"CONNECT",...}` 會重開 UART，DTR/RTS 一動
ESP32 就重置——所以每次 CONNECT 之後裝置的 RAM 設定（含 `stage_pulse_offset` 的
臨時改動）都回到 NVS 值。`peek.py` 存在就是為了**不重置**地問問題。

**7. core 在 perif DISCONNECT 之後沒有關掉 tty fd。** 要燒錄韌體必須整個重啟 core，
光送 DISCONNECT 不夠。

**8c. 全幅 ROI 的 bench 上，core 要帶 `INSP_CAM_TRIGMODE_ONCE=1` 啟動。**（2026-08-17）
`!fi` 注入兩次＝重複套用 TriggerMode，全幅時相機會死到 DeviceReset（CameraLayer_Aravis
的 2026-08-11 註解量過）——症狀是硬體觸發 0 frame、軟觸發卻正常，看起來像接線問題。
另外：`cam_grab.py lines` 的 50ms 輪詢看不到 ms 級脈衝、閒置讀 False 不代表脈衝進不去；
這版韌體 `PIN_ON`/`PIN_MODE` 沒 ack（默默忽略），靜態電平測試等於沒測。
接線驗證的正解是「打脈衝、數 frame」：trig_cam_pulse × N vs 相機收到的 frame 數。

**8a. `uinsp_panel.py` 在這台 bench 上要 `--http-port 8766`。** 預設 8765 被
webctld（WebUI 測試的瀏覽器控制服務）佔走，panel 直接 Address-in-use 死掉。
另外 panel 和 core 對序列埠互斥（陷阱 7：core 連 DISCONNECT 都不放 tty）——
開 panel 前先停 core，兩邊同開會互相打碎 frame。

**8b. 「桌面時基不走」是假的——序列被污染才會 meas=0。**（2026-08-17 兩度驗證後
翻案）乾淨序列 `clear_error → set_setup plate.freq → enter_insp_mode` 在桌上板
（無步進出力）一樣讓 SYS_STEP_COUNT 邏輯轉起來：meas 1150→7270→13305→15000
穩住（102→103→101）。先前 meas=0 的 run 中間都夾過失敗命令（flat-key set_setup、
freq≠0 時的 stepper_disable 被拒、殘留 error state）。完整迴圈在桌面可跑：
60 顆 phantom @5/s → gate_in +402（NA 迴圈再入）→ stage 排程打 CAM1 → 相機硬體
觸發 frame → core FI 判定 → verdict_in +119、NA +141、cam_sync learned 8。
另注意 `count{}`/`gate.out` 這類計數是 NVS 壽命值，跨重啟累積——判斷單次 run
要用前後差值，別像 dryrun 的 `judged=` 那樣讀絕對值。

**8. log ring 跨 core 重啟累積。** dump 裡讀到的那幾行可能屬於上一次的 run。
`?lat` 的直方圖同理是**自 core 啟動累積**的——比較兩個條件要用前後差值，或重啟 core。

---

## 我該用哪一支？

| 你的問題 | 用這支 |
|---|---|
| 檢驗要花多久？整條鏈的時間花在哪？ | **`static_part_profile.py`** |
| 機器能不能連續跑幾小時不停機？ | `soak_sched.py`（有主題佇列）/ `soak_real.py` |
| 判定會不會落在錯的顆上？ | **`slip_probe.py`** |
| 真零件、真感測、真分選跑得動嗎？ | `real_parts.py` / `soak_verdicts.py` |
| 配對在什麼條件下會壞？ | `jitter_sweep.py`（掃雜訊）/ `burst_pairing.py`（掃丟幀） |
| 相機為什麼有時候起不來？ | `cam_flake.py` |
| 曝光窗到底落在觸發的哪裡？ | `expose_window.py` |
| 盤速跟得上進料嗎？ | `rate_hold.py` / `rate_ctl.py` / `speed_soak.py` |
| 設定鍵在四個地方有沒有對齊？ | `check_cfg_keys.py` |
| 不想重開機，只想問一下狀態 | `peek.py` |
| 手動下一條命令 | `perif_console.py` |

---

## 整機時序 / 延遲

### `static_part_profile.py` ★ 量檢驗延遲就用這支

一顆真零件用 jog 停在相機下，`stepper_disable` 讓盤子**實體不動、邏輯照轉**，再用
`virt_pulse` 灌虛擬物件。線上流量跟產線一模一樣，而**每一幀都有真零件**。

```sh
python3 static_part_profile.py                   # jog 抓件，60s @ 30/s
python3 static_part_profile.py --no-jog          # 零件已在位
python3 static_part_profile.py --seconds 300 --rate 35
```

輸出三段分解，並印 `acquisition + core e2e vs 實測 cam_lat` 的自洽檢查——
**偏差大就代表這輪不可信**。

參考值（2026-08-13，MV-CA050-11UM，test1.hydef，30/s，1284 顆全數判定）：

```
cam_lat      觸發 → 判定到手     avg 20.33ms  max 52.98ms
acquisition  觸發 → 影格到 core  avg 11.82ms  max 18.84ms   100% 在 10-20ms
core e2e     影格 → 報告寫出     avg  8.47ms  max 39.31ms
  └ match                       avg  6.64ms  max 30.02ms
rej_busy 0, no_object 0, multi_object 0, UNANSWERED 0
```

**看不到的**：SEL 致動被 `!SYS_STEPPER_DISABLED` 擋掉，所以 `SEL1/2/3=0` 是預期。
零件姿態不變，真機每顆姿態不同會更貴（轉盤時 `match` 8.5ms）——**這是下限不是典型值**。

`match` 在兩次 jog 之間差 45%（零件停的位置不同）。**`acquisition` 三輪 11.55 /
11.69 / 11.82ms 完全不動**，不受盤速、速率、位置影響——要追長期趨勢就追這個。

### `?lat`（core 端，不是獨立腳本）

在 console 上送 `?lat`，印出 core 的分段表（`queue / match / match_cpu / rep_json /
insp_off / inspect / wait / write / e2e` + 引擎自己的 stage 拆解，牆鐘與行程 CPU
並排）加 `e2e` 的桶，以及判定路徑計數（`judged / no_object / multi_object /
no_report / wrong_type / no_reports / no_labeled`）。

**`match_cpu` 遠大於 `match`（實測 3-4 倍）代表工作散在多核上跑，不是被擋住。**
兩者都要看——只看牆鐘分不出「被阻塞」和「在別的執行緒上跑」。

---

## 長時間 soak

| 工具 | 做什麼 |
|---|---|
| `soak_sched.py` | soak 佇列，**每個主題帶一個會失敗的預測**。跑幾小時看不到問題證明不了什麼——這支要求你先講會發生什麼 |
| `soak_real.py` | 長時間真零件 soak，一次回答殘差分佈等三個問題 |
| `soak_verdicts.py` | A1：真判定的 run。8 小時 394k 顆全 NA 那次的成因不是檢驗，是那顆 core |
| `soak_pairing.py` | 靜盤 rig 上的負載 vs 品質掃描 |
| `speed_soak.py` | 盤速連續變動的長 soak，重點是**夠多次不同的變速**，不是單一工作點 |
| `flatten_soak.py` | 把 soak 的 JSONL 攤成寬 CSV + 事件表，給分析用 |
| `regress_watch.py` | 過夜盯場：2026-08-06 修掉的那些有沒有回來 |

---

## 配對正確性

| 工具 | 做什麼 |
|---|---|
| `slip_probe.py` ★ | **判定有沒有落在錯的顆上。** 其他檢查都只回答比較弱的問題——`agree`/`disagree` 是兩種配對互比，其中一個啞掉就整個安靜 |
| `jitter_sweep.py` | 掃**雜訊**而不是負載，找配對真正的斷點 |
| `burst_pairing.py` | 隨機爆發，製造丟幀——那才是能區分時間戳配對和位置配對的形狀 |
| `dryrun_pairing.py` | 單次 dry-run 試驗，從硬重置的板子開始 |
| `edge_sweep.sh` | 一次一個確定性故障，把配對走到邊界。**過載是錯的儀器**——它本來就該停機 |

---

## 相機 / 光學

| 工具 | 做什麼 |
|---|---|
| `cam_flake.py` | 相機多久拒絕一次 `AcquisitionStart`，什麼能改變機率 |
| `expose_window.py` | 曝光窗相對觸發落在哪。時間戳配對**假設**影像屬於觸發當下那顆 |
| `cam_grab.py` | 用 Aravis 從 Python 抓幀 |
| `uinsp_vision_loop.py` | 閉整圈：gate → 光/觸發 → 影像 → 判定 |

**背光時序不能亂調。** 實測只有生產設定 `light_delay 50 / duration 350 µs` 能定位到
零件；`100/1000`、`300/3000`、`500/15000` 三組**全部 20/20 `no_object`**。
加長照明不是浪費，是直接讓畫面不可用。

---

## 速率 / 吞吐

| 工具 | 做什麼 |
|---|---|
| `rate_hold.py` | 用盤速把物件速率壓在目標上（估計器 + 除法） |
| `rate_ctl.py` | 在主機上閉速度迴路，先確認機器**會跟隨**再談要不要做進韌體 |
| `speed_soak.py` | 見上 |

**`rej_busy` 不是吞吐上限**，它是：

```
rej_busy > 0  ⟺  進料速率 × (SWITCH_offset ÷ 2×plate_freq) > PIPE_INFO_LEN (100)
```

同時在飛的物件超過環的 100 格。實測 plate 3000 時 30/s → 在飛 149 顆 → 拒掉 31%；
plate 10000 → 在飛 45 顆 → `rej_busy` 0。看到它要調的是**盤速或進料**，不是檢驗。

---

## 設定 / 雜項

| 工具 | 做什麼 |
|---|---|
| `check_cfg_keys.py` | 把設定文件的鍵表跟定義它的韌體對照——同一份知識有四份拷貝 |
| `uinsp_cfg.py` | 扁平↔分組鍵映射的**唯一真相來源**（`plate`/`gate`/`cam`/`skip_policy`） |
| `peek.py` | **不重置**地問裝置問題 |
| `board_query.py` | 用序列埠問板子——**這會重開機**，所以問錯誤歷史等於清掉它 |
| `perif_console.py` | console port 的極簡 client |
| `recal_leak.py` | 每次 RECAL 少 96 bytes 是漏還是暫時的 |
| `blow_stop_test.py` | 停機會不會截斷已經吹出去的氣——`SELn_Count` 在**開始**時就加了 |
| `fault_sel_test.py` | `SEL_SUPPRESSED` 端到端，數字全部事先已知 |

---

## 寫新工具時

- **匯入 `CONN`**：`from real_parts import CONN`，別自己抄一份連線設定
- **每條離開路徑都要把盤子停下、背光關掉**，包含 Ctrl-C 和早期失敗。腳本死了不是
  讓盤子繼續轉的理由
- **前後差值**，不要引用自 core 啟動累積的平均
- **印一個自洽檢查**（像 `static_part_profile.py` 的 `acquisition + e2e vs cam_lat`），
  讓讀的人不用讀完全部才發現這輪是壞的
- **把限制寫在檔案裡**，不要留給下一個人重新發現

## 斷線備份（comm-lost backup）怎麼測

要驗的是:主機死掉之後,SEL/NA 計數有沒有進 flash,並在板子被重開之後回來。

**先把儀器搞對,不然結論會是錯的。** `cnt_nvs_lat_ms` 量的是 `selHoldMs()`,
一個由吹氣寬度算出的固定值,所以每次存檔都回報同一個數字(這台是 55)。舊記錄和
新記錄長得一模一樣。**用 `cnt_nvs_seq`**——它每次存檔遞增,而且寫進記錄本身。

欄位在 `get_backup_stat`,不在 `get_running_stat`(後者已在溢位邊緣)。

```
{"type":"get_backup_stat"}
  -> cnt_restored / comm_lost_backup / host_timeout_ms
     cnt_nvs_seq / cnt_nvs_lat_ms / cnt_nvs_writes / cnt_nvs_fails
```

流程:

1. `get_backup_stat` 記下 `cnt_nvs_seq`,並確認 `comm_lost_backup` 是 **true**
   (核心的心跳約 2 秒武裝一次;開機時必定是 false)
2. 讓機器跑起來、把計數推上去
3. `kill -9` 掉核心
4. 等 20 秒,重新啟動核心
5. 再讀 `get_backup_stat`:`cnt_nvs_seq` 應該 **+1**,`cnt_restored` 應該 true,
   計數應該回到主機死亡當下的值

**第 4 步一定會重開板子**(開 port 拉 DTR/RTS,見 UINSP_CAVEATS),所以任何只存在
RAM 的證據——`error_hist`、`cnt_nvs_writes`——都會在這一步消失。這正是那個延遲數字
被存進 NVS 記錄裡的原因:否則它在唯一需要它的情境下不可觀測。

**記得測 RECAL。** 看門狗曾經只認 `READY`,而機器管線一空就會自己進 RECAL(104)——
在那裡殺主機,原本什麼都不會發生。輪詢 `get_running_stat` 的 `state` 直到讀到
104 再殺,不要假設它待在 101。用 `stepper_disable` 可以讓盤面只在邏輯上轉,
狀態機行為完全相同而工件不會被轉走。
