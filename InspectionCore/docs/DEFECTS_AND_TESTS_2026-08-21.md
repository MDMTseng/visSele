# 缺陷清單與測試清單 —— 2026-08-21

`BENCH_WORKLIST_2026-08-19.md` 是**順序**(先做什麼);這份是**盤點**(有什麼、
什麼狀態)。兩份都在維護:工作清單回答「現在做哪一項」,這份回答「總共欠多少」。

狀態:**已修驗證** / **已改碼未燒** / **未修**。
「已改碼未燒」只有韌體有 —— core 改完就重建了,韌體要燒錄才生效。

---

## A. 缺陷清單

### A0 已修並驗證(2026-08-20/21)

| # | 缺陷 | 位置 | 驗證方式 |
|---|---|---|---|
| 0.1 | `recv_ERROR` / `recv_RESET` 宣告 `int` 無 `return`,-O2 編成一條 `ud2`。**呼叫到就是行程死亡**;按板子 reset 就會殺掉 core | `wiringPanel.cpp:1519,1524` | `objdump` 掃描 **0** 個 `ud2` 開頭函式;`quote_crash.mjs` **3/3** 存活,且裝置 `error_hist` = `[11,11,11]` 證明觸發三次都真的發生 |
| 0.1b | `sampleAngleOffset(acv_XY)` / `nodeInfoIdxCorrection` 兩個同型空殼 | `common_lib/ImageSampler.cpp:627,745` | 同上掃描。兩者今天無呼叫者,給的是**有定義的中性值**(`preOffset` / `targetIdx`),不是猜出來的語意 |
| 0.1c | `-w` 全域關警告,這類缺陷因此無聲存活 | `CMakeLists.txt:57` | 加 `-Werror=return-type`;全樹重建通過 = 樹裡沒有其他違規 |
| **0.2** | **RESYNC 復原是單向的。** `send_RESET()` 只治對端;core 自己的 parser 卡在 `ERROR_SEC`,只認**收到**的 RESET_PACKET,而裝置回的是普通的 `RESET_OK` frame。結果:core 永遠 latch 住,RESYNC 每 9 秒重試到天荒地老,對面板子完全健康 | `Data_Layer_Protocol.hpp/.cpp`、`wiringPanel.cpp:6249` | 新增 `request_rx_resync()`。log 抓到復原瞬間:`RESET_PACKET sent` → `RESET_OK` → `pong id=252`。`bareboard_up.mjs` 隨後一次到位 **READY t+9s** |

> **0.2 是 0.1 遮出來的。** 兩者在同一條路徑上:core 開 COM3 → DTR 重開板子 →
> boot ROM 用 115200 印訊息而 core 以 230400 讀 → frame 外垃圾。修掉 `ud2` 之後
> core 不再死,露出來的是它也**永遠不會復原**。

**0.2 的已知代價(寫在註解裡)**:旗標只在下一批位元組抵達時消化。耦合是對的
(沒有流量就沒有東西可重新同步),但對一條完全靜默的對端救不回來。

### A0b 燒錄當天發現並修掉的 core 缺陷 —— 2026-08-21

| # | 缺陷 | 位置 |
|---|---|---|
| **0.3** | **core 開埠時拉起 RTS,把板子按在重置。** flow=none 時 `simple_uart.c` 設 `RTS_CONTROL_ENABLE`,而這塊轉接板的 RTS 直接接 ESP32 的 EN。**core 一開埠,板子就送不出任何位元組** —— `INSP_PERIF_RAW=1` 實測 0 bytes,而同一時刻用 `rts=False` 開同一個埠可以收到每秒一個 CRC 正確的 SYSTIME。已修成 `RTS_CONTROL_DISABLE` / `DTR_CONTROL_DISABLE`,鏈路隨即 UP | `contrib/simple_uart/simple_uart.c:265,270` |

