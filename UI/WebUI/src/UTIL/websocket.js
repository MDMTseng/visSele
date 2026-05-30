// Stateful WebSocket helpers (auto-reconnect, request tracking, alive tracking).
// Cohesive networking unit relocated from MISC_Util.

import * as log from 'loglevel';

export class websocket_autoReconnect{
  
  constructor(url,timeout=5000) {
      this.url=undefined;
      this.reconnectionCounter=0;
      this.reconnectGap_ms=5000;
      this.connectionTimeout_ms=timeout;
      this.connectionTimer=null;
      this.readyState=undefined;
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
      
      // this.aR_action={
      //   CONNECTING:1,
      //   CONNECTING_TIMEOUT:2,
      //   CONNECTING_ERROR:3,
      //   CONNECTED:4,
      //   CONNECTED_ERROR:5,
      //   CONNECTED_ERROR:5,
      // }
  }
  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}

  onreconnection(reconnectionCounter){return true;}
  onconnectiontimeout(){}

  setWebsocketCallbackUndefined(ws)
  {
    ws.onopen=
    ws.onmessage=
    ws.onerror=
    ws.onclose=undefined;
  }

  _set_aRState(newState)
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

  
  _action_error(ev)
  {
    this._set_aRState(this.aR_states.ERROR);
    return this._action_RECONN();
  }

  _action_remote_disconnection(ev)
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

  _action_connect(url)
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
        this.connectionTimer = setTimeout(()=>{
            this._action_connection_timeout();

            this.onconnectiontimeout();
            
        },this.connectionTimeout_ms);
        // this.state.WS_DB_Insert.binaryType = "arraybuffer";
    
        this.websocket.onopen = (ev)=>{
          
          this.readyState=this.websocket.readyState;
          this.onNewState(this.readyState);
          this._action_connection_open();
          return this.onopen(ev);
        };
        this.websocket.onmessage =(ev)=> this.onmessage(ev);
        this.websocket.onerror =(ev)=>{
          this.readyState=this.websocket.readyState;
          this.onNewState(this.readyState);
          //setTimeout(()=>this._connect(url),10);
          this._action_error(ev);
          return this.onerror(ev);
        }
        this.websocket.onclose =(ev)=>{
          this.readyState=this.websocket.readyState;
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

  _connect(url) {
    //console.log("_connect: curState:",this.aR_state);
    if(this._get_aRState()==this.aR_states.IDLE)
      return this._action_connect(url);
    return false;
  }
  
  
  onNewState(state){
    //console.log(this.curState,state);
    if(this.curState!=state)
    {
      this.onStateUpdate(state,this.curState);
      this.curState=state;
    }
  }
  onStateUpdate(curState,preState){

    //console.log("onStateUpdate",curState,preState);
  }

  close() {
    this._action_local_disconnection();
  }

  send(data){
    return this.websocket.send(data);
  }
}

export class websocket_reqTrack{
  // maxAge_ms: outstanding requests older than this get rejected and evicted
  // by a 5s-cadence sweep. Plugs the round-6 leak where a caller's own timeout
  // would .reject() the promise but leave the trackWindow entry behind, so a
  // late/never ACK accumulated forever until reconnect. 30s is a safe default
  // (longer than every in-tree caller's own timeout); set 0 to disable.
  constructor(websocket,trackKey="req_id",maxAge_ms=30000) {
    let onopen = websocket.onopen;
    let onmessage = websocket.onmessage;
    let onerror = websocket.onerror;
    let onclose = websocket.onclose;
    this.trackKey=trackKey;
    this.trackWindow={};
    if (maxAge_ms > 0) {
      this._sweepHandle = setInterval(() => this._sweepStale(maxAge_ms), 5000);
    }

    websocket.onopen=(ev)=>{
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onopen(ev);
    };
    websocket.onclose=(ev)=>{
      Object.keys(this.trackWindow).forEach(key=>{
        if(this.trackWindow[key].reject!==undefined)
          this.trackWindow[key].reject()
      })
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onclose(ev);
    };
    websocket.onmessage=(ev)=>{
      this.readyState=this.websocket.readyState;
      let p = JSON.parse(ev.data);
      if(p.type=="ACK")
      {
        
        let tKey=p[this.trackKey];
        if(tKey!==undefined)
        {
          let tobj = this.trackWindow[tKey];
          if(tobj!==undefined)
          {
            delete this.trackWindow[tKey];
            tobj.resolve(p);
          }
          else
          {
            tKey=undefined;
          }
        }
        
        if(tKey===undefined){
          
          this.onTrackError({
            type:"ACK_TRACK_ERR",
            data:p
          });
        }
      }
      else
        this.onmessage(ev,p);
    };
    websocket.onerror=(ev)=>{

      Object.keys(this.trackWindow).forEach(key=>{
        if(this.trackWindow[key].reject!==undefined)
          this.trackWindow[key].reject()
      })
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onerror(ev);
    };
    this.websocket=websocket;
  }
  
