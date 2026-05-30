import { round } from 'UTIL/MISC_Util';

// Per-shape module: MEASURE.
// Part of the per-shape vertical-slice keystone (see OPENQUESTION). This file
// owns:
//   - applyDefaults:               defaults the editor expects (was a case in
//                                  InspectionEditorLogic.Shape_Attr_Fill).
//   - applyMeasureLimitCoupling:   the value/USL/LSL <-> UCL/LCL pure math used
//                                  by DefConfUI's jsonChange handler.

export const type = 'measure';

export function applyDefaults(shape) {
  let out = shape;
  if (typeof out.value_A != 'number') { out = { ...out }; out.value_A = 0; }
  if (typeof out.value_B != 'number') { out = { ...out }; out.value_B = 1; }
  if (typeof out.value_X != 'number') { out = { ...out }; out.value_X = 0; }
  if (typeof out.value_Y != 'number') { out = { ...out }; out.value_Y = 1; }
  if (typeof out.quality_essential != 'boolean')     { out = { ...out }; out.quality_essential = true; }
  if (typeof out.orientation_essential != 'boolean') { out = { ...out }; out.orientation_essential = false; }
  // Note: legacy Shape_Attr_Fill assigned these without cloning first (mutating
  // the input if the booleans were missing). Preserve that for byte-identical
  // behavior with the legacy path; out has already been cloned above if any of
  // value_A/B/X/Y/quality/orientation were missing.
  if (out.NGasNA != true) out.NGasNA = false;
  if (out.NAasNG != true) out.NAasNG = false;
  return out;
}

// When a measure's value/USL/LSL (and their _b back-value variants) change in the
// property sheet, derive the dependent control limits. Pure: mutates `obj`.
// Extracted verbatim from DefConfUI's jsonChange (formulas preserved exactly,
// including the value_b branch using obj.value).
export function applyMeasureLimitCoupling(obj, changedKey, preVal) {
  if (obj.value === undefined) return;
  switch (changedKey) {
    case "value":
      obj.LCL = round(obj.LCL - preVal + obj.value, 0.001);
      obj.UCL = round(obj.UCL - preVal + obj.value, 0.001);
      obj.LSL = round(obj.LSL - preVal + obj.value, 0.001);
      obj.USL = round(obj.USL - preVal + obj.value, 0.001);
      break;
    case "value_b":
      obj.LCL_b = round(obj.LCL_b - preVal + obj.value, 0.001);
      obj.UCL_b = round(obj.UCL_b - preVal + obj.value, 0.001);
      obj.LSL_b = round(obj.LSL_b - preVal + obj.value, 0.001);
      obj.USL_b = round(obj.USL_b - preVal + obj.value, 0.001);
      break;
    case "LSL":
      obj.LCL = round((obj.value + (obj.LSL - obj.value) * 2 / 3), 0.001);
      break;
    case "USL":
      obj.UCL = round((obj.value + (obj.USL - obj.value) * 2 / 3), 0.001);
      break;
    case "LSL_b":
      obj.LCL_b = round((obj.value_b + (obj.LSL_b - obj.value_b) * 2 / 3), 0.001);
      break;
    case "USL_b":
      obj.UCL_b = round((obj.value_b + (obj.USL_b - obj.value_b) * 2 / 3), 0.001);
      break;
  }
}
