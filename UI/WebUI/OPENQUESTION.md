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
