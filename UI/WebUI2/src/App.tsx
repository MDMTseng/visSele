import React from 'react';
import { useState, useEffect,useRef,useMemo,useContext, useCallback } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch,Select } from 'antd';
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
  DisconnectOutlined,LinkOutlined } from '@ant-design/icons';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign} from './UTIL/MISC_Util';

import {listCMDPromise} from './XCMD';


import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg,PtRotate2d} from './UTIL/MathTools';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message,Space } from 'antd';


import { type_CameraInfo,type_IMCM} from './AppTypes';
import './basic.css';


import {SingleTargetVIEWUI_ColorRegionDetection,
  SingleTargetVIEWUI_Orientation_ColorRegionOval,
  SingleTargetVIEWUI_Orientation_ShapeBasedMatching,
  SingleTargetVIEWUI_SurfaceCheckSimple,
  SingleTargetVIEWUI_JSON_Peripheral,
  CompParam_InspTarUI,CompParam_UIOption } from './InspTarView';

const { Option } = Select;
const { SubMenu } = Menu;
  
enum EDIT_PERMIT_FLAG {
  OPONLY=0,
  XXFLAGXX=1<<0
}



type IMCM_type=
{
  camera_id:string,
  trigger_id:number,
  trigger_tag:string,
  image_info:{
    full_height: number
    full_width: number
    height: number
    image: ImageData
    offsetX: number
    offsetY: number
    scale: number
    width: number
  }
}


function PtsToXYWH( pt1:VEC2D, pt2:VEC2D)
{
  let x,y,w,h;

  x=pt1.x;
  w=pt2.x-pt1.x;

  y=pt1.y;
  h=pt2.y-pt1.y;


  if(w<0){
    x+=w;
    w=-w;
  }
  
  if(h<0){
    y+=h;
    h=-h;
  }
  return {
    x,y,w,h
  }
}

function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number,drawCenterPoint:boolean=true)
{
  let ctx = g.ctx;
  // ctx.lineWidth = 5;

  let x = region.x;
  let y = region.y;
  let w = region.w;
  let h = region.h;
  ctx.beginPath();
  ctx.setLineDash([lineWidth*10,lineWidth*3,lineWidth*3,lineWidth*3]);
  // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
  ctx.lineWidth = lineWidth;
  ctx.rect(x,y,w,h);
  ctx.stroke();
  ctx.closePath();

  if(drawCenterPoint)
  {
    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth*2/3;
    canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);
  }



}


type IMCM_group={[trigID:string]:IMCM_type}


  

type MenuItem = Required<MenuProps>['items'][number];

var enc = new TextEncoder();

const _DEF_FOLDER_PATH_="data/SP1.xprj";
// import ReactJsoneditor from 'jsoneditor-for-react';

// declare module 'jsoneditor-react'jsoneditor-for-react"

// import 'jsoneditor-react/es/editor.min.css';

let INSPTAR_BASE_STREAM_ID = 51000

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


