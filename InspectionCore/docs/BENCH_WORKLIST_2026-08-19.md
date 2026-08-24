# 裸板工作清單 —— 依序處理

寫於 2026-08-19,取代 scratchpad 的 `REMAINING_TESTS.md`(那份只有測試,沒有這一天
挖出來的缺陷)。這份是**單一事實來源**:缺陷與測試放在同一條順序上,因為其中兩項
測試被一個缺陷擋住,分開列會看不出來。

證據在 `CONSOLE_ABUSE_2026-08-19.md`(逐項重現步驟)與
`AUDIT_BACKLOG_2026-08-18.md`(P1 條目)。這份只講**做什麼、什麼順序、為什麼是這個
順序**。

狀態只有三種:**未做** / **進行中** / **已完成**。

---

## 順序總覽

```
階段 0  F1              <- 擋住 A2 剩餘 與 A4,先做
階段 1  產品 P1 四項     <- 和 F1 同一次重建一起進
階段 2  其餘缺陷八項     <- 各自獨立,可拆可併
階段 3  剩餘測試六項     <- A2/A4 等階段 0,其他隨時可做
階段 4  全面回歸         <- 改完之後,今天綠過的東西要重跑
```

---

## 階段 0 — 解除阻塞

### 0.1 `recv_ERROR` / `recv_RESET` 的 `ud2` — **已完成 2026-08-20** — P1

`wiringPanel.cpp:1496,1500`。宣告 `int` 卻沒有 `return`,gcc -O2 把整個函式編成一條
`ud2`。**呼叫到就是行程死亡。**

```cpp
int recv_RESET()                    { /* 空 */ }
int recv_ERROR(ERROR_TYPE errorcode){ /* 空 */ }
```

**修法**:各補 `return 0;`。同時加 `-Werror=return-type`,順手把
`ImageSampler.cpp:628,746` 兩個同型空殼(`sampleAngleOffset(acv_XY)`、
`nodeInfoIdxCorrection`)一起補掉 —— 它們今天沒有呼叫者,但都在檢測管線裡。

**2026-08-20 更新 —— 這條已經不是理論了。** 接上真相機、用 WebUI 實際操作時,
**使用者按了板子上的 reset,core 就死了**(`insp_crash_9396_20260820_105559.dmp`,
RIP 同樣落在 `recv_ERROR`,呼叫端是 `Data_Layer_Protocol.cpp:237` 的
`INIT_CHAR_ERROR`)。

機制完全平凡:板子重開時 boot ROM 用 **115200** 印開機訊息,而 core 正以
**230400** 在讀 —— 於是 frame 外出現一串垃圾位元組。log 裡崩潰前最後幾行就是
`WARN:: UART:COM3 channel still has residue data length:34` 加一堆亂碼。

沒有 console、沒有 fuzz、沒有惡意輸入。**按一下 reset 鍵就會殺掉 core。**
`win-mingw-msys`(真相機那個 build)的 `recv_ERROR` 同樣是一條 `ud2` —— 同一份
原始碼,每個 build 都一樣。

**為什麼排第一,而且不只是「一個待修缺陷」**:A2 剩下的形狀和整個 A4 都是靠
**大量產生鏈路錯誤**來測的,而每產生一個就得重啟 core。在這條修好之前,那兩項
不是難做,是做不出乾淨結果。

**重建注意**:OpenCV 釘在 4.13.0(MSYS2 現在出貨 5,會在 Core0_1 編到一半炸)。

**驗收**:`quote_crash.mjs` 跑三次都不當;binary 掃描沒有 `ud2` 開頭的函式:

```sh
objdump -d visSele.exe | awk '
  /^[0-9a-f]+ </ { sym=$0; sub(/^[0-9a-f]+ </,"",sym); sub(/>:$/,"",sym); pend=1; next }
  pend==1 && /\t/ { if ($0 ~ /\tud2/) print sym; pend=0 }' | sort -u | c++filt
```

