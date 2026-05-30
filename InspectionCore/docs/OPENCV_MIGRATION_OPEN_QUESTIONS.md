# acvImage → cv::Mat migration: open questions

Logged while running the unlink in autonomous mode. Each item is something
that needs a deliberate choice (not just slog) before more progress can land.

## Q1. Heavy-FM body porting strategy

The remaining FeatureManager bodies that still own `FeatureMatching(acvImage*)`
as their canonical impl all do per-pixel work via `CVector[][]` loops or call
acv helpers (`acvCloneImage`, `acvBoxFilter`, `acvDrawBlock`, `acvThresholdMap`,
`binarize_bg_flatten_cv` w/ acv signature, `binaryDownScale`,
`acvComponentLabeling_cv` w/ acv shim, `imageDownScale`, `backLightBlockCalc`,
`backLightNonBackGroundExclusion`, `EdgePointOpt*`, `pointSobel`,
`extractContourDataToContourGrid`, `contourGridGrayLevelRefine`, etc.):

- FeatureManager_binary_processing_group (canonical body still acvImage; has
  cv::Mat override that just bridges back).
- FeatureManager_sig360_circle_line (13 refs, ~9 method calls).
- FeatureManager_platingCheck (14 refs / 14 method calls).
- FeatureManager_gen (FM0 path uses acvCloneImage, ReSize loops).
- FeatureManager_stage_light_report (heavy via internal acv helpers).

**Option A** — port each body to cv::Mat-native, drop the acv override.
Requires either porting the acv helpers too OR replacing each helper call
inline (cv::copyTo / boxFilter / rectangle / threshold equivalents).

**Option B** — leave the acv overrides in place; rely on the base mutual
bridge. Acv lib stays linked because the bodies still call acv free
functions. Won't get us to unlink.

**Option C** — add cv::Mat overloads to the acv helpers in CvBridge
(`acvCloneImage_cv`, `acvBoxFilter_cv`, …), migrate each FM to call them.
Same volume of work but better separation: each helper migration is
testable in isolation.

**Recommendation:** Option C is the path with least drift risk per commit.
Helpers are smaller and easier to validate than whole FM bodies. Migration
gate already catches per-leaf drift to 1e-4.

**Needs decision:** confirm C, or pick A if you'd rather see whole bodies
rewritten directly.

## Q2. ImageSampler interface

`common_lib/include/ImageSampler.h` has 4 sampling overloads taking
`acvImage*`. Internally they call `acvUnsignedMap1Sampling*(acvImage*, ...)`
which lives in `acvImage/acvImage_SpDomainTool.cpp`.

Callers: only `Core0_1/wiringPanel.cpp::ImageDownSampling(acvImage&, ...)`
and `CoreHub/wiringPanel_ext.cpp`. In wiringPanel the call is *inside* the
legacy `ImageDownSampling(acvImage&, acvImage&)` body — already bridged by
a cv::Mat overload that creates a shim.

**Option A** — add cv::Mat overloads to ImageSampler that bridge via the
existing shim pattern.

**Option B** — fold ImageSampler interface change into the Phase 3b
acv_XY removal (since the geometry types are co-located).

**Recommendation:** B. The cost of the bridge today is zero (the legacy
path still works); pairing the interface change with the schema-affecting
acv_XY removal avoids two interface churns.

**Needs decision:** confirm B.

## Q3. CameraLayer migration

`CameraLayer::SnapFrame` takes a `void *obj` and hands it to a user
callback (`SNAP_Callback`). The current callback casts it to `acvImage*`
and calls `ReSize` + `ExtractFrame` into `CVector[0]`. wiringPanel's cv::Mat
`getImage` overload allocates a local `acvImage tmp_acv`, calls the legacy
path, and `copyTo`s into the destination cv::Mat.

**Option A** — keep the camera path acvImage-internal forever; the shim is
zero copy after the final copy. Acv lib must stay linked via CameraLayer.

**Option B** — switch SNAP_Callback to take a raw buffer (uint8_t*, w, h)
and update each camera backend (BMP, Aravis, MindVision, HikRobot). The
backends already call `ExtractFrame(buf, channels, npx)` with a raw
pointer; the acvImage layer is just a wrapper.

