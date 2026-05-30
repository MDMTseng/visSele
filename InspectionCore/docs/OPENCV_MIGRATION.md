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
