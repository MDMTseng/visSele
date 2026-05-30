# acvImage → OpenCV migration plan

Long-term goal: drop the in-tree `acvImage/` library and use `cv::Mat` for image
storage / `cv::*` primitives for geometry, throughout the `MatchingEngine` and
the inspection pipeline.

The QA harness in `test_suite/qa/` + `test_suite/suite.py` (~540 cases as of
this writing, including ground-truth tolerance comparison against the 5 MP
golden in `qa_imgstress`) is the safety net for each step. The committed
baseline `test_suite/expected/10221.json` is the byte-identical regression
oracle.

## Status

- **Step 1 DONE** — `FEATURE_OPENCV` flipped to mandatory; the previously-opt-in
  cv-based sources (`LabelingCV.cpp`, `BinarizeCV.cpp`, `SearchPointCV.cpp`,
  `CvBridge.cpp`, `ChessboardExtract.cpp`, `LensCalib.cpp`) are now unconditional
  in the build. `find_package(OpenCV REQUIRED)` is top-level. The
  `#ifdef FEATURE_OPENCV` guards are now always-true but left in place; they
  cost nothing and removing them is a mechanical follow-up.

## Validation for the daemon LoadIMGFile sites

The `--insp` path's image-load swap (`LoadIMGFile` → `cv::imread` + `useExtBuffer`
acvImage shim) has been extracted into a shared primitive
`loadImageCv(path, out_mat, out_acv)` in `CvBridge`. This is the validated
migration unit: it's exercised by every `--insp` test we run (the migration
gate, suite, QA modules, qa_imgstress), so any regression in the loader pattern
is caught immediately.

The BPG daemon `LoadIMGFile` call sites in `Core0_1/wiringPanel.cpp`
(approximately lines 1952, 2033, 2372, 4690, 4830, 5066, 5105) can adopt
`loadImageCv` mechanically when a daemon-level test harness exists. The
mechanical swap is low risk because:
  - The loader primitive itself is gated.
  - `cv::imread` and `LoadIMGFile` produce byte-identical output on this
    codebase's images (verified by a one-off probe, removed).

A real BPG-protocol Python test client is the proper next step before
migrating those sites, but is a separate (half-day) project; the swap pattern
is recorded here so it's ready when that arrives.

- **Step 2 IN PROGRESS** — migrate engine-internal image storage from
  `acvImage *` to `cv::Mat`. Keep `acv_XY` / `acv_Line` / `acv_Circle` /
  `acv_LineFit` / `acv_CircleFit` POD geometry types **unchanged** during this
  step (they sit in the report schema; touching them risks the golden diff).

  **Step 2a — DONE — add zero-copy BGR view primitive (`acvImageBgrView`)** in
  `CvBridge`. `acvImage`'s row layout is contiguous
  (`CVector[i] = CVector[i-1] + RealWidth*Channel`), so a `cv::Mat` header can
  wrap its ROI without copying. Writes via the view mutate the underlying
  `acvImage`. This is the workhorse primitive for the rest of step 2.

  **Step 2b — NEXT (DEFERRED PENDING SCOPE DECISION) — per-TU signature flip**.
  Reconnaissance at the `binarize_bg_flatten_cv` call site
  (`FeatureManager_group.cpp` around line 300) shows that the binary image flows
  immediately into several `acv*` downstream consumers
  (`acvThresholdMap`, `acvThreshold`, `binaryDownScale`, `acvDrawBlock`).
  Flipping ONLY `binarize_bg_flatten_cv`'s signature to `cv::Mat` would force
  acv→cv→acv conversions at the call site — strictly worse than the current
  state. The right move is a coordinated flip of the *whole binarization +
  labeling slot* — `binarize_bg_flatten_cv` plus its downstream `acv*` cousins
  that touch the binary image — and to plumb a single `cv::Mat binary_img`
  through that slot. This is bigger than one TU but is the natural
  cv::Mat-island, then step 2c moves the next slot, etc.

- **Step 3 PENDING** — once Step 2 is complete and stable, swap acv geometry
  types for `cv::Point2f` / `cv::Vec*` / etc. in internal computation, keeping
  the report-struct fields as wrappers / typedefs first so the JSON schema is
  preserved. Final flip: change the schema, regenerate the golden baseline.

- **Step 4 PENDING** — retire `acvImage/`, prune `BPG_Protocol/smem_channel`
  acv touch-points, drop the now-vestigial `FEATURE_OPENCV` option.

## Guardrails (apply to every step)

1. **Golden byte-identical**: `--insp` on the 10221 sample must produce
   `expected/10221.json` exactly. Regenerate the baseline only when an
   intentional, schema-affecting change lands (step 3 end).
