# Design: localization trust score (the never-miss alarm)

Draft 2026-09-06. A per-located-object score set that flags a pose which "succeeded"
(passed min_score) but is actually wrong -- mirror/alias lock, wrong-edge lock, clutter
theft, low coverage. Priority: uInsp 不可檢錯 -- a false-confident wrong pose is the
worst outcome, so gates are HARD and independent, never averaged into one number.

## What already exists (grounded)

- Only 8 refine points (`shape_matcher.h:24` kDefaultOptPointsPublic, used
  shape_matcher.cpp:2588) -- every "fraction" is k/8, coarse but present.
- `refine_residual` = MEAN |point-to-normal-line| over gated points (roi_refine.cpp
  ~679); the 2x-median outlier cut applies to the SOLVE not the residual, so a
  clutter-stolen point inflates it -- exactly the signal we want. It is computed
  (shape_matcher.cpp:2675) then DROPPED (only used for face arbitration ~2704); never
  reaches the report (FM passes only m.score -> similarity).
- **Alternates are already refined every frame for free**: NMS keeps up to 3 alternate
  poses per location (ALT_MAX 3, ALT_MIN_DEG 10, ALT_SCORE_GAP 10, shape_matcher.cpp
  ~2364) and the mirror face is kept apart; each goes through refineROI, so the residual
  of every alternate and the mirror already exists. CON's mirror (6/20 frames wrong at
  coarse 0.99) is the proof residual separates what coarse cannot.
- `score_floor` per point computed at addModel (shape_matcher.cpp ~1895) then UNUSED
  (the gate roi_reject_low_score is default off). So an inlier count is one comparison away.
- No symmetry/alias-period field; matching_angle_margin only bounds the search.

## Components, ranked (failure caught / cost)

- **A. Ambiguity margin** (highest value): primary residual vs best alternate/mirror
  residual. Catches mirror, 15deg/160deg alias, and the weak-coarse-but-right case
  (ok97: alias 0.872 vs true 0.992). Misses wrong-edge (both basins fit). ~0 cost when
  the alternate already exists; one extra refineROI (8 pts x 3 iters, sub-ms) if a
  mirror/alias pose must be synthesised. This is the only component that addresses
  不可檢錯's worst case; coarse similarity misses it entirely.
- **B. Fit residual (px)**: clutter theft on 1-3 points, gross mismatch. Misses
  wrong-edge (all 8 agree on the wrong turn -> residual ~0). Cost 0 (already computed).
- **C. Inlier count k/8** (points with score >= score_floor*pct, WITHOUT dropping them
  so measurements do not move): clutter, occlusion, wrong-edge on distinctive points.
  Cost 0 once counted; names which points were stolen.
- **D. Coverage** = fraction of ALL model edge points with a scene edge within d px along
  the normal at the final pose: clutter objects at 0.50-0.56 (the §8 count
  nondeterminism), partial part, gross mislocation. Needs the all-points set
  (fs.cached_templ_scene already exists) not the 8; ~1-3 ms.
- **E. Clutter** = scene edges in the model bbox with no model edge nearby: burr/foreign
  object, and the ONLY component that sees wrong-edge lock (extra coil turns = unexplained
  edges). Dear: warp bbox to template frame, Sobel, threshold, lookup in a model-edge
  distance transform precomputed at addModel; ~3-6 ms.
