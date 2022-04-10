import JSum from 'jsum';



export function Num2Str_padding(pad:number,num:number)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
}

export function round(num:number,round_nor=1)
{
  let tmp = (1/round_nor);
  let round_nor_inv = Math.round(tmp);
  if(round_nor_inv==0)
  {
    round_nor_inv=tmp;
  }

  return Math.round(num*round_nor_inv)/round_nor_inv;
}


export function xstate_GetCurrentMainState(state:any)
{
  if(typeof state.value === "string")
  {
    return {state:state.value, substate:null};
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return {state:state_Str, substate:complexState[state_Str]};

}

export function xstate_GetMainState(state:any)
{
  if(typeof state.value === "string")
  {
    return state.value;
  }
  let complexState = state.value;
  return Object.keys(complexState)[0];
}

export function xstate_GetSubState(state:any)
{
  if(typeof state.value === "string")
  {
    return undefined;
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return complexState[state_Str];
}


export function GetObjElement(rootObj:{[key:string]:any}|undefined,keyTrace:((string|number)[]),traceIdxTo=keyTrace.length-1)
{
  if(rootObj===undefined)return undefined;
  let obj:any = rootObj;
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


export function ObjShellingAssign(rootObj:{[key:string]:any}|undefined,keyTrace:((string|number)[]),data:any=undefined)
{
  if(rootObj===undefined)return undefined;
  let obj:any = rootObj;
  let traceIdxTLen = keyTrace.length;
  if( rootObj === undefined)return;
  let root:any=undefined;

  
  if(obj instanceof Object)
  {
    obj=root={...obj};
  }
  else if(obj instanceof Array)
  {
    obj=root=[...obj];
  }
  else
  {
    return undefined;
  }


  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    //console.log(obj,key,obj[key]);
    
    let subObj:any=obj[key];
    
    console.log(obj,key,subObj);
    if(i==traceIdxTLen-1)
    {
      if(data!==undefined)
      {
        obj[key]=data;
      }
      return root;
    }



  
    if(subObj instanceof Object)
    {
      subObj={...subObj};
    }
    else if(obj instanceof Array)
    {
      subObj=[...subObj];
    }


    obj[key]=subObj;
    obj=subObj;




    if( obj === undefined)return obj;
  }
  return obj;
}


export function isString (value:any) {
  return typeof value === 'string' || value instanceof String;
}


/*
export class websocket_autoReconnect{
  url:string
  reconnectionCounter:number
  reconnectGap_ms:number
  connectionTimeout_ms:number
  connectionTimer:number | undefined//timer id
  readyState:number
  aR_states:any//an enum
  aR_state:number
  websocket:WebSocket|undefined
  OPEN:number
  CONNECTING:number
  CLOSED:number
  CLOSING:number
  curState:number
  

  constructor(url:string,timeout=5000) {
      this.url=url;
      this.reconnectionCounter=0;
      this.reconnectGap_ms=5000;
      this.connectionTimeout_ms=timeout;
      this.connectionTimer=undefined;
      this.readyState=-1;
      this.curState=-1;
      this.aR_states={
        IDLE:0,
        READY2RECONN:100,
        CONNECTING:1,
        TIMEOUT:2,
        CONNECTED:3,
        DISCONNECTED_FROM_REMOTE:4,
        DISCONNECTED_FROM_LOCAL:5,
        ERROR:6
      }
      this.aR_state=this.aR_states.IDLE;
      this._set_aRState(this.aR_states.IDLE);

      this._connect(url);
      
      this.OPEN=this.CLOSING=this.CLOSED=this.CONNECTING=-1;
      // this.aR_action={
      //   CONNECTING:1,
      //   CONNECTING_TIMEOUT:2,
      //   CONNECTING_ERROR:3,
      //   CONNECTED:4,
      //   CONNECTED_ERROR:5,
      //   CONNECTED_ERROR:5,
      // }
  }
  onopen(ev:any){}
  onmessage(ev:any){}
  onerror(ev:any){}
  onclose(ev:any){}

  onreconnection(reconnectionCounter:number){return true;}
  onconnectiontimeout(){}

  setWebsocketCallbackUndefined(ws:any)
  {
    ws.onopen=
    ws.onmessage=
    ws.onerror=
    ws.onclose=undefined;
  }

  _set_aRState(newState:number)
  {
    //console.log("cur s:",this.aR_state, " ns:",newState);
    this.aR_state=newState;
  }
  _get_aRState()
  {
    
    return this.aR_state;
  }
  
  _action_local_disconnection()
  {
    switch(this._get_aRState())
    {
      case this.aR_states.CONNECTED:
      case this.aR_states.CONNECTING:
      this._set_aRState(this.aR_states.DISCONNECTED_FROM_LOCAL);
      this._action_enter_idle(); 
      
      this.reconnectionCounter=0;
      //this._action_connect();
      return true;
    }
    return false;
  }
  
  _action_enter_idle()
  {
    switch(this._get_aRState())
    {
      case this.aR_states.ERROR:
      case this.aR_states.DISCONNECTED_FROM_REMOTE:
      case this.aR_states.DISCONNECTED_FROM_LOCAL:
      case this.aR_states.TIMEOUT:
      case this.aR_states.READY2RECONN:
        if(this.websocket!==undefined)
        {
          this.setWebsocketCallbackUndefined(this.websocket);
          this.websocket.close();
          this.websocket=undefined;
          // if(this.websocket.readyState!=this.websocket.OPEN)
          // {
          //   this._connect(this.url);
          // }
          // // else
          // //   this.websocket.close();//this should trigger _connect affter close/error event
        }
        if(this.connectionTimer!==undefined)
        {
          clearTimeout(this.connectionTimer);
          this.connectionTimer=undefined;
        }
        this._set_aRState(this.aR_states.IDLE);

        
        return true;
      
      case this.aR_states.IDLE:



        //When In idle
        return true;
    }
    return false;
  }

  
  _action_error(ev:any)
  {
    this._set_aRState(this.aR_states.ERROR);
    return this._action_RECONN();
  }

  _action_remote_disconnection(ev:any)
  {
    switch(this._get_aRState())
    {
      case this.aR_states.CONNECTED:
      this._set_aRState(this.aR_states.DISCONNECTED_FROM_REMOTE);
      this._action_RECONN();
      return true;
    }
    return false;
  }

  _action_connection_timeout()
  {
    switch(this._get_aRState())
    {
      case this.aR_states.CONNECTING:
        this._set_aRState(this.aR_states.TIMEOUT);
        this._action_RECONN();
      return true;
    }
    return false;
  }
  _action_RECONN()
  {
    //console.log("try enter _action_RECONN curr state:",this.aR_state);
    switch(this._get_aRState())
    {
      case this.aR_states.ERROR:
      case this.aR_states.DISCONNECTED_FROM_REMOTE:
      case this.aR_states.TIMEOUT:

        //console.log("enter _action_RECONN >>");
      //Clean up
        if(this.websocket!==undefined)
        {
          this.setWebsocketCallbackUndefined(this.websocket);
          this.websocket.close();
          this.websocket=undefined;
        }
        if(this.connectionTimer!==undefined)
        {
          clearTimeout(this.connectionTimer);
          this.connectionTimer=undefined;
        }


        //Be in READY2RECONN, and setTimeout to reconn
        this._set_aRState(this.aR_states.READY2RECONN);
        
        this.reconnectionCounter++;
        
        if(this.onreconnection(this.reconnectionCounter)==true)
        {
          setTimeout(()=>{
            this._action_connect(this.url);
          },this.reconnectGap_ms)
          
        }
        else
        {
          this._action_enter_idle();
        }
        
        return true;
    }
    return false;
  }
  
  _action_connection_open()
  {
    switch(this._get_aRState())
    {
      case this.aR_states.CONNECTING:

        this.reconnectionCounter=0;
        this._set_aRState(this.aR_states.CONNECTED);
        clearTimeout(this.connectionTimer);
        this.connectionTimer=undefined;
      return true;
    }
    return false;
  }

  _action_connect(url:string)
  {
    switch(this._get_aRState())
    {
      case this.aR_states.IDLE:
      case this.aR_states.READY2RECONN:


        this.url=url;
        this.websocket = new WebSocket(url);
        this.OPEN=WebSocket.OPEN;
        this.CONNECTING=WebSocket.CONNECTING;
        this.CLOSED=WebSocket.CLOSED;
        this.CLOSING=WebSocket.CLOSING;
        
        this.onNewState(this.CONNECTING);
        this.connectionTimer =window.setTimeout(()=>{
            this._action_connection_timeout();

            this.onconnectiontimeout();
            
        },this.connectionTimeout_ms);
        // this.state.WS_DB_Insert.binaryType = "arraybuffer";
    
        this.websocket.onopen = (ev)=>{
          
          this.readyState=this.websocket?.readyState ??NaN;
          this.onNewState(this.readyState);
          this._action_connection_open();
          return this.onopen(ev);
        };
        this.websocket.onmessage =(ev)=> this.onmessage(ev);
        this.websocket.onerror =(ev)=>{
          this.readyState=this.websocket?.readyState??NaN;
          this.onNewState(this.readyState);
          //setTimeout(()=>this._connect(url),10);
          this._action_error(ev);
          return this.onerror(ev);
        }
        this.websocket.onclose =(ev)=>{
          this.readyState=this.websocket?.readyState??NaN;
          this.onNewState(this.readyState);
          this._action_remote_disconnection(ev)
          return this.onclose(ev);
        }
        this._set_aRState(this.aR_states.CONNECTING);
        //When In idle
        return true;
    }
    return false;
  }

  _connect(url:string) {
    //console.log("_connect: curState:",this.aR_state);
    if(this._get_aRState()==this.aR_states.IDLE)
      return this._action_connect(url);
    return false;
  }
  
  
  private onNewState(state:number){
    //console.log(this.curState,state);
    if(this.curState!=state)
    {
      this.onStateUpdate(state,this.curState);
      this.curState=state;
    }
  }
  onStateUpdate(curState:number,preState:number){

    //console.log("onStateUpdate",curState,preState);
  }

  close() {
    this._action_local_disconnection();
  }

  send(data:string | ArrayBufferLike | Blob | ArrayBufferView){
    return this.websocket?.send(data)??undefined;
  }
}
*/
// export class websocket_reqTrack{
//   trackKey:string

//   constructor(websocket:websocket_autoReconnect|WebSocket,trackKey="req_id") {
//     let onopen = websocket.onopen;
//     let onmessage = websocket.onmessage;
//     let onerror = websocket.onerror;
//     let onclose = websocket.onclose;
//     this.trackKey=trackKey;
//     this.trackWindow={};

//     websocket.onopen=(ev)=>{
//       this.trackWindow={};
//       this.readyState=this.websocket.readyState;
//       this.onopen(ev);
//     };
//     websocket.onclose=(ev)=>{
//       Object.keys(this.trackWindow).forEach(key=>{
//         if(this.trackWindow[key].reject!==undefined)
//           this.trackWindow[key].reject()
//       })
//       this.trackWindow={};
//       this.readyState=this.websocket.readyState;
//       this.onclose(ev);
//     };
//     websocket.onmessage=(ev)=>{
//       this.readyState=this.websocket.readyState;
//       let p = JSON.parse(ev.data);
//       if(p.type=="ACK")
//       {
        
//         let tKey=p[this.trackKey];
//         if(tKey!==undefined)
//         {
//           let tobj = this.trackWindow[tKey];
//           if(tobj!==undefined)
//           {
//             delete this.trackWindow[tKey];
//             tobj.resolve(p);
//           }
//           else
//           {
//             tKey=undefined;
//           }
//         }
        
//         if(tKey===undefined){
          
//           this.onTrackError({
//             type:"ACK_TRACK_ERR",
//             data:p
//           });
//         }
//       }
//       else
//         this.onmessage(ev,p);
//     };
//     websocket.onerror=(ev)=>{

//       Object.keys(this.trackWindow).forEach(key=>{
//         if(this.trackWindow[key].reject!==undefined)
//           this.trackWindow[key].reject()
//       })
//       this.trackWindow={};
//       this.readyState=this.websocket.readyState;
//       this.onerror(ev);
//     };
//     this.websocket=websocket;
//   }
  
//   onTrackError(ev){}

//   onopen(ev){}
//   onmessage(ev){}
//   onerror(ev){}
//   onclose(ev){}


  
//   close() {
//     let ret = this.websocket.close();
//     this.readyState=this.websocket.readyState;
//     return ret;
//   }


//   send_obj(data,replacer){

//     let tKey = data[this.trackKey];

//     while(tKey===undefined||
//       Object.keys(this.trackWindow).reduce((match,id)=>match||id === tKey,false))
//       //Check existance
//     {
//       //if tKey is undefined / exists in the lookup table
//       tKey = Math.floor(Math.random()*16777215).toString(16);
//     }
//     data[this.trackKey] = tKey;


//     this.websocket.send(JSON.stringify(data,replacer));
  
//     let trackObj={
//       time:Date.now(),
//       resolve:undefined,
//       reject:undefined,
//       data:data,
//       rsp:undefined
//     };
//     this.trackWindow[tKey]=trackObj;
//     return new Promise((resolve, reject)=>{
//       trackObj.resolve=resolve;
//       trackObj.reject=reject;
//     });
//   }
// }




// export class websocket_aliveTracking
// {
//   constructor(config)
//   {
//     if(config.onStateChange===undefined)
//       config.onStateChange=()=>{};

      
//     if(config.connectionTimeout_ms===undefined)
//       config.connectionTimeout_ms=5000;
//     if(config.pingpongTimeout_ms===undefined)
//       config.pingpongTimeout_ms=5000;
      
//     if(config.reconnectTimeout_ms===undefined)
//       config.reconnectTimeout_ms=5000;
      
//     if(config.url===undefined)
//       config.url=undefined;
      
//     if(config.binaryType===undefined)
//       config.binaryType="blob";


//     if(config.trackKey===undefined)
//       config.trackKey="req_id";
//     if(config.pingPacket===undefined)
//       config.pingPacket={type:"PING"};

//     this.trackWindow={};



//     this.states={
//       INIT:"INIT",
//       NO_URL:"NO_URL",
//       CONNECTING:"CONNECTING",
//       CONNECTED:"CONNECTED",
//       DISCONNECTING:"DISCONNECTING",
//       DISCONNECTED:"DISCONNECTED",
//       ERROR:"ERROR"
//     };

//     this.errorInfo={
//       NO_ERROR:"NO_ERROR",
//       GENERIC:"GENERIC",
//       NO_URL:"NO_URL",

//     };


//     this.ERROR_INFO=this.errorInfo.NO_ERROR;
    
    
//     this.acts={
//       CONNECT:"CONNECT",
//       CONNECT_DONE:"CONNECT_DONE",
//       DISCONNECT:"DISCONNECT",
//       DISCONNECT_DONE:"DISCONNECT_DONE",

//       ERROR:"ERROR"
//     };

//     this.config=config;

//     this.onStateChange=config.onStateChange;
//     this.websocket=undefined;

    
//     this.pingStat=false;


//     this.connectionTimeout=undefined;
//     this.connectionTimeout_ms=config.connectionTimeout_ms;

//     this.pingpongTimeout=undefined;
//     this.pingpongTimeout_ms=config.pingpongTimeout_ms;

//     this.reconnectTimeout=undefined;
//     this.reconnectTimeout_ms=config.reconnectTimeout_ms;

//     if(this.config.url!==undefined)
//     {
//       this.RESET(this.config.url)
//     }
//   }

//   getURL()
//   {
//     return this.config.url;
//   }

//   getErrorInfo()
//   {
//     return this.ERROR_INFO;
//   }
//   RESET(url=this.config.url)
//   {
    


//     this.config.url=url;
//     if(this.websocket!==undefined)
//     {
//       this.close();
//     }
    

//     this.ERROR_INFO=this.errorInfo.NO_ERROR;

    
//     this.connectionTimeout_ms=this.config.connectionTimeout_ms;
//     this.pingpongTimeout_ms=this.config.pingpongTimeout_ms;
//     this.reconnectTimeout_ms=this.config.reconnectTimeout_ms;
    
//     if(this.reconnectTimeout!==undefined)
//       clearTimeout(this.reconnectTimeout);
//     this.reconnectTimeout=undefined;


//     this.connectionTimeout=undefined;
//     this.pingpongTimeout=undefined;

//     this.pingStat=false;

//     if(url===undefined)
//     {
//       console.log(">>");
//       this.reconnectTimeout_ms=-1;
//       this.state=this.states.INIT;
//       this.ERROR_INFO=this.errorInfo.NO_URL;
//       this.state=this.stateTransfer(this.state,this.acts.ERROR);
      
//       return;
//     }

    
//     this.state=this.states.INIT;

//     this.websocket= new WebSocket(url);
//     this.websocket.binaryType=this.config.binaryType;

//     this.websocket.onclose=(ev)=>{
//       console.log("ONCLOSE....");
//       this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);
//       this.onclose(ev)
//     };
//     this.websocket.onerror=(ev)=>{
//       console.log("ERROR....");
//       this.ERROR_INFO=this.errorInfo.GENERIC;
//       this.state=this.stateTransfer(this.state,this.acts.ERROR);
//       this.onerror(ev)
//     };
//     this.websocket.onopen=(ev)=>{
//       this.state=this.stateTransfer(this.state,this.acts.CONNECT_DONE);
//       this.onopen(ev)
//     };
//     this.websocket.onmessage=(ev)=>{
//       // console.log(ev)

//       if(this.config.trackKey!==undefined)
//       {
//         let doFindId=false;

//         try{

//           let msg_obj = JSON.parse(ev.data);
          
//           ev.data_obj=msg_obj;
//           let tKey=msg_obj[this.config.trackKey];

//           if(tKey!==undefined)
//           {
//             let tobj = this.trackWindow[tKey];
//             if(tobj!==undefined)
//             {
//               delete this.trackWindow[tKey];
//               doFindId=true;
//               tobj.resolve(msg_obj);
//             }
//             else
//             {
//               tKey=undefined;
//             }
//           }
//         }
//         catch(e)
//         {

//         }

//         if(!doFindId)
//         {
//           this.onmessage_obj(ev);
//         }

//       }
//       else
//       {
//         this.onmessage(ev);
//       }
//     };
    
//     this.state=this.stateTransfer(this.state,this.acts.CONNECT);
//   }
//   close()
//   {
//     this.reconnectTimeout_ms=-1;//to stop reconnection
//     this._close();
//   }
//   _close()
//   {
    
//     this._CLEANUP();
    
//     if(this.websocket!==undefined)
//     {
//       this.websocket.close();
//     }
//     this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);
//     this.websocket=undefined;
//   }

//   onopen(ev){}
//   onmessage(ev){}
//   onmessage_obj(ev){}
//   onerror(ev){}
//   onclose(ev){}


  
//   _CLEANUP()
//   {
//     if(this.websocket!==undefined)
//     {
//       this.websocket.onclose=undefined;
//       this.websocket.onerror=undefined;
//       this.websocket.onopen=undefined;
//       this.websocket.onmessage=undefined;
//     }

//     if(this.connectionTimeout!==undefined)
//       clearTimeout(this.connectionTimeout);
//     this.connectionTimeout=undefined;

//     if(this.pingpongTimeout!==undefined)
//       clearTimeout(this.pingpongTimeout);
//     this.pingpongTimeout=undefined;

    
//     Object.keys(this.trackWindow).forEach(key=>{
//       if(this.trackWindow[key].reject!==undefined)
//         this.trackWindow[key].reject()
//     })
//     this.trackWindow={};

//   }

//   PING()
//   {
//     do{
      
//       if(this.websocket===undefined)
//       {
//         this.state=this.stateTransfer(this.state,this.acts.ERROR);
//         break;
//       }
//       if(this.websocket.readyState!==1)
//       {
//         this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);

//         break;
//       }
//       // console.log("PING");
//       if(this.pingStat==true)
//       {
//         console.log("PING timeout close");
//         this._close();
//         break;
//       }

//       this.pingStat=true;
//       this.send_obj(this.config.pingPacket)
//       .then(ev=>{
//         // console.log(ev);
//         this.pingStat=false;
//       })
//       .catch(e=>{
//         console.log(e)
//       });

//     }while(false);

//     if(this.pingpongTimeout_ms>0)
//       this.pingpongTimeout=setTimeout(()=>{this.PING()},this.pingpongTimeout_ms);
//   }

//   stateTransfer(state,act,info)
//   {
//     let newState=this._stateTransfer(state,act,info);

//     if(newState!==state)
//     {
//       // console.log(newState,"< (",act,")=",state);
//       this.onStateChange(newState,state,act);
//       switch(state)//exit
//       {
//         case this.states.CONNECTING:
//           clearTimeout(this.connectionTimeout);
//           this.connectionTimeout=undefined;
//           break;
//         case this.states.CONNECTED:
          
//           this.pingStat=true;
//           clearTimeout(this.pingpongTimeout);
//           this.pingpongTimeout=undefined;
//           break;
//         case this.states.DISCONNECTING:
//           break;
//         case this.states.DISCONNECTED:
//           break;
//         case this.states.ERROR:
//           break;
//       }

      
//       switch(newState)//enter
//       {
//         case this.states.CONNECTING:
//           if(this.connectionTimeout_ms>=0)
//           {
//             this.connectionTimeout=setTimeout(()=>{
//               console.log("connection timeout......");
//               this._close();
//             },this.connectionTimeout_ms);
//           }
//           break;
//         case this.states.CONNECTED:
//           this.pingStat=false;
          
//           if(this.pingpongTimeout_ms>0)
//           {
//             setTimeout(this.PING.bind(this),1000);
//           }
          
//           break;
//         case this.states.DISCONNECTING:
//           break;
//         case this.states.DISCONNECTED:
//           this._CLEANUP();
//           if(this.reconnectTimeout_ms>=0)
//           {
//             this.reconnectTimeout=setTimeout(()=>this.RESET(),this.reconnectTimeout_ms);
//           }
//           break;
//         case this.states.ERROR:
//           this._CLEANUP();
//           if(this.reconnectTimeout_ms>=0)
//           {
//             this.reconnectTimeout=setTimeout(()=>this.RESET(),this.reconnectTimeout_ms);
//           }
//           break;
//       }
//     }

//     return newState;
    
//   }
//   _stateTransfer(state,act,info)
//   {
//     switch(state)
//     {
//       case this.states.INIT:

//         switch(act)
//         {
//           case this.acts.CONNECT:
//             return this.states.CONNECTING;
//             break;
//           case this.acts.ERROR:
//             return this.states.ERROR;
//             break;
//         }
//         break;
//       case this.states.CONNECTING:

//         switch(act)
//         {
//           case this.acts.CONNECT_DONE:
//             return this.states.CONNECTED;
//             break;
//           case this.acts.DISCONNECT_DONE:
//             return this.states.DISCONNECTED;
//             break;
//           case this.acts.ERROR:
//             return this.states.ERROR;
//             break;

//         }
//         break;
//       case this.states.CONNECTED:
//         switch(act)
//         {
//           case this.acts.DISCONNECT://from local
//           return this.states.DISCONNECTING;
//             break;
//           case this.acts.DISCONNECT_DONE:
//             return this.states.DISCONNECTED;
//             break;
//           case this.acts.ERROR:
//             return this.states.ERROR;
//             break;

//         }
//         break;
//       case this.states.DISCONNECTING:
//         switch(act)
//         {
//           case this.acts.DISCONNECT_DONE:
//             return this.states.DISCONNECTED;
//             break;
//           case this.acts.ERROR:
//             return this.states.ERROR;
//             break;
//         }
//         break;
//       case this.states.DISCONNECTED:
//         // switch(act)
//         // {
//         //   case this.acts.ERROR:
//         //     return this.states.ERROR;
//         //     break;
//         // }
//         break;
//       case this.states.ERROR:
//         // console.log("You stuck here");
//         return this.states.ERROR;
//         break;
//       default:
//         return this.states.ERROR;
//         break;
//     }
//     return state;
//   }


//   keyGen()
//   {
//     return Math.floor(Math.random()*16777215).toString(16);
//   }

  
//   keyGenUnique(preferedKey)
//   {
//     let key = preferedKey;
//     if(key===undefined)
//     {
//       key=this.keyGen();
//     }
    
//     while(this.trackWindow[key]!==undefined){//if the key existed
//       key = this.keyGen();//re-generate
//     }
//     return key;
//   }
  
//   send(data)
//   {
//     if(this.state !== this.states.CONNECTED)return;
//     this.websocket.send(data);
//   }
//   send_obj(data,replacer){

//     let tKey =this.keyGenUnique(data[this.config.trackKey])

//     data[this.config.trackKey] = tKey;


//     this.send(JSON.stringify(data,replacer));
  
//     let trackObj={
//       time:Date.now(),
//       resolve:undefined,
//       reject:undefined,
//       data:data,
//       rsp:undefined
//     };
//     this.trackWindow[tKey]=trackObj;
//     return new Promise((resolve, reject)=>{
//       trackObj.resolve=resolve;
//       trackObj.reject=reject;
//     });
//   }
  
// }



export const copyToClipboard = (str:string) => {
  // const el = document.createElement('textarea');
  // el.value = str;
  // document.body.appendChild(el);
  // el.select();
  // document.execCommand('copy');
  // document.body.removeChild(el);



  if (!navigator.clipboard){
    // use old commandExec() way
  } else{
      navigator.clipboard.writeText(str).then(
          function(){
              alert("yeah!"); // success 
          })
        .catch(
          function() {
              alert("err"); // error
        });
  }   



};





export function twoEQXY(a:number,b:number,c:number,d:number,e:number,f:number) {
  /* we solve the linear system
  * ax+by=e
  * cx+dy=f
  */
 let determinant = a*d - b*c;
 if(determinant != 0) {
   let x = (e*d - b*f)/determinant;
   let y = (a*f - e*c)/determinant;
   return [x,y];
 }
 return [NaN,NaN];
}

export function Calibration_MMPP_offset(old1:number,new1:number,old2:number,new2:number,cur_mmpp:number,cur_offset=0) {
 //pix1=old1/cur_mmpp-cur_offset
 //mmpp*(pix1+offset)=new1
 //mmpp*pix1 + mmpp*offset = new1
 //X   *a    +           Y =    e
 //b==1

 //a = old_pix1  b=1  e=new1
 //c = old_pix2  d=1  f=new2
 let old_pix1=old1/cur_mmpp-cur_offset;
 let old_pix2=old2/cur_mmpp-cur_offset;
 let retSolver=twoEQXY(
  old_pix1,1,old_pix2,1,new1,new2);
 let [mmpp,mmpp_x_offset] = retSolver;
 return {
   mmpp,offset:(mmpp_x_offset/mmpp)
 };
}







function extractAtomGroup(exp_str_arr:string[], mark = "$") {
  let lastH = exp_str_arr[exp_str_arr.length - 1];
  let matchD = lastH.match(/\(([^\(\)]+)\)/);
  //let matchD=/\([^\(\)]+\)/g.exec(lastH);
  if (matchD !== null && matchD.length > 0) {
    exp_str_arr[exp_str_arr.length - 1] = matchD[1];

    var newstr = lastH.replace(/\([^\(\)]+\)/, mark);
    exp_str_arr.push(newstr);
  }
  return exp_str_arr;
}
function extractArithGroup(expression:string, sep = "\\-\\+", mark = "$") {
  let regx_param;
  let regx_opera;

  if (mark == "$") mark += "$";

  regx_param = new RegExp("(.+)[" + sep + "](.+)");
  regx_opera = new RegExp(".+([" + sep + "]).+");

  let matchD = expression.match(regx_param);
  if (matchD == null) {
    return [expression];
  }
  var rep = expression.replace(regx_opera, mark + "$1" + mark);
  return [matchD[1], matchD[2], rep];
}

function extractArith(exp_stack:string[], operator_regexp = "\\-\\+", mark = "$") {
  return exp_stack
    .map(exp => {
      if (operator_regexp == "," && exp.includes(",")) {
        if (exp.includes(",")) {
          let splitArr = exp.split(",");
          let aggStr = mark;
          for (let i = 1; i < splitArr.length; i++) aggStr += "," + mark;

          splitArr.push(aggStr);
          return splitArr;
        }
        return exp;
      }

      let parse_target = operator_regexp;
      let arrayX = extractArithGroup(exp, parse_target, mark);
      while (true) {
        let arrayY = extractArithGroup(arrayX[0], parse_target, mark);
        if (arrayY.length == 1) break;
        arrayX.shift();
        arrayX = arrayY.concat(arrayX);
      }

      return arrayX;
    })
    .flat();
}
export type ESP2POST_FuncCallBack = (_key:string, val:any|any[])=>any
export type ESP2POST_FuncSet ={[key: string]:ESP2POST_FuncCallBack}
export function Exp2PostfixExp(exp_str:string) {//exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  let retA = [exp_str];

  let p_Mark = "$";
  
  //exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  // First pass Postfix exp converter
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }

  let postExpCalc_Func = {
    default: (key:string, vals:any) => {
      if (vals !== undefined) {
        
        //console.log("ori.key",key);
        let skey = key.split("$");
        let newKey =skey.reduce((nk,ele,idx)=>{
          if(idx==0)return ele;
          let repK = ((idx-1)>=vals.length)?"$":vals[idx-1]
          return nk+"(" + repK + ")"+ele;
        },"");
        key=newKey;
        
        // key = key.replace(/\$/g, "#");
        // vals.forEach(val => {
        //   key = key.replace("#", "(" + val + ")");
        // });
        // key = key.replace(/\#/g, "$");
        
//         console.log("...",newKey,key);
      }
      return key;
    }
  };
  //console.log("retA",retA);
  //["5-1/4*3", "4+5", "3+Math.tan$/3,1+2*3/$/6", "Math.max$"]
  retA = retA.map(exp => {//parse piece expression with +-*/... operators 
    exp = exp.replace(/\$/g,"#");
    let a_post_exp = [",","\\-\\+","%","\\*\\/","\\^"].reduce(
      (pexp,exp)=>
       extractArith(pexp, exp, "$"),[exp]);
    //Assamble back with "(" ")"
    let XXY = PostfixExpCalc(a_post_exp, postExpCalc_Func,false);
    
    return XXY[0].replace(/\#/g,"$");
  });
  //console.log("retA",retA);
  //["(5)-(((1)/(4))*(3))", "(4)+(5)", "((3)+((Math.tan$)/(3))),((1)+((((2)*(3))/($))/(6)))", "Math.max$"]
  //Assamble back with "(" ")"
  retA = PostfixExpCalc(retA, postExpCalc_Func);
  // console.log(retA)
  // retA=retA.filter(A=>A!="$")

  retA=retA.flat();
  
  
  //Now we have all the "(" ")" to do the 2nd pass Postfix exp converter

  //console.log("retA",retA);
  //["Math.max(((3)+((Math.tan((5)-(((1)/(4))*(3))))/(3))),((1)+((((2)*(3))/(((4)+(5))))/(6))))"]
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }
  //console.log("retA",retA);
  //["3", "5", "1", "4", "$/$", "3", "$*$", "$-$", "Math.tan$", "3", "$/$", "$+$", "1", "2", "3", "$*$", "4", "5", "$+$", "$", "$/$", "6", "$/$", "$+$", "$,$", "Math.max$"]
  return retA;
}

const regex_multi_group = /^\$(,\$)*$/gm;//match  $ or $,$ or $,$,$ or $,$,$,$ ...
export function PostfixExpCalc(postExp:string[], funcSet:ESP2POST_FuncSet,handleMultiGroup=true) {
  let valStack:any[] = [];
  postExp.forEach(exp => {
    let valPush;
    
    if (exp.includes !== undefined && exp.includes("$")) {
      let groupFlagCount = exp.split("$").length - 1;
      let tmpArr = valStack.splice(valStack.length - groupFlagCount, 9999999);
      if (funcSet[exp] !== undefined) 
      {
        valPush = funcSet[exp](exp,tmpArr.flat());
      }
      else if(handleMultiGroup&&exp.match(regex_multi_group)!=null)
      {
        valPush =tmpArr.flat();
      }
      else
      {
        valPush = funcSet.default(exp, tmpArr.flat());
      }

    } else {
      if (funcSet[exp] !== undefined) {
        valPush = funcSet[exp];
      } else if (funcSet.default !== undefined) {
        valPush = funcSet.default("default",exp);
      } else {
        valPush = NaN;
      }
    }

    valStack.push(valPush);
  });
  return valStack;
}


function ExpCalcBasic(postExp_:string[],funcSet:ESP2POST_FuncSet,fallbackFunctionSet:ESP2POST_FuncCallBack) {
  let postExp=postExp_.filter(exp=>exp!="$")
  funcSet = {
    min$: (key,vals) => Math.min(...vals),
    max$:  (key,vals) => Math.max(...vals),
    "$+$":  (key,vals) => vals[0] + vals[1],
    "$-$":  (key,vals) => vals[0] - vals[1],
    "$*$":  (key,vals) => vals[0] * vals[1],
    "$/$":  (key,vals) => vals[0] / vals[1],
    "$^$":  (key,vals) => Math.pow(vals[0] , vals[1]),
    ...funcSet,
    default:fallbackFunctionSet
  };

  return PostfixExpCalc(postExp, funcSet)[0];
}


export class CircularCounter{
  protected _total_size:number
  protected _size:number
  protected _sidx:number
  protected _eidx:number

  constructor(total_size:number)
  {
    this._total_size=total_size;
    this._sidx=0;
    this._eidx=0;
    this._size=0;
    this.clear();
  }
  
  size()
  {
    return this._size;
  }

  clear()
  {
    this._sidx=0;
    this._eidx=0;
    this._size=0;
  }
  tsize()
  {
    return this._total_size;
  }




  //|0|1|2|3|4|5|6|7|8|9
  //    |t|.........|h|   h:for next data(no actual data in it)    t:tail data idx(has data)
  //f(1)--------|*|
  //r(1)--|*|
  
  f(idx=-1)//from Q idx to array idx
  {
    idx+=1;
    if(idx>this._size)return -1;
    idx+=this._sidx-idx+this._total_size;
    return idx%this._total_size;
  }
  
  r(idx=0)
  {
    if(idx>=this._size)return -1;
    idx=this._eidx+idx;
    return idx%this._total_size;
  }
  
  enQ(force=false)
  {
    //if(!force && this._size>=this._total_size)return false;
    
    if(this._size==this._total_size)
    {
      if(!force)return false;
      //if we force push data then we deQ one data out
      this.deQ();
    }
    this._sidx+=1;
    this._sidx%=this._total_size;
    this._size++;
    return true;
  }
  
  deQ()
  {
    if(this._size<=0)return false;
    this._eidx+=1;
    this._eidx%=this._total_size;
    this._size--;
    return true;
  }
}




type CONSUME_QUEUE_PROMISE = (cq:ConsumeQueue)=>Promise<ConsumeQueue>
export class ConsumeQueue{
  protected cC:CircularCounter;
  protected queue:Array<number>;
  protected term:boolean;
  protected inPromise:boolean;
  protected onTerminationState:((cq:ConsumeQueue)=>void )|undefined;
  
  protected consumePromiseFunc:CONSUME_QUEUE_PROMISE;

  constructor(consumePromiseFunc:CONSUME_QUEUE_PROMISE,QSize=200,onTerminationState: ((cq:ConsumeQueue)=>void )|undefined=undefined ) {

    console.log(consumePromiseFunc,QSize,onTerminationState);
    this.cC=new CircularCounter(QSize);
    this.queue=new Array(QSize);
    this.term=false;
    this.inPromise=false;
    this.onTerminationState=onTerminationState;
    this.consumePromiseFunc=consumePromiseFunc;   
    //consumePromiseFunc has to return promise
    //resolve() will kick next consume
    //reject will stop kick next consume you will need to do it manually
  }
  size()
  {
    return this.cC.size();
  }
  f(idx:number)
  {
    let qidx = this.cC.f(idx);
    if(qidx===-1)return undefined;
    return this.queue[qidx];
  }


  enQ(data:any)
  {
    if(this.cC.size()>=this.cC.tsize())
    {
      //full or error
      return false;
    }
    this.queue[this.cC.f()]=data;
    this.cC.enQ();
    return true;
  }

  deQ()
  {
    if(this.cC.size()==0)return undefined;
    let data = this.queue[this.cC.r()];
    this.cC.deQ();
    return data;
  }

  head()
  {
    if(this.cC.size()==0)return undefined;
    let data = this.queue[this.cC.r()];
    return data;
  }

  _doTermAct()
  {
    // console.log("T2");
    if(this.term)//not in promise state, and if it's terminated, call the onTerminationState and never do any thing more
    {
      
      // console.log("T3",this.onTerminationState);
      if(this.onTerminationState!==undefined)
        this.onTerminationState(this);
      this.onTerminationState=undefined;//only call it once
      return true;
    }
    return false;
  }

  termination()
  {
    this.term=true;
    // console.log("T1");

    if(this._doTermAct())
    {
      return;
    }
  }



  kick()
  {
    //console.log("kick inPromise:"+this.inPromise);
    if(this.inPromise)
      return;

    
    if(this._doTermAct())
    {
      return;
    }
    
    this.inPromise=true;
    this.consumePromiseFunc(this).then(result=>{
      //console.log("Consume ok? result",result);
      this.inPromise=false;


      if(this._doTermAct())
      {
        return;
      }

      if(this.cC.size()!=0)
      {
        this.kick();//kick next consumption
      }
    }).catch(e=>{
      


      if(this._doTermAct())
      {
        return;
      }

      console.log("Consume failed... e=",e);
      this.inPromise=false;
    });
  }
}



// let exp_str = "Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)";
// //"3+tan(5-1/4*3)/3"
// //"1+2*3/(4+5)/6"

// console.log(//eval(exp_str),
//   ExpCalc(exp_str, {
//     "$>$?$:$":(vals)=>{
//       return vals[0]>vals[1]?vals[2]:vals[3];
//     },
//     "Math.sin$":(vals)=>{
//       return Math.sin(vals[0]);
//     },
//     "Math.tan$":(vals)=>{
//       return Math.tan(vals[0]);
//     },
//     "Math.min$":(vals)=> Math.min(...vals),
//     "Math.max$":(vals)=> Math.max(...vals),
//     default: (key, vals) =>{
//       if(vals===undefined)//it's a single value parsing
//       {
//         let pv=parseFloat(key);
//         if(pv!=pv)
//           {
//             throw "ERROR: key:"+key+" is not parsible!";
//           }
//         return pv;
//       }
      
//       if(key.match(/^\$[\,\$]+$/gm)!==null)
//         return vals;
//       throw "ERROR: "+key+" is not defined!";
//       return vals;
//     }
//   })
// );


export const LocalStorageTools={
  getlist:(lsKey:string)=>
  {
  
    if(localStorage===undefined)return [];
    let LocalS_list = localStorage.getItem(lsKey);
    if(LocalS_list==null)return [];
    try {
      let list = JSON.parse(LocalS_list);
      return list;
  
        
    } catch (e) {
    }
    return [];
  },
  setlist:(lsKey:string,list:Array<any>)=>localStorage.setItem(lsKey, JSON.stringify(list)),
  appendlist:(lsKey:string,data:any,pre_removeFilter:(data:any,idx:number)=>boolean)=>
  {
    let LocalS_list = LocalStorageTools.getlist(lsKey);

    if(pre_removeFilter!==undefined)
    {
      LocalS_list = LocalS_list.filter(pre_removeFilter);
    }
    LocalS_list.unshift(data);
    LocalStorageTools.setlist(lsKey, LocalS_list)
    return true;
  },
  getobj:(lsKey:string)=>
  {
  
    if(localStorage===undefined)return undefined;
    let LocalS_list = localStorage.getItem(lsKey);
    if(LocalS_list==null)return undefined;

    try {
      return JSON.parse(LocalS_list);
     
    } catch (e) {
    }
    return undefined;
  },
  setobj:(lsKey:string,obj:any)=>localStorage.setItem(lsKey, JSON.stringify(obj)),



}



export function ID_debounce(dbid:number|undefined,func:()=>void,func_clear_dbid:()=>void,delay:number=500)
{
  if(dbid!==undefined)
  {
    clearTimeout(dbid)
    dbid=undefined

    dbid=window.setTimeout(()=>{
      func();
      func_clear_dbid();
    },delay)
  }
  else
  {
    func();
    dbid=window.setTimeout(()=>{
      func_clear_dbid();
    },delay)
  }
  return dbid;
}



export function ID_throttle(thid:{timeout:number,last_func:()=>void}|undefined,func:()=>void,func_clear_thid:()=>void,delay:number=500)
{
  if(thid===undefined)
  {
    func();
    thid={
      timeout:window.setTimeout(()=>{
        if(thid)
        {
          thid.timeout=-1;
          thid.last_func();
        }
        func_clear_thid();
      },delay),
      last_func:()=>undefined
    }
  }
  else
  {
    thid.last_func=func;
  }
  return thid;
}