function CameraSetupEditUI({camSetupInfo,CoreAPI,onCameraSetupUpdate}:{ camSetupInfo:type_CameraInfo, CoreAPI:BPG_WS,onCameraSetupUpdate:(caminfo:type_CameraInfo)=>void}){

  const _this = useRef<{canvasComp:DrawHook_CanvasComponent|undefined,imgCanvas:HTMLCanvasElement
  }>({
    canvasComp:undefined,
    imgCanvas:document.createElement('canvas') as HTMLCanvasElement
  }).current;

  const _this2 = useRef<any>({}).current;


  const [Local_IMCM,setLocal_IMCM]=
    useState<type_IMCM|undefined>(undefined);
  
  useEffect(()=>{//load default

    (async ()=>{
      let api =CoreAPI
      // await api.InspTargetExchange(camSetupInfo.id,{
      //   id:"",
      //   data:{
          

      //   }
      // });

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
          if(ctx2nd)
          {
            if(IMCM.image_info.image instanceof ImageData)
              ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
            else if(IMCM.image_info.image instanceof HTMLImageElement)
              ctx2nd.drawImage(IMCM.image_info.image, 0, 0);
          }


          setLocal_IMCM(IMCM)
          // console.log(IMCM)

        },
        reject:(pkts)=>{

        }
      })


      await api.CameraTriggerInfoMocking(camSetupInfo.id);


      // await api.CameraSetup(camSetupInfo,0);
      onCameraSetupUpdate(camSetupInfo);
    })()

    return ()=>{
      (async ()=>{
        let api =CoreAPI
        await api.CameraSetChannelID([camSetupInfo.id],0,{
          resolve:()=>0,
          reject:()=>0
        });


        await api.CameraTriggerInfoMocking(camSetupInfo.id,"",true);

      })()
    }
  },[])






  return <> 
    trigger_on:
    <Switch checkedChildren="O" unCheckedChildren="X" checked={camSetupInfo.trigger_mode==1} onChange={(check)=>{
      onCameraSetupUpdate({...camSetupInfo,trigger_mode:check?1:0})
    }}/>
    <br/>
    exposure:<InputNumber value={camSetupInfo.exposure} onChange={(num)=>{
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
    RGain:<InputNumber value={camSetupInfo.RGain} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,RGain:num})
    }}/>
    GGain:<InputNumber value={camSetupInfo.GGain} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,GGain:num})
    }}/>
    BGain:<InputNumber value={camSetupInfo.BGain} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,BGain:num})
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
    pixel_size(mm):<InputNumber value={camSetupInfo.pixel_size} onChange={(num)=>{
      onCameraSetupUpdate({...camSetupInfo,pixel_size:num})
    }}/>
    <br/>
    <Switch checkedChildren="反X" unCheckedChildren="正X" checked={camSetupInfo.mirrorX} onChange={(check)=>{
      onCameraSetupUpdate({...camSetupInfo,mirrorX:check})
    }}/>
    <Switch checkedChildren="反Y" unCheckedChildren="正Y" checked={camSetupInfo.mirrorY} onChange={(check)=>{
      onCameraSetupUpdate({...camSetupInfo,mirrorY:check})
    }}/>
    <Button key={">ROI>>"} onClick={()=>{
      
      if(_this.canvasComp==undefined)return;
      
      onCameraSetupUpdate({...camSetupInfo,ROI:{x:0,y:0,w:999999,h:999999}});
      _this.canvasComp.UserRegionSelect((info,state)=>{
        if(state==2)
        {
          console.log(info);
          
          let x,y,w,h;
          
          let roi_region=PtsToXYWH(info.pt1,info.pt2);
          console.log(roi_region)
          // onDefChange(newRule)
          onCameraSetupUpdate({...camSetupInfo,ROI:roi_region})
          if(_this.canvasComp==undefined)return;
          _this.canvasComp.UserRegionSelect(undefined)
        }
      })
    }}>ROI</Button>
    <br/>
    <HookCanvasComponent style={{height:"300px"}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      if(ctrl_or_draw==true)//ctrl
      {
        


        const imageData = g.ctx.getImageData(g.mouseStatus.x-2, g.mouseStatus.y-2, 1, 1);
        _this2.fetchedPixInfo = imageData;

      }
      else//draw
      {
        if(Local_IMCM!==undefined)
        {
          // g.ctx.save();
          // let scale=Local_IMCM.image_info.scale;
          // g.ctx.translate(-Local_IMCM.image_info.full_width/2,-Local_IMCM.image_info.full_height/2);
          // g.ctx.scale(scale,scale);
          // g.ctx.translate(-0.5, -0.5);
          
          // g.ctx.drawImage(_this.imgCanvas, 0, 0);
          // g.ctx.restore();

          
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       
        if (_this2.fetchedPixInfo !== undefined) {
            g.ctx.save();
            g.ctx.resetTransform();
            // console.log(_this.fetchedPixInfo)
            let pixInfo = _this2.fetchedPixInfo.data;
            g.ctx.font = "1.5em Arial";
            g.ctx.fillStyle = "rgba(250,100, 50,1)";

            // g.ctx.fillText(rgb2hsv(pixInfo[0], pixInfo[1], pixInfo[2]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)

            g.ctx.fillText((pixInfo as number[]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)
            g.ctx.restore();
        }


        // console.log(canvas_obj);
        

        if(canvas_obj.regionSelect!==undefined && (canvas_obj.regionSelect.pt1!==undefined && canvas_obj.regionSelect.pt2!==undefined))
        {

          let ctx = g.ctx;
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          {
            let sel_region=PtsToXYWH(canvas_obj.regionSelect.pt1,canvas_obj.regionSelect.pt2);
            drawRegion(g,canvas_obj,sel_region,canvas_obj.rUtil.getIndicationLineSize());
          }

      
        }
        let ctx = g.ctx;
        
      }
    }
    }/>


    <pre>{
      JSON.stringify(camSetupInfo,null,2)
    }</pre>
  </>
}