- F. Coarse similarity: adds nothing once B/D exist; keep as the min_score admission only
  (0.99 at the mirror pose, 51 at ok97's true pose -- both over- and under-trusts).

## Combine: independent hard gates

Not a weighted 0-1 score (0.1 px residual averaged with 0 ambiguity margin would read
"0.8 trust" for a coin-flip pose). Each is disqualifying alone:
`poor_fit` residual > res_max; `low_inliers` inliers < min; `ambiguous_pose` (below);
`low_coverage`; `high_clutter`. Any gate -> object reported with trust.ok=false and its
judges forced NA (never PASS), the way an orientation-essential rejection already is not
a locate failure. Thresholds: global defaults, overridden per recipe by a sweep that
records each component's distribution on good frames and sets the gate at ~3x its p95,
never below the global. Report the raw values always so the screen shows why.

## Ambiguity score detail

amb_margin = min over alternates (residual_alt - residual_primary); amb_ratio =
residual_alt_best / residual_primary. Alternates from, cheapest first: (1) existing
same-group + face-pair members (free); (2) mirror synthesis when has_flip but the mirror
was suppressed -- one refineROI from the primary pose with the flipped set; (3) alias
synthesis from (x,y,angle+p_k) for alias angles p_k. p_k from a template self-similarity
curve computed once at addModel (rotate templ 0..360, self-correlate; peaks above ~0.8 of
the main lobe = alias angles: 180 for the symmetric spring, 15 for CON, 160 for ok97),
stored as def field shape_alias_deg (studio-visible, operator-overridable); absent ->
compute at load + log; no secondary peak -> no synthesis, zero cost for asymmetric parts.
Flag ambiguous if amb_margin < 0.3 px OR amb_ratio < 1.5. A refine that returns to the
same basin (angle within ALT_MIN_DEG) is not an alternate.

## Report / codes

- MatchResult: add n_points, n_inliers, alt_residual (-1 none), alt_angle_diff,
  alt_is_mirror, coverage, clutter (-1 not computed).
- FeatureReport single: add a `trust {residual, alt_residual, coverage, clutter,
  inliers, n_points, code}` filled in the consumer loop before SingleMatching_shape;
  emit as "trust":{...} ALWAYS (useful even when fine).
- locate.code gains `ambiguous_pose` only for "the only object's every pose is ambiguous"
  (nothing else reportable), stamped in the empty-slot manner of coarse_only/cache_stale.
  coarse_only recipes get trust.code = untrusted_coarse (no residual exists).

## Validation

Positive, must flag 100%: CON 20-frame mirror set; ok97/ok98 with SBM_GLOBAL_TOPK on
(forces the 160deg alias); ok08 (0.58), ok39; CON with SHAPE_ROI_SEARCH=30 (coil-turn
lock -- the honest test of E; A/B expected to miss it). Negative false-flag budget:
fleet_eq over 247 own-image + AUG set, < 1% of frames flagged and 0 verdict flips
(fleet_eq already prints FAIL->PASS/PASS->FAIL). stability_sweep to confirm the trust
values are run-stable.

## Minimal viable, in order

1. Plumb what exists (zero cost): refine_residual + group into the report; n_inliers
   counted without dropping points. Gates poor_fit, low_inliers.
2. Ambiguity from free alternates + mirror synthesis -- catches CON and ok97 for at most
   one extra refine.
3. Alias curve at addModel + alias synthesis -- closes the "alternate never offered" hole.
4. Coverage on the all-points set (~1-3 ms) -- kills the §8 clutter-count nondeterminism.
5. Clutter (~3-6 ms) -- the only wrong-edge-lock detector; per-recipe opt-in on 2 cores.

Steps 1-3 are the minimal viable score: under a millisecond, catch every listed failure
except wrong-edge lock (which needs 5). Start there.

## Step 1 landed + validated (2026-09-06): residual + inlier trust, emit-only

Plumbed refine_residual and an inlier count (points agreeing within 2x median residual,
counted WITHOUT dropping them so measurements are unchanged) from refineROI through
MatchResult to a "trust":{residual,npts,inliers,code} object on every located object
(FeatureReport_UTIL). Gates poor_fit (residual > SBM_TRUST_RES_MAX, default 1.0 px) and
low_inliers (inliers < SBM_TRUST_INL_FRAC*npts, default 0.75). EMIT-ONLY: it reports the
code but does not yet force judges NA -- so this cut is a pure output addition, zero risk
to measurements, to measure the false-flag budget first.

Fleet validation (239 recipes, own image, global 1.0 px): only 4 primary-object flags,
and all 4 are the known-questionable recipes -- ok42 (documented 8px-unstable, res 3.0),
ok97/ok98 (the 160deg mirror alias, res 5.2), ok39 (the NMS-order mislocation, res 3.65).
Residual distribution on the other 235: p50 0.013, p90 0.055, p95 0.144, max (of the
non-flagged) well under 1.0 -- a ~7x margin. So the residual gate alone, at a GLOBAL
threshold, separates trustworthy from questionable with ~1.7% flag and effectively zero
false positives; per-recipe calibration is barely needed. Under noise15 the good
primaries stay < 0.13 px while clutter/wrong detections read 3-5.6 px (ok68).

Next: step 2 (ambiguity margin for the mirror cases -- ok97/98 already flag via residual,
but ambiguity is the general catch), then wire the gates to force judges NA behind a
per-recipe enable once the budget is accepted.
