// Per-subtype draw module for measure.circle_info
// Extracted verbatim from measure/index.js — body unchanged. Signature:
//   draw(ctx, shape, subObjs, renderer, sctx) -> measureValue
// where sctx = { db_obj, shapeList, unitConvert, measValueAdjStr }.
// Receives subObjs (already resolved) so we don't duplicate the lookup.
// Returns measureValue (number) or undefined; the caller pushes to measureValueCache.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import dclone from 'clone';
import * as log from 'loglevel';

export function draw(ctx, shape, subObjs, renderer, sctx) {
  const { db_obj, shapeList, unitConvert, measValueAdjStr } = sctx;
  let measureValue;
                  ctx.lineWidth = renderer.getIndicationLineSize();
                  //ctx.strokeStyle=renderer.colorSet.measure_info; 

                  ctx.font = renderer.getFontStyle(1);
                  let arc = threePointToArc(subObjs[0].pt1, subObjs[0].pt2, subObjs[0].pt3);

                  /*let lineInfo = {
                    x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                    x1:shape.pt1.x,y1:shape.pt1.y,
                  };*/
                  let arrowSize = 3 * renderer.getPrimitiveSize();
                  renderer.canvas_arrow(ctx, shape.pt1.x, shape.pt1.y, arc.x, arc.y, arrowSize);
                  //renderer.drawReportLine(ctx, lineInfo);

                  renderer.drawpoint(ctx, shape.pt1);

                  let fontPx = renderer.getFontHeightPx();
                  ctx.font = renderer.getFontStyle(1);

                  ctx.save();
                  ctx.translate(shape.pt1.x, shape.pt1.y);
                  

                  let tagName="CI.";
                  switch(shape.info_type)
                  {
                    case SHAPE_TYPE._circle_info_type.max_diameter:
                      tagName+="maxD";
                      break;
                      
                    case SHAPE_TYPE._circle_info_type.min_diameter:
                      tagName+="minD";

                      break;
                      
                    case SHAPE_TYPE._circle_info_type.roughness_max:
                      tagName+="roughnessMax";

                      break;
                      
                    case SHAPE_TYPE._circle_info_type.roughness_min:
                      tagName+="roughnessMin";

                      break;
                      
                      
                    case SHAPE_TYPE._circle_info_type.roughness_rmse:
                      tagName+="roughnessRMSE";
                      break;
                    default:
                      tagName+="NA";
                      break;

                  }

                  ctx.strokeStyle = "black";
                  if (shape.inspection_value !== undefined) {

                    let marginPC = (shape.inspection_value > shape.value) ?
                      (shape.inspection_value - shape.value) / (shape.USL - shape.value) :
                      -(shape.inspection_value - shape.value) / (shape.LSL - shape.value);
                      
                    renderer.drawInspMeasureInfoText(ctx,
                      shape.name,
                      tagName + (shape.inspection_value * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      shape.name,
                      tagName + shape.value.toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "L:" + (shape.LSL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + " U:" + (shape.USL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "?" + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  
                
            
  return measureValue;
}
