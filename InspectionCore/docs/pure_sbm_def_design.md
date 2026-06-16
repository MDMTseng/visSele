# Pure-SBM def — design & plan (X1.2)

Status: **DESIGN / PLAN ONLY — not yet implemented.** Branch `ct/X1.2_dev`.
Author hand-off: ct (繁中). Companion notes: `visSele_2.0_shape_based_redesign.md`,
memory `shape-based-localizer-x12`, `morph-similarity-rbf`.

## 1. Goal

Make the def file **self-contained as pure SBM**: the shape localizer (line2Dup +
ROI refine) must run **without reading any sig360 block** (no `@__SIGNATURE__`
feature, no signature radius array, no sig360 `pt1`).

- **Migrated def** = functionally pure SBM, but **keeps the original sig360 block
  dormant** for A/B and per-def revert (flip `locating_engine` back). The SBM path
  must NOT read the sig360 data — it reads only SBM-native fields.
- **Fresh def** = pure SBM, no sig360 block authored at all.

Non-goals (later increments): deleting `ContourGrid`/signature C++ code; the
caliper-only refactor.

## 2. Current sig360 coupling (what must be broken)

Verified in `FeatureManager_sig360_circle_line.cpp`:

| Train-time input | Source today | Native? |
|---|---|---|
| **silhouette / feature-extraction mask** | `raw_sig_radius` (sig360 signature, mm) → `fillPoly` → dilate (`trainShapeMatcher` ~6432) | ❌ **the only real coupling** |
| origin + reg angle/flip | `def_image_reg` (top-level `{cx,cy,angle,flip}`, parse ~2052) | ✅ already native |
| origin fallback | sig360 `pt1` (`ref_center_mm`, used only when no reg) | drop for pure SBM |
| `def_mmpp` | def `mmpp` | ✅ generic |
| `localization_roi`, `anchor_corner`, calipers | already native | ✅ |

Already in place (no work needed):
- Parse tolerates a missing signature when `locating_engine == 1`
  (`parse_jobj:2276` — the `matching_without_signature` requirement is gated off).
- `trainShapeMatcher` already falls back to an Otsu interior-blob mask when
  `raw_sig_radius` is empty — but that fallback is fragile (cage frame) and is **not**
  what we want for authored defs.
- Runtime magnification portability (`def_mmpp/current_mmpp` template scaling via
  `ensureShapeScale`/`buildShapeMatcher`) is **done** and orthogonal to this.

So the entire job reduces to: **add a native mask source** + an authoring/migration
path that fills it.

## 3. New SBM-native def schema (on `featureSet[0]`)

All polygons are **object-frame mm, origin-relative, canonical (part-upright)
orientation** — the SAME frame `localization_roi` already uses, so they render through
the existing `TemplateDomain_TO_PixDomain(pt, reg_sin=-sin(reg), reg_cos, reg_flip_f,
originPx, def_mmpp)` at train time with no new transform.

```jsonc
{
  "locating_engine": "shape_based",
  // Registration / origin (re-settable in the authoring UI). Stays where it is today
  // (top-level def_image_reg) OR mirrored onto the featureSet — see Open decision D2.
  "def_image_reg": { "cx": <mm>, "cy": <mm>, "angle": <rad>, "isFlipped": <bool> },

  // NEW — feature-extraction region authoring. mask = union(include) AND-NOT union(exclude).
  "localization_include": [ [ {"x":mm,"y":mm}, ... ],  ... ],  // ≥1 polygon
  "localization_exclude": [ [ {"x":mm,"y":mm}, ... ],  ... ],  // 0+ polygons (avoid areas)

  // EXISTING — kept. Optional extra AND restriction (rigid sub-region). If only
  // localization_roi is present (legacy migrated def), it acts as the include region.
  "localization_roi": [ {"x":mm,"y":mm}, ... ],

  // NEW (D3) — optional user-edited ROI-refine sample points (object-frame mm). When
  // present, the matcher uses these instead of auto selectOptimizedPoints(); absent =>
  // auto. Authored in the settings window after an auto pass.
  "roi_refine_points": [ {"x":mm,"y":mm}, ... ],

  // existing
  "shape_match_scale": 0.3,
  "morph_mode": "...", "morph_alpha": ..., "anchor_corner": <per search point>,
  // mmpp at top level / featureSet -> def_mmpp
}
```