function InspTargetUI_MUX(param:CompParam_InspTarUI)
{
  if(param.def.type=="ColorRegionDetection")
  return <SingleTargetVIEWUI_ColorRegionDetection {...param} />;

  if(param.def.type=="Orientation_ColorRegionOval")
  return <SingleTargetVIEWUI_Orientation_ColorRegionOval {...param} />;


  if(param.def.type=="Orientation_ShapeBasedMatching")
  return <SingleTargetVIEWUI_Orientation_ShapeBasedMatching {...param} />;


  if(param.def.type=="SurfaceCheckSimple")
  return <SingleTargetVIEWUI_SurfaceCheckSimple {...param} />;


  if(param.def.type=="JSON_Peripheral")
  return <SingleTargetVIEWUI_JSON_Peripheral {...param} />;


  return  <></>;
}





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

              onConfChange(setCompleteFlag({..._config,itid:it.id}))

        
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






function UtilUI_MUX({UIOption,onUIOptionUpdate}:CompParam_UIOption)


// CompParam_UIOption
{
  switch(UIOption.subtype)
  {
    case "ASS":
      break;
    case "uInsp_Ctrl_Panel":








    
      break;
    default:


      return  <>
      
        <Dropdown
          trigger={["click"]}
          overlay={<>
            <Menu>
              {
                ["ASS","uInsp_Ctrl_Panel"].map(uitype=><Menu.Item key={uitype} 
                  onClick={()=>{
                    onUIOptionUpdate({...UIOption,subtype:uitype})
                  }}>
                    {uitype}
                  </Menu.Item>)

              }
            </Menu>
          </>}
        >
      
        <a onClick={(e) => e.preventDefault()}>
          <Space>
            ...
            <DownOutlined />
          </Space>
        </a>


        </Dropdown>

      
      
      
      </>;
  }

  return  <>UTIL..ddd..:{UIOption.subtype}</>;
}




