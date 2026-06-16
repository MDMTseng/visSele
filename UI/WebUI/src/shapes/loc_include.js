// Per-shape module: LOC_INCLUDE — a polygon region where the shape-based
// localizer extracts line2Dup features. Saved as the def's localization_include.
import {
  applyDefaultsRegion, availableRefShapes as availableRefShapesRegion,
  fitCameraCenterRegion, makeDraw, makePropertySheet,
} from './_locRegionCommon.jsx';

export const type = 'loc_include';

export function applyDefaults(shape) { return applyDefaultsRegion(shape); }
export function availableRefShapes() { return availableRefShapesRegion(); }
export function fitCameraCenter(shape) { return fitCameraCenterRegion(shape); }

export const draw = makeDraw('#00c853');           // green = include
export const PropertySheet = makePropertySheet('loc_include');