**這條改寫了 0.2 的病歷。** 先前那些「core 連上但鏈路永不同步、RESYNC 每 9 秒重來、
停掉 core 板子就完全健康」的現象,原因是**停掉 core 讓 RTS 掉了下來**,不是 parser
被治好。0.2(RESYNC 單向復原)仍然是真實且必要的修正 —— 裝置回的是普通
`RESET_OK` frame,core 的 parser 確實需要自己的解鎖路徑 —— 但它**不是**當時讓鏈路
活過來的原因。那次「修好之後就通了」是巧合:當下 RTS 剛好是低的。

---

### A1 韌體 P1 —— **已於 2026-08-21 燒錄並驗證**

| # | 缺陷 | 位置 |
|---|---|---|
| ~~1.1~~ | **✅ 已燒錄並驗證 2026-08-21** —— 裝置現在送出的是合法 JSON,九個位元組是十六進位:`{"dbg":"recv_ERROR:1 {'type':'ping'} dat:7B 22 74 79 70 65 22 3A 22"}`(即 `{"type":"`),core 丟棄的畸形 frame 數 **0**。`recv_ERROR` 把未逸出的位元組塞進 `{"dbg":"..."}`,一個 `"` 就讓裝置**從自己的錯誤處理裡送出壞 frame**。同一行的 `string((char*)recv_data,0,9)` 還是**越界讀**(先轉 `std::string`,一路讀到 NUL,而那個緩衝區既非文字也無終止符)。改成 `dataL` 界定的十六進位傾印,兩個一起解 | `LegacyFirmware.cpp:4295` |
| ~~1.2~~ | **✅ 已燒錄並驗證 2026-08-21** —— 直接走序列埠:`{"x":{}}` / `{"x":[]}` / 兩者並存都回 pong,`error_hist []`。`proto_fuzz` 的 `trailing garbage` 與 `duplicate keys` 兩個 latch 一併消失。`json_seg_parser` 表達不了 `{}` 和 `[]`。合法 JSON 被判格式錯誤 → `SERIAL_PROTOCOL_ERROR` latch → **板子還在發 SYSTIME 但不收指令,停不下來**。現實觸發:WebUI 存檔時某群組未改動而送 `"plate":{}` | `src/comm/json_seg_parser.cpp` |
| ~~1.3~~ | **✅ 已燒錄並驗證 2026-08-21** —— `cmd_sweep --phase 1`:**0/20**(原本 20/20 沉默),回覆帶回打錯的字 `{"err":"unknown_type","type":"palte","ack":false}`。未知 `type` **完全不回應**(不是 `ack:false`,是零位元組)。呼叫端分不出「你打錯字」和「板子死了」。這才是 `plate.freq` 陷阱的真正機制 | `LegacyFirmware.cpp` dispatch 鏈尾 |
| ~~1.5~~ | **✅ 已燒錄並驗證 2026-08-21** —— `{"type":"trig_report","on":true,...,"ack":true}`,`cmd_sweep` 獨立複驗。**`trig_report` 成功卻回 `ack:false`。** `doRsp=true` 有設,`rspAck` 從來沒設。一個能用卻回報失敗的指令比壞掉的更糟 —— 檢查 ack 的呼叫端(`fw_tolerance.mjs` 就是)會斷定機器拒絕了,然後去找一個不存在的原因。**2026-08-21 新增**;掃過整條 dispatch 鏈,同型只有這一個 | `LegacyFirmware.cpp:7235` |

### A2 未修