### 0.1 的收尾與**它揭出來的下一個缺陷** — 2026-08-20

原始碼裡 `recv_RESET` / `recv_ERROR` 早就補上 `return 0;` 了(清單落後)。今天補完的是
另外兩個同型空殼與那道防線:

- `common_lib/ImageSampler.cpp` 的 `sampleAngleOffset(acv_XY)`(回 `preOffset`,
  即角度表為空時的中性值)與 `nodeInfoIdxCorrection`(回 `targetIdx`,恆等修正)。
  兩者今天都沒有呼叫者,所以給的是**有定義的值**而不是猜出來的語意 —— 猜錯會把
  當機換成檢測管線裡一個安靜的錯誤偏移。
- `CMakeLists.txt` 加 `-Werror=return-type`。原本是 `-w`(全域關警告),這正是這類
  缺陷能活下來的原因;`-Werror=` 即使在 `-w` 之後仍會重新啟用該項診斷。

**全樹重建通過**,代表沒有其他 return-type 違規。驗收掃描:

```
objdump -d visSele.exe | (ud2-first functions)  ->  0
```

**`quote_crash.mjs` 3/3,而且這次證明觸發真的發生了。** 第一次跑是空測(當時鏈路
斷著,裝置沒產生壞 frame);修好鏈路後重跑,裝置的 `error_hist` 變成
**`[11,11,11]`** —— 三次 `SERIAL_PROTOCOL_ERROR`,正好三次嘗試。core 三次都活著。

**驗收兩項都過:** `ud2` 掃描 0 個 + 觸發三次不當機。

#### 新缺陷:RESYNC 之後鏈路永不恢復(F1 一直遮著它)

core 一啟動就開 COM3,DTR 讓板子重開;板子的 boot ROM 用 115200 印訊息而 core 以
230400 在讀,於是 frame 外出現垃圾 —— **這正是 F1 崩潰的那條路徑**。修掉 `ud2`
之後 core 不再死,露出來的是:它也**永遠不會復原**。

```
[ 4117] perif machine_type: uInspESP32 -> 1          <- 鏈路曾經是好的
[20173] perif CONNECT reuses the open channel ... (no port reopen, no device reset)
[26490] perif: link RESYNC requested -- RESET_PACKET sent, port left open
[35481] perif: link RESYNC requested -- ...          每 ~9 秒,無限
```

`wiringPanel.cpp:6249`。症狀:console(4099)對任何指令都沉默,
`perif_hold --status` 說「channel exists but the device did not answer」,
`bareboard_up.mjs` 停在「board never answered get_running_stat」。

**故障在 core 這一側,不在板子。** 同一時間停掉 core 直接開 COM3:

```
ping -> {'type':'pong','ack':True}     error_hist -> []     state -> 100
```

板子乾淨、沒有 latch、狀態正常。所以 RESET_PACKET 沒有讓 core 自己的資料層解開。

**這擋住 3.1 / 3.2 的程度和 F1 一樣** —— 兩者都需要一條能用的鏈路來反覆製造錯誤。

##### 已修 2026-08-20 —— `request_rx_resync()`

`Data_Layer_Protocol.hpp` / `.cpp` / `wiringPanel.cpp:6249`。RESYNC 現在治**兩端**:

```cpp
perifCH->send_RESET();        // 治裝置(它的 parser 認 RESET_PACKET 位元組)
perifCH->request_rx_resync(); // 治我們自己(裝置只回普通的 "RESET_OK" frame)
```

用旗標而不是直接呼叫 `RESET()`:parser 狀態屬於 UART 接收執行緒
(`Data_UART_Layer::recv_data_thread`),從 BPG 執行緒去清 `buffIdx` 是資料競爭。
`recv_data()` 在自己擁有的 frame 邊界消化這個旗標。

**代價要講清楚**:旗標只在**下一批位元組抵達時**才被消化。這個耦合是對的
(沒有流量就沒有東西可以重新同步),但也表示對一條完全靜默的對端,光靠這個
救不回來。

