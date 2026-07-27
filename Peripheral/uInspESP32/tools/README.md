# uInspESP32 測試工具

`uinsp_test.py` —— 直接對韌體下 JSON 命令，協助跑 `docs/HW_VERIFICATION_CHECKLIST.md`。

**不需要 core、不需要 WebUI。** 這是刻意的：階段 0–3 出問題時，唯一可能出錯的
就只有韌體本身，不用去猜是不是上層的問題。

```sh
pip install pyserial
```

## 用法

```sh
python uinsp_test.py ports                          # 找序列埠
python uinsp_test.py --port COM6 stage0             # 階段 0：韌體單獨 + NVS
python uinsp_test.py --port COM6 bench              # ★ 光板：完整 tid 往返
python uinsp_test.py --port COM6 probe              # ★ 光板：協定 + 相機觸發
python uinsp_test.py --port COM6 edge               # ★ 光板：深層路徑
python uinsp_test.py --port COM6 iotrace            # ★ 光板：真實 IO 時序
python uinsp_test.py --port COM6 errorpath          # 階段 0.7：錯誤路徑
python uinsp_test.py --port COM6 monitor --seconds 60   # 階段 2：tid 連續性
python uinsp_test.py --port COM6 selectors          # 階段 3.1/3.2：出口對應
python uinsp_test.py --port COM6 all                # 依序全跑

python uinsp_test.py --port COM6 send '{"type":"get_setup"}'   # 單發命令
python uinsp_test.py --port COM6 -v monitor         # -v 印出每一個收發框
```

## ★ 光板診斷（`bench`）—— 不用接任何機構

**只要 ESP32 板 + USB 線就能跑完整條 tid 路徑。**

```sh
python uinsp_test.py --port COM6 bench --count 10 --cat 1
python uinsp_test.py --port COM6 bench --count 20 --freq 1500 --interval-ms 150
```

原理：`trig_phamton_pulse` 直接呼叫 `newPulseEvent()`，**完全繞過閘門感測**，
所以會產生帶有真 tid 的真物件，一路跑到氣閥輸出。而閘門腳位是 `INPUT_PULLUP`
且 `_senseInv_=true`，**沒接線時讀到的是「無物件」**，不會有假觸發干擾。

**B.0 會先用 `set_setup` 把 `SWITCH`／`SEL*` 的 stage-pulse-offset 往下游推
（見 `_widen_selector_window`）。** 真機幾何裡，相機觸發（`bTrigInfo` 送出的點）
到選別氣閥只差 `SWITCH-L1A_on`≈43 步——韌體內的 C++ core 來得及回，但透過序列埠
下命令的 host 追不上。把窗口撐開後，這條測試量的是「管線有沒有把 tid 正確導向」，
而不是「host 能不能在 17ms 內回覆」。跑完 B.13 會還原成原本的 offset。

檢查項目：

| # | 內容 |
|---|---|
| **B.0** | **用韌體參數把選別窗口撐開**（跑完還原）|
| B.1–B.4 | 設定 plateFreq、進檢測模式、**確認 timer ISR 到速在跳**、狀態為 READY |
| B.5 | 每個假脈衝產生一個帶真 tid 的物件，並**各 announce 兩次**（CAM1 `tidx=1`＋CAM2 `tidx=2`）|
| B.6 | 物件 `tid` 嚴格 +1 遞增 |
| B.7 | 全部回報完不應進錯誤態 |
| B.8 | 對應的 SEL 計數器增加正確筆數 |
| **B.9** | **回報一個不存在的 tid → 必須停機**（`INSP_RESULT_MATCHES_NO_OBJECT`）|
| B.10 | `clear_error` 可恢復 |
| **B.11/B.12** | **不回報的物件 → 乾淨停機**（`OBJECT_HAS_NO_INSP_RESULT`），且板子仍會回應 |
| B.13 | 回到 IDLE、選別窗口與 plateFreq 還原 |

**B.3 容易被忽略但很關鍵**：`Run_ACTS` 只在 timer ISR 裡執行。`plateFreq` 是 0 的話
假脈衝會被接受、然後永遠不會被處理——看起來就像管線壞了。而且轉盤是**斜坡加速**的，
在還沒到速時發脈衝會落在 3.5mm 去重門檻內被丟掉，所以 bench 會 `_wait_at_speed`
一直讀 `SYS_STEP_COUNT` 直到步進速率不再爬升（斜坡到頂）才開始發。

**B.9 是整個設計的安全網。** tid 配對失步時，靠的就是韌體對不認得的 tid 直接停機，
而不是安靜地分錯槽。這一項如果沒過，代表安全網不存在。

**B.11/B.12 取代了原本階段 0.7 需要人工遮閘門的做法**，而且更可靠——那條路徑
原本會從 ISR 呼叫 `pinMode`/`digitalWrite`（commit `535d92fb` 修的），**回歸的症狀
是當機或重開而不是乾淨的錯誤態**，所以 B.11 特別檢查「板子還會不會回應」。

