
import BPG_Protocol from './UTIL/BPG_Protocol';

import { GetObjElement} from './UTIL/MISC_Util';
export let CORE_ID= "CORE_ID";

type TYPE_OBJECT ={[key:string]:any}
  
export class BPG_WS
{
  id:string
  reqWindow: Map<string|number,  
    {
      time: number,
      pkts: any[],
      promiseCBs:{
        reject(...arg:any[]):any,
        resolve(...arg:any[]):any,
      }|undefined;
      _PGINFO_:any
    }
  >
  pgIDCounter:number
  websocket:WebSocket|undefined//just for websocket
  isConnected:boolean
  url:string


  constructor(id:string)
  {
    this.id=id;
    this.reqWindow= new Map();
    
    // {"aa":{time:0,pkts:[],promiseCBs:{resolve:()=>{},reject:()=>{}}}};
    this.pgIDCounter= 0;
    this.websocket=undefined;
    this.isConnected=false;
    this.url=""
  }

  connect(info:{url:string})
  {
    let url = info.url;
    this.url=url
    console.log(">>>>",info);
    this.websocket=new WebSocket(url);

    this.websocket.binaryType ="arraybuffer"; 

    this.websocket.onopen=(e:any)=>{
      // ACT_EXT_API_CONNECTED(this.id);//wait until HR arrive
    }
    this.websocket.onclose=(ev:any)=>{
      setTimeout(() => {
        this.connect({url:this.url});
      }, 5*1000);//wait for 5sec and reconnect
      this.onDisconnected();
    }
    
    this.websocket.onerror=(er:any)=>{
      console.log("ERROR::",er);
    }
    this.websocket.onmessage=(ev:any)=>{
      this.onmessage(ev);
    }
  }

  onmessage(evt:any){
    
    //log.info("onMessage:::");
    //log.info(evt);
    if (!(evt.data instanceof ArrayBuffer)) return;
    
    let header = BPG_Protocol.raw2header(evt);
    if(header===undefined)return;
    // log.info("onMessage:["+header.type+"]");
    let pgID = header.pgID;

    let parsed_pkt:any = undefined;
    let SS_start = false;

    switch (header.type) {
      case "HR":
        {
          let HR = BPG_Protocol.raw2obj(evt);
          console.log("HR:",HR);
          let version = GetObjElement(HR,["data","version"])||"_";
          console.log(version)
          this.onConnected();
          

          this.send(
            "HR",0,{ a: ["d"] },undefined,
            {
              reject:(...arg:any[])=>console.error(arg),
              resolve:(...arg:any[])=>console.log(arg),
            })

          this.send( "LD", 0, { filename: "data/default_camera_param.json"},
            undefined, 
            {reject:(...arg:any[])=>console.error(arg),
              resolve: (data) => {
              console.log(data);
          }});

          // comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "HR", 0, { a: ["d"] });
          break;
        }
      case "IM"://special treate unpack image info
        {

          let pkg = BPG_Protocol.raw2obj_IM(evt);
          if(pkg==null)
          {
            parsed_pkt=header;
          }
          else
          {
            let newPacket={...header,image_info:pkg}
            parsed_pkt = newPacket;

          }
          break;
        }
      default:
        {
          
          let report = BPG_Protocol.raw2obj(evt);
          parsed_pkt = report;
          break;
        }
    }

    
    {
      let req_pkt = this.reqWindow.get(pgID);

      if (req_pkt !== undefined)//Find the tracking req
      {
        if (parsed_pkt !== undefined)//There is an act, push into the req acts
          req_pkt.pkts.push(parsed_pkt);

        if (!SS_start && header.type == "SS")//Get the termination session[SS] pkt
        {//remove tracking(reqWindow) info and Dispatch the pkt
          
          let stacked_pkts = req_pkt.pkts;
          if (req_pkt._PGINFO_ === undefined || req_pkt._PGINFO_.keep !== true) {
            this.reqWindow.delete(pgID)
          }
          else {
            req_pkt.pkts = [];
          }
          if (req_pkt.promiseCBs !== undefined) {
            req_pkt.promiseCBs.resolve(stacked_pkts);
          }
          else
          {
            console.error(">>>>promiseCBs is undefined");
          }
          //////
        }

      }
      else//No tracking req info in the window
      {
        if (SS_start)//And it's SS start, put the new tracking info
        {
          this.reqWindow.set(pgID,
            {
              time: new Date().getTime(),
              pkts: [parsed_pkt],
              promiseCBs:undefined,
              _PGINFO_:undefined
            })
        }
        else {
          // let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
          // if (act !== undefined)
          // comp.props.DISPATCH(act);
          console.log("LOSS TRACK pkts", parsed_pkt,this.reqWindow);
        }
      }

    }
  }

  
  cameraDiscovery()
  {
    return new Promise((resolve,reject)=>{

      this.send(
        "CM",0,{type:"discover"},undefined,
        {
          reject:(arg:any[])=>reject(arg),
          resolve:(arg:any[])=>resolve(arg),
        })
    })
  }

