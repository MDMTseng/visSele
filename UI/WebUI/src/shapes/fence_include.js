// Per-shape module: FENCE_INCLUDE — a polygon inside which a caliper scan is
// allowed to pick up an edge. Saved as the def's measure_fence_include.
//
// NOT loc_include. That one says where line2Dup extracts features in order to
// FIND the part; this one says where a measurement may look once the part has
// been found. They are drawn the same way and mean different things, which is
// why they are different types and different colours.
import {
  applyDefaultsRegion, availableRefShapes as availableRefShapesRegion,
  fitCameraCenterRegion, makeDraw, makePropertySheet,
} from './_locRegionCommon.jsx';

export const type = 'fence_include';

export function applyDefaults(shape) { return applyDefaultsRegion(shape); }
export function availableRefShapes() { return availableRefShapesRegion(); }
export function fitCameraCenter(shape) { return fitCameraCenterRegion(shape); }

export const draw = makeDraw('#2979ff');           // blue = measure here
export const PropertySheet = makePropertySheet('fence_include');
