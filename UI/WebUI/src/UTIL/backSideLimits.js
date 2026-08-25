// Back-side (flipped-part) measurement limits — the switch, and what it governs.
//
// Its own module so that both the editor model (InspectionEditorLogic) and the
// wire-def generator (MISC_Util) can read it: those two already import each
// other's neighbours, and putting the flag in either one closes an import cycle.
// Same reason MeasureResultResolution.js exists.
//
// DISABLED 2026-08-25. An audit found the feature broken on both sides of the
// screen at once, and every failure is SILENT -- the number reads as configured
// while the machine does something else:
//
//   - couple_value_b re-centred the back limits on the FRONT target
//     (shapes/measure/index.js), so editing value_b moved nothing.
//   - the per-製程 override whitelist carries no _b keys and the margin editor
//     has no back columns (DefConfUI), so a 製程 that tightened USL left USL_b
//     at the root value -- and a flipped part is judged on USL_b. The override
//     was a no-op for exactly the parts this bench runs.
//   - the UI's aux geometry omits the flip term the core applies
//     (shapeVectorParse vs FeatureManager_sig360_circle_line.cpp), so on a
//     mirrored part the drawn point and the measured point are different points.
//
// While it is off, a flipped part is judged against the SAME limits as a front
// part -- which is what an operator who never opened the back-side panel already
// believes is happening.
//
// Nothing is destroyed: the def FILE keeps its _b values. They are stripped from
// the generated WIRE def only, so the core stops applying them too, and they
// come back whole when this flag does.
export const BACK_SIDE_LIMITS_ENABLED = false;

// The five keys the feature owns, in one place, so the flag and the strip cannot
// drift apart.
export const BACK_SIDE_KEYS = ['value_b', 'USL_b', 'LSL_b', 'UCL_b', 'LCL_b'];

// Remove those keys from a generated def, in place, at any depth.
//
// The core reads USL_b/LSL_b straight out of the def and judges a flipped part
// against them, so turning the feature off in the UI alone would make the screen
// and the sorter disagree -- the exact failure the flag exists to end.
export function stripBackSideLimits(node) {
  if (Array.isArray(node)) { node.forEach(stripBackSideLimits); return; }
  if (!node || typeof node !== 'object') return;
  BACK_SIDE_KEYS.forEach((k) => { if (k in node) delete node[k]; });
  Object.keys(node).forEach((k) => stripBackSideLimits(node[k]));
}
