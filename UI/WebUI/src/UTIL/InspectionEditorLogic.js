import {
  distance_point_point,
  threePointToArc,
  intersectPoint,
  closestPointOnLine,
  PtRotate2d_sc,
  vecXY_addin
} from 'UTIL/MathTools';

import { isString } from 'UTIL/MISC_Util';
import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import { SHAPE_TYPE, UI_SM_STATES } from 'REDUX_STORE_SRC/actions/UIAct';
import { GetObjElement } from 'UTIL/MISC_Util';
import dclone from 'clone';
import { mkLog } from 'UTIL/logger';

import JSum from 'jsum';
import dateFormat from 'dateformat';
const log = mkLog('editor.model');

// Moved to UTIL/MeasureResultResolution.js to break a circular import
// (shapes → canvas/renderConst → InspectionEditorLogic → shapes). Re-exported
// here for backward compatibility with consumers that import the legacy path.
export { MEASURERSULTRESION } from './MeasureResultResolution';
import { MEASURERSULTRESION } from './MeasureResultResolution';


export const MEASURERSULTRESION_priority =
{
  [MEASURERSULTRESION.NA]: 0,
  [MEASURERSULTRESION.UNSET]: 0,

  [MEASURERSULTRESION.LSNG]: 1,
  [MEASURERSULTRESION.USNG]: 1,
  [MEASURERSULTRESION.SNG]: 1,
  [MEASURERSULTRESION.NG]: 1,
  [MEASURERSULTRESION.UCNG]: 2,
  [MEASURERSULTRESION.LCNG]: 2,
  [MEASURERSULTRESION.CNG]: 2,

  [MEASURERSULTRESION.UOK]: 3,
  [MEASURERSULTRESION.LOK]: 3,
  [MEASURERSULTRESION.OK]: 3,


};

export function MEASURERSULTRESION_reducer(res, measure_result_region) {
  if(measure_result_region===undefined)
    measure_result_region =MEASURERSULTRESION.NA;
  if (res == MEASURERSULTRESION.NA) return res;

  if (res == MEASURERSULTRESION.USNG || res == MEASURERSULTRESION.LSNG) {
    if (measure_result_region == MEASURERSULTRESION.NA)
      return measure_result_region;
    return res;
  }

  if (res == MEASURERSULTRESION.UCNG || res == MEASURERSULTRESION.LCNG) {
    if (measure_result_region == MEASURERSULTRESION.NA ||
      measure_result_region == MEASURERSULTRESION.USNG ||
      measure_result_region == MEASURERSULTRESION.LSNG
    )
      return measure_result_region;
    return res;
  }
  //If the res is undefined/UOK/LOK then the new result is the return value
  return measure_result_region;
}

// Shape_Attr_Fill — fills in per-shape-type defaults expected by the editor.
// Keystone step 1: delegates to the per-shape registry (src/shapes/) instead
// of a giant switch. Adding a new shape type is now a one-file change in
// src/shapes/<type>.js + a one-line entry in src/shapes/index.js.
//
// Unregistered types pass through unchanged (matches the pre-refactor default
// case). Existing per-type behavior is preserved verbatim by the modules.
import { getShapeModule } from 'JSSRCROOT/shapes';

export function Shape_Attr_Fill(shapeObject) {
  const mod = getShapeModule(shapeObject && shapeObject.type);
  if (mod && typeof mod.applyDefaults === 'function') {
    return mod.applyDefaults(shapeObject);
  }
  return shapeObject;
}

export class InspectionEditorLogic {
  constructor() {
    this.reset();
  }

  reset() {
    this.shapeCount = 0;
    this.shapeList = [];
    this.inherentShapeList = [];
    this.editShape = null;
    this.editPoint = null;

    this.state = null;


    this.sig360info = null;
    this.inspreport = null;
    this.img = null;
  }

  Setsig360info(sig360info) {
    log.info(sig360info);

    {//round signature info down to 0.000001
      
      let signatureInfoX = sig360info.reports[0].signature;
      signatureInfoX.magnitude = signatureInfoX.magnitude.map((val) => Math.round(val * 100000) / 100000);//most 3 decimal places //to 0.001mm/1um
      signatureInfoX.angle = signatureInfoX.angle.map((val) => Math.round(val * 100000) / 100000);//most 3 decimal places// 0.001*180/pi=0.057 deg

      this.sig360MaxMagnitude= signatureInfoX.magnitude.reduce((max,val) =>val>max?val:max ,0);
    }


    
    this.sig360info = sig360info;
  }

  getInitStatisticSPState()
  {
    return {
      NA_count: 0,
      CNG_count: 0,
      consecutive_CNG_count: 0,
      max_consecutive_CNG_count: 0,
      fuzzy_consecutive_CNG_count: 0,
      fuzzy_consecutive_CNG_info:0,
      max_fuzzy_consecutive_CNG_count: 0,



      SNG_count: 0,
      consecutive_SNG_count: 0,
      fuzzy_consecutive_SNG_count: 0,
      fuzzy_consecutive_SNG_info:0,
      max_consecutive_SNG_count: 0,
      max_fuzzy_consecutive_SNG_count: 0,

      
    };
  }

  resetStatisticState(edit_info)
  {

  
    //reportStatisticState.statisticValue
    let measureList =
      dclone(this.shapeList.filter((feature) =>
        feature.type == SHAPE_TYPE.measure))
        .map((feature) => {
          //console.log(feature);
          feature.statistic = {
            count_stat:
            {
              NA: 0,
              UOK: 0,
              LOK: 0,

              UCNG: 0,
              LCNG: 0,

              USNG: 0,
              LSNG: 0,
            },
            histogram: {
              xmin: 1.2 * (feature.LSL - feature.value) + feature.value,
              xmax: 1.2 * (feature.USL - feature.value) + feature.value,
              histo: new Array(502).fill(0)//The first value and last value are the value excced xmin& xmax
            },
            count: 0,
            //those value should be undefined, but since the count is 0 so the following calc should ignore those value
            sum: 0,
            sqSum: 0,//E[X^2]*count
            mean: 0,//E[X]*count
            variance: 0,//E[X^2]-E[X]^2
            //deviation = Sigma = sqrt(variance)
            sigma: 0,

            sp:this.getInitStatisticSPState(),
            //
            CP: 0,
            CK: 0,
            CPU: 0,
            CPL: 0,
            CPK: 0,
            MIN:NaN,
            MAX:NaN
          }
          return feature;
        });
    
    log.debug("[stats-reset] prior", edit_info.reportStatisticState);
    edit_info.reportStatisticState={
      ...edit_info.reportStatisticState,
      historyReport: [],
      statisticValue:{measureList},
      reportCount:0,
      emptyReportCount:0
    }

    return edit_info;
  }

