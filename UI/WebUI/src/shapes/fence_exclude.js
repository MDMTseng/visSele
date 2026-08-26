// Per-shape module: FENCE_EXCLUDE — a polygon carved OUT of the measurement
// fence: a caliper scan may not pick up an edge inside it. Saved as the def's
// measure_fence_exclude.
//
// A def with only excludes is meaningful and works: the fence starts as the
// whole image and these carve holes in it. See fence_include for why this is
// not loc_exclude.
import {
  applyDefaultsRegion, availableRefShapes as availableRefShapesRegion,
  fitCameraCenterRegion, makeDraw, makePropertySheet,
} from './_locRegionCommon.jsx';

export const type = 'fence_exclude';

export function applyDefaults(shape) { return applyDefaultsRegion(shape); }
export function availableRefShapes() { return availableRefShapesRegion(); }
export function fitCameraCenter(shape) { return fitCameraCenterRegion(shape); }

export const draw = makeDraw('#ff9100');           // amber = do not measure here
export const PropertySheet = makePropertySheet('fence_exclude');
