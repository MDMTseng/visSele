# WebUI (1st gen) — Team Handoff

> Status: factory-deployed, actively maintained on `webui/editor-refactor`. This
> doc is the single entry point for a new dev. Read top-to-bottom once, then
> use it as a reference.

---

## 1. What this is (and what it isn't)

This is the **1st-generation** inspection operator WebUI — the React 16 app that
runs on the factory touchscreen next to `visSele` (Core0_1). It speaks a binary
protocol (BPG) over WebSocket to the core on `:4090`.

**It is**:
- The shipping factory UI. Real lines run on it.
- React 16 + Redux + xstate, ~2-year-old codebase, deep class structure.
- The target of an ongoing rework — `webui/editor-refactor` branch — that
  modernizes seams without breaking factory deployment.

**It is NOT**:
- WebUI2 (`UI/WebUI2/`) — that's a TypeScript node-graph-based 2nd-gen sibling.
  It shares no code with this app. Some patterns are borrowed FROM WebUI2 (e.g.
  the per-shape vertical-slice idea); the engine model and comm path are NOT
  portable. Do not attempt a wholesale port.

**Core pair**: this WebUI talks to **Core0_1 (legacy visSele)**, not CoreHub
(the 2nd-gen sibling). When Core0_1 ships a backward-incompatible change, it is
deliberate. Restore behavior in the **consumer (this WebUI)**, not in core —
see [feedback memory `fix-consumer-not-provider`].

---

## 2. Quick start (5 min)

Prereqs: macOS arm64 dev box (or Linux); Node 20+; cmake 3.x; vcpkg-built OpenCV
already wired in InspectionCore.

```bash
# 1. Build core (one-time per change)
cd InspectionCore
cmake --build build/mac-arm64 --target visSele -j8

# 2. Start core + log drainer (real one; not the mock)
cd InspectionCore/Core0_1 && \
  INSP_LOG_DAEMON=1 INSP_LOG_WS_PORT=4091 INSP_LOG_LEVEL=DEBUG \
  INSP_LOG_DIR=/tmp/insp-real/ \
  DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele \
  > /tmp/visSele.log 2>&1 &

# 3. Start the Vite dev server
cd UI/WebUI && npm run dev   # serves http://127.0.0.1:8081/

# 4. (Optional) Start the QA harness driver
cd UI/WebUI && node tools/webctl/webctld.mjs http://127.0.0.1:8081/ &
```

Check `lsof -nP -iTCP:4090,4091,8081 -sTCP:LISTEN`. Three lines = all good.

