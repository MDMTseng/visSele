import * as logX from 'loglevel';
import {UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;
let StateReducer = (state, action) => {
  
  console.log(action.type);
  switch(action.type)
  {
    case "MWWS_SEND"://get PING trigger and alive count --1
      if(action.data.id!=="EverCheckWS" || action.data.data===undefined)break;
      let data=action.data.data;
      if(data.data===undefined||data.data.msg===undefined)break;
      if(data.tl=="PD" && data.data.msg.type=="PING")
      {
        if(state.alive<=0)
        {
          state={...state,connected:false,alive:0}
        }
        else
        {
          state={...state,alive:state.alive-1}
        }
      }
      break;
    case UISEV.uInsp_Machine_Info_Update:
      
      state={...state,machineInfo:{...state.machineInfo,...action.data}};
      console.log(state)
      break;
    case UISEV.PD_DATA_Update:
      let pd_data = action.data;
      switch(pd_data.type)
      {
        case "CONNECT":
          console.log("CONNECT");
          state={...state,connected:true,alive:1}
        break;
        case "DISCONNECT":
          console.log("DISCONNECT");
          state={...state,connected:false,alive:0}
        break;
        case "MESSAGE":
          //console.log(pd_data.msg);
          switch(pd_data.msg.type)
          {
            case "PONG":
              state={...state,alive:1,
                error_codes:pd_data.msg.error_codes,
                res_count:pd_data.msg.res_count
              };
              //console.log("PONG",state);
            break;
            case "error_info":
                state={...state,error_codes:pd_data.msg.error_codes};
            break;
            case "res_count":
                state={...state,res_count:pd_data.msg.res_count};
            break;
            case "get_setup_rsp":
              let machineInfo = pd_data.msg;
              delete machineInfo.id;
              delete machineInfo.type;
              delete machineInfo.st;
              state={...state,machineInfo}
              console.log("get_setup_rsp",state);
            break;
          }
        break;
      }
    break;
  }
  //logX.info(action)
  return state;
}

let uInspReducer = (state = {
  connected:false,
  alive:0,
  machineInfo:undefined
}, action) => {
  if(action.type === undefined || action.type.includes("@@redux/"))return state;



  var d = new Date();
  //console.log(action);
  let newState=state;
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
    //log.debug(newState);
    return newState;
  }
}

export default uInspReducer