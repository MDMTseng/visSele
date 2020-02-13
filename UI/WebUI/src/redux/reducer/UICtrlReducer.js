
import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {xstate_GetCurrentMainState,GetObjElement,isString} from 'UTIL/MISC_Util';
import {InspectionEditorLogic} from './InspectionEditorLogic';

import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';
import APP_INFO from 'JSSRCROOT/info.js';
import * as logX from 'loglevel';
import dclone from 'clone';
import { loadavg } from 'os';

import dateFormat from 'dateFormat';
import JSum from 'jsum';
import semver from 'semver'

let log = logX.getLogger("UICtrlReducer");

let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;

const default_MinRepeatInspReport=3;

function Edit_info_reset(newState)
{
  let empty_edit_info ={
    inspReport:undefined,
    reportStatisticState:{
      trackingWindow:[],
      historyReport:[],
      newAddedReport:[],
      statisticValue:undefined,
    },
    MinRepeatInspReport:default_MinRepeatInspReport,
    sig360info:[],
    matching_angle_margin_deg:180,
    matching_angle_offset_deg:0,
    matching_face:0,
    intrusionSizeLimitRatio:0.001,
    img:null,
    DefFileName:"",
    DefFileTag:[],
    //inspOptionalTag:"",
    DefFileHash:"",
    list:[],
    __decorator:{
      list_id_order:[]
    },
    inherentShapeList:[],
    
    edit_tar_info:null,//It's for usual edit target

    //It's the target element in edit target
    //Example 
    //edit_tar_info={iii:0,a:{b:[x,y,z,c]}}
    //And our goal is to trace to c
    //Then, edit_tar_ele_trace={obj:b, keyHist:["a","b",3]}
    edit_tar_ele_trace:null,

    //This is the cadidate info for target element content
    edit_tar_ele_cand:null,
    //camera_calibration_report:undefined // the camera calibration data shouldn't be reset
    mouseLocation:undefined,
    loadedDefFile:undefined,
  }


  newState.edit_info=Object.assign({},newState.edit_info,empty_edit_info);
  newState.edit_info._obj.reset();
  
  newState.edit_info.edit_tar_info = null;
  
  newState.edit_info.list=newState.edit_info._obj.shapeList;
  newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
  
}


function Default_UICtrlReducer()
{
  //ST = d;
  //log.info("ST...",JSON.stringify(ST));
  let defState = {
    MENU_EXPEND:false,


    showSplash:true,
    showSM_graph:false,
    WS_CH:undefined,
    edit_info:{
      defModelPath:"data/cache_def",
      _obj:new InspectionEditorLogic(),
      inspReport:undefined,
      reportStatisticState:{
        trackingWindow:[],
        historyReport:[],
        newAddedReport:[],
        statisticValue:undefined,
        overallStat:{
          OK:0,
          WARN:0,
          NG:0,
          lastTS:0,
          T:0,
          soft_T:0,
          softIdx:0.1
        }
      },
      sig360info:[],
      matching_angle_margin_deg:180,
      matching_angle_offset_deg:0,
      matching_face:0,//-1 for back(flipped ) face only, 0 for front and back face, 1 for front face only
      img:null,
      DefFileName:"",
      DefFileTag:[],
      inspOptionalTag:[],
      list:[],
      __decorator:{
        listIDOrder:[]
      },
      inherentShapeList:[],

      edit_tar_info:null,//It's for usual edit target

      //It's the target element in edit target
      //Example 
      //edit_tar_info={iii:0,a:{b:[x,y,z,c]}}
      //And our target is c
      //Then, edit_tar_ele_trace={obj:b, keyHist:["a","b",3]}
      edit_tar_ele_trace:null,

      //This is the cadidate info for target element content
      edit_tar_ele_cand:null,
      camera_calibration_report:undefined,
      mouseLocation:undefined
    },
    version_map_info:undefined,
    WebUI_info:APP_INFO,
    sm:null,
    c_state:null,
    p_state:null,
    state_count:0,
    WS_ID:"EverCheckWS",
    InspectionMonitor_URL:"http://gitexp.idcircle.me/hyvision_monitor/0.0.0/",
  }

  Edit_info_reset(defState);
  return defState;
}




function UpdateListIDOrder(cur_listIDOrder,list)
{
  //remove disappeared shape id
  let listIDOrder=cur_listIDOrder.filter(id=>list.find(shape=>shape.id==id));

  let newIDs = list.//find new IDs to add in
    filter(shape=>listIDOrder.find(id=>id==shape.id)===undefined).
    map(shape=>shape.id);

  listIDOrder=[...listIDOrder,...newIDs];
  return listIDOrder;
}


