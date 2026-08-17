# Search Point (定位/量測點) — Semantics, Rework, and Debugging

Everything known about the Core0_1 search point (`featureDef_searchPoint`, subtype
`anglefollow`): what it does, the legacy algorithm, the OpenCV rework
(`SearchPointCV`), every finding from instrumenting the golden datapoint, the
current state, and how to run/debug it.

---

## 1. What a search point is

A search point locates a single 2D point on the object by **scanning along a ray**
and finding an edge. It is defined relative to a reference feature (usually a line):

- `position` — nominal point, authored in **mm**, in the template/golden frame.
- `ref[0].id` / `target_id` — the reference feature whose vector orients the search.
- `angleDeg` — rotation applied to the reference vector to get the search direction.
- `margin`, `width` — region extents, authored in **mm** (see §3 for which axis).
- `search_far` (`reverse`) — flip the search direction.
- `locating_anchor` — if true, this point also feeds the ConstrainMap (anchor).
- `locating` — `0` = legacy contour path (default), `"caliper"`/`1` = new grayscale path.
- `edge` block (optional) — `method`, `polarity`, `nth`, `min_strength`.

Outputs (report, in absolute image-mm): `x`, `y`, `status`.

### Units & frames (critical)
- `margin`/`width` are **mm in the def** → converted to **px** in
  `SPointMatching_ReportGen` via `spoint.margin /= mmpp; spoint.width /= mmpp;`
  (`FeatureManager_sig360_circle_line.cpp` ~3595). Per-machine `mmpp` differs
  (different lens/camera), which is why the def is authored in mm.
  `mmpp = 0.008857849` for the golden datapoint (≈112.9 px/mm).
- `position` is transformed before use: `cm.convert(position)` (anchor/ConstrainMap
  warp) → `TemplateDomain_TO_PixDomain(..., cached_sin, cached_cos, flip_f, calibCen, mmpp)`
  (pose). So the pt fed to the scanner is in **pixel image coords**, pose- and
  anchor-corrected.

### Two evaluation passes (important when debugging)
A search point used as a locating anchor is evaluated **twice**:
1. **Prewarp / anchor pass** — `cm` = identity (the ConstrainMap is not built yet).
   pt = pose-only position. **Stable** across runs (does not depend on edge-finding).
2. **Measurement pass** — `cm.convert` applied. pt = anchor-corrected position.
   This pt **drifts** as you change edge-finding code, because the anchors that
   build the ConstrainMap are themselves search points using the same scanner.

For the golden datapoint sp3: prewarp pt ≈ (1695,1214); measurement pt lands on the
arc (≈1649,1203 with the original anchors). The ~46px difference is the anchor
correction (local deformation + residual pose).

---

## 2. Legacy algorithm (`searchPoint_process`, `locating==0`)

File: `MatchingEngine/FeatureManager_sig360_circle_line.cpp` (~1288).

1. Get reference vector from `target_id` (`ParseMainVector`), rotate by `angleDeg`
   (sign-flipped if `flip_f<0`), normalize → `searchVec_nor` (search direction).
   `searchVec = acvVecNormal(searchVec_nor)` = perpendicular = `{-s.Y, s.X}`.
2. `pt = def.position` (already pose+anchor transformed by the caller).
3. Collect object **contour** points in a band around pt:
   `edge_grid.getContourPointsWithInLineContour(line{vec=searchVec_nor, anchor=pt}, width/2, margin, searchDir, m_sections, ...)`.
4. Select the contour point **nearest to `start_line`**, where
   `start_line = line shifted by searchVec*(-999)`.
5. Average contour points within `reng = 2px` of that nearest one (weight
   `1 - (dist-nearestDist)/reng`), report the average.

### What the selection REALLY does (decoded from the math)
`acvDistance(start_line, p)` reduces to `(p - pt)·searchVec + 999`. Minimizing it
picks the contour point with the **most-negative projection onto `searchVec` (the
perpendicular axis)** — i.e. the **lateral-extreme silhouette point** within the
band. It is **NOT** a "first hit along the ray." This is why, e.g., sp3 lands on the
bottom-right corner and sp5 on the rightmost point: they are **silhouette extremes**,
found by taking the lateral extreme over a band.

### Band axes (decoded from `ContourGrid.cpp` + `acvRotation`)
After rotating a candidate into the line frame:
- `out.X = projection onto search dir`  → tested `< epsilonX = width/2`
- `out.Y = projection onto perpendicular` → tested `< epsilonY = margin`

So **`width` = scan DEPTH along the search direction (±width/2)** and
**`margin` = LATERAL half-extent (±margin)**. The band is `width` deep × `2*margin`
wide, centered on pt.

