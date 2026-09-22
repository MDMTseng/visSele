# SBM locator / trust backlog (deferred options)

As of 2026-09-06. Everything LANDED this cycle is result-neutral or emit-only or a bug
fix (see the per-topic docs). Everything BELOW changes measurements or is a larger build,
so it is parked with the gate it needs. New code is NOT in the field yet (in test), which
is the right window to land measurement-changing items -- but each still needs its gate.

Verification tools that exist: `UI/WebUI/tools/webctl/fleet_eq.mjs` (two cores, per-object
diff + judge FAIL->PASS / PASS->FAIL summary), `sbm_roi_sweep.mjs` (per-recipe acceptance
with augmentation), `_noise_ab.mjs`, `_deform.mjs` (shear/scale), `_trust_fleet.mjs`.

## Already landed (context, not backlog)
- Match speed ~26->20 ms/frame @2thr: T8 local-maxima promotion, T4 idiv->shift, cache-order
  refine, template PCA cache, NMS total-order, batched-uint8 similarityLocal. (SBM_TUNING §6-8)
- Trust score step 1+2 EMIT-ONLY: trust{residual,inliers,alt_residual,code}, gates
  poor_fit/low_inliers/ambiguous_pose; validated (9/239 flags, all true positives).
  (SBM_TRUST_SCORE_DESIGN.md)
- ROI outlier gate: index bug fixed + metric now normal-residual. (verdict-safe)
- shape_roi_spacing / shape_roi_search / shape_roi_prescale def fields; cache_stale locate code.

## Backlog, ranked by value

1. **Trust gates -> judges NA -- LANDED (f30de44e, 2026-09-06), per-recipe opt-in.**
   `shape_trust_na` + `shape_trust_res_max` + `shape_trust_inl_frac`; ambiguous_pose
   defers to an orientation-essential judge; report `trust.forced_na`. Off by default.
   (SBM_TRUST_SCORE_DESIGN.md step 3)
2. **trust NA is an OPERATOR KNOB (4255ac3d, 2026-09-07).** SBM studio 「定位信任」開/關 + 「殘差上限」(default off; 3 px when on). No fleet migration; budgets in `trust_budget.json` are a suggestion per recipe if someone asks. ambiguous_pose never forces.
   Derives res_max per recipe from an in-spec shear/scale/rot/shift/gain set; recipes
   whose own reference fits > 1 px go to REVIEW (they are the true positives). Remaining:
   run over the fleet, migrate the two fields into the recipes that pass, fleet_eq on/off.
3. **ROI de-overlap adoption -- ADOPTED LOCALLY (2026-09-07)**, same migrate list. Was: Sweep found 97 recipes verdict-safe;
   list in `tools/webctl/roi_spacing_adopt.json`, ok11 needs ground-truth review. Speed
   only (not robustness -- SBM_TUNING §11), so adopt where speed matters. Needs def
   migrate to reach the frozen fleet.
4. **Trust coverage (step 4) + clutter (step 5).** coverage kills the min_score clutter-
   count nondeterminism; clutter is the ONLY wrong-edge-lock detector. ~1-3 / 3-6 ms.
   (SBM_TRUST_SCORE_DESIGN.md)
5. **Trust mirror synthesis (step 2b).** For recipes that do not keep NMS alternates
   (shape_nms_angle=360): one extra refineROI from the mirror pose to get alt_residual.
   Plus a shape_alias_deg field + a template self-similarity curve at addModel for the
   alias angles. Residual already catches ok97/98, so lower urgency.
6. **ROI selection leverage bias, within the include area.** Current selector picks
   inward points (CON: lever median 118 vs 287 available). Correct objective =
   include-region (operator, by deformation knowledge) INT high-leverage INT
   distinctiveness INT spacing. Regenerate a few recipes, A/B rigid + deform + trust.
   Only reaches the fleet via re-migrate; and outer points move most under deformation,
   so it must be tested WITH shear/scale, not just rotation.
7. **Feature-count / threshold SF-regenerate sweep.** shape_num_features / strong_thres
   act only at generation (cache_stale otherwise); the aggressive tuner profile sweeps
   them via SF regenerate. (SBM_TUNING §3, sbm_tune.mjs aggressive)
8. **Coarse global top-K (SBM_GLOBAL_TOPK).** -38% coarse but unsafe until the coarse
   score is trustworthy (ok97 true pose scores 51). Revisit after any coarse-score work.
   (SBM_TUNING §7)
9. **Angle margin tightening -- NOT an engineering item (decided 2026-09-06).** The def
   field exists (`matching_angle_margin_deg`); it is the operator's statement about the
   machine, so it stays in the recipe UI and nobody bulk-edits it. For the record: test1
   at +-45 deg is 21 -> 12 ms with no result change, so it is the cheapest lever there is.
10. **Per-frame angle prior.** Biggest coarse lever (~10-12 ms) but per-recipe opt-in,
    needs an alias-period guard + periodic full sweep. (SBM round-2 template agent)

