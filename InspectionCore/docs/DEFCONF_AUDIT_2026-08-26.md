# DefConf audit — primitives, aux features, measurement — 2026-08-26

Three parallel reads of the definition-configuration path: primitive features,
auxiliary (composite) features, and measurement. Everything below was found by
reading a code path end to end, in both the WebUI and the core, and comparing
what the screen shows against what the machine executes.

**The unifying failure mode is silence.** Almost nothing here throws, logs, or
paints red. A number reads as configured and the machine does something else.
That is why they survived: each one looks like a working feature.

Status column: **FIXED** (this session), **DISABLED** (feature turned off rather
than repaired), **OPEN** (still live).

---

## Fixed or disabled today

| | Defect | Where | Status |
|---|---|---|---|
| A1 | `edge.polarity:'any'` executed as `falling`. `SP_BOTH` existed in the enum and in `search_point_cv` and was unreachable from a def. `'any'` was also the UI's seed for every new search_point — so the default choice was the one that did not mean what it said. | `FeatureManager_sig360_circle_line.cpp:1162` | **FIXED** `8258c446` — ANY→SP_BOTH, logged; UI now seeds `falling` (what the core actually ran), so behaviour is unchanged and `any` becomes a deliberate choice. |
| — | calc: the editor validated with the UI's evaluator, which is a **superset** of the core's. `judge_CALC` implements six operators (`+ − * / max min`) and returns −2 for anything else; the UI has `$^$` too. `a^2` computed on screen, saved, then made every part NA forever via an uncleared `STATUS_UNSET`. | `DefConfUI.js:483`, `JudgeCALC.cpp` | **FIXED** `0102174e` — validation now checks the core's set and names the offending operator. |
| M1 | `couple_value_b` re-centred the BACK limits on the FRONT target (`obj.value` where `obj.value_b` was meant). Editing `value_b` 10→20 left `LSL_b/USL_b` at 9/11. | `shapes/measure/index.js:55` | **FIXED** (typo) + **DISABLED** `6403662c` |
| M4 | Per-製程 overrides carry no `_b` keys (`PERSIST` whitelist) and the margin editor has no back columns, so an override that tightened `USL` left `USL_b` at the root — and a flipped part is judged on `USL_b`. The override was a no-op for exactly the parts this bench runs. | `DefConfUI.js:828` | **DISABLED** `6403662c` |
| X2 | UI aux geometry omits the flip term the core applies (`shapeVectorParse` vs core `angle *= -1` when `flip_f < 0`). On a mirrored part the drawn aux point is not where the measurement was taken. | `InspectionEditorLogic.js:1500` vs `FeatureManager…:507` | **DISABLED** `6403662c` |

**Back-side (flipped-part) limits are disabled**, gated by
`BACK_SIDE_LIMITS_ENABLED` in `UI/WebUI/src/UTIL/backSideLimits.js`. The `_b`
keys are also stripped from the generated **wire def** — disabling in the UI
alone would have left the core still judging flipped parts on `USL_b`, which is
the same class of divergence the flag exists to end. The def **file** keeps its
values; nothing an operator configured was destroyed.

Re-enabling requires fixing M1 (done), M4, and X2 together — not one of them.

---

## Open — verdict divergence (the screen and the machine disagree)

| | Defect | Where | Failure |
|---|---|---|---|
| **A2** | An NG produced by `NAasNG` is lost in the part roll-up. `MEASURERSULTRESION_reducer` is sticky for `NA/USNG/LSNG/UCNG/LCNG` but **not** for `NG`, so a later `UOK` overwrites it. | `InspectionEditorLogic.js:75`, `InspectionUI.js:1372` | Core converts the same item to `STATUS_FAILURE` and never loses it. Part is physically rejected; panel shows a green verdict. `gradeMismatch` cannot see it — it compares item status only. |
| **A3** | The UI folds the **first** matching tag's override into the wire def; the reducer's `MarginInfoExtraction` grades with the **last** match. | `InspectionUI.js:2748` vs `UICtrlReducer.js:332` | A part carrying two tags with override rows is judged by the core on tag A and coloured on screen by A-merged-with-B. Order-dependent, and it makes the mismatch counter fire with nobody having changed a limit. |

