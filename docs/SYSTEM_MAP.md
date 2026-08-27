# visSele 三方系統大地圖（uInsp 板 × Core × WebUI）

> ⚠️ **2026-08-27 部分過時。** §2 邊界合約、§5 韌體、§7 不干擾矩陣仍然有效且最完整。
> **但**:§1 的埠位圖沒有 launcher 也沒有 4098 控制埠;§3 的行號**全部失效**
> (`wiringPanel.cpp` 已 12,945 行)。逐條對照見 `docs/SYSTEM_OVERVIEW.md` §8。

> 給新 agent 的 onboarding 主文件。最後全面校準：2026-08-18（三個測繪 agent 逐檔驗證 + 桌面整合實測）。
> 讀這份拿到「全局在哪、邊界合約是什麼、什麼不能碰」；各方內部細節由文末的權威文件表接手。
> 各 CAVEATS 檔是 append-only 陷阱帳本 — **grep 你的症狀，不要通讀**。

## 0. 這是什麼機器

旋轉盤高速分選機：零件從震動送料進轉盤 → gate 感測器偵測 → 背光下相機拍照 →
core 量測判定 OK/NG/NA → 板子在 SEL 工位吹氣分流。三方分工：

- **uInsp 板（ESP32）** `Peripheral/uInspESP32/` — 實時層：盤時基（ISR）、gate、
  相機觸發、frame↔件配對、判定執行（吹氣）。單檔韌體 `src/app/LegacyFirmware.cpp`（~9.6k 行）。
- **Core（visSele，C++）** `InspectionCore/Core0_1/` — 量測層：相機取像、
  MatchingEngine 量測、判定產生、對 WebUI 的 BPG/WS 服務、對板子的序列通道。
  樞紐檔 `wiringPanel.cpp`（~10.8k 行）。
- **WebUI（React+Redux）** `UI/WebUI/` — 操作層：def 編輯器、產檢畫面、校正、
  報表；一切經 BPG-over-WebSocket 到 core。

平台：mac arm64 開發 bench（fake/real 相機皆可）；工廠部署 Windows（MinGW cross、
HikRobot SDK）。世代注意：`Core0_1`＝一代機（工廠部署中，回溯相容 opt-in）、
`CoreHub`＝二代；`uInspESP32_v2`＝停滯的重寫世代 — **都不要誤入**。

## 1. 拓撲與埠

```
                 ┌────────────────────────────┐
                 │  WebUI (React, vite :8081) │  ← webctld(Playwright) :8765
                 └────────────┬───────────────┘  ← uinsp_panel.py :8766（獨佔板子時）
             BPG over WS :4090 │   （log WS :4091 = inspd_log 子行程）
                 ┌────────────┴───────────────┐
                 │  core visSele (cwd=Core0_1)│  ← INSP_PERIF_CONSOLE :4099（慣例埠，env 給的）
                 └───────┬──────────┬─────────┘
        USB3Vision(Aravis│/HikRobot)│ serial 230400 8N1（DTR→EN：開埠=板子重開機）
                 ┌───────┴──┐   ┌───┴──────────────┐
                 │  相機     │   │ uInspESP32 板    │─ gate(27) 步進(22/23/13)
                 │ Line0←CAM1│   │ LegacyFirmware   │─ L1A(16) CAM1(17) L2A(18) CAM2(19)
                 └──────────┘   └──────────────────┘─ SEL1/2/3(25/26/32) FEEDER(21)
```

一台 bench 的完整佈局：core:4090/4091/4099、vite:8081、webctld:8765、panel:8766
（panel 與 core **互斥佔用序列埠**，開 panel 前停 core）。

## 2. 跨邊界合約（最重要的一章）

### 2.1 BPG over WebSocket（:4090）

