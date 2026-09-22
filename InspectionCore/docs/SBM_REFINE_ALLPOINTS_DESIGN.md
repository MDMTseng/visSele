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

## Phase A result (2026-09-06): robust all-points ICP does NOT beat ROI under noise

Implemented the MAD residual gate + normal-direction weighting on the inverse-ICP path
(icp_refine.cpp, ICPConfig robust_mad / robust_k / weight_by_dir; SHAPE_REFINE=icp_ls +
SBM_ICP_ROBUST=1 SBM_ICP_WEIGHT_DIR=1). Noise A/B vs the ROI default (_noise_ab.mjs,
sigma 0-20, 3 seeds, pose vs each core's own unperturbed):

- Aggregate p95 pose drift at sigma 10: ROI 4.5 px vs icp_ls 24 px -- icp_ls worse.
- Per recipe it is MIXED, not uniformly worse: comparable/better on several (ok11 sig10
  0.030 vs 0.074 px, ok68 0.028 vs 0.052), but CATASTROPHIC on big/fragile parts (ok42
  0.51deg/24px, ok00 6px, ok09 11px at sig20). Those dominate the p95.

Root cause: the refine is scene-driven inverse ICP over Canny edges of the SCENE. At
sigma 10+ the noisy scene grows spurious Canny edges everywhere, so the correspondence
population is majority-noise; the MAD threshold is then computed FROM the noise and
cannot separate it, and the direction gate lets ~half of random-normal noise edges
through. ROI wins because matchTemplate correlates the whole template PATCH against the
scene (a patch average is inherently robust to zero-mean noise), not single nearest
edges. This confirms the pre-existing result ("ICP not faster, less stable under noise").

The gap is NOT the LSQ robustness (that is what was added and it did not fix it); it is
the SCENE edge extraction keeping noise edges. To make all-points competitive would
need a noise-suppressing sub-pixel edge extraction on the scene (HALCON's MinContrast
threshold suppresses noise vectors BEFORE the fit; our EdgeScene Canny at fixed 20/60
does not). That is a larger change with ROI already ahead in this regime.

Verdict: keep RefineMode::ROI as the default and the general choice. The all-points
path stays env-gated and documented; it is a per-recipe option at best (mixed results),
never a blanket replacement, and the catastrophic big-part divergences (ok42 24px) are
uInsp-fatal, so any adoption needs the drift + verdict-flip gate per recipe. Phases B-D
are not worth pursuing unless the scene edge extraction is reworked first.
The empirical answer to "is ROI near-optimal": for THIS noisy, patch-correlatable
regime, the patch-correlation ROI refine is the better-suited family, and the industrial
all-points edge LSQ (great when edges are clean) is not a free upgrade here.

Note (high-contrast parts): the fleet parts are clean, black-and-white edges, yet
edge-ICP still loses under noise -- because the weakness is not the part's edges but the
SCENE noise edges Canny fires on everywhere. A clean part does not help when the image
noise adds its own edges around it. That is exactly why a patch-correlation refine (ROI)
is the right family for a noisy sensor even on high-contrast parts: it averages a patch,
it does not chase individual edges.

## Follow-up (2026-09-06): does Sobel-first make the ROI refine more noise-robust? No.

Matched the ROI patch on gradient magnitude instead of raw intensity (roi_refine.cpp,
SBM_ROI_SOBEL=1 plain Sobel, =2 Gaussian(3)+Sobel/DoG), same noise A/B. p95 pose drift
(px) vs raw-intensity ROI: raw 1.06/4.53/4.85/4.95 at sigma 5/10/15/20; DoG
4.33/4.12/12.72/16.0 (rot diverged 172deg at 20); plain Sobel 9.89/15.16/17.14/15.87.
Plain Sobel much worse everywhere; DoG marginally better only at sigma 10, then loses /
diverges. Sobel is a differentiator -- it multiplies high-frequency noise power, and a
3x3 Gaussian pre-blur cannot tame it at high sigma. Raw-intensity patch NCC averages the
zero-mean noise and stays the robust choice. Same lesson as edge-ICP: gradient/edge
domain is noise-fragile, patch-intensity correlation is noise-robust. Knob left
env-gated, default off, for a low-noise high-precision case where DoG's sharper peak
might pay.

## Follow-up 2 (2026-09-06): Gaussian pre-blur before the ROI NCC? Neutral, slight risk.

SBM_ROI_BLUR=<k> low-passes both patch and window before the raw-intensity NCC (pure
denoise, not a differentiator). p95 pose drift vs raw: ~unchanged at sigma 5-15 (raw
4.53/4.85 vs blur3 4.43/4.44 at sigma 10/15), but at sigma 20 blur3 DIVERGED on a
fragile recipe (178deg / 17.5px) that raw did not; blur5 12px. Not a win.
Reason: the ROI NCC already correlates ~900 px of the 30x30 patch -- that averaging IS
the noise suppression, so a 3-5px Gaussian adds almost nothing and can soften/shift the
peak enough to tip a marginal part into a wrong lock.

Conclusion of the refine-preprocessing line (Sobel, DoG, Gaussian): the raw-intensity
patch NCC is already near its noise-robustness ceiling; no patch-domain preprocessing
improves it. The remaining high-noise failures are on geometrically fragile parts, which
no refine-input transform fixes -- those need coarse-stage / geometry work. Both knobs
(SBM_ROI_SOBEL, SBM_ROI_BLUR) stay env-gated, default off.
