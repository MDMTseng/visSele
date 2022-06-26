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


class CameraMan{

}


class CameraSet{
  
}



const _DEF_FOLDER_PATH_="data/Test1.xprj";
function VIEWUI(){
  const _this = useRef<any>({}).current;
  
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));



  const ACT_FILE_Save= async(filename:string,content:Uint8Array) => {
    
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



  const [defInfoCritUpdate,_setDefInfoCritUpdate]=useState(0)

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

    
    let cameraInfo= await ACT_FILE_Load(PrjDefFolderPath+"/cameraInfo.json");

    
    // console.log(InspTars_ids);

    // let InspTars_main=await Promise.all(
    //   InspTars
    //   .map((t:{id:string})=>t.id)
    //   .map((id:string)=> ACT_FILE_Load(PrjDefFolderPath+"/it_"+id+"/main.js")));

    // console.log(InspTars_main);
    return {
      path:PrjDefFolderPath,
      folderInfo:await ACT_Folder_Struct(PrjDefFolderPath,9),
      main,
      cameraInfo,
      InspTars_main
      // cameras:
    }
  }

  useEffect(()=>{//load default


    LOADPrjDef( _DEF_FOLDER_PATH_)
    .then((e)=>{
      console.log(e);
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