  onTrackError(ev){}

  _sweepStale(maxAge_ms) {
    const now = Date.now();
    let evicted = 0;
    Object.keys(this.trackWindow).forEach((key) => {
      const t = this.trackWindow[key];
      if (t && typeof t.time === "number" && now - t.time > maxAge_ms) {
        if (typeof t.reject === "function") {
          try { t.reject(new Error("trackWindow timeout (sweep)")); } catch (_) {}
        }
        delete this.trackWindow[key];
        evicted++;
      }
    });
    if (evicted > 0) log.warn("[ws-reqTrack] swept stale requests", { evicted, maxAge_ms });
  }

  destroy() {
    if (this._sweepHandle !== undefined) {
      clearInterval(this._sweepHandle);
      this._sweepHandle = undefined;
    }
  }

  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}


  
  close() {
    let ret = this.websocket.close();
    this.readyState=this.websocket.readyState;
    return ret;
  }


  send_obj(data,replacer){

    let tKey = data[this.trackKey];

    while(tKey===undefined||
      Object.keys(this.trackWindow).reduce((match,id)=>match||id === tKey,false))
      //Check existance
    {
      //if tKey is undefined / exists in the lookup table
      tKey = Math.floor(Math.random()*16777215).toString(16);
    }
    data[this.trackKey] = tKey;


    this.websocket.send(JSON.stringify(data,replacer));
  
    let trackObj={
      time:Date.now(),
      resolve:undefined,
      reject:undefined,
      data:data,
      rsp:undefined
    };
    this.trackWindow[tKey]=trackObj;
    return new Promise((resolve, reject)=>{
      trackObj.resolve=resolve;
      trackObj.reject=reject;
    });
  }
}




export class websocket_aliveTracking
{
  constructor(config)
  {
    config.onStateChange||=()=>{};
    config.connectionTimeout_ms||=5000;
    config.pingpongTimeout_ms||=5000;
    config.reconnectTimeout_ms||=5000;
    config.url||=undefined;
    config.binaryType||="blob";


    config.trackKey||="req_id";
    config.pingPacket||={type:"PING"};

    this.trackWindow={};



    this.states={
      INIT:"INIT",
      NO_URL:"NO_URL",
      CONNECTING:"CONNECTING",
      CONNECTED:"CONNECTED",
      DISCONNECTING:"DISCONNECTING",
      DISCONNECTED:"DISCONNECTED",
      ERROR:"ERROR"
    };

    this.errorInfo={
      NO_ERROR:"NO_ERROR",
      GENERIC:"GENERIC",
      NO_URL:"NO_URL",

    };


    this.ERROR_INFO=this.errorInfo.NO_ERROR;
    
    
    this.acts={
      CONNECT:"CONNECT",
      CONNECT_DONE:"CONNECT_DONE",
      DISCONNECT:"DISCONNECT",
      DISCONNECT_DONE:"DISCONNECT_DONE",

      ERROR:"ERROR"
    };

    this.config=config;

    this.onStateChange=config.onStateChange;
    this.websocket=undefined;

    
    this.pingStat=false;


    this.connectionTimeout=undefined;
    this.connectionTimeout_ms=config.connectionTimeout_ms;

    this.pingpongTimeout=undefined;
    this.pingpongTimeout_ms=config.pingpongTimeout_ms;

    this.reconnectTimeout=undefined;
    this.reconnectTimeout_ms=config.reconnectTimeout_ms;

    if(this.config.url!==undefined)
    {
      this.RESET(this.config.url)
    }
  }

  getURL()
  {
    return this.config.url;
  }

