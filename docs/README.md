# visSele — 文件地圖 / Start Here

這個 repo 有 **30+ 份文件,分散在三個目錄**。這份地圖存在的唯一理由是:
**新 session 不必讀完才知道該讀哪一份。**

先讀本頁的 §1 §2 §5,再依你要做的事跳到對應文件。其餘不要通讀。

---

## 1. 這是什麼機器

旋轉玻璃盤全檢機。零件在盤面上轉,經過相機拍照,核心判定,再由噴嘴把不同等級
吹進不同料道。四個子系統:

| 子系統 | 角色 | 語言 / 位置 |
|---|---|---|
| **相機** | Hikrobot MV-CA050-11UM,硬體觸發(Line0) | `InspectionCore/CameraLayer/` |
| **Core0_1** | 影像檢驗核心,1gen,**已在工廠部署** | C++,`InspectionCore/Core0_1/` |
| **uInspESP32** | 板端:轉盤步進、閘門感測、觸發相機、依判定驅動噴嘴 | C++/Arduino,`Peripheral/uInspESP32/` |
| **WebUI** | 設定與監看,1gen,**已在工廠部署** | React+antd,`UI/WebUI/` |

Core0_1 ↔ WebUI 走 **BPG over WebSocket (4090)**;
Core0_1 ↔ uInspESP32 走 **USB serial 230400**,核心的周邊 console 在 **4099**。

> **世代提醒**:`CoreHub` 是 2gen、`uInspESP32_v2` 是另一代重寫、`UI/WebUI2` 是新 UI。
> **它們都不是目前產線在跑的東西。** 除非明確要做 2gen,否則不要讀那些目錄的文件。

---

## 2. 動手前必讀的鐵則

這些是踩過才知道的,違反的代價都很具體:

- **絕不 `git commit -a` / `git add -A`**。worktree 的建置 symlink 在
  `InspectionCore/contrib/shape_based_matching`,會被掃進去並在合併時毀掉 submodule。
  **一律明確列出路徑。**
- **絕不 `kill -9` core**。相機會被卡住,之後每個行程都拿不到,要 DeviceReset 才救得回。
  用 SIGINT,等它自己收。
- **絕不擅自覆寫板子的 NVS**。設定以 wire JSON 存在板子上;
  曾經因為版本號跳動把 `io_on_level` 洗掉(這台機器是 active-low)。
- **機器在跑時不要開板子的序列埠**。開埠會 reset ESP32,你要查的證據當場消失。
  改問核心的 console:`printf '{"type":"get_running_stat"}\n' | nc 127.0.0.1 4099`。
- **不要在跑量測的同一台機器上做粗重分析**。會汙染結果——這件事在
  `POSTMORTEM_2026-08-10_stall.md` 有實例。

---

## 3. 我想做 X,該讀哪一份

