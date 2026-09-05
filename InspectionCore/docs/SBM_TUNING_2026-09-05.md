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
