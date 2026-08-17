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

---

## K. `intrusionSizeLimitRatio` is gone (2026-08-07)

The binary-path gate in `FeatureManager_group::FeatureMatching` — "if the
cage-connected blob exceeds `ratio * image area`, raise
`EXTERNAL_INTRUSION_OBJECT` and inspect nothing" — has been removed, along with
its WebUI slider, its redux action/reducer, its `edit_info` default and its def
emission. `obj_detect` clean-space regions replace it.

**Why it had to go rather than be retuned.** It was one number for the whole
def. It could only ever say "something somewhere in this image is too big", so
the only available response was to throw the entire frame away — no way to say
*where* clean matters, no way to distinguish "a speck landed next to the part"
from "the part itself has residue", and no way to choose between rejecting the
part and distrusting the measurement. A clean-space region says all of that per
region, in mm², with a per-region `on_fail`.

**The default was a trap.** `intrusionSizeLimitRatio = (val != NULL) ? *val : 0`
— a def *without* the key got ratio 0, which makes `CLimit` 0, which rejects
every frame with any intruding area at all. Every working def therefore had to
carry the key. That is why `DefConfUI` wrote `deffile.intrusionSizeLimitRatio=1`
in four separate places before sending a def anywhere: not a policy, a
workaround for a default that could not be left alone.

**Old defs are unaffected.** The key is no longer read, and an unknown key is
ignored, so a factory def loads byte-identically and simply stops being gated.
Nothing needs to be edited on disk.

**`EXTERNAL_INTRUSION_OBJECT = 3` stays in the enum**, reserved and never
raised. Deleting it would shift every code after it under anything holding an
old report.

Measured, same image, same def, old binary vs new:

```
golden def (ratio 0.1)     before 7395 B    after 7395 B   byte-identical
same def forced to 0.0     before error=3, 251 B empty     after error=0, 7355 B
```

**What you lose.** A sig360 def that was relying on this to refuse contaminated
frames now has no such guard until someone places an `obj_detect` region with
`dark_thresh` + `dark_area_max` on it. The replacement is strictly better but it
is opt-in, and the gate was not. Which defs need a region is a per-def call.

## L. The station region applies in FI only (2026-08-07)

`inspection_region` in `machine_setting.json` filters located objects to the
station. It is now enforced **only in FI sessions** (`"FI"`, hardware-triggered
full inspection). In CI (`"CI"`, free-run, which is what the editor uses) the
region is published onto the bacpac as zero-size, i.e. the same "no region
configured" path a machine without one takes.

**Why.** The region describes where a part stands when the machine fires at it.
In CI nothing is driving the machine: you are dragging a def around, checking a
light, looking at the plate. Filtering there hides objects for a reason
unrelated to what you are doing — and it hides them *while you are drawing the
very box that hides them*, so the box cannot be placed by watching its effect.

**What still shows.** The geometry is sent in the report in both modes, so the
overlay draws the box during setup. `station.region.active` says which mode you
are in, and the canvas draws the region **dashed** with `檢驗區域(設定中·未過濾)`
when it is false. A solid box that is not selecting anything looks exactly like
one that is, until a part goes the wrong way.

**The offline harness is unconditional.** `--insp` applies the region with no
mode check: it *is* a full inspection of one frame, with no editor to get in the
way of. `qa_insp_region.py` therefore still covers the filter itself; it does
not cover this gate.

**The session start says which it got** — `insp session: FI -- station region
ENFORCED` / `... CI -- station region off (setup view shows everything)`, logged
only when a region is configured.

## M. "Take a new image" worked once, then stopped (2026-08-07)

DefConfUI's 立即 (BPG `EX`, `trigger_type:0`) fired a software trigger and often
came back 圖像獲取失敗 — intermittently, which is what made it look like a
camera problem. It was two independent bugs, both in `CameraLayer::SnapFrame`.

**1. One successful snap stops acquisition.** `TriggerCount(1)` sets the burst
counter `takeCount`; when the frame arrives `STREAM_NEW_BUFFER_CB` decrements it
to 0 and calls `arv_camera_stop_acquisition()` (`CameraLayer_Aravis.cpp:644`).
`SnapFrame` never started it again, and `TriggerMode()`'s stop/write/start block
only restores acquisition `if (burst_was_running)` — which is false precisely
because the previous snap turned it off. So a software trigger went out to a
camera with no running stream and no frame could ever come back.