  rootDefInfoLoading(root_defFile,edit_info,inspEditorLogic=this)
  {
    log.debug("[def-load]", { root_defFile, edit_info });
    if (root_defFile.type === "binary_processing_group") {
      let doExit = false;
      let clone_featureSet = dclone(root_defFile.featureSet);
      clone_featureSet.forEach((feature) => {//we ignore the key that starts with "__", two "_"
        Object.keys(feature).
          filter(key => key.startsWith("__")).
          forEach((keyW__) => delete feature[keyW__]);
      })
  
  
      let sha1_info_in_json = JSum.digest(clone_featureSet, 'sha1', 'hex');
      // Clear any prior integrity error; set it below only on an actual mismatch.
      edit_info.defIntegrityError = null;
      if (root_defFile.featureSet_sha1 !== undefined)//If there is a saved sha1, check integrity
      {
        let sha1_info_in_file = root_defFile.featureSet_sha1;
        if (sha1_info_in_file !== sha1_info_in_json) {
          log.error("[sha1-mismatch]", { inFile: sha1_info_in_file, recomputed: sha1_info_in_json });
          doExit = true;
          // Hard block: refuse the def and surface a blocking modal (a watcher
          // in script.jsx pops Modal.error on this flag). A failed integrity
          // check means the def must NOT be trusted for inspection.
          edit_info.defIntegrityError = {
            expected: sha1_info_in_file,
            actual: sha1_info_in_json,
            defName: root_defFile.name,
          };
        }
      }

      /*if(edit_info.DefFileHash==sha1_info_in_json)
      {
        //No need to wipe out the data;
        break;
      }*/
      //Edit_info_reset(newState);


      if (doExit) {
        edit_info.DefFileHash = undefined;
        return edit_info; // bugfix: callers (RepDisplayUI) assign the return; bare `return` blanked the view
      }
      //console.log(dclone(edit_info))
      edit_info.DefFileHash = sha1_info_in_json;
      edit_info.DefFileHash_pre = root_defFile.featureSet_sha1_pre;
      edit_info.DefFileHash_root = root_defFile.featureSet_sha1_root;
  
      if (root_defFile.name === undefined) {
        var now = new Date();
        var time = dateFormat(now, "yyyymmdd_HHMMss");
        edit_info.DefFileName = "Sample_" + time;
      }
      else {
        edit_info.DefFileName = root_defFile.name;
      }
  
      if (root_defFile.tag !== undefined) {
        let tagInfo = root_defFile.tag;
        if (isString(tagInfo))
          tagInfo = root_defFile.tag.split(",");
  
        if (Array.isArray(tagInfo)) {
          edit_info.DefFileTag = tagInfo;
        }
      }
  
  
      edit_info.loadedDefFile = dclone(root_defFile);
  
  
      if (typeof root_defFile.intrusionSizeLimitRatio == 'number') {
        edit_info.intrusionSizeLimitRatio =
          root_defFile.intrusionSizeLimitRatio
      }
      
      root_defFile.featureSet.forEach((report) => {
        switch (report.type) {
          case "sig360_extractor":
          case "sig360_circle_line":
            {
              if (report.matching_angle_margin_deg !== undefined)
                edit_info.matching_angle_margin_deg = report.matching_angle_margin_deg;
              if (report.matching_angle_offset_deg !== undefined)
                edit_info.matching_angle_offset_deg = report.matching_angle_offset_deg;
              if (report.matching_face !== undefined)
                edit_info.matching_face = report.matching_face;
              // Phase-2 sig360 perf opt-ins (core commits 72352281 + ee1cd247).
              // Default to legacy (v1, downsample 1) — byte-identical to pre-milestone.
              if (typeof report.matching_version === 'number')
                edit_info.matching_version = report.matching_version;
              if (typeof report.inspection_downsample === 'number')
                edit_info.inspection_downsample = report.inspection_downsample;
  
  
              edit_info = Object.assign({}, edit_info);
  
              inspEditorLogic.SetDefInfo(report);
  
  
              edit_info.edit_tar_info = null;

              edit_info.__decorator = { ...edit_info.__decorator, ...report.__decorator };

              edit_info.__decorator.list_id_order =
                UpdateListIDOrder(edit_info.__decorator.list_id_order, inspEditorLogic.shapeList);
  
              edit_info.inherentShapeList = inspEditorLogic.UpdateInherentShapeList();
  
              edit_info= this.resetStatisticState(edit_info);
              // log.info(reportStatisticState.statisticValue);
            }
            break;
          case "camera_calibration":
            // bugfix: was `log.error(action)` with `action` undefined → ReferenceError that
            // aborted loading any feature after a camera_calibration entry. Feature is ignored on load.
            /*if(report.error!==undefined &&report.error == 0)
            {
              edit_info.camera_calibration_report = root_report;
            }
            else
            {
              edit_info.camera_calibration_report = undefined;
            }*/
            break;
        }
  
      });
    }

    
    edit_info.inherentShapeList = this.UpdateInherentShapeList();

    return edit_info;
  }

