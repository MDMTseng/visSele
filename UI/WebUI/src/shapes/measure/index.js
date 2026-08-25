import { round } from 'UTIL/MISC_Util';
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from '../_schemaHelpers';
import * as distMod from './distance';
import * as angleMod from './angle';
import * as radiusMod from './radius';
import * as circleInfoMod from './circle_info';
import * as calcMod from './calc';

// Per-shape module: MEASURE.
// Part of the per-shape vertical-slice keystone (see OPENQUESTION). This file
// owns:
//   - fields:                       editor schema + defaults + per-field side
//                                   effects (limit coupling, back_value toggle)
//   - applyDefaults:                derived from fields (was a case in
//                                   InspectionEditorLogic.Shape_Attr_Fill)
//   - applyMeasureLimitCoupling:    pure math kept exported for devtools
//                                   (__GP_MEASURE__) and any other consumer
//
// Per-subtype contributions (info_type, calc_f, ref-button row) come from the
// subtype modules in measure/{distance,angle,radius,circle_info,calc}.js.

export const type = 'measure';

export { MeasurePropertySheet as PropertySheet } from '../_propertySheet/MeasurePropertySheet';

// Per-subtype registry — keyed by SHAPE_TYPE.measure_subtype.<name>. Each
// subtype module exports draw + availableRefShapes; some also export their
// own buildWhiteListKey (circle_info.info_type, calc.calc_f).
const SUBTYPE_REGISTRY = {
  [SHAPE_TYPE.measure_subtype.distance]:    distMod,
  [SHAPE_TYPE.measure_subtype.angle]:       angleMod,
  [SHAPE_TYPE.measure_subtype.radius]:      radiusMod,
  [SHAPE_TYPE.measure_subtype.circle_info]: circleInfoMod,
  [SHAPE_TYPE.measure_subtype.calc]:        calcMod,
};

// canvasCtrl: measure refs depend on the subtype — dispatch to the subtype module.
export function availableRefShapes(shapeList, subtype) {
  const subMod = SUBTYPE_REGISTRY[subtype];
  if (subMod && subMod.availableRefShapes) return subMod.availableRefShapes(shapeList);
  return shapeList;
}

// (no fitCameraCenter — measure doesn't pan-to-shape in the legacy code.)

// onChange helpers for the limit-coupling fields. Each writes its dependent
// control limits after the field's new value has been written to `obj`. Pure
// math; preserve formulas exactly so byte-identical def serialization holds.
const couple_value    = (obj, prev) => { if (obj.value === undefined) return;
  obj.LCL = round(obj.LCL - prev + obj.value, 0.001);
  obj.UCL = round(obj.UCL - prev + obj.value, 0.001);
  obj.LSL = round(obj.LSL - prev + obj.value, 0.001);
  obj.USL = round(obj.USL - prev + obj.value, 0.001); };
// value_b, not value. Every line here read obj.value, so editing the back target
// re-centred the back limits on the FRONT one and they did not move at all --
// e.g. value_b 10 -> 20 left LSL_b/USL_b at 9/11. Fixed even though the feature
// is disabled: a bug left in place is one that comes back with the flag.
const couple_value_b  = (obj, prev) => { if (obj.value_b === undefined) return;
  obj.LCL_b = round(obj.LCL_b - prev + obj.value_b, 0.001);
  obj.UCL_b = round(obj.UCL_b - prev + obj.value_b, 0.001);
  obj.LSL_b = round(obj.LSL_b - prev + obj.value_b, 0.001);
  obj.USL_b = round(obj.USL_b - prev + obj.value_b, 0.001); };
const couple_LSL = (obj) => { if (obj.value === undefined) return;
  obj.LCL = round(obj.value + (obj.LSL - obj.value) * 2 / 3, 0.001); };
const couple_USL = (obj) => { if (obj.value === undefined) return;
  obj.UCL = round(obj.value + (obj.USL - obj.value) * 2 / 3, 0.001); };
const couple_LSL_b = (obj) => { if (obj.value === undefined) return;
  obj.LCL_b = round(obj.value_b + (obj.LSL_b - obj.value_b) * 2 / 3, 0.001); };
const couple_USL_b = (obj) => { if (obj.value === undefined) return;
  obj.UCL_b = round(obj.value_b + (obj.USL_b - obj.value_b) * 2 / 3, 0.001); };

// back_value_setup toggle: when turned ON, seed the _b values from the
// non-_b counterparts (UI starts as a copy). When turned OFF, remove the
// _b keys entirely so they vanish from the def + property sheet.
const toggle_back_value = (obj /*, prev */) => {
  if (obj.back_value_setup) {
    obj.value_b = obj.value;
    obj.USL_b   = obj.USL;
    obj.LSL_b   = obj.LSL;
    obj.UCL_b   = obj.UCL;
    obj.LCL_b   = obj.LCL;
  } else {
    delete obj.value_b;
    delete obj.USL_b;
    delete obj.LSL_b;
    delete obj.UCL_b;
    delete obj.LCL_b;
  }
};

