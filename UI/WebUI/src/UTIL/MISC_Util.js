

export function Num2Str_padding(pad,num)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
}

export function round(num,round_nor=1)
{
  let tmp = (1/round_nor);
  let round_nor_inv = Math.round(tmp);
  if(round_nor_inv==0)
  {
    round_nor_inv=tmp;
  }

  return Math.round(num*round_nor_inv)/round_nor_inv;
}


export function xstate_GetCurrentMainState(state)
{
  if(typeof state.value === "string")
  {
    return {state:state.value, substate:null};
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return {state:state_Str, substate:complexState[state_Str]};

}

export function xstate_GetMainState(state)
{
  if(typeof state.value === "string")
  {
    return state.value;
  }
  let complexState = state.value;
  return Object.keys(complexState)[0];
}

export function xstate_GetSubState(state)
{
  if(typeof state.value === "string")
  {
    return undefined;
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return complexState[state_Str];
}


export function GetObjElement(rootObj,keyTrace,traceIdxTo=keyTrace.length-1)
{
  let obj = rootObj;
  let traceIdxTLen = traceIdxTo+1;
  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    //console.log(obj,key,obj[key]);
    obj = obj[key];

    if( obj === undefined)return;
  }
  return obj;
}



export function DictConv(key,dict,dictTheme)
{
  let translation = (dictTheme===undefined)?undefined:GetObjElement(dict,[dictTheme, key]);

  if(translation===undefined)
  {
    translation = GetObjElement(dict,["fallback", key]);
  }

  return translation;
}



export class websocket_autoReconnect{
  constructor(url,timeout=5000) {
      this.url=undefined;
      this.wsclose=false;
      this.reconnectionCounter=0;
      this.connectionTimeout_ms=timeout;
      this.connectionTimer=null;
      this.readyState=undefined;
      this._connect(url);

  }
  open(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}

  onreconnection(reconnectionCounter){}
  onconnectiontimeout(){}

  setWebsocketCallbackUndefined(ws)
  {
    ws.open=
    ws.onmessage=
    ws.onerror=
    ws.onclose=undefined;
  }
  _connect(url) {
    if(this.wsclose)return;
    
    console.log("_connect");
    this.url=url;
    if(this.websocket!==undefined)
    {
        this.reconnectionCounter++;
        let doconnection = this.onreconnection(this.reconnectionCounter);
        if(doconnection!==undefined && doconnection!=true)
        {
          this.websocket=undefined;
          return;
        }
    }

    if(this.connectionTimer!==undefined)
      clearTimeout(this.connectionTimer);
    this.connectionTimer=undefined;

    this.websocket = new WebSocket(url);
    this.OPEN=this.websocket.OPEN;
    this.CONNECTING=this.websocket.CONNECTING;
    this.CLOSED=this.websocket.CLOSED;
    this.CLOSING=this.websocket.CLOSING;
    this.connectionTimer = setTimeout(()=>{
        this.close();
        this.onconnectiontimeout();
    },this.connectionTimeout_ms);
    // this.state.WS_DB_Insert.binaryType = "arraybuffer";

    this.websocket.onopen = (ev)=>{
      this.readyState=this.websocket.readyState;
      clearTimeout(this.connectionTimer);
      this.connectionTimer=undefined;
      return this.onopen(ev);
    };
    this.websocket.onmessage =(ev)=> this.onmessage(ev);
    this.websocket.onerror =(ev)=>{
      this.readyState=this.websocket.readyState;
      setTimeout(()=>this._connect(url),10);
      return this.onerror(ev);
    }
    this.websocket.onclose =(ev)=>{
      this.readyState=this.websocket.readyState;
      setTimeout(()=>this._connect(url),10);
      
      return this.onclose(ev);
    }
  }
  
  reconnect() {
    console.log(    this.wsclose,this.websocket);
    this.wsclose=false;
    if(this.websocket!==undefined)
    {
      if(this.websocket.readyState!=this.websocket.OPEN)
      {
        setWebsocketCallbackUndefined(this.websocket);
        this.websocket=undefined;
        this._connect(this.url);
      }
      else
        this.websocket.close();//this should trigger _connect affter close/error event
    }
    else
    {
      this._connect(this.url);
    }
  }
  close(url) {
    this.wsclose=true;
    
    if(this.connectionTimer!==undefined)
      clearTimeout(this.connectionTimer);
    this.connectionTimer=undefined;
    if(this.websocket!==undefined)
    {
        this.websocket.close();

    }
  }

  send(data){
    return this.websocket.send(data);
  }
}