Open `http://127.0.0.1:8081/` in your own Chrome (NOT the webctld Playwright
one — that's for tests; see §7).

---

## 3. Where things live

```
UI/WebUI/
├── src/
│   ├── script.jsx              ★ boot, store, ReactDOM.render, top-level connection logic
│   ├── MAINUI.js               main screen (mode select, file pickers, status bar)
│   ├── DefConfUI.js            def-editor screen (the heart of operator workflow)
│   ├── InspectionUI.js         live-inspection screen (report streaming, stats)
│   ├── BackLightCalibUI.js     stage-light calibration flow
│   ├── InstInspUI.js           instant single-shot inspection
│   ├── RepDisplayUI.js         historical report viewer
│   ├── EverCheckCanvasComponent.js  THE canvas (drawing, pan/zoom, hit-test, draw_INSP)
│   ├── domain.d.ts             progressive-TS domain types (Shape, EditInfo, ...)
│   ├── info.js                 build info + default System_Setting
│   │
│   ├── comm/
│   │   ├── BPG_WS.js           BPG-over-WebSocket transport to :4090
│   │   └── CoreLogClient.js    inspd_log.v1 WS client (:4091)
│   │
│   ├── canvas/
│   │   ├── renderUTIL.js       shape draw helpers (gradually thinning out)
│   │   ├── renderConst.js      colors, font sizes, MEASURE_RESULT_VISUAL_INFO
│   │   └── CameraCtrl.js       camera/zoom state
│   │
│   ├── shapes/                 ★ per-shape vertical slice (line/arc/measure/...)
│   │   ├── index.js            registry (getShapeModule)
│   │   ├── line.js arc.js search_point.js aux_point.js aux_line.js
│   │   ├── measure/
│   │   │   ├── index.js        measure dispatcher + applyMeasureLimitCoupling
│   │   │   ├── distance.js angle.js radius.js circle_info.js calc.js
│   │   ├── propertySheet.js    buildSchema(edit_tar, ctx) — schema dispatcher
│   │   └── _schemaHelpers.js   fields-decl → applyDefaults + buildWhiteListKey
│   │
│   ├── component/
│   │   ├── baseComponent.jsx   JsonEditBlock, BPG_FileBrowser, primitive widgets
│   │   ├── rdxComponent.jsx    redux-bound display widgets, tag UI
│   │   ├── ComponentBoundary.jsx  granular error boundary (per-canvas)
│   │   └── CoreLogPanel.jsx    inspd_log live tail panel
│   │
│   ├── redux/
│   │   ├── redux.js            store ctor, middleware chain
│   │   ├── actions/UIAct.js DefConfAct.js   action types/creators
│   │   ├── reducer/UICtrlReducer.js         monolithic reducer (~1500 lines)
│   │   ├── reducer/ConnectionInfoReducer.js
│   │   ├── reducer/spcStats.js              pure SPC stats (extracted)
│   │   ├── middleware/ActionThrottle.js     debounce/throttle for hot actions
│   │   └── middleware/ECStateMachine.js     xstate-driven state machine
│   │
│   ├── UTIL/
│   │   ├── logger.js           ★ mkLog facade + NAMESPACES registry + verbose helpers
│   │   ├── diagLog.js          in-browser ring buffer (capture console.* into RAM)
│   │   ├── inspDBQueue.js      durable IndexedDB queue (failed-insert replay)
│   │   ├── InspectionEditorLogic.js   THE editor model (shapeList, lookups, sha1)
│   │   ├── BPG_Protocol.js     raw framing + map_BPG_Packet2Act + INSPECTION_STATUS
│   │   ├── websocket.js        autoReconnect + reqTrack helpers
│   │   ├── DefLoadWithImageFallback.js  imgsrc-extension probing
│   │   ├── MathTools.js MISC_Util.js InspectionStatus.js MeasureResultResolution.js
│   │   └── structures.js BPG_Protocol.js
│   │
│   └── languages/zh_TW.js      i18n dict (zh_TW only; English fallback by key name)
│
├── tools/webctl/
│   ├── webctld.mjs             Playwright daemon (drives browser for QA)
│   ├── webctl.mjs              CLI client
│   ├── golden.mjs              def-JSON oracle (regression by serialization)
│   ├── flows.mjs               behavioral regression (load/select/edit/add/del)
│   ├── mock_inspd_log.mjs      mock inspd_log.v1 WS server (replays fixtures)
│   ├── qa/r*_*.mjs             40+ focused regression scripts
│   └── baseline/               golden snapshots + fixtures (incl. inspd_log/)
│
├── docs/
│   └── TEAM_HANDOFF.md         (this file)
│
└── OPENQUESTION.md             ★ live structural backlog + decisions
```

---

## 4. Architecture in one diagram

```
                          ┌──────────────────────┐
                          │  visSele (Core0_1)   │
                          │  C++, :4090 BPG WS   │◄────────────┐
                          │  :4091 inspd_log WS  │◄────┐       │
                          └──────────────────────┘     │       │
                                                       │       │
   ┌──────────────────────────────────────┐            │       │
   │   WebUI (this app, React 16)         │            │       │
   │                                      │            │       │
   │  comm/BPG_WS  ──── core control + image + reports ─┘       │
   │  comm/CoreLogClient ── log stream ────────────────────────┘
   │                                      │
   │  Redux (single store) :              │
   │  ├─ UIData {edit_info {_obj, ...},   │
   │  │           c_state, sm, ...}       │
   │  └─ ConnInfo {CORE_ID, ..._CONN_INFO}│
   │                                      │
   │  State machine: xstate, modes        │
   │    MAIN -> DEFCONF_MODE -> INSP_MODE │
   │                                      │
   │  Canvas: EverCheckCanvasComponent    │
   │    .draw() reads edit_info,          │
   │    dispatches via .EmitEvent         │
   │                                      │
   │  diagLog ring (RAM, 2000 entries) ───┐
   │  IDB queue (failed inspection inserts)│
   │                                      │  ←─ "Download Diagnostics" button
   └──────────────────────────────────────┘     pulls ring + IDB into one file
```

**Single store, single state machine, single canvas instance.** Mode is the
xstate `c_state.value`; UI shells branch off that. Everything that mutates def
goes through actions; the canvas drives interaction-events via `EmitEvent` →
action.

---

## 5. The two WebSocket channels

| Channel | Port | Protocol | What |
|---|---|---|---|
| **BPG core** | 4090 | binary BPG over WS | LD (def+image), II (instant insp), CI (live insp), ST (settings), report streams |
| **inspd_log** | 4091 | JSON over WS, subprotocol `inspd_log.v1` | log lines from visSele's `LOG*` macros, OTel-aligned schema |

`comm/BPG_WS.js` owns 4090. `comm/CoreLogClient.js` owns 4091. Both have:
- Auto-reconnect with exponential backoff
- Lifecycle events through `mkLog('comm.ws')` / `mkLog('comm.corelog')`
- Promise-keyed request/response over an `id` field

**Reading the BPG line**: `BPG_Protocol.raw2obj`, `raw2Obj_IM` (binary image
frames), `map_BPG_Packet2Act` (BPG → redux action). The IM path is the most
involved — supports both raw RGBA and JPEG (opt-in via core GS).

**Reading the inspd_log line**: see `docs/LOGGING_WEBUI.md` (in InspectionCore
docs). Frame is OTel-shaped: `{severityNumber, severityText, body, attributes
{module, code.filepath, code.lineno, code.function}, timeUnixNano,
tsMsSinceStart, traceId, spanId}`.

---

## 6. Logging system (WebUI side) — read this once

We aggressively unified all logging in commits `1f84ca68` → `004bdbca` (5
commits). The contract:

### One facade

```js
import { mkLog } from 'UTIL/logger';
const log = mkLog('editor.shapes');
log.debug('[event-tag]', structuredArg);
log.info('[ok]', { a: 1, b: 2 });
log.warn('[recoverable]', { ... });
log.error('[bug]', err);
```

**No raw `console.log` anywhere in src/**. Two exceptions in `script.jsx`
(window.onerror / unhandledrejection) — they bypass loglevel to guarantee
delivery into `diagLog`'s wrapped console.

### Namespaces (authoritative — in `UTIL/logger.js` NAMESPACES)

```
comm.ws       — WebSocket lifecycle (BPG transport)
comm.bpg      — BPG framing/dispatch
comm.db       — DB_WS (inspection-record persistence)
comm.api      — generic API middleware
comm.corelog  — CoreLogClient (the inspd_log channel)

editor.model    — InspectionEditorLogic
editor.reducer  — UICtrlReducer + ConnectionInfoReducer + spcStats
editor.action   — ActionThrottle middleware
editor.shapes   — per-shape modules
editor.def      — def-file serialize + sha1 + load/save

canvas.draw   — renderUTIL + per-frame draw (default WARN — would be NOISY at INFO)
canvas.ctrl   — EverCheckCanvasComponent ctrl + interaction
canvas.cam    — CameraCtrl

ui.main       — MAINUI top shell
ui.defconf ui.insp ui.report ui.calib ui.instinsp
ui.base       — baseComponent.jsx
ui.rdx        — rdxComponent.jsx
ui.boundary   — ComponentBoundary / RootErrorBoundary

db.idb        — IndexedDB queue
i18n          — dictLookUp missing-key warnings
qa            — dev-only QA hooks
```

Adding a new namespace? Edit `NAMESPACES` in `UTIL/logger.js`. In dev mode an
unknown namespace logs a console.warn so drift gets flagged.

### Levels

```
debug  — devtools-only noise; OFF in production by default
info   — lifecycle (connect, load, mode flip)
warn   — recoverable problem (reconnect, NAK, fallback)
error  — bug / incident (uncaught throw, sha1 mismatch)
```

### Runtime control (no rebuild needed)

```js
__log.verbose('comm.ws')    // flip one namespace to debug; persisted to localStorage
__log.verbose()              // global debug
__log.quiet('comm.ws')       // restore registered default
__log.quiet()                // restore all defaults
__log.list()                 // see current per-ns level
__log.namespaces()           // see what's registerable
__log.set('foo.bar', 'trace')
```

URL param works too: `?logLevel=debug` or `?logLevel:comm.ws=trace`.

### diagLog ring (in-browser)

Every `mkLog().*` call funnels through `loglevel` → `console.*` → `diagLog`
wrapped console, which captures up to 2000 entries in RAM with timestamps. The
"Download Diagnostics" button in the right-side drawer dumps this to a text
file. Pair this with the IDB failed-insert queue for the support bundle.

### Core-side logging (inspd_log)

That's a **separate channel** on port 4091. Read `docs/LOGGING_WEBUI.md` in
`InspectionCore/docs/`. The WebUI consumes it via `CoreLogPanel`, see §10.

---

## 7. Testing (webctl)

We don't use Jest/RTL. Instead we have a webctl harness that drives a real
Playwright Chromium against the Vite dev server. Tests are integration-shaped.

### Three layers

```
golden.mjs       def-JSON serialization oracle. Run after def-related changes.
flows.mjs        8 behavioral flows (load/select/edit/editInput/add/addArc/
                 addMeasure/addThenDelete). Run after ANY change.
qa/r*_*.mjs      40+ focused regression scripts (r6_shapeattr, r7_downsamp,
                 r10_smoke, ...). Run after rounds of related changes.
```

### Typical workflow

```bash
# Pre-flight (every change)
npm run typecheck                 # tsc --noEmit; allowJs=true, checkJs=false
                                  # — files opt into checking via // @ts-check

# Functional regression
WEBCTL_MODEL=/path/to/some_def \
  node tools/webctl/flows.mjs verify   # 8 flows. Should be 8 PASS.

# QA suite
node tools/webctl/qa/r1_editor.mjs
node tools/webctl/qa/r6_shapeattr.mjs
node tools/webctl/qa/r10_smoke.mjs       # known intermittent — see §9
```

### Test-running constraints

- **webctld holds a single browser tab**. Run one test script at a time.
- The Playwright viewport is `1280×720` — intentionally fixed for deterministic
  baselines. Do not change. For your own hand-debugging, open your own Chrome.
- webctld can grab fullPage screenshots: `/shot?path=...&full=1`.

### Mock inspd_log for log-panel work

```bash
node tools/webctl/mock_inspd_log.mjs --port 4091 \
  --file tools/webctl/baseline/inspd_log/caliper_verify_x60.log.gz \
  --rate 15 --drop-every 80
```

This emits the OTel schema as if it were a real drainer. **The CoreLogPanel
URL doesn't change** — both real drainer and mock listen on
`ws://<host>:4091/log`. When core is rebuilt and `INSP_LOG_DAEMON=1` set, the
mock is no longer needed.

---

## 8. The recent rework (so you can read git log productively)

This is what the team has been doing on `webui/editor-refactor`. Each wave is
a coherent unit of work; the commit messages are dense and worth reading.

### Wave A — comm robustness (commits before `433ec7ff`)
Wave 1 BPG decode fixes, parseFloat NaN guards, dead-code sweep. Set the
stage for the rest.

### Wave B — Keystone per-shape vertical slice
The single largest structural change. Moved each shape's setup-UI + canvas
control + draw logic into one module per shape (`src/shapes/<type>.js`).
Replaces 700-line dispatch switches in renderUTIL / DefConfUI / EverCheck
with thin `getShapeModule(type)` lookups.

Commits: `446ac70d` (fields schema unification), and the keystone phase
commits earlier on the branch.

### Wave C — Logging unification
5 commits (`1f84ca68 f99d8d6d 73f764a6 004bdbca`) wiping ~200 raw `console.log`
across 32 files, all routed through `mkLog`. See §6 above.

### Wave D — Comm structured logs + age sweep
`05b6ca92` (BPG_WS structured), `68bf441e` (websocket.js structured),
`34944e6a` (trackWindow age-sweep — plugs a leak where un-ACK'd requests
piled up forever).

### Wave E — Phase-2 sig360 perf opt-ins
`a9a79fc1` exposed `matching_version` + `inspection_downsample` per-def so
operators can opt in to the v2 morph-boundary dual-sig matcher (3.8× speedup
on the 5MP golden, byte-identical when off).

### Wave F — JPEG image transfer
`5329e7c4` taught `raw2Obj_IM` + `SetImg` the `format` byte. Default raw RGBA
preserves all behavior; flipping `GS {IMG_STREAMING_JPEG_QUALITY: 75}` cuts
image bandwidth ~40-60×.

### Wave G — CoreLogPanel (inspd_log integration)
`5acd4e0d` (initial panel + mock + client + drawer), `8380b2f9` (OTel
migration), `c815b5c2` (gzip in mock), `f11567bd` (time-fallback for
degenerate drainer timestamps). End-to-end live tail of core logs in the
WebUI, no code change between mock and real drainer.

### Wave H — Operator-edge fixes
`6cfce177` migrated 4 main-screen LD callsites to `loadDefWithImageFallback`
so the cv::imread extension-probing regression in Core0_1 stops breaking
def loads from the MAIN file picker.

---

## 9. Caveats (the painful ones)

These are non-obvious traps that have cost the team real time. Read once;
internalize.

### 9.1 cv::imread doesn't probe extensions
Core0_1 used to use `LoadIMGFile` which automatically tried `.png/.jpg/.bmp`
when given an extensionless path. The acvImage→cv::Mat migration replaced
it with bare `cv::imread`, which doesn't probe. **WebUI's LD imgsrc
convention sends the basename without extension**, so this regressed every
LD until we added `UTIL/DefLoadWithImageFallback.js`. ALL future LD callsites
must use that helper, never raw `ACT_WS_SEND_BPG("LD", ...)`.

### 9.2 1006 disconnect flake (known, not fixed)
Occasional `ws.close` code 1006 on first paint or under sustained QA. Core
stays up the whole time. Surfaced now via `comm.ws` structured logs (`[ws]
close-1006-abnormal`). Reconnect picks it up within 10s. Causes
`r10_smoke S1/S9/S10` to intermittently fail; documented in `OPENQUESTION.md`
"Known QA flake" section.

### 9.3 ActionThrottle middleware can swallow rapid dispatch
`ActionThrottle.js` debounces actions with the same `ATID` (default 100ms,
posEdge=true). Tests that dispatch then immediately read state may see stale
state. The `r6_shapeattr addMinimal` test was bitten by this; pattern is to
add an explicit `await sleep(200)` between dispatch and state read in tests.

### 9.4 `edit_info._obj` is a mutable class instance in Redux state
`InspectionEditorLogic` lives inside `edit_info._obj`. Multiple mutators
(SetShape, SetShapeList, Setsig360info, SetCameraParamInfo) historically
mutated in place. We collapsed `edit_info.list` (a mirror of `_obj.shapeList`)
in commit `c066f404`. SetShape is now immutable; the others still mutate.
Re-renders rely on the top-level `{...newState}` spread at every reducer exit
to bust selectors. **Do not selectors-drill `_obj.shapeList` referentially**
— always go through a render-stable wrapper. See OPENQUESTION Q1.

### 9.5 Per-frame draw is imperative, not React
The canvas (`EverCheckCanvasComponent`) is one HUGE class component. It
imperatively redraws on every prop update. React reconciliation does NOT see
inside the canvas. This is correct (canvas-rendering style) but surprises
people. Don't try to React-ify it.

### 9.6 Loglevel binds method refs at setLevel time
`diagLog` wraps `console.*` at boot. Loggers created BEFORE `initDiag` bind
to the raw console and bypass the diag ring. The `initLogger()` rebind loop
in `UTIL/logger.js` fixes this for the migration period; once every file
uses `mkLog`, the loop becomes a no-op.

### 9.7 The two raw `console.error` calls in script.jsx are intentional
For window.onerror / unhandledrejection. They go straight to the wrapped
console (which IS diagLog) without going through loglevel, guaranteeing
delivery even if loglevel binding state is weird.

### 9.8 inspd_log drainer death modes
- **Pre-5fc8b106**: heartbeat-based parent-death detection died after ~5s of
  producer idleness. Fixed; current code uses `getppid()==1` on POSIX.
- **Pre-e7edf574**: per-line timestamps weren't stamped; every frame had
  `timeUnixNano == hello.startedUnixNano`. WebUI Panel has a 3-tier fallback
  (`timeUnixNano - startedUnixNano` → `tsMsSinceStart` → `Date.now() - startedMs`
  with `~` marker). Don't remove the fallback even after core fix — defensive.

### 9.9 OpenCV migration is in flight
Core0_1 is undergoing acvImage→cv::Mat migration (Phase 3a/3b done; see
`InspectionCore/docs/OPENCV_MIGRATION.md`). Field names / wire format are
preserved (cv::Point2f still serializes as `{x,y}`), but quirks like 9.1
can drop unexpectedly. When something LD/II/CI-related breaks, first ask
"did something in Core0_1's migration just land?". `git log -10 -- Core0_1/`.

### 9.10 React 16 + componentWillX warnings
The codebase is React 16 with class components using deprecated lifecycle
methods (`componentWillMount`, `componentWillReceiveProps`). React 17 codemod
would change this, but factory deployment freezes React 16. Just live with
the deprecation noise (it's filtered out of diag ring by default).

### 9.11 Vite + Webpack
We have BOTH Vite (`npm run dev`) and Webpack (`npm run build` for prod).
Vite is for dev iteration (HMR, fast boot). Production bundle is webpack.
They share src/ but have different module-resolution semantics — when you
add an import, test BOTH dev and prod.

### 9.12 The factory environment has no devtools
Operator devices ship to factories with no Chrome devtools, sometimes no
keyboard. ALL debug paths must work via the on-screen Drawer:
- "Download Diagnostics" pulls diagLog ring + IDB queue
- "Core Logs" opens CoreLogPanel
- `__log.verbose('comm.ws')` from any persistent console mechanism

Anything that requires F12 / console access to inspect IS A BUG in this
codebase, not a feature.

### 9.13 Caliper-mode per-caliper overlay: stale-state traps
The line/arc caliper feature attaches a per-caliper hit array to each
detected shape (`cal_hits`). Hits are rendered both in inspection mode
(via `ShapeAdjustsWithInspectionResult` merging onto `eObject.cal_hits`)
and in def-conf mode (via `renderer.cal_hits_by_id` built from
`edit_info.inspReport`). Three traps lurked in this surface:

1. **NA early-return left stale hits on the redux-stored shape.**
   `ShapeAdjustsWithInspectionResult` early-returns when
   `inspection_status === NA`. Before fixing, it didn't touch
   `eObject.cal_hits` — so the prior successful inspection's hits
   stayed attached after a failing run. Fix: rebuild cal_hits with all
   entries marked outlier (`st:1` → red X marks) so the visual signal
   for a failed fit is "all crosses turn red," and clear the stale fit-
   endpoint fields:
   ```js
   if (eObject.inspection_status === INSPECTION_STATUS.NA) {
     delete eObject._pt1; delete eObject._pt2; delete eObject.adj_pt1;
     if (inspAdjObj.cal_hits)
       eObject.cal_hits = inspAdjObj.cal_hits.map(h => ({ ...h, st: 1 }));
     else delete eObject.cal_hits;
     return;
   }
   ```
   The matching def-conf side override lives in
   `EverCheckCanvasComponent.draw_DEFCONF` (when building
   `cal_hits_by_id` from `inspReport`).

2. **`modShapeCleanUp` returned `undefined` on NA**, which made the
   CHECK callback skip `SetShape` entirely. The shape in redux never got
   the cleared state — old `cal_hits` lived on forever. Return the
   cleared shape on NA so `SetShape` fires and the redux store stays
   consistent with the latest fit outcome.

3. **`shape.cal_hits` outranks `renderer.cal_hits_by_id`** in the
   per-shape draw fallback chain. If a prior run set `shape.cal_hits`
   and a later run produced a different (or empty) `cal_hits_by_id`, the
   stale shape data wins. Either clear `shape.cal_hits` explicitly (1)
   or reverse the priority — we chose (1) because per-shape storage is
   the authoritative source after `SetShape`.

Together these three were the "why are old green X marks still showing
when the fit failed?" bug. If you add another caliper-overlay field,
walk through all three sources and decide who clears it on failure.

### 9.14 Frame conventions for inspection report coords
The core ships per-shape report coords in different frames depending on
the field — match what each consumer expects:

| Field | Frame | When |
|-------|-------|------|
| `detectedLines[i].cx/cy/vx/vy`, `pt1`, `pt2` | image-mm | `ShapeAdjustsWithInspectionResult` forward-transforms via `pointForwardTrans`, then overwrites shape pt1/pt2 |
| `detectedCircles[i].x/y/r`, `pt1/2/3` | image-mm | same |
| `cal_hits[i].x/y` | **object-frame mm** (def coord system) | pass through unchanged — already in the def's pt1/pt2 frame |

If you see hit coordinates in image-px range (~1500+), the core's
mm-conversion didn't run for that path. See
`InspectionCore/docs/CORE0_1_CAVEATS.md#h5`.

### 9.15 Compact PropertySheet — number-input commit semantics changed
The rewritten `JsonEditBlock` (+ per-shape `PropertySheet` components)
uses **commit-on-blur/Enter** for number inputs instead of the legacy
NumPad popup's "commit each typed value". Tests / scripts that fire only
the `input` DOM event (and not `blur`) will not see the redux state
update. Either fire blur in your test, or call the underlying redux
action directly. The `editInput` flow in `tools/webctl/flows.mjs` is the
canonical example of the broken pattern; update baselines accordingly.

---

## 10. CoreLog integration (inspd_log on :4091)

Living spec: `InspectionCore/docs/LOGGING_WEBUI.md`. The WebUI side
implementation:

### Components
- `src/comm/CoreLogClient.js` — transport. Reconnect 1s→2s→4s→8s→16s→cap30s.
  Promise-keyed commands (subscribe/setLevel/getModules/dumpNow/ping).
- `src/component/CoreLogPanel.jsx` — UI. Virt-less scroll with 5000-entry cap.
  Module checkbox row + needle filter + pause + follow + dump_now + clear +
  "set all → SEV" row. Crash banner with stack expander. Backpressure
  `dropped` frames render as gap markers.

### Schema (OTel-aligned)
```js
// log frame
{
  type: 'log',
  timeUnixNano: '1748751131019144000',  // string, int64-safe
  tsMsSinceStart: 1019144,              // convenience
  severityNumber: 9,                    // OTel: TRACE=1 DEBUG=5 INFO=9 WARN=13 ERROR=17 FATAL=21
  severityText: 'INFO',
  body: 'rendered message',
  attributes: {
    module: 'cam.bmp',
    'code.filepath': '...',
    'code.lineno': 41,
    'code.function': '...',
  },
  traceId: null, spanId: null,           // reserved for future span tracing
}

// other frame types: hello, logBatch, backlogChunk, dropped, crash, ack, pong, modules
```

### Mock for dev when core isn't running
See §7.

### Caveats specific to this integration
- See §9.8 for the time-fallback story.
- The drainer is opt-in via `INSP_LOG_DAEMON=1`. If you don't set it, :4091
  is closed and the Panel sits at "connecting…" forever (correctly).
- `setLevel` reaches the producer via an IPC ring (commit `b7e392ec`); it
  takes effect on the next ring tick.

---

## 11. Per-shape vertical slice (Keystone)

The structural rework that everything else hangs off of. Each shape type has
ONE module that owns:

```js
// src/shapes/line.js
export const type = 'line';
export const fields = {           // unified defaults + editor-schema decl
  vertex_touch_searching: { editor: 'switch', default: false },
  locating: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['contour','caliper'] }),
    default: 'contour',
    normalize: (v) => (v === 'caliper' ? 'caliper' : 'contour'),
  },
};
export const applyDefaults = (shape) => applyDefaultsFromFields(shape, fields);
export const buildWhiteListKey = (ctx) => buildWhiteListKeyFromFields(fields, ctx);
export const availableRefShapes = (shapeList) => [];
export const fitCameraCenter = (shape) => ({ x: (shape.pt1.x+shape.pt2.x)/2, ... });
export const draw           = (ctx, shape, renderer, opts) => { ... };
export const drawInspection = (ctx, shape, renderer)       => { ... };
```

Adding a new shape type: write `src/shapes/<type>.js`, add one line to
`src/shapes/index.js` (`getShapeModule` registry). DONE — no other file
needs to change.

Measure has a subtype dispatcher (`measure/index.js`) with subtype modules in
`measure/{distance,angle,radius,circle_info,calc}.js`.

`distance` still delegates to `renderer.drawMeasureDistance` (heavily coupled
to renderer state — intentionally left in renderUTIL per the "keep coupled
logic together" rule from feedback memory).

---

## 12. Backlog (OPENQUESTION.md)

OPENQUESTION.md is the live structural backlog. Don't archive it. Sections:

- **Q1 ✅ DONE** — `edit_info.list` → `_obj.shapeList` collapse (commit `c066f404`)
- **Q2 APPROVED, planned** — edit_info god-object 3-bucket split (cold editor /
  cold defMeta / HOT runtime+results). Fresh-session-sized. Approach: type
  consumers via `// @ts-check` first; move one bucket at a time; tsc lists
  every missed site.
- **Q3 Next rework section** — comm-layer Path B (decouple BPG_WS from
  `comp.props.ACT_*`), whiteListKey schema (✅ done as `_schemaHelpers.js`),
  baseComponent split (deferred — failed twice, needs root-cause).
- **North-star** — per-shape vertical slices (Keystone delivered the editor +
  draw + canvas-ctrl piece; remaining is collapsing renderUTIL's `drawMeasure*`
  helpers if needed).
- **Structural rework backlog** rounds 2-8 — detailed line-numbered findings.
- **DECISIONS** — durable decisions the team has made (e.g. dead-code = kill,
  DB write fail → IDB buffer, sha1 mismatch → HARD BLOCK, WebUI2 scope).
- **Round 9-onward known QA flake** — r10_smoke 1006.

When picking work, read OPENQUESTION top-to-bottom first. Many items have
context that isn't obvious from code.

---

## 13. The webctl harness (read the code, it's small)

```
tools/webctl/
├── webctld.mjs        ~150 lines. HTTP server on :8765 wrapping Playwright.
│                      Endpoints: /goto /reload /click /fill /press /wait
│                      /eval /text /shot /shutdown. Browser stays open
│                      across requests; state persists.
├── webctl.mjs         CLI client (rarely used directly).
├── golden.mjs         Two modes: capture / verify. Replays a def through the
│                      real LD path, snapshots the resulting GenFeature_..._JSON
│                      to baseline/. Byte-identical regression oracle.
├── flows.mjs          8 behavioral flows. Each: reset(), do flow, snapshot
│                      {c_state, shapes, edit_tar, inputs, def}. capture/verify.
├── regress.mjs        umbrella runner.
├── mock_inspd_log.mjs ~250 lines. Standalone Node WS server speaking inspd_log.v1.
│                      Knows the OTel schema; replays disk-format fixtures or
│                      emits synthetic; supports --crash-after, --drop-every.
└── qa/                40+ focused regression .mjs scripts. Naming: r<round>_<topic>.mjs.
    └── SUMMARY.md     What each one covers.
```

If you add a regression test, name it `r<N>_<topic>.mjs`, drop it in `qa/`,
update `SUMMARY.md`. Don't add baselines that aren't bit-stable.

---

## 14. Cross-team communication

The Core team and WebUI team coordinate via git. There's no shared
issue tracker. Practical conventions:

- **Schema / wire contracts** → commit to `InspectionCore/docs/*.md`. Both
  sides read the doc; PR review is on the doc as much as the code.
- **Cross-team todos / questions** → `docs/CROSS_TEAM/INBOX.md` (or similar;
  see the discussion in this session). Append, don't overwrite. Date-stamp.
- **Heads-up commits** → use a `[ask-webui]` / `[heads-up]` prefix in the
  subject. Easy to grep: `git log --oneline -20 --grep='\[ask-webui\]'`.
- **Bug reports between teams** → commit a markdown post-mortem in
  `docs/POSTMORTEMS/` linked from the inbox. We have one for the drainer
  self-exit (see history for `[inspd_log] drainer self-exits ~4-6s` thread).

---

## 15. Don't-do list (anti-patterns)

From accumulated feedback. Internalize these.

- **Don't add raw `console.log`**. Use `mkLog`. The lint rule isn't on (React
  16 codebase would explode); convention is enforced socially.
- **Don't add a new feature flag / fallback** unless it's truly conditional.
  We aggressively delete dead branches.
- **Don't fragment finely**. Coupled / stateful / side-effectful logic stays
  in one file even if big. Extract only pure / functional / intuitive pieces.
  (See feedback memory `refactor-splitting`.)
- **Don't fix Core's back-compat regression in Core**. If Core0_1's migration
  drops a behavior the WebUI was relying on, fix the WebUI side. Core stays
  minimal. (See feedback memory `fix-consumer-not-provider`.)
- **Don't bypass `loadDefWithImageFallback`** for any LD with an extensionless
  imgsrc. There are still legacy code paths that do — those are bugs.
- **Don't change Playwright viewport** in webctld.mjs for "cosmetic" reasons.
  All visual baselines will break.
- **Don't run `git push --force`** without explicit user authorization. Same
  for any destructive git op.
- **Don't ship without `npm run typecheck`** at minimum + flows green for
  anything touching shape / def / canvas seams.

---

## 16. Onboarding day 1 (concrete)

```bash
# 0. Clone + install
cd visSele/UI/WebUI && npm install
# (Core repo presumably already cloned; cmake/vcpkg set up)

# 1. Read this doc end-to-end. ~30 min.

# 2. Read OPENQUESTION.md. Understand the SHAPE of remaining work. ~20 min.

# 3. Read the recent log:
git log --oneline -30
# Pick 3-5 commits that look relevant to where you'll work and read their
# full messages. Commit messages in this repo are dense; they're the
# "design doc" of the change.

# 4. Run the suite once:
cd UI/WebUI && npm run typecheck
WEBCTL_MODEL=/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify \
  node tools/webctl/flows.mjs verify

# 5. Start everything (§2 above), poke around the live UI.

# 6. Make a trivial change (e.g. `__log.verbose('comm.ws')` from console,
#    observe diag ring fill; then add a log line in some module via mkLog;
#    confirm it shows in "Download Diagnostics"). Don't commit; just touch
#    everything once.

# 7. Pick a low-risk item from OPENQUESTION (e.g. dead-key i18n, dead-action
#    cleanup) and ship a commit. Run typecheck + flows. Commit with detailed
#    message in the house style (lowercase scope, body explains WHY).
```

---

## 17. People / contact

[fill in once team forms]

---

## Appendix A — Quick reference

### Common commands
```bash
npm run dev                                  # Vite dev server :8081
npm run typecheck                            # tsc --noEmit
node tools/webctl/flows.mjs verify           # 8-flow regression
node tools/webctl/qa/r10_smoke.mjs           # smoke
node tools/webctl/mock_inspd_log.mjs --port 4091   # mock core-log server

# Real core + drainer
INSP_LOG_DAEMON=1 INSP_LOG_WS_PORT=4091 \
  DYLD_LIBRARY_PATH=../build/mac-arm64 \
  ../build/mac-arm64/visSele

# Devtools-only log control
__log.verbose('comm.bpg')      // crank one namespace to debug
__log.quiet()                  // restore defaults
__GP_DIAG__.downloadDiag()     // download ring buffer
__GP_DIAG__.diagCount()        // ring size
```

### Useful redux probes (devtools console)
```js
__GP_STORE__.getState().UIData.edit_info._obj.shapeList
__GP_STORE__.getState().UIData.c_state.value
__GP_DEF__()                   // generated def (binary_processing_group root)
__GP_BPG__.raw2obj(evt)        // for poking the BPG wire
__GP_MEASURE__.applyMeasureLimitCoupling(obj, key, preVal)
```

### Branch / commit conventions
- Long-running branch: `webui/editor-refactor`. Don't squash; commit-by-commit
  history is the design log.
- Commit subject: `kind(scope): brief`. kind ∈ {feat, fix, refactor, perf,
  test, log, obs, docs, chore}. scope ∈ {webui, webui/log, webui/shapes,
  webui/comm, webctl, ...}.
- Body: WHY first, WHAT second. File paths welcome.
- Co-author line at the bottom when AI-paired.
