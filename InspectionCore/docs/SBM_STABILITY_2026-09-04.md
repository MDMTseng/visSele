# Localisation and measurement stability: sig360 vs migrated SBM, eight field recipes

Run 2026-09-04 with `UI/WebUI/tools/webctl/stability_sweep.mjs` against copies of
`hy_sync/OK` recipes. Each recipe in two forms: the original sig360 def, and the
def the one-click 升級 produced (`migrate_dump.mjs`, i.e. exactly what SAVE would
write). Each form: one baseline inspection of the reference image, then 20
perturbed ones via `img_property.perturb` -- rotation −10…+10° in 2° steps (10)
and translation ±20 / ±50 px on each axis plus two diagonals (10). `shift_x` /
`shift_y` were added to `TestPerturb.h` for this. Raw output:
`SBM_STABILITY_2026-09-04_raw.txt`.

## Localisation

| Recipe | sig360: located / rot residual / shift residual | SBM: located / rot residual / shift residual |
|---|---|---|
| CTA_PMT_FINGER | 20/20 / 0.00° / 0.20 px | 20/20 / 0.023° / 0.00 px |
| BSG-25X65X080 | 20/20 / 0.00° / 0.00 px | 20/20 / 0.040° / 0.05 px |
| NIP-LT22TW2601-01 | 20/20 / 0.33° / 0.00 px | 20/20 / 0.142° / 0.17 px |
| CG2050038P | 20/20 / 0.50° / 0.00 px | 20/20 / 0.043° / 0.06 px |
| MODEL3131 | 20/20 / 0.00° / 0.12 px | 20/20 / 0.023° / 0.00 px |
| CON-LT14BH2051-01 | 20/20 / 0.00° / 0.08 px | 20/20 / **14.9°** / **7.1 px** → after face arbitration 0.077° / 0.00 px |
| 8G2570072B | 20/20 / 0.00° / 0.08 px | 20/20 / 0.021° / 0.00 px |
| BOS-LT12BH4211 M18 | **0/20 — sig360 does not find the part on its own png** | 20/20 / 0.014° / 0.00 px |

Rotation and translation signs agree between the two engines (checked; not
assumed).

**CON on SBM: 6 of 20 runs return a pose +15° off, similarity still 0.99.**
Traced with ROI refine disabled (`SHAPE_REFINE=none`) and the coarse candidate
dump (`SHAPE_DBG=1`, angle-aware NMS so nothing is suppressed):

* The coarse stage itself picks the wrong pose. Every one of the 21 frames has
  two candidates within 0.4 points of each other at the top: the true one
  (flip=0, ~0°) and a **mirrored** one (flip=1, ~15°) -- e.g. base 99.6 vs 99.4,
  s0,20 99.2 vs 99.4. The S-shaped spring, mirrored and turned ~15°, nearly
  coincides with itself, and at 0.3× the ~10 px wire is ~3 px with 8 orientation
  bins of 45°, so the two tie and pixel phase decides. The threshold (50) is
  nowhere near either.
* The recipe declares `matching_face = 0` (both faces), so the mirrored template
  is in the race. With `matching_face = 1` the coarse stage at 0.3× is 20/20
  correct (within its 1° step). With `shape_match_scale` 1.0 it is also 20/20
  (finer features break the tie), at 64 ms vs 22 ms.
* sig360 on the same recipe never missed: it scores front and back candidates
  by its own signature error and picks by that, not by a shared coarse score.

So the failure needs both: a part that is nearly its own mirror image, and a
recipe that accepts both faces.

**Fixed in the matcher (submodule):** the two faces no longer suppress each
other in NMS -- each keeps its own best at a location -- both are ROI-refined,
and a pair at the same place with opposite `flipped` is settled by the refine
residual (mean point-to-line fit at full resolution), which a mirrored pose
cannot fake. Measured with both faces enabled at match_scale 0.3: 20/20
correct, rotation residual ≤ 0.077°, 26 ms (was 22 ms with one refine). The
same image mirrored top-bottom locates as `isFlipped=true` at 0°, mirrored
left-right as `isFlipped=true` at 180°, both with the same 14/15 judges as the
unmirrored frame. Seven other recipes unchanged.

Separately, the SBM matcher always searched 0…360° (`modc.angle` in
`buildShapeMatcher`) -- it never read `matching_angle_margin_deg`, and these
recipes all declare 180 anyway. The
judges on those runs still came out within tolerance because the anchor morph
(10 search points) re-registers the measurements; the pose in the report is
nonetheless wrong.

Done the same day: `buildShapeMatcher` now bounds the raw template rotation by
the def's `matching_angle_margin_deg` / `matching_angle_offset_deg` when the
margin is under 180°. Doing so exposed a second bug in the matcher: `addModel`
pushed the unrotated base as template 0 and rotated from `start+step`, while
the decoder assumes template k = `start + k·step` -- only consistent for the
full circle. With a ±10° window the unperturbed image came back at +15.6° with
every judge NA. Fixed (template k is exactly `start + k·step`; full-circle
order unchanged). Re-measured CON with a 10° margin: every translation and
8 of 10 rotations correct, all judges OK; the two misses (−10°, −6°) are the
same coarse false peak (+15°) still inside the window. A margin under ~5°
would exclude it, but the real fix for this part is the match scale above.

## Measurement values

Per judge, the range (max−min) of the reported value over the 20 perturbed runs.
"Worse" means SBM's range is more than twice sig360's and above 0.005 mm.