| # | 缺陷 | 位置 | 嚴重度 |
|---|---|---|---|
| ~~1.4~~ | synth sender 是永生 detached thread | **✅ 已修並驗證 2026-08-21** —— 加 `synthSenderStop`/`synthSenderDone` 與 `retireSynthSender()`,**由 `delete_PeripheralChannel` 先問再決定要不要釋放**。實測順序 `DELETING` → `retired in 1ms` → `DELETED...`。<br>**自審時補強的一點**:`delete_PeripheralChannel` 的安全論證是「每個使用者都在 `perif_tx_lock` 內重讀全域 `perifCH`,所以指標換成 NULL 之後沒人再拿得到它」—— 而 synth sender **捕獲 `this`、從不重讀全域**,那個不變式不保護它。所以逾時路徑改成**故意洩漏該通道**並 LOGE,而不是釋放一塊仍在被寫入的記憶體:一個被遺棄的通道是有界成本,未定義行為不是 | `wiringPanel.cpp:1293,1580,2937` |
| **2.0** | **WebUI 無限重發 RESYNC。** `LINK_RESYNC_MAX = 2` 沒有擋住它 —— 鏈路健康時仍每 9 秒發一次。台架上一個忘了關的分頁就會一直對機器送 RESYNC | `UI/WebUI/src/perif/PerifAPI.js:685` | 新增 2026-08-21 |
| ~~2.1~~ | console 行長上限 4096 > 裝置 frame buffer 2048 | **✅ 已修並驗證 2026-08-21** —— 上限降到 1900 並**拒絕而非截斷**。實測回 `{"err":"line over 1900 bytes -- refused, not truncated (device frame buffer is 2048)"}`。原本超長會靜靜丟掉尾巴,送出的東西開頭仍是 `{` 所以守門放行,裝置收到截斷 frame —— 正是會 latch 它的輸入 | `wiringPanel.cpp` |
| ~~2.2~~ | `!TL` 注入無條件被拒,而且回報成功 | **✅ 已修並驗證 2026-08-21** —— `d.size = payload.size() + 1`,並改成回報真實結果。實測 `{"core":"PD","injected":true,"rc":0}`(原本永遠是被拒卻回「injected」)。**`!pd` 注入現在真的會動** —— `perif_hold.mjs` 存在的唯一理由就是這條;能否取代它需在無 BPG 客戶端的情況下另測 | 同上 |
| ~~2.3~~ | 非 JSON 錯誤回覆寫死 100 bytes,字面值 91 | **✅ 已修並驗證 2026-08-21** —— 改用 `sizeof(lit)-1`,長度不可能再和文字脫節。實測回覆 90 字元 + 換行 = 91 bytes,與字面值一致(原本每一行畸形輸入都讀過字串常數尾端 9 bytes) | 同上 |
| ~~2.4~~ | 第二個 console 客戶端靜默排隊,指令延後執行 | **✅ 已修並驗證 2026-08-21** —— 在讀取迴圈內用零逾時 `select` 輪詢 listen socket,發現第二個客戶端就直接回絕。實測回 `{"err":"console busy -- one client at a time"}`。原本它會在 backlog 裡看似連上、毫無回應,等第一個離開後把累積的指令**一次全部執行**到一台會移動零件的機器上 | 同上 |
| 2.5 | `get_version` 走 framing 層回覆並丟掉呼叫端 id | `LegacyFirmware.cpp:5131` | 2026-08-21 直連 COM3 再次實證:`ping` / `get_running_stat` 都回,只有它 NO REPLY |
| 2.6 | SEL trace 事件 tid 恆為 0 —— **儀器缺口**,沒有 tid 就只能量脈衝寬度,不能量「這一顆零件的 SEL 有沒有在對的時間開」 | `LegacyFirmware.cpp:3306-3368` | |
| ~~2.7~~ | `FindInspShapeObject` 預設參數陷阱 | **✅ 已修 2026-08-21** —— 五個區塊複製了同一個 bug。JS 預設參數只在引數是 `undefined` 時生效,而那正是「屬性不存在」的樣子:報告缺少 `detectedLines` 時,`FindShapeIdx(id, inspReport.detectedLines)` 不是搜尋空的,而是搜尋**編輯器自己的 `shapeList`**(被編輯的形狀一定在裡面),回傳一個屬於完全不同陣列的索引。改成先 `Array.isArray` 守門的迴圈,並在 `FindShape` 加上非陣列即回 `undefined` 的防線 | `InspectionEditorLogic.js:793,1029` |
| 2.8 | `webctld` 瀏覽器死掉不重建;預設 URL 是 8080 | `tools/webctl/webctld.mjs` | 是 C1 三個間歇的線索 |
| ~~2.9~~ | 面板不顯示 `SEL_SUPPRESSED` / `SEL1_NO_QUOTA` | **✅ 已修 2026-08-21(非零情境待現場重現)** —— 沿用面板既有慣用法「歸零時安靜,非零時佔一整行」,顏色用紅色而非其他警告的 `#c60`:那些說「沒人判定這顆零件」,這條說「判定沒有落實到零件」。`SEL1_NO_QUOTA` 是被吹除額度吃掉的 NG —— 一顆判定不良卻留在良品流裡、且不計入 `SEL1_Count`(對帳基準)的零件。帶 `data-testid="uinsp-unsorted"`,`uinsp_live_counts.mjs` 已更新為會讀它。**歸零不顯示已實測**;非零渲染是 `cnt.SEL_SUPPRESSED > 0 &&` 的直接守門,但今晚無法穩定製造出非零(判定一直落在 NA,快餵則掉 112),留待現場 | `uInspESP32_UI.jsx` |
| ~~2.8b~~ | `cam_1.cur_width/height` 回報未初始化記憶體 | **✅ 已修 2026-08-21** —— 兩邊都改:`fullFrameW/H` 給定初值 0(它們只由 `SET()` 寫入,`CLEAR()`/`RESET()` 都不碰,沒載入校正時就是垃圾);欄位改名為 `calib_frame_w/h`,因為值來自**校正圖**不是相機,叫 `cur_width` 會誘導下一個人把它讀成即時感光元件尺寸。`UI/WebUI/src` 無讀者,改名零成本。binary 確認 `cur_width` 已消失 | `ImageSampler.h:117` · `wiringPanel.cpp:3956` |

