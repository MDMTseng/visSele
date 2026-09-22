# 建立新物件 — UX 流程現況

> **現況文件。** 描述的是 `2.0.0-rc2` 目前程式碼實際的行為。發現不符就當場改這份。
> 視覺版(流程圖、畫面示意)在 artifact:
> <https://claude.ai/code/artifact/a56a2951-2634-4041-ba21-2f5c45ddae66>
>
> 相關:`WEBUI_CAVEATS.md`(症狀導向)、`InspectionCore/docs/DEF_FILE_FORMAT.md`(def 欄位)。
> 檔名穩定、行號會漂 — grep 文中提到的符號。

涵蓋範圍:從 DefConfUI 工具列按下 **take**,到 SBM 定位設定完成、可以存檔為止。
以及舊 def 要怎麼進到同一條路上。

---

## 0. 一頁摘要

```
take → ① 取名(必填) → ② 滿版取景器 → ③ SBM studio(自動開啟) → 回主編輯器存檔
                ↑ 取消什麼都不變        ↑ 取消停串流+還原影像

舊 def:載入後若 locating_engine != shape_based,工具列沒有任何 SBM 入口。
        設定頁「→ migrate to shape_based」按下去才會有,並直接開 studio。
```

三件事決定這條流程的形狀,都不是版面偏好:

1. **量測全部是相對於 `def_image_reg` 寫下來的。** 沒有它就存檔,特徵和量測會釘在一個
   跟零件無關的原點上 —— 而且它照樣檢驗得出數字。
2. **比例尺(mm/px)屬於「拍這張圖的那台機器」,不屬於 def。**
3. **轉換定位器有代價**(特徵要重訓、def 要重存),所以必須是人主動選的。

---

## 1. 進入點

| 進入點 | 條件 | 程式位置 |
|---|---|---|
| 工具列 **take** | 永遠可見(dirty 時先跳確認,見 §1.5) | `DefConfUI.js` `key="TAKE"` |
| 工具列 **SBM定位設定** / **SBM定位設定 2** | 只在 `edit_info.locating_engine === 'shape_based'` | `key="SBMSETUP"` / `key="SBMSETUP2"` |
| 設定頁 **→ migrate to shape_based** | 只在 `locating_engine !== 'shape_based'` | `ACT_Migrate_To_Shape` |

> **SBM 按鈕以前是無條件顯示的,而且一按就 `dispatch(Locating_Engine_Update('shape_based'))`。**
> 也就是只想點進去看看的人,就把一個 sig360 配方轉換掉了。現在轉換只屬於 migrate 那一顆。

---

## 1.5 進 take 之前:未儲存變更的確認

只在 **dirty** 時出現。判斷方式和返回鍵完全一樣:
`defFileGeneration(edit_info).featureSet_sha1 !== edit_info.DefFileHash`
—— 刻意共用同一個定義,「這個 def 髒了嗎」有兩個不同答案比任何一個答案都糟。

| | |
|---|---|
| 標題 | 目前的變更還沒存檔 |
| 預設鍵 | **先回去存檔** |
| 另一個 | 丟掉變更,繼續建立新物件 |

> **契約:打開這個對話框就是從「上次存檔的狀態」開始。取消也不例外。**
>
> 這是刻意選的規則,不是副作用。要在未存的編輯之上做 take,等於得替每一條路徑
> 決定「編輯器的哪一半活下來」—— 而那是一組沒有人會列舉正確的狀態。
> 一條規則就好:**建立新物件從上次存檔的狀態開始;取消就把 def 重新載入回去。**
>
> 所以取消**不是**回到你當下的編輯狀態,是回到磁碟上的版本。對話框文案就是這樣寫的。

---

## 2. 畫面 ① — 取名

`TakeSetupDialog`,`phase === 'name'`。

| 欄位 | 必填 | 去向 |
|---|---|---|
| 物件名稱 | **是** | `edit_info.DefFileName`,同時決定新檔名 |
| 標籤(逗號分隔) | 否 | `edit_info.DefFileTag`(陣列) |

