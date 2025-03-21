import React from 'react';
import { useState, useEffect,useRef,useMemo,useContext, useCallback,createContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch,Select,TreeSelect } from 'antd';
import type { TreeSelectProps } from 'antd';

import {CameraSetupEditUI} from './CameraSetupEditUI';

import { DraggableModal, DraggableModalProvider } from 'ant-design-draggable-modal'
import 'ant-design-draggable-modal/dist/index.css'

import ReactFlow, {
  MiniMap,
  Controls,
  Background,
  useNodesState,
  useEdgesState,
  addEdge,
} from 'reactflow';

import 'reactflow/dist/style.css';

// import ResponsiveReactGridLayout from 'react-grid-layout';

import { ResponsiveReactGridLayoutX } from './UICardComp';
import type { MenuProps, MenuTheme } from 'antd/es/menu';
import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,
  DisconnectOutlined,LinkOutlined,CopyOutlined,LoadingOutlined,ReloadOutlined,BorderOuterOutlined,FullscreenOutlined,FullscreenExitOutlined  } from '@ant-design/icons';
import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign,ObjReccursiveOverride} from './UTIL/MISC_Util';
import { DDDD } from './InspTarConfigUI';

import {listCMDPromise} from './XCMD';


import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg,PtRotate2d} from './UTIL/MathTools';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message,Space,Popover } from 'antd';


import { type_CameraInfo,type_IMCM} from './AppTypes';
import './basic.css';


import {InspTargetUI_MUX ,InspTargetTypes} from './InspTarView';

import {UtilUI_MUX,UtilUI_TYPES} from './UtilUIView';

import {ObjTree,IMCM_type,EDIT_PERMIT_FLAG} from './SingleTargetVIEWUI_UTIL';


import { info } from 'console';
import { type } from '@testing-library/user-event/dist/type';

const { Option } = Select;
const { SubMenu } = Menu;
  

export type CompParam_GlobalVariable = {
  global_variable: any,

  // global_variable_selector: () =>Promise<string|undefined>,
  set_global_variable: ((path: string[],new_value:any) => void) | undefined,
}

export const ITGlobalVariableContext = createContext<CompParam_GlobalVariable>({
  global_variable: {},
  // global_variable_selector: () => { return new Promise<string>((resolve, reject) => { reject("") }) },
  set_global_variable: (ngv)=>{},
});



let WidgetWSegs=60;
let WidgetHSegs=40;
// let WidgetSegHeight=20;



type IMCM_group={[trigID:string]:IMCM_type}


  

type MenuItem = Required<MenuProps>['items'][number];

var enc = new TextEncoder();

// const _DEF_FOLDER_PATH_="data/Test1_xprj";

const _DEF_FOLDER_PATH_="data/Pack_xprj";
// import ReactJsoneditor from 'jsoneditor-for-react';

// declare module 'jsoneditor-react'jsoneditor-for-react"

// import 'jsoneditor-react/es/editor.min.css';


const { TabPane } = Tabs;
const { Header, Content, Footer,Sider } = Layout;



// {
//   readonly:boolean,
//   style?:any,
//   width:string,height:string,
//   renderHook:((ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,rule:any)=>void)|undefined,
//   IMCM_group:{[trigID:string]:IMCM_type},
//   rule:any,
//   report:any,
//   onDefChange:(updatedRule:any,doInspUpdate:boolean)=>void}





function UICard_Config({inspTarList,config,onConfChange}:{inspTarList:any[],config:any,onConfChange:(newConf:any)=>any})
{


  // const [newUIEleID,setNewUIEleID]= useState("");
  // const [newUIEleType,setNewUIEleType]= useState("");
  
  let _config={id:"",type:"",...config};


  function setCompleteFlag(config:any)
  {
    return {...config,complete:(config.id.length!==0 && config.type.length!==0 )}
  }

  let SubSelUI:any=null;

  function randomUUID(len:number=10)
  {
    return "U_"+Math.random().toString(36).substring(2, 2+len);
  }
  switch(_config.type)
  {
    case "InspTar":
      
    SubSelUI=<><br/>
    

    
    InspTarID:
    <Dropdown
    trigger={["click"]}
    overlay={<>
      <Menu>
        {
          inspTarList.map(it=><Menu.Item key={it.id} 
            onClick={()=>{

              let card_id=_config.id;
              if(card_id=="" || card_id.startsWith("$"))
              {
                card_id="$IT_"+it.id;
              }
              onConfChange(setCompleteFlag({..._config,itid:it.id,ittype:it.type,id:card_id}))

        
            }}>
              {it.id}
            </Menu.Item>)
        }
        </Menu>
      </>}
    >
      
      <a onClick={(e) => e.preventDefault()}>
        <Space>
          {_config.itid}
          <DownOutlined />
        </Space>
      </a>


    </Dropdown>
    
    
    </>
    break;

    case "Util":
      
    SubSelUI=<><br/>
    
    
    ddss:
    <Dropdown
    trigger={["click"]}
    overlay={<>
      <Menu>
        {
          UtilUI_TYPES.map(U_Typoe=><Menu.Item key={U_Typoe} 
            onClick={()=>{


              let card_id=_config.id;
              if(card_id=="" || card_id.startsWith("$"))
              {
                card_id="$U_"+Date.now()+"_"+U_Typoe;
              }
              onConfChange(setCompleteFlag({..._config,itid:card_id,ittype:U_Typoe,id:card_id}))

        
            }}>
              {U_Typoe}
            </Menu.Item>)
        }
        </Menu>
      </>}
    >
      
      <a onClick={(e) => e.preventDefault()}>
        <Space>
          {_config.type}
          <DownOutlined />
        </Space>
      </a>


    </Dropdown>
    
    
    
    
    
    
    
    </>
    break;

  }

  return <>
  
  

    <Input value={_config.id} placeholder={"卡片名稱"}
      onChange={(e)=>{
        let value=e.target.value;
        // setNewUIEleID(value);
        onConfChange(setCompleteFlag({..._config,id:value}))
      }}
      />




    <br/>
    <Dropdown
    trigger={["click"]}
    overlay={<>
      <Menu>
        {
          ["InspTar","Util"].map(uitype=><Menu.Item key={uitype} 
            onClick={()=>{
              // let newDefConfig=ObjShellingAssign(defConfig,["main","WidgetLayout",idx],{...uilayoutInfo,type:uitype})
              // onDefChange(newDefConfig)

              onConfChange(setCompleteFlag({..._config,type:uitype}))
            }}>
              {uitype}
            </Menu.Item>)

        }
        </Menu>
      </>}
    >
      
      <a onClick={(e) => e.preventDefault()}>
        <Space>
          {_config.type}
          <DownOutlined />
        </Space>
      </a>


    </Dropdown>
    <br/>


    {SubSelUI}
        
  </>
}







