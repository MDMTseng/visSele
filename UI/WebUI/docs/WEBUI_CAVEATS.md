# WebUI — Caveats & Hard-Won Gotchas

Concrete traps in the 1st-gen WebUI (React + Redux + BPG-over-WebSocket), found while
bringing a real machine up on `ct/uinsp_2mach` (2026-08-05). Companion to
`InspectionCore/docs/CORE0_1_CAVEATS.md`. Each item: trap → why → what to do.

Most of these cost hours because the failure surfaced **somewhere other than its cause**:
a def the core refuses, a camera that never triggers, a screen that draws nothing. When
the UI and the core disagree, trust the core's log — it is the only place that records
what actually arrived.

> File paths are stable; line numbers drift — grep the named symbols.

---

## A. The def that leaves the browser is not the file on disk

### A1. `defFileGeneration(edit_info)` builds from the **live editor state**
`InspectionUI` sends `definfo: defFileGeneration(this.props.edit_info)` — not the `.hydef`
on disk. So "I removed that from the file" does not change what the core receives. A def
that fails to parse while the saved file looks clean is this, every time.
- **To check what the core actually got:** the core logs one line per FI/CI —
  `[FI] pgID:… hasDeffile:true (deffile:no definfo:yes)` — followed by the parse result.

### A2. `loc_include` / `loc_exclude` are localization regions, never features
They live in `featureSet[0].features` while being authored, and the core's sig360 parser
rejects an unknown feature type by failing the **whole def**:
`feature[7] has unknown type:[loc_include]` → `cJSON parse failed`. The engine is then left
with no features at all — `ImgInspection` returns in ~3 µs and every part is judged NA.
- `defFileGeneration` now strips them unconditionally. It used to strip them only inside the
  `locating_engine === 'shape_based'` branch, so any other locator shipped them to the core.
- **Why it kept coming back:** loading a def re-created them as shapes from
  `localization_include/exclude` (`InspectionEditorLogic.addRegionShapes`), so a def that had
  ever used the shape locator carried them in the editor forever. That round-trip is
  currently commented out; the regions stay in the file but are not editable on the canvas.

### A3. A def-less `CI`/`FI` is not inert
It falls through to `camera->TriggerMode(1)` and leaves whatever the engine already held, so
a run of them reads as an inspection session that quietly never reloads its def. The stream
subscribe/unsubscribe calls (`{_PGID_, _PGINFO_}` with no `definfo`) are exactly this shape.

### A4. `3 µs` is the tell
`ImgInspection … 0.003000ms` in `insp.log` means the engine has **no features** — a 2448×2048
frame cannot be matched that fast. A real inspection on this machine is 40–100 ms. Use this
to separate "def never arrived" from "def arrived and failed to parse" from "def is fine".

---

## B. Camera trigger policy is spread across three places

### B1. `trigger_mode` is overloaded, and `1` does **not** mean "software trigger"
The UI sends `{"CameraSetting":{"trigger_mode":0|1}}`, where the UI's meaning is
`0` = free-run, `1` = stop free-running. The core maps `1` to `TriggerMode(1)`, which must
keep the camera listening to the **hardware** line — on this machine the trigger rides the
backlight line driven by the peripheral board. Making `TriggerMode(1)` select
`TriggerSource=Software` (its nominal meaning) makes the camera deaf to the plate for as long
as the UI has the stream paused. A software trigger borrows the Software source for the
instant it fires instead (`CameraLayer_Aravis::Trigger`).

### B2. `APP_INSP_MODE.componentDidMount` free-runs the camera
It sent `trigger_mode: 0` unconditionally — right for CI (continuous preview), wrong for FI,
which pairs one frame to one part off the machine's trigger. The mount runs **before** the
FI branch arms the hardware trigger, so it silently undid it: frames arrived that no part had
asked for, and the core logged
`perif: frame with no pending trigger -- pairing desynced?` then
`result with no paired tid -- not sent`. Currently commented out entirely.

### B3. `timeout: -1` means "wait forever", on the main loop thread
`triggerSnapExam(trigger_type=0, timeout=-1)` reaches `CameraLayer::SnapFrame`, whose abort
thread returns immediately for a negative timeout — so nothing ever notifies the condition
variable. `SnapFrame` runs inside the WebSocket command handler, so one snap that never gets
a frame stops the core serving **every** client, with no log. Now clamped to 30 s in the core;
the UI still sends `-1`.

---

## C. Where inspection results actually come from