That is the "sometimes": the first press after **anything** that starts
acquisition (a CI session, `camera_ez_reconnect`, an ROI or exposure change)
succeeds and re-arms nothing. Every press after it waits out the full timeout.

Fix: `SnapFrame` calls `StartAquisition()` before triggering. Idempotent on
Aravis (immediate ACK when already streaming) and a no-op elsewhere, so the
paths that were already correct — FI, which streams — pay nothing.

**2. The snap timeout belonged to no snap in particular.** The wait was
`conV.wait()` guarded by a *detached thread* that slept `timeout_ms` and then
set `snapFlag=-1` if it found `snapFlag==1`. `snapFlag` is shared by every snap
and carries no identity, so a snap that finished early left a live timer that
aborted whichever snap was in flight when it expired.

Measured, 3s timeout, presses 400ms apart: snap 1 succeeded in 276ms, snap 2
died at **2528ms** — 3s after snap 1 started, not after snap 2. The UI sends
`timeout:-1`, clamped to 30s, so every press armed a 30-second landmine for the
next one.

Fix: `conV.wait_for(lock, timeout, pred)`. The deadline cannot outlive the wait.
`SnapAbort()` is kept as public virtual API but is no longer used here.

**Measured, same machine, same camera:**

```
before   0/10 ACK   every press timed out (after the first, which had been
                    re-armed by a reconnect); failures at 2528/2530 ms, i.e.
                    on the PREVIOUS press's deadline
after   10/10 ACK   173-228 ms each, GAP 150ms..400ms, timeout 3000 and -1
```

`UI/WebUI/tools/webctl/snap_probe.mjs` is the harness — it presses 立即 N times
over the same wire the browser uses and reports ACK + whether an IM actually
came back. This class of bug is invisible from the UI (you retry, it works, you
move on) and obvious in one run of that.

**Still true, not fixed:** an `EX` snap during an FI run would stop the FI
stream after its one frame (same `takeCount` path). Pre-existing, and now
self-healing on the next snap, but the two should not be used together.

## N. The caliper clamp is in def-mm; the cost it bounds is in px (2026-08-07)

**Deferred, not fixed.** Written up because it is invisible from the outside and
the ten QA cases it fails are now marked KNOWN, which is how a deferred defect
stops being a discovery and starts being a decision.

`FeatureManager_sig360_circle_line.cpp:275-277` (arc) and `:1622-1624` (line)
clamp pathological caliper sub-params:

```cpp
if (cir.cal_count  >  512) cir.cal_count  =  512;
if (cir.cal_width  >   64) cir.cal_width  =   64;
if (cir.cal_length >  256) cir.cal_length =  256;
```

with the comment "even at these caps worst-case per primitive is ~10^8 ops ->
completes in ~1s". That arithmetic only works if the numbers are pixels. They
are **def-mm**, and both paths correctly multiply by `ppmm` before the
measurement loop (arc at `:4329` inline, line at `:4876` earlier — the
conversion itself is right, that was checked). At this machine's
`ppmm ≈ 113 px/mm` the clamped ceiling is

```
width  64 mm  ->  7,226 px      the whole image is 2,592 x 1,944 px
length 256 mm -> 28,900 px      i.e. 2.8x the image wide, 11x long
512 * (2*7226+1) * 28900        ~2e11 ops per primitive, not 1e8
```

**A guard whose ceiling is ten times the image bounds nothing.**

**Measured** (golden def, 4 arcs / 7 lines, `--insp`):

```
caliper (count/width/length, def units)   arc      line
16 / 2 / 16                              0.29s    0.18s
32 / 4 / 32                              1.16s    0.18s
64 / 8 / 64                              7.93s    0.35s
128 / 16 / 64                           31.61s    0.53s
512 / 64 / 256  (== the caps)          TIMEOUT   10.99s
4096 / 2049 / 4096 (over the caps)     TIMEOUT   11.67s
```

At-cap and over-cap take the same time, which is the proof that **the clamp
does fire** — it just clamps to something unusable. The arc/line gap is not a
second bug: line's cost saturates because its scan lines run off the image and
get clipped, while the arc's radial scans stay inside it.

