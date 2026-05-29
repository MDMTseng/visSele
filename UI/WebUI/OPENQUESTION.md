# Open questions — WebUI rework (branch `webui/editor-refactor`)

Decisions deferred for the user. Each is a scope/priority call, not a blocker.
Context for all: regression net is `tools/webctl/` — `golden.mjs` (serialized-def
oracle) and `flows.mjs` (behavioral: load/select/edit, verifies re-renders).

---

## Q1. Collapse `edit_info.list` → single source `_obj.shapeList`? — ✅ DONE

Resolved: `SetShape` now replaces the array immutably (drag still commits only on
mouse-up, so no per-frame re-render impact — the per-frame work is the canvas's
imperative `draw()`). The `list` field, its 4 sync points, and all consumers
(3 component selectors + 11 canvas draw reads + reducer/logic internals) now read
`_obj.shapeList`. Verified: golden + flows PASS, canvas + shape-list render, Insp/
Analysis modes mount clean.

(original analysis below for history)

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

## Q2. Split the `edit_info` god-object — APPROVED (coarse), planned

**Direction (user):** yes, split it — but **coarse, not finely fragmented**. A handful
of cohesive groups, not many micro-slices.

**Splitting philosophy (applies project-wide):** keep complexly-interrelated / stateful /
side-effectful logic together in one file (size is fine); only extract pieces that are
pure / functional / side-effect-free or intuitively self-contained. So: this is a *state
grouping* (coarse buckets) — NOT a directive to fragment the coupled stateful model
(`InspectionEditorLogic`) or the reducer. Good *code* extraction targets are pure helpers
(geometry, serialization, pure transforms).

**Proposed coarse grouping (3 buckets):**
1. **editor (cold)** — `_obj`, `inherentShapeList`, `edit_tar_info`, `edit_tar_ele_trace`,
   `edit_tar_ele_cand`, `__decorator`.
2. **defMeta (cold)** — `DefFileName/Tag/Hash*`, `loadedDefFile`, `defModelPath`,
   `inspOptionalTag`, `matching_*`, `intrusionSizeLimitRatio`.
3. **runtime/results (HOT)** — `img`, `inspReport`, `reportStatisticState`, `statSetting`,
   `sig360info`, `stage_light_report`, `mouseLocation`. Separating these is the perf win
   (cold consumers stop re-rendering on every image frame / report — the "避免重繪" concern).

**Churn reality:** large. `edit_tar_info` alone has ~40 access sites in DefConfUI, plus
the canvas (`edit_DB_info.*`), AnalysisUI, and the reducer/model. Every field move = many
read-site renames. `flows.mjs` only covers DefConfUI select/edit, not all modes.

**Safe approach (decided):** do NOT hand-rename ~40 sites blind.
1. **Type the consumers first** (progressive TS now in place): convert / `// @ts-check`
   the files touching a group, typed against `EditInfo` in `src/domain.d.ts`.
2. Move one group, update `domain.d.ts`, run `npm run typecheck` → tsc lists every missed
   site (compiler-verified rename).
3. Verify with golden + flows + manual mode checks. One group per commit.
4. Broaden `flows.mjs` (create/delete shape, save, inspection, analysis) before/with the
   HOT group, since those paths aren't covered yet.

Execute as the next focused pass.

---

## Q3. Next rework section

Remaining candidates (all larger, fresh-session sized):
- **Comm layer**: extract a thin typed `bpgClient` (sendCommand→Promise) out of
  `script.jsx`/`MW_API`. High leverage, medium risk (touches connection logic).
- **whiteListKey typed schema**: unify `JsonEditBlock` spec + `Shape_Attr_Fill` defaults
  into one per-shape schema. Contained, golden+flows-verifiable, improves shape editor.
- **edit_info god-object split** (Q2).

Recommendation order: broaden `flows.mjs` coverage → type the `edit_info` consumers →
Q2 god-object split (group-by-group, tsc-verified) → whiteListKey schema → comm layer.

---

## North-star (long-term direction, not now): per-shape vertical slices

Co-locate everything for a given shape/primitive type (line, arc, circle, search_point,
measure, aux_*) into ONE module per shape — its **setup component** (property sheet),
its **canvas control** (interaction/edit/hit-test), and its **draw** logic, plus its
defaults/schema. Goal: adding/extending a primitive = a single-module change instead of
editing DefConfUI + the canvas + renderUTIL + Shape_Attr_Fill separately. "Group by
feature (shape), not by layer" — fits the cohesion philosophy.

How it relates: the layer-based `renderUTIL`/`CameraCtrl` extraction is a fine
intermediate; later, each shape's draw would move from `renderUTIL` into its shape module
(renderUTIL becoming a thin dispatcher). The **whiteListKey typed schema** above is the
natural FIRST step (a per-shape schema would live in the shape's module). Steer toward
this only when extending shapes / already touching these seams — don't fragment early.