### C1. The canvas draws from `edit_info`, not from a report prop
`EverCheckCanvasComponent` reads `edit_DB_info.inherentShapeList` and the per-measure
`detailStatus` — which the **reducer** writes via `edit_info._obj.getMeasure_detailStatus`
inside `EVENT_Inspection_Report`. So the drawing pipeline is
`reducer → edit_info._obj → canvas`, and `this.props.inspectionReport` is not part of it.
- Consequence: "make InspectionUI render from the packets" is not a component change. The
  drawable state is produced by ~500 lines of reducer that `DefConfUI` and `script.jsx`'s SPC
  listeners also consume.

### C2. FI drops NA reports in the reducer
`UICtrlReducer` had `reportSkip = (inspMode=="FI") && (uInspResult == NA || UNSET)`, so in FI
a NA verdict was discarded before it could be drawn — while CI drew fine. On an empty plate
every verdict is NA, so **FI draws nothing and CI looks healthy**, with the core sending both
RP and IM in each case. Display policy living in the reducer is what makes this invisible.
- Note `__surpress_display` is a *separate* flag set a few lines above the same `break`, so
  "not into statistics" and "not displayed" are currently one parameter doing two jobs.

### C3. Everything is NA on an empty plate — by construction
A verdict needs `srep.size()==1` **and** `extra_area_ratio < 0.1`. On a bare plate the texture
labels ~1000 components and the target is ~2 % of the area, so the ratio is ~0.98. That gate
is a coarse "nothing else in the scene" filter that only works with an ROI, and is slated for
removal — do not tune it, and do not report it as a defect.

---

## D. Dev-loop traps

### D1. A util module edit does **not** hot-reload
Vite Fast Refresh only handles component modules. Editing `UTIL/MISC_Util.js` logs no HMR
update at all, so the running tab keeps the old `defFileGeneration` — and you will "fix" the
same bug repeatedly. **Hard reload (Cmd+Shift+R) after touching anything under `UTIL/`.**
- To check which copy a tab is running:
  `(await import('/src/UTIL/MISC_Util.js')).defFileGeneration.toString().includes('…')`
  — the dynamic import hits the tab's module registry, not the server.
- Several components also log `Could not Fast Refresh ("…" export is incompatible)`; those
  invalidate rather than refresh, which also needs a real reload.

### D2. Duplicate class members silently win
`uInspESP32_API` defined `trigPhantomPulse` twice; the later one sent `trig_phamton_pulse`
(typo) and shadowed the working one, so the button never did anything. Vite prints
`Duplicate member … in class body` at startup — read it.

### D3. `insp.log` is a ring — file order is not time order
The drainer wraps, so `tail insp.log` can show you data from an earlier run. Filter by the
bracketed timestamp instead, and remember it resets per ring name. Getting this wrong makes
you "confirm" that a packet never arrived when it arrived twenty minutes ago.

### D4. The core can be up, listening, and still unreachable
`mainLoop` binds 4090 **before** camera init and only serves WebSocket after it. If the camera
cannot be opened the core sits in a discovery retry loop: the port accepts TCP, the handshake
is never answered, and the UI reports "cannot find core". Check the core's stdout for repeated
`>>>>>>driver_name:Aravis>>` before suspecting the UI.

### D5. Never `kill -9` the core
SIGTERM runs `sigroutine`, which tears the WebSocket down and releases the camera. SIGKILL
leaves the camera streaming, and the next process cannot recover it on its own (its
`acquisition_started` is false, so it never issues a stop). Symptom: `USB3Vision write_memory
error (invalid-parameter)` on every `AcquisitionStart`, then no frames at all. The core now
retries once after an unconditional stop, but a wedged control channel (every register read
timing out) still needs the camera physically replugged.

### D6. Driving the UI headlessly: two silent traps (2026-08-07)

Both of these cost an hour and both produce a confident WRONG conclusion —
"the app is broken" instead of "the harness missed". `tools/webctl`.

**A synthetic `.click()` does nothing on the div-based controls.** DefConfUI's
left menu (`重新設定/TAKE`, `儲存/SAVE`, `讀取/LOAD` …) is built from divs, and
`eval("el.click()")` neither opens the dialog nor raises anything. Use a real
Playwright click:

```
node webctl.mjs click "text=重新設定/TAKE"
```

Plain `<button>` elements *do* respond to `.click()`, which is what makes the
failure look intermittent rather than categorical. Keep `.click()` as the
fallback for the opposite case — Playwright refusing with "element is not
visible" because an antd modal is stacked underneath the full-screen DefConf
editor (that is how the 背光 `全部關` button has to be pressed).

**antd keeps a closed modal in the DOM at `display:none`.** So

```js
document.querySelector('.ant-modal-body').innerText   // <-- lies
```

returns text from a dialog nobody can see. A leftover `相機重連中...` read this
way was reported as a blocked UI while the camera was healthy the entire time
(core said `cam_status 0`, `present true`, and answered a second client 5/5).
Filter first:

