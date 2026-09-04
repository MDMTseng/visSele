# The shape_based def format, as the core reads it — and what migration must produce

Written after a full review of the sig360 → shape_based migration path on
2026-09-04, prompted by "load test2 in the WebUI, upgrade it, and the result is
flagged as the old format". It was. This records the format, what was wrong,
what was decided, and what was measured after the fix, so the next person can
check a def against it instead of re-deriving it.

## The format (authority: `FeatureManager_sig360_circle_line.cpp`, the reader)

| Field | Canonical place | Legacy place still read |
|---|---|---|
| `locating_engine: "shape_based"` | `featureSet[0]` | — |
| `def_image_reg {cx, cy, angle, isFlipped}` | `featureSet[0]` | def top level |
| `shape_cache {ver, fp, crop, origin, levels, roi{half, at, pts, png}}` | `inherentfeatures[]` entry `{id:100100, type:"sbm_info", name:"@__SBM_INFO__"}` | `featureSet[0].__shape_cache` |
| `localization_include` / `localization_exclude` | same `sbm_info` entry | `featureSet[0]` root |
| `roi_refine_points` | same `sbm_info` entry | `featureSet[0]` root |

**`shape_cache.roi` is what makes a def self-contained.** It holds the pixel
windows the ROI refine stage reads (one PNG strip + origins + the frozen point
selection). With it, the core needs no picture on disk. Without it, only the
coarse pyramid levels exist.

`roi_refine_points` semantics in the core: **absent = auto-select; empty array
= explicitly none** (no windows are baked, the cache comes out without `roi`).

## Which shipped cores can read what

| Core | reads `sbm_info` | cache without `roi` |
|---|---|---|
| 1.1.104 (2026-08-26) | **no — the whole def fails to parse** (closed vocabulary) | — |
| 1.1.105 (2026-08-28) | yes | runs coarse, warns once |
| 2.0.0-rc2 before this change | yes | refused (`-2`), silent sig360 fallback |
| 2.0.0-rc2 after this change | yes | **runs coarse, reports `locate.code = "coarse_only"` on every frame** |

A def saved by the rc2 WebUI is unreadable by a 1.1.104 core. Check the fleet
before publishing recipes.

## What was wrong (all fixed in the same commit as this file)

1. **Save always wrote `roi_refine_points: []`.** The core read that as
   "explicitly none", baked no windows, and the by-the-book flow
   升級 → 生成特徵點 → 存檔 produced the old format. Now the key is written only
   when the operator placed points.
2. **自動產生 ROI 點 sent `SF` without `regenerate`.** Auto-selection needs the
   refine candidates, which only extraction produces; without regenerate the core
   could neither extract nor load a roi-less cache, so it returned nothing and
   the `.catch(() => {})` hid it. Now regenerates, commits the returned cache
   with the points, and shows an error when empty.
3. **Migration seeded `def_image_reg.angle` from the sig360 extractor report,
   whose `orientation` is 0 by definition.** test2's part sits at −0.0233 rad;
   the frame came out turned by 1.33°, fingerprint `ao-1.3334` → `ao0.0000`.
   Now seeded from the entry inspection report (`inspReport.reports[0].rotate`),
   the same source the TAKE path uses.
4. **The core refused roi-less caches and fell back to sig360, and the banner
   said "機器不會載入".** Neither was what happened. Decision (2026-09-04): ROI is
   the bonus, not the entry ticket — load the levels, run coarse, say so.
5. `upgrade_defs.mjs` failed on every WebUI-saved def and blamed the core;
   it also sat on a refused websocket until timeout (the core admits ONE client
   unless `INSP_ALLOW_MULTI_CLIENT=1`). Both messages now say what happened.

## Measured after the fix (test2, stripped to pure sig360, driven through the real UI)

| Step | Result |
|---|---|
| 升級 | `def_image_reg.angle = -0.02327`, matches the original def |
| 生成特徵點 (no points placed) | `shape_cache.roi` present, 8 auto windows, fp `…|ao-1.3334` |
| 自動產生 | 8 points, cache re-committed with them |
| save output | `sbm_info {shape_cache, roi_refine_points}`; no `roi_refine_points` key when none placed |
| II on the result | `locator: shape_based`, no `locate` note, similarity 1.0000, 7/7 judges SUCCESS |
| II on a roi-less def | `locator: shape_based`, `locate.code: coarse_only`, object found (sim 0.998) |
| `upgrade_defs.mjs --dry-run` on a WebUI-saved def | `ok (8 windows, levels identical)` |

## Still open

* `upgrade_defs.mjs` upgrades shape_based defs only. A sig360 def still needs
  the WebUI button; a batch converter for the 32 field recipes would have to
  run an inspection per def to get `rotate`.
* The include region has no "start from the sig360 silhouette" button; the
  core uses the silhouette when none is drawn, but nothing on screen says so.
