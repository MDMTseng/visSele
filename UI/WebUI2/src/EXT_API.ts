
import BPG_Protocol from './UTIL/BPG_Protocol';

import { GetObjElement} from './UTIL/MISC_Util';
import { type_CameraInfo} from './AppTypes';



export let CORE_ID= "CORE_ID";

type TYPE_OBJECT ={[key:string]:any}
  

var enc = new TextEncoder();
export class BPG_WS
{
  id:string
  reqWindow: Map<string|number,  
    {
      time: number,
      pkts: any[],
      latest_cached_pkts: any[],
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
    // console.log("onMessage:["+header.type+"]");
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
            req_pkt.latest_cached_pkts=req_pkt.pkts;
            req_pkt.pkts = [];
          }
          if (req_pkt.promiseCBs !== undefined) {
            req_pkt.promiseCBs.resolve(stacked_pkts);
            if(req_pkt._PGINFO_!==undefined && req_pkt._PGINFO_.hookedCBs!==undefined)
            {
              let hookedCBs=req_pkt._PGINFO_.hookedCBs;
              Object.keys(hookedCBs).forEach(key=>{
                hookedCBs[key].resolve(stacked_pkts);
              })
            }
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
              latest_cached_pkts: [],
              promiseCBs:undefined,
              _PGINFO_:undefined
            })
        }
        else {
          // let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
          // if (act !== undefined)
          // comp.props.DISPATCH(act);
          console.log("UNTRACK pkts", parsed_pkt,this.reqWindow);
        }
      }

    }
  }

  
  send_P(
    tl:string|undefined,
    prop:number,
    data?:TYPE_OBJECT,
    uintArr?:Uint8Array)
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


  send_cbs_attach(PGID:number,cbs_key:string,promiseCBs:{
    reject(...arg:any[]):any,
    resolve(...arg:any[]):any,
  })
  {
    
    let req_pkt = this.reqWindow.get(PGID);
    if(req_pkt===undefined)
    {
      req_pkt={
        time: new Date().getTime(),
        pkts: [],
        latest_cached_pkts: [],
        promiseCBs:undefined,
        _PGINFO_: undefined
      }
      this.reqWindow.set(PGID,req_pkt);
    }

    if(req_pkt._PGINFO_===undefined)
    {
      req_pkt._PGINFO_={};
    }
    
    if(req_pkt._PGINFO_.hookedCBs===undefined)
    {
      req_pkt._PGINFO_.hookedCBs={};
    }

    req_pkt._PGINFO_.hookedCBs[cbs_key]=promiseCBs;
    if(req_pkt.latest_cached_pkts.length>0)
    {
      promiseCBs.resolve(req_pkt.latest_cached_pkts);
    }
  }
  send_cbs_detach(PGID:number,cbs_key:string|undefined)
  {

    let req_pkt = this.reqWindow.get(PGID);
    if(req_pkt===undefined)return;

    if(cbs_key===undefined)
    {
      req_pkt._PGINFO_.hookedCBs={};
      return;
    }
    if(req_pkt._PGINFO_===undefined)return;
    
    if(req_pkt._PGINFO_.hookedCBs===undefined)return;
    delete req_pkt._PGINFO_.hookedCBs[cbs_key];
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
  _send(info:{
    tl:string|undefined,
    prop:number,
    data:TYPE_OBJECT|undefined,
    uintArr:Uint8Array|undefined,
    promiseCBs:{
      reject(...arg:any[]):any,
      resolve(...arg:any[]):any,
    }
  })
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
    // console.log(info.data,PGID);
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
      
      let set_PGINFO_=PGINFO;
      
      let req_pkt = this.reqWindow.get(PGID);
      if(req_pkt!==undefined)
      {
        set_PGINFO_= {...req_pkt._PGINFO_,...PGINFO};
      }

      if(PGID===51000)
      {
        console.log(set_PGINFO_);
      }

      this.reqWindow.set(PGID,{
        time: new Date().getTime(),
        pkts: [],
        latest_cached_pkts: [],
        promiseCBs:info.promiseCBs,
        _PGINFO_: set_PGINFO_
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


  async queryDiscoverList()
  {
    let cameraListInfos=await this.send_P(
      "CM",0,{type:"discover",do_refresh:true}) as any[];

    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"
    console.log(CM.data);
    return CM.data as {name:string,id:string,driver_name:string}[];
  }

  
  async queryConnCamList()
  {
    let cameraListInfos=await this.send_P("CM",0,{type:"connected_camera_list"}) as any[];
    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"

    let camList = CM.data as {name:string,id:string,driver_name:string}[];


    return camList;
  }

  
  
  async InjectImage(imagePath:string,tags:string[],triggerID:number)
  {
    return this.send_P(
      "CM",0,{
        type:"trigger",
        img_path:imagePath,
        trigger_tags:tags,
        trigger_id:triggerID,
        channel_id:-1
      })
  }

  async CameraSWTrigger(camera_id:string,ttags:string|string[],trigger_id:number,doTriggerInfoMocking:boolean=true)
  {
    let trigger_tag:string|undefined=undefined;
    let trigger_tags:string[]|undefined=undefined;
    //check if trigger_tag is string array
    if(typeof ttags==="string")
    {
      trigger_tag=ttags;
    }
    else if(ttags instanceof Array)
    {
      trigger_tags=ttags;
    }
    return this.send_P(
      "CM",0,{
        type:"trigger",
        soft_trigger:true,
        mocking_trigger_info:doTriggerInfoMocking,
        id:camera_id,
        trigger_tag,
        trigger_tags,
        // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
        trigger_id:trigger_id,
      })
  }

  CameraSetChannelID(camera_id_List:string[],channel_id:number,
    cbs:{ reject(...arg: any[]): any; resolve(...arg: any[]): any; })
  {
    let connctResult=camera_id_List.map((cam_id,idx)=>
      this.send("CM",0,{
        type:"set_camera_channel_id",
        id:cam_id,
        _PGID_:channel_id,_PGINFO_:{keep:true}
      },undefined,cbs)
    )
  }
  

  async DisconnectAllCamera()
  {
    let connCamList=await this.queryConnCamList();
    
    let disconnInfo = await Promise.all(
      connCamList.map(cam=>this.send_P("CM",0,{type:"disconnect",id:cam.id}))
    )

    return disconnInfo;
  }

  async isConnectedCameraFullfillList(camera_id_List:string[])
  {
    let connCamList=await this.queryConnCamList();
    for(let i=0;i<camera_id_List.length;i++)
    {
      let cam_id=camera_id_List[i];
      let cam = connCamList.find(caminfo=>caminfo.id==cam_id);
      if(cam===undefined) return false;
    }
    return true;
  }


  async CameraSetup(camSetup:type_CameraInfo,trigger_mode=2)//0 continuous   1:software trigger   2:any trigger
  {
    return await this.send_P("CM",0,{
      ...camSetup,
      type:"setting",
      trigger_mode,
    }).then((camInfoPkts:any)=>camInfoPkts[0].data)
    
  }

  
  async CameraTriggerInfoMocking(camID:string,tag:string="_STREAM_",doRemove:boolean=false)//0 continuous   1:software trigger   2:any trigger
  {
    return await this.send_P("CM",0,{
      type:"trigger_info_mocking",
      id:camID,
      tag,
      remove:doRemove,
    })
    
  }

  async CameraClearTriggerInfo()
  {
    return await this.send_P("CM",0,{
      type:"clean_trigger_info_matching_buffer"
    })
  }
  async CameraCheckAndConnect(camera_setup_List:type_CameraInfo[],froceDisconnectAll=false)
  {
    if(froceDisconnectAll)
      await this.DisconnectAllCamera();
    let camera_id_List=camera_setup_List.map(info=>info.id)
    let isFullfill= await this.isConnectedCameraFullfillList(camera_id_List)

    if(isFullfill==false)
    {
      await this.DisconnectAllCamera();
      let discoveredCam = await  this.queryDiscoverList();
      let cameraList:{
        name: string;
        id: string;
        available:boolean,
        driver_name: string;
      }[]=[];
      

      console.log(camera_id_List,discoveredCam);
      for(let i=0;i<camera_id_List.length;i++)
      {
        let camid=camera_id_List[i];
        let caminfoFound = discoveredCam.find(cam=>cam.id==camid);
        // if(caminfoFound===undefined)throw "2";
        let available=caminfoFound!==undefined
        if(caminfoFound===undefined)
        {
          caminfoFound={
            id:camid,
            name:"",
            driver_name:""
          }
        }
        // else
        cameraList.push({...caminfoFound,available});
      }


      // let cameraList=camera_id_List.map((camid)=>discoveredCam.find(cam=>cam.id==camid))
      console.log(cameraList);



      let connctResult=await Promise.all(cameraList.map(cam=>
        this.send_P("CM",0,{
          type:"connect",
          misc:"data/BMP_carousel_test1",
          id:cam.id}).then((camInfoPkts:any)=>camInfoPkts[0].data)
        
        ));//
      console.log(connctResult);


      for( let camQueryInfo of cameraList )
      {
        let camSetup=camera_setup_List.find(csl=>csl.id==camQueryInfo.id)
        if(camSetup===undefined)continue;

        await this.CameraSetup(camSetup,2)
      }

    }

    return await this.queryConnCamList();
  }





  FILE_Save(filename:string,jobj={},humanReadable=false){
    let jstr="";
    if(humanReadable)
      jstr= JSON.stringify(jobj, null, 2)
    else 
      jstr= JSON.stringify(jobj)


    return this.send_P("SV",0,{filename,make_dir:true},enc.encode(jstr))
  }
  

  FILE_Save_Raw=(filename:string,content:Uint8Array) => {
    
    return this.send_P("SV",0,{filename},content)
  }




  async FILE_Load (filename:string) {
    
    let data = await this.send_P("LD",0,{filename}) as any;

    let FL=data.find((info:any)=>info.type=="FL")
    if(FL===undefined)
    {
      throw "Read Failed  FL is undefined";
    }
    
    return FL.data;
  }


  async Folder_Struct(path:string,depth=1) {
    
    let data = await this.send_P("FB",0,{path,depth}) as any;

    let FS=data.find((info:any)=>info.type=="FS")
    if(FS===undefined)
    {
      throw "Read Failed  FS is undefined";
    }
    
    return FS.data;
  }

  async FileRename(filePath_from:string,filePath_to:string) {
    
    return await this.send_P("SV",0,{filename:filePath_to,type:"rename",from:filePath_from}) as any;
  }

  // const [defInfo,setDefInfo]=useState(defInfo_Default)

  
  // let setDefInfoCritUpdate=(_defInfo:typeof defInfo)=>{
  //   setDefInfo(_defInfo);
  //   _setDefInfoCritUpdate(defInfoCritUpdate+1);
  // }
  



  async InspTargetSetGlobalVariable(new_global_variable:any)
  {
    return await this.send_P("IT",0,{type:"global_value",data:new_global_variable})
  }

  async InspTargetRemoveAll()
  {
    return await this.send_P("IT",0,{type:"delete_all"})
  }


  async InspTargetGetInfo()
  {
    return await this.send_P("IT",0,{type:"list"})
  }
  
  async InspTargetCreate(defInfo:any,env_path:string)
  {
    await this.send_P("IT",0,{type:"create",id:defInfo.id,defInfo,env_path})
  }
  async InspTargetUpdate(defInfo:any)
  {
    await this.send_P("IT",0,{type:"update",id:defInfo.id,defInfo})
  }
  async InspTargetExchange(inspTarId:string,data:any,_PGID_:number|undefined=undefined)
  {
    
    return await this.send_P("IT",0,{type:"exchange",id:inspTarId,data,_PGID_})
  }









  StrInfoDec(str:string) {



    let info = str.replace(/\.[^/.]+$/, "").split(' ').reduce((info:any, seg:string) => {
      let idx =seg.indexOf("=");
      let key = seg.substring(0,idx);
      let cont = seg.substring(idx+1);
      
      {
        let fnum=parseFloat(cont);
        if(fnum==fnum)
        {
          return {
            ...info,
            [key]: fnum
          }
        }
      }


      if (seg.startsWith("$[") && seg.charAt[seg.length-1]=="]") {
        return {
          ...info,
          [key]: seg.substring(idx+3,seg.length-1).split(',')
        }
      }


      if (key=="tags" || key=="atags" ) {//cheat
        return {
          ...info,
          [key]: seg.substring(idx+1,seg.length).split(',')
        }
      }


      if ((seg.charAt[0]=="{" &&seg.charAt[seg.length-1]=="}") || (seg.charAt[0]=="[" &&seg.charAt[seg.length-1]=="]"))
      {
        try{


          return {
            ...info,
            [key]: JSON.parse(cont)
          }
        }
        catch(e)
        {

        }
      }

      return {
        ...info,
        [key]: cont
      }
    }, {
    }) as any;
  
    return info;
  }

  Enc2StrInfo(json:any){
    let str=""
    Object.keys(json).forEach((key)=>{
        let v=json[key];


        if((key =="tags" || key =="atags" )&& v.length==0)return;
        str+=key+"=";
        if(key =="tags" || key =="atags")
        {
            v.forEach((t:string,idx:number)=>{
                if(idx!=v.length-1)
                {
                    str+=t+","
                }
                else
                {
                    str+=t
                }
            })
            str+=" "
            return;
        }
        if(typeof v == 'number')
        {

            str+=v

            str+=" "
            return;
        }
    })
    if(str=="")return undefined;
    return str.slice(0,-1);//remove last " "
  }

  FileNnameParse(name:string) {



    let imgInfo = name.replace(/\.[^/.]+$/, "").split(' ').reduce((info:any, seg:string) => {
      if (seg.startsWith('t=')) {
        return {
          ...info,
          t: parseInt(seg.slice(2))
        }
      }
      if (seg.startsWith('tid=')) {
        return {
          ...info,
          tid: parseInt(seg.slice(4))
        }
      }
      if (seg.startsWith('tags=')) {
        return {
          ...info,
          tags: seg.slice(5).split(',')
        }
      }
    }, {
      t: undefined,
      tid: undefined,
      tags: []
    }) as {
      t:number|undefined,
      tid:number|undefined,
      tags:string[]
    };
  
    return imgInfo;
  }


  FileStructFilter(folderStruct:any, filter:(fileInfo:{
    name:string,
    type:string,
    size_bytes:number,
    atime_ms:number,
    ctime_ms:number,
    mtime_ms:number,
  },folderPath:string)=>boolean|undefined) {
    if (folderStruct === undefined || folderStruct.files === undefined) return folderStruct;
    // console.log(folderStruct);
  
    let newStruct = { ...folderStruct };
    newStruct.files = [];
    for (let i = 0; i < folderStruct.files.length; i++) {
      let file = folderStruct.files[i];
      if (file.type == "DIR") {
        let newFilderInfo = { ...file };
        newFilderInfo.struct = this.FileStructFilter(file.struct, filter);
        newStruct.files.push(newFilderInfo);
      }
      else if (file.type == "REG") {
        if(filter(file,newStruct.path)==true)
          newStruct.files.push(file);
  
      }
    }
    return newStruct;
  }

  FileStructFilter_XX(folderStruct:any, partial_tags:string[], trigger_tags:string[], trigger_id:number|undefined, start_time_ms:number = NaN, end_time_ms:number = NaN) {
    if (folderStruct === undefined || folderStruct.files === undefined) return folderStruct;
    // console.log(folderStruct);
  
    let newStruct = { ...folderStruct };
    newStruct.files = [];
    for (let i = 0; i < folderStruct.files.length; i++) {
      let file = folderStruct.files[i];
      if (file.type == "DIR") {
        let newFilderInfo = { ...file };
        newFilderInfo.struct = this.FileStructFilter_XX(file.struct, partial_tags, trigger_tags, trigger_id);
        newStruct.files.push(newFilderInfo);
      }
      else if (file.type == "REG") {
        let imgInfo = this.FileNnameParse(file.name);
  
        if (imgInfo === undefined) continue;
        if (imgInfo.tags === undefined) continue;
        if (trigger_id !== undefined && imgInfo.tid != trigger_id) continue;
  
        if (imgInfo.t !== undefined) {
          if (imgInfo.t < start_time_ms) continue;
          if (imgInfo.t > end_time_ms) continue;
        }
  
        let findAll = true;
        for (let j = 0; j < partial_tags.length; j++) {
  
          if (imgInfo.tags.find((tag:string) => tag.includes(partial_tags[j])) === undefined) {
            findAll = false;
            break;
          }
        }
  
  
        if (findAll == false) continue;
  
  
        for (let j = 0; j < trigger_tags.length; j++) {
  
          if (imgInfo.tags.find((tag:string) => tag == trigger_tags[j]) === undefined) {
            findAll = false;
            break;
          }
        }
  
        if (findAll == false) continue;
  
        newStruct.files.push({
          ...file,
          info: imgInfo
        });
  
      }
    }
    return newStruct;
  }
  
  FileStructFlatten(folderStruct:any):any {
    if (folderStruct === undefined) return [];
    let FileList = [];
    let path = folderStruct.path;
  
    if (folderStruct.files === undefined) return [];
    for (let i = 0; i < folderStruct.files.length; i++) {
  
      let file = folderStruct.files[i];
      if (file.type == "DIR") {
        FileList.push(this.FileStructFlatten(file.struct))
      }
  
      else if (file.type == "REG") {
        FileList.push({
          ...file,
          path: path
        })
      }
    }
    return FileList.flat();
  }

  async InspTargetEnvPath(inspTarId:string)
  {
          
    let pkts=await this.InspTargetExchange(inspTarId,{type:"get_info"}) as any[];

    
    let IFInfo=pkts.find((pkt)=>pkt.type=="IF");
    if(IFInfo===undefined || IFInfo.data===undefined)
    {
      return undefined
    }

    return IFInfo.data.env_path;
  }


  async InspTargetEnvFolderStructure(inspTarId:string,subpath:string="",depth:number=1)
  {
    let env_path = await this.InspTargetEnvPath(inspTarId)
    if(env_path === undefined)
    {
      return undefined
    }
    return (await this.Folder_Struct(env_path+"/"+subpath,depth));
  }

  
  async ImageLoadFromInspTar_ex(IMSV_IT_id:string, sub_path:string, partial_tags:string[], trigger_tags:string[], trigger_id:number|undefined, folderDepth = 2, candSelectCB:(((candList:any)=>Promise<any>)|undefined) = undefined) {
    let fStruct = await this.InspTargetEnvFolderStructure(IMSV_IT_id, sub_path, folderDepth);
  
    let filteredStruct = this.FileStructFilter_XX(fStruct, partial_tags, trigger_tags, trigger_id);
  
    // console.log(filteredStruct,FileStructFlatten(filteredStruct))
  
    let candList = this.FileStructFlatten(filteredStruct);
    // let path=fStruct.path
    // let imageInfo =(fStruct.files).filter((finfo) => /.+\.png/i.test(finfo.name))
  
    // console.log(path,imageInfo);
  
    if (candList.length > 0) {
      let cand = undefined
      if (candSelectCB === undefined)
        cand = candList[0]
      else {
        cand = await candSelectCB(candList);
      }
      if(cand===undefined)return undefined;
      // console.log(cand.info.tags);
      await this.InjectImage(cand.path + "/" + cand.name, cand.info.tags, cand.info.tid)
      return cand;
    }
    return undefined;
  }
  
















  async InspTargetSetStreamChannelID(inspTarId:string,channel_id:number,
    cbs:{ reject(...arg: any[]): any; resolve(...arg: any[]): any; })
  {
    this.send("IT",0,{
      type:"exchange",
      id:inspTarId,
      data:{type:"stream_info",stream_id:channel_id},
      _PGID_:channel_id,_PGINFO_:{keep:true}
    },undefined,cbs)
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
  isConnected:boolean
  checkReconnectionInterval:number
  runPINGInterval:number
  idCounter:number
  PINGCount:number
  
  machineSetup:{[key:string]:any}|undefined
  pingHeartBeatEnable:boolean
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
    this.pingHeartBeatEnable=true;
    this.isConnected=false;
    
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


  _onDisconnected()
  {
    this.trackingWindow=new Map();
    this.onDisconnected();
  }
  
  _onConnected()
  {
    this.trackingWindow=new Map();
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
      console.log(ret);
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
              this._onDisconnected();  
              this.isConnected=false;
              break;
            case "CONNECT":
              this.CONN_ID=PD_data.CONN_ID;
              this._onConnected();

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
        this._onDisconnected();
        
      }
    });
  }
  disconnect()
  {
    //TODO
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

  enablePING(enable:boolean=true)
  {
    this.pingHeartBeatEnable=enable;
    if(enable==false)
    {
      this.PINGCount=0;
    }
  }

  _sendPing()
  {
    if(this.CONN_ID===undefined)return ;

    if(this.pingHeartBeatEnable==false)return;

    if(this.PINGCount>=2)
    {
      //time to disconnect
      this.PINGCount=0; 
      this.isConnected=false;
      this.CONN_ID=undefined;
      // this.machineSetup=undefined;
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

      this.isConnected=true;
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
      {
        reject(`ID ${data.id} collision`);
        return false;
      }
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
    
    return true;

  }

  send_P(data:{
    [key:string]:any
  })
  {
    return new Promise((resolve,reject)=>this.send(data,resolve,reject))
  }
  
}

export class CNC_Perif extends GenPerif_API
{
  constructor(id:string,pg_id_channel:number)
  {
    super(id,pg_id_channel);
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