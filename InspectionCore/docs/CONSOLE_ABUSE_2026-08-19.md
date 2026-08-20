# 週邊 dev console 協定面壓測 — 2026-08-19

裸板(無相機、無轉盤)。對象是 `wiringPanel.cpp` 的 `PerifConsoleThread`,
2026-08-19 才移植到 Windows,先前只驗過 happy path。

工具:`UI/WebUI/tools/webctl/console_abuse.mjs`(九個 case,每個之後都用**新連線**
跑一次 `get_running_stat` 當關卡,並比對 SYSTIME 不倒退)。

**結果:一個 core 當掉、四個真缺陷、一個一直在騙人的工具。**
其中兩個嚴重度和 console 無關 —— console 只是最便宜的觸發路徑。

---

## F1 — `PerifChannel::recv_ERROR` 是一條 `ud2`,任何鏈路協定錯誤都會殺掉 core

**嚴重度:P1。這條不是 bench 專屬,是出貨機的當機路徑。**

```
0000000140151ff0 <_ZN12PerifChannel10recv_ERROREN18Data_JsonRaw_Layer10ERROR_TYPEE>:
   140151ff0:	0f 0b                	ud2
```

原始碼(`wiringPanel.cpp:1494-1502`):

```cpp
  int recv_RESET()
  {
    // printf("Get recv_RESET\n");
  }
  int recv_ERROR(ERROR_TYPE errorcode)
  {
    // printf("Get recv_ERROR:%d\n",errorcode);
  }
```

宣告成 `int` 卻沒有 `return`。從非 void 函式尾端掉出去是 UB,gcc -O2 直接把
**整個函式編成一條 `ud2`**。不是回傳垃圾值 —— 是進去就死。

### 觸發面有多大

core 端的 data layer 在**五個**條件下呼叫 `recv_ERROR`
(`common_lib/Data_Layer_Protocol.cpp`):

| 行 | 條件 | 現實中什麼時候發生 |
|---|---|---|
| 231 | `INIT_CHAR_ERROR` —— frame 外出現非 `{`、非空白的位元組 | **線路雜訊、板子在 frame 中間重開、半截 frame** |
| 251 | `RECV_BUFFER_FULL`(20480) | 裝置回覆沒收尾 |
| 351 | `JSON_FORMAT_ERROR` | 裝置吐出壞 JSON |
| 485 | `RAW_DATA_OVERSIZE` | raw 模式長度欄位過大 |
| 525 | `RAW_CRC_ERROR` | **CRC trailer 一個 bit 翻掉** |

第 231 行那條最要命:**線上一個雜訊位元組就足以讓 core 執行非法指令。**
`Data_Layer_Protocol.hpp` 的註解還寫著 "frames with a BAD trailer are dropped
and counted, never latched" —— 第 525 行不是 dropped,是 `ud2`。

`recv_RESET()` 是同一個形狀,而且它在**復原路徑**上:core 自己的 parser latch 之後,
data layer 在資料流裡找到 RESET_PACKET 就呼叫它(`:303`)。**故障和它的復原都是 ud2。**

### 實際發生過

2026-08-19 17:42,console 壓測途中 core 死亡,minidump
`InspectionCore/Core0_1/insp_crash_4616_20260819_174252.dmp`:

```
EXCEPTION code=0xC000001D (ILLEGAL_INSTRUCTION) addr=0x7ff764c41ff0
  RIP module offset = 0x151ff0
  PerifChannel::recv_ERROR(Data_JsonRaw_Layer::ERROR_TYPE)  wiringPanel.cpp:1500
  <- Data_JsonRaw_Layer::recv_data()   Data_Layer_Protocol.cpp:547
  <- Data_Layer_IF::recv_data()        Data_Layer_IF.hpp:93
  <- Data_UART_Layer::recv_data_thread()  Data_Layer_PHY.cpp:374
```

崩潰前最後一行 log 是 2100 byte 的 console 行造成的 tx stall(見 F2)。

**不是每次都當。** 同樣的 2100 byte 行重跑 6 次,0 次當機 —— 裝置 latch、core 送
RESET_PACKET、link RESYNC,7–9 秒內復原。當機需要裝置在 latch 期間剛好吐出一個
frame 外的雜散位元組,那是時序問題。**但缺陷本身沒有機率成分:呼叫到就是死。**

### 同一類還有兩個(目前無呼叫者)

掃過整個 binary,第一條指令就是 `ud2` 的函式共四個:

| 函式 | 位置 | 現在有呼叫者嗎 |
|---|---|---|
| `PerifChannel::recv_ERROR` | `wiringPanel.cpp:1500` | **有,已當機** |
| `PerifChannel::recv_RESET` | `wiringPanel.cpp:1496` | **有,在復原路徑上** |
| `angledOffsetTable::sampleAngleOffset(acv_XY)` | `ImageSampler.cpp:628` | 沒有(`float` 版才有) |
| `nodeInfoIdxCorrection(...)` | `ImageSampler.cpp:746` | 沒有 |

