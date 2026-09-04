# SBM 定位更新計畫 — 速度 / 準度 / 強健度 / score 鑑別度

2026-08-31

這份計畫的每一條都有實測支撐,量測平台是 **Raspberry Pi 5 @ 2.4 GHz、4 執行緒、
NEON、governor=performance、throttled=0x0**,合成場景 2560×1920(4.92 MP)、單物件
360 templates、`match_scale=0.5`、σ=10,除非另外註明。

> **先讀這段再看下面任何數字。** 全部量測用的是**合成實心六邊形**,不是產線零件;
> 平台是 **ARM/NEON**,產線是 **Windows x86/AVX2**。所以:
>
> * **比例**(A 比 B 快幾倍、誤差差幾倍)可以拿來排優先序,這是它的用途。
> * **絕對值**(15 ms、0.06 px)**不能**拿去做節拍預算。
> * 凡是跟「邊緣強度分布」有關的參數(`num_features`、`blur`、`weak/strong_thres`),
>   效果**完全取決於零件形狀**,必須用實件重跑才能採用。
>
> 每一項都標了「可直接做 / 需驗證 / 需設計」。**只有標「可直接做」的兩項我認為
> 不必先量產線實件**(原本是三項,`skip_voting` 在驗收時被降級——見第 1 項)。

---

## 2026-09-04 更新 — 核對過一次,三項的狀態變了

這份計畫寫於 8/31,基準是 rc2 `a27ac458` 之前的 master。之後 rc2 走到 `69da9a7a`,
其中兩件事直接影響下面的內容,另外一件是這份計畫促成的。逐項核對過現在的 rc2,
沒有變的就沒有動。

| 項目 | 8/31 的狀態 | 現在 |
|---|---|---|
| 1 `skip_voting` | 未設定 | **未變**(rc2 仍 0 筆),已降級 P1 |
| 2 `refine_residual` | 被丟掉 | **已實作**,見下方 |
| 3 `shape_match_scale` | 不確定產線值 | **已回答:真實 def 設 0.5** |
| 4 score 混用 | — | 未變 |
| 5 角度掃描 0–360 | 未接 def margin | **未變**(`:8497` 仍寫死),但見下方 |
| 6 參數調校 | — | 未變 |
| 7 ICP 誤用 | `SHAPE_REFINE` 可切 | **未變**(`:8450` 仍在) |
| 8 ROI 離群過濾 | 全關 | **未變**(rc2 仍 0 筆) |
| 9 `ICP_Sparse` 文件不符 | 同路徑 | **未變**(`4e34397:2318` 仍同路徑) |
| 10 形變 | — | 未變 |

### 3 已回答:產線 def **有**設 `shape_match_scale = 0.5`

一份真實的 def(2448×2048、5.01 MP、`shape_based`)確認:

```
shape_match_scale        = 0.5
shape_strong_thres       = 80
matching_angle_margin_deg = 180
其餘皆未設定 -> 用程式預設
```

**所以第 3 項的「若未設 = 3.7× 損失」不成立,那個風險不存在。** 這是這份計畫裡
唯一一個「查核後發現不用做」的項目。

順帶,它也讓第 5 項的預期收益歸零:`matching_angle_margin_deg = 180` 等於不限制,
所以就算把 margin 接到 sweep 上,對這份 def 也不會少掉任何一個 template。要有收益,
得先有人把 margin 收窄——而那是配方決策,不是程式改動。

### 2 已實作,並且在實作過程中發現一個我自己的 bug

`refine_residual` 已接進報告(`ct/sbm_residual_rc2`)。**第一版是錯的**:值寫在
`SingleMatching_shape` 內、緊鄰 `similarity`,而該函式在 locating-anchor 的第二遍
量測時又呼叫兩次 `RESET_REPORT`,把值洗成 -1。函式庫算出 0.227,報告寫 -1。

它躲過了我當時的驗證,因為我只驗了**負向**——「不該出現時確實不出現」——而當時
手上的 fixture 從不跑 ROI refine,所以永遠是 -1,正向情況根本沒有被測到。
是一份真實的 def 送進來才露餡。

修法:賦值移到呼叫點,在所有 reset 之後、複製進 `reports` 之前。

**第一批真實數值:**

| 輸入 | `refine_residual` |
|---|---:|
| `samples/headless/sample1`(合成、乾淨) | 0.0139 px |
| 真實畫面的 def | **0.2270 px** |