  send_P(
    tl:string,
    prop:number,
    data:TYPE_OBJECT|undefined,
    uintArr:Uint8Array|undefined=undefined)
  {
    return new Promise((resolve,reject)=>{
      let ret=this._send({
        tl,
        prop,
        data,//json object
        uintArr,
        promiseCBs:{
          reject:(arg:any[])=>reject(arg),
          resolve:(arg:any[])=>resolve(arg),
        }
      })
      if(ret==false)
      {
        reject(null);
      }

    })
  }
  send(
    tl:string|undefined,
    prop:number,
    data:TYPE_OBJECT|undefined,
    uintArr:Uint8Array|undefined,
    promiseCBs:{
      reject(...arg:any[]):any,
      resolve(...arg:any[]):any,
    })
  {

    return this._send({
      tl,
      prop,
      data,//json object
      uintArr,
      promiseCBs
    })
  }
  _send(info:any)
  {
    if(this.websocket===undefined || this.websocket.readyState!==WebSocket.OPEN)
    {
      if(info.promiseCBs!==undefined)
      {
        info.promiseCBs.reject("Not connected");
      }
      return false;
    }
    let PGID = undefined;
    let PGINFO = undefined;
    if (info.data instanceof Object) {
      PGID = info.data._PGID_;
      PGINFO = info.data._PGINFO_;
      delete info.data["_PGID_"];
      delete info.data["_PGINFO_"];
    }
    if(PGID>65535)
    {
      if(info.promiseCBs!==undefined)
      {
        info.promiseCBs.reject("PGID exceeds 65535");
      }
      return false;
    }
    if (PGID === undefined) {
      let maxNum=500;
      PGID = this.pgIDCounter++;
      
      if(this.pgIDCounter>maxNum)
      {
        this.pgIDCounter=0;
      }
      while(this.reqWindow.get(PGID)!==undefined)
      {
        PGID = this.pgIDCounter++;
        
        if(this.pgIDCounter>maxNum)
        {
          this.pgIDCounter=0;
        }
      }

    }


    if (info.data instanceof Uint8Array) {
      // ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl, data.prop, PGID, null, data.data));
      console.log("");
      throw new Error('Here is not allowed anymore');
    }
    else {
      this.reqWindow.set(PGID,{
        time: new Date().getTime(),
        pkts: [],
        promiseCBs:info.promiseCBs,
        _PGINFO_: PGINFO
      });

      if(info.tl!==undefined)
        this.websocket.send(BPG_Protocol.objbarr2raw(info.tl, info.prop, PGID, info.data, info.uintArr));
    }


    return true;

  }


  close()
  {
    if(this.websocket===undefined)return;
    return this.websocket.close();
  }