### Legacy also has a directional gradient filter
`getContourPointsWithInLineContour` keeps a contour point only if
`dotP * flip_f > cosSim` where `dotP = sobel × line_vec` (cross product). This is a
**geometric** gradient-vs-search-direction filter (not light/dark polarity). It is
why legacy's contour set on a huge band is a restricted sub-segment, and is the main
reason the grayscale rework still differs on huge-band corner points (sp5/sp20) —
see §5.

---

## 3. OpenCV rework — `SearchPointCV`

Files: `MatchingEngine/SearchPointCV.{h,cpp}` (FEATURE_OPENCV only), called from the
`locating==1` branch of `searchPoint_process` (~1379).

Goal (per user): a robust, grayscale, soft-edge-tunable path that does
**remap → sobel → (mask) → per-row edge → lateral-extreme**, back-compatible with old
defs (opt-in via `locating:"caliper"`), producing a superset report.

### Pipeline (current)
1. **Rectify** a region centered on pt. Axes match legacy:
   - columns `x` = search direction, span `W = width` (±width/2), pt at center `cx`.
   - rows `y` = perpendicular/lateral, span `H = 2*margin` (±margin), pt at center `cy`.
   - sampled with `acvUnsignedMap1Sampling` + backlight factor; off-image rows flagged.
2. **Blur** along the edge only (`cv::blur(Size(1,blurSize))`, blurSize=3) — denoise
   without moving the edge.
3. **Sobel** along the search direction (`cv::Sobel(CV_16S, 1, 0)`).
4. **Mask (currently DISABLED for debugging):** when enabled, keep sobel only on the
   silhouette **boundary ring** = `dilate(mask) AND NOT erode(mask)` (±maskDilate px,
   default 8), so interior gradients are excluded. Mask predicate = "not white
   background" (`isObjectPx`). NOTE: as of this writing `labelImg` is passed `nullptr`
   in the caller (mask off) — see `FeatureManager...cpp` ~1412.
5. **Per-row edge** (`rowEdgeCenter`): strongest gradient blob centroid in the row,
   after polarity + `edgeSuppress` (=10) noise subtraction. Returns centroid, summed
   weight `w`, spread `sigma`, and **peak gradient**.
6. **Strength gate:** keep only rows whose blob peak ≥ `peakFrac * maxPeak`
   (`peakFrac = 0.40`). The true silhouette gradient dwarfs backlight noise, so this
   isolates the real edge **without** needing the mask. (This replaced relying on the
   mask to suppress background-noise edges.)
7. **Lateral-extreme selection** (matches legacy semantics): pick the surviving edge
   with the smallest row `y` (= most-negative perp = `acvVecNormal(s)`, which flips
   with `search_far` exactly like legacy `searchVec`). Then average edges within
   `considerRange` rows (legacy `reng=2`, `alphaKeep=0`), weighted by lateral
   proximity × edge quality (`w/(sigma+1)`).
8. Map the averaged `(ex,ey)` back to image coords:
   `out = pt + (ex-cx)*s + (ey-cy)*perp`.

### Polarity
Default `SP_LIGHT_TO_DARK` (outer silhouette: dark object on bright backlit
background → entering object is bright→dark). Def `polarity` overrides: RISING →
`SP_DARK_TO_LIGHT`, FALLING → `SP_LIGHT_TO_DARK`.

### Current call params (caller ~1405)
`blur=3, suppress=10, considerRange=2 (lateral px), alphaKeep=0, maskDilate=8,
labelImg=nullptr (mask OFF), polarity=SP_LIGHT_TO_DARK default`.

### `search_point_scan` (non-OpenCV fallback)
`MatchingEngine/Caliper.cpp` provides an interim acv-only scan used when
FEATURE_OPENCV is off. Not the primary path.

---

## 4. Validation against the golden datapoint

Harness reproduces the legacy engine. Ground truth = legacy contour report.
Red line: **>4px** divergence is a concern.

Legacy spoints that resolve in the harness (px → the rest are NA because the
harness's contour grid lacked points there):
- sp5: legacy (1637.9, 415.5)
- sp6: legacy (864.0, 1269.3)
- sp20: legacy (1244.6, 1334.2)

Rework results vs legacy (after axes fix + lateral-extreme selection):
- **sp6: ~10px** — effectively reproduces.
- **sp5 / sp20: far off** — root cause in §5.

---

## 5. Open issue: huge-band corner spoints (sp5/sp20)

sp5 `width=13mm≈1468px` (±733 deep) × `margin≈113px` lateral. The band is enormous.

- Legacy's contour set there is a **directional-filtered sub-segment** (only perp
  ∈ [86,111] for sp5; `minPerp=86`). Its lateral extreme = a specific corner.
