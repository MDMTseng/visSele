
import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';
import {InspectionEditorLogic} from './InspectionEditorLogic';

import * as logX from 'loglevel';
import dclone from 'clone';
import { loadavg } from 'os';

let log = logX.getLogger("UICtrlReducer");

let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;
function Default_UICtrlReducer()
{
  //ST = d;
  //log.info("ST...",JSON.stringify(ST));
  return {
    MENU_EXPEND:false,


    showSplash:true,
    showSM_graph:false,
    WS_CH:undefined,
    edit_info:{
      defModelPath:"data/cache_def",
      _obj:new InspectionEditorLogic(),
      defInfo:[],
      inspReport:undefined,
      reportStatisticState:{
        trackingWindow:[],
        historyReport:[],
        statisticValue:undefined
      },
      sig360report:[],
      img:null,

      list:[],
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
      session_lock:null,
      camera_calibration_report:undefined,
      mouseLocation:undefined
    },
    sm:null,
    c_state:null,
    p_state:null,
    state_count:0,
    WS_ID:"EverCheckWS"
  }
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

  function EVENT_Inspection_Report(newState,action)
  {


    let keepInTrackingTime_ms=4000;
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

    if(action.data.type === "binary_processing_group")
    {
      action.data.reports.forEach((report)=>
      {
        switch(report.type)
        {
          case "sig360_extractor":
          case "sig360_circle_line":
          {
            newState.edit_info=Object.assign({},newState.edit_info);
            //newState.report=action.data;
            newState.edit_info._obj.SetInspectionReport(report);
            let inspReport = newState.edit_info._obj.inspreport;

            newState.edit_info.inspReport = inspReport;
            inspReport.time_ms = currentTime_ms;

            if(mmpcampix===undefined)
            {
              break;
            }

            let reportStatisticState = newState.edit_info.reportStatisticState;


            reportStatisticState.trackingWindow.forEach((srep_inWindow)=>{
              srep_inWindow.isCurObj=false;
              //Reset the current object property, then we will check if there's a new similar report object as it.
            });

            
            reportStatisticState.trackingWindow = //Check if the trackingWindow object is timeout
              reportStatisticState.trackingWindow.filter((srep_inWindow)=>
                {
                  let tdiff = currentTime_ms - srep_inWindow.time_ms;
                  if(tdiff<keepInTrackingTime_ms)
                  {
                    return true;
                  }
                  //if the time is longer than 4s then remove it from matchingWindow
                  //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
                  reportStatisticState.historyReport.push(srep_inWindow);//And put it into the historyReport
                  
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

                if(closeRep !== undefined)
                {
                  //blend the report with the existed report in tracking window  

                  //log.info(">>>>>",closeRep,singleReport);


                  function valueAveIn(ave,new_val,datCount_before)
                  {
                    
                      ave+=(1/(datCount_before+1))*(new_val-ave);
                      return ave;
                  }
                  
                  closeRep.area=valueAveIn(closeRep.area,singleReport.area,closeRep.repeatTime);
                  closeRep.cx=valueAveIn(closeRep.cx,singleReport.cx,closeRep.repeatTime);
                  closeRep.cy=valueAveIn(closeRep.cy,singleReport.cy,closeRep.repeatTime);
                  //closeRep.area+=(1/(closeRep.repeatTime+1))*(sjrep.area-cjrep.area);

                  closeRep.judgeReports.forEach((cjrep)=>{
                    let id = cjrep.id;
                    let sjrep = singleReport.judgeReports.find((sjrep)=>sjrep.id==id);
                    if(sjrep===undefined)return;

                    let dataDiff = sjrep.value-cjrep.value;

                    if(cjrep.subtype===SHAPE_TYPE.measure_subtype.angle)
                    {
                      if(dataDiff>180)dataDiff-=360;
                    }
                    cjrep.value+=(1/(closeRep.repeatTime+1))*(dataDiff);
                  });

                  closeRep.detectedLines.forEach((clrep)=>{
                    let id = clrep.id;
                    let slrep = singleReport.detectedLines.find((slrep)=>slrep.id==id);
                    if(slrep===undefined)return;

                    
                    clrep.cx=valueAveIn(clrep.cx,slrep.cx,closeRep.repeatTime);
                    clrep.cy=valueAveIn(clrep.cy,slrep.cy,closeRep.repeatTime);
                    clrep.vx=valueAveIn(clrep.vx,slrep.vx,closeRep.repeatTime);
                    clrep.vy=valueAveIn(clrep.vy,slrep.vy,closeRep.repeatTime);
                  });

                  
                  closeRep.detectedCircles.forEach((ccrep)=>{
                    let id = ccrep.id;
                    let screp = singleReport.detectedCircles.find((screp)=>screp.id==id);
                    if(screp===undefined)return;
                    //TODO: average the arc info
                    //the arc info uses three points
                    ccrep.x=valueAveIn(ccrep.x,screp.x,closeRep.repeatTime);
                    ccrep.y=valueAveIn(ccrep.y,screp.y,closeRep.repeatTime);
                    ccrep.r=valueAveIn(ccrep.r,screp.r,closeRep.repeatTime);
                    ccrep.s=valueAveIn(ccrep.s,screp.s,closeRep.repeatTime);
                  });

                  
                  closeRep.searchPoints.forEach((ccrep)=>{
                    let id = ccrep.id;
                    let screp = singleReport.searchPoints.find((screp)=>screp.id==id);
                    if(screp===undefined)return;
                    //TODO: average the arc info
                    //the arc info uses three points
                    ccrep.x=valueAveIn(ccrep.x,screp.x,closeRep.repeatTime);
                    ccrep.y=valueAveIn(ccrep.y,screp.y,closeRep.repeatTime);
                  });

                  //closeRep.seq.push(singleReport);//Push current report into the sequence
                  closeRep.time_ms = currentTime_ms;
                  closeRep.repeatTime+=1;
                  closeRep.isCurObj=true;
                }
                else
                {

                  //If there is no report in tracking window similar to the current report
                  //Add into the trackingWindow
                  let treport = dclone(singleReport);
                  treport.time_ms = currentTime_ms;
                  treport.add_time_ms = currentTime_ms;
                  treport.repeatTime=0;
                  //treport.seq=[singleReport];
                  treport.isCurObj=true;
                  reportStatisticState.trackingWindow.push(treport);
                }
              });
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
            log.error(action);
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
      newState.showSplash=false;
      if(action.type===UISEV.Inspection_Report)
      {
        EVENT_Inspection_Report(newState,action);
      }
      return newState;
    case UISTS.DEFCONF_MODE:
    case UISTS.INSP_MODE:
    {


      newState.showSplash=false;
      switch(action.type)
      {
        case UISEV.Image_Update:
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info.img=action.data;
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
          root_defFile.featureSet.forEach((report)=>
          {
            switch(report.type)
            {
              case "sig360_extractor":
              case "sig360_circle_line":
              {

                newState.edit_info=Object.assign({},newState.edit_info);
                newState.edit_info._obj.SetDefInfo(report);
                let reportStatisticState = newState.edit_info.reportStatisticState;

                newState.edit_info.edit_tar_info = null;
                
                newState.edit_info.list=newState.edit_info._obj.shapeList;
                newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
                

                
                //reportStatisticState.statisticValue
                let measureList=
                  dclone(newState.edit_info.list.filter((feature)=>
                    feature.type==SHAPE_TYPE.measure ))
                  .map((feature)=>{
                    feature.statistic={
                      count : 0,
                      //those value should be undefined, but since the count is 0 so the following calc should ignore those value
                      mean: 0,
                      variance: 0,
                      //
                      CP:0,
                      CA:0,
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
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info._obj.SetSig360Report(action.data);
          newState.edit_info.sig360report = newState.edit_info._obj.sig360report;
        break;

        case UISEV.Session_Lock:
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info.session_lock = (action.data);
        break;

        case DefConfAct.EVENT.Shape_List_Update:
          newState.edit_info._obj.SetShapeList(action.data);
          newState.edit_info.edit_tar_info = null;
          newState.edit_info.list=newState.edit_info._obj.shapeList;
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
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.aux_point,
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
              docheck:true
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
                case SHAPE_TYPE.measure_subtype.distance:
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
                    submargin1:0,
                    margin:1
                  });
              if(acceptData)
                obj[keyTrace[keyTrace.length-1]] = cand;
            }

            log.info(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
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
    
    log.debug(newState);
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