| 你要做的事 | 讀這個 | 狀態 |
|---|---|---|
| **新 agent 接手整個系統:三方大地圖(板×core×WebUI)、邊界合約、跑產不干擾矩陣** | `docs/SYSTEM_MAP.md` | **2026-08-18 全面校準,從這裡開始** |
| **接手 Core0_1:現在做到哪、下一步、哪些會誤導你** | `InspectionCore/docs/HANDOVER_2026-08-18.md` | **現況地圖,最新** |
| 想知道有哪些已知 bug / 哪些已修 | `InspectionCore/docs/AUDIT_BACKLOG_2026-08-18.md`(本輪)、`AUDIT_BACKLOG_2026-08-15.md`(上一輪) | 逐項標 VERIFIED/REPORTED、FIXED/OPEN |
| 要測 / 想知道有哪些回歸測試 | `InspectionCore/docs/REGRESSION_TESTS.md` | **2026-08-19 更新**,四層測試的總索引 + 13 條坑 |
| **要寫或改 WebUI 的自動化測試** | `UI/WebUI/docs/TEAM_HANDOFF.md` §7 §13、`UI/WebUI/tools/webctl/qa/SUMMARY.md` | **2026-08-19 更新**;選元素的規則、五層架構、七個失敗各是什麼(沒有一個是新缺陷) |
| **只有裸板(無相機、無轉盤),要安排測試** | `InspectionCore/docs/BARE_BOARD_TEST_PLAN_2026-08-19.md` | **2026-08-19**;三條 track、已實測 vs 推測分開標 |
| **手邊沒有相機和板子,想測穩定性** | `node UI/WebUI/tools/webctl/suite_nohw.mjs --list` | 14 項可跑 / 8 項標明缺什麼 |
| 第一次接觸這個系統 | `InspectionCore/docs/TEAM_ONBOARDING.md` | 現行 |
| 看系統拓樸 / 協定 / 資料流 | `InspectionCore/docs/ARCHITECTURE.md` | 現行 |
| 在本機把核心跑起來 | `InspectionCore/docs/RUNNING_CORE0_1.md` | 現行 |
| 改 Core0_1 前想知道有哪些坑 | `InspectionCore/docs/CORE0_1_CAVEATS.md` | **追加式紀錄** |
| 改韌體前想知道有哪些坑 | `Peripheral/uInspESP32/docs/UINSP_CAVEATS.md` | **追加式紀錄** |
| 改 WebUI 前想知道有哪些坑 | `UI/WebUI/docs/WEBUI_CAVEATS.md` | **追加式紀錄** |
| 接手 WebUI | `UI/WebUI/docs/TEAM_HANDOFF.md` | **2026-08-19 更新**(分支、Windows/macOS 兩台機的差異、測試層) |
| 搞懂「一顆零件從進到被吹走」的完整流程與時序預算 | `Peripheral/uInspESP32/docs/MACHINE_FLOW.md` | **現行,最重要** |
| 追「判定為什麼變了 / 為什麼沒判定」 | `InspectionCore/docs/UINSP_VERDICT_PATH.md` | 現行 |
| 韌體任何實作都必須滿足的契約 | `Peripheral/uInspESP32/docs/FIRMWARE_CONTRACT.md` | 現行 |
| frame↔object 配對的現況與下一步 | `Peripheral/uInspESP32/docs/PAIRING_MIGRATION_STATUS.md` | 現行 |
| 配對到底被證明了什麼(證據等級) | `Peripheral/uInspESP32/docs/PAIRING_VALIDATION_2026-08-06.md` | 現行 |
| 併發 / thread safety | `Peripheral/uInspESP32/docs/CONCURRENCY_ANALYSIS.md` | 靜態分析 |
| 可靠性的長期規劃 | `Peripheral/uInspESP32/docs/RELIABILITY_ROADMAP.md` | 規劃 |
| **韌體還差什麼才算 dev complete(接手先讀這份)** | `Peripheral/uInspESP32/docs/DEV_COMPLETE_CHECKLIST.md` | **現行,2026-08-12** |
| 量測引擎(caliper / search point / 定位) | `InspectionCore/docs/measurement_pipeline_and_caveats.md` | 現行 |
| search point(定位/量測點)的語意與 rework | `InspectionCore/docs/search_point_rework.md` | 現行 |
| 系統與量測引擎 rework 的完整流水帳 | `InspectionCore/docs/PROJECT_NOTES.md` | 歷史紀錄 |
| 影像傳輸 / JPEG 線路格式 | `InspectionCore/docs/IMG_TRANSFER_JPEG.md` | 現行 |
| 日誌系統與 WebUI 整合 | `InspectionCore/docs/LOGGING_WEBUI.md` | 部分未實作 |
| **一個「排程問題」其實是記憶體洩漏的完整過程** | `InspectionCore/docs/POSTMORTEM_2026-08-10_stall.md` | 現行 |
| **報告路徑長延遲的完整調查與修正** | `InspectionCore/docs/REPORT_2026-08-10_latency.md` | 現行 |

### 只是設計 / 尚未實作(不要當成現況)

讀之前先看它自己的 Status 行。這些描述的是**打算做的事**,不是機器現在的行為:

- `InspectionCore/docs/caliper_primitive_locating_design.md` — DESIGN
- `InspectionCore/docs/pure_sbm_def_design.md` — DESIGN / PLAN ONLY
- `InspectionCore/docs/sbm_setup_studio_plan.md` — PLAN
- `InspectionCore/docs/obj_detect_region_design.md` — P1+P3 已上線,P2 未做
- `InspectionCore/docs/BACPAC_MACHINECONTEXT_REFACTOR.md` — P0 已上線(`309a762b`),P1+ 未做;每階段 golden 把關
- `InspectionCore/docs/OPENCV_MIGRATION.md` / `_OPEN_QUESTIONS.md` — 長期遷移
- `Peripheral/uInspESP32/docs/GATE_DEBOUNCE_TEST_PLAN.md` — 測試計畫
- `Peripheral/uInspESP32/docs/HW_VERIFICATION_CHECKLIST.md` — **每一項都還沒驗證過**

### 特定日期的快照(只在對照那天的資料時才有用)

- `InspectionCore/docs/HEADLESS_TEST_FIXTURE_2026-08-07.md` — 2026-08-07 的機台幾何
- `Peripheral/uInspESP32/docs/PAIRING_VALIDATION_2026-08-06.md` — 該日的證據
- `InspectionCore/docs/HANDOVER_2026-08-13.md` — 已被接續;只在追那天的事時讀
- `InspectionCore/docs/HANDOVER_2026-08-16.md` — 已被 `HANDOVER_2026-08-18.md` 接續;只在追那兩天的事時讀
- `InspectionCore/docs/REPORT_2026-08-11_fullframe_wedge.md` — full frame 拍 3-25 張後停機;**未解**,含排除項與四個被撤回的結論

---

## 4. 三種文件,三種讀法

搞混這個會浪費很多時間:

| 類型 | 例子 | 怎麼讀 |
|---|---|---|
| **追加式紀錄**(`*_CAVEATS.md`) | 62KB / 68KB,只增不改 | **不要通讀**。`grep` 你的症狀關鍵字。新發現往後面追加,不要改寫舊條目——舊條目記錄的是當時的真相 |
| **現況文件** | `MACHINE_FLOW.md`、`ARCHITECTURE.md` | 描述機器**現在**的行為,可以修改。發現不符就當場改 |
| **設計 / 計畫** | `*_design.md`、`*_plan.md` | 描述**打算**做的事。看 Status 行,不要當成現況 |

---

## 5. 現在的狀態

**狀態不寫在這裡,寫在 handover。** 本頁是不常變的地圖;會動的東西(現在做到哪、
下一步、哪些會誤導你)放在**最新一份 `InspectionCore/docs/HANDOVER_*.md`**——
目前是 `HANDOVER_2026-08-18.md`。每輪 sprint 收尾時只更新那份,這樣這段永遠指向
真相,而不是某天的快照。開工前讀那份的 §1 State 與 §2 What will mislead you。

- **開什麼工 / 排序**:`HANDOVER_2026-08-18.md` §3 TODO
- **有哪些 bug、修了哪些**:`AUDIT_BACKLOG_2026-08-18.md`(本輪,逐項標註 VERIFIED/REPORTED)
  與 `AUDIT_BACKLOG_2026-08-15.md`(上一輪,仍有未結項)
- **怎麼驗**:`REGRESSION_TESTS.md`

分支目前是 `ct/win-bench-bringup`(此段隨 handover 更新)。

---

## 6. 寫文件的規矩

- **量測寫數字,不寫形容詞。** 「很慢」沒有用,「1372.6ms,預算 792ms」有用。
- **寫下是什麼量測殺掉了哪個假設**,不只寫結論。下一個人才不會重跑同一個實驗。
- **錯了就寫錯了。** postmortem 裡我保留了自己六個錯誤假設和一次「這不是我們的
  bug」的誤判——那些比正確答案更能省下時間。
- **CAVEATS 只追加。** 舊條目是當時的真相,不要事後修飾。