- The grayscale detection (mask on or off) finds silhouette edges across the **full**
  lateral band (perp ∈ [−112,112]), because the part boundary runs continuously and
  the ±733px-deep scan crosses boundaries everywhere. So the lateral extreme lands at
  the band edge / wrong crossing.
- Polarity (light/dark) alone does **not** fix it — legacy's filter is geometric
  (`dotP*flip_f`), not intensity-based.

Decision pending (user): (A) replicate legacy's directional filter + contiguous
silhouette-section logic for one unified path; (B) route only normal/soft-edge
spoints to the new path and keep legacy contour for huge corner spoints;
(C) redefine a cleaner robust corner finder and validate on real parts (legacy is a
floor, not a contract).

---

## 6. Edge-finding findings (sp3 debugging)

- With **mask ON**, edges were constrained to the object but the wrong silhouette
  side could be picked.
- With **mask OFF + no strength gate**, the per-row "strongest blob" picked tiny
  **background backlight noise** in every row → green edges scattered all over the
  background. Fix: the **peak strength gate** (§3.6).
- After the gate, edges cleanly track the arc (no noise).
- Polarity pinned to outer silhouette (§3 Polarity) so the band's outer edge is used
  consistently rather than flipping between the band's two gradients.
- Near the arc apex the per-row scan runs nearly parallel to the edge → weak gradient
  → dashed/gappy detection there (expected; apex still covered).

### The lateral-offset / pt-drift caveat
On the **prewarp** pass the arc sits at the band edge because pt is ~46px laterally
off the arc (the offset the anchor warp corrects). On the **measurement** pass the arc
is centered, but that pt drifts as edge-finding changes the anchors (anchor coupling).
For reproducible edge-finding debugging, use the **prewarp** dump (stable pt); for
seeing the centered arc, use the measurement dump.

---

## 7. How to run / debug

Headless golden-sample harness. **Run from `Core0_1/`** so the relative calib path
`data/default_camera_param.json` resolves (else the calib map is uninit and all
line/circle fits return NaN).

```bash
# build
cmake --build /Users/mdm/workspace/visSele/InspectionCore/build/mac-arm64 --target visSele -j8

# run (from Core0_1/)
cd /Users/mdm/workspace/visSele/InspectionCore/Core0_1
../build/mac-arm64/visSele --insp "data/10155  3G2570090B-1.png" /tmp/def_sponly_cal.hydef /tmp/sp_cal_out.json
```

`--insp <img> <def.hydef> <out.json>` — headless inspection, writes report JSON
(implemented in `Core0_1/wiringPanel.cpp`, cp_main ~4912).

### Debug env vars
- `SPCV_DUMP=1` — writes `/tmp/spcv_pt<X>_<Y>_<W>x<H>.png` (rectified region: green =
  per-row edges, blue circle+cross = final, red tint = masked-out), plus
  `spcvraw_pt*` (raw gray), `spcvmask_pt*` (mask), `/tmp/spcv_imgpts.csv`
  (image-space BOX/EDGE/FINAL rows for full-image overlay), and `[SPCV] ... eps=...`
  stderr lines (perp/along extents, yMin).
- `SP_PT_DUMP=1` — `[SPPT]` lines: pt transform chain (raw def pos → after
  `cm.convert` → after pose, px) + pose/calibCen/mmpp.
- `SP_LEGACY_DUMP=1` — `[SPLEG]` lines from the legacy contour path: searchVec,
  band extents, section counts, nearest idx/dist, final, perp/along ranges.

### Defs used
- `/tmp/def_sponly_cal.hydef` — search points with `locating:"caliper"` (new path).
- `/tmp/def_line1_contour.hydef` — pure legacy (`locating` unset), for `SP_LEGACY_DUMP`.
- Golden image: `Core0_1/data/10155  3G2570090B-1.png`.

### Full-image overlay (after a SPCV_DUMP run)
`/tmp/spcv_imgpts.csv` has `FINAL/BOX/EDGE` rows in image coords; render with a small
python/cv2 script to see picks on the actual part (yellow=nominal pt, green=edges,
blue=final, red=legacy target where known).

---

## 8. How to test a search point and read the result correctly

### A. Run and read the report
1. Build, then run `--insp` from `Core0_1/` (§7). Output JSON is written to `out.json`.
2. Each search point report has `id`, `status`, `x`, `y` in **absolute image-mm**
   (px = mm / mmpp, mmpp ≈ 0.008857849). `status==0` (SUCCESS) means a point was
   found; NA means no edge passed.
3. To compare against pixels (e.g. dumps, legacy): `px = mm / mmpp`.