封包：`TL[2] | prop[1] | pgID[BE16] | len[BE32] | payload(JSON)`，16MiB 上限。
每個請求以 `SS {start:false, cmd, ACK, errMsg}` 收尾。多 client：全收；第一個
成為 default_peer 並自動訂閱串流，其他用 SB 加入；**最後一個 peer 關閉 →
TriggerMode(1) + perif teardown**。core 在握手完成時主動推 HR（版本/建置來源，
pgID 0xFF）— WebUI 的 SPLASH 只靠這個離開。

| TL | 方向 | 意義 |
|---|---|---|
| HR | 雙向 | 握手/心跳；core 版 HR 是 SPLASH 的出口 |
| SB | in | 訂閱/退訂串流 `{"stream":bool}` |
| SV | in | 存檔（def、`__CACHE_IMG__`、machine_setting；tmp+fsync+rename） |
| FB / LB | in | 目錄瀏覽 / 二進位檔（縮圖）→ BL |
| GS | in | 讀值：camera_info、perif_pairing（link 健康計數）、佇列狀態、路徑 |
| LD | in | 讀檔回傳（FL/DF/IM）。**不會載 def 進引擎** — 要用 FI/`!fi` |
| II | in | 單張檢驗（編輯器 CHECK；neutral_bacpac、region 只吃請求的 work_region） |
| SF | in | 從 def 訓練 shape localizer |
| CI / FI | in | 開檢驗 session。CI=free-run、station region OFF（setup 用）；FI=硬體觸發、region 強制、判定送板。帶 deffile/definfo 才重載 def；不帶就沿用引擎現狀 |
| EX | in | 特徵萃取（featureDetect.json）→ SG |
| RC | in | 動作群：camera_ez_reconnect（3s 限流）、lens_calibrate、field_calib_*、bmp_carousel、calib_files_load、cam_doorbell_ping |
| SC | in | log_dump、exec（**要 INSP_ALLOW_EXEC=1**）、檔案存在檢查 |
| ST | in | 設定群：InspAreaBypass、InspectionParam、CameraSetting、MachineSetting、串流 FPS/JPEG…|
| PD | in | 周邊通道：CONNECT / DISCONNECT / MESSAGE（轉發 JSON 給板子） |
| SS/RP/IM/DF/FL/SG/BL/PD/GS | out | session 框/報告/影像/檔案/簽章/…；每 frame 批 = SS→RP→IM→SS |

**門鈴**（推播，省輪詢）：camera_state 走 pgID **0xCA11**、perif_state 走
**0xCA12** — 事件驅動（PD/RC handler 轉變當下推）＋ 1s 取樣器 safety net。
WebUI 收到就 poke 對應的 single-flight 查詢鏈。

### 2.2 PD 周邊通道與序列

- WebUI 用固定 pgID 開通道：**10025 SLID、10026 CNC、10027 uInspESP32**（
  `_PGID_`/`_PGINFO_:{keep:true}` 釘住 reqWindow entry）。
- **PD CONNECT = 開序列埠 = DTR 拉 EN = 板子重開機**。RAM 設定回 NVS 值、
  時鐘模型歸零。相同 conn 資訊視為 client 重連（不會重建 pipeline）。
- core 的 ping thread 每 ~2s 心跳，第 20 拍夾帶 `comm_lost_backup` 重武裝
  （板子重開機後自癒）。**DISCONNECT 後 core 不會關 tty fd** — 要燒韌體必須整個
  重啟 core。一個壞 frame → 板子 err 11 鎖死到 `RESET`。
- 判定回送格式：`{"type":"report","tid":-1,"cat","cam_ts","pcnt","hus"}` —
  **tid 恆 -1，配對是板子的事**（2026-08-12 起 core 端配對函式已淘空）。

### 2.3 配對（frame ↔ 件）— 板子擁有

- **CAM_SYNC**（時戳）：板子在 CAM1 邊緣記 `cam_us`，相機 frame 帶 `cam_ts`，
  core 報告回傳 `cam_ts`；offset 由 8 顆校正脈衝學得（一般報告不教 — 循環論證），
  漂移補償 slope 常開。窗外連兩次 → CAM_CLOCK_LOST(13) **停機不猜**。
