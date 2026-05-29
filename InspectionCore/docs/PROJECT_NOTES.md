# visSele Inspection System — Project & Rework Notes

A complete record of the system, the measurement-engine rework, and every issue
hit along the way. Companion docs: `measurement_pipeline_and_caveats.md`,
`caliper_primitive_locating_design.md`. Branch: `refactor/inspection-img-streaming`.

---

## 1. System overview (the apps)

Monorepo `/Users/mdm/workspace/visSele/`:

- **`InspectionCore/Core0_1`** — the **1st-gen inspection app** ("visSele" binary).
  C++/`acvImage` (homegrown image lib). **Deployed on many factory machines**,
  actively producing inspection data uploaded to a data server. This is the app
  being reworked. Talks to the UI over a **BPG binary protocol on WebSocket :4090**.
- **`InspectionCore/CoreHub`** — the **2nd-gen app** (OpenCV/`cv::Mat`,
  `InspectionTarget` modular pattern: `InspTar_DimMeasure`, `InspTar_CameraCalib`,
  `InspTar_ImgSrc`, ...). Newer/cleaner; has mature versions of some algorithms we
  port FROM (e.g. the search-point measurement).
- **`UI/WebUI`** — the operator web UI (webpack dev server, ~:8080). Connects to the
  core over BPG/WebSocket. Has a **"check golden sample"** feature that runs the real
  inspection on the stored golden image (great for testing without a camera).
- **`UI/WebUI2`, `UI/InspectionMonitor`, `Electron_XPLAT`** — other UI pieces.
- **`Peripheral/`** — ESP32 / stepper / EtherCAT motion + sensor firmware.

### The measurement engine (where the rework lives)
`MatchingEngine/FeatureManager_sig360_circle_line.cpp` — the core inspection logic.
Per-image pipeline:
```
capture/load -> binarize -> label (CCL) -> per labeled object:
  ORIENTATION MATCH (sig360 polar signature: contour_sig | edge_sig)
  -> LOCATING-ANCHOR (定位點) refine (ConstrainMap warp)
  -> per-feature LOCATE via TreeExecution dependency DAG
       (line / circle(arc) / aux-point / search-point)
  -> MEASURES (judges: distance/angle/radius/calc/...) vs USL/LSL
  -> report (BPG "RP")
```
- **TreeExecution** — recursive memoized DAG resolver (`FeatureManager_sig360_circle_line.cpp`
  ~4065). Each feature: locate-once, deps first. Cycle guard added this session.
- **ConstrainMap** (定位點) — datum/anchor warp: anchors located, their
  nominal→located displacements warp every def point before measuring. Anchors ARE
  search points. Exists because (a) coarse-pose precision is low and (b) parts deform.
- **Report coords** are absolute image-mm (`px = mm / mmpp`), `offset={0}`, for both
  lines (`cx/cy/vx/vy`, NaN serializes as null), circles (`x/y/r`), search points (`x/y`).

### Build
- Pure **CMake + vcpkg** (rewrote the old Makefile/CMake hybrid). Presets:
  `mac-arm64`, `mac-arm64-opencv`, `win-mingw` (MSYS2/MinGW for the Surface deploy).
- **OpenCV is OPTIONAL** via `FEATURE_OPENCV` (vcpkg or Homebrew). The current
  `build/mac-arm64` is `FEATURE_OPENCV=ON`; `MatchingEngine` links opencv
  core/imgproc/imgcodecs/calib3d **only** (Homebrew `opencv_dnn` has a broken
  libprotobuf dep → linking full `OpenCV_LIBS` dyld-aborts). `visSele` gets OpenCV
  transitively through `MatchingEngine`. cmake 4 gotcha: needs
  `CMAKE_POLICY_VERSION_MINIMUM=3.5`.

### Component-label pixel encoding (bit me — see issues)
Labeled image (`acvComponentLabeling`): background = white (255,255,255); object
interior = the 24-bit label value `Num = c0|c1<<8|c2<<16` (R channel 0 for small
labels); object **contour = (B,G,R)=(1,128,1) = 98305** (generic marker). A thin
wire is almost ALL contour, little/no interior.

---

## 2. The rework — goal, constraints, validation