2. **All QA modules green**: `python3 test_suite/qa/run_all.py` exits 0
   (modulo the documented 2 non-engine-bug failures: IEEE float32
   cancellation in `qa_calc`, and one test-isolation issue in `qa_measure`).
3. **Suite green**: `python3 test_suite/suite.py` 79/79.
4. **Imgstress drift**: per-judge drift against ground truth stays under the
   per-perturbation tolerance documented in `qa_imgstress.py`.
5. **No `nan`/`inf` tokens** in any output JSON (regex check in
   `qa_imgstress._check_against_golden`).

## Current status (2026-05-31)

Phase 3a — `binary_processing_group` entry-point cv-clean. Done:
- FeatureManager interface flipped to mutual bridges (both signatures non-pure
  with a re-entry guard; subclasses override either).
- 5 FMs migrated cv::Mat-native: FM_Blank, FM_GenMatching, FM_nop,
  FeatureManager_group (dispatcher), FeatureManager_binary_processing_group.
- Helpers ported / added: `cvCloneImage`, `cvThresholdMap`, `binarize_bg_flatten_cv`
  signature flipped (was acvImage-typed but cv-internal).
- binary_processing_group body now owns `binary_img_storage` as cv::Mat,
  drops the legacy `binary_img` / `ds_binary_img` acvImage shim members,
  passes cv::Mat to sub-features via the base bridge.
- `setOriginalImage(cv::Mat&)` overload added — auto-binds a per-instance
  acvImage shim so unmigrated sig360 internals keep reading `originalImage`
  as acvImage* without the caller maintaining one.
- Dropped dead: ImageStackAddUp::Export(acvImage*), all ImgInspection_*
  acvImage* overloads, MatchingEngine::FeatureMatching(acvImage*), saveInspectionSample
  acvImage* overload, sig360 buff1/buff2/buff_/buffer_img param, group's
  binaryDownScale / labeledUpScale.
- wiringPanel members: image_pipe_info::img and the static test1_buff are cv::Mat;
  BPG class members tmp_buff/cacheImage/dataSend_buff are cv::Mat.

Phase 3a deep sweep update: gate-exercised path is now cv-native end-to-end.
- MatchingCore: contour-extraction chain (acvContourExtraction + cvContourWalk),
  extractLabeledContourDataToContourGrid, pointSobel, contourGridGrayLevelRefine,
  ContourFilter all take cv::Mat. EdgePointOpt/_/2, refineEdgeInfo, dead extract*
  fns and the dead `if(0)` originalImage acvDrawLine deleted.
- edgeTracking ctor takes cv::Mat&; contourPixExtraction/pixFetch use _gray_cv
  directly; getImageCv() hands cv::Mat to callers.
- Caliper.cpp (caliper_measure, search_point_scan, caliper_locate_line/circle):
  cv::Mat& gray + cvUnsignedMap1Sampling. SearchPointCV.cpp (search_point_cv,
  isObjectPx, labelAt): cv::Mat&.
- binary_processing.originalImage is now cv::Mat originalImage_cv (no acvImage
  shim member); setOriginalImage takes cv::Mat&.
- sig360: _img_shim / labeledBuff acvImage* drop out; p_cropImg is cv::Mat;
  search_point_cv / caliper_locate_* receive cv::Mat (no acvImage round-trips).
- wiringPanel SNAP_Callback + getImage cv::Mat-native; ImageDownSampling
  cv::Mat-native (ImageSampler grew cv::Mat sample overloads in common_lib).
  Dead acv ImageDownSampling, transpose helpers, #if 0 MJPEG block, and the
  acvImage_BasicTool include all removed.

Remaining blockers to unlinking acvImage from CMakeLists:
- FeatureManager base still owns `acvImage _buff` (used by FM_platingCheck) and
  `FeatureMatching(acvImage*)` virtual default (with cv::Mat -> acvImage
  shim) -- keeps the class transitively reachable.
- FM_platingCheck + FM_stage_light_report bodies use acvBoxFilter, RGBToGray,
  acvCloneImage, sobelSpread, imageDownScale, backLight* helpers. Both
  unexercised by gate (Q7 risk).
- ImageSampler acvImage* overloads still defined (gated by Q2 — defer to 3b).
- LabelingCV acvImage* overloads (legitimate bridge for the FM_platingCheck/
  stage_light_report path).
- CvBridge acvImage<->cv::Mat shim API itself.