這給了門檻第一個錨點。合成資料推出來的 0.3–0.5 px 起點,對照真實件的 0.227,
**可能太鬆**。要定門檻需要一批正常件的分布,不是一顆。

### 新增並已解決:`shape_cache` 不帶 `refine_points`

這是跑上面那些量測時發現的,已回報,rc2 已修(`c0ecd070` 等)。

從 def 的 JSON `shape_cache` 載入的 def,`FeatureSet::refine_points` 是空的,
`selectOptimizedPoints()` 因此回傳 0 點,**整段 ROI refine 被跳過**——匹配成功、
分數很高、報告完全正常,少掉的只有精度。

rc2 的修法不是補上序列化,而是**把第二條載入路徑整個移除**:「THERE IS ONE WAY TO
LOAD A TRAINED LOCALISER」。舊格式的 def 現在會落到抽取路徑,而抽取在 studio 外
會被拒絕。同時 `samples/headless` 的 gate 也加嚴到會擋下靜默失去 ROI refine 的 def。

回報過程中我自己犯的一個錯,記在這裡以免重演:issue 裡的重現指令寫了
`--preset linux`,那個 preset 是我在自己分支上加的,rc2 沒有。對方為此查了一輪
(`ba21336c`)。**回報時的重現步驟必須在對方的樹上可執行,不能夾帶自己未推送的改動。**

### 另一件值得單獨處理的事

三個 Linux 編譯阻斷點,`ct/uinsp_2mach_linux_build`(`3edc7320`, 2026-08-10)
三週前就修過同樣四項,驗證平台也相同,但那條分支從未合入。同樣的東西被兩個人
獨立發現兩次,代表**沒有任何步驟會在 Linux 上編譯這棵樹**。`samples/headless`
現在存在了,是掛這個 gate 的明顯位置。

---

## 0. 摘要:按「價值 / 風險」排序

| # | 項目 | 類別 | 實測效果 | 風險 | 分期 |
|---|---|---|---|---|---|
| 1 | `skip_voting` 從未被設定 | 速度 | **1.24×**,但**會改變結果** | 中(逐 def 驗證) | ~~P0~~ → **P1** |
| 2 | `refine_residual` 被丟掉 | 鑑別度 | 現成的壞配對偵測 | 極低(唯讀) | P0 |
| 3 | 確認產線 `shape_match_scale` | 速度 | 若未設 = 3.7× 損失 | 無(先查再說) | P0 |
| 4 | score 混用了兩件事 | 鑑別度 | 雜物下 score 全盲 | 中 | P1 |
| 5 | 角度掃描永遠 0–360 | 速度 | ~1.2×(上限有限) | 低 | P1 |
| 6 | `num_features` / `blur` 調校 | 速度 | 合計 1.61× | 中(挑形狀) | P1 |
| 7 | ICP 系列在雜訊下崩潰 | 強健度 | 需擋住誤用 | 低 | P1 |
| 8 | ROI 離群過濾等於沒有 | 強健度 | 遮蔽 20% 就垮 | 高(需設計) | P2 |
| 9 | `ICP_Sparse` 與文件不符 | 正確性 | 文件錯誤 | 極低 | P2 |
| 10 | 形變(shear)無對策 | 強健度 | 角度全模式失守 | 高(需研究) | P3 |

---

## P0 — 可直接做,不需先驗實件

> 原本這裡有三項。第 1 項(`skip_voting`)在驗收時發現會改變結果,已降級到 P1,
> 內容留在原處並標了修正。**現在 P0 只有第 2、3 項。**

### 1. `skip_voting`:核心從未設定它 —— 但它不是免費的

> **2026-08-31 修正。** 這一項原本被我列為 P0「可直接做」,驗收條件寫的是
> 「輸出逐位元相同」。**驗收沒過,所以這一項降級為 P1,預設維持關閉。**

**現況**  `buildShapeMatcher()` 設了 `min_score`、`nms_angle`、`refine`、`T_levels`、
`weak/strong_threshold`、`blur_kernel_size`、`match_scale`——**沒有 `skip_voting`**。
`grep -rn skip_voting MatchingEngine/ Core0_1/` 是零筆,它停在函式庫預設 `false`。

**看起來像免費的午餐**  `shape_matcher.h` 的說明是:

> Skip 3x3 neighborhood voting (saves ~7ms at 20MP).
> **Safe to enable with higher edge thresholds (50/80)**

