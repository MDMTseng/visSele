# Core0_1 — Caveats & Hard-Won Gotchas

Concrete traps in the **Core0_1** (1st-gen, factory-deployed) C++ inspection core,
discovered while reworking the measurement engine (caliper line/arc, search points,
OpenCV integration). Read with `PROJECT_NOTES.md`, `caliper_primitive_locating_design.md`,
and `search_point_rework.md`. Each item: trap → why → what to do.

> File paths are stable; line numbers drift — grep the named symbols.

---

## A. Build & toolchain

### A1. `FEATURE_OPENCV` comes from CMake, never from a source `#define`
The macro is set by `target_compile_definitions(MatchingEngine PUBLIC FEATURE_OPENCV)` in
`CMakeLists.txt` (gated by `-DFEATURE_OPENCV=ON`). A `#define FEATURE_OPENCV 1` at the top of
a `.cpp` was once added just to satisfy IDE IntelliSense — it **forces the OpenCV path on
regardless of build config** and silently diverges non-OpenCV builds. Don't do it.
- **To un-grey `#ifdef FEATURE_OPENCV` in the editor:** generate `compile_commands.json`
  (`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` is in CMakeLists now) and point clangd / the VSCode
  C/C++ extension at `build/mac-arm64/compile_commands.json`. Never `#define` build macros in
  source to make tooling happy.

### A2. Homebrew OpenCV `dnn` is broken on this setup
Link only `opencv_core opencv_imgproc opencv_imgcodecs opencv_calib3d`. Pulling the full
OpenCV (incl. `dnn`) fails to link. The build dir in use is `build/mac-arm64` (Mac arm64 dev;
Windows deploy is MSYS2-MinGW — see `project_build_toolchain_notes`).

### A3. OpenCV is opt-in per feature, acv is the fallback
New code uses `cv::Mat` under `#ifdef FEATURE_OPENCV` with an acv fallback
(`CvBridge.{h,cpp}` does `acvImage`↔`cv::Mat`). The user explicitly allowed OpenCV to replace
acv where it helps — but keep the non-OpenCV path compiling.

---

## B. Running & validating

### B1. The `--insp` harness MUST run from `Core0_1/`
`visSele --insp <img.png> <def.hydef> <out.json>` is the headless golden-sample harness
(`Core0_1/wiringPanel.cpp`, cp_main). It loads the camera calibration via the **relative path**
`data/default_camera_param.json`.
- **If run from the wrong cwd** (e.g. `build/mac-arm64`), the calib file isn't found, the calib
  map stays **uninitialized**, `img2ideal` divides by an uninit `RNormalFactor=0`, and **every
  line/circle fit returns NaN** while sig360 matching still "works." This looks like a deep
  engine bug but is just cwd.
- **Always:** `cd InspectionCore/Core0_1 && ../build/mac-arm64/visSele --insp ...`.

### B2. STALE CORE — rebuild ≠ restart
The WebUI talks to a **long-running** core process on `ws://localhost:4090`. A fresh binary on
disk does nothing until that process is killed + relaunched.
- **Symptom:** a new feature is wrong in the live app but correct in `--insp` on the same image;
  legacy/unchanged paths look fine.
- **Detect:** `ps -o pid,lstart -p <pid>` (start time older than the binary mtime) +
  `lsof -nP -p <pid> | grep LISTEN` (should show `*:4090`).
- **Relaunch:** `cd Core0_1 && DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele &`

### B3. Validation ladder
Legacy report = the **floor** (reproduce it first), not ground truth ("it looks right"). Real
parts = the **ceiling** (the rework may beat legacy with a written reason).
- **>4px divergence from legacy is a red line** unless justified.
- **Measured values are ground truth, NOT RMS / pixel positions / overlay prettiness.** A
  cleaner-looking fit that shifts a downstream measure the wrong way is wrong.
- **Don't diagnose legacy as buggy from raw numbers** — the operator's visual is authoritative.
  Several "obvious bugs" (e.g. two search points at the same point, a 2-point vertex line) were
  correct geometry.
- Harness/compare: `run_caliper_test.sh`, `run_sp_test.sh` (env-tunable edge config).

---

## C. Image / label encoding (binarized object map)

The labeled image (`m_labeledImg`) encodes:
- **background** = white `(255,255,255)`.
- **object interior** = the label value `Num = c0 | c1<<8 | c2<<16` with R channel small.
- **object contour** = `(B,G,R) = (1,128,1) = 98305`.

### C1. A thin object (wire) is ALL contour, no interior
Masking by `label == objLabel` fails for thin features (every pixel is the `98305` contour
value, none carry the interior label) → the mask erases the object itself.
- **Fix used:** predicate = "**not white background**" (`p[0]==255 && p[1]==255 && p[2]==255`
  ⇒ background, else object). See `SearchPointCV.cpp isObjectPx`.
- `labelAt()` returning `0xFFFFFF` is **not** a stale buffer — it's just white background.

---

## D. Coordinates, pose, and the two-pass evaluation

### D1. Report coords are absolute image-mm
Report geometry is in mm: `px = mm / mmpp`, with `offset = {0,0}`. `mmpp` is **per-machine**
(lens/camera) — never hardcode. Def geometry that's authored in mm is converted by `/mmpp` at
parse/use time.

### D2. A primitive's nominal position is pose- AND anchor-transformed
`position → cm.convert(...)` (ConstrainMap / locating-anchor warp) `→ TemplateDomain_TO_PixDomain(... cached_sin, cached_cos, flip_f, calibCen, mmpp)` (object pose).
Skipping either ⇒ the primitive is searched at the wrong place.