**Impact is bad-input only.** Real defs run `count 10 / width 0.5mm` — tens to
hundreds of times below any cap — so nothing measured on the machine is
affected. The cost is that a typo (0.5 -> 500) stalls the core for tens of
seconds per frame with no error code, on a machine that runs continuously.

**The fix when it is time:** clamp *after* the mm->px conversion, or better
clamp the work directly —
`ops = count * (2*length_px/step + 1) * (width_px + 1) <= budget`, shrinking
count then length. Measured throughput is ~4.3M samples/s, so a budget of ~2e6
puts a primitive at ~0.5s. Any such change should leave the ten KNOWN cases in
`qa_measure.py` green and the golden byte-identical.

## O. The golden angle judge drifted 2.7% since 2026-05-30 (2026-08-07)

**Open, unattributed.** Recorded because it was found by accident and would
otherwise be found again the same way.

Golden sample `10221 BOS-LT12BH4211 SORTING_bk`, same def, same image:

```
judge  subtype    2026-05-30   2026-08-07     drift
  8    distance    8.601038     8.600740     0.003%
 12    distance    8.468927     8.469253     0.000%
 13    distance    8.601037     8.600742     0.003%
 14    angle       2.792538     2.716893     2.71%   <---
```

The three distances are unchanged to a part in 30,000. The one **angle** moved
2.7%. Lines 1 and 2 (its operands) carry no `locating` key, so this is the
**legacy line fit**, not the caliper path — the caliper rework is not the
suspect. 98 `MatchingEngine/` commits sit in that window and it has not been
bisected. Candidates by shape: labeling (the OpenCV `connectedComponents`
drop-in changes which boundary points exist and their order), morph/WLS, the
primitive-locating edge selector.

It may well be a fix rather than a regression — an angle is far more sensitive
to which points feed the fit than a distance is, so a better boundary would
move exactly this and leave the distances alone. Nobody has established which.

**How it surfaced, which is the part worth keeping.** `qa_calc` had these four
values *hardcoded* as CALC operands. When the measurement moved, eight of
fourteen property trials reported "the CALC evaluator computed the wrong
answer" — a failure pointing at a module that was working perfectly. The
operands now come from a golden run at test time, and the drift has its own
case, `golden_operand_drift_since_20260530`, marked KNOWN with this section as
its reason. Delete that KNOWN entry the moment the drift is explained.

**A test that hardcodes another subsystem's output has quietly become a test of
that subsystem, and it will report its findings under the wrong name.**

### O addendum — why judge 14 is the fragile one (2026-08-07, same day)

Not ill-conditioning of the angle, and not a setup or transform error. **Its two
operands are different kinds of line.**

```
line 2   matching_pts 525   fitted along the whole 4.65 mm edge
line 1   matching_pts   2   "vertex_touch_searching": true
```

In that mode the line is defined by where it **touches** the contour at its two
ends, and `lf.matching_pts = 2` is a hardcoded mode marker
(`FeatureManager_sig360_circle_line.cpp:~2700`), not a count of weak support.
The mode also runs deliberately looser gates: `lineCurvatureMax` 10 vs 0.15,
`cosSim` 0.3 vs 0.9.

A line through two touch points rotates when either point moves a pixel, and the
judge is a 2.79 deg angle, so that rotation arrives at full size on a small
number. That is the mechanism behind both observations: 11.2% under WebP
recompression while every distance held to 0.04%, and 2.7% across the code
window above.

**Ruled out on the way, each by measurement, and each of them was my first
answer:**

| suspicion | check | result |
|---|---|---|
| def line 1 placed off the edge | perpendicular scan along nominal | edge within 0.05 mm, 200/200 scanlines |
| object transform / frame error | 4 arc centres, def+`(cx,cy)` vs fitted | agree to 0.065 mm, spread < 0.5 px |
| line fits drifting from nominal | all 3 lines, report frame only | within 0.07 mm |

The first two "findings" were artifacts of **my own** image registration: the
origin came from the centroid of all dark pixels, and this image has a dark
border strip that pulled it ~1 mm. Excluding an 80 px border moved every
conclusion. A registration you derived yourself is a measurement instrument, and
it needs its own witness before its output is evidence — the arcs were that
witness and they were available from the start.

## PerifSendThread 被餓 1252ms,而 USER_INTERACTIVE 擋不住(2026-08-10)