而核心的 `shape_weak_thres` / `shape_strong_thres` 預設**正好就是 50 / 80**。
速度也是真的:17.22 → **13.85 ms(1.24×)**。

**但它會改變結果。**  跨 8 個角度 × 3 個雜訊等級共 24 組,逐位元比對開關兩種設定:

| 結果 | 數量 |
|---|---|
| 逐位元相同 | **9 / 24** |
| 有差異 | **15 / 24** |

多數差異可以忽略(位置 < 0.1 px、角度 < 1°、分數差一個量化階 ≈ 0.37)。
**但 `deg=271, σ=0` 這一格:位置差 1.93 px、角度差 60.02°、殘差差 1.21**
——兩個設定鎖到了**不同的候選姿態**,而兩者分數只差 0.74。

機制很直接:跳過投票會擾動量化後的方向圖,分數因此位移約一個量化階,
而這在兩個姿態接近平手的地方足以翻轉勝負。

**結論**  在一台 def 以 USL/LSL 驗證過的機器上,這是**靜默的量測改變**,
不能靠改預設值發給每一份既有 def。

**已實作的形式**  `shape_skip_voting` def key,**預設 0(維持現狀)**。
設為 1 才啟用,而且如果該 def 把門檻調到 50/80 以下,會**拒絕並記一行 LOGE**
——那是函式庫自己宣告的安全條件,不該假設它成立。

**採用方式**  逐 def:在該零件上量過速度與量測值都可接受之後,才在該 def 裡開。

### 2. `refine_residual`:函式庫算好了,核心沒讀

**現況**  `MatchResult` 有這個欄位:

```cpp
float refine_residual = -1.0f; ///< ROI refine fit quality: mean |point-to-line|
                               ///< residual (px) ... Low (~<1px) = trustworthy;
                               ///< high = points disagree (occlusion / wrong / off
                               ///< match). -1 = not computed (refine != ROI).
```

核心固定用 `RefineMode::ROI`,所以**它每一幀都被算出來**。
`grep -rn refine_residual MatchingEngine/ Core0_1/ UI/WebUI/src` 是**零筆**。

**它有效**  殘差與位置誤差同向,而且跨兩種破壞型態都成立:

| 狀況 | `refine_residual` | 位置誤差 |
|---|---:|---:|
| 正常 | 0.063 | 0.058 px |
| 雜物 10% | 0.133 | 0.196 px |
| 雜物 20% | 0.415 | 0.650 px(p95 2.33) |
| 遮蔽 20% | 1.459 | 2.458 px |
| 遮蔽 40% | 4.731 | 8.177 px |

**這是目前唯一能抓到「雜物型壞配對」的訊號**(見第 4 項:score 抓不到)。

**已實作**(2026-08-31)
1. 欄位放在 `FeatureReport_sig360_circle_line_single`,緊鄰 `similarity`——**不是**
   原本計畫寫的 `LocateOutcome`。`LocateOutcome` 只在「沒有物件」時才發出
   (`if (L.reason[0] != 0)`),而殘差是**每一顆找到的零件**都有的事實,兩者
   生命週期不同。
2. `SingleMatching_shape()` 多收一個參數,從 `sbm::MatchResult` 直接帶過來,
   不重算——sbm 在它已經跑過的 ROI refine 裡算好了。
3. `RESET_REPORT()` 每幀先設回 `-1`。這一步是必要的:`reportDataPool` 跨幀重用,
   而 `resize()` 會把新項目 value-initialize 成 **0**,那正好是「完美擬合」的值。
4. JSON 只在 `>= 0` 時輸出,所以 sig360 report 與非 ROI 模式的 shape report
   **與這個欄位存在之前逐位元相同**,消費端也能用「鍵不存在」分辨「沒量」
   而不必知道 -1 這個哨兵值。
5. **只觀測,沒有任何東西依它分支。**

**待辦**  累積一輪產線資料後再定門檻。合成資料的起點是 **0.3–0.5 px**,但這個值
跟 mmpp 與零件尺寸直接相關,**必須用實件重新校**。

---

### 3. 確認產線 def 的 `shape_match_scale`

**現況**  `FeatureManager_sig360_circle_line.h:359` 的預設是 **`1.0f`,也就是關閉**。
`pure_sbm_def_design.md` 寫的 `0.3` 是**設計文件裡的計畫值,不是程式預設**。
這個 checkout 沒有 `data/`,所以我無法確認產線 def 實際設了什麼。

