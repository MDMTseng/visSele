// Per-shape module: LOC_EXCLUDE — a polygon "avoid generation" region carved out
// of the include mask. Saved as the def's localization_exclude.
import {
  applyDefaultsRegion, availableRefShapes as availableRefShapesRegion,
  fitCameraCenterRegion, makeDraw, makePropertySheet,
} from './_locRegionCommon.jsx';

export const type = 'loc_exclude';

export function applyDefaults(shape) { return applyDefaultsRegion(shape); }
export function availableRefShapes() { return availableRefShapesRegion(); }
export function fitCameraCenter(shape) { return fitCameraCenterRegion(shape); }

export const draw = makeDraw('#ff5252');           // red = exclude
export const PropertySheet = makePropertySheet('loc_exclude');
