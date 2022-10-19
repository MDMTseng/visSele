import React from 'react';
import { useState, useEffect,useRef,useMemo,useContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch,Select } from 'antd';

const { Option } = Select;

import type { MenuProps, MenuTheme } from 'antd/es/menu';
const { SubMenu } = Menu;
import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,MoreOutlined,PlayCircleOutlined,
  DisconnectOutlined,LinkOutlined } from '@ant-design/icons';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign} from './UTIL/MISC_Util';

import {listCMDPromise} from './XCMD';


import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg,PtRotate2d} from './UTIL/MathTools';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message } from 'antd';


import { type_CameraInfo,type_IMCM} from './AppTypes';
import './basic.css';


enum EDIT_PERMIT_FLAG {
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


type CompParam_InspTarUI =   {
  display:boolean,
  style?:any,
  stream_id:number,
  fsPath:string,
  EditPermitFlag:number,
  width:number,height:number,
  renderHook:((ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,rule:any)=>void)|undefined,
  // IMCM_group:IMCM_group,
  def:any,
  report:any,
  onDefChange:(updatedDef:any,ddd:boolean)=>void}

  

type MenuItem = Required<MenuProps>['items'][number];

var enc = new TextEncoder();

const _DEF_FOLDER_PATH_="data/Test1.xprj";
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

function ColorRegionDetection_SingleRegion({srule,onDefChange,canvas_obj}:
  {
    srule:any,
    onDefChange:(...param:any)=>void,
    canvas_obj:DrawHook_CanvasComponent
  })
{
  
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  let srule_Filled={
    
    region:[0,0,0,0],
    colorThres:10,
    hough_circle:{
      dp:1,
      minDist:5,
      param1:100,
      param2:30,
      minRadius:5,
      maxRadius:15
    },
    hsv:{
      rangeh:{
        h:180,s:255,v:255
      },
      rangel:{
        h:0,s:0,v:0
      },
    },

    ...srule
  }
  const _this = useRef<any>({}).current;
  return <>
    <Button key={">>>"} onClick={()=>{
        canvas_obj.UserRegionSelect((info,state)=>{
          if(state==2)
          {
            console.log(info);
            
            let x,y,w,h;

            x=info.pt1.x;
            w=info.pt2.x-info.pt1.x;
        
            y=info.pt1.y;
            h=info.pt2.y-info.pt1.y;
        
        
            if(w<0){
              x+=w;
              w=-w;
            }
            
            if(h<0){
              y+=h;
              h=-h;
            }

            let newRule={...srule_Filled,region:[Math.round(x),Math.round(y),Math.round(w),Math.round(h)]};
            onDefChange(newRule)

            canvas_obj.UserRegionSelect(undefined)
          }
        })
      }}>設定範圍</Button>
    
      <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onDefChange(undefined);
                // onDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
{/* 
    <br/>hough_circle
    <Slider defaultValue={srule_Filled.hough_circle.minRadius} max={100} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,minRadius:v}};
      onDefChange(newRule)
    },()=>_this.trigTO=undefined,500);

    }}/>
    
    <Slider defaultValue={srule_Filled.hough_circle.maxRadius} max={100} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,maxRadius:v}};
        onDefChange(newRule)
      },()=>_this.trigTO=undefined,500);

    }}/>  */}




    <>
    
    
    <br/>結果顯示
    <Slider defaultValue={srule_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["resultOverlayAlpha"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>

    <br/>HSV
    <Slider defaultValue={srule_Filled.hsv.rangeh.h} max={180} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","h"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.h} max={180} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","h"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    <Slider defaultValue={srule_Filled.hsv.rangeh.s} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","s"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.s} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","s"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>




    <Slider defaultValue={srule_Filled.hsv.rangeh.v} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","v"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.v} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{

        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","v"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>





    </>

    <br/>細節量
    <Slider defaultValue={srule_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}





function SingleTargetVIEWUI_ColorRegionDetection({display,stream_id,fsPath,width,height,EditPermitFlag,style=undefined,renderHook,def,report,onDefChange}:CompParam_InspTarUI){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined,
    drawHooks:[],
    ctrlHooks:[]


  });
  const [cacheDef,setCacheDef]=useState<any>(def);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);


  const [defReport,setDefReport]=useState<any>(undefined);
  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);
  let _this=_.current;
  let c_report:any = undefined;
  if(_this.cache_report!==report)
  {
    if(report!==undefined)
    {
      _this.cache_report=report;
    }
  }
  c_report=_this.cache_report;


  useEffect(() => {
    console.log("fsPath:"+fsPath)
    _this.cache_report=undefined;
    setCacheDef(def);
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, [def]); 
  // console.log(IMCM_group,report);
  // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  
  enum editState {
    Normal_Show = 0,
    Region_Edit = 1,
  }
  
  const [stateInfo,setStateInfo]=useState<{st:editState,info:any}[]>([{
    st:editState.Normal_Show,
    info:undefined
  }]);

  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);

  
  const [queryCameraList,setQueryCameraList]=useState<any[]|undefined>(undefined);
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);

  
  let stateInfo_tail=stateInfo[stateInfo.length-1];



  function onCacheDefChange(updatedDef: any,ddd:boolean)
  {
    console.log(updatedDef);
    setCacheDef(updatedDef);

    

    (async ()=>{
      await BPG_API.InspTargetUpdate(updatedDef)
      
      await BPG_API.CameraSWTrigger("Hikrobot-00F71598890","TTT",4433)
      
      // await BPG_API.CameraSWTrigger("BMP_carousel_0","TTT",4433)

    })()
    
  }

  
  useEffect(() => {//////////////////////

    (async ()=>{

      let ret = await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

      await BPG_API.send_cbs_attach(
        cacheDef.stream_id,"ColorRegionDetection",{
          
          resolve:(pkts)=>{
            // console.log(pkts);
            let IM=pkts.find((p:any)=>p.type=="IM");
            if(IM===undefined)return;
            let CM=pkts.find((p:any)=>p.type=="CM");
            if(CM===undefined)return;
            let RP=pkts.find((p:any)=>p.type=="RP");
            if(RP===undefined)return;
            console.log("++++++++\n",IM,CM,RP);


            setDefReport(RP.data)
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
        }

      )
      // await BPG_API.InspTargetSetStreamChannelID(
      //   cacheDef.id,stream_id,
      //   {
      //     resolve:(pkts)=>{
      //       // console.log(pkts);
      //       let IM=pkts.find((p:any)=>p.type=="IM");
      //       if(IM===undefined)return;
      //       let CM=pkts.find((p:any)=>p.type=="CM");
      //       if(CM===undefined)return;
      //       let RP=pkts.find((p:any)=>p.type=="RP");
      //       if(RP===undefined)return;
      //       console.log("++++++++\n",IM,CM,RP);


      //       setDefReport(RP.data)
      //       let IMCM={
      //         image_info:IM.image_info,
      //         camera_id:CM.data.camera_id,
      //         trigger_id:CM.data.trigger_id,
      //         trigger_tag:CM.data.trigger_tag,
      //       } as type_IMCM
  
      //       _this.imgCanvas.width = IMCM.image_info.width;
      //       _this.imgCanvas.height = IMCM.image_info.height;
  
      //       let ctx2nd = _this.imgCanvas.getContext('2d');
      //       ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
  
  
      //       setLocal_IMCM(IMCM)
      //       // console.log(IMCM)
  
      //     },
      //     reject:(pkts)=>{
  
      //     }
      //   }
      // )

    })()
    return (() => {
      (async ()=>{
        await BPG_API.send_cbs_detach(
          stream_id,"ColorRegionDetection");
            
        // await BPG_API.InspTargetSetStreamChannelID(
        //   cacheDef.id,0,
        //   {
        //     resolve:(pkts)=>{
        //     },
        //     reject:(pkts)=>{
    
        //     }
        //   }
        // )
      })()
      
    })}, []); 
  // function pushInSendGCodeQ()
  // {
  //   if(_this.isSendWaiting==true || _this.gcodeSeq.length==0)
  //   {
  //     return;
  //   }
  //   const gcode = _this.gcodeSeq.shift();
  //   if(gcode==undefined || gcode==null)return;
  //   _this.isSendWaiting=true;
  //   ACT_WS_GET_OBJ((api)=>{
  //     api.send({"type":"GCODE","code":gcode},
  //     (ret)=>{
  //       console.log(ret);
  //       _this.isSendWaiting=false;
  //       pushInSendGCodeQ(_this.gcodeSeq);

  //     },(e)=>console.log(e));
  //   })
  // }


  if(display==false)return null;



  let EDIT_UI= null;
  
  switch(stateInfo_tail.st)
  {
    
    case editState.Normal_Show:


      let EditUI=null;
      if(true)//allow edit
      {
        EditUI=<>
        <Button key={"_"+10000} onClick={()=>{
          
          let newDef={...cacheDef};
          newDef.regionInfo.push({region:[0,0,0,0],colorThres:10});
          onCacheDefChange(newDef,false)

          
          setStateInfo([...stateInfo,{
            st:editState.Region_Edit,
            info:{
              idx:newDef.regionInfo.length-1
            }
          }])

        }}>+</Button>

        

        {cacheDef.regionInfo.map((region:any,idx:number)=>{
          return <Button key={"_"+idx} onClick={()=>{
            if(_this.canvasComp===undefined)return;


            setStateInfo([...stateInfo,{
              st:editState.Region_Edit,
              info:{
                idx:idx
              }
            }])




          }}>{"idx:"+idx}</Button>
        })}
        </>
      }

      EDIT_UI=<>
        
        <Input maxLength={100} value={cacheDef.id} 
          style={{width:"100px"}}
          onChange={(e)=>{
            console.log(e.target.value);

            let newDef={...cacheDef};
            newDef.id=e.target.value;
            onCacheDefChange(newDef,false)


          }}/>

        <Input maxLength={100} value={cacheDef.type} disabled
          style={{width:"100px"}}
          onChange={(e)=>{
            
          }}/>

        <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/>


        <Dropdown
          overlay={<>
            <Menu>
              {
                queryCameraList===undefined?
                  <Menu.Item disabled danger>
                  <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                    Press to update
                  </a>
                  </Menu.Item>
                  :
                  queryCameraList.map(cam=><Menu.Item key={cam.id} 
                  onClick={()=>{
                    let newDef={...cacheDef};
                    newDef.camera_id=cam.id;
                    // HACK_do_Camera_Check=true;
                    onCacheDefChange(newDef,true)
                  }}>
                    {cam.id}
                  </Menu.Item>)

              }
            </Menu>
          </>}
        >
          <Button onClick={()=>{
            // queryCameraList
            setQueryCameraList(undefined);
            BPG_API.queryDiscoverList()
              .then((e: any)=>{
                console.log(e);
                setQueryCameraList(e[0].data)
              })
            // let api=await getAPI(CORE_ID) as BPG_WS;
            // let cameraListInfos=await api.cameraDiscovery() as any[];
            // let CM=cameraListInfos.find(info=>info.type=="CM")
            // if(CM===undefined)throw "CM not found"
            // console.log(CM.data);
            // return CM.data as {name:string,id:string,driver_name:string}[];
            
          }}>{cacheDef.camera_id}</Button>
        </Dropdown>



        <Input maxLength={100} value={cacheDef.trigger_tag} 
          style={{width:"100px"}}
          onChange={(e)=>{
            let newDef={...cacheDef};
            newDef.trigger_tag=e.target.value;
            onCacheDefChange(newDef,false)
        }}/>

        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onCacheDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
        <br/>
        <Button onClick={()=>{
          onCacheDefChange(cacheDef,true);
        }}>SHOT</Button>
        <Button onClick={()=>{
          onDefChange(cacheDef,true)
        }}>SAVE</Button>

        {EditUI}


      </>


      break;

    case editState.Region_Edit:


      if(cacheDef.regionInfo.length<=stateInfo_tail.info.idx)
      {
        break;
      }
      
      let regionInfo=cacheDef.regionInfo[stateInfo_tail.info.idx];

      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>
        <ColorRegionDetection_SingleRegion 
          srule={regionInfo} 
          onDefChange={(newDef_sregion)=>{
            // console.log(newDef);

            
            let newDef={...cacheDef};
            if(newDef_sregion!==undefined)
            {
              newDef.regionInfo[stateInfo_tail.info.idx]=newDef_sregion;
            }
            else
            {
              
              newDef.regionInfo.splice(stateInfo_tail.info.idx, 1);
              
              let new_stateInfo=[...stateInfo]
              new_stateInfo.pop();

              setStateInfo(new_stateInfo)

            }

            onCacheDefChange(newDef,true)
            // _this.sel_region=undefined

          }}
          canvas_obj={_this.canvasComp}/>
      </>

      break;
  }
  

  
  return <div style={{...style,width:width+"%",height:height+"%"}}  className={"overlayCon"}>

    <div className={"overlay"} >

      {EDIT_UI}

    </div>

    
    <HookCanvasComponent style={{}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);
      if(ctrl_or_draw==true)//ctrl
      {
        // if(canvas_obj.regionSelect===undefined)
        // canvas_obj.UserRegionSelect((onSelect,draggingState)=>{
        //   if(draggingState==1)
        //   {

        //   }
        //   else if(draggingState==2)
        //   {
        //     console.log(onSelect);
        //     canvas_obj.UserRegionSelect(undefined)
        //   }
        // });
        
        // ctrlHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
        if(canvas_obj.regionSelect!==undefined)
        {
          if(canvas_obj.regionSelect.pt1===undefined || canvas_obj.regionSelect.pt2===undefined)
          {
            return;
          }
      
          let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
           
          
          // console.log(canvas_obj.regionSelect);
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
          _this.sel_region={
            x,y,w,h
          }
        }
      }
      else//draw
      {
        if(Local_IMCM!==undefined)
        {
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       

        let ctx = g.ctx;
        
        {
          cacheDef.regionInfo.forEach((region:any,idx:number)=>{

            let region_ROI=
            {
              x:region.region[0],
              y:region.region[1],
              w:region.region[2],
              h:region.region[3]
            }
            
            ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
            drawRegion(g,canvas_obj,region_ROI,canvas_obj.rUtil.getIndicationLineSize());
                    
            ctx.font = "40px Arial";
            ctx.fillStyle = "rgba(0, 179, 0,0.5)";
            ctx.fillText("idx:"+idx,region_ROI.x,region_ROI.y)

            
            let region_components=GetObjElement(c_report,["regionInfo",idx,"components"]);
            // console.log(report,region_components);
            if(region_components!==undefined)
            {
              region_components.forEach((regComp:any)=>{

                canvas_obj.rUtil.drawCross(ctx, {x:regComp.x,y:regComp.y}, 5);

                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(regComp.area,regComp.x,regComp.y)
                
                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(`${regComp.x},${regComp.y}`,regComp.x,regComp.y+5)
              })

            }



          })
        }


        if(canvas_obj.regionSelect!==undefined && _this.sel_region!==undefined)
        {
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          drawRegion(g,canvas_obj,_this.sel_region,canvas_obj.rUtil.getIndicationLineSize());
      
        }
      }

      
      if(renderHook)
      {
        // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
      }
    }
    }/>

  </div>;

}