function TargetViewUIShow({displayIDList,defConfig,UIEditFlag,EditPermitFlag,onDefChange,renderHook}:{displayIDList:string[],defConfig:any,UIEditFlag:boolean,EditPermitFlag:number, onDefChange:(updatedDef:any)=>void,renderHook:any})
{
  
  
  const [newUIEleConf,setNewUIEleConf]= useState<any>({});
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
  let WidgetLayout=defConfig.main.WidgetLayout===undefined?[]:defConfig.main.WidgetLayout;

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
    WidgetLayout.push({...defalutLayoutInfo,i:ID_ADD_NEW_ELE,type:ID_ADD_NEW_ELE})
  }





  let layoutSrcEle=WidgetLayout.map((layoutInfo:any)=>{
    let tatId=layoutInfo.i;

    //  let eleInfo = InspTarList.find((it:any)=>it.id==tatId);
    //  if(eleInfo)return eleInfo;

    let eleInfo = defConfig.main.WidgetInfo.find((uu:any)=>uu.id==tatId);
    



    if(eleInfo)
    {
      if(eleInfo.type=="InspTar" && eleInfo.itid!==undefined)
      {
        let itDef = InspTarList.find((it:any)=>it.id==eleInfo.itid);
        eleInfo={...eleInfo,inspTarDef:itDef}
      }
      return eleInfo;
    }

    return undefined;
  })
  console.log(layoutSrcEle);



  return <ResponsiveReactGridLayoutX layouts={{lg:WidgetLayout}}
    style={{height:"100%",overflow:"scroll"}}
      //  onDrop={(e) => onDrop(e)} 
    onLayoutChange={(curL,allL)=>{
      console.log(curL,allL)

      let newWidgetLayout=WidgetLayout.map((layo:any,index:number)=>{
        return {...layo,...curL[index]}
      })
      
      let newDefConfig=ObjShellingAssign(defConfig,["main","WidgetLayout"],newWidgetLayout)
      onDefChange(newDefConfig)




    }}
    className="layout" 
    //  layouts={layouts} 
    breakpoints={{ lg: 4, md: 3, sm: 2, xs: 1, xxs: 0 }}//{{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
    cols={{ lg: 20, md: 20, sm: 20, xs: 20, xxs: 20 }}
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

        // if(layoutSrcEle[idx]===undefined)return null;
        let UI:JSX.Element=<></>
        let UIEditUI:JSX.Element=<></>
        switch(uilayoutInfo.type)
        {
          case "InspTar":
            UI=<InspTargetUI_MUX 
              display={uilayoutInfo.display!=false} 
              style={{float:"left",width:"100%",height:"100%",overflow:"scroll"}} 
              EditPermitFlag={EditPermitFlag}
              key={uilayoutInfo.i} 
              systemInspTarList={InspTarList}
              def={layoutSrcEle[idx].inspTarDef} 
              report={undefined} 
              fsPath={defConfig.path+"/it_"+layoutSrcEle[idx].inspTarDef.id}
              renderHook={renderHook} 
              onDefChange={(new_rule,doInspUpdate=true)=>{
                let newDefConfig={...defConfig,InspTars_main:[...InspTarList]};
                if(new_rule===undefined)
                {
                  // newDefConfig.InspTars_main=newDefConfig.InspTars_main.filter((itar:any,cidx:number)=>cidx!=idx);
                }
                else
                {
                  console.log(new_rule);
                  let idx = InspTarList.findIndex((itar:any)=>itar.id==new_rule.id);
                  if(idx<0)return;
                  newDefConfig.InspTars_main[idx]=new_rule;
                }
                
                onDefChange(newDefConfig)
              }}
              APIExport={undefined}

              UIOption={uilayoutInfo}
              showUIOptionConfigUI={UIEditFlag}
              onUIOptionUpdate={(newUIOption)=>{
                console.log(newUIOption)
              }}
            />
              
          break;

          case "Util":
            console.log(layoutSrcEle[idx]);
            UI=<UtilUI_MUX UIOption={layoutSrcEle[idx]}   
            showUIOptionConfigUI={false}            
            onUIOptionUpdate={(new_conf:any,doInspUpdate=true)=>{
              console.log(new_conf)
              let tar_idx = defConfig.main.WidgetInfo.findIndex((uu:any)=>uu.id==uilayoutInfo.i);
              console.log(tar_idx,new_conf)
              if(tar_idx<0)return;

              let new_defConfig=ObjShellingAssign(defConfig,["main","WidgetInfo",tar_idx],new_conf);
              console.log(new_defConfig)
              onDefChange(new_defConfig)
    

            }}/>
            break;
          
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

                let newWidgetInfo=[...defConfig.main.WidgetInfo];
                newWidgetInfo.push({
                  ...newUIEleConf
                });

                let newDefConfig=ObjShellingAssign(defConfig,["main","WidgetInfo"],newWidgetInfo)


                let new_WidgetLayout=[...WidgetLayout];
                new_WidgetLayout[new_WidgetLayout.length-1]={
                  ...new_WidgetLayout[new_WidgetLayout.length-1],
                  i:newUIEleConf.id,
                  type:newUIEleConf.type
                }
                newDefConfig=ObjShellingAssign(newDefConfig,["main","WidgetLayout"],new_WidgetLayout)
                console.log(newDefConfig);
                onDefChange(newDefConfig)
          
                setNewUIEleConf({});


              }}>

                +

              </Button>
            
            </div>
            break;
          default:
            
            break;
        }
        return <div key={uilayoutInfo.i}>
          {UI}
      
        
        {
          UIEditFlag==false?null: <div  style={{background:UIEditFlag?"rgba(255,255,255,0.7)":undefined,position: "absolute",width:"100%",height:"100%",top:"0px"}}>
            {uilayoutInfo.i}

            <br/>
            {uilayoutInfo.type}

            <br/>

            <Switch checkedChildren="顯示" unCheckedChildren="隱藏" checked={uilayoutInfo.display!=false} onChange={(check)=>{

              console.log(uilayoutInfo,check);
              let newDefConfig=ObjShellingAssign(defConfig,["main","WidgetLayout",idx],{...uilayoutInfo,display:check})
              onDefChange(newDefConfig)
            }}/>

          </div>
        }
         
      
      </div>})}
      
    </ResponsiveReactGridLayoutX>

}



let DAT_ANY_UNDEF:any=undefined;





const initialNodes = [
  { id: '1', position: { x: 0, y: 0 }, data: { label: '1' } },
  { id: '2', position: { x: 0, y: 100 }, data: { label: '2' } },
  { id: '3', position: { x: 0, y: 200 }, data: { label: '3' } },
];

