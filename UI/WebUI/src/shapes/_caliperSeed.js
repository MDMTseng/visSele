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
export function arcSagittaPx(shape, mmpp) {
  if (!shape || !shape.pt1 || !shape.pt2 || !shape.pt3 || !mmpp) return null;
  const a = arcSweep(shape.pt1, shape.pt2, shape.pt3);
  if (!a || !Number.isFinite(a.r) || a.r <= 0) return null;
  const chord = Math.hypot(shape.pt3.x - shape.pt1.x, shape.pt3.y - shape.pt1.y);
  if (2 * a.r <= chord) return null;
  const sagMm = a.r - Math.sqrt(Math.max(a.r * a.r - (chord / 2) ** 2, 0));
  return sagMm / mmpp;
}