- **CAM_PCNT**（計數）：**已淘汰，不是備援也不是第二意見。** 程式碼還在(`report_match_pcnt`
  可開、兩法同開不一致 → err 16),但設計上不再使用,實機 `report_match_pcnt=false`。
  三個理由,都量過:(1) 超過相機幀率下限時它**不是盲,是自信地給錯答案**——相機用自己的
  節奏出圖而 `ExtTriggerCount` 仍約 1:1 前進,每張離它被標記的那一發再滑 ~420µs,約每
  12 張繞完一圈;用每發變動的 PRBS 背光裁決:150Hz 時 104/104 正確,200Hz 時 53/92(＝亂猜)。
  (2) 它連 offset 都學不到:`pcnt` 只有在 host 開 `INSP_CAM_TRIG_WATERMARK` 時才會出現在
  報告裡,而它是關的。(3) `CAM_PULSE_N` 會數**韌體驅動 CAM1 的每一條路徑**,包含
  `calFireNow` 與 `trig_cam_*` 命令——測試工具每打一發脈衝就把 offset 永久推一格,
  而且跟真的滑移長得一模一樣。
  **`cam_ts` 是對「成像事件」的量測,可以棄權;`pcnt` 是對「請求」的記帳,不能。兩者不對等。**
- `reset_running_stat` 會**殺掉時鐘模型**（run 中不可恢復）；A/B 只能用
  `reset_latency_stat`。

### 2.4 設定檔所有權矩陣（`Core0_1/data/`）

| 檔 | 誰讀 | 關鍵 |
|---|---|---|
| `machine_setting.json` | core（啟動+ST）＋ WebUI（HR 後 LD） | core 讀：`inspection_region`/`clean_regions`（**全感測器 px**，相機 ROI 原點由 sampler 補償）。WebUI 讀：`InspectionMode`、`uInspESP32_peripheral_conn_info`（PD CONNECT 的內容）。key 尾加 `1` = 停用慣例；`"pairing"` key 已死 |
| `default_camera_setting.json` | core（啟動/RC/ST）＋ WebUI | exposure/gain/gamma/blacklevel/framerate(-1=不限，限了會吃觸發)；**裁切用 `InspectionROI`，存進來的 `ROI` key 會被刪掉** — 2026-08-18 實測驗證（35/s 100/100 只有裁切下可能） |
| `lens_calib.json` | core **啟動自動載**＋RC | 遠心模型；換入必在 matchingEnglock 下 |
| `field_calib.json` | core **僅 on-demand**（RC）— 啟動不自動載（headless 缺口，BUILD.md 樣例是舊的） | 16×16 亮/暗場格 |
| `*.hydef` | CI/FI/II/SF/`!fi`/`--insp` | def 本體；sidecar `<base>.png` 模板；`featureSet_sha1` 完整性（WebUI 載入驗、存檔衝突檢查用） |
| 板子 NVS | 板子自己 | wire JSON 原樣存（cfg_json）；**版本不合＝回編譯預設**（io_on_level 事故根源 → 現在無極性預設，未定義極性=high-Z+IO_ARMED=false 拒進檢）；壽命計數器跨重啟 |

### 2.5 uInsp 狀態/錯誤碼速查

狀態：0 INIT / **100 IDLE** / 102 CAL（時鐘校正）/ 103 SPINUP / **101 READY（到速分選）**
/ 104 RECAL（跑中補校）/ 112 ERROR（鎖存，clear_error 解）/ 113 FATAL / 140 TEST。
錯誤：1 判定無對象、2 到 SWITCH 未答、10 cam_trig 佇列滿、11 序列協定錯（鎖存）、
12 host 斷訊、13 時鐘失配、14 校正不收斂、15 spin-up 逾時、16 兩配對法不一致、
0xff SEL 配額用盡。`get_state_names` 可拉全表。

## 3. Core 內部速圖