五小時 soak 的第 8 次嘗試,1.3 小時內停機三次,全部 `error_hist [1]`
(`INSP_RESULT_MATCHES_NO_OBJECT`)。因果鏈是清楚的:報告晚到超過
CAM→SWITCH 預算 → 該顆被掃成 UNANSWERED → 遲到的報告再也對不到物件。

**不是線路。** 98 筆 `perif tx stall` 的分佈:

```
min 20.2  p50 33.1  p90 94.2  max 374.3 ms
超過 792ms 預算者:0
```

**不是 TSQueue。** `pop_blocking` 在 mutex 下用 predicate 等待、`push` 在 mutex 下
notify,不會遺失喚醒。而且 core 自己就量了生產者側:

```
perif WAIT SPIKE 1252.4ms: idle_before 1281.9ms, depth_at_pop 46, write 0.21ms
  push_max 0.380ms -- producer never blocked -> notify was prompt,
                      consumer was NOT SCHEDULED
```

佇列裡躺著 46 筆,執行緒閒置 1281.9ms,真正寫出去 0.21ms。

**而 QoS 提升早就做過了,而且不夠。** `PerifSendThread` 進入時已經是
`pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0)`,那是上一輪針對
量到的 **216ms** 間隙做的修正。同樣一條執行緒現在被餓 **1252ms**——
**所以「再調高優先權」這個方向已經走到底了,不要再往那裡加。**

尖峰當下的負載狀態:

```
waterLvL: insp:0/10  dview:10/10  snap:0/5   poolSize:18
ImgInspection 29.377ms (CPU time)     <- 穩態是 7ms
```

預覽佇列滿載、檢驗 CPU 時間四倍。有一陣 CPU 爆量,而只需要 0.21ms 的發送執行緒
排不進去。

### 未解,以及一個必須先排除的干擾

**這三次尖峰的歸因目前不可信**,因為量測期間同一台 Mac 上還跑著本次調查自己的
分析(15MB dump 的反覆 grep、建置)。halt 2 的時刻與其中一次分析重疊。
**把自己造成的排程壓力當成受測系統的性質,是這類量測最容易犯的錯。**

下一步順序:

1. 同條件重跑,期間主機保持安靜(只讀已落地的小檔)。尖峰若消失,上面三筆的
   歸因整個重寫;若仍在,才是機器自己的。
2. 若仍在:預覽關掉(`NO_STREAM`)做 A/B。`dview 10/10` 指向預覽編碼是主要的
   競爭者,而它不在關鍵路徑上——真要修,方向是讓它讓路,不是讓發送執行緒插隊。

目標平台是 Windows,但這次不能推給平台:被餓的是一條已經拿到最高 QoS 的執行緒,
競爭者是我們自己的影像路徑。

### 量出來了:兩條消費者執行緒一起凍住,而且都沒燒 CPU(2026-08-10, run 11)

上一節留了兩個推論。加上儀表之後(`self_cpu_over_gap` 用
`CLOCK_THREAD_CPUTIME_ID` 量發送執行緒自己的 CPU;`dview_beat_age` 量預覽執行緒
上次繞回迴圈至今多久),兩個都變成量測:

```
wait      self_cpu_over_gap   dview_beat_age    判讀
 149.8ms      0.08ms            177.4ms         預覽也停
 234.9ms      0.07ms              0.5ms         預覽照常
 250.3ms      0.09ms            284.1ms         預覽也停
 278.6ms      0.05ms              0.1ms         預覽照常
1372.6ms      0.11ms           1391.8ms         預覽也停
```

**一、`self_cpu_over_gap` 每一筆都是 0.05–0.11ms。** 跨越 1372ms 的空窗只燒了
0.11ms CPU,所以「它在跑、只是忙別的事」**排除**。

但這個量**不能**區分「可執行卻沒被排到」和「阻塞在一把鎖上」——**兩者都不燒
CPU**。初稿在這裡寫成「卡在別的東西上已排除」,是過度宣稱,已更正。
`consumer was NOT SCHEDULED` 仍然只是推論。

**二、造成停機的那次,預覽執行緒停了 1391.8ms,發送執行緒 idle 1391.3ms。**
同一個區間、幾乎同樣長度。**兩條一起凍住,不是互搶。**

