// Per-subtype draw module for measure.radius
// Extracted verbatim from measure/index.js — body unchanged. Signature:
//   draw(ctx, shape, subObjs, renderer, sctx) -> measureValue
// where sctx = { db_obj, shapeList, unitConvert, measValueAdjStr }.
// Receives subObjs (already resolved) so we don't duplicate the lookup.
// Returns measureValue (number) or undefined; the caller pushes to measureValueCache.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import dclone from 'clone';
import { mkLog } from "UTIL/logger";
import { overlayKit, OVERLAY, measureLabelName } from 'JSSRCROOT/canvas/overlayKit';
const log = mkLog("editor.shapes");

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
                  // A radius is drawn FROM THE CENTRE outward, with the head on
                  // the arc and the centre marked -- the old arrow pointed at the
                  // arc from the label point and never drew the centre, so
                  // nothing on screen said which circle the number belonged to.
                  const K = overlayKit(ctx, renderer);
                  const onArc = { x: arc.x + dispVec.x, y: arc.y + dispVec.y };
                  ctx.save();
                  ctx.setLineDash([]);
                  ctx.strokeStyle = K.C.datum; ctx.fillStyle = K.C.datum;
                  ctx.lineWidth = K.lw * K.S.thin_w;
                  K.seg({ x: arc.x - 1.8 * K.ps, y: arc.y }, { x: arc.x + 1.8 * K.ps, y: arc.y });
                  K.seg({ x: arc.x, y: arc.y - 1.8 * K.ps }, { x: arc.x, y: arc.y + 1.8 * K.ps });
                  ctx.strokeStyle = ctx.fillStyle = K.C.reading;
                  ctx.lineWidth = K.lw * K.S.line_w;
                  K.seg({ x: arc.x, y: arc.y }, onArc);
                  K.arrow(onArc, Math.atan2(dispVec.y, dispVec.x), K.S.arrow_head * K.ps);
                  // leader from the arc to wherever the operator parked the label
                  ctx.strokeStyle = K.withAlpha(K.C.reading, 0.8);
                  ctx.setLineDash(K.dash('tie'));
                  K.seg(onArc, shape.pt1);
                  ctx.restore();

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
                      measureLabelName(shape),
                      "R" + (shape.inspection_value * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      measureLabelName(shape),
                      "R" + shape.value.toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "L:" + (shape.LSL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + " U:" + (shape.USL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "Now:" + (arc.r * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  
                
            
  return measureValue;
}
