# 桌面整合驗證 — 2026-08-17（新 CAM1 接線 + Hikrobot USB 直插）

一次性量測記錄（dated file 慣例：數字屬於當天的硬體與設定，不自動保鮮）。
環境：mac bench、板子 USB 序列直插（`/dev/cu.usbserial-0001`）、Hikrobot
MV-CA050-11UM USB3、**無步進出力**（盤不會物理轉）、無零件。
接線：相機觸發線接**正規 CAM1 pin（GPIO 17）**——不再是 2026-08 初代 bring-up
的「觸發騎背光線 L1A」；背光仍在 lpin 16。

## 結論表

| 鏈段 | 判定 | 證據 |
|---|---|---|
| 序列鏈路 / CONNECT / 心跳 / comm_lost_backup | ✅ | CONN_ID 分配、PONG 流、backup 由 ping thread 延遲武裝 |
| perif 門鈴（0xCA12） | ✅（修了 2 個 bug，見下） | doorbell.mjs 三相 PASS（真板在線） |
| NVS 設定完整性 | ✅ | `get_setup`：SEL1_on=30000/off=30800、io_on_level 全 0、cfg_crc 在 |
| 相機 USB / 軟觸發 | ✅ | 直連 Aravis 軟觸發 5/5；core II snap 有 RP |
| **CAM1 pin → Line0 硬體觸發** | ✅ | 10 pulses → 10 RP（經 core FI 全鏈） |
| 觸發速率（全幅） | ✅ | 2/5/10/15/20 每秒 ×40：**RP 40/40 每檔零掉發** |
| 觸發速率（產線 ROI 2448×512） | ✅ | 25/s、35/s ×100：**RP 100/100** |
| 背光相位 | ✅ | light_delay 掃描：0-300µs 亮（mean 229）、≥500µs 暗（~99）@200µs 曝光 |
| 邏輯盤時基（無馬達） | ✅ | meas 1150→7270→13305→15000 穩住（102→103→101） |
| 完整迴圈 gate→CAM1→frame→verdict→路由 | ✅ | 60 phantom @5/s：verdict_in +119、NA +141、cam_sync learned 8 |
| 34/s 完整迴圈 soak 60s | ✅ 穩定（吞吐受板端語意節流，見下） | 無新錯誤、鏈路零 tx_fail、收盤乾淨 |
| 真零件全鏈延遲 / 真分選 | ⏳ 需放零件 | `static_part_profile.py` / `soak_verdicts.py` |

## bench 啟動配方

```sh
cd InspectionCore/Core0_1
INSP_CAM_TRIGMODE_ONCE=1 INSP_PERIF_CONSOLE=4099 ../build/mac-arm64/visSele
```

- `INSP_CAM_TRIGMODE_ONCE=1` **必要**：全幅或大 ROI 下重複套用 TriggerMode
  （例如 `!fi` 注兩次）會讓相機死到 DeviceReset（CameraLayer_Aravis 2026-08-11
  註解），症狀是「硬體觸發 0 frame、軟觸發正常」，看起來像接線問題。
- 產線裁切：`data/default_camera_setting.json` 的 `"ROI":[0,416,2448,512]`
  （蓋住照亮帶 y≈428-880）。inspection_region / clean_regions 是全感測器座標，
  core 的 sampler origin offset 自動補償，不需要改 def 或 machine_setting。
- 板子進檢模式的乾淨序列：`clear_error` → `set_setup {"plate":{"freq":15000}}`
  （**grouped key**，flat `plate_freq` 會被 `unknown_keys` 拒收）→
  `enter_insp_mode`。序列中夾到任何被拒的命令（如 freq≠0 時 stepper_disable）
  會讓 meas 卡 0 —— 這正是本日一度誤判「桌面沒時基」的原因。

## 修掉的 bug（都已 push）

1. **perif 門鈴 1s 取樣看不到快速 DISCONNECT→重連**：PD CONNECT/DISCONNECT
   handler 改為轉變當下同步推門鈴，取樣器降為 safety net（`9d65a28e`）。
2. **不帶 CONN_ID 的 DISCONNECT 從未成功**：`-1` 萬用分支是死碼，被前面守衛
   先擋（同 commit 修正）。

## 三條假線索（花掉最多時間的部分，記下來防重踩）

1. `!fi` 注入兩次 → 全幅 repeat-TriggerMode 殺相機 → 0 frame ≠ 接線問題。
2. `cam_grab.py lines` 的 50ms 輪詢看不到 3.3ms 脈衝；閒置讀 False（0x4）只說
   明閒置電平低於門檻，不代表脈衝進不去。**接線驗證的正解：打脈衝、數 frame**
   （`trig_cam_pulse` ×N vs core RP 計數）。
3. 這版韌體 `PIN_ON`/`PIN_MODE` 沒 ack、默默忽略——靜態拉線測試等於沒測。
   直連 Aravis 矩陣測得 RisingEdge 2/3 也是測試框架 drain 假象：實鏈 20/s、
   35/s 都零掉發，3.3V 電平疑慮銷案。

## 曝光窗量測（direct Aravis，200µs 曝光 / gain 0 / 光窗 600µs）

`trig_cam_pulse` 波形：CAM 拉起 → light_delay → 光亮 light_duration → 全關。
掃 light_delay：0/100/200/300µs → mean ~229（亮）；500µs 起 → mean ~99。
亮暗轉折在 300-500µs 之間。注意 `expose_window.py` 是舊接線寫的（用
trig_cam_burst 打光線當觸發），新接線下不能直接用——要移植到
`trig_cam_pulse`（TODO）。

## 34/s 迴圈 soak 的判讀

注入 2040 phantom @34/s（間距 29.4ms，貼著 min_detect_sep 28.571ms）：
gate 收 530（74% 被最小間距擋掉——序列注入抖動貼線，是防重複偵測正常工作）、
CAM1 實發 115、verdict 67、NA 89、無新錯誤。**ad-hoc 塞爆注入不是吞吐量測**：
注入率→件流的映射由 gate 間距與管線容量控制，量吞吐要用 `soak_sched.py`
（正確餵件紀律）/`burst_pairing.py`。`err_hist [1]` 在這類 run 後出現＝年輕
配對模型配不上的報告，已知良性。

## 對照工具

- `UI/WebUI/tools/webctl/fi_watch.mjs <secs>`：訂閱 4090 數 RP/IM——本日所有
  「打脈衝數 frame」測試的 core 側計數器。
- `UI/WebUI/tools/webctl/rc_once.mjs`：發一次 RC camera_ez_reconnect。
- `UI/WebUI/tools/webctl/ii_snap.mjs`：II 軟觸發抓一張，驗相機不驗接線。
- 板側都走 console 4099（`peek.py`、`get_running_stat` 前後差值——count 是
  NVS 壽命計數，絕對值無意義）。
