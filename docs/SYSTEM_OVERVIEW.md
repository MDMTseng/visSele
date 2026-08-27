# visSele 系統總覽與陷阱（現況文件）

> **校準日期:2026-08-27。** 這是**現況文件**——描述機器現在的行為,發現不符就當場改。
>
> 讀這份拿到「全局長什麼樣、邊界在哪、什麼會咬人」。細節由 §9 的權威文件表接手。
>
> **與 `docs/SYSTEM_MAP.md` 的關係**:那份 2026-08-18/19 校準,三方合約(BPG 封包表、
> uInsp 狀態碼、跑產不干擾矩陣)仍然是最完整的,**繼續用**。但它成文於 launcher 上線之前,
> 且引用的行號已全數失效。§8 逐條列出哪裡不能信。

---

## 0. 這是什麼機器

旋轉玻璃盤高速全檢分選機。零件從震動送料進轉盤 → gate 感測器偵測到 → 背光下相機拍照
→ core 量測並判定 OK/NG/NA → 板子在 SEL 工位吹氣,把不同等級分進不同料道。

一句話抓住設計原則:**preview 拖慢判定就是缺陷**。整條資料路徑每一段都
drop-oldest,不對相機回壓。看畫面的人可以掉格,判定不行。

---

## 1. 五層,不是四層

多數舊文件寫「三方」或「四個子系統」。**現在是五層** —— launcher/更新層是
2026-08-23 之後才有的,`SYSTEM_MAP.md` 完全沒有它。

| 層 | 做什麼 | 位置 / 語言 |
|---|---|---|
| **Launcher** | Electron 殼:選版本、spawn core、健康檢查、監督、更新與回退 | `UI/Launcher/`(原始碼)、`export_v2/launcher/`(打包),JS |
| **WebUI** | 操作層:def 編輯器、SBM Studio、產檢畫面、校正、報表 | `UI/WebUI/`,React+Redux+antd |
| **Core (visSele)** | 量測層:取像、MatchingEngine 量測、產判定、對 WebUI 的 BPG/WS、對板子的序列通道 | `InspectionCore/Core0_1/`,C++ |
| **uInsp 板 (ESP32)** | 實時層:盤時基(ISR)、gate、觸發相機、frame↔件配對、執行判定(吹氣) | `Peripheral/uInspESP32/`,C++/Arduino |
| **相機** | Hikrobot MV-CA050-11UM,硬體觸發(Line0) | `InspectionCore/CameraLayer/` |

**世代警告**(這條每個新手都會踩):`CoreHub` 是二代、`uInspESP32_v2` 是停滯的重寫、
`UI/WebUI2` 是新 UI。**沒有一個是產線在跑的東西。** 除非明確要做二代,不要讀那些目錄。

樞紐檔各一個,都很大:`wiringPanel.cpp`(**12,945 行**)、
`LegacyFirmware.cpp`(**10,043 行**)。

---

## 2. 埠位圖(2026-08-27 實測)

```
              ┌────────────────────────────────┐
              │  Launcher (Electron)           │
              │   選版本 → spawn → 監督        │
              └───────┬──────────────┬─────────┘
                      │ 4098 control │  載入 WebUI
                      │ (ping/shutdown)         │
              ┌───────┴──────────────┴─────────┐
              │  core visSele                  │
              │  cwd=Core/  chdir=<workingDir> │
              └──┬─────────┬─────────┬─────────┘
        4090 BPG/WS   4091 log/WS   4099 perif console
        (WebUI)       (inspd_log     (**預設不開**)
                       子行程)
                      │                         │
              ┌───────┴───┐         ┌───────────┴────────┐
              │  相機      │         │ uInspESP32 板      │
              │ Line0←CAM1│         │ serial 230400 8N1  │
              └───────────┘         └────────────────────┘
```

