# shape 定位的倍率可攜性 — 驗證方法、結果、以及一個安全性發現

2026-08-22。

問題的起點:*「每個機種放大倍率不一樣、對比也不同,參數寫死在設定檔裡很難處理。」*

結論先講:**倍率這一半其實已經是自動的,而且今天驗證了它是對的**;真正沒人管的是
對比。另外驗證過程中撞到一個比參數自動化更該優先處理的問題 —— 見第 5 節。

---

## 1. 倍率是自動的,對比不是

`ensureShapeScale()`(`FeatureManager_sig360_circle_line.cpp:7457`)每幀比對 def 的
教導 `mmpp` 和現場 `sampler->mmpP_ideal()`:

```
want = def_mmpp / current_mmpp      // 模型縮放倍率,快取在 shape_built_scale
```

只在數值真的變了才重建,超出 `[0.2, 5]` 會大聲拒絕而不是硬縮。**換相機、換鏡頭、
換工作距離,def 不用改參數。**

`shape_match_scale`(粗比對的場景縮小)本來是寫死的,現在也跟著 mmpp 走:

```
match_scale_eff = shape_match_scale / want
                = shape_match_scale * (current_mmpp / def_mmpp)
```

比值是 **current / def,不是 def / current**。守恆的量是**粗比對階段看到的模板像素
數**,而模型放大和場景縮小是相乘的,所以補償是除法。方向寫反的話,零件在畫面上
只剩一半大的那台(最需要解析度的那台)會被壓到 35 px 而開始漏找。

對比則完全沒有處理:`weak_threshold` / `strong_threshold` 是拿**原始灰階梯度的
平方**直接比較,沒有正規化。而且兩者作用對象不同 —— `strong` 只在教導時決定哪些點
成為模板特徵,`weak` 每一幀都用在現場影像上。現場比教導暗的話,場景邊緣掉到門檻
以下、根本沒被量化,分數崩掉,但模板完全正常。症狀是「同一個 def 換一台機器就是
找不到」,不是「找錯」。

---

## 2. 三條測試路徑,只有第三條有效

| 改什麼 | 結果 | 能驗到什麼 |
|---|---|---|
| `def_mmpp`(def 的 `mmpp`) | **訓練直接失敗** | 什麼都驗不到 |
| `cur_mmpp`(`cam_param`) | 訓練正常,定位失敗 | 算術與接線 |
| **影像尺度 + `cur_mmpp` 同步** | **定位成功** | 端到端 |

### 為什麼改 `def_mmpp` 必然失敗

`def_mmpp` 身兼兩職,不可能只動其中一個:

1. **訓練時**把 def 的 mm 幾何換算成**模板影像像素** —— origin(`:6970`)、遮罩
   (`:7012`)、ROI 多邊形(`:7002`)、signature 半徑(`:7033`)
2. **執行時**當 `ensureShapeScale` 的分子

模板 PNG 是固定的 2448×2048。把 `def_mmpp` 加倍,`originPx` 就從 (1082, 670) 掉到
(541, 335),遮罩跟著移到沒有特徵的地方:

```
[shape] masked features=0 too few; retrying without mask
[shape] only 0 features extractable; aborting shape training
[shape] training failed; falling back to sig360 for this def
```

**而 `ii_dump` 的輸出看起來完全正常** —— 三組都「定位成功」、數值相近。真相是兩組
退回 sig360 了,而且它們彼此的數值完全相同(同一個引擎),只有基準組是 shape。
**不撈 log 就會得到「三種倍率都正常」這個完全錯誤的結論。**

### 有效的做法:`cam_param`

def 的 `featureSet[0].cam_param.{ppb2b,mmpb2b}` 會直接寫進校正圖
(`wiringPanel.cpp:4530-4536` 的 II 路徑,`:6636` 的 `apply_def_cam_param`),也就是
`cur_mmpp`。訓練用 `def_mmpp`,兩者互不干擾,可以乾淨隔離。

```
mmpP_ideal = calibmmpB / (map_loca_scale * calibPpB)
```

這台 `map_loca_scale == 1`,所以 `mmpb2b` 就是 mmpp。

---

