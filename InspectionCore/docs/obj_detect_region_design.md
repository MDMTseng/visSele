# obj_detect — region brightness/Sobel measurement (design & plan)

Status: **P1 + P3 SHIPPED**, plus the clean-space extension below (2026-08-07).
P2 (expose the stats to judge/calc) still not done — `OBJ_DETECT` is declared in the
FEATURETYPE enum and has no users.

---

# Clean-space extension (2026-08-07)

Two gaps between what P1 shipped and what a "this space must be clean" check needs.

## 1. The region's verdict never reached the part

P1 measured the region, self-judged it, put it in the report JSON and drew it in the
WebUI — and stopped there. `ImgPipeProcessCenter_imp` builds the part verdict from
`InspStatusReduce(judgeReports)` alone, so a region could sit on screen bright red
while the part it condemned went out the OK chute. `detectedObjDetects` now folds in
at the same place.

It folds in **there** and not into `judgeReports` on purpose: a region answers a
different question from every other feature. Not "is this part within tolerance" but
"was the field clean enough for that question to mean anything".

## 2. `on_fail` — and why the default is NA

```jsonc
"on_fail": "na"   // default, may be omitted
"on_fail": "ng"
```

A dirty clean-space region usually means something is lying in the field of view.
That makes the measurement of *this* part untrustworthy; it does not make the part
bad. `STATUS_NA` is absorbing in `InspStatusReducer`, so the part drops out of the
verdict entirely: no actuation, it recirculates, and it gets measured again on a
clean field. Only a region the operator explicitly marks `on_fail:"ng"` ejects.

This is the same intent the legacy `extra_area_ratio < 0.1` gate had (don't eject on
a contaminated frame) without its two problems: it was one global rule with a
hardcoded constant, and with no ROI it made *every* frame NA.

## 3. `dark_thresh` — the dark-area statistic

```jsonc
"dark_thresh": 128,        // grey level; pixel < thresh counts as dark. Absent = off.
"dark_area_min": n,        // mm^2
"dark_area_max": n,
"dark_ratio_min": 0..1,    // dark px / region px
"dark_ratio_max": 0..1
```

reported as `dark_ratio` and `dark_area_mm2` (both omitted from the JSON when
`dark_thresh` is absent — cJSON writes a bare `NaN` token that `JSON.parse` rejects).

**Neither mean nor max answers "is anything sitting here."** On a clean bright field
a 0.3 mm speck moves `bright_mean` by a fraction of a grey level, and `bright_max`
fires on a single noisy pixel. Counting pixels under a threshold is the question that
was actually being asked. `dark_area_mm2` is the operator-facing one — "no speck
bigger than 0.5 mm²" is a sentence you can hold someone to; a ratio is not.

Every field is **per region**. Two regions in one def can hold two different
thresholds, two different area limits, and two different `on_fail` — verified.

### Measured before downsample, and that is not an optimisation detail

`downsample` uses `INTER_AREA`, which averages. A 2 px speck seen through
`downsample:4` comes out at 1/16 of its contrast and walks straight back over the
threshold — the setting that exists to make the region fast would erase the one thing
this measurement exists to catch. So the dark count runs on the full-resolution crop,
*before* the resize. One `threshold` + `countNonZero` is cheap enough not to need the
help. Brightness and Sobel still measure after the downsample, as before.

## What is verified, and what is not

Verified end-to-end through `visSele --insp` (real parse → place → measure →
self-judge), on `10221 BOS-LT12BH4211 SORTING_bk.png`:

- `dark_ratio` is monotonic in `dark_thresh` over a 16→240 sweep
  (0.00011 → 0.036 → 0.054 → 0.068 → 0.088) — it is counting pixels under a
  threshold, not doing something else that happens to correlate
- `dark_area_mm2` tracks it (1.94 mm² at thresh 128)
- absent `dark_thresh` → neither field is emitted
- a bound moved across the measured value flips the region status, and flips it to
  what `on_fail` asked for: absent → NA, `"na"` → NA, `"ng"` → FAILURE
- `dark_area_max` trips on the same path as `dark_ratio_max`
- two regions, two thresholds, independent results

**Not verified: the fold into the part verdict (§1).** That code is in
`ImgPipeProcessCenter_imp`, which the `--insp` harness does not run — only the live
image pipeline reaches it. It needs one run on the machine with a def carrying a
region, checking that a tripped region moves `uInspStatus` to NA and the part is not
actuated. Until that run happens this half is correct by construction only.

---

## Original P1/P3 design follows.

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