  // Element-binding step of the shape create/edit flow. Given the current edit
  // target (edit_info.edit_tar_info), a key trace into it (edit_tar_ele_trace),
  // and a picked candidate (edit_tar_ele_cand), validate + write the binding,
  // seed defaults for newly-created targets, then clear the trace/candidate.
  // Mutates and returns edit_info. (Extracted verbatim from UICtrlReducer.)
  applyEditTarSubstate(edit_info, substate) {
    switch (substate) {
      case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
        {
          if (edit_info.edit_tar_info == null) {
            edit_info.edit_tar_ele_trace = null;
            edit_info.edit_tar_ele_cand = null;
            break;
          }

          if (edit_info.edit_tar_ele_trace != null && edit_info.edit_tar_ele_cand != null) {
            let keyTrace = edit_info.edit_tar_ele_trace;
            let obj = GetObjElement(edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
            let cand = edit_info.edit_tar_ele_cand;

            log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
            obj[keyTrace[keyTrace.length - 1]] = {
              id: cand.shape.id,
              type: cand.shape.type
            };

            log.info(obj, edit_info.edit_tar_info);
            edit_info.edit_tar_info = Object.assign({}, edit_info.edit_tar_info);
            edit_info.edit_tar_ele_trace = null;
            edit_info.edit_tar_ele_cand = null;
          }
          break;
        }
      case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_AUX_LINE_CREATE:
        {
          if (edit_info.edit_tar_info == null) {
            edit_info.edit_tar_info = {
              type: (substate == UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE) ?
                SHAPE_TYPE.aux_point : SHAPE_TYPE.aux_line,
              ref: [{}, {}]
            };

            edit_info.edit_tar_ele_trace = null;
            edit_info.edit_tar_ele_cand = null;
            break;
          }
          log.info(edit_info.edit_tar_ele_trace, edit_info.edit_tar_ele_cand);

          if (edit_info.edit_tar_ele_trace != null && edit_info.edit_tar_ele_cand != null) {
            let keyTrace = edit_info.edit_tar_ele_trace;
            let obj = GetObjElement(edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
            let cand = edit_info.edit_tar_ele_cand;

            log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
            obj[keyTrace[keyTrace.length - 1]] = {
              id: cand.shape.id,
              type: cand.shape.type
            };

            log.info(obj, edit_info.edit_tar_info);
            edit_info.edit_tar_info = Object.assign({}, edit_info.edit_tar_info);
            edit_info.edit_tar_ele_trace = null;
            edit_info.edit_tar_ele_cand = null;
          }
        }
        break;
      case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
        {
          if (edit_info.edit_tar_info == null) {
            edit_info.edit_tar_info = {
              type: SHAPE_TYPE.measure,
              subtype: SHAPE_TYPE.measure_subtype.NA,
              //importance:0,
              back_value_setup: false
              //ref:[{},{}]
            };
            edit_info.edit_tar_ele_trace = ["subtype"];
            edit_info.edit_tar_ele_cand = null;
            //break;
          }
          log.info(edit_info.edit_tar_ele_trace, edit_info.edit_tar_ele_cand);

          if (edit_info.edit_tar_ele_trace != null && edit_info.edit_tar_ele_cand != null) {
            let keyTrace = edit_info.edit_tar_ele_trace;
            let obj = GetObjElement(edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
            let cand = edit_info.edit_tar_ele_cand;


            if (keyTrace[0] == "ref" && cand.shape !== undefined) {
              let acceptData = true;
              let subtype = edit_info.edit_tar_info.subtype;
              switch (subtype) {
                case SHAPE_TYPE.measure_subtype.sigma: break;
                case SHAPE_TYPE.measure_subtype.distance://No specific requirement
                  if (cand.shape.type == SHAPE_TYPE.search_point ||
                    cand.shape.type == SHAPE_TYPE.aux_point ||
                    cand.shape.type == SHAPE_TYPE.arc) {
                    //We allow these three
                  }
                  else if (cand.shape.type == SHAPE_TYPE.line) {//Might need to check the angle if both are lines

                  }
                  else {
                    log.info("Error: " + subtype +
                      " doesn't accept " + cand.shape.type);
                    acceptData = false;
                  }
                  break;
                case SHAPE_TYPE.measure_subtype.radius://Has to be an arc
                case SHAPE_TYPE.measure_subtype.circle_info://Has to be an arc
                  if (cand.shape.type != SHAPE_TYPE.arc) {
                    log.info("Error: " + subtype +
                      " Only accepts arc");
                    acceptData = false;
                  }
                  break;
                case SHAPE_TYPE.measure_subtype.angle://Has to be an line to measure
                  if (cand.shape.type != SHAPE_TYPE.line&&cand.shape.type != SHAPE_TYPE.search_point) {
                    log.info("Error: " + subtype +
                      " Only accepts line & spoint");
                    acceptData = false;
                  }
                  break;

                case SHAPE_TYPE.measure_subtype.calc://Has to be an line to measure
                  if (cand.shape.type != SHAPE_TYPE.measure) {
                    log.info("Error: " + subtype +
                      " Only accepts measure");
                    acceptData = false;
                  }
                  break;
                default:
                  log.info("Error: " + subtype + " is not in the measure_subtype list");
                  acceptData = false;
              }
              if (acceptData) {
                log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
                obj[keyTrace[keyTrace.length - 1]] = {
                  id: cand.shape.id,
                  type: cand.shape.type
                };
              }
            }
            else if (keyTrace[0] == "subtype") {
              let acceptData = true;
              switch (cand) {
                case SHAPE_TYPE.measure_subtype.sigma:
                case SHAPE_TYPE.measure_subtype.radius:
                  edit_info.edit_tar_info.ref = [{}];
                  break;

                case SHAPE_TYPE.measure_subtype.circle_info:
                  edit_info.edit_tar_info.ref = [{}];
                  edit_info.edit_tar_info.info_type="NA";
                  break;

                case SHAPE_TYPE.measure_subtype.calc:
                  edit_info.edit_tar_info.ref = [];
                  edit_info.edit_tar_info.calc_f = {
                    exp: "0",
                    post_exp: ["0"]
                  };
                  break;

                case SHAPE_TYPE.measure_subtype.distance:
                  edit_info.edit_tar_info.ref = [{}, {}];
                  edit_info.edit_tar_info.ref_baseLine = {};
                  break;
                case SHAPE_TYPE.measure_subtype.angle:
                  edit_info.edit_tar_info.ref = [{}, {}];
                  break;
                default:
                  log.info("Error: " + cand + " is not in the measure_subtype list");
                  acceptData = false;
              }
              edit_info.edit_tar_info =
                Object.assign(edit_info.edit_tar_info,
                  {
                    pt1: { x: 0, y: 0 },
                    value: 0,
                    USL: 0,
                    LSL: 0,
                    UCL: 0,
                    LCL: 0,
                  });
              if (acceptData)
                obj[keyTrace[keyTrace.length - 1]] = cand;
            }
            else if (keyTrace[0] == "ref_baseLine") {
              obj[keyTrace[keyTrace.length - 1]] = {
                id: cand.shape.id,
                type: cand.shape.type
              };
            }

            log.info(obj, edit_info.edit_tar_info);
            edit_info.edit_tar_info = Object.assign({}, edit_info.edit_tar_info);
            edit_info.edit_tar_ele_trace = null;
            edit_info.edit_tar_ele_cand = null;
          }
        }
        break;
      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        if (edit_info.edit_tar_ele_trace != null && edit_info.edit_tar_ele_cand != null) {
          let keyTrace = edit_info.edit_tar_ele_trace;
          let obj = GetObjElement(edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
          let cand = edit_info.edit_tar_ele_cand;

          log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
          obj[keyTrace[keyTrace.length - 1]] = {
            id: cand.shape.id,
            type: cand.shape.type
          };

          edit_info.edit_tar_info = Object.assign({}, edit_info.edit_tar_info);
          edit_info.edit_tar_ele_trace = null;
          edit_info.edit_tar_ele_cand = null;
        }
        break;
    }
    return edit_info;
  }

  getMeasure_detailStatus(measureReport,control_Margin_table=this.shapeList)
  {
    let measureDef = control_Margin_table.find((feature) => feature.id == measureReport.id);
    //console.log(measure, measureDef);
    if (measureDef === undefined || measureReport.status === INSPECTION_STATUS.NA || measureReport.value!=measureReport.value) {
      return MEASURERSULTRESION.NA;
    }    
    else if (measureReport.status === INSPECTION_STATUS.UNSET) {
      return MEASURERSULTRESION.UNSET;
    }
    else if (measureReport.value < measureDef.LSL) {
      return MEASURERSULTRESION.LSNG;
    }
    else if (measureReport.value > measureDef.USL) {
      return MEASURERSULTRESION.USNG;
    }
    else if (measureReport.value < measureDef.LCL) {
      return MEASURERSULTRESION.LCNG;
    }
    else if (measureReport.value > measureDef.UCL) {
      return MEASURERSULTRESION.UCNG;
    }
    else if (measureReport.value < measureDef.value) {
      return MEASURERSULTRESION.LOK;
    }
    else {
      return MEASURERSULTRESION.UOK;
    }


    // else if (measureReport.status === INSPECTION_STATUS.UNSET) {
    //   return MEASURERSULTRESION.UNSET;
    // }
    // else if (measureReport.value < measureDef.LSL) {
    //   return MEASURERSULTRESION.LSNG;
    // }
    // else if (measureReport.value < measureDef.LCL) {
    //   return MEASURERSULTRESION.LCNG;
    // }
    // else if (measureReport.value < measureDef.value) {
    //   return MEASURERSULTRESION.LOK;
    // }
    // else if (measureReport.value < measureDef.UCL) {
    //   return MEASURERSULTRESION.UOK;
    // }
    // else if (measureReport.value < measureDef.USL) {
    //   return MEASURERSULTRESION.UCNG;
    // }
    // else {
    //   return MEASURERSULTRESION.USNG;
    // }

  }

  getsig360info_mmpp() {
    try {
      //console.log(this.sig360info);
      return this.sig360info.reports[0].mmpp;

    } catch (e) {
    }

    return 1;
  }

  setsig360infoCenter(center){
    
    this.sig360info.reports[0].cx=center.x;
    this.sig360info.reports[0].cy=center.y;
  }
  getsig360infoCenter() {

    let center = { x: 0, y: 0 };
    try {
      center.x = this.sig360info.reports[0].cx;
      center.y = this.sig360info.reports[0].cy;

    } catch (e) {
      center.x = 0;//(this.secCanvas.width / 2)
      center.y = 0;//(this.secCanvas.height / 2)
    }

    return center;
  }

  SetShapeList(shapeList) {
    this.shapeList = shapeList;
    let maxId = 0;
    this.shapeList.forEach((shape) => {
      if (maxId < shape.id) {
        maxId = shape.id;
      }
    });
    this.shapeCount = maxId;
  }

  SetDefInfo(defInfo) {
    this.SetShapeList(defInfo.features);

    //this.inherentShapeList = defInfo.featureSet[0].inherentShapeList;
    log.info(defInfo);
    let sig360info = defInfo.inherentfeatures[0];

    this.Setsig360info(
      {
        reports: [
          {
            cx: sig360info.pt1.x,
            cy: sig360info.pt1.y,
            area: sig360info.area,
            orientation: sig360info.orientation,
            signature: sig360info.signature,
            mmpp: defInfo.mmpp,
            cam_param: defInfo.cam_param,
          }
        ]
      }
    );
    this.UpdateInherentShapeList();

    let lostRefObjs = this.findLostRefShapes();

    this.shapeList = this.shapeList.filter((shape) => !lostRefObjs.includes(shape));
    this.UpdateInherentShapeList();
  }

  SetCameraParamInfo(cameraParam) {
    this.cameraParam = cameraParam;
  }

  SetState(state) {
    if (this.state != state) {
      this.shapeCount = 0;
      this.editShape = null;
      this.editPoint = null;
    }
  }

  FindShape(key, val, shapeList = this.shapeList) {
    let idx = shapeList.findIndex((shape) => shape[key] == val);
    return (idx < 0) ? undefined : idx;
  }

  FindShapeIdx(id, shapeList = this.shapeList) {
    return this.FindShape("id", id, shapeList);
  }

  FindShapeObject(key, val, shapeList = this.shapeList, inherentShapeList = this.inherentShapeList) {
    let idx = this.FindShape(key, val, shapeList);
    if (idx !== undefined) return shapeList[idx];
    idx = this.FindShape(key, val, inherentShapeList);
    if (idx !== undefined) return inherentShapeList[idx];
    return undefined;
  }


  UpdateInherentShapeList() {
    this.inherentShapeList = [];
    if (this.sig360info === null || this.sig360info === undefined) return;
    let setupTarget = this.sig360info.reports[0];

    log.debug(setupTarget);
    let id = 100000;
    let signature_id = id;
    this.inherentShapeList.push({
      id: signature_id,
      type: SHAPE_TYPE.sign360,
      name: "@__SIGNATURE__",
      pt1: { x: setupTarget.cx, y: setupTarget.cy },//The location on the image
      pt2: { x: 0, y: 0 },//The ref location that we use as graphic center

      area: setupTarget.area,
      orientation: 0,
      signature: setupTarget.signature
    });
    id = signature_id + 1;
    this.inherentShapeList.push({
      id: id++,
      type: SHAPE_TYPE.aux_point,
      name: "@__SIGNATURE__.centre",
      ref: [{
        id: signature_id,
        keyTrace: ["pt2"]
      }]
    });
    this.inherentShapeList.push({
      id: id++,
      type: SHAPE_TYPE.aux_line,
      name: "@__SIGNATURE__.orientation",
      ref: [{
        name: "@__SIGNATURE__",
        keyTrace: ["orientation"]
      }]
      //ref:"__OBJ_CENTRAL__"
    });
    id = 100100;
    this.shapeList.forEach((shape) => {
      if (shape.type == SHAPE_TYPE.arc) {
        this.inherentShapeList.push({

          id: id + shape.id * 10,
          type: SHAPE_TYPE.aux_point,
          name: shape.name + ".centre",
          ref: [{
            //name:shape.name,
            id: shape.id,
            element: "centre"
          }]
        });
      }
    });

    return this.inherentShapeList;
  }

  GenerateFeature_sig360_circle_line() {
    return {
      "type": "sig360_circle_line",
      "ver": "0.0.1.0",
      "unit": "px",
      "mmpp": this.sig360info.reports[0].mmpp,
      cam_param: this.sig360info.reports[0].cam_param,
      features: this.shapeList,
      inherentfeatures: this.inherentShapeList
    };

  }

  findLostRefShapes(shapeList = this.shapeList, inherentShapeList = this.inherentShapeList) {
    let totalList = shapeList.concat(inherentShapeList);
    let lostRefShape = totalList.filter(shape => {
      if (shape.ref === undefined && shape.ref_baseLine === undefined)
        return false;
      let totalRef = shape.ref;
      if (GetObjElement(shape, ["ref_baseLine", "id"]) !== undefined) {
        totalRef = [...totalRef, shape.ref_baseLine];
      }
      let lostRef = totalRef.reduce((lostRef, ref) => {
        if (lostRef) return lostRef;
        return totalList.find((shape) => ref.id == shape.id) == undefined;
      }, false);
      if (lostRef) return true;


      return false;
    });
    return lostRefShape;
  }

  FindShapeRefTree(id, shapeList = this.shapeList, inherentShapeList = this.inherentShapeList) {
    let totalList = shapeList.concat(inherentShapeList);
    let ref_layer = totalList.filter(shape => {
      if (shape.ref === undefined && shape.ref_baseLine === undefined)
        return false;
      let hasRef = shape.ref.find(ref => ref.id == id) !== undefined;
      if (hasRef) return true;

      if (GetObjElement(shape, ["ref_baseLine", "id"]) == id)
        return true;

      return false;
    }).map(ref_shape => {
      let ref_tree = this.FindShapeRefTree(ref_shape.id, shapeList, inherentShapeList);
      if (ref_tree.length == 0)
        return { id: ref_shape.id, shape: ref_shape };
      return { id: ref_shape.id, shape: ref_shape, ref_tree };
    });

    return ref_layer;
  }
  FlatRefTree(refTree) {
    let idList = [];
    refTree.forEach(refShapeInfo => {
      idList.push(refShapeInfo);
      if (refShapeInfo.ref_tree !== undefined)
        idList = idList.concat(this.FlatRefTree(refShapeInfo.ref_tree));
    });
    return idList;
  }
  SetShape(shape_obj, id)//undefined means add new shape
  {
    let pre_shape = null;
    let pre_shape_idx = undefined;

    if (shape_obj == null)//For delete
    {
      if (id !== undefined) {
        pre_shape_idx = this.FindShapeIdx(id);
        log.debug("SETShape>", pre_shape_idx);
        if (pre_shape_idx != undefined) {
          let refTree = this.FindShapeRefTree(id);
          log.debug("[refTree]", refTree);
          let flatRefTree = this.FlatRefTree(refTree);
          log.debug("[flatRefTree]", flatRefTree);
          this.shapeList = this.shapeList
            .filter((shape) => flatRefTree.find(fRef => shape.id == fRef.id) === undefined)
            .filter((shape) => id != shape.id);

        }
      }
      //UpdateInherentShapeList();
      return pre_shape;
    }

    log.info("SETShape>", this.shapeList, shape_obj, id);


    let ishapeIdx = this.FindShapeIdx(shape_obj.id, this.inherentShapeList);
    //If the id is in the inherentShapeList Exit, no change is allowed
    if (ishapeIdx != undefined) {
      log.error("Error:Shape id:" + id + " name:" + shape_obj.name + " is in inherentShapeList which is not changeable.");
      return null;
    }

    if (id != undefined)//If the id is assigned, which might exist in the shapelist
    {
      let tmpIdx = this.FindShapeIdx(id);
      let nameIdx = this.FindShape("name", shape_obj.name);

      //Check if the name in shape_obj exits in the list and duplicates with other shape in list(tmpIdx!=nameIdx)
      if (nameIdx !== undefined && tmpIdx != nameIdx) {
        log.error("Error:Shape id:" + id + " Duplicated shape name:" + shape_obj.name + " with idx:" + nameIdx + " ");
        return null;
      }
      log.info("SETShape>", tmpIdx);
      if (tmpIdx != undefined) {
        pre_shape = this.shapeList[tmpIdx];
        pre_shape_idx = tmpIdx;
      }
      else {
        log.error("Error:Shape id:" + id + " doesn't exist in the list....");
        return null;
      }
    }
    else {//If the id is undefined, find an available id then append shapelist with this object
      this.shapeCount++;
      id = this.shapeCount;
    }

    //log.info("FoundShape>",pre_shape);
    let shape = null;
    shape = { ...shape_obj, id };
    if (pre_shape == null) {
      if (shape.name === undefined) {
        shape.name = "["+id+"]";
      }
      // immutable replace (new array ref) so consumers selecting shapeList re-render
      this.shapeList = [...this.shapeList, shape];
    }
    else {
      if (pre_shape_idx != undefined) {
        this.shapeList = this.shapeList.map((s, i) => (i === pre_shape_idx ? shape : s));
      }
    }

    if (this.editShape !== null && this.editShape.id == id) {
      this.editShape = shape;
    }
    //UpdateInherentShapeList();
    return shape;

  }



  FindInspShapeObject(id, inspReport) {
    if (inspReport == undefined) return undefined;
    {
      let inspIdx = this.FindShapeIdx(id, inspReport.detectedCircles);
      if (inspIdx != undefined) {
        return inspReport.detectedCircles[inspIdx];
      }
    }



    {
      let inspIdx = this.FindShapeIdx(id, inspReport.detectedLines);
      if (inspIdx != undefined) {
        return inspReport.detectedLines[inspIdx];
      }
    }


    {
      let inspIdx = this.FindShapeIdx(id, inspReport.auxPoints);
      if (inspIdx != undefined) {
        return inspReport.auxPoints[inspIdx];
      }
    }

    {
      let inspIdx = this.FindShapeIdx(id, inspReport.searchPoints);
      if (inspIdx != undefined) {
        return inspReport.searchPoints[inspIdx];
      }
    }

    {
      let inspIdx = this.FindShapeIdx(id, inspReport.judgeReports);
      if (inspIdx != undefined) {
        return inspReport.judgeReports[inspIdx];
      }
    }

    return undefined;
  }

  
  ShapeAdjustsWithInspectionResult(shape,shapeList ,InspResult, oriBase = false){
    let cos_v = Math.cos(-InspResult.rotate);
    let sin_v = Math.sin(-InspResult.rotate);
    let flip_f = (InspResult.isFlipped) ? -1 : 1;

    let eObject=shape;
    if (eObject == null) return;

    // console.log(shape.inspection_status,INSPECTION_STATUS.NA,shape,shapeList ,InspResult);
    let inspAdjObj = this.FindInspShapeObject(eObject.id, InspResult);
    if (InspResult != undefined && inspAdjObj == undefined) {
      return;
    }
    eObject.inspection_status = inspAdjObj.status;

    if(eObject.inspection_status==INSPECTION_STATUS.NA )
    {
      // Fit failed. Replace any stale per-frame derived fields so they don't
      // carry over from a prior SUCCESS run (_pt1/_pt2/adj_pt1 = legacy fit
      // endpoints; would draw a misleading "success" line otherwise). Pass
      // cal_hits through unchanged — each hit's `st` (0=missed, 1=outlier,
      // 2=inlier from the attempted fit) drives the per-caliper visual:
      // missed → gray box no X, outlier → red X, inlier → green X.
      delete eObject._pt1; delete eObject._pt2;
      delete eObject.adj_pt1;
      if (inspAdjObj && inspAdjObj.cal_hits) {
        eObject.cal_hits = inspAdjObj.cal_hits;
      } else {
        delete eObject.cal_hits;
      }
      return;
    }
    function pointForwardTrans(_pt)
    {
      let pt={x:_pt.x,y:_pt.y};
      pt = PtRotate2d_sc(pt, sin_v, cos_v, flip_f);
      pt.x += InspResult.cx;
      pt.y += InspResult.cy;
      return pt;
    }

    function pointInvTrans(_pt)
    {
      let pt={x:_pt.x,y:_pt.y};
      pt.x -= InspResult.cx;
      pt.y -= InspResult.cy;
      if (flip_f < 0) {
        pt = PtRotate2d_sc(pt, sin_v, cos_v, flip_f);
      }
      else {
        pt = PtRotate2d_sc(pt, -sin_v, cos_v, 1);
      }
      return pt;
    }
    ["pt1", "pt2", "pt3"].forEach((key) => {
      if (eObject[key] === undefined) return;
      eObject[key] = pointForwardTrans(eObject[key]);
    });

    switch (eObject.type) {
      case SHAPE_TYPE.line:
        {
          ["pt1", "pt2"].forEach((key) => {
            eObject["_"+key] = closestPointOnLine(inspAdjObj, eObject[key]);
          });

          ["pt1", "pt2"].forEach((key) => {
            eObject[key] = inspAdjObj[key];
          });
          // Caliper-mode per-caliper hits — core emits them in OBJECT-FRAME
          // mm (def coord system). Passed through unchanged so they always
          // sit between the def's own pt1/pt2 regardless of which frame the
          // canvas is rendering in. When the whole fit FAILED (status != SUCCESS),
          // force every hit to status=0 so the WebUI grays all boxes.
          // Pass through unchanged — per-hit st (0/1/2) drives the visual.
          if (inspAdjObj.cal_hits) {
            eObject.cal_hits = inspAdjObj.cal_hits;
          }
          // console.log(dclone(eObject));
          // if (InspResult.isFlipped) {
          //   let tmp = eObject.pt1;
          //   eObject.pt1 = eObject.pt2;
          //   eObject.pt2 = tmp;
          // }

        }
        break;


      case SHAPE_TYPE.arc:
        {
          ["pt1", "pt2", "pt3"].forEach((key) => {
            if(inspAdjObj[key]!==undefined)//if report has the data use it
              eObject[key]= inspAdjObj[key];
            else
            {//or calculate it from define data
              eObject[key].x -= inspAdjObj.x;
              eObject[key].y -= inspAdjObj.y;
              let mag = Math.hypot(eObject[key].x, eObject[key].y);
              eObject[key].x = eObject[key].x * inspAdjObj.r / mag + inspAdjObj.x;
              eObject[key].y = eObject[key].y * inspAdjObj.r / mag + inspAdjObj.y;
            }
          });
          // Caliper-mode per-caliper hits — see line case for the rationale.
          // Pass through unchanged — per-hit st (0/1/2) drives the visual.
          if (inspAdjObj.cal_hits) {
            eObject.cal_hits = inspAdjObj.cal_hits;
          }
        }
        break;

      case SHAPE_TYPE.search_point:
        {
          let vec = this.shapeVectorParse(eObject, shapeList);
          let o_pt1={
            x:inspAdjObj.x,
            y:inspAdjObj.y
          };
          let line ={
            cx:inspAdjObj.x,
            cy:inspAdjObj.y,
            vx:vec.x,
            vy:vec.y,
          }

          if(eObject.locating_anchor==true)
          {
            eObject.adj_pt1={
              x:inspAdjObj.x,
              y:inspAdjObj.y,
            }
          }
          else
          {
            eObject.adj_pt1 = closestPointOnLine(line, eObject.pt1);
          }

          if (oriBase)//rotate back to original orientation
          {
            eObject.adj_pt1= pointInvTrans(eObject.adj_pt1);
          }
          eObject.pt1=o_pt1;
          // Per-hit caliper points (caliper-mode search_point only). Passed
          // through unchanged — drawn as dots in search_point's draw.
          if (inspAdjObj.cal_hits) {
            eObject.cal_hits = inspAdjObj.cal_hits;
          }
          // {
          //   let vec = this.shapeVectorParse(eObject, shapeList);
          //   let line ={
          //     cx:inspAdjObj.x,
          //     cy:inspAdjObj.y,
          //     vx:vec.x,
          //     vy:vec.y,
          //   }
          //   // console.log({...eObject.pt1});
          //   eObject.adj_pt1 = closestPointOnLine(line, eObject.pt1);
          //   // console.log({...eObject.adj_pt1},{...inspAdjObj});
          //   eObject.pt1.x=inspAdjObj.x;
          //   eObject.pt1.y=inspAdjObj.y;
          // }
        }
        break;

        


      case SHAPE_TYPE.measure:
        {
          eObject.inspection_value = inspAdjObj.value;
          //console.log(eObject);
        }
        break;
    }
    if (oriBase)//rotate back to original orientation
    {
      ["pt1", "pt2", "pt3"].forEach((key) => {
        if (eObject[key] === undefined) return;
        
        eObject[key] = pointInvTrans(eObject[key]);
      });
    }
  }

  ShapeListAdjustsWithInspectionResult(shapeList, InspResult, oriBase = false) {
    shapeList.forEach((eObject) => {
      this.ShapeAdjustsWithInspectionResult(eObject,shapeList, InspResult, oriBase)
    });

    
  }


  FindClosestCtrlPointInfo(location, shapeList = this.shapeList) {
    let pt_info = {
      pt: null,
      key: null,
      shape: null,
      dist: Number.POSITIVE_INFINITY
    };

    shapeList.forEach((shape) => {
      let tmpDist;

      switch (shape.type) {
        case SHAPE_TYPE.line:
        case SHAPE_TYPE.arc:
        case SHAPE_TYPE.search_point:
        case SHAPE_TYPE.measure:
          ["pt1", "pt2", "pt3"].forEach((key) => {
            if (shape[key] === undefined) return;
            tmpDist = distance_point_point(shape[key], location);
            if (pt_info.dist > tmpDist) {
              pt_info.shape = shape;
              pt_info.key = key;
              pt_info.pt = shape[key];
              pt_info.dist = tmpDist;
            }
          });
          break;

        case SHAPE_TYPE.aux_point:
          {
            let point = this.auxPointParse(shape);
            tmpDist = distance_point_point(point, location);
            if (pt_info.dist > tmpDist) {
              pt_info.shape = shape;
              pt_info.key = undefined;
              pt_info.pt = point;
              pt_info.dist = tmpDist;
            }
          }
          break;

      }
    });
    return pt_info;
  }

  FindClosestInherentPointInfo(location, inherentShapeList) {
    let pt_info = {
      pt: null,
      key: null,
      shape: null,
      dist: Number.POSITIVE_INFINITY
    };
    inherentShapeList.forEach((ishape) => {
      if (ishape == null) return;
      if (ishape.type != SHAPE_TYPE.aux_point) return;
      let point = this.auxPointParse(ishape);
      let tmpDist = distance_point_point(point, location);
      if (pt_info.dist > tmpDist) {
        pt_info.shape = ishape;
        pt_info.key = null;
        pt_info.pt = point;
        pt_info.dist = tmpDist;
      }

    });

    return pt_info;
  }

  auxPointParse(aux_point, shapelist = this.shapeList) {
    let point = undefined;
    if (aux_point.type != SHAPE_TYPE.aux_point) return point;

    if (aux_point.ref.length == 1) {
      let ref0_shape = this.FindShapeObject("id", aux_point.ref[0].id, shapelist);
      if (ref0_shape === undefined) {
        return undefined;
      }

      if (aux_point.ref[0].keyTrace !== undefined) {
        point = GetObjElement(ref0_shape, aux_point.ref[0].keyTrace);
        //point.ref = dclone(aux_point.ref);//Deep copy
        //point.ref[0]._obj=ref0_shape;
      }
      else {
        switch (ref0_shape.type) {
          case SHAPE_TYPE.arc:
            {
              let shape_arc = ref0_shape;
              let arc = threePointToArc(shape_arc.pt1, shape_arc.pt2, shape_arc.pt3);
              point = arc;
              point.ref = dclone(aux_point.ref);//Deep copy
              point.ref[0]._obj = shape_arc;
            }
            break;
        }
      }
    }
    else if (aux_point.ref.length == 2) {

      let ref0_shape = this.FindShapeObject("id", aux_point.ref[0].id, shapelist);
      if (ref0_shape === undefined) return undefined;
      let ref1_shape = this.FindShapeObject("id", aux_point.ref[1].id, shapelist);
      if (ref1_shape === undefined) return undefined;


      let v0 = this.shapeVectorParse(ref0_shape, shapelist);
      let v1 = this.shapeVectorParse(ref1_shape, shapelist);
      if (v0 === undefined || v1 === undefined) {
        return undefined;
      }

      let p0 = this.shapeMiddlePointParse(ref0_shape, shapelist);
      let p1 = this.shapeMiddlePointParse(ref1_shape, shapelist);

      if (p0 === undefined || p1 === undefined) {
        return undefined;
      }
      vecXY_addin(v0, p0);//Let vx become another point on line
      vecXY_addin(v1, p1);

      let retPt = intersectPoint(p0, v0, p1, v1);
      return retPt;

    }

    return point;
  }
  searchPointParse(search_point, shapelist = this.shapeList) {
    let point = undefined;
    if (search_point.type != SHAPE_TYPE.search_point) return undefined;

    if (search_point.ref.length == 1) {
      let ref0_shape = this.FindShapeObject("id", search_point.ref[0].id);
      if (ref0_shape === undefined) return undefined;
      switch (ref0_shape.type) {
        case SHAPE_TYPE.line:
          {
            point = search_point.pt1;
          }
          break;
      }
    }

    return point;
  }



  shapeMiddlePointParse(shape, shapelist = this.shapeList) {
    switch (shape.type) {

      case SHAPE_TYPE.line:
        return { x: (shape.pt1.x + shape.pt2.x) / 2, y: (shape.pt1.y + shape.pt2.y) / 2 };
      case SHAPE_TYPE.arc:
        return threePointToArc(shape.pt1, shape.pt2, shape.pt3);
      case SHAPE_TYPE.aux_point:
        return this.auxPointParse(shape, shapelist);
      case SHAPE_TYPE.search_point:
        return this.searchPointParse(shape, shapelist);
    }
    return undefined;
  }

  shapeVectorParse(shape, shapelist = this.shapeList) {
    switch (shape.type) {

      case SHAPE_TYPE.line:
        return { x: (shape.pt2.x - shape.pt1.x), y: (shape.pt2.y - shape.pt1.y) };
      case SHAPE_TYPE.search_point:
        {
          if (shape.ref === undefined || shape.ref.length != 1) return undefined;

          let refObj = this.FindShapeObject("id", shape.ref[0].id, shapelist);

          if (refObj === undefined || refObj.type !== SHAPE_TYPE.line) return undefined;
          let lineVec = this.shapeVectorParse(refObj, shapelist);

          if (lineVec === undefined) return undefined;
          let angle = Math.atan2(lineVec.y, lineVec.x) + shape.angleDeg * Math.PI / 180;
          return { x: Math.cos(angle), y: Math.sin(angle) };
        }
    }
    return undefined;
  }

}




export function UpdateListIDOrder(cur_listIDOrder, list) {
  //remove disappeared shape id
  let listIDOrder = cur_listIDOrder.filter(id => list.find(shape => shape.id == id));

  let newIDs = list.//find new IDs to add in
    filter(shape => listIDOrder.find(id => id == shape.id) === undefined).
    map(shape => shape.id);

  listIDOrder = [...listIDOrder, ...newIDs];
  return listIDOrder;
}



const default_MinRepeatInspReport = 2;

export function Edit_info_Empty() {
  return {
    stage_light_report: undefined,
    inspReport: undefined,

    reportStatisticState: {
      trackingWindow: [],
      historyReport: [],
      newAddedReport: [],
      reportCount:0,
      emptyReportCount:0,
      hideTrackingWindowObj:false,
      statisticValue: undefined,
      overallStat: {
        OK: 0,
        WARN: 0,
        NG: 0,
        lastTS: 0,
        T: 0,
        soft_T: 0,
        softIdx: 0.1
      }
    },
    statSetting: {
      keepInTrackingTime_ms: 3000,
      historyReportlimit: 2000,
      minReportRepeat: default_MinRepeatInspReport,
      headReportSkip: 3
    },
    sig360info: [],
    matching_angle_margin_deg: 180,
    matching_angle_offset_deg: 0,
    matching_face: 0,
    // sig360 perf opt-ins; 1/1 = byte-identical to pre-milestone.
    matching_version: 1,
    inspection_downsample: 1,
    intrusionSizeLimitRatio: 0.1,
    img: null,
    DefFileName: "",
    DefFileTag: [],
    inspOptionalTag:[],
    DefFileHash: "",
    __decorator: {
      list_id_order: [],
      extra_info: [],
      control_margin_info:{}
    },
    inherentShapeList: [],

    edit_tar_info: null,//It's for usual edit target

    //It's the target element in edit target
    //Example 
    //edit_tar_info={iii:0,a:{b:[x,y,z,c]}}
    //And our goal is to trace to c
    //Then, edit_tar_ele_trace={obj:b, keyHist:["a","b",3]}
    edit_tar_ele_trace: null,

    //This is the cadidate info for target element content
    edit_tar_ele_cand: null,
    //camera_calibration_report:undefined // the camera calibration data shouldn't be reset
    loadedDefFile: undefined,

    DefFileHash: undefined,
    DefFileHash_pre: undefined,
    DefFileHash_root: undefined,
  };
}