`MEASURERSULTRESION_priority` (`InspectionEditorLogic.js:55`) is exported and
imported nowhere. The real reducer is the hand-written cascade above, so that
table is not the specification it looks like.

---

## Open — silent wrong values

| | Defect | Where |
|---|---|---|
| **B1** | The root row's `quality_essential` tristate is a dead control: the row object has no such field and `update` writes back only six keys. Clicking 是/否 is accepted and discarded. That field is the single thing deciding whether a measurement counts — the symptom is "I disabled it and it still rejects parts". | `DefConfUI.js:871` |
| **B2** | UI and core disagree on degenerate caliper params. `count=0` → UI draws 10, core runs 2 (line) / 3 (arc). `count=1` → UI draws one box at the midpoint, core runs two at the endpoints. `locating:'caliper'` with no `caliper` object → UI 0.1 mm boxes, core 0.5 mm. | `_caliperFields.js:146` vs `Caliper.cpp:257,552` |
| **B3** | Arc caliper auto-width is computed from the **complementary** arc — the seeder ignores pt2 while the drawing code correctly ports the core's through-pt2 selection. A 30° arc clicked "the other way" seeds width from 330°: ten boxes at ~11× the intended width, each averaging across the whole feature. | `arc.js:186`, `:216`, `ArcPropertySheet.jsx:138` |
| **B4** | The DefConf stale-hits fingerprint omits `width`, `angleDeg`, `search_far`, `ref` (and arc `direction`/`fit_mode`), all of which change the search band. Rotate a search point 90° after a run and the old hits stay on screen pinned to the new box, reading as fresh confirmation. | `UICtrlReducer.js:220`, `EverCheckCanvasComponent.js:2240` |
| **B5** | `min_inliers > count` is accepted. `count=4, min_inliers=5` is NA for every part forever, while the overlay shows four green inlier crosses — the one screen that could explain the NA argues against it. | `LinePropertySheet.jsx:91`, `ArcPropertySheet.jsx:208` |
| **B6** | `edge.min_strength=0` is displayed and editable; the core substitutes 10. Same for `blur`→3, `mask_dilate`→8, `include_range`→2 px. `test1.hydef` sits on these hidden substitutions today. | `FeatureManager…:1174` |
| **B7** | An `undefined`/NaN limit makes all four comparisons false → returns `UOK`, a green pass. The core guards a NaN *value* explicitly but has no guard for a NaN *limit*. | `InspectionEditorLogic.js:656` |
| **B8** | `lockCaliper` is applied to line and arc property sheets but not to search_point, which keeps its `contour` default in a shape_based def that has no contour to follow. | `SearchPointPropertySheet.jsx:258` |

---

## Open — aux / composite features

| | Defect | Where |
|---|---|---|
| **X1** | The UI never uses the core's reported aux point. The core emits `auxPoints[].x/y`; `ShapeAdjustsWithInspectionResult` has no `case aux_point`, so only `status` is consumed and the intersection is re-derived in JS. Two independent implementations of the same geometry. | `InspectionEditorLogic.js:1102`, `aux_point.js:110` |
| **X3** | The core reports an aux point as **SUCCESS with a NaN position**: `acvIntersectPoint` returns NaN for parallel/degenerate lines and the result is not checked for finiteness. JSON prints it as `x:null`. `TreeExecution` also computes both dependency statuses and discards them. | `FeatureManager…:1020`, `vis_geom.cpp:43` |
| **X4** | `aux_line` guards on `subObjs.length == 2` — always true for a 2-slot ref — then dereferences a null. Any missing ref takes the whole editor down through the error boundary. `aux_point` null-checks correctly; `aux_line` does not. | `aux_line.js:37` |
| **X5** | `auxPointParse` returns `undefined` on an unresolved ref and is passed straight into an unguarded `distance_point_point`. Moving the mouse over the canvas throws. | `InspectionEditorLogic.js:1355`, `MathTools.js:2` |
| **X6** | Ref-tree walks use `if (shape.ref === undefined && shape.ref_baseLine === undefined) return false` — an `&&`, so a shape with only `ref_baseLine` falls through and dereferences `shape.ref`. Kills def load or the delete-cascade dialog. Needs a hand-edited/legacy def to reach. | `InspectionEditorLogic.js:936`, `:957` |
| **X7** | Inherent aux points are drawn from **def** geometry during inspection: `drawInherent` calls `auxPointParse(shape)` with no shapeList, so it defaults to the def rather than the inspection-adjusted clone. Every arc-centre marker renders at the taught position while everything around it renders at the measured one. | `aux_point.js:133` |
| **X8** | Smaller, all confirmed: `searchPointParse` ignores its own `shapelist` parameter; the UI's `intersectPoint` has no zero-denominator guard (the core added one); lost-ref pruning is single-pass, so pruning one shape can orphan a second that then ships in the def; inherent ids are `100100 + id*10` and collide once a user shape id reaches ~10000. | various |

