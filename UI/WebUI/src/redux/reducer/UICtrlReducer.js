
import { UI_SM_STATES, UI_SM_EVENT, SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { xstate_GetCurrentMainState, GetObjElement, isString, shapeDefFingerprint } from 'UTIL/MISC_Util';
import { InspectionEditorLogic,UpdateListIDOrder,Edit_info_Empty,DEF_SCOPED_EDIT_INFO_KEYS,DEF_LOCALIZER_SCOPED_KEYS,MEASURERSULTRESION,effectiveLimits } from 'UTIL/InspectionEditorLogic';
import { pickCtrlMargin } from 'UTIL/ctrlMarginPick';
import { convertShapeForShapeBased } from '../../shapes/_caliperSeed';

import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import APP_INFO from 'JSSRCROOT/info.js';
import { mkLog } from 'UTIL/logger';
import dclone from 'clone';
import JSum from 'jsum'

import {GetDefaultSystemSetting} from 'JSSRCROOT/info.js';
import dateFormat from 'dateformat';
import semver from 'semver'
import EC_zh_TW from 'LANG/zh_TW';
import { TrademarkCircleOutlined } from '@ant-design/icons';

import { statReducer } from './spcStats';
const log = mkLog('editor.reducer');

let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;


function Edit_info_reset(newState) {
  newState.edit_info = {...newState.edit_info, ...Edit_info_Empty()};
  newState.edit_info._obj.reset();
}


function Default_UICtrlReducer() {
  //ST = d;
  //log.info("ST...",JSON.stringify(ST));
  let defState = {
    machine_custom_setting:{},
    System_Setting:GetDefaultSystemSetting(),
    showSM_graph: false,
    defConf_lock_level: 0,
    edit_info: Edit_info_Empty(),
    WebUI_info: APP_INFO,
    sm: null,
    c_state: null,
    p_state: null,
    state_count: 0,
    FILE_default_camera_setting:{},
    DICT:EC_zh_TW
  }
  defState.edit_info.defModelPath=undefined;
  defState.edit_info._obj=new InspectionEditorLogic();

  return defState;
}

function StateReducer(newState, action) {
  newState.state_count++;
  if (action.type == "ev_state_update") {
    newState.c_state = action.data.c_state;
    newState.p_state = action.data.p_state;
    newState.sm = action.data.sm;
    newState.state_count = 0;
    log.info(newState.p_state.value, " + ", action.data.action, " > ", newState.c_state.value);

    switch (newState.c_state.value)//enter state action
    {

      case UISTS.MAIN:
        {
          newState.edit_info = { ...newState.edit_info, inspOptionalTag: [] };
        }
        break
    }
  }

  switch (action.type) {

    case UISEV.Def_Model_Path_Update:
      newState.edit_info = { ...newState.edit_info, defModelPath: action.data };
      //Edit_info_reset(newState);
      break;


    case "System_Setting_Update":
      log.debug("[System_Setting_Update]", action.data);
      newState=
      {
        ...newState,
        System_Setting:action.data
      };
    break;

    case "FILE_default_camera_setting":

      newState=
      {
        ...newState,
        FILE_default_camera_setting:action.data
      };
      break;

    // The instrument scale, from data/lens_calib.json -- ONE source for the
    // whole UI.
    //
    // Every consumer used to compute mmpp as
    // camera_calibration_report.reports[0].mmpb2b / .ppb2b. That report is
    // dead: the core stopped emitting it (FeatureReport_UTIL's
    // camera_calibration case is commented out), so the field is permanently
    // undefined and each of the five call sites either crashed or bailed --
    // InspectionUI even carries a defensive "no camera_calibration_report,
    // skipping" branch for exactly that.
    //
    // lens_calib.json is what actually describes this instrument, it is what
    // the core pushes into the sampler (push_mmpp_to_sampler), and CalibrationUI
    // already calls it "the authority". So the UI reads the same file, and mmpp
    // has one definition on both sides instead of two that could disagree.
    case "FILE_lens_calib":
      {
        const lc = action.data;
        const um = lc && Number(lc.um_per_px);
        // um_per_px is microns; mmpp is mm. Undefined (not 0, not 1) when the
        // file is missing or malformed -- callers must be able to tell "no
        // calibration" from "a scale of 1", which is how a bad number gets
        // applied as if it were real.
        const mmpp = (Number.isFinite(um) && um > 0) ? um / 1000 : undefined;
        log.info("[lens_calib] instrument mmpp =", mmpp);
        newState = { ...newState, FILE_lens_calib: lc, instrument_mmpp: mmpp };
      }
      break;

    case UISEV.Control_SM_Panel:
      newState.showSM_graph = action.data;
      return newState;

  }
  let stateObj = xstate_GetCurrentMainState(newState.c_state);
  let substate = stateObj.substate;




  function EVENT_Inspection_Report(newState, action,ignoreInspData=false) {
    let repType = GetObjElement(action, ["data", "type"]);
    if (repType === undefined) return;
    switch (repType) {
      case "binary_processing_group":
        {
          let statSetting = newState.edit_info.statSetting;
          let inspOptionalTag = "" + newState.edit_info.inspOptionalTag;
          if (newState.DefFileTag !== undefined && newState.DefFileTag.length != 0) {
            inspOptionalTag = newState.edit_info.DefFileTag + "," + inspOptionalTag;
          }
          if (newState.MachTag !== undefined && newState.MachTag.length != 0) {
            inspOptionalTag = newState.MachTag + "," + inspOptionalTag;
          }
          let currentDate = action.date;
          let currentTime_ms = currentDate.getTime();

          let camParam = newState.edit_info._obj.cameraParam;
          let sig360MaxMagnitude = newState.edit_info._obj.sig360MaxMagnitude;
          // let mmpcampix = newState..cameraParam.mmpb2b/this.db_obj.cameraParam.ppb2b;

          let mmpcampix;
          if (camParam === undefined) {
            mmpcampix = undefined;
          }
          else {
            mmpcampix = camParam.mmpb2b / camParam.ppb2b;
          }

          // THE LOCATOR'S OWN COMMENT, kept so the inspection screen can show it.
          //
          // The core emits `locate` only when it has something to say, and the
          // case that matters is not "no object" -- it is "SBM features not
          // trained (sig360 fallback in use)", which arrives on runs that
          // LOCATE FINE. A def whose shape cache no longer matches falls back,
          // sig360 picks the part up, every measurement passes, and nothing on
          // screen distinguishes it from the def running as designed. That is
          // the one state worth interrupting somebody about, and until now it
          // existed only in a field nobody read.
          {
            const env = GetObjElement(action, ["data", "reports", 0]) || {};
            const note = env.locate;
            const dropped = env.region_dropped || 0;
            newState.edit_info.locateNote =
              (note && note.reason) ? { ...note, dropped, at: currentTime_ms }
              : (dropped > 0 ? { reason: null, dropped, at: currentTime_ms } : undefined);
          }

          let subFeatureDefSha1 = action.data.subFeatureDefSha1;
          let machine_hash = action.data.machine_hash;
          // if(typeof subFeatureDefSha1 == "string")
          // {
          //   if(subFeatureDefSha1.length>8)
          //   {
          //     subFeatureDefSha1 =  subFeatureDefSha1.substring(0,8);
          //   }
          // }

          action.data.reports.forEach((report) => {

            switch (report.type) {
              case "sig360_extractor":
                newState.edit_info = Object.assign({}, newState.edit_info);

                Edit_info_reset(newState);
                newState.edit_info._obj.Setsig360info(action.data);
                newState.edit_info.sig360info = newState.edit_info._obj.sig360info;
                // Legacy "camera_calibration" WS report is no longer emitted
                // (loadCameraCalibParam was removed). Pull cam_param from the
                // extraction report so rendering mmpp stays correct.
                if (report.cam_param) {
                  newState.edit_info._obj.SetCameraParamInfo(report.cam_param);
                }
              break;
              case "sig360_circle_line":
                {
                  newState.edit_info = { ...newState.edit_info };
                  //newState.report=action.data;
                  let inspReport = report;

                  if (report.cam_param) {
                    newState.edit_info._obj.SetCameraParamInfo(report.cam_param);
                    camParam = report.cam_param;
                    mmpcampix = camParam.mmpb2b / camParam.ppb2b;
                  }

                  newState.edit_info.inspReport = inspReport;
                  inspReport.time_ms = currentTime_ms;
                  // How long the core took on this frame. It rides on the
                  // TOP-LEVEL report (action.data) because it describes the
                  // whole inspection, not one feature set -- but the canvas
                  // reads reports[0], so carry it across the same way
                  // subFeatureDefSha1/machine_hash are pulled out above.
                  // Snapshot each shape so the def-conf cal_hits overlay can
                  // tell whether the user has edited the def since this
                  // inspection ran (stale → don't show hits).
                  //
                  // This was a WHITELIST of seven keys and had already drifted:
                  // width, angleDeg, search_far, ref and the arc's
                  // direction/fit_mode all change the search band and none were
                  // listed, so rotating a search point 90 degrees after a run
                  // left the old hits on screen pinned to the NEW box, reading
                  // as fresh confirmation.
                  //
                  // shapeDefFingerprint inverts it: strip the per-frame RESULTS
                  // and keep everything else, which is the same rule the def
                  // itself is built with. A field added tomorrow is covered the
                  // day it is added, instead of being silently unwatched until
                  // somebody notices the hits are lying.
                  const _shapeList = newState.edit_info._obj.shapeList || [];
                  inspReport.shape_fingerprints = {};
                  for (const s of _shapeList) inspReport.shape_fingerprints[s.id] = shapeDefFingerprint(s);

                  if (mmpcampix === undefined) {
                    break;
                  }

                  //let overallStat = reportStatisticState.overallStat;


                  

                  let reportStatisticState = newState.edit_info.reportStatisticState;
                  // console.log(report,reportStatisticState)
                  reportStatisticState.reportCount++;
                  if(report.reports===undefined||report.reports.length==0)
                  {
                    reportStatisticState.emptyReportCount++;
                  }

                  reportStatisticState.newAddedReport = [];

                  //Reset the current object property, then we will check if there's a new similar report object as it.
                  reportStatisticState.trackingWindow.forEach((srep_inWindow) => {
                    srep_inWindow.isCurObj = false;
                  });

                  //Check if the trackingWindow object is timeout(from tracking window)
                  reportStatisticState.trackingWindow =
                    reportStatisticState.trackingWindow.filter((srep_inWindow) => {
                      let tdiff = currentTime_ms - srep_inWindow.time_ms;
                      if (tdiff < statSetting.keepInTrackingTime_ms) {
                        return true;
                      }
                      //if the time is longer than 4s then remove it from matchingWindow
                      if (srep_inWindow.repeatTime > statSetting.minReportRepeat
                        && srep_inWindow.headSkipTime == 0) {
                        // The SAME per-製程 override the verdict is graded
                        // against further down (cur_MarginInfo -> resultGrading).
                        // The statistics used to read the ROOT shapes instead, so
                        // CP/CPK described a tolerance the part was never judged
                        // by -- and a 製程 that TIGHTENS the tolerance therefore
                        // read as more capable than it is, which is the wrong
                        // direction to be wrong in.
                        //
                        // Computed here rather than reusing cur_MarginInfo: that
                        // is declared several hundred lines below and this runs
                        // first, so referencing it would be a temporal dead zone
                        // -- a crash, not a stale value. pickCtrlMargin is the
                        // single source both paths already agree on.
                        const _dec = newState.edit_info.__decorator;
                        const _statMargin = (_dec && _dec.control_margin_info)
                          ? pickCtrlMargin(newState.edit_info.inspOptionalTag,
                                           _dec.control_margin_info).info
                          : undefined;
                        reportStatisticState.statisticValue = statReducer(reportStatisticState.statisticValue, srep_inWindow, _statMargin);

                        delete srep_inWindow.headSkipTime;
                        delete srep_inWindow.minReportRepeat;
                        delete srep_inWindow.maxReportRepeat;

                        // History keeps the VERDICT, and nothing else.
                        //
                        // A KEEP-list, deliberately: everything that reads
                        // historyReport is known and small. The trend chart
                        // (ControlChart) reads time_ms plus judgeReports
                        // {id,name,value,detailStatus}; the stats table is built
                        // from statisticValue's running aggregates and never
                        // touches this array; statReducer above already consumed
                        // the FULL report a few lines up. So a report earns its
                        // place in the history by those two fields alone.
                        //
                        // Measured on a real report from this machine: 22.6 kB per
                        // object, judgeReports 272 B of it (1.2%), the point clouds
                        // (searchPoints/detectedLines/detectedCircles/auxPoints)
                        // 97.6%. Kept 1000 deep that was ~1.2 GB of live heap after
                        // four minutes at 30/s (bench, 2026-08-20: 600 MB -> 2.5 GB
                        // running -> 1.8 GB once stopped, i.e. retained not churn).
                        //
                        // The geometry still travels in newAddedReport, which the
                        // DB upload and the line/angle readout consume and which is
                        // emptied every batch -- nothing that needs the points
                        // loses them.
                        reportStatisticState.historyReport.push({
                          time_ms:      srep_inWindow.time_ms,
                          judgeReports: srep_inWindow.judgeReports,
                        });
                        // Trim to the CONFIGURED limit. This used to slice to a
                        // hardcoded 1000 regardless of historyReportlimit, so
                        // lowering the setting did nothing at all -- the buffer sat
                        // at ~1000 whatever the config said.
                        const _histLimit = (statSetting.historyReportlimit > 0)
                          ? statSetting.historyReportlimit : 100;
                        if (reportStatisticState.historyReport.length > _histLimit) {
                          reportStatisticState.historyReport =
                            reportStatisticState.historyReport.slice(-_histLimit);
                        }

                        reportStatisticState.newAddedReport.push(srep_inWindow);
                      }
                      else {
                        log.error("the current data only gets few samples, ignore",
                          "this error case is to remove abnormal sample that's caused by air blow");
                        log.error("repeatTime:", srep_inWindow.repeatTime)
                        log.error("headSkipTime:", srep_inWindow.headSkipTime)
                      }
                      return false;
                    });

                  if(action.data.__surpress_display==true)
                  {
                    reportStatisticState.__surpress_display=true;
                  }
                  else
                  {
                    reportStatisticState.__surpress_display=false;
                  }
                  if (ignoreInspData==true || inspReport.reports === undefined) {
                    break;
                  }

                  {//Do matching in tracking_window
                    
                    
                    // Shared with InspectionUI's fold, which is what the WIRE
                    // DEF is built from. This reduce kept the LAST matching tag
                    // while that side takes the FIRST, so a part carrying two
                    // tags with rows was sorted by one 製程 and coloured by the
                    // other. See UTIL/ctrlMarginPick.js.
                    function MarginInfoExtraction(tags,control_margin_info=newState.edit_info.__decorator.control_margin_info)
                    {
                      const pick = pickCtrlMargin(tags, control_margin_info);
                      if (pick.ambiguous.length)
                        log.warn('[margin] more than one 製程 has a control-margin row for this part: '
                          + pick.ambiguous.join(', ') + ' -- using ' + pick.tag);
                      return pick.info;
                    }
                    // isFlipped decides which set of limits applies. It is not a
                    // display detail: the core judges a flipped part against the
                    // _b limits, so grading without it produces a verdict for the
                    // other side of the part.
                    function resultGrading(judgeReports,marginInfo,fallback_marginInfo,isFlipped)
                    {
                      ////marginInfo may be shapelist
                      //it consists [{id,value,USL,LSL,UCL,LCL}....]
                      
                      let pfilled_marginInfo=fallback_marginInfo.map(fm=>{
                        let loc_info=marginInfo.find(m=>m.id==fm.id);
                        if(loc_info===undefined)
                          return fm;
                        return {...fm,...loc_info};
                      });
                      judgeReports.forEach((jud)=>{
                        jud.detailStatus=
                          newState.edit_info._obj.getMeasure_detailStatus(jud,pfilled_marginInfo,isFlipped);

                        // Stamp the limits that were ACTUALLY used, alongside the
                        // verdict they produced.
                        //
                        // The UI draws a scale behind each reading -- spec limits,
                        // production control limits and target. Its only source for
                        // those was `shape_def` (the root shapeList), while the
                        // verdict here comes from pfilled_marginInfo: the root
                        // overlaid with the per-製程 control margins. Whenever a
                        // 製程 overrides a limit the drawn scale and the colour
                        // beside it would have disagreed, and nothing on screen
                        // would say which one was lying. Taking both from the same
                        // object makes that impossible rather than unlikely.
                        const _lim = pfilled_marginInfo.find(m=>m.id==jud.id);
                        if(_lim!==undefined)
                        {
                          // Through effectiveLimits, the same call the verdict
                          // above went through -- so the scale drawn on the row
                          // cannot show one side's limits under the other side's
                          // colour.
                          jud.lim = effectiveLimits(_lim, isFlipped);
                        }

                        // The CORE's verdict is the one that acted.
                        //
                        // It is what moved the part, and it is computed from the
                        // def this WebUI sent it -- the core never invents limits,
                        // it only has what came down FI/CI. So when the two
                        // disagree it is always this side that has mis-read a def
                        // it wrote itself, and overwriting the core's answer with
                        // the local one puts a colour on screen that contradicts
                        // where the part physically went.
                        //
                        // That is what happened with flipped parts for as long as
                        // this function ignored isFlipped. It was invisible: the
                        // screen simply looked wrong to nobody in particular. So
                        // rather than trust that the two implementations agree,
                        // count the times they do not -- __gradeMismatch is read
                        // by the diag probe, so a drift becomes a number instead
                        // of an anecdote.
                        const _coreSaid = jud.status;
                        const _localNG =
                          jud.detailStatus==MEASURERSULTRESION.USNG||
                          jud.detailStatus==MEASURERSULTRESION.LSNG||
                          jud.detailStatus==MEASURERSULTRESION.SNG||
                          jud.detailStatus==MEASURERSULTRESION.NG;
                        const _localNA = jud.detailStatus==MEASURERSULTRESION.NA;
                        if(!_localNA && _coreSaid!==undefined
                           && _coreSaid!==INSPECTION_STATUS.NA
                           && _localNG!==(_coreSaid===INSPECTION_STATUS.FAILURE))
                        {
                          if(typeof window!=='undefined')
                          {
                            window.__gradeMismatch=(window.__gradeMismatch||0)+1;
                            window.__gradeMismatchLast={
                              id:jud.id, name:jud.name, value:jud.value,
                              isFlipped:!!isFlipped, core:_coreSaid,
                              ui:jud.detailStatus, lim:jud.lim };
                          }
                        }

                        if(jud.detailStatus==MEASURERSULTRESION.NA)
                        {
                          jud.status = INSPECTION_STATUS.NA;
                        }
                        else if(
                          jud.detailStatus==MEASURERSULTRESION.USNG||
                          jud.detailStatus==MEASURERSULTRESION.LSNG||
                          jud.detailStatus==MEASURERSULTRESION.SNG||
                          jud.detailStatus==MEASURERSULTRESION.NG)
                        {
                          jud.status = INSPECTION_STATUS.FAILURE;
                        }
                        else
                        {
                          jud.status = INSPECTION_STATUS.SUCCESS;
                        }
                      });
                    }

                    let root_MarginInfo=newState.edit_info._obj.shapeList;
                    let cur_MarginInfo=MarginInfoExtraction(newState.edit_info.inspOptionalTag);
                    if(cur_MarginInfo===undefined)
                    {
                      cur_MarginInfo=root_MarginInfo;
                    }
                    // console.log(cur_MarginInfo,newState.edit_info.inspOptionalTag);
                    //new inspection report >
                    //  [update/insert]> tracking_window >
                    //     [if no update after 4s]> historyReport

                    let imageW_mm=NaN;
                    let imageH_mm=NaN;
                    if(newState.edit_info.img!=null)
                    {
                      imageW_mm=newState.edit_info.img.full_width*mmpcampix;
                      imageH_mm=newState.edit_info.img.full_height*mmpcampix;  
                    }

                    inspReport.reports.forEach((singleReport) => {

                      if(camParam.mask_radius!==undefined)
                      {
                        let dist= Math.hypot(singleReport.cx-imageW_mm/2,singleReport.cy-imageH_mm/2);
                        
                        // console.log(dist,sig360MaxMagnitude,camParam.mask_radius);
                        let isOutOfCircleMaskRange=(dist+sig360MaxMagnitude)>(camParam.mask_radius);
                        if(isOutOfCircleMaskRange)
                        {
  
                          return;//ignore
                        }
  
                      }


                      let closeRep = reportStatisticState.trackingWindow.reduce((closeRep, srep_inWindow) => {
                        if (closeRep !== undefined) return closeRep;
                        //Check direction consistency
                        if (singleReport.isFlipped != srep_inWindow.isFlipped) {
                          return closeRep;
                        }

                        //Check area consistency
                        let areaDiff = singleReport.area / srep_inWindow.area;
                        if (areaDiff > 1.2 || areaDiff < 1 / 1.2) {
                          return closeRep;
                        }

                        //Check retation consistency
                        let angleDiff = singleReport.rotate - srep_inWindow.rotate;
                        if (angleDiff > 180) angleDiff = angleDiff - 360;
                        if (angleDiff > 4 || angleDiff < -4) {
                          return closeRep;
                        }

                        //Check position consistency
                        let distance = Math.hypot(singleReport.cx - srep_inWindow.cx, singleReport.cy - srep_inWindow.cy);

                        if (distance > mmpcampix*4) {
                          return closeRep;
                        }
                        //If we get here, which means the information is very similar.
                        //return/mark the current object as same report object
                        return srep_inWindow;
                      }, undefined);
                      function valueAveIn(ave, new_val, datCount_before) {

                        ave += (1 / (datCount_before + 1)) * (new_val - ave);
                        return ave;
                      }
                      
                      
                      //HACK:the core might return null in value and still give non-NA status HACK it, then fix it on Core
                      singleReport.judgeReports.forEach(sjrep=>{
                        if(sjrep.value === undefined || sjrep.value===null)
                        {
                          sjrep.status=INSPECTION_STATUS.NA;
                        }


                      });

                      let maxReportRepeat = statSetting.maxReportRepeat;
                      if (closeRep !== undefined && maxReportRepeat!==undefined && closeRep.repeatTime>maxReportRepeat)
                      {
                        closeRep.time_ms = currentTime_ms;
                        closeRep.isCurObj = true;
                      }
                      else if (closeRep !== undefined) {
                        //blend the report with the existed report in tracking window  




                        closeRep.area = valueAveIn(closeRep.area, singleReport.area, closeRep.repeatTime);
                        closeRep.cx = valueAveIn(closeRep.cx, singleReport.cx, closeRep.repeatTime);
                        closeRep.cy = valueAveIn(closeRep.cy, singleReport.cy, closeRep.repeatTime);
                        // The match score gets blended on the SAME rule as the
                        // three above, and it has to. A tracked object keeps
                        // its entry for many frames; leaving similarity alone
                        // would pin it to the first sighting -- which is the
                        // one frame taken while the part was still entering,
                        // and typically its worst. Somebody tuning the
                        // threshold against that number would be tuning
                        // against a value the machine stopped seeing.
                        // Guarded, unlike cx/cy: those always exist, and a
                        // report from a locator that has no match score would
                        // otherwise turn this into NaN and put "NaN" on the
                        // panel instead of nothing.
                        if (Number.isFinite(closeRep.similarity) && Number.isFinite(singleReport.similarity))
                          closeRep.similarity = valueAveIn(closeRep.similarity, singleReport.similarity, closeRep.repeatTime);
                        else if (Number.isFinite(singleReport.similarity))
                          closeRep.similarity = singleReport.similarity;
                        //closeRep.area+=(1/(closeRep.repeatTime+1))*(sjrep.area-cjrep.area);

                        closeRep.detectedLines.forEach((clrep) => {

                          let id = clrep.id;
                          let slrep = singleReport.detectedLines.find((slrep) => slrep.id == id);
                          if (clrep.status == INSPECTION_STATUS.NA && slrep.status == INSPECTION_STATUS.NA) return;
                          

                          if (slrep.status == INSPECTION_STATUS.NA) {//new report is NA
                            //don't do anything
                          }
                          else if(clrep.status == INSPECTION_STATUS.NA)//reports in history is NA 
                          {
                            
                            clrep.cx = slrep.cx;
                            clrep.cy = slrep.cy;
                            clrep.vx = slrep.vx;
                            clrep.vy = slrep.vy;
                          }
                          else
                          {

                            clrep.cx = valueAveIn(clrep.cx, slrep.cx, closeRep.repeatTime);
                            clrep.cy = valueAveIn(clrep.cy, slrep.cy, closeRep.repeatTime);
                            clrep.vx = valueAveIn(clrep.vx, slrep.vx, closeRep.repeatTime);
                            clrep.vy = valueAveIn(clrep.vy, slrep.vy, closeRep.repeatTime);

                          }
                        });


                        closeRep.detectedCircles.forEach((ccrep) => {
                          let id = ccrep.id;
                          let screp = singleReport.detectedCircles.find((screp) => screp.id == id);

                          if (ccrep.status == INSPECTION_STATUS.NA && screp.status == INSPECTION_STATUS.NA) return;
                          


                          if (screp.status == INSPECTION_STATUS.NA) {//new report is NA
                            //don't do anything
                          }
                          else if(ccrep.status == INSPECTION_STATUS.NA)//reports in history is NA 
                          {
                            
                            ccrep.x = screp.x
                            ccrep.y = screp.y
                            ccrep.r = screp.r
                            ccrep.s = screp.s
                          }
                          else
                          {

                            ccrep.x = valueAveIn(ccrep.x, screp.x, closeRep.repeatTime);
                            ccrep.y = valueAveIn(ccrep.y, screp.y, closeRep.repeatTime);
                            ccrep.r = valueAveIn(ccrep.r, screp.r, closeRep.repeatTime);
                            ccrep.s = valueAveIn(ccrep.s, screp.s, closeRep.repeatTime);
                          }



                        });


                        closeRep.searchPoints.forEach((ccrep) => {
                          let id = ccrep.id;
                          let screp = singleReport.searchPoints.find((screp) => screp.id == id);
                          
                          if (ccrep.status == INSPECTION_STATUS.NA && screp.status == INSPECTION_STATUS.NA) return;
                          

                          
                          if (screp.status == INSPECTION_STATUS.NA) {//new report is NA
                            //don't do anything
                          }
                          else if(ccrep.status == INSPECTION_STATUS.NA)//reports in history is NA 
                          {
                            
                            ccrep.x = screp.x
                            ccrep.y = screp.y
                          }
                          else
                          {

                            ccrep.x = valueAveIn(ccrep.x, screp.x, closeRep.repeatTime);
                            ccrep.y = valueAveIn(ccrep.y, screp.y, closeRep.repeatTime);
                          }

                        });


                        
                        closeRep.judgeReports.forEach((cjrep) => {
                          
                          let sjrep = singleReport.judgeReports.find((sjrep_) => sjrep_.id == cjrep.id);

                          
                          //console.log("=======",sjrep);
                          if (sjrep === undefined || sjrep.status == INSPECTION_STATUS.NA||sjrep.value!=sjrep.value) 
                          {//Skip this value
                            return;
                          }
                          
                          if (cjrep.status == INSPECTION_STATUS.NA||cjrep.value!=cjrep.value) {
                            cjrep.status = sjrep.status;//If the original value is NA, replace it with the new one
                            cjrep.value = sjrep.value;//Might be NA as well
                            return;
                          }
                          //The remaining is sjrep and cjrep are available
                          
                          cjrep.value += (1 / (closeRep.repeatTime + 1)) * (sjrep.value - cjrep.value);

                        });

                        resultGrading(closeRep.judgeReports,cur_MarginInfo,root_MarginInfo,closeRep.isFlipped);
                        //closeRep.seq.push(singleReport);//Push current report into the sequence
                        closeRep.time_ms = currentTime_ms;
                        closeRep.repeatTime += 1;
                        if (closeRep.headSkipTime > 0) {
                          closeRep.headSkipTime--;
                          //When down to zero, reset repeatTime
                          //Zero repeatTime will let next incoming data to overwrite current data
                          if (closeRep.headSkipTime == 0) {
                            closeRep.repeatTime = 0;
                          }
                        }

                        closeRep.isCurObj = true;
                      }
                      else {

                        //If there is no report in tracking window similar to the current report
                        //Add into the trackingWindow
                        let treport = dclone(singleReport);

                        
                        resultGrading(treport.judgeReports,cur_MarginInfo,root_MarginInfo,treport.isFlipped);


                        treport.time_ms = currentTime_ms;
                        treport.add_time_ms = currentTime_ms;
                        treport.subFeatureDefSha1 = subFeatureDefSha1;
                        treport.tag = inspOptionalTag;
                        treport.machine_hash = machine_hash;
                        treport.repeatTime = 1;
                        treport.headSkipTime = statSetting.headReportSkip;
                        
                        treport.minReportRepeat = statSetting.minReportRepeat;
                        treport.maxReportRepeat = statSetting.maxReportRepeat;
                        //treport.seq=[singleReport];
                        treport.isCurObj = true;
                        reportStatisticState.trackingWindow.push(treport);
                      }


                    });

                    //Remove the non-Current object with repeatTime<=1, which suggests it's a noise
                    //In other word, in order to stay, you need to be a CurObj/ repeatTime>2
                    reportStatisticState.trackingWindow =
                      reportStatisticState.trackingWindow.
                        filter((srep_inWindow) => (srep_inWindow.isCurObj || srep_inWindow.repeatTime >= statSetting.minReportRepeat));
                      
                    if(action.data.__surpress_display==true)
                    {
                      // reportStatisticState.trackingWindow.forEach(rep=>{
                      //   rep.isCurObj = false;
                      // });
                      reportStatisticState.__surpress_display=true;
                    }
                    else
                    {
                      reportStatisticState.__surpress_display=false;
                    }
                  }

                  newState.edit_info.reportStatisticState={...reportStatisticState};
                  if (false) {
                    let reportGroup = newState.edit_info.inspReport.reports[0].reports.map(report => report.judgeReports);
                    let measure1 = newState.edit_info.reportStatisticState.measure1;
                    if (measure1 === undefined) measure1 = [];
                    measure1.push({
                      genre: "G" + Math.random(), sold: Math.random()
                    })
                    if (measure1.length > 20) measure1.shift();
                    newState.edit_info.reportStatisticState = Object.assign({},
                      newState.edit_info.reportStatisticState,
                      {
                        measure1: measure1
                      });
                    ;
                  }
                }
                break;
              case "camera_calibration":
                if (report.error !== undefined && report.error == 0) {
                  newState.edit_info._obj.SetCameraParamInfo(report);
                  newState.edit_info.camera_calibration_report = action.data;
                }
                else {
                  newState.edit_info._obj.SetCameraParamInfo(undefined);
                  newState.edit_info.camera_calibration_report = undefined;
                }
                break;
            }

          });
        }
        break;

      case "stage_light_report":
        {
          newState.edit_info.stage_light_report = action.data;
        }
        break;
    }
    //newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
  }

  do{
        //console.log(action);
        if (stateObj.state == UISTS.DEFCONF_MODE && newState.defConf_lock_level != 0 && action.IGNORE_DEFCONF_LOCK!=true) {
          let level3Filter = [DefConfAct.EVENT.DefConf_Lock_Level_Update]

          let level2Filter = level3Filter.concat([DefConfAct.EVENT.Edit_Tar_Update]);

          let level1Filter = level2Filter.concat(
            [DefConfAct.EVENT.Shape_Decoration_ID_Order_Update,
            DefConfAct.EVENT.Shape_Decoration_Extra_Info_Update]);

          let matchWL = level1Filter.find(actT => actT === action.type);
          //console.log("action.type:"+action.type,"   ",matchWL);
          if (matchWL === undefined) {
            break;
          }
        }
        //console.log(action.type,action);
        switch (action.type) {
          case DefConfAct.EVENT.DefConf_Lock_Level_Update:
            newState = { ...newState, defConf_lock_level: action.data };
            //console.log(newState);
            break;

          case UISEV.StatSettingParam_Update:
            newState.edit_info.statSetting =
            {
              ...newState.edit_info.statSetting,
              ...action.data
            };
            log.debug("[StatSettingParam_Update]", newState.edit_info.statSetting);
            break;

            
          case UISEV.StatInfo_Clear:

          
            log.debug("[StatInfo_Clear]");
            newState.edit_info = 
            newState.edit_info._obj.resetStatisticState(newState.edit_info);

            break;
            
          case UISEV.Image_Update:
            newState.edit_info = { ...newState.edit_info, img: action.data };
            break;


            
          case UISEV.SIG360_Report_Update:
          case UISEV.SIG360_Extraction:
              
              // Edit_info_reset(newState);
              newState.edit_info._obj.Setsig360info(action.data);
              
              newState.edit_info.sig360info = newState.edit_info._obj.sig360info;
              break;
  
  
          case UISEV.Inspection_Report:
            {
              let reportSkip =false;
              let inspMode=GetObjElement(newState,["machine_custom_setting","InspectionMode"]);
              let uInspResult=GetObjElement(action,["data","uInspResult"]);

              // The station block is TOP-LEVEL, next to uInspResult -- it
              // describes the machine's station, not any one located object, so
              // it does not belong in the per-object sig360 sub-report that
              // becomes edit_info.inspReport. Keep it where the panel can find
              // it. Undefined against a core that does not send it.
              // Same reasoning for the timing: how long the core took is a
              // property of the FRAME, not of any one located object, so it
              // rides top-level next to station rather than inside the sig360
              // sub-report. Putting it on inspReport was the first attempt and
              // it silently never appeared on the live path -- that sub-report
              // is only built on some branches, while this one runs for every
              // report from both the editor's II and the live CI/FI stream.
              //
              // build_ms is undefined on the live path by design: CI/FI build
              // the engine once at session open, so there is no per-frame def
              // build to report.
              newState.edit_info = { ...newState.edit_info,
                station: GetObjElement(action,["data","station"]),
                // WHICH LOCALIZER RAN -- from the core, not from the def.
                //
                // A def asking for shape_based gets it only if shape training
                // succeeded; otherwise the core falls through to sig360 and
                // measures anyway. Both look identical from here, so this is
                // the core's own answer about the frame that was just run.
                // Sibling of station rather than a member of insp_timing: same
                // object literal, so it is paired to the same frame, without
                // putting a non-timing fact inside something called timing.
                insp_locator: GetObjElement(action,["data","reports",0,"locator"]),
                insp_timing: {
                  wall_ms:  GetObjElement(action,["data","insp_wall_ms"]),
                  cpu_ms:   GetObjElement(action,["data","insp_cpu_ms"]),
                  build_ms: GetObjElement(action,["data","def_build_ms"]),
                  // The per-phase breakdown of THIS frame, names as the core
                  // produced them. Absent against an older core, and then the
                  // caption simply has one less line.
                  phase_ms: GetObjElement(action,["data","insp_phase_ms"]),
                } };

              //when in Full inspection mode if the uInspResult(the final result sends to inspection machine)
              //is NA/UNSET(may caused by dirty image/ non-single object...), when means to tell insp mach skip this one
              //so we gonna skip the report to put in(even if there may be a result)
              // reportSkip=(inspMode=="FI")&&
              //   ((uInspResult== INSPECTION_STATUS.NA)  ||  (uInspResult== INSPECTION_STATUS.UNSET))

              
              EVENT_Inspection_Report(newState, action,reportSkip);

            }
            break;

          case UISEV.Define_File_Update:

            let root_defFile = action.data;
            if (root_defFile.type === "binary_processing_group") {
              let bk_inspOptionalTag=undefined;
              // console.log(">>>>>>>>>>>>>>>>>",action);
              if(action.keepCurTag)
              {
                bk_inspOptionalTag=newState.edit_info.inspOptionalTag;
              }
              Edit_info_reset(newState);
              newState.edit_info = 
                newState.edit_info._obj.rootDefInfoLoading(root_defFile,newState.edit_info);

              
              if(bk_inspOptionalTag!==undefined)
              {
                newState.edit_info.inspOptionalTag=bk_inspOptionalTag;
              }
            }
            break;

          case DefConfAct.EVENT.Instrument_Mmpp_Set: {
            // Dropping the old signature is half the fix and the less obvious
            // half. getEditorMmpp reads sig360info FIRST, and a retake does not
            // clear it -- so without this the previous def's mmpp keeps winning
            // and the number set here would never be read. The signature also
            // describes a part that is no longer in the picture.
            // Assigned, not Setsig360info(null): that setter dereferences
            // sig360info.reports[0] on its first line. The def loader's own
            // no-signature branch does exactly this.
            newState.edit_info._obj.sig360info = null;
            newState.edit_info._obj.instrumentMmpp = action.data;
            newState.edit_info.inherentShapeList =
              newState.edit_info._obj.UpdateInherentShapeList();
            break;
          }

          case DefConfAct.EVENT.Def_Retake: {
            // Same key set the def loader resets, for the same reason: another
            // def's recipe settings are worse than none, because they configure
            // a locator that then looks right.
            //
            // keepMeasurements narrows that to the LOCALIZER's keys: the picture
            // changed, so registration / trained features / extraction regions
            // are gone whatever happens, but the calipers and the matching
            // parameters were authored against the part, not against the frame,
            // and re-drawing them for every retake is the thing this mode exists
            // to avoid.
            const _keep = !!(action.data && action.data.keepMeasurements);
            const _blank = Edit_info_Empty();
            const _keys = _keep ? DEF_LOCALIZER_SCOPED_KEYS : DEF_SCOPED_EDIT_INFO_KEYS;
            for (const k of _keys) newState.edit_info[k] = _blank[k];
            if (_keep) {
              // The localization polygons live in the shape list and belong to
              // the localizer, so they go with it -- everything else stays.
              newState.edit_info._obj.SetShapeList(
                (newState.edit_info._obj.shapeList || []).filter(
                  (sh) => !(sh && (sh.type === 'loc_include' || sh.type === 'loc_exclude'))));
            } else {
              newState.edit_info._obj.SetShapeList([]);
            }
            newState.edit_info.edit_tar_info = null;
            newState.edit_info.inherentShapeList = newState.edit_info._obj.UpdateInherentShapeList();
            // What is on screen is no longer the saved def's reference image, so
            // nothing may stamp that file as the shape template any more.
            newState.edit_info.__img_fresh_capture = true;
            break;
          }

          case DefConfAct.EVENT.Shape_List_Update:
            newState.edit_info._obj.SetShapeList(action.data);
            newState.edit_info.edit_tar_info = null;
            newState.edit_info.__decorator.list_id_order =
              UpdateListIDOrder(newState.edit_info.__decorator.list_id_order, newState.edit_info._obj.shapeList);
            newState.edit_info.inherentShapeList = newState.edit_info._obj.UpdateInherentShapeList();
            break;

          case DefConfAct.EVENT.Edit_Tar_Update:
            newState.edit_info.edit_tar_info =
              (action.data == null) ? null : Object.assign({}, action.data);

            newState.edit_info.edit_tar_ele_trace = null;
            newState.edit_info.edit_tar_ele_cand = null;
            break;

          case DefConfAct.EVENT.Edit_Tar_Ele_Trace_Update:
            let edit_tar_ele_trace=(action.data == null) ? null : action.data.slice();
            // newState.edit_info.edit_tar_ele_trace =
              // (action.data == null) ? null : action.data.slice();
            newState={...newState,edit_info:{...newState.edit_info,edit_tar_ele_trace}}

            break;
          case DefConfAct.EVENT.Edit_Tar_Ele_Cand_Update:
            newState.edit_info.edit_tar_ele_cand =
              (action.data == null) ? null : (action.data instanceof Object) ? Object.assign({}, action.data) : action.data;
            log.info("DEFCONF_MODE_Edit_Tar_Ele_Cand_Update", newState.edit_info.edit_tar_ele_cand);
            break;
          
          case UISEV.machine_custom_setting_Update:
            {
              newState.machine_custom_setting ={...newState.machine_custom_setting ,...action.data};
              
            }
            break;

          case DefConfAct.EVENT.DefFileName_Update:
            {
              newState.edit_info = Object.assign({}, newState.edit_info, { DefFileName: action.data });
              break;
            }

          case DefConfAct.EVENT.DefFileTag_Update:
            {
              newState.edit_info = { ...newState.edit_info, DefFileTag: action.data };
              break;
            }

          case DefConfAct.EVENT.MachTag_Update:
            {
              newState = { ...newState, MachTag: action.data };
              break;
            }


          case DefConfAct.EVENT.Matching_Angle_Margin_Deg_Update:
            {
              // R8: was an unconditional assign — garbage (strings, objects, NaN) landed
              // verbatim. Require a finite number; ignore anything else.
              if (typeof action.data === 'number' && Number.isFinite(action.data)) {
                newState.edit_info = { ...newState.edit_info, matching_angle_margin_deg: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Matching_Face_Update:
            {
              // R8: was unconditional. The HR-resolve comment in BPG_WS.js notes valid
              // values are -1 (back) / 0 (both) / 1 (front); accept only those.
              if (action.data === -1 || action.data === 0 || action.data === 1) {
                newState.edit_info = { ...newState.edit_info, matching_face: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Matching_Version_Update:
            {
              // sig360 matching algo: 1 = legacy v1 (byte-identical pre-milestone),
              // 2 = morph-boundary dual-sig + centroid iter (core ee1cd247).
              if (action.data === 1 || action.data === 2) {
                newState.edit_info = { ...newState.edit_info, matching_version: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Inspection_Downsample_Update:
            {
              // Pre-CCL downsample factor; 1 = no downsample (default). Core caps
              // at 4× per the perf commit (ee1cd247).
              if (typeof action.data === 'number' && Number.isFinite(action.data) &&
                  action.data >= 1 && action.data <= 8) {
                newState.edit_info = { ...newState.edit_info, inspection_downsample: Math.floor(action.data) };
              }
              break;
            }

          case DefConfAct.EVENT.Sig_Match_Sim_Thres_Update:
            {
              // sig360 minimum similarity to accept a match (core "sig_match_sim_thres",
              // default 0.9). Lower = more permissive. Clamp to [0,1].
              if (typeof action.data === 'number' && Number.isFinite(action.data) &&
                  action.data >= 0 && action.data <= 1) {
                newState.edit_info = { ...newState.edit_info, sig_match_sim_thres: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Morph_Mode_Update:
            {
              // Anchor-morph model: "tps" (similarity-base RBF, core default) |
              // "wls_similarity" | "legacy".
              if (typeof action.data === 'string' &&
                  ['tps', 'wls_similarity', 'legacy'].includes(action.data)) {
                newState.edit_info = { ...newState.edit_info, morph_mode: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Morph_TPS_Lambda_Update:
            {
              // RBF bending stiffness (core default 0.5). undefined => use core default.
              if (action.data === undefined ||
                  (typeof action.data === 'number' && Number.isFinite(action.data) && action.data >= 0)) {
                newState.edit_info = { ...newState.edit_info, morph_tps_lambda: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Morph_Max_Iter_Update:
            {
              // Morph relocation iterations (core default 1). undefined => core default.
              if (action.data === undefined ||
                  (typeof action.data === 'number' && Number.isFinite(action.data) && action.data >= 1)) {
                newState.edit_info = { ...newState.edit_info,
                  morph_max_iter: action.data === undefined ? undefined : Math.floor(action.data) };
              }
              break;
            }

          case DefConfAct.EVENT.Morph_Alpha_Update:
            {
              // Morph re-location relaxation / learning-rate, (0,1] (core default 1).
              // <1 damps overshoot so the iteration converges on large deformations.
              if (action.data === undefined ||
                  (typeof action.data === 'number' && Number.isFinite(action.data) && action.data > 0 && action.data <= 1)) {
                newState.edit_info = { ...newState.edit_info, morph_alpha: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Shape_Match_Scale_Update:
            {
              // Shape-locator coarse-match downscale, (0,1] (core default 1). 0.3 ~3x
              // faster; ROI refine restores accuracy. undefined => core default.
              if (action.data === undefined ||
                  (typeof action.data === 'number' && Number.isFinite(action.data) && action.data > 0 && action.data <= 1)) {
                newState.edit_info = { ...newState.edit_info, shape_match_scale: action.data };
              }
              break;
            }

          case DefConfAct.EVENT.Locating_Engine_Update:
            {
              // Localizer: "sig360" (contour signature) or "shape_based" (line2Dup +
              // ROI refine). The shape locator trains from the def's <base>.png sidecar.
              if (action.data === 'sig360' || action.data === 'shape_based') {
                newState.edit_info = { ...newState.edit_info, locating_engine: action.data };
              }
              // THE PRIMITIVES FOLLOW THE ENGINE, HERE, IN THE EDITOR'S OWN SHAPES.
              //
              // shape_based has no contour grid, so every line/arc/search point
              // must locate by caliper. That conversion used to live only in
              // defFileGeneration, on the OUTPUT: what the core got and what the
              // file said were converted, what the canvas and the property sheet
              // showed were not. Right after 升級 some primitives drew as caliper
              // (the ones the core's reply had attached hits to), the rest as
              // contour, and the caliper fields were empty boxes -- until a save
              // and a reload made the file the truth. Reported 2026-09-04.
              //
              // Here, because this is the one place the engine changes: the
              // migration button, the settings radio, TAKE and the studio opener
              // all dispatch this. Same function as the save path, so the two
              // cannot disagree; the save path stays as the safety net for a
              // primitive drawn after the flip. Flat arcs are LEFT contour and
              // listed in __primitive_migration for the UI to name -- they need
              // re-teaching, and converting them measures a wrong radius that
              // passes (see _caliperSeed).
              if (action.data === 'shape_based' && newState.edit_info._obj
                  && Array.isArray(newState.edit_info._obj.shapeList)) {
                const _obj = newState.edit_info._obj;
                const mmpp = _obj.getEditorMmpp ? _obj.getEditorMmpp() : 1;
                const converted = [], left = [];
                let changed = false;
                const next = _obj.shapeList.map((s) => {
                  const r = convertShapeForShapeBased(s, mmpp);
                  const label = (s && (s.name || ('id ' + s.id))) || '?';
                  if (r.action === 'converted') { converted.push(label); changed = true; }
                  else if (r.action === 'left_contour_arc') left.push(label);
                  return r.shape;
                });
                if (changed) {
                  _obj.SetShapeList(next);
                  newState.edit_info.edit_tar_info = null;
                  newState.edit_info.__decorator.list_id_order =
                    UpdateListIDOrder(newState.edit_info.__decorator.list_id_order, _obj.shapeList);
                  newState.edit_info.inherentShapeList = _obj.UpdateInherentShapeList();
                }
                if (changed || left.length)
                  newState.edit_info.__primitive_migration = { converted, leftContourArcs: left, at: Date.now() };
              }
              break;
            }

          case DefConfAct.EVENT.EditInfo_Patch:
            {
              // Generic shallow merge into edit_info (localization settings:
              // def_image_reg, roi_refine_points). New object ref so selectors re-run.
              if (action.data && typeof action.data === 'object') {
                // Held before the merge: the snapshot below has to record the
                // settings the cache was trained against, not the new ones.
                const prevEditInfo = newState.edit_info;
                newState.edit_info = { ...newState.edit_info, ...action.data };
                // CHANGING THE REGISTRATION OR THE ROI POINTS INVALIDATES THE
                // TRAINED FEATURES — but do NOT throw them away.
                //
                // The core no longer refuses a cache whose fingerprint has
                // moved -- it loads it and warns. So this flag no longer means
                // "the def is about to fall back to sig360"; it means the
                // features, and the crop and origin that came with them, are
                // older than the registration on screen, and that registration
                // is therefore not in effect yet.
                //
                // Still worth tracking, for two reasons: it is the only thing
                // that can tell the operator a setting is waiting for a
                // generation, and __shape_lastGood is the revert.
                //
                // Deleting the cache was the first fix and it made things
                // worse: the def then leaves the studio with NO features at
                // all, which is strictly less recoverable than an older set.
                // Keep the last one that WORKED, together with the settings it
                // was trained against, so there is always something to go back
                // to.
                //
                // Nothing here decides what to do about it. The save path does,
                // because that is the last moment a def can still be fixed.
                // roi_refine_points is NOT here, and that is the point.
                //
                // They are attached to the feature set after extraction, never
                // fed into it, and the core rebuilds them from the def on every
                // cache load -- so moving one has never changed a feature. It
                // was listed anyway, which meant adding a refine point marked
                // the whole set stale and demanded a regeneration that produced
                // byte-identical features. The core's fingerprint dropped them
                // in the same change.
                const touched = ['def_image_reg'].filter((k) => k in action.data);
                if (touched.length && newState.edit_info.__shape_cache
                    && !newState.edit_info.__shape_stale) {
                  newState.edit_info.__shape_stale = touched.join('+');
                  // The snapshot IS the revert: cache plus the two fields that
                  // fingerprint it. Restoring all three together makes the
                  // cache valid again by construction.
                  newState.edit_info.__shape_lastGood = {
                    cache: newState.edit_info.__shape_cache,
                    def_image_reg: prevEditInfo.def_image_reg,
                    roi_refine_points: prevEditInfo.roi_refine_points,
                  };
                  console.warn('__shape_cache is now stale (' + touched.join('+')
                    + ' changed). Press 生成特徵點 before saving, or revert.');
                }
              }
              break;
            }

          case DefConfAct.EVENT.DefFileHash_Update:
            {
              let DefFileHash_root = newState.edit_info.DefFileHash_root;//root is still root
              let DefFileHash_pre = newState.edit_info.DefFileHash;//old hash become pre
              let DefFileHash = action.data;

              if (DefFileHash_root === undefined) {
                DefFileHash_root = DefFileHash_pre;
                if (DefFileHash_root === undefined) {
                  DefFileHash_root = DefFileHash;
                }
              }

              newState.edit_info = { ...newState.edit_info, DefFileHash_root, DefFileHash_pre, DefFileHash };
              break;
            }


          case DefConfAct.EVENT.InspOptionalTag_Update:
            {
              let inspOptionalTag = action.data;
              let tags = inspOptionalTag;

              tags = tags.filter((check_tag, check_idx) => {
                if (check_tag.length == 0) return false;
                for (let ii = 0; ii < check_idx; ii++) {
                  if (tags[ii] == check_tag) return false;
                }
                return true;
              });//Chekc duplication and remove empty tag
              inspOptionalTag = tags;
              newState.edit_info = { ...newState.edit_info, inspOptionalTag };
              break;
            }
          case DefConfAct.EVENT.Shape_Decoration_ID_Order_Update:
            {
              log.info("action.data:", action.data);

              // New identities all the way down (edit_info AND __decorator),
              // like the inspOptionalTag case above. Writing the nested field
              // in place left every mapped prop reference-equal, react-redux
              // bailed out, and the drag reorder "sprang back" -- the order
              // WAS recorded, it just didn't render until something else
              // forced a redraw.
              newState.edit_info = { ...newState.edit_info,
                __decorator: { ...newState.edit_info.__decorator,
                  list_id_order:
                    UpdateListIDOrder(action.data, newState.edit_info._obj.shapeList) } };
              break;
            }


          case DefConfAct.EVENT.Shape_Decoration_Extra_Info_Update:
            {
              //log.info("action.data:",action.data);

              newState.edit_info.__decorator = { ...newState.edit_info.__decorator, extra_info: action.data };
              break;
            }

          case DefConfAct.EVENT.Shape_Decoration_Control_Margin_Info_Update:
            {
              //log.info("action.data:",action.data);

              newState.edit_info.__decorator = { ...newState.edit_info.__decorator, control_margin_info: action.data };
              break;
            }
          case DefConfAct.EVENT.Shape_Set:
            {
              //Three cases
              //ID undefined but shaped is defiend -Add new shape
              //ID is defined and shaped is defiend - Modify an existed shape if it's in the list
              //ID is defined and shaped is null   - delete  an existed shape if it's in the list

              if(action.data.shape!=null && action.data.shape!=undefined )
              {
                let shape = action.data.shape;
                if (shape.subtype === SHAPE_TYPE.measure_subtype.calc) {

                  const regexp = /\[(\d+)\]/g;
                  const matches = shape.calc_f.exp.matchAll(regexp);

                  let ref = [];
                  for (const match of matches) {
                    ref.push({ id: parseInt(match[1]) });
                  }
                  //console.log(ref);
                  shape.ref = ref;
                }
              }
              let newID = action.data.id;
              //log.info("newID:",newID);

              let shape = newState.edit_info._obj.SetShape(action.data.shape, newID);

              newState.edit_info.__decorator.list_id_order =
                UpdateListIDOrder(newState.edit_info.__decorator.list_id_order, newState.edit_info._obj.shapeList);

              newState.edit_info.inherentShapeList =
                newState.edit_info._obj.UpdateInherentShapeList();

              if (newID !== undefined) {//If this time it's not for adding new shape(ie, newID is not undefined)
                
                let tmpTarIdx =
                  newState.edit_info._obj.FindShapeIdx(newID);
                if (tmpTarIdx === undefined)//In this case we delete the shape in the list 
                {
                  newState.edit_info.edit_tar_info = null;
                }
                else {//Otherwise, we deepcopy the shape
                  newState.edit_info.edit_tar_info =
                    dclone(newState.edit_info._obj.shapeList[tmpTarIdx]);
                }
                
              }
              else {//We just added a shape, set it as an edit target
                newState.edit_info.edit_tar_info =
                  dclone(shape);
              }

              newState.edit_info = Object.assign({}, newState.edit_info);
            }
            break;
        }


        // Element-binding step of shape create/edit (extracted to the model).
        newState.edit_info._obj.applyEditTarSubstate(newState.edit_info, substate);



        return newState;
    
  }while(false);
  return newState;
}


function newStateUpdate(state,action)
{
  let ret_state = StateReducer(state, action);
  if(ret_state===undefined)return state;
  return {...ret_state}
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {


  if (action.type === undefined || action.type.includes("@@redux/")) return state;
  let newState = state;

  var d = new Date();

  if (action.type === "ATBundle") {
    return action.data.reduce((_state, action) => {
      action.date = d;
      return newStateUpdate(_state,action);
    }, newState);
  }
  else {
    action.date = d;
    return newStateUpdate(newState,action);;
  }

  return newState;
}
export default UICtrlReducer