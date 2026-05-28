# Object Measurement Pipeline — Architecture, Status & Caveats

Scope: `MatchingEngine/FeatureManager_sig360_circle_line.*` (+ `MatchingCore.cpp`
edgeTracking, `Caliper.*`, `EdgeSelect.*`, `ContourGrid.cpp`). Branch
`refactor/inspection-img-streaming`. Companion: `caliper_primitive_locating_design.md`.

This is the reference for the measurement subsystem — how a part goes from image
to pass/fail measurements — plus the running list of caveats to keep in mind as
we develop. Update it as things change.

---

## 1. End-to-end flow (per labeled object)

```
binarize ─► label (CCL) ─► per object region:
  1. ORIENTATION MATCH  (signature: contour_sig | edge_sig)  -> pose: rotate, isFlipped,
       center; cached_cos/sin, flip_f, calibCen, mmpp.
  2. LOCATING-ANCHOR REFINEMENT (定位點): locate `locating_anchor` search points,
       build ConstrainMap (nominal->located warp).
  3. PRIMITIVE LOCATING via the dependency tree (TreeExecution):
       lines, circles, aux points, search points — each located once, deps first.
       Each def point is transformed by cm.convert() (anchor warp) +
       Template->Pix (pose) before locating.
  4. JUDGES: DISTANCE/ANGLE/RADIUS/CALC/... vs USL/LSL (also via TreeExecution).
  5. Re-measure if an orientation_essential judge fails (reDo_orien).
```

## 2. Orientation match (signature)
- `matching_method` def field: `contour_sig` (default, binary silhouette radius
  signature) | `edge_sig` (grayscale gradient-edge signature; exposure/tint/defect
  robust). Output `rotate/isFlipped/similarity`. See memory `project-orientation-match`.
- Speed knob `edge_sig_ray_step` (default 1.5). Build dominates the match (~10us).

## 3. Dependency tree — `TreeExecution`
- **Recursive, memoized lazy DAG eval.** Status reset to `STATUS_UNSET` for all
  features (RESET_REPORT, ~4214), then driver calls `TreeExecution(id)` per
  feature. Guard: `if status != UNSET return cached`. Else resolve deps
  recursively (aux->lines; searchpoint->target_id; judge->OBJ1/OBJ2_id), compute,
  cache. `FindFeatureReportIndex` maps id->report.
- **Cycle guard (DONE, commit 16079f6a):** each branch sets an in-progress sentinel
  (STATUS_NA) right after the UNSET guard, before recursing deps, so a cyclic /
  self-referential def returns NA instead of recursing forever. Sentinel is
  overwritten by the real result on every valid acyclic path -> no behavior change
  for valid defs. Applied to line/circle/aux/searchpoint/judge branches.
- **CAVEAT — O(N^2):** each TreeExecution(id) re-scans all feature lists; fine for
  typical N, could be a map later.

