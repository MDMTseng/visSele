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

1. **Wire trust gates to force judges NA (per-recipe).** Today emit-only. poor_fit /
   low_inliers -> NA is straightforward. ambiguous_pose -> NA MUST defer to a passing
   orientation-essential judge (that judge is how a symmetric part is legitimately
   resolved; blanket NA false-rejects good symmetric parts). Gate: per-recipe enable +
   fleet_eq 0 FAIL->PASS. This is the payoff of the trust work (不可檢錯).
2. **poor_fit threshold per-recipe with a DEFORMATION budget.** A global 1.0px false-flags
   most recipes under 1-2% scale / 0.02 shear (SBM_TRUST_SCORE_DESIGN.md deformation
   caveat: 9 -> 179/239 at 1% scale). Add shear+scale to sbm_roi_sweep's acceptance axes
   so each recipe's poor_fit sits above (noise floor + in-spec deformation). Prereq for #1.
3. **ROI de-overlap adoption (shape_roi_spacing).** Sweep found 97 recipes verdict-safe;
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
