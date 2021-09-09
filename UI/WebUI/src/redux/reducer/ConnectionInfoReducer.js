import * as logX from 'loglevel';
import { UI_SM_EVENT } from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;

import JSum from 'jsum'
import {GetObjElement} from 'UTIL/MISC_Util';

let DefFile_DB_W_ID="DefFile_DB_W_ID";
let Insp_DB_W_ID="Insp_DB_W_ID";
let CORE_ID= "CORE_ID";
let CAM1_ID= "CAM1_ID";

let uInsp_API_ID="uInsp_API_ID";
let SLID_API_ID="SLID_API_ID";
//TODO: Move inspection report reducer logic to here
let StateReducer = (state, action) => {


  switch(action.type)
  {
    case "WS_CONNECTED":
    case "WS_DISCONNECTED":
    case "WS_ERROR":
    {
      switch(action.id)
      {
        case CORE_ID:
          
          if(GetObjElement(state,["CORE_ID_CONN_INFO","type"]) == action.type)break;
          console.log(action)
          return {...state,CORE_ID_CONN_INFO:action}
          break;
        case DefFile_DB_W_ID:
          if(GetObjElement(state,["DefFile_DB_W_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,DefFile_DB_W_ID_CONN_INFO:action}
          break;
        case Insp_DB_W_ID:
          if(GetObjElement(state,["Insp_DB_W_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,Insp_DB_W_ID_CONN_INFO:action}
          break;
          
        case CAM1_ID:
          if(GetObjElement(state,["CAM1_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,CAM1_ID_CONN_INFO:action}
          break;

        
        case uInsp_API_ID:
          if(GetObjElement(state,["uInsp_API_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,uInsp_API_ID_CONN_INFO:action}
          break;

        case SLID_API_ID:
          if(GetObjElement(state,["SLID_API_ID_CONN_INFO","type"]) == action.type)break;
          return {...state,SLID_API_ID_CONN_INFO:action}
          break;
      }
      return state;
    }


    case UISEV.Core_Camera_Status_Update:

      let oldCamJsum=undefined;
      if(status.CAMERA_CONN_INFO!==undefined)
      {
        oldCamJsum=JSum.digest(status.CAMERA_CONN_INFO, 'SHA256', 'hex');
      }
      let newCamJsum=JSum.digest(action.data, 'SHA256', 'hex');
      if(oldCamJsum==newCamJsum)
      {
        return state;//no change required
      }
      return {...state,CAMERA_CONN_INFO:action.data};
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

  SLID_API_ID,
  SLID_API_ID_CONN_INFO:undefined,
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