後兩個是 `//TODO` 空殼,都在檢測管線裡。今天沒人呼叫,但一次多載解析改變或一個新
呼叫點就是立即當機,而且編譯器的警告沒有到任何人眼前。

掃描指令(可重跑,適合放進 CI):

```sh
objdump -d visSele.exe | awk '
  /^[0-9a-f]+ </ { sym=$0; sub(/^[0-9a-f]+ </,"",sym); sub(/>:$/,"",sym); pend=1; next }
  pend==1 && /\t/ { if ($0 ~ /\tud2/) print sym; pend=0 }' | sort -u | c++filt
```

**最小修法**:四個函式各補一行 `return 0;` / `return 0.0f;`。
外加 `-Werror=return-type`,這一整類就不會再出現。

---

## F2 — console 行長上限 4096,裝置 frame buffer 只有 2048

`PerifConsoleThread` 收到換行才送出,途中 `if (line.size() < 4096) line += c;`。
裝置端是 `uint8_t dataBuff[2048]`(`Peripheral/.../Data_Layer_Protocol.hpp`,
註解寫 "Sized to comfortably hold the largest legitimate command"),滿了就
`RECV_BUFFER_FULL` → **`SERIAL_PROTOCOL_ERROR`(11),latched,機器停**。

實測門檻:

| 送出長度 | 裝置回答 | 結果 |
|---|---|---|
| 1500 | 是 | 正常 |
| 2040 | 是 | 正常 |
| 2100 | 否 | **latch,error_hist 多一個 11,7–9 秒後靠 link RESYNC 復原** |

console 的 JSON 守門只看第一個非空白字元是不是 `{`。那個守門的註解寫得很清楚:

> A console whose typo stops the production line is the console's bug, not the
> operator's, and the guard is one character.

**同一個 console 現在用自己的行長上限做到了它想擋的事。** 長度沒被檢查,而且
上限比對端能收的多一倍。

**修法**:上限降到裝置的 frame buffer 以下,並且**超長要回錯誤、不要截斷後送出**
—— 截斷後的物件開頭是 `{`,守門一定放行。

---

## F3 — `!TL` 注入從來沒有成功過,而且它每次都回報成功

**這條最花時間,因為它偽裝成「能用」。**

`toUpperLayer` 有一道守門(`wiringPanel.cpp:3167`):payload 的**宣告長度之內**
必須含一個 NUL,否則拒收:

```
[PD] payload of 139 bytes is not NUL-terminated -- refusing
```

WebUI 和本目錄所有 ws 客戶端都配置 `body.length + 1` 並把那個零算進長度,所以通過。
console 走 `GenStrBPGData`:

```cpp
BPG_dat.size    = strlen(jsonStr);   // 終止符在 dat_raw[size],長度「之外」
BPG_dat.dat_raw = (uint8_t *)jsonStr;
```

`memchr(dat_raw, 0, size)` 對任何純文字 payload 必然是 NULL → **無條件拒收**。
而 console 在送出**之前**就先回 `{"core":"PD injected"}`,所以看起來完全正常。

### 為什麼今天之前沒人發現

整個 2026-08-19,`:4090` 上還掛著一個上次 WebUI 測試留下的 **Playwright
headless Chromium**,PD CONNECT 是**它**做的。`bareboard_up.mjs` 一直「有效」,
是因為通道早就被別人開好了。

把瀏覽器殺掉再跑,立刻現形:

```
$ taskkill /IM chrome-headless-shell.exe /F
$ node bareboard_up.mjs --freq 15000
1. PD CONNECT COM3 (this reboots the board -- once)
2. waiting for the board to answer
   board never answered get_running_stat
```

**這是「孤兒 core 佔埠」的同一種陷阱,換了一層。** 差別在孤兒 core 會讓你量到錯的
數字,孤兒瀏覽器會讓你以為自己有一條根本不存在的無頭路徑。

### 已處理

`bareboard_up.mjs` 改成用 **BPG websocket(:4090)** 送 PD CONNECT,framing 與
`link_fault.mjs` 相同(尾端多一個零位元組,並算進長度)。無瀏覽器實測通過:

```
1. PD CONNECT COM3 over BPG :4090 (this reboots the board -- once)
   up: state=100
   READY at t+9s: valid=true offset_us=800 learned=8
```

console 的 `!TL` 本身**還沒修**。修法是注入時把長度改成含終止符:

```cpp
BPG_protocol_data d = m_BPG_Protocol_Interface::GenStrBPGData(tl, payload.c_str());
d.size = (uint32_t)payload.size() + 1;   // 守門要的終止符
```

`payload` 是同步呼叫內的區域 `std::string`,`c_str()` 保證有終止符,指標在
`toUpperLayer` 回來之前都有效。

**純 JSON 打裝置的路徑不受影響** —— 那條不經過 `toUpperLayer`。死的只有 `!TL`。

