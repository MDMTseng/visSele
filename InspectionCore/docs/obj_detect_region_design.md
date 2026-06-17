# obj_detect — region brightness/Sobel measurement (design & plan)

Status: **DESIGN.** Branch `ct/X1.2_dev`.

## Goal (ct)

A new measurement mode beside the dimensional ones (distance/angle/radius): the user
box-selects a rectangle; the core measures **brightness** and **Sobel edge** over that
region and judges them. Primary use: on backlit glass-disk AOI, place a region in the
surrounding area (absolute image position) and require low brightness/edge there → no
foreign material → prevents false eject (replaces the old intrusion gate with a flexible,
per-region check).

Decisions (ct, 2026-06-18):
- **Stats: mean + max only** (lighter), for both brightness and Sobel edge → 4 values:
  `bright_mean`, `bright_max`, `edge_mean`, `edge_max`.
- **Region: axis-aligned rectangle** in object-frame mm (drag two corners). May render as
  a rotated rect on the image when it follows the pose.
- **Judging: BOTH** — (a) the region self-judges each stat against optional min/max
  bounds (own OK/NG status), AND (b) the stats are exposed as values the judge/calc
  system can reference.
- **Pose options (per region):** `ignore_rotation` (region stays axis-aligned),
  `ignore_translation` (region pinned to the teach/absolute image position → the
  "圖像絕對位置偵測"). Independent flags.

## Def schema (a feature in `featureSet[0].features`)

```jsonc
{
  "type": "obj_detect",
  "id": <int>, "name": "...",
  "pt1": {"x": mm, "y": mm},      // object-frame mm, one corner
  "pt2": {"x": mm, "y": mm},      // opposite corner
  "ignore_rotation": false,
  "ignore_translation": false,
  // optional self-judge bounds (omit a field = unbounded on that side):
  "bright_mean_min": n, "bright_mean_max": n,
  "bright_max_min":  n, "bright_max_max":  n,
  "edge_mean_min":   n, "edge_mean_max":   n,
  "edge_max_min":    n, "edge_max_max":    n
}
```

## Placement at inspection (the 4 object-frame corners → image px)

Corners = (pt1.x,pt1.y),(pt2.x,pt1.y),(pt2.x,pt2.y),(pt1.x,pt2.y) (axis-aligned in object
frame). Each → image px via `TemplateDomain_TO_PixDomain(pt, sin, cos, flip, center, mmpp)`:
- `center` = `ignore_translation ? regOriginPx(teach) : calibCen(live)`
  (regOriginPx = reg_center_mm / mmpp; reg_* from def_image_reg).
- `(sin,cos,flip)` = `ignore_rotation ? (0,1,1)` :
  `ignore_translation ? (reg_sin=-sin(reg_angle), reg_cos, reg_flip)` :
  `(cached_sin, cached_cos, flip_f)` (live pose).

So: follow-both = live pose; ignore_rotation = live position, axis-aligned;
ignore_translation = teach position + teach rotation (pinned where drawn); both = teach
position, axis-aligned. (No deformation morph applied — a region is a rigid window.)

## Stat computation

`eT.getImageCv()` = full-res gray; `eT.getImgOffset()` = crop offset. Transform the 4
corners (subtract imgOffset), take their bounding rect (clamped to image), crop the gray;
build a CV_8U mask via `fillPoly(4 corners in crop coords)`. Then:
- `bright_mean`/`bright_max` = `cv::mean`/`minMaxLoc` of the gray crop under the mask.
- Sobel: `Sobel(crop, dx,1,0)`, `Sobel(crop, dy,0,1)`, magnitude = `|dx|+|dy|` (or hypot);
  `edge_mean`/`edge_max` over the mask.

## Self-judge

status = NA if region degenerate/off-image; else FAILURE if any present bound is violated
(`bright_mean < bright_mean_min` etc.), else SUCCESS.

## Plumbing (mirror the line/measure feature)

- `FeatureReport.h`: `featureDef_objDetect` (id,name,pt1,pt2,flags,8 bounds);
  `FeatureReport_objDetectReport` (def*, 4 floats, status).
- `..sig360..h`: `vector<featureDef_objDetect> objDetectList;` +
  `vector<FeatureReport_objDetectReport>* detectedObjDetects` in the single report.
- `..sig360..cpp`: `parse_objDetectData` (+ parse hook in parse_jobj for "obj_detect"
  array); pool resize/bind in SingleMatching + SingleMatching_shape; `ObjDetect_ReportGen`
  (placement + stats + self-judge); TreeExecution dispatch; FindFeatureDefIndex /
  FindFeatureReportIndex branches.
- `FeatureReport_UTIL.cpp`: `acv_ObjDetectReport2JSON` + add to the single-report JSON.
- Judge exposure (P2): new judge `measure_type` OBJ_DETECT_{BRIGHT_MEAN,BRIGHT_MAX,
  EDGE_MEAN,EDGE_MAX} resolving from `detectedObjDetects`; OR CALC by id.

## Phases

- **P1 (core)** — struct + parse + placement + stats + self-judge + report + JSON.
  Validate via `--insp` (inject an obj_detect feature; check the stats + status in out.json).
- **P2 (core)** — expose the 4 stats to the judge/calc system.
- **P3 (WebUI)** — `obj_detect` shape (rect drag tool) + property sheet (flags + bounds) +
  canvas draw (rect + live values) + defFileGeneration emit + report overlay.
