import * as logX from 'loglevel';
import {UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;
let uInspReducer = (state = {
  connected:false,
  alive:0
}, action) => {
  if(action.type === undefined || action.type.includes("@@redux/"))return state;
  var d = new Date();
  
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
    case UISEV.PD_DATA_Update:
      let pd_data = action.data;

      switch(pd_data.type)
      {
        case "CONNECT":
          console.log("CONNECT");
          state={...state,connected:true,alive:3}
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
              state={...state,alive:3}
              console.log("PONG",state);
            break;
          }





        break;
      }
    break;
  }
  logX.info(action)
  return state;
}
export default uInspReducer