## 3. 算術驗證(改 `cur_mmpp`,影像不動)

```
             cur_mmpp     want    預測 ms_eff   實際 log
 baseline   0.0138859     1.00    0.50(no-op)  沒印 log        ✓
 coarser    0.0277719     0.50    1.0000       1.0000          ✓
 finer      0.0069430     2.00    0.2500       0.2500          ✓
```

```
[shape] match_scale 0.5000 -> 1.0000 for model scale 0.5000 (def_mmpp 0.013886)
[shape] match_scale 0.5000 -> 0.2500 for model scale 2.0000 (def_mmpp 0.013886)
```

像素變粗(零件變小)→ 粗比對**關閉**;像素變細(零件變大)→ 壓到 0.25。夾限也有
作用:coarser 算出 1.0 剛好觸上界。

---

## 4. 端到端驗證(影像尺度 + mmpp 同步)

用 `img_scale.mjs` 把教導圖縮放,配上對應的 `cam_param`,物理尺寸應維持不變。

```
measure   base      half(x0.5)  偏差      dbl(x2)   偏差
  [0]    2.3173    2.3202     +0.12%    2.3177    +0.02%
  [1]    1.0909    1.1359     +4.13%    0.9972    -8.59%
  [2]    2.7985    2.7926     -0.21%    2.8079    +0.34%
```

**兩種尺度都定位成功**(對照組:mmpp 單獨改而影像不動時,x2 完全找不到)。大尺寸
特徵在 **0.35% 以內**;最小的 1.09mm 偏差 **4–9%**,不可用 —— 它在半尺寸圖上只剩
**39 px**(原圖 78 px)。

### 這個方法的限制,不可略過

縮放圖是從同一張拍攝**內插**出來的,不是真正的光學倍率變化:

- **縮小**接近真實(2×2 binning 本質就是平均,資訊確實變少)
- **放大是假的** —— 沒有增加任何真實資訊。真實的 2 倍放大會給更多細節,量測應該
  變準而不是變差

所以 dbl 的 −8.59% **不能歸咎於程式碼**。這個方法分不開「程式誤差」和「重採樣
誤差」,唯一能分開的是真實 binning 或真的換鏡頭。同樣的限制也適用於 fake camera
路徑 —— 只要餵的是重採樣的圖就分不開。

---

## 5. 安全性發現:尺度錯誤時不會乾淨失敗

驗證過程中的對照組(mmpp 改成 2 倍、影像不動)本該完全找不到零件,實際上:

```
similarity 0.571      min_score = 50      -> 通過
真實零件在 px(1082.0, 670.1)
它找到的在 px(1132.0, 651.8)              -> 差 50 px, -18 px
```

**一個尺度完全錯誤的模型,在錯誤的位置拿到通過分數**,然後機器會從那個位置開始
量尺寸、輸出判定。

失敗模式是**不對稱**的:

| 模型尺度 | 結果 |
|---|---|
| 縮小(0.5) | **偽匹配過關**(57 分),位置錯 50 px |
| 放大(2.0) | 乾淨地找不到(放不進檢驗框) |

縮小後的模板是原物件的子集,容易在局部結構上找到「像」的東西;放大則直接放不下。
`ensureShapeScale` 的 `[0.2, 5]` 防護對 2 倍完全無感。

這和時戳配對那邊反覆警告的是同一類問題,只是換到空間域 ——
*「時鐘放不了位置的影格,這台機器不可以拿去分選」*、
*「停機是可以看見的,悄悄分錯的料件不是」*。**空間上目前沒有等價的防線。**

**建議的廉價防線**:定位成功後,交叉檢查匹配區域的實際尺寸是否符合當前 scale 下的
預期。模型被縮成一半時尺寸會明顯不符,而這個檢查不依賴 similarity 分數。報告裡
已有 `area`,`shapeFeatureSet->levels[0]` 有模板尺寸。

**我認為這比參數自動化更該優先** —— 自動化解決的是「設定麻煩」,這個解決的是
「會靜默給出錯誤量測」。

---

## 6. 順帶發現

### 檢驗框小於零件的旋轉掃掠(生產中每幀在印)