**Goal (user):** the legacy logic sometimes produces **unexpected results** (unstable
across orientation/lighting); fix it "once and for all" with a real rework — more
accurate AND more stable, especially on imperfect parts (curved lines, oval arcs).

**The two hard constraints ("don't change too much" means exactly these):**
1. **Old def files must still load + work.**
2. **Report schema stays a SUPERSET** (keep every field; add freely).
Beyond those, free to rework for robustness/efficiency — OpenCV allowed in Core0_1
("you can use opencv to replace my acv"). Don't be opt-in-only for its own sake.

**Validation ladder:**
1. Reproduce the known-good **legacy datapoint** (a saved real inspection report) —
   no regression on a good case.
2. Later: **real-world harsh-condition test** to prove it beats legacy.
- Legacy is a **floor, not a forever-contract** ("the report this time just looks
  right"). Principled deviations are OK *if justified* (sub-pixel grayscale vs binary
  contour, robust fit, true edge) — but **>4px off legacy is a RED LINE** (legacy
  holds ~4px; a bigger gap means OUR bug, not "we're better").
- **Changing a feature's locating method silently shifts its calibrated measured
  value** (proven: line1→caliper turned 8.60mm into 6.87mm, flipped pass→NG). So a
  robust drop-in must match **both position AND direction-sign**.

**Milestones:** M1 OpenCV bridge ✅ · M2 robust search-point (in progress) ·
M3 robust line+circle (line done) · M4 real-world test.

---

## 3. What was built (by area, with commits)

### Build system rewrite
Pure CMake+vcpkg, presets, OpenCV gating, MSYS2/MinGW target. (See memories
`project-build-rewrite`, `project-build-toolchain-notes`.)

### Camera calibration
- Telecentric solver (dependency-free C++ port) + robust MAD outlier rejection
  (`4bfb0e8a`, `3ea107ea`).