function TargetViewUIShow({globalVariable,WidgetSetID,defConfig,UIEditFlag,EditPermitFlag,onDefChange,defDoReload,onDefDelete,renderHook}:{globalVariable:any,WidgetSetID:string,defConfig:any,UIEditFlag:boolean,EditPermitFlag:number, 
  onDefChange:(updatedDef:any,updateIdx:number)=>void,
  defDoReload:(it_id:string)=>void
  onDefDelete:(it_id:string)=>void,
  renderHook:any},

)
{
  
  const _this = useRef<any>({
    apiTable:{}

  }).current;
  const [newUIEleConf,setNewUIEleConf]= useState<any>({});
  const [FSIdx,setFSIdx]= useState<number>(-1);
  let InspTarList=defConfig.InspTars_main;

  // let displayInspTarIdx:number[]=[];
  // let displayInspTarIdx_hide:number[]=[];
  // if(defConfig!==undefined)
  // {
  //   displayInspTarIdx=displayIDList
  //     .map(itarID=>InspTarList.findIndex((itar:any)=>itar.id==itarID))
  //     .filter(idx=>idx>=0)
  
  //   displayInspTarIdx_hide
  //     =InspTarList.map((itar:any,idx:number)=>idx).filter((idx:number)=>{
  //       if(displayInspTarIdx.find(idx_to_show=>idx_to_show==idx)===undefined)
  //         return true;
  //       return false;
  //     });
  
  // }

  useEffect(()=>{//load default
    console.log(">>TargetViewUIShow>>>>>>>>>>>>>>");
  },[])


  useEffect(()=>{//load default
    setNewUIEleConf({});
  },[UIEditFlag])


  useEffect(()=>{
    console.log("WidgetSetID:",WidgetSetID);
    _this.apiTable={};
    setFSIdx(-1);
  },[WidgetSetID])
  // console.log(InspTarList);
  // console.log(displayIDList,displayInspTarIdx,displayInspTarIdx_hide);




  let defalutLayoutInfo={
    "w": 1,
    "h": 1,
    "x": 0,
    "y": 999,
    "i":undefined,
    "isDraggable": true,
    "isResizable": true
    }

  let WidgetTabKey=(GetObjElement(defConfig,["main","UIInfo"])??[])
    .findIndex((info:any)=>info.id==WidgetSetID);
  if(WidgetTabKey<0)WidgetTabKey=0;

  
  let WidgetLayout=GetObjElement(defConfig,["main","UIInfo",WidgetTabKey,"WidgetLayout"])??[];
  let WidgetInfo=GetObjElement(defConfig,["main","UIInfo",WidgetTabKey,"WidgetInfo"])??[];
  //cross check
  WidgetLayout=WidgetLayout.filter((layout:any)=>WidgetInfo.find((info:any)=>info.id==layout.i)!==undefined);
  WidgetInfo=WidgetInfo.filter((info:any)=>WidgetLayout.find((layout:any)=>layout.i==info.id)!==undefined);

  function updateWidgetLayout(newWidgetInfo :any,new_WidgetLayout:any)
  {
    console.log("updateWidgetLayout",newWidgetInfo,new_WidgetLayout);
    let newDefConfig=defConfig;
    if(newDefConfig.main.UIInfo===undefined)
    {
      newDefConfig.main.UIInfo=[];
    }
    if(newWidgetInfo!==undefined)
      newDefConfig=ObjShellingAssign(newDefConfig,["main","UIInfo",WidgetTabKey,"WidgetInfo"],newWidgetInfo)

    if(new_WidgetLayout!==undefined)
      newDefConfig=ObjShellingAssign(newDefConfig,["main","UIInfo",WidgetTabKey,"WidgetLayout"],new_WidgetLayout)
    // console.log(newDefConfig);
    if(newWidgetInfo!==undefined || new_WidgetLayout!==undefined)
    {
      console.log("updateWidgetLayout",newWidgetInfo,new_WidgetLayout);
      onDefChange(newDefConfig,-12)
    }
  }

  // WidgetLayout=InspTarList.map((itar:any)=>{
  //   let layoutInfo=WidgetLayout.find((layoutInfo:any)=>layoutInfo.i==itar.id);
  //   return layoutInfo?layoutInfo:{...defalutLayoutInfo,"i": itar.id};
  // });



  WidgetLayout=WidgetLayout.map((ilayout:any)=>({
    ...ilayout,
    isDraggable:UIEditFlag,
    isResizable:UIEditFlag
  }));



  
  let ID_ADD_NEW_ELE="ADD_NEW_ELE"
  WidgetLayout=WidgetLayout.filter((uii:any)=>uii.i!==ID_ADD_NEW_ELE);
  
  if(UIEditFlag)
  {
    WidgetLayout.push({...defalutLayoutInfo,i:ID_ADD_NEW_ELE,type:ID_ADD_NEW_ELE,w:4,h:2})
  }





  let layoutSrcEle=WidgetLayout.map((layoutInfo:any)=>{//make sure the order of WidgetLayout and WidgetInfo is same
    let tatId=layoutInfo.i;

    //  let eleInfo = InspTarList.find((it:any)=>it.id==tatId);
    //  if(eleInfo)return eleInfo;

    let eleInfo = WidgetInfo.find((uu:any)=>uu.id==tatId);
    



    if(eleInfo)
    {
      if(eleInfo.type=="InspTar" && eleInfo.itid!==undefined)//if it's inspTar, attach it's def
      {
        let itDef = InspTarList.find((it:any)=>it.id==eleInfo.itid);
        eleInfo={...eleInfo,inspTarDef:itDef}
      }
      return eleInfo;
    }

    return undefined;
  })
  // console.log(WidgetLayout);


  
  let ID_CLOSE_FS="CLOSE_FS"
  if(FSIdx!=-1)
  {
    WidgetLayout=WidgetLayout.map((lao:any)=>{
      return {...lao,display:false,y:999,w:1,h:1}
    });
    WidgetLayout.push({...defalutLayoutInfo,i:ID_CLOSE_FS,type:ID_CLOSE_FS,w:WidgetWSegs,h:1,y:0,x:0})

    WidgetLayout[FSIdx]={...WidgetLayout[FSIdx]};
    WidgetLayout[FSIdx].display=true;
    WidgetLayout[FSIdx].x=0;
    WidgetLayout[FSIdx].y=1;
    WidgetLayout[FSIdx].w=WidgetWSegs;
    WidgetLayout[FSIdx].h=WidgetHSegs-1;


  }

  return <ResponsiveReactGridLayoutX layouts={{lg:WidgetLayout}}
    style={{height:"100%",overflow:"scroll",background:FSIdx!=-1?"rgba(255,0,0,0.1)":undefined}}
      //  onDrop={(e) => onDrop(e)} 
    onLayoutChange={(curL,allL)=>{
      if(FSIdx!=-1)return;
      console.log(curL,allL)

      let newWidgetLayout=WidgetLayout.map((layo:any,index:number)=>{
        return {...layo,...curL[index]}
      })
      
      updateWidgetLayout(undefined,newWidgetLayout);

    }}
    className="layout" 
    //  layouts={layouts} 
    breakpoints={{ lg: 4, md: 3, sm: 2, xs: 1, xxs: 0 }}//{{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
    cols={{ lg: WidgetWSegs, md: WidgetWSegs, sm: WidgetWSegs, xs: WidgetWSegs, xxs: WidgetWSegs }}
    rows={WidgetHSegs}
    // rowHeight={WidgetSegHeight}
    //  // rowHeight={300}
    //  // width={1000}
    resizeHandles={["se"]}
    isDroppable={true}
    autoSize={true}
    >
    {/* <div key="a" style={{ backgroundColor: "#ccc" }}><span>a</span></div>
    <div key="b" style={{ backgroundColor: "#ccc" }}>b</div>
    <div key="c" style={{ backgroundColor: "#ccc" }}>c</div>
    <div key="d" style={{ backgroundColor: "#ccc" }}> */}

      {WidgetLayout.map((uilayoutInfo:any,idx:number)=>{
        // console.log(uilayoutInfo);
        // if(layoutSrcEle[idx]===undefined)return null;

        let UI:JSX.Element=<></>
        let UIEditUI:JSX.Element=<></>


        let type=layoutSrcEle[idx]?.type??uilayoutInfo.type;//use layoutSrcEle to fetch type first, if not found, use WidgetInfo to fetch type(usually for temporary UI ie.ADD_NEW_ELE....)
        switch(type)
        {
          case "InspTar":
          {



            console.log(">>>>>layoutSrcEle>>>>>>",defConfig,layoutSrcEle);
            if(layoutSrcEle[idx].inspTarDef===undefined)
            {
              console.log(layoutSrcEle[idx],uilayoutInfo);
              UI= <>檢驗項目已移除: 原InspTar id:{layoutSrcEle[idx].itid}  type:{layoutSrcEle[idx].ittype}</>;
            }
            else
            {
              let it_id=layoutSrcEle[idx].inspTarDef.id;
              let path=defConfig.path+"/it_"+it_id;//OK

              {
                let it_path=defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==it_id);
                if(it_path!==undefined)
                {
                  path=it_path.path;
                }
              }
              
              UI=<InspTargetUI_MUX 
                display={uilayoutInfo.display!=false} 
                style={{float:"left",width:"100%",height:"100%",overflow:"scroll",borderColor:"#AAA",borderStyle:"solid",borderWidth:"2px",borderRadius:"10px"}} 
                EditPermitFlag={EditPermitFlag}
                key={uilayoutInfo.i} 
                systemInspTarList={InspTarList}
              def={layoutSrcEle[idx].inspTarDef} 
              cameraList={defConfig.main.CameraInfo}
              report={undefined} 
              fsPath={path}
              renderHook={renderHook} 
              onDefChange={(new_rule,doInspUpdate=true)=>{
                let newDefConfig={...defConfig,InspTars_main:[...InspTarList]};
                if(new_rule===undefined)
                {


                  let newWidgetInfo=[...WidgetInfo];
                  newWidgetInfo=newWidgetInfo.filter((info:any)=>info.id!=uilayoutInfo.i);
                  updateWidgetLayout(undefined,WidgetLayout);


                  onDefDelete(layoutSrcEle[idx].inspTarDef.id as string);
                  // newDefConfig.InspTars_main=newDefConfig.InspTars_main.filter((itar:any,cidx:number)=>cidx!=idx);
                }
                else
                {
                  let idx = InspTarList.findIndex((itar:any)=>itar.id==new_rule.id);
                  console.log(new_rule,"idx",idx);
                  if(idx<0)return;
                  newDefConfig.InspTars_main[idx]=new_rule;
                }
                
                onDefChange(newDefConfig,idx)
              }}
              defDoReload={()=>{
                defDoReload(layoutSrcEle[idx].inspTarDef.id as string);
              }}
              APIExport={(apis)=>{
                if(_this.apiTable[uilayoutInfo.i]===undefined)//set initial camera state
                {
                  setTimeout(()=>{
                    let CamInfo=WidgetLayout[idx]?.CamInfo;
                    if(CamInfo!==undefined)
                    {
                      console.log(_this.apiTable[uilayoutInfo.i]?.setCameraState(CamInfo));
                      
                    }
                  },300);
                }
                _this.apiTable[uilayoutInfo.i]=apis;

              }}

              UIOption={uilayoutInfo}
              showUIOptionConfigUI={UIEditFlag}
              onUIOptionUpdate={(newUIOption)=>{
                console.log(newUIOption)
              }}
            

              // global_variable={defConfig.main?.global_variable}

              // onGlobalVariableUpdate={(new_global_variable)=>{
              //   let newDefConfig={...defConfig,main:{...defConfig.main,global_variable:new_global_variable}};
              //   onDefChange(newDefConfig,-12);

              // }}
              />
            }
              
          }
          break;

          case "Util":
          {
            console.log(layoutSrcEle[idx]);
            UI=<UtilUI_MUX UIOption={layoutSrcEle[idx]}   
            globalVariable={globalVariable}

            IT_defReload={(id:string)=>{
              defDoReload(id);
            }}
            showUIOptionConfigUI={false}    
            defConfig={defConfig}

            WidgetLayout={WidgetLayout}
            WidgetInfo={WidgetInfo}
            updateWidgetLayout={(newWidgetInfo :any,new_WidgetLayout:any)=>{
              updateWidgetLayout(newWidgetInfo,new_WidgetLayout);
            }}


            systemInspTarList={InspTarList}      
            cameraList={defConfig.main.CameraInfo}
            onUIOptionUpdate={(new_conf:any,doInspUpdate=true)=>{
              console.log(new_conf)
              let tar_idx = WidgetInfo.findIndex((uu:any)=>uu.id==uilayoutInfo.i);
              console.log(tar_idx,new_conf)
              if(tar_idx<0)return;
              let new_WidgetInfo=[...WidgetInfo];
              new_WidgetInfo[tar_idx]=new_conf;
              updateWidgetLayout(new_WidgetInfo,undefined);
            }}
            APIExport={(apis:any)=>{
              _this.apiTable[uilayoutInfo.i]=apis;
            }}
            UI_API_Table={_this.apiTable}
            />
            break;
          }
          
          case ID_ADD_NEW_ELE:
            return <div key={uilayoutInfo.i}>

              




              <UICard_Config 
                inspTarList={InspTarList}
                config={newUIEleConf}
                onConfChange={(nconf)=>{
                  setNewUIEleConf(nconf)
                }}
              
              />


            <br/>


              <Button disabled={newUIEleConf.complete!=true}  onClick={()=>{

                console.log(newUIEleConf);
                let newWidgetInfo=[...WidgetInfo];
                newWidgetInfo.push({
                  ...newUIEleConf
                });


                let new_WidgetLayout=[...WidgetLayout];
                new_WidgetLayout[new_WidgetLayout.length-1]={
                  ...new_WidgetLayout[new_WidgetLayout.length-1],
                  i:newUIEleConf.id,
                  type:newUIEleConf.type,
                }
                updateWidgetLayout(newWidgetInfo,new_WidgetLayout);
          
                setNewUIEleConf({});


              }}>

                +

              </Button>
            
            </div>
            break;
              
          case ID_CLOSE_FS:
            // return <div key={uilayoutInfo.i}><Button  onClick={()=>{

            //   setFSIdx(-1);

            // }}>

            //   收回設定全螢幕模式

            // </Button>
            // </div>
            return <></>;
          
          default:
            
            break;
        }
        return <div key={uilayoutInfo.i}>
            <div style={{
              position: "absolute", 
              top: "2px",
              right: "2px",
              zIndex: 100,
              cursor: "pointer",
              fontSize: "10px",
              transition: "transform 0.2s ease",
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.transform = "scale(2)";
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.transform = "scale(1)";
            }}
            >
              <Popover 
                trigger="hover"
                content={
                  <>
                    {FSIdx!=-1&&FSIdx!=idx?null:<Button 
                      size="small" 
                      onClick={() => {
                        // Add fullscreen functionality here
                        setFSIdx((FSIdx==-1)?idx:-1);
                      }}
                    >
                      {FSIdx==-1?<FullscreenOutlined />:<FullscreenExitOutlined />}
                    </Button>}


                    {/* <Button 
                      size="small" 
                      onClick={() => {
                      }}
                    >
                      ResetCamera
                    </Button> */}
                  </>
                }
              >
                <Button 
                  type="text"
                  icon={<BorderOuterOutlined style={{ fontSize: '10px' }} />}
                  size="small"
                  style={{ padding: '2px' }}
                  onClick={(e) => {
                    e.stopPropagation();
                    // Add your menu/action handling here
                  }}
                />
              </Popover>
            </div>
          {UI}
      
        
        {
          UIEditFlag==false?null: <div  style={{background:UIEditFlag?"rgba(255,255,255,0.7)":undefined,position: "absolute",width:"100%",height:"100%",top:"0px"}}>

            <Popconfirm
              title="確定刪除?"
              onConfirm={()=>{
                let newWidgetInfo=[...WidgetInfo];
                newWidgetInfo=newWidgetInfo.filter((info:any)=>info.id!=uilayoutInfo.i);
                updateWidgetLayout(newWidgetInfo,undefined);
              }}
              onCancel={()=>{
              }}
              okText="Yes"
              cancelText="No"
            >
              <Button danger type='primary'>X</Button>
            </Popconfirm>
            <br/>
            {uilayoutInfo.i}

            <br/>
            {uilayoutInfo.type}

            <br/>

            <Switch checkedChildren="顯示" unCheckedChildren="隱藏" checked={uilayoutInfo.display!=false} onChange={(check)=>{

              console.log(uilayoutInfo,check);
              let new_WidgetLayout=[...WidgetLayout];
              new_WidgetLayout[idx]={...uilayoutInfo,display:check};
              updateWidgetLayout(undefined,new_WidgetLayout);
            }}/>

            <Input value={uilayoutInfo.name} 
            onMouseDown={(e) => e.stopPropagation()}
            onMouseMove={(e) => e.stopPropagation()} 
            onMouseUp={(e) => e.stopPropagation()}
            
            onChange={(e)=>{
              let new_WidgetLayout=[...WidgetLayout];
              new_WidgetLayout[idx]={...uilayoutInfo,name:e.target.value};
              updateWidgetLayout(undefined,new_WidgetLayout);
            }}/>

            <Button onClick={()=>{
              console.log(WidgetLayout,idx);
            }}>...</Button>


            <Button onClick={()=>{
              setFSIdx((FSIdx==-1)?idx:-1);
            }}>FullScreen</Button>

            <br/>
            <Button onClick={()=>{
              let CamInfo=_this.apiTable[uilayoutInfo.i]?.getCameraState();
              // console.log(_this.apiTable[uilayoutInfo.i]?.getCameraState());

              let new_WidgetLayout=[...WidgetLayout];
              new_WidgetLayout[idx]={...uilayoutInfo,CamInfo};
              updateWidgetLayout(undefined,new_WidgetLayout);
            }}>SaveCam</Button>

            <Button onClick={()=>{
              let CamInfo=WidgetLayout[idx]?.CamInfo;
              if(CamInfo!==undefined)
                console.log(_this.apiTable[uilayoutInfo.i]?.setCameraState(CamInfo));
            }}>SetCam</Button>

          </div>
        }
         
      
      </div>})}
      
    </ResponsiveReactGridLayoutX>

}