| Recipe | judges | SBM worse | SBM better | note |
|---|---|---|---|---|
| CTA | 10 | 1 ([19] angle, 0.36° vs 0.15°) | 2 | [3] [8] NG on both engines, on the reference image |
| BSG | 14 | 1 (unnamed "1", 0.046 vs 0.016) | 0 | [Par] NG on both |
| NIP | 19 | 5 (all R0.5 radii, 0.016–0.031 vs 0.003–0.008) | 0 | |
| CG | 6 | 1 ([5], 0.0065 vs 0.0011) | 1 | |
| MODEL3131 | 8 | 5 ([1][5][6][7][8]; [7] 0.097 vs 0.007, NA at +8°/+10°) | 0 | see below |
| CON | 15 | 11 → 2 after face arbitration ([A1] [A2] angle judges, 0.2° vs 0.03°, tol 4°) | 0 | was dominated by the 6 wrong-pose runs |
| 8G | 7 | 0 | 0 | "1" NG on both |

**MODEL3131 [7] drifts linearly with rotation and not at all with translation**:
5.03 mm at −10°, 4.99 at 0°, 4.84 at +6°, NA beyond; under ±50 px shift it holds
to 0.0001 mm.

Found, the same day. Three steps:

1. A hybrid def -- sig360 locator with the migrated caliper primitives -- is
   stable across ±10° (4.990–4.992). So the calipers are fine; the SBM **pose**
   is what moves.
2. Per perturbation, the SBM-reported centre minus the sig360-reported centre:
   zero at 0° and under any shift, and 2.30 px per degree of rotation on
   MODEL3131, 0.53 px/° on NIP, ~0 on the other five.
3. The recipe's template origin sits 66.3 px from the template centre on
   MODEL3131, 15.1 px on NIP, under 1.2 px on the rest. 2·|d|·π/180 predicts
   2.32 and 0.53 px/° -- the observed slopes to two digits. That is the signature
   of rotating the centre→origin offset by −θ instead of +θ.

`shape_matcher.cpp` did exactly that (`rad = -raw_angle`) in three places --
coarse, after ICP, after ROI refine. Fixed to `+raw_angle`. The calipers on
MODEL3131 were half-missing their edges by 14 px at 6°, which is where the
0.15 mm and the NA came from. Recipes whose origin happens to sit on the template
centre never showed it, which is why the earlier sweeps on test2 looked clean.

## Baseline value shift, same picture, sig360 → SBM

Mostly a few µm. The ones over 25% of tolerance or over 0.02 mm:
CTA [19] +1.01° (angle judge, tol 4°); BSG "1" −0.054, [11] R0.5 +0.023;
NIP "1"/[11] +0.030; MODEL3131 [6] +0.031 (tol 0.1), [8] +0.026;
CON [R2] +0.132, [A1] −0.26°, [A2] −0.31°, [Par] +0.079.
The CON ones are the caliper arcs measuring a different radius than contour did
on the same picture -- the arc-conversion caveat in `_caliperSeed.js`, in numbers.

## Measurements that never measured

* BOS-LT12BH4211 M18: the sig360 recipe does not locate its own reference image
  (0 objects). Under SBM it locates (sim 1.000) but 4 judges are NA and 2 NG:
  two search points find no edge over min_strength 60, arc @arc_24[1] and
  @arc_24[1][1][1][1][1] NA. The recipe and its picture disagree; nothing here
  should be tuned around it.
* NG on both engines, on the reference image itself: CTA [3] [8], BSG [Par],
  CON "1", 8G "1". Either the reference part is out of spec or the limits are;
  the migration changes nothing about them.

## `shape_nms_angle` on a 180°-symmetric part (2026-09-05)

10609 8G2570072B is a Z-shaped spring, nearly its own image under 180°. Its
recipe declares `matching_angle_margin_deg = 90`, so with the margin now
honoured the SBM search is −90…+90° and a 180° pose is never a candidate --
39 raw candidates, all within ±3°. With the margin widened to 180 in a copy:

| `shape_nms_angle` | matcher output | core report |
|---|---|---|
| 360 (default) | 1: 0° score 99.0 | 1 object |
| 180 | 2: 0° 99.0 and 180° 98.1 | **2 objects at the same place**, both 6/7 judges |
| 160 | same as 180 | same |

So the angle-aware NMS does what it says. Note what it does NOT do: nothing
downstream arbitrates between the two poses -- the core reports both as
separate objects, which a counter would take as two parts. For a part that is
genuinely symmetric the recipe's angle margin is the right tool (as 8G's own
recipe already has it); `shape_nms_angle` < 360 is for cases where a later
judge is meant to reject the wrong pose, and that judge has to exist.

## Orientation-essential retry on the SBM path (2026-09-05)

sig360 has always retried the next candidate when an `orientation_essential`
judge fails. The SBM path checked the same flag and then dropped the object
(`return -2`); with the default `shape_nms_angle` 360 there was no other pose
to try -- measured on 8G with a failing essential judge: one pose, zero
objects, `locate.code = orient_judge`.

Now the matcher keeps alternate poses of a location past NMS (up to 3, ≥10°
apart, within 10 points of the primary) tagged with one `group`, and the core
measures a group in order: the first pose whose essential judges pass is the
object, later members are skipped. Grouping compares template centres, so it
is independent of where the recipe's origin sits and of how it rotates.
`shape_nms_angle` < 360 keeps its old meaning (separate objects).

| 8G, full-circle search | kept | reported |
|---|---|---|
| no essential judge | 0° 99.0 and 180° 98.1, one group | 1 object (0°), alternate skipped |
| essential judge that fails both poses | same | 0 objects, both tried (`ret=-2` twice), candidates 2 |
| recipe's own 90° margin | 0° only | 1 object |

Eight-recipe sweep: results identical to the previous core, 11–57 ms.
