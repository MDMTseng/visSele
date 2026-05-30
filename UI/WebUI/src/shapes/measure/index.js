import { round } from 'UTIL/MISC_Util';

// Per-shape module: MEASURE.
// Part of the per-shape vertical-slice keystone (see OPENQUESTION). This file
// owns:
//   - applyDefaults:               defaults the editor expects (was a case in
//                                  InspectionEditorLogic.Shape_Attr_Fill).
//   - applyMeasureLimitCoupling:   the value/USL/LSL <-> UCL/LCL pure math used
//                                  by DefConfUI's jsonChange handler.

export const type = 'measure';

export function applyDefaults(shape) {
  let out = shape;
  if (typeof out.value_A != 'number') { out = { ...out }; out.value_A = 0; }
  if (typeof out.value_B != 'number') { out = { ...out }; out.value_B = 1; }
  if (typeof out.value_X != 'number') { out = { ...out }; out.value_X = 0; }
  if (typeof out.value_Y != 'number') { out = { ...out }; out.value_Y = 1; }
  if (typeof out.quality_essential != 'boolean')     { out = { ...out }; out.quality_essential = true; }
  if (typeof out.orientation_essential != 'boolean') { out = { ...out }; out.orientation_essential = false; }
  // Note: legacy Shape_Attr_Fill assigned these without cloning first (mutating
  // the input if the booleans were missing). Preserve that for byte-identical
  // behavior with the legacy path; out has already been cloned above if any of
  // value_A/B/X/Y/quality/orientation were missing.
  if (out.NGasNA != true) out.NGasNA = false;
  if (out.NAasNG != true) out.NAasNG = false;
  return out;
}

// When a measure's value/USL/LSL (and their _b back-value variants) change in the
// property sheet, derive the dependent control limits. Pure: mutates `obj`.
// Extracted verbatim from DefConfUI's jsonChange (formulas preserved exactly,
// including the value_b branch using obj.value).
export function applyMeasureLimitCoupling(obj, changedKey, preVal) {
  if (obj.value === undefined) return;
  switch (changedKey) {
    case "value":
      obj.LCL = round(obj.LCL - preVal + obj.value, 0.001);
      obj.UCL = round(obj.UCL - preVal + obj.value, 0.001);
      obj.LSL = round(obj.LSL - preVal + obj.value, 0.001);
      obj.USL = round(obj.USL - preVal + obj.value, 0.001);
      break;
    case "value_b":
      obj.LCL_b = round(obj.LCL_b - preVal + obj.value, 0.001);
      obj.UCL_b = round(obj.UCL_b - preVal + obj.value, 0.001);
      obj.LSL_b = round(obj.LSL_b - preVal + obj.value, 0.001);
      obj.USL_b = round(obj.USL_b - preVal + obj.value, 0.001);
      break;
    case "LSL":
      obj.LCL = round((obj.value + (obj.LSL - obj.value) * 2 / 3), 0.001);
      break;
    case "USL":
      obj.UCL = round((obj.value + (obj.USL - obj.value) * 2 / 3), 0.001);
      break;
    case "LSL_b":
      obj.LCL_b = round((obj.value_b + (obj.LSL_b - obj.value_b) * 2 / 3), 0.001);
      break;
    case "USL_b":
      obj.UCL_b = round((obj.value_b + (obj.USL_b - obj.value_b) * 2 / 3), 0.001);
      break;
  }
}

