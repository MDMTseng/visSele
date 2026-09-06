# SBM coarse-locate cost, the refine's capture range, and a per-recipe tuner

Work of 2026-09-05 on branch `ct/view-path`. Everything measured on the bench laptop
(i5-8250U, 8 threads) with a core built from that branch; the fleet is the 247
`hy_sync/OK` copies in `data/ok*` (see `SBM_STABILITY_2026-09-04.md` for how they
were made). Scripts: `UI/WebUI/tools/webctl/sbm_tune.mjs` (tracked), `_capture.mjs`,
`_loc_sweep.mjs`, `_loc_phase.mjs` (scratch).

## 1. Where a frame's time goes

Six `test1` field frames (2448x2048, 4-5 parts each), stages separated by difference
(`SHAPE_REFINE=none` removes the refine; stripping every caliper primitive removes
measurement -- the core's phase histogram only fills on the live camera path, not on II):

| | 8 threads | 2 threads (`OMP_NUM_THREADS=2`, `INSP_CV_THREADS=2`) |
|---|---|---|
| full frame, scale 0.5, step 1° | 51 ms | 84 ms |
| coarse match only | 38 ms | 72 ms |
| ROI refine | ~3 ms | ~6 ms |
| measurement (4 parts) | ~9 ms | ~12 ms |
| full frame, scale 0.3 + step 2° | 30 ms | 50 ms |
| coarse only, scale 0.3 + step 2° | 14.6 ms | 39 ms |
| coarse only, scale 0.3 + step 3° | -- | 30 ms |
| coarse only, scale 0.25 + step 2° | -- | 49 ms (slower: more candidates to refine) |

The coarse line2Dup stage is 75% of the frame and is already parallel (OpenMP over
templates, MIPP SIMD). Going 8 -> 2 threads costs 1.7x, not 4x, so part of it is
memory-bound. On a 2-core target the lever is fewer templates and fewer pixels, not
more threads. A Surface Go 3 at roughly half this clock would sit near 130-170 ms per
frame at today's defaults and 80-100 ms at scale 0.3 / step 2°.

## 2. The refine decides how coarse the coarse stage may be

`roi_refine.cpp`: each ROI point is matched along its edge normal within
`search_half = 15 px` (full resolution, `kDefaultROIHalf`, hard-coded -- the def's
`roi.half` 28 only sizes the window cut-out), at most 3 iterations, re-match when the
angle moved more than 2°, per-iteration clamps 0.2 rad / 10 px, outliers beyond 2x the
median residual dropped.

So a coarse pose error is affordable only while every ROI point's displacement along
its normal stays inside the search range:

    disp_p = angle_err * lever_p + trans_err,   lever_p = |x_p*ny_p - y_p*nx_p|

with (x_p, y_p) the point relative to the template centre and n_p its edge normal (both
in `shape_cache.roi.pts`). Tangential displacement is invisible to a normal-profile
match (aperture), which is why an elongated part tolerates several degrees while a
part with edges facing the rotation direction does not. Worst cases per knob:
angle_err = step/2, trans_err = 0.71/scale px (half a coarse pixel, diagonal). With a
budget C = 12 px (3 of the 15 in hand) and, because of the outlier rejection, the
MEDIAN lever rather than the maximum:

| recipe | lever median (px) | predicted step max | measured (worst-case rotation S/2, 1.5S) |
|---|---|---|---|
| CON (symmetric spring) | 254 | 4.3° | 3° fine (0.03°); 4° -> 0.88°; 6° -> 8.9° |
| MODEL3131 (off-centre origin, big) | 697 | 1.6° | 2° fine; 3° -> 0.15°; 4° -> 0.74°; 6° drops judges to 1/8 |
| 8G (thin Z spring) | 174 | 6.3° | 8° fine (0.02°); 10° -> 3.5° |

Scale the same way: 0.71/scale <= C - lever*(S/2): 0.25 is safe on all three, 0.20
breaks MODEL3131 (1.66 px), 0.15 breaks 8G too.

Two caveats the numbers carry:
* The reported similarity is the COARSE score and falls with the step even when the
  refine recovers the pose exactly (CON 0.99 -> 0.93 at 6°). A coarser step needs a
  lower `shape_min_score`, which lets clutter through. The tuner therefore also holds
  the similarity to within 0.05 of the base.
* Bigger parts have bigger levers. The 20 fleet recipes with a median lever over 500 px
  (templates 2000+ px, several the full sensor) are pinned at step 1-2°, and they are
  the frames that cost the most. Section 4 is about them.

## 3. `sbm_tune.mjs`: predict, verify, adopt only when it pays

Per recipe: predict the step from the lever, verify candidates around it on the
worst-case set (rotation S/2, -S/2, 1.5S; shift half a coarse pixel on x, y, both --
rotation and shift are never combined, because the perturbation rotates about the
image centre and a combined case would need the sign conventions the sweep avoids);
take the largest passing step and back off one grid step; then try smaller scales at
that step the same way; adopt the tuned pair only if the same worst-case frames run at
least 15% faster. "Passing" is relative to the base def's own worst case: pose no worse
than base + 0.05° / 0.1 px, judges not fewer, exactly one object, similarity within
0.05.

Fleet (247 recipes):

| | |
|---|---|
| tuned | 207 |
| adopted (>= 15% faster) | 62 -- median 21%, max 50%; their sum 1063 -> 805 ms |
| kept (gain under 15%) | 145 |
| skipped | 40: 31 lose the part or a judge under their OWN worst case, 7 do not locate their own picture, 2 coarse-only |
| whole-fleet sum of one frame each | 4169 -> 3909 ms (6%) |

Adopted settings: scale 0.25 on 50 of 62; step 4-6° on 44, 2-3° on 14, 8-10° on 4.
Not one of the 20 large-lever recipes was adopted (15 stay at 1°, 4 at 2°, 1 at 3°),
which is why the fleet total moves only 6%: the expensive frames are exactly the ones
the capture range pins.

### What the first run got wrong, kept here so it is not redone

* **Feature count and edge thresholds are no-ops on a cached def.** The first run
  "accepted" `shape_num_features` 128 -> 32 and `shape_strong_thres` 80 -> 120 on all
  148 recipes it finished. A def carrying a self-contained `shape_cache` loads its
  features from the cache; the fingerprint that encodes those knobs is stored but never
  compared at load (`FeatureManager_sig360_circle_line.cpp:7866, 7923, 8441`). Changing
  them at inspection time changes nothing -- which is also a field problem: an operator
  who edits a threshold without regenerating gets a def that says one thing and runs
  another. They need a sweep with an SF regenerate per candidate, and the core should
  compare the fingerprint on load and at least warn.
* **An absolute pose tolerance is below the perturbation's own noise.** With 0.2 px
  the base def failed on 90 recipes; the base worst-case shift error has a median of
  0.16-0.33 px. Hence the relative criterion above.
* **A 180° flip of a symmetric part reads as a 180° error.** The rotation check will
  flag ok37, ok56, ok170, ok183, ok221, ok38 (rot 179.5°, 200°, 180°, 359°, 359°,
  177.6°) although their judges may be identical either way. These need eyes, not a
  metric.

### The instability the sweep found in today's settings (worth more than the tuning)

Base defs that fail their own worst case (31): pose jumps of 2-8 px under a half-pixel
shift -- ok04, ok36, ok42 (big part, 8 px), ok95, ok163/164/166, ok200, ok214, ok245
and others; a judge lost under a half-pixel shift -- ok01, ok18, ok27, ok121/125,
ok152, ok162, ok208 (values sitting on a tolerance edge); outright mislocation --
ok08 (sim 0.58), ok39, ok97/98 (1865 px). The full list with numbers is in
`_sbm_tune_fleet2.txt`.

## 4. Widening the capture: two experiments on the refine

Two diagnostic env knobs were added in `shape_matcher.cpp` at the `refineROI` call:
`SHAPE_ROI_SEARCH=<px>` widens the 1-D search half-range; `SHAPE_ROI_PRESCALE=<f>`
runs a coarse-to-fine pre-pass -- the same refine on the scene and template scaled by
f with the search range unchanged (so 15 px at f = 0.5 is 30 px of full-resolution
capture), then the normal full-resolution pass from the result. The pre-pass resizes
the scene per candidate here; production would cache one scaled scene per frame.

Worst-case rotation error (deg) vs angle step, three recipes:

| step | CON 15 | CON 30 | CON pre 0.5 | M3131 15 | M3131 30 | M3131 pre | ok42 15 | ok42 30 | ok42 pre |
|---|---|---|---|---|---|---|---|---|---|
| 1° | 0.032 | **0.921** | 0.039 | 0.007 | 0.007 | 0.007 | 0.333 | 0.071 | **0.008** |
| 2° | 0.038 | 0.038 | **14.9** | 0.032 | 0.005 | 0.003 | 0.323 | 0.071 | 0.011 |
| 3° | 0.032 | 0.921 | 0.039 | 0.149 | 0.003 | 0.006 | 0.520 | 0.053 | 0.003 |
| 4° | 0.877 | 0.919 | 0.040 | 0.744 | 0.016 | 0.005 | 2.46 | 0.190 | 0.004 |
| 6° | 8.85 | 1.24 | 14.9 | 2.44 | 1.75 | 1.52 | 3.85 | 2.27 | 2.59 |
| 8° | 13.6 | 1.00 | 0.068 | 3.27 | 3.48 | 3.23 | 4.19 | 4.33 | 4.27 |

Time per frame: search 30 costs about +10 ms, the pre-pass about +5 ms on these
(single part; the per-candidate scene resize is most of it).

What this says:

* **The capture range is real and movable.** MODEL3131 goes from 2-3° to 4° with
  either knob; ok42 (lever 965) from 2° to 4°, and its 1° precision improves from
  0.33° to 0.07° (search 30) or 0.008° (pre-pass) -- at 15 px the default refine was
  not fully converging on this big part in its 3 iterations. Coarse-to-fine gives the
  large parts the best precision of the three, not merely equal.
* **Wider capture also captures the wrong thing.** CON with search 30 locks 0.92°
  off at 1°, 3° and 4° -- the wider normal search reaches the neighbouring coil turn.
  CON with the pre-pass converges onto the 15° mirrored pose at steps 2°, 6°, 10° (the
  same false peak `SBM_STABILITY` documents), i.e. the half-resolution pass confirms
  the wrong candidate before the face arbitration sees it. Neither knob is a safe
  default; both are a per-recipe choice, and the tuner already has the machinery to
  verify one (it is a third knob to sweep, gated by the same worst-case set).
* The knobs stay env-only for now. A def field (`shape_roi_search`, `shape_roi_prescale`)
  plus the tuner verifying them is the next step if the large-part frame time matters;
  the per-point `score_floor` that already exists in `SamplePoint` is the natural guard
  against the wrong-edge lock and is not used by the pre-pass today.

## 5. Also found on the way

* `edge_profile` payloads on a search point with `rel_strength` 0 produced tens of
  thousands of hits and an O(n²) cJSON append -- minutes per frame, the core looked
  hung. Fixed on `ct/2.0rc2` (`41b58f24`): hits capped at 600, tail-pointer append.
* The core's log WS server binds 4091 unconditionally; a second core on port 4091
  beside the launcher's does not get its main port. Bench cores use 4093+.

## 6. 2026-09-06: where the matcher's time goes, and what the agents found

Measured on the continuous-inspection path (carousel of three `test1` field frames,
2448x2048, `INSP_AREA_BYPASS=1` so the station gate does not drop the parts; the II
path re-trains the matcher per request and is not representative). Stage split from
`SBM_PROFILE=<N>` (new, env-gated, `shape_matcher.cpp` StageProf + line2Dup's
accumulators, whose two accounting bugs -- quantize included Sobel, refine never
counted -- are fixed):

| per frame, ms | 8 thr | 2 thr |
|---|---|---|
| prep (resize 0.5 + pad) | 0.7 | 0.8 |
| coarse (line2Dup) | 11.2 | 17.9 |
| ROI refine (~2 candidates) | 4.1 | 4.4 |
| total | 16.0 | 23.1 |

Inside coarse at 2 threads, by thread-time share: **T4 local refinement 78-80%**,
T8 full-image similarity 15%, threshold scan 6%. The reason: 360 templates x 6.5
candidates over `min_score` 50 = ~2340 local 16x16 refinements per frame for ~2
real objects. "Pyramid refinement 0.0 ms" in the old profiler was a dead counter.
Preprocessing (blur, Sobel, quantize, voting, spread/linearize) is ~4.5-5 ms and does
not scale with threads (memory-bound; 17 MB working set against a 4 MB L3 on the
target). `OMP_WAIT_POLICY` active vs passive made no difference on the 4-core bench.

Four read-only reviews (preprocessing, coarse similarity, ROI refine, system) --
ranked by ms saved on the 2-core target per unit of accuracy risk:

1. Station `insp_region`: the coarse match already runs on the crop when set
   (FeatureManager 8812-8899); check production machine_setting. 10-14 ms.
2. Thread config on the target: nothing sets OMP threads; libgomp opens 4, TBB 4,
   Sobel nests OMP sections over TBB. `OMP_NUM_THREADS=2 INSP_CV_THREADS=2
   OMP_WAIT_POLICY=passive OMP_PROC_BIND=true`; the 6500Y throttles under 4-thread
   load. 3-8 ms plus thermal headroom.
3. Global top-K before the T4 refinement (implemented, below). 8-9 ms.
4. Duplicate features in the scaled detector: `addModel` (FeatureManager 8669) uses
   the image-less overload, so the 0.5 detector gets coordinate-halved full-res
   features; at T8 many collapse onto the same cell. Re-extract at scale (the image
   overload, shape_matcher 2109-2139). 4-6 ms, and probably better coarse scores.
5. `skip_voting` fused Sobel+quantize path exists (AVX2) but is off (`MatchConfig`
   default false, FM never sets it); the default path runs a scalar quantize loop.
   Needs `-DSBM_GRADIENT_KERNEL=1` so the operator matches extraction. 1.5 ms.
6. ROI refine: `roiPCA` recomputed per candidate although `cached_lock_info` exists;
   `cv::matchTemplate` 60x60 through DFT where a direct 31x31 NCC is 3x cheaper;
   template window re-rotated per candidate. ~1 ms per candidate.
7. Threshold scan vectorisation (0.8-1.5), level-0 spread only in candidate windows
   (1.2), band-tiling the preprocess (2-3, seam risk), `-mavx2` on MatchingEngine.

Not recommended: cross-frame pipelining (both cores are busy in coarse; breaks
one-trigger-one-result ordering), rewriting the AVX2 kernel, the 1-D edge refine
path (wrong-edge locks), a previous-frame angle window as a default.

### Global top-K (`SBM_GLOBAL_TOPK`, default off)

`matchClass` was restructured into coarse-for-all-templates -> global cap -> refine
per candidate (`line2Dup.cpp`). Kept: global rank < K, or coarse score within
`SBM_GLOBAL_MARGIN` (20) of the best, or among the best `SBM_GLOBAL_PER_TEMPLATE` (1)
of its template. Results: test1 2-thread SBM 23.1 -> 14.4 ms with bit-identical poses;
fleet (247 recipes, own picture, K=64): 229 identical, 11 differ. Of the 11, most are
clutter at similarity 0.50-0.56 that the capped path no longer reports; ok70/72 report
different clutter; **ok97/ok98 lose the true pose** (refined 0.992) to a 160-deg alias
(0.872).

`SBM_TOPK_DEBUG=1` shows why: the true pose's coarse T8 score is **51.5** (min_score
50) while its refined score is 98.9 -- global rank 355, second within its template.
On this recipe the coarse level is nearly uninformative, and the object survives
today only because everything above 50 gets refined. Any candidate cap is unsafe
for such a recipe, so top-K stays off by default and must be a per-recipe knob
verified by `sbm_sweep`; the real fix is the coarse score itself (item 4 above --
duplicate features at T8 are the prime suspect), after which a cap becomes safe.