### On the reference model

References are **by id** throughout, on both sides, and ids are stable across
reorder — deletion cascades correctly. The one positional dependency is the ref
**slot**: the core picks the aux subtype purely from whether `ref[1]` exists, and
reads the ids with a throwing fetch. An aux point whose slot was emptied by an
edit becomes either a silently different subtype or a hard def-parse rejection —
not a validation error. Cycles are prevented only at pick time in the UI
(`refChainHasLoop`, which does not follow `ref_baseLine`); the core's defence is
the in-progress `STATUS_NA` stamp in `TreeExecution`.

---

## Open — statistics (from the earlier read, same session)

| | Defect | Where |
|---|---|---|
| **S1** | `CP/CPK/CK` use `measure.USL`/`measure.LSL` — the **root** shape — not the per-製程 effective limits. A 製程 that tightens the tolerance produces an optimistic CPK, which is the wrong direction to be wrong in. | `spcStats.js:172` |
| **S2** | Statistics are not reset when the 製程 changes: counts judged under two different limit sets accumulate in one bucket, and the histogram range is still the one built at reset time. | `resetStatisticState` callers |
| **S3** | Per-item failure statistics exist only in one browser tab (`count_stat`, accumulated in redux). The core computes none of it and nothing persists it, so "which item fails most" is gone on reload. | `spcStats.js`, `InspectionUI.js:2135` |

Two other statistics defects found and **fixed** this session (`ab651788`): the
first occurrence of an unseen `detailStatus` was dropped, and the table summed
OK and NG from two hardcoded lists so an unrecognised status counted toward
`count` but neither column — the row silently stopped adding up.

---

## Comments that would actively mislead the next reader

- `shapes/line.js:17` "core default: count=30" and `arc.js:166` "count=36" — the
  core default is **10** for both.
- `InspectionEditorLogic.js:1216` — two contradictory instructions in one
  comment block, directly above the code deciding what a failed caliper looks
  like.
- `_caliperFields.js:7` documents the wire shape as `{count, width, length,
  step}`; the UI writes `{count, width, min_inliers, max_error}` and never
  writes `length`/`step`.
- `search_point.js:343` says the core runs `caliper_measure` along the search
  vector. It runs `search_point_cv` — a different algorithm with different
  polarity handling and different parameter units. `caliper_measure` is the arc
  path.
- `_caliperFields.js:340` `drawEdgePolarityArrow` is dead code with no caller,
  and its "RISING points into light" convention is exactly the documentation
  someone would reach for when checking A1.

---

## Suspected, not confirmed — need a mirrored-part run

- Arc caliper **box** status colours misalign on a flipped part: the core
  recomputes the arc after the pose+flip transform, so core index 0 may be the
  UI's index `count-1`. The X marks carry coordinates and are fine; the greyed
  box marking "this caliper found nothing" would appear at the wrong end.
- search_point scan side reverses for flipped parts: the core negates `vec` when
  `flip_f > 0` (i.e. **not** flipped), the asymmetric branch. The line path
  handles flip deliberately with a comment; this one looks like the same problem
  solved on the wrong branch.

Both are moot while back-side limits are disabled, but they are geometry, not
limits — they would still apply if a flipped part is inspected at all.