**實測復原**:

```
[50665] perif: link RESYNC requested -- RESET_PACKET sent
[50673] [perif RX] reply={"type":"RESET_OK","data":""}
[50677] [perif RX] reply={"type":"pong","id":252,"ack":true}   <- 通了
```

修好之後 `bareboard_up.mjs` 一次到位:**READY at t+9s, valid=true, offset_us=800,
learned=8**。

**RTS/EN 那個假設是錯的,記下來免得再走一次。** `simple_uart.c:245,248` 確實在
開埠時把 RTS/DTR 都拉起(`RTS_CONTROL_ENABLE` / `DTR_CONTROL_ENABLE`),而且沒有
人清掉,看起來完全像「core 持有埠期間板子一直被按在 reset」。**但板子照跑** ——
1548 個 RX frame、SYSTIME 連續、CAMSYNC 正常。這塊板的 RTS 沒有接到 EN。

##### 附帶發現:每 9 秒的 RESYNC 是孤兒瀏覽器發的

不是 core 的看門狗。`UI/WebUI/src/perif/PerifAPI.js:685`,而且 `LINK_RESYNC_MAX = 2`
顯然沒有擋住它 —— 鏈路健康時仍每 9 秒發一次,無限。台架上一個忘了關的分頁就會
一直對機器送 RESYNC。歸到階段 2,未修。

---

## 階段 1 — 產品 P1,建議與 0.1 同一次重建

### 1.1 裝置把未逸出的位元組塞進 JSON 除錯訊息 — **已改碼,未燒錄** — P1

`LegacyFirmware.cpp:4285`。`dataBuff` 有把 `"` 換成 `'`,`recv_data` 沒有:

```cpp
dbg_printf("recv_ERROR:%d %s dat:%s", errorcode, dataBuff,
           string((char*)recv_data,0,9).c_str());   // <-- 生的
```

`dbg_printf` 包成 `{"dbg":"..."}`,那九個位元組裡一個引號就讓字串提早結束。

**修法**:和 `dataBuff` 一樣逸出,或直接改成十六進位傾印(除錯用途下更好讀)。

**與 0.1 的關係**:兩個各自修都能斷鏈,**但兩個都是錯的**。0.1 讓 core 不會死,
1.1 讓裝置不會送出壞 frame。只修一邊會留下一個安靜的協定違規。

### 1.2 `json_seg_parser` 表達不了 `{}` 和 `[]` — **已改碼,未燒錄** — P1

`src/comm/json_seg_parser.cpp`。`{` 之後無條件推 `OBJ_KEY`,而 `case OBJ_KEY` 只接受
`"` 和空白,所以空物件的 `}` 是格式錯誤;`[` 推 `ARR_END, DAT`,空陣列的 `]` 落到
`case DAT` 最後的 `else` 被當成純量開頭。**兩者都是合法 JSON。**

格式錯誤 → `JSON_FORMAT_ERROR` → `SERIAL_PROTOCOL_ERROR` latch。板子**還在發
SYSTIME**,但不收任何指令 —— 從主機看它是活的,**停不下來**。

**現實觸發**:一次 WebUI 存檔,某個群組沒改動而送 `"plate":{}`。已實測會 latch。

**修法**:`OBJ_KEY` 在物件為空時接受 `}`(pop OBJ_KEY / OBJ_SEP / DAT,再 pop
`OBJ_END`,回 `OBJECT_COMPLETE`);`DAT` 在外層是 `ARR_END` 時同樣接受 `]`。

**驗收**:`proto_fuzz.mjs --group json` 的 `ping + empty array` / `empty object` /
`{}` 三個 case 不再 latch。

### 1.3 未知 `type` 完全不回應 — **已改碼,未燒錄** — P1

49 個真指令全部有回應;20 個非指令(含只差一個字母的錯字)**全部沉默** ——
不是 `ack:false`,是零。呼叫端無法分辨「你打錯字」和「板子死了」。

