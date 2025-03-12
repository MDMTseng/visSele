import { type_CameraInfo } from './AppTypes';
import { BPG_WS } from './EXT_API';
import { useState,useEffect,useRef } from 'react';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComp/CanvasComponent';
import {type_IMCM} from './AppTypes';

import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch,Select,TreeSelect } from 'antd';

import { Row, Col,Input,Tag,Modal,message,Space,Popover } from 'antd';

import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg,PtRotate2d} from './UTIL/MathTools';


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



export function CameraSetupEditUI({camSetupInfo,CoreAPI,onCameraSetupUpdate,imageSavePath="./data/"}:{ camSetupInfo:type_CameraInfo, CoreAPI:BPG_WS,onCameraSetupUpdate:(caminfo:type_CameraInfo|undefined)=>void,imageSavePath?:string}){

    const _this = useRef<{canvasComp:DrawHook_CanvasComponent|undefined,imgCanvas:HTMLCanvasElement
    }>({
      canvasComp:undefined,
      imgCanvas:document.createElement('canvas') as HTMLCanvasElement
    }).current;
  
    const _this2 = useRef<any>({}).current;
  
    const [previewWindowHeight,_setPreviewWindowHeight]=
      useState<number>(camSetupInfo?.previewWindowHeight||300);
    
    const [showDetailCtrl,setShowDetailCtrl]=useState<boolean>(false);
  
    const [Local_IMCM,setLocal_IMCM]=
      useState<type_IMCM|undefined>(undefined);
    



    function setPreviewWindowHeight(height:number)
    {
      _setPreviewWindowHeight(height);
      onCameraSetupUpdate({...camSetupInfo,previewWindowHeight:height})
    }
    
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
  
  
    let detailUI=showDetailCtrl?
    <>

      <br/>
      gamma:<InputNumber value={camSetupInfo.gamma} onChange={(num)=>{
        onCameraSetupUpdate({...camSetupInfo,gamma:num})
      }}/> 
      {" "}
      black_level:<InputNumber value={camSetupInfo.black_level} onChange={(num)=>{
        onCameraSetupUpdate({...camSetupInfo,black_level:num})
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
    </>:<></>;
  
  
  
    return <> 
      別名:<Input value={camSetupInfo.side_name} onChange={(side_name)=>{
        onCameraSetupUpdate({...camSetupInfo,side_name:side_name.target.value})
      }}/>
      <br/>
      
      trigger_on:
      <Switch checkedChildren="O" unCheckedChildren="X" checked={camSetupInfo.trigger_mode==1} onChange={(check)=>{
        onCameraSetupUpdate({...camSetupInfo,trigger_mode:check?1:0})
      }}/>
      {" "}
      exposure:<InputNumber value={camSetupInfo.exposure} onChange={(num)=>{
        onCameraSetupUpdate({...camSetupInfo,exposure:num})
      }}/>
      {" "}
      analog_gain:<InputNumber value={camSetupInfo.analog_gain} onChange={(num)=>{
        onCameraSetupUpdate({...camSetupInfo,analog_gain:num})
      }}/>
      {" "}
      <Switch style={{marginLeft:"30px"}} checkedChildren="顯示更多" unCheckedChildren="隱藏細節" checked={showDetailCtrl} onChange={(check)=>{
        setShowDetailCtrl(check);
      }}/>

      <br/>
      {detailUI}
      <Button onClick={()=>{
        setPreviewWindowHeight(previewWindowHeight*1.5);
      }}>+</Button>
  
      <Button onClick={()=>{
        setPreviewWindowHeight(previewWindowHeight/1.5);
      }}>-</Button>
  
      <Button onClick={()=>{
        CoreAPI.CameraSaveLatestImage(camSetupInfo.id,`${imageSavePath}/${camSetupInfo.side_name||camSetupInfo.id}_${Date.now()}.png`);
      }}>SaveLatestImg</Button>
  
  
  
      <HookCanvasComponent style={{height:previewWindowHeight+"px"}} dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
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
            g.ctx.imageSmoothingEnabled = false;
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
      
      <Popconfirm
        title="確定刪除?"
        onConfirm={()=>{
  
          onCameraSetupUpdate(undefined);
        }}
        onCancel={()=>{
        }}
        okText="Yes"
        cancelText="No"
      >
        <Button danger type='primary'>X</Button>
      </Popconfirm>
    </>
  }
  
  
  