---

## F4 — 非 JSON 錯誤回覆寫死 100 bytes,字面值只有 91

```cpp
consock_write(cli, "{\"err\":\"not JSON -- raw text latches the device "
             "parser; use {\\\"type\\\":...}, !TL, or ?lat\"}\n", 100);
```

實測字面值含換行是 **91** bytes。多寫的 9 bytes 是 NUL 加上 `.rodata` 裡下一個
字面值的開頭,實際抓到的線上位元組:

```
... "}\n  00 7b 22 65 72 72 22 3a  ->  NUL + {"err":"
```

越界讀 8 bytes,並且送給客戶端一個 NUL 加半截字串。以行為單位讀取的客戶端會看到
一行多出來的垃圾。旁邊那行 `"no perif channel"` 的 27 是對的,所以是單點筆誤。

**修法**:`sizeof(lit) - 1`,或改用 `strlen`。

---

## F5 — 第二個 console 客戶端不會被拒絕,它的指令會延後執行

accept 迴圈是單執行緒:`accept()` 之後進內層讀取迴圈,直到**那個**客戶端離開才
回到 `accept()`。所以第二個客戶端:

- TCP 上**連線成功**(在 listen backlog 裡),操作者看到的是「已連線」
- 送出的東西完全沒人讀,也沒有任何 echo
- 第一個客戶端離開的那一刻,它排隊的位元組才被讀進來並**執行**

實測:B 在 A 佔線期間收到 **0 bytes**,A 離開後收到 4327 bytes,而且 B 的
`get_running_stat` 確實**在那時候才執行**。

程式碼註解說的是相反的事:

```cpp
consock_t old = g_perifConsoleClient.exchange(cli);
if (old != CONSOCK_BAD) consock_close(old);   // one client at a time
```

那個分支從這條路徑到不了 —— 舊客戶端離開時已經把 slot 清成 `CONSOCK_BAD` 了。

在一台會推動實體零件的機器上,一個「看起來已連線」的終端機、操作者打完指令沒反應、
幾分鐘後另一個人關掉他的視窗、指令這時候才生效 —— 這個組合不能留著。

**修法**:accept 之後如果已有客戶端,回一行 `{"err":"console busy"}` 並關掉新的;
或改成真正的多客戶端(select 兩個 fd)。前者一行,後者才符合註解。

---

## 通過的項目

| case | 結果 |
|---|---|
| `partial_then_close` —— 半行無換行後斷線 | 沒有東西送到裝置 |
| `no_newline_close` —— 整條指令但沒有換行 | 沒有送出,正確 |
| `slow_reader` —— 20 秒完全不讀、97 個 stat 請求 | **core 沒有卡住**;之後新客戶端 212ms 拿到 stat。非阻塞 echo 的取捨成立 |
| `unknown_tl` —— `!xx` | core 拒收(見 F3,所有 `!TL` 都拒收) |
| `tl_edges` —— `!pd` / `!pd ` / `!p {}` / `!pdx{}` / `!!! {}` | 4 個落到 not-JSON 守門,1 個進注入路徑,都沒有到裝置 |

`slow_reader` 那一條值得單獨記:註解說那個非阻塞寫入是因為
"That happened, and it wedged the core mid-experiment"。**現在證實它擋住了。**

---

---

## F6 — synth sender 是一條永生 detached thread,通道刪掉之後繼續讀已釋放記憶體

**嚴重度:台架 P1,出貨機不受影響**(整段 gated 在 `INSP_CAM_TS_SYNTH=1`)。
發現於 B2/B4 鏈路測試,`wiringPanel.cpp:1293-1318`。

```cpp
void startSynthSender()
{
  bool expected = false;
  if (!synthSenderUp.compare_exchange_strong(expected, true)) return;
  std::thread([this]{
    while (true)                                  // <-- 沒有離開條件
    {
      uint32_t t = synthPendTail.load(...);       // <-- this 的成員
      ...
      sendReportTo_perifCH(this, e.tid, ...);     // <-- 用已釋放的 this 發 verdict
    }
  }).detach();                                    // <-- 沒有 join
}
```

`this` 是 `PerifChannel`。`delete_PeripheralChannel()` 在**每次 DISCONNECT、每次
reopen、每次最後一個 BPG 客戶端關閉**時 `delete doomed` —— 那條 thread 不知道,
繼續以 500µs 間隔戳已釋放的物件。

而且會累積:`synthSenderUp` 是**成員**,所以每個新的 `PerifChannel` 都會再起一條。
重連十次就有十條 detached thread,其中九條在打已釋放的記憶體。

### 兩次實際崩潰

| 時間 | dump | RIP | 讀取位址 |
|---|---|---|---|
| 18:00 | `insp_crash_11284_...dmp` | `wiringPanel.cpp:1300`(`synthPendTail.load`) | `0x1fd4e8a8c64` |
| 18:20 | `insp_crash_2972_...dmp` | `wiringPanel.cpp:1305`(`synthPend[t%N]` / `sendReportTo_perifCH`) | `0x2102f242c20` |

