import React from 'react';
import { useState, useEffect,useRef,useMemo,useContext } from 'react';
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


import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg,PtRotate2d} from './UTIL/MathTools';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message } from 'antd';


import { type_CameraInfo,type_IMCM} from './AppTypes';
import './basic.css';


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



type IMCM_group={[trigID:string]:IMCM_type}


type CompParam_InspTarUI =   {
  display:boolean,
  style?:any,
  stream_id:number,
  fsPath:string,
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

    <br/>colorThres
    <Slider defaultValue={srule_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}





function SingleTargetVIEWUI_ColorRegionDetection({display,stream_id,fsPath,width,height,style=undefined,renderHook,def,report,onDefChange}:CompParam_InspTarUI){
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
    CalibDataCollection = 100,
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
  function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number)
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

    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth*2/3;
    canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);



  }





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
    

    case editState.CalibDataCollection:
      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>









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
      lengthl:400,

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





    </>

    <br/>colorThres
    <Slider defaultValue={srule_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(srule_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

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
    CalibDataCollection = 100,
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
  function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number)
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

    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth*2/3;
    canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);



  }





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
    

    case editState.CalibDataCollection:
      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>









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

    ...def
  }
  const _this = useRef<any>({}).current;
  return <>

  
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
      



        <>
    
    
    <br/>結果顯示
    <Slider defaultValue={def_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onDefChange(ObjShellingAssign(def_Filled,["resultOverlayAlpha"],v));
    },()=>_this.trigTO=undefined,500);

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

    <br/>colorThres
    <Slider defaultValue={def_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onDefChange(ObjShellingAssign(def_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}


function SingleTargetVIEWUI_SurfaceCheckSimple({display,stream_id,fsPath,width,height,style=undefined,renderHook,def,report,onDefChange}:CompParam_InspTarUI){
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

  
  enum EditState {
    Normal_Show = 0,
    Region_Edit = 1,
    CalibDataCollection = 100,
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
            // let RP=pkts.find((p:any)=>p.type=="RP");
            // if(RP===undefined)return;
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
  function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number)
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

    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth*2/3;
    canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);



  }





  let EDIT_UI= null;
  
  switch(editState)
  {
    
    case EditState.Normal_Show:


      let EditUI=null;
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

        <Button key={"_"+10000} onClick={()=>{
          setEditState( EditState.Region_Edit);
        }}>EDIT</Button>

        <Button onClick={()=>{
          onDefChange(cacheDef,true)
        }}>SAVE</Button>


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

function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number)
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

  // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
  ctx.lineWidth = lineWidth*2/3;
  canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);



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


      await api.CameraSetup(camSetupInfo,0);
    })()

    return ()=>{
      (async ()=>{
        let api =CoreAPI
        await api.CameraSetChannelID([camSetupInfo.id],0,{
          resolve:()=>0,
          reject:()=>0
        });
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


  if(param.def.type=="SurfaceCheckSimple")
  return <SingleTargetVIEWUI_SurfaceCheckSimple {...param} />;


  return  <></>;
}


function TargetViewUIShow({displayIDList,defConfig,onDefChange,renderHook}:{displayIDList:string[],defConfig:any, onDefChange:(updatedDef:any)=>void,renderHook:any})
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
        return  <><br/>---hide----<br/></>
      }
      return  <InspTargetUI_MUX 
        display={idx<displayInspTarIdx.length} 
        width={100/displayInspTarIdx.length} 
        height={100} 
        stream_id={50120}
        style={{float:"left"}} 
        key={inspTar.id} 
        def={inspTar} 
        report={undefined} 
        fsPath={defConfig.path}
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
    console.log(prjDef)
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
      await api.InspTargetCreate(inspTar);

      await api.InspTargetSetStreamChannelID(
        inspTar.id,inspTar.stream_id,
        {
          resolve:(pkts)=>{
            console.log(pkts);
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
          console.log(RP);
          let filteredKey=Object.keys(_this.listCMD_Vairable.reportListener)
            .filter(key=>{
              let repListener=_this.listCMD_Vairable.reportListener[key];
              if(repListener.inspTar_id && repListener.inspTar_id!==RP.id)return false;
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
        content:undefined
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
                    let api =BPG_API
                    console.log(ncam);
                    let trigMode=ncam.trigger_mode;
                   
                    await api.CameraSetup(ncam,trigMode);
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


  let siderUI=
  <Sider width={siderBaseSize+extSizerSize}>
  {baseSiderTabs}
  {extSiderTabs}

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
      <TargetViewUIShow displayIDList={displayInspTarId} defConfig={defConfig}  onDefChange={(newdef:any)=>{setDefConfig(newdef)}}  renderHook={_this.listCMD_Vairable.renderHook}/>}
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
      CNC_api.onConnected=()=>ACT_EXT_API_CONNECTED(CNC_PERIPHERAL_ID);
  
      CNC_api.onInfoUpdate=(info:[key: string])=>ACT_EXT_API_UPDATE(CNC_api.id,info);
  
      CNC_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CNC_PERIPHERAL_ID);
      
      CNC_api.BPG_Send=core_api.send.bind(core_api);
  
      ACT_EXT_API_REGISTER(CNC_api.id,CNC_api);
    }
    






    core_api.onConnected=()=>{
      ACT_EXT_API_CONNECTED(CORE_ID);

      CNC_api.connect({
        // uart_name:"/dev/cu.SLAB_USBtoUART",
        uart_name:"/dev/cu.usbserial-1420",
        baudrate:460800//230400//115200
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