// Common fields shared by every measure subtype. Declaration order =
// property-sheet display order. `default` + `normalize` reproduce the legacy
// applyDefaults type-coercion (number-or-zero / boolean-or-X). Keys without
// defaults (USL/LSL/value etc.) are populated at measure creation upstream;
// declaring them here only wires the editor schema + onChange.
const commonFields = {
  angleDeg:             { editor: 'AngleRangeSetup' },
  value:                { editor: 'input-number', onChange: couple_value },
  USL:                  { editor: 'ULRangeSetup', onChange: couple_USL },
  LSL:                  { editor: 'ULRangeSetup', onChange: couple_LSL },
  UCL:                  { editor: 'ULRangeSetup' },
  LCL:                  { editor: 'ULRangeSetup' },
  value_b:              { editor: 'input-number', onChange: couple_value_b },
  USL_b:                { editor: 'ULRangeSetup', onChange: couple_USL_b },
  LSL_b:                { editor: 'ULRangeSetup', onChange: couple_LSL_b },
  UCL_b:                { editor: 'ULRangeSetup' },
  LCL_b:                { editor: 'ULRangeSetup' },
  back_value_setup:     { editor: 'switch', onChange: toggle_back_value },
  importance:           { editor: 'input-number' },
  width:                { editor: 'SimpleSetup' },
  quality_essential:    { editor: 'switch', default: true,  normalize: (v) => typeof v === 'boolean' ? v : true },
  orientation_essential:{ editor: 'switch', default: false, normalize: (v) => typeof v === 'boolean' ? v : false },
  NGasNA:               { editor: 'switch', default: false, normalize: (v) => v === true },
  NAasNG:               { editor: 'switch', default: false, normalize: (v) => v === true },
  value_A:              { editor: 'input-number', default: 0, normalize: (v) => typeof v === 'number' ? v : 0 },
  value_B:              { editor: 'input-number', default: 1, normalize: (v) => typeof v === 'number' ? v : 1 },
  value_X:              { editor: 'input-number', default: 0, normalize: (v) => typeof v === 'number' ? v : 0 },
  value_Y:              { editor: 'input-number', default: 1, normalize: (v) => typeof v === 'number' ? v : 1 },
  ref_baseLine:         { editor: { __OBJ__: 'btn', id: 'div', element: 'div' } },
};

// Expose for jsonChange dispatch (applyFieldChange merges with subtype fields).
export const fields = commonFields;

// The default `ref` editor is a row of 3 ref-pick buttons; the calc subtype
// replaces it with its calc_f editor (handled in calc.js). Kept as a literal
// here because the spec depends on edit_tar.subtype, not on a per-key default.
function refEditorFor(subtype) {
  if (subtype === SHAPE_TYPE.measure_subtype.calc) return undefined;
  return {
    __OBJ__: 'div',
    ...[0, 1, 2].reduce((acc, key) => {
      acc[key + ''] = { __OBJ__: 'btn', id: 'div', element: 'div' };
      return acc;
    }, {}),
  };
}

export function buildWhiteListKey(ctx) {
  const { edit_tar } = ctx;
  const common = buildWhiteListKeyFromFields(commonFields, ctx);
  const subMod = SUBTYPE_REGISTRY[edit_tar.subtype];
  const subSlice = (subMod && subMod.buildWhiteListKey) ? subMod.buildWhiteListKey(ctx) : {};
  return { ...common, ref: refEditorFor(edit_tar.subtype), ...subSlice };
}

// Resolve the per-field onChange for a key on this measure shape. Used by
// DefConfUI's unified jsonChange. Looks at common fields first; subtype-
// specific fields can override (none today, but the lookup is symmetric).
export function fieldFor(edit_tar, key) {
  const subMod = SUBTYPE_REGISTRY[edit_tar.subtype];
  const subFields = (subMod && subMod.fields) || null;
  if (subFields && subFields[key]) return subFields[key];
  return commonFields[key];
}

export function applyDefaults(shape) {
  return applyDefaultsFromFields(shape, commonFields);
}

// Kept exported for devtools (__GP_MEASURE__) + any external consumer. The
// dispatch by changedKey duplicates what the per-field onChange does; both
// paths share the same couple_* primitives so they stay in sync.
export function applyMeasureLimitCoupling(obj, changedKey, preVal) {
  if (obj.value === undefined) return;
  switch (changedKey) {
    case "value":   couple_value(obj, preVal); break;
    case "value_b": couple_value_b(obj, preVal); break;
    case "LSL":     couple_LSL(obj); break;
    case "USL":     couple_USL(obj); break;
    case "LSL_b":   couple_LSL_b(obj); break;
    case "USL_b":   couple_USL_b(obj); break;
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
import { mkLog } from "UTIL/logger";
const log = mkLog("editor.shapes");
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
              renderer.drawIcon(ctx, "compass", shape.pt1.x-imgWH/2+compassOffset.x,shape.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
            }
            
            if(shape.quality_essential==false)
            {
              let theta=(180+45)*Math.PI/180;
              let compassOffset={x:offsetR*Math.cos(theta),y:offsetR*Math.sin(theta)};
              renderer.drawIcon(ctx, "eye_invisible", shape.pt1.x-imgWH/2+compassOffset.x,shape.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
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
            const sctx = { db_obj, shapeList, unitConvert, measValueAdjStr, subShapeValues, measureValueCache };
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