這兩條執行緒設計上完全解耦:各自的佇列、各自的 mutex、非阻塞、滿了丟最舊。
它們同時停擺,代表原因在應用層以下。

**推論鏈上被判死的東西:**

- `USER_INTERACTIVE`(以及任何執行緒優先權調整)。餓死的不只一條執行緒,
  提高其中一條的優先權不會讓兩條都被排到。這次是**有證據**判死,不是推測。
- 「預覽搶 CPU」。A/B 已經否定(`--no-stream` 跑 40.5 分鐘照樣停機),
  這裡再補一刀:預覽自己也是受害者。
- 「`dview 10/10` = 預覽很忙」。佇列滿代表**消費者落後**;那個消費者當時
  根本沒在跑。**這是整條調查裡最貴的一次誤讀。**

**還有一件事不要混為一談:** 234.9 / 278.6ms 那兩筆,預覽是照常跑的
(beat age 0.5ms / 0.1ms)。小尖峰(一般排程抖動,都在 792ms 預算內、無害)
和大尖峰(兩條一起凍、造成停機)**是兩種現象**。把它們當成同一件事的不同大小,
會找錯方向。

**下一步要量主機側**,而不是再改核心:行程的 RSS、major/minor page fault、
系統記憶體壓力,以 1Hz 取樣後和尖峰時刻對照。候選是分頁/記憶體壓力、
配置器的 stop-the-world、或行程層級節流——目前都只是候選。

### 回收 pipe 插槽時 `report_json` 沒被釋放 —— 每幀漏一棵 cJSON 樹(2026-08-10)

`image_pipe_info_gc` 把插槽還回 `resourcePool` 時**沒有釋放
`datViewInfo.report_json`**,而 `resourcePool::retResrc` 只是把 flag 翻回 0
(不呼叫解構子、不釋放任何東西)。插槽重新發出後 `ImgPipeProcessCenter_imp` 直接
覆寫指標,舊樹失聯。**唯一有刪除的是內聯非 pass-down 分支,量產路徑幾乎不走。**

量到的規模:88.9M 筆存活配置 / 3.82 GB / 平均 42.9 bytes,尺寸只有兩個峰
(64B × 48.5M = cJSON 節點,16B × 40.0M = 它的字串)。36.5 幀/秒下每幀約 553 個
節點、約 92 MB/分鐘,40 分鐘達 3.46 GB。修正:`635865bc`。

**傷害不是「用了很多記憶體」**:3.46 GB 在 16 GB 主機上引發壓縮與交換,core 自己
的頁面被換出,任何執行緒碰到就卡在解壓縮——表現為「兩條解耦的執行緒同時凍住
1.4 秒且都不燒 CPU」,看起來完全像排程問題。追錯了一天。

> **`ps -o rss` 在 macOS 上看不到已壓縮的頁面。** 這個行程的 RSS 顯示 23 MB,
> 實際 `phys_footprint` 是 3.46 GB。**會被壓縮的洩漏,對最順手的工具是隱形的。**
> 用 `/usr/bin/footprint -p <pid>`;`peak == current` 且持續成長就是還在漏。

完整過程、六個錯誤假設、以及每個假設是被什麼量測殺掉的:
**`POSTMORTEM_2026-08-10_stall.md`**。

## 2026-08-11 — full frame wedges the camera, and every setting still reads correct

At ROI 2448x2048 with triggering, the camera delivers 3-25 images and then
delivers nothing, with no error anywhere. It survives a core restart; only
`arv-tool-0.8 -n <cam> control DeviceReset` clears it. The production crop is
unaffected in every run so far. **Cause not identified** -- see
REPORT_2026-08-11_fullframe_wedge.md for what was excluded and for the four
conclusions that had to be withdrawn.

Two things from that report are worth having here because they will bite
again:

- **The log ring accumulates across core restarts.** 27 `bufferStatus:5`
  entries read out of a dump belonged to other runs and produced a confident
  diagnosis of the wrong mechanism. Diagnostics that matter now go to stderr.
- **`cam_max_fps` and `n_valid` are not liveness signals.** `cam_max_fps` is
  derived from the minimum frame interval, so it is a high-water mark that
  survives the camera stopping; `n_valid` only counts frames whose watermark
  decoded, so it reads 0 whenever the watermark is off regardless of how many
  frames arrive. Count `INSP_CAM_FRAME_TRACE` lines instead.