兩次都是 `ACCESS_VIOLATION READ`,呼叫堆疊都是
`std::thread::_Invoker<...startSynthSender()::{lambda()#1}>`。

**這和 F1 是不同的 bug。** F1 是 `ud2`(ILLEGAL_INSTRUCTION),在週邊 RX thread 上;
F6 是 use-after-free(ACCESS_VIOLATION),在洩漏的 synth thread 上。
F1 的 2100-byte 重現實驗跑 6 次 0 次當機,現在知道原因了 —— 那六次量到的是 F1,
而當天真正比較常發生的是 F6,觸發條件是**重連**,不是超長行。

### 崩潰之前它還會做事

`sendReportTo_perifCH(this, ...)` 在物件被釋放後、記憶體被重用前仍可能成功送出。
那是**上一個測試的通道**吐出來的 verdict。**任何在重連之後做的配對或計數量測都要
重做**,除非能確定當時沒有洩漏的 sender 存在。

### 修法

`while(true)` 改成看一個 `std::atomic<bool> synthSenderStop`,由 `~PerifChannel`
設起,解構子等 thread 認完再回來(或改成 `std::jthread` / `shared_ptr` 守衛)。
`synthSenderUp` 是成員這件事本身沒錯 —— 錯的是 thread 活得比成員久。

### 對台架的實務影響

在修好之前:**盡量少做 DISCONNECT/CONNECT**。`bareboard_up.mjs` 現在會先檢查通道
是否已在,已在就不重連(也就不會多一條 sender);`link_fault.mjs` 一次跑就會製造
至少三條。跑完鏈路類測試之後**重啟 core**,不要接著量數據。

---

## B2 — perif link 生命週期(AUDIT 1.4)全鏈驗證通過

`link_fault.mjs`,對面是真板子換成假 TCP 板再換回來。六個階段的 `link` 計數:

| 階段 | connected | suspect | tx_fail | consec | dropped_no_channel |
|---|---|---|---|---|---|
| CONNECT 假板 | true | false | 0 | 0 | 0 |
| CI 開始出 verdict | true | false | 0 | 0 | 0 |
| **假板死掉** | true | **true** | **51** | 51 | 0 |
| 同 desc 重連 | **false** | true | 51 | 51 | **17** |
| frame 繼續流,無通道 | false | true | 51 | 51 | **61** |
| **還原真板** | **true** | **false** | 51 | **0** | 61 |

三件事被證實:

1. **suspect 會擋住 reuse。** 第四列 `connected` 掉成 false —— desc 相同但鏈路
   suspect,所以走完整 reopen 而不是「同一台機器、保留通道」。這正是那段註解說的
   設計意圖(desc 描述的是埠,不是埠的健康)。
2. **`tx_fail` 是累計、`tx_fail_consec` 是連續。** 還原後前者留在 51、後者歸零。
   兩個都需要:一個說「這條鏈路今天壞過幾次」,一個說「現在是不是正在壞」。
3. **`dropped_no_channel` 只在通道真的不存在時才動**(17 → 61),和 `tx_fail`
   分得很乾淨 —— 寫入失敗和沒有東西可寫是兩件事。

## B4 — `link_fault.mjs` 不還 slot(trap 13)已修

還原不是一件事而是兩件:PD CONNECT 回真板,**而且**要留一個東西 attach 著。
通道屬於開它的 BPG 客戶端,客戶端一關通道就被刪 —— 所以這支程式沒辦法「既還原又
結束」。現在它在關閉 websocket 前 spawn 一個 detached `perif_hold.mjs` 接手。
`--no-restore` 保留原本的行為給除錯用。

**新工具 `perif_hold.mjs`**:開通道並持有,`--status` 查詢目前通道在不在。
無頭台架的前提條件 —— 沒有它,任何 PD CONNECT 都活不過自己的行程。

---

## A1 — 指令面一致性掃描

工具:`UI/WebUI/tools/webctl/cmd_sweep.mjs`。三個 phase:每個指令裸送、錯型別、
以及「acked 但沒生效」的獵捕。每個 probe 之後都跑存活關卡並比對 SYSTIME。

**先修正一件事:指令面是 56 個 type,不是 70 個。**
裸板計畫裡那份 70 名清單混進了設定**群組名**(`plate`、`gate`、`cam`、
`skip_policy`、`io_on_level`、`stage_pulse_*`)和**列舉值**(`center`、`centre`、
`slow_only`、`stop_only`、`slow_and_stop`、`none`、`alt`、`mode`、`abort`、`prbs`)。
真正的清單來自韌體自己的 dispatch:

```sh
grep -oP 'strcmp\(type,\s*"\K[a-z0-9_]+' src/app/LegacyFirmware.cpp | sort -u
```

順帶補上三個原本沒測到的:`enter_insp_test_mode`、`get_sel1_cd`、`set_sel1_cd`。