**這是 `plate.freq` 陷阱真正的機制。** 先前記成「acked 但被忽略」是錯的,
phase 3 對八個群組逐一試扁平寫法,沒有一個回 `ack:true`。

**修法**:dispatch 鏈尾補一個 `else`,回 `ack:false` 並帶上收到的 type。一個分支。

**驗收**:`cmd_sweep.mjs --phase 1` 的 phase 1b 從 20/20 沉默變成 0/20。

### 1.4 synth sender 是永生 detached thread — **未做** — P1(台架限定)

`wiringPanel.cpp:1293`。`std::thread([this]{ while(true) ... }).detach()`,沒有離開
條件、沒有 join,捕獲 `PerifChannel`。`delete_PeripheralChannel()` 在每次
DISCONNECT / reopen / 最後一個 BPG 客戶端關閉時釋放它。`synthSenderUp` 是成員,
所以每個新通道再起一條。

整段 gated 在 `INSP_CAM_TS_SYNTH=1`,**出貨機不受影響**。但:

> 崩潰**之前**它還能在死通道上送 verdict。**任何重連之後做的配對或計數量測都不
> 可信**,除非確定當時沒有洩漏的 sender。

**修法**:`std::atomic<bool> synthSenderStop`,`~PerifChannel` 設起並等 thread 認完。

**修好之前的權宜**:少做 DISCONNECT/CONNECT;跑完鏈路類測試重啟 core 再量數據。

---

## 階段 2 — 其餘缺陷,各自獨立

| # | 缺陷 | 位置 | 修法 | 狀態 |
|---|---|---|---|---|
| 2.1 | console 行長上限 4096 > 裝置 frame buffer 2048 | `wiringPanel.cpp` PerifConsoleThread | 上限降到 2048 以下,**超長回錯誤不要截斷後送出**(截斷後開頭仍是 `{`,守門必放行) | 未做 |
| 2.2 | `!TL` 注入無條件被拒,而且回報成功 | 同上 | 注入時 `d.size = payload.size() + 1`(`c_str()` 保證終止符,同步呼叫內指標有效) | 未做 |
| 2.3 | 非 JSON 錯誤回覆寫死 100 bytes,字面值 91 | 同上 | `sizeof(lit) - 1` 或 `strlen` | 未做 |
| 2.4 | 第二個 console 客戶端靜默排隊,指令延後執行 | 同上 accept 迴圈 | accept 後若已有客戶端就回 `{"err":"console busy"}` 並關掉;或改真多客戶端(才符合現有註解) | 未做 |
| 2.5 | `get_version` 走 framing 層回覆並丟掉呼叫端 id | `LegacyFirmware.cpp:5131` | 讓它走一般回覆路徑,或至少回填 id | 未做(2026-08-20 直連 COM3 再次實證:`ping`/`get_running_stat` 都回,只有 `get_version` NO REPLY) |
| 2.6 | SEL 的 trace 事件 tid 恆為 0 | `LegacyFirmware.cpp:3306-3368` | 傳入 `pli->tid`,和 CAM/L1A/L2A 一致 | 未做 |
| 2.7 | `FindInspShapeObject` 預設參數陷阱 | `UI/WebUI/src/UTIL/InspectionEditorLogic.js` | 每個區塊先判斷 list 存在;或讓 `FindShape` 被明確傳入 `undefined` 時不退回 `this.shapeList`(較徹底,要先確認沒有呼叫端依賴退回) | 未做 |
| 2.8b | `cam_1.cur_width` / `cur_height` 回報未初始化記憶體 | `wiringPanel.cpp:3886` | 兩個欄位取自 `calib_bacpac.sampler->getCalibMap()->fullFrameW/H` —— 是**校正圖**不是相機,名字卻叫 `cur_width`。實測值 `1936534903` = `0x736F6C77`(ASCII 位元組),重開相機後不變。目前 `UI/WebUI/src` 沒有人讀它,所以還沒害到人;但一個叫 `cur_width` 的欄位吐未初始化記憶體,下一個相信它的人就會中。要嘛改成真的相機尺寸,要嘛改名並確保 calib map 有初始化 | 未做 |
| 2.8 | `webctld` 瀏覽器死掉不重建;預設 URL 是 8080 | `tools/webctl/webctld.mjs` | `page.isClosed()` 時重建 context/page;預設 URL 改 8081 或啟動時大聲抱怨 | 未做 |