- 名稱空白時「下一步」是 disabled。名稱必填的理由:確認之後這是一個**獨立存檔的新物件**,
  放行空白等於讓存檔對話框沿用前一個配方的檔名 —— 那是它唯一不能叫的名字。
- **此時完全沒有 dispatch。** 名稱、標籤、switch 都是元件內的 local state。
- Enter 鍵等同「下一步」。

---

## 3. 畫面 ② — 取景器

`TakeSetupDialog`,`phase === 'capture'`。Modal `width: "96vw"`、`bodyStyle.height: "86vh"`。

### 3.1 版面

```
┌──────────────────────────────────────────┐
│  影像(flex 撐滿,自己不捲動)              │  ← TakePreviewCanvas
│  [● 串流中] 或 [等待觸發訊號…] badge      │
├──────────────────────────────────────────┤
│ 名稱  [switch 清除/保留]  ▶串流 ⏱觸發 ✓使用 取消 │
│ 說明文字 + 比例尺來源                      │
└──────────────────────────────────────────┘
```

### 3.2 三種取得影像的方式

| | 畫面行為 | 樣板來源(core cache) | mm/px 來源 |
|---|---|---|---|
| **▶ 開始串流** | 連續更新;其他控制停用,只留紅色停止鍵 | `__LAST_DATA_VIEW_CACHE_IMG__` | 機台 `data/lens_calib.json` |
| **⏱ 等待觸發** | 畫面凍住 + 橘色 badge,最多 10 秒 | `__CACHE_IMG__` | 機台 `data/lens_calib.json` |
| **✓ 直接使用** | 不動作,拿畫面上現有的圖 | `__CACHE_IMG__` | **這個 def 原本的** |

- 「使用這一幀」在 `!hasImage`(`edit_info.img` 為空)時 disabled。全新機器、或 def 的 `.png`
  被刪掉但開機預設載入的路徑還在時,就是這個狀態 —— 那時只能串流或等觸發。
- 串流中所有其他控制都 disabled,這是規格,不是 bug。停下來時**畫面上凍住的那一幀就是要用的**。

### 3.3 串流怎麼實作的

```js
ST  { CameraSetting: { trigger_mode: 0, down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL } }
CI  { _PGID_: 11007, _PGINFO_: { keep: true },
      definfo: { type: "stage_light_report", ... }, IMG_ignore_calib: true }
停止:CI { _PGID_: 11007, _PGINFO_: { keep: false } }
```

- **核心沒有「不帶 def 的純預覽」。** `wiringPanel.cpp` 的 CI/FI 處理器對
  `deffile == NULL && defInfo == NULL` 直接拒絕。
- 送真 def 會讓量測引擎跑在一個還沒有特徵的零件上,畫面被 NA 蓋掉。
  `stage_light_report` 是 `CalibrationUI` 已經在用的輕量型別:出畫格、不做量測。
- **PGID 11007 是這條流程專用的。** 快速驗證用 11004、鏡頭校正用 10105 —— 分開才不會
  停這條的時候誤殺別人的訂閱。

### 3.4 等待觸發

沿用既有的 `EX` 單張路徑,`trigger_type: 2`、`timeout: 10000`。畫面在等待期間是凍住的
(這是選定的行為),所以**一定要有 badge** —— 沒有 badge 的等待畫面和當掉的畫面長得一模一樣。

失敗(沒等到 / 取像異常)會 `message.error` 並回到閒置,**不會**改動任何 def 狀態。

### 3.5 switch:清除 / 保留量測設定

預設 **清除**。這是「建立新物件」,從空白開始是預期;保留錯的量測比重畫一次貴。

| | 影像 | 量測特徵 | 比對參數 | 定位線 | SBM 特徵 | include/exclude | ROI 點 |
|---|---|---|---|---|---|---|---|
| 清除 | 新的 | 清空 | 清空 | 清空 | 清空 | 清空 | 清空 |
| 保留 | 新的 | **保留** | **保留** | 清空 | 清空 | 清空 | 清空 |

