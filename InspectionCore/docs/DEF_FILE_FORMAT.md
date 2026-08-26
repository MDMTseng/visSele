# The def file (`.hydef`) — structure

Current as of **2026-08-27**. Two things moved that day; both are marked.

A def is one JSON file plus **one sidecar image** (`<def>.png`). The image is
NOT embedded: ROI refine matches against its pixels, and a 7 MB template inside
every def would be paid for on every load and every wire transfer.

```
def.hydef
├─ type      "binary_processing_group"
├─ name, tag
├─ intrusionSizeLimitRatio
├─ featureSet_sha1 / _pre / _root      the recipe identity
└─ featureSet[]                        one entry today
```

`featureSet_sha1` is not decoration: it rides every inspection report into the
database as `subFeatureDefSha1`, so it is how a measurement is tied back to the
recipe that produced it.

## featureSet[0]

| group | keys |
|---|---|
| identity | `type: "sig360_circle_line"`, `ver`, `unit` |
| **object frame** | **`def_image_reg`**, `mmpp`, `cam_param` |
| which locator | `locating_engine: "shape_based"` \| absent = sig360 |
| SBM — where to extract | `localization_include` / `_exclude`, `localization_roi` |
| SBM — how to extract | `shape_weak_thres` (50), `shape_strong_thres` (80), `shape_num_features` (128), `shape_pyramid_T` |
| SBM — how to refine | `roi_refine_points` |
| SBM — at runtime | `shape_match_scale`, `shape_min_score` (50) |
| sig360 | `sig_match_sim_thres` (**core default 0.7**, not the 0.9 the editor seeds), `sig_relative_match_sim_thres`, `matching_without_signature` |
| both | `matching_angle_margin_deg`, `matching_angle_offset_deg`, `matching_face`, `morph_*` |
| the measurements | `features[]` |
| the trained result | `inherentfeatures[]` |
| NOT recipe | `__decorator` — added after the digest |

### `def_image_reg` — moved here 2026-08-27

```json
{ "cx": 15.025, "cy": 9.305, "angle": -0.00222, "isFlipped": false }
```

Where the part sits in the sidecar image. `cx/cy` in mm; **`angle` is in ROTATE
space, not image-atan2 space** — it is written from an inspection report's
`rotate`, and `image angle = -rotate`. Mixing those two spaces is the single
easiest mistake to make here; see `sbmInspectResult.js`'s `imageAngleOf`.

It decides the object frame **everything else in the def is expressed in**:
every `pt1`, every polygon, every ROI point. The core's priority is

```
def_image_reg  >  sign360 pt1  >  Otsu interior-blob centroid
```

so a def without it does not fail — it silently picks a different origin.

It used to live at the def top level, where the sub-feature parser could not
see it and a helper (`def_stamp_context`) copied it down from four call sites.
It moved because `featureSet_sha1` hashes `featureSet` only: a registration
change did not change the def hash, so the save-conflict check could not see a
reg-only edit and the database could not tell measurements apart across a
change of coordinate system. **Old defs still load** — the reader prefers the
sub-feature copy and falls back to the root.

## `features[]` — the measurement recipe

| type | what it is |
|---|---|
| `line`, `arc` | geometric primitives; `locating: "caliper"` puts them on the caliper path |
| `search_point` | one caliper straddling an expected edge |
| `aux_point`, `aux_line` | derived geometry (line crossing, centre) other shapes reference |
| `measure` | a judgement: value + limits |

A `search_point` carries `pt1`, `angleDeg`, `margin`, `width`, `search_far`,
`ref`, and an `edge` object:

```json
"edge": { "method": "first", "polarity": "falling", "nth": 0,
          "min_strength": 60, "include_range": 0, "manual_offset": 0 }
```

**`edge` is an opt-in container, and that is what makes requiredness possible.**
Its presence means somebody chose caliper mode, so `min_strength` can be
REQUIRED inside it: absent means NA with a reason, not a guessed default. A bare
new scalar could never be required, because every def written before it existed
would go NA. Prefer an object over loose scalars for exactly this reason.

`include_range` and the rest are OPTIONAL: absent, **or an explicit 0**, means
the step is not applied. Those two used to be the same thing and were silently
replaced with tuned defaults.

A `measure` carries `subtype`, `USL/LSL/UCL/LCL`, `quality_essential`,
`NAasNG` / `NGasNA`, `importance`, `ref`, `ref_baseLine`.

## `inherentfeatures[]` — what training produced

Two locators, two trained representations, one array. This is where the SBM
half landed on 2026-08-27.

