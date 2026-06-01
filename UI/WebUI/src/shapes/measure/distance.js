// Per-subtype draw module for measure.distance
// Extracted verbatim from measure/index.js — body unchanged. Signature:
//   draw(ctx, shape, subObjs, renderer, sctx) -> measureValue
// where sctx = { db_obj, shapeList, unitConvert, measValueAdjStr }.
// Receives subObjs (already resolved) so we don't duplicate the lookup.
// Returns measureValue (number) or undefined; the caller pushes to measureValueCache.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import dclone from 'clone';
import { mkLog } from "UTIL/logger";
const log = mkLog("editor.shapes");

// canvasCtrl: distance can ref anything except other measures.
export function availableRefShapes(shapeList) {
  return shapeList.filter((s) => s.type !== 'measure');
}

export function draw(ctx, shape, subObjs, renderer, sctx) {
  const { db_obj, shapeList, unitConvert, measValueAdjStr } = sctx;
  let measureValue;
                  measureValue = renderer.drawMeasureDistance(ctx, shape, subObjs, shapeList, unitConvert,measValueAdjStr);

  return measureValue;
}