右邊五欄兩種模式都清空,而且**沒有選項可以留下它們**:它們描述的是一個已經不存在的座標系。
量測特徵不一樣 —— 它們是對著**零件**畫的,所以留得住。

實作:`DefConfAct.Def_Retake(keepMeasurements)`,reducer 依旗標選擇要清哪一組鍵:

- `DEF_SCOPED_EDIT_INFO_KEYS` — 全部(清除模式)
- `DEF_LOCALIZER_SCOPED_KEYS` — 只有定位器的那些(保留模式),
  同時把 shapeList 裡的 `loc_include` / `loc_exclude` 濾掉

兩份清單都在 `UTIL/InspectionEditorLogic.js`。**新增一個 def-scoped 設定時,兩邊都要看一次。**

### 3.6 取消

`closeTake()`:

1. `stopStream()` — 一定送,即使沒串流過(冪等)
2. `reloadSavedDef()` — `loadDefFile(defModelPath, ...)`,整個 def 重載回上次存檔的樣子
3. 關閉 modal

**取消 = 重載,不是「還原編輯狀態」。** 串流的畫格落在 `edit_info.img`,那也是 def 自己影像的
槽,所以取消後畫布本來就得換掉;與其做一個「只換圖、保住編輯」的特例
(那需要照 `switchImage` 的做法只 dispatch IM 那一個封包),不如整個重載 ——
**「取消了但還是髒的」是一個這個畫面其他地方都不存在的狀態,而沒被列舉的狀態就是 bug 住的地方。**

> **`__img_fresh_capture` 為真時跳過重載。** 那表示上一次 take 之後還沒存過檔,
> `defModelPath` 指的是一個還不存在的檔案 —— 載入失敗會留下一個被清空的 def 配一張串流畫格,
> 比什麼都不做更糟。

> **順帶記一個機制:** `BPG_WS.js` 送出時**沒給 `promiseCBs` 的話,回覆會整包
> `WSDataDispatch`**,也就是每個封包都 map 成 action 丟進 reducer。
> `useDefImages.switchImage` 就是為了避開這個才刻意只 dispatch IM 那一個封包
> —— 換樣本圖不能連帶把 `sig360_extractor` 報告送進 `Edit_info_reset`。
> 寫 fire-and-forget 的 LD 之前先想一下這條。

---

## 4. 確認之後(`finishTake`)

順序**不能改**,每一步都有理由:

```js
stopStream();
dispatch(Def_Retake(opt.keep));                    // ← 一定在最前面
ACT_Cache_Img_Save(CORE_ID, TMP_REF_BASE, opt.srcType);
dispatch(EditInfo_Patch({ __tmp_ref_image_path: TMP_REF_BASE + ".png" }));
dispatch(DefFileName_Update(opt.name));
dispatch(DefFileTag_Update(opt.tags));
dispatch(Locating_Engine_Update('shape_based'));
if (opt.fromCamera) loadInstrumentMmpp().then(mmpp => dispatch(Instrument_Mmpp_Set(mmpp)));
claimNewDefPath(opt.name).then(newPath => {
  ACT_Def_Model_Path_Update(newPath);
  setModal_view(undefined);
  setTimeout(() => openSBM2(true), 0);             // ← 延後一個 tick
});
```

| 步驟 | 為什麼 |
|---|---|
| `Def_Retake` 最前面 | 它會清掉所有 def-scoped 的鍵。名稱、標籤、定位器、暫存樣板路徑寫在它**前面**會被自己清掉。 |
| `ACT_Cache_Img_Save(..., srcType)` | 核心的定位器只從**磁碟上的檔案**訓練,沒有任何路徑會用記憶體裡的影像。 |
| `TMP_REF_BASE = "data/__retake_ref"` | 固定一個檔名,下一次擷取覆蓋上一次。每次取一個新名字會在 `data/` 留一堆永遠沒人刪的檔案。 |
| `Locating_Engine_Update('shape_based')` | take 就是「這是一個 SBM 物件」。這是唯一一個不用猜的場合:操作者剛剛說了他要建新零件、並挑了要用哪一幀。 |
| `setTimeout(..., 0)` | 上面那串 dispatch 要先落地。提早掛載的話,studio 讀到的是**剛被清空之前**的 `edit_info`。 |