### A3 新缺陷 —— 2026-08-21 協定 fuzz 打出來的

裝置 latch 的定義:`healthy_after=false`,之後要 `clear_error` 才回來。
每一項都是**合法或半合法輸入讓機器停下來**。

| 探針 | 結果 | 判讀 |
|---|---|---|
| deep nesting ×60 | **latch**,3.2s / 1 次 | 巢狀深度沒有上限檢查,超過就進錯誤態 |
| deep array ×60 | **latch**,3.2s / 1 次 | 同上,陣列側 |
| unterminated string | **latch**,3.2s / 1 次 | 未閉合字串沒有長度上限 |
| trailing garbage | **latch**,**9.6s / 3 次** | 這正是 `quote_crash` 的形狀。慢復原那一類 |
| **duplicate keys** | **latch**,**9.7s / 3 次** | **2026-08-21 補測到的**,先前 run 在它之前就中止了 |
| **leading comma** | **latch**,3.2s / 1 次 | 同上,補測 |
| NUL right after brace | **latch**,救得回 | console 行組裝器把 NUL 送進裝置 |
| valid ping,一次一個位元組 | **完全無回應**,但板子健康 | 分片送達的**合法** ping 沒有答案 —— 分片路徑有問題 |
| tab and vtab | 無回應,板子健康 | 由缺陷 1.3 解釋(未知 type 沉默) |
| 其餘 12 個(兩個物件一行、超長 key、超長字串、unicode escape、raw control char、超大數字、負零/指數、bare true、頂層陣列、空白洪水、NUL in the middle、CR inside line、deep nesting ×8 ×20) | 通過 | |

**測試工具本身的缺陷 —— 已修 2026-08-21。** `proto_fuzz.mjs` 只送一次
`clear_error`、等 1.5 秒、檢查一次,然後宣告 **UNRECOVERABLE 並中止整個 run**。
它在第 6 個探針(共 18 個)就停了。改成重試六次並**回報復原耗時**之後:

* `trailing garbage` **9.6 秒 / 3 次就回來了** —— 先前那個「救不回來」是純粹的沒耐心
* 後面 12 個探針因此第一次跑到,其中 **`duplicate keys` 是新的 latch**

復原耗時本來就是資料:「latch 但 1.4 秒回來」和「latch 且要 11 秒」是對同一個探針的
兩種不同答案,舊的形狀兩種都表達不了。

### A4 硬體 —— 現場處理

| 項目 | 狀態 |
|---|---|
| CAM1 硬體觸發訊號不到相機 | ESP32 GPIO17 確認在切換(`pin_read` 0↔1,framed 協定重測過);相機 free-run 正常(5/5 @ 816×528);相機 Line0/1/2 在 GPIO17 切換時**電位完全不動**。open-drain 釋放時 GPIO17 讀到 **0** → 那一端浮空,線在**板子這一側**就沒接上。**需要現場拿電表** |
| 相機 IO 能力 | Line0=Input;Line1=Strobe(可輸出);Line2=Input,宣稱可 Strobe 但 `LineMode` 一律 `0x80000106`(寫死輸入)。反向測試因此做不成 |