const initialEdges = [{ id: 'e1-2', source: '1', target: '2' },{ id: 'e2-3', source: '2', target: '3' },{ id: 'e1-3', source: '1', target: '3' }];

function NodeFlow_DEMO({defConfig}:any) {
  const [nodes, setNodes, onNodesChange] = useNodesState(initialNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialEdges);

  const onConnect = useCallback((params) => setEdges((eds) => addEdge(params, eds)), [setEdges]);

  console.log(defConfig.InspTars_main);

  useEffect(() => {
    let nodes=defConfig.InspTars_main.map((it:any,idx:any)=>({
      id:it.id,
      position:{x:0,y:idx*100},
      data:{label:it.id}
    })).filter((node:any)=>node.id!=="ImTran")

    let edges=defConfig.InspTars_main.map((it:any,idx:number,arr:any[])=>{
      let cid=it.id;
      let cand_it=arr
        .filter((sit:any)=>sit.match_tags.find((tag:string)=>tag==cid)!==undefined)
      
      return cand_it
        .map((it:any)=>it.id)
        .map((id:string)=>(
          { id: `${cid}-${id}`, source:cid, target: id }
        ))
    }).flat()
    console.log(nodes,edges);
    setNodes(nodes);
    setEdges(edges);

  },defConfig)

  return (
    <ReactFlow
      nodes={nodes}
      edges={edges}
      onNodesChange={onNodesChange}
      onEdgesChange={onEdgesChange}
      onConnect={onConnect}
    >
      <MiniMap />
      <Controls />
      <Background />
    </ReactFlow>
  );
}