**2.6 值得單獨說**:它不是缺陷而是**儀器缺口**。沒有 tid,SEL 只能量「脈衝多寬」,
不能量「**這一個**零件的 SEL 有沒有在對的時間開」—— 而後者才是會抓到誤分料的量測。
補上之後 A3 才真的完整。

**2.8 是 C1 的線索**,見 3.3。

---

## 階段 3 — 剩餘測試

### 3.1 A2 協定 fuzz 剩餘 12 個形狀 — **未做** — **等 0.1**

`proto_fuzz.mjs --group json` 在第六個 probe 當掉 core 之後中斷。未測:
重複 key、超長 key、超長字串、unicode escape、raw control char、超大數字、
leading comma、頂層陣列、空白洪水、一行兩個物件。
`--group line`(NUL 截斷、逐位元組送達)整組還沒跑。

### 3.2 A4 故障注入與恢復 — **未做** — **等 0.1** — 破壞性,Track A 最後

`crash_test`、`wdt_test`、`fault`。每一個之後確認:板子重啟、NVS 設定還在
(`machine_id` / `io_on_level` / `cfg_from_nvs`)、core 能重新連上。
另有 `faultSkipTrig()`(整個吞掉 trigger)和 `faultTidOffset()` 可測。

### 3.3 C1 三個間歇 — **未做** — 最大的信任問題

`doorbell`、`r6_inspection` T1、`r7_inspbug` T1。三者單獨跑都綠。

**先驗 2.8 這條線索**:`webctld` 的 `page`/`context` 在模組載入時建立、之後永不重建。
瀏覽器或分頁一旦進壞狀態,之後每個套件都 FAIL,單獨重跑卻會過 —— 那正是這三個
間歇的特徵。我為了找 F3 殺掉 Chromium 時親眼看到整個 runner 在 164ms 內全數 FAIL。

**驗完再看** `ActionThrottle_type: 'express'`(`ActionThrottle.js` 對它短路)。

**性質是調查不是測試**,時間盒不住,建議另開時段。

### 3.4 C3 板子在線才能測的 WebUI 路徑 — **未做**

uInsp 面板即時計數、統計歷史的清空與匯出、`cat_ok`/`cat_ng` 改動後 UI 是否跟上。
(`hist_wiring.mjs` 已過。)

### 3.5 D1 fw 容忍度參數 — **未做**

最乾淨的入口是 `{"type":"trig_report","on":false}` —— 板子照打相機、照跑分料,
只是不告訴 core,而且刻意照樣 consume queue,不用改程式。要逼的:
`unanswered_stop_after: 10`、`SYNC_TOMB_N = 4`、多餘 frame、`ISRTrigQ` 溢出(32 深)。
對帳資料齊全(`trig_report_on`、`trig_suppressed`、`tq`、`tqcap`,而且 `CAM_PULSE_N`
刻意放在 `ISRTrigQ` 區塊外面)。

### 3.6 D2 真實圖像與 def 檔 — **等使用者提供**

使用者明說會給並要我主動要。**已提三次。**

與 0.1 有關:`ImageSampler` 那兩個 `ud2` 空殼目前沒有呼叫者,**真實 def 檔正好是
「會不會踩到」的答案**。若 0.1 已補 `return`,踩到也只是回錯值而不是當機 ——
所以先修再拿真檔跑,比較安全。

---

## 階段 4 — 回歸