**執行緒**（mainLoop 全部生出、**從不 join**，關機 `_exit(0)`）：
main(WS select+全部 TL dispatch) / inspection(ImgPipeProcess：整 frame 抱
camera_lifetime_lock，量測抱 matchingEnglock) / preview(datView→SS/RP/IM/SS) /
snap-save / perif-send(節流 2ms) / perif-ping / perif-watchdog(2ms 排程遲滯量測) /
CamStateWatch(1s 門鈴) / slow-frame-save / perif console / 相機 driver 執行緒（
frame callback 在 driver thread 跑：pool→ExtractFrame→inspQueue push）/ UART rx /
`inspd_log` 子行程（shm ring 16MB → 4091）。

**鎖序**：`camera_lifetime_lock → matchingEnglock →（subscribersLock → linkLayerLock）
→ image_send_lock → per-conn sendMutex`；`perif_tx_lock` 獨立（規矩：先鎖再重讀
`perifCH`）；`g_station_cfg_lock` 管 region 換入；MT_LOCK 是 no-op 別信。

**資料流**：driver cb →（滿了丟舊）inspQueue(10) → 量測+判定 → perifSendQueue(256)
＋ datViewQueue(10) → 板子 ＆ WS 訂閱者。原則「preview 拖慢判定就是缺陷」——
每一段都 drop-oldest 不回壓相機。

**wiringPanel.cpp 找路**（行號 ~2026-08-18）：81-990 全域/佇列/鎖；992- PerifChannel；
1507- `g_inspCtx`（校正+站台 knobs 的家）；1739- 相機 glue（CameraSetup/LoadCameraSetting
/station loaders）；**2841-5908 toUpperLayer TL dispatch**；6310 相機 frame callback；
6791 InspResultAction_s；7510 perifDeliverResult；8049-8703 輔助執行緒群；
8704 ImgPipeProcessCenter_imp（每 frame 主體）；9846 mainLoop；10305 cp_main（CLI）。

**CLI/env**：`--insp <img> <def> <out.json>` 離線單張（量測變更的 golden gate；
exit 2/3/4）；`FORCE_BMP_CAROUSEL=<dir>` 假相機；`INSP_CAM_TRIGMODE_ONCE=1`
（全幅 bench 必開）；`INSP_PERIF_CONSOLE=4099`；`INSP_AREA_BYPASS`；
`INSP_ALLOW_EXEC`；`INSP_LOG_*`。必須在 `Core0_1/` 目錄下執行。

## 4. WebUI 內部速圖

- **狀態機**：自製 middleware（ECStateMachine 解讀 xstate 形狀的表，非 xstate 庫）。
  SPLASH →(HR)→ MAIN →{DEFCONF_MODE, INSP_MODE, INSTINSP_MODE}；MAIN 之下另有
  local submenu（RootSelect/Calibration/RepDisplay/InstInsp/Setting — 不是 SM 態）。
  **MAIN+EXIT → SPLASH 死路**（只有 HR 能出來）— 所以測試的 toMain 必須
  「讀態+dispatch 同一次 in-page eval」＋卡 SPLASH 就踢 socket。
- **通訊**：`comm/BPG_WS.js`（reqWindow/pgID 0-500、`_PGID_`釘選、1s systemStatusPull、
  斷線 10s 重撥）；`WSDataDispatch` 先攔兩個門鈴再批次 dispatch（ATBundle express）。
  `Cam_Stat_Query`：6s 輪詢+門鈴 poke、single-flight、soft-cam 政策
  （`ALLOW_SOFT_CAM` 是 **WebUI** 設定，core 沒有這回事）。
- **Redux**：`UIData`（edit_info：def 模型 `_obj`、DefFileHash、報告統計管線、
  matching knobs）＋ `ConnInfo`；perif 連線態已搬到 `perif/PerifAPI.js` 模組 store
  （30s 健康輪詢＋0xCA12 poke；suspectSrc 防 flap）。
- **def 生命週期**：LD 載入（驗 `featureSet_sha1`，錯即擋）→ 編輯 → triggerSave
  先重讀磁碟 sha1 防衝突 → SV。離開編輯器比 sha1 → 未存檔警告。
