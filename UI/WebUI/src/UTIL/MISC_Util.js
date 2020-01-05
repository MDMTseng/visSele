

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
  if( rootObj === undefined)return;
  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    //console.log(obj,key,obj[key]);
    obj = obj[key];

    if( obj === undefined)return;
  }
  return obj;
}

export function isString (value) {
  return typeof value === 'string' || value instanceof String;
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
      this.reconnectGap_ms=5000;
      this.connectionTimeout_ms=timeout;
      this.connectionTimer=null;
      this.readyState=undefined;
      this._connect(url);

  }
  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}

  onreconnection(reconnectionCounter){}
  onconnectiontimeout(){}

  setWebsocketCallbackUndefined(ws)
  {
    ws.onopen=
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
      //setTimeout(()=>this._connect(url),10);
      return this.onerror(ev);
    }
    this.websocket.onclose =(ev)=>{
      this.readyState=this.websocket.readyState;
      setTimeout(()=>this._connect(url),this.reconnectGap_ms);
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
  close() {
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

export class websocket_reqTrack{
  constructor(websocket) {
    let onopen = websocket.onopen;
    let onmessage = websocket.onmessage;
    let onerror = websocket.onerror;
    let onclose = websocket.onclose;

    this.trackWindow={};

    websocket.onopen=(ev)=>{
      this.readyState=this.websocket.readyState;
      this.onopen(ev);
    };
    websocket.onclose=(ev)=>{
      this.readyState=this.websocket.readyState;
      this.onclose(ev);
    };
    websocket.onmessage=(ev)=>{
      this.readyState=this.websocket.readyState;
      let p = JSON.parse(ev.data);
      let type=p.type;
      if(type=="ACK" || type=="NAK")
      {
        let req_id=p.req_id;
        if(req_id!==undefined)
        {
          let tobj = this.trackWindow[req_id];
          if(tobj!==undefined)
          {
            delete this.trackWindow[req_id];
            if(type=="ACK")
            {
              tobj.resolve(p);
            }
            else
            {
              tobj.reject(p);
            }
          }
          else
          {
            req_id=undefined;
          }
        }
        
        if(req_id===undefined){
          
          this.onTrackError({
            type:"ACK_TRACK_ERR",
            data:p
          });
        }
      }
      this.onmessage(ev,p);
    };
    websocket.onerror=(ev)=>{
      this.readyState=this.websocket.readyState;
      this.onerror(ev);
    };
    this.websocket=websocket;
  }
  
  onTrackError(ev){}

  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}


  
  close() {
    return this.websocket.close();
  }

  send_obj(data,replacer){

    let req_id = data.req_id;

    while(req_id===undefined||
      Object.keys(this.trackWindow).reduce((match,id)=>match||id === req_id,false))
      //Check existance
    {
      //if req_id is undefined / exists in the lookup table
      req_id = Math.floor(Math.random()*16777215).toString(16);
    }
    data.req_id = req_id;


    this.websocket.send(JSON.stringify(data,replacer));
  
    let trackObj={
      time:Date.now(),
      resolve:undefined,
      reject:undefined,
      data:data,
      rsp:undefined
    };
    this.trackWindow[req_id]=trackObj;
    return new Promise((resolve, reject)=>{
      trackObj.resolve=resolve;
      trackObj.reject=reject;
    });
  }
}


export function undefFallback(val,fallback) {
  return  val!==undefined?val:fallback;
}

  
export function dictLookUp(key,dict,theme) {
  let transVal;
  if(Array.isArray(key))
  {
    transVal=  GetObjElement(dict,key);
    return undefFallback(transVal,key[key.length-1]);
  }
  let dictTheme=undefFallback(theme,"fallback");
  transVal = GetObjElement(dict,[dictTheme, key]);
  return  undefFallback(transVal,key);
}


export const copyToClipboard = str => {
  const el = document.createElement('textarea');
  el.value = str;
  document.body.appendChild(el);
  el.select();
  document.execCommand('copy');
  document.body.removeChild(el);
};