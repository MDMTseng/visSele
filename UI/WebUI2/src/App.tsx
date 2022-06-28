import React from 'react';
import { useState, useEffect,useRef,useMemo } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio } from 'antd';
const { SubMenu } = Menu;

import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,MoreOutlined,PlayCircleOutlined } from '@ant-design/icons';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign} from './UTIL/MISC_Util';

import {listCMDPromise} from './XCMD';



import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message } from 'antd';


import './basic.css';

// import ReactJsoneditor from 'jsoneditor-for-react';

// declare module 'jsoneditor-react'jsoneditor-for-react"

// import 'jsoneditor-react/es/editor.min.css';

let INSP1_REP_PGID_ = 51000

let HACK_do_Camera_Check=false;
const { TabPane } = Tabs;
const { Header, Content, Footer,Sider } = Layout;
type type_InspDef={
  name:string,
  
  cameraList:{id:string,[key:string]:any}[]

  rules:{
    name:string,
    id:string
    [key:string]:any
  }[]
}


type type_CameraInfo={
  id:string,
  driver_name:string,
  model:string,
  name: string,
  serial_nbr:string,
  vendor:string,

  analog_gain:number,
  exposure:number,
  RGain:number,
  GGain:number,
  BGain:number,
  gamma:number,
  frame_rate:number,
}

class CameraMan{

}


class CameraSet{
  
}



var enc = new TextEncoder();

