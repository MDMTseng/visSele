# WebUI (1st-gen) — Architecture & Orientation

Operator UI for the **visSele / Core0_1** machine-vision inspection app. React + Redux,
talks to the C++ core over a custom binary protocol (BPG) on a WebSocket. This doc maps
the whole app so you can navigate and rework it safely.

> Line numbers below are approximate (the code drifts) — trust the **file paths** and the
> **structure**, and grep for the named symbols. Verify before relying on a specific line.

---

## 0. TL;DR mental model

```
 React components ──dispatch──▶ Redux actions ──▶ MW_API middleware ──▶ BPG_WS.send()
        ▲                                                                    │
        │ mapStateToProps / useSelector                                      │  ws://localhost:4090
        │                                                                    ▼
   Redux store ◀──dispatch── map_BPG_Packet2Act() ◀── BPG_Protocol.raw2obj ◀── C++ CORE
   (UIData / InspData / ConnInfo)
```

- The **core** (C++ `visSele`, separate repo `InspectionCore`) does all vision work. The
  UI only edits the **definition** (`.hydef`), sends it, triggers inspection, and renders
  **results** + the camera image.
- Everything core↔UI goes through **BPG packets** over WebSocket. There is no REST API.
- A def is a list of **shapes/primitives** (line, arc, circle, search_point, measure …).
  Inspection returns a **report** (detectedLines, detectedCircles, searchPoints,
  judgeReports) that is matched back to shapes **by `id`**.

---

## 1. Entry & build

- Entry: `src/script.jsx` (webpack `entry`, see `webpack.config.js`). Mounts the React
  tree into `#container` (`index.html`) wrapped in the Redux `<Provider>`.