### 4.1 樣板來源:兩個 cache 不是同一個

| | 內容 | 誰更新它 |
|---|---|---|
| `__CACHE_IMG__` | 進 DefConf 時載入的影像(def 自己的 `.png`) | `LD`、`EX` |
| `__LAST_DATA_VIEW_CACHE_IMG__` | 最後一張通過 data view 的畫格 | CI/FI 串流 |

**串流不會更新 `__CACHE_IMG__`。** 搞錯的話會把前一個配方的圖存成新物件的樣板,
而且畫面上完全看不出差別。`saveAlternateImage` 的註解記著上一次踩到這個。

### 4.2 比例尺:跟著圖走

| 影像來源 | mm/px |
|---|---|
| 串流 / 等待觸發(相機實拍) | `data/lens_calib.json` 的 `um_per_px / 1000`,退而求其次 `1 / m` |
| 使用現有圖像 | 這個 def 原本的,不動 |

`Instrument_Mmpp_Set(mmpp)` 做兩件事,第二件比較不明顯:

1. `_obj.instrumentMmpp = mmpp`
2. **`_obj.sig360info = null`** — `getEditorMmpp()` 第一順位讀 sig360info,而 `Def_Retake`
   **不會**清它。不清的話舊 mmpp 一直贏,剛寫進去的值永遠讀不到。而且那個簽章描述的是一個
   已經不在畫面上的零件。
   (用指派而不是 `Setsig360info(null)`:那個 setter 第一行就 deref `sig360info.reports[0]`。
   def 載入器自己的 no-signature 分支也是直接指派。)

`getEditorMmpp()` 的順位:**真的 sig360 報告** > `instrumentMmpp` > `cam_param.mmpb2b/ppb2b` > 1。
sig360 報告排第一是因為它是對**這張圖**的量測,比機台的標稱值更準。

> **讀不到 `data/lens_calib.json` 會跳警告,不會安靜地用代用值。**
> 一個沒有誠實比例尺的 def,比一個還不能量測的 def 更糟 —— 每個尺寸都是 `px × mmpp`,
> 錯的比例會讓整個 def 用一個一致的、看起來合理的、錯的數字量出來。

### 4.3 新檔名

`claimNewDefPath(name)`:

1. 取原本 `defModelPath` 的**資料夾**(那是它的同類檔案住的地方)
2. 檔名不合法字元換成 `_`
3. `FB { path: dir, depth: 1 }` 列出資料夾,撞名就依序試 `名稱[1]`、`名稱[2]`…
4. **列不到就退回純名稱**,交給存檔瀏覽器自己的「檔案已存在」提示

> 列表失敗不該擋住拍照 —— 這是刻意的 best-effort。

結果寫回 `defModelPath`,所以存檔對話框會開在同一個資料夾、預設新名字,
**原本的 def 檔案不可能被蓋掉**。

---

## 5. 畫面 ③ — SBM studio(v2)

`SBMStudio2.jsx` 的 `SBMSetupView2`。由 take 自動開啟(標題「新物件 — 先設定定位」),
或由工具列 / migrate 開啟。

> **v2 是 v1 的整份複製,而且是刻意的。** 兩顆按鈕都留著,新版出問題時線上可以立刻退回舊版。
> 只有 view 重複;`HookCanvasComponent` 和所有純函式(`sbmSweep`、`sbmInspectResult`、
> `matchThreshold`)都是 import 原本的。**重要修正要改兩邊。**