```js
[...document.querySelectorAll('.ant-modal')].filter(m => m.getBoundingClientRect().height > 0)
```

The same rule applies to every DOM text read in this app.

**Navigation, for reference.** The right-edge icon strip
(aim/camera/cloud/cloud/robot) is the collapsed connection panel — clicking any
of it opens the Build Info drawer, which then covers the right side; close it by
clicking the **mask** (`.ant-drawer-mask`), not the X. DefConfUI is reached from
the left sidebar's pencil (編輯). The v2 sidebar strip only renders once its
peripheral channel is up, so on a fresh page load it is briefly absent.

**Worth knowing while testing the take-image path:** press it more than once. It
used to work exactly once per acquisition start, and the second press failed on
a timer left behind by the first — see `InspectionCore/docs/CORE0_1_CAVEATS.md`
§M, and B3 above for why the `timeout: -1` in that request matters.
`tools/webctl/snap_probe.mjs` presses it N times over the same wire without a
browser, which is the cheapest way to see it.

---

## 系統狀態面板的周邊列點不開 — adapter 掉了 `id`（2026-08-18 修）

症狀:右側系統狀態抽屜裡點「全檢設備 v2」(或全檢設備 / 坡檢設備 / CNC)
**完全沒有反應**,面板叫不出來;core / 相機 / 兩個 DB 那四列卻正常。

原因:`System_Status_Display` 的 `onItemClick(conn_info)` 是靠
**`switch(connInfo.id)`** 決定要開哪個 modal。Redux 來源的那四列免費拿到
`id`——它們的 conn_info **就是** dispatch 出去的 action 本身,action 帶 `id`。
2026-08-16 周邊連線狀態搬去 `perif/PerifAPI.js` 的模組 store 之後,
`script.jsx` 的 `_linkToConn(link)` 是**新造**一個 `{type, brief_info}` 物件,
沒有 `id` → `switch(undefined)` 一個 case 都不中 → 靜默什麼都不做。

四列的顏色點、SUSPECT 標示都照常運作,所以看起來「UI 是活的、只是點了沒事」,
很容易誤判成 antd `visible`/`open` 又踩到,或 modal 被 CSS 蓋住。

修法:`_linkToConn(link, id)` 把 id 帶進結果物件。

**通則:任何把新資料源包成舊 conn_info 形狀的 adapter,必須連身分欄位一起帶。**
點擊路由靠的是 `id`,不是顯示欄位。驗證方式(webctld):

```sh
node webctl.mjs click 'button:has-text("全檢設備v2")'
node webctl.mjs eval "[...document.querySelectorAll('.ant-modal-wrap')].map(e=>({h:e.offsetHeight,t:(e.querySelector('.ant-modal-title')||{}).innerText}))"
# 期望看到 h>0 且 t=="全檢設備 v2 (uInspESP32)"
```

---

## 相機參數面板：相機不回讀、ACK 綠了不代表有套用（2026-08-18）

`component/CameraParamPanel.jsx` 是唯一的相機參數編輯器,主選單的 Camera modal
與相機校正頁共用同一份。三件事決定了它為什麼長這樣:

1. **相機不回讀。** `GS camera_info` 只有身分、mmpp、校正狀態,**沒有 exposure /
   gain**。面板上的值來自 `data/default_camera_setting.json`(核心開機時
   `CameraSettingFromFile` 讀的同一份),不是感測器。兩個瀏覽器各自調不會互相看到。
2. **ACK 是綠的不代表感測器變了。** 驅動的每個 setter 都可能拒絕(基底類別對未實作
   的一律 NAK),`CameraSetup` 把被拒的名字累積到 `camera_info.setup_failed`——
   那是**唯一**看得到「設了但沒進去」的地方。面板會把它標紅。
3. **每推一次就 stop/start acquisition 一次。** `CameraSetup` 在 setters 前後
   Stop/StartAquisition,所以每個按鍵推一次會讓相機起停十幾遍;面板做了 300ms 去抖。
   同樣的理由:**跑產時不要動這個面板。**

**ROI 故意不可編輯。** 存檔的裁切在 `InspectionROI`,只由「檢測畫面框選後儲存」
(`save_insp_roi`)這一個動作寫;其他路徑只能設執行期的 `ROI`,而載入時會把檔案裡的
`ROI` 丟掉。這個分離就是防止「開全幅看一下然後放棄選取」把機台的裁切永久洗掉——
真的發生過。面板只唯讀顯示它。

存檔走 read-modify-write:只覆蓋自己那四個鍵,`InspectionROI`/`framerate`/`ww`
等其他鍵原樣保留。