**Recommendation:** B. The cleanest unlink. Cost: one struct + signature
change touching all `CameraLayer_*.cpp` SnapFrame call sites — but they're
small. Risk: the BMP backend in test paths.

**Needs decision:** confirm B. (If A, acvImage can never be fully
unlinked.)

## Q4. acv POD geometry types — Phase 3b

`acv_XY`, `acv_Line`, `acv_Circle`, `acv_LineFit`, `acv_CircleFit` are
used in 651+ refs across MatchingEngine + the FeatureReport struct
(`include/FeatureReport.h`). Removing them is a **schema-affecting** change
— the daemon `--insp` JSON layout numbers and field names come from these
types.

**Needs decision:** When to start. Recommended only after Q1-Q3 land and
the acvImage class is fully unlinked, since the geometry types live in
their own translation unit (`acvImage/`-adjacent) and won't block the
class unlink.

## Q5. Files outside Core0_1 / MatchingEngine

`sidePrj/linemod/linemod.cpp` has 20 acvImage refs / 24 member calls. Not
in any Core0_1 build path AFAICT (`grep -n linemod CMakeLists.txt` →
empty). Confirm it's dead?

**Needs decision:** delete `sidePrj/linemod/` or leave alone?

## Q6. The `transpose(acvImage*, acvImage*)` overload

Still used by `SNAP_Callback` (camera path) for the `img_transpose`
hardware-mounting hack. Goes away if Q3 is resolved as B.

---

## Q7. Validation gap for untested FMs

The committed migration_gate baseline `expected/10221.json` only exercises
FMs that appear in the golden def: `binary_processing_group`, `sig360_circle_line`,
plus subfeature types (arc, aux_line, aux_point, line, measure, search_point,
sign360). The remaining heavy FMs — `FM_gen` (debug-only `cp_main` path),
`FeatureManager_platingCheck`, `FeatureManager_stage_light_report` — are
**not exercised** by migration_gate. Porting their bodies cannot be safely
validated by the existing oracle.

**Needs decision:** for the unexercised FMs, do we:
  (a) Add per-FM golden samples + def files to the regression harness
      before migrating them. Bigger up-front cost, safe.
  (b) Migrate behind a feature flag / dead-code-detect first: delete them
      if no production def references them; migrate if they're live.
  (c) Migrate blind and trust manual smoke. Discouraged.

**Recommendation:** (b) first to drop unused FMs, then (a) for whatever's
left. The cp_main "data/gen_TEST/B.BMP" path for FM_gen smells like a
developer-only debug entry that may be deletable wholesale.

## Q8. Migrating the exercised heavy FMs

`binary_processing_group` and `sig360_circle_line` are the production path
and what migration_gate actually validates. Migrating their bodies means
translating ~15-20 acv helper calls each into cv::Mat ops. Drift risk is
real (e.g., acvBoxFilter vs cv::boxFilter normalization, acvThreshold's
strict `>` vs THRESH_BINARY's `>`, acvComponentLabeling's labeling order,
acvHSVThreshold's HSV ranges 0..256 vs OpenCV's 0..180/0..255). Each
helper needs a per-pair equivalence check before its FM body is flipped.

**Needs decision:** Approach gradient:
  (i) One mega-commit per FM, body-rewrite all at once.
  (ii) Helper-by-helper migration (Option C from Q1): add cv overload,
       swap one call site, gate-validate, repeat. Slow but each step is
       a single 1-line diff.
  (iii) Build a "double-write" test harness: run both acv and cv versions
       side-by-side on the golden, diff their outputs at each helper
       boundary, then flip when delta == 0.

**Recommendation:** (ii). Already laid the groundwork with `cvCloneImage`
in CvBridge (commit e3ecbf78). Each helper migration is bounded, the
existing migration_gate catches numeric drift end-to-end, and small
commits are easy to revert.

## Decisions I'm making without asking

- Continue draining easy / mechanical wins: dead-code deletion, signature
  flips on dispatcher-style FMs, base-class default-bridge consumers.
- Keep validating each commit with migration_gate (190 leaves, 1e-4) +
  daemon_smoke + suite. Any drift = revert.
- Skip `FM_camera_calibration.cpp` (not in CMakeLists, dead file).
- Skip `linemod.cpp` for the duration of Phase 3a (Q5 above).