### Also fixed / found

* Greedy NMS consumed `raw_matches` in OpenMP completion order, so which pose
  claimed a location depended on thread timing (ok39: 0.617 reported where 0.991
  stands; ok68: 5-10 objects across runs of the same picture). `raw_matches` is now
  sorted by similarity before NMS. ok68 still varies between runs -- a second
  order-dependence remains downstream (candidate loop / dedup), not yet located.
* `shape_blur` is not part of the cache fingerprint; changing it silently mismatches
  cached features.
* Colour-camera path: `cvtColor GRAY2BGR` on 15 MB per frame only for byte-identical
  transport (3-5 ms on the target); mono cameras unaffected.

Tools: `tools/webctl/fleet_eq.mjs <portA> <portB>` inspects every recipe's own picture
on two cores and diffs objects (count, pose, similarity, judge values); `_ci_prof.mjs`
drives CI on the carousel and reports `insp_wall_ms`; `_ii_loop.mjs` (DUMP=1) prints
poses for a quick A/B.

### Re-extracting the down-scaled detector's features (experimental, `SBM_STORE_SCALED=1`)

The cache can now carry a `scaled` block: features extracted from the template
resized by `shape_match_scale`, used by the 0.x detector instead of the full-res set
with halved coordinates (`FeatureManager` stores it at SF, `ShapeMatcher::addModel`
takes it as a fourth argument; `SBM_NO_SCALED_SET=1` ignores it for A/B). Six
recipes regenerated and compared against coordinate scaling on their own picture:

| recipe | full-res levels | scaled (0.3) levels | effect |
|---|---|---|---|
| MODEL3131 (2194 px) | 129 / 65 | 131 / 66 | same pose, similarity 0.992 -> 1.000 |
| ok39 (1344 px) | 132 / 77 | 131 / 51 | true pose found (1.000); clutter up to 0.744 |
| ok97 (1628 px, thin) | 90 / 65 | 43 / 23 | pose 1.000 but coarse rank of the true pose 772nd |
| ok68 (398 px) | 108 / 58 | 34 / **9** | every object 0.882-1.000: nine features saturate |

So the scaled set is right in principle and wrong in practice for small parts at
0.3: the blur + INTER_AREA resize leaves too few gradients above the thresholds,
the coarse score saturates, and the coarse rank of the true pose gets worse, not
better. It stays off by default. To become the default it needs a feature floor at
the coarse level (fall back to coordinate scaling below it) and probably thresholds
scaled with the resize.

Two things the experiment exposed on the way:
* **Correction to section 6's premise.** The "705 / 385 features per level" quoted
  there was the LENGTH OF THE FLAT ARRAY (5 numbers per feature): the caches hold
  141 / 77 (test1) and 129 / 65 (MODEL3131), and across the fleet level 0 holds 129-299
  features on 208 recipes and <=128 on 36. There is no selector drift; a regenerate
  today gives the same order of count (129 / 65 vs 141 / 77). The "duplicate
  features at T8" argument is correspondingly weaker: 65-77 coarse features on a
  half-res template, not 385.