| 埠 | 誰開 | 預設 | 備註 |
|---|---|---|---|
| **4090** | core | 開 | BPG over WebSocket,WebUI 的唯一通道 |
| **4091** | `inspd_log` 子行程 | 開 | log ring 的 WS 出口,subprotocol `inspd_log.v1`,路徑 `/log` |
| **4098** | core | **開** | **控制埠,launcher 專用**。`INSP_CONTROL_PORT` 覆寫,`=0` 關閉。只有 `ping` 與 `shutdown` 兩個命令 |
| **4099** | core | **不開** | `INSP_PERIF_CONSOLE=<port>` 有設才開。慣例用 4099,**不是預設值** |

### 4098 vs 4099 —— 這一組最容易搞錯

兩個都是「TCP 打 JSON 進去」,但**完全不同**:

- **4098 是 launcher 的監督通道。** 只答 `ping`(回 pid/uptime/version/git)與
  `shutdown`。回應帶 **pid**,因為只靠埠號認不出是誰 —— 上一輪殘留的 core 會用同一個
  埠回答,看起來跟剛啟動的子行程一模一樣(曾經真的發生:測試把 shutdown 送給了陌生人)。
- **4099 是問板子問題用的 dev console**,可以下 ~58 種板子命令。
  **它預設不開。** `boot.js` 只在 launcher 自己的環境裡已經有 `INSP_PERIF_CONSOLE`
  時才傳給 core —— 產線機器不該只因為某個工程師需要就開控制埠。

> ⚠️ **`docs/README.md` §2 的鐵則寫「改問核心的 console:`nc 127.0.0.1 4099`」。**
> 在 launcher 起的 core 上**那個埠是關的**,命令會直接連不上。要用得先
> `INSP_PERIF_CONSOLE=4099` 起 launcher。

---

## 3. 開機路徑(launcher 怎麼把 core 叫起來)

版本包自己帶開機配方:`scripts/boot.js`。**launcher 不知道任何 visSele 專屬的事** ——
這正是「舊 launcher 能跑新版本」的原因。

```
export_v2/
  app/
    current.json      {version, previous, selectedAt}   ← 現在要跑哪版
    last_good.json    {version, ranForS, at}            ← 跑滿門檻才升格
    1.1.103/ 1.1.104/ 1.1.105/
      info.json
      scripts/boot.js     ← 這版怎麼開,由這版自己說
      Core/               ← visSele.exe + 相機 SDK 的 DLL 與 .cti
      WebUI/              ← 打包好的前端(assets/index-*.js)
  launcher/
    Xception INSP-win32-x64/    ← 打包好的 Electron 殼
```

**cwd 與 chdir 是兩件事,兩個都關鍵**(`boot.js` 的註解說得最清楚):

- `cwd: 'Core'` —— 相機 SDK 的 producer 模組(`MvProducer*.cti`)和 DLL 就在執行檔旁邊。
- `args: ['chdir=<workingDir>']` —— core 內部**每一條路徑都是相對的**
  (`data/machine_setting.json`、`data/featureDetect`、`data/SAMPLE` 還有十幾個),
  所以這個參數決定**它跑的是哪一台機器的設定**。

> **這裡曾經無聲壞掉過**:chdir 目標不存在時 core 會忽略錯誤、留在啟動目錄繼續跑,
> 對著**另一份 `data/`** 檢驗而什麼都不報。現在是 hard-fail。

---

## 4. 一顆零件的完整路徑

```
震動送料 → 轉盤 → gate 感測(ISR,去彈跳/寬度/最小距離/最小時距)
   → tid 進 RBuf(100) → 9 個 stage 任務註冊
   → CAM1_on:打觸發 + 記 cam_us + 發 cam_trig announce(佇列 32,滿=err 10)
   → 相機 Line0 硬體觸發曝光
   → driver thread: pool → ExtractFrame → inspQueue(10, drop-oldest)
   → inspection thread: 定位 → 量測 → 判定
   → perifSendQueue(256) → 板子      ＆     datViewQueue(10) → WS 訂閱者
   → 板子在 SWITCH 工位按 insp_status 分流:cat→SEL1/2/3 吹氣、0xFFFF=NA 重繞
```

