// What each number in a property sheet is allowed to be.
//
// THESE MIRROR THE CORE'S OWN LIMITS, and say where each one comes from. The
// core already clamps -- CALIPER_MAX_COUNT 512, CALIPER_MAX_WIDTH 64,
// CALIPER_MAX_LENGTH 256 in Caliper.h, the min-count floors in
// caliper_effective_count -- but it clamps AFTER the def is written. Before
// this, a caliper count of 0 or -5 was accepted by the field, saved into the
// def, and then quietly run as 2. The number on screen was not the number the
// machine used, which is the same defect as a threshold defaulting to 10 with
// nothing saying so.
//
// So the bound belongs in one place, next to the reason for it, and the field
// enforces it on commit. Where the core has no limit, none is invented here:
// an arbitrary cap is a wrong answer that looks like a considered one.
//
// SIGN IS PART OF THE CONTRACT. Most of these cannot be negative -- a count, a
// width, a distance, a strength floor. Two deliberately can:
//
//   manual_offset  a bias applied along the search direction; the whole point
//                  is that it goes either way, and the apex fit suggests
//                  negative values.
//   USL/LSL/UCL/LCL/value and the value_A..Y mapping
//                  measurements and their coefficients. A spec limit below zero
//                  is a real thing to want.
import { CALIPER_MIN_COUNT_LINE, CALIPER_MIN_COUNT_ARC } from '../_caliperFields';

// Caliper.h
export const CALIPER_MAX_COUNT  = 512;
export const CALIPER_MAX_WIDTH  = 64;    // mm at def level, clamped before /= mmpp
export const CALIPER_MAX_LENGTH = 256;   // mm, same

// A width or a span of exactly 0 is not a setting, it is a primitive that
// cannot measure. The step is the smallest value the field can hold (4 dp).
const EPS = 0.0001;

export const B = {
  // ---- caliper block -----------------------------------------------------
  count_line:  { min: CALIPER_MIN_COUNT_LINE, max: CALIPER_MAX_COUNT, step: 1, int: true },
  count_arc:   { min: CALIPER_MIN_COUNT_ARC,  max: CALIPER_MAX_COUNT, step: 1, int: true },
  cal_width:   { min: EPS, max: CALIPER_MAX_WIDTH,  step: 0.01 },
  cal_length:  { min: 0,   max: CALIPER_MAX_LENGTH, step: 0.01 },  // 0 = use the def's margin
  // min_inliers 0 means "engine default" (2 for line, 3 for arc), so 0 is a
  // value and not an absence. The cap is the caliper count itself, but that is
  // a live value rather than a constant -- passed in by the sheet.
  min_inliers: { min: 0, step: 1, int: true },
  max_error:   { min: 0, step: 0.001 },    // 0 = no cap on the MAD threshold

  // ---- edge block --------------------------------------------------------
  // Raw gradient units. No upper bound in the core, and none invented: the
  // scale depends on the operator (central difference ~130, search-point Sobel
  // ~460), which is exactly why the edge-profile panel exists.
  min_strength:  { min: 0, step: 1 },
  nth:           { min: 0, step: 1, int: true },
  include_range: { min: 0, step: 0.001 },
  manual_offset: { step: 0.001 },          // SIGNED -- see the note above
  rel_strength:  { min: 0, max: 1, step: 0.05 },

  // ---- geometry ----------------------------------------------------------
  margin:      { min: EPS, step: 0.01 },   // search half-depth / region size
  width:       { min: EPS, step: 0.01 },
  angleDeg:    { step: 1 },                // wrapped by the sheet, not clamped
  importance:  { min: 0, step: 1, int: true },

  // ---- measure -----------------------------------------------------------
  limit:       { step: 0.001 },            // SIGNED: value / USL / LSL / UCL / LCL
  mapping:     { step: 0.001 },            // SIGNED: value_A / _B / _X / _Y
};