### A1-1 — 空的 `[]` 或 `{}` 會讓板子變聾,任何指令都一樣

**嚴重度:P1,產品面。這是本輪最嚴重的發現。**

| 送出 | 結果 |
|---|---|
| `{"type":"ping","x":[]}` | **latch** |
| `{"type":"ping","x":{}}` | **latch** |
| `{"type":"ping","x":[[]]}` / `{"a":{}}` | **latch** |
| `{"type":"get_running_stat","x":[]}` | **latch** |
| `{"type":"set_setup","plate":{}}` | **latch** |
| `{}` —— 空 frame 本身 | **latch** |
| `{"type":"ping","x":[1]}` / `{"a":1}` / `""` | 正常 |

latch 之後板子**還在發 SYSTIME**,但不接受任何指令。從主機看它是活的。
`clear_error` 每次都能救回來(A7 那個修正有效),但在那之前機器停不下來。

原因在 `include/comm/json_seg_parser.hpp` / `src/comm/json_seg_parser.cpp`:

```cpp
case NUL:
  if(ch=='{')
  {
    pushStackHead(OBJ_END);
    pushStackHead(DAT);
    pushStackHead(OBJ_SEP);
    pushStackHead(OBJ_KEY);   // <-- 無條件要求下一個是 key
    return OBJECT_START;
  }
```

`{` 之後一律推 `OBJ_KEY`,而 `case OBJ_KEY:` 只接受 `"` 和空白,其他一律
`RESULT::ERROR`。所以 `{}` 的那個 `}` 直接是格式錯誤。
陣列同理:`[` 推 `ARR_END, DAT`,`]` 落到 `case DAT:` 最後的 `else`,被當成純量值
的開頭。

**這個 parser 表達不了空物件和空陣列**,而兩者都是合法 JSON。格式錯誤 →
`JSON_FORMAT_ERROR` → `recv_ERROR` → `SERIAL_PROTOCOL_ERROR` latch。

**現實中誰會踩到**:任何一次 WebUI 存檔,只要某個設定群組沒有被改動而以
`"plate":{}` 送出,就會停掉機器。已實測 `{"type":"set_setup","plate":{}}` 會 latch。

**修法**:`OBJ_KEY` 在物件為空時接受 `}`(pop 掉 OBJ_KEY / OBJ_SEP / DAT 再 pop
`OBJ_END`,回 `OBJECT_COMPLETE`);`DAT` 在外層是 `ARR_END` 時同樣接受 `]`。

### A1-2 — 未知的 `type` 完全不回應

49 個真指令**全部有回應**。20 個不是指令的名字(含 `get_versionn` —— 只差一個
字母的錯字、以及 `definitely_not_a_command`)**全部零回應**:不是 `ack:false`,
不是錯誤,是沉默。

**這才是 plate.freq 陷阱真正的機制。** 原本記的是「acked 但被忽略」,不對 ——
phase 3 對 get_setup 的八個群組逐一試扁平寫法,沒有任何一個回 `ack:true`,
八個全部沉默。所以修法不是「讓 ack 誠實」,而是**在 dispatch 鏈尾補一個 else,
未知 type 回 `ack:false`**。一個分支的事。

對呼叫端而言,「你打錯字」和「板子死了」現在長得一模一樣。

### A1-3 — `get_version` 用別的通道回覆,而且丟掉呼叫端的 id

它是 56 個裡唯一一個由 `rsp_JsonRaw_version()`(framing 層)回覆的:

```
送 {"type":"get_version","id":9003}
收 {"type":"rsp_JsonRaw_version","id":100446,"version":"0.0.1"}
```

`id` 是它自己的流水號,不是呼叫端的。任何用 id 對帳的客戶端會一直等,而答案
其實早就在 buffer 裡。也是唯一一個回覆裡沒有 `ack` 的指令。

### 通過的部分

- 49 個真指令**全部有回覆**,沒有一個逾時
- 沒有任何指令回覆缺少 `ack`(除了 A1-3 的 `get_version`)
- 錯型別的字串、`null`、負數、數字給布林 —— 全部安全處理,沒有一個造成問題
  (唯一的例外是空容器,而那不是型別問題,是 parser 問題)
- 沒有任何一個「acked 但沒生效」
- 每個 probe 之後板子都還在,SYSTIME 沒倒退過

### 掃描的限制(下次要修)

phase 1 裡 `enter_insp_mode` / `exit_insp_mode` 會改變狀態,所以後面的指令是在
IDLE(100)而不是 READY(101)下測的 —— 一批 `"set plate_freq to 0 first"` 的
拒絕是狀態造成的,不是指令面的問題。**狀態相依的拒絕和指令面缺陷被混在一起了。**
要分開,每個 probe 之前都得把狀態復原,那會讓掃描慢十倍。目前的折衷是:
記下來,並且在讀結果時把 `state=100` 的那幾列當成「未測」。