**antd 按鈕文字會被插空白**:`存檔` 在 DOM 裡是 `存 檔`,Playwright 選 `has-text("存檔")`
會 timeout,用 `has-text("存")`。

---

## 運算核心 modal 的設定:沒有一個是「核心狀態」（2026-08-18）

`component/CoreStatusPanel.jsx`。核心**沒有** runtime 設定檔:每個旋鈕都是
session 生命期的變數,經 ST 設定,而且**一個都不回讀**(GS 只回計數與佇列)。
所以面板顯示的是「這個瀏覽器上次送出的值」,不是核心的值——UI 上直接這樣寫,
不要假裝在顯示狀態。三個會咬人的地方:

1. **CI/FI 會把一部分歸零。** 每次開始檢驗,核心把 `saveInspFailSnap`、
   `saveInspNASnap`、`SKIP_NA_DATA_VIEW` 設回 false(wiringPanel 的 CI/FI 分支)。
   所以「不傳 NA 影像」「儲存 NA 快照」只在當前這段檢驗內有效,面板分開一格標明。
2. **NG 快照設定故意不放這裡。** 它由「設定」頁的 machine_custom_setting 擁有,
   而且 InspectionUI 在每次開始全檢後會重推一次。兩個地方都能寫、其中一個還會
   自動重發,是讓設定變得無法解釋的標準做法。
3. **進入量測設定會無條件把 `IMG_STREAMING_JPEG_QUALITY` 改成 85**(DefConfUI
   componentDidMount)。在核心面板調的品質不會活過一趟量測設定。

`IMG_STREAMING_MAX_FPS` / `IMG_STREAMING_JPEG_QUALITY` 則會撐到核心重開為止。

**log_dump 的檔名是固定的。** SC `{type:"log_dump"}` 走 on-demand 路徑,
`inspd_log_main` 以 `fixed_name=on_demand` 寫出 → **`latest_dump.dump`**,
每按一次蓋掉上一份;`crash_<utc>.dump` 只有真的崩潰才會產生。這也是唯一能把
INFO/DEBUG 從執行中的核心撈出來的方法(磁碟 persist 預設只留 WARN 以上)——
本輪就是用它驗證面板推的 `IMG_STREAMING_JPEG_QUALITY=60` 真的進了核心。

**測試小抄**:antd modal 關閉後 DOM 還在,且 `.ant-modal-wrap` 的 offsetHeight
可能仍 >0;要判斷開沒開請看 **`.ant-modal-content`** 的 offsetHeight。

---

## useEffect 依賴自己會設的 state = 無限迴圈（2026-08-18,實測 420 GS/s）

`component/uInspESP32_UI.jsx` 的分料口查詢:

```js
useEffect(() => {
  const ask = () => sendGS('perif_pairing', { resolve: pv => setOutlets(pv) });
  ask();                                   // ← 每次 effect 都問
  const h = setInterval(() => { if (!outlets) ask(); }, 10000);
  return () => clearInterval(h);
}, [outlets]);                             // ← 而 outlets 是它自己設的
```

回覆裡的 `setOutlets(pv)` 每次都是**新物件參考** → 依賴變了 → effect 重跑 →
`ask()` → 回覆 → …… 唯一的節流是 WebSocket 來回時間。實測(檢驗畫面開著、機器在跑):

- **420 GS/s** 打到核心,佔全部進站封包的 **99.7%**
- 每一發都在核心裡重建 `perif_pairing` JSON、拿鎖,而機器正在檢驗
- 也把核心日誌的封包取樣器灌爆(佔整份 log 的 60-70%)——**這才是它被抓到的原因**

**它只有在機器接線齊全時才會爆**:`if (pv.cat_ok && pv.cat_ng)` 這個守衛讓還沒
拿到分料口接線的機器根本不會呼叫 `setOutlets`,迴圈就不會啟動。所以它在 bench
上長期看不出來。

修法:effect 只在掛載時問一次,重試交給那個 10s timer,並用 **ref** 讓 timer 讀到
當下的答案,而不是把 state 放進依賴。

**通則:effect 的依賴陣列裡不能有這個 effect 自己會 set 的 state**,除非那個
set 有嚴格的收斂條件。回傳新物件的網路回覆永遠不收斂。

**怎麼發現的**:核心日誌普查(`docs/CORE0_1_CAVEATS.md` 的 log census)。瀏覽器
端要複驗很便宜——掛一個 `WebSocket.prototype.send` 的計數器:

```js
window.__t={};const o=WebSocket.prototype.send;
WebSocket.prototype.send=function(d){const u=new Uint8Array(d);
  const tl=String.fromCharCode(u[0],u[1]);__t[tl]=(__t[tl]||0)+1;return o.apply(this,arguments)};
```
