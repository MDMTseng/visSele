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
