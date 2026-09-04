// How a contour-mode primitive becomes a caliper-mode one.
//
// ONE COPY, because there are two callers and they must not drift:
//
//   the WebUI      _caliperFields.caliperField()'s derive, i.e. what a shape
//                  gets the moment somebody switches `locating` to 'caliper'
//   Node           tools/def_convert.mjs, which converts whole def files
//                  offline so a migration can be measured before it is made
//
// The conversion belongs to the WebUI -- the core has no business rewriting a
// recipe. Extracting the rule rather than reimplementing it is what keeps the
// offline verification honest: it runs the same function the editor runs, so a
// def converted by the tool is byte-identical to one converted by hand.
//
// Deliberately a LEAF module. No React, no path aliases, no renderer -- Node
// imports it directly, without Vite. MathTools is imported relatively for the
// same reason, and is itself import-free. The .js extension is required: Node
// ESM will not resolve an extensionless relative specifier, and Vite accepts
// the explicit one either way.
import { arcSweep } from '../UTIL/MathTools.js';

// What the core reads, and what happens when it is missing.
//
//   caliper: { count, width, length, step, min_inliers, max_error }
//
// NESTED. The parser takes them from that sub-object
// (FeatureManager_sig360_circle_line.cpp: `cJSON *calo = JFetch_OBJECT(obj,
// "caliper")`), so keys written at the shape's top level are not read at all
// and every value silently falls back to the parse defaults -- count 10 and
// width 0.5mm. On a 40px corner that is a 50px-wide caliper. The 2026-08-26
// audit called this out for a def "authored elsewhere"; an offline converter
// that wrote the flat form hit exactly it, and every measurement taken through
// it was of the defaults rather than of the values being tested.
export const CALIPER_COUNT_DEFAULT = 10;

// length and step are deliberately NOT seeded: the core's sentinels are what is
// wanted. length falls back to initMatchingMargin -- the contour search depth
// the def already carries -- and step to 1px.
export const CALIPER_SEED_MIN_INLIERS = 5;
export const CALIPER_SEED_MAX_ERROR = 0.1;

// Along-edge length in def mm, per shape type. This is what the boxes tile.
export function lineGeomLength(s) {
  if (!s || !s.pt1 || !s.pt2) return 0;
  return Math.hypot(s.pt2.x - s.pt1.x, s.pt2.y - s.pt1.y);
}

// arcSweep, not the chord and not the two-chord path: both understate a real
// sweep, and the understatement grows with curvature, so the tightest arcs --
// which have the fewest boxes already -- would get the fewest of all. arcSweep
// is also the function that knows the sweep is the one THROUGH pt2; using the
// complementary arc seeds a width up to 11x too large (fixed 2026-08-26).
export function arcGeomLength(s) {
  if (!s || !s.pt1 || !s.pt2 || !s.pt3) return 0;
  const a = arcSweep(s.pt1, s.pt2, s.pt3);
  return (a && Number.isFinite(a.length)) ? a.length : 0;
}

export function geomLengthOf(shape) {
  if (!shape) return 0;
  if (shape.type === 'line') return lineGeomLength(shape);
  if (shape.type === 'arc') return arcGeomLength(shape);
  return 0;
}

// The seed itself.
//
// count is FIXED and width follows from it, so the boxes tile the feature
// exactly however long it is. Deriving count from a fixed spacing instead --
// one box per 0.1mm, say -- was tried and is worse than it looks: it puts 32
// boxes on a 3mm line, and overlapping windows share pixels, so they are not
// independent samples. More calipers is not more measurement.
export function seedCaliper(shape, countDefault = CALIPER_COUNT_DEFAULT) {
  const L = geomLengthOf(shape);
  const count = countDefault;
  // 0.1 is the fallback when the shape has no usable geometry yet -- a shape
  // mid-draw, or one whose points have not been set.
  const width = (L > 0 && Number.isFinite(L)) ? (L / count) : 0.1;
  return {
    count,
    width,
    min_inliers: CALIPER_SEED_MIN_INLIERS,
    max_error: CALIPER_SEED_MAX_ERROR,
  };
}