## `data/slowframes` 的檔名 CPU 欄位在 2026-08-13 之前全是錯的

存慢幀的地方讀 `g_lastMatchProcCpuUs`,而計算它的區塊排在存圖之後,所以每個
`slow_XXXX_<wall>ms_cpu<cpu>ms.png` 蓋的是**前一幀**的 CPU。既有的 60 個檔案
讀起來像「牆鐘 1594ms / CPU 4ms = 被擋住」,那是假的——前一幀是正常幀。已修
(`ff752c10`),但**舊檔案要重收才有意義**。聚合直方圖 (`match` / `match_cpu`)
從來沒有這個問題,它們加在賦值之後。

## `get_running_stat` 的裝置回覆已經在溢位邊緣

跑 30 秒後就序列化到 2886 / 3072 bytes,而裡面的計數器只會變大。加六列各五個
數字的表格會直接把 `StaticJsonDocument<3072>` 撐爆,結果**不是報錯,是安靜地
掉欄位**,整包變成無法解析的截斷 JSON。長時間跑之後讀不到 `free_heap` 就是這個
原因。任何新欄位都應該另開命令(如 `get_spikes` / `get_schema`),直到這包被
瘦身為止。

## 核心在 perif `DISCONNECT` 之後沒有關掉 tty fd

`delete_PeripheralChannel()` 之後 `lsof` 仍看得到核心持有
`/dev/cu.usbserial-0001`,所以燒錄韌體前必須整個重啟核心,光送 DISCONNECT 不夠。

## `?lat`:不用 BPG client 就能讀核心的分段延遲

`lat_hist` 掛在 `perif_pairing` GS 項目上。設 `INSP_PERIF_CONSOLE=<port>` 後,
在該 socket 上送 `?lat` 會直接印出分段表(queue/match/inspect/wait/write/e2e,
加引擎自己的 stage 拆解,牆鐘與行程 CPU 並排)以及 `e2e` 的桶。

## 開啟序列埠會把 ESP32 整台重開(DTR/RTS → EN)

任何**開啟** `/dev/cu.usbserial-0001` 的動作都會 power-cycle 分料機:open 時拉起
的 DTR/RTS 接在開發板的 auto-reset 電路上。特徵是 `reset_reason_name` 讀到
`POWERON`、`uptime_s` 從頭開始。

實測 2026-08-13:

- 重整瀏覽器 → WebUI 送 PD CONNECT → 核心重開 port → **uptime 611s → 1s**。
  盤面停、在飛的物件全丟、計數歸零。
- **關閉** port 無害(`c_cflag &= ~HUPCL`)。核心被 `kill -9` 之後,板子在沒人
  持有 port 的 12 秒間活得好好的。
- **核心重啟仍然會重開板子,而且使用者空間修不掉。** 在 `open()` 之後立刻
  `TIOCMBIC` 掉 DTR|RTS 做過也量過,完全沒有效果——脈衝發生在 open() 內部,
  ioctl 來不及。已退回。只有硬體改動(EN 對地加電容 / 切掉復位走線)能根除。

CONNECT 因此改成:同一台裝置且通道還開著就**沿用**,不關不開也不送 RESET
(`PerifChannel::conn_desc` 比對)。要真的重開請先送 DISCONNECT。

推論任何「板子自己重開了」的問題,先在可疑動作前後各讀一次 `uptime_s`。

## perif 心跳:`ping` 只在鏈路安靜時發,而且它負責武裝裝置的看門狗

`PerifPingThread` 每 100ms 檢查一次,只有在 `last_tx_us` 超過 100ms 沒動時才送
`{"type":"ping"}`。省的不是頻寬(10/s × 15 bytes 在 230400 baud 上約 1.7%,
不值一提),是 **`perif_tx_lock`**——那把鎖與報告路徑共用,實測有 140–215ms 的
等待,跑批時多塞一個定期寫入者會把報告排在 ping 後面。

每 20 拍(約 2 秒)其中一拍改送 `{"type":"comm_lost_backup","on":true}`,這是裝置
端 host-link 看門狗的武裝開關。**不要改回從 CONNECT handler 送一次**:開 port
會重開板子,那則訊息落進正在開機的裝置直接消失(同一個 handler 裡連送兩次
`send_RESET()`,看起來就是這個競態的舊補丁)。掛在心跳上才會自我修復,而且順序
才對——先讓餵食看門狗的流量跑起來,再武裝它。