function VIEWUI(){


  const _this = useRef<any>({
    listCMD_Vairable:{
      $DEFPATH:"",
      inCMD_Promise:false,
      InspTarDispIDList:undefined,
      cur_defInfo:{},
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



  const [defConfig,setDefConfig]=useState<any>(undefined);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);

  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);



  const emptyModalInfo={
    timeTag:0,
    visible:false,
    type:"",
    onOK:()=>{},
    onCancel:()=>{},
    title:"" as string|undefined,
    DATA:DAT_ANY_UNDEF,
    content:DAT_ANY_UNDEF

  }
  const [modalInfo,setModalInfo]=useState(emptyModalInfo);

  async function LOADPrjDef(PrjDefFolderPath:string)
  {
    let api = BPG_API
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
    CNC_API.disconnect();
    if(main.PeripheralInfo && main.PeripheralInfo.connection_info)
    {
      CNC_API.machineSetup=main.PeripheralInfo.machine_setup;

      CNC_API.connect(main.PeripheralInfo.connection_info);
    }


    let XCmds=await  api.FILE_Load( PrjDefFolderPath+"/XCmds.json");
    _this.listCMD_Vairable.$DEFPATH=PrjDefFolderPath;
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

    let api = BPG_API
    await api.FILE_Save(PrjDefFolderPath+"/main.json",PrjDef.main,true)
    await api.FILE_Save(PrjDefFolderPath+"/XCmds.json",PrjDef.XCmds,true)

    for(let it of PrjDef.InspTars_main)
    {
      let path = PrjDefFolderPath+"/it_"+it.id+"/main.json"
      console.log(path,it)
      await api.FILE_Save(path,it,true)
    }
    return true
  }
  async function CameraInfoDoConnection(CameraInfo:type_CameraInfo[],froceReconnect=false)
  {
    let api = BPG_API
    let connCameraInfo = await api.CameraCheckAndConnect(CameraInfo,froceReconnect)
    return CameraInfo.map((ci:type_CameraInfo)=>{
     let connTar = connCameraInfo.find(cCam=>cCam.id==ci.id);
     return {...ci,available:connTar!==undefined}
    })
  }
  async function ReloadPrjDef(path:string)  
  {
    let prjDef = await LOADPrjDef( path)

    _this.listCMD_Vairable.InspTarDispIDList=prjDef.InspTars_main.reduce((pval,cval)=>{
      console.log(cval);
      if(cval.default_hide!==true)
        pval.push(cval.id);
      return pval;
    },[] as string[]);



    console.log(_this.listCMD_Vairable.InspTarDispIDList,prjDef)
    let api = BPG_API

    prjDef.main.CameraInfo= await CameraInfoDoConnection(prjDef.main.CameraInfo,true)
    // updateDefInfo();
    await api.InspTargetRemoveAll()

    let inspTarStreamingId=INSPTAR_BASE_STREAM_ID;
    for(let inspTar of prjDef.InspTars_main)
    {
      
      let id=inspTar.id;
      // if(inspTar.stream_id===undefined)
      // {
      inspTar.stream_id=inspTarStreamingId;
      inspTarStreamingId++;
      // }

      // console.log(id,inspTar)


      await api.InspTargetCreate(inspTar,prjDef.path+"/it_"+inspTar.id);

      await api.InspTargetSetStreamChannelID(
        inspTar.id,inspTar.stream_id,
        {
          resolve:(pkts)=>{
            // console.log(pkts);
          },
          reject:(pkts)=>{
            console.log(pkts);
  
          }
        }
      )





      let collection_PGID=53450
      let cbKey="TargetVIEWUI_CB";
      await api.send_cbs_attach(inspTar.stream_id,cbKey,{
        resolve:(pkts)=>{
          
          let RP=pkts.find((info:any)=>info.type=="RP")
          if(RP===undefined)return;
          RP=RP.data;
          let filteredKey=Object.keys(_this.listCMD_Vairable.reportListener)
            .filter(key=>{
              let repListener=_this.listCMD_Vairable.reportListener[key];
              if(repListener.inspTar_id && repListener.inspTar_id!==RP.InspTar_id)return false;
              if(repListener.trigger_id && repListener.trigger_id!==RP.trigger_id)return false;
              if(repListener.type && repListener.type!==RP.type)return false;
              if(repListener.report!==undefined)return false;
              return true;

            })
          
          filteredKey.forEach(key=>{
            if(_this.listCMD_Vairable.reportListener[key].resolve!==undefined)
            {
              _this.listCMD_Vairable.reportListener[key].resolve(RP);
            }
            else
            {
              _this.listCMD_Vairable.reportListener[key].report=RP;
            }
          })
          // console.log(RP.data);
        },
        reject:(pkts)=>{

        }
      })
      
    }
    
    let infoList = await api.InspTargetGetInfo();
    

    // InspTargetReload(defInfo:any,_PGID_:number)
    // ddd
    setDefConfig(prjDef);
    console.log(prjDef,infoList)
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



  let displayInspTarId=_this.listCMD_Vairable.InspTarDispIDList as string[];//=defInfo.rules.map((rule,idx)=>def.id+" ");
  
  let displayInspTarIdx:number[]=[];
  let displayInspTarIdx_hide:number[]=[];
  if(defConfig!==undefined)
  {
    if(_this.listCMD_Vairable.InspTarDispIDList===undefined)
    {
      displayInspTarId=defConfig.InspTars_main.map((itar:any)=>itar.id);
    }
  
   
    displayInspTarIdx=displayInspTarId
      .map(itarID=>defConfig.InspTars_main.findIndex((itar:any)=>itar.id==itarID))
      .filter(idx=>idx>=0)
  
    displayInspTarIdx_hide
      =defConfig.InspTars_main.map((itar:any,idx:number)=>idx).filter((idx:number)=>{
        if(displayInspTarIdx.find(idx_to_show=>idx_to_show==idx)===undefined)
          return true;
        return false;
      });
  
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
          _this.listCMD_Vairable.USER_INPUT=setting.data.map((info:any)=>info.default);
          _this.listCMD_Vairable.USER_INPUT_LOCK=false;
          let updateUI=(data:any)=>
          {

            let content=data.map((info:any,dataIndex:number)=>{

              let doms=

              info.opts.map((opt:any)=>{



                if (typeof opt === 'object' ) {

                  switch(opt.type)
                  {
                    case "InspTar_UI":{
                      let id = opt.id

                      let itar=defConfig.InspTars_main.find( (ipt:any)=>ipt.id==id)
                      // console.log(itar)
                      if(itar===undefined)return "InspTar NotFound"
    
                      return <InspTargetUI_MUX 
                        display={true} 
                        // width={80} 
                        // height={70} 
                        style={{float:"left"}} 
                        EditPermitFlag={EDIT_PERMIT_FLAG.OPONLY}
                        key={id} 
                        systemInspTarList={defConfig.InspTars_main}
                        def={itar} 
                        report={undefined} 
                        fsPath={defConfig.path+"/it_"+id}
                        renderHook={undefined} 
                        onDefChange={(new_rule,doInspUpdate=true)=>{
    
                        }}
              
                        APIExport={opt.APIExport}


                        UIOption={undefined}
                        showUIOptionConfigUI={false}
                        onUIOptionUpdate={(newUIOption)=>{
                          console.log(newUIOption)
                        }}
                      />
                    }
    

                    case "button":{
                      let key = opt.key || opt.text
                      let text= opt.text || key

    
                      return <Button onClick={()=>{

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
      
      
                        }}>{(typeof text === 'string')?text:text(dataIndex)}</Button>
                    }
    

                  }
                  return "OBJ INFO IS NOT HANDLED"
                }




                if(opt=="$\n")return <br/>;


                // if(opt.startsWith("$\s")){
                //   let count=opt.slice(2);
                //   return 
                // }

                if(opt=="$\s")return " ";



                if(opt.startsWith("$t:")){
                  return opt.slice(3).replace(/ /g, "\u00A0")
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

              console.log(doms)
              return <>
                
                {(info.text===undefined || info.text===null)?null:<Divider> {(typeof info.text === 'string')?info.text:info.text(dataIndex)} </Divider>}
    

                {doms}


              </>
            });

            setModalInfo({
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
                reject(false)
                setModalInfo({...modalInfo,visible:false})
              },
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
      setModalInfo({
        timeTag:Date.now(),
        visible:true,
        type:"CHECK",
        onOK:()=>{
          setModalInfo({...modalInfo,visible:false})
        },
        onCancel:()=>{
          setModalInfo({...modalInfo,visible:false})
        },
        title:"!!!!錯誤 例外!!!!",
        DATA:{info:`${JSON.stringify(e,null,2)}`},
        content:JSON.stringify(e,null,2)
      })
    });
  }



  // console.log(displayInspTarId,displayInspTarIdx,displayInspTarIdx_hide);

  let InspMenu=
  menuCol('檢驗', 'insp',undefined, [
    ...(
      defConfig===undefined?[ menuCol("WAIT...","WAIT...")]:
      (
        [
          ...displayInspTarIdx.map((InspTarIdx:number,listIndex:number)=>{
            let inspTar=defConfig.InspTars_main[InspTarIdx];

            return  menuCol(<div onClick={()=>{
              


              displayInspTarId.splice(listIndex, 1);
              _this.listCMD_Vairable.InspTarDispIDList=displayInspTarId;


              console.log(displayInspTarId)
              setForceUpdateCounter(forceUpdateCounter+1);



            }}>
              {inspTar.id}
            </div>,inspTar.id)
          }),
          menuCol(<div onClick={()=>{
              


            const layout = 
            [
              {
                  "w": 1,
                  "h": 1,
                  "x": 0,
                  "y": 0,
                  "i": "a",
                  "moved": false,
                  "static": false,
                  "isDraggable": true,
                  "isResizable": true
              },
              {
                  "w": 2,
                  "h": 1,
                  "x": 1,
                  "y": 0,
                  "i": "b",
                  "moved": false,
                  "static": false,
                  "isDraggable": true,
                  "isResizable": true
              },
              {
                  "w": 2,
                  "h": 1,
                  "x": 0,
                  "y": 1,
                  "i": "c",
                  "moved": false,
                  "static": false,
                  "isDraggable": true,
                  "isResizable": true
              },
              {
                  "w": 1,
                  "h": 1,
                  "x": 3,
                  "y": 0,
                  "i": "d",
                  "moved": false,
                  "static": false,
                  "isDraggable": true,
                  "isResizable": true
              }
            ]
            

            setModalInfo({
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
              DATA:"",
              content:<>
                {/* <NodeFlow_DEMO defConfig={defConfig}/> */}

                <ResponsiveReactGridLayoutX layouts={undefined}
                          //  onDrop={(e) => onDrop(e)} 
                  onLayoutChange={(curL,allL)=>{
                    console.log(curL,allL)
                  }}
                  className="layout" 
                  //  layouts={layouts} 
                  breakpoints={{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
                  cols={{ lg: 5, md: 5, sm: 5, xs: 5, xxs: 5 }}
                  //  // rowHeight={300}
                  //  // width={1000}
                  resizeHandles={["se"]}
                  isDroppable={true}
                  autoSize={true}
                  //  onWidthChange={(containerWidth,margin,cols,cpadding)=>{
                  //   console.log(containerWidth,margin,cols,cpadding);
                  //  }}
                  
                
                
                >
                  <div key="a" style={{ backgroundColor: "#ccc" }}><span>a</span></div>
                  <div key="b" style={{ backgroundColor: "#ccc" }}>b</div>
                  <div key="c" style={{ backgroundColor: "#ccc" }}>c</div>
                  <div key="d" style={{ backgroundColor: "#ccc" }}>


                  </div>
                </ResponsiveReactGridLayoutX>
              </>
            })

          }}>
            ------------
          </div>,"divLine"),
          ...displayInspTarIdx_hide.map((InspTarIdx:number,listIndex:number)=>{
            let inspTar=defConfig.InspTars_main[InspTarIdx];

            return  menuCol(<div onClick={()=>{
              


              displayInspTarId.push(inspTar.id);
              _this.listCMD_Vairable.InspTarDispIDList=displayInspTarId;


              console.log(displayInspTarId)
              setForceUpdateCounter(forceUpdateCounter+1);



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

  let cameraMenu=
  menuCol('相機', 'cam',undefined, [
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
                content:<CameraSetupEditUI key={keyTime} CoreAPI={BPG_API} camSetupInfo={ncamInfo}  onCameraSetupUpdate={ncam=>{
                  console.log(ncam)
                  updater(ncam);
                  (async function(){
                    console.log(ncam);
                    let trigMode=ncam.trigger_mode;
                   
                    await BPG_API.CameraSetup({...ncam,frame_rate:15},trigMode);
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
                  setDefConfig(new_defConfig)
                  setModalInfo(emptyModalInfo)
                  
                },
                
                onCancel:()=>{
                  
                  (async function(){
                    let api =BPG_API
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
    menuCol(<Dropdown
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
        let api = BPG_API
        let camList = await api.queryDiscoverList();
        setCameraQueryList(camList);
        console.log(camList)
      })();
      
    }}>+
    </div>



    </Dropdown>, 'Add'),
    menuCol(
    
    <div onClick={()=>{
      (async ()=>{
        let newPrjDef={...defConfig};
        newPrjDef.main.CameraInfo= await CameraInfoDoConnection(defConfig.main.CameraInfo,true)


        setDefConfig(newPrjDef);
      })();
      
    }}>Refresh
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
          setDefConfig(new_defConfig);
          setXCMDIdx(-1);
        }}>+</div>,"_ADD_")//new xcmd 

      ]
      
    )

  const items: MenuItem[] = [
    cameraMenu,
    InspMenu,
    xcmdMenu
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
                  setDefConfig(new_defConfig);

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


                    setDefConfig(new_defConfig);
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
                      setDefConfig(new_defConfig);


                    }}>+</Button>
                    <Button size="small" onClick={()=>{
                  
                      let new_cmd_list=[...curXCMD.cmds]
                      new_cmd_list.splice(idx, 1);
                      let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds"],new_cmd_list);
                      setDefConfig(new_defConfig);
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
                  setDefConfig(new_defConfig);

                }}/>
              </>)
            }

            <Button size="small" onClick={()=>{
              
              let new_cmd_list=[...curXCMD.cmds]
              new_cmd_list.push("");
              let new_defConfig=ObjShellingAssign(defConfig,["XCmds",xCMDIdx,"cmds"],new_cmd_list);
              setDefConfig(new_defConfig);

            }}>+</Button>



















  </div>


  let siderUI=(editPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)==0?null:
  <Sider width={siderBaseSize+extSizerSize}>
  {baseSiderTabs}
  {extSiderTabs}
  </Sider>
    


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
    
        <Menu.Item key="2" onClick={()=>{
          SavePrjDef(_DEF_FOLDER_PATH_,defConfig);
        }}>SAVE</Menu.Item>
          </>
        }


    </Menu>
    

    
    <DraggableModalProvider>
    <DraggableModal
        title={modalInfo.title}
        visible={modalInfo.visible}
        onOk={modalInfo.onOK}
        // confirmLoading={confirmLoading}
        onCancel={modalInfo.onCancel}
      >
        {modalInfo.content}
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
    (defConfig===undefined)?"WAIT": 
      <TargetViewUIShow displayIDList={displayInspTarId} defConfig={defConfig} UIEditFlag={UIEditFlag} EditPermitFlag={editPermitFlag}  onDefChange={(newdef:any)=>{
        console.log(newdef);
        setDefConfig(newdef)
        }}  renderHook={_this.listCMD_Vairable.renderHook}/>}
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
