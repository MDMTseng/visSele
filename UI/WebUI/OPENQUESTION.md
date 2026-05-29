# Open questions — WebUI rework (branch `webui/editor-refactor`)

Decisions deferred for the user. Each is a scope/priority call, not a blocker.
Context for all: regression net is `tools/webctl/` — `golden.mjs` (serialized-def
oracle) and `flows.mjs` (behavioral: load/select/edit, verifies re-renders).

---

## Q1. Collapse `edit_info.list` → single source `_obj.shapeList`? — ✅ DONE

Resolved: `SetShape` now replaces the array immutably (drag still commits only on
mouse-up, so no per-frame re-render impact — the per-frame work is the canvas's
imperative `draw()`). The `list` field, its 4 sync points, and all consumers
(3 component selectors + 11 canvas draw reads + reducer/logic internals) now read
`_obj.shapeList`. Verified: golden + flows PASS, canvas + shape-list render, Insp/
Analysis modes mount clean.

(original analysis below for history)

**What:** `edit_info.list` is a mirror of `edit_info._obj.shapeList`, re-synced by hand
at 4 points. The duplication is a footgun (forget to re-sync → divergence).

**Why it's not a simple change:** `InspectionEditorLogic.SetShape()` mutates the array
**in place** — `this.shapeList.push(shape)` (line ~1003) and `this.shapeList[idx] = shape`
(line ~1007). So `_obj.shapeList`'s reference does NOT change on add/edit. Three
components select `edit_info.list` via reference-sensitive `useSelector`/`mapState`
(DefConfUI, AnalysisUI, InspectionUI). Re-pointing them to `_obj.shapeList` would break
their re-render on add/edit, because the array ref is stable across in-place mutation.

**To do it safely** SetShape (and other mutators) must switch to immutable array
replacement (`this.shapeList = [...]`), then components can select `_obj.shapeList`.

**Decision needed:** worth the immutability refactor (+ full flows re-verification), or
leave the cheap `list` mirror as-is? Reward is modest (remove one footgun); risk is real.
Recommendation: **leave as-is** unless we're already in this code for another reason.

---

## Q2. Split the `edit_info` god-object — APPROVED (coarse), planned

**Direction (user):** yes, split it — but **coarse, not finely fragmented**. A handful
of cohesive groups, not many micro-slices.

**Splitting philosophy (applies project-wide):** keep complexly-interrelated / stateful /
side-effectful logic together in one file (size is fine); only extract pieces that are
pure / functional / side-effect-free or intuitively self-contained. So: this is a *state
grouping* (coarse buckets) — NOT a directive to fragment the coupled stateful model
(`InspectionEditorLogic`) or the reducer. Good *code* extraction targets are pure helpers
(geometry, serialization, pure transforms).

**Proposed coarse grouping (3 buckets):**
1. **editor (cold)** — `_obj`, `inherentShapeList`, `edit_tar_info`, `edit_tar_ele_trace`,
   `edit_tar_ele_cand`, `__decorator`.
2. **defMeta (cold)** — `DefFileName/Tag/Hash*`, `loadedDefFile`, `defModelPath`,
   `inspOptionalTag`, `matching_*`, `intrusionSizeLimitRatio`.
3. **runtime/results (HOT)** — `img`, `inspReport`, `reportStatisticState`, `statSetting`,
   `sig360info`, `stage_light_report`, `mouseLocation`. Separating these is the perf win
   (cold consumers stop re-rendering on every image frame / report — the "避免重繪" concern).

**Churn reality:** large. `edit_tar_info` alone has ~40 access sites in DefConfUI, plus
the canvas (`edit_DB_info.*`), AnalysisUI, and the reducer/model. Every field move = many
read-site renames. `flows.mjs` only covers DefConfUI select/edit, not all modes.

**Safe approach (decided):** do NOT hand-rename ~40 sites blind.
1. **Type the consumers first** (progressive TS now in place): convert / `// @ts-check`
   the files touching a group, typed against `EditInfo` in `src/domain.d.ts`.
2. Move one group, update `domain.d.ts`, run `npm run typecheck` → tsc lists every missed
   site (compiler-verified rename).
