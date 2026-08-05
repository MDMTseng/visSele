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