## 2026-09-07: deformed-part findings (test1 sheared/scaled in the object frame, rotated 0..345)
- **ROI outlier gate flips the pose on deformed parts.** 2x-median cuts through the broad but
  CONSISTENT residual distribution of a sheared part (+-2 px), so which half survives depends
  on the pose: 2.2 deg pose flips at 118/120 deg, residual 0.5..2.15 for the same part. Coarse
  init error (3 deg at 120) and fixed correspondences were red herrings (re-matching does not
  help). `SBM_ROI_OUTLIER_K=4` (or no gate): max angle error 2.19 -> 0.47 deg, residual max
  2.15 -> 1.07, undeformed part identical; fleet_eq K2 vs K4 on all own images: 204 identical,
  36 differ at 1e-4 mm, 0 count / 0 verdict changes; fleet _deform2: scale angle drift median
  -35%, shear same; true positives (ok39/42/97/232) still poor_fit with a MORE honest residual.
  **Pending: make 4.0 the default (one constant in roi_refine.cpp).**
- ROI residual vs ICP residual as a deformation meter: ROI std 0.13 (after K=4; 0.34 before),
  ICP std 0.01 over 24 rotations of the same sheared part. ICP is the angle-invariant
  deformation readout; ROI keeps the pose (noise). Cost: icp_ls +2 ms/frame, plain icp +0.3.
  Candidate item: ROI pose + one ICP residual pass as the deformation meter (near item 4).
- ROI under isotropic scale shows a 0.3-1 deg rotation bias that ICP does not (asymmetric point
  set trades scale for rotation). Selection with radial balance (item 6) is the fix.
- Diagnostics landed: SBM_ROI_VERBOSE (per-point src/dst/residual), SBM_ROI_REMATCH[_DEG],
  SBM_ROI_OUTLIER_K, INSP_LOG_KEEP_STDERR to see core LOGE on the bench.

## 2026-09-07: items 6-10 status
- **(7) SBM_PROFILE fixed** (85fde5d3): prints via fprintf, no log-level change; totals are honest now.
- **(8) match exception -> locate.code `match_except`** + reason (85fde5d3). A thrown match no
  longer looks like an empty scene.
- **(9) MODEL3131 "0 objects on CI" -- not a matcher bug.** The CI path crops the scene to the
  def's inspection_region (2328x1035 here) and the 2194 px part's rotation sweep does not fit;
  II does not crop, so it finds the part. The code already logs this (1 line in 100). For a
  bench run use `INSP_SHAPE_NOCROP=1`; for the machine it is the recipe's region to fix.
- **(10) ok01 / ok18 "misses" under deformation -- not locate misses.** The part IS found; an
  orientation-essential judge could not be measured on the 1% scaled / 0.02 sheared frame and
  rejected the detection (locate.code orient_judge, candidates 1..3). Designed behaviour: the
  judge that resolves orientation must measure, or the pose is not trusted.
- **(6) feature count 32-64**: aggressive tuner step 5 already does it per recipe with the
  augmentation gate; run `PROFILE=aggressive sbm_tune.mjs` when a recipe needs the 15-20%.

## Open findings (not features, worth remembering)
- **Run-to-run process-level nondeterminism** on near-min_score clutter: two single-
  threaded runs of the same image differ on 0.50-0.75 clutter (ok68 count). Real objects
  (0.98+) never move. Likely an uninitialized read in the coarse buffers; the fused
  linear-memory writer covers every cell, so it is elsewhere. (SBM_TUNING §8)
- **All-points refine (icp_ls) loses to ROI under noise** even with MAD + direction
  weighting; the scene Canny keeps noise edges. Patch-correlation ROI is the right family
  for a noisy sensor. Env-gated, documented dead end unless scene edge extraction is
  reworked. (SBM_REFINE_ALLPOINTS_DESIGN.md)
- **Sobel/DoG/Gaussian ROI preprocessing** does not help noise (raw patch NCC is near the
  ceiling). Env-gated, off. (SBM_REFINE_ALLPOINTS_DESIGN.md follow-ups)
- **QSV JPEG encode** not worth it for the ROI-crop hot path (CPU offload only, gray is
  the weak spot). Parked. (separate JPEG investigation)
- **Non-{4,8} pyramids matched nothing -- FIXED (sbm f138ac0, 2026-09-06).** The scene
  was padded to a hard-coded 16; line2Dup asserts cols % T at every level, the throw was
  caught in FeatureManager and the def silently located nothing (error 0). Padding is
  now lcm(T_l << l). (`INSP_LOG_KEEP_STDERR=1` is how to see such LOGEs on the bench.)
  **Three levels do not pay, even on big parts**: test1 0.5/{4,8,16} 23.6 vs 21.8 ms;
  ok39 (1344 px) 0.3/{4,8,16} 25.8 vs 22.4 ms and 1.8x the candidates. The extra top
  level's spread (T16 at 1/16 scale = +-100 px in scene px) makes its gate BLUNTER, so it
  passes more, not fewer, and the T8 hop then repeats the work the T8 gate did before.
  `shape_match_scale=1.0` with {4,8}: 48 ms (preprocessing is per-pixel). Smaller scale
  (0.25) is the only lever that moved the fixed cost (ok39 22.4 -> 18.8) but at 1.7x the
  low-score candidates -- same trade as 0.3 on test1. Keep {4,8}; the knob now works
  but nothing measured wants it.
- **SBM_PROFILE inflates wall time ~19 ms/frame** (it flips sbm log to Debug). Ratios are
  fine, absolute totals are not; quote `insp_wall_ms` with the env unset. Fix: print the
  stage line without raising the log level.