3. Verify with golden + flows + manual mode checks. One group per commit.
4. Broaden `flows.mjs` (create/delete shape, save, inspection, analysis) before/with the
   HOT group, since those paths aren't covered yet.

Execute as the next focused pass.

---

## Q3. Next rework section

Remaining candidates (all larger, fresh-session sized):
- **Comm layer**: Path A DONE — `BPG_WS` transport relocated to `comm/BPG_WS.js`
  (deps injected; behavior unchanged; script.jsx 2485→2066). **Path B (follow-up):**
  decouple it from `comp.props.ACT_*` into a thin promise/event client so it no longer
  knows about React/redux — the bigger, riskier dependency inversion (37 coupling points).
- **whiteListKey typed schema**: unify `JsonEditBlock` spec + `Shape_Attr_Fill` defaults
  into one per-shape schema. Contained, golden+flows-verifiable, improves shape editor.
- **edit_info god-object split** (Q2).
- **baseComponent.jsx split** (1078 lines = JsonEditBlock editor + BPG_FileBrowser family):
  ATTEMPTED TWICE & REVERTED — **deferred / not worth it without deeper investigation.**
  Attempt 1 (re-export from a new BPG_FileBrowser.jsx) created a `baseComponent`↔`BPG_FileBrowser`
  cycle via `CardFrameWarp` → "type is invalid" / blank editor. Attempt 2 broke the cycle
  (CardFrameWarp moved to a leaf module) — typecheck clean, no errors, but DEFCONF mode STILL
  rendered blank (0 inputs); main screen fine. So the `export {...} from './...'` re-export of
  the file-browser family interacts badly with DefConfUI's `import * as BASE_COM` namespace use
  in a way that's not a simple cycle. Both caught by `flows.mjs` (golden passed). Needs root-cause
  study (maybe avoid namespace re-export: have callers import file-browser from its own module
  and drop the re-export) before retrying. Low priority.

Recommendation order: broaden `flows.mjs` coverage → type the `edit_info` consumers →
Q2 god-object split (group-by-group, tsc-verified) → whiteListKey schema → comm layer.

---

## North-star (long-term direction, not now): per-shape vertical slices

Co-locate everything for a given shape/primitive type (line, arc, circle, search_point,
measure, aux_*) into ONE module per shape — its **setup component** (property sheet),
its **canvas control** (interaction/edit/hit-test), and its **draw** logic, plus its
defaults/schema. Goal: adding/extending a primitive = a single-module change instead of
editing DefConfUI + the canvas + renderUTIL + Shape_Attr_Fill separately. "Group by
feature (shape), not by layer" — fits the cohesion philosophy.

