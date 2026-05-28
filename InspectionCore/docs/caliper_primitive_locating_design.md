# Caliper / Section-Based Primitive Locating — Design

Status: **DESIGN** (not yet implemented). Branch `refactor/inspection-img-streaming`.
Companion code already landed: the edge-selector toolbox (`MatchingEngine/EdgeSelect.{h,cpp}`).

## 1. Motivation

Today every measurement edge is found the same way: `edgeTracking::calc_info`
(`MatchingCore.cpp`) tracks **every contour pixel** and takes the **centroid of
the gradient profile** of a 1-pixel-wide scan. Two problems:

- **Wrong point on non-standard edges.** The centroid lands *between* peaks /
  off the boundary for double/nearby edges, shadows, asymmetric or rounded
  edges. (Validated: weak shadow @8 + true edge @20 → centroid 17.3 vs true ~20.)
- **Per-pixel = noisy + slow.** A 1-pixel scan has poor SNR; tracking thousands
  of contour pixels is expensive, and the result depends on a clean binary
  contour (binarize → label → contour-trace).

Mainstream metrology (Halcon measure objects / `measure_pos`, Cognex/Keyence
**caliper** tools) instead uses **caliper / section / projection** measurement.
This design adopts that model.

## 2. Concept

A **caliper** is a small measurement section straddling the expected edge:

```
        projection (average) direction  ║  (parallel to the edge)
   ┌───────────────────────────────────┐
   │   ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·   │   length L  ── search direction
   │   ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·   │   (perpendicular to the edge,
   │   ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·   │    across the boundary)
   └───────────────────────────────────┘
            width W (averaged)
```

Per caliper:
1. Sample the **grayscale** on a grid of L (across edge) × W (along edge).
2. **Project**: average the W samples per row → one clean 1-D profile of length L
   (SNR improves ~√W).
3. Differentiate → gradient profile → **`edge_select`** picks the sub-pixel edge
   (method/polarity per primitive: strongest default; first/last/middle/nth).
4. Map the sub-pixel offset back to image coords → one **edge point** + a quality
   (peak strength) for weighting.

Place N calipers along the nominal primitive (from the def + the object pose),
collect N edge points, then **robust-fit** the primitive.

Backlight compensation (`sampler->sampleBackLightFactor_ImgCoord`) and the lens
`img2ideal` correction apply at the sampling/point stage exactly as today.

## 3. Per-primitive caliper placement

- **Line** (`featureDef_line` p0,p1): N calipers evenly spaced along p0→p1,
  oriented **perpendicular** to the line; L = search margin (`initMatchingMargin`),
  W = caliper width (new). Edge points → robust line fit.
- **Circle/arc** (`featureDef_circle` pt1,pt2,pt3 → center,r,angle span): N
  calipers spaced by angle along the arc, oriented **radially**; project
  tangentially. `outter_inner` → polarity. Edge points → robust circle fit.
- **Search point** (`featureDef_searchPoint`): a single caliper along the search
  ray at the anchor; returns the selected edge point (no fit).
- **Aux point**: unchanged (geometric intersection / center of fitted primitives).

Pose: calipers are placed using the object's matched rotation/flip/center
(cached_cos/sin/flip_f, calibCen) — same pose the current code uses. So the
**measurement decouples from the binary contour**; only the pose (orientation
signature) still needs it (or use the edge-response signature).

## 4. Edge selection (the toolbox)

Each caliper calls `edge_select(signedGrad, L, params, &pos, &strength)`:
- `method`: STRONGEST (default) | FIRST | LAST | MIDDLE | NTH
- `polarity`: ANY | RISING(dark→light) | FALLING
- `min_strength`, adaptive noise floor, sub-pixel parabola.
Per-primitive, so a tricky line and an easy arc can differ. Default
strongest/any reproduces the old centroid result on clean edges.

## 5. Robust fitting

- **Line:** keep RANSAC + percentile outlier rejection (already present in
  `SingleMatching_line`); feed caliper points (fewer, cleaner) instead of contour
  pixels. Weight by caliper edge strength.
- **Circle:** add Tukey/RANSAC outlier rejection around `CircleFitByHyper`
  (currently weighting only).
- Per-caliper quality (strength, or "no clear edge") → low weight / drop. Report
  the count of valid calipers + fit residual (existing `s`, `matching_pts`).

## 6. Def schema (backward compatible)

Add an optional per-primitive block; absent ⇒ current contour method (bit-for-bit).

```json
"locating": "caliper",            // primitive-level: "contour"(default) | "caliper"
"caliper": {
  "count": 30,                    // # sections (or auto from primitive length)
  "width": 9,                     // W, pixels averaged along the edge
  "search": 20,                   // L, half-length across the edge (px)
  "spacing": "uniform"
},
"edge": { "method":"strongest", "polarity":"any", "nth":0, "min_strength":0 }
```

`edge` is shared with the calc_info path. `caliper` only applies when
`locating=="caliper"`. Reports (`acv_LineFit`/`acv_CircleFit`,
`rotate/isFlipped/similarity`, judge USL/LSL) are **unchanged**.

## 7. Rollout / backward compat

- New `locating:"caliper"` per primitive; default `"contour"` keeps every existing
  project bit-for-bit.
- On clean edges, caliper + strongest ≈ the old result (validate equivalence).
- Behavior-changing where it matters (non-standard edges) → migrate + validate
  part-by-part on the rig. (User accepted "new default" eventually; staged via the
  flag is safest.)

## 8. Setup UX (how the user dials in the right primitive)

- **Mechanism:** the `caliper` + `edge` def blocks (count/width/search +
  method/polarity).
- **Future WebUI:** per-primitive panel showing the placed calipers overlaid on
  the image, and for a selected caliper a **live 1-D profile + gradient plot** with
  the candidate peaks and the selected one — click a peak to set method/polarity.
  This is how the user *sees* and fixes a wrong edge.
- **Now (no UI):** core can emit per-caliper diagnostics (profile + candidate
  peaks) for a debug client.

## 9. Implementation plan (phased)

1. **Caliper sampler + edge** (pure, testable): given image, caliper geometry
   (center, edge-dir, L, W, step) + `EdgeSelectParams` → one edge point + strength.
   Reuse `sampler` for backlight/`img2ideal`. Unit-test on synthetic edges
   (clean, double, asymmetric, noisy) vs the per-pixel result.
2. **Line caliper locator**: place calipers along p0→p1, gather points, robust
   line fit. Validate accuracy/robustness/speed vs current on synthetic lines.
3. **Circle caliper locator** + circle outlier rejection.
4. **Search-point caliper**.
5. **Def parsing** (`locating`,`caliper`,`edge`) + wire into
   `SingleMatching_line`/`_circle`/searchPoint behind the flag.
6. **Diagnostics** (per-caliper profile/peaks) for setup.
7. Rig validation; then consider flipping defaults.

## 10. Open questions

- Auto `count`/`width` from primitive length & expected edge sharpness?
- Caliper length L vs the search margin: reuse `initMatchingMargin` or separate?
- For arcs, equal-angle vs equal-arc-length caliper spacing.
- Quality/verification gate to flag "edge not found in K calipers" → NA.
- Keep the contour path available indefinitely (legacy projects) — yes.

## References
- Edge-selector toolbox: `MatchingEngine/EdgeSelect.{h,cpp}` (commit 97e51664).
- Current locating map: see memory `project-primitive-locating`.
- Halcon Solution Guide III-B "1D Measuring" (caliper/measure objects); Cognex
  caliper tool docs.