### 5.1 版面

- 畫布**固定、自己不捲動**,整張圖永遠可見。橫向側欄在右、直向在下
  (flex + 一條 `max-aspect-ratio: 1/1` media query)。
- 進度條**釘在側欄上方**,不隨內容捲走。
- **只有中間的控制區會捲。**
- 底部固定一顆大按鈕,用**文字**寫出下一步是什麼。
- 可點的都 ≥ 40px,畫布工具 44px,主按鈕 48px。
- 工具說明是畫布下方的常駐一行,**不是 tooltip** —— 目標機是 Surface Go,可能觸控,
  hover 不存在。其中包含「改了定位特徵必須重新生成」這種會讓 def 安靜壞掉的警告。

### 5.2 三個步驟(而且只有三個)

| # | 完成條件(**查真實狀態算出來的**) |
|---|---|
| 1 定位 | `Number.isFinite(def_image_reg.cx)` |
| 2 特徵範圍 | 至少一個 `loc_include` |
| 3 生成特徵 | `__shape_cache` 存在 **且** `!__shape_stale` |

三格都綠,這個 def 就能用。**不是「點過就打勾」** —— 可以用點擊推進的進度條是會說謊的進度條。

### 5.3 工具(選用,不影響進度)

「工具 · 選用,不影響進度」標題下面:**測試**(單次檢驗、全資料夾)、**強健性掃描**、
**ROI 取樣點**、**參數**(min score / coarse scale / angle / NMS / face / weak / strong)。

> **測試不是關卡。** 如果驗證是第四步,「跳過第四步」就會變成日常動作 —— 而一個天天被跳過的
> 步驟,等於訓練所有人忽略進度條,連帶「特徵已失效」也一起被忽略。

---

## 6. 會擋人的地方

三個都是**問一次,不是拒絕**,而且都留了出口。
沒有出口的 modal 比它想防的狀態更糟(這個分支前面就用一個診斷把離開按鈕卡死過)。

| 觸發時機 | 條件 | 選項 |
|---|---|---|
| 按下 take | def 有未儲存變更 | 先回去存檔 / **丟掉變更繼續**(見 §1.5) |
| 離開自動開啟的 studio | `auto && !def_image_reg` | 留下來設定 / **仍要離開** |
| 存檔 | `isShapeEngine && __img_fresh_capture && !def_image_reg` | 去設定定位 / **仍要存檔** |
| 離開 studio、存檔 | `isShapeEngine && __shape_stale` | 還原上一版 / **仍要離開(存檔)** |

- 存檔那兩個都要求 `locating_engine === 'shape_based'`。sig360 從輪廓自己找物件座標系,
  **不需要定位線** —— 在那邊問等於問一個它沒有也不想要的東西。
- 「特徵已失效」擋的不是「你沒測」,是**「你即將存下一個不會用 SBM 定位的 def」**。
  那是狀態錯誤,不是流程未完成 —— 所以一個是紅框、一個是進度條。

---

## 7. 舊 def 的兩條路

1. **按 take** — 變成一個新物件,走上面的主流程。原本的檔案不會被動到。
2. **設定頁「→ migrate to shape_based」**(`ACT_Migrate_To_Shape`)—
   - `Locating_Engine_Update('shape_based')` + `Shape_Match_Scale_Update(0.3)`
   - **從 sig360 的簽章錨點種 `def_image_reg`** — 沒有它的話 `drawImage` 會 translate
     `-(0,0)`,物件座標系原點落在**影像角落**,零件會繞著角落轉。在參考影像上量起來正常
     (旋轉是 0),在轉過角度的零件上就錯,而且沒有東西會報告。
   - 直接開 studio。轉換只是一半:def 現在用一個沒有訓練特徵的定位器,在按下「生成特徵點」
     並重新存檔之前,它會退回 sig360 —— 從外面看跟沒轉換一模一樣。

---

## 8. 已知落差 / 尚未做的事