**若未設定,這是目前最大的單一損失:**

| `match_scale` | 5MP 粗比對(360T,不含 refine) |
|---|---:|
| 1.0(程式預設) | **63.37 ms** |
| 0.7 | 28.54 ms |
| 0.5 | **15.07 ms** |
| 0.25 | 4.79 ms |

**而且降取樣不會犧牲定位精度。** 這點單獨驗過:粗比對本身的誤差在全解析度是
2.0 px、在 0.5 降取樣是 2.83 px,但 **ROI refine 之後兩者都是 0.06 px**。
refine 在全解析度跑,其成本與 `match_scale` 無關(4 顆零件實測:ms=1.0 每顆
0.48 ms、ms=0.5 每顆 0.54 ms,實質相同)。

**動作**  先去產線機器 `data/` 底下 grep 一次 `shape_match_scale`。這是查核,不是改動。

---

## P1 — 需要用實件驗證後再上

### 4. score 混用了兩件事,雜物下全盲

**現況**  `MatchResult::score` 是**粗比對階段**的分數,在 refine 之前就定案。
它同時還是 `min_score` 的比較對象,而 `min_score` 又**同時是粗金字塔的剪枝門檻**
(`bench_multiobj.cpp` 自己的註解點出這件事)。所以一個數字被當成三種東西用:
匹配信心、剪枝門檻、結果過濾。

**後果(實測)**

| 破壞 | score | 位置誤差 p95 | score 有反應? |
|---|---:|---:|---|
| 遮蔽 40% | 99.6 → 72.9 | 10.6 px | 有 |
| 雜物 40% | 99.6 → 96.9 | 2.9 px | **幾乎沒有** |
| ICP + σ=40 | 99.6 → **99.6** | 18.0 px | **完全沒有** |

最後一列最嚴重:refine 把姿態拉歪 18 px,score 一動也不動,因為它根本是在
refine 之前算的。

**改動方向**  把「找到了嗎」與「找得對不對」分成兩個數字往上報:
* `score` — 維持現狀(粗比對相似度),不改語意,避免動到既有配方的門檻。
* `refine_residual` — 第 2 項接出來的那個,作為姿態可信度。

**不要做的事**  不要為了「讓 score 更準」而去改 score 的計算方式。它同時是剪枝
門檻,改它會讓每一份既有 def 的召回率悄悄改變,而那是不可逆的現場風險。

---

### 5. 角度掃描永遠是 0–360

**現況**  `buildShapeMatcher()`:

```cpp
modc.angle.start = 0; modc.angle.end = 360; modc.angle.step = shape_angle_step_deg;
// Diagnostic override: SHAPE_ANG_RANGE="start,end" ... to
// test a constrained angle search before wiring it to the def margin.
```

def 有 `matching_angle_margin_deg`,但它只在 `:5888` / `:5922` 當**比對後的拒絕
濾網**用,不會限制建模的角度範圍。註解自己寫了「before wiring it to the def margin」
——這件事已經被辨識出來,只是沒做。

**預期效益有限,要先講清楚**  per-stage profile 顯示,1280×960 / 360 templates 下:

| 階段 | ms | 與 template 數相關? |
|---|---:|---|
| GaussianBlur + Sobel + spread/LUT/linearize | 8.84(**75%**) | **否** |
| Coarse similarity | 2.99(25%) | 是 |

而且吞吐量矩陣直接證實:templ=1 是 10.20 ms、templ=360 是 12.90 ms——**多 359 個
template 只多 2.7 ms**。所以把角度砍到 ±30°(12× 少的 template)大約只省 20%,
不是 12×。

**仍然值得做**,因為成本極低(把既有的 margin 接到既有的 env hook 上),
而且它同時降低誤配率——角度範圍外的候選根本不會產生。

**風險**  `matching_angle_margin` 的預設是 `M_PI`(±180°,等於不限制)。接上去之後,
**任何一個 margin 設錯的 def 會從「多算一些」變成「找不到」**。必須先掃過所有現有
def 的 margin 值,並在 margin 生效時寫一行 log。

---

### 6. 特徵與前處理參數

單獨與組合的實測(基準 = 核心現況):