  getErrorInfo()
  {
    return this.ERROR_INFO;
  }
  RESET(url=this.config.url)
  {
    


    this.config.url=url;
    if(this.websocket!==undefined)
    {
      this.close();
    }
    

    this.ERROR_INFO=this.errorInfo.NO_ERROR;

    
    this.connectionTimeout_ms=this.config.connectionTimeout_ms;
    this.pingpongTimeout_ms=this.config.pingpongTimeout_ms;
    this.reconnectTimeout_ms=this.config.reconnectTimeout_ms;
    
    if(this.reconnectTimeout!==undefined)
      clearTimeout(this.reconnectTimeout);
    this.reconnectTimeout=undefined;


    this.connectionTimeout=undefined;
    this.pingpongTimeout=undefined;

    this.pingStat=false;

    if(url===undefined)
    {
      log.warn("[ws] connect called without url; aborting", { reconnects: this.reconnectionCounter });
      this.reconnectTimeout_ms=-1;
      this.state=this.states.INIT;
      this.ERROR_INFO=this.errorInfo.NO_URL;
      this.state=this.stateTransfer(this.state,this.acts.ERROR);
      
      return;
    }

    
    this.state=this.states.INIT;

    this.websocket= new WebSocket(url);
    this.websocket.binaryType=this.config.binaryType;

    this.websocket.onclose=(ev)=>{
      const meta = { url, code: ev && ev.code, reason: ev && ev.reason, reconnects: this.reconnectionCounter };
      if (ev && ev.code === 1006) log.warn("[ws-auto] close 1006 (abnormal)", meta);
      else log.info("[ws-auto] close", meta);
      this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);
      this.onclose(ev)
    };
    this.websocket.onerror=(ev)=>{
      log.warn("[ws-auto] error event", { url, reconnects: this.reconnectionCounter });
      this.ERROR_INFO=this.errorInfo.GENERIC;
      this.state=this.stateTransfer(this.state,this.acts.ERROR);
      this.onerror(ev)
    };
    this.websocket.onopen=(ev)=>{
      this.state=this.stateTransfer(this.state,this.acts.CONNECT_DONE);
      this.onopen(ev)
    };
    this.websocket.onmessage=(ev)=>{
      // console.log(ev)

      if(this.config.trackKey!==undefined)
      {
        let doFindId=false;

        try{

          let msg_obj = JSON.parse(ev.data);
          
          ev.data_obj=msg_obj;
          let tKey=msg_obj[this.config.trackKey];

          if(tKey!==undefined)
          {
            let tobj = this.trackWindow[tKey];
            if(tobj!==undefined)
            {
              delete this.trackWindow[tKey];
              doFindId=true;
              tobj.resolve(msg_obj);
            }
            else
            {
              tKey=undefined;
            }
          }
        }
        catch(e)
        {

        }

        if(!doFindId)
        {
          this.onmessage_obj(ev);
        }

      }
      else
      {
        this.onmessage(ev);
      }
    };
    