* The coarse T8 level is a weak discriminator for small parts at scale 0.3 whatever
  the features: the "refine everything above min_score" policy is what makes those
  recipes work, and it is also the 78% of matcher time. Speed and safety here are
  the same knob; the sweep has to see both.

### ROI refine: template PCA cached at addModel (2026-09-06)

`refineROI` recomputed `roiPCA` (two `cv::Sobel` + magnitude + eigen) on the
UNROTATED template patch of every sample point, every candidate, every re-match --
although the answer is a function of the template alone. It is now computed once in
`precomputeFeatureCaches` (via `roi_refine::templatePCA`, the same patch `refineROI`
would cut: same rounding, same border shrink) and carried on `SamplePoint`. The
runtime path uses the cache only when its patch half matches; a border-shrunk point
falls back to a fresh compute. `SBM_NO_PCA_CACHE=1` disables it; `SBM_PCA_CHECK=1`
recomputes and prints any cached-vs-fresh mismatch.

Verified with `fleet_eq.mjs` old-exe vs new-exe, 2 threads: `SBM_PCA_CHECK` printed
zero mismatches over the fleet; 232/247 identical, and every differing recipe is
either the known run-to-run NMS-order clutter (ok68/70/72/213, which differ A-vs-A
too) or a 1e-5 last-digit float change from the rebuild (ok42/ok195: same pose to
5 dp, same judge status). insp_wall_ms fleet median 26.9 -> 25.7, sum 7494 -> 7218
(~4%). Small, because the refine is ~4 ms of a ~26 ms frame and PCA is ~20% of it;
zero accuracy cost. A direct 31x31 NCC was then tried in place of `cv::matchTemplate`, on the review's
claim that these small sizes route through a DFT. On this OpenCV 4.13 build that is
false: `matchTemplate` already does a tuned spatial correlation, and the hand-written
NCC (integral image for window energy, double accumulation) ran the frame at 50 ms vs
25 and shifted 91 recipes at the 1e-4 judge level. Reverted -- `matchTemplate` stays.