const _DEF_FOLDER_PATH_="data/Test1.xprj";
function VIEWUI(){
  const _this = useRef<any>({}).current;
  
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));



  const ACT_FILE_Save= async(filename:string,jobj={},humanReadable=false) => {
    let jstr="";
    if(humanReadable)
      jstr= JSON.stringify(jobj, null, 2)
    else 
      jstr= JSON.stringify(jobj)

    let api=await getAPI(CORE_ID) as BPG_WS;

    return await api.send_P("SV",0,{filename,make_dir:true},enc.encode(jstr))
  }
  

  const ACT_FILE_Save_Raw= async(filename:string,content:Uint8Array) => {
    
    let api=await getAPI(CORE_ID) as BPG_WS;

    return await api.send_P("SV",0,{filename},content)
  }

  const ACT_FILE_Load= async (filename:string) => {
    
    let api=await getAPI(CORE_ID) as BPG_WS;
    let data = await api.send_P("LD",0,{filename}) as any;

    let FL=data.find((info:any)=>info.type=="FL")
    if(FL===undefined)
    {
      throw "Read Failed  FL is undefined";
    }
    
    return FL.data;
  }


  const ACT_Folder_Struct= async (path:string,depth=1) => {
    
    let api=await getAPI(CORE_ID) as BPG_WS;
    let data = await api.send_P("FB",0,{path,depth}) as any;

    let FS=data.find((info:any)=>info.type=="FS")
    if(FS===undefined)
    {
      throw "Read Failed  FS is undefined";
    }
    
    return FS.data;
  }



  async function queryConnCamList()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let cameraListInfos=await api.send_P("CM",0,{type:"connected_camera_list"}) as any[];
    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"

    let camList = CM.data as {name:string,id:string,driver_name:string}[];


    return camList;
  }


  async function queryDiscoverList()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let cameraListInfos=await api.cameraDiscovery() as any[];
    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"
    console.log(CM.data);
    return CM.data as {name:string,id:string,driver_name:string}[];
  }


  async function DisconnectAllCamera()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let connCamList=await queryConnCamList();
    
    let disconnInfo = await Promise.all(
      connCamList.map(cam=>api.send_P("CM",0,{type:"disconnect",id:cam.id}))
    )

    return disconnInfo;
  }

  async function isConnectedCameraFullfillList(camera_id_List:string[])
  {
    let connCamList=await queryConnCamList();
    for(let i=0;i<camera_id_List.length;i++)
    {
      let cam_id=camera_id_List[i];
      let cam = connCamList.find(caminfo=>caminfo.id==cam_id);
      if(cam===undefined) return false;
    }
    return true;
  }



  async function CameraCheckAndConnect(camera_setup_List:type_CameraInfo[],froceDisconnectAll=false)
  {
    if(froceDisconnectAll)
      await DisconnectAllCamera();
    let api=await getAPI(CORE_ID) as BPG_WS;
    let camera_id_List=camera_setup_List.map(info=>info.id)
    let isFullfill= await isConnectedCameraFullfillList(camera_id_List)

    if(isFullfill==false)
    {
      await DisconnectAllCamera();
      let discoveredCam = await queryDiscoverList();
      let cameraList:{
        name: string;
        id: string;
        driver_name: string;
      }[]=[];
      

      console.log(camera_id_List,discoveredCam);
      for(let i=0;i<camera_id_List.length;i++)
      {
        let camid=camera_id_List[i];
        let caminfoFound = discoveredCam.find(cam=>cam.id==camid);
        if(caminfoFound===undefined)throw "2";
        cameraList.push(caminfoFound);
      }


      // let cameraList=camera_id_List.map((camid)=>discoveredCam.find(cam=>cam.id==camid))
      console.log(cameraList);



      let connctResult=await Promise.all(cameraList.map(cam=>
        api.send_P("CM",0,{
          type:"connect",
          misc:"data/BMP_carousel_test1",
          id:cam.id}).then((camInfoPkts:any)=>camInfoPkts[0].data)
        
        ));//
      console.log(connctResult);


      for( let camQueryInfo of cameraList )
      {
        let camSetup=camera_setup_List.find(csl=>csl.id==camQueryInfo.id)
        if(camSetup===undefined)continue;
        api.send_P("CM",0,{
          ...camSetup,
          type:"setting",
          trigger_mode:2,
        }).then((camInfoPkts:any)=>camInfoPkts[0].data)
      }

    }

    return await queryConnCamList();
  }


  // const [defInfo,setDefInfo]=useState(defInfo_Default)

  
  // let setDefInfoCritUpdate=(_defInfo:typeof defInfo)=>{
  //   setDefInfo(_defInfo);
  //   _setDefInfoCritUpdate(defInfoCritUpdate+1);
  // }
  const [defReport,setDefReport]=useState<any>(undefined);
  

  async function getAPI(API_ID:string)
  {
    let api=await new Promise((resolve,reject)=>{
      ACT_EXT_API_ACCESS(API_ID,(api)=>{
        if(api===undefined)reject();
        resolve(api) 
      })
    });

    return api;
  }
  async function InspTargetReload(defInfo:any,_PGID_:number)
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let ret = await api.send_P("IT",0,{type:"delete_all"})
    await api.send_P("IT",0,{type:"create",id:"XXX",defInfo,_PGID_:_PGID_,_PGINFO_:{keep:true}})
      

    return await api.send_P("IT",0,{type:"list"});
  }

  function CameraSetChannelID(camera_id_List:string[],channel_id:number,
    cbs:{ reject(...arg: any[]): any; resolve(...arg: any[]): any; })
  {
    ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
      let api=_api as BPG_WS
      let connctResult=camera_id_List.map((cam_id,idx)=>
        api.send("CM",0,{
          type:"set_camera_channel_id",
          id:cam_id,
          _PGID_:channel_id,_PGINFO_:{keep:true}
          
          
        },undefined,cbs)
      )
      
    })
  }



  function updateDefInfo_(_defInfo:any,camTrigInfo:{id:string,trigger_tag:string,trigger_id:number,img_path:string|undefined}|undefined=undefined)
  {

    return InspTargetReload(_defInfo,INSP1_REP_PGID_).then(e=>{
      console.log(e)
      ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
        let api=_api as BPG_WS
        api.send(undefined,0,{_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
          resolve:(e)=>{
            let RP = e.find((pkt:any)=>pkt.type=="RP");
            if(RP===undefined)
            {
              setDefReport(undefined)
              return;
            }
  
            console.log(RP.data)
            setDefReport(RP.data)
            
            // console.log(e)
          },
          reject:(e)=>{
            console.log(e)
          },
        })

        if(camTrigInfo!==undefined)
          api.send_P(
            "CM",0,{
              type:"trigger",
              soft_trigger:true,
              id:camTrigInfo.id,
              trigger_tag:camTrigInfo.trigger_tag,
              // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
              trigger_id:camTrigInfo.trigger_id,
              img_path:camTrigInfo.img_path,
              channel_id:50201
            })

      })
    })
  }




  

  async function updateDefInfo(_defInfo:any,camTrigInfo:{id:string,trigger_tag:string,trigger_id:number,img_path:string|undefined}|undefined=undefined)
  {

    console.log("+===========",_defInfo);
    let reloadRes = await InspTargetReload(_defInfo,INSP1_REP_PGID_);
    console.log("+===========",reloadRes);
    let api = await getAPI(CORE_ID)as BPG_WS

    api.send(undefined,0,{_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
      resolve:(e)=>{
        let RP = e.find((pkt:any)=>pkt.type=="RP");
        if(RP===undefined)
        {
          setDefReport(undefined)
          return;
        }

        console.log(RP.data)
        setDefReport(RP.data)
        
        // console.log(e)
      },
      reject:(e)=>{
        console.log(e)
      },
    })



    if(camTrigInfo!==undefined)
      api.send_P(
        "CM",0,{
          type:"trigger",
          soft_trigger:true,
          id:camTrigInfo.id,
          trigger_tag:camTrigInfo.trigger_tag,
          // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
          trigger_id:camTrigInfo.trigger_id,
          img_path:camTrigInfo.img_path,
          channel_id:50201
        })

    return reloadRes;

  }

  async function LOADPrjDef(PrjDefFolderPath:string)
  {
    let main= await ACT_FILE_Load(PrjDefFolderPath+"/main.json");

    
    let InspTars_main:any[]=[];
    {
      let InspTars=main.InspTars;
      let InspTars_ids:string[]= InspTars.map((t:{id:string})=>t.id)
      for(let id of InspTars_ids)
      {
        let path = PrjDefFolderPath+"/it_"+id+"/main.json"
        InspTars_main.push(await ACT_FILE_Load(path));
      }
    }

    let cameraIdCollect=InspTars_main.map(it=>it.camera_id);
    for(let camId of cameraIdCollect)//Add empty camera info that's not in the cameras collection
    {
      let camInfoIdx= main.CameraInfo.findIndex((cinfo:type_CameraInfo)=>cinfo.id===camId)
      if(camInfoIdx===-1)
      {
        main.CameraInfo.add({
          id:camId
        })
      }
    }



    let XCmds=await ACT_FILE_Load( PrjDefFolderPath+"/XCmds.json");
    return {
      path:PrjDefFolderPath,
      _folderInfo:await ACT_Folder_Struct(PrjDefFolderPath,9),
      main,
      InspTars_main,
      XCmds
    }
  }

  async function SavePrjDef(PrjDefFolderPath:string,PrjDef:(any))
  {

    await ACT_FILE_Save(PrjDefFolderPath+"/main.json",PrjDef.main,true)
    await ACT_FILE_Save(PrjDefFolderPath+"/XCmds.json",PrjDef.XCmds,true)

    for(let it of PrjDef.InspTars_main)
    {
      let path = PrjDefFolderPath+"/it_"+it.id+"/main.json"
      await ACT_FILE_Save(path,it,true)
    }
    return true
  }

  useEffect(()=>{//load default

    (async()=>{
      let prjDef = await LOADPrjDef( _DEF_FOLDER_PATH_)

      let connCameraInfo = await CameraCheckAndConnect(prjDef.main.CameraInfo,true)
      // updateDefInfo();

      console.log(prjDef)
      console.log(connCameraInfo)
    })()
    .then(r=>{
      // console.log(r)
    })
  },[])
  // console.log(defInfo);
  return <>
   111
  </>

}