**配對(frame ↔ 件)是板子的事,不是 core 的事。** 回送格式
`{"type":"report","tid":-1,...}` 裡 **tid 恆為 -1**;core 端的配對函式 2026-08-12
起已淘空。配對靠 **CAM_SYNC 時戳**;`CAM_PCNT` 計數法**已淘汰**(程式碼還在但實機關閉)。

---

## 5. def 檔與 SBM 定位(2026-08-26/27 大改,舊文件全部過時)

這一段是最近改動最密集的地方,舊文件在這裡最容易騙人。權威是
`InspectionCore/docs/DEF_FILE_FORMAT.md`。

### 5.1 兩種定位引擎

`featureSet[0].locating_engine` 決定用哪個:

- `"sig360"` —— 簽章式定位(原本的)。
- `"shape_based"` —— SBM,line2Dup 形狀比對。

**失敗會無聲退回。** 如果 SBM 的特徵不可用,core 會退回 sig360,**量測照樣全過、
畫面看起來完全正常**,只有報告信封裡的 `locate.code` 不一樣。這是整套快取機制
能製造的最安靜的故障,所以現在:

- core 出 `locate = {best, thres, candidates, reason, code}`,
  code ∈ `untrained | train_failed | no_candidate | below_thres | no_region`
- InspectionUI 只在 `untrained` / `train_failed` 兩碼掛紅色 banner
  (**認 code,不認 reason 字串** —— 字串會被改寫,那天畫面就靜音了)

### 5.2 特徵存在哪(2026-08-27 搬過)

```
featureSet[0]
├── def_image_reg          ← 2026-08-27 從 def 根層搬進來
├── locating_engine
├── roi_refine_points
└── inherentfeatures[]     ← 閉集字彙
    ├── {type:"sign360", ...}
    └── {type:"sbm_info", name:"@__SBM_INFO__", shape_cache:{...}}  ← SBM 特徵住這
```

- 舊的**頂層 `__shape_cache` 存檔時會被移除,不是鏡射**。core 兩邊都讀,
  `@__SBM_INFO__` 蓋過舊的。
- `inherentfeatures` 與 `features[]` 都是**閉集字彙**:未知 `type` 回 -1,
  **整個 def 載入失敗**。加新型別要同時改 core 的 parser。

### 5.3 快取指紋 —— 什麼會讓特徵失效

```
v1|WxH|contentSum|nfN|T…|wWEAK|sSTRONG|roiN:M|ao<deg>|<ROI 點…>
```

任何一項變動,快取就 stale。**最會咬人的是 `ao`(= `def_image_reg.angle` 換算的度數)**:
動了定位就等於作廢特徵。

指紋對不上時 core 記 `[shape] cache stale; re-extracting || was: … || now: …`
—— **兩串都印**,因為「stale」本身分不出是某個參數有意改了、還是參考影像被換掉,
而這決定要重生一個 def 還是全廠的 def。

### 5.4 特徵萃取只在 studio 發生

`shape_extract_allowed()` **預設 NO**。只有 SF handler(studio 的「生成特徵點」)
會開一次窗、parse 完就關。

> **這條與 5.3 合起來就是最容易誤判的故障**:快取 stale + 萃取被擋 = 檢驗端完全
> 沒有特徵 → 退回 sig360 → banner。而**同一個 def 用 `--insp` 跑卻會過**,因為
> CLI 允許萃取,它會安靜地重新訓練一次。診斷時務必分清楚。
>
> `SHAPE_DBG=1` 下兩種情形各有一行:
> `[SHAPE_DBG] cache HIT: N features from def, no extraction` / `[SHAPE_DBG] cache stale`。
> **命中那行是 2026-08-27 才加的** —— 在那之前 stderr 兩種情況長得一模一樣
> (模板設定那行兩邊都印),「用了快取」和「無聲重訓」從外面分不出來。