- Chessboard extraction (`e85dde0b`), selectable normal(perspective) vs telecentric
  (`6329e671`), point-space undistortion + JSON (`444cdc6e`), runner + apply flag
  (`0f653e04`). LEFT: wire into Core0_1 BPG + WebUI selector (tasks #11/#12).

### Orientation matching (lighting-robust)
- Edge-response rotational signature (grayscale gradient ray-cast vs binary contour)
  behind `matching_method:"edge_sig"` (`6d30fa01`, `dcff493e`); speed knob
  `edge_sig_ray_step` (`5721c54c`). Default `contour_sig` unchanged.
- Fragment-vote contour matcher parked as a lab experiment (`301f410f`).

### Labeling + binarization
- OpenCV connected-components labeling (correct + faster, FEATURE_OPENCV) `b3168c74`,
  build `acv_LabeledData` from CC stats `b62c22dd`.
- Calibration-free vignette-tolerant binarization (bg-flatten) `370d3780`.

### Primitive locating — caliper/section (the main rework)
- Edge-selector toolbox `EdgeSelect` (strongest/first/last/middle/nth + polarity +
  sub-pixel) `97e51664`; design doc `5ce78286`.
- `Caliper` unit (project/average section) `e514732d`, robust line locator
  (weighted-TLS + MAD) `10379f77`, robust circle locator (Kasa + MAD) `29823bff`.
- Wired into pipeline behind per-feature `locating:"caliper"` (default contour):
  line `d5604a4f`, circle `d81a7216`, search-point `026eeb00`.
- **Caliper LINE direction-orient fix `f8749254`** — orient TLS dir to def p0→p1;
  reproduces legacy measures to **<0.01mm** while using 27-inlier MAD vs legacy's
  2-point vertex fit. THE validated win.
- TreeExecution **cycle guard** `16079f6a`.

### Anchor (定位點) robustness — PARKED
- `EdgeSelectInfo` quality (`5cd3ce3d`), `anchor_patch_zncc` ZNCC matcher
  (`bb212667`), design (`dca2fae0`), parked (`72fe153e`). Plan: every locating_anchor
  must pass a ZNCC-patch check vs the golden image (sent-with-def); any fail → fail
  whole object. Tasks #17/#19/#20.

### Test infrastructure
- **`visSele --insp <img.png> <def.hydef> <out.json>`** headless golden-sample
  inspection (`144b0fe4`), faithful after calib-map init fix (`d2f6090b`) — it
  **reproduces the legacy engine exactly** (line cx/vx identical to ~6 digits).
- `SPCV_DUMP=1` env saves `/tmp/spcv*_ptX_Y*` (rectified region | mask | edges) for
  visual debug. **`SendUserFile`** is the way to show the user overlays (no tunnel).

### M1/M2 OpenCV port (search-point)
- `acvImage↔cv::Mat` bridge `CvBridge` (`d33fdca6`).
- Interim acv `search_point_scan` (`550b4e9a`), then `SearchPointCV` port of CoreHub
  (rectify→sobel→first-hit) `93d1ceb0`, off-image guard `ed0cda03`, dilated-label
  mask scaffold `0288f41c`, **mask fix (not-white-background)** `4e3f571c`.

### Test data
`/Users/mdm/workspace/HY_sync/DEV/test/`:
- `10221 BOS-LT12BH4211 SORTING.hydef` + `.png` (golden image, 2592x1944, mmpp 0.008858)
- `..._bk.hydef` + `_bk.png` — known-good def copy
- `..._bk_REPORT.json` — **the real legacy inspection report = ground truth**
  (`reports[0].reports[0]` has searchPoints x/y, detectedLines, judgeReports value/status).
The part is a thin **wire form** (dark lines on white); imperfect (curved lines,
oval arcs). Run harness from `Core0_1/` cwd with
`DYLD_LIBRARY_PATH=build/mac-arm64`.

---

## 4. Every issue / bug / misdiagnosis hit

Build/infra:
- cmake 4 rejected old `cmake_minimum_required` + stale cache → policy flag + clear cache.
- `compile.sh` exited 0 on failure → grep log for `error:`.
- Homebrew `opencv_dnn` broken libprotobuf → dyld abort → link only core/imgproc/
  imgcodecs/calib3d.
- visSele blocked on "Camera init retry" with no camera → BMP-carousel soft-cam fallback.

Caliper/edge:
- `edge_select` first/last grabbed boundary zeros → adaptive noise floor + strict local-max.
- Fragment-vote: rotations scattered (near-circular ambiguity) → parked as toy.

Headless harness (`--insp`):
- First version: ALL line/circle fits came back **NaN** while sign360 matched — the
  bare `neutral_bacpac` sampler had an **uninitialized calib map** → `img2ideal`
  divided by uninit RNormalFactor → NaN poisoned every edge refine. Fix: call
  `LoadCameraCalibrationFile(...)` to RESET+load the calib map (`d2f6090b`). After
  that the harness reproduces legacy exactly.

Caliper line (rig/harness validation):
- Initially compared caliper vs a `_bk` def that was a *different* def → lines 2/30
  differed → not a clean A/B. Fixed by reverting line1 in the *current* def.
- **Line direction sign flipped** vs legacy (TLS sign arbitrary) → dependent search
  points' search angle flipped → measures shifted mm and flipped pass→NG. Fixed by
  orienting to def p0→p1 (`f8749254`).
- Misread legacy L1 `mpts=2 / vx=-291` as a "degenerate fit / bug" — it's a normal
  **vertex_touch_searching** line representation; user corrected. Lesson: don't
  diagnose legacy "bugs" from raw numbers; the user's visual result is authoritative.
- Misread sp3==sp5 (same point) as a collapse bug — it's **correct geometry**
  (bottom-right IS the rightmost point on this part).
- Forcing line1→caliper(first/falling) "fixed the fit" but **broke the measures**
  (8.60→6.87mm) — wrong edge selection; legacy value was closer to truth. Lesson:
  measured values are ground truth, not "clean RMS"/pixel positions.

`vertex_touch_searching` (height/width lines) — NOT yet implemented for caliper:
- Legacy fits the edge DIRECTION, then offsets the line to a **convex-hull
  supporting/tangent line at the OUTERMOST point** (keeping direction). For a curved
  edge a plain best-fit sits *inside* → too-small height/width.
- **Both curvature signs:** convex bump → tangent at apex; **concave/inward curve →
  link the two outermost FEET** (chord bridging the concavity). Convex hull handles
  both; a naive "offset to single farthest point" is wrong for the concave case.
- TODO (M3): expose caliper inlier points → supporting-line/chord, gated on
  `vertex_touch_searching`.

Search-point (M2) — a long chain:
- `caliper_measure`-based version diverged 0.12–3.1mm; some NA. Root: it averaged
  across the `width` band (smoothing) — but a search point **scans for the FIRST
  hit**; "smoothing it makes the result off" (user). Fix: thin scan, no width-average.
- Span was ±margin centered; legacy is ONE-SIDED `[pt, pt+margin]`. Fixed.
- Default method should be FIRST (nearest hit), not STRONGEST.
- Apparent huge errors were partly an artifact of comparing against a contour
  baseline whose **line1 was mislocated** (ref line drives the search angle). User:
  "make sure the ref line locates correctly, spoint needs it."
- Ported CoreHub OpenCV method (`SearchPointCV`: remap → sobel → mask → local-max →
  scan). The wide-band points (sp5/6/20/21) blew up because the band extends into
  **background** and grayscale picks up specks/dust (legacy uses the binary contour →
  object-only). User's fix: **dilated label mask**.
- Mask attempt 1 wrong: misdiagnosed `labelAt`→0xFFFFFF as a "stale buffer". It's not
  stale — that's just white background. The bug was the **predicate**: `label==objLabel`
  fails on a thin wire (all contour pixels = 98305, not interior). Fixed to "not white
  background" (`4e3f571c`); mask now correctly isolates the wire (verified visually).
- **Still open:** wide-band **extremal geometry**. sp5's search region (margin depth +
  width band from nominal) doesn't even contain the legacy result (region = bowtie
  loop; legacy sp5 = bottom-right corner ~490px away). The legacy reaches it somehow
  (long-range / different direction / margin-width role). NEXT: instrument the legacy
  contour search-point path (`searchPoint_process` ~1420) to learn how, then match.