How it relates: the layer-based `renderUTIL`/`CameraCtrl` extraction is a fine
intermediate; later, each shape's draw would move from `renderUTIL` into its shape module
(renderUTIL becoming a thin dispatcher). The **whiteListKey typed schema** above is the
natural FIRST step (a per-shape schema would live in the shape's module). Steer toward
this only when extending shapes / already touching these seams — don't fragment early.

---

## Structural rework backlog (from 4-agent hunt, 2026-05-29)

Prioritized candidates found by auditing the whole tree. (Dead modes
CableWire/GenMatching/Blank already deleted.)

**Duplication (high value):**
- **`CanvasComponent` wrapper cloned across ~5 files** (MAINUI, InspectionUI,
  DefConfUI, RepDisplayUI, BackLightCalibUI) — same lifecycle/resize/down-samp;
  differs only by canvas-ctrl subclass + clamp ceiling (10 vs 15) + mmpp source.
  Extract one configurable base. Biggest cross-file copy-paste. Verify: flows + visual.
- **`uInsp_API` ≈ `GenPerif_API`** (~350 dup lines, script.jsx) → `uInsp_API extends
  GenPerif_API` (as SLID_API already does). Risk: M (pulse_hz/rate behavior).
- **value↔control-limit coupling** dup (`DefConfUI:2342` & `SubDimEditUI:1166`) →
  one pure `recomputeLimits()`. Verify: golden.

**Structural extractions:**
- **6 API classes nested in APPMasterX constructor** (DB_WS/Cam_Stat_Query/uInsp_API/
  GenPerif_API/SLID_API/Platform_API, ~1450 lines) → relocate to comm/ with injected
  deps (the proven BPG_WS pattern). Verify: typecheck + flows.
- **Report-tracking/matching engine** (~420 lines inline in
  `UICtrlReducer.EVENT_Inspection_Report`, ~L113-595) → extract to `reportTracking.js`
  (sibling of spcStats.js). Highest-leverage reducer extraction. Verify: flows (+ may
  need inspection-report fixtures).
- **collapse 4 near-dup binding blocks** in `InspectionEditorLogic.applyEditTarSubstate`.
- **Type the BPG codecs** (`BPG_Protocol.js` raw2header/raw2obj/objbarr2raw) — cleanest TS target.

**Render-stability (HIGH — unblocks per-shape schema + fixes latent bug):**
- `JsonEditBlock` renders renderLib entries as JSX element *types* (`<Render_comp/>`),
  and `whiteListKey` (with `__OBJ__` closures) is rebuilt inline every render →
  React remounts subtrees → editor sub-state loss (SubDimEditUI/Measure_Calc_Editor).
  Also `GenTarEditUI` is a class *method* using hooks rendered as `<this.GenTarEditUI/>`
  (fragile Rules-of-Hooks). Fixing render-lib/whiteListKey identity stability is the
  real prerequisite for the per-shape schema redesign (and likely why the baseComponent
  split kept breaking). Verify: flows + manual sub-editor state-persistence check.

**More dead code (low effort):** script.jsx auto-update (checkUpdateInfo/isNewVersionExist
~104-138) + SystemServicePanel_UI (~141) appear uncalled; DList (DefConfUI 206-256),
completeCtrlMarginInfo (479), Num2Str_padding, raw2obj_rawdata, if(false) blocks. Grep-confirm then delete.

**Note:** the hardware `CameraCtrl` (BPG_Protocol.js:261) vs view `CameraCtrl`
(canvas/CameraCtrl.ts) name collision is a footgun — consider renaming the hardware one.

---

## Round-2 hunt (4 agents, 2026-05-29) — net-new

**BUGS (fix first — small, safe, golden/flows-verifiable):**
1. [HIGH] `InspectionEditorLogic.js:339-342` rootDefInfoLoading doExit path does bare `return;` → caller (RepDisplayUI.js:146) gets undefined → TypeError blanks report on corrupt-def load. Fix: `return edit_info;`.
2. [HIGH] `InspectionEditorLogic.js:409` `log.error(action)` — `action` undefined → ReferenceError aborts feature-load after a camera_calibration entry. Fix: delete/restore.
3. [MED] `script.jsx` DB_WS.send `_insertFailed(x,...)` — `x` undefined → ReferenceError, promise hangs. Fix: pass `data`.
4. [MED] Core BPG `reqWindow` (BPG_WS.js) has no timeout reaper AND is not purged on socket close → hung promises + leak when core drops mid-request (relevant during backend dev). Fix: reaper + reject-all on close (mirror websocket.js trackWindow).
5. [LOW] `ShapeAdjustsWithInspectionResult` (:1073) inverted null-guard (latent).

**Refactor candidates:**
- Canvas subclasses: lift shared `drawImageLayer`/`mouseToWorld`/inspection `colorSet` to base (blit preamble already drifted: INSP `offsetX-0.5*scale` vs others `offsetX/scale-0.5`); dead `draw_INSP if(true) else`, `SetStreamImageSrc` no-op, `rotateVector`; per-mode file split after.
- Dispatch boilerplate → `redux/dispatchHelpers.js`: `ACT_WS_SEND_BPG` ×9 (+useBpgSend), `ACT_WS_GET_OBJ` ×7 (+usePeripheralApi), CanvasComponent mapDispatch ×4 (editTarDispatchProps), EV_UI_ACT wrappers ×4.
- InspectionUI ~300 dead lines: `class DB`(238), `SLID_InspMonitor`(210), `RestrictiveCircleREdit`(1621), `AngledCalibrationHelper`(1337,~225), `SLID_SP_UI`/`hideSLID`, dead `if(false)`(2306,~70); bug: `EV_UI_inspMode()` called(1951,2199) but not in mapDispatch → no-op.
- `down_samp_level_update` handler copy-pasted ×4 (InspectionUI:933/RepDisplay:35/BackLightCalib:28/InstInsp:71), drifted (clamp 10 vs 15, stray *2, calib source) → `computeDownSampParams()`.
- InstInspUI: teardown `_PGINFO_:{keep:true}` likely should be false (stream leak; same in BackLightCalib:252); broken `this.props.ACT_ERROR()` in a function component.
- Extract pure `OK_NG_BOX_COLOR_TEXT`+result-status presentation (InspectionUI) → module.
- `applyEditTarSubstate` 4 near-dup binding blocks → private `_bindCand()`.
- Dead exports: `raw2obj_rawdata`, `Num2Str_padding`; dead `if(false)` UICtrlReducer:564-578.

---

## DefConfUI deep-dive (4-agent, 2026-05-29)

**Mental model:** `APP_DEFCONF_MODE` renders per-SM-substate a canvas + right menu/property-sheet.
THREE manually-synced state stores: (1) xstate SM = mode only; (2) redux edit_info =
_obj.shapeList + transient edit_tar_info/_ele_trace/_ele_cand; (3) canvas imperative
EditShape/EditPoint/CandEditPointInfo/mouseStatus. Create = drag local → mouse-up commit
(Shape_Set+SUCCESS) → SM→SHAPE_EDIT → property sheet from edit_tar_info. Property sheet =
generic JsonEditBlock fed a UNION whiteListKey (renders only fields present on shape).
Ref-binding = 2-dispatch handshake (_ele_trace UI + _ele_cand canvas → applyEditTarSubstate).

**Hotspots (file:line):** render() substate switch (~2499-3013, ~500 lines); GenTarEditUI
(~2116, class method w/ hooks, holds per-shape whiteListKey 2174-2264 + value↔limit jsonChange
2265-2351); DEFCONF_MODE_NEUTRAL_UI (~1421-2080, 650 lines); InspMarginEditor (490-818);
Measure_Calc_Editor (820-996).

**Root causes:** split-brain (EditShape≈edit_tar_info, one Shape_Set serves add/modify/delete →
canvas re-derives intent via mouse-edge flags ifOnMouseLeftClickEdge/mouseTriggeredUpdate);
whiteListKey rebuilt inline each render → renderer remount/state-loss; schema+defaults+visibility
in 3 places; ctrlLogic_DEFCONF is a 2nd state machine re-switching on substate; two modal systems.

**Prioritized plan:**
1. DEAD CODE (safe, big): `uiType=="deco"` branch (2395-2489) + SubDimEditUI (1026-1167) +
   uiType state — UNREACHABLE (only trigger commented at 2151-2156). aux_line path (ACT_Aux_Line_Add_Mode
   1436, button commented 1553-1557, case 2699-2733). Commented blocks 1366-1381/1909-1916/2868-2875/2535-2538.
   ⚠️ confirm deco/extra_info abandoned vs planned before deleting.
2. PER-SHAPE SCHEMA + render-stability (keystone/north-star step 1): schema registry per type →
   generates whiteListKey via useMemo (fixes remount) + derives Shape_Attr_Fill defaults + one
   applyLimitCoupling() (dup'd 3x: 2286-2313, SubDimEditUI 1117, MEASURE_CREATE 2559); GenTarEditUI
   → real top-level function component. Generic JsonEditBlock engine stays untouched.
3. EXPLICIT INTENT: split Shape_Set → Shape_Add/Modify/Delete (kills mouse-edge heuristics); make
   canvas EditShape a projection of edit_tar_info (drop tmp_EditShape_id); lift AvailableShapeFilter
   to InspectionEditorLogic. Collapse 2 modal systems.
4. PURE EXTRACTIONS → DefConf/ folder: propertyRenderers (Acc widgets + renderMethods), MeasureCalcEditor,
   MarginEditor; dedup II/INST_CHECK→ShapeAdjustsWithInspectionResult flow (toolbar 1866-1938 vs
   edit-CHECK 2827-2885). ref whiteListKey 3-vs-10-slot drift (2248 vs 2543).

Don't fragment: render() switch, NEUTRAL_UI, ctrlLogic_DEFCONF, GenTarEditUI (coupled stateful).

---

## Round-3 hunt (perf / coverage / def-format / coupling, 2026-05-30)

**Coupling (do first — small/low-risk, prevent cycle landmines):**
- [HIGH,S] BPG_Protocol ↔ UIAct CIRCULAR import (UIAct imports INSPECTION_STATUS from BPG_Protocol; BPG_Protocol imports * as UIAct). Move INSPECTION_STATUS enum to a leaf; both import the leaf. Verify typecheck+golden.
- [HIGH,S] Two same-named CameraCtrl classes (BPG_Protocol.js:261 camera-settings vs canvas/CameraCtrl.ts viewport) — rename BPG one → CameraTransferCtrl (2 import sites: DefConfUI:15, InspectionUI:21).
- [MED] EverCheckCanvasComponent `export default {7 classes}` object hides coupling + defeats tree-shaking; the :28 renderConst re-export is the baseComponent-break pattern → named exports, consumers import what they use + import renderConst directly.
- [MED, highest-leverage seam] MISC_Util re-exports websocket/expr/structures (:92,156,157) → highest fan-in hub mixing pure+stateful. Stop re-export; ~4 websocket consumers import UTIL/websocket directly; MISC_Util becomes pure-helpers leaf.
- [MED] baseComponent/BPG_FileBrowser CYCLE-FREE PLAN: extract file-browser cluster (388-486+) to component/BPG_FileBrowser.jsx as a PURE LEAF (imports only React+antd+GetObjElement, NEVER baseComponent/rdxComponent), baseComponent does NOT re-export it, update 4 consumers (DefConfUI/MAINUI/rdxComponent/RepDisplayUI) to import direct. No barrel = no cycle. (supersedes the earlier failed attempts)

**Perf (P1+P2 = most of the win, no edit_info split needed):**
- [S,riskM] UICtrlReducer.newStateUpdate returns fresh {...ret_state} every action → all selectors re-run. Return `state` for ignored branches (verify canvas still redraws via its img-diff).
- [S] MAINUI CanvasComponent: add shouldComponentUpdate (c_state/img/inherentShapeList) + remove 2 console.logs/frame (171,174). InspectionUI CanvasComponent: add SCU (draw already gated).
- [S] remove hot-path console.log UICtrlReducer:838.

**Def-format / expandability:**
- [M] No def `ver` migration path (ver written, never read) — add ver-keyed normalize-on-load. Biggest expandability gap.
- [L] Shape_Attr_Fill missing circle/aux_line/aux_point branch; adding a primitive touches ~6 places → per-shape schema keystone payoff.
- [M] sha1 lineage reorder/decorator-fragile (strip rule in 2 files) → one canonicalForHash(featureSet).
- [S/M] Type BPG codec: BpgHeader + discriminated BpgPacket union (map_BPG_Packet2Act default silently drops unknown packets); raw2Obj_IM 15-byte layout; defFileGeneration→HyDef; Shape_Attr_Fill(Shape):Shape.

**Verification-net coverage gaps (add flows):**
- [S,high] shape DELETE (Shape_Set{id,null}); [L,highest] ref-binding (search_point/aux/measure via applyEditTarSubstate); [M] measure subtype change (ref arity); [S] back_value_setup toggle; [M] SV true round-trip (SV→re-LD→equal); [L] INSP result + CHECK overlay (assert store/DOM not pixels); multi-shape edits; down_samp level 2/3; error/disconnect (assert __rdyErr). Skip undo (no undo action exists).

---

## Round-4 hunt (renderUTIL / MAINUI+peripheral / state-machine / UX, 2026-05-30)

**renderUTIL (per-shape draw / north-star):** line/arc/search_point/aux draw cleanly extractable → shapes/<type>.draw; measure branch (~510 lines, renderUTIL.js:698-1210) is the prize — label block copy-pasted 4x → extract `drawMeasureLabel()` first, then split subtypes; `drawInspectionShapeList`(1220-1313) duplicates `drawShapeList`'s switch. Dead: drawLineArrow stub(275), empty aux_line in drawInherentShapeList(237), drawSignature orphan(468), `if(true||...)`(549), commented blocks. BUGS: `.toFixed` binds to `.mult` not product (452) + concat into digit arg (1183).

**MAINUI + peripheral:** uInsp_API extends GenPerif_API → ~140 byte-identical dup lines removed (override triggerPing + 4 setup hooks; SLID_API proves base). Relocate 6 API classes out of APPMasterX ctor → comm/ (id,comp,StoreX); DB_WS/Platform_API only need DISPATCH (cleanest). Dead: commented RootSelect block(1183-1207), empty DeConf/Inspection cases(1272-1277) + unused PrecisionValidation statesTable key, commented ELECTRON_IPC(script.jsx:102-124); GenPerif.triggerPing computes res_count then discards. MAINUI back-button menu item rebuilt 4x → backMenuItem()/makeSider() helper.

**State-machine:** orphan dead — AUX_LINE_CREATE (state+event+3 consumer branches), INSTINSP_MODE (whole mode, no dispatcher), orphan events WS_channel/Camera_Info_Update/Insp_Mode_Update. [HIGH] substate→allowed-shape-types duplicated in AvailableShapeFilter (canvas) + applyEditTarSubstate (model) → one pure substateShapeSpec(substate) table in the model.

**UX / i18n / error-handling:**
- [P2, S, HIGH-integrity] sha1 mismatch on load only console.error'd then loads anyway (InspectionEditorLogic:325) → surface blocking warning modal (return-fix already makes this safe).
- [P1, M] DefConf Save is fire-and-forget, no success/fail feedback (DefConfUI:1561-1588); pattern exists in InstInspUI:2281 (check SS.data.ACK → 存檔成功/失敗 toast) → port it.
- [P3, S] missing i18n keys render as raw identifiers (dictLookUp MISC_Util:96); dict only 159 lines; many keys unlisted (USL_b/LSL_b/quadrant...).
- [P4, M] hardcoded Chinese strings bypass DICT (script.jsx:1841/1873, MAINUI:1314/1569, DefConfUI:2479).
- [P5, S] file overwrite signaled only by red OK button (no "will overwrite X" text); isASCII silently drops CJK filename keystrokes (baseComponent:435/457).
- Fine: shape-delete has confirm; disconnect splash+watchdog ok; file-browser has 5s timeout. Keyboard support absent app-wide (intentional touchscreen).

---

## Round-5 hunt (robustness / model / build-deps / react-readiness, 2026-05-30)

**Robustness BUGS (Wave 1):**
- [S] BPG_WS.onmessage: `case "HR"` falls through into `case "SS"` (no break) → TypeError on malformed handshake stalls dispatch.
- [S/M] BPG decode no length/bounds checks; raw2Obj_IM `if(true||...)` DISABLES the image-size guard → corrupt IM frame → RangeError / multi-GB OOM. Re-enable guard + validate offset+9+length<=byteLength + try/catch onmessage.
- [S] parseFloat no NaN/Infinity guard at DefConfUI:2202,2298 → NaN/Inf → null in serialized def (corrupts limits). Add Number.isFinite guard.
- [S] urlConcat slice typo (BPG_WS.js:13 `xbase.length` should be `xadd.length`).
- sha1 now correctly enforced (my return-fix); only gap: def with NO featureSet_sha1 skips check (treat as untrusted).

**Model (InspectionEditorLogic):** [M] parse* family (auxPointParse/searchPointParse/shapeMiddlePointParse/shapeVectorParse) dup ref-lookups → one resolveRef(); [M] extract pure pointForwardTrans/pointInvTrans from ShapeAdjustsWithInspectionResult; [S/M] group parse* into pure ShapeGeometry module + TS types. Dead: searchPointParse ignores param (near-noop), setsig360infoCenter no caller, commented blocks, UpdateInherentShapeList double-called in rootDefInfoLoading.

**Build/deps:**
- [RESOLVED — false alarm] `__DEV_MODE__`: both refs guarded (info.js:4 try/catch, script.jsx:72 `typeof` check); webpack-not-defining-it is harmless. No prod crash. See Round 6.
- [S] Remove UNUSED deps: @antv/g2, @antv/data-set, ajv, text-encoding, localStorage(pkg), cytoscape(+cose-bilkent, only used by dead xstate_visual.js).
- [S] git hygiene: add root .gitignore; delete orphan root .ttf, stray WebUI/ + 未命名檔案夾/ dirs, stale jsconfig.json (conflicts with tsconfig); dead service-worker/PWA_manifest.
- [S] extract shared less modifyVars (dup in vite+webpack configs); wire `typecheck` into regress.
- [M] two transition-group libs (react-transition-group@1 + react-addons-css-transition-group) → consolidate.

**React-18/19 readiness (deferred upgrade, ~18 edits/6 files):** blockers = string refs `this.refs.canvas` (11), componentWillUpdate (5, all drive canvas updateCanvas), componentWillMount (1), ReactDOM.render→createRoot (1). Sequence: refs → componentWill* → bump+createRoot last. Smells (fix-now-ok): .bind(this) in render, key={idx}, MAINUI SCU reads string ref.

---

## Round 6 (DB/persistence · CSS/layout · report-tracking extraction design · WebUI2 patterns)

**RESOLVED — `__DEV_MODE__` is NOT a prod crash (round-5 false alarm):** both refs are guarded — `info.js:4` is inside try/catch, `script.jsx:72` uses `typeof __DEV_MODE__ !== "undefined"`. webpack prod DefinePlugin (webpack.config.js:18) defines only NODE_ENV, but that's fine because neither ref is bare. No action needed.

**DB / persistence (scattered + silently lossy):**
- [M, HIGH — data integrity] Inspection-report DB writes fail silently: empty `.catch` at InspectionUI.js:173-175; DB_WS retry deliberately commented out (script.jsx:428-441) and on send-failure it resolve()s + deQs anyway → failed inserts DROPPED, not requeued, zero operator signal. For a factory traceability DB this is the biggest gap. Verify: kill DB ws mid-inspection → records should requeue + visible failed-count.
- [S, MED] trackWindow no age-based purge: send_obj stamps `time:Date.now()` (websocket.js:395,879) but nothing reads it; DB_WS 5s timeout rejects but does NOT delete trackWindow[tKey] → late/never ACK leaks forever until reconnect. Sweep by `time` on the existing interval.
- [M, MED] Dup between the two DB_WS writers (Insp_DB_W_ID/DefFile_DB_W_ID, script.jsx:542-543, identical) and two near-identical send_obj impls (websocket.js:378 reqTrack vs :869 aliveTracking). Folds into the tracked DB_WS→comm/ relocation.
- [S/M, MED] Query path orphaned/inconsistent: DB_Query.js uses jsonp/HTTP while writes use WS; defFileQuery/inspectionQuery (lines 4,26) have NO timeout (hang on dead endpoint). With AnalysisUI deleted, check for live callers — may be dead code.

**CSS / layout (coherent hand-rolled utility framework; mostly low-risk cleanup):**
- [S] Delete dead: empty style/MaT.css (unimported); unused classes veleXY90/vboxImg/cardblock/widthF800.../HX3/HX5-7/height115/deepblue.../radarScanPanel/pokemonSpriteCon. The widthF/N media queries are the ONLY live breakpoints and they're unused → no responsive behavior is actually live.
- [S] Bug: `palatte-gray-8` (DefConfUI.js:1612) — only `palatte-grey-*` exists (sp_style.css:277) → element silently gets no background. Fix class or add gray alias.
- [M, HIGH leverage "easier UI edits"] Hardcoded fixed-px touch heights: `.s` height:50px (basis.css:100) + HX1=50px…HX4=200px ladder (basis.css:202-217) used ~30×. Extract `--touch-row` token → retune all touch targets at once. (The % widthN/heightN grid is fine — leave it.)
- [M] `cm`-unit font sizing (.s 0.4cm basis.css:103; sFontSize/mFontSize/lFontSize 0.3-0.7cm sp_style.css:526-534) renders inconsistently across touchscreen DPI vs dev + doesn't scale w/ zoom → convert to rem/px tokens.
- [S/M] Modal sizing (.modal-sizing/size90/size95, sp_style.css:121-175) overrides antd with !important — fragile for antd upgrade; document the flex contract (header flex:0 2, body 1 2, footer 2 2).
- Non-issues (honest): z-index (one rule, no war); inline styles (110, mostly one-off geometry); .blockS/.sp_Style scoping convention is good.

**Report-tracking engine extraction (concrete plan — highest-leverage reducer carve):**
- TARGET: `EVENT_Inspection_Report` in UICtrlReducer.js:113-604 (~490 lines); tracking core 163-580. Precedent: statReducer→spcStats.js, applyMeasureLimitCoupling→measure.js (mutate-and-return seam).
- PURE (extract): matching+blending+grading+windowing — timeout/flush (:192-227), MarginInfoExtraction (:244-251), resultGrading (:252-285), mask-cull/closeRep-match/valueAveIn blends/insert-prune (:306-548). All deps injectable.
- STAYS (coupled): action switch, immutable Object.assign churn (:156,165,563), sig360/camera_calibration/stage_light branches that call `_obj.Set*` (:160,583,587), context pulled off newState.edit_info.
- SEAM (one coarse fn, not shattered): new src/redux/reducer/reportTracking.js → `trackInspectionReport(reportStatisticState, inspReport, ctx) -> reportStatisticState`; ctx bundles statSetting/mmpcampix/currentTime_ms/margins/getDetailStatus/statReducer etc. Reducer case shrinks to: assemble ctx, call, `edit_info.reportStatisticState={...result}`.
- HARNESS: add `inspectReport` flow to flows.mjs — dispatch N canned Inspection_Report actions (fixed date/seeded values), extend SNAP (flows.mjs:114) to capture trackingWindow (id/repeatTime/cx/cy/area/status/judge), historyReport.length, reportCount, emptyReportCount, statisticValue.measureList (id/count/mean/sigma/CPK), float-rounded for byte-stable JSON. Baseline before → byte-identical after.

**WebUI2 (TS sibling) — proven patterns (the north-star, already shipped there):**
- [VERY HIGH, directional] Per-shape vertical slice EXISTS: SingleTargetVIEWUI_<Type>.tsx (DimMeasure/ArcFitting/Orientation_SBM/CameraCalib), each co-locating setup UI + canvas ctrl + drawHook (DimMeasure.tsx:310,:612). String-keyed registry COMPONENT_MAP (InspTarView.tsx:5356) with union type DERIVED from the map (keyof typeof, :5372) + type guard + useMemo MUX. This is the literal opposite of DefConfUI.js's generic whiteListKey switch — and it's the target architecture, validated in prod. whiteListKey schema work = step 1 toward it.
- [back-portable] `type_DrawHook=(ctrl_or_draw,g,canvas_obj)=>void` (CanvasComponent.tsx:121); HookCanvasComponent takes a `dhook` prop (:503); shape supplies own draw via useCallback. WebUI already has the same canvas base → adding one drawHook prop is the lever for per-shape rendering ownership.
- [directional, "keep Redux" caveat] Promise-based BPG/WS client: BPG_WS in EXT_API.ts:16 keys reqWindow:Map<pgID,{...promiseCBs}>, send_P() resolves on SS terminator (:143,:197), send_cbs_attach for streaming (:223). Cleaner than WebUI's callback/redux-dispatch split — borrow the Promise client BESIDE Redux, don't replace.
- [incremental] Shared prop contract CompParam_InspTarUI (SingleTargetVIEWUI_UTIL.tsx:19-46) every shape view implements; AppTypes.ts types stream payloads. WebUI's domain.d.ts is 165 lines, def mostly `any` → type incrementally under allowJs.
- Honest caveats: WebUI2 def/report still `any`; noisy console.log; XCMD.ts eval (:530); DimMeasure 5600L / InspTarView 5400L (files large — the BOUNDARIES are the clean parts). State still Redux (no lighter-store migration signal).
