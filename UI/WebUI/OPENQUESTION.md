# Open questions — WebUI rework (branch `webui/editor-refactor`)

Decisions deferred for the user. Each is a scope/priority call, not a blocker.
Context for all: regression net is `tools/webctl/` — `golden.mjs` (serialized-def
oracle) and `flows.mjs` (behavioral: load/select/edit, verifies re-renders).

---

## Q1. Collapse `edit_info.list` → single source `_obj.shapeList`? (orig refactor item #2)

**What:** `edit_info.list` is a mirror of `edit_info._obj.shapeList`, re-synced by hand
at 4 points. The duplication is a footgun (forget to re-sync → divergence).

**Why it's not a simple change:** `InspectionEditorLogic.SetShape()` mutates the array
**in place** — `this.shapeList.push(shape)` (line ~1003) and `this.shapeList[idx] = shape`
(line ~1007). So `_obj.shapeList`'s reference does NOT change on add/edit. Three
components select `edit_info.list` via reference-sensitive `useSelector`/`mapState`
(DefConfUI, AnalysisUI, InspectionUI). Re-pointing them to `_obj.shapeList` would break
their re-render on add/edit, because the array ref is stable across in-place mutation.

**To do it safely** SetShape (and other mutators) must switch to immutable array
replacement (`this.shapeList = [...]`), then components can select `_obj.shapeList`.

**Decision needed:** worth the immutability refactor (+ full flows re-verification), or
leave the cheap `list` mirror as-is? Reward is modest (remove one footgun); risk is real.
Recommendation: **leave as-is** unless we're already in this code for another reason.

---

## Q2. Split the `edit_info` god-object?

**What:** `edit_info` mixes the model (`_obj`), editor selection (`edit_tar_*`), def
metadata (DefFileName/Hash/...), inspection results (inspReport/reportStatisticState),
and matching params. It's read by 7 components. Splitting into named sub-objects is the
highest-clarity change for "easier to mod".

**Why deferred:** large blast radius (7 readers) and the same reference-sensitive
re-render concerns as Q1. Now partially verifiable via `flows.mjs`, but flows cover
DefConfUI select/edit — not all 7 readers / all modes.

**Decision needed:** prioritize this (big, high-value clarity) vs. the comm-layer
extraction vs. the whiteListKey typed-schema editor? Would want broader flow coverage
(create/delete shape, save, inspection, analysis mode) before attempting.

---

## Q3. Next rework section

Remaining candidates (all larger, fresh-session sized):
- **Comm layer**: extract a thin typed `bpgClient` (sendCommand→Promise) out of
  `script.jsx`/`MW_API`. High leverage, medium risk (touches connection logic).
- **whiteListKey typed schema**: unify `JsonEditBlock` spec + `Shape_Attr_Fill` defaults
  into one per-shape schema. Contained, golden+flows-verifiable, improves shape editor.
- **edit_info god-object split** (Q2).

Recommendation order: broaden `flows.mjs` coverage first → whiteListKey schema → comm
layer → god-object split.
