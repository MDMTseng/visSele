import { mkLog } from 'UTIL/logger';
import { UI_SM_EVENT } from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;
const log = mkLog('editor.reducer');

import {GetObjElement} from 'UTIL/MISC_Util';

let DefFile_DB_W_ID="DefFile_DB_W_ID";
let Insp_DB_W_ID="Insp_DB_W_ID";
let CORE_ID= "CORE_ID";
let CAM1_ID= "CAM1_ID";
let Platform_API_ID="Platform_API_ID";
let uInsp_API_ID="uInsp_API_ID";
let uInspESP32_API_ID="uInspESP32_API_ID";
let SLID_API_ID="SLID_API_ID";



let CNC_API_ID="CNC_API_ID";
//TODO: Move inspection report reducer logic to here
let StateReducer = (state, action) => {


  switch(action.type)
  {
    case "WS_CONNECTED":
    case "WS_UPDATE":
    case "WS_DISCONNECTED":
    case "WS_ERROR":
    {
      switch(action.id)
      {
        // uInsp/uInspESP32/SLID/CNC cases removed 2026-08-16: peripheral
        // link state lives in src/perif/PerifAPI.js's own store now
        // (usePerifLink / usePerifConn), not in Redux.
        case CORE_ID:
          
          if(action.type=="WS_UPDATE")
          {
            delete action["type"];
            return {...state,CORE_ID_CONN_INFO:{...state.CORE_ID_CONN_INFO,...action}}
          }
          if(GetObjElement(state,["CORE_ID_CONN_INFO","type"]) == action.type)break;
          log.debug("[core-conn-info]", action);
          return {...state,CORE_ID_CONN_INFO:action}
          break;
        case DefFile_DB_W_ID:
          if(action.type=="WS_UPDATE")
          {
            delete action["type"];
            return {...state,DefFile_DB_W_ID_CONN_INFO:{...state.DefFile_DB_W_ID_CONN_INFO,...action}}
          }
          if(GetObjElement(state,["DefFile_DB_W_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,DefFile_DB_W_ID_CONN_INFO:action}
          break;
        case Insp_DB_W_ID:
          if(action.type=="WS_UPDATE")
          {
            delete action["type"];
            return {...state,Insp_DB_W_ID_CONN_INFO:{...state.Insp_DB_W_ID_CONN_INFO,...action}}
          }
          if(GetObjElement(state,["Insp_DB_W_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,Insp_DB_W_ID_CONN_INFO:action}
          break;

        case Platform_API_ID:
          if(action.type=="WS_UPDATE")
          {
            delete action["type"];
            return {...state,Platform_API_ID_CONN_INFO:{...state.Platform_API_ID_CONN_INFO,...action}}
          }
          if(GetObjElement(state,["Platform_API_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,Platform_API_ID_CONN_INFO:action}
          break;
  



          
        case CAM1_ID:
          if(action.type=="WS_UPDATE")
          {
            delete action["type"];
            return {...state,CAM1_ID_CONN_INFO:{...state.CAM1_ID_CONN_INFO,...action}}
          }
          if(GetObjElement(state,["CAM1_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,CAM1_ID_CONN_INFO:action}
          break;

        



      }
      return state;
    }


  }
  // logX.info(action)
  return state;
}

let ConnectionInfoReducer = (state = {
  CORE_ID,
  CORE_ID_CONN_INFO:undefined,

  DefFile_DB_W_ID,
  DefFile_DB_W_ID_CONN_INFO:undefined,
  Insp_DB_W_ID,
  Insp_DB_W_ID_CONN_INFO:undefined,
  
  CAM1_ID,
  CAM1_ID_CONN_INFO:undefined,

  uInsp_API_ID,
  uInsp_API_ID_CONN_INFO:undefined,

  uInspESP32_API_ID,
  uInspESP32_API_ID_CONN_INFO:undefined,

  SLID_API_ID,
  SLID_API_ID_CONN_INFO:undefined,
  
  CNC_API_ID,
  CNC_API_ID_CONN_INFO:undefined,

  Platform_API_ID,
  Platform_API_ID_CONN_INFO:undefined,
}, action) => {
  if (action.type === undefined || action.type.includes("@@redux/")) return state;



  var d = new Date();
  //console.log(action);
  let newState = state;
  if (action.type === "ATBundle") {
    newState = action.data.reduce((state, action) => {
      action.date = d;
      return StateReducer(state, action);
    }, newState);

    return newState;
  }
  else {
    action.date = d;
    newState = StateReducer(newState, action);
    //log.debug(newState);
    return newState;
  }
}

export default ConnectionInfoReducer