改完之後**今天綠過的都要重跑**,因為階段 0/1 動的是共用檔:

- `console_abuse.mjs --skip-risky`(9 case)
- `cmd_sweep.mjs`(三個 phase)
- `proto_fuzz.mjs`(這時應該能一次跑完)
- `latch_loop.mjs --cycles 40`
- `io_timing.mjs`(含 SEL,若 2.6 已修則加驗 tid 歸屬)
- `bpg_sweep --include-crashers` / `churn` / `fd_leak` / `slow_client` / `doorbell`
- `link_fault.mjs`(六階段 + 還 slot)

---

## 台架啟動配方(三個坑都在這裡)

```sh
# 1) 只能有一個 core。孤兒 core 佔住 4099 和 COM3,新的會靜靜退出。
tasklist | grep -i visSele            # 必須是空的

# 2) 不能有孤兒瀏覽器。它會替你做 PD CONNECT,讓你以為無頭路徑能用。
tasklist | grep -i chrome-headless    # 必須是空的

# 3) PATH 兩段都要:mingw64 給 OpenCV/libgomp,build 目錄給 MVCAMSDK_X64.DLL。
export PATH="/c/msys64/mingw64/bin:<repo>/InspectionCore/build/nohik-cv4:$PATH"

# 4) cwd 必須是 Core0_1(data/ 在這裡)
cd InspectionCore/Core0_1
INSP_PERIF_CONSOLE=4099 INSP_CAM_TS_SYNTH=1 \
INSP_CAM_TS_OFFSET_US=800 INSP_CAM_TS_MULT=1.0 \
  ../build/nohik-cv4/visSele.exe > core.log 2>&1 &

# 5) 冷機到 READY(會自己 spawn 一個 detached perif_hold 持有通道)
node UI/WebUI/tools/webctl/bareboard_up.mjs --freq 15000

# WebUI 測試另外需要:
WEBCTL_HEADLESS=1 WEBCTL_URL=http://localhost:8081 node UI/WebUI/tools/webctl/webctld.mjs &
```

### 接真相機時(2026-08-20 實測)

```sh
# build 要選對:nohik-cv4 是 FEATURE_HIKROBOT=OFF,看不到相機,而且畫面上
# 不會有任何異狀 —— 它只會列舉到 BMP carousel 然後給你一台假相機。
#   nohik-cv4       HikRobot=OFF  OpenCV 4.13
#   win-mingw-msys  HikRobot=ON   OpenCV 4.13   <- 真相機用這個
#
# PATH 要三段,不是兩段:
#   mingw64/bin                        OpenCV / libgomp
#   build/win-mingw-msys               自己的 build
#   build/nohik-cv4                    MVCAMSDK_X64.DLL 只有這裡有(MindVision
#                                      的 link-time 相依,即使沒有 MindVision 相機)
#   MVS/Runtime/Win64_x64              MvCameraControl.dll(HikRobot)
export PATH="/c/msys64/mingw64/bin:<repo>/InspectionCore/build/win-mingw-msys:<repo>/InspectionCore/build/nohik-cv4:/c/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64:$PATH"
```

**相機接上之後就不可以 `taskkill /F` core** —— 要換 build 就趁 core 還沒握到
相機的時候換。

**這條的界線在「有沒有在串流」,2026-08-20 兩次實測**:

| core 死法 | 相機當時狀態 | 結果 |
|---|---|---|
| `ud2` 崩潰(完全沒有清理) | `TriggerMode(2)`,一張都沒抓過 | **相機沒事**,重開 core 直接接上 |
| `taskkill /F` | `trigger_mode 0` 自由運轉 10fps | **相機卡死**:Windows PnP 還顯示 OK,但 `MV_CC_EnumDevices` 回 0 台,core 陷入無窮 discovery 迴圈(68 次還在跑) |

我從第一列推論「這台相機不怕突然死」然後套用到第二列 —— **推論跨過了唯一有差別的
那個變數**,結果就是把相機弄卡。