// ───── DRAW (keystone step 3d) ─────────────────────────────────────────────
// Extracted verbatim from renderUTIL.case SHAPE_TYPE.measure. The case body is
// large (~510 lines) but mostly orchestration — the subtype-specific drawing
// still delegates to renderer.drawMeasure{Distance,Angle,Radius,...} helpers in
// renderUTIL. A deeper per-subtype split (shapes/measure/draw_<subtype>.js) is
// a follow-up; this commit's win is dropping the giant inline block out of
// renderUTIL so the dispatcher case becomes uniform with all other shapes.
//
// opts carries the legacy drawShapeList args + a few measure-specific ones:
//   ShapeColor          — top-level color override (capital S; passed-through)
//   measureValueCache   — caller-owned array; this draw pushes {id,obj,value}
//                         into it for downstream stats display.
import ColorMod from 'color';
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { GetObjElement } from 'UTIL/MISC_Util';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
import { BPG_ExpCalc } from 'UTIL/BPG_Protocol';
import dclone from 'clone';
import * as log from 'loglevel';
import { MEASURE_RESULT_VISUAL_INFO, SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';

export function draw(ctx, shape, renderer, {
  inFullDisplay = true, shapeList = [], ShapeColor = undefined,
  next_ShapeColor = null, skip_id_list = [],
  unitConvert = { unit: 'mm', mult: 1 }, drawSubObjs = false,
  measureValueCache = [],
} = {}) {
  let shapeColor = ColorMod(SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default).alpha(0.8);
            let db_obj = renderer.db_obj;
            if (shape.ref === undefined) return;
            let subObjs = shape.ref
              .map((ref) => db_obj.FindShapeObject("id", ref.id, shapeList));
            let subShapeValues;
            if (drawSubObjs)
            {
              subShapeValues = renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
            }
            if(subShapeValues===undefined)
            {
              subShapeValues=[];
            }

            
            let imgWH=renderer.getPointSize()*4;
            let offsetR=renderer.getPointSize()*5;

            if(shape.orientation_essential)
            {
              let theta=180*Math.PI/180;
              let compassOffset={x:offsetR*Math.cos(theta),y:offsetR*Math.sin(theta)};
              ctx.drawImage(renderer.iconSet["compass"], shape.pt1.x-imgWH/2+compassOffset.x,shape.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
            }
            
            if(shape.quality_essential==false)
            {
              let theta=(180+45)*Math.PI/180;
              let compassOffset={x:offsetR*Math.cos(theta),y:offsetR*Math.sin(theta)};
              ctx.drawImage(renderer.iconSet["eye_invisible"], shape.pt1.x-imgWH/2+compassOffset.x,shape.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
              // renderer.draw_aimcross(ctx, shape.pt1, renderer.getPointSize()*3,0.3);
            }

            let subObjs_valid = subObjs.reduce((acc, cur) => acc && (cur !== undefined), true);
            if (!subObjs_valid) return;

            if (ShapeColor == undefined || ShapeColor == null) {
              if (shape.color !== undefined) {
                ctx.strokeStyle = shape.color;
                ctx.fillStyle = shape.color;
              }
              else {
                ctx.strokeStyle = shapeColor;
                ctx.fillStyle = shapeColor;
              }
            }
            else {
              ctx.strokeStyle = ShapeColor;
              ctx.fillStyle = ShapeColor;

            }
            ctx.lineWidth = renderer.getIndicationLineSize();
            let measureValue;
            if (shape.inspection_value !== undefined) {
              if(typeof shape.inspection_value === 'number'){
                //good
              }
              else
              {
                shape.inspection_value=Number.NaN;
              }
            }
            let measValueAdjStr = "";
            
            if(shape.value_A!==undefined && shape.value_B!==undefined && shape.value_X!==undefined && shape.value_Y!==undefined)
            {

              measValueAdjStr+=" "+shape.value_A+"~"+shape.value_B+" => "+shape.value_X+"~"+shape.value_Y;
            }

            // console.log(shape);
            switch (shape.subtype) {
              case SHAPE_TYPE.measure_subtype.distance:
                {
                  measureValue = renderer.drawMeasureDistance(ctx, shape, subObjs, shapeList, unitConvert,measValueAdjStr);

                }
                break;
              case SHAPE_TYPE.measure_subtype.angle:
                {
                  let obj0_pt2=subObjs[0].pt2;

                  if(obj0_pt2===undefined)
                  {
                    
                    obj0_pt2= db_obj.shapeVectorParse(subObjs[0], shapeList);
                    obj0_pt2.x+=subObjs[0].pt1.x;
                    obj0_pt2.y+=subObjs[0].pt1.y;
                  }

                  let obj1_pt2=subObjs[1].pt2;
                  
                  if(obj1_pt2===undefined)
                  {
                    
                    obj1_pt2 = db_obj.shapeVectorParse(subObjs[1], shapeList);
                    obj1_pt2.x+=subObjs[1].pt1.x;
                    obj1_pt2.y+=subObjs[1].pt1.y;
                  }
                  //console.log(shape,subObjs,obj0_pt2,obj1_pt2);

                  
                  let srcPt =
                    intersectPoint(subObjs[0].pt1, obj0_pt2, subObjs[1].pt1, obj1_pt2);

                  ctx.lineWidth = renderer.getIndicationLineSize();
                  //ctx.strokeStyle=renderer.colorSet.measure_info; 

                  ///ctx.fillStyle=renderer.colorSet.measure_info; 
                  //renderer.drawpoint(ctx, srcPt,"cross");

                  let sAngle = Math.atan2(subObjs[0].pt1.y - srcPt.y, subObjs[0].pt1.x - srcPt.x);
                  let eAngle = Math.atan2(subObjs[1].pt1.y - srcPt.y, subObjs[1].pt1.x - srcPt.x);
                  //eAngle+=Math.PI;

                  let angleDiff = (eAngle - sAngle) % (2 * Math.PI);
                  if (angleDiff < 0) {
                    angleDiff += Math.PI * 2;
                  }
                  if (angleDiff > Math.PI) {
                    angleDiff -= Math.PI;
                  }


                  let quadrant = 0;

                  //if(shape.quadrant===undefined)
                  {

                    let midwayAngle = Math.atan2(shape.pt1.y - srcPt.y, shape.pt1.x - srcPt.x);//-PI~PI

                    let angleDiff_midway = (midwayAngle - sAngle) % (2 * Math.PI);
                    if (angleDiff_midway < 0) {
                      angleDiff_midway += Math.PI * 2;
                    }

                    if (angleDiff_midway < angleDiff) {
                      quadrant = 1;
                    }
                    else if (angleDiff_midway < Math.PI) {
                      quadrant = 2;
                    }
                    else if (angleDiff_midway < (Math.PI + angleDiff)) {
                      quadrant = 3;
                    }
                    else {
                      quadrant = 4;
                    }


                  }

                  {
                    shape.quadrant = quadrant;
                  }

                  let dist = Math.hypot(shape.pt1.y - srcPt.y, shape.pt1.x - srcPt.x);
                  let margin_deg = shape.margin * Math.PI / 180;
                  let draw_sAngle = sAngle, draw_eAngle = eAngle;
                  let ext_Angle1 = sAngle, ext_Angle2 = eAngle;
                  switch (quadrant % 4) {
                    case 1:
                      {

                      }
                      break;

                    case 2:
                      {
                        draw_sAngle += angleDiff;
                      }
                      break;
                    case 3:
                      {
                        draw_sAngle += Math.PI;

                      }
                      break;
                    case 0:
                      {
                        draw_sAngle = draw_sAngle + angleDiff + Math.PI;

                      }
                      break;
                  }
                  //log.debug(angleDiff*180/Math.PI,sAngle*180/Math.PI,eAngle*180/Math.PI);
                  if (quadrant % 2 == 0)//if our target quadrant is 2 or 4..., find the complement angle 
                  {
                    angleDiff = Math.PI - angleDiff;
                  }

                  draw_eAngle = draw_sAngle + angleDiff;

                  if (quadrant % 2 == 0) {
                    ext_Angle1 = draw_eAngle;
                    ext_Angle2 = draw_sAngle;
                  }
                  else {
                    ext_Angle1 = draw_sAngle;
                    ext_Angle2 = draw_eAngle;
                  }



                  renderer.drawArcArrow(ctx, srcPt.x, srcPt.y, dist, draw_sAngle, draw_eAngle);

                  renderer.drawpoint(ctx, shape.pt1);

                  let measureDeg = angleDiff * 180 / Math.PI;


                  {

                    ctx.lineWidth = renderer.getIndicationLineSize();
                    ctx.setLineDash([renderer.getPrimitiveSize(), 1*renderer.getPrimitiveSize()])

                    let arcPt={x:srcPt.x + dist * Math.cos(ext_Angle1),y:srcPt.y + dist * Math.sin(ext_Angle1)};
                    let closestPt=closestPointOnPoints(arcPt,[subObjs[0].pt1,obj0_pt2]);
                    renderer.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    arcPt={x:srcPt.x + dist * Math.cos(ext_Angle2),y:srcPt.y + dist * Math.sin(ext_Angle2)};
                    closestPt=closestPointOnPoints(arcPt,[subObjs[1].pt1,obj1_pt2]);
                    renderer.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    ctx.setLineDash([]);
                  }

                  let x = shape.pt1.x + (shape.pt1.x - srcPt.x) / dist * 4 * renderer.getPrimitiveSize();
                  let y = shape.pt1.y + (shape.pt1.y - srcPt.y) / dist * 4 * renderer.getPrimitiveSize();



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
                      (shape.inspection_value).toFixed(renderer.fixedDigit.A) + "º",
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      shape.name,
                      ""+shape.value.toFixed(renderer.fixedDigit.A) + "º",
                      "L:" + shape.LSL.toFixed(renderer.fixedDigit.A) + "º U:" + shape.USL.toFixed(renderer.fixedDigit.A) + "º",
                      "Now:" + (measureDeg).toFixed(renderer.fixedDigit.A) + "º" + measValueAdjStr,
                      fontPx)
                    
                    measureValue=measureDeg;
                  }
                  ctx.restore();
                }
                break;

              case SHAPE_TYPE.measure_subtype.radius:
                {
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

                  break;
                  
                
            
                }


                
              case SHAPE_TYPE.measure_subtype.circle_info:
              
                {
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

                  break;
                  
                
            
                }

              break;

              case SHAPE_TYPE.measure_subtype.calc:
                {
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

                  break;
                }
            }


            


            measureValueCache.push({
              id:shape.id,
              obj:shape,
              value:measureValue
            })
}