  onConnected()
  {

    console.log("onConnected");
  }

  onDisconnected()
  {

    console.log("onDisconnected");
  }
}
export type CORE_API_TYPE=BPG_WS





export let CNC_PERIPHERAL_ID= "CNC_PERIPHERAL_ID";

export class  GenPerif_API
{

  id:string
  CONN_ID:any
  trackingWindow: Map<number|string,  
    {
      reject(...arg:any[]):any,
      resolve(...arg:any[]):any,
    }
  >
  websocket:WebSocket|undefined//just for websocket
  pg_id_channel:number

  connInfo:any
  inReconnection:boolean

  checkReconnectionInterval:number
  runPINGInterval:number
  idCounter:number
  PINGCount:number
  
  machineSetup:{[key:string]:any}|undefined
  constructor(id:string,pg_id_channel:number)
  {
    this.CONN_ID=undefined;
    this.pg_id_channel=pg_id_channel;
    this.id=id;
    this.connInfo=undefined;
    this.inReconnection=false;
    this.checkReconnectionInterval=window.setInterval(()=>this.checkReConnection(),3000);//watch dog to do reconnection
    this.runPINGInterval=window.setInterval(()=>this._sendPing(),3000);//watch dog to do reconnection

    this.trackingWindow=new Map();
    this.idCounter=10;
    this.PINGCount=0;

    this.machineSetup=undefined;

    
  } 
  cleanUpTrackingWindow()
  {
    let keyList =Array.from(this.trackingWindow.keys());
    keyList.forEach(key=>{
      let trackingEntry=this.trackingWindow.get(key);
      if(trackingEntry!=undefined)
      {
        let reject = trackingEntry.reject;
        if(reject !==undefined)
        {
          reject("CONNECTION ERROR");
        }
      }
      this.trackingWindow.delete(key);
    })
  }

  cleanUpConnection()
  {
    this.cleanUpTrackingWindow();
    
  }


  BPG_Send(
    tl:string,
    prop:number,
    data:TYPE_OBJECT|undefined,
    uintArr:Uint8Array|undefined,
    promiseCBs:{
      reject(...arg:any[]):any,
      resolve(...arg:any[]):any,
    }|undefined)
  {

  }
  onDisconnected()
  {
    
  }
  
  onConnected()
  {
    
  }


  protected _onDisconnected()
  {
    this.onDisconnected();
  }
  
  protected _onConnected()
  {
    this.onConnected();
  }

  onInfoUpdate(newInfo:{[key:string]:any})
  {
    
  }
  
  saveMachineSetupIntoFile(filename:string)
  {
    
    this.BPG_Send("SV", 0,
      { filename: filename },
      new TextEncoder().encode(JSON.stringify(this.machineSetup, null, 4)),
      {
        resolve:(res)=>{
          console.log(res);
        }, 
        reject:(res)=>{
          console.log(res);
        }, 
      }
    )
  }


  LoadFileToMachine(filename:string) {
    new Promise((resolve, reject) => {

      // log.info("LoadSettingToMachine step2");
      this.BPG_Send("LD", 0,
        { filename },
        undefined, { resolve, reject }
      );
      setTimeout(() => reject("Timeout"), 1000)
    }).then((pkts:any) => {

      // log.info("LoadSettingToMachine>> step3", pkts);
      if (pkts[0].type != "FL")
      {
        return;
      }
      let machInfo = pkts[0].data;
      
      this.machineSetupUpdate(machInfo,true);
    }).catch((err) => {

      // log.info("LoadSettingToMachine>> step3-error", err);
    })
  }



  machineSetupUpdate(newMachineInfo:{[key:string]:any},doReplace=false)
  {

    this.machineSetup=doReplace==true?newMachineInfo:{...this.machineSetup,...newMachineInfo};
    console.log(this.machineSetup);
    this.onInfoUpdate(this.machineSetup);
    this.send({type:"set_setup",...newMachineInfo},
    (ret)=>{
      //HACK: just assume it will work
      // this.machineSetup={...this.machineSetup,...newMachineInfo};
      // console.log(ret);
    },(e)=>console.log(e));
  }
  