### D3. TWO evaluation passes (anchor vs measurement) — and the coupling
A primitive used as a locating anchor is evaluated twice:
1. **prewarp / anchor pass** — `cm` = identity (the ConstrainMap isn't built yet). Position is
   pose-only. **Stable** across runs.
2. **measurement pass** — `cm.convert` applied. The corrected position.
- **Caveat:** the measurement-pass position **drifts** as you change edge-finding code, because
  the anchors that build the ConstrainMap are themselves primitives using the same code. When
  debugging edge-finding in isolation, use the **prewarp** pass (stable); for final values use
  the measurement pass. A "why does my point keep moving as I tweak" is this coupling, not a bug.

---

## E. Measurement engine

### E1. Two distinct mechanisms — don't conflate them
- **Caliper** = line/arc measurement (`Caliper.cpp`): straddle a known edge, project along it,
  robust-fit a line (weighted TLS + MAD) or circle (Kasa + MAD). Opt-in `locating:"caliper"`.
- **Search point** = silhouette/cap finder (`SearchPointCV.cpp`): scan a ray, take the lateral
  extreme. **Search points do NOT use the caliper.**

### E2. `width` means OPPOSITE things in caliper vs search point
This bit hard. Be explicit about which primitive:
- **Caliper** (`caliper_measure`): `length` = search half-length **ACROSS** the edge (span
  `2*length`); `width` = projection width **ALONG** the edge (averaged for SNR).
- **Search point** (`SearchPointCV`, decoded from legacy `getContourPointsWithInLineContour` +
  `acvRotation`): `width` = scan **DEPTH along** the search direction (`±width/2`); `margin` =
  **lateral** half-extent (`±margin`). (These were originally swapped in the rework — a real bug.)

### E3. Arc angle sweep must be CCW over the true span (reflex/wrap)
`caliper_locate_circle` must sweep `angStart→angEnd` using the **CCW span**
`((angEnd-angStart) mod 2π)`, NOT the raw difference. `convert3Pts2ArcData` defines the arc as
the CCW arc through the middle point pt2, which can exceed π. Using the raw difference sends
calipers the wrong way around the circle for reflex/wrapped arcs → half find no edge, fit
diverges (seen: center off 55px). This is fixed; don't reintroduce a naive linear interp.

### E4. Arc primitives have `type:"arc"`, not `"circle"`
A def patcher / type filter that only matches `"circle"` silently skips arcs → they stay on the
legacy path. (Caused a false "0.00 diff vs legacy" because it was comparing legacy-to-legacy.)

### E5. Edge selection is per-primitive; defaults assume backlit dark-on-bright
`edge_select` toolbox: method (`strongest`/`first`/`last`/`middle`/`nth`) + polarity
(`any`/`rising`/`falling`) + `min_strength`, with an adaptive `0.15*maxgrad` noise floor.
- **Default = `strongest` + `falling`** (white→dark outer silhouette).
- `STRONGEST` on a **thick band** picks whichever of the two band edges is stronger →
  inconsistent across calipers → outliers. Use `first`/polarity for thick/ambiguous edges.
- **Inner edges / holes / bright-on-dark need `rising`.** Wrong polarity = fit snaps to the
  wrong edge (presents as "doesn't snap").

### E6. Search-point edge finding needs a strength gate, not just a mask
With the object mask disabled, per-row "strongest blob" picks **background backlight noise** in
every row. Fix: keep only edges whose peak ≥ `0.4 * maxPeak` in the region. Default polarity
`LIGHT_TO_DARK`. Lateral-extreme selection (cap apex), not first-hit-along-ray.

### E7. `vertex_touch_searching` lines
Legacy fits the edge direction then **offsets the line to the outermost point** (for
height/width to the silhouette extreme); for inward-curved edges it links the two outermost
feet. A plain best-fit sits inside → too-small height/width. (Note: line1's apparent 15px
caliper error turned out to be **edge selection**, fixed by `first/falling`, not the
vertex_touch offset — verify which before "fixing" vertex_touch.)

### E8. Caliper report adds `confidence`
Line/circle caliper results carry `confidence` = mean inlier edge confidence
(`strength·(1-0.7·runnerUp/strength)·(0.5+0.5·sharpness)`), used as the fit weight and
serialized only on caliper-path results. Pairs with `s` (rms) + `matching_pts`.

### E9. `TreeExecution` cycle guard
Feature execution is a recursive **memoized DAG**. The cycle guard marks in-progress nodes with
a `STATUS_NA` sentinel; if you add a feature-exec branch, set/clear that sentinel or a ref
cycle hangs/misreports.

---

## F. Backward-compat contract (the overarching constraint)

Core0_1 is **factory-deployed on many machines** with lots of uploaded data. The hard rules:
- **Be back-compatible with old defs** and produce a **superset** of the report schema.
- All rework is **opt-in** (e.g. `locating:"caliper"`); default paths (`locating` unset / 0 /
  `"contour"`) must behave exactly as before.
- Otherwise you may rework freely for robustness/efficiency/fewer bugs, and may borrow proven
  CoreHub (2nd-gen) features.
- Don't change the app's external behavior or break the WebUI's expectations without a flag.

---

## H. Caliper + per-caliper-hits plumbing (new in this rework)

When the line/arc caliper feature gained a per-caliper hit overlay
(`cal_hits` in the report JSON, gray/blue boxes + green/red X markers in
the WebUI), several non-obvious traps surfaced.

### H1. `memset(&Report, 0, sizeof(Report))` becomes UB when the struct grows a vector
`FeatureReport_lineReport` / `_circleReport` used to be POD, so the legacy
`memset` zero-init was safe. Adding `std::vector<CaliperHit> cal_hits;` to
them makes `memset` undefined behavior (corrupts the vector's internal
pointers; later destruction would crash). The fix is value-initialization:
```cpp
FeatureReport_lineReport Report = {};   // C++ value-init; vector ctor runs
Report.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
```
Same trap will return for any struct that gains a non-trivial member —
`std::vector`, `std::string`, anything with a destructor. Search for
remaining `memset(&Report, 0, ...)` patterns when adding members.

### H2. `Caliper.h` ↔ `FeatureManager.h` circular include
`FeatureReport.h` is included by `FeatureManager.h`. Caliper.h includes
`FeatureManager.h` (for `FeatureManager_BacPac`). So a `CaliperHit` struct
defined in `Caliper.h` and referenced from `FeatureReport.h` creates a
cycle: half-parsed `FeatureManager.h` → `FeatureReport.h` references
`CaliperHit` → not yet defined.

**Fix**: define `CaliperHit` in `FeatureReport.h` (the consumer-side
aggregator). `Caliper.h` gets it transitively via `FeatureManager.h ↔
FeatureReport.h`. Only `acv_XY` is required at definition time, which
`FeatureReport.h` already pulls in.

### H3. Three coordinate frames in the report — keep them straight
A single inspection report (per detected line/circle) mixes coordinate
systems by field, depending on what the consumer expects:

| Field | Frame | Convention |
|-------|-------|------------|
| `end_pt1`, `end_pt2`, `line_anchor`, circle `x`/`y`/`r` | image-mm | rebased to image-px then `* mmpp` |
| `cal_hits[i].pt` | **object-frame mm** | also through `PixDomain_TO_TemplateDomain` (inverse of `TemplateDomain_TO_PixDomain`) — same coord frame as the def's `pt1`/`pt2` |

Reason: existing consumers expect end_pt1/pt2 in image-mm (they apply the
`{cx, cy, rotate, isFlipped}` transform downstream). But cal_hits is new
and lives on the def-conf overlay, which works in def-frame mm — so the
core does the inverse transform once and ships object-frame coords. This
keeps the def-conf overlay code trivial (no transform at the WebUI).

Per-hit frame summary:
- **image-px**: as returned by `caliper_locate_*` (`r.hits[i].pt`).
- **image-mm**: after `acvVecMult(..., mmpp)` (used for `end_pt1`/`pt2`).
- **object-frame mm**: after `PixDomain_TO_TemplateDomain` (used for
  `cal_hits[i].pt`).

### H4. Caliper sub-params (`cal_width`, `cal_length`, `cal_step`) are def-mm — NOT raw px
Despite header comments saying "px", these are **mm at the def level** and
get converted to px inside the per-primitive `*_ReportGen`:
```cpp
lineDef.cal_width   /= mmpp;
lineDef.cal_length  /= mmpp;   // sentinel -1 passes through
lineDef.cal_step    /= mmpp;
lineDef.cal_max_error /= mmpp;
```
This was the design choice to keep the entire def schema in one unit (mm)
— matches `pt1`/`pt2`/`margin`. The header comments now reflect this; the
in-code defaults are mm-equivalents (`cal_width = 0.5f`, not `9`).

### H5. Early-return in `CircleMatching_ReportGen` skipped the conversion loop
The circle ReportGen has an early-return on `cf.circle.radius != cf.circle.radius`
(NaN check). The bottom-of-function block that converts `cr.pt1/pt2/pt3 *=
mmpp` is wrapped in a `{ ... }` after that check, so on the NaN path it
never ran. We initially put the cal_hits px → object-frame conversion in
that block — and cal_hits shipped at image-px (~1600) on failed fits,
breaking the WebUI overlay.

**Fix**: convert at *push-time* inside the inlier-collection loop, so
**all** return paths emit correctly-framed hits:
```cpp
for (const auto &h : rr.hits) {
  CaliperHit ih = h;
  acv_XY pix_pt = acvVecAdd(h.pt, off);
  ih.pt = PixDomain_TO_TemplateDomain(pix_pt, ..., calibCen, mmpp);
  cr.cal_hits.push_back(ih);
}
```
Generalizable rule: any new field that needs a frame conversion should be
converted at the point it's *built*, not lumped into a tail-of-function
loop that early-returns can bypass.

### H6. `Report.status` was hardcoded `r.nInlier >= 5` instead of using `r.ok`
`LineMatching_caliper` had a literal `if (r.nInlier >= 5)
Report.status = STATUS_SUCCESS;` — inherited from the contour-path
expectation that a line fit needs ≥5 contour points. With caliper mode
that threshold was wrong: even a clean 3-or-4-inlier fit reported NA,
making the user think the caliper engine had failed when it hadn't.

`CaliperLineResult.ok` is gated by the configurable `cal.min_inliers`
(default 2 for line, 3 for circle). Use it directly:
```cpp
if (r.ok) Report.status = STATUS_SUCCESS;
```

### H7. `cv::imwrite` throws on extensionless filenames → uncaught → process terminates
The `__CACHE_IMG__` family of BPG handlers in `wiringPanel.cpp` called
`cv::imwrite(fileName, img)` directly. When the WebUI sent an
extensionless cache path (`/.../arc_test`), OpenCV can't pick an encoder
and throws `cv::Exception`. The throw is uncaught → `libc++abi` terminates
the whole `visSele` process. The inspd_log drainer catches the crash dump
but the user just sees the app vanish.

**Fix**: wrap every cache imwrite in a defensive helper that
(a) catches `cv::Exception`, (b) auto-appends `.png` if no extension,
(c) refuses empty paths/images. Pattern in `wiringPanel.cpp:safe_imwrite_cache`.
General rule for any `cv::*` API that can throw: catch at the entry point
of every public dispatch handler.

### H8. `convert3Pts2ArcData` CCW-through-pt2 convention must match WebUI
The arc caliper sweeps CCW from `sAngle` to `eAngle`. To make that sweep
pass through `pt2` (the mid-point), `convert3Pts2ArcData` **flips
sAngle/eAngle** when the CCW arc from pt1 to pt3 wouldn't naturally
include pt2 (i.e. when pt2 sits on the long arc). The flip is the magic
that makes the caliper-index order well-defined.

If a consumer (e.g. the WebUI overlay) computes the sweep with naive
`atan2` of pt1/pt3 and goes the short way unconditionally, the per-caliper
status array misaligns with the on-screen boxes by reversal. Port the
exact `angle21 / angle31` comparison:
```cpp
if (angle31 > angle21) { sAngle = a1; eAngle = a3; }
else                   { sAngle = a3; eAngle = a1; }
```

---

## G. Where things live

- Engine: `MatchingEngine/FeatureManager_sig360_circle_line.cpp` (line/arc/spoint report gen,
  parse, `searchPoint_process`, `LineMatching_caliper`, `CircleMatching_ReportGen`).
- Caliper: `MatchingEngine/Caliper.{cpp,h}` (`caliper_measure`, `caliper_locate_line/circle`,
  `profile_to_edge`, strip dump).
- Search point: `MatchingEngine/SearchPointCV.{cpp,h}` (cap-finder).
- Edge toolbox: `MatchingEngine/EdgeSelect.{cpp,h}` (+ `EdgeSelectInfo` quality fields).
- Bridge: `MatchingEngine/CvBridge.{cpp,h}`.
- Report JSON: `MatchingEngine/FeatureReport_UTIL.cpp`; structs in
  `MatchingEngine/include/FeatureReport.h`, `acvImage/include/acvImage_BasicTool.hpp`.
- Harness/headless mode: `Core0_1/wiringPanel.cpp` (`--insp`).
- Debug env: `CALIP_DUMP` (caliper strips), `SPCV_DUMP` (search-point remaps), `SP_PT_DUMP`
  (pt transform chain), `SP_LEGACY_DUMP` (legacy contour search).

## I. search_point: `searchVec` / `barVec` convention

In `FeatureManager_sig360_circle_line.cpp::searchPoint_process`:

- `searchVec` — the scanline direction (axis the scan walks along to
  find the edge).
- `barVec`    — perpendicular to `searchVec`; parallel to the rendered
  width-bar and to the projection line the WebUI's
  `closestPointOnLine` uses.

(Historical naming was `searchVec` / `searchVec_nor` where `_nor`
meant "normal **of** searchVec" — i.e. perpendicular — not
"normalized". Renamed to `barVec` 2026-06-02 because the `_nor`
suffix tripped people up.)

WebUI side (`UI/WebUI/src/UTIL/InspectionEditorLogic.js` search_point
non-anchor case): builds a projection line through `inspAdjObj.x/y`
along `vec = shapeVectorParse(sp)` = bar direction (= core's
`barVec`). `closestPointOnLine(line, def.pt1)` projects pt1 onto that
line. **Anchor shifts along the line direction are cancelled by the
projection**; shifts perpendicular (along `searchVec`) survive.

Consequence — any directional bias like `manual_offset` must be
applied along `searchVec` to be visible. Apply along `barVec` and the
WebUI projection silently eats it.

Side caveat for `SearchPointCV.cpp`: the caller passes `barVec` as the
`searchDir` parameter (legacy). Inside, `s = searchDir` is used as
the row axis with `nS = width` and `nP = 2*margin`. Internally
consistent — but it means SearchPointCV's `s` is the bar axis and its
`perp` is the actual scanline. Don't confuse this when reading
SearchPointCV.

---

## J. Driving a real sorter (uInspESP32 peripheral)

Traps found bringing Core0_1 + WebUI up against a live uInspESP32 machine
(plate + blow selectors + hardware-triggered camera), 2026-08-04. Every one of
these presents as a *different* layer than the one at fault, which is why they
are worth writing down.

### J1. Nothing inspects until an `FI`/`CI` packet arrives — and the failure looks like comms
`ImgPipeProcessCenter_imp()` opens with:

```c
if (bpg_pi.cameraFramesLeft == 0) return;
```

`cameraFramesLeft` is set **only** by the `CI` / `FI` BPG commands
(`wiringPanel.cpp`, `checkTL("CI"…)/checkTL("FI"…)`; absent `frame_count` means
`-1` = unlimited). Until one arrives, every frame is discarded at that first
line: no inspection runs, no verdict is produced, no `report` is sent — and the
board faults with `OBJECT_HAS_NO_INSP_RESULT` because parts reach SWITCH
unanswered. It reads exactly like a dead serial link.

**Two separate things must both be done**, and neither is implied by the other:

1. `InspectionMode` in `data/machine_setting.json` must be **`FI`**
   (設定 → 測量模式 → 全檢). `CI` (檢驗) is continuous streaming for an operator
   placing parts by hand and **does not report to the peripheral**; `FI` (全檢)
   is the triggered mode that does. `觸發檢驗` is the third option.
2. In the UI, pick a 檢測方式 tag and press the ▶ button
   (`.anticon-caret-right`) — that is what actually emits the packet.

Confirm with `DataType_BPG:[FI]` followed by `this->cameraFramesLeft:-1` in
`insp.log`. If you only see `[CI]`, mode 1 is wrong.

### J2. Peripheral traffic is invisible unless you ask for it
`[perif TX]` / `[perif RX]` are gated on `getenv("INSP_PERIF_LOG")`, and the raw
byte tap on `INSP_PERIF_RAW`. Without them you cannot see the serial
conversation at all and will misread silence as "the core never sent it".

Three traps in reading those logs, all of which produced wrong conclusions here:
- `[perif TX]` is logged **before** the write, so its presence does not prove
  the bytes went out.
- `[perif RX] reply=%.120s` truncates at 120 chars — a full ~950-byte
  `get_setup` reply looks identical to a stub.
- The raw tap prints ~11 bytes per line, so grepping it for a string like
  `"0.0.0"` finds nothing. Strip the `[raw NN] ` prefixes and rejoin before
  searching.

Also: **the logctrl shm ring survives process death and `inspd_log` replays
it**, so a fresh run's `insp.log` is polluted with older runs' lines whose
timestamps interleave nonsensically. Use `INSP_LOG_RING_NAME=<fresh>` whenever
you are measuring, or you will chase ghosts.

### J3. `framerate` in `default_camera_setting.json` is a hard trigger ceiling
It was `2`, which turns `AcquisitionFrameRateEnable` on and pushes the camera's
exposure floor to **500 ms** — about 2 captures/s against a machine running ~14
parts/s. The camera silently refuses triggers it cannot service (no error,
`frame_id` stays contiguous), so this surfaces as unexplained NA/UNANSWERED
counts and never points at the camera. `SetFrameRate` treats negative/NaN as
"disable the limiter"; `-1` restores the sensor ceiling (35.18 fps / 28.4 ms at
full-frame 2448×2048 Mono8 on MV-CA050-11UM).

### J4. One global `perifCH`, and CONNECT deletes before it builds
`delete_PeripheralChannel()` runs unconditionally at the *top* of the CONNECT
handler, before the new PHY is even attempted — so a failed connect leaves you
with nothing (`PHYLayer is not able to eatablish`). And `perifCH` is a single
global, not per-client and not per-peripheral-kind, so **any** CONNECT from
**any** WebSocket client evicts the working channel. Two browser tabs open on
the WebUI is enough to make the link flap. Configure exactly one peripheral in
`machine_setting.json`; disable the others by renaming the key (the file's own
convention — `uInsp_peripheral_conn_info1`, `SLID_peripheral_conn_info1`).

### J5. The board owns its config; do not push it back
`Perif_API_Base.machineSetupReSync()` (WebUI) stores the whole `get_setup` reply
and hands it straight back as `set_setup`, stripping only 4 envelope fields — so
`ver`/`name`/`cur_state`/`error_hist`/`cfg_crc`/`reset_reason`/`xtal_mhz` get
pushed at a board that has no use for them. The board persists to NVS and comes
up on it (`cfg_from_nvs`), so the host has no business re-pushing settings just
because it connected. `uInspESP32_API` overrides this to be read-only; note it
must *actively clear* `this.machineSetup`, because `connect()` pushes that field
**directly**, bypassing the override.

### J6. Opening the port reboots the board, so the first command is lost
DTR toggles on `open()`, hard-resetting the ESP32. The host then has a live port
to a board whose UART is not up yet, and the first command out of `connect()`
lands in that window — the board reports it as `recv_ERROR:2` with the frame
mangled a dozen bytes in (`{'type':'get_s<garbage>`). PING survives this **only
by accident**: it repeats every 3 s, so one eventually lands. Any *one-shot*
command at connect is simply lost. `uInspESP32_API` retries the config resync
until it takes (typically succeeds on try 2). A cleaner fix would be to stop
resetting the board on port open, or to wait for its
`system_info "State changed 0 -> 100"` before talking.

### J7. FIFO trigger↔frame pairing needs the two streams 1:1
`ImgPipeProcessCenter_imp` pairs the oldest unclaimed `cam_trig{tid}` with the
oldest frame. That is only correct while frames and parts are 1:1.

**Cause found and fixed: `TriggerSource` was `"Anyway"`.** That tells the camera
to accept a trigger from *any* source including its internal one, so the sensor
free-runs between hardware pulses. It triggers correctly on Line0 as well, so it
passes a bench test and looks settled — but the surplus frames drift the
pairing, and a drifted pairing hands parts their neighbours' verdicts. The
tell is direct: with `"Anyway"` an **idle** machine still produced frames at
~26 ms intervals. Now set to `Line0` with an `"Anyway"` fallback
(`CameraLayer_Aravis.cpp`, both the ctor's inline setup and `TriggerMode()`).

Measured at `plate_freq` 3000, before → after:

| | "Anyway" | `Line0` |
|---|---|---|
| frames vs parts | 449 vs ~213 (2×) | 103 vs ~99 (1:1) |
| "frame with no pending trigger" | 273 | 1 |
| `perif trig` pending | 4, drifting | 1, steady |
| report latency (avg) | 844 ms | 348 ms |

**Second cause, and the one that actually mattered: `SetFrameRate` rejected the
"no limit" sentinel.** The WebUI's `setCameraSpeed_HIGHEST()` sends the *finite*
value `9999999` (`BPG_Protocol.js`); `SetFrameRate` handled `isinf` but returned
`NAK` for anything else above max — so the request was refused and whatever
limit was already in force stayed. On the real machine something pushed
`framerate:8` at inspection start, the `9999999` that immediately followed was
rejected, and the camera ran the whole production test **pinned at 8 fps
(125 ms exposure floor)** while parts arrived far faster. Now "faster than max"
disables the limiter, and says so.

With both fixes in (Line0 + the sentinel), measured at `plate_freq` 3000 over
309 parts:

| | broken | fixed |
|---|---|---|
| ResultingFrameRate | 8.00 fps | 35.18 fps |
| exposure floor | 125 ms | 28.4 ms |
| report latency avg | 348 → 929 ms, growing | **162 ms, flat** |
| report latency max | 4803 ms | **218 ms** |
| `perif trig` pending | 1 → 5, growing | **0, steady** |
| cam_trig vs frames | ~2:1 | 514 (~257 parts) vs 261, **1:1** |
| faults | `err[2]` | none |

**What that latency actually measures:** the firmware stamps `trig_us` where the
object is *registered at the gate* (next to `gate_pulse`, before the stage tasks
are scheduled) — despite the name it is **not** the camera trigger. So the
number is gate→report, and it contains a pure-transport term: `CAM1_on` is 654
ticks after the gate and the stage timer runs at `2 × plate_freq`, i.e. **109 ms
at plate_freq 3000**. The vision loop proper is therefore ~54 ms avg / ~109 ms
max, which cross-checks against the core-side parts: cam_trig→frame ~29 ms,
inspect 7–14 ms, report round trip ~8 ms.

That split matters for scaling, because only one half moves with plate speed.
At production `plate_freq` 15000 the transport term shrinks to 654/30000 =
**21.8 ms** while the vision term does not change, so expect roughly
**21.8 + ~109 = ~131 ms against the 996 ms budget (~7.6× margin)**. Comparing
against SWITCH is apples-to-apples: that deadline is also measured from the gate
(`pressure = gate_pulse + SWITCH - SYS_STEP_COUNT`). Inspection itself was never
the bottleneck — 7–14 ms per frame with `insp:0/10`.

**Still worth doing:** the pairing is still positional. The existing guard retires surplus triggers only on **`frame_id` gaps**, which
by construction cannot see a trigger the camera silently *refused* — `frame_id`
stays contiguous in that case (see `cameralayer-validation`). So FIFO position
alone can never be made safe here. The fix is to stop inferring the pairing:
either carry the tid through explicitly, or reconcile against the camera's own
Line0 rising-edge event count (the HikRobot layer already accounts for exactly
this via `_line0RisingEdges`; the Aravis layer does not yet).

### J8. A def-less WebUI page can receive every frame and draw none of it

The camera-calibration page (相機校正) streamed nothing while the core was doing
everything right. The failure has a specific shape worth recognizing, because
nothing in it logs.

**The end-to-end path is fine right up to the last step.** The core encodes the
frame, routes it to the pgID the page registered, and hands it to a real
subscriber; the socket delivers it; the demux tallies it; the reducer stores it
in `edit_info.img`. Then `Preview_CanvasComponent.draw()` returns on its first
line, because `db_obj.cameraParam === undefined`.

`cameraParam` only ever arrives from a def's `cam_param` or from an inspection
report that carries one (`UICtrlReducer` `sig360_extractor` /
`sig360_circle_line`). A page that deliberately loads **no def** — calibration,
lens aiming, any raw-frame preview — can therefore never satisfy that guard. And
because the guard `return`s rather than throwing, the symptom is a fully
transparent canvas with a **silent console**: no error, no warning, no
`[demux] untracked packet`. Every layer reports success.

`scaleImageToFitScreen` has the same def-shaped assumption one level down: it
fits to `inherentShapeList[0].signature.magnitude` (the editor wants the *part*
filling the view, not the sensor). `edit_info` still holds whatever def was
loaded last, so a def-less preview gets zoomed to a stale, unrelated scale even
once it does draw.

**The scale is not a detail to fall back on.** A raw-frame preview has three
spatial inputs, and in the def path *all three* come from the def: mmpp from
`cam_param`/sig360, the origin from the sig360 centre, and the fit extent from
the signature magnitude. For an instrument page every one of them is the wrong
number — a def's mmpp describes whatever image was side-loaded *with that def*
(different camera, lens, standoff), and there is no part to centre or frame. The
authority is `lens_calib.json` `um_per_px`; the def's `mmpp` is only ever about
the sideloaded image.

`Preview_CanvasComponent.SetStandalonePreview(mmpp)` takes all three out at
once: instrument mmpp in, `EditDBInfoSync` becomes a no-op, `db_obj` and
`edit_DB_info` stay null, origin becomes the frame's own centre. Default is off,
so the def editor is unchanged — drawing overlays at a guessed mmpp is exactly
what the original guard exists to prevent.

The centring is not cosmetic either: `draw()` puts the frame's top-left at the
world origin and relied on the sig360 translate to centre it, so without an
explicit offset the frame slides off-centre the moment the canvas resizes
(opening the side panel is enough).

**Bisecting this class of bug.** Three hypotheses died before the real one
(`TriggerSource=Line0` starving frames; a promoted peer losing its stream
subscription; the full-res payload overrunning the socket) because "the core
sent it" and "the page drew it" had no observable between them. Count packets at
**both** ends:

- core: `img transfer(DL:%d) %fms pgID:%d subscribers:%zu` at the send site —
  `pushToSubscribers` is a fan-out, so sending to an empty list is otherwise
  indistinguishable from not sending
- WebUI: `__GP_WS__.rxTally()` (inbound, keyed `pgID:type`) and
  `__GP_WS__.reqWindowIDs()` (which pgIDs are registered);
  `__GP_CALIB_CANVAS__.rUtil.get_mmpp()` reads back the scale actually in use

`{"10105:IM": 102}` with `edit_info.img` set and a transparent canvas localizes
the loss to the draw call in one step.

### J9. Clearing a device fault does not clear the host's half of the pairing

`report{tid,cat}` is only meaningful while both ends agree on which objects
exist. The firmware wipes its object ring (`RESET_ALL_PIPELINE_QUEUE`) in three
places — `clear_error`, entering `INSPECTION_MODE_ERROR`, and entering `IDLE` —
and `tid_counter` keeps counting across all of them. So a stale tid never
collides with a live object; it matches **nothing**, and the firmware faults
with `INSP_RESULT_MATCHES_NO_OBJECT` (err=1) the instant a report names it.

The core's `perifTriggerQueue` used to be drained only on CONNECT. That made
"clear the error and press continue" fail deterministically:

1. machine faults, device ring is emptied, ids continue from (say) 173
2. the core is still holding 169–172 — it was never told
3. first frame of the restarted run pairs with 169
4. `report{tid:169}` → no such object → err=1, immediately

The only escape was a reconnect. Note the operator sees a *different* error code
from the one they just cleared, which reads like a new problem rather than a
leftover.

Fixed on both sides, and the RX side is the one that matters:

- **RX (`tap_device_state`)** — watch the device's `state` for 112/100. This is
  the reliable half, because the device also faults on its own, and because
  `clear_error` travels straight from the WebUI to the board over the peripheral
  passthrough: the core is never consulted and would otherwise never learn.
- **TX** — also act on `clear_error` / `enter_insp_mode` / `exit_insp_mode` as
  they pass through, since the RX tap only fires at the next status poll (up to
  a second later) and the plate can already be running by then.

Both call `forget_pending_triggers()`, which drops queued triggers *and* the
results computed for them. In-flight frames then report `tid=-1` and are logged
and dropped — no fault.

**Related, still open: the deadline is in pulses, the latency is in
milliseconds.** `pressure = gate_pulse + SWITCH - SYS_STEP_COUNT` is a pulse
budget, so raising `plate_freq` shrinks the wall-clock time to answer
proportionally, while the host's report latency does not change at all. A run
that is comfortable at 3000 can fault on the objects already in flight the
moment the speed is raised — observed at 494 ms after a `set_setup`, as err=2
`OBJECT_HAS_NO_INSP_RESULT`. Compounding it, `perifTriggerQueue` carries a
standing backlog whenever the camera silently refuses a trigger (102 announced
vs 100 frames over 35 s, `pending` 2→4, `missed(NA):0`): the frame-gap guard
cannot see those because `frame_id` stays contiguous (see J7), so every report
runs `pending × object_period` late — 1.4 s at the observed rates, against a
2.49 s budget. Until pairing is evidential rather than positional, drain the
in-flight objects before changing plate speed.

### J10. The pairing is moving to the device; the host's half is scaffolding

Which camera frame belongs to which physical part is the single point where a
wrong answer becomes a mis-sorted part. The core currently answers it in
`Core0_1/PerifTriggerPairing.hpp` by matching the camera's frame timestamp
against the device's trigger timestamp, having estimated the offset between the
two clocks.

**That whole module is on its way out.** The device fired the trigger, so it
knows the object and the instant; it announced the instant and then discarded
it. Bootstrap, drift EWMA, staleness sweep, offset TTL, failure-driven resync,
early frame dump and the idle heartbeat all exist to reconstruct one value the
firmware threw away. The firmware now keeps it (`pipeLineInfo.cam_us`) and
reports carry `cam_ts`, so matching happens where the ground truth is.

Migration is deliberately evidence-driven rather than argued: reports carry
**both** `tid` and `cam_ts`, the device matches by tid and simultaneously
computes what the timestamp match would give, and counts agreement. Promotion
is one config flag (`report_match_ts`) justified by `cam_sync.disagree` staying
0 over real production traffic. Until then the new path is a permanently-running
observer, which is a far stronger check than any one-off A/B.

Full status, measured numbers and the tooling traps that produced several wrong
conclusions along the way: `Peripheral/uInspESP32/docs/PAIRING_MIGRATION_STATUS.md`.

**Two host-side facts worth keeping even after the deletion:**

- `gate_pulse == 0` on a `cam_trig` means the device fired the camera directly
  (`trig_cam_pulse`) with no pipeline object behind it. Reporting a verdict
  against one faults the device with `INSP_RESULT_MATCHES_NO_OBJECT`. The core
  marks these `sync_only`: paired for the clock, never reported.
- Opening the peripheral serial port toggles DTR and hard-resets the ESP32, so
  every core restart reloads the board from NVS. Runtime-only settings are lost
  — which is how `unanswered_policy` silently reverted to 0 mid-session and left
  the machine running at 18.8% unjudged with an empty error log.

### J11. The camera reconnect was a self-sustaining crash loop

`Core0_1/` accumulated **80+ `crash_*.dump` files across 2026-08-05/06** — one
every 10–30 minutes, five in eight minutes at one point. It was not random, and
it needed no human. Three separate defects closed a loop:

**1. A failed camera open silently became a fake camera.**
`camera_ez_reconnect` ended with `camera = getCamera(0); //Fallback BMP test
folder`. When the real camera could not be opened, the core installed a
`CameraLayer_BMP_carousel` reading `data/BMP_carousel_test/` — and said nothing.
The WebUI showed a picture, so everything looked alive. It was
`snap_2026-06-04_08-53-52-042.png`. Lens calibration, exposure setup and clock
calibration all ran against a two-month-old still and failed for reasons that
made no sense. Frame size is the tell: the real camera was 496x416, the
substitute 2448x2048.

**2. The WebUI auto-reconnects on exactly that condition.** `script.jsx` ~855:

```js
if (cam0 === undefined ||
    (System_Setting.ALLOW_SOFT_CAM == false && cam0.includes("CameraLayer_BMP")))
  isInOperation = false;
if (camInfo[0].cam_status != 0) isInOperation = false;
if (!isInOperation) { ... this.reconnection(); }   // fires camera_ez_reconnect
```

`ALLOW_SOFT_CAM` is false on this machine. So a failed reconnect installed a
soft cam, which the UI read as "camera lost", which fired another reconnect.
**Failure guaranteed the next failure.**

**3. Reconnect was a use-after-free.** The body was `delete camera; camera =
NULL; camera = getCamera(1);` — no `StopAquisition()`, no drain, no lock — run
from the BPG thread while `ImgPipeProcessThread` was mid-frame holding the same
pointer (`ImgInspection` takes it as `cam` and hands it to the matcher via
`bacpac->cam`). `crash_20260806T035429Z.dump` ends:

```
~CameraLayer_BMP_carousel Descructor...
FeatureMatching ... this:0xb0b024000
ImgInspection 7.942000ms
=== SIGSEGV ===
```

**Fixed** (2026-08-06): a `camera_lifetime_lock` held for the whole of one
frame's inspection and by anyone destroying the camera; `StopAquisition()`
before `delete`; a 3-second floor between reconnects; and **no BMP substitution
on the reconnect path** — a failed reconnect now leaves no camera at all, and
every reader is NULL-guarded. A fake camera that looks live is worse than none.

### J12. "Is the camera connected?" had one answer, and it was always yes

`CameraLayer_Aravis::isInOperation()` was:

```cpp
{//TODO: check availability
    return CameraLayer::ACK;
}
```

`getCameraJsonInfo()` returns `cam_json_info`, a string built at construction.
And Core0_1's three camera callbacks all begin `if (type != EV_IMG) return NAK;`
— so `EV_CTRL_LOST`, which the Aravis layer does emit, was delivered to nobody.
(Only CoreHub handles it, `InspectionTarget.cpp:945`.)

Consequence, reproduced by the user: **unplug the camera, reload the WebUI, and
the panel still shows its id, model and serial.** Identity was cached, health
was hardcoded, and the disconnect event was dropped — three layers agreeing on
a camera that was not physically attached. Restarting the core was the only way
to make the system ask again.

**Fixed** (2026-08-06): `isInOperation()` checks a `_ctrl_lost` flag (set by the
control-lost handler) and otherwise performs a real register read
(`arv_camera_get_integer(camera,"Width",&error)`), latching the failure;
`camera_info` gained a `present` field. Note the ordering hazard: making this
honest *without* J11's fix would have converted an occasional manual crash into
an automatic one, because the UI auto-reconnects on `cam_status != 0`. The two
had to ship together, and did.

### J13. The log system is not trustworthy, and it cost a day

On 2026-08-06 every piece of real evidence came from somewhere other than the
log: a raw TCP tap on `INSP_PERIF_CONSOLE`, crash dumps, and a hand-rolled
websocket client. Twice a wrong conclusion was drawn from "it is not in the
log", which only ever meant the log did not arrive.

Four separate problems, all still open:

1. **`persist` is OFF by default.** Logs live in a 16MB shm ring and nowhere
   else, so anything not read live is gone at restart. Workaround in use:
   `INSP_LOG_PERSIST_LEVEL=info INSP_LOG_DIR=/tmp/insplog INSP_LOG_FILE=insp`.
   That should be the default for a machine under development.
2. **The `inspd_log` drainer can die silently.** Observed: it started, bound
   4091, and vanished; port closed, WebUI "Core Logs" empty, and nothing
   anywhere said so. The core never notices its own drainer is gone.
3. **Port 4091 serves one client at a time**, and a second connection hangs in
   the opening handshake rather than being refused. Two people (or a person and
   a tool) looking at the same machine block each other with no diagnostic.
4. **Long records are corrupted in transport** -- already documented at
   `wiringPanel.cpp` (the reason `INSP_PERIF_CONSOLE` exists at all). A
   ~1kB `get_running_stat` reply is exactly the size that loses its tail.

The crash dump is the one part that works: it writes the entire retained ring,
which is how J11 was solved.

---

## J14. `serialRTT` in the perif log is NOT a round trip

`wiringPanel.cpp:668` computes it as:

```cpp
long rtt = last_tx_us ? (long)(perif_now_us() - last_tx_us) : -1;
```

That is *time since the host last transmitted anything*, which is a round trip
only when the line being logged is the reply to that transmission. Announcements
(`cam_trig`, `report` acks) are sent by the device unprompted, so for them the
number means nothing at all — it is just how long ago the last poll went out.

Measured on 2026-08-07 over a 214 s window at 10 objects/s:

```
主動宣告 (no "id")   n=2149  median 1527 ms   <- meaningless
命令回覆 (has "id")  n=122   median   19 ms
  pong                n=72   median  9.6 ms   max  106 ms
  get_verdict_log     n=16   median 1760 ms   max 2980 ms
  get_running_stat    n=34   median 1518 ms   max 2751 ms
```

This cost a wrong conclusion twice in one night: "the link is saturated,
serialRTT 1.4–2.9 s" was quoted as the strongest argument for moving to 230400
baud. It is not congestion. Two independent measurements say so:

- **Wire utilisation is 8.4%.** Device→host traffic is 969 B/s (2271 replies,
  91 bytes mean, 99.6% complete in the log) against 11520 B/s of 115200 8N1
  capacity.
- **Small replies are fast.** `pong` answers in 9.6 ms median.

What *is* real is that **large replies take ~25x their wire time**: a
`get_verdict_log` reply is ~800 bytes, which is 70 ms on the wire, and it
measures 1760 ms. Doubling the baud rate would take that 70 ms to 35 ms and
leave the other 1690 ms untouched. The thing to chase is the per-read chunking
or throttling, not the bandwidth.

It matters beyond the test rig: `get_running_stat` is what the WebUI panel
polls, and a 1.5 s answer to it is a panel that feels stuck.

To read the log correctly, split on whether the reply carries an `"id"` — every
reply echoes the request's id (`LegacyFirmware.cpp:4191`), so its presence is
exactly the "was this solicited" test.

### J14 addendum — what 230400 actually changed (2026-08-07)

The paragraph above concluded "large replies take ~25x their wire time; chase
the chunking, not the bandwidth." Measured after the move to 230400, that
conclusion does not survive:

```
                     ping          get_running_stat
idle                 5.7 ms          65.4 ms
under 30 Hz load    10.9 ms          66.3 ms      <- unchanged by load
idle again           5.8 ms          66.3 ms
```

`get_running_stat` is 66 ms flat, load or no load. At 115200 the same reply
measured 1518 ms median. A 2x baud change producing a 23x improvement is not
explained by wire time, and it is not explained by contention either — the load
column above is flat. **What the remaining factor is has not been established**,
and the honest position is that it is unknown rather than any of the stories
tried so far.

What IS settled:

- 66 ms for `get_running_stat` is fine for a 1 Hz WebUI poll. Whatever the
  1518 ms was, it is gone.
- `ping` roughly doubles under load (5.7 -> 10.9 ms). That is real contention,
  and it is small.
- The move to 230400 was still worth making, but the reason given for it at the
  time (congestion) was wrong on the evidence, and the benefit turned out to be
  somewhere other than where it was predicted.