---

## 6. Core 內部

**執行緒**(mainLoop 全部生出、**從不 join**,關機 `_exit(0)`):
main(WS select + 全部 TL dispatch)/ inspection / preview / snap-save /
perif-send(節流 2ms)/ perif-ping / perif-watchdog / CamStateWatch(1s 門鈴)/
slow-frame-save / perif console(要 env)/ 相機 driver thread / UART rx /
`inspd_log` 子行程(shm ring 16MB → 4091)。

**鎖序**(違反會死鎖):
```
camera_lifetime_lock → matchingEnglock →（subscribersLock → linkLayerLock）
→ image_send_lock → per-conn sendMutex
```
`perif_tx_lock` 獨立(規矩:先鎖再重讀 `perifCH`);`g_station_cfg_lock` 管 region 換入;
**`MT_LOCK` 是 no-op,別信它**。

**掉件計數器**(2026-08-27 匯出到 `precess_queue_status`):
```json
inspQueue     {"capacity":10,"size":0,"dropped":0,"acq_refused":0,"pool_empty":0}
datViewQueue  {"capacity":10,"size":0,"dropped":0}
inspSnapQueue {"capacity":5,"size":0,"dropped":0}
```
在這之前 drop 只進 log,沒有人能從外面量。

**crash dump**:`Core0_1/crash_reports/<YYYYMMDD>/`,含整個 ring。
> ⚠️ **`PRODUCER_DIED` 不等於 crash。** 每次 `--insp` CLI 都是一個短命的 log producer,
> 結束時 drainer 就記一筆。這在 2026-08-27 讓人以為 core 在使用者操作時掛了八次。
> 看 `signal:` 那行,再看尾巴是不是 `--insp: wrote <path>`。

---

## 7. 會咬人的陷阱

### 7.1 版本控制
- **絕不 `git commit -a` / `git add -A`。** worktree 的建置 symlink 在
  `InspectionCore/contrib/shape_based_matching`,會被掃進去、合併時毀掉 submodule。
  **一律明確列路徑。**
- **stash 不會被 push。** 換機器/機器故障就沒了。要保留就推成分支:
  `git push origin "$(git rev-parse stash@{0})":refs/heads/wip/<name>`

### 7.2 硬體
- **絕不 `kill -9` core** —— 相機會卡住,之後每個行程都拿不到,要 DeviceReset 才救得回。
- **PD CONNECT = 開序列埠 = DTR 拉 EN = 板子重開機。** 盤上件全 NA、時鐘模型歸零。
- **機器在跑時不要開板子的序列埠**(`uinsp_panel.py` 等)—— 板子 reset,證據當場消失。
- **console 單客戶**,第二條連線把第一條無聲踢掉。
- **絕不擅自覆寫板子的 NVS**。曾因版本號跳動洗掉 `io_on_level`(這台是 active-low)。

### 7.3 量測
- **`--insp` 前先清掉 `machine_setting.json` 的 `inspection_region`/`clean_regions`**,
  跑完還原。**這個陷阱已經咬了三次。** 或用 `INSP_AREA_BYPASS=1`。
- `--insp` 允許萃取特徵,live 檢驗不允許 —— 見 §5.4。
- 量測改動的 golden gate:改完 leaf-diff 必須 bit-identical,除非你**有意**改行為並記錄。

### 7.4 WebUI
- **React 16,沒有 `useSyncExternalStore`。** 用了會 runtime error。照 `usePerifLink` 的寫法。
- **`import * as BPG_Protocol` 拿不到 `map_BPG_Packet2Act`** —— 它只在 default export 上。
  這個曾讓影像切換在 core 裡成功、卻到不了 canvas。