| 設定 | ms | score | vs 基準 |
|---|---:|---:|---:|
| 基準(T={4,8} blur=7 thres=50/80 nf=128) | 17.22 | 99.6 | 1.00× |
| `num_features=32` | 14.27 | **100.0** | 1.21× |
| `blur=5` | 15.14 | 100.0 | 1.14× |
| `blur=3` | 14.58 | 100.0 | 1.18× |
| `thres=70/100` | 16.33 | 99.6 | 1.05× |
| **組合:skip_voting + nf32 + blur5 + thres70/100** | **10.69** | 99.3 | **1.61×** |

`num_features` 從 128 降到 32 分數反而變好(留下的是最強的邊),但**這正是最挑
形狀的一項**:實心六邊形特徵多且強,細線零件砍到 32 很可能不夠。

**動作**  用產線實件跑一次同樣的 sweep 再決定。`sbm_setup_studio_plan.md` 提到的
studio 是做這件事的地方。

---

### 7. 擋住 ICP 的誤用

**實測**(位置誤差 px,15 次):

| refine | σ=0 | σ=20 | σ=40 | σ=40 p95 |
|---|---:|---:|---:|---:|
| **ROI** | 0.052 | 0.064 | **0.081** | 0.166 |
| ICP / ICP_Sparse | 0.149 | 1.792 | **9.050** | 18.003 |
| ICP_Subpixel | **0.012** | 1.515 | **17.939** | 32.196 |

**σ=20 是斷崖**,之後 ICP 比 ROI 差約 110 倍。角度同樣:σ=40 時 ROI 是
0.207°/最大 0.369°,ICP 是 6.08°/最大 16.7°。

核心目前寫死 `RefineMode::ROI`,**這是對的**。但有一個 `SHAPE_REFINE` 環境變數
可以切成 `icp`,而切過去之後 score 完全不會反映姿態已經壞掉(見第 4 項)。

**改動**  `SHAPE_REFINE=icp` 生效時寫一行明確的 warning log,說明它在 σ>20 的
畫面上不可信。這個 hook 是給 bench 用的,不該讓人在產線上安靜地踩到。

**附帶**  `ICP_Subpixel` 在乾淨畫面下位置誤差 0.012 px,比 ROI 的 0.052 好 4 倍,
成本相當。如果哪天量測精度吃緊而畫面夠乾淨,這是現成選項——但目前 `SHAPE_REFINE`
沒有這個值,而且我只驗了位置沒驗角度。

---

## P2 — 需要設計

### 8. ROI 的離群過濾實質上不存在

**現況**  `MatchConfig` 有 `roi_distinct_pct`、`roi_reject_low_score`、
`roi_weight_by_distinct`、`roi_min_spacing` 等一整組過濾開關,**全部預設關閉**,
核心一個都沒設。

**打開也沒用(實測)**  `+reject` 與 `+distinct 0.3` 的數字跟預設**逐格相同**,
只有在遮蔽 50% 那格 `roi_reject_low_score` 才有作用(6.171 → 4.003 px)。
`roi_distinct_pct=0.3` 在這個形狀上完全沒有剔除任何點——它依「點的辨識度相對
最大值」剔除,而這個形狀的點辨識度都差不多。**它的效果完全取決於零件形狀,
不能當通用保護。**

**容忍度**

| 破壞型態 | 安全 | 開始壞 | 不能用 |
|---|---|---|---|
| 遮蔽(平背景) | ≤10%(0.058 px) | **20%(2.458 px)** | 30%(5.8 px) |
| 雜物(疊亂線) | ≤10%(0.196 px) | 20%(p95 已 2.33 px) | 30%(p95 4.92 px) |

遮蔽 10% → 20% 之間是**斷崖,沒有緩衝**(0.058 → 2.458,42 倍)。

**方向**  真正的解是在 ROI 解算層加穩健估計(RANSAC / IRLS / Huber),而不是
依賴訓練期的點篩選。這是函式庫層的改動,屬於 sbm repo 而不是核心。
**在那之前,第 2 項的 `refine_residual` 閘門是唯一的防線**,而它是現成的。

---

### 9. `ICP_Sparse` 與文件不符

`shape_matcher.cpp:2321`:

```cpp
bool do_icp = (cfg.refine == RefineMode::ICP || cfg.refine == RefineMode::ICP_Sparse
               || cfg.refine == RefineMode::ICP_Subpixel) && ...
```

三個模式進同一個 `refineInverse()`,唯一差別是 `ICP_Subpixel` 在
`buildTemplateScene()` 開 `use_subpixel`。**`ICP_Sparse` 沒有任何專屬行為。**