## 相機層:被拒絕的 frame 會被當成真的 frame 用

兩個 `ExtractFrame` 呼叫端(`SNAP_Callback`、擷取 callback)都把回傳狀態丟掉,所以
一次拒絕——或一個尺寸為 0 的 `frameInfo`——會留下一個沒填過的 buffer 繼續往下走,
`cvtColor` 的 `!_src.empty()` assert 直接把整個行程帶走。已修:擷取路徑把 pool
slot 還回去等下一張(跟上面「pool 空」的處理一致),`SNAP_Callback` 回 NAK。

觸發它的是假相機讀不到的檔案(`jog_center.png`,`imread` 回 -1)。**副檔名過濾擋
不住這種**:副檔名是對的,內容解不開。

同一組問題還有一個:`CameraLayer_BMP::CalcROI` 的每個邊界檢查都拿 `img_load` 的
尺寸去比,空 Mat 時它們疊成一個原點 (−6,−6) 的 **6×6 視窗**——

```
tmpX >= cols-5      ->  0 >= -5      ->  tmpX = 0-5-1 = -6
tmpW+tmpX > cols    ->  99999-6 > 0  ->  tmpW = 0-(-6) = 6
```

不是拒絕,是一張看起來很合理的小圖,進了 `FeatureMatching` 之後以
`std::length_error` 收場。已修:空影像回 NAK。

**未解:崩潰頻率與畫面亮度相關。** 假相機的曝光模擬關掉(畫面恢復正常亮度)之後
崩潰變頻繁,而在它還把畫面壓成 1% 亮度時可以正常載入。上面兩個守衛都與亮度無關,
所以**它們不解釋這個相關性**。最便宜的判定:齒輪面板打開 `expo_sim_en`、`expo_us`
設 50 重現暗畫面;崩潰若跟著消失,要查的是二值化/標記那條路徑(變亮 → 前景大增 →
標記數量),不是相機層。

## 影像通道:bench 上永遠是 3 通道,真機的 mono 相機是 1 通道

2026-08-15 之前,每一個載入路徑都用 `cv::IMREAD_COLOR`,它會把灰階檔複製成 BGR。
FI、II、`--insp`、背景圖、以及 **BMP 假相機**全都如此。而真的 mono sensor 一路
保持 `CV_8UC1`(`ImageDownSampling` 的註解寫得很清楚)。

結果是 **bench 上根本產生不出 1 通道的 frame**,mono 專屬的程式碼路徑無法被執行
到。`pointSobel` 的邊界檢查用像素(`X+offset >= cols`)、索引卻寫死 3 通道位元組
(`(X+offset)*3`),在 1 通道影像上每一個取樣點都讀錯位置(不是只有 `cols/3` 之後
——`x=3` 就已經讀到 byte 3 而非 byte 1),右下角還會越界約兩列。它活到現在,就是
因為離線重播與假相機都給 3 通道。最後是靠 `test_suite/test_pointSobel_channels.cpp`
直接比對同內容的 1ch 與 3ch 才抓到(修正前 663/663 個取樣點不一致)。

已改成 `IMREAD_ANYCOLOR`(彩色檔仍給 3 通道,既有素材行為不變),假相機也會透過
`GetLoadedChannels()` 回報 `frameInfo.channelCount`——**在那之前它從不設這個欄位**,
所以核心的 `(channelCount == 1) ? 1 : 3` 永遠選 3(Aravis 有依 pixel format 設)。
存檔端不用改,`cv::imwrite` 本來就照 Mat 原樣寫。

實測(同內容):II 路徑 927 個量測值完全相同;carousel 定位相同(cx 15.0246),
穩態記憶體 258MB → 149MB,吞吐 21.3 → 23.2/s。pool 存 30 張完整影像,所以省的是
這個量級。

**影響範圍要說準**:`pointSobel` 只在 sig360 的 signature 比對通過之後才被呼叫
(`contourGridGrayLevelRefine`)。用 `locating_engine: shape_based` 的 def 走不到它;
沒有 `locating_engine` 欄位的舊 def(預設 sig360)才會。