Visualization gotchas:
- Red-tinting masked pixels was invisible on white background → saved the mask as its
  own image (white=allowed, black=masked) to verify.

---

## 5. Current state & remaining work

DONE & validated: build rewrite; orientation edge_sig; labeling/binarization;
caliper **line** (reproduces legacy <0.01mm, more robust); caliper circle (conventions
OK; oval explains radius variance); TreeExecution cycle guard; the `--insp` harness
(faithful ground-truth tool); search-point mask now isolates the object.

OPEN / NEXT:
- **M2 search-point wide-band extremal geometry** (instrument legacy, match). Narrow
  search points already ~4–15px; wide extremal (sp5/6/20/21) off.
- **M3 caliper line vertex_touch_searching** (convex-hull supporting line / foot chord).
- **M4** real-world harsh-condition test.
- Caliper circle: full decouple roughness from binary contour; orient if its
  normal/angle sign is consumed.

PARKED: anchor ZNCC quality-check (tasks #17/#19/#20); golden-image-with-def delivery.
DEFERRED (good-enough zone): TreeExecution readability refactor (design captured;
do only when extending, with measure-identical validation).

OLDER PENDING: lens calib → Core0_1 BPG + WebUI selector (tasks #11/#12); background-
evenness WebUI save (#8); win-mingw build validation.

All new behavior is opt-in / additive — **old defs use the legacy path unchanged**.

## 6. Key file map
- `MatchingEngine/FeatureManager_sig360_circle_line.cpp` — engine (orientation, DAG,
  per-feature locate, anchor, search-point branch ~1379, TreeExecution ~4065,
  binary loop/SingleMatching ~4362/~5237, FeatureMatching ~4956).
- `MatchingEngine/{EdgeSelect,Caliper,SearchPointCV,CvBridge,AnchorPatch,EdgeSignature,
  TelecentricCalib,LensCalib,ChessboardExtract,LabelingCV,BinarizeCV}.{h,cpp}`.
- `MatchingEngine/MatchingCore.cpp` — `edgeTracking`, `extractLabeledContourDataToContourGrid` (~1784, shows label match).
- `Core0_1/wiringPanel.cpp` — BPG handlers, `--insp` mode (cp_main ~4912), inspection
  dispatch (`ImgInspection_*`), def-set handlers (~2056/2215).
- `docs/` — this file + `measurement_pipeline_and_caveats.md` + `caliper_primitive_locating_design.md`.
- Memory: `~/.claude/projects/-Users-mdm-workspace-visSele/memory/` (MEMORY.md index +
  project-* / reference-* topic files).