## 7. 2026-09-06: second agent round on the match path, and the local-maxima promotion

Four deeper reviews (coarse candidate generation, auto-ROI selection, template/angle
count, runtime kernels). Landed and verified so far: promote only 3x3 LOCAL MAXIMA of
the T8 response to the T4 refine, instead of every cell above min_score.

The 78% of the matcher that is T4 local refinement is driven by ~6.5 candidates per
template over min_score for ~2 real objects: a true pose lights a 2-3 cell plateau at
T8 and every cell of it was being refined. Keeping only the plateau's local max (tie
rule: >= toward already-scanned neighbours, > toward not-yet, so a flat plateau keeps
one) is the LINE-MOD standard; the T4 refine window (+-32 px at level 0) already
contains the suppressed neighbours, so the kept candidate's refine reaches at least
their score. `line2Dup.cpp` threshold scan; `SBM_NO_LOCALMAX=1` restores the old scan.

Verified with fleet_eq, same binary, localmax off vs on (isolates the algorithm from
rebuild float noise): 232/247 identical, 7 differ, and the 7 are all benign -- no real
object gained or lost, every judge status unchanged; ok42/ok195 shift one judge value
by 1e-5 (a dropped near-duplicate lets another candidate win the refine by an epsilon),
ok68/70/72/213 are the clutter recipes that already vary run-to-run. insp_wall_ms
fleet median 32.6 -> 28.0, sum 9090 -> 7972 (~13%). Result-neutral in the meaningful
sense (no detection or judge-status change), not literally bit-exact; run sbm_sweep's
worst case before trusting it default-on beyond the fleet snapshot.

