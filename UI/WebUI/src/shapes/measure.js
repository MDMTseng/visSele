import { round } from 'UTIL/MISC_Util';

// Per-shape (measure) logic — first piece of the per-shape schema (shapes/).
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
