# SBM Setup Studio — self-contained full-screen authoring modal (plan)

Status: **PLAN + P-A in progress.** Branch `ct/X1.2_dev`. Supersedes the inline
SettingUI controls and the first "reuse main canvas" modal (commit ce472e68).

## Goal (ct)

The shape-based localizer setup must be a **self-contained full-screen modal**: ALL
SBM settings + visualization live inside it; the user configures everything, sees the
result, iterates, and exits when satisfied. Reference: WebUI2
`SingleTargetVIEWUI_Orientation_ShapeBasedMatching.tsx` (+ CoreHub
`InspTar_Orientation_ShapeBasedMatching.cpp`).

Decisions (ct, 2026-06-17):
- **Visualization** = the GENERATED FEATURE POINTS (line2Dup features + ROI points),
  obtained via a core round-trip (like WebUI2's 生成特徵點). Overlaid on the image.
- **Self-contained** = move ALL SBM params into the modal (regions, localization line,
  ROI, shape_match_scale, angle range/offset, matching_face, min_score) + a Save/Exit.
- **Architecture** = a NEW independent canvas inside the modal (à la WebUI2
  HookCanvasComponent), NOT driven by the global redux state machine.

## Phases

### P-A. Core round-trip: extract feature points (DOING)
The localizer trains from the def at parse time. Expose the trained feature geometry so
the UI can overlay it.
- `FeatureManager_sig360_circle_line`: after `trainShapeMatcher` builds the FeatureSet,
  populate members `shape_feat_mm` (line2Dup gradient features) and `shape_roi_mm` (ROI
  refine sample points), both in **object-frame mm** (the def's own frame, same as the
  include polygons). Conversion: feature pixel (cropped-template coords) → full-image px
  (+cropRect.tl) → `PixDomain_TO_TemplateDomain(px, reg_sin=-sin(reg), reg_cos,
  reg_flip_f, originPx, def_mmpp)`.
- Getter `getShapeFeaturePoints(feat, roi)`.
- CLI `visSele --shape-features <def> <out.json>` (headless-validatable): trains and
  dumps `{ "features":[{x,y}...], "roi":[{x,y}...], "reg":{...}, "mmpp":... }`.
- WS command `"SF"` (shape features): takes `definfo` inline (like CI/FI), trains,
  returns the points — the UI consumer. (Mirror the CI handler in wiringPanel.cpp:2341.)
- Validation: points must lie on the part edges within the include region (overlay on
  _n2_shape).

### P-B. Independent canvas in the modal
A lightweight canvas (image + pan/zoom + overlays + polygon/line drawing), local state
(no global state machine). Draws: reference image, include (green fill) / exclude (red
fill) polygons, localization line + origin, generated feature points (from P-A), ROI
points. Tools: draw/edit include, exclude, localization line, ROI override.
Open question: reuse the legacy EverCheckCanvas vs. a fresh minimal canvas — lean fresh
+ minimal to stay decoupled, but assess porting cost first.

### P-C. Consolidated settings + Save/Exit in the modal
Move shape_match_scale, matching_angle_margin/offset, matching_face, shape_min_score,
ROI controls into the modal. A "生成特徵點 / refresh" button (P-A round-trip), a
"測試定位" (optional later), and a "完成/儲存" that writes the def and closes. SettingUI
keeps only the engine toggle + migrate.

### P-D. Polish
Edit/delete individual regions & ROI points; feature-point density tuning; cross-mmpp.

## Notes
- `PixDomain_TO_TemplateDomain` (FeatureManager_sig360_circle_line.cpp:3164) is the
  px→object-mm inverse; `TemplateDomain_TO_PixDomain` the forward (used for the mask).
- WS def-info-inline pattern: wiringPanel.cpp:2215/2381 (`JFetch_OBJECT(json,"definfo")`
  → `AddMatchingFeature`).
- The reused-main-canvas modal (ce472e68) is an interim; P-B replaces its canvas.