### The rest of round 2, ranked, NOT yet done

Landed after the local-maxima commit (both verified result-neutral, fleet median
25.6 -> 21.2 ms, ~13% more): accessLinearMemory uses mask/shift not idiv (T is a
power of two, f>=0), and the refine visits candidates in (template, location) order
so neighbours reuse L2. The 7 fleet recipes that "differ" are the run-to-run set --
proven inherent by running one binary with the sort off on both ports (ok42/ok195
still wiggle 1e-5 with no code change); the source is OMP completion order feeding
the NMS stable-sort tie-break, a separate reproducibility item.

Still worth doing (bit-exact, match path):
* similarityLocal (the T4 kernel, the 78%): kill the 4 runtime idiv/feature in
  accessLinearMemory (T is 4/8 -> shift/mask), register-resident uint8 accumulator
  like similarity(), and sort candidates by (template,location) before the refine loop
  so neighbours reuse L2. Kernel agent: ~5-9 ms wall combined, all bit-exact.
* thread_local scratch for the per-template / per-candidate Mats (~0.5 ms).
* (Correctness, already safe in the shipped core.) The core builds the shape lib with
  -mavx2 -mfma, not -march=native (InspectionCore/CMakeLists.txt:291), so no AVX-512
  leaks into the Amber-Lake target; only the submodule's standalone test CMake uses
  -march=native, which is dev-only. No change needed for the shipped binary.
* Build: was -- pin -march=skylake -mavx2 -mfma (or x86-64-v3), NOT -march=native --
  native on an AVX-512 dev box emits zmm and SIGILLs the Amber-Lake target. Correctness.

Auto-ROI overlap (the quality concern, also speed): production uses the grid selector
with roi_min_spacing=0, so 598 point-pairs across 231/244 fleet templates have
overlapping 30 px windows (433 pairs >50% overlap); the frozen 8 points average only
~6 distinct. Overlapping windows are redundant refine work AND correlated Jacobian rows
(fake outlier-gate redundancy). Deployed defs carry the points frozen, so a selector
change reaches them only by re-migration OR a de-dup of user_opt_points at load. Plan:
default roi_min_spacing to auto (15 px, or max(15, 0.06*min(W,H)) so big parts spread),
consider max_points 8->6, add a det(JtJ) rank guard that raises the count back or marks
the recipe coarse-only for a one-edge part. ~1.5-2 ms and better conditioning; needs
re-migration to reach the fleet.

Angle/template count: 181 of 246 recipes still sit at matching_angle_margin_deg 180
though the machine's orientation is often known; a 90 margin halves coarse for that
recipe (already wired, configure-only), 30 cuts it ~11 ms. Per-frame angle prior with
an in-frame full-sweep fallback is the biggest single lever (~10-12 ms) but is a
per-recipe opt-in gated by the alias period. Angle coarse-to-fine breaks the ok97/CON
class per the existing 3-4 deg sweep data; not until the coarse scores are fixed.

## 8. 2026-09-06: NMS determinism, and why ROI de-overlap is not a blanket default

Two task-list items from the second agent round, both delivered with the caveat the
verification turned up.