## 4. 定位點 / locating-anchor registration — `ConstrainMap`
- Datum/fiducial-based local fine-alignment ON TOP of the coarse signature pose.
- `locating_anchor` (bool on a search point's anglefollow data) marks an anchor.
  Flow (~4607-4659): after coarse pose, locate all anchor search points; each
  located point mapped back to template domain (`Image_mm_Domain_TO_TemplateDomain`)
  -> `cm.anchorPairs[j] = {from=nominal, to=located}`.
- `cm.convert()` = `convert_polar` (~3227): distance-weighted (Shepard-like)
  interpolation of the `to-from` displacement field. Applied to EVERY line/circle
  def point (`LineMatching_ReportGen:3808`, `CircleMatching_ReportGen:3502`) so
  measurements register to the actual located datums. Then re-measure.
- **CAVEAT — anchors are SEARCH POINTS** -> they use the search-point locating path,
  which is NOT yet caliper-ified (still contour/centroid edge). So 定位點 accuracy
  is currently gated by the old edge method. Caliper-ifying search-point would
  directly improve anchor accuracy -> better registration everywhere.
- **Anchor robustness rework (in progress).** An anchor is a VITAL datum and exists
  for two reasons (user): (1) some primitives are very sensitive to initial position
  but coarse-pose precision is low; (2) parts deform and need correction. So a LARGE
  anchor displacement is LEGITIMATE/expected — abnormality must be judged by
  APPEARANCE, not position. Policy: every `locating_anchor` must be found AND pass
  its gate; ANY failure (or not-found) FAILS THE WHOLE OBJECT (don't measure against
  a bad/partial datum; the old code silently skipped non-SUCCESS anchors at :4693).
  Validation = ZNCC template-patch match (`anchor_patch_zncc`, commit bb212667):
  small taught patch (from the golden image, pose-normalized template domain,
  auto-sized + downsampled at teach time) vs the runtime neighborhood sampled in the
  same template domain; require score >= threshold. ZNCC is brightness-invariant.
  Find stays caliper (sub-pixel, handles the along-edge aperture problem). Edge
  quality (`EdgeSelectInfo`: strength/runnerUp/signed, commit 5cd3ce3d) is the cheap
  fallback gate when no patch is taught yet. Patch is BAKED INTO THE DEF (the live
  `definfo` arrives as a JSON blob over BPG with no image — wiringPanel.cpp:2072 —
  so the runtime core can't rely on the sibling .png). See memory
  [[project-locating-anchor]]. LEFT: def patch schema+parse, template-domain runtime
  sampling, object hard-fail wiring, teach-side patch capture (WebUI).

## 5. Primitive locating — contour (legacy) vs caliper (new)

### Edge sub-pixel (the root accuracy)
- Legacy: `edgeTracking::calc_info` = CENTROID of the gradient profile of a
  1px-wide scan per contour pixel. **CAVEAT — wrong point on non-standard edges**
  (double/nearby edge, shadow, asymmetric, rounded): centroid lands between peaks.
- New toolbox: `EdgeSelect` — peak-detect + selection rule (STRONGEST default /
  FIRST/LAST/MIDDLE/NTH) + polarity (ANY/RISING/FALLING) + adaptive noise floor +
  sub-pixel parabola. On a clean edge STRONGEST == old centroid (back-compat).

### Caliper / section model (mainstream; Halcon/Cognex)
- `Caliper.*`: `caliper_measure` (project/average a length x width section ->
  1D profile -> edge_select -> sub-pixel point, ~sqrt(W) SNR gain),
  `caliper_locate_line` (calipers along p0->p1 + weighted-TLS + MAD outlier
  rejection), `caliper_locate_circle` (radial calipers + Kasa + MAD).
- Validated standalone: ~5x sub-pixel accuracy from projection; defect calipers
  rejected by the robust fit; clean edges unchanged.

### Per-primitive def config (backward compatible; default contour)
```
line/circle def:  "locating":"caliper"   (absent/"contour" => legacy)
                  "caliper":{ "count":N, "width":W }     // search len = margin
                  "edge":{ "method":"strongest|first|last|middle|nth",
                           "polarity":"any|rising|falling", "nth":N, "min_strength":S }
```

### Integration status
- LINE: WIRED (`LineMatching_ReportGen` branches to caliper; commit d5604a4f).
- CIRCLE: WIRED (`CircleMatching_ReportGen` branches at the fit; commit d81a7216).
  Roughness/maxD/minD still use the contour s_points (partial decoupling).
- SEARCH-POINT: WIRED (commit 026eeb00). `locating:"caliper"` runs a single
  `caliper_measure` straddling the edge along `searchVec_nor` (length=margin,
  width=width), offset-corrected, rep.pt in image-px (caller ×mmpp). Default
  contour unchanged. Also lifts 定位點 anchor accuracy. NEEDS RIG VALIDATION.
- AUX-POINT: geometric (intersection/center) — no edge locating, unchanged.

## 6. Reports / backward-compat contract (must preserve)
- Def: featureDef_line{p0,p1,initMatchingMargin,searchVec,MatchingMarginX,...},
  featureDef_circle{pt1,pt2,pt3,initMatchingMargin,outter_inner},
  featureDef_searchPoint{width,margin,anglefollow{position,target_id,angleDeg,
  search_far,locating_anchor}}, featureDef_judgeDef{measure_type,OBJ1/2_id,USL,LSL,...}.
- Report: acv_LineFit{line(anchor,vec),matching_pts,end_pt1,end_pt2,s},
  acv_CircleFit{circle(circumcenter,radius),matching_pts,s},
  searchPoint.pt, single{rotate,isFlipped,similarity,cx,cy}. Existing UIs/projects
  depend on these field names + semantics. New features are ADDITIVE + default-off.

## 7. Consolidated CAVEATS / TODO (live list)
1. **Caliper line+circle integration NEEDS RIG VALIDATION** — coordinate/output
   conventions (image offset, endpoint projections, units) verified by code review
   only; confirm on hardware with a `locating:"caliper"` def before relying on it.
2. ~~Search-point caliper~~ — DONE (commit 026eeb00); improves 定位點 anchors. RIG-VALIDATE.
3. ~~TreeExecution cycle guard~~ — DONE (commit 16079f6a, in-progress sentinel).
4. **No anchor (定位點) outlier rejection** — a bad anchor warps local measurements.
5. **Circle roughness still uses the binary contour** even in caliper mode (partial
   decoupling); full caliper decoupling would drop the contour dependency.
6. **FEATURE_OPENCV gating:** caliper edge_select/Caliper are pure C++ (always
   built), but labeling/bg-flatten/chessboard/lens calib require
   `cmake --preset mac-arm64 -DFEATURE_OPENCV=ON`. Default-off build uses legacy paths.
7. **Setup UX:** per-primitive `edge`/`caliper` config is the contract; a future
   WebUI live caliper-overlay + profile/peak picker is how the user dials it in.
   Core could emit per-caliper diagnostics for a debug client meanwhile.
8. **Backlight comp + img2ideal** are applied during caliper/edge sampling
   (`sampleBackLightFactor_ImgCoord`); calibration (background-evenness, lens)
   feeds the measurement here.

## References
- `docs/caliper_primitive_locating_design.md`
- memories: project-primitive-locating, project-orientation-match,
  project-calibration, project-labeling, reference-shape-matching-sota.