  machineSetupReSync() {
    console.log("*************");
    this.send({type:"get_setup"},
    (ret)=>{
      if(ret["ack"]!=true)
      {
        console.log("get setup failed:",ret);
        return;
      }
      delete ret["type"];
      delete ret["id"];
      delete ret["st"];
      delete ret["ack"];
      this.machineSetup=ret;
      if(this.machineSetup!=undefined)
        this.machineSetupUpdate(this.machineSetup,true);
    },(e)=>console.log(e));
  }

  connect(connInfo:any)
  {
    if(this.inReconnection==true)
    {//still in reconnection state, return
      return false;
    }
    
    this.onDisconnected();
    this.connInfo=connInfo;
    this.inReconnection=true;
    // this.LoadFileToMachine();
    this.BPG_Send("PD", 0, {type:"CONNECT",...connInfo, _PGID_: this.pg_id_channel, _PGINFO_: { keep: true }},undefined,
    {
      resolve: (stacked_pkts) => {
        let PD=stacked_pkts.find((pkt:any)=>pkt.type=="PD");
        this.inReconnection=false;
        if(PD!==undefined)
        {
          let PD_data=PD.data;
          switch(PD_data.type)
          {
            case "MESSAGE":
            {
              let CONN_ID = PD_data.CONN_ID;
              let msg = PD_data.msg;
              let msg_id = msg!==undefined? msg.id:undefined;
              let trwin=this.trackingWindow.get(msg_id);
              if(trwin!==undefined)
              {
                if(trwin.resolve!==undefined)
                  trwin.resolve(msg);
                this.trackingWindow.delete(msg_id);
              }
              else
              {
                console.log(msg);
              }
            }
              break;
            case "DISCONNECT":
              this.CONN_ID=undefined;
              this.cleanUpConnection();
              this.onDisconnected();
              break;
            case "CONNECT":
              this.CONN_ID=PD_data.CONN_ID;
              this.onConnected();

              if(this.machineSetup!==undefined)
              {
                // this.default_pulse_hz = machInfo.pulse_hz;
                this.send({type:"set_setup",...this.machineSetup},
                (ret)=>{
                  this.machineSetupReSync();
                  
                },(e)=>console.log(e));
              }
              else
              {
                this.machineSetupReSync();
              }
              
              break;
          }
        }
      },
      reject:(e)=>{
        this.CONN_ID=undefined;
        this.inReconnection=false;
        this.cleanUpConnection();
        console.log(e);
        this.onDisconnected();
        
      }
    });
  }

  checkReConnection()
  {
    // console.log(this.connInfo,this.connected,this.inReconnection);
    if(this.connInfo===undefined ||this.CONN_ID!==undefined || this.inReconnection==true)
    {
      return;
    }
    this.connect(this.connInfo);
    // this.checkReconnectionTimeout=setTimeout(,);
  }

  // getMachineSetup()
  // {
  //   return this.machineSetup;
  // }

  findAvailableID()
  {
    let id=this.idCounter;
    while(this.trackingWindow.get(id)!==undefined)
    {
      this.idCounter++;
      if(this.idCounter>999999)
      {
        this.idCounter=0;
      }
      id=this.idCounter;
    }
    return id;
  }
  