| 項目 | 狀態 |
|---|---|
| **ROI 改了仍會讓特徵失效** | UI 已把 ROI 標為「選用」,但 `UICtrlReducer.js` 的 `touched` 陣列仍含 `roi_refine_points`,核心指紋也仍把 ROI 點算進去。要改需要相容比對(直接改指紋格式會讓**現有每個 def 的快取都失效一次**)。`SBMStudio2.jsx` 的 ROI 區塊有註解標明這個落差。 |
| 三種測試(A 掃描 / B 全幅 / C 多原圖增強) | **只有 A 存在**(單軸)。B、C 和人工標註都還沒做。 |
| 兩階段參數調校 | 未實作。 |
| v1 的「還沒有樣板影像」誤報 | v2 已修(有 retake sidecar 時不再誤報);v1 維持原樣,它還是後備。 |
| **未實機驗證** | 串流後取消畫布有沒有回到 def 圖;保留模式下量測是否真的留著、SBM 是否真的清掉。兩者都要**瀏覽器**,`take_cache_probe.mjs` 那條路碰不到。串流那一幀已經驗過了(見 §8.5)。 |

---

## 8.5 怎麼測

| 層 | 需要什麼 | 涵蓋 | 狀態 |
|---|---|---|---|
| ① 純邏輯 | 什麼都不用,<1s | `refPngPathOf`、`nextFreeName`/`takenNamesFrom`、`pickMmpp`/`mmppFromLensCalib`、兩份 def-scoped 鍵表的一致性 | **已有**:`unit_defnaming.mjs`、`unit_mmpp.mjs`、`unit_def_scoped_keys.mjs`,都在 `suite_nohw.mjs` 裡 |
| ② 瀏覽器＋core | 無相機 | take → 確認 → 取名 → 使用現有圖 → `finishTake` → studio 開啟 → 三格進度 → 三個 guard → 生成特徵點 | 探針未寫,但 fixture 已備妥:`fixtures/sbm_synth.png` + `.hydef`(見下) |
| ③ ②＋`FORCE_BMP_CAROUSEL` | 無相機 | 串流起停、取消重載 | 未寫。核心的 BMP carousel(`wiringPanel.cpp` 的 `FORCE_BMP_CAROUSEL=<folder>`)就是為無頭測試做的,畫格是確定性的 |
| ③′ core＋相機,不用瀏覽器 | **要相機** | 兩個 cache 的差異 | **已有**:`take_cache_probe.mjs`,已實機通過 |
| ④ ＋裸板 | 無相機 | 等待觸發(EX trigger_type 2)—— 裸板的 `trig_cam_pulse` 就能產生訊號 | 未寫 |

### shape_based 的 fixture

現有的 `caliper_verify_tagged` 是 **sig360** 的 def —— 改版之後 sig360 的 def 在工具列上
根本看不到 SBM 按鈕,所以拿它測不到 studio。因此另外做了一組:

```
fixtures/make_sbm_fixture_image.py   畫出零件(純 stdlib,無 PIL/numpy)
fixtures/sbm_synth.png               2448×2048,26 KB
fixtures/make_sbm_fixture_def.mjs    要一個跑著的 core 抽特徵,寫出 def
fixtures/sbm_synth.hydef             shape_based,28 KB
```

**合成而不是真實照片,因為這個 repo 是公開的。** 一張真實畫格會把客戶零件的外形、
表面處理、孔位發布給任何 clone 的人。畫出來的零件不會外洩任何東西,而且是更好的
fixture:每個像素都是這裡決定的,兩台機器拿到位元完全相同的輸入,測試失敗就代表程式
變了、而不是相機變了。體積也差 100 倍(26 KB vs 2.9 MB)。

形狀的四個條件都不是隨便挑的:**不碰邊界**(`trainShapeMatcher` 取「不碰邊的最大連通
區域」,碰邊會輸給背光場)、**旋轉不對稱**(n 重對稱會讓旋轉掃描回報 360/n 度的殘差,
看起來像災難性誤差但不是)、**有內部細節**(只有外輪廓的話位置準、角度差)、**邊緣要
軟**(一像素的階躍是真實鏡頭產不出來的梯度)。

