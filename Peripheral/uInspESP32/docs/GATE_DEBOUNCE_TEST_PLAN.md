# 閘門去彈跳（gate debounce）測試計畫

## 背景

`onTimer` 每個 timer tick 都會呼叫 `GateSensing()` 取樣閘門腳位（`PIN_I_GATE`，
`INPUT_PULLUP`＋`_senseInv_`，閒置＝無物件＝LOW）。這條路徑**先前沒有任何
去彈跳**：任一 tick 只要讀到與上一狀態不同就當成真邊沿，且 `minWidth=0`，所以
連 1-tick 的雜訊尖波都會被當成一顆物件（帶真 tid）送進管線。

已改：把計數式去彈跳（原本是死碼 `GateSensing2`）併回 `GateSensing()`——
邊沿必須連續 `DEBOUNCE_*_THRES` 個取樣都維持新狀態才被接受：

| 參數 | 邊沿 | 作用 | 預設 |
|---|---|---|---|
| `gate_debounce_rise`（`DEBOUNCE_H_THRES`）| 上升（物件到）| 拒絕短暫 HIGH 尖波（否則變成 phantom 物件）| 2 |
| `gate_debounce_fall`（`DEBOUNCE_L_THRES`）| 下降（物件離）| 容忍短暫 LOW 凹陷（否則一顆被切成兩顆）| 2 |

- 兩者以 **gate 取樣 tick** 為單位；1 tick ≈ 1 步 ≈ `_PLAT_DIST_um_PER_STEP`
  ＝在 350mm 盤上約 **38µm** 行程。時間上 1 tick ＝ `1/(2·plate_freq)` 秒。
- 皆可用 `set_setup` 即時調（`gate_debounce_rise`/`gate_debounce_fall`），
  `get_setup` 可讀回；**不寫入 NVS**，重開回預設。設為 0 會被夾成 1。
- 預設 2/2 ＝拒絕任一邊沿的單取樣雜訊，這是「一定正確」的最小值；真實料件寬達
  數百步，2 只讓前緣延後 1 步（固定偏移，下游無感）。原本 `1+20µm/step` 在此
  解析度下算出來是 1（＝沒有去彈跳）。
- 物件參考點仍取**尾緣**（`end_pulse`），與去彈跳前一致，所以校正過的
  `stage_pulse_offset` 不用重算。（改成取物件中心是清單 5.3 另一件會動到校正的事。）

## 為什麼不能用 phantom 脈衝測

`trig_phantom_pulse` 直接呼叫 `newPulseEvent()`，**完全繞過 `GateSensing()`**。
所以 `bench`/`edge`/`stress`/`iotrace` 一律測不到去彈跳——去彈跳只在真實閘門
訊號（或注入到閘門取樣輸入）時才會跑到。這也是清單 5.4 一直掛著「需要現場」
的原因。

---

## 方法 A（決定性，需要硬體）——真實感測器 + 注入彈跳

**需要**：接上真實閘門感測器、盤子會轉、`monitor` 看 tid 連續性。

### 注入彈跳的三種方式
1. **訊號產生器**（最可控）：把閘門線接到函數產生器/第二顆 MCU，產生一個乾淨
   的物件脈衝，並在邊沿疊上可控寬度的尖波叢（例如前緣加 1～5 個 tick 的
   HIGH/LOW 抖動）。掃寬度找出去彈跳的實際門檻。
2. **真實料件**：跑會讓感測器抖動的料——邊緣反光、有接縫/孔洞、半透明的，
   最容易觸發 split（凹陷）與 phantom（反光尖波）。
3. **機械式**：做一個帶刻意凹槽/毛邊的靶件，重複進站。

### 量什麼
- `monitor --seconds N` 看 **tid 是否嚴格 +1**、有沒有斷號。
- `get_running_stat` 的 `NA_Count` / SEL 計數 vs **實際進站料件數**。
- 是否出現 `INSPECTION_MODE_ERROR` / `INSP_RESULT_MATCHES_NO_OBJECT`。
- 每顆的閘門寬度分佈（可搭配 io_trace 看 L1A/CAM 的實際落點）。

### 程序
1. 先 `gate_debounce_rise=1, gate_debounce_fall=1`（＝關閉去彈跳），跑一批帶彈跳
   的料，記下 split 數與 phantom 數（基準線，應能重現問題）。
2. 逐步調高 rise / fall（1→2→3→…），每個設定跑同一批，直到 split／phantom 歸零。
3. 記下**最小可用值**。過大會：前緣延遲變大（rise）、把兩顆靠很近的料黏成一顆
   （fall 太大且大於料件間隙）。