let DAT_ANY_UNDEF:any=undefined;

function GV_LayersFlating(GVs:any,key:string)
{
  if(GVs===undefined)return {};
  let tGV=GVs[key];
  let srcKey=tGV?.["$base"];
  if(srcKey===undefined)return tGV;
  let baseGV=GV_LayersFlating(GVs,srcKey) as any;
  if(baseGV===undefined)return tGV;

  return ObjReccursiveOverride(baseGV,tGV);
}


function GlobalVariableEditor({variables,onGlobalVariableUpdate,onGlobalVariableIDSelect}:{variables:any,onGlobalVariableUpdate:(new_global_variable:any)=>void,onGlobalVariableIDSelect:(id:string)=>void})
{

  
  const [curGVName,_setCurGVName]= useState(Object.keys(variables)[0]);
  const [showCollapsedGV,setShowCollapsedGV]= useState(false);


  const [UIUpdCD,setUIUpdCD]= useState(0);
  const [newItemID,setNewItemID]= useState("");



  function setCurGVName(newName:string)
  {
    _setCurGVName(newName);
    onGlobalVariableIDSelect(newName);
  }

// {Object.keys(GVs).map((key:string)=>{
//   let GV=GVs[key];
//   return  <div>
//     ________{key}_________
//     <ObjTree obj={GV} padding={0} onLeafSelect={(value,name,path)=>{
//       console.log(name,path)
//     }}/>
//   </div>

// })
// }

  let tGV=variables[curGVName];

  let srcKey=tGV?.["$base"];

  return <>


    <Switch checkedChildren="顯示結果" unCheckedChildren="顯示分離" checked={showCollapsedGV} onChange={()=>{setShowCollapsedGV(!showCollapsedGV)}}/>
    <Radio.Group onChange={(e)=>{setCurGVName(e.target.value)}} defaultValue={curGVName}>
    {Object.keys(variables).map((key:string)=><Radio.Button value={key}>{key}</Radio.Button>)}
    </Radio.Group>
    <br/>



    {showCollapsedGV?<>
      <ObjTree obj={GV_LayersFlating(variables,curGVName)} padding={0} />
      
    
    </>:<>
      
      <ObjTree obj={tGV} padding={0} 
      renderer={
        (value,name,path)=>{
          console.log(value);
          if(typeof value=="object") 
          {
            let allow2Add=newItemID.length>0 && value[newItemID]===undefined;
          return<div id={name} style={{position:"relative",left:`${path.length*15}px`}}>
            <Popover id={"pvf_"+name+"_"+UIUpdCD} trigger="click" content={<div>
              <Input value={newItemID} onChange={(e)=>{
                setNewItemID(e.target.value);
              }} />
              <Button disabled={!allow2Add} size='small' type="primary" onClick={()=>{

                let objX={...GetObjElement(variables,[curGVName,...path,name])};
                objX[newItemID]=0;

                let newGV=ObjShellingAssign(variables,[curGVName,...path,name],objX);
                onGlobalVariableUpdate(newGV);

                setUIUpdCD(UIUpdCD+1);
              }}>變數</Button>
              <Button  disabled={!allow2Add} size='small'onClick={()=>{
                
                let objX={...GetObjElement(variables,[curGVName,...path,name])};
                objX[newItemID]={};

                let newGV=ObjShellingAssign(variables,[curGVName,...path,name],objX);
                onGlobalVariableUpdate(newGV);
              }} >組合</Button>
              <br/>
              <Button size='small' onClick={()=>{
                  let objX={...GetObjElement(variables,[curGVName,...path])};
                  delete objX[name];

                  let newGV=ObjShellingAssign(variables,[curGVName,...path],objX);
                  onGlobalVariableUpdate(newGV);

              }} danger type="primary" >刪除</Button>


            </div>} title="新增內容">
              <Button size='small' type="primary" onClick={()=>{

              }}>{name}[+]</Button>
            </Popover>
          </div>;
          }


          if(path.length==0 && name=="$base")//at first layer
            return <Dropdown
              overlay={<>
                <Menu>
                  {[...Object.keys(variables).filter(key=>key!==curGVName),"_NON_"].map((key)=> <Menu.Item onClick={()=>{
                    onGlobalVariableUpdate(ObjShellingAssign(variables,[curGVName,"$base"],key))

                  }}>
                  <p>
                    {key}
                  </p>
                </Menu.Item>)}
              
                </Menu>

                {/* setCurGVName(value) */}
              </>}
            >
              <Button size='small' type="primary">{name}:{value}</Button>
            </Dropdown>

          
            return <div id={name} style={{position:"relative",left:`${path.length*15}px`}}>
              <Popover id={"pv_"+name} trigger="focus" content={<div>
                {/* OK or cancel */}
                <Button id={"pv_"+name+"del"} danger size='small'  onClick={()=>{

                  let objX={...GetObjElement(variables,[curGVName,...path])};
                  delete objX[name];

                  let newGV=ObjShellingAssign(variables,[curGVName,...path],objX);
                  onGlobalVariableUpdate(newGV);
                }}>刪除</Button>
              </div>} title="確定要刪除？">

                <Button size='small' onClick={()=>{

                }}>{name}:{value}</Button>

              </Popover>

          </div>
            
          return undefined;








        }
      } onLeafSelect={(value,name,path)=>{
      }}/>


      { 
        (srcKey===undefined || variables[srcKey]===undefined)?null:
        <>

        <br/>
        ----連結源頭:{srcKey}

        <br/>
        <ObjTree obj={variables[srcKey]} padding={0} onLeafSelect={(value,name,path)=>{

        }}/>
    
        </>


      }
    
    </>}
  
  </>
}