## core 是有狀態的:量測基準必須在剛啟動的 core 上取

同一個 def、同一張圖,結果會因為 core 之前做過什麼而改變。2026-08-15 曾經差點把
一個 0.15px(0.002mm)的差異誤判成自己改壞——比對用的「修改前基準」其實取自一個
已經跑過 120 次 INST_CHECK 的 core。用 `git stash` 還原程式碼重建後**仍然**不一致,
才確定不是程式碼。

當時的根因是 core 開機不載入鏡頭校正(要等 WebUI 送 `RC{calib_files_load}`),所以
「有沒有被載入過校正」會改變結果。這點已修(開機自動載入 `data/lens_calib.json`,
並在 `camera_info` 回報 `lens_calib_loaded` / `_autoloaded` / `_rms_px`),但教訓仍成立。

判斷「不一致是不是自己造成的」的順序:①同一 binary 連跑 3 次,先確認結果是決定性
的;②`git stash` 還原、重建、再比對——這一步才分得出程式碼與環境;③兩者都排除,
才去找環境裡變了什麼。

## 崩潰要看 .ips,不要從自家 log 的最後一行推論

完整 backtrace 在 `~/Library/Logs/DiagnosticReports/visSele-<時間>.ips`,含訊號、
觸發的執行緒、每一層 symbol 與行號。跑完重現步驟就去那裡拿最新一份。

**自家 log 的最後一筆常常是紅鯡魚。** 追 `camera_ez_reconnect` 崩潰時,三份 dump
都停在 `PHYLayer is not able to eatablish`,看起來像周邊通道——那其實是 perifCH 每
3 秒重連的常態噪音。真正的點在 `FeatureReport_UTIL.cpp:386` 的 `cameraCalib2JSON`,
完全不同的執行緒。log 停在哪只代表那是最後一筆**寫完並 flush** 的記錄,尤其在
persist 預設關閉、drainer 會無聲死掉的情況下。

順帶:`pkill`(SIGTERM)本身在 2026-08-15 之前也會產生 .ips(signal handler 裡做
`delete ifwebsocket`,主執行緒還在用它)。已修,handler 只設旗標。

---

## 日誌:ring 是「跨行程、跨世代」的具名共享記憶體(2026-08-18)

`latest_dump.dump` 裡有**每一代 core 的日誌**,不是這次跑的。ring 是具名 shm
(`insp_log_ring`),core 結束不 unlink,drainer 重開也是 attach 而非重建。所以:

- 用 dump 做「這次跑了多少行」的統計會被前幾代污染(踩過:一份 8.5MB 的 dump
  裡有 21568 筆 HR,全是好幾天前重連風暴留下的)。
- 時間戳也不能拿來過濾——不同世代的 epoch 不同,同一份檔案裡會同時出現
  `[ 2836.5]` 和 `[554192.4]`。
- **要乾淨的量測就給它一個新名字**:`INSP_LOG_RING_NAME=xxx_$$ ./visSele`,
  啟動訊息會寫 `(created)` 而不是 `(connected)`。這是本輪 before/after 對照
  唯一可信的做法。
- dump 會同時寫 disk 段與 ephemeral 段,**同一行可能出現兩次**;統計前先 dedupe。

## SC log_dump 的檔名是固定的

on-demand dump → `latest_dump.dump`(`inspd_log_main` 用 `fixed_name=on_demand`),
每次覆蓋;`crash_<utc>.dump` 只有真崩潰才產生。要留就先改名。
headless 觸發:`UI/WebUI/tools/webctl/logdump.mjs`。

## 「日誌噪音」是可以量的,而且量了才知道錯在哪

方法:同一份負載跑兩個 binary,各給一個新 ring,`SC log_dump` 後依
(level, file:line) 做普查。2026-08-18 的結果:同一段 20s / 488 幀、開 NA 快照的
負載,**3982 行 → 494 行(每幀 8.14 → 1.01)**,ERROR **1226 → 26**。

真正的收穫不是行數:普查把 `SAVE::<path>` 這種「說自己成功了但其實沒有」的
行揪出來,直接證出 backlog 7.2(快照從 08-12 起一張都沒存成)。
**丟掉回傳值的日誌比沒有日誌更糟**——它會主動說謊。