Mask resolution priority in `trainShapeMatcher` (new):
1. `localization_include` (− `localization_exclude`) — **pure SBM native**.
2. `localization_roi` alone (legacy) used as include.
3. `raw_sig_radius` (un-migrated def that only flipped the engine flag) — back-compat.
4. Otsu interior blob — last-resort fallback.

A migrated or fresh def always hits (1), so it never reads sig360 (3).

## 4. Manual authoring workflow (WebUI) — fresh pure-SBM def

Per ct: the fresh def's region is **drawn by hand**, not auto-binarized.

On the def-editor canvas (reference image shown), the user can:
1. **Draw include polygon(s)** — where line2Dup features are generated.
2. **Draw exclude polygon(s)** — "avoid generation" areas subtracted from include
   (e.g. a logo, a moving sub-part, a reflective patch).
3. **Add / delete** any include/exclude polygon (a "ROI set" the user edits freely).
4. **Set / re-set localization info** — pick the origin point + orientation
   (and flip) on the reference image → writes `def_image_reg`.
5. (Preview) overlay the resulting mask; optionally a "train preview" round-trip to
   the core that renders the extracted feature points (`SHAPE_DRAW_ROI` already does
   this server-side) so the user sees what the region yields before saving.

Pixel→object-frame-mm conversion (UI side), inverse of the core's render transform:
given a canvas pixel `p`, origin pixel `o = reg.center / def_mmpp`, reg angle `φ`,
flip `s`:
```
v   = (p - o) * def_mmpp           // mm, image frame, origin-relative
obj = R(+φ) · diag(1, s) · v       // remove reg -> canonical object frame
```
(The core applies `R(-φ)`·flip when rendering — see the −reg note in
`shape-based-localizer-x12`; UI must use the exact inverse, verify sign against a
SHAPE_DUMP_MASK round-trip on a non-zero reg def.)

## 5. Migration (one-click) — sig360 → pure SBM, sig360 kept