### 通過標準
- 連續 N 顆（建議 N≥200）：**物件數 == 實際料件數**。
- tid 無斷號、無 `INSP_RESULT_MATCHES_NO_OBJECT`。
- 寬度分佈集中（無異常短脈衝＝雜訊殘留、無異常長脈衝＝黏連）。
- 把選定的 rise/fall 記進部署設定；因為不進 NVS，開機腳本要重下。

---

## 方法 B（建議做，可在光板上自動測）——韌體注入閘門取樣

概念與 `trig_phantom_pulse`/`io_trace` 對稱：那兩個讓「產生物件」「觀察輸出」
可在桌上測；這個讓「閘門輸入」也能在桌上測。

### 需要的韌體 hook（尚未實作）
在 `GateSensing()` 取樣處加一個**可覆寫的輸入源**：

```c
// armed 時，GateSensing 從注入 FIFO 取樣，而非讀實體腳位。POD、單生產者
// (命令) 單消費者 (ISR)，比照 ISRTrigQ 的紀律。
volatile bool GATE_INJECT_ARMED=false;
RingBuf_Static<uint8_t, 512, uint16_t> GATE_INJECT_Q;   // 每個元素 = 一個 tick 的位準
...
uint8_t new_Sense = GATE_INJECT_ARMED
    ? gate_inject_next()          // 取一個注入樣本（空了就維持最後值）
    : GPIOLS32_GET(PIN_I_GATE);
```

命令：
- `gate_inject_arm`：清空 FIFO、`GATE_INJECT_ARMED=true`。
- `gate_inject_load`：push 一段位準樣本（可用 run-length 壓縮，如 `[[1,120],[0,2],[1,3],...]`＝1 持續 120 tick、0 持續 2 tick…）。
- `gate_inject_stop`：`GATE_INJECT_ARMED=false`，回實體腳位。

搭配已有的 `io_trace` dump，就能：注入一段帶彈跳的位準序列 → 看產生了幾顆物件、
tid 落點、以及各 SEL/L1A/CAM 邊沿——**全部在光板上、可自動斷言**。

### 這能自動測到的案例
| 案例 | 注入 | 預期（在合適的 rise/fall 下）|
|---|---|---|
| 乾淨脈衝（基準）| HIGH 120 tick | 剛好 1 顆物件 |
| 空檔單-tick 尖波 | LOW…, HIGH 1, LOW… | **0 顆**（rise≥2 拒絕）|
| 邊界：尖波 = rise-1 / rise | … | 前者拒、後者接受 |
| 物件內短凹陷（< fall）| HIGH 60, LOW 1, HIGH 60 | **1 顆**（不切開）|
| 物件內長凹陷（≥ fall）| HIGH 60, LOW 5, HIGH 60 | **2 顆**（切開）|
| 兩顆正常料 | HIGH 120, LOW 100, HIGH 120 | 2 顆、tid 連續 |

實作後會新增一個 `gatetest` 子命令跑上表，並附「關閉去彈跳時基準線會失敗」的
反向 fake 自我測試（比照 `edge` 的 `FakeFirmwareNoSkip` 等）。

---

## 與既有防護的關係（belt and braces）

去彈跳不是唯一防線，理解疊加關係才好調：
- **`newPulseEvent` 的 3.5mm（~91 步）去重門檻**：把「一顆被切成兩顆」的第二段
  （距前一顆 <3.5mm）擋掉——所以 split 多半不會變成雙數 tid。但**空檔中的孤立
  雜訊**（距鄰居 >3.5mm）它擋不住，會變 phantom。去彈跳補的正是這一塊。
- **`pulse_min_width`**：寬度下限。設成真實料件寬度的一個下界，可再擋掉「比料件
  窄很多」的雜訊脈衝，即使沒有時間去彈跳。是最便宜的過渡防護（不用重燒）。
- **建議**：rise 去彈跳擋孤立尖波（phantom），fall 去彈跳擋內部凹陷（split），
  minWidth 擋殘餘窄脈衝，3.5mm 門擋近距重複。四者互補。

## 現況

- 去彈跳邏輯已併回 live path、可即時調、預設 2/2（拒絕單取樣雜訊）。
- 方法 A 需要現場硬體，尚未跑。
- 方法 B 的注入 hook 尚未實作（建議下一步做，才能把這項也拉進光板自動測）。