// The edge rule, extracted for the reason the caliper seed was: two callers, and
// they must not drift. Before this was shared, the SAVE path in MISC_Util carried
// its own copy -- `min_strength: 60`, and `falling` for every shape including arcs
// -- so an arc converted by SAVING measured a different edge than the same arc
// converted by the offline tool, and only the offline one had ever been measured.
// That is the wrong way round: saving under shape_based converts every primitive
// without the user touching one, so it is the path a migration actually takes.
//
// min_strength 0 is not an oversight. Contour has no gradient floor at all:
// contourGridGrayLevelRefine computes a Sobel at every contour point and then sets
// edgeRsp = 1 unconditionally, so it takes every point on the 128 crossing. "No
// floor" is 0, which is also the core's own parse default. Measured: 0 and 40 give
// bit-identical radii against a 200-to-black silhouette, because both wire edges
// are far above either threshold. The floor was never what decided anything.
// One value, one place to argue with it. It was three: 0 here, 10 in
// edgeField's derive, 60 wherever a shape was created -- so what a line
// measured depended on which code path had last touched it.
//
// 30 is a decision the measurement supports rather than forces. Measured 0 vs
// 30 across the reference corpus: 7 of 41 judge values move, every one of them
// by <=0.0027mm, and the three largest -- SME-0020-1A-H's [R] and its parallel
// pair -- move slightly TOWARD the pre-conversion value. So a floor is doing a
// little good here and no harm. (An earlier note claimed 0 and 40 were
// bit-identical; that held for one arc-radius test, not for the corpus.)
//
// The reason not to sit at 0 is not in this corpus at all. _caliperFields'
// objection is: now that the core honours a 0, 0 means no gradient floor
// whatsoever, and locks onto noise as readily as onto the part. Everything
// measured here was shot backlit at 200 against black wire, where both edges
// tower over any threshold in this range -- so this corpus cannot speak for
// other lighting, and 30 is the floor chosen for the lighting it cannot see.
// 60 was only ever a starting guess.
export const EDGE_MIN_STRENGTH = 30;

export const EDGE_SEED = {
  method: 'strongest', polarity: 'falling', nth: 0, min_strength: EDGE_MIN_STRENGTH,
};

// falling is right for a silhouette's OUTER boundary; an arc usually measures an
// INNER radius, where falling takes the wrong side of the wire. Measured across
// the reference corpus: falling put every R1.0 out by -0.11mm -- about half a wire
// thickness -- and rising brought it to -0.01mm.
export const ARC_POLARITY = 'rising';

export function seedEdge(shape) {
  return {
    ...EDGE_SEED,
    ...(shape && shape.type === 'arc' ? { polarity: ARC_POLARITY } : {}),
  };
}

// A SEARCH POINT'S edge seed, which is NOT the line/arc one.
//
// Different scanner, different defaults: `first` rather than `strongest` (a
// search point wants the first edge along its ray, not the best one anywhere on
// it), and include_range / manual_offset, which lines and arcs do not have.
// These are the values EverCheckCanvasComponent already seeds a NEWLY DRAWN
// search point with; a converted one gets the same, so a def does not depend on
// how its points came to exist.
//
// min_strength stays 60 here and is not folded into EDGE_MIN_STRENGTH. The 30
// was chosen against caliper line/arc measurements on backlit silhouettes; a
// search point scans a ray for a first crossing, which is a different question,
// and nothing has been measured to say 30 answers it. One number covering both
// would be tidier and less true.
//
// It is REQUIRED, not cosmetic: the core NAs a caliper-mode search point whose
// edge.min_strength is unset, and says which knob (FeatureManager_sig360
// _circle_line.cpp:1226). A conversion that set `locating` and not this would
// trade a silent failure for a loud one -- better, but still a failure.
export const SEARCH_POINT_EDGE_SEED = {
  method: 'first', polarity: 'falling', nth: 0,
  min_strength: 60, include_range: 0.01, manual_offset: 0,
};

