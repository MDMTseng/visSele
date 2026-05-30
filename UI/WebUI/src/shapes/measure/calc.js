// Per-subtype draw module for measure.calc
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
                  renderer.drawpoint(ctx, shape.pt1);


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
                      "C" + (shape.inspection_value).toFixed(renderer.fixedDigit.C),
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {

                    let totalValueList = [...subShapeValues,...measureValueCache];

                    measureValue=BPG_ExpCalc(
                      shape.calc_f.post_exp,
                      totalValueList.reduce((set,ele)=>{
                        set["["+ele.id+"]"]=ele.value;
                        return set;
                      },{}))[0];
                    
                    if(measureValue===undefined)
                      measureValue=NaN;
                    //console.log(measureValueCache,shape,measureValue);
                    renderer.drawDefMeasureInfoText(ctx,
                      shape.name,
                      "C" + shape.value.toFixed(renderer.fixedDigit.C),
                      "L:" + shape.LSL.toFixed(renderer.fixedDigit.C) + " U:" + shape.USL.toFixed(renderer.fixedDigit.C),
                      "Now:" +measureValue.toFixed(renderer.fixedDigit.C + measValueAdjStr),
                      fontPx);
                    

                    // PostfixExpCalc(shape.post_exp,,,,);
                    // text = "Now:" + ">>";//(Math.hypot(point.x-point_on_line.x,point.y-point_on_line.y)*unitConvert.mult).toFixed(3)+unitConvert.unit;
                    // renderer.draw_Text(ctx, text, fontPx, shape.pt1.x, shape.pt1.y + Y_offset);



                  }
                  ctx.restore();

  return measureValue;
}