function Orientation_ColorRegionOval_SingleRegion({srule,onDefChange,canvas_obj}:
  {
    srule:any,
    onDefChange:(...param:any)=>void,
    canvas_obj:DrawHook_CanvasComponent
  })
{
  
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  let srule_Filled={
    
    region:[0,0,0,0],
    colorThres:10,
    hsv:{
      rangeh:{
        h:180,s:255,v:255
      },
      rangel:{
        h:0,s:0,v:0
      },
    },

    contour:{
      lengthh:400000,
      lengthl:70,

      areah:639030,
      areal:400,

    },

    ...srule
  }
  const _this = useRef<any>({}).current;
  return <>
    <Button key={">>>"} onClick={()=>{
        canvas_obj.UserRegionSelect((info,state)=>{
          if(state==2)
          {
            console.log(info);
            
            let x,y,w,h;

            x=info.pt1.x;
            w=info.pt2.x-info.pt1.x;
        
            y=info.pt1.y;
            h=info.pt2.y-info.pt1.y;
        
        
            if(w<0){
              x+=w;
              w=-w;
            }
            
            if(h<0){
              y+=h;
              h=-h;
            }

            let newRule={...srule_Filled,region:[Math.round(x),Math.round(y),Math.round(w),Math.round(h)]};
            onDefChange(newRule)

            canvas_obj.UserRegionSelect(undefined)
          }
        })
      }}>設定範圍</Button>
    
      <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onDefChange(undefined);
                // onDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
{/* 
    <br/>hough_circle
    <Slider defaultValue={srule_Filled.hough_circle.minRadius} max={100} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,minRadius:v}};
      onDefChange(newRule)
    },()=>_this.trigTO=undefined,500);

    }}/>
    
    <Slider defaultValue={srule_Filled.hough_circle.maxRadius} max={100} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,maxRadius:v}};
        onDefChange(newRule)
      },()=>_this.trigTO=undefined,500);

    }}/>  */}




    <>
    
    
    <br/>結果顯示
    <Slider defaultValue={srule_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["resultOverlayAlpha"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>

    <br/>HSV
    <Slider defaultValue={srule_Filled.hsv.rangeh.h} max={180} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","h"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.h} max={180} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","h"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    <Slider defaultValue={srule_Filled.hsv.rangeh.s} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","s"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.s} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","s"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>




    <Slider defaultValue={srule_Filled.hsv.rangeh.v} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","v"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.v} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{

        onDefChange(ObjShellingAssign(srule_Filled,["hsv","rangel","v"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    <br/>Edge thres
    <Slider defaultValue={srule_Filled.contour.lengthh} max={3000} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["contour","lengthh"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.contour.lengthl} max={3000} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["contour","lengthl"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    </>

    <br/>細節量
    <Slider defaultValue={srule_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}

function rgb2hsv (r:number, g:number, b:number) {
  let rabs, gabs, babs, rr, gg, bb, h=0, s, v:number, diff:number, diffc, percentRoundFn;
  rabs = r / 255;
  gabs = g / 255;
  babs = b / 255;
  v = Math.max(rabs, gabs, babs),
  diff = v - Math.min(rabs, gabs, babs);
  diffc = (c:number) => (v - c) / 6 / diff + 1 / 2;
  percentRoundFn = (num:number) => Math.round(num * 100) / 100;
  if (diff == 0) {
      h = s = 0;
  } else {
      s = diff / v;
      rr = diffc(rabs);
      gg = diffc(gabs);
      bb = diffc(babs);

      if (rabs === v) {
          h = bb - gg;
      } else if (gabs === v) {
          h = (1 / 3) + rr - bb;
      } else if (babs === v) {
          h = (2 / 3) + gg - rr;
      }
      if (h < 0) {
          h += 1;
      }else if (h > 1) {
          h -= 1;
      }
  }
  // return {
  //     h: Math.round(h * 360),
  //     s: percentRoundFn(s * 100),
  //     v: percentRoundFn(v * 100)
  // };

  return [h * 180,s * 255,v * 255];
}


function TestInputSelectUI({folderPath,stream_id}:{folderPath:string,stream_id:number})
{
  const _this = useRef<any>({}).current;
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [imageFolderInfo,setImageFolderInfo]=useState<any>(undefined);
  const [finalReports,setFinalReports]=useState<any>({});
  const [latestSelect,setLatestSelect]=useState<any>(undefined);

  const injectID_Prefix="s_InjectID_";
  const cbs_key="xxxx";
  _this.finalReports=finalReports;




  function FileListReset()
  {
    (async ()=>{
      let folderContent=await BPG_API.Folder_Struct(folderPath,2);
      let regex = /.+\.png/i;

      let imageInfo=folderContent.files
        .filter((finfo:any)=>(finfo.name!="FeatureRefImage.png")&&regex.test(finfo.name))
        .sort((finfo1:any,finfo2:any)=>finfo1.mtime_ms-finfo2.mtime_ms)

      console.log(imageInfo);
      
      folderContent.files=imageInfo;
      setImageFolderInfo(folderContent);

      setFinalReports({})//clear
      setLatestSelect(undefined);
      console.log(folderContent)


    })()
  }



  useEffect(() => {//////////////////////
    console.log("TestInputSelectUI INIT");




    BPG_API.send_cbs_attach(
      stream_id,cbs_key,{
        
        resolve:(pkts)=>{
          let RP=pkts.find((p:any)=>p.type=="RP");
          if(RP===undefined)return;

          let tags=RP.data.tags as string[];
          let injID=tags.find((tag:string)=>tag.startsWith(injectID_Prefix));
            
          if(injID===undefined)return;
          injID=injID.replace(injectID_Prefix,"");


          setFinalReports({..._this.finalReports,[injID]:RP.data})


        },
        reject:(pkts)=>{

        }
      }

    )


    FileListReset();










    return (() => {

      BPG_API.send_cbs_detach(stream_id,cbs_key);
    
      console.log("TestInputSelectUI EXIT");
    });
  },[]);

  function ImgTest(folder_path:string, fileInfo:{name:string})
  {
    let sIDTag=injectID_Prefix+fileInfo.name;

    BPG_API.InjectImage(folder_path+"/"+fileInfo.name,["UI_Inject",sIDTag],Date.now());

    setLatestSelect({
      ...imageFolderInfo,
      sIDTag,
      file:fileInfo
    });
    
  }

  let bottonRunAll=imageFolderInfo===undefined?null:
  <Button onClick={()=>{

    setFinalReports({})//clear
    setLatestSelect(undefined);
    imageFolderInfo.files.forEach((file:any)=>{
      // let resultType=NaN;
      // let report=finalReports[file.name];
      // if(report!==undefined)
      // {
      //   resultType=report.report.category
      // }
      if(file.name.startsWith("IG_"))return;

      ImgTest(imageFolderInfo.path, file);
    })

  }}>

    群組測試
  </Button>

  // console.log(finalReports,latestSelect)
  let bottonS=imageFolderInfo===undefined?null:
  imageFolderInfo.files.map((file:any)=>{
    let hasGenOK=false;
    let hasGenNG=false;
    let report=finalReports[file.name];
    if(report!==undefined)
    {
      report.report.sub_reports.forEach((subrep:any)=>{
        if(subrep.category==1)hasGenOK=true;
        if(subrep.category==-1)hasGenNG=true;
      })
    }
    let pureGenOK=hasGenOK && !hasGenNG;
    let pureGenNG=!hasGenOK && hasGenNG;

    // console.log(hasGenOK,hasGenNG)
    // console.log(file.name,pureGenOK,pureGenNG)

    return <Button key={file.name} type={(pureGenOK||pureGenNG)?"primary":"dashed"} danger={hasGenNG} ghost={!hasGenOK&&!hasGenNG}
    onClick={()=>{
      ImgTest(imageFolderInfo.path, file);
      
    }}>
      {file.name.replace(".png","")}
    </Button>
  })


  async function fileRename(folder_path:string,cName:string,nName:string)
  {
    let renameResult=await BPG_API.FileRename(folder_path+"/"+cName,folder_path+"/"+nName);
    console.log(renameResult);
    FileListReset();

  }

  //`確定命名為OK?`
  function Btn_LatestSelectFile_Rename(prefix:string,btnText:string,confirmText:string)
  {
    return <Popconfirm
        title={confirmText}
        onConfirm={()=>{}}
        onCancel={()=>{}}
        okButtonProps={{danger:true,onClick:()=>{
          
          let fname=latestSelect.file.name;
          fname=prefix+fname.replace(/^[a-zA-Z]+_/g, "");
          fileRename(latestSelect.path,latestSelect.file.name,fname);
        }}}
        okText={"好"}
        cancelText="No"
      >
      <Button onClick={()=>{}}>
        {btnText}
      </Button>
    </Popconfirm> 
  }

  return <>
  
    <Button danger type="primary" onClick={()=>{
      FileListReset();

    }}>檔案重整</Button>

    {bottonRunAll}

    <br/>
    {latestSelect===undefined?null:<>
      {latestSelect.file.name}

{/* 
      {Btn_LatestSelectFile_Rename("NG_","NG",`確定命名為NG?`)}



      {Btn_LatestSelectFile_Rename("OK_","OK",`確定命名為OK?`)}
 */}


      {latestSelect.file.name.startsWith("IG_")? 
      Btn_LatestSelectFile_Rename("","加入群組測試",`確定設定至群組測試清單?`):
      Btn_LatestSelectFile_Rename("IG_","忽略群組測試",`確定設定至群組測試 忽略清單?`)}


    </>}
    <div style={{width:"100%",height:"400px", background:"rgba(0,0,0,0.8)",overflow:"scroll"}}>
      {bottonS}

      <br/><br/>說明:
      <Button key={"all OK log"} type="primary">
        可檢全OK
      </Button>

      <Button key={"all NG log"} type="primary" danger>
        可檢全NG
      </Button>

      <Button key={"all NG OK Mix"} type="dashed" danger>
        可檢OK NG混合
      </Button>

      <Button key={"no insp"} type="dashed" ghost>
        無可檢
      </Button>
    </div>


    </>
}


function SingleTargetVIEWUI_Orientation_ShapeBasedMatching({display,stream_id,fsPath,width,height,style=undefined,renderHook,def,EditPermitFlag,report,onDefChange}:CompParam_InspTarUI){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined,
    drawHooks:[],
    ctrlHooks:[],

    

    featureImgCanvas:document.createElement('canvas'),
  });
  const SBM_FEAT_REF_IMG_NAME="FeatureRefImage.png"
  let _this=_.current;
  const [cacheDef,setCacheDef]=useState<any>(def);
  const [featureInfoExt,setFeatureInfoExt]=useState<any>({});

  const [featureInfo,setFeatureInfo]=useState<any>({});

  const [defReport,setDefReport]=useState<any>(undefined);


  const emptyModalInfo={
    timeTag:0,
    visible:false,
    type:"",
    onOK:(minfo:any)=>{},
    onCancel:(minfo:any)=>{},
    title:"",
    DATA:DAT_ANY_UNDEF,
    contentCB:(minfo:any)=><></>

  }
  const [modalInfo,setModalInfo]=useState(emptyModalInfo);


  let c_report:any = undefined;
  if(_this.cache_report!==report)
  {
    if(report!==undefined)
    {
      _this.cache_report=report;
    }
  }
  c_report=_this.cache_report;


  useEffect(() => {
    console.log("fsPath:"+fsPath)
    _this.cache_report=undefined;
    setCacheDef(def);
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, [def]); 
  // console.log(IMCM_group,report);
  // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  
  enum EditState {
    Normal_Show = 0,
    Feature_Edit = 1,
    Search_Region_Edit = 2,
    Test_Saved_Files= 3,
    NA=-99999
  }
  
  const [editState,_setEditState]=useState<EditState>(EditState.Normal_Show);
  
  function setEditState(newEditState:EditState)
  {
    let state3Ev:EditState[]=[];//3 elements, leave,stay,enter
    if(newEditState!=editState)
    {
      state3Ev=[editState,EditState.NA,newEditState]
    }
    state3Ev.forEach((st,idx)=>{
      
      switch(st)//current state
      {
        case EditState.Normal_Show:
          if(idx==2)//enter
          {
            
          }
          else if(idx==0)//leave
          {
            
          }
          break;
        case EditState.Feature_Edit:
          if(idx==2)//enter
          {
            setFeatureInfo(cacheDef.featureInfo===undefined?{}:cacheDef.featureInfo);

            //if(featureInfoExt.IM===undefined)//do a init image fetch
            (async ()=>{

              let pkts = await BPG_API.InspTargetExchange(cacheDef.id,{
                type:"extract_feature",
                image_path:fsPath+"/"+SBM_FEAT_REF_IMG_NAME,
                feature_count:-1,
                image_transfer_downsampling:1,
              }) as any[];
  
              let newFeatureInfoExt:any={};
              
              
              let IM=pkts.find((p:any)=>p.type=="IM");
              if(IM!==undefined)
              {
                _this.featureImgCanvas.width = IM.image_info.width;
                _this.featureImgCanvas.height = IM.image_info.height;
      
                let ctx2nd = _this.featureImgCanvas.getContext('2d');
                ctx2nd.putImageData(IM.image_info.image, 0, 0);
                newFeatureInfoExt.IM=IM;
                
              }
  
              setFeatureInfoExt({...featureInfoExt,...newFeatureInfoExt})
  
  
  
            })()
          }
          else if(idx==0)//leave
          {
            setFeatureInfo({})
          }
          break;
      
        case EditState.Search_Region_Edit:          
        if(idx==2)//enter
        {
        }
        else if(idx==0)//leave
        {
        }
        break;

        case EditState.Test_Saved_Files:
        if(idx==2)//enter
        {
        }
        else if(idx==0)//leave
        {
        }
        break;

      }
    })
    _setEditState(newEditState);
  }
  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  const [updateC,setUpdateC]=useState(0);


  function onCacheDefChange(updatedDef: any,doTakeNewImage:boolean=true)
  {
    console.log(updatedDef);
    setCacheDef(updatedDef);

    

    (async ()=>{
      await BPG_API.InspTargetUpdate(updatedDef)
      
      // await BPG_API.CameraSWTrigger("Hikrobot-00F92938639","TTT",4433)
      if(doTakeNewImage)
        await BPG_API.CameraSWTrigger("BMP_carousel_0","s_Step_25",Date.now())

    })()
    
  }

  
  useEffect(() => {//////////////////////

    (async ()=>{

      let ret = await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

      await BPG_API.send_cbs_attach(
        cacheDef.stream_id,"KEY_KEY_Orientation_ShapeBasedMatching",{
          
          resolve:(pkts)=>{
            // console.log(pkts);
            let IM=pkts.find((p:any)=>p.type=="IM");
            if(IM===undefined)return;
            let CM=pkts.find((p:any)=>p.type=="CM");
            if(CM===undefined)return;
            let RP=pkts.find((p:any)=>p.type=="RP");
            if(RP===undefined)return;
            // console.log("++++++++\n",IM,CM,RP);


            setDefReport(RP.data)
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
        }

      )

    })()
    return (() => {
      (async ()=>{
        await BPG_API.send_cbs_detach(
          stream_id,"KEY_KEY_Orientation_ShapeBasedMatching");
            
        // await BPG_API.InspTargetSetStreamChannelID(
        //   cacheDef.id,0,
        //   {
        //     resolve:(pkts)=>{
        //     },
        //     reject:(pkts)=>{
    
        //     }
        //   }
        // )
      })()
      
    })}, []); 
  // function pushInSendGCodeQ()
  // {
  //   if(_this.isSendWaiting==true || _this.gcodeSeq.length==0)
  //   {
  //     return;
  //   }
  //   const gcode = _this.gcodeSeq.shift();
  //   if(gcode==undefined || gcode==null)return;
  //   _this.isSendWaiting=true;
  //   ACT_WS_GET_OBJ((api)=>{
  //     api.send({"type":"GCODE","code":gcode},
  //     (ret)=>{
  //       console.log(ret);
  //       _this.isSendWaiting=false;
  //       pushInSendGCodeQ(_this.gcodeSeq);

  //     },(e)=>console.log(e));
  //   })
  // }


  if(display==false)return null;




  let EDIT_UI= null;
  
  switch(editState)
  {
    
    case EditState.Normal_Show:


      let EditUI=null;
      if( (EditPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)!=0 )//allow edit
      {
        EditUI=<>
          


          <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onCacheDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 


        <TagsEdit_DropDown tags={cacheDef.trigger_tags}
          onTagsChange={(newTags)=>{
            
            onCacheDefChange({...cacheDef,trigger_tags:newTags},false)
          }}>
          <a>TAGS</a>
        </TagsEdit_DropDown>


        {/* <Button onClick={()=>{
          BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
        }}>重試</Button> */}


              
        <Popconfirm key={"SAVE feat ref image "+updateC}
            title={`確定要儲存此圖為特徵參考圖？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              { 
                (async ()=>{

                  let pkts = await BPG_API.InspTargetExchange(cacheDef.id,{
                    type:"cache_image_save",
                    folder_path:fsPath+"/",
                    image_name:SBM_FEAT_REF_IMG_NAME,
                  }) as any[];
                  console.log(pkts);
      
                })()
                setUpdateC(updateC+1);
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>存為特徵參考圖</Button>
        </Popconfirm> 


              

        <Button onClick={()=>{
          onDefChange(cacheDef,true)
        }}>Commit</Button>
        <Button onClick={()=>{
          
          setEditState( EditState.Feature_Edit);

        }}>編輯特徵</Button>

        <Button  onClick={()=>{
          
          setEditState( EditState.Search_Region_Edit);

        }}>編輯搜尋範圍</Button>

        </>
      }

      EDIT_UI=<>
        
        <Input maxLength={100} value={cacheDef.id} disabled
          style={{width:"300px"}}
          onChange={(e)=>{
          }}/>
        {/* 
        <Input maxLength={100} value={cacheDef.type} disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/>

        <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/> */}

        {/* <Button onClick={()=>{
          onCacheDefChange(cacheDef,true);
        }}>照相</Button> */}



        {EditUI}


        <br/>
        

          <Button onClick={()=>{



            let default_name=Date.now();

            setModalInfo({
              timeTag:Date.now(),
              visible:true,
              type:">>",
              onOK:(minfo:typeof modalInfo)=>{
                // resolve(true)


              (async ()=>{
                  let name=minfo.DATA.prefix+minfo.DATA.name+".png";
                let pkts = await BPG_API.InspTargetExchange(cacheDef.id,{
                  type:"cache_image_save",
                  folder_path:fsPath+"/",
                  image_name:name,
                }) as any[];
                console.log(pkts);
                  // if(pkts[0].data.ACK)
                  // {
                  // }
                  // else
                  // {
                  //   message.info(`${name} 儲存失敗`);
                  // }

                  message.info(`${name} 儲存...`);
                  setModalInfo({...minfo,visible:false})
    
              })()
              },
              onCancel:(minfo:typeof modalInfo)=>{
                // reject(false)
                setModalInfo({...minfo,visible:false})
              },
              title:"儲存當前圖檔",
              DATA:{
                prefix:"",
                name:default_name
              },
              contentCB:(minfo:typeof modalInfo )=><>

                檔案名稱:
                <Input addonBefore={ 
                <Select key={default_name} defaultValue="___" onChange={value=>setModalInfo(ObjShellingAssign(minfo,["DATA","prefix"],value))   }>
                  <Option value="___">{"___"}</Option>
                  <Option value="[OK]">OK</Option>
                  <Option value="[NG]">NG</Option>
                </Select>} value={minfo.DATA.name}
                onChange={(ev)=>{
                  setModalInfo(ObjShellingAssign(minfo,["DATA","name"],ev.target.value))

                }} />
    
              </>
            })
          }}>
          儲存當前圖檔
          </Button>


        <Button  onClick={()=>{
          setEditState( EditState.Test_Saved_Files);
        }}>測試儲存圖檔</Button>
      </>


      break;

    case EditState.Feature_Edit:


      EDIT_UI=<>
        <Popconfirm
            key={"UIBack"}
            title={`確定要更新？`}
            onConfirm={()=>{}}
            onCancel={()=>{
              
              setEditState(EditState.Normal_Show)

            }}
            okButtonProps={{danger:true,onClick:()=>{
              setCacheDef({...cacheDef,featureInfo:featureInfo})
              setEditState(EditState.Normal_Show)
              
            }}}
            okText={"Yes"}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{

          }}>{"<"}</Button>
        </Popconfirm> 







       <Button key={"_"+10000} onClick={()=>{
          
          
          (async ()=>{
            
            let obj={
              type:"extract_feature",
              image_path:fsPath+"/"+SBM_FEAT_REF_IMG_NAME,
              num_features:cacheDef.num_features,
              weak_thresh:featureInfo.weak_thresh,
              strong_thresh:featureInfo.strong_thresh,
              T:[2,2],
              image_transfer_downsampling:-1,
              mask_regions:featureInfo.mask_regions
            }
            console.log(obj)
            let pkts = await BPG_API.InspTargetExchange(cacheDef.id,obj) as any[];
            console.log(pkts);

            let newFeatureInfo:any={};
            let newFeatureInfoExt:any={};
            
            
            let IM=pkts.find((p:any)=>p.type=="IM");
            if(IM!==undefined)
            {
              _this.featureImgCanvas.width = IM.image_info.width;
              _this.featureImgCanvas.height = IM.image_info.height;
    
              let ctx2nd = _this.featureImgCanvas.getContext('2d');
              ctx2nd.putImageData(IM.image_info.image, 0, 0);
              newFeatureInfoExt.IM=IM;
              
            }


            let RP=pkts.find((p:any)=>p.type=="RP");
            if(RP!==undefined)
            {
              newFeatureInfo.templatePyramid=RP.data;
            }

            
            setFeatureInfo({...featureInfo,...newFeatureInfo})
            setFeatureInfoExt({...featureInfoExt,...newFeatureInfoExt})



          })()

        }}>Templ</Button>  

        特徵數:
        <InputNumber  min={10} value={cacheDef.num_features}
        onChange={(num)=>{
          setCacheDef({...cacheDef,num_features:num})
        }} />


        圖像邊緣強度:
        <InputNumber value={featureInfo.strong_thresh}
        onChange={(num)=>{
          setFeatureInfo({...featureInfo,strong_thresh:num})
        }} />
        特徵強度:
        <InputNumber value={featureInfo.weak_thresh}
        onChange={(num)=>{
          setFeatureInfo({...featureInfo,weak_thresh:num})
        }} />

        <br/>
        



        { 
          featureInfo.mask_regions===undefined?null:
          featureInfo.mask_regions.map((regi:any,idx:number)=>
            

              
            <Popconfirm
                key={"regi_del_"+idx+"..."+updateC}
                title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
                onConfirm={()=>{}}
                onCancel={()=>{}}
                okButtonProps={{danger:true,onClick:()=>{
                  if(delConfirmCounter!=0)
                  {
                    setDelConfirmCounter(delConfirmCounter-1);
                  }
                  else
                  { 
                    let new_mask_regions=[...featureInfo.mask_regions];

                    new_mask_regions.splice(idx, 1);
                    
                    setFeatureInfo({...featureInfo,mask_regions:new_mask_regions})
                    
                  }
                }}}
                okText={"Yes:"+delConfirmCounter}
                cancelText="No"
              >
              <Button danger type="primary" onClick={()=>{
                setDelConfirmCounter(3);
              }}>{idx}</Button>
            </Popconfirm> 



          )
        }

        <Button key={"AddNewFeat"} onClick={()=>{


      
          if(_this.canvasComp==undefined)return;
          _this.sel_region=undefined;
          _this.canvasComp.UserRegionSelect((info:any,state:number)=>{
            if(state==2)
            {
              console.log(info);
              
              let x,y,w,h;
              
              let roi_region=PtsToXYWH(info.pt1,info.pt2);
              console.log(roi_region)
              let regInfo = {...roi_region,isBlackRegion:false};
              
              let mask_regions=featureInfo.mask_regions===undefined?[]:[...featureInfo.mask_regions];

              mask_regions.push(regInfo);
              setFeatureInfo({...featureInfo,mask_regions})
              
              // onDefChange(newRule)
              if(_this.canvasComp==undefined)return;
              _this.canvasComp.UserRegionSelect(undefined)
            }
          })
        }}>+特徵範圍</Button>  



        <br/>

        { 
          featureInfo.refine_match_regions===undefined?null:
          featureInfo.refine_match_regions.map((regi:any,idx:number)=>
            

              
            <Popconfirm
                key={"regi_del_"+idx+"..."+updateC}
                title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
                onConfirm={()=>{}}
                onCancel={()=>{}}
                okButtonProps={{danger:true,onClick:()=>{
                  if(delConfirmCounter!=0)
                  {
                    setDelConfirmCounter(delConfirmCounter-1);
                  }
                  else
                  { 
                    let new_refine_match_regions=[...featureInfo.refine_match_regions];

                    new_refine_match_regions.splice(idx, 1);
                    
                    setFeatureInfo({...featureInfo,refine_match_regions:new_refine_match_regions})
                    
                  }
                }}}
                okText={"Yes:"+delConfirmCounter}
                cancelText="No"
              >
              <Button danger type="primary" onClick={()=>{
                setDelConfirmCounter(3);
              }}>{idx}</Button>
            </Popconfirm> 



          )
        }

        <Button key={"AddRefineFeat"} onClick={()=>{


      
          if(_this.canvasComp==undefined)return;
          _this.sel_region=undefined;
          _this.canvasComp.UserRegionSelect((info:any,state:number)=>{
            if(state==2)
            {
              console.log(info);
              
              let x,y,w,h;
              
              let roi_region=PtsToXYWH(info.pt1,info.pt2);
              console.log(roi_region)
              let regInfo = {...roi_region,isBlackRegion:false};
              
              let refine_match_regions=featureInfo.refine_match_regions===undefined?[]:[...featureInfo.refine_match_regions];

              refine_match_regions.push(regInfo);
              setFeatureInfo({...featureInfo,refine_match_regions})
              
              // onDefChange(newRule)
              if(_this.canvasComp==undefined)return;
              _this.canvasComp.UserRegionSelect(undefined)
            }
          })
        }}>+校位範圍</Button>  


      </>

      break;
  



    case EditState.Search_Region_Edit:


      EDIT_UI=<>
        <Button danger type="primary" onClick={()=>{

          setEditState(EditState.Normal_Show)
        }}>{"<"}</Button>

        <Button onClick={()=>{

          onCacheDefChange(cacheDef,false);
          BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
        }}>ReCheck</Button>

        scaleD:
        <InputNumber value={cacheDef.matching_downScale}
        onChange={(num)=>{

          setCacheDef({...cacheDef,matching_downScale:num})
        }} />


        Sim:
        <InputNumber value={cacheDef.similarity_thres}
        onChange={(num)=>{

          setCacheDef({...cacheDef,similarity_thres:num})
        }} />

        
        MagThres:
        <InputNumber value={cacheDef.magnitude_thres}
        onChange={(num)=>{
          setCacheDef({...cacheDef,magnitude_thres:num})
        }} />


        
        {" "}
        校位下限(0~1):
        <InputNumber min={0} max={1} value={cacheDef.refine_score_thres}
          onChange={(num)=>{
            setCacheDef({...cacheDef,refine_score_thres:num})
        }} />

        <Switch checkedChildren="強制" unCheckedChildren="盡力" checked={cacheDef.must_refine_result==true} onChange={(check)=>{
          setCacheDef({...cacheDef,must_refine_result:check})
        }}/>

        <br/>

        {        
          cacheDef.search_regions===undefined?null:
          cacheDef.search_regions.map((regi:any,idx:number)=>
            

              
            <Popconfirm
                key={"regi_del_"+idx+"..."+updateC}
                title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
                onConfirm={()=>{}}
                onCancel={()=>{}}
                okButtonProps={{danger:true,onClick:()=>{
                  if(delConfirmCounter!=0)
                  {
                    setDelConfirmCounter(delConfirmCounter-1);
                  }
                  else
                  { 
                    let new_search_regions=[...cacheDef.search_regions];

                    new_search_regions.splice(idx, 1);
                    
                    setCacheDef({...cacheDef,search_regions:new_search_regions})
                    setUpdateC(updateC+1);
                  }
                }}}
                okText={"Yes:"+delConfirmCounter}
                cancelText="No"
              >
              <Button danger type="primary" onClick={()=>{
                setDelConfirmCounter(3);
              }}>{idx}</Button>
            </Popconfirm> 



          )
        }

        <Button key={"AddNewRegion"} onClick={()=>{


      
          if(_this.canvasComp==undefined)return;
          _this.sel_region=undefined;
          _this.canvasComp.UserRegionSelect((info:any,state:number)=>{
            if(state==2)
            {
              console.log(info);
              
              let x,y,w,h;
              
              let roi_region=PtsToXYWH(info.pt1,info.pt2);
              console.log(roi_region)
              let regInfo = {...roi_region,isBlackRegion:false};
              
              let search_regions=cacheDef.search_regions===undefined?[]:[...cacheDef.search_regions];

              search_regions.push(regInfo);
              setCacheDef({...cacheDef,search_regions})
              
              // onDefChange(newRule)
              if(_this.canvasComp==undefined)return;
              _this.canvasComp.UserRegionSelect(undefined)
            }
          })
        }}>+搜尋範圍</Button>  


      </>
      break;
  
  
  
    
    case EditState.Test_Saved_Files:{

      let folderPath=cacheDef.testInputFolder||fsPath;
      let result_InspTar_stream_id=51001;//HACK hard coded
      EDIT_UI=<>
        <Button danger type="primary" onClick={()=>{

          setEditState(EditState.Normal_Show)
        }}>{"<"}</Button>
        <TestInputSelectUI folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
      </>
    }break;
  
  
  
  
  }
  

  
  return <div style={{...style,width:width+"%",height:height+"%"}}  className={"overlayCon"}>

    <div className={"overlay"} style={{width:"100%"}}>

      {EDIT_UI}

    </div>

    <Modal
        title={modalInfo.title}
        visible={modalInfo.visible}
        onOk={()=>modalInfo.onOK(modalInfo)}
        // confirmLoading={confirmLoading}
        onCancel={()=>modalInfo.onCancel(modalInfo)}
      >
        {modalInfo.visible?modalInfo.contentCB(modalInfo):null}
    </Modal>
    
    <HookCanvasComponent style={{}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);

      let ctx=g.ctx;

      let camMag=canvas_obj.camera.GetCameraScale();
      if(ctrl_or_draw==true)//ctrl
      {
        if(canvas_obj.regionSelect!==undefined)
        {
          if(canvas_obj.regionSelect.pt1===undefined || canvas_obj.regionSelect.pt2===undefined)
          {
            return;
          }
      
          let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
          
          
          // console.log(canvas_obj.regionSelect);
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
          _this.sel_region={
            x,y,w,h
          }


        }

        const imageData = ctx.getImageData(g.mouseStatus.x, g.mouseStatus.y, 1,1);
        // 
        _this.fetchedPixInfo=imageData;
      }
      if(editState==EditState.Normal_Show || editState==EditState.Search_Region_Edit||editState==EditState.Test_Saved_Files)
      {      
        if(ctrl_or_draw==true)//ctrl
        {
        }
        else//draw
        {
    
          if(Local_IMCM!==undefined)
          {
            g.ctx.save();
            let scale=Local_IMCM.image_info.scale;
            g.ctx.scale(scale,scale);
            g.ctx.translate(-0.5, -0.5);
            g.ctx.drawImage(_this.imgCanvas, 0, 0);
            g.ctx.restore();
          }




          if(defReport!==undefined)
          {
            // console.log(defReport)
            defReport.report.forEach((match:any,idx:number)=>{
              
              if(match.confidence<=0)return;
              ctx.lineWidth=4/camMag;
              ctx.strokeStyle = `HSLA(0, 100%, 50%,1)`;
              canvas_obj.rUtil.drawCross(ctx, {x:match.center.x,y:match.center.y}, 12/camMag);
              
              let angle = match.angle;

              ctx.font = "40px Arial";
              ctx.fillStyle = "rgba(150,100, 100,0.8)";
              ctx.fillText("idx:"+idx,match.center.x,match.center.y-40)
              ctx.font = "20px Arial";
              ctx.fillText("ang:"+(angle*180/3.14159).toFixed(2),match.center.x,match.center.y-20)
              ctx.fillText("sim:"+match.confidence.toFixed(3),match.center.x,match.center.y-0)


              

              ctx.lineWidth=4/camMag;
              let vec=PtRotate2d({x:100,y:0},angle,1);
              canvas_obj.rUtil.drawLine(ctx,{x1:match.center.x,y1:match.center.y,x2:match.center.x+vec.x,y2:match.center.y+vec.y})

            })

            {
              ctx.save();
              ctx.resetTransform();
              ctx.font = "20px Arial";
              ctx.fillStyle = "rgba(150,100, 100,0.5)";
              ctx.fillText("ProcessTime:"+(defReport.process_time_us/1000).toFixed(2)+" ms",20,180)
              ctx.restore();
            }
          }
          // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
          try{

            if(cacheDef.search_regions!==undefined)
            {
              cacheDef.search_regions.forEach((regi:any,idx:number)=>{
                ctx.strokeStyle = "rgba(150,50, 50,0.8)";
                if(defReport && defReport.report[idx]!==undefined)
                {
                  if(defReport.report[idx].confidence>=0)
                    ctx.strokeStyle = "rgba(50,150, 50,0.8)";

                }
                
             
                drawRegion(g,canvas_obj,{x:regi.x,y:regi.y,w:regi.w,h:regi.h},canvas_obj.rUtil.getIndicationLineSize(),false  ); 
                ctx.font = "20px Arial";
                ctx.fillStyle = "rgba(50,150, 50,0.8)";
                ctx.fillText("idx:"+idx,regi.x,regi.y)
  
              })
            }
          }
          catch(e)
          {

          }
          

        }

      }
      
      if(editState==EditState.Feature_Edit)
      {
        if(ctrl_or_draw==true)//ctrl
        {
        }
        else//draw
        {

          let camMag=canvas_obj.camera.GetCameraScale();
          if(featureInfoExt.IM!==undefined)
          {
            g.ctx.save();
            let scale=featureInfoExt.IM.image_info.scale;
            g.ctx.scale(scale,scale);
            g.ctx.translate(-0.5, -0.5);
            g.ctx.drawImage(_this.featureImgCanvas, 0, 0);
            g.ctx.restore();
          }

          
          if(featureInfo.templatePyramid!==undefined)
          {
            if(canvas_obj.regionSelect===undefined)//when in region select, hide the template info
            {

            let mult=1;
            for(let i=0;i<featureInfo.templatePyramid.length;i++,mult*=2)
            {
              let template = featureInfo.templatePyramid[i]
              template.features.forEach((temp_pt:any)=>{
                // ctx.strokeStyle = "rgba(255, 0, 0,1)";
                ctx.lineWidth=4/mult;
                ctx.strokeStyle = `HSLA(${300*i/featureInfo.templatePyramid.length}, 100%, 50%,1)`;
                canvas_obj.rUtil.drawCross(ctx, {x:(temp_pt.x+template.tl_x)*mult,y:(temp_pt.y+template.tl_y)*mult}, 12/mult);
              })
            }
  
          }
            if(featureInfo.mask_regions!==undefined)
            {
              featureInfo.mask_regions.forEach((regi:any,idx:number)=>{
                
                ctx.strokeStyle = 
                ctx.fillStyle = "rgba(150,100, 100,0.8)";
                
            
                drawRegion(g,canvas_obj,{x:regi.x,y:regi.y,w:regi.w,h:regi.h},canvas_obj.rUtil.getIndicationLineSize()); 
                let fontSize_eq=40/camMag;
                if(fontSize_eq>40)fontSize_eq=40;
                ctx.font = (fontSize_eq)+"px Arial";
                ctx.fillText("idx:"+idx,regi.x,regi.y)

              })
            }


          }

          if(featureInfo.refine_match_regions!==undefined)
          {            
            if(featureInfo.refine_match_regions!==undefined)
            {
              featureInfo.refine_match_regions.forEach((regi:any,idx:number)=>{
                
                ctx.fillStyle = 
                ctx.strokeStyle = "rgba(100,100, 200,0.8)";
                
            
                drawRegion(g,canvas_obj,{x:regi.x,y:regi.y,w:regi.w,h:regi.h},canvas_obj.rUtil.getIndicationLineSize()); 
                let fontSize_eq=40/camMag;
                if(fontSize_eq>40)fontSize_eq=40;
                ctx.font = (fontSize_eq)+"px Arial";
                ctx.fillText("idx:"+idx,regi.x,regi.y)

              })
            }
          }


        }
      }

      
      if(ctrl_or_draw==false)
      {
        if(canvas_obj.regionSelect!==undefined && _this.sel_region!==undefined)
        {
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          drawRegion(g,canvas_obj,_this.sel_region,canvas_obj.rUtil.getIndicationLineSize());
      
        }

        if(_this.fetchedPixInfo!==undefined)
        {
          ctx.save();
          ctx.resetTransform();
          // console.log(_this.fetchedPixInfo)
          let pixInfo=_this.fetchedPixInfo.data;
          ctx.font = "1.5em Arial";
          ctx.fillStyle = "rgba(250,100, 50,1)";

          ctx.fillText(rgb2hsv(pixInfo[0],pixInfo[1],pixInfo[2]).map(num=>num.toFixed(1)).toString(),g.mouseStatus.x,g.mouseStatus.y)
          ctx.restore();
        }
      }



      
      if(renderHook)
      {
        // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
      }
    }
    }/>

  </div>;

}


function SingleTargetVIEWUI_Orientation_ColorRegionOval({display,stream_id,fsPath,width,height,style=undefined,renderHook,def,report,onDefChange}:CompParam_InspTarUI){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined,
    drawHooks:[],
    ctrlHooks:[]


  });
  const [cacheDef,setCacheDef]=useState<any>(def);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);


  const [defReport,setDefReport]=useState<any>(undefined);
  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);
  let _this=_.current;
  let c_report:any = undefined;
  if(_this.cache_report!==report)
  {
    if(report!==undefined)
    {
      _this.cache_report=report;
    }
  }
  c_report=_this.cache_report;


  useEffect(() => {
    console.log("fsPath:"+fsPath)
    _this.cache_report=undefined;
    setCacheDef(def);
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, [def]); 
  // console.log(IMCM_group,report);
  // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  
  enum editState {
    Normal_Show = 0,
    Region_Edit = 1,
  }
  
  const [stateInfo,setStateInfo]=useState<{st:editState,info:any}[]>([{
    st:editState.Normal_Show,
    info:undefined
  }]);

  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);

  
  const [queryCameraList,setQueryCameraList]=useState<any[]|undefined>(undefined);
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);

  
  let stateInfo_tail=stateInfo[stateInfo.length-1];



  function onCacheDefChange(updatedDef: any,ddd:boolean)
  {
    console.log(updatedDef);
    setCacheDef(updatedDef);

    

    (async ()=>{
      await BPG_API.InspTargetUpdate(updatedDef)
      
      // await BPG_API.CameraSWTrigger("Hikrobot-00F92938639","TTT",4433)
      
      await BPG_API.CameraSWTrigger("BMP_carousel_0","TTT",4433)

    })()
    
  }

  
  useEffect(() => {//////////////////////

    (async ()=>{

      let ret = await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

      await BPG_API.send_cbs_attach(
        cacheDef.stream_id,"ColorRegionDetection",{
          
          resolve:(pkts)=>{
            // console.log(pkts);
            let IM=pkts.find((p:any)=>p.type=="IM");
            if(IM===undefined)return;
            let CM=pkts.find((p:any)=>p.type=="CM");
            if(CM===undefined)return;
            let RP=pkts.find((p:any)=>p.type=="RP");
            if(RP===undefined)return;
            console.log("++++++++\n",IM,CM,RP);


            setDefReport(RP.data)
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
        }

      )

    })()
    return (() => {
      (async ()=>{
        await BPG_API.send_cbs_detach(
          stream_id,"ColorRegionDetection");
            
        // await BPG_API.InspTargetSetStreamChannelID(
        //   cacheDef.id,0,
        //   {
        //     resolve:(pkts)=>{
        //     },
        //     reject:(pkts)=>{
    
        //     }
        //   }
        // )
      })()
      
    })}, []); 
  // function pushInSendGCodeQ()
  // {
  //   if(_this.isSendWaiting==true || _this.gcodeSeq.length==0)
  //   {
  //     return;
  //   }
  //   const gcode = _this.gcodeSeq.shift();
  //   if(gcode==undefined || gcode==null)return;
  //   _this.isSendWaiting=true;
  //   ACT_WS_GET_OBJ((api)=>{
  //     api.send({"type":"GCODE","code":gcode},
  //     (ret)=>{
  //       console.log(ret);
  //       _this.isSendWaiting=false;
  //       pushInSendGCodeQ(_this.gcodeSeq);

  //     },(e)=>console.log(e));
  //   })
  // }


  if(display==false)return null;


  let EDIT_UI= null;
  
  switch(stateInfo_tail.st)
  {
    
    case editState.Normal_Show:


      let EditUI=null;
      if(true)//allow edit
      {
        EditUI=<>
        <Button key={"_"+10000} onClick={()=>{
          
          let newDef={...cacheDef};
          newDef.regionInfo.push({region:[0,0,0,0],colorThres:10});
          onCacheDefChange(newDef,false)

          
          setStateInfo([...stateInfo,{
            st:editState.Region_Edit,
            info:{
              idx:newDef.regionInfo.length-1
            }
          }])

        }}>+</Button>

        

        {cacheDef.regionInfo.map((region:any,idx:number)=>{
          return <Button key={"_"+idx} onClick={()=>{
            if(_this.canvasComp===undefined)return;


            setStateInfo([...stateInfo,{
              st:editState.Region_Edit,
              info:{
                idx:idx
              }
            }])




          }}>{"idx:"+idx}</Button>
        })}
        </>
      }

      EDIT_UI=<>
        
        <Input maxLength={100} value={cacheDef.id} 
          style={{width:"100px"}}
          onChange={(e)=>{
            console.log(e.target.value);

            let newDef={...cacheDef};
            newDef.id=e.target.value;
            onCacheDefChange(newDef,false)


          }}/>

        <Input maxLength={100} value={cacheDef.type} disabled
          style={{width:"100px"}}
          onChange={(e)=>{
            
          }}/>

        <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/>


        <Dropdown
          overlay={<>
            <Menu>
              {
                queryCameraList===undefined?
                  <Menu.Item disabled danger>
                  <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                    Press to update
                  </a>
                  </Menu.Item>
                  :
                  queryCameraList.map(cam=><Menu.Item key={cam.id} 
                  onClick={()=>{
                    let newDef={...cacheDef};
                    newDef.camera_id=cam.id;
                    // HACK_do_Camera_Check=true;
                    onCacheDefChange(newDef,true)
                  }}>
                    {cam.id}
                  </Menu.Item>)

              }
            </Menu>
          </>}
        >
          <Button onClick={()=>{
            // queryCameraList
            setQueryCameraList(undefined);
            BPG_API.queryDiscoverList()
              .then((e: any)=>{
                console.log(e);
                setQueryCameraList(e[0].data)
              })
            // let api=await getAPI(CORE_ID) as BPG_WS;
            // let cameraListInfos=await api.cameraDiscovery() as any[];
            // let CM=cameraListInfos.find(info=>info.type=="CM")
            // if(CM===undefined)throw "CM not found"
            // console.log(CM.data);
            // return CM.data as {name:string,id:string,driver_name:string}[];
            
          }}>{cacheDef.camera_id}</Button>
        </Dropdown>



        <Input maxLength={100} value={cacheDef.trigger_tag} 
          style={{width:"100px"}}
          onChange={(e)=>{
            let newDef={...cacheDef};
            newDef.trigger_tag=e.target.value;
            onCacheDefChange(newDef,false)
        }}/>

        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onCacheDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
        <br/>
        <Button onClick={()=>{
          onCacheDefChange(cacheDef,true);
        }}>SHOT</Button>


        <Button onClick={()=>{
          onDefChange(cacheDef,true)
        }}>SAVE</Button>

        {EditUI}


      </>


      break;

    case editState.Region_Edit:


      if(cacheDef.regionInfo.length<=stateInfo_tail.info.idx)
      {
        break;
      }
      
      let regionInfo=cacheDef.regionInfo[stateInfo_tail.info.idx];

      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>
        <Orientation_ColorRegionOval_SingleRegion 
          srule={regionInfo} 
          onDefChange={(newDef_sregion)=>{
            // console.log(newDef);

            
            let newDef={...cacheDef};
            if(newDef_sregion!==undefined)
            {
              newDef.regionInfo[stateInfo_tail.info.idx]=newDef_sregion;
            }
            else
            {
              
              newDef.regionInfo.splice(stateInfo_tail.info.idx, 1);
              
              let new_stateInfo=[...stateInfo]
              new_stateInfo.pop();

              setStateInfo(new_stateInfo)

            }

            onCacheDefChange(newDef,true)
            // _this.sel_region=undefined

          }}
          canvas_obj={_this.canvasComp}/>
      </>

      break;
  }
  

  
  return <div style={{...style,width:width+"%",height:height+"%"}}  className={"overlayCon"}>

    <div className={"overlay"} >

      {EDIT_UI}

    </div>

    
    <HookCanvasComponent style={{}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);
      if(ctrl_or_draw==true)//ctrl
      {
        // if(canvas_obj.regionSelect===undefined)
        // canvas_obj.UserRegionSelect((onSelect,draggingState)=>{
        //   if(draggingState==1)
        //   {

        //   }
        //   else if(draggingState==2)
        //   {
        //     console.log(onSelect);
        //     canvas_obj.UserRegionSelect(undefined)
        //   }
        // });
        
        // ctrlHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
        if(canvas_obj.regionSelect!==undefined)
        {
          if(canvas_obj.regionSelect.pt1===undefined || canvas_obj.regionSelect.pt2===undefined)
          {
            return;
          }
      
          let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
           
          
          // console.log(canvas_obj.regionSelect);
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
          _this.sel_region={
            x,y,w,h
          }
        }
      }
      else//draw
      {
        if(Local_IMCM!==undefined)
        {
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       

        let ctx = g.ctx;
        
        {
          cacheDef.regionInfo.forEach((region:any,idx:number)=>{

            let region_ROI=
            {
              x:region.region[0],
              y:region.region[1],
              w:region.region[2],
              h:region.region[3]
            }
            
            ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
            drawRegion(g,canvas_obj,region_ROI,canvas_obj.rUtil.getIndicationLineSize());
                    
            ctx.font = "40px Arial";
            ctx.fillStyle = "rgba(0, 179, 0,0.5)";
            ctx.fillText("idx:"+idx,region_ROI.x,region_ROI.y)

            
            let region_components=GetObjElement(c_report,["regionInfo",idx,"components"]);
            // console.log(report,region_components);
            if(region_components!==undefined)
            {
              region_components.forEach((regComp:any)=>{

                canvas_obj.rUtil.drawCross(ctx, {x:regComp.x,y:regComp.y}, 5);

                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(regComp.area,regComp.x,regComp.y)
                
                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(`${regComp.x},${regComp.y}`,regComp.x,regComp.y+5)
              })

            }



          })
        }


        if(canvas_obj.regionSelect!==undefined && _this.sel_region!==undefined)
        {
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          drawRegion(g,canvas_obj,_this.sel_region,canvas_obj.rUtil.getIndicationLineSize());
      
        }

        if(defReport)
        {
          
          ctx.strokeStyle = "rgba(255,0,100,0.5)";
          defReport.report.forEach((reg:any)=>{
            if(reg.center===undefined || reg.angle===undefined)return;
            canvas_obj.rUtil.drawCross(ctx, {x:reg.center.x,y:reg.center.y}, 5);
            let angle = reg.angle;
            if(angle>Math.PI/2)angle-=Math.PI;
            let vec=PtRotate2d({x:30,y:0},angle,1);
            canvas_obj.rUtil.drawLine(ctx,{x1:reg.center.x,y1:reg.center.y,x2:reg.center.x+vec.x,y2:reg.center.y+vec.y})
            // console.log(reg,canvas_obj)
          })
        }
      }

      
      if(renderHook)
      {
        // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
      }
    }
    }/>

  </div>;

}



function SurfaceCheckSimple_EDIT_UI({def,onDefChange,canvas_obj}:
  {
    def:any,
    onDefChange:(...param:any)=>void,
    canvas_obj:DrawHook_CanvasComponent
  })
{
  
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  let def_Filled={
    
    W:500,
    H:500,

    hsv:{
      rangeh:{
        h:180,s:255,v:255
      },
      rangel:{
        h:0,s:0,v:0
      },
    },
    colorThres:10,
    resultOverlayAlpha:0,
    img_order_reverse:false,

    ...def
  }
  const _this = useRef<any>({}).current;
  return <>

    <br/>XOffset:
    <InputNumber value={def_Filled.X_offset}
    onChange={(num)=>{
      let newDef={...def_Filled,X_offset:num}
      onDefChange(newDef,true);
    }} />
    {"  "}YOffset:
    <InputNumber value={def_Filled.Y_offset}
    onChange={(num)=>{
      let newDef={...def_Filled,Y_offset:num}
      onDefChange(newDef,true);
    }} />
  
    <br/>W:
    <InputNumber min={10} max={2000} value={def_Filled.W}
    onChange={(num)=>{
      let newDef={...def_Filled,W:num}
      onDefChange(newDef,true);
    }} />
    {"  "}H:
    <InputNumber min={10} max={2000} value={def_Filled.H}
    onChange={(num)=>{
      let newDef={...def_Filled,H:num}
      onDefChange(newDef,true);
    }} />
    {"  "}角度調整:
    <InputNumber value={def_Filled.angle_offset}
    onChange={(num)=>{
      let newDef={...def_Filled,angle_offset:num}
      onDefChange(newDef,true);
    }} />
    <br/>
    面積閾值:
    <InputNumber value={def_Filled.area_thres}
    onChange={(num)=>{
      let newDef={...def_Filled,area_thres:num}
      onDefChange(newDef,true);
    }} />
      
    單線長閾值:
    <InputNumber value={def_Filled.line_length_thres}
    onChange={(num)=>{
      let newDef={...def_Filled,line_length_thres:num}
      onDefChange(newDef,true);
    }} />
    單面積閾值:
    <InputNumber value={def_Filled.point_area_thres}
    onChange={(num)=>{
      let newDef={...def_Filled,point_area_thres:num}
      onDefChange(newDef,true);
    }} />
      



        <>
    
    
    <br/>結果顯示
    <Slider defaultValue={def_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(def_Filled,["resultOverlayAlpha"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>

    
    圖序反轉:
    <Switch checkedChildren="左至右" unCheckedChildren="右至左" checked={def_Filled.img_order_reverse==true} onChange={(check)=>{
      onDefChange(ObjShellingAssign(def_Filled,["img_order_reverse"],check));
    }}/>

    <br/>HSV
    <Slider defaultValue={def_Filled.hsv.rangeh.h} max={180} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(def_Filled,["hsv","rangeh","h"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={def_Filled.hsv.rangel.h} max={180} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(def_Filled,["hsv","rangel","h"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    <Slider defaultValue={def_Filled.hsv.rangeh.s} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(def_Filled,["hsv","rangeh","s"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={def_Filled.hsv.rangel.s} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(def_Filled,["hsv","rangel","s"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>




    <Slider defaultValue={def_Filled.hsv.rangeh.v} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(def_Filled,["hsv","rangeh","v"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={def_Filled.hsv.rangel.v} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{

        onDefChange(ObjShellingAssign(def_Filled,["hsv","rangel","v"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>





    </>

    <br/>細節量
    <Slider defaultValue={def_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(def_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}

  
function TagsEdit_DropDown({tags,onTagsChange,children}:{tags:string[],onTagsChange:(tags:string[]) => void,children: React.ReactChild })
{
  const [visible, _setVisible] = useState(false);
  const [newTagTxt, setNewTagTxt] = useState("");


  const [tagDelInfo, setTagDelInfo] = useState({tarTag:"",countdown:0});


  function setVisible(enable:boolean)
  {
    setTagDelInfo({...tagDelInfo,tarTag:""});
    _setVisible(enable);
  }

  let isNewTagTxtDuplicated = tags.find(tag=>tag==newTagTxt)!=undefined;
  return <Dropdown   onVisibleChange={setVisible} visible={visible}
      overlay={<Menu>
        {
          [...tags.map((tag:string,index:number)=>(
          <Menu.Item key={tag+"_"+index} 
            onClick={()=>{
              if(tagDelInfo.tarTag!=tag)
              {
                setTagDelInfo({
                  tarTag:tag,
                  countdown:3
                });
                return;
              }

              if(tagDelInfo.countdown>0)
              {
                setTagDelInfo({...tagDelInfo,countdown:tagDelInfo.countdown-1});
                return;
              }

              let newList=[...tags]
              newList.splice(index, 1);
              onTagsChange(newList);
              }}>
              {tag+((tagDelInfo.tarTag!=tag)?"":("   cd:"+tagDelInfo.countdown))}
          </Menu.Item>)),

          <Menu.Item key={"ADD"} 
          onClick={(e)=>{
          }}>

          <Input maxLength={100} value={newTagTxt} status={isNewTagTxtDuplicated?"error":undefined}
            onChange={(e)=>{
              setNewTagTxt(e.target.value);
            }}
            onPressEnter={(e)=>{
              if(isNewTagTxtDuplicated==false)
              {
                onTagsChange([...tags,newTagTxt]);
                setNewTagTxt("");
              }
          }}/>

          </Menu.Item>
        ]
        }
      </Menu>}
    >
      {children}
    </Dropdown>

}

const CAT_ID_NAME={
  "0":"NA",
  "1":"OK", 
  "-1":"NG",
  "-40000":"空",

  "-700":"點過大",
  "-701":"邊過長",
}
const _MM_P_STP_=4;
const _OBJ_SEP_DIST_=4;
function SingleTargetVIEWUI_SurfaceCheckSimple({display,stream_id,fsPath,width,height,EditPermitFlag,style=undefined,renderHook,def,report,onDefChange}:CompParam_InspTarUI){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined,
    drawHooks:[],
    ctrlHooks:[],

    stepQueryTime:1000,
    periodicTask_HDL:undefined,
    periodicTask:()=>{}

  });

  const [perifConnState,setPerifConnState]=useState<boolean>(false);


  const [cacheDef,setCacheDef]=useState<any>(def);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);


  const [defReport,setDefReport]=useState<any>(undefined);
  const [NGInfoList,setNGInfoList]=useState<{location_mm:number,category:number}[]>([]);
  const [latestRepStepCount,setLatestRepStepCount]=useState(0);
  const [reelStep,setReelStep]=useState<number>(0);

  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);
  let _this=_.current;
  let c_report:any = undefined;
  if(_this.cache_report!==report)
  {
    if(report!==undefined)
    {
      _this.cache_report=report;
    }
  }
  c_report=_this.cache_report;


  useEffect(() => {
    console.log("fsPath:"+fsPath)
    _this.cache_report=undefined;
    setCacheDef(def);
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, [def]); 
  // console.log(IMCM_group,report);
  // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  
  enum EditState {
    Normal_Show = 0,
    Region_Edit = 1,
  }
  
  const [editState,setEditState]=useState<EditState>(EditState.Normal_Show);

  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);

  
  const [queryCameraList,setQueryCameraList]=useState<any[]|undefined>(undefined);
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);


  function onCacheDefChange(updatedDef: any,ddd:boolean)
  {
    console.log(updatedDef);
    setCacheDef(updatedDef);

    

    (async ()=>{
      console.log(">>>");
      await BPG_API.InspTargetUpdate(updatedDef)

    })()
    
    BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
  }

  function periodicCB()
  {
    // console.log("jkdshfsdhf;iohsd;iofjhsdio;fhj;isdojfhdsil;");
    _this.periodicTask_HDL=undefined;
    return;
    {(async ()=>{
      // console.log(  CNC_API.isConnected)
      if(CNC_API.isConnected!=perifConnState)
        setPerifConnState(CNC_API.isConnected);

      // if(CNC_API.isConnected)
      {

        // console.log("SEND....")
        let ret = await CNC_API.send_P({"type":"GET_CUR_STEP_COUNTER"}) as any
        // console.log(ret)
        if(ret.step!=reelStep)
        {
          setReelStep(ret.step)
          _this.stepQueryTime=200;
        }
        else
        {
          _this.stepQueryTime+=50;
          if(_this.stepQueryTime>1000)
            _this.stepQueryTime=1000;
        }
      }

    })()
    .catch((e)=>{

      // console.log(e)
      if(_this.periodicTask_HDL!==undefined)
      {
        window.clearTimeout(_this.periodicTask_HDL);
      }
      _this.periodicTask_HDL=window.setTimeout(_this.periodicTask,_this.stepQueryTime);
    })

    if(_this.periodicTask_HDL!==undefined)
    {
      window.clearTimeout(_this.periodicTask_HDL);
    }
    _this.periodicTask_HDL=window.setTimeout(_this.periodicTask,_this.stepQueryTime);
    // if(_this.periodicTask_HDL!==undefined)
    // {
    //   window.clearTimeout(_this.periodicTask_HDL);
    // }
    // _this.periodicTask_HDL=window.setTimeout(_this.periodicTask,_this.stepQueryTime);
    
   }
  }
  _this.periodicTask=periodicCB;

  _this.TMP_NGInfoList=NGInfoList;
  _this.perifConnState=perifConnState;
  useEffect(() => {//////////////////////

    (async ()=>{

      let ret = await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

      await BPG_API.send_cbs_attach(
        cacheDef.stream_id,"SurfaceCheckSimple",{
          
          resolve:(pkts)=>{
            // console.log(pkts);
            let IM=pkts.find((p:any)=>p.type=="IM");
            if(IM===undefined)return;
            let CM=pkts.find((p:any)=>p.type=="CM");
            if(CM===undefined)return;
            let RP=pkts.find((p:any)=>p.type=="RP");
            if(RP===undefined)return;
            // console.log("++++++++\n",IM,CM,RP);


            // setDefReport(RP.data)
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
            let rep=RP.data;
            setDefReport(rep);
            if(rep.report.category<=0 && _this.perifConnState)
            {
              CNC_API.send_P({"type":"EM_STOP"})
              .then((ret)=>{
                // console.log(ret)
              })
              .catch(e=>{

              })



              let repAtStepTag = rep.tags.find((tag:string)=>tag.startsWith("s_Step_"));
              if(repAtStepTag!==undefined)
              {
                let repAtStep=parseInt(repAtStepTag.replace('s_Step_',''));
                // rep.report
                console.log(rep,repAtStep)

                let newNGInfo = rep.report.sub_reports.map((bad_ele:any,index:number)=>({location_mm:-repAtStep*_MM_P_STP_+index*_MM_P_STP_,category:bad_ele.category}))
                  .filter((ele:any)=>ele.category<=0)
                console.log(newNGInfo,"mm")

                setLatestRepStepCount(repAtStep);
                setNGInfoList([..._this.TMP_NGInfoList,...newNGInfo])
              }
            
            }
  
          },
          reject:(pkts)=>{
  
          }
        }

      )

    })()

    _this.periodicTask();

    return (() => {
      window.clearTimeout(_this.periodicTask_HDL);
      (async ()=>{
        await BPG_API.send_cbs_detach(
          stream_id,"SurfaceCheckSimple");
            
        // await BPG_API.InspTargetSetStreamChannelID(
        //   cacheDef.id,0,
        //   {
        //     resolve:(pkts)=>{
        //     },
        //     reject:(pkts)=>{
    
        //     }
        //   }
        // )
      })()
      
    })}, []); 
  // function pushInSendGCodeQ()
  // {
  //   if(_this.isSendWaiting==true || _this.gcodeSeq.length==0)
  //   {
  //     return;
  //   }
  //   const gcode = _this.gcodeSeq.shift();
  //   if(gcode==undefined || gcode==null)return;
  //   _this.isSendWaiting=true;
  //   ACT_WS_GET_OBJ((api)=>{
  //     api.send({"type":"GCODE","code":gcode},
  //     (ret)=>{
  //       console.log(ret);
  //       _this.isSendWaiting=false;
  //       pushInSendGCodeQ(_this.gcodeSeq);

  //     },(e)=>console.log(e));
  //   })
  // }


  if(display==false)return null;



  let EDIT_UI= null;
  
  switch(editState)
  {
    
    case EditState.Normal_Show:


      let EditUI=null;

      if( (EditPermitFlag&EDIT_PERMIT_FLAG.XXFLAGXX)!=0 )//allow edit
      {
      EDIT_UI=<>
        
        {/* <Input maxLength={100} value={cacheDef.type} disabled
          style={{width:"100px"}}
          onChange={(e)=>{
            
          }}/>

        <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/> */}




        <TagsEdit_DropDown tags={cacheDef.trigger_tags}
          onTagsChange={(newTags)=>{
            
            onCacheDefChange({...cacheDef,trigger_tags:newTags},false)
          }}>
          <a>TAGS</a>
        </TagsEdit_DropDown>




        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onCacheDefChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
        <br/>
        <Button onClick={()=>{
          onCacheDefChange(cacheDef,true);
        }}>SHOT</Button>


        <Button key={"_"+10000} onClick={()=>{
          setEditState( EditState.Region_Edit);
        }}>EDIT</Button>

        <Button onClick={()=>{
          onDefChange(cacheDef,true)
        }}>SAVE</Button>
        </>
      }


      EDIT_UI=<>
        
        <Input maxLength={300} value={cacheDef.id} disabled
          style={{width:"300px"}}
          onChange={(e)=>{
          }}/>
        {EDIT_UI}
        {"  "}
        {
          (perifConnState)?<>        
            <Button onClick={()=>{
              CNC_API.send_P({"type":"Encoder_Reset"})
            }}>歸零</Button>
    
            <Button onClick={()=>{
    
            CNC_API.send_P({"type":"TRIG_CAMERA_TAKE"})
            // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
            }}>測試觸發</Button>
            <Button onClick={()=>{
    
              CNC_API.send_P({"type":"LIGHT_1_ON"})
              // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
            }}>LON</Button>
            <Button onClick={()=>{
    
              CNC_API.send_P({"type":"LIGHT_1_OFF"})
              // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
            }}>LOFF</Button>
            </>:
            null
        }
        <br/>
        {
          (NGInfoList.length>0)?
          <Popconfirm
              placement="rightBottom"
              title={`確定要刪除全部NG？ 再按:${delConfirmCounter+1}次`}
              onConfirm={()=>{}}
              onCancel={()=>{}}
              okButtonProps={{danger:true,onClick:()=>{
                if(delConfirmCounter!=0)
                {
                  setDelConfirmCounter(delConfirmCounter-1);
                }
                else
                {
                  setNGInfoList([])
                }
              }}}
              okText={"Yes:"+delConfirmCounter}
              cancelText="No"
            >
            <Button danger type="primary" onClick={()=>{
              setDelConfirmCounter(5);
            }}>X</Button>
          </Popconfirm> :null
        }
        {
          NGInfoList.map((nginfo,index)=>
            <Button danger onClick={()=>{

              let newList=[...NGInfoList]
              newList.splice(index, 1);
              setNGInfoList(newList);
            }}>{((nginfo.location_mm+reelStep*_MM_P_STP_)/_OBJ_SEP_DIST_)+"顆 ["+CAT_ID_NAME[nginfo.category+""]+"]"}</Button>
          )
        }


      </>


      break;

    case EditState.Region_Edit:

      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          setEditState( EditState.Normal_Show);
        }}>{"<"}</Button>


        <SurfaceCheckSimple_EDIT_UI 
            def={cacheDef} 
            onDefChange={(newDef)=>{
              onCacheDefChange(newDef,true);
              
            }}
            canvas_obj={_this.canvasComp}/>
        
      </>

      break;
  }
  
  let img_order_reverse= cacheDef.img_order_reverse===true;
  // console.log("img_order_reverse:"+img_order_reverse) 
  return <div style={{...style,width:width+"%",height:height+"%"}}  className={"overlayCon"}>

    <div className={"overlay"} >

      {EDIT_UI}

    </div>

    
    <HookCanvasComponent style={{}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);
      if(ctrl_or_draw==true)//ctrl
      {
        // if(canvas_obj.regionSelect===undefined)
        // canvas_obj.UserRegionSelect((onSelect,draggingState)=>{
        //   if(draggingState==1)
        //   {

        //   }
        //   else if(draggingState==2)
        //   {
        //     console.log(onSelect);
        //     canvas_obj.UserRegionSelect(undefined)
        //   }
        // });
        
        // ctrlHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
        if(canvas_obj.regionSelect!==undefined)
        {
          if(canvas_obj.regionSelect.pt1===undefined || canvas_obj.regionSelect.pt2===undefined)
          {
            return;
          }
      
          let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
           
          
          // console.log(canvas_obj.regionSelect);
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
          _this.sel_region={
            x,y,w,h
          }
        }
      }
      else//draw
      {
        let camMag=canvas_obj.camera.GetCameraScale();
        if(Local_IMCM!==undefined)
        {
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       

        let ctx = g.ctx;
        

        if(canvas_obj.regionSelect!==undefined && _this.sel_region!==undefined)
        {
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          drawRegion(g,canvas_obj,_this.sel_region,canvas_obj.rUtil.getIndicationLineSize());
      
        }



        if(defReport!==undefined)
        {
          {
            ctx.save();
            ctx.resetTransform();
            ctx.font = "20px Arial";
            ctx.fillStyle = "rgba(150,100, 100,0.5)";
            let Y=350;
            ctx.fillText("Result:"+defReport.report.category + " DIST:"+(reelStep*_MM_P_STP_/_OBJ_SEP_DIST_)+"顆",20,Y);
            ctx.fillText("ProcessTime:"+(defReport.process_time_us/1000).toFixed(2)+" ms",20,Y+30)

            ctx.restore();
          }
          let g_cat=defReport.report.sub_reports;
          g_cat.forEach((catInfo:any,_index:number)=>{
            // console.log(catInfo,cacheDef);
            
            // console.log(catInfo);

            let index=img_order_reverse?(g_cat.length-1-_index):_index;
            let TextY=15;

            
            ctx.font = TextY+"px Arial";
            if(catInfo.category>0)
              ctx.fillStyle = "rgba(0, 255, 0,1)";
            else
              ctx.fillStyle = "rgba(255, 0, 0,1)";

            
              

            let curOffset=(reelStep*_MM_P_STP_-latestRepStepCount*_OBJ_SEP_DIST_+_index*_MM_P_STP_)/_OBJ_SEP_DIST_;
            ctx.fillText(CAT_ID_NAME[catInfo.category+""]+" "+curOffset+"顆",cacheDef.W*index,0+TextY+5)

            ctx.font = (TextY*0.7).toFixed(2)+"px Arial";
            ctx.fillText(catInfo.score,cacheDef.W*index,0+TextY+TextY+5)


            ctx.strokeStyle = "rgba(179, 0, 0,1)";
            drawRegion(g,canvas_obj,{
              x:cacheDef.W*index,
              y:0,
              w:cacheDef.W,
              h:cacheDef.H
            },
            canvas_obj.rUtil.getIndicationLineSize(),
            false);

            
            catInfo.elements.forEach((ele:any)=>{

              if(CAT_ID_NAME[ele.category+""]=="OK")
                ctx.strokeStyle = "rgba(0, 179, 0,0.6)";
              else
                ctx.strokeStyle = "rgba(179, 0, 0,0.6)";
              
              ctx.fillStyle = ctx.strokeStyle;
              canvas_obj.rUtil.drawCross(ctx, {x:ele.x,y:ele.y}, 1);
              
              
              // let fontSize_eq=10/camMag;
              // if(fontSize_eq>10)fontSize_eq=40;
              // ctx.font = (fontSize_eq)+"px Arial";
              ctx.font ="1px Arial";
              ctx.fillText(CAT_ID_NAME[ele.category+""],ele.x,ele.y);

              

            });



          })
        }
      }

      if(renderHook)
      {
        // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
      }
    }
    }/>

  </div>;
  
}



function CameraSetupEditUI({camSetupInfo,CoreAPI,onCameraSetupUpdate}:{ camSetupInfo:type_CameraInfo, CoreAPI:BPG_WS,onCameraSetupUpdate:(caminfo:type_CameraInfo)=>void}){

  const _this = useRef<{canvasComp:DrawHook_CanvasComponent|undefined,imgCanvas:HTMLCanvasElement
  }>({
    canvasComp:undefined,
    imgCanvas:document.createElement('canvas') as HTMLCanvasElement
  }).current;

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
            ctx2nd.putImageData(IMCM.image_info.image, 0, 0);

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
    {/* <pre>{
      JSON.stringify(camSetupInfo,null,2)
    }</pre> */}
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


  return  <></>;
}


function TargetViewUIShow({displayIDList,defConfig,EditPermitFlag,onDefChange,renderHook}:{displayIDList:string[],defConfig:any,EditPermitFlag:number, onDefChange:(updatedDef:any)=>void,renderHook:any})
{
  let InspTarList=defConfig.InspTars_main;
  let displayInspTarIdx:number[]=[];
  let displayInspTarIdx_hide:number[]=[];
  if(defConfig!==undefined)
  {
    displayInspTarIdx=displayIDList
      .map(itarID=>InspTarList.findIndex((itar:any)=>itar.id==itarID))
      .filter(idx=>idx>=0)
  
    displayInspTarIdx_hide
      =InspTarList.map((itar:any,idx:number)=>idx).filter((idx:number)=>{
        if(displayInspTarIdx.find(idx_to_show=>idx_to_show==idx)===undefined)
          return true;
        return false;
      });
  
  }

  useEffect(()=>{//load default
    console.log(">>TargetViewUIShow>>>>>>>>>>>>>>");
  },[])
  console.log(InspTarList);
  console.log(displayIDList,displayInspTarIdx,displayInspTarIdx_hide);





  // function SingleTargetVIEWUI_ColorRegionDetection({readonly,width,height,style=undefined,renderHook,IMCM_group,def,report,onDefChange}:CompParam_InspTarUI)
  return <>{
    // displayInspTarIdx.map((idx:number)=>InspTarList[idx]).map((inspTar:any)=>inspTar.id+":"+inspTar.type+",")
    [...displayInspTarIdx,-1,...displayInspTarIdx_hide].map((idx:number)=>idx>=0?InspTarList[idx]:undefined).map((inspTar:any,idx:number)=>{
    
      if(inspTar===undefined)
      {
        return  null;//<><br/>---hide----<br/></>
      }
      console.log(defConfig.path,inspTar.id);
      return  <InspTargetUI_MUX 
        display={idx<displayInspTarIdx.length} 
        width={100/displayInspTarIdx.length} 
        height={100} 
        stream_id={50120}
        style={{float:"left"}} 
        EditPermitFlag={EditPermitFlag}
        key={inspTar.id} 
        def={inspTar} 
        report={undefined} 
        fsPath={defConfig.path+"/it_"+inspTar.id}
        renderHook={renderHook} 
        onDefChange={(new_rule,doInspUpdate=true)=>{
          console.log(new_rule);

          let idx = InspTarList.findIndex((itar:any)=>itar.id==new_rule.id);
          if(idx<0)return;

          let newDefConfig={...defConfig,InspTars_main:[...InspTarList]};
          newDefConfig.InspTars_main[idx]=new_rule;
          
          onDefChange(newDefConfig)
        }}
      />})
  } </>;
}



let DAT_ANY_UNDEF:any=undefined;



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


  const [defReport,setDefReport]=useState<any>(undefined);
  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);



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
          // console.log(RP);
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
      if(setting.type==="SELECTS")
      {//preset
        _this.listCMD_Vairable.USER_INPUT=setting.data.map((info:any)=>info.default);
      }
      await new Promise((resolve,reject)=>{
        setModalInfo({
          timeTag:Date.now(),
          visible:true,
          type:setting.type,
          onOK:()=>{
            resolve(true)
            setModalInfo({...modalInfo,visible:false})
          },
          onCancel:()=>{
            reject(false)
            setModalInfo({...modalInfo,visible:false})
          },
          title:setting.title,
          DATA:_setting,
          content:undefined
        })
      })
      
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
          menuCol("------------","divLine"),
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
                   
                    await BPG_API.CameraSetup({...ncam,frame_rate:5},trigMode);
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


            <Button disabled={crunAbortCtrl!==undefined} onClick={()=>{
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
          
          setEditPermitFlag(editPermitFlag^EDIT_PERMIT_FLAG.XXFLAGXX)
        }}>EDIT_LEVEL {editPermitFlag}</Menu.Item>
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
      <TargetViewUIShow displayIDList={displayInspTarId} defConfig={defConfig} EditPermitFlag={editPermitFlag}  onDefChange={(newdef:any)=>{
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
    
    core_api.connect({
      url:"ws://127.0.0.1:4090"
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
