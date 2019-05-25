

export const MWWS_EVENT = {
    ERROR:"MWWS_ERROR",
    CONNECT:"MWWS_CONNECT",
    DISCONNECT:"MWWS_DISCONNECT",
    SEND:"MWWS_SEND",
};

export const MWWebSocket = WSData => store => next => action => {

  if(action.type==undefined || !action.type.startsWith("MWWS_"))
  {
    return next(action);
  }

  let info = action.data;
  let id = info.id;
  if(id === undefined )
  {
    id = info.url;
  }
  
  if(id === undefined)return next({type:MWWS_EVENT.ERROR,data:{str:"no id/url is found",info:info}});


  switch (action.type) {
    case MWWS_EVENT.CONNECT:
    {
      if(WSData[id] !== undefined )
      {//There is a connection session on this id
        //Disconnect it first
        store.dispatch({type:MWWS_EVENT.DISCONNECT,data:WSData[id]});
      }
      WSData[id] = undefined;

      info.websocket= new WebSocket(info.url);

      if(info.binaryType!==undefined)
        info.websocket.binaryType = info.binaryType; 
      
      info.websocket.onopen = function(ev)
      {
        this.onopen(ev,this);
      }.bind(info);
      info.websocket.onmessage = function(ev)
      {
        this.onmessage(ev,this);
      }.bind(info);
      info.websocket.onclose = function(ev)
      {
        this.onclose(ev,this);
        WSData[this.id]=undefined;
      }.bind(info);

      info.websocket.onerror = function(ev)
      {
        this.onerror(ev,this);
        WSData[this.id]=undefined;
      }.bind(info);

      WSData[id]=info;
      break;
    }

    case MWWS_EVENT.SEND:
    {
      if(WSData[id] === undefined )
      {
        return next({type:MWWS_EVENT.MWWS_ERROR});
      }

      if(WSData[id].send === undefined)
      {
        WSData[id].websocket.send(info.data);
      }
      else
      {
        WSData[id].send(info.data,WSData[id],info.promiseCBs);
      }
      break;
    }

    // User request to disconnect
    case MWWS_EVENT.DISCONNECT:
    {
      WSData[id].websocket.close();
      break;
    }

    default: // We don't really need the default but ...
      break;
  };

  return next(action);
};

