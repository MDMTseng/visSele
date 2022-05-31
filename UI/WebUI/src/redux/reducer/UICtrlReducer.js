
import { UI_SM_STATES, UI_SM_EVENT, SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { xstate_GetCurrentMainState, GetObjElement, isString } from 'UTIL/MISC_Util';
import { InspectionEditorLogic,UpdateListIDOrder,Edit_info_Empty,MEASURERSULTRESION } from 'UTIL/InspectionEditorLogic';

import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import APP_INFO from 'JSSRCROOT/info.js';
import * as logX from 'loglevel';
import dclone from 'clone';
import { loadavg } from 'os';
import JSum from 'jsum'

import {GetDefaultSystemSetting} from 'JSSRCROOT/info.js';
import dateFormat from 'dateFormat';
import semver from 'semver'
import EC_zh_TW from 'LANG/zh_TW';
import { TrademarkCircleOutlined } from '@ant-design/icons';

let log = logX.getLogger("UICtrlReducer");

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

    case UISEV.Control_SM_Panel:
      newState.showSM_graph = action.data;
      return newState;

  }
  let stateObj = xstate_GetCurrentMainState(newState.c_state);
  let substate = stateObj.substate;

  function histDataReducer(histoInfo, dataValue) {
    if (dataValue < histoInfo.xmin) {
      histoInfo.histo[0]++;
      return histoInfo;
    }
    if (dataValue > histoInfo.xmax) {
      histoInfo.histo[histoInfo.histo.length - 1]++;
      return histoInfo;
    }
    let dataRegion = histoInfo.histo.length - 2;//The first value and last value are the value excced xmin& xmax

    //If the data is in the boundary then there must be a position for it.
    let val_idx = Math.floor(dataRegion * (dataValue - histoInfo.xmin) / (histoInfo.xmax - histoInfo.xmin));
    //Suppose xmax=21 xmin=20 dataRegion=4(index 0~3)
    //idx = floor(dataRegion*(value-min)/(max-min+1));
    //=>value=21   (4*(21-20)/(21-20))=>4/1=4  :on the edge case the idx might hit the boundary
    //=>value=20   (4*(20-20)/(21-20))=>0/1=0 


    //Suppose xmax=20 xmin=10 dataRegion=14(index 0~13)
    //=>value=214   (13*(14-10)/(20-10))=>13*4/10=5.2  :on the edge case the idx might hit the boundary
    //=>value=19    (13*(19-10)/(20-10))=>13*9/10=117/10=11.7
    //=>value=19.9    (13*(19.9-10)/(20-10))=>13*9.9/10=117/10=12.87

    if (val_idx >= dataRegion) val_idx = dataRegion - 1;//Just in case it hits upper boundary;
    histoInfo.histo[val_idx + 1]++;//with 1 padding for lower over boundary data

    return histoInfo;
  }

  function statReducer_sp(stat_sp,measuredef,new_rep)
  {

    // console.log(stat_sp,measuredef,new_rep);

    if( (new_rep.status!=INSPECTION_STATUS.FAILURE && new_rep.status!=INSPECTION_STATUS.SUCCESS) || measuredef.quality_essential==false)
    {
      return stat_sp;
    }
    let new_sp={...stat_sp};
    if(new_rep.value>measuredef.USL)//upper NG
    {
      new_sp.SNG_count++;
      new_sp.consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_info=5;
    }
    else if(new_rep.value<measuredef.LSL)//lower NG
    {
      new_sp.SNG_count++;
      new_sp.consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_info=5;
    }
    else 
    {
      new_sp.consecutive_SNG_count=0;
      if((new_sp.fuzzy_consecutive_SNG_info)>0)
      {
        new_sp.fuzzy_consecutive_SNG_info--;
      }
      else 
        new_sp.fuzzy_consecutive_SNG_count=0;
    }

    
    if(new_rep.value>measuredef.UCL)//upper NG
    {
      new_sp.CNG_count++;
      new_sp.consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_info=5;
    }
    else if(new_rep.value<measuredef.LCL)//upper NG
    {
      new_sp.CNG_count++;
      new_sp.consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_info=5;
    }
    else
    {
      new_sp.consecutive_CNG_count=0;
      if((new_sp.fuzzy_consecutive_CNG_info)>0)
      {
        new_sp.fuzzy_consecutive_CNG_info--;
      }
      else 
        new_sp.fuzzy_consecutive_CNG_count=0;
    }


    if(new_sp.max_consecutive_SNG_count<new_sp.consecutive_SNG_count)
    {
      new_sp.max_consecutive_SNG_count=new_sp.consecutive_SNG_count;
    }
    if(new_sp.max_fuzzy_consecutive_SNG_count<new_sp.fuzzy_consecutive_SNG_count)
    {
      new_sp.max_fuzzy_consecutive_SNG_count=new_sp.fuzzy_consecutive_SNG_count;
    }

    if(new_sp.max_consecutive_CNG_count<new_sp.consecutive_CNG_count)
    {
      new_sp.max_consecutive_CNG_count=new_sp.consecutive_CNG_count;
    }
    if(new_sp.max_fuzzy_consecutive_CNG_count<new_sp.fuzzy_consecutive_CNG_count)
    {
      new_sp.max_fuzzy_consecutive_CNG_count=new_sp.fuzzy_consecutive_CNG_count;
    }
    
    return new_sp;
    // stat.sp
  }

  function statReducer(statistic, report) {

    //if the time is longer than 4s then remove it from matchingWindow
    //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
    statistic.measureList.forEach((measure) => {
      let new_rep = report.judgeReports.find((rep) => rep.id == measure.id);
      //measure.statistic
      let stat = measure.statistic;
      if (new_rep === undefined) {
        stat.count_stat.NA++;
        log.error("The incoming inspection report doesn't match the defFile");
        return;
      }

      if (new_rep.status == INSPECTION_STATUS.NA) {
        stat.count_stat.NA++;
        log.error("The measure[" + new_rep.name + "] is NA");
        return;
      }


      let nv_val = new_rep.value;

      if (stat.count_stat[new_rep.detailStatus] === undefined) {
        stat.count_stat[new_rep.detailStatus] = 0;
      }
      else {
        stat.count_stat[new_rep.detailStatus]++;
      }

      stat.count++;
      stat.sum += nv_val;
      stat.mean = stat.sum / stat.count;
      stat.sqSum += nv_val * nv_val;
      stat.variance = stat.sqSum / stat.count - stat.mean * stat.mean;//E[X^2]-E[X]^2
      stat.sigma = Math.sqrt(stat.variance);
      

      stat.sp=statReducer_sp(stat.sp,measure,new_rep);
      // console.log(new_rep.detailStatus);
      // stat.sp={

      // }


      stat.CPU = (measure.USL - stat.mean) / (3 * stat.sigma);
      stat.CPL = (stat.mean - measure.LSL) / (3 * stat.sigma);
      stat.CP = Math.min(stat.CPU, stat.CPL);
      stat.CK = (stat.mean - measure.value) / ((measure.USL - measure.LSL) / 2);
      stat.CPK = stat.CP * (1 - Math.abs(stat.CK));

      if(!(stat.MIN<=nv_val))//consider MIN=NaN as init state
      {
        stat.MIN=nv_val;
      }

      if(!(stat.MAX>=nv_val))//consider MAX=NaN as init state
      {
        stat.MAX=nv_val;
      }



      stat.histogram = histDataReducer(stat.histogram, nv_val);
      //log.info(stat);

    });

    return statistic;
  }



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
                console.log(action.data);
                newState.edit_info._obj.Setsig360info(action.data);
                newState.edit_info.sig360info = newState.edit_info._obj.sig360info;
              break;
              case "sig360_circle_line":
                {
                  newState.edit_info = { ...newState.edit_info };
                  //newState.report=action.data;
                  let inspReport = report;

                  newState.edit_info.inspReport = inspReport;
                  inspReport.time_ms = currentTime_ms;

                  if (mmpcampix === undefined) {
                    break;
                  }

                  //let overallStat = reportStatisticState.overallStat;


                  

                  let reportStatisticState = newState.edit_info.reportStatisticState;
                  reportStatisticState.reportCount++;

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
                        reportStatisticState.statisticValue = statReducer(reportStatisticState.statisticValue, srep_inWindow);

                        reportStatisticState.historyReport.push(srep_inWindow);//And put it into the historyReport
                        //limit historyReport length to 2000
                        if (reportStatisticState.historyReport.length > statSetting.historyReportlimit) {
                          reportStatisticState.historyReport =
                            reportStatisticState.historyReport.slice(Math.max(reportStatisticState.historyReport.length - 1000, 1));
                        }
                        delete srep_inWindow.headSkipTime;
                        delete srep_inWindow.minReportRepeat;
                        delete srep_inWindow.maxReportRepeat;

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
                    
                    
                    function MarginInfoExtraction(tags,control_margin_info=newState.edit_info.__decorator.control_margin_info)
                    {
                      if(control_margin_info===undefined)return undefined;
                      return tags.reduce((marginInfo,tag)=>
                        (control_margin_info[tag]!==undefined)?
                          control_margin_info[tag]:marginInfo
                        ,undefined);
                    }
                    function resultGrading(judgeReports,marginInfo,fallback_marginInfo)
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
                          newState.edit_info._obj.getMeasure_detailStatus(jud,pfilled_marginInfo);

                          
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
                        if (angleDiff > 2 || angleDiff < -2) {
                          return closeRep;
                        }

                        //Check position consistency
                        let distance = Math.hypot(singleReport.cx - srep_inWindow.cx, singleReport.cy - srep_inWindow.cy);

                        if (distance > mmpcampix*1.5) {
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

                        //log.info(">>>>>",closeRep,singleReport);




                        closeRep.area = valueAveIn(closeRep.area, singleReport.area, closeRep.repeatTime);
                        closeRep.cx = valueAveIn(closeRep.cx, singleReport.cx, closeRep.repeatTime);
                        closeRep.cy = valueAveIn(closeRep.cy, singleReport.cy, closeRep.repeatTime);
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

                        resultGrading(closeRep.judgeReports,cur_MarginInfo,root_MarginInfo);
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

                        
                        resultGrading(treport.judgeReports,cur_MarginInfo,root_MarginInfo);


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
                      reportStatisticState.trackingWindow.forEach(rep=>{
                        rep.isCurObj = false;
                      });
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
            console.log("StatSettingParam_Update", newState.edit_info.statSetting);
            break;

            
          case UISEV.StatInfo_Clear:

          
            console.log("StatInfo_Clear.........");
            newState.edit_info = 
            newState.edit_info._obj.resetStatisticState(newState.edit_info);

            break;
            
          case UISEV.Image_Update:
            newState.edit_info = { ...newState.edit_info, img: action.data };
            break;

          case UI_SM_EVENT.Canvas_Mouse_Location:
            {
              newState.edit_info.mouseLocation = action.data;
            }
            break;

            
          case UISEV.SIG360_Report_Update:
          case UISEV.SIG360_Extraction:
              
              // Edit_info_reset(newState);
              console.log(action.data);
              newState.edit_info._obj.Setsig360info(action.data);
              
              newState.edit_info.sig360info = newState.edit_info._obj.sig360info;
              break;
  
  
          case UISEV.Inspection_Report:
            {
              let reportSkip =false;
              let inspMode=GetObjElement(newState,["machine_custom_setting","InspectionMode"]);
              let uInspResult=GetObjElement(action,["data","uInspResult"]);

              //when in Full inspection mode if the uInspResult(the final result sends to inspection machine)
              //is NA/UNSET(may caused by dirty image/ non-single object...), when means to tell insp mach skip this one
              //so we gonna skip the report to put in(even if there may be a result)
              reportSkip=(inspMode=="FI")&&
                ((uInspResult== INSPECTION_STATUS.NA)  ||  (uInspResult== INSPECTION_STATUS.UNSET))

              
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

          case DefConfAct.EVENT.Shape_List_Update:
            newState.edit_info._obj.SetShapeList(action.data);
            newState.edit_info.edit_tar_info = null;
            newState.edit_info.list = newState.edit_info._obj.shapeList;
            newState.edit_info.__decorator.list_id_order =
              UpdateListIDOrder(newState.edit_info.__decorator.list_id_order, newState.edit_info.list);
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
              newState.edit_info = { ...newState.edit_info, matching_angle_margin_deg: action.data };
              break;
            }

          case DefConfAct.EVENT.Matching_Face_Update:
            {
              newState.edit_info = { ...newState.edit_info, matching_face: action.data };
              break;
            }

          case DefConfAct.EVENT.IntrusionSizeLimitRatio_Update:
            {
              if (typeof action.data == 'number') {
                newState.edit_info = { ...newState.edit_info, intrusionSizeLimitRatio: action.data };
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

              newState.edit_info.__decorator.list_id_order =
                UpdateListIDOrder(action.data, newState.edit_info.list);
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
              console.log(newState.edit_info.__decorator );
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
              newState.edit_info.list = newState.edit_info._obj.shapeList;

              newState.edit_info.__decorator.list_id_order =
                UpdateListIDOrder(newState.edit_info.__decorator.list_id_order, newState.edit_info.list);

              newState.edit_info.inherentShapeList =
                newState.edit_info._obj.UpdateInherentShapeList();

              if (newID !== undefined) {//If this time it's not for adding new shape(ie, newID is not undefined)
                
                let tmpTarIdx =
                  newState.edit_info._obj.FindShapeIdx(newID);
                console.log(tmpTarIdx,"_");
                //log.info(tmpTarIdx);
                if (tmpTarIdx === undefined)//In this case we delete the shape in the list 
                {
                  newState.edit_info.edit_tar_info = null;
                }
                else {//Otherwise, we deepcopy the shape
                  newState.edit_info.edit_tar_info =
                    dclone(newState.edit_info.list[tmpTarIdx]);
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


        switch (substate) {
          case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
            {
              if (newState.edit_info.edit_tar_info == null) {
                /*newState.edit_info.edit_tar_info = {
                  type:SHAPE_TYPE.search_point,
                  pt1:{x:0,y:0},
                  angle:90,
                  margin:10,
                  width:40,
                  ref:[{}]
                };*/
                newState.edit_info.edit_tar_ele_trace = null;
                newState.edit_info.edit_tar_ele_cand = null;
                break;
              }

              if (newState.edit_info.edit_tar_ele_trace != null && newState.edit_info.edit_tar_ele_cand != null) {
                let keyTrace = newState.edit_info.edit_tar_ele_trace;
                let obj = GetObjElement(newState.edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
                let cand = newState.edit_info.edit_tar_ele_cand;

                log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
                obj[keyTrace[keyTrace.length - 1]] = {
                  id: cand.shape.id,
                  type: cand.shape.type
                };

                log.info(obj, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_info = Object.assign({}, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_ele_trace = null;
                newState.edit_info.edit_tar_ele_cand = null;
              }
              break;
            }
          case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
          case UI_SM_STATES.DEFCONF_MODE_AUX_LINE_CREATE:
            {
              if (newState.edit_info.edit_tar_info == null) {
                newState.edit_info.edit_tar_info = {
                  type: (substate == UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE) ?
                    SHAPE_TYPE.aux_point : SHAPE_TYPE.aux_line,
                  ref: [{}, {}]
                };

                newState.edit_info.edit_tar_ele_trace = null;
                newState.edit_info.edit_tar_ele_cand = null;
                break;
              }
              log.info(newState.edit_info.edit_tar_ele_trace, newState.edit_info.edit_tar_ele_cand);

              if (newState.edit_info.edit_tar_ele_trace != null && newState.edit_info.edit_tar_ele_cand != null) {
                let keyTrace = newState.edit_info.edit_tar_ele_trace;
                let obj = GetObjElement(newState.edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
                let cand = newState.edit_info.edit_tar_ele_cand;

                log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
                obj[keyTrace[keyTrace.length - 1]] = {
                  id: cand.shape.id,
                  type: cand.shape.type
                };

                log.info(obj, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_info = Object.assign({}, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_ele_trace = null;
                newState.edit_info.edit_tar_ele_cand = null;
              }
            }
            break;
          case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
            {
              if (newState.edit_info.edit_tar_info == null) {
                newState.edit_info.edit_tar_info = {
                  type: SHAPE_TYPE.measure,
                  subtype: SHAPE_TYPE.measure_subtype.NA,
                  //importance:0,
                  back_value_setup: false
                  //ref:[{},{}]
                };
                newState.edit_info.edit_tar_ele_trace = ["subtype"];
                newState.edit_info.edit_tar_ele_cand = null;
                //break;
              }
              log.info(newState.edit_info.edit_tar_ele_trace, newState.edit_info.edit_tar_ele_cand);

              if (newState.edit_info.edit_tar_ele_trace != null && newState.edit_info.edit_tar_ele_cand != null) {
                let keyTrace = newState.edit_info.edit_tar_ele_trace;
                let obj = GetObjElement(newState.edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
                let cand = newState.edit_info.edit_tar_ele_cand;


                if (keyTrace[0] == "ref" && cand.shape !== undefined) {
                  let acceptData = true;
                  let subtype = newState.edit_info.edit_tar_info.subtype;
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
                      newState.edit_info.edit_tar_info.ref = [{}];
                      break;

                    case SHAPE_TYPE.measure_subtype.circle_info:
                      newState.edit_info.edit_tar_info.ref = [{}];
                      newState.edit_info.edit_tar_info.info_type="NA";
                      break;

                    case SHAPE_TYPE.measure_subtype.calc:
                      newState.edit_info.edit_tar_info.ref = [];
                      newState.edit_info.edit_tar_info.calc_f = {
                        exp: "0",
                        post_exp: ["0"]
                      };
                      break;

                    case SHAPE_TYPE.measure_subtype.distance:
                      newState.edit_info.edit_tar_info.ref = [{}, {}];
                      newState.edit_info.edit_tar_info.ref_baseLine = {};
                      break;
                    case SHAPE_TYPE.measure_subtype.angle:
                      newState.edit_info.edit_tar_info.ref = [{}, {}];
                      break;
                    default:
                      log.info("Error: " + cand + " is not in the measure_subtype list");
                      acceptData = false;
                  }
                  newState.edit_info.edit_tar_info =
                    Object.assign(newState.edit_info.edit_tar_info,
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

                log.info(obj, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_info = Object.assign({}, newState.edit_info.edit_tar_info);
                newState.edit_info.edit_tar_ele_trace = null;
                newState.edit_info.edit_tar_ele_cand = null;
              }
            }
            break;
          case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
            if (newState.edit_info.edit_tar_ele_trace != null && newState.edit_info.edit_tar_ele_cand != null) {
              let keyTrace = newState.edit_info.edit_tar_ele_trace;
              let obj = GetObjElement(newState.edit_info.edit_tar_info, keyTrace, keyTrace.length - 2);
              let cand = newState.edit_info.edit_tar_ele_cand;

              log.info("GetObjElement", obj, keyTrace[keyTrace.length - 1]);
              obj[keyTrace[keyTrace.length - 1]] = {
                id: cand.shape.id,
                type: cand.shape.type
              };

              newState.edit_info.edit_tar_info = Object.assign({}, newState.edit_info.edit_tar_info);
              newState.edit_info.edit_tar_ele_trace = null;
              newState.edit_info.edit_tar_ele_cand = null;
            }
            break;


        }



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