function App() {
  
  const _ = useRef<any>({
  });
  let _this=_.current;
  const dispatch = useDispatch();
  const CORE_API_INFO = useSelector((state:StoreTypes) => state.EXT_API[CORE_ID]);


  const ACT_EXT_API_REGISTER= (...p:Parameters<typeof EXT_API_REGISTER>) => dispatch(EXT_API_REGISTER(...p));
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const ACT_EXT_API_UPDATE= (...p:Parameters<typeof EXT_API_UPDATE>) => dispatch(EXT_API_UPDATE(...p));
  const ACT_EXT_API_CONNECTED= (...p:Parameters<typeof EXT_API_CONNECTED>) => dispatch(EXT_API_CONNECTED(...p));
  const ACT_EXT_API_DISCONNECTED= (...p:Parameters<typeof EXT_API_DISCONNECTED>) => dispatch(EXT_API_DISCONNECTED(...p));

  // const [camList,setCamList]=useState<{[key:string]:{[key:string]:any,list:any[]}}>({});
  useEffect(() => {
    
    let core_api=new BPG_WS(CORE_ID);
    core_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CORE_ID);


    ACT_EXT_API_REGISTER(core_api.id,core_api);
    
    core_api.connect({
      url:"ws://localhost:4090"
    });




    let CNC_api=new CNC_Perif(CNC_PERIPHERAL_ID,20666);

    {
      CNC_api.onConnected=()=>ACT_EXT_API_CONNECTED(CNC_PERIPHERAL_ID);
  
      CNC_api.onInfoUpdate=(info:[key: string])=>ACT_EXT_API_UPDATE(CNC_api.id,info);
  
      CNC_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CNC_PERIPHERAL_ID);
      
      CNC_api.BPG_Send=core_api.send.bind(core_api);
  
      ACT_EXT_API_REGISTER(CNC_api.id,CNC_api);
    }
    






    core_api.onConnected=()=>{
      ACT_EXT_API_CONNECTED(CORE_ID);

      CNC_api.connect({
        uart_name:"COM5",
        baudrate:115200
      });
    }

    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, []); 
  if(GetObjElement(CORE_API_INFO,["state"])!=1)
  {
    return <div>Wait....</div>;
  }
  return  <VIEWUI/>;

}

export default App;
