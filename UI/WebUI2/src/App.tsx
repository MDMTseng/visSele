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
  readonly:boolean,
  style?:any,
  stream_id:number,
  width:number,height:number,
  renderHook:((ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,rule:any)=>void)|undefined,
  IMCM_group:IMCM_group,
  def:any,
  report:any,
  onDefChange:(updatedDef:any,ddd:boolean)=>void}

  

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





function SingleTargetVIEWUI_ColorRegionDetection({readonly,stream_id,width,height,style=undefined,renderHook,IMCM_group,def,report,onDefChange}:CompParam_InspTarUI){
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

  
  useEffect(() => {

    (async ()=>{

      let ret = await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      await BPG_API.InspTargetSetStreamChannelID(
        cacheDef.id,stream_id,
        {
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
        await BPG_API.InspTargetSetStreamChannelID(
          cacheDef.id,0,
          {
            resolve:(pkts)=>{
            },
            reject:(pkts)=>{
    
            }
          }
        )
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


  useEffect(() => {
    let newIMCM=IMCM_group[cacheDef.trigger_tag+cacheDef.camera_id];
    // console.log(newIMCM,rule.trigger_tag);
    if(Local_IMCM===newIMCM)return;
    
    if(newIMCM===undefined)return;
    _this.imgCanvas.width = newIMCM.image_info.width;
    _this.imgCanvas.height = newIMCM.image_info.height;
    // console.log(IMCM.image_info);
    let ctx2nd = _this.imgCanvas.getContext('2d');
    ctx2nd.putImageData(newIMCM.image_info.image, 0, 0);
    setLocal_IMCM(newIMCM);
    if(_this.canvasComp!==undefined)
    {
      _this.canvasComp.draw();
    }
  }, [IMCM_group]); 

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
      if(readonly==false)
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
                    HACK_do_Camera_Check=true;
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

function CameraSetupEditUI({camSetupInfo,fetchCoreAPI,onCameraSetupUpdate}:{ camSetupInfo:type_CameraInfo, fetchCoreAPI:()=>Promise<BPG_WS>,onCameraSetupUpdate:(caminfo:type_CameraInfo)=>void}){

  const _this = useRef<any>({

    imgCanvas:document.createElement('canvas')
  }).current;

  const [Local_IMCM,setLocal_IMCM]=
    useState<type_IMCM|undefined>(undefined);
  
  useEffect(()=>{//load default

    (async ()=>{
      let api = await fetchCoreAPI()
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
      (async ()=>{
        let api = await fetchCoreAPI()
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


  const _this = useRef<any>({
    listCMD_Vairable:{
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
  const [IMCM_group,setIMCM_group]=useState<IMCM_group>({});
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));

  const [defConfig,setDefConfig]=useState<any>(undefined);
  const [cameraQueryList,setCameraQueryList]=useState<any[]|undefined>([]);


  const [defReport,setDefReport]=useState<any>(undefined);
  const [forceUpdateCounter,setForceUpdateCounter]=useState(0);




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
    // let reloadRes = await api.InspTargetUpdate(_defInfo,INSP1_REP_PGID_);
    let reloadRes = await api.InspTargetUpdate(_defInfo);
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
      await api.InspTargetCreate(inspTar);
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



  // console.log(displayInspTarId,displayInspTarIdx,displayInspTarIdx_hide);

  let InspMenu=
  menuCol('Insp', 'insp',undefined, [
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
  menuCol('Camera', 'cam',undefined, [
    ...(
      defConfig===undefined?[ menuCol("WAIT...","WAIT...")]:
      (defConfig.main.CameraInfo
        .map((cam:type_CameraInfo,index:number)=>
          ( menuCol(<div onClick={()=>{
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
    cameraMenu,
    InspMenu,
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
    

  function InspTargetUI_MUX(param:CompParam_InspTarUI)
  {
    if(param.def.type=="ColorRegionDetection")
    return <SingleTargetVIEWUI_ColorRegionDetection {...param} />;


    return  <></>;
  }

  
  function TargetViewUIShow(InspTarList:any[],displayIDList:string[],IMCM_group:IMCM_group)
  {

    let InspTarList_show=InspTarList;
    let InspTarList_hide;

      
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

    console.log(InspTarList);
    console.log(displayIDList,displayInspTarIdx,displayInspTarIdx_hide);

    // let inspTar=defConfig.InspTars_main [index]
    // let rep_rules=GetObjElement(defReport,["rules"]);
    // let subRuleRep=undefined;
    // if(rep_rules!==undefined)
    //   subRuleRep=rep_rules.find((rep_rule:any)=>rep_def.id==inspTar.id);
    // console.log(rule,subRuleRep);
    // subRuleRep
    // let whsetting={w:50,h:50};
    // if(show)
    //   whsetting=WHArr[index];
    // return <SingleTargetVIEWUI_ColorRegionDetection 
    // readonly={false} 
    // width={displaySetting.w+"%"} 
    // height={displaySetting.h+"%"} 
    // style={{float:"left",display:displaySetting.hide?"none":undefined}} 
    // key={inspTar.id} 
    // IMCM_group={IMCM_group} 
    // rule={inspTar} 
    // report={subRuleRep} 
    // renderHook={_this.listCMD_Vairable.renderHook} 
    // onDefChange={(new_rule,doInspUpdate=true)=>{
    
    // }}/>



    // function SingleTargetVIEWUI_ColorRegionDetection({readonly,width,height,style=undefined,renderHook,IMCM_group,def,report,onDefChange}:CompParam_InspTarUI)
    return <>
    {
      // displayInspTarIdx.map((idx:number)=>InspTarList[idx]).map((inspTar:any)=>inspTar.id+":"+inspTar.type+",")
      displayInspTarIdx.map((idx:number)=>InspTarList[idx]).map((inspTar:any)=><InspTargetUI_MUX 
        readonly={false} 
        width={20} 
        height={100} 
        stream_id={50120}
        style={{float:"left"}} 
        key={inspTar.id} 
        IMCM_group={IMCM_group} 
        def={inspTar} 
        report={undefined} 
        renderHook={_this.listCMD_Vairable.renderHook} 
        onDefChange={(new_rule,doInspUpdate=true)=>{
          console.log(new_rule);

          let idx = InspTarList.findIndex(itar=>itar.id==new_rule.id);
          if(idx<0)return;

          let newDefConfig={...defConfig,InspTars_main:[...InspTarList]};
          newDefConfig.InspTars_main[idx]=new_rule;
          
          setDefConfig(newDefConfig)
        }}/>)
    }
    <br/>---hide----<br/>
    {
      displayInspTarIdx_hide.map((idx:number)=>InspTarList[idx]).map((inspTar:any)=>inspTar.id+",")
    }
    
    
    </>;
  }


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
    { (defConfig===undefined)?"WAIT":TargetViewUIShow(defConfig.InspTars_main,displayInspTarId,IMCM_group)}
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

      // CNC_api.connect({
      //   uart_name:"COM5",
      //   baudrate:115200
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