---

## B. 測試清單

### B0 已完成

| 測試 | 結果 |
|---|---|
| A1 指令面掃描 | 指令面是 **56** 不是 70;找到 1.2 / 1.3 / 2.5 |
| A2 協定 fuzz | **2026-08-21 補完** json + line 兩組;找到上表 6 個新 latch |
| A3 IO 時序 | 首次自動化驗證;寬度與偏移吻合到 pulse,改設定會跟著動 |
| B1 console 協定面 | 9 case,5 個缺陷 + 1 次 core 當機 |
| B2 perif link 生命週期 | 六階段全過 |
| B3 既有 core 測試重跑 | 5/5 綠 |
| B4 `link_fault` 還 slot | 已修,並補上 `perif_hold.mjs` |
| C2 dev 警告 | S11 已修並 PASS;T6 揭出底下的真 bug(2.7) |
| `quote_crash` 驗收 | 3/3,觸發經 `error_hist` 證實 |
| `ud2` binary 掃描 | 0 |
| 裸板冷機到 READY | `bareboard_up` READY t+9s,valid=true,offset_us=800,learned=8 |

### B1 待做

| # | 測試 | 狀態 |
|---|---|---|
| ~~3.1b~~ | A2 剩下的 shapes | **✅ 完成 2026-08-21** —— 18/18 全跑完,新增 `duplicate keys` / `leading comma` 兩個 latch |
| ~~3.2~~ | A4 故障注入與恢復 | **✅ PASS 2026-08-21** —— 新工具 `fault_recover.mjs`。`wdt_test` 與 `crash_test` 各自:板子重開(uptime 倒退)、NVS 完好、**鏈路自己回來(9.1s / 1.5s)**。`fault` / `faultSkipTrig` / `faultTidOffset` 尚未涵蓋 |
| 3.3 | C1 三個間歇 | **部分完成 2026-08-21 —— 清單本身過期了**,見下 |
| 3.4 | C3 板子在線的 WebUI 路徑:uInsp 面板即時計數、統計歷史清空與匯出、`cat_ok`/`cat_ng` 改動後 UI 是否跟上 | 未做 |
| ~~3.5~~ | D1 fw 容忍度參數 | **✅ PASS 2026-08-21** —— 新工具 `fw_tolerance.mjs`。`stop_only`+`stop_after=5` 停在 **unanswered=5 / consec=5**(剛好門檻);`none` 餵 15 顆未判定**從不停機**。兩案例互相否證,證明 policy 真的被讀取。順帶抓到缺陷 1.5 |
| 3.6 | D2 真實圖像與 def 檔 | **等使用者提供**,已提四次 |
| ~~4~~ | 全面回歸 | **✅ 綠 2026-08-21**,見下表。未跑:`cmd_sweep`(它的 phase 1b 是 1.3 的燒錄後驗收)、`io_timing`(2.6 修好後才完整) |

### B1b 3.3 的實測結果 —— 「三個間歇」已經不是三個了

先修並驗證了 2.8 那條線索(見缺陷表),然後實際跑:

| | 結果 |
|---|---|
| `doorbell` | **PASS** —— suppression / triplet / perif 三項子檢查全過 |
| `r7_inspbug` | **PASS**(完整套件中) |
| `r6_inspection` **T1** | **PASS** —— 而 T1 正是被記為間歇的那一個 |

`r6_inspection` 仍然失敗,但**失敗的位置換了,而且是穩定重現的**:T2 / T4 / T5,
單獨跑和批次跑一樣,從 `toMain` 正規化過的 MAIN 起始狀態跑也一樣。T3 當下讀到的
還是 `MAIN`,到 T4 就變成 `SPLASH` —— 所以是 **T2 的動作把 app 推到 SPLASH**。