function StateReducer(newState,action)
{
  newState.state_count++;
  if(action.type == "ev_state_update")
  {
    newState.c_state = action.data.c_state;
    newState.p_state = action.data.p_state;
    newState.sm = action.data.sm;
    newState.state_count=0;
    log.info(newState.p_state.value," + ",action.data.action," > ",newState.c_state.value);


  }
  
  if (action.type === UISEV.Control_SM_Panel) {
    newState.showSM_graph = action.data;
    return newState;
  }

  switch(action.type)
  {
    case UISEV.Connected:
      newState.WS_CH=action.data;
      log.info("Connected",newState.WS_CH);
    return newState;
    
    case UISEV.Version_Map_Update:
      log.info("Version_Map_Update",action.data);
      let version_map_info = action.data;
      version_map_info.webUI_info=APP_INFO;
      
      {
        let core_info = version_map_info.core_info;
        version_map_info.recommend_ver={};
        let coreVersion=core_info.version;
        if(coreVersion===undefined)
        {
          coreVersion="0.0.0";
        }
        let WebUI_Version = version_map_info.core2wui[coreVersion];
        
        if(WebUI_Version!==undefined && WebUI_Version.ver !== undefined)
        {
          let versions = WebUI_Version.ver;

          let webUI_resource="http://hyv.idcircle.me";
          if(core_info.webUI_resource !== undefined)webUI_resource=core_info.webUI_resource;
        
          let localV=semver.clean(APP_INFO.version);
          
          let maxV = versions
            .map(ver=>semver.clean(ver))
            .reduce((maxV,ver)=>semver.gt(maxV, ver)?maxV:ver);
          let hasNewVer = semver.gt(maxV, localV);
          
          if(hasNewVer)
          {
            version_map_info.recommend_info={
              versions,
              url:webUI_resource+"/"+versions[versions.length-1]
            }
          }
        }

      }
      newState={...newState,version_map_info};
    return newState;
    
    case UISEV.Disonnected:
      newState.WS_CH=undefined;
    return newState;

    case UISEV.Control_SM_Panel:
      newState.showSM_graph = action.data;
    return newState;
  }
  switch(newState.c_state.value)
  {
    case UISTS.SPLASH:
      newState.showSplash=true;
      return newState;
  }

  let stateObj = xstate_GetCurrentMainState(newState.c_state);
  let substate = stateObj.substate;

  function histDataReducer(histoInfo,dataValue)
  {
    if(dataValue<histoInfo.xmin)
    {
      histoInfo.histo[0]++;
      return histoInfo;
    }
    if(dataValue>histoInfo.xmax)
    {
      histoInfo.histo[histoInfo.histo.length-1]++;
      return histoInfo;
    }
    let dataRegion=histoInfo.histo.length-2;//The first value and last value are the value excced xmin& xmax

    //If the data is in the boundary then there must be a position for it.
    let val_idx=Math.floor(dataRegion*(dataValue-histoInfo.xmin)/(histoInfo.xmax-histoInfo.xmin));
    //Suppose xmax=21 xmin=20 dataRegion=4(index 0~3)
    //idx = floor(dataRegion*(value-min)/(max-min+1));
    //=>value=21   (4*(21-20)/(21-20))=>4/1=4  :on the edge case the idx might hit the boundary
    //=>value=20   (4*(20-20)/(21-20))=>0/1=0 


    //Suppose xmax=20 xmin=10 dataRegion=14(index 0~13)
    //=>value=214   (13*(14-10)/(20-10))=>13*4/10=5.2  :on the edge case the idx might hit the boundary
    //=>value=19    (13*(19-10)/(20-10))=>13*9/10=117/10=11.7
    //=>value=19.9    (13*(19.9-10)/(20-10))=>13*9.9/10=117/10=12.87

    if(val_idx>=dataRegion)val_idx=dataRegion-1;//Just in case it hits upper boundary;
    histoInfo.histo[val_idx+1]++;//with 1 padding for lower over boundary data

    return histoInfo;
  }


  function statReducer(statistic,report)
  {
    
    //if the time is longer than 4s then remove it from matchingWindow
    //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
    statistic.measureList.forEach((measure)=>{
      let new_rep = report.judgeReports.find((rep)=>rep.id == measure.id);
      //measure.statistic
      let stat = measure.statistic;
      if(new_rep === undefined)
      {
        stat.count_stat.NA++;
        log.error("The incoming inspection report doesn't match the defFile");
        return;
      }

      if(new_rep.status == INSPECTION_STATUS.NA)
      {
        stat.count_stat.NA++;
        log.error("The measure["+new_rep.name+"] is NA");
        return;
      }


      let nv_val = new_rep.value;

      if(stat.count_stat[new_rep.detailStatus]===undefined)
      {
        stat.count_stat[new_rep.detailStatus]=0;
      }
      else
      {
        stat.count_stat[new_rep.detailStatus]++;
      }
      
      stat.count++;
      stat.sum+=nv_val;
      stat.mean = stat.sum/stat.count;
      stat.sqSum+=nv_val*nv_val;
      stat.variance = stat.sqSum/stat.count-stat.mean*stat.mean;//E[X^2]-E[X]^2
      stat.sigma = Math.sqrt(stat.variance);


      stat.CPU = (measure.USL-stat.mean)/(3*stat.sigma);
      stat.CPL = (stat.mean-measure.LSL)/(3*stat.sigma);
      stat.CP = Math.min(stat.CPU,stat.CPL);
      stat.CK = (stat.mean-measure.value)/((measure.USL-measure.LSL)/2);
      stat.CPK = stat.CP*(1-Math.abs(stat.CK));

      
      stat.histogram = histDataReducer(stat.histogram,nv_val);
      //log.info(stat);
    
    });
    return statistic;
  }



  function EVENT_Inspection_Report(newState,action)
  {
    let keepInTrackingTime_ms=3000;
    let MinRepeatInspReport = newState.edit_info.MinRepeatInspReport;
    let inspOptionalTag = newState.edit_info.DefFileTag+","+newState.edit_info.inspOptionalTag;
    let currentDate = action.date;
    let currentTime_ms = currentDate.getTime();

    let camParam = newState.edit_info._obj.cameraParam;
   // let mmpcampix = newState..cameraParam.mmpb2b/this.db_obj.cameraParam.ppb2b;
    
    let mmpcampix ;
    if(camParam===undefined)
    {
      mmpcampix=undefined;
    }
    else
    {
      mmpcampix = camParam.mmpb2b/camParam.ppb2b;
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

    if(action.data.type === "binary_processing_group")
    {
      action.data.reports.forEach((report)=>
      {
        switch(report.type)
        {
          case "sig360_extractor":
          case "sig360_circle_line":
          {
            newState.edit_info={...newState.edit_info};
            //newState.report=action.data;
            newState.edit_info._obj.SetInspectionReport(report);

            let inspReport = newState.edit_info._obj.inspreport;

            newState.edit_info.inspReport = inspReport;
            inspReport.time_ms = currentTime_ms;

            if(mmpcampix===undefined)
            {
              break;
            }
            
            //let overallStat = reportStatisticState.overallStat;




            let reportStatisticState = newState.edit_info.reportStatisticState;

            reportStatisticState.newAddedReport=[];
 
            //Reset the current object property, then we will check if there's a new similar report object as it.
            reportStatisticState.trackingWindow.forEach((srep_inWindow)=>{
              srep_inWindow.isCurObj=false;
             });

            //Check if the trackingWindow object is timeout(from tracking window)
            reportStatisticState.trackingWindow = 
              reportStatisticState.trackingWindow.filter((srep_inWindow)=>
                {
                  let tdiff = currentTime_ms - srep_inWindow.time_ms;
                  if(tdiff<keepInTrackingTime_ms)
                  {
                    return true;
                  }
                  //if the time is longer than 4s then remove it from matchingWindow
                  //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
                  if(srep_inWindow.repeatTime>2 && srep_inWindow.headSkipTime==0)
                  {
                    reportStatisticState.statisticValue = statReducer(reportStatisticState.statisticValue,srep_inWindow);
                    
                    reportStatisticState.historyReport.push(srep_inWindow);//And put it into the historyReport
                    //limit historyReport length to 2000
                    if(reportStatisticState.historyReport.length>2000)
                    {
                      reportStatisticState.historyReport=
                      reportStatisticState.historyReport.slice(Math.max(reportStatisticState.historyReport.length - 1000, 1));
                    }
                    reportStatisticState.newAddedReport.push(srep_inWindow);
                  }
                  else
                  {
                    log.error("the current data only gets few samples, ignore",
                    "this error case is to remove abnormal sample that's caused by air blow");
                    log.error("repeatTime:",srep_inWindow.repeatTime)
                    log.error("headSkipTime:",srep_inWindow.headSkipTime)
                  }
                  return false;
                });
            
            if(inspReport.reports === undefined)
            {
              break;
            }
            
            {//Do matching in tracking_window


              //new inspection report >
              //  [update/insert]> tracking_window >
              //     [if no update after 4s]> historyReport
              inspReport.reports.forEach((singleReport)=>{
                
                let closeRep = reportStatisticState.trackingWindow.reduce((closeRep,srep_inWindow)=>{
                  if(closeRep!==undefined)return closeRep;
                  //Check direction consistency
                  if(singleReport.isFlipped != srep_inWindow.isFlipped)
                  {
                    return closeRep;
                  }

                  //Check area consistency
                  let areaDiff = singleReport.area/srep_inWindow.area;
                  if(areaDiff>1.2 || areaDiff < 1/1.2)
                  {
                    return closeRep;
                  }

                  //Check retation consistency
                  let angleDiff = singleReport.rotate - srep_inWindow.rotate;
                  if(angleDiff>180)angleDiff=angleDiff-360;
                  if(angleDiff>5 || angleDiff<-5)
                  {
                    return closeRep;
                  }

                  //Check position consistency
                  let distance = Math.hypot(singleReport.cx - srep_inWindow.cx,singleReport.cy - srep_inWindow.cy);
                
                  if(distance>mmpcampix/2)
                  {
                    return closeRep;
                  }
                  //If we get here, which means the information is very similar.
                  //return/mark the current object as same report object
                  return srep_inWindow;
                },undefined);
                function valueAveIn(ave,new_val,datCount_before)
                {

                  ave+=(1/(datCount_before+1))*(new_val-ave);
                  return ave;
                }
                if(closeRep !== undefined)
                {
                  //blend the report with the existed report in tracking window  

                  //log.info(">>>>>",closeRep,singleReport);



                  
                  closeRep.area=valueAveIn(closeRep.area,singleReport.area,closeRep.repeatTime);
                  closeRep.cx=valueAveIn(closeRep.cx,singleReport.cx,closeRep.repeatTime);
                  closeRep.cy=valueAveIn(closeRep.cy,singleReport.cy,closeRep.repeatTime);
                  //closeRep.area+=(1/(closeRep.repeatTime+1))*(sjrep.area-cjrep.area);

                  closeRep.detectedLines.forEach((clrep)=>{
                    
                    if(clrep.status==INSPECTION_STATUS.NA)return;
                    let id = clrep.id;
                    let slrep = singleReport.detectedLines.find((slrep)=>slrep.id==id);
                    if(slrep===undefined || slrep.status==INSPECTION_STATUS.NA)
                    {
                      clrep.status = INSPECTION_STATUS.NA;
                      return;
                    }
                    
                    clrep.cx=valueAveIn(clrep.cx,slrep.cx,closeRep.repeatTime);
                    clrep.cy=valueAveIn(clrep.cy,slrep.cy,closeRep.repeatTime);
                    clrep.vx=valueAveIn(clrep.vx,slrep.vx,closeRep.repeatTime);
                    clrep.vy=valueAveIn(clrep.vy,slrep.vy,closeRep.repeatTime);
                  });

                  
                  closeRep.detectedCircles.forEach((ccrep)=>{
                    if(ccrep.status==INSPECTION_STATUS.NA)return;
                    let id = ccrep.id;
                    let screp = singleReport.detectedCircles.find((screp)=>screp.id==id);
                    if(screp===undefined || screp.status==INSPECTION_STATUS.NA)
                    {
                      ccrep.status = INSPECTION_STATUS.NA;
                      return;
                    }
                    //TODO: average the arc info
                    //the arc info uses three points
                    ccrep.x=valueAveIn(ccrep.x,screp.x,closeRep.repeatTime);
                    ccrep.y=valueAveIn(ccrep.y,screp.y,closeRep.repeatTime);
                    ccrep.r=valueAveIn(ccrep.r,screp.r,closeRep.repeatTime);
                    ccrep.s=valueAveIn(ccrep.s,screp.s,closeRep.repeatTime);
                  });

                  
                  closeRep.searchPoints.forEach((ccrep)=>{
                    if(ccrep.status==INSPECTION_STATUS.NA)return;
                    let id = ccrep.id;
                    let screp = singleReport.searchPoints.find((screp)=>screp.id==id);
                    if(screp===undefined || screp.status==INSPECTION_STATUS.NA)
                    {
                      ccrep.status = INSPECTION_STATUS.NA;
                      return;
                    }
                    //TODO: average the arc info
                    //the arc info uses three points
                    ccrep.x=valueAveIn(ccrep.x,screp.x,closeRep.repeatTime);
                    ccrep.y=valueAveIn(ccrep.y,screp.y,closeRep.repeatTime);
                  });

                  closeRep.judgeReports.forEach((cjrep)=>{
                    
                    if(cjrep==INSPECTION_STATUS.NA)
                    {
                      cjrep.value=NaN;
                      return;
                    }
                    let id = cjrep.id;
                    let sjrep = singleReport.judgeReports.find((sjrep)=>sjrep.id==id);
                    if(sjrep===undefined)return;
                    if(sjrep.status==INSPECTION_STATUS.NA)
                    {
                      cjrep.status=INSPECTION_STATUS.NA;
                      cjrep.value=NaN;
                      return;
                    }


                    if(cjrep.value==cjrep.value)//if original value is NOT NAN
                    {
                      let dataDiff = sjrep.value-cjrep.value;
                      if(cjrep.subtype===SHAPE_TYPE.measure_subtype.angle)
                      {
                        if(dataDiff>Math.PI)dataDiff-=Math.PI;
                        if(dataDiff<-Math.PI)dataDiff+=Math.PI;
                        //console.log(dataDiff);
                      }
                      if(dataDiff==dataDiff)
                        cjrep.value+=(1/(closeRep.repeatTime+1))*(dataDiff);
  
                    }
                    else//if original value is NAN
                      cjrep.value=sjrep.value;

                    let defInfo = newState.edit_info.list;
                    let sj_def = defInfo.find((sj_def)=>sj_def.id==id);
                    if(sj_def===undefined)return;




                    if(cjrep.value !== cjrep.value)//NAN
                    {
                      cjrep.status=INSPECTION_STATUS.NA;
                    }
                    else if(cjrep.value<sj_def.USL && cjrep.value>sj_def.LSL)
                    {
                      cjrep.status=INSPECTION_STATUS.SUCCESS;
                    }
                    else
                    {
                      cjrep.status=INSPECTION_STATUS.FAILURE;
                    }
                  });

                  //closeRep.seq.push(singleReport);//Push current report into the sequence
                  closeRep.time_ms = currentTime_ms;
                  closeRep.repeatTime+=1;
                  if(closeRep.headSkipTime>0)
                  {
                    closeRep.headSkipTime--;
                    //When down to zero, reset repeatTime
                    //Zero repeatTime will let next incoming data to overwrite current data
                    if(closeRep.headSkipTime==0)
                    {
                      closeRep.repeatTime=0;
                    }
                  }
                  closeRep.isCurObj=true;
                }
                else
                {

                  //If there is no report in tracking window similar to the current report
                  //Add into the trackingWindow
                  let treport = dclone(singleReport);
                  treport.time_ms = currentTime_ms;
                  treport.add_time_ms = currentTime_ms;
                  treport.subFeatureDefSha1=subFeatureDefSha1;
                  treport.tag=inspOptionalTag;
                  treport.machine_hash=machine_hash;
                  treport.repeatTime=1;
                  treport.headSkipTime=3;
                  //treport.seq=[singleReport];
                  treport.isCurObj=true;
                  reportStatisticState.trackingWindow.push(treport);
                }


              });

              //Remove the non-Current object with repeatTime<=1, which suggests it's a noise
              //In other word, in order to stay, you need to be a CurObj/ repeatTime>2
              reportStatisticState.trackingWindow=
                reportStatisticState.trackingWindow.
                filter((srep_inWindow)=>(srep_inWindow.isCurObj || srep_inWindow.repeatTime>=MinRepeatInspReport));
            }

            
            if(false){
              let reportGroup = newState.edit_info.inspReport.reports[0].reports.map(report=>report.judgeReports);
              let measure1 = newState.edit_info.reportStatisticState.measure1;
              if(measure1 === undefined)measure1=[];
              measure1.push({
                genre: "G"+Math.random(), sold:Math.random()
              })
              if(measure1.length>20)measure1.shift();
              newState.edit_info.reportStatisticState=Object.assign({},
                newState.edit_info.reportStatisticState,
                {
                  measure1:measure1
                });
              ;
            }
          }
          break;
          case "camera_calibration":
            if(report.error!==undefined &&report.error == 0)
            {
              newState.edit_info._obj.SetCameraParamInfo(report);
              newState.edit_info.camera_calibration_report = action.data;
            }
            else
            {
              newState.edit_info._obj.SetCameraParamInfo(undefined);
              newState.edit_info.camera_calibration_report = undefined;
            }
          break;
        }
        
      });
    }
    //newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
  }


  switch(stateObj.state)
  {
    case UISTS.SPLASH:
      newState.showSplash=true;
      return newState;
    case UISTS.MAIN:
    case UISTS.DEFCONF_MODE:
    case UISTS.INSP_MODE:
    {


      newState.showSplash=false;
      switch(action.type)
      {

        case UISEV.MinRepeatInspReport_Update:
          let MinRepeatInspReport = 
            (action.data===undefined)?default_MinRepeatInspReport:action.data;
          newState.edit_info={...newState.edit_info,MinRepeatInspReport};
        break;
        case UISEV.Image_Update:
          newState.edit_info={...newState.edit_info,img:action.data};
        break;

        case UISEV.Def_Model_Path_Update:
          newState.edit_info={...newState.edit_info,defModelPath:action.data};
          //Edit_info_reset(newState);
        break;
        case UI_SM_EVENT.Canvas_Mouse_Location:
        {
          newState.edit_info.mouseLocation = action.data;
        }
        break;
        case UISEV.Inspection_Report:
        {
          EVENT_Inspection_Report(newState,action);
        }
        break;

        case UISEV.Define_File_Update:
        let root_defFile=action.data;
        
        if(root_defFile.type === "binary_processing_group")
        {
          let doExit=false;
          let clone_featureSet=dclone(root_defFile.featureSet);
          clone_featureSet.forEach((feature)=>{//we ignore the key that starts with "__", two "_"
            Object.keys(feature).
              filter(key=>key.startsWith("__")).
              forEach((keyW__)=>delete feature[keyW__]);
          })
          

          let sha1_info_in_json = JSum.digest(clone_featureSet, 'sha1', 'hex');
          if(root_defFile.featureSet_sha1!==undefined)//If there is a saved sha1, check integrity 
          {
            let sha1_info_in_file = root_defFile.featureSet_sha1;
            if(sha1_info_in_file !== sha1_info_in_json)
            {
              doExit=true;
            }
          }
          
          /*if(newState.edit_info.DefFileHash==sha1_info_in_json)
          {
            //No need to wipe out the data;
            break;
          }*/
          Edit_info_reset(newState);


          if(doExit)
          {
            newState.edit_info.DefFileHash=undefined;
            break;
          }
          //console.log(dclone(newState.edit_info))
          newState.edit_info.DefFileHash=sha1_info_in_json;

          if(root_defFile.name === undefined)
          {
            var now = new Date();
            var time = dateFormat(now, "yyyymmdd_HHMMss");
            newState.edit_info.DefFileName="Sample_"+time;
          }
          else
          {
            newState.edit_info.DefFileName   = root_defFile.name;
          }

          if(root_defFile.tag !== undefined)
          {
            let tagInfo = root_defFile.tag;
            if(isString(tagInfo))
              tagInfo=root_defFile.tag.split(",");
              
            if(Array.isArray(tagInfo))
            {
              newState.edit_info.DefFileTag=tagInfo;
            }
          }
          

          newState.edit_info.loadedDefFile=dclone(root_defFile);

          
          if(typeof root_defFile.intrusionSizeLimitRatio == 'number')
          {
            newState.edit_info.intrusionSizeLimitRatio = 
              root_defFile.intrusionSizeLimitRatio
          }

          root_defFile.featureSet.forEach((report)=>
          {
            switch(report.type)
            {
              case "sig360_extractor":
              case "sig360_circle_line":
              {
                if(report.matching_angle_margin_deg!==undefined)
                  newState.edit_info.matching_angle_margin_deg=report.matching_angle_margin_deg;
                if(report.matching_angle_offset_deg!==undefined)
                  newState.edit_info.matching_angle_offset_deg=report.matching_angle_offset_deg;
                if(report.matching_face!==undefined)
                  newState.edit_info.matching_face=report.matching_face;
          

                newState.edit_info=Object.assign({},newState.edit_info);

                newState.edit_info._obj.SetDefInfo(report);

                let reportStatisticState = newState.edit_info.reportStatisticState;

                newState.edit_info.edit_tar_info = null;
                
                newState.edit_info.list=newState.edit_info._obj.shapeList;
                newState.edit_info.__decorator={...newState.edit_info.__decorator,...report.__decorator};
                
                newState.edit_info.__decorator.list_id_order=
                  UpdateListIDOrder(newState.edit_info.__decorator.list_id_order,newState.edit_info.list);
                
                newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
                
                log.info(newState.edit_info.inherentShapeList);

                //reportStatisticState.statisticValue
                let measureList=
                  dclone(newState.edit_info.list.filter((feature)=>
                    feature.type==SHAPE_TYPE.measure ))
                  .map((feature)=>{
                    console.log(feature);
                    feature.statistic={
                      count_stat:
                      {
                        NA:0,
                        UOK:0,
                        LOK:0,
                        
                        UCNG:0,
                        LCNG:0,

                        USNG:0,
                        LSNG:0,
                      },
                      histogram:{
                        xmin:1.2*(feature.LSL-feature.value)+feature.value,
                        xmax:1.2*(feature.USL-feature.value)+feature.value,
                        histo:new Array(502).fill(0)//The first value and last value are the value excced xmin& xmax
                      },
                      count : 0,
                      //those value should be undefined, but since the count is 0 so the following calc should ignore those value
                      sum:0,
                      sqSum:0,//E[X^2]*count
                      mean: 0,//E[X]*count
                      variance: 0,//E[X^2]-E[X]^2
                      //deviation = Sigma = sqrt(variance)
                      sigma: 0,
                      //
                      CP:0,
                      CK:0,
                      CPU:0,
                      CPL:0,
                      CPK:0,
                    }
                    return feature;
                  });
                  reportStatisticState.statisticValue={
                    
                    measureList:measureList
                  }
                log.info(reportStatisticState.statisticValue);
              }
              break;
              case "camera_calibration":

                log.error(action);
                /*if(report.error!==undefined &&report.error == 0)
                {
                  newState.edit_info.camera_calibration_report = root_report;
                }
                else
                {
                  newState.edit_info.camera_calibration_report = undefined;
                }*/
              break;
            }
            
          });
        }
        break;
        case UISEV.SIG360_Report_Update:
        case UISEV.SIG360_Extraction:
          newState.edit_info=Object.assign({},newState.edit_info);
          console.log(action.data);
          newState.edit_info._obj.Setsig360info(action.data);
          newState.edit_info.sig360info = newState.edit_info._obj.sig360info;
        break;



        case DefConfAct.EVENT.Shape_List_Update:
          newState.edit_info._obj.SetShapeList(action.data);
          newState.edit_info.edit_tar_info = null;
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          newState.edit_info.__decorator.list_id_order=
            UpdateListIDOrder(newState.edit_info.__decorator.list_id_order,newState.edit_info.list);
          newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
        break;

        case DefConfAct.EVENT.Edit_Tar_Update:
          newState.edit_info.edit_tar_info=
            (action.data == null)? null : Object.assign({},action.data);
          
          newState.edit_info.edit_tar_ele_trace=null;
          newState.edit_info.edit_tar_ele_cand=null;
        break;
        case DefConfAct.EVENT.Edit_Tar_Ele_Trace_Update:
          newState.edit_info.edit_tar_ele_trace=
            (action.data == null)? null : action.data.slice();
        break;
        case UISEV.EC_Save_Def_Config:
        {
          if(newState.WS_CH==undefined)break;
        }
        break;
        case DefConfAct.EVENT.Edit_Tar_Ele_Cand_Update:
          newState.edit_info.edit_tar_ele_cand=
            (action.data == null)? null :(action.data instanceof Object)? Object.assign({},action.data):action.data;
            log.info("DEFCONF_MODE_Edit_Tar_Ele_Cand_Update",newState.edit_info.edit_tar_ele_cand);
        break;

        case DefConfAct.EVENT.DefFileName_Update:
        {
          newState.edit_info=Object.assign({},newState.edit_info,{DefFileName:action.data});
          break;
        }

        case DefConfAct.EVENT.DefFileTag_Update:
        {
          newState.edit_info={...newState.edit_info,DefFileTag:action.data};
          break;
        }


        case DefConfAct.EVENT.Matching_Angle_Margin_Deg_Update:
        {
          newState.edit_info={...newState.edit_info,matching_angle_margin_deg:action.data};
          break;
        }
        
        case DefConfAct.EVENT.Matching_Face_Update:
        {
          newState.edit_info={...newState.edit_info,matching_face:action.data};
          break;
        }

        case DefConfAct.EVENT.IntrusionSizeLimitRatio_Update:
        {
          if(typeof action.data == 'number')
          {
            newState.edit_info={...newState.edit_info,intrusionSizeLimitRatio:action.data};
          }
          break;
        }
        
        
        case DefConfAct.EVENT.InspOptionalTag_Update:
        {
          let inspOptionalTag = action.data;
          let tags=inspOptionalTag;

          tags = tags.filter((check_tag,check_idx)=>{
            if(check_tag.length==0)return false;
            for(let ii=0;ii<check_idx;ii++)
            {
              if(tags[ii]==check_tag)return false;
            }
            return true;
          });//Chekc duplication and remove empty tag
          inspOptionalTag=tags;
          newState.edit_info={...newState.edit_info,inspOptionalTag};
          break;
        }
        case DefConfAct.EVENT.Shape_Decoration_ID_Order_Update:
        {
          log.info("action.data:",action.data);
          
          newState.edit_info.__decorator.list_id_order=
            UpdateListIDOrder(action.data,newState.edit_info.list);
        }

        case DefConfAct.EVENT.Shape_Set:
        {
          //Three cases
          //ID undefined but shaped is defiend -Add new shape
          //ID is defined and shaped is defiend - Modify an existed shape if it's in the list
          //ID is defined and shaped is null   - delete  an existed shape if it's in the list

          let newID=action.data.id;
          log.info("newID:",newID);
          let shape = newState.edit_info._obj.SetShape(action.data.shape,newID);
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          
          newState.edit_info.__decorator.list_id_order=
            UpdateListIDOrder(newState.edit_info.__decorator.list_id_order,newState.edit_info.list);
          
          newState.edit_info.inherentShapeList=
            newState.edit_info._obj.UpdateInherentShapeList();
          if(newID!==undefined)
          {//If this time it's not for adding new shape(ie, newID is not undefined)
            let tmpTarIdx=
            newState.edit_info._obj.FindShapeIdx( newID );
            log.info(tmpTarIdx);
            if(tmpTarIdx === undefined)//In this case we delete the shape in the list 
            {
              newState.edit_info.edit_tar_info=null;
            }
            else
            {//Otherwise, we deepcopy the shape
              newState.edit_info.edit_tar_info = 
                dclone(newState.edit_info.list[tmpTarIdx]);
            }
          }
          else
          {//We just added a shape, set it as an edit target
            newState.edit_info.edit_tar_info = 
              dclone(shape);
          }

          newState.edit_info=Object.assign({},newState.edit_info);
        }
        break;
      }

      
      switch(substate)
      {
        case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            /*newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.search_point,
              pt1:{x:0,y:0},
              angle:90,
              margin:10,
              width:40,
              ref:[{}]
            };*/
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
            break;
          }
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            log.info("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            log.info(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
          break;
        }
        case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
        case UI_SM_STATES.DEFCONF_MODE_AUX_LINE_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            newState.edit_info.edit_tar_info = {
              type:(substate==UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE)?
                SHAPE_TYPE.aux_point:SHAPE_TYPE.aux_line,
              ref:[{},{}]
            };
            
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
            break;
          }
          log.info(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            log.info("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            log.info(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
        }
        break;
        case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.measure,
              subtype:SHAPE_TYPE.measure_subtype.NA,
              docheck:true,
              back_value_setup:false
              //ref:[{},{}]
            };
            newState.edit_info.edit_tar_ele_trace=["subtype"];
            newState.edit_info.edit_tar_ele_cand=null;
            //break;
          }
          log.info(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;
            
            
            if(keyTrace[0]=="ref" && cand.shape!==undefined)
            {
              let acceptData=true;
              let subtype = newState.edit_info.edit_tar_info.subtype;
              switch(subtype)
              {
                case SHAPE_TYPE.measure_subtype.sigma:break;
                case SHAPE_TYPE.measure_subtype.distance://No specific requirement
                  if(cand.shape.type==SHAPE_TYPE.search_point || 
                    cand.shape.type==SHAPE_TYPE.aux_point || 
                    cand.shape.type==SHAPE_TYPE.arc )
                  {
                    //We allow these three
                  }
                  else if(cand.shape.type==SHAPE_TYPE.line)
                  {//Might need to check the angle if both are lines

                  }
                  else
                  {
                    log.info("Error: "+ subtype+ 
                      " doesn't accept "+cand.shape.type);
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.radius://Has to be an arc
                  if(cand.shape.type!=SHAPE_TYPE.arc)
                  {
                    log.info("Error: "+ subtype+ 
                      " Only accepts arc");
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.angle://Has to be an line to measure
                if(cand.shape.type!=SHAPE_TYPE.line)
                {
                  log.info("Error: "+ subtype+ 
                    " Only accepts line");
                  acceptData=false;
                }
                break;
                
                case SHAPE_TYPE.measure_subtype.calc://Has to be an line to measure
                if(cand.shape.type!=SHAPE_TYPE.measure)
                {
                  log.info("Error: "+ subtype+ 
                    " Only accepts measure");
                  acceptData=false;
                }
              break;
                default :
                  log.info("Error: "+ subtype+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              if(acceptData)
              {
                log.info("GetObjElement",obj,keyTrace[keyTrace.length-1]);
                obj[keyTrace[keyTrace.length-1]]={
                  id:cand.shape.id,
                  type:cand.shape.type
                };
              }
            }
            else if(keyTrace[0] == "subtype")
            {
              let acceptData=true;
              switch(cand)
              {
                case SHAPE_TYPE.measure_subtype.sigma:
                case SHAPE_TYPE.measure_subtype.radius:
                  newState.edit_info.edit_tar_info.ref=[{}];
                break;
                
                case SHAPE_TYPE.measure_subtype.calc:
                  newState.edit_info.edit_tar_info.ref=[{},{},{},{}];
                  newState.edit_info.edit_tar_info.calc_f={
                    exp:"",
                    post_exp:[]
                  };
                break;

                case SHAPE_TYPE.measure_subtype.distance:
                    newState.edit_info.edit_tar_info.ref=[{},{}];
                    newState.edit_info.edit_tar_info.ref_baseLine={};
                  break;
                case SHAPE_TYPE.measure_subtype.angle:
                  newState.edit_info.edit_tar_info.ref=[{},{}];
                break;
                default :
                  log.info("Error: "+ cand+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              newState.edit_info.edit_tar_info = 
                Object.assign(newState.edit_info.edit_tar_info,
                  {
                    pt1:{x:0,y:0},
                    value:0,
                    USL:0,
                    LSL:0,
                    UCL:0,
                    LCL:0,
                  });
              if(acceptData)
                obj[keyTrace[keyTrace.length-1]] = cand;
            }
            else if(keyTrace[0] == "ref_baseLine")
            {
              obj[keyTrace[keyTrace.length-1]]={
                id:cand.shape.id,
                type:cand.shape.type
              };
            }

            log.info(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
        }
        break;
        case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            log.info("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
        break;

        
      }



      return newState;
    }
  }
  return newState;
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

  
  if(action.type === undefined || action.type.includes("@@redux/"))return state;
  let newState = Object.assign({},state);

  var d = new Date();

  if(action.type==="ATBundle")
  {
    newState = action.data.reduce((state,action)=>{
      action.date=d;
      return StateReducer(state,action);
    },newState);
    
    return newState;
  }
  else
  {
    action.date=d;
    newState = StateReducer(newState,action);
    log.debug(newState);
    return newState;
  }

  return newState;
}
export default UICtrlReducer