// Whether an arc can be converted at all.
//
// Contour and caliper use the taught geometry for different things. Contour
// treats the def arc as a SEARCH REGION: it finds the real contour near it and
// fits that, so a sloppily taught arc still recovers the true bend. Caliper
// takes it literally, laying boxes along the taught arc and searching radially
// from the taught centre -- so a nearly-collinear set of three points puts that
// centre far away and the boxes end up nearly parallel instead of fanning
// around the feature.
//
// Measured on 93020 BCG-20X40X53 [13]: taught points nearly collinear,
// circumcentre 129px out with r=129px against a real bend of r=20px, boxes
// spanning 15.9 degrees across a corner that turns close to 90. The edges it
// finds are real -- they land on the taught points, strengths 43-103 -- but
// they trace a nearly straight run, and the circle through them is 0.30mm
// against contour's 0.21mm. The fit does not fail loudly; [13] does return
// FAILURE, but its neighbour returns SUCCESS with a radius half again too big.
//
// Sagitta is the cheap proxy: small sagitta <=> distant centre <=> parallel
// boxes, and it needs nothing but the def. 12 of the 14 arcs in the reference
// corpus measure 8.6-51px and convert to within ~0.01mm; the two that do not
// need RE-TEACHING by whoever set the recipe, not converting.
// 3px is where the reference corpus splits cleanly: 12 of 14 arcs measure
// 8.6-51px and convert to within ~0.01mm; the two that do not measure 1.2 and 1.7.
export const ARC_MIN_SAGITTA_PX = 3;

// THE CONVERSION, AS ONE FUNCTION, so that the editor and the save path agree.
//
// The save path (MISC_Util.defFileGeneration) used to be the only place this
// happened, on the OUTPUT. The editor's own shapes stayed contour until the def
// was saved and reloaded -- so right after 升級 the canvas drew some primitives
// as caliper (the ones the core's reply had attached hits to) and the rest as
// contour, and the property sheet, already switched to caliper fields, showed
// empty boxes for shapes that had no `caliper`/`edge` yet. Reported 2026-09-04.
// Migration now converts the editor's shapes THROUGH THIS, and saving converts
// through it again (idempotent), so the two cannot disagree.
//
// Returns { shape, action }:
//   'kept'             already caliper, or not a locating primitive
//   'converted'        locating set to caliper, caliper/edge seeded where absent
//   'left_contour_arc' arc too flat to convert (see ARC_MIN_SAGITTA_PX); it
//                      stays contour and the core will refuse to measure it
//                      under shape_based -- it needs RE-TEACHING by a person
// Contour-era fields (initMatchingMargin, ...) are NOT removed: setting
// `locating` back is the whole of a revert.
export function convertShapeForShapeBased(s, mmpp) {
  if (!s) return { shape: s, action: 'kept' };
  if (s.type === 'search_point') {
    if (s.locating === 'caliper' && s.edge && typeof s.edge.min_strength === 'number')
      return { shape: s, action: 'kept' };
    const c = { ...s, locating: 'caliper' };
    if (!c.edge) c.edge = { ...SEARCH_POINT_EDGE_SEED };
    // min_strength is required in caliper mode; a def carrying an edge block
    // without it is NA'd by name. Fill only that.
    else if (typeof c.edge.min_strength !== 'number')
      c.edge = { ...c.edge, min_strength: SEARCH_POINT_EDGE_SEED.min_strength };
    return { shape: c, action: 'converted' };
  }
  if (s.type !== 'line' && s.type !== 'arc') return { shape: s, action: 'kept' };
  if (s.locating === 'caliper') return { shape: s, action: 'kept' };
  if (s.type === 'arc') {
    const sag = arcSagittaPx(s, mmpp);
    if (sag !== null && sag < ARC_MIN_SAGITTA_PX) return { shape: s, action: 'left_contour_arc', sagittaPx: sag };
  }
  const c = { ...s, locating: 'caliper' };
  if (!c.caliper) c.caliper = seedCaliper(s);
  if (!c.edge) c.edge = seedEdge(s);
  return { shape: c, action: 'converted' };
}

export function arcSagittaPx(shape, mmpp) {
  if (!shape || !shape.pt1 || !shape.pt2 || !shape.pt3 || !mmpp) return null;
  const a = arcSweep(shape.pt1, shape.pt2, shape.pt3);
  if (!a || !Number.isFinite(a.r) || a.r <= 0) return null;
  const chord = Math.hypot(shape.pt3.x - shape.pt1.x, shape.pt3.y - shape.pt1.y);
  if (2 * a.r <= chord) return null;
  const sagMm = a.r - Math.sqrt(Math.max(a.r * a.r - (chord / 2) ** 2, 0));
  return sagMm / mmpp;
}