- Build scripts (`package.json`):
  - `npm run dev` — webpack-dev-server on **:8080**, hot reload (use this for UI work).
  - `npm run build_dev` — dev build (source maps, `NODE_ENV=dev`).
  - `npm run build-w` — watch build.
  - `npm run build` — production (minified/obfuscated, `NODE_ENV=production`).
  - `npm run start-electron` — Electron shell (loads :8080 or file://). `electron/fs/path`
    are webpack `externals`.
- Output: `dist/`.

---

## 2. State management (Redux)

Store: `src/redux/redux.js` — `combineReducers` of three slices, plus middleware
(redux-thunk, `MW_API` (the BPG sender), an xstate state-machine MW, an action throttle).

| slice | reducer file | holds |
|---|---|---|
| `UIData` | `reducer/UICtrlReducer.js` | `edit_info` (the def + shapes being edited), state-machine state (`c_state`/`p_state`/`sm`), `System_Setting`, camera settings, `DICT` (i18n), per-measure statistics |
| `InspData` | `reducer/InspDataReducer.js` | latest inspection report, `sig360info`, image frames |
| `ConnInfo` | `reducer/ConnectionInfoReducer.js` | per-endpoint WS connection status: `CORE_ID`, `CAM1_ID`, def/result DB writers, peripheral APIs (uInsp/SLID/CNC) |

Action creators live in `src/redux/action*` (e.g. `UIAct.js`, `DefConfAct.js`). Key ones:
- `EV_WS_SEND_BPG(connId, type, prop, jsonData, uintArr, promiseCBs)` — send a BPG packet.
- `EV_WS_Image_Update`, `EV_WS_Inspection_Report`, `EV_WS_Define_File_Update`,
  `EV_WS_SIG360_Report_Update` — incoming-packet → store.
- State-machine events `UI_SM_EVENT.{Edit_Mode, Insp_Mode, Analysis_Mode}`.

Components connect via `connect(mapStateToProps, mapDispatchToProps)` (class components) or
`useSelector`/`useDispatch` (function components).

**The xstate machine** (in `redux.js`) gates UI modes: `DEFCONF_MODE` ↔ `INSP_MODE` ↔
`ANALYSIS_MODE`, and within edit: NEUTRAL → {Line_Create, Arc_Create, Search_Point_Create,
…} → SHAPE_EDIT → NEUTRAL. Transitions fire `DefConfAct.EVENT.SUCCESS/FAIL`.

---

## 3. Communication layer (BPG over WebSocket)

Connection setup + reconnection in `src/script.jsx` (the `BPG_WS` class). Core URL is
`ws://localhost:4090`. Reconnect loop on `onclose` (~10s); camera/peripheral health checks
poll on their own timers. **The UI talks to a *running* core process** — if you rebuild the
core, that process must be **restarted** or the UI keeps hitting stale code.

Protocol: `src/UTIL/BPG_Protocol.js`.
- **Header = 9 bytes**: `[0..1]` 2-char type tag, `[2]` prop byte, `[3..4]` pgID (packet
  group id, u16), `[5..8]` length (u32 BE). Payload = UTF-8 JSON (+ optional binary, e.g.
  image pixels for `IM`).
- **Packet types** (the 2-char tags; see the parse switch in `BPG_Protocol.js`):

  | tag | direction | carries |
  |---|---|---|
  | `HR` | in | handshake / version / ready |
  | `IM` | in | camera image frame (width, height, cam id, scale, offset, pixels) |
  | `IR` | in | inspection report (detectedLines/Circles, searchPoints, judgeReports) |
  | `RP` | in | report (alt form) |
  | `DF` | in/out | define file (the feature definition) |
  | `SG` | in | sig360 signature features |
  | `FL`/`LD` | in/out | file load (e.g. `data/default_camera_param.json`, machine settings) |
  | `SV` | out | save file to core |
  | `SS` | in/out | session start/end markers (wrap multi-packet responses) |
  | `GS` | out | get status (camera_info, queue, binary path) |
  | `RC` | out | reconnect camera |
  | `PD` | in/out | peripheral device (uInsp / SLID / CNC), routed by a CONN_ID |

- **Send path**: component `dispatch(EV_WS_SEND_BPG(...))` → `MW_API` middleware →
  `BPG_WS.send()` → `BPG_Protocol.objbarr2raw()` → `ws.send()`.
- **Receive path**: `BPG_WS.onmessage` → `BPG_Protocol.raw2obj` / `raw2Obj_IM` → packets
  accumulate per **pgID** in a request window; on `SS` end the batch is dispatched via
  `map_BPG_Packet2Act()` → Redux actions.
- **Request/response correlation** is by **pgID** + the `SS` start/end envelope. An optional
  `promiseCBs.resolve()` fires when a request's response group completes — that's how you do
  an async "request X and await it" (relevant for the planned on-demand caliper-strip fetch).

---

## 4. Data model — the definition (`.hydef`)

Serialized by `InspectionEditorLogic.js → GenerateFeature_sig360_circle_line()`:

```js
{
  type: "sig360_circle_line", ver, unit: "px",
  mmpp,                 // mm per pixel (calibration scale)
  cam_param,            // camera calibration
  features: shapeList,            // user-editable shapes (the whole objects, passed through)
  inherentfeatures: inherentShapeList  // computed/auxiliary (regenerated, not authored)
}
```

**Key fact:** `features` is `shapeList` serialized *wholesale* — whatever properties sit on
a shape object reach the core verbatim. So adding a new per-primitive setting is often just:
set the field on the shape (+ render a control for it); no serializer change needed.

Shape types (`SHAPE_TYPE` enum in `UIAct.js`) and notable fields:
- `line` — `pt1`,`pt2`; `vertex_touch_searching` (bool); **`locating`** (`"contour"`|`"caliper"`).
- `arc` — `pt1`,`pt2`,`pt3` (3 pts on the arc); `locating`.
- `circle` — derived (center+radius).
- `search_point` — origin + a `ref` to a line for direction; `search_far`, `locating_anchor`,
  `line_thickness_value`. **Does NOT use the caliper** (own core path).
- `measure` — references other shapes; `value_A/B/X/Y` (expression), `USL/LSL/UCL/LCL`
  limits, `quality_essential`, `orientation_essential`, `NGasNA`, `NAasNG`.
- `aux_line` / `aux_point` — auxiliary/computed geometry.

Common fields: `id` (int, the join key to results), `name`, `type`, `ref[]` (`{id, element|keyTrace}`),
`ref_baseLine`.

Storage: `shapeList` (authored, persisted) vs `inherentShapeList` (computed, regenerated).
`FindShapeObject/FindShapeIdx` search both.

**Caliper line/arc def schema** (set when `locating:"caliper"`; all optional, sane defaults):
```js
"caliper": { "count":N, "width":px, "length":px, "step":px },  // length/step <=0 => initMatchingMargin / 1
"edge":    { "method":"strongest|first|last|middle|nth", "polarity":"any|rising|falling", "nth":0, "min_strength":0 }
```
Default edge = `strongest` / `falling` (white→dark silhouette). Core treats `locating` !=
`"caliper"` as legacy contour.

---

## 5. Data model — the report

A report (from `IR`/`RP`) looks like:
```js
{
  detectedLines:   [{id, status, cx, cy, vx, vy, s, matching_pts, confidence?, pt1, pt2}],
  detectedCircles: [{id, status, x, y, r, s, matching_pts, confidence?}],
  searchPoints:    [{id, status, x, y}],
  judgeReports:    [{id, status, value, detailStatus, name}],   // the "measure" results
  rotate, cx, cy, isFlipped, mmpp
}
```
- `status`: `INSPECTION_STATUS` enum (SUCCESS=0, FAILURE=-1, NA=-128, …).
- Geometry is in **absolute image px** (multiply by `mmpp` for mm). The object pose
  (`rotate, cx, cy, isFlipped`) maps results back into the def's template frame.
- `confidence` (lines/circles) is the new caliper quality signal (mean inlier edge
  confidence); present only on caliper-path results. Pairs with `s` (rms) + `matching_pts`.

Matching results → def shapes: `InspectionEditorLogic.js → FindInspShapeObject(id, report)`
(searches detectedLines/Circles/searchPoints/judgeReports by `id`) and
`ShapeAdjustsWithInspectionResult` (applies the pose transform).

---

## 6. Key components

| file | responsibility |
|---|---|
| `src/MAINUI.js` | app shell; routes between DEFCONF / INSP / ANALYSIS modes; system-status panel |
| `src/DefConfUI.js` | definition editor: shape list + the property sheet (via `JsonEditBlock` + a `whiteListKey` spec). Serializes the def on save. Hosts the **CHECK** golden-sample button (`key="CHECK"`). |
| `src/component/baseComponent.jsx` | `JsonEditBlock` — the generic object editor that renders only fields listed in `whiteListKey`, mapping each to a control ("input", "input-number", "switch", a `Dropdown_List`, or a custom render fn). This is how most property UIs are built. |
| `src/EverCheckCanvasComponent.js` | the canvas: draws the camera image + shape overlays + result status/measure text. Owns the px↔screen transform and mouse interaction for shape create/edit. |
| `src/RepDisplayUI.js` | result display panel / screenshot. |
| `src/InspectionUI.js` | inspection execution + live result overlay; golden-sample flow. |
| `src/UTIL/InspectionEditorLogic.js` | the def "model": shape CRUD, ref trees, serialization, result→shape matching, statistics. |
| `src/UTIL/BPG_Protocol.js` | wire protocol (pack/unpack, type dispatch). |

---

## 7. Patterns & gotchas (read before reworking)

- **`whiteListKey`-driven editor.** Property sheets are declarative: a dict
  `{fieldName: control}` passed to `JsonEditBlock`. A field only renders if it's in the dict
  *and* present on the object. To add a control, add a `whiteListKey` entry and ensure the
  field exists (defaults set in `InspectionEditorLogic.js → Shape_Attr_Fill`). Example just
  added: the line/arc `locating` dropdown `["contour","caliper"]`.
- **Def passed through wholesale** (§4) — minimal plumbing to add per-primitive settings.
- **`featureSet_sha1*` hash fields.** A def carries `featureSet_sha1`, `_sha1_pre`,
  `_sha1_root` integrity hashes. Hand-edited defs fail to load until these are removed/recomputed.
- **px vs mm.** Def + canvas are in **px**; `mmpp` converts to mm. Results are absolute image
  px and need the pose transform to land in def space.
- **Stale-core gotcha.** The UI talks to a long-running core process on :4090. After
  rebuilding the core, **restart that process** or the UI runs old core code (this exact
  trap caused "caliper looks wrong in the UI but right in the harness").
- **Action throttling.** A throttle middleware batches rapid dispatches (mouse drag, image
  stream) — don't assume every dispatch hits the reducer synchronously.
- **Multi-packet responses** are framed by `SS` start/end and keyed by `pgID`; use
  `promiseCBs` for request/await semantics.

---

## 8. Rework starting points (suggestions)

- The generic `whiteListKey` + `JsonEditBlock` editor is the highest-leverage area: it's
  powerful but implicit. Consider a typed per-shape schema (fields, control, default,
  validation) that drives both `Shape_Attr_Fill` defaults and the editor, instead of the
  two being maintained separately.
- Communication is spread across `script.jsx` (huge), `BPG_Protocol.js`, and `MW_API`.
  A thin typed client (sendCommand→Promise, typed packet codecs) would isolate the protocol
  from React.
- State slices (`UIData`) mix UI mode, the def model, and statistics; splitting the def
  model out of the reducer (it largely lives in `InspectionEditorLogic`) would clarify
  ownership.

(Engine/def-schema details for the caliper rework live in the core repo:
`InspectionCore/docs/caliper_primitive_locating_design.md`,
`InspectionCore/docs/search_point_rework.md`.)