Extract results (example):
```python
import json
d=json.load(open('/tmp/sp_cal_out.json')); mmpp=d['mmpp']
def walk(o):
    if isinstance(o,dict):
        if 'x' in o and 'y' in o and 'id' in o and 'cx' not in o and 'r' not in o:
            print('sp%s st=%s  mm=(%.3f,%.3f)  px=(%.1f,%.1f)'%(
                o['id'],o.get('status'),o['x'],o['y'],o['x']/mmpp,o['y']/mmpp))
        for v in o.values(): walk(v)
    elif isinstance(o,list):
        [walk(v) for v in o]
walk(d)
```

### B. What a CORRECT result looks like (acceptance criteria)
A search point is behaving correctly when **all** of these hold:

1. **Region is on the feature.** On the **measurement pass** dump the target
   feature (e.g. sp3's arc) is roughly **centered** in the rectified region, not
   clipped at an edge. (On the prewarp pass it may be offset — that's expected; see
   §6.) Check the `[SPCV] ... eps=... rowPerp[..]` line: the edges should straddle a
   reasonable lateral range, not sit only at `±margin` (band boundary).
2. **Edges land on the intended silhouette.** In the `spcv_pt*.png` dump the green
   per-row edges should track **one** consistent edge (the outer silhouette for a
   dark-on-bright part), not scatter across the background and not flip between the
   two sides of a thick band. If you see green dots in the background → strength gate
   too low / wrong region. If they hug the inner band edge → polarity is wrong.
3. **Final pick is on the target point.** The blue marker should sit on the
   silhouette at the corner/extreme the point is meant to measure (sp3 = bottom-right
   arc apex, sp5 = rightmost point, etc.).
4. **Value matches expectation.** See C.

### C. How to judge the value (expected result)
There are two reference standards, in order of authority:

1. **Legacy report = the floor.** Reproduce the legacy contour result first. Run the
   same image with a **legacy** def (`locating` unset) — or read the known legacy
   values (§4) — and compare:
   - `< ~1px`: faithful reproduction (target for normal points; e.g. sp6 ≈ 10px is
     borderline, < 1px is the goal once edge-finding is final).
   - `> 4px`: **red line** — treat as a regression unless there is a confident,
     written reason the new answer is *better* than legacy (legacy is "looks right,"
     not ground truth). Principled deviation is allowed but must be justified and
     later validated on real parts.
2. **Real-part / metrology truth = the ceiling.** After reproducing legacy, validate
   on real harsh-condition parts (M4). The new path is allowed to beat legacy here;
   that's the whole point of the rework (robustness to lighting/deformation).

Caveats when judging values (lessons learned):
- **Measured values are ground truth, not RMS/pixel positions.** A "cleaner-looking"
  fit that changes a downstream measure (mm/pass) the wrong way is wrong.
- **Don't diagnose legacy "bugs" from raw numbers.** Some legacy results that look
  odd (e.g. two search points at the same location) are geometrically correct (the
  bottom-right IS the rightmost point there). The user's visual is authoritative.
- **Compare the same pass.** Use the measurement pass for final values; the prewarp
  pt is offset by the anchor correction and is for stable debugging only.

### D. Step-by-step test loop
```bash
cmake --build /Users/mdm/workspace/visSele/InspectionCore/build/mac-arm64 --target visSele -j8
cd /Users/mdm/workspace/visSele/InspectionCore/Core0_1
IMG="data/10155  3G2570090B-1.png"

# 1) new (caliper) path, with dumps
SPCV_DUMP=1 SP_PT_DUMP=1 ../build/mac-arm64/visSele --insp "$IMG" /tmp/def_sponly_cal.hydef /tmp/sp_cal_out.json

# 2) legacy reference (for the same points)
SP_LEGACY_DUMP=1 ../build/mac-arm64/visSele --insp "$IMG" /tmp/def_line1_contour.hydef /tmp/sp_legacy_out.json

# 3) compare mm/px per id (script in §8.A), confirm < red line
# 4) eyeball the dump images: region centered, edges on outer silhouette, final on target
```

---

## 9. Status summary

- Range/axes bug — **fixed** (`width`=depth, `margin`=lateral; region centered on pt).
- Selection — **lateral-extreme** (correct legacy semantics), not first-hit.
- Strength gate — **added** (isolates real edge without the mask).
- Polarity — **pinned** to outer silhouette (overridable).
- Mask — **temporarily disabled** (`labelImg=nullptr`) for debugging.
- sp6 reproduces legacy (~10px); **sp5/sp20 open** (huge-band directional-filter gap).
- Anchor coupling: measurement pt drifts with edge-finding changes — separate
  locating-anchor robustness concern.

All changes are opt-in via `locating:"caliper"`; defs without it use the unchanged
legacy contour path.
