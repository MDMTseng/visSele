import { round } from 'UTIL/MISC_Util';
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';

// Per-shape module: MEASURE.
// Part of the per-shape vertical-slice keystone (see OPENQUESTION). This file
// owns:
//   - applyDefaults:               defaults the editor expects (was a case in
//                                  InspectionEditorLogic.Shape_Attr_Fill).
//   - applyMeasureLimitCoupling:   the value/USL/LSL <-> UCL/LCL pure math used
//                                  by DefConfUI's jsonChange handler.

export const type = 'measure';

// canvasCtrl: measure refs depend on the subtype — dispatch to the subtype module.
export function availableRefShapes(shapeList, subtype) {
  const subMod = SUBTYPE_REGISTRY[subtype];
  if (subMod && subMod.availableRefShapes) return subMod.availableRefShapes(shapeList);
  return shapeList; // unknown subtype → unrestricted (matches legacy fallthrough)
}

// (no fitCameraCenter — measure doesn't pan-to-shape in the legacy code.)

// Editor property-sheet schema slice for measure — merged with the base schema
// (type/subtype/name/margin) + the per-subtype slice (info_type, calc_f, ...).
export function buildWhiteListKey(ctx) {
  const { edit_tar } = ctx;
  const common = {
    angleDeg: 'AngleRangeSetup',
    value: 'input-number',
    USL: 'ULRangeSetup', LSL: 'ULRangeSetup',
    UCL: 'ULRangeSetup', LCL: 'ULRangeSetup',
    value_b: 'input-number',
    USL_b: 'ULRangeSetup', LSL_b: 'ULRangeSetup',
    UCL_b: 'ULRangeSetup', LCL_b: 'ULRangeSetup',
    back_value_setup: 'switch',
    importance: 'input-number',
    width: 'SimpleSetup',
    quality_essential: 'switch',
    orientation_essential: 'switch',
    NGasNA: 'switch',
    NAasNG: 'switch',
    value_A: 'input-number',
    value_B: 'input-number',
    value_X: 'input-number',
    value_Y: 'input-number',
    ref_baseLine: { __OBJ__: 'btn', id: 'div', element: 'div' },
  };
  // The default `ref` is a row of 3 ref-pick buttons; calc subtype replaces it
  // with its calc_f editor (handled in calc.js below).
  const baseRef = (edit_tar.subtype === SHAPE_TYPE.measure_subtype.calc) ? undefined : {
    __OBJ__: 'div',
    ...[0, 1, 2].reduce((acc, key) => {
      acc[key + ''] = { __OBJ__: 'btn', id: 'div', element: 'div' };
      return acc;
    }, {}),
  };
  // Subtype-specific contributions (info_type for circle_info, calc_f for calc, etc.)
  const subMod = SUBTYPE_REGISTRY[edit_tar.subtype];
  const subSlice = (subMod && subMod.buildWhiteListKey) ? subMod.buildWhiteListKey(ctx) : {};
  return { ...common, ref: baseRef, ...subSlice };
}

// Per-subtype registry — keyed by SHAPE_TYPE.measure_subtype.<name>. Each
// subtype module exports draw (wired in the switch below) and may export
// buildWhiteListKey (buildWhiteListKey above dispatches via this map).
const SUBTYPE_REGISTRY = {
  [SHAPE_TYPE.measure_subtype.distance]:    distMod,
  [SHAPE_TYPE.measure_subtype.angle]:       angleMod,
  [SHAPE_TYPE.measure_subtype.radius]:      radiusMod,
  [SHAPE_TYPE.measure_subtype.circle_info]: circleInfoMod,
  [SHAPE_TYPE.measure_subtype.calc]:        calcMod,
};

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
import * as distMod from './distance';
import * as angleMod from './angle';
import * as radiusMod from './radius';
import * as circleInfoMod from './circle_info';
import * as calcMod from './calc';

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
// Per-subtype draw is now in shapes/measure/{distance,angle,radius,circle_info,calc}.js
// (SUBTYPE_REGISTRY below); distance still delegates to renderer.drawMeasureDistance
// (stateful, coupled to renderer/db_obj — intentionally left in renderUTIL).
//
// opts carries the legacy drawShapeList args + a few measure-specific ones:
//   ShapeColor          — top-level color override (capital S; passed-through)
//   measureValueCache   — caller-owned array; this draw pushes {id,obj,value}
//                         into it for downstream stats display.
import ColorMod from 'color';
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
            const sctx = { db_obj, shapeList, unitConvert, measValueAdjStr };
            switch (shape.subtype) {
              case SHAPE_TYPE.measure_subtype.distance:
                measureValue = distMod.draw(ctx, shape, subObjs, renderer, sctx);
                break;
              case SHAPE_TYPE.measure_subtype.angle:
                measureValue = angleMod.draw(ctx, shape, subObjs, renderer, sctx);
                break;
              case SHAPE_TYPE.measure_subtype.radius:
                measureValue = radiusMod.draw(ctx, shape, subObjs, renderer, sctx);
                break;
              case SHAPE_TYPE.measure_subtype.circle_info:
                measureValue = circleInfoMod.draw(ctx, shape, subObjs, renderer, sctx);
                break;
              case SHAPE_TYPE.measure_subtype.calc:
                measureValue = calcMod.draw(ctx, shape, subObjs, renderer, sctx);
                break;
            }


            


            measureValueCache.push({
              id:shape.id,
              obj:shape,
              value:measureValue
            })
}
