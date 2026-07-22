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
python uinsp_test.py --port COM6 errorpath          # 階段 0.7：錯誤路徑
python uinsp_test.py --port COM6 monitor --seconds 60   # 階段 2：tid 連續性
python uinsp_test.py --port COM6 selectors          # 階段 3.1/3.2：出口對應
python uinsp_test.py --port COM6 all                # 依序全跑

python uinsp_test.py --port COM6 send '{"type":"get_setup"}'   # 單發命令
python uinsp_test.py --port COM6 -v monitor         # -v 印出每一個收發框
```

每次跑完會產出 `uinsp_verify_report.md`（`-o` 可改路徑），把結果貼回來就能對照。

## 各子命令在測什麼

| 子命令 | 對應清單 | 自動判定 | 需要人 |
|---|---|---|---|
| `stage0` | 0.1–0.6 | PING、`get_setup` 欄位、persist 寫入、**斷電後 NVS 保留** | 斷電重開兩次 |
| `errorpath` | 0.7 | 錯誤後板子是否還會回應（**沒回應 = 當掉或重開 = 回歸**）| 手動遮閘門、目視氣閥 |
| `monitor` | 2.3–2.4 | `tid` 是否嚴格連號、`Qs` 佇列深度上限 | 放料 |
| `selectors` | 3.1–3.3 | — | **逐一打氣閥、記錄實體桶** |

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