**def 必須由核心生成,不能手寫。** `shape_cache` 帶著一個涵蓋樣板和每個抽取參數的指紋,
核心載入時會重算 —— 對不上就當 stale 拒絕,而隱式重抽是關掉的,所以手寫的 fixture 會
載入成功、拒絕自己的特徵、退回 sig360,正好是這個 fixture 要用來測的失敗。

已驗證(核心日誌):

```
[shape] loaded 193 features from def cache (no re-extraction)
[shape] from def cache: crop [0,0 2448x2048] origin(1224.0,1024.0) variants=360
```

**踩過一次值得記下來:`localization_include` 是物件座標系的 mm,不是像素**
(`FeatureManager_sig360_circle_line.cpp` 的「Object-frame mm polygon arrays」)。
第一版寫成像素,大了 80 倍,遮罩什麼都沒框到,核心印
`masked features=0 too few; retrying without mask` 然後拿整張圖去抽 ——
**fixture 照樣做出來了,這才是問題**:它會帶著一個什麼都沒做的區域出貨,
第一個相信它的人會得到「區域功能沒用」的結論。

### 尚未解決

在**這台開發機**上對 fixture 跑 II 得到 `matches=0`,原因是核心用機台的校正把模型重新
縮放了:`def_mmpp=0.0125` vs `cur_mmpp=0.013886` → 模型縮到 0.9002。fixture 不該依賴
某一台機器的鏡頭校正,所以第②層探針要嘛連 `data/lens_calib.json` 也 fixture 化,要嘛
找出讓 II 真正忽略校正的方式(這次傳的 `calibInfo:{type:'disable'}` 沒有阻止縮放)。

另外 II 會把 def **解析兩次**,第二次沒看到快取並印
`SBM features not trained (sig360 fallback in use)`。第一次是命中的,所以定位器是好的,
但這兩行同時出現在日誌裡會誤導追問題的人 —— 還沒查清楚哪一次才是實際用來比對的。

### 兩個 cache:已用真相機驗過

`take_cache_probe.mjs`,不需要瀏覽器。它做的是 take 對話框做的那串動作 ——
載入一張已知影像 → 自由跑 → 串流 `stage_light_report` 八秒 → 停 → 從**兩個** cache 各存一張。
實測(2026-08-29,MV-CA050-11UM):

```
__CACHE_IMG__          2448x2048   ← 仍是載入的那張
__LAST_DATA_VIEW__     1856x660    ← 相機畫格,帶著相機的 ROI
載入的影像              2448x2048
```

**所以「使用這一幀」在串流後選 `__LAST_DATA_VIEW_CACHE_IMG__` 是對的**,而串流確實不會
動到 `__CACHE_IMG__`。搞錯的話會把前一個配方的圖寫成新物件的樣板,而畫布上兩種情況
看起來完全一樣 —— 沒有任何一刻看得出不對。

寫這支探針時我踩了它要防的同一類錯:第一版用**檔案大小**判斷「還是同一張圖嗎」,
把一個正確的結果判成失敗。核心存檔會重新編碼,平坦的合成圖用 zlib level 9 壓過再被重編
幾乎脹一倍(25970 → 45355 bytes)。**尺寸有鑑別力,大小沒有。**

### 控件怎麼定位

**用 `data-testid`,絕不用位置、幾何或標籤文字**(`TEAM_HANDOFF.md` §13,那裡記著文字選擇器
解析到檔案瀏覽器、而且因為全是 icon-only 按鈕所以**點錯是靜默的**)。

這個流程特別會咬人的地方:

- `開始串流` / `停止串流` **會互換**,`等待觸發` 在串流時整個消失 —— 文字選擇器會找不到。
- **disabled 按鈕**(沒影像的 `使用這一幀`、串流中的全部):Playwright 的 click 會**等到 timeout**,
  不是乾脆失敗。所以每顆會停用的按鈕都額外帶 `data-enabled`。
