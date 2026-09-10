// Per-subtype draw module for measure.circle_info
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

// Editor schema slice for the circle_info subtype: adds the info_type dropdown.
export function buildWhiteListKey(ctx) {
  return {
    info_type: {
      __OBJ__: ctx.renderMethods.Dropdown_List,
      list: Object.keys(SHAPE_TYPE._circle_info_type),
    },
  };
}

// canvasCtrl: circle_info refs an arc (same as radius).
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

                  /*let lineInfo = {
                    x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                    x1:shape.pt1.x,y1:shape.pt1.y,
                  };*/
                  // Every info_type used to draw the SAME picture -- one arrow
                  // to the centre -- so "max diameter" and "roughness RMSE"
                  // were distinguishable only by three characters of tag text.
                  // Now the drawing shows the quantity.
                  const K = overlayKit(ctx, renderer);
                  const IT = SHAPE_TYPE._circle_info_type;
                  const isRough = (shape.info_type === IT.roughness_max
                                || shape.info_type === IT.roughness_min
                                || shape.info_type === IT.roughness_rmse);
                  const c = { x: arc.x, y: arc.y };
                  const val = (shape.inspection_value !== undefined) ? shape.inspection_value : undefined;
                  ctx.save();
                  // The nominal circle, as a reference: dash-dot, datum colour.
                  ctx.setLineDash(K.dash('datum'));
                  ctx.strokeStyle = K.C.datum; ctx.lineWidth = K.lw * K.S.thin_w;
                  ctx.beginPath(); ctx.arc(c.x, c.y, arc.r, arc.thetaS, arc.thetaE); ctx.stroke();
                  ctx.setLineDash([]);
                  K.seg({ x: c.x - 1.8 * K.ps, y: c.y }, { x: c.x + 1.8 * K.ps, y: c.y });
                  K.seg({ x: c.x, y: c.y - 1.8 * K.ps }, { x: c.x, y: c.y + 1.8 * K.ps });

                  if (!isRough) {
                    // A diameter: the chord through the centre, arrowheads on
                    // the circle at both ends. Its direction is the one the
                    // operator parked the label in -- the report does not say
                    // where the extreme actually lay, so the drawing shows the
                    // quantity's SHAPE, and the number stays authoritative.
                    const ang = Math.atan2(shape.pt1.y - c.y, shape.pt1.x - c.x);
                    const rr = (val !== undefined && val > 0) ? val / 2 : arc.r;
                    const p1 = K.at(c, ang, rr), p2 = K.at(c, ang + Math.PI, rr);
                    ctx.strokeStyle = ctx.fillStyle = K.C.reading;
                    ctx.lineWidth = K.lw * K.S.line_w;
                    K.seg(p1, p2);
                    K.arrow(p1, ang, K.S.arrow_head * K.ps);
                    K.arrow(p2, ang + Math.PI, K.S.arrow_head * K.ps);
                    ctx.strokeStyle = K.withAlpha(K.C.reading, 0.8);
                    ctx.setLineDash(K.dash('tie'));
                    K.seg(p1, shape.pt1);
                  } else {
                    // Roughness is microns on a millimetre circle: at 1:1 it is
                    // invisible, so the deviation band is drawn at a gain and
                    // the gain is printed with the value.
                    const v = (val !== undefined ? val : shape.value) || 0;
                    const gain = (v > 0) ? Math.max(1, Math.min(2000, (2.5 * K.ps) / v)) : 1;
                    const d = Math.max(v * gain, 0.6 * K.ps);
                    ctx.strokeStyle = K.C.reading; ctx.lineWidth = K.lw * K.S.thin_w;
                    ctx.setLineDash([]);
                    for (const rr of [arc.r - d, arc.r + d]) {
                      if (rr <= 0) continue;
                      ctx.beginPath(); ctx.arc(c.x, c.y, rr, arc.thetaS, arc.thetaE); ctx.stroke();
                    }
                    const mid = (arc.thetaS + arc.thetaE) / 2;
                    ctx.strokeStyle = ctx.fillStyle = K.C.reading;
                    ctx.lineWidth = K.lw * K.S.line_w;
                    K.seg(K.at(c, mid, arc.r - d), K.at(c, mid, arc.r + d));
                    K.chip('x' + Math.round(gain), K.at(c, mid, arc.r + d + K.S.chip_gap * K.ps).x,
                           K.at(c, mid, arc.r + d + K.S.chip_gap * K.ps).y, K.C.reading, OVERLAY.font.small);
                    ctx.strokeStyle = K.withAlpha(K.C.reading, 0.8);
                    ctx.setLineDash(K.dash('tie'));
                    K.seg(K.at(c, mid, arc.r), shape.pt1);
                  }
                  ctx.restore();

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
                      measureLabelName(shape),
                      tagName + (shape.inspection_value * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      measureLabelName(shape),
                      tagName + shape.value.toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "L:" + (shape.LSL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit + " U:" + (shape.USL * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit,
                      "?" + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  
                
            
  return measureValue;
}