**NMS input is now a total order** (shape_matcher.cpp): raw_matches were stable-sorted
by (similarity, template_id) only, so equal-score same-template matches at different
places kept OpenMP push order and the greedy NMS kept whichever it saw first -- the
reported pose and the grouped object count drifted run to run. Total order by
(similarity, template_id, x, y) makes NMS a function of the match set. This removed the
thread-order source but NOT all variation: two SINGLE-THREADED runs of the same image
still differ on near-min_score clutter (ok68 9 vs 6 objects at 0.75), so there is a
residual PROCESS-level source (an uninitialized read that flips detections sitting on
the threshold; real objects at 0.98+ never move). The fully-fused linear-memory writer
covers every cell, so it is elsewhere; left as an open reproducibility item because it
only ever touches sub-threshold clutter.

**ROI point de-overlap: built, default OFF.** The production selector left
roi_min_spacing at 0, and the frozen fleet defs carry 8 points selected with spacing
off -- 231/244 templates have overlapping 30 px windows, 433 pairs over 50%, ~6 of the
8 points distinct. selectOptimizedPoints now applies the greedy spacing filter to the
frozen user_opt_points as well (they bypassed it), so a spacing can de-overlap the
deployed fleet at load with no re-migration; with it on the fleet runs ~18% faster
(median 25 -> 20 ms) and ok232 goes 0 -> 1 (a part the overlapped set missed).

It is default OFF because it MOVES MEASUREMENTS. fleet_eq's new acceptance summary
(object-count changes, and the uInsp-critical judge FAIL->PASS / PASS->FAIL flips)
found ok11 judge38 going 2.3032 (FAIL) -> 2.2937 (PASS) -- stable in each config, so a
real change, not run-to-run noise. On a metrology system a measured-value change on a
real part is a per-recipe decision against ground truth, exactly what sbm_sweep's
acceptance profiles are for; it is not something to flip on for 247 recipes at once.
Path to adopt: make roi_min_spacing a def field, sweep each recipe with
SBM_ROI_SPACING, adopt only where every judge margin holds. fleet_eq.mjs now prints
the FAIL->PASS / PASS->FAIL summary for exactly this gate.

Landed and safe this round (match path, all verified result-neutral): T8 local-maxima
promotion, T4 idiv->shift + cache-order refine, template PCA cache, total-order NMS.
Frame ~26 -> ~21 ms at 2 threads. Then similarityLocal was batched into a uint8
accumulator (widen once per 63 features instead of per feature) -- estimated 1-2 ms
but actually ~11% (fleet sum 6967 -> 6198, median 25.1 -> 22.4), bit-exact; the
per-feature widen cost more than the memory-bound model predicted. SBM_NO_LOCALACC=1
to A/B. Not adopted (change measurements or poor return):
ROI de-overlap (default off, needs per-recipe sweep), angle coarse-to-fine
(breaks ok97/CON). Configure-only, per recipe: angle margin (181 recipes still at 180).

## 9. 2026-09-06: ROI de-overlap routed through a per-recipe acceptance sweep

`shape_roi_spacing` is now a def field (0 off / <0 auto = ROI half / >0 px) ->
MatchConfig.roi_min_spacing, so each recipe can carry its own spacing instead of the
global env. `tools/webctl/sbm_roi_sweep.mjs` decides per recipe with a strict rule fit
for a metrology machine: ADOPT only where de-overlapping changes NO judge verdict on
the unperturbed part AND is faster AND keeps every judge under a small augmentation
set; any verdict flip (either direction) goes to REVIEW for ground truth, never
auto-adopted.

Fleet result (spacing auto, single clean core): 97 adopt (5-22% faster each,
verdict-preserving), 1 REVIEW (ok11 judge38 2.3032 FAIL -> 2.2937 PASS), 141 no safe
gain, 7 no object. The adopt/review lists are in `tools/webctl/roi_spacing_adopt.json`.
Applying the 97 is a def-migrate step (set shape_roi_spacing=-1 on those recipes),
NOT auto-deployed here -- hy_sync is read-only and deployment is the operator's flow.
ok11 needs a human to say whether the part is good (then PASS is the fix for a false
reject) or the old FAIL was right (then keep spacing off for it).

Harness note that cost an hour: env-gated behaviour and the def field appeared not to
work because seven orphaned cores from earlier launches were all LISTENING on port
4093 (SO_REUSEADDR), and the client hit stale ones. Kill by PID from
`netstat -ano | grep :4093`, not by image name, before trusting a bench result.