- **`SBM定位設定` 是 `SBM定位設定 2` 的前綴**,文字選擇器抓第一顆會同時命中兩顆。
  兩顆分別是 `sbm-studio-v1` / `sbm-studio-v2`。
- **`IconButton` 原本不轉發未知 props**,所以掛在它上面的 `data-testid` 會被靜靜丟掉。
  已改成轉發所有 `data-*`(只有 `data-*`;整包 spread 會把 `onClick`/`dict` 灌到 DOM 上)。
- **studio 的核心操作是 canvas 拖曳/點擊**(定位線、多邊形頂點、ROI 點)—— 這是幾何,
  **testid 救不了**。要嘛用確定性的 fixture 影像算座標,要嘛開一個 dev hook 直接 dispatch。

### 掛上去的 hook

發布的是**語意**,不只是 handle —— 值得斷言的是「它用的是對的 cache」,不是「有一顆按鈕被點到」。

| testid | 帶的屬性 |
|---|---|
| `take` / `sbm-studio-v1` / `sbm-studio-v2` | — |
| `take-name` / `take-tags` / `take-next` / `take-cancel` | `take-next` 帶 `data-enabled` |
| `take-capture` | `data-phase`(idle/streaming/waiting)、`data-has-image`、`data-from-camera`、`data-src`(cache/lastview)、`data-keep` |
| `take-keep` / `take-stream-start` / `take-stream-stop` / `take-wait-trigger` | — |
| `take-use-frame` | `data-enabled` |
| `sbm2` | `data-step`、`data-done`(三格 0/1)、`data-stale`、`data-features`、`data-roi`、`data-regions`、`data-tool` |
| `sbm2-seg-<i>` | `data-state`(done/now/todo) |
| `sbm2-block-step-<i>` / `sbm2-block-opt-<i>` | `data-done`、`data-now`、`data-open` |
| `sbm2-tool-<id>` | `aria-pressed` |
| `sbm2-next` | `data-action`(locline/include/generate/close)、`data-enabled` |
| `sbm2-generate` | — |

`data-src` 和 `data-from-camera` 是這裡最值錢的兩個:**兩個 cache 拿錯、比例尺拿錯,畫面上都完全正常**,
所以斷言必須打在這兩個屬性上,不能只看有沒有跑完。

---

## 9. 相關程式位置

| 東西 | 位置 |
|---|---|
| take 對話框 | `UI/WebUI/src/DefConfUI.js` — `TakeSetupDialog`、`TakePreviewCanvas` |
| take 接線 | 同檔 `key="TAKE"` 的 onClick:`startStream` / `stopStream` / `waitForTrigger` / `finishTake` / `claimNewDefPath` / `loadInstrumentMmpp` |
| studio v2 | `UI/WebUI/src/SBMStudio2.jsx` |
| studio v1(後備) | `UI/WebUI/src/SBMStudio.jsx` |
| def-scoped 鍵清單 | `UI/WebUI/src/UTIL/InspectionEditorLogic.js` — `DEF_SCOPED_EDIT_INFO_KEYS`、`DEF_LOCALIZER_SCOPED_KEYS` |
| mmpp 順位 | 同檔 `getEditorMmpp()` |
| Def_Retake / Instrument_Mmpp_Set | `UI/WebUI/src/redux/reducer/UICtrlReducer.js` |
| 樣板路徑推導 | `UI/WebUI/src/UTIL/MISC_Util.js` — `refPngPathOf`、`stampRefImagePath` |
| 核心端訓練 | `InspectionCore/MatchingEngine/FeatureManager_sig360_circle_line.cpp` — `trainShapeMatcher()` |
| CI/FI 拒絕無 def 的請求 | `InspectionCore/Core0_1/wiringPanel.cpp` — `checkTL("CI"...)` |