- **MAIN + EXIT → SPLASH 是死路**,只有 HR 能出來。
- **合成 DOM 事件無效**,要用 Playwright。
- **滑鼠放開不要自動算貼合** —— 使用者有時故意放在不完美的狀態。

### 7.5 沒有機器的 bench
- 假相機:`FORCE_BMP_CAROUSEL=<dir>`,或沒有真相機時自動 fallback。
- **只有板子、沒有轉盤也能跑完整 pipeline**:`SOAK_PHANTOM=1`
  (`trig_phantom_pulse` 直接呼叫 `newPulseEvent()`,完全繞過 gate 感測)。
  但要記住 **`edges`(gate.in)在 phantom run 裡不會動** —— 它數的是感測器邊緣,
  而那不存在;要看 `admitted`(gate.out)。
  ⚠️ **`SOAK_PHANTOM` 尚未實測**(2026-08-27,語法過、端到端沒跑過)。

---

## 8. 過時聲明登記簿

> 這一節是這份文件存在的一半理由。以下每一條都在 2026-08-27 逐項驗證過。

### 8.1 行號 —— 全部失效

`wiringPanel.cpp` 從 ~10.8k 長到 **12,945 行**,`LegacyFirmware.cpp` 從 ~9.6k 長到
**10,043 行**。**任何文件裡的行號都不要信**,一律 grep 符號名。

`SYSTEM_MAP.md` §3 的「找路」表整段失效,實測對照:

| 符號 | SYSTEM_MAP 寫的 | 實際 |
|---|---|---|
| `toUpperLayer` dispatch | 2841–5908 | **3683** |
| `ImgPipeProcessCenter_imp` | 8704 | **7425** |
| `mainLoop` | 9846 | **11888** |
| `cp_main` | 10305 | **12354** |
| `perifDeliverResult` | 7510 | **9115** |
| `InspResultAction_s` | 6791 | **1087** |

同樣失效:`BENCH_WORKLIST_2026-08-19.md:107` 的 `wiringPanel.cpp:6249`、
`AGENT_AUDIT_2026-08-26_RECOVERED.md` 裡全部 `wiringPanel.cpp:NNNN`。

### 8.2 具體的錯誤聲明

| 文件 | 過時的話 | 現況 |
|---|---|---|
| `docs/README.md` §2 | 「改問核心的 console:`nc 127.0.0.1 4099`」 | **4099 預設不開**,launcher 只在環境已有 `INSP_PERIF_CONSOLE` 時才傳。要先設 env |
| `docs/README.md` §1、`SYSTEM_MAP.md` §1 | 「周邊 console 在 4099」當成常態埠 | 4099 是**選配**;常開的是 **4098 控制埠**,兩份文件都沒提 |
| `SYSTEM_MAP.md` 全篇 | 沒有 launcher / 版本 / 更新層 | launcher 2026-08-23 上線,見 §3 與 `docs/LAUNCHER_REDESIGN_2026-08-23.md` |
| `SYSTEM_MAP.md` §1 圖 | `core visSele (cwd=Core0_1)` | launcher 下是 `cwd=Core/` + `chdir=<workingDir>`,兩件事,見 §3 |
| `BARE_BOARD_TEST_PLAN_2026-08-19.md:16` | 「`PerifConsoleThread` 整個在 `#ifndef _WIN32` 裡,4099 永遠不開」 | **已移植到 Windows**(同日,`wiringPanel.cpp` 檔頭註解記載),現在是 `#ifdef _WIN32` 分支 |
| `BACPAC_MACHINECONTEXT_REFACTOR.md` :36/:181 | 把 `__shape_cache` 當 def 裡的頂層欄位 | 已搬進 `featureSet[0].inherentfeatures` 的 `@__SBM_INFO__`,頂層存檔時移除 |
| `ARCHITECTURE.md`(2026-08-17) | log daemon 是 opt-in | **預設 ON**(`SYSTEM_MAP` §8 已記過,這裡再記一次因為 ARCHITECTURE 本身沒改) |
| `ARCHITECTURE.md` / `RUNNING_CORE0_1.md` | 提到 `default_camera_param.json` | **檔案不存在**,已由 lens/field calib 取代 |