實測佐證:兩者在 10 個不同條件下的位置與角度誤差**逐位元相同**。

但 header 把它們寫成不同演算法、不同成本與精度:

> `ICP_Sparse` — ICP using sparse matching features (~0.1ms/obj, ~2deg)
> `ICP` — ICP using dense Canny edges, integer EDT (~0.3ms/obj, <0.05deg, ~0.66px)

**動作**  要嘛實作 sparse 路徑,要嘛把 enum 值標成 deprecated alias 並修正註解。
現在的狀態會讓人以為自己選了便宜版本。

**順帶**  同一份 header 說 ROI refine 是 ~0.05 ms/物件,我在 Pi 上量到
**~0.5 ms/物件**(4 顆零件兩次獨立量測:0.48、0.54)。那個數字八成是 x86 量的;
ARM 上慢 10 倍合理,但如果有人拿 header 的數字做節拍預算會低估。

---

## P3 — 需要研究

### 10. 形變(shear)

位置誤差(σ=10 固定):

| refine | shear=0 | 0.05 | 0.10 | 0.15 |
|---|---:|---:|---:|---:|
| **ICP** | 0.156 | **0.140** | **0.136** | **0.237** |
| ROI | 0.060 | 0.627 | 1.022 | 1.155 |

**結論與雜訊那組完全相反:形變下 ICP 的位置幾乎不退化,ROI 退到 1.155 px。**
合理——ICP 用密集邊緣做最小平方,系統性形變會被平均掉;ROI 依賴少數點的
**局部外觀**,而 shear 把那個扭掉了。

**但角度沒有人守得住**:shear=0.15 時所有模式(**包含 `None`**)的角度最大偏移
都是 58–68°。那是粗比對階段偶爾鎖到錯誤角度,refine 只是繼承——所以要修的是
粗比對(以形變後樣本擴充訓練、或降 `min_score`),換 refine 沒有用。

**先決條件**  在投入之前,要先知道產線零件**到底會不會形變、形變多大**。
如果實際 shear 遠小於 0.05,這整項不值得做。而且我用的是**線性且全域**的 shear,
真實的變形(局部凹陷、毛邊、厚度不均)不見得有這個性質。

---

## 建議執行順序

1. **查核**(不改任何東西):產線 def 的 `shape_match_scale`、`matching_angle_margin_deg`、
   `shape_num_features` 實際值。
2. **P0-2 `refine_residual` 接出來,只觀測** — 累積產線資料以校門檻。
   (原本排在這裡的 `skip_voting` 已降級:它會改變結果,見第 1 項的修正。)
4. 依 (1) 的結果決定 **P0-3** 要不要調 `match_scale`。
5. 用實件重跑 **P1-6** 的參數 sweep。
6. **P1-5 角度範圍**,務必先掃過所有 def 的 margin 值。
7. P1-4 / P1-7 的 log 與欄位整理。
8. P2 / P3 視 (3) 累積到的殘差分布再決定。

---

## 附:本輪量測程式

都在 scratchpad,**沒有 commit**;要保留的話值得移進 `tests/`:

| 檔案 | 量什麼 |
|---|---|
| `bench_sbm_scale.cpp` | 粗比對 vs `match_scale`,不含 refine |
| `bench_knobs.cpp` | 各參數單獨與組合的速度/分數代價 |
| `bench_refine.cpp` | refine 各模式成本與位置精度 |
| `bench_robust.cpp` | 雜訊與 shear 下的穩定度 |
| `bench_roi_outlier.cpp` | 遮蔽/雜物容忍度與 `refine_residual` |
| `bench_orient_hist.cpp`, `bench_scale.cpp`, `bench_e2e.cpp` | 梯度方向直方圖定向(評估後**不建議**取代 SBM,見下) |

**方向直方圖那條路線的結論**:貼身乾淨裁切下 0.5 ms、誤差 0.56°,但(a)它只算
角度不找位置,(b)零件的**直方圖**若近似週期性就有歧義,而這從外觀看不出來,
(c)一旦 SBM 也吃同樣的降取樣,速度優勢消失(全幅 0.25x:直方圖 4.76 ms vs
SBM 4.79 ms,而 SBM 還多做了定位)。**不建議取代 SBM**;若要用,只適合當已知
位置後 0.4 ms 的獨立角度覆核。