---

## A2 — 協定 fuzz 打真板

工具:`proto_fuzz.mjs`(device JSON 形狀 + console 行組裝器)、
`latch_loop.mjs`(量測 latch/recover 循環)、`quote_crash.mjs`(F1 的確定性重現)。

### A2-1 — 一行殺掉 core,三次三中,每次一秒

```
{"type":"ping"} "AAAA"
```

這條**通過 console 的 JSON 守門**(開頭是 `{`),乾淨地 frame 成一個 ping,
然後在 frame 之間留下一個引號。core 一秒內 SIGILL。

三次獨立重現(每次都是全新的 core + bareboard_up):
`insp_crash_11080_193154`、`insp_crash_10152_193243`、`insp_crash_12336_193309`,
RIP 全部落在 `PerifChannel::recv_ERROR`(`wiringPanel.cpp:1500`),
`0xC000001D ILLEGAL_INSTRUCTION`。

**這是兩個 bug 串起來的,單獨任何一個都活得下去。**

**bug 一 —— 裝置把未逸出的位元組塞進 JSON 除錯訊息**
(`LegacyFirmware.cpp:4285`,`MData_JR::recv_ERROR`):

```cpp
for(int i=0;i<buffIdx;i++)
  if(dataBuff[i]=='"') dataBuff[i]=''';          // dataBuff 有逸出
...
dbg_printf("recv_ERROR:%d %s dat:%s", errorcode, dataBuff,
           string((char*)recv_data,0,9).c_str());  // recv_data 沒有
```

`dbg_printf` 把輸出包成 `{"dbg":"..."}`。所以 `recv_data` 前九個位元組裡只要有一個
`"`,那個字串就提早結束,裝置送出一個**格式錯誤的 frame**。
`recv_data` 只有一種錯誤會帶:`INIT_CHAR_ERROR`(`Data_Layer_Protocol.cpp:355`)——
也就是「frame 外面出現了一個位元組」。

**bug 二 —— core 收到格式錯誤的 frame 就執行非法指令**(F1)。

### 為什麼顯而易見的觸發方式反而不行

空容器(`{"type":"ping","x":[]}`)一樣會 latch 裝置,但走的是
`JSON_FORMAT_ERROR`,那條呼叫 `recv_ERROR` 時 `recv_data == NULL` ——
沒有 `dat:` 欄位、沒有未逸出的位元組、送出去的 JSON 是合法的。

`latch_loop.mjs` 實測:**40/40 個 latch + recover 循環,core 全部存活。**
差別就在那一個引號。這也解釋了為什麼早上那個 2100-byte 的重現跑六次都不當 ——
那條走的也是 `RECV_BUFFER_FULL`,同樣不帶 raw 位元組。

### 現實中的觸發路徑,不需要有人惡意

不需要 dev console(出貨機上 `INSP_PERIF_CONSOLE` 沒設)。真正的路徑是
**裝置在 core 寫到一半時重開或掉一個 frame**:板子回來之後看到的是上一個訊息的
尾巴,例如 `...","cat":3}` —— frame 外的位元組,而且前九個位元組裡幾乎一定有引號。
`INIT_CHAR_ERROR` → 未逸出的 dbg → core `ud2`。

**每一次 DTR toggle、每一次電源瞬斷、每一次半截 frame,都是這條路徑的入口。**

### A2-2 — 深層巢狀超過 stack 就 latch

`json_seg_parser` 的 `kMaxStackDepth = 48`,每層物件推最多四個狀態。

| 深度 | 結果 |
|---|---|
| 8 層 | 正常 |
| 20 層 | 正常 |
| **60 層** | **latch**(`clear_error` 可救) |
| 60 層陣列 | **latch**(`clear_error` 可救) |
| 未終止的字串 | **latch**(`clear_error` 可救) |

深度上限本身是合理的防護 —— 問題還是「超過上限的處置是 latch 整台機器」,
和 A1-1 同一個根:**這個 parser 沒有「拒絕這一個 frame 然後繼續」的能力,
只有「停機」。**

### 本輪沒跑完的

`proto_fuzz.mjs --group json` 在第六個 probe(trailing garbage)當掉 core 之後
就中斷了,後面十二個形狀還沒測:重複 key、超長 key、超長字串、unicode escape、
raw control char、超大數字、leading comma、頂層陣列、空白洪水、一行兩個物件。
**要等 F1 修好才有辦法一次跑完** —— 現在每碰到一個會產生 INIT_CHAR_ERROR 的形狀
就得重啟 core,而那正是這組 probe 想大量產生的東西。

console 行組裝器那組(NUL 截斷、逐位元組送達)同理,還沒跑。

---

## A3 — IO 時序驗證:從「不可行」變成「已驗證」

工具:`io_timing.mjs`。**這塊原本完全沒有任何自動化測試**,而它直接決定零件被吹到
哪個料道 —— 設錯不會當機,只會安靜地把好件丟進不良槽。

