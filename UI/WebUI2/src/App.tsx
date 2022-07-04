import React from 'react';
import { useState, useEffect,useRef,useMemo } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch } from 'antd';

import type { MenuProps, MenuTheme } from 'antd/es/menu';
const { SubMenu } = Menu;
import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,MoreOutlined,PlayCircleOutlined,
  DisconnectOutlined,LinkOutlined } from '@ant-design/icons';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign} from './UTIL/MISC_Util';

import {listCMDPromise} from './XCMD';



import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message } from 'antd';


import { type_CameraInfo,type_IMCM} from './AppTypes';
import './basic.css';



type MenuItem = Required<MenuProps>['items'][number];

var enc = new TextEncoder();

const _DEF_FOLDER_PATH_="data/Test1.xprj";
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



function APPUI()
{
  return <></>
}



function CameraSetupEditUI({camSetupInfo,fetchCoreAPI,onCameraSetupUpdate}:{ camSetupInfo:type_CameraInfo, fetchCoreAPI:()=>Promise<BPG_WS>,onCameraSetupUpdate:(caminfo:type_CameraInfo)=>void}){

  const _this = useRef<any>({

    imgCanvas:document.createElement('canvas')
  }).current;

  const [Local_IMCM,setLocal_IMCM]=
    useState<type_IMCM|undefined>(undefined);
  
  useEffect(()=>{//load default

    (async ()=>{
      let api = await fetchCoreAPI()
      await api.CameraSetChannelID([camSetupInfo.id],51009,{
        resolve:(pkts)=>{
          // console.log(pkts);
          let IM=pkts.find((p:any)=>p.type=="IM");
          if(IM===undefined)return;
          let CM=pkts.find((p:any)=>p.type=="CM");
          if(CM===undefined)return;
          // console.log("++++++++\n",IM,CM);
          let IMCM={
            image_info:IM.image_info,
            camera_id:CM.data.camera_id,
            trigger_id:CM.data.trigger_id,
            trigger_tag:CM.data.trigger_tag,
          } as type_IMCM

          _this.imgCanvas.width = IMCM.image_info.width;
          _this.imgCanvas.height = IMCM.image_info.height;

          let ctx2nd = _this.imgCanvas.getContext('2d');
          ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


          setLocal_IMCM(IMCM)
          // console.log(IMCM)

        },
        reject:(pkts)=>{

        }
      })
      await api.CameraSetup(camSetupInfo,0);
    })()

    return ()=>{
    }
  },[])
  return <> 
    {/* <pre>{
      JSON.stringify(camSetupInfo,null,2)
    }</pre> */}
    esposure:<InputNumber value={camSetupInfo.exposure} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,exposure:num})
    }}/>
    <br/>
    analog_gain:<InputNumber value={camSetupInfo.analog_gain} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,analog_gain:num})
    }}/>
    <br/>
    frame_rate:<InputNumber value={camSetupInfo.frame_rate} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,frame_rate:num})
    }}/>
    <br/>
    gamma:<InputNumber value={camSetupInfo.gamma} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,gamma:num})
    }}/>
    <br/>
    black_level:<InputNumber value={camSetupInfo.black_level} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,black_level:num})
    }}/>
    <br/>
    <Switch checkedChildren="反X" unCheckedChildren="正X" checked={camSetupInfo.mirrorX} onChange={(check)=>{
      onCameraSetupUpdate({...camSetupInfo,mirrorX:check})
    }}/>
    <Switch checkedChildren="反Y" unCheckedChildren="正Y" checked={camSetupInfo.mirrorY} onChange={(check)=>{
      onCameraSetupUpdate({...camSetupInfo,mirrorY:check})
    }}/>
    
    <br/>
    <HookCanvasComponent style={{height:"300px"}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      // _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);
      if(ctrl_or_draw==true)//ctrl
      {
      }
      else//draw
      {
        if(Local_IMCM!==undefined)
        {
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.translate(-Local_IMCM.image_info.full_width/2,-Local_IMCM.image_info.full_height/2);
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       

        let ctx = g.ctx;
        
      }
    }
    }/>
  </>
}

let DAT_ANY_UNDEF:any=undefined;


