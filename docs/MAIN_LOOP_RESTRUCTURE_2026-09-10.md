# 主邏輯迴圈的整頓 — 設計說明

2026-09-10 起。目標是把「WebSocket 指令必須在 select 執行緒上跑完」這件事拆掉，
而且拆的每一步都要能被量出來、能被關掉。

## 現況

`mainLoop()`（wiringPanel.cpp）就是整個 core 的主迴圈：

```
select(200ms)
  └ ws_server::runLoop
      ├ accept()                      新連線
      └ 每個 ready 的 ws_conn::runLoop
          └ recv → WS 解框 → BPG 重組
              └ m_BPG_Protocol_Interface::toUpperLayer   ← ~3400 行
```

`toUpperLayer` 底下的 handler 全部是**同步**跑在這條執行緒上：讀寫 def 檔、掃目錄
（FB）、編 JPEG、開關序列埠（PD，缺埠時 `new Data_UART_Layer` 會 throw）、II 直接跑
一次完整檢驗。這些跑著的時候，accept 不會發生、其他 client 的封包不會被讀、已斷線
peer 的 CLOSING 也不會被處理。

這同時也是 09-10 兩次 crash 的背景：recv buffer 一直長大的那段期間，正是這條執行緒
在別的地方忙。

## 為什麼不是直接丟一個 queue 上去

peer 的生命週期歸 select 執行緒管。`ws_conn` 物件本身不會被 delete（pool 會重用
slot），所以指標永遠有效——但**身分會被重用**：一個排隊中的封包，處理到一半時它的
peer 可能已經 CLOSING、slot 已經配給新連線。所以搬動之前需要：

- 每個 slot 一個 epoch，`setSocket()/RESET()` 時 +1；入隊時記下 epoch，出隊時比對。
- CLOSING 必須把該 peer 還在隊列裡的封包丟掉，而且若 worker 正在處理該 peer 就要等它。

## 邊界（打算怎麼切）

**必須留在 I/O 執行緒**：OPENING / HAND_SHAKING_FINISHED / CLOSING / ERROR_EV，
`peers`、`default_peer`、subscribe/unsubscribe、`dropPeerState`。這些就是 peer 生命
週期本身，搬走只會把同一個 use-after-free 換個執行緒發生。

**可以搬到單一 command worker**：DATA_FRAME 之後的所有 handler。用**一條**執行緒而不是
thread pool——目前所有 handler 的相互排序是隱性契約（載入 def 後才 CHECK、設定後才
取像），一條執行緒保留現有語意，pool 不會。

**關閉順序**：`g_shutdownRequested` → 停止入隊 → worker 收到 sentinel 後排空並結束 →
既有的 teardown（`terminationFlag`、shutdown dump、`_exit(0)`）不變。

## 步驟

- **A（本次，已做）** 先量，不搬。`toUpperLayer` 變成薄包裝，計時後把結果分到
  `g_histCmd`（單一 handler）與 `g_histServe`（一整輪 select 之外的時間），並依兩碼
  tag 分類累計。超過 200 ms 的單一 handler 直接在 log 出一行。全部經由既有的 GS
  `perif_pairing.lat_hist` 發出：新增 `cmd`、`serve` 兩個直方圖，以及 `cmd_by_tl`。
  行為零改變。
### A 量到了什麼（2026-09-10，6 輪，bench core）

    tag     n     total_ms    avg_ms    max_ms   share
    II      6      1505.9     251.0     278.2   86.7%
    FB      6       212.1      35.4      46.7   12.2%
    LD      6        12.5       2.1       2.7    0.7%
    PD     12         4.6       0.4       1.0    0.3%
    GS      7         2.5       0.4       1.8    0.1%
    cmd:   n=38   avg=45.74ms  max=278.2ms
    serve: n=1466 avg=1.19ms   max=278.3ms

三件事：

1. **II 就是那個。** 一次 CHECK 平均擋住 select 執行緒 251 ms，最壞 278 ms，佔全部
   阻塞時間的 87%。`serve` 的 max 幾乎等於 II 的 max——那一輪整條 socket 層就是死的。
2. **FB 是第二名，而且便宜就能修。** 35 ms 全花在 `cJSON_DirFiles` 的目錄走訪上。
3. **PD 不是問題（負面結果，但重要）。** 缺埠時 construct-throw-cleanup 的整條路徑
   平均 0.4 ms。09-10 crash 前 log 裡滿滿的 COM3 重試，**不是**用停頓來解釋的；
   那條線索到此為止，別再往「PD 拖住迴圈」的方向找。

重現：`node UI/WebUI/tools/webctl/stall_probe.mjs --rounds 6 --def <hydef> --img <png>`

- **B** 依 A 量到的資料決定要搬哪些 tag。先搬 II，再看 FB。先搬**最貴且與 peer 生命週期無關**的那幾個
  （目前預期是 FB、SV、LD、II），而不是一次全搬。
- **C** epoch + 入隊 + 單一 worker + CLOSING 排空，並保留 `INSP_INLINE_CMD=1` 走回舊路徑。

A 已經量出東西了：II 佔 87%。B 從 II 開始。
