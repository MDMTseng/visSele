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
python uinsp_test.py --port COM6 bench --count 20 --freq 1500 --interval-ms 300
```

原理：`trig_phamton_pulse` 直接呼叫 `newPulseEvent()`，**完全繞過閘門感測**，
所以會產生帶有真 tid 的真物件，一路跑到氣閥輸出。而閘門腳位是 `INPUT_PULLUP`
且 `_senseInv_=true`，**沒接線時讀到的是「無物件」**，不會有假觸發干擾。

檢查項目：

| # | 內容 |
|---|---|
| B.1–B.4 | 設定 plateFreq、進檢測模式、**確認 timer ISR 真的在跳**、狀態為 READY |
| B.5 | 每個假脈衝**剛好**產生一個 `bTrigInfo` |
| B.6 | `tid` 嚴格 +1 遞增 |
| B.7 | 全部回報完不應進錯誤態 |
| B.8 | 對應的 SEL 計數器增加正確筆數 |
| **B.9** | **回報一個不存在的 tid → 必須停機**（`INSP_RESULT_MATCHES_NO_OBJECT`）|
| B.10 | `clear_error` 可恢復 |
| **B.11/B.12** | **不回報的物件 → 乾淨停機**（`OBJECT_HAS_NO_INSP_RESULT`），且板子仍會回應 |
| B.13 | 回到 IDLE、plateFreq 還原 |

**B.3 容易被忽略但很關鍵**：`Run_ACTS` 只在 timer ISR 裡執行。`plateFreq` 是 0 的話
假脈衝會被接受、然後永遠不會被處理——看起來就像管線壞了。所以 bench 會連續讀兩次
`SYS_STEP_COUNT` 確認它真的在前進。

**B.9 是整個設計的安全網。** tid 配對失步時，靠的就是韌體對不認得的 tid 直接停機，
而不是安靜地分錯槽。這一項如果沒過，代表安全網不存在。

**B.11/B.12 取代了原本階段 0.7 需要人工遮閘門的做法**，而且更可靠——那條路徑
原本會從 ISR 呼叫 `pinMode`/`digitalWrite`（commit `535d92fb` 修的），**回歸的症狀
是當機或重開而不是乾淨的錯誤態**，所以 B.11 特別檢查「板子還會不會回應」。

`--interval-ms` 必須大於 `SYS_MIN_PULSE_TIME_SEP_us`（預設約 67ms）且對應距離
超過 3.5mm，否則 `newPulseEvent` 會退回脈衝。預設 250ms 對兩個門檻都安全；
B.5 失敗時會提示調大這個值。

每次跑完會產出 `uinsp_verify_report.md`（`-o` 可改路徑），把結果貼回來就能對照。

## 各子命令在測什麼

| 子命令 | 對應清單 | 需要什麼硬體 | 需要人 |
|---|---|---|---|
| `stage0` | 0.1–0.6 | **只要板子** | 斷電重開兩次 |
| `bench` | 0.7 / 2.x 的韌體側 | **只要板子** | 無 |
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
python test_uinsp_test.py        # 15 項，不需要硬體
```

用假的序列埠模擬韌體行為，驗證分框（跨讀取切斷、背對背訊息、字串內含大括號、
逸出引號、雜訊重同步）以及回覆配對（非同步的 `bTrigInfo` 不會被誤認為命令回覆）。

這組測試已經抓到一個真的 bug：頂層 JSON 陣列會讓 `msg.get("id")` 拋例外並
**殺掉 reader thread**，之後所有接收靜默停擺。上機才發現的話會非常難查。