## 10. 2026-09-06: how mature metrology locators do it, and what it means for the ROI question

A survey of HALCON shape-based matching (Steger 2001/2002), Cognex PatMax (US7088862),
Matrox GMF, Euresys EasyFind vs the line2Dup approach here. Verdict: the FAMILY is
right -- every metrology-grade vendor converged on edge-gradient shape matching (score
= mean normalised dot product of gradient DIRECTIONS, illumination-invariant, occlusion-
tolerant) + a rigid least-squares refine on point-to-normal-line distances, 3-4 iters,
robust-gated. line2Dup is a coarser re-implementation (8-bin quantised, spread response
maps) of exactly HALCON's score, which is why it NEEDS the separate refine that HALCON's
least_squares and PatMax's force-field attraction are the industrial versions of.

The reframe for our ROI question: **mature systems do NOT select a handful of ROI
points.** HALCON refines on ALL model edge points (optionally uniformly decimated),
weighted, rejecting correspondences beyond a robust sigma. PatMax uses all edge dipoles
with continuous weights (force distance x direction agreement x gradient magnitude) plus
painted eval regions. The per-point matchTemplate over a chosen 8-point subset is a
line2Dup-community compromise (per-point matchTemplate is too dear to run on all points),
not something the industrial tools do. So "a better ROI-subset selector" is polishing a
compromise; the near-optimal move is to stop selecting a subset.

What to borrow, ranked (each needs the sweep + verdict-flip gate, all change measurements):
1. Refine on ALL edge points, weighted + robust-sigma, point-to-normal-line LSQ (the
   Steger/PatMax step). Replaces the per-point matchTemplate; the pose solve stays a
   cheap 3x3 per iteration. Biggest change, biggest upside, needs a roi_refine rewrite.
2. Sub-pixel edge extraction (Steger facet/parabola along the gradient) on template and
   image before the LSQ -- nearest-integer correspondences leave ~0.25 px radial error.
3. Re-score fine-level candidates with the CONTINUOUS unit-vector dot product + a
   MinContrast noise floor (line2Dup's spread blurs the peak; HALCON keeps it sharp) --
   this is what makes an occluded hit land in the SAME pose (repeatability).
4. Two scores: coverage (fraction of model found) AND clutter (extra edges near the
   model). A low clutter score is the never-miss alarm that the locator latched onto a
   burr / foreign object -- directly serves uInsp 不可檢錯.
5. Greediness=0-style safe search (never prune the true pose); cheap given the known
   angle band.

Evaluation hooks added (default off, match-time, work on frozen defs): SBM_WEIGHT_BY_LOCK
(weight the solve by per-point lock distinctiveness), SBM_REJECT_LOW_SCORE / SBM_REJECT_PCT
(drop a match below score_floor*pct -- a gross wrong-edge/mirror lock). Quick A/B: lock
weighting moves measurements on 237/247 recipes (small, no verdict flip on the own image),
so it is NOT a free win -- same per-recipe ground-truth gating as roi_spacing. These are
band-aids on the subset design; item 1 is the real answer.

Conclusion on "is the current auto-ROI near-optimal": on its own axis (pose covariance of
the chosen subset) yes, and switching D->G/c-optimal is not worth it (section, earlier).
But the subset architecture itself is the compromise; the mature answer is all-points
weighted robust refine, which also subsumes the overlap and distinctiveness questions
(no subset -> no overlap to fix, robust sigma -> no gross-error point to screen).

## 11. 2026-09-06: ROI de-overlap effect on existing defs -- speed only, not robustness

Ran the fleet with SBM_ROI_SPACING=-1 (de-overlap on frozen defs at load, no re-migrate)
vs off, noise A/B. Pose drift is unchanged; judge loss under noise is slightly WORSE with
de-overlap on (sigma 5: 3 vs 2; sigma 15: 7 vs 5). De-overlap removes redundant points,
which is faster (the 5-22% the sweep found) but also removes the point-count redundancy
the 2x-median outlier gate relies on -- so it is a speed optimisation, NOT a robustness
one, and marginally reduces noise resilience. Under uInsp (never-miss > speed) adopt only
where speed matters and robustness is verified to hold. Corrects the earlier expectation
that better conditioning would improve robustness: conditioning improves, but noise
resilience comes from redundancy, not conditioning.