### 可行性:成立,而且是迴圈閉合之後才成立的

`IO_TRACE_LOG` 的呼叫點都在檢測管線裡、都帶 `task->src->tid`,所以裸板原本進不去。
**phantom train 讓真的 task 走完管線**,timer ISR 就照真零件的方式排程腳位,
`io_trace_arm` / `io_trace_dump` 讀得回來。實測 `n=120`(緩衝區滿)。

事件格式 `[pulse, pin, val, tid]`。`pulse` 是**轉盤步進計數**,不是微秒:
timer 跑在 `2 * plate.freq`,所以 freq=15000 時一個 pulse = 1/30000 s = 33.333µs。
腳位對照 `HardwareConfig.hpp`:16=L1A、17=CAM1、18=L2A、19=CAM2、25/26/32=SEL1/2/3、
0=SWITCH。

### 寬度 —— 全部吻合到 pulse

| 腳位 | 設定 µs | 預期 pulses | 實測 | 樣本 |
|---|---|---|---|---|
| L1A | 350 | 11 | **11** | 12 |
| CAM1 | 3333 | 100 | **100** | 12 |
| L2A | 3333 | 100 | **100** | 12 |
| CAM2 | 3333 | 100 | **100** | 12 |
| SEL3 | 50000 | 1500 | **1500** | 9 |

SEL 那一列要把 dry_run 關掉才量得到(dry_run 會抑制 SEL 致動,`SEL_SUPPRESSED` 在
數)。關掉之前必須讓轉盤停下 —— `set_dry_run` 要求 `plate_freq_current == 0`,
而 accel=2000 從 15000 降到 0 要 **7.5 秒**。第一次只等 2.5 秒,指令被拒
(`ack:false`,`plate_freq_current: 9857`),**而且失敗得很安靜地正確** ——
沒有半套生效。量完已還原 dry_run=on。

### 偏移 —— 用同一個 tid 的兩站距離來驗

絕對偏移是「一圈裡的位置」,trace 能驗的是**同一個零件在兩站之間的距離**,
而那正是決定零件落點的量。

| 配對 | 設定差 | 實測 | 樣本 |
|---|---|---|---|
| `CAM2_on -> CAM1_on` | 198 | **198** | 12 |
| `CAM1_on -> SWITCH` | 20385 | **20385** | 9 |

### 真正的測試是「改了會不會跟著動」

靜態吻合證明不了什麼 —— 預設值本來就可能是同一份常數推出來的。
所以把 `stage_pulse_width_us.CAM1` 從 3333 改成 1667µs 再量一次:

```
expected 50 pulses, measured 51  ->  THE SETTING REACHES THE PIN
```

差的那一個 pulse 是排程器對非整數寬度的進位,和 L1A 的 350µs(10.5 → 11)是同一個
行為。量完已還原成 3333。

### 這次發現的儀器缺口

**SEL 的 trace 事件 tid 恆為 0。** 呼叫點寫死:

```cpp
IO_TRACE_LOG(PIN_O_SEL1,1,cur_pulse,0);
IO_TRACE_LOG(PIN_O_SEL3,0,cur_pulse,0);
```

所以 SEL 的邊緣**無法歸屬到任何一個零件**。可以量「SEL 脈衝多寬」,不能量
「**這一個**零件的 SEL 有沒有在對的時間開」—— 而後者才是會抓到誤分料的那個量測。
CAM/L1A/L2A 那些站都帶 `task->src->tid`,只有 SEL 這一組沒有。
補上 tid 是幾行的事,補上之後這條測試才真的完整。

### 還沒驗的

`stage_pulse_center` 目前全部是 0,沒有非零的情況可以驗。要驗得先設一個非零值,
而它的語意(相對於哪個基準置中)還沒讀碼確認 —— 留給下一輪。

---

## C2 — 兩個「把 dev 警告當 error」的 case

原本的判斷是「測試本身要改,不是產品有問題」。**對了一半。**

### S11(`r10_smoke`)—— 是測試的問題,已修,現在 PASS

過濾器用**逐條列舉**警告文字:`validateDOMNesting`、`Each child in a list`、
`Failed prop type`…… 然後在
`Warning: React does not recognize the ` + '`%s`' + ` prop on a DOM element` 上失敗 ——
一條還沒有人遇過的警告。**這種清單永遠落後一個版本。**

改成**依形狀過濾**:React 對每一條 dev 警告都加 `Warning:` 前綴,而 production
bundle 一條都不會發。一個斷言若是「這幾個轉換過程中沒有新的 error」,就不該把它們
算進去。改完 S11 通過。

### T6(`r6_inspection`)—— 過濾器一直在**遮住診斷**,底下是真的產品 bug

同樣的修改套到 T6 之後,它沒有變綠,而是換了一條訊息:

```
unhandledrejection: TypeError: Cannot read properties of undefined (reading '0')
    at InspectionEditorLogic.FindInspShapeObject   (InspectionEditorLogic.js:763:36)
    at InspectionEditorLogic.ShapeAdjustsWithInspectionResult
    at InspectionEditorLogic.ShapeListAdjustsWithInspectionResult
    at INSP_CanvasComponent.draw_INSP              (EverCheckCanvasComponent.js:1111)
    at INSP_CanvasComponent.draw
```

這條**本來就在失敗**,只是 `newErr[0]` 只印第一條,而第一條是那個 React 警告。
過濾器沒有遮住這個 TypeError —— 它遮住的是**診斷**。

而且 T6 的註解寫著:

> (Defensive R7-era TypeError filter removed: the "(reading '0')" path was
>  confirmed fully fixed by the InspectionUI down_samp_level_update null-guard.
>  Any future appearance is a genuine regression — let it fail loudly.)

**它現在正是在 fail loudly,而且沒有人聽見。** 那個 null-guard 補在別的地方,
真正的崩潰點不在它守的那條路上。

### 根因:JavaScript 預設參數的陷阱

`InspectionEditorLogic.js` 服務端第 763 行是

```js
return inspReport.auxPoints[inspIdx];
```

`inspReport.auxPoints` 是 `undefined`,而 `inspIdx` 是 `0` —— 所以是 `undefined[0]`。

為什麼 list 不存在卻拿得到索引 0:

```js
FindShape(key, val, shapeList = this.shapeList) {      // <-- 預設參數
  let idx = shapeList.findIndex((shape) => shape[key] == val);
  return (idx < 0) ? undefined : idx;
}
FindShapeIdx(id, shapeList = this.shapeList) { return this.FindShape("id", id, shapeList); }
```

`FindShapeIdx(id, inspReport.auxPoints)` 傳進 `undefined`,**預設參數就生效了**,
於是它去搜 `this.shapeList`,在那裡找到形狀、回傳索引 0,
呼叫端再拿這個索引去索引**另一個**(不存在的)陣列。

**傳 `undefined` 給選擇性參數,會安靜地換成另一份資料。**

`FindInspShapeObject` 裡六個區塊(`detectedCircles`、`detectedLines`、`auxPoints`、
`searchPoints`……)全都是這個形狀:報告裡缺哪個欄位,就從 `this.shapeList` 拿到一個
不屬於它的索引。

**最小修法**:`FindInspShapeObject` 每個區塊先判斷該 list 存在再查;
或讓 `FindShape` 在被明確傳入 `undefined` 時不要退回 `this.shapeList`。
後者比較徹底,但要確認沒有呼叫端在依賴那個退回行為。

**未修** —— 與 core 的四個 P1 同樣的處置:記錄、不動產品碼。

### 順帶發現:`webctld` 的瀏覽器死掉就不會回來

`context` 和 `page` 在模組載入時各建一次,之後沒有任何重建。瀏覽器一旦不在,
後續每個請求都回
`page.reload: Target page, context or browser has been closed`,而且**永遠**如此。

我為了找 F3 把 Playwright 的 Chromium 殺掉,就踩到了這個 —— 整個 QA runner 在
164ms 內全數 FAIL,看起來像是套件壞了。

**這是 C1 那三個間歇的一條實質線索**:如果瀏覽器或分頁在長跑途中進了壞狀態,
之後每一個套件都會失敗,而單獨重跑其中任何一個都會過。那正是 trap 12 的特徵
(「三者單獨跑都綠」)。下一輪 C1 應該先驗這條,再去看 `ActionThrottle_type`。

另外 `webctld` 預設連 `http://localhost:8080`,而這台的 app 在 **8081**,
所以重啟時一定要帶 `WEBCTL_URL=http://localhost:8081`,否則它會停在
`chrome-error://chromewebdata/` 而且不會抱怨。

---

## 台架狀態(本輪結束時)

- core PID 新起,`INSP_CAM_TS_MULT=1.0`,log 在
  `<scratchpad>/core_nobrowser.log`
- 板子 101 READY、`valid=true`、`offset_us=800`、`plate.freq=15000`
- **Playwright headless Chromium 已全部關掉**,`:4090` 上只有 `bareboard_up.mjs`
  自己那條(執行完就斷,通道留著)
- 崩潰 minidump 保留在 `InspectionCore/Core0_1/insp_crash_4616_20260819_174252.dmp`

## 新工具

| 檔案 | 用途 |
|---|---|
| `console_abuse.mjs` | 九個 case 的協定面壓測,每個之後有存活關卡 |
| `console_oversize_crash.mjs` | F1/F2 的最小重現(含完整因果鏈註解) |
| `big_reply_probe.mjs` | 逐一量測唯讀指令的最大單行回覆,對照 core 的 frame buffer |
| `fake_perif.mjs` | TCP 假裝置。PD CONNECT 支援 `ip`+`port`,所以可以把**任意位元組**餵進 core 的 data layer —— A2 fuzz 和 B2 鏈路測試要的就是這個 |