Phase 3a status:
- BPG class image members (`tmp_buff`/`cacheImage`/`dataSend_buff`) → `cv::Mat`.
- `image_pipe_info::img` (per-pipe captured frame) → `cv::Mat`; camera
  `ExtractFrame` writes into `Mat.data` after `create(H,W,CV_8UC3)`.
- Static downsample buffer `test1_buff` in `InspResultAction_s` → `cv::Mat`;
  `ImageDownSampling(cv::Mat&, const cv::Mat&)` overload added.
- `SEND_acvImage` consumes `cv::Mat*` via `BPG_protocol_data_acvImage_Send_info::img`.
- `MatchingEngine::FeatureMatching(cv::Mat&)` overload and base-class default
  bridge already in place.
- `loadImageCv`, `acvImageBgrView` primitives in `CvBridge` are the shim layer.

Remaining blockers to unlinking `acvImage`:
1. **`MatchingEngine` interface flip** (FeatureManager.h pure-virtual is still
   `FeatureMatching(acvImage*)`). 24 overrides. Strategy: per-class, keep
   `acvImage*` as a 3-line shim wrapper and implement the body in the
   `cv::Mat&` override; flip the pure-virtual once all 24 are converted.
2. **`MatchingEngine` heavy users** with `CVector`/`ReSize` pixel loops:
   `FM_camera_calibration.cpp` (12 refs/18 calls), `FeatureManager_platingCheck.cpp`
   (14/14), `FeatureManager_gen.cpp` (26/8 — but pixel access is delegated to
   `acvUnsignedMap1Sampling`, so cv::getRectSubPix may be a drop-in),
   `FeatureManager_group.cpp` (15/13), `FeatureManager_sig360_circle_line.cpp` (13/9).
3. **`common_lib/ImageSampler`** — 4 `acvImage*` overloads in
   `include/ImageSampler.h`. Wraps `acv_XY` + pixel fetch. Phase 3b
   (`acv_XY` removal) and 3a converge here.
4. **`Core0_1/wiringPanel.cpp` remaining refs** (~45): `SNAP_Callback` (camera
   side still acvImage*), `ImgInspection_JSONStr`/`ImgInspection_DefRead` entry
   points, `saveInspectionSample(acvImage*)` bridge, `transpose(acvImage*,..)`,
   `getImage(CameraLayer*, acvImage*)`. Most are shim-thin; SNAP_Callback needs
   CameraLayer to grow a cv::Mat hookup.
5. **CMakeLists.txt** — `common_lib`, `CameraLayer`, `MatchingEngine` all
   `PUBLIC` link `acvImage`. Drop only after (1)-(4).

Recommended next 3 commits:
1. Flip FeatureManager interface — keep both signatures non-pure with mutual
   bridges (cv::Mat ↔ acvImage via `acvImageBgrView`/`useExtBuffer`). Use a
   thread-local re-entry guard to abort on "neither overridden". Existing 24
   overrides keep working unchanged.
2. Migrate the 5 heavy MatchingEngine FMs: implement `FeatureMatching(cv::Mat&)`
   bodies natively; reduce the `acvImage*` override to a `cv::Mat view =
   acvImageBgrView(img); return FeatureMatching(view);` shim.
3. `common_lib/ImageSampler` cv::Mat overloads + `CameraLayer` cv::Mat extract +
   wiringPanel `SNAP_Callback`/`getImage` migration.

After that: delete the `acvImage*` overrides from every FM, mark the base
acvImage entry pure-virtual gone, drop acvImage from `target_link_libraries` in
CMakeLists. (Compile must stay green at each step with `migration_gate` +
`daemon_smoke` + `suite` validation.)

## What NOT to migrate (yet)

- The acv types live in the **report struct definitions** (`include/FeatureReport.h`).
  Don't touch those until Step 3, and only behind a schema-version bump.
- The `BPG_Protocol/smem_channel` IPC layer ships `acvImage` between processes
  (live daemon path). Leave it alone until Step 4 — the daemon is factory
  deployed.
- The `data/` defs are user-authored content; the migration is a backend swap,
  not a content change.

## Where the existing OpenCV path already lives

- `MatchingEngine/CvBridge.cpp` — `acvImage ↔ cv::Mat` translation. Will become
  the central transition point during Step 2.
- `MatchingEngine/LabelingCV.cpp`, `BinarizeCV.cpp`, `SearchPointCV.cpp` —
  already use `cv::Mat` internally for connected-components labeling,
  binarization, and search-point matching; previously gated by
  `#ifdef FEATURE_OPENCV` (still gated, but the gate is now always true).
- `MatchingEngine/ChessboardExtract.cpp`, `LensCalib.cpp` — calibration
  utilities already on `cv::calib3d`.