**確定性失敗和間歇是兩種問題**,後者難追前者不難。清單上「三跑 1 過 2 敗」那個
描述已經不對了。

**完整套件 = 基準線**:`qa/run.mjs` 跑出 **35 PASS / 0 SKIP / 4 FAIL / 182.9s**,
與 2026-08-19 的 35/0/4/187s 完全一致,失敗的也是同樣四個
(`r4_purelib`、`r6_decorator`、`r6_inspection`、`r10_smoke`)。
**所以這幾天對共用檔的所有改動沒有造成任何回歸** —— 階段 4 的 WebUI 部分等於順帶做掉了。

排除掉的假設:頁面死亡不是原因。整輪套件跑完 `/health` 的 `rebuilds` 仍是 1
(就是我手動殺 Chromium 那次),期間沒有任何頁面死掉。

### B1c 階段 4 全面回歸 —— 2026-08-21

改完階段 0/1/2 之後重跑。動到的共用檔:`wiringPanel.cpp`、`Data_Layer_Protocol.hpp/.cpp`、
`ImageSampler.h`、`CMakeLists.txt`、`InspectionEditorLogic.js`、`uInspESP32_UI.jsx`。

| 測試 | 結果 |
|---|---|
| `console_abuse --skip-risky` | **8 cases, 0 FAIL, 0 FINDING** —— 這支當初找出 5 個缺陷 |
| `qa/run.mjs` | **35 PASS / 0 SKIP / 4 FAIL / 184s** = 基準線,失敗的是同樣四個 |
| `bpg_sweep --include-crashers` | **35/35** |
| `churn` | 90 次,longest freeze 0s,core GS 有回應 |
| `fd_leak` | **-0.07 handles/attempt** |
| `slow_client` | A 維持 60/s,B 慢讀不影響 |
| `link_fault` | 六階段完成,`suspect=false`、`tx_fail_consec=0`、slot 已歸還 |
| `latch_loop --cycles 40` | **core survived all 40**(latched 35, recovered 27) |

`console_abuse` 用它自己的方式確認了每一條 console 修正:
`nonjson_reply` 回 **91 bytes**(2.3);`second_client` 的 B **`executed late=false`**(2.4);
`latch_threshold` **no probe latched**,2040/2100 bytes 都 `answered=false` 而不是把裝置打 latch(2.1);
`tl_edges` **injected=1**(2.2)。

#### 我自己引入又修掉的一個競態,記下來

2.4 的第一版把「有沒有第二個客戶端」的輪詢放在讀取迴圈**開頭**。那是錯的:
客戶端關閉到迴圈察覺之間有一個窗口(一次 5ms sleep),在那裡輪詢會把**下一個**
客戶端擋在一個已經沒人持有的 slot 外面。

`console_abuse.mjs` 每次 liveness 檢查都刻意開一條新連線(就是為了抓「slot 被洩漏」),
於是第一個 case 就踩中,回報成「console 死了 30 秒」。

修法是把輪詢移進 `EWOULDBLOCK` 分支 —— 到那裡代表在位客戶端**確實連著而且只是安靜**,
這時候有連線待接才真的是第二個客戶端。移完之後 8/8 全過。

### B2 被硬體擋住

| 測試 | 擋住的原因 |
|---|---|
| 記憶體 soak(驗 `deQ` + history 兩個修正) | 需要真實負載 = 需要相機出圖 = 需要觸發線 |
| `dv_bench`、`rc_hammer`、`qwatch`、真實檢測 | 同上 |
| 轉盤實測(任何要 `plate_freq_meas > 0` 的) | 沒有轉盤 |

---

## C. 台架注意事項(今天踩到的)

- **孤兒瀏覽器會佔住單一客戶端 slot**,所有工具被擋在門外,而症狀是「channel
  exists but the device did not answer」。用 `INSP_ALLOW_MULTI_CLIENT=1` 繞過,
  或關掉分頁。
- **core 的日誌不走 stdout。** `insp.log disabled (persist OFF)` —— 都在共享記憶體
  環裡,用 `logdump.mjs` 取。`INSP_PERIF_LOG=1` 才有 `[perif RX]/[perif TX]`。
