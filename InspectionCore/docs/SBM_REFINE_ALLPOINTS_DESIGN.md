# Design: all-points weighted robust pose refine (the HALCON/PatMax step)

Draft 2026-09-06. Goal: replace the per-point-matchTemplate ROI-subset refine with the
step every metrology-grade locator uses -- refine the pose on ALL model edge points by
robust least squares on point-to-normal-line distances. Grounded in what the repo
already has; this is evaluate-and-harden, not a from-scratch rewrite.

## Why (recap, see SBM_TUNING §10)

HALCON `find_shape_model` (least_squares) and Cognex PatMax both refine on the whole
model contour, weighted, with a robust sigma, minimizing displacement along the edge
normal, 3-4 iterations. They do NOT select a handful of points. The current
`RefineMode::ROI` (roi_refine.cpp) runs a `cv::matchTemplate` per selected point over 8
points -- a line2Dup-community compromise because per-point matchTemplate is dear. That
compromise is the source of the overlap problem, the distinctiveness-screen question,
and the 8-point outlier-gate fragility. Dropping the subset dissolves all three.

## What already exists (do not rebuild)

- `RefineMode::ICP` / `ICP_Subpixel` and `icp_refine.cpp` (883 lines): inverse ICP over
  a template `EdgeScene` (icp_refine.h:48) -- Canny edge map + per-pixel normals + a
  distance field (closest_x/y) + optional 2nd-order facet SUBPIXEL edge positions
  (`EdgeScene::build(..., subpixel)`). `buildTemplateScene` is called once at addModel;
  `cached_templ_scene` / `templ_scene_valid` on FeatureSet (shape_matcher.h:114) cache it.
  `refineInverse(templ_scene, scene, ...)` iterates point-to-line + a small point-to-point
  term (kEdgeP2PWeight 0.01, kCornerP2PWeight 1.0).
- So the "all model points -> nearest subpixel scene edge -> normal-distance LSQ, N iters"
  machinery is present. It is NOT the default (default is ROI, FeatureManager
  buildShapeMatcher ~8666) and is reachable only via `SHAPE_REFINE=icp` (diagnostic).

## Gaps vs the industrial step

1. **No robust rejection.** icp_refine.cpp has weights but no median/sigma outlier gate
   (grep: no median/huber/tukey). HALCON keeps only correspondences within a robustly
   estimated sigma; PatMax weights by force distance. Add a per-iteration robust gate:
   after building correspondences, drop or Huber-weight those whose normal residual
   exceeds k * MAD(residuals). This is the single most important addition -- it is what
   makes the all-points solve beat the 8-point one on gross errors (wrong-edge/mirror).
2. **Weighting.** Add per-correspondence weight = gradient magnitude (PatMax) and/or the
   direction-agreement cos(dtheta) between model and scene edge normals (rejects a point
   that landed on a differently-oriented edge). Optionally fold in the existing
   distinctiveness (lock_major) but that is per-selected-point today; for all-points use
   the cheap per-correspondence direction agreement instead.
3. **Correspondence search range.** ROI uses search_half 15 px (the capture budget the
   tuner reasons about). The ICP distance field has its own max_dist (buildTemplateScene
   default 20). Must reconcile so the capture range the coarse tuner assumes still holds.
4. **Coverage + clutter scores** (separate, later): count model points with a valid
   in-range correspondence (coverage) and scene edges near the model with no model point
   (clutter). Low coverage or high clutter = the never-miss alarm (uInsp 不可檢錯).

## Plan

**Phase A -- harden icp_refine (code, self-contained).**
- Add robust MAD gate + gradient/direction weighting to the `refineInverse(EdgeScene, scene, ...)`
  path only (leave the ICP_Subpixel maths otherwise intact). Config fields on ICPConfig,
  off by default so existing behaviour is untouched.
- Env `SHAPE_REFINE=icp_ls` (or a mode enum) to select the hardened all-points path for
  evaluation, plus SBM_ICP_ROBUST_K, SBM_ICP_WEIGHT for tuning. Match-time, so it works
  on frozen defs with no re-migration.

**Phase B -- evaluate against the current ROI refine (no rebuild loop; uses existing tools).**
- fleet_eq.mjs baseline(ROI) vs all-points on two cores: object-count changes, judge
  FAIL->PASS / PASS->FAIL (the gate already there), pose deltas, time.
- Robustness: extend sbm_roi_sweep.mjs (or a sibling) to run the augmentation set
  (rot/shift/gain/noise) and compare worst-case judge margin, ROI vs all-points. The
  claim to test: all-points is EQUAL-or-better on margin and strictly better on the
  gross-error recipes (CON coil / mirror, ok68 clutter) with no verdict flip.
- Timing on the 2-core emulation: all-points ICP over the distance field is ~O(model pts
  x iters) with no per-point matchTemplate; expected comparable to or cheaper than the
  8-point ROI (each ROI point is a 60x60 matchTemplate). Confirm with SBM_PROFILE.

**Phase C -- adopt per recipe, never blanket.**
- Expose `shape_refine_mode` def field (roi | icp_ls). Like shape_roi_spacing, this
  changes measurements, so adopt only per recipe where the sweep shows verdict-preserving
  + equal/better robustness. The 97 roi_spacing adopts and this are the same acceptance
  machinery.

**Phase D -- (optional, later) continuous-direction fine re-score + coverage/clutter.**
- Re-score fine-level candidates with the unit-vector dot product + MinContrast floor
  (repeatability under occlusion), and emit coverage/clutter as locate fields. Separate
  from the refine change; sequence after A-C.

## Risks / non-goals

- Changes measurements on ~all recipes (like every refine change) -> per-recipe sweep
  gate is mandatory; not a default flip.
- Not a coarse-stage change; the coarse capture-range budget (SBM_TUNING §2) still bounds
  how far off the init pose may be, so the correspondence search range must match it.
- Not selection: this removes the subset question rather than answering it; roi_spacing /
  weight_by_lock / the distinctiveness screen become moot on recipes that adopt icp_ls.
- Keep RefineMode::ROI as the default and the fallback; icp_ls is opt-in until the fleet
  sweep says otherwise.

## First concrete step

Add the MAD robust gate + direction-agreement weight to `refineInverse(EdgeScene, ...)`
behind ICPConfig flags and a `SHAPE_REFINE=icp_ls` selector, then run fleet_eq ROI vs
icp_ls with the verdict-flip summary on the 247 fleet copies -- that one A/B says whether
the all-points path is worth carrying to Phases B-D.