**每個物件會 announce 兩次**（CAM1 `tidx=1`、CAM2 `tidx=2`，同一 tid）。bench 按
tid 去重，**每顆只回報一次**——對已經過站的 tid 重複回報，本身就會讓機台失步
（`INSP_RESULT_MATCHES_NO_OBJECT`）。回報是**一收到 `bTrigInfo` 就發**、不是等固定
sleep，這樣即使不撐窗口也能讓判定跑在物件前面。

`--interval-ms` 只管相鄰假脈衝的間距：必須大於 `SYS_MIN_PULSE_TIME_SEP_us`
（預設約 67ms）且在 `--freq` 下對應距離超過 3.5mm，否則 `newPulseEvent` 會退回脈衝。
預設 120ms 在 `--freq 1000` 下對兩個門檻都安全；物件數 < 發射數時會提示調大它。
選別窗口的競速由 B.0 撐窗口處理，不再靠 `--interval-ms`。

每次跑完會產出 `uinsp_verify_report.md`（`-o` 可改路徑），把結果貼回來就能對照。

## ★ 協定 + 相機觸發（`probe`）—— 其餘命令處理器

`bench`/`edge`/`stress` 沒碰到、但**安全又能觀察**的命令處理器。一樣只要板子：

| # | 內容 | 為什麼重要 |
|---|---|---|
| P.1 | `ask_JsonRaw_version` 回 `rsp_JsonRaw_version` 帶版本字串（**注意**：回覆 id 寫死 100446，是非同步訊息不是配對回覆）| core 就是靠這個握手確認對面是 uInsp 韌體、不是這塊板上一版的 CNC image |
| P.2 | `reset_running_stat` 把 SEL/NA 計數全部歸零 | 之前沒測過；也是讓測試能用絕對值而非增量斷言的唯一手段 |
| P.3 | `trigCamPulse` 只 announce **一次**、帶呼叫端給的 `trigger_id`，且**不建立管線物件**（Qs 不變）| 這是階段 1 依賴的相機觸發 announce 路徑，接相機前就能先驗；對照假脈衝是 announce 兩次（CAM1+CAM2）且會進 RBuf |

**刻意排除**（會驅動輸出、這裡沒有可讀回的狀態，清單也把它們留給現場人工）：
`PIN_ON`/`PIN_OFF`/`PIN_MODE`（裸 GPIO）、`sel_act`（打氣閥，屬階段 3「錯了不會自己
顯現」那條）、`stepper_enable`/`disable`（會動盤子）、`save_setup`（燒 flash；NVS
存活是階段 0.5，本來就得真的斷電）。

## ★ 深層路徑（`edge`）—— bench 沒走到的那幾條

`bench`/`stress`/`stall` 走的都是「回報 cat=1、一切正常」的主線。`edge` 補上
主線之外、但整個設計依賴的那幾條路，一樣**只要板子 + USB**：

| # | 內容 | 為什麼重要 |
|---|---|---|
| E.1 | `cat=0xFFFF`（NA）是**判定**不是錯誤：NA_Count 加、氣閥不動、不停機 | 階段 2 分選關閉時整條線就是靠它回流 |
| E.2 | 回報較新的 tid 時，**更舊的未回報物件被標 SKIP** 靜默通過，不停機 | 這是 FIFO 失步的吸收器；壞掉的話每次小事故都變成停機 |
| E.3 | 落在去重門檻內（時間 / 3.5mm）的脈衝被退回時**不消耗 tid** | 若退回也吃號，閘門一次彈跳就讓之後所有配對全錯位 |
| E.4 | `set_sel1_cd N`：SEL1 再動作 N 次後**靜默停止**（限量停機的錯誤路徑已被註解掉）| 批量跑到上限的症狀跟氣閥壞掉一模一樣，先在桌上見過一次 |
| E.5 | 雜訊 → 停機（錯誤碼 11）＋之後所有位元組被靜默吃掉，**一次 RESET** 就恢復並自動 redeem（`recv_RESET` 直接走 `handleResetCommand`，實機驗證 112→101）| 雜訊必須上鎖而不是聳肩續跑；「每次連線送兩次 RESET」（清單 5.5）是保險、不是必要 |
| E.6 | 把選別窗口撐到 14000 步、塞入 115 顆 → **RBuf（100 深）滿了之後靜默退回多的脈衝**，不當機、不停機，排空後乾淨 | 佇列滿的行為從沒被驗過；壞掉的話產線堵料時就是當機而不是丟料 |

E.1–E.4 在 plateFreq=600 下跑（門檻時序才拉得開）；E.5 要在檢測模式中做——
IDLE 狀態表**沒有** INSPECTION_ERROR 轉移，雜訊在 IDLE 只上鎖不記錯誤。
跑完 E.7 會把 offset、plateFreq、minDetectTimeSep_us、sel1_cd 全部還原。

## ★ 真實 IO 時序（`iotrace`）—— 韌體當自己的邏輯分析儀

`bench` 靠計數器驗「有沒有分對槽」，但計數器看不到**實體接腳的邊沿時序**。
韌體現在會把 `Run_ACTS` 裡每一個 L1A/CAM/SWITCH/SEL 的 GPIO 邊沿連同當下的
pulse count 記進一個環狀緩衝（`io_trace_arm`/`io_trace_dump`，**預設關閉、上鎖旁
路只是一個 volatile 判斷，量產零成本**）。發一顆假脈衝再 dump，板子就變成自己的
邏輯分析儀——不用示波器、也**不用撐窗口**：

