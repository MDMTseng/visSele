import * as logX from 'loglevel';
import { UI_SM_EVENT } from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;


let DefFile_DB_W_ID="DefFile_DB_W_ID";
let Insp_DB_W_ID="Insp_DB_W_ID";
let CORE_ID= "CORE_ID";
let CAMERA_ID= "CORE_ID";


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
          return {...state,CORE_ID_CONN_INFO:action}
          break;
        case DefFile_DB_W_ID:
          return {...state,DefFile_DB_W_ID_CONN_INFO:action}
          break;
        case Insp_DB_W_ID:
          return {...state,Insp_DB_W_ID_CONN_INFO:action}
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
  CAMERA_INFO:[]
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