- **RTS 拉起會把板子按在重置 —— 這條我先前記反了,以這條為準。**
  `simple_uart.c` 在 flow=none 時設 `RTS_CONTROL_ENABLE`,也就是**只要埠開著就一直
  拉起 RTS**。這塊台架轉接板的 RTS **直接接到 ESP32 的 EN**(沒有自動下載電路的
  電晶體對),所以 core 一開埠,板子就不再送出任何位元組。

  2026-08-21 四種線態實測,板子沉默的充要條件是 RTS 拉起:

  | | DTR=0 | DTR=1 |
  |---|---|---|
  | **RTS=0** | 運轉 | 運轉 |
  | **RTS=1** | **沉默** | **沉默** |

  **症狀不是「沒有資料」。** core 開埠、收到零位元組、parser 永遠拼不出完整 frame,
  於是看門狗每 9 秒要求一次 RESYNC —— 而只要任何東西**關掉**那個埠、讓 RTS 掉下來,
  板子看起來就完全健康。這讀起來完全像協定故障,而它不是。先前那些「core 連上但
  鏈路永不同步」的紀錄,原因都在這裡。

  已修:flow=none 時改設 `RTS_CONTROL_DISABLE` / `DTR_CONTROL_DISABLE`。
  `uinsp_test.py` 的 `UInspLink` 一直都在開埠前把兩條線拉低,core 從來沒有。
- **板子的序列協定是 brace-framed JSON + CRC16-CCITT**,裸文字指令會被回
  `recv_ERROR:1` 靜靜丟掉。用 `uinsp_test.UInspLink`(它同時把 DTR/RTS 拉低,
  所以**不會重開板子**)。
- **接了真相機就不可以 `taskkill /F`**,用 `tools/stop_core.ps1`。
- **原始碼裡不能有 NUL 位元組,否則 git 會把整個檔案當成二進位。**
  這個倉庫設了 `core.autocrlf=true`(見 `.gitattributes`):倉庫存 LF,Windows
  工作區是 CRLF,而 git 在比對時會正規化回來 —— 所以**工作區是 CRLF 完全正常,
  diff 也是乾淨的**。

  但 `* text=auto` 的判準是內容:檔案裡出現一個 NUL,git 就當它是二進位,
  **正規化因此停止**,CRLF 工作區於是每一行都不同。實測 `LegacyFirmware.cpp`:
  有 NUL 時 CRLF 工作區 diff 是 **9603/9558**,拿掉 NUL 之後 CRLF 和 LF 都是 **46/1**。

  2026-08-21 自審時抓到,而那個 NUL 是當天寫進去的:
  `if(hn>0) hex[hn-1]='<NUL>';` —— 本該是 `'\0'`。
  **編譯器接受它**(內含 NUL 的字元常數值就是 0),韌體照樣建置成功,
  所有測試全綠。只有 `git diff` 看得出來。

  檢查:`python -c "print(b'\x00' in open(P,'rb').read())"`。

- **工具鏈的兩個反斜線陷阱(就是上面那個 NUL 的來源)。**
  1. 這裡的 shell heredoc 即使加引號也會吃掉一層反斜線,所以 Python 原始碼裡的
     `'\\0'` 會變成 `'\0'`,再被 Python 解讀成**真正的 NUL 字元**寫進檔案。
     需要字面反斜線時用 `chr(92)` 組出來。
  2. `io.open(p,'w')` 是文字模式,寫入時把 `\n` 翻成 `os.linesep`。這**不會**
     造成假差異(autocrlf 會處理),但改檔時用 `open(p,'wb')` 仍然比較乾淨。
- **`cfg_crc` 不是 NVS 身分。** 它涵蓋的是**當下生效**的設定,所以任何沒有
  `save_setup` 的執行期修改(`bareboard_up` 就會設 `plate.freq=15000`)在重開機後
  都會讓它改變。`fault_recover.mjs` 第一版把它算進 NVS 比對,於是回報了一個
  不存在的韌體缺陷;查 `plate.freq` 發現它回到 0,那就是全部的解釋。
  身分要看 `machine_id` / `io_on_level` / `cfg_from_nvs`。
