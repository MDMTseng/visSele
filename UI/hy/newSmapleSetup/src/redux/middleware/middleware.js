
import {BPG_WS_EVENT,UI_SM_EVENT} from '../actions/UIAct';

export const ActionThrottle = ATData => store => next => action => {
  //console.log("ActionThrottle", ATData);
  
  if (ATData.initted === undefined)
  {
    if (typeof(ATData.time ) != typeof(0))
    {
      ATData.time=30;
    }

    if (typeof(ATData.posEdge ) != typeof(true))
    {
      ATData.posEdge=true;
    }
    ATData.actions=[];
    ATData.initted=true;
    ATData.timeout_obj=null;
  }

  switch(action.ActionThrottle_type)
  {
    case "express":
    return next(action);

    case "flush":
    {
      console.log("Trigger.....");
      clearTimeout(ATData.timeout_obj);
      ATData.timeout_obj=null;
      if(ATData.actions.length==0)return;
      let actions = ATData.actions;
      ATData.actions=[];
      return next({
        type:"ATBundle",
        data:actions
      });
    }
    break;
    default:
      if(ATData.timeout_obj == null)
      {
        ATData.timeout_obj = setTimeout(()=>{
          store.dispatch({ActionThrottle_type:"flush"});
        },ATData.time);

        if(ATData.posEdge)
          return next(action);
        else
          ATData.actions.push(action);
        return;
      }
      else
      {
        ATData.actions.push(action);
      }
    return;
    
  }


  return next(action);
  
};


export const logger = (store) => (next) => (action) =>{
  //console.log("action fired", action);
  next(action)
}

export const error_catch = (store) => (next) => (action) =>{
    next(action)

}

export const middleware_BPG_WebSocket = store => next => action => {
  switch (action.type) {
    // User request to connect
    case BPG_WS_EVENT.BPG_WS_REGISTER:
    {
      console.log("sdfljpsodfj");
      // Configure the object
      let websocket = action.ws;

      //this.binary_ws = new BPG_WEBSOCKET.BPG_WebSocket(url);//"ws://localhost:4090");

      // Attach the callbacks
      websocket.onopen_bk = (event) => store.dispatch(UIAct.EV_WS_Connected(event));
      websocket.onclose_bk = (event) => store.dispatch(UIAct.EV_WS_Disconnected(event));
      websocket.onmessage_bk = (event) => store.dispatch(UIAct.EV_WS_RECV(event));

      break;
    }

    // User request to send a message
    case BPG_WS_EVENT.BPG_WS_SEND:
    {
      let websocket = action.ws;
      websocket.send(action.data);
      break;
    }

    // User request to disconnect
    case BPG_WS_EVENT.BPG_WS_DISCONNECT:
    {
      let websocket = action.ws;
      websocket.close();
      break;
    }

    default: // We don't really need the default but ...
      break;
  };

  return next(action);
};