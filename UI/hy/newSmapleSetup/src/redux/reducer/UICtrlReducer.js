
import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';
import {InspectionEditorLogic} from './InspectionEditorLogic';

let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;
function Default_UICtrlReducer()
{
  //ST = d;
  //console.log("ST...",JSON.stringify(ST));
  return {
    MENU_EXPEND:false,


    showSplash:true,
    showSM_graph:false,
    WS_CH:undefined,
    edit_info:{
      _obj:new InspectionEditorLogic(),
      defInfo:[],
      inspReport:[],
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
      session_lock:null
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
    console.log(newState.p_state.value," + ",action.data.action," > ",newState.c_state.value);
  }
  
  if (action.type === UISEV.Control_SM_Panel) {
    newState.showSM_graph = action.data;
    return newState;
  }


  switch(action.type)
  {
    case UISEV.Connected:
      newState.WS_CH=action.data;
      console.log("Connected",newState.WS_CH);
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
    case UISTS.MAIN:
      newState.showSplash=false;
      return newState;
    
  }

  let stateObj = xstate_GetCurrentMainState(newState.c_state);
  console.log()
  let substate = stateObj.substate;
  switch(stateObj.state)
  {
    case UISTS.SPLASH:
      newState.showSplash=true;
      return newState;
    case UISTS.MAIN:
      newState.showSplash=false;
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


        case UISEV.Inspection_Report:
          newState.edit_info=Object.assign({},newState.edit_info);
          //newState.report=action.data;
          newState.edit_info._obj.SetInspectionReport(action.data);
          newState.edit_info.inspReport = newState.edit_info._obj.inspreport;
          console.log(newState.edit_info.inspReport);
          //newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
        break;

        case UISEV.Define_File_Update:
          
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info._obj.SetDefInfo(action.data);
          
          newState.edit_info.edit_tar_info = null;
          
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
          
          //newState.edit_info.inherentShapeList=
            //newState.edit_info._obj.UpdateInherentShapeList();
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
          newState.edit_info.list=(action.data == null)? []: action.data;
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
        case UISEV.EC_Trigger_Inspection:
        {
          if(newState.WS_CH==undefined)break;
          let dat=action.data;
          if(dat === undefined)
            dat={};
          newState.WS_CH.send("II",0,dat);
        }
        break;
        case DefConfAct.EVENT.Edit_Tar_Ele_Cand_Update:
          newState.edit_info.edit_tar_ele_cand=
            (action.data == null)? null :(action.data instanceof Object)? Object.assign({},action.data):action.data;
            console.log("DEFCONF_MODE_Edit_Tar_Ele_Cand_Update",newState.edit_info.edit_tar_ele_cand);
        break;

        case DefConfAct.EVENT.Shape_Set:
        {
          //Three cases
          //ID undefined but shaped is defiend -Add new shape
          //ID is defined and shaped is defiend - Modify an existed shape if it's in the list
          //ID is defined and shaped is null   - delete  an existed shape if it's in the list

          let newID=action.data.id;
          console.log("newID:",newID);
          let shape = newState.edit_info._obj.SetShape(action.data.shape,newID);
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          
          newState.edit_info.inherentShapeList=
            newState.edit_info._obj.UpdateInherentShapeList();
          if(newID!==undefined)
          {//If this time it's not for adding new shape(ie, newID is not undefined)
            let tmpTarIdx=
            newState.edit_info._obj.FindShapeIdx( newID );
            console.log(tmpTarIdx);
            if(tmpTarIdx === undefined)//In this case we delete the shape in the list 
            {
              newState.edit_info.edit_tar_info=null;
            }
            else
            {//Otherwise, we deepcopy the shape
              newState.edit_info.edit_tar_info = 
                JSON.parse(JSON.stringify(newState.edit_info.list[tmpTarIdx]));
            }
          }
          else
          {//We just added a shape, set it as an edit target
            newState.edit_info.edit_tar_info = 
              JSON.parse(JSON.stringify(shape));
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

            console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            console.log(obj,newState.edit_info.edit_tar_info);
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
          console.log(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            console.log(obj,newState.edit_info.edit_tar_info);
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
          console.log(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
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
                    console.log("Error: "+ subtype+ 
                      " doesn't accept "+cand.shape.type);
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.radius://Has to be an arc
                  if(cand.shape.type!=SHAPE_TYPE.arc)
                  {
                    console.log("Error: "+ subtype+ 
                      " Only accepts arc");
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.angle://Has to be an line to measure
                if(cand.shape.type!=SHAPE_TYPE.line)
                {
                  console.log("Error: "+ subtype+ 
                    " Only accepts line");
                  acceptData=false;
                }
              break;
                default :
                  console.log("Error: "+ subtype+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              if(acceptData)
              {
                console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
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
                  console.log("Error: "+ cand+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              newState.edit_info.edit_tar_info = 
                Object.assign(newState.edit_info.edit_tar_info,
                  {
                    pt1:{x:0,y:0},
                    value:0,
                    margin:1
                  });
              if(acceptData)
                obj[keyTrace[keyTrace.length-1]] = cand;
            }

            console.log(obj,newState.edit_info.edit_tar_info);
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

  if(action.type==="ATBundle")
  {
    action.data.reduce((state,action)=> StateReducer(state,action),newState);
    console.log(newState);
    return newState;
  }
  else
  {
    newState = StateReducer(newState,action);
    console.log(newState);
    return newState;
  }

  return newState;
}
export default UICtrlReducer