```json
[ { "id": 100000, "type": "sign360",  "name": "@__SIGNATURE__",
    "pt1": {"x":15.03,"y":9.31}, "pt2": {"x":0,"y":0}, "area": 1,
    "signature": { "magnitude": [360 radii, mm], "angle": [360 angles, rad] } },

  { "id": 100100, "type": "sbm_info", "name": "@__SBM_INFO__",
    "shape_cache": { "ver":1, "fp":"v1|...", "crop":[x,y,w,h],
                     "origin":[cx,cy], "tw":548, "th":548,
                     "levels":[ {level,tl_x,tl_y,w,h,f:[...]} ] } },

  { "id": 100001, "type": "aux_point", "name": "@__SIGNATURE__.centre", "ref": [...] } ]
```

**`inherentfeatures` is a CLOSED vocabulary.** An unrecognised `type` does not
get skipped — it returns -1 and the whole def fails to parse, leaving the
engine with no features at all. `features[]` behaves the same way; that is what
the `loc_include` incident was. Adding a type means shipping the core FIRST.

### `shape_cache` and its fingerprint

```
v1|2448x2048|833348291|nf128|T4,8,|w50.00|s80.00|roi1:11|ao-0.1271|<each ROI point>
   dims   content-sum  extraction params        reg angle, deg
```

The core recomputes this and **refuses a cache that does not match**, loudly.
So the cache can go stale but cannot go wrong. What invalidates it:

- the reference image changed (dimensions or content),
- an extraction parameter changed,
- the ROI points changed,
- **`def_image_reg.angle` changed** — the one that bites, because a save used
  to be able to move it.

A stale cache means the SBM path is skipped and sig360 covers. **That still
locates**, so the screen looks normal — which is why the report carries
`locate.code = "untrained"` and the inspection screen shows a red banner.

## What the hash covers

```js
featureSet_sha1 = JSum.digest(report.featureSet, 'sha1', 'hex')   // BEFORE __decorator
```

| | in the hash |
|---|---|
| `features[]` | yes |
| `inherentfeatures[]` — signature AND trained SBM features | **yes, as of 2026-08-27** |
| `def_image_reg` | **yes, as of 2026-08-27** |
| every parameter | yes |
| `__decorator` | no — per-session UI state, added after the digest |

So today: **changing the registration, the ROI points, or regenerating the
features all count as a def revision.** None of the three did before.

That was a deliberate reversal for the trained features, which used to be
added after the digest so a def with a cache and one without hashed alike. The
argument for the change: what the machine matches against is part of the
recipe, the signature is hashed for exactly that reason, and an entry in
`inherentfeatures` that is not hashed while its siblings are is a subtlety
somebody trips over. The cost — press 生成特徵點, save, and the def is a new
version to everything keyed on the hash — is real and was accepted.

**Testing note:** delete `featureSet_sha1` from a hand-edited def and the
save-conflict dialog stops appearing; both sides treat an absent hash as
"nothing to compare". Those reports then reach the database with an empty def
identity, so do not use it for a run whose data matters.

## What is NOT in the def

| | where it lives | why |
|---|---|---|
| the reference image | sidecar `<def>.png` | ROI refine reads its pixels; embedding 7 MB would be paid on every load |
| the working region / station | `machine_setting.json` | per machine, not per recipe |
| 製程 margins | `__decorator.control_margin_info` | folded into `shape_list` when the wire def is built |
| lens / field calibration | `data/lens_calib.json`, `field_calib.json` | per machine |

## Where the code is

| | |
|---|---|
| writes the def | `UI/WebUI/src/UTIL/MISC_Util.js` — `defFileGeneration` |
| reads it into the editor | `UI/WebUI/src/UTIL/InspectionEditorLogic.js` |
| parses it in the core | `InspectionCore/MatchingEngine/FeatureManager_sig360_circle_line.cpp` — `parse_jobj` |
| trains / loads the cache | same file — `trainShapeMatcher`, `shape_cache_load` |
| stamps context onto old defs | `InspectionCore/Core0_1/wiringPanel.cpp` — `def_stamp_context` |

## Contracts that hold this shape

```bash
cd UI/WebUI && node tools/pipeline_contract.mjs      # the def round trip, the hash rules
cd InspectionCore && python test_suite/insp_contract.py --exe build/win-mingw-msys/visSele.exe
```

The second one drives the real binary through `--insp`, which is the only way
to exercise def parse → cache → localization → caliper → judgement from a
shell. Use `INSP_AREA_BYPASS=1` when inspecting a TRAINING image: `--insp`
enforces the production station and the part in a training image is not at it.