- **canvas**：EverCheckCanvasComponent 基類＋各模式子類；畫圖在 `canvas/renderUTIL.js`；
  per-shape 模組在 `src/shapes/`（north-star 垂直切片，A-C 部分完成）；
  cal_hits 是物件座標 mm 直接疊 def 形狀，帶 shape_fingerprints 過期檢查。
- **關鍵檔**：script.jsx（樞紐）、MAINUI、DefConfUI、InspectionUI、CalibrationUI、
  RepDisplayUI、component/StationRegionPanel、perif/*、UTIL/InspectionEditorLogic、
  UTIL/BPG_Protocol。dev 版有 `__GP_*` QA 把手（webctl 測試靠它們）。

## 5. uInsp 韌體內部速圖

- **ISR**（IRAM 全鏈，40MHz XTAL 守門）：2×plate_freq tick → SYS_STEP_COUNT++ →
  StepGo → GateSensing（去彈跳/寬度/最小距離/最小時距 30ms 預設；被拒＝件重繞，
  絕不發無 frame 的觸發）→ phantom 服務 → Run_ACTS（stage 任務：位置制 offset
  ticks＋時間制 width µs，雙緩衝快照發佈）。ISR 內**禁浮點**。
- **pipeline**：gate 過 → tid 進 RBuf(100) → 9 個 stage 任務註冊 → CAM1_on 打
  觸發+記 cam_us+發 cam_trig announce（佇列 32，滿=err 10）→ SWITCH 時按
  insp_status 分流：cat→SEL1/2/3 吹氣、0xFFFF=NA 重繞、SKIP/UNSET 計數＋
  連續未答策略。VERD_LOG(64) 是 slip 取證儀。
- **命令面**：JSON line（~58 型）。讀：`poll`（120B 熱查）、`get_setup`、
  `get_running_stat`（**~2886/3072B 溢位邊緣，靜默掉欄位；新欄位開新命令**）、
  `get_backup_stat`、`get_verdict_log`。寫：`set_setup` **grouped keys**（未知
  key＝整包拒收）＋`persist:true`（persist guard：IDLE/READY 且 freq==0 才准）。
  測試注入：`trig_phantom_pulse/train`、`virt_pulse`（唯一像真件的）、
  `trig_cam_pulse`（光先亮→delay→100µs CAM 脈衝）、`trig_cam_burst`（IDLE 限定）、
  `jog`（IDLE 限定，絕對座標）。
- **serial**：`setRxBufferSize(2048)` 必在 begin 前；host watchdog 要 core 武裝；
  DTR=重開機。**PIN_ON/PIN_MODE 這版沒 ack、默默忽略**。
- **腳位**：見 §1 圖；`io_on_level` 全 active-low（共陽光耦），high-Z 是安全態。
  觸發電氣：Line0 光耦 3.3V 邊緣但實測 35/s 零掉發；CAM1/L1A 在 2026-08-05→06
  短暫併線過 — 該窗口的舊文件不可信。

## 6. Bench 操作手冊

```sh
# core（桌面全配方）
cd InspectionCore/Core0_1
INSP_CAM_TRIGMODE_ONCE=1 INSP_PERIF_CONSOLE=4099 ../build/mac-arm64/visSele
# 等 ~20-30s init；!fi 太早會打死 core（歷史陷阱，NULL guard 已補但別賭）

# WebUI dev
cd UI/WebUI && npm run dev          # :8081
node tools/webctl/webctld.mjs       # :8765 Playwright 控制器

# 板子設定面板（獨佔板子——先停 core！）
python3 Peripheral/uInspESP32/tools/uinsp_panel.py --port /dev/cu.usbserial-0001 --http-port 8766
```

- 假相機：`FORCE_BMP_CAROUSEL=<folder>`（或無真相機時自動 fallback）。
- 真相機（Hikrobot USB3 via Aravis on mac）：插上即被 discover 優先；裁切設
  `default_camera_setting.json` 的 `InspectionROI`。
- 板子在 `/dev/cu.usbserial-0001`；PD CONNECT 由 WebUI 或 `!pd` 完成。
- 量測改動的 golden gate：`--insp` 前先備份並清掉 machine_setting 的
  inspection_region/clean_regions（**已咬三次的陷阱**），跑完還原。
- 桌面（無馬達）也能跑完整邏輯迴圈：乾淨序列 `clear_error → set_setup
  {"plate":{"freq":15000}} → enter_insp_mode`，meas 會爬到 15000；中途夾到被拒
  命令會卡 meas=0（假死象）。

## 7. 測試總覽與「跑產不干擾」矩陣

三層測試的完整目錄：`InspectionCore/docs/REGRESSION_TESTS.md`（core 離線/活體
＋WebUI）與 `Peripheral/uInspESP32/tools/TESTS.md`（板子 ~25 支）。以下是
**「機器正在跑產（FI session 進行、板子 101 READY）」時的安全分級** — 新 agent
在活機上動手前先查這張表：

### ✅ 跑產中安全（純觀察，不改任何狀態）
| 工具/操作 | 說明 |
|---|---|
| `fi_watch.mjs` | 只 SB 訂閱數 RP/IM |
| `soak.mjs`（觀察用途） | 訂閱統計報告率 — 但它假設有 def 參數時會發 CI，**只看不發** |
| `perifstat.mjs` / `caminfo.mjs` / GS 任意讀值 | 唯讀 |
| `peek.py` | 走 console 免重置問板子（前提：console 4099 沒別的 client — **console 單客戶，第二條連線會把第一條無聲踢掉**） |
| `get_running_stat` / `poll` / `get_verdict_log` 前後差值 | 唯讀；計數是 NVS 壽命值，一律用差值 |
| WebUI 只進 MAIN 看狀態、開 PerifStatus 面板 | 唯讀 |
| `dv_bench.mjs` | 只訂閱量流量 |

### ⚠️ 會擾動但可恢復（不要在乎產出的那一輪做）
| 操作 | 影響 |
|---|---|
| WebUI 進出 InspectionUI / cycle.mjs / flows inspCycle | 進出會重開 CI/FI session、切相機 TriggerMode、退出時 restore 限規。**注意：離開 InspectionUI 讓 uInspESP32 停機（exit_insp_mode）是設計上的正常行為** — 操作面「退出檢驗＝停機」；列在這格只是提醒測試別在乎產出的批次上跑 |
| DefConfUI 的 INST_CHECK（II/EX） | 搶 matchingEnglock、動 neutral_bacpac；量測會插隊 |
| ST InspAreaBypass / InspectionParam / CameraSetting | 立即改變在線判定行為 |
| `reset_latency_stat` | 清延遲計數（本來就是拿來 A/B 的，不動時鐘） |
| `!fi` 重注 def | 重載引擎＋re-TriggerMode（全幅 bench 沒開 TRIGMODE_ONCE 會殺相機） |

### ❌ 跑產中禁止（毀掉一輪 run 或更糟）
| 操作 | 為什麼 |
|---|---|
| **PD CONNECT / DISCONNECT**（含 doorbell.mjs phase 3、link_fault、pd 類探針、uinsp_panel 開埠、任何重開序列埠） | DTR → **板子斷電重開**：盤上件全 NA、時鐘模型歸零、RAM 設定回 NVS |
| **RC camera_ez_reconnect**（含 rc_hammer.mjs、WebUI 相機重連鈕） | delete 相機重建 — 觸發鏈斷、in-flight frame 作廢 |
| bpg_sweep.mjs | 會掃 SV/RC/PD/ST 的變體 — 有 run-mode 警告，聽它的 |
| churn.mjs / slow_client*.mjs | 蓄意 wedge WS 層（測試設計就是製造壓力） |
| `reset_running_stat` | **殺 CAM_SYNC 時鐘模型**，run 中不可恢復（~10s 後 112 停機） |
| `save_setup` / `clear_saved_setup` / `stepper_disable` / `trig_cam_burst` / `sel_act` / `jog` | 板子的 persist guard / IDLE-only 會拒，但別靠拒收當安全網 |
| 核 core 重啟 / `!fi` 過早 / 燒韌體 | core 重啟=序列重開=板子重開機（機器已能存活但當輪報廢）；燒韌體要停 core |

**經驗法則**：凡是「開序列埠、刪相機、換 session、reset 帶 running 字樣」的都是
破壞性；凡是「GS/訂閱/差值讀數」都安全。不確定時先 `poll` 看 state — 101/104
就是產中。

## 8. 文件權威表

| 主題 | 權威文件 |
|---|---|
| 全域索引 | `docs/README.md`（本檔上層） |
| Core 開放問題/bug 帳本 | `InspectionCore/docs/AUDIT_BACKLOG_2026-08-15.md`（Tier 1-7 全帶 FIXED/OPEN 標注） |
| Core sprint 敘事 | `InspectionCore/docs/HANDOVER_2026-08-18.md`(最新) |
| 測試怎麼跑 | `InspectionCore/docs/REGRESSION_TESTS.md`＋`Peripheral/uInspESP32/tools/TESTS.md` |
| 板子韌體必守不變量 | `Peripheral/uInspESP32/docs/FIRMWARE_CONTRACT.md` |
| 整機流程/時戳參考點 | `Peripheral/uInspESP32/docs/MACHINE_FLOW.md` |
| 配對遷移現況 | `PAIRING_MIGRATION_STATUS.md`（配 `PAIRING_VALIDATION_2026-08-06.md` 讀） |
| 桌面整合一次性量測 | `Peripheral/uInspESP32/docs/DESK_INTEGRATION_2026-08-17.md` |
| 陷阱帳本（append-only, grep 用） | `CORE0_1_CAVEATS.md`、`UINSP_CAVEATS.md` |

**已知過時聲明**（測繪時逐一驗出，別被咬）：`default_camera_param.json` 已不存在
（lens/field calib 取代，RUNNING/ARCHITECTURE 舊文提到的是屍體）；field_calib
並非開機自動載（BUILD.md 樣例輸出是舊的）；log daemon 現在預設 ON（ARCHITECTURE
說 opt-in 是舊的）；`uInspESP32_v2/DOC` 與 `CoreHub/Note.md` 屬別的世代；
2026-08-05→06 窗口的 CAM1/L1A 併線描述已失效。

## 9. 新 agent 第一天

1. 讀本檔 §0-2；跳 `DEV_COMPLETE_CHECKLIST.md`（板子側的 cut line）。
2. 起 bench（§6 配方），`node bpg_sweep.mjs --include-crashers` 應 35/35、
   `flows.mjs verify` 應 9/9 — 這是「我沒把環境弄壞」的基準。
3. 動量測程式碼前：跑一次 `--insp` 三 def 基準留檔（記得清 region）；改完 leaf-diff
   必須 bit-identical，除非你「有意」改行為並記錄。
4. 動 WS/鎖前：讀 backlog 2.8/6.8/7.1 的鎖域現況；churn/doorbell 是你的回歸網。
5. 動板子前：查 §7 矩陣 + TESTS.md 共用陷阱；記住 CONNECT=重開機、
   reset_running_stat=殺時鐘、console 單客戶。
6. 最貴的十個陷阱速記：machine_setting region 吃掉 golden（三次）；`!ld` 不載 def；
   `!fi` 太早/重複（TRIGMODE_ONCE）；LineStatusAll 輪詢看不到 ms 脈衝；PIN_ON 無 ack；
   NVS 計數是壽命值；MAIN+EXIT=SPLASH 死路；合成 DOM 事件無效（用 Playwright）；
   log ring「沒在 log 裡」不代表沒發生；`git commit -a` 會毀 submodule（**永遠列明路徑**）。