### 8.3 讀起來像現況、其實是歷史的文件

- **`AGENT_AUDIT_2026-08-26_RECOVERED.md`** 大段引用 `search_point_cv` 的
  `labelImg` / `objLabel` / `isObjectPx` / `labelAt` 遮罩。**那段程式碼已整個移除。**
  文件本身是稽核紀錄(記錄當時的真相,沒有錯),但引用的程式碼片段已不存在,
  搜尋 `labelImg` 只會在這份文件裡找到。
- `HANDOVER_*.md` 是**敘事快照**,不是現況。只有最新一份(`HANDOVER_2026-08-26c.md`)
  描述現在;更早的只在追那幾天的事時讀。

### 8.4 被否決但實作還在的東西

- **量測 fence polygon** —— 做完、能跑,然後被否決:量測 primitive 自己的 region
  就是那個 mask,而且 primitive 會跟著 morph 走,polygon 是剛性的。
  實作在 stash(已推成 `wip/measure-fence-polygon`)。**不要再提案。**
- **B4 自動重新武裝** —— 開機狀態太複雜,先擱置。`wip/b4-auto-rearm`。

---

## 9. 權威文件表

| 主題 | 讀這份 |
|---|---|
| 全域索引 | `docs/README.md` |
| 三方邊界合約(BPG 封包表、uInsp 狀態/錯誤碼、跑產不干擾矩陣) | `docs/SYSTEM_MAP.md` §2 §5 §7 —— **仍是最完整的**,但先讀本文 §8 |
| def 檔格式、指紋、閉集字彙 | `InspectionCore/docs/DEF_FILE_FORMAT.md` |
| SBM 最新一輪(含實測數字) | `InspectionCore/docs/HANDOVER_2026-08-26c.md` |
| launcher / 版本 / 更新 | `docs/LAUNCHER_REDESIGN_2026-08-23.md` |
| 整機流程與時序預算 | `Peripheral/uInspESP32/docs/MACHINE_FLOW.md` |
| 韌體不變量 | `Peripheral/uInspESP32/docs/FIRMWARE_CONTRACT.md` |
| 陷阱帳本(append-only,**grep 症狀,不要通讀**) | `CORE0_1_CAVEATS.md`、`UINSP_CAVEATS.md`、`WEBUI_CAVEATS.md` |
| 怎麼驗 | `InspectionCore/docs/REGRESSION_TESTS.md` |

---

## 10. 接手第一天

1. 讀本文 §1 §2 §5 §7 §8。**§8 先讀,不然會被舊文件帶偏。**
2. 起 bench,確認基準線(`docs/SYSTEM_MAP.md` §6 的配方仍然有效)。
3. 動量測程式碼前:先跑 `--insp` 留 golden(**記得先清 region**)。
4. 動鎖或 WS 前:讀 §6 的鎖序。
5. 動板子前:讀 `SYSTEM_MAP.md` §7 的不干擾矩陣。記住 CONNECT=重開機、
   `reset_running_stat`=殺時鐘、console 單客戶。
6. **文件與現實不符時,改文件** —— 這是現況文件,不是快照。

---

## 11. 寫文件的規矩(沿用,仍然有效)

- **量測寫數字,不寫形容詞。** 「很慢」沒用,「1372.6ms,預算 792ms」有用。
- **寫下是什麼量測殺掉了哪個假設**,不只寫結論。
- **錯了就寫錯了。** 被撤回的結論比正確答案更省下一個人的時間。
- **CAVEATS 只追加**,舊條目是當時的真相,不要事後修飾。
- **行號會死,符號名不會。** 引用程式碼一律給符號名。