    this.state=this.stateTransfer(this.state,this.acts.CONNECT);
  }
  close()
  {
    this.reconnectTimeout_ms=-1;//to stop reconnection
    this._close();
  }
  _close()
  {
    
    this._CLEANUP();
    
    if(this.websocket!==undefined)
    {
      this.websocket.close();
    }
    this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);
    this.websocket=undefined;
  }

  onopen(ev){}
  onmessage(ev){}
  onmessage_obj(ev){}
  onerror(ev){}
  onclose(ev){}


  
  _CLEANUP()
  {
    if(this.websocket!==undefined)
    {
      this.websocket.onclose=undefined;
      this.websocket.onerror=undefined;
      this.websocket.onopen=undefined;
      this.websocket.onmessage=undefined;
    }

    if(this.connectionTimeout!==undefined)
      clearTimeout(this.connectionTimeout);
    this.connectionTimeout=undefined;

    if(this.pingpongTimeout!==undefined)
      clearTimeout(this.pingpongTimeout);
    this.pingpongTimeout=undefined;

    
    Object.keys(this.trackWindow).forEach(key=>{
      if(this.trackWindow[key].reject!==undefined)
        this.trackWindow[key].reject()
    })
    this.trackWindow={};

  }

  PING()
  {
    do{
      
      if(this.websocket===undefined)
      {
        this.state=this.stateTransfer(this.state,this.acts.ERROR);
        break;
      }
      if(this.websocket.readyState!==1)
      {
        this.state=this.stateTransfer(this.state,this.acts.DISCONNECT_DONE);

        break;
      }
      // console.log("PING");
      if(this.pingStat==true)
      {
        log.warn("[ws-auto] ping timeout; closing", { url: this.url });
        this._close();
        break;
      }

      this.pingStat=true;
      this.send_obj(this.config.pingPacket)
      .then(ev=>{
        this.pingStat=false;
      })
      .catch(e=>{
        log.warn("[ws-auto] ping send failed", { url: this.url, err: String(e) });
      });

    }while(false);

    if(this.pingpongTimeout_ms>0)
      this.pingpongTimeout=setTimeout(()=>{this.PING()},this.pingpongTimeout_ms);
  }

  stateTransfer(state,act,info)
  {
    let newState=this._stateTransfer(state,act,info);

    if(newState!==state)
    {
      // console.log(newState,"< (",act,")=",state);
      this.onStateChange(newState,state,act);
      switch(state)//exit
      {
        case this.states.CONNECTING:
          clearTimeout(this.connectionTimeout);
          this.connectionTimeout=undefined;
          break;
        case this.states.CONNECTED:
          
          this.pingStat=true;
          clearTimeout(this.pingpongTimeout);
          this.pingpongTimeout=undefined;
          break;
        case this.states.DISCONNECTING:
          break;
        case this.states.DISCONNECTED:
          break;
        case this.states.ERROR:
          break;
      }

      
      switch(newState)//enter
      {
        case this.states.CONNECTING:
          if(this.connectionTimeout_ms>=0)
          {
            this.connectionTimeout=setTimeout(()=>{
              log.warn("[ws-auto] connection timeout; closing", { url: this.url, timeout_ms: this.connectionTimeout_ms });
              this._close();
            },this.connectionTimeout_ms);
          }
          break;
        case this.states.CONNECTED:
          this.pingStat=false;
          
          if(this.pingpongTimeout_ms>0)
          {
            setTimeout(this.PING.bind(this),1000);
          }
          
          break;
        case this.states.DISCONNECTING:
          break;
        case this.states.DISCONNECTED:
          this._CLEANUP();
          if(this.reconnectTimeout_ms>=0)
          {
            this.reconnectTimeout=setTimeout(()=>this.RESET(),this.reconnectTimeout_ms);
          }
          break;
        case this.states.ERROR:
          this._CLEANUP();
          if(this.reconnectTimeout_ms>=0)
          {
            this.reconnectTimeout=setTimeout(()=>this.RESET(),this.reconnectTimeout_ms);
          }
          break;
      }
    }

    return newState;
    
  }
  _stateTransfer(state,act,info)
  {
    switch(state)
    {
      case this.states.INIT:

        switch(act)
        {
          case this.acts.CONNECT:
            return this.states.CONNECTING;
            break;
          case this.acts.ERROR:
            return this.states.ERROR;
            break;
        }
        break;
      case this.states.CONNECTING:

        switch(act)
        {
          case this.acts.CONNECT_DONE:
            return this.states.CONNECTED;
            break;
          case this.acts.DISCONNECT_DONE:
            return this.states.DISCONNECTED;
            break;
          case this.acts.ERROR:
            return this.states.ERROR;
            break;

        }
        break;
      case this.states.CONNECTED:
        switch(act)
        {
          case this.acts.DISCONNECT://from local
          return this.states.DISCONNECTING;
            break;
          case this.acts.DISCONNECT_DONE:
            return this.states.DISCONNECTED;
            break;
          case this.acts.ERROR:
            return this.states.ERROR;
            break;

        }
        break;
      case this.states.DISCONNECTING:
        switch(act)
        {
          case this.acts.DISCONNECT_DONE:
            return this.states.DISCONNECTED;
            break;
          case this.acts.ERROR:
            return this.states.ERROR;
            break;
        }
        break;
      case this.states.DISCONNECTED:
        // switch(act)
        // {
        //   case this.acts.ERROR:
        //     return this.states.ERROR;
        //     break;
        // }
        break;
      case this.states.ERROR:
        // console.log("You stuck here");
        return this.states.ERROR;
        break;
      default:
        return this.states.ERROR;
        break;
    }
    return state;
  }


  keyGen()
  {
    return Math.floor(Math.random()*16777215).toString(16);
  }

  
  keyGenUnique(preferedKey)
  {
    let key = preferedKey;
    if(key===undefined)
    {
      key=this.keyGen();
    }
    
    while(this.trackWindow[key]!==undefined){//if the key existed
      key = this.keyGen();//re-generate
    }
    return key;
  }
  
  send(data)
  {
    if(this.state !== this.states.CONNECTED)return;
    this.websocket.send(data);
  }
  send_obj(data,replacer){

    let tKey =this.keyGenUnique(data[this.config.trackKey])

    data[this.config.trackKey] = tKey;


    this.send(JSON.stringify(data,replacer));
  
    let trackObj={
      time:Date.now(),
      resolve:undefined,
      reject:undefined,
      data:data,
      rsp:undefined
    };
    this.trackWindow[tKey]=trackObj;
    return new Promise((resolve, reject)=>{
      trackObj.resolve=resolve;
      trackObj.reject=reject;
    });
  }
  
}