/*

let objBase=
{
  base:{
  "a": 1,
  "b": {
    "c": 2,
    "d": {
      "g": 3000,
      "k": 5000
    }
  }
} ,

  b2: {
    b:{
      d:{g:3}
    },
    $base:"base"
  },
  top: {
    b:{
      d:{k:5}
    },
    $base:"b2"
  },

};


console.log(GV_LayersFlating(objBase,"top"));

=>{
  "a": 1,
  "b": {
    "c": 2,
    "d": {
      "g": 3,
      "k": 5
    }
  },
  "$base": "b2"
} 

*/



function VIEWUI(){


  const _this = useRef<any>({
    listCMD_Vairable:{
      $DEFPATH:"",
      inCMD_Promise:false,
      DefConfig:undefined,
      widgetSetID:"",
      reportListener:{
        _key_:{//example
          time:0,
          trigger_tag:"sss",
          trigger_id:100,
          camera_id:"Cam1",
          
          report:undefined,
          resolve:(...v:any)=>null,
          reject:(...e:any)=>null,
          // reject:undefined,
        }
      },
  
    }

  }).current;

  const dispatch = useDispatch();
  
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);

  const [xCMDIdx,setXCMDIdx]=useState(-1);


  const [GlobalVariableID,setGlobalVariableID]=useState("base");

  const [defConfig,_setDefConfig]=useState<any>(undefined);
  const [saveDefConfIndexes,setSaveDefConfIndexes]=useState<number[]>([]);

  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);

  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);
  const [refUISetIdx,setrefUISetIdx]=useState(-1);
  const [newUIID,setNewUIID]=useState("");
  const [cameraLoading,setCameraLoading]=useState(false);

  // console.log(">>saveDefConfIndexes",saveDefConfIndexes);
  function setDefConfig(newDC:any,inspTarIndex:number=NaN)
  {
    if(inspTarIndex==inspTarIndex)
    {
      let newIdexes=[...saveDefConfIndexes,inspTarIndex];
      setSaveDefConfIndexes(newIdexes);
    }
    
    _this.listCMD_Vairable.DefConfig=newDC;
    _setDefConfig(newDC);
  }

  const emptyModalInfo={
    timeTag:0,
    visible:false,
    type:"",
    onOK:()=>{},
    onCancel:()=>{},
    title:"" as string|undefined,
    DATA:DAT_ANY_UNDEF,
    content:DAT_ANY_UNDEF,
    footer:null as any|undefined,

  }
  const [modalInfo,setModalInfo]=useState(emptyModalInfo);



  async function LOADPrjDef(PrjDefFolderPath:string)
  {
    let api = BPG_API
    let main= await api.FILE_Load(PrjDefFolderPath+"/main.json");

    let InspTars_main:any[]=[];
    let InspTars_exInfo:{path:string,id:string}[]=[];
    {
      let InspTars=main.InspTars;

      


      let InspTars_ids:string[]= InspTars.map((t:{id:string})=>t.id)
      InspTars_exInfo=InspTars_ids.map(id=>
        ({
          path:PrjDefFolderPath+"/it_"+id+"/",//OK
          id
        })
      )

      let idx=0;
      for(let id of InspTars_ids)
      {
        let path=InspTars_exInfo[idx].path;
        InspTars_main.push(await  BPG_API.FILE_Load(path+"main.json"));
        idx++;
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
    // CNC_API.disconnect();
    // if(main.PeripheralInfo && main.PeripheralInfo.connection_info)
    // {
    //   CNC_API.machineSetup=main.PeripheralInfo.machine_setup;

    //   CNC_API.connect(main.PeripheralInfo.connection_info);
    // }


    let XCmds=await  api.FILE_Load( PrjDefFolderPath+"/XCmds.json");
    _this.listCMD_Vairable.$DEFPATH=PrjDefFolderPath;
    return {
      path:PrjDefFolderPath,
      InspTars_exInfo,
      // _folderInfo:await api.Folder_Struct(PrjDefFolderPath,9),
      main,
      InspTars_main,
      XCmds
    }
  }
  async function SavePrjDef(PrjDefFolderPath:string,PrjDef:(any))
  {

    let api = BPG_API
    await api.FILE_Save(PrjDefFolderPath+"/main.json",PrjDef.main,true)
    await api.FILE_Save(PrjDefFolderPath+"/XCmds.json",PrjDef.XCmds,true)

    
    for(let it of PrjDef.InspTars_main)
    {

      let path = defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==it.id).path;
      console.log(path,it)
      await api.FILE_Save(path+"/main.json",it,true)
    }
    return true
  }
  async function CameraInfoDoConnection(CameraInfo:type_CameraInfo[],froceReconnect=false)
  {
    let api = BPG_API
    let connCameraInfo = await api.CameraCheckAndConnect(CameraInfo,froceReconnect)
    

    let camAvaInfo=CameraInfo.map((ci:type_CameraInfo)=>{
      let connTar = connCameraInfo.find(cCam=>cCam.id==ci.id);
      return {...ci,available:connTar!==undefined}
     })


    _this.listCMD_Vairable.CameraInfo=camAvaInfo;
    return camAvaInfo;
  }
  
  async function setupInspTarToCore(defConfig:any,inspTar:any)
  {

    let api = BPG_API
    // if(inspTar.stream_id===undefined)
    // {
    // }

    // console.log(id,inspTar)
    let it_idx=defConfig.main.InspTars.findIndex((tar:any)=>tar.id==inspTar.id);
    if(it_idx<0)return;
    
    let it_path=defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==inspTar.id).path;

    await api.InspTargetCreate(inspTar,it_path);
    await setUpITStreamID(inspTar,api.getInspTarStreamingID(defConfig,inspTar.id));
  }

  async function setUpITStreamID(inspTar:any,inspTarStreamingId:number)
  {


    inspTar.stream_id=inspTarStreamingId;
    let api = BPG_API
    await api.InspTargetSetStreamChannelID(
      inspTar.id,inspTarStreamingId,
      {
        resolve:(pkts)=>{
          // console.log(pkts);
        },
        reject:(pkts)=>{
          console.log(pkts);

        }
      }
    )

    let cbKey="TargetVIEWUI_CB";
    await api.send_cbs_attach(inspTar.stream_id,cbKey,{
      resolve:(pkts)=>{
        
        let RP=pkts.find((info:any)=>info.type=="RP")
        if(RP===undefined)return;
        RP=RP.data;
        let filteredKey=Object.keys(_this.listCMD_Vairable.reportListener)
          .filter(key=>{
            let repListener=_this.listCMD_Vairable.reportListener[key];
            if(repListener.inspTar_id && repListener.inspTar_id!==RP.source_id)return false;
            if(repListener.trigger_id && repListener.trigger_id!==RP.trigger_id)return false;
            if(repListener.type && repListener.type!==RP.type)return false;
            if(repListener.report!==undefined)return false;
            return true;

          })
        
        filteredKey.forEach(key=>{
          let liInfo=_this.listCMD_Vairable.reportListener[key];
          if(liInfo.listen_style=="persist_callback" )
          {
            if(liInfo.callback!==undefined)
            {
              liInfo.callback(RP,pkts,liInfo);
            }

            return;
          }


          if(liInfo.resolve!==undefined)
          {
            liInfo.resolve(RP,pkts);
          }
          else
          {
            liInfo.report=RP;
          }
        })
        // console.log(RP.data);
      },
      reject:(pkts)=>{

      }
    })
  }

  async function ReloadPrjDef(path:string)  
  {
    let prjDef = await LOADPrjDef( path)

    console.log(prjDef)
    let api = BPG_API

    if(prjDef.main.DisableCameraReload==true)
    {//do not reload camera
      
    }
    else
    {
      setCameraLoading(true);
      prjDef.main.CameraInfo= await CameraInfoDoConnection(prjDef.main.CameraInfo,true)
      setCameraLoading(false);
    }
   
    // updateDefInfo();
    await api.InspTargetRemoveAll()

    
    for(let inspTar of prjDef.InspTars_main)
    {
      
      // }
      let inspTarId:string=inspTar.id;
      // console.log(id,inspTar)

      setupInspTarToCore(prjDef,inspTar);
      
    }


    
    
    let infoList = await api.InspTargetGetInfo();
    

    // InspTargetReload(defInfo:any,_PGID_:number)
    // ddd
    setDefConfig(prjDef);
    setSaveDefConfIndexes([]);
    console.log(prjDef,infoList)
  }

  async function remove_InspTarget(id:string)
  {
    //remove from defConfig
    let newDefConfig={...defConfig};

    //check if it is in newDefConfig.main.InspTars
    console.log(newDefConfig.main.InspTars,id);
    if(newDefConfig.main.InspTars.find((itar:any)=>itar.id==id)===undefined)
    {
      console.log("Not in main.InspTars");
      return false;
    }



    let bkFolder=_this.listCMD_Vairable.$DEFPATH+"/IT_BK";

    let tarIT_Folder=defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==id).path;
    
    await BPG_API.InspTargetRemove(id);

    await BPG_API.send_P("FO",0,{type:"create",path:bkFolder})//create a backup folder
    await BPG_API.send_P("FO",0,{type:"move",from:tarIT_Folder,to:bkFolder+"/it_"+id})//create a backup folder OK

    newDefConfig.InspTars_main=newDefConfig.InspTars_main.filter((itar:any)=>itar.id!==id);
    newDefConfig.main.InspTars=newDefConfig.main.InspTars.filter((itar:any)=>itar.id!==id);
    setDefConfig(newDefConfig);
    return true;
  }


  async function reload_InspTarget(id:string)
  {
    let newDefConfig={...defConfig};
    let idx = newDefConfig.InspTars_main.findIndex((itar:any)=>itar.id==id);
    if(idx<0)return;


    let path=defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==id).path;


    let loadedDef=await  BPG_API.FILE_Load(path+"/main.json")
    
    loadedDef.stream_id=BPG_API.getInspTarStreamingID(defConfig,id);
    console.log("reload_InspTarget","loadedDef:",loadedDef,"path:",path,"defConfig:",defConfig)


    newDefConfig.InspTars_main[idx]=loadedDef;
    setDefConfig(newDefConfig);
    await BPG_API.InspTargetUpdate(loadedDef,path);
    
  }

  async function create_new_InspTarget(type:string,id:string)
  {
    
    let templatePath="data/SYSDAT/InspectionTarget_Template/"+type;
    let targetPath=_this.listCMD_Vairable.$DEFPATH+"/it_"+id;//new inspTar default path
    await BPG_API.send_P("FO",0,{type:"copy",from:templatePath,to:targetPath})//create a backup folder

    //try to read targetPath/main.json
    let def=await BPG_API.FILE_Load(targetPath+"/main.json");
    def.id=id;//alter the id
    def.match_tags=[id+"_Inject"];

    await BPG_API.FILE_Save(targetPath+"/main.json",def);
    // def.type=type;//should be the same as the template

    let newDefConfig={...defConfig};
    newDefConfig.InspTars_main.push(def);
    newDefConfig.InspTars_exInfo.push({path:targetPath,id});//make sure the path keep up to InspTars_main
    newDefConfig.main.InspTars.push({id});
    setDefConfig(newDefConfig);

    
    await setupInspTarToCore(newDefConfig,def);
    
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



  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  
  const [crunIdx,setCRunIdx]=useState(-1);
  const [crunInfo,setCRunInfo]=useState("");
  const [crunAbortCtrl,setCRunAbortCtrl]=useState<AbortController|undefined>(undefined);
  const [editPermitFlag,setEditPermitFlag]=useState<number>(0);
  const [UIEditFlag,setUIEditFlag]=useState<boolean>(false);



  const [newITType, setNewITType] = useState<string|undefined>(undefined);
  const [newITName, setNewITName] = useState<string|undefined>(undefined);

  



  function menuCol(
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



  function constructListCMDUI(UIData:any,updateUI:(data_:any)=>any)
  {

    let data=(typeof UIData === 'function')?UIData():UIData;
    let content=data.map((info_:any,dataIndex:number)=>{

      if(info_==null)return null;
      let info=(typeof info_ === 'function')?info_():info_;

      let opts=(typeof info.opts === 'function')?info.opts():info.opts;
      let doms=

      opts.map((opt:any)=>{



        if (typeof opt === 'object' ) {

          switch(opt.type)
          {
            case "InspTar_UI":{
              let id = opt.id

              let itar=defConfig.InspTars_main.find( (ipt:any)=>ipt.id==id)
              // console.log(itar)
              if(itar===undefined)return "InspTar NotFound"

              let path=defConfig.InspTars_exInfo.find((exInfo:any)=>exInfo.id==id).path;
              return <InspTargetUI_MUX 
                display={true} 
                // width={80} 
                // height={70} 
                // style={{float:"left"}} 
                EditPermitFlag={EDIT_PERMIT_FLAG.OPONLY}
                key={id} 
                systemInspTarList={defConfig.InspTars_main}
                cameraList={[]}
                def={itar} 
                report={undefined} 
                fsPath={path}
                renderHook={undefined} 
                onDefChange={(new_rule,doInspUpdate=true)=>{

                }}
      
                UIOption={undefined}
                showUIOptionConfigUI={false}
                onUIOptionUpdate={(newUIOption)=>{
                  console.log(newUIOption)
                }}

                {...opt.params}
              />
            }


            case "button":{
              let key = opt.key || opt.text
              let text= opt.text || key
              

              return <Button 
              
                {...opt}
                
                onClick={()=>{
                  console.log(opt)
                if(_this.listCMD_Vairable.USER_INPUT_LOCK==true)return;//skip
                _this.listCMD_Vairable.USER_INPUT_LOCK=true;
                (async ()=>{
                  try{
                  if(opt.onClick!==undefined)
                    await opt.onClick(updateUI);
                  else 
                    await info.callback(dataIndex,key,updateUI);
                  _this.listCMD_Vairable.USER_INPUT_LOCK=false;
                  }
                  catch(e)
                  {
                    console.error(e)
                  }
                })().catch(e=>{
                  console.error(e)
                })


                }}
                
                type={opt.btnType}
                
                >{(typeof text === 'string')?text:text(dataIndex)}</Button>
            }

            case "divider":{
              let filterd_opt={...opt}
              filterd_opt.type=opt.divider_type;
              if(opt.onClick!==undefined)
              {
                filterd_opt.onClick=()=>{
                  opt.onClick(updateUI);
                }
              }
              return <Divider {...filterd_opt}>{opt.text}</Divider>
            }
          }
          return "OBJ INFO IS NOT HANDLED"
        }




        if(opt=="$\n")return <br/>;


        if(opt.startsWith("$space:")){
          let count=opt.slice(7);
          return <div style={{width:count,float:"left"}}/>
        }

        if(opt=="$\s")return " ";



        if(opt.startsWith("$t:")){
          return opt.slice(3).replace(/ /g, "\u00A0")
        }

        if(opt.startsWith("$pre:")){
          return <pre style={{flexShrink: 0,overflow:"scroll"}}>{opt.slice(5).replace(/ /g, "\u00A0")}</pre>
        }



        if(opt.startsWith("$divider:")){
          return <Divider> {opt.slice(8)} </Divider>
        }



        return<Button onClick={()=>{

          if(_this.listCMD_Vairable.USER_INPUT_LOCK==true)return;//skip
          _this.listCMD_Vairable.USER_INPUT_LOCK=true;

          (async ()=>{
            await info.callback(dataIndex,opt,updateUI);
            _this.listCMD_Vairable.USER_INPUT_LOCK=false;
          })();


          }}>{opt}</Button>
      })

      console.log(info)
      return <>
        
        {(info.text===undefined || info.text===null)?null:<Divider> <p onClick={(info.onClick!==undefined)?(()=>info.onClick(updateUI)):undefined}>{(typeof info.text === 'string')?info.text:info.text(dataIndex)}</p> </Divider>}


        {doms}


      </>
    });

    return content;

  }

  function listCMDPromiseRun(cmds:string[])
  {
    const abortController = new AbortController();

    setCRunAbortCtrl(abortController);
    listCMDPromise(BPG_API,CNC_API,_this.listCMD_Vairable,cmds,(index,info)=>{
      // console.log(info)
      if(_this.crunIdx!=index)
      {
        setCRunIdx(index);
        _this.crunIdx=index
        setCRunInfo(info)
      }

      
      _this.throttle_Info_UPDATE=
      ID_throttle(_this.throttle_Info_UPDATE,()=>{
        setCRunInfo(info)
      },()=>_this.throttle_Info_UPDATE=undefined,100);
      // else
      // {
      //   setCRunInfo(info)
      // }
    },abortController.signal,
    async (setting)=>{
      let _setting={...setting}
      _this.listCMD_Vairable.USER_INPUT=undefined;

      if(setting.type==="SEL_CBS")
      {//preset
        await new Promise((resolve,reject)=>{
          console.log("setting:",setting)
          let data=(typeof setting.data === 'function')?setting.data():setting.data;
          _this.listCMD_Vairable.USER_INPUT=data.map((info:any)=>info.default);
          _this.listCMD_Vairable.USER_INPUT_LOCK=false;
          let updateUI=(data_:any)=>
          {


            let content = constructListCMDUI(data_,updateUI);

            setModalInfo({

              ...emptyModalInfo,
              timeTag:Date.now(),
              visible:true,
              type:setting.type,
              onOK:()=>{
                abortController.abort();
                resolve(true)
                setModalInfo({...modalInfo,visible:false})
              },
              onCancel:()=>{
                abortController.abort();
                resolve(true)
                setModalInfo({...modalInfo,visible:false})
              },
              footer:null,
              
              title:setting.title,
              DATA:_setting,
              content:content
            })
          }
          updateUI(setting.data);
        })



      
      }




      return _this.listCMD_Vairable.USER_INPUT;
    })
    .then(_=>{
      abortController.abort();
      console.log("DONE")
      setCRunAbortCtrl(undefined);
    })
    .catch(e=>{
      console.log(e);
      setCRunAbortCtrl(undefined);
      delete e.cmd
      if(e.e!==undefined)
        e.e=e.e.toString();

      let stackTrace=e.stackTrace??(new Error().stack);

      setModalInfo({

        ...emptyModalInfo,
        timeTag:Date.now(),
        visible:true,
        type:"CHECK",
        onOK:()=>{
          setModalInfo({...modalInfo,visible:false})
        },
        onCancel:()=>{
          setModalInfo({...modalInfo,visible:false})
        },
        footer:null,
        title:"!!!!錯誤 例外!!!!",
        DATA:{},
        content:<>
          {JSON.stringify(e,null,2)}
          <pre>{stackTrace}</pre>
        </>
      })
    });
  }



  // console.log(displayInspTarId,displayInspTarIdx,displayInspTarIdx_hide);


  _this.newITName=newITName;
  _this.newITType=newITType;


  function InspTarBluePrintUI()
  {
    let latestModalInfo={
      ...emptyModalInfo,
      timeTag:Date.now(),
      type:"AA",
      visible:true,
      onOK:()=>{
        setModalInfo({...modalInfo,visible:false})
      },
      onCancel:()=>{
        setModalInfo({...modalInfo,visible:false})
      },
      title:undefined,
      footer:null,
      DATA:"",
      content:()=><>
        {/* <NodeFlow_DEMO defConfig={defConfig}/> */}

        <div style={{width:"100%",height:"100%"}}  className={"overlayCon"}>

          <div className={"overlay"} style={{zIndex:1}}  >


            <Input placeholder="newITName" onChange={(e)=>{setNewITName(e.target.value)}} value={_this.newITName}/>

            <Dropdown 
                trigger={['click']}
                overlay={
                  <Menu>
                    {InspTargetTypes.map(
                      (type,index)=><Menu.Item key={index} onClick={()=>{setNewITType(type)}}>{type}</Menu.Item>)}



                  </Menu>
                } >
                
                <Button > 檢驗類型:{_this.newITType}</Button>

            </Dropdown>

            <br/>

            <Button disabled={_this.newITType===undefined || _this.newITName===undefined} onClick={()=>{
              create_new_InspTarget(_this.newITType,_this.newITName);
              setNewITName(undefined);
              setNewITType(undefined);
              setTimeout(()=>{
                InspTarBluePrintUI();
              },1);
            }}>AddNew</Button>
          </div>




          <DDDD defConfig={defConfig} nodeInfo={defConfig.main.InspTarNodeInfo} onNodesInfoChange={(nInfo)=>{
            console.log(nInfo)


            setDefConfig(ObjShellingAssign(defConfig,["main","InspTarNodeInfo"],nInfo),-12)



            // let new_defConfig=ObjShellingAssign(defConfig,["XCmds"],new_xCMDList);


            // //console.log(defConfig,new_defConfig)
            // setDefConfig(new_defConfig,-1);



          }}
          
          nodeUpdateMinInterval={500}
          onNodeEvent={(event)=>{
            console.log(event)
          }}></DDDD>
        </div>


      </>
    };

    setModalInfo(latestModalInfo)
  }


  let InspMenu=
  menuCol('檢驗', 'insp',undefined, [
    ...(
      defConfig===undefined?[ menuCol("WAIT...","WAIT...")]:
      (
        [

          menuCol(<div onClick={()=>{
            InspTarBluePrintUI();
          }}>
            ------------
            
            
          </div>,"divLine"),
      
          ...defConfig.InspTars_main.map((inspTar:any)=>{

            return  menuCol(<div onClick={()=>{
              


            }}>
              {inspTar.id}
            </div>,inspTar.id)
          }),
      
      
        ]
        
        
      )
        
        
        // defConfig.InspTars_main.filter((inspTar:any,index:number)=>index)
        // .map((inspTar:any,index:number)=>
        //   ( menuCol(<div onClick={()=>{
            


        //     displayInspTarId.splice(index, 1);
        //     _this.listCMD_Vairable.InspTarDispIDList=displayInspTarId;


        //     console.log(displayInspTarId)
        //     setForceUpdateCounter(forceUpdateCounter+1);



        //   }}>
        //     {inspTar.id}
        //   </div>,inspTar.id) )))
    )])

  async function reConnectCamera()
  {
    let newPrjDef={...defConfig};

    setCameraLoading(true);
    newPrjDef.main.CameraInfo= await CameraInfoDoConnection(defConfig.main.CameraInfo,true)
    setCameraLoading(false);


    setDefConfig(newPrjDef,-1);
  }

  let cameraMenu=
  menuCol('相機', 'cam',cameraLoading?<LoadingOutlined/>:
    <ReloadOutlined onClick={(e)=>{
      reConnectCamera();
      e.preventDefault();
    }}/>, [
    ...(
      defConfig===undefined?[ menuCol("WAIT...","WAITCam")]:
      (defConfig.main.CameraInfo
        .map((cam:type_CameraInfo,index:number)=>
          ( menuCol(<div onClick={()=>{
            let keyTime=Date.now();//use time as key to force CameraSetupEditUI remount
            function updater(ncamInfo:type_CameraInfo){
              
              setModalInfo({...emptyModalInfo,
                title:cam.id,
                visible:true,
                footer:undefined,
                content:<CameraSetupEditUI key={keyTime} CoreAPI={BPG_API} camSetupInfo={ncamInfo}  onCameraSetupUpdate={ncam=>{
                  if(ncam===undefined)
                  {

                    // setModalInfo(emptyModalInfo)
                    return;
                  }
                  console.log(ncam)
                  updater(ncam);
                  (async function(){
                    console.log(ncam);
                    let trigMode=ncam.trigger_mode;
                    let capFrameRate=10;
                    let curFrameRate=ncam.frame_rate;
                    if(curFrameRate>capFrameRate)
                      curFrameRate=capFrameRate;
                    await BPG_API.CameraSetup({...ncam,frame_rate:curFrameRate},trigMode);
                  })()
                  

                }}
                />,
                
                onOK:()=>{

                  let new_defConfig= ObjShellingAssign(defConfig,["main","CameraInfo"],defConfig.main.CameraInfo);
                  new_defConfig.main.CameraInfo[index]=ncamInfo;
                  
                  (async function(){
                    let api =BPG_API
                    await api.CameraSetup(ncamInfo,2);
                    await api.CameraClearTriggerInfo();
                  })()
                  // console.log(setModalInfo);
                  setDefConfig(new_defConfig,-1)
                  setModalInfo(emptyModalInfo)
                  
                },
                
                onCancel:()=>{
                  
                  (async function(){
                    let api =BPG_API
                    await api.CameraSetup(cam,2);
                    await api.CameraClearTriggerInfo();
                  })()
                  
                  setModalInfo(emptyModalInfo)
                },

              })
            }
            updater(cam);

            console.log(cam,index)}
          
          
          }>{cam.side_name||cam.id}</div>,cam.id+"_"+index,
          cam.available?<LinkOutlined/>:<DisconnectOutlined/>) )))
    ),
    menuCol(<Dropdown
      trigger={["click"]}
      disabled={cameraQueryList===undefined}
      overlay={<>
        <Menu>
          {
            cameraQueryList===undefined || cameraQueryList.length===0?
              <Menu.Item disabled danger>
              <a target="_blank" rel="noopener noreferrer">
                just a sec...
              </a>
              </Menu.Item>
              :
              cameraQueryList
              .filter(camFound=>{  
                let foundCam=defConfig.main.CameraInfo.find((cam:any)=>cam.id===camFound.id);
                return foundCam===undefined;
              })
              .map((cam,idx)=><Menu.Item key={cam.id+"_"+idx} 
              onClick={()=>{
                console.log(defConfig.main.CameraInfo)
                console.log(cam)
                cam.available=false;
                let new_camInfo=[...defConfig.main.CameraInfo,cam];
                setCameraLoading(true);
                CameraInfoDoConnection(new_camInfo).then(result_camInfo=>{

                  let new_defConfig= ObjShellingAssign(defConfig,["main","CameraInfo"],result_camInfo);
                  setDefConfig(new_defConfig,-2)
                });

                setCameraLoading(false);
              }}>
                {cam.id}
              </Menu.Item>)

          }
        </Menu>
      </>}
    ><div onClick={()=>{

      setCameraQueryList(undefined);
      (async ()=>{
        let api = BPG_API
        let camList = await api.queryDiscoverList();
        setCameraQueryList(camList);
        console.log(camList)
      })();
      
    }}>+
    </div>



    </Dropdown>, 'Add'),
    menuCol(
    
    <div onClick={reConnectCamera}>Refresh
    </div>, 'Refresh')



  ])




  let xcmdMenu=
  menuCol(<div onClick={()=>{

     setXCMDIdx(-1);
     setDelConfirmCounter(delConfirmCounter+1);
    }}>程序指令</div>, 'xcmd',undefined,
    
      defConfig===undefined?[ menuCol("WAIT...","WAIT...")]:
      [
        ...defConfig.XCmds.map((xcmd:any,index:number)=>menuCol(<div onClick={()=>{
          setXCMDIdx(xCMDIdx==index?-1:index);

        }}>{xcmd.id}</div>,xcmd.id+index))
        ,
        menuCol(        <div onClick={()=>{

          

          let new_xCMDList=[...defConfig.XCmds];

          let newName="NEW XCMD";
          for(let i=0;;i++)
          {
            let checkName=newName;
            if(i!==0)
            {
              checkName+=" "+i; 
            }
            //console.log(checkName);
            if(new_xCMDList.find(xcmd=>xcmd.id==checkName)===undefined)
            {
              newName=checkName;
              break;
            }
          }

          new_xCMDList.push({
            id:newName,
            cmds:[]
          });

          // new_xCMDList.splice(xCMDIdx, 1);
          
          let new_defConfig=ObjShellingAssign(defConfig,["XCmds"],new_xCMDList);


          //console.log(defConfig,new_defConfig)
          setDefConfig(new_defConfig,-1);
          setXCMDIdx(-1);
        }}>+</div>,"_ADD_xcmd")//new xcmd 

      ]
      
    )

  
    
    _this.cached_defConfig=defConfig;
    _this.GVEX=defConfig?.main?.global_variable;
    let globalVMenu=
    menuCol(<div onClick={()=>{
      }}>變數設定:{GlobalVariableID}</div>, 'globalV',undefined,
        [
          ...Object.keys({..._this.GVEX}).map((key:string)=>menuCol(<div onClick={()=>{
            setGlobalVariableID(key);
          }}>{key}</div>,key)),

          menuCol(        <div onClick={()=>{
            
            // _this.defCon_global_variable=defConfig.main?.global_variable?.base;


            let GVs=defConfig.main?.global_variable;

            setModalInfo({
              ...emptyModalInfo,
              timeTag:Date.now(),
              type:"AA",
              visible:true,
              onOK:()=>{
                setModalInfo({...modalInfo,visible:false})
              },
              onCancel:()=>{
                setModalInfo({...modalInfo,visible:false})
              },
              title:undefined,
              footer:null,
              DATA:"",
              content:()=><>

              <GlobalVariableEditor variables={_this.GVEX} 
                onGlobalVariableUpdate={(nV)=>{
                  // console.log(nV)
                  
                  setDefConfig(ObjShellingAssign(_this.cached_defConfig,["main","global_variable"],nV),-1);
  
                }}
                
                onGlobalVariableIDSelect={(ID)=>{
                  setGlobalVariableID(ID);
                }}
                
                />
 


              </>
            })
            
          }}>...</div>,"_ADD_")//new xcmd 
          



        ]
        
      )

  const items: MenuItem[] = [
    cameraMenu,
    InspMenu,
    xcmdMenu,
    globalVMenu
  ];


  let siderBaseSize=200;
  let extSiderSizeMul=xCMDIdx==-1?0:1;
  let extSizerSize=siderBaseSize*extSiderSizeMul;

  let baseSiderTabs=
  <div style={{float:"left",height:"100%",width:(100*(1)/(1+extSiderSizeMul))+"%"}}>
  <Menu mode="inline" theme="dark" selectable={false}

    items={items}
  >
  </Menu>
  </div>



  let curXCMD:any=undefined;
  if(xCMDIdx!=-1)
  {
    curXCMD=defConfig.XCmds[xCMDIdx];
  }


  let extSiderTabs=xCMDIdx==-1?null:
  <div style={{float:"left",height:"100%",width:(100*(extSiderSizeMul)/(1+extSiderSizeMul))+"%",

  overflow: "scroll",
  boxShadow: "-5px 5px 15px rgb(0 0 0 / 50%)",
  padding: "5px",
  background: "white",
  color: "black"}}>

    <Input maxLength={100} value={curXCMD.id}
                style={{margin:"1px"}}
                onChange={(e)=>{
                  let value=e.target.value;
                  let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"id"],value);
                  console.log(defConfig,xCMDIdx,new_defConfig);
                  setDefConfig(new_defConfig,-5);

                }}/>
    {/* {xCMDWidthM==1?
      <Button onClick={()=>{setXCMDWidthM(3);}}>+</Button>:
      <Button onClick={()=>{setXCMDWidthM(1);}}>-</Button>
    } */}


            <Button disabled={false} onClick={()=>{
              listCMDPromiseRun(curXCMD.cmds);
            }}>Run</Button>

            <Button disabled={crunAbortCtrl===undefined || (crunAbortCtrl&&crunAbortCtrl.signal.aborted)}   onClick={()=>{
              if(crunAbortCtrl===undefined)return;
              crunAbortCtrl.abort();
              setDelConfirmCounter(delConfirmCounter+1);//HACK this is just to force the update,delConfirmCounter would not be used at this stage
            }}>STOP</Button> 
            
            <Popconfirm
                title="Are you sure to delete this task?"
                onConfirm={()=>{
                  
                }}
                okButtonProps={{danger:true,onClick:()=>{
                  if(delConfirmCounter==0)
                  {
                    let new_xCMDList=[...defConfig.XCmds];
                    new_xCMDList.splice(xCMDIdx, 1);
                    
                    let new_defConfig=ObjShellingAssign(defConfig,["XCmds"],new_xCMDList);


                    setDefConfig(new_defConfig,-9);
                    setXCMDIdx(-1);

                  }
                  else
                  {
                    setDelConfirmCounter(delConfirmCounter-1);
                  }
                }}}
                onCancel={()=>{}}
                okText={"Yes:"+delConfirmCounter}
                cancelText="No"
              >
              <Button danger type="primary" onClick={()=>{
                setDelConfirmCounter(5);
              }}>DEL</Button>
            </Popconfirm> 
           
            
            <Divider style={{margin: "5px"}}> ACMD </Divider>
            {
              curXCMD.cmds.map((cmd:string,idx:number)=><>


                <Dropdown
                  overlay={<>
                    <Button size="small" onClick={()=>{
                      let new_cmd_list=[...curXCMD.cmds]
                      new_cmd_list.splice(idx, 0, "");
                      let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds"],new_cmd_list);
                      setDefConfig(new_defConfig,-9);


                    }}>+</Button>
                    <Button size="small" onClick={()=>{
                  
                      let new_cmd_list=[...curXCMD.cmds]
                      new_cmd_list.splice(idx, 1);
                      let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds"],new_cmd_list);
                      setDefConfig(new_defConfig,-9);
                    }}>-</Button>
                  </>}
                >
                  <a style={{color:"#000"}} className="ant-dropdown-link"  onClick={e => e.preventDefault()}>
                    <DownOutlined />
                  </a>
                </Dropdown>

                {crunIdx==idx?(crunInfo.length?crunInfo:"<--------"):null}


                <Input.TextArea value={cmd} 
                // rows={1}
                autoSize
                tabIndex={-1}
                onKeyDown={(e)=>{
                  if (e.key == 'Tab') {
                    // e.preventDefault();

                  }
                }}
                style={{margin:"1px"}}
                onChange={(e)=>{
                  
                  let value=e.target.value;
                  console.log(value)
                  

                  let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds",idx],value);
                  console.log(new_defConfig);
                  setDefConfig(new_defConfig,-9);

                }}/>
              </>)
            }

            <Button size="small" onClick={()=>{
              
              let new_cmd_list=[...curXCMD.cmds]
              new_cmd_list.push("");
              let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds"],new_cmd_list);
              setDefConfig(new_defConfig,-9);

            }}>+</Button>



















  </div>


  let siderUI=(editPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)==0?null:
  <Sider width={siderBaseSize+extSizerSize}>
  {baseSiderTabs}
  {extSiderTabs}
  </Sider>
    

  let WidgetTableInfo=(GetObjElement(defConfig,["main","UIInfo"])??[])



  return <>

    <Layout style={{ height: '100%' }}>
    <Header style={{ width: '100%' }}>

    <Menu theme="dark" mode="horizontal" selectable={false}>
        <Menu.Item key="SHOW_EDIT" onClick={()=>{
          // if(editPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)
          // {
          // }
          let newFlag=editPermitFlag^EDIT_PERMIT_FLAG.XXFLAGXX;
          setEditPermitFlag(newFlag)

          if(newFlag==0)
          {
            setUIEditFlag(false);
          }

        }}>EDIT_LEVEL {editPermitFlag}</Menu.Item>


        <Menu.Item key="UIEditCtrl" onClick={()=>{
          setUIEditFlag(!UIEditFlag)
        }}>UIEdit mode: {UIEditFlag?"O":"X"}</Menu.Item>

        {
          (editPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)==0?null:<>
        <Menu.Item key="1" onClick={()=>{
          BPG_API.CameraClearTriggerInfo();
            }}>ClearTriggerInfo</Menu.Item>
    
        <Menu.Item disabled={saveDefConfIndexes.length==0} key="2" onClick={()=>{
          SavePrjDef(_DEF_FOLDER_PATH_,defConfig);
          setSaveDefConfIndexes([]);
        }}>SAVE</Menu.Item>

          </>
        }





        <Dropdown 
          // trigger={['click']}
          overlay={
            <>{[...Object.keys({..._this.GVEX}).
              map((key:string)=><Button onClick={()=> 
                {
                  setGlobalVariableID(key);
                }
              }> {key}</Button>)
            ]}</>} 
        > 
              
        
          <Menu.Item disabled={true} key="999" onClick={()=>{
          }}>----{GlobalVariableID}----</Menu.Item>

        </Dropdown>


        

    </Menu>
    

    
    <DraggableModalProvider>
    <DraggableModal
        title={modalInfo.title}
        visible={modalInfo.visible}
        onOk={modalInfo.onOK}
        // confirmLoading={confirmLoading}
        onCancel={modalInfo.onCancel}
        footer={modalInfo.footer}
      >
        {typeof modalInfo.content === 'function'?modalInfo.content():modalInfo.content }
    </DraggableModal>
    </DraggableModalProvider>

    </Header>

    <Layout>
    {siderUI}
    
    <Content className="site-layout" style={{ padding: '0 0px'}}>
    
    {/* { (defConfig===undefined)?"WAIT":
      displayInspTarId.map(tar=><p>{tar}</p>)
    } */}
    {/* {
      displayInspTarIdx.map((InspTarIdx:number,listIndex:number)=>{
        let inspTar=defConfig.InspTars_main[InspTarIdx];
        return inspTar.id;
      })
    } */}
    { 
    (defConfig===undefined)?"WAIT": <>
      <Space wrap>
      {
        WidgetTableInfo.map((tableInfo:any,idx:number)=>(<div>
          <Button type={_this.listCMD_Vairable.widgetSetID==tableInfo.id?'primary':undefined}
          onClick={()=>{
            _this.listCMD_Vairable.widgetSetID=tableInfo.id;
            setForceUpdateCounter(forceUpdateCounter+1);
          }}>
            {tableInfo.id}
          </Button>

          {UIEditFlag?
          <>
            <Button type={_this.listCMD_Vairable.widgetSetID==tableInfo.id?'primary':undefined}
              onClick={()=>{
                if(idx==refUISetIdx)
                  setrefUISetIdx(-1);
                else
                  setrefUISetIdx(idx);
              }}>
                <CopyOutlined/>
            </Button>
            
            <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
            onConfirm={()=>{
              
            }}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter==0)
              {
                setDefConfig(ObjShellingAssign(defConfig,["main","UIInfo"],
                defConfig.main.UIInfo.filter((info:any)=>info.id!=tableInfo.id)),-12)

              }
              else
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
            }}}
            onCancel={()=>{}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
            >
            <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
            }}>X</Button>
            </Popconfirm> 
          </>
          :null}

        </div>
        ))
        
      }

      {UIEditFlag?<>
      {/* Add a vertical split bar */}
      <Divider type="vertical" style={{height:"30px"}}/>
      <Input placeholder={"新的UI頁面 ID"} status={newUIID.length==0?"error":undefined}
        style={{width:"100px"}}
        onChange={(e:any)=>setNewUIID(e.target.value)}
        value={newUIID}
        onPressEnter={(e:any)=>{


        }}
      />
      {<Button danger disabled={newUIID.length==0} onClick={()=>{
        

        if(newUIID.length==0)return;



        let newUIInfo={
          id:newUIID,
        }
        if(refUISetIdx>=0)
        {
          newUIInfo={
            ...WidgetTableInfo[refUISetIdx],
            id:newUIID,
          }
        }
        setDefConfig(ObjShellingAssign(defConfig,["main","UIInfo",WidgetTableInfo.length],newUIInfo),-12)
        setrefUISetIdx(-1)

        setNewUIID("");

      }}>{refUISetIdx<0?"建立空白頁面":"複製頁面:"+WidgetTableInfo[refUISetIdx].id}</Button> }
      </>:null}

      </Space>


      <ITGlobalVariableContext.Provider value={
        {

          global_variable:GV_LayersFlating(defConfig.main?.global_variable,GlobalVariableID),
          set_global_variable:(path, new_value)=>{

            let curGV=defConfig.main?.global_variable;


            //HACK, sometimes after this callback is called, the onDefChange will be called right after, however the global_variable there is the old one, so the global_variable change is override back
            //so we cache the new_global_variable here, and use it in the onDefChange
            _this.CACHED_GLOBAL_VARIABLE=ObjShellingAssign(curGV,[GlobalVariableID,...path],new_value);

            console.log(curGV,path,new_value,_this.CACHED_GLOBAL_VARIABLE)
            let new_defConfig=ObjShellingAssign(defConfig,["main","global_variable"],_this.CACHED_GLOBAL_VARIABLE);

            BPG_API.InspTargetSetGlobalVariable(GV_LayersFlating(_this.CACHED_GLOBAL_VARIABLE,GlobalVariableID));
            setDefConfig(new_defConfig,-1);
          }
        }}>

      <TargetViewUIShow globalVariable={_this.listCMD_Vairable} WidgetSetID={_this.listCMD_Vairable.widgetSetID} defConfig={defConfig} UIEditFlag={UIEditFlag} EditPermitFlag={editPermitFlag}  
        
        onDefChange={(newdef:any, updateIdx)=>{

          if(_this.CACHED_GLOBAL_VARIABLE!==undefined)//HACK, look above
            newdef=ObjShellingAssign(defConfig,["main","global_variable"],_this.CACHED_GLOBAL_VARIABLE);
          setDefConfig(newdef,updateIdx)

          _this.CACHED_GLOBAL_VARIABLE=undefined;
        }}  
        
        renderHook={_this.listCMD_Vairable.renderHook}

        onDefDelete={(id:string)=>{

          remove_InspTarget(id);
        }}

        defDoReload={(id:string)=>{
          
          reload_InspTarget(id);
        }}
        />
      </ITGlobalVariableContext.Provider>
      </>}
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

  useEffect(() => {
    const handleBeforeUnload = (event:any) => {
      event.preventDefault();
      // Custom message text is ignored in most modern browsers, but it is required for some legacy support
      event.returnValue = "Are you sure you want to leave this site?";
    };

    // Add event listener when the component mounts
    window.addEventListener("beforeunload", handleBeforeUnload);

    // Clean up the event listener when the component unmounts
    return () => {
      window.removeEventListener("beforeunload", handleBeforeUnload);
    };
  }, []);
  // const [camList,setCamList]=useState<{[key:string]:{[key:string]:any,list:any[]}}>({});
  useEffect(() => {
    
    let core_api=new BPG_WS(CORE_ID);
    core_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CORE_ID);


    ACT_EXT_API_REGISTER(core_api.id,core_api);
    
    const { REACT_APP_MY_ENV } = process.env;
    console.log(REACT_APP_MY_ENV)
    core_api.connect({
      url:"ws://127.0.0.1:4090"//4039"
    });




    let CNC_api=new CNC_Perif(CNC_PERIPHERAL_ID,20666);

    {
      CNC_api.onConnected=()=>{ACT_EXT_API_CONNECTED(CNC_PERIPHERAL_ID)};
  
      CNC_api.onInfoUpdate=(info:[key: string])=>ACT_EXT_API_UPDATE(CNC_api.id,info);
  
      CNC_api.onDisconnected=()=>{ACT_EXT_API_DISCONNECTED(CNC_PERIPHERAL_ID)};
      
      CNC_api.BPG_Send=core_api.send.bind(core_api);
  
      ACT_EXT_API_REGISTER(CNC_api.id,CNC_api);
    }
    






    core_api.onConnected=()=>{
      ACT_EXT_API_CONNECTED(CORE_ID);

      // CNC_api.connect({
      //   // uart_name:"/dev/cu.SLAB_USBtoUART",
      //   uart_name:"/dev/cu.usbserial-0001",
      //   baudrate:460800//230400//115200
      // });
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