function VIEWUI(){
  const _this = useRef<any>({}).current;
  
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));

  const [defConfig,setDefConfig]=useState<any>(undefined);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);


  const [defReport,setDefReport]=useState<any>(undefined);

  async function getAPI(API_ID:string=CORE_ID)
  {
    let api=await new Promise((resolve,reject)=>{
      ACT_EXT_API_ACCESS(API_ID,(api)=>{
        if(api===undefined)reject();
        resolve(api) 
      })
    });

    return api;
  }
  
  async function getCoreAPI()
  {
   
    return await getAPI(CORE_ID)as BPG_WS;
  }
  
  
  const emptyModalInfo={
    timeTag:0,
    visible:false,
    type:"",
    onOK:()=>{},
    onCancel:()=>{},
    title:"",
    DATA:DAT_ANY_UNDEF,
    content:DAT_ANY_UNDEF

  }
  const [modalInfo,setModalInfo]=useState(emptyModalInfo);

  async function updateDefInfo(_defInfo:any,camTrigInfo:{id:string,trigger_tag:string,trigger_id:number,img_path:string|undefined}|undefined=undefined)
  {

    let api = await getAPI(CORE_ID)as BPG_WS
    console.log("+===========",_defInfo);
    let reloadRes = await api.InspTargetUpdate(_defInfo,INSP1_REP_PGID_);
    console.log("+===========",reloadRes);

    api.send(undefined,0,{_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
      resolve:(e)=>{
        let RP = e.find((pkt:any)=>pkt.type=="RP");
        if(RP===undefined)
        {
          setDefReport(undefined)
          return;
        }

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
    let api = await getAPI(CORE_ID)as BPG_WS
    let main= await api.FILE_Load(PrjDefFolderPath+"/main.json");

    
    let InspTars_main:any[]=[];
    {
      let InspTars=main.InspTars;
      let InspTars_ids:string[]= InspTars.map((t:{id:string})=>t.id)
      for(let id of InspTars_ids)
      {
        let path = PrjDefFolderPath+"/it_"+id+"/main.json"
        InspTars_main.push(await  api.FILE_Load(path));
      }
    }

    let cameraIdCollect=InspTars_main.map(it=>it.camera_id).filter(id=>id!==undefined);
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



    let XCmds=await  api.FILE_Load( PrjDefFolderPath+"/XCmds.json");
    return {
      path:PrjDefFolderPath,
      _folderInfo:await api.Folder_Struct(PrjDefFolderPath,9),
      main,
      InspTars_main,
      XCmds
    }
  }

  async function SavePrjDef(PrjDefFolderPath:string,PrjDef:(any))
  {

    let api = await getAPI(CORE_ID)as BPG_WS
    await api.FILE_Save(PrjDefFolderPath+"/main.json",PrjDef.main,true)
    await api.FILE_Save(PrjDefFolderPath+"/XCmds.json",PrjDef.XCmds,true)

    for(let it of PrjDef.InspTars_main)
    {
      let path = PrjDefFolderPath+"/it_"+it.id+"/main.json"
      await api.FILE_Save(path,it,true)
    }
    return true
  }
  async function CameraInfoDoConnection(CameraInfo:type_CameraInfo[],froceReconnect=false)
  {
    let api = await getAPI(CORE_ID)as BPG_WS
    let connCameraInfo = await api.CameraCheckAndConnect(CameraInfo,froceReconnect)
    return CameraInfo.map((ci:type_CameraInfo)=>{
     let connTar = connCameraInfo.find(cCam=>cCam.id==ci.id);
     return {...ci,available:connTar!==undefined}
    })
  }
  async function ReloadPrjDef(path:string)
  {
    let prjDef = await LOADPrjDef( path)
    console.log(prjDef)
    let api = await getAPI(CORE_ID)as BPG_WS

    prjDef.main.CameraInfo= await CameraInfoDoConnection(prjDef.main.CameraInfo,true)
    // updateDefInfo();
    await api.InspTargetRemoveAll()

    for(let inspTar of prjDef.InspTars_main)
    {
      
      let id=inspTar.id;

      // console.log(id,inspTar)
      await api.InspTargetCreate(inspTar,12000);
    }


    // InspTargetReload(defInfo:any,_PGID_:number)
    // ddd
    setDefConfig(prjDef);
    console.log(prjDef)
  }

  useEffect(()=>{//load default

    ReloadPrjDef(_DEF_FOLDER_PATH_)
    .catch(e=>{
      console.log(e)
    })
  },[])
  // console.log(defInfo);
  // return <>
  //  <APPUI></APPUI>
  // </>

  function getItem(
    label: React.ReactNode,
    key?: React.Key | null,
    icon?: React.ReactNode,
    children?: MenuItem[],
    disabled=false
  ): MenuItem {
    return {
      key,
      icon,
      children,
      label,
      disabled
    } as MenuItem;
  }


  let cameraMenu=
  getItem('Camera', 'cam',undefined, [
    ...(
      defConfig===undefined?[]:
      (defConfig.main.CameraInfo
        .map((cam:type_CameraInfo,index:number)=>
          ( getItem(<div onClick={()=>{
            let keyTime=Date.now();//use time as key to force CameraSetupEditUI remount
            function updater(ncamInfo:type_CameraInfo){
              
              setModalInfo({...emptyModalInfo,
                title:cam.id,
                visible:true,
                content:<CameraSetupEditUI key={keyTime} fetchCoreAPI={getCoreAPI} camSetupInfo={ncamInfo}  onCameraSetupUpdate={ncam=>{
                  console.log(ncam)
                  updater(ncam);
                  (async function(){
                    let api = await getAPI(CORE_ID)as BPG_WS
                    console.log(ncam);
                    await api.CameraSetup(ncam,0);
                  })()
                  

                }}
                />,
                
                onOK:()=>{

                  let new_defConfig= ObjShellingAssign(defConfig,["main","CameraInfo"],defConfig.main.CameraInfo);
                  new_defConfig.main.CameraInfo[index]=ncamInfo;
                  
                  (async function(){
                    let api = await getCoreAPI()
                    await api.CameraSetup(ncamInfo,2);
                    await api.CameraClearTriggerInfo();
                  })()
                  // console.log(setModalInfo);
                  setDefConfig(new_defConfig)
                  setModalInfo(emptyModalInfo)
                  
                },
                
                onCancel:()=>{
                  
                  (async function(){
                    let api = await getCoreAPI()
                    await api.CameraSetup(cam,2);
                    await api.CameraClearTriggerInfo();
                  })()
                  
                  setModalInfo(emptyModalInfo)
                }
              })
            }
            updater(cam);

            console.log(cam,index)}
          
          
          }>{cam.id}</div>,cam.id,
          cam.available?<LinkOutlined/>:<DisconnectOutlined/>) )))
    ),
    getItem(<Dropdown
      trigger={["click"]}
      disabled={cameraQueryList===undefined}
      overlay={<>
        <Menu>
          {
            cameraQueryList===undefined || cameraQueryList.length===0?
              <Menu.Item disabled danger>
              <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                just a sec...
              </a>
              </Menu.Item>
              :
              cameraQueryList.map(cam=><Menu.Item key={cam.id} 
              onClick={()=>{
                console.log(defConfig.main.CameraInfo)
                console.log(cam)
                cam.available=false;
                let new_camInfo=[...defConfig.main.CameraInfo,cam];
                
                CameraInfoDoConnection(new_camInfo).then(result_camInfo=>{

                  let new_defConfig= ObjShellingAssign(defConfig,["main","CameraInfo"],result_camInfo);
                  setDefConfig(new_defConfig)
                });
              }}>
                {cam.id}
              </Menu.Item>)

          }
        </Menu>
      </>}
    ><div onClick={()=>{

      setCameraQueryList(undefined);
      (async ()=>{
        let api = await getAPI(CORE_ID)as BPG_WS
        let camList = await api.queryDiscoverList();
        setCameraQueryList(camList);
        console.log(camList)
      })();
      
    }}>+
    </div>
    </Dropdown>, 'Add'),
  ])


  const items: MenuItem[] = [
    cameraMenu
  ];

  let siderUI=
  <Sider width={200}>

    <Menu mode="inline" theme="dark" selectable={false}

      items={items}
    >

              
      {/* <SubMenu key="INSP" title="Camera" >
        
        {
          defConfig===undefined?<></>:
          defConfig.main.CameraInfo.map((cam:type_CameraInfo)=>(
          <Menu.Item key={cam.id} onClick={()=>{}}>
            {cam.id}
          </Menu.Item>))
        }

        <Menu.Item key="Add">
          +
        </Menu.Item>
      </SubMenu> */}

    </Menu>

  </Sider>



  return <>

    <Layout style={{ height: '100%' }}>
    <Header style={{ width: '100%' }}>

    <Menu theme="dark" mode="horizontal" selectable={false}>
        <Menu.Item key="1" onClick={()=>{
        }}>Calib_Param_Edit</Menu.Item>
        <Menu.Item key="2" onClick={()=>{
          SavePrjDef(_DEF_FOLDER_PATH_,defConfig);
        }}>SAVE</Menu.Item>
    </Menu>
    

    
    <Modal
        title={modalInfo.title}
        visible={modalInfo.visible}
        onOk={modalInfo.onOK}
        // confirmLoading={confirmLoading}
        onCancel={modalInfo.onCancel}
      >
        {modalInfo.content}
    </Modal>


    </Header>

    <Layout>
    {siderUI}
    
    <Content className="site-layout" style={{ padding: '0 0px'}}>
    Content
    </Content>
  
    </Layout>
  
    </Layout>
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