**復原**:實體拔插 USB。`pnputil /restart-device` 和 `Disable-PnpDevice` 都要系統
管理員權限,一般 shell 兩個都會失敗。

**所以正確做法是用 `run_core.sh` 前景啟動、用 Ctrl-C 關閉。** 背景啟動會讓
graceful 路徑(SIGINT → `g_shutdownRequested` → `terminationFlag`)變成不可達,
Windows 只剩 `/F` 可用,而 `/F` 在串流中就是這一格。

**沒有畫面不一定是壞掉。** `getCamera()` 開機就設 `TriggerMode(2)` = 硬體觸發,
一個邊緣一張圖。`plate.freq=0` 時沒有 gate 脈衝、沒有 CAM1 stage 脈衝,
所以**一張都不會有**,而這是正確行為。要在沒有轉盤的情況下看畫面:

```sh
node UI/WebUI/tools/webctl/trigmode.mjs 0     # 自由運轉 10fps
node UI/WebUI/tools/webctl/snap_probe.mjs     # 驗證:IM=y SG=y 才是真的通了
```

Inspection UI 自己會把相機設回硬體觸發,所以進檢測模式後可能又靜止 ——
那不是退步,那是它在要求實際運轉需要的模式。

**沒有 shutdown 指令**,只能 `taskkill /IM visSele.exe /F`。這台是 BMP_carousel
假相機,沒有實體 USB handle,所以 `/F` 不會卡死相機;**接上真相機時不可以這樣做**。

---

## 不要做的

- **相機相關**:`dv_bench`、`rc_hammer`、`soak`、`qwatch`、真實檢測。沒有相機。
- **轉盤實測**:任何需要 `plate_freq_meas > 0` 的東西。
- **燒 NVS**:`save_setup` / `clear_saved_setup`。現在的 NVS 是好的,弄壞要重設
  `io_on_level`(這台是 active-low)。
- **`InspectionCore/test_suite/`**:四層阻塞,最硬的是 10221 golden 不在 repo。
- **`git commit -a` / `git add -A`**:worktree build symlink
  (`InspectionCore/contrib/shape_based_matching`)會被掃進去毀掉 submodule。
  **一律逐一列出路徑。**
- **`kill -9` 接了真相機的 core**:相機會卡死,要 DeviceReset。
- **在機台上做重運算分析。**
- **未經要求就 commit / push。**
- **機器運轉時開板子的序列埠**:每次 PD CONNECT 都會 toggle DTR 重開機。

---

## 已完成(2026-08-19)

| 測試 | 結果 |
|---|---|
| B1 console 協定面 | 9 case,找到 5 個缺陷 + 1 次 core 當機 |
| B2 perif link 生命週期 | 六階段全過(AUDIT 1.4) |
| B3 既有 core 測試重跑 | 5/5 綠,console 移植沒有波及共用檔 |
| B4 `link_fault` 還 slot | 已修,並補上 `perif_hold.mjs` |
| A1 指令面掃描 | 指令面是 56 不是 70;找到 1.2 / 1.3 / 2.5 |
| A2 協定 fuzz | 部分;找到 1.1,與 F1 串成 22 字元確定性殺 core |
| A3 IO 時序 | **首次有自動化驗證**;寬度與偏移全部吻合到 pulse,改設定會跟著動 |
| C2 dev 警告 | S11 已修並 PASS;T6 揭出底下的真 bug(2.7) |

**新工具 10 個**、**修過的工具 5 個**,commit `7470ad33` → `a38acee2` → `b16567bf`
(**尚未 push**)。

先前完成的(CAMSYNC 漂移精度、漂移補償量化、`CAM_CLOCK_LOST` 不可達、
`match_window_us` 200µs 地板、裸板迴圈閉合 60/60)見
`BARE_BOARD_TEST_PLAN_2026-08-19.md` 與 `UINSP_CAVEATS.md`。
