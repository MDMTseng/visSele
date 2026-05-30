// Per-subtype draw module for measure.radius
// Extracted verbatim from measure/index.js — body unchanged. Signature:
//   draw(ctx, shape, subObjs, renderer, sctx) -> measureValue
// where sctx = { db_obj, shapeList, unitConvert, measValueAdjStr }.
// Receives subObjs (already resolved) so we don't duplicate the lookup.
// Returns measureValue (number) or undefined; the caller pushes to measureValueCache.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import dclone from 'clone';
import * as log from 'loglevel';

// canvasCtrl: radius refs an arc.
export function availableRefShapes(shapeList) {
  return shapeList.filter((s) => s.type === 'arc');
}

export function draw(ctx, shape, subObjs, renderer, sctx) {
  const { db_obj, shapeList, unitConvert, measValueAdjStr } = sctx;
  let measureValue;
                  ctx.lineWidth = renderer.getIndicationLineSize();
                  //ctx.strokeStyle=renderer.colorSet.measure_info; 

                  ctx.font = renderer.getFontStyle(1);
                  let arc = threePointToArc(subObjs[0].pt1, subObjs[0].pt2, subObjs[0].pt3);
                  let dispVec = { x: shape.pt1.x - arc.x, y: shape.pt1.y - arc.y };
                  let mag = Math.hypot(dispVec.x, dispVec.y);
                  let dispVec_normalized = { x: dispVec.x / mag, y: dispVec.y / mag };
                  dispVec.x *= arc.r / mag;
                  dispVec.y *= arc.r / mag;//{x:dispVec.x*arc.r/mag,y:dispVec.x*arc.r/mag};

                  /*let lineInfo = {
                    x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                    x1:shape.pt1.x,y1:shape.pt1.y,
                  };*/
                  let arrowSize = 3 * renderer.getPrimitiveSize();
                  renderer.canvas_arrow(ctx, shape.pt1.x, shape.pt1.y, arc.x + dispVec.x, arc.y + dispVec.y, arrowSize);
                  //renderer.drawReportLine(ctx, lineInfo);

                  renderer.drawpoint(ctx, shape.pt1);

                  dispVec_normalized.x *= 5 * renderer.getPrimitiveSize();
                  dispVec_normalized.y *= 5 * renderer.getPrimitiveSize();



                  let fontPx = renderer.getFontHeightPx();
                  ctx.font = renderer.getFontStyle(1);




                  ctx.save();
                  ctx.translate(shape.pt1.x, shape.pt1.y);
                  
                  ctx.strokeStyle = "black";
                  if (shape.inspection_value !== undefined) {

                    let marginPC = (shape.inspection_value > shape.value) ?
                      (shape.inspection_value - shape.value) / (shape.USL - shape.value) :
                      -(shape.inspection_value - shape.value) / (shape.LSL - shape.value);
                      
                    renderer.drawInspMeasureInfoText(ctx,
                      shape.name,
                      "R" + (shape.inspection_value * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      shape.name,
                      "R" + shape.value.toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "L:" + (shape.LSL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + " U:" + (shape.USL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "Now:" + (arc.r * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  
                
            
  return measureValue;
}