`ACT_Migrate_To_Shape` / `defFileGeneration` (`MISC_Util.js`):
1. Set `locating_engine = "shape_based"`, `shape_match_scale = 0.3` (today).
2. **NEW — bake the silhouette**: convert the sig360 signature
   `signature_data[i] = (R mm, θ rad)` → `localization_include = [ [ (R·cosθ,
   R·sinθ) for each non-empty bin ] ]`. The signature is already mm/object-frame, so
   this is a pure reshape (no mmpp needed for the silhouette itself; this realises the
   user's "store feature positions in mm" intent and is magnification-portable).
3. Keep `def_image_reg` (already carried over on re-save).
4. **Keep the sig360 block** (the `@__SIGNATURE__` feature stays). The SBM path now
   reads `localization_include`, so sig360 is purely dormant.
5. Carry `anchor_corner`, morph params untouched.

Result: a def whose SBM localization no longer depends on sig360, while sig360
remains for A/B. The user can then open the authoring UI and edit the baked region.

## 6. Core changes (`FeatureManager_sig360_circle_line.{cpp,h}`)

1. `parse_jobj`: parse `localization_include` (array of polygons) and
   `localization_exclude` into `std::vector<std::vector<acv_XY>> loc_incl_mm,
   loc_excl_mm;` (mirror the existing `localization_roi` parser ~2005).
2. `trainShapeMatcher` mask block (~6425): build mask from include−exclude FIRST
   (render each polygon via the existing `TemplateDomain_TO_PixDomain`, `fillPoly`
   include into mask, `fillPoly` exclude to subtract, then the same ~5px dilate on the
   include before subtract). Fall through to `localization_roi`, then
   `raw_sig_radius`, then Otsu — in that priority. `mask_src` string updated for the
   diagnostics.
3. Drop sig360 `pt1` from the origin priority when a reg is present (already the
   case); leave it only as a legacy fallback.
4. (D3) Parse `roi_refine_points` (object-frame mm); when non-empty, pass them to the
   FeatureSet in place of the auto `selectOptimizedPoints()` (submodule API may need a
   `setRefinePoints(pts)` / explicit-points addModel path — confirm in P3).
5. No change needed to the no-signature parse guard (already gated) or to
   `needsBinaryPreprocessing()` (already `!(locating_engine==1 && shape_ready)`), but
   **verify** a def with NO sig360 sub-features takes the raw-gray group fast path end
   to end (`FeatureManager_binary_processing_group::FeatureMatching`).

## 7. WebUI changes

- `MISC_Util.js defFileGeneration`: emit `localization_include` / `localization_exclude`
  (+ for migration, the baked include from the signature). For a fresh pure-SBM def,
  do **not** emit the `@__SIGNATURE__` feature.
- New canvas authoring tools (DefConfUI + the shape/redux layer): draw/add/delete
  include & exclude polygons; set localization origin+angle; pixel→mm per §4; live mask
  overlay.
- Keep `stampRefImagePath` (runtime `_ref_image_path`) — unchanged.

## 8. Backward-compat / A-B

- Migrated def: native include present + sig360 dormant → core uses native; flip
  `locating_engine` back to `sig360` → uses the kept block. A/B preserved.
- Old shape def with only `raw_sig_radius` (engine flag flipped, never migrated):
  still works via priority (3).
- Fresh def: native only; no sig360.

## 9. Validation plan

1. **Bake parity**: migrate `B5S_dddddn2`; `SHAPE_DUMP_MASK` of the baked
   `localization_include` must match the current signature-derived mask pixel-for-pixel;
   detection + measures unchanged vs current shape path.
2. **Pure-SBM proof**: by hand, strip the sig360 block from a migrated def → it must
   train + inspect **identically** (proves zero sig360 dependency).
3. **Manual author**: draw include + exclude on a ref image → mask matches intent;
   `SHAPE_DRAW_ROI` shows features only inside the region; localization stable.
4. **Cross-mmpp**: pure-SBM def on a different-`mmpp` frame (ties in the
   `ensureShapeScale` work).
5. **Regression**: a sig360 def is byte-identical; a migrated def reverted to sig360
   matches the pre-migration baseline.

## 10. Decisions (ct, 2026-06-16)

- **D1 field naming — pending final OK, default A**: two new explicit arrays
  `localization_include` / `localization_exclude` on `featureSet[0]`;
  `localization_roi` stays as-is (optional rigid-AND). (Alternative B — fold into a
  `localization_roi:{include,exclude}` object — rejected: it changes the existing flat
  `localization_roi` shape and needs version handling.)
- **D2 reg location — DECIDED: keep top-level `def_image_reg`.** Loader already copies
  it into the featureSet; no move.
- **D3 ROI-refine points — DECIDED: auto for v1, user-editable in the settings
  window.** `selectOptimizedPoints` auto-generates; the def editor's settings window
  lets the user adjust/pin/remove the ROI sample points afterward. (Persist the edited
  point set as a native field, e.g. `roi_refine_points` in object-frame mm, consumed by
  the matcher in place of auto-selection when present.)
- **D4 migration bake — DECIDED: auto-bake on migration, editable afterward.** The
  one-click migrate auto-bakes `localization_include` from the signature; the user can
  then edit the region (and the ROI points per D3) in the authoring UI.

## 11. Phasing

- **P1 (core) — DONE (commit c9f734fc).** `localization_include/exclude` parsed into
  `loc_incl_mm`/`loc_excl_mm`; mask priority include(−exclude)>signature>Otsu in
  `trainShapeMatcher`; no-sig360 path verified. Bundled the mmpp portability change.
- **P2 (migration bake) — DONE (commit 3542ef89).** `defFileGeneration` bakes the
  signature → include; sig360 kept. Bake parity + pure-SBM proof validated.
- **P3 (authoring UI) — DONE (commit fbc68e9e).** loc_include/loc_exclude shapes +
  click-to-add-vertex tool; save strips them from features → localization arrays; load
  rebuilds them; registration + ROI-pts settings panel. **Frame note:** shapeList is
  object-frame mm (def `unit:"px"` is a legacy misnomer), so polygon points = the
  localization arrays verbatim — no pixel→mm transform needed (§4's formula is moot).
  Validated via node-replicated save + core. KNOWN GAP: pre-filling the auto
  roi_refine_points into the override editor needs a core round-trip (empty = auto today).
- **P4 — TODO**: fresh pure-SBM def end to end (author from scratch, no sig360 emitted);
  cross-mmpp (§9.4).