**關鍵是壓低 plateFreq**（預設 200）。真機幾何裡相機到選別只差 43 步，但步頻一低，
這 43 步在牆鐘上就變成幾十毫秒，host 的判定照樣趕得上真實窗口，於是 SWITCH 會帶著
真 verdict 派工、SEL 也照時序打出來。實測 dump（`plateFreq=200`）：

```
L1A_on @654  CAM1_on @654  L2A_on @654  CAM2_on @654   ← 四路燈/相機同一 pulse
CAM1_off @656  L2A_off @656  CAM2_off @656
L1A_off @666
SWITCH @697  val=1          ← host 在真窗口內回覆，SWITCH 帶 cat=1 派工
SEL1_on @700  SEL1_off @701
```

| # | 內容 |
|---|---|
| I.1 | `io_trace_arm` 成功 |
| I.2 | trace 有錄到且 dump 完整（`n==emitted==rows`；否則代表 3KB dump buffer 被截，發少一點）|
| I.3 | 邊沿依 pulse 單調不倒序 |
| I.4 | **燈/相機每個邊沿都落在設定的 offset**（這組不依賴 host 判定，永遠說實話）|
| I.5 | CAM1 與 CAM2 在同一 pulse 觸發（雙相機同步）|
| I.6 | **SWITCH 帶著實際 verdict 在真窗口內派工**（沒撐窗口；若沒趕上會是 UNSET 大數）|
| I.7 | 對應 SEL 在設定 offset on+off |
| I.8 | 回 IDLE、plateFreq 還原 |

RAM 代價：trace 環（120 筆）＋ dump buffer 約 5KB 靜態，佔用從 12.7% 升到 14.2%，
仍是九牛一毛（見上面「空間 & 算力」的分析）。

## 各子命令在測什麼

| 子命令 | 對應清單 | 需要什麼硬體 | 需要人 |
|---|---|---|---|
| `stage0` | 0.1–0.6 | **只要板子** | 斷電重開兩次 |
| `bench` | 0.7 / 2.x 的韌體側 | **只要板子** | 無 |
| `probe` | 版本握手/計數歸零/相機觸發 | **只要板子** | 無 |
| `edge` | NA/SKIP/去重/限量/協定鎖/佇列滿 | **只要板子** | 無 |
| `iotrace` | 真實 IO 邊沿時序（韌體自錄）| **只要板子** | 無 |
| `errorpath` | 0.7 | 板子 + 閘門 | 手動遮閘門、目視氣閥 |
| `monitor` | 2.3–2.4 | 完整機構 | 放料 |
| `selectors` | 3.1–3.3 | 板子 + 氣閥 + 料槽 | **逐一打氣閥、記錄實體桶** |

**前兩個只要板子。** 建議先在桌上把 `stage0` + `bench` 跑到全綠，再上機——
那樣機台上出問題時，韌體已經被排除了。

## 幾個設計上的取捨

**線路格式**：純 JSON 文字、靠大括號配平分界、無二進位框架、無分隔符
（對照 `src/comm/Data_Layer_Protocol.cpp` 確認過）。

**開頭會先送一次 `{"type":"RESET"}`**。韌體只要收到非 `{`/`[` 開頭的位元組就會
latch 協定錯誤，之後除了 RESET 什麼都不理。上一輪跑到一半中斷很容易留下這個狀態。

**工具端遇到雜訊會重新同步而不是報錯**，跟韌體行為刻意不同——中途 attach 時
一定會落在訊息中間，不該因此死掉。

**`monitor` 檢查的是整條 tid 路徑賴以成立的假設**：`tid` 嚴格 +1 遞增、一顆料件
一個。斷號就代表 core 端的 FIFO 配對前提不成立，會直接印紅字。

`Qs` 是韌體 RBuf 深度（上限 `PIPE_INFO_LEN`=100）。持續逼近就代表 host 回報
跟不上觸發。

**`selectors` 不做任何推論。** `INTEGRATION_MAP.md` §8 有個「SEL1 可能是良品出口」
的推測，這個子命令要求你**實際打氣閥、看料件掉進哪個桶**再填 `cat_ok`/`cat_ng`。
理由是階段 3 是唯一會真的噴出料件的階段，而噴出的料件不回流——**猜錯不會自己
顯現**，只會得到一整桶錯的東西而系統一切正常。

## 自我測試

```sh
python test_uinsp_test.py        # 32 項，不需要硬體
```

用假的序列埠模擬韌體行為，驗證分框（跨讀取切斷、背對背訊息、字串內含大括號、
逸出引號、雜訊重同步）以及回覆配對（非同步的 `bTrigInfo` 不會被誤認為命令回覆）。

這組測試已經抓到一個真的 bug：頂層 JSON 陣列會讓 `msg.get("id")` 拋例外並
**殺掉 reader thread**，之後所有接收靜默停擺。上機才發現的話會非常難查。