```
[shape] inspection region 366x294 is under the part's rotation sweep 298x298:
        it can be found at some angles but not all. NOT reported as an error
```

框高 294 < 298,所以零件在某些角度放不進框就找不到,**依角度隨機漏找**,操作員只
會看到良率略低而沒有任何錯誤訊息。差距只有 4 px,框高調到 305 左右即可覆蓋。

**但這也可能是刻意的用法** —— 用緊的方形框把轉 45 度的料排除在考慮之外。程式碼
刻意把這件事分成兩個問題,(a) 完全放不下才 raise `INSP_REGION_TOO_SMALL`,
(b) 只是部分角度放不下就維持一行 log,因為「這裡的漏找不是框有問題的證據」。

若角度篩選是**意圖**,用框達成的代價是:無聲(和「沒有零件」無法區分)、條件是
`w|cosθ| + h|sinθ|` 這種幾何式而非「±X 度」、而且**會隨倍率漂移**(判定用
`levels[0]` 乘 `shape_built_scale`),在跨機種移植的脈絡下特別不舒服。

### `matching_angle_margin_deg` 沒有接到 shape 路徑

def 早就有 `matching_angle_margin_deg` / `matching_angle_offset_deg`(`:2028`),
sig360 路徑有用(`:5497`),但 **shape 路徑一次都沒引用**,固定搜尋全 360°:

```cpp
modc.angle.start = 0; modc.angle.end = 360; modc.angle.step = shape_angle_step_deg;
// Diagnostic override: SHAPE_ANG_RANGE="start,end" ... to test a constrained
// angle search before wiring it to the def margin.
```

註解自己寫了「在接到 def 的 margin 之前」。所以要做角度篩選,正規欄位已經存在、
只是還沒接;第三條路是定位後用報告裡的 `rotate` 判定,那個能給操作員看得見的理由。

---

## 7. 工具

- **`img_scale.mjs`**(新增)`node img_scale.mjs in.png out.png factor`,用 Chromium
  縮放(這台沒有 PIL 也沒有 sharp,沿用 `crop_zoom.mjs` 的做法)。
- **`ii_dump.mjs <def> <img...>`** 離線 II,不碰機台。**注意:def 檔是由客戶端讀取的
  ,路徑相對於 cwd,而 `ws` 只在 `tools/webctl/node_modules`,所以要在該目錄執行。**
- **`logdump.mjs`** 撈 log ring 到 `Core0_1/latest_dump.dump`。**這是必要的** ——
  `ii_dump` 的輸出無法區分 shape 與 sig360 fallback,只有 log 能。
- 模板路徑要注入到 **sub-feature** 物件(`featureSet[0]._ref_image_path`),不是 def
  頂層 —— 那個鍵是 sub-feature 自己解析的(`:2127`)。加在頂層會靜默失敗。

### fake camera(還沒跑)

`FORCE_BMP_CAROUSEL=<資料夾>` 取代真實相機(`wiringPanel.cpp:10804`),控制介面是
`SC` 的 target `bmp_carousel`,支援 `next`/`prev`/`replay`/`jump`/`pause`/`resume`
(`:5390`)。它比離線 II 多驗到站體檢驗框、淨空區、報告路徑、板子配對 —— 整條即時
鏈。但**量測精度那一題它答不了**,因為圖仍是重採樣的。當成功能性驗證用。

---

## 8. 待辦

1. **尺度合理性交叉檢查**(第 5 節)—— 優先,因為它擋的是靜默錯誤量測
2. 檢驗框 294 → 305,或明確記錄 294 是刻意的角度篩選
3. `matching_angle_margin_deg` 接到 shape 路徑
4. 對比自適應:教導時從模板梯度分布推導 `strong_thres`(`shape_cache_fingerprint`
   已經把 weak/strong 算進指紋,自動推導的值會正確地讓快取失效)
5. 真實 binning 的座標系整合 —— `eval_clean_regions`(`:2776`)和 shape 站體裁切
   目前**沒有除以 dsampLevel**,而 group 的 cage 有。這可能在
   `inspection_downsample > 1` 時就已經是既有缺陷,值得先查