  _sendPing()
  {
    if(this.CONN_ID===undefined)return ;


    if(this.PINGCount>=2)
    {
      //time to disconnect
      this.PINGCount=0;
      
      this.connect(this.connInfo);
      return;
    }
    this.PINGCount++;
    // console.log(this.CONN_ID);

    this.triggerPing();
    // this.machineSetupUpdate({pulse_hz:0});
  }
  triggerPing()
  {
    
    this.send({type:"PING"},(ret:any)=>{
      // console.log(ret);
      delete ret["type"]
      delete ret["id"]
      delete ret["st"]
      let machineStatus={...ret};
      let res_count=machineStatus.res_count||{OK:0,NG:0,NA:0};

      let currentTime_ms=new Date().getTime();

      this.PINGCount=0;
    },(errorInfo:any)=>console.log(errorInfo));

  }
  send(data:{
    [key:string]:any
  },resolve:(...arg: any[])=> any,
    reject: (...arg: any[])=> any)
  {
    if(this.CONN_ID===undefined)
    {
      reject("CONN ID is not set");
      return ;
    }


    if(data.id!==undefined )
    {
      if(this.trackingWindow.get(data.id)!==undefined)
        reject(`ID ${data.id} collision`);
    }
    else
    {
      data.id=this.findAvailableID();
    }
    this.trackingWindow.set(data.id,{resolve,reject});

    this.BPG_Send("PD", 0, //just send
    {
      msg:data,
      CONN_ID:this.CONN_ID,
      type:"MESSAGE"
    },undefined, {
      resolve:d=>d,
      reject:d=>console.log(d)
    });
    


  }
  
}

export class CNC_Perif extends GenPerif_API
{
  gcodeSeq:string[]
  isSendWaiting:boolean
  constructor(id:string,pg_id_channel:number)
  {
    super(id,pg_id_channel);
    this.gcodeSeq=[]
    this.isSendWaiting=false
  }

  
  KickSendGCodeQ()
  {
    if(this.isSendWaiting==true || this.gcodeSeq.length==0)
    {
      return;
    }
    const gcode = this.gcodeSeq.shift();
    if(gcode==undefined || gcode==null)return;
    this.isSendWaiting=true;

    this.send({"type":"GCODE","code":gcode},
      (ret)=>{
        console.log(ret)
        this.isSendWaiting=false;
        this.KickSendGCodeQ();
      },(e)=>console.log(e));
  }

  pushGCode(GCodeArr:string[])
  {
    this.gcodeSeq=this.gcodeSeq.concat(GCodeArr);
    this.KickSendGCodeQ();
  }


  protected _onDisconnected()
  {
    this.gcodeSeq=[];
    this.isSendWaiting=false;
    this.onDisconnected();
  }
  
  protected _onConnected()
  {
    this.onConnected();
  }
}



export class  InspCamera_API
{

  id:string
  stream_pg_id:number
  constructor(id:string,stream_pg_id:number)
  {
    this.stream_pg_id=stream_pg_id;
    this.id=id;
    
  } 


  BPG_Send(
    tl:string,
    prop:number,
    data:TYPE_OBJECT|undefined,
    uintArr:Uint8Array|undefined,
    promiseCBs:{
      reject(...arg:any[]):any,
      resolve(...arg:any[]):any,
    }|undefined)
  {

  }
  onDisconnected()
  {
    
  }
  
  onConnected()
  {
    
  }


  protected _onDisconnected()
  {
    this.onDisconnected();
  }
  
  protected _onConnected()
  {
    this.onConnected();
  }

  onInfoUpdate(newInfo:{[key:string]:any})
  {
    
  }
  

  connect(camInfo:any)
  {
    this.BPG_Send(
    "CM",0,{
      type:"connect",
      driver_idx:camInfo.driver_idx,
      cam_idx:camInfo.cam_idx,
      misc:camInfo.misc,
      _PGID_:this.stream_pg_id,
      _PGINFO_:{keep:true}
    },undefined,
    {
      reject:(arg:any[])=>console.error(arg),
      resolve:(stacked_pkts:any[])=>{

        // let PD=stacked_pkts.find((pkt:any)=>pkt.type=="PD");
        console.log(stacked_pkts);
        
      }
    })
  }

  checkReConnection()
  {
  }

  checkConnection()
  {
  }

  
}