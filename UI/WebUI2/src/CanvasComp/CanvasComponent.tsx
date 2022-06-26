'use strict'

import React, { useState, useEffect, useRef ,createRef,useLayoutEffect} from 'react';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from '../UTIL/MathTools';

import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg} from '../UTIL/MathTools';


import Ajv from "ajv"
const ajv = new Ajv()

// let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
// let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
// let BPG_FileBrowser_varify_info = BASE_COM.BPG_FileBrowser_varify_info;
import {EverCheckCanvasComponent_proto} from './CanvasInterface';


import {
  round as roundX,
  GetObjElement, Exp2PostfixExp, PostfixExpCalc,xstate_GetCurrentMainState,
  LocalStorageTools
} from '../UTIL/MISC_Util';

let xState=xstate_GetCurrentMainState;

import * as log from 'loglevel';
import dclone from 'clone';
import Modal from "antd/lib/modal";
import Menu from "antd/lib/menu";
import Button from "antd/lib/button";
import Icon from 'antd/lib/icon';
import Tag from 'antd/lib/tag';
import Table  from 'antd/lib/table';
import Checkbox from "antd/lib/checkbox";
import InputNumber from 'antd/lib/input-number';
import Input from 'antd/lib/input';
const { CheckableTag } = Tag;
const { TextArea } = Input;
import Divider from 'antd/lib/divider';
import Dropdown from 'antd/lib/Dropdown'
import Slider from 'antd/lib/Slider';
import Popover from 'antd/lib/Popover';

const SubMenu = Menu.SubMenu;

import { useSelector,useDispatch } from 'react-redux';
import { 
  LockOutlined,
  CloseOutlined,
  PlusOutlined,
  FormOutlined,
  EditOutlined,
  SaveOutlined,
  ExportOutlined,
  SettingOutlined,
  CameraOutlined,
  ArrowLeftOutlined,
  DownOutlined, TrophyOutlined,
  SubnodeOutlined
} from '@ant-design/icons';



const LOCALSTORAGE_KEY="GenMatching.RecentDefFiles"


function getLocalStorage_RecentFiles(key=LOCALSTORAGE_KEY)
{
  let LocalS_RecentDefFiles =LocalStorageTools.getlist(key);
  // LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter(BPG_FileBrowser_varify_info);
  // console.log(LocalS_RecentDefFiles);
  return LocalS_RecentDefFiles;
}

function appendLocalStorage_RecentFiles(key=LOCALSTORAGE_KEY,fileInfo:any)
{
  
  return LocalStorageTools.appendlist(key,fileInfo,
    (ls_fileInfo,idx) =>
      (idx<100)&&//Do list length limiting
      (ls_fileInfo.path != fileInfo.path));

}


export type type_DrawHook_g={
  ctx:CanvasRenderingContext2D,
  // base_img:CanvasRenderingContext2D,
  // base_img_scale:number,
  // mmpp_mult:number,
  worldTransform:DOMMatrix
  worldTransform_inv:DOMMatrix


  mouseOnCanvas:VEC2D,
  pmouseOnCanvas:VEC2D,

  mouseStatus:{
    x:number,
    y:number,
    px:number,
    py:number,
    status:number,
    pstatus:number
  },
  mouseEdge:boolean,
}

export type type_DrawHook=(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>void

export class DrawHook_CanvasComponent extends EverCheckCanvasComponent_proto {

  regionSelect:{
    pt1:VEC2D|undefined,
    pt2:VEC2D|undefined,
    pcvst1:VEC2D|undefined,
    pcvst2:VEC2D|undefined,
    onSelect:(info:any,ctx:any)=>any,
    start:boolean
  }|undefined

  
  onMouseStatUpdate:(pt:VEC2D,pcvst:VEC2D)=>void
  // onViewPortUpdate:(pt:VEC2D,pcvst:VEC2D)=>void



  g:type_DrawHook_g|undefined


  cur_worldTransform:DOMMatrix
  cur_worldTransform_inv:DOMMatrix


  drawHook:type_DrawHook|undefined


  constructor(canvasDOM:HTMLCanvasElement) {
    super(canvasDOM);
    this.reset();
    this.regionSelect=undefined;
    this.cur_worldTransform_inv= new DOMMatrix();
    this.cur_worldTransform= new DOMMatrix();
    this.drawHook=undefined

    this.onMouseStatUpdate=(pt,pcvst)=>0;
  }


  canvasResize(canvas:HTMLCanvasElement,width:number,height:number)
  {
    width = Math.floor(width);
    height = Math.floor(height);
    canvas.width = width;
    canvas.height = height;
  }

  regionSelectUpdate(pcvst:VEC2D,selState:number)
  {
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = wMat.scale(1);//copy
    let worldTransform_inv = worldTransform.invertSelf();
    let pt=this.VecX2DMat(pcvst, worldTransform_inv);


    this.onMouseStatUpdate(pt,pcvst);

    if(this.regionSelect===undefined)return;
    if(selState!=0 && this.regionSelect.start!=true)return;
    this.regionSelect.start=true;
    if(selState==0)
    {
      this.regionSelect.pt1= this.regionSelect.pt2=pt;
      this.regionSelect.pcvst1= this.regionSelect.pcvst2=pcvst;
    }

    if(selState==1 || selState==2)
    {
      this.regionSelect.pt2=pt;
      this.regionSelect.pcvst2=pcvst;
    }

    
    this.regionSelect.onSelect(this.regionSelect,selState);
  }

  reset()
  {
  }

  onmouseup(evt:MouseEvent) {
    let pos = this.getMousePos(evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 0;

    if(this.regionSelect===undefined)
    {
      this.camera.EndDrag();
  
      this.debounce_zoom_emit();
    }
    else
    {
      // console.log(this.regionSelect);
      this.regionSelectUpdate(pos,2);
    }
    this.ctrlLogic();
    this.draw();
  }


  onmousemove(evt:MouseEvent) {
    let pos = this.getMousePos(evt);
    // console.log(pos);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    let doDraw=true;
    if(this.regionSelect===undefined)
    {
      if (doDragging) {
        if (this.mouseStatus.status == 1) {
          
          doDraw=true;
          this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });
        }
  
      }
    }
    else
    {
      this.regionSelectUpdate(this.mouseStatus,1);
    }


    if(doDraw)
    {
      this.ctrlLogic();
      this.draw();
    }
  }

  scaleHeadToFitScreen()
  {
    this.camera.SetOffset({x:this.canvas.width/2,y:this.canvas.height/2});
    
  }



  
  UserRegionSelect(onSelect:
    ((info:{pt1: VEC2D,pt2: VEC2D,pcvst1: VEC2D,pcvst2: VEC2D},SeleState:number)=>void) |undefined)
  {
    if(onSelect==undefined)
    {
      this.regionSelect=undefined;
      return;
    }


    this.regionSelect={
      pt1:undefined,
      pt2:undefined,
      pcvst1:undefined,
      pcvst2:undefined,
      onSelect,
      start:false
    };
  }
  onmousedown(evt:MouseEvent) 
  {

    super.onmousedown(evt);
    // this.doDragging=false;
    
    if(this.regionSelect!==undefined)
    {
      this.regionSelectUpdate(this.mouseStatus,0);
    }

  }


  setTransform(ctx:CanvasRenderingContext2D,matrix:DOMMatrix)
  {
    ctx.resetTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);
  }

  draw() {

    if(this.g===undefined)return;
    // console.log(this.c_state);
    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    };
    let ctx = this.g.ctx;
    // let ctx2nd = this.secCanvas.getContext('2d');
    if(ctx==null )return;
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    this.setTransform(this.g.ctx,this.g.worldTransform);

    ctx.setLineDash([]);

    // {
    //   ctx.imageSmoothingEnabled = false;//scale!=1;
    //   ctx.webkitImageSmoothingEnabled = scale!=1;

    //   //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
    //   ctx.save();

    //   let curScale=this.camera.GetCameraScale();
    //   if(true){

    //     ctx.scale(mmpp_mult, mmpp_mult);
    //     if (this.IM.offsetX !== undefined && this.IM.offsetY !== undefined) {
    //       // ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
    //       ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
    //     }
    //     // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
    //     ctx.drawImage(this.secCanvas, 0, 0);
        
    //     ctx.strokeStyle = "rgba(120, 120, 120,30)";
        
    //     ctx.lineWidth = 10/curScale/mmpp_mult;
    //     this.rUtil.drawImageBoundaryGrid(ctx,this.IM,100000/curScale);
  
    //   }
      
    //   ctx.restore();
    // }

    if(this.drawHook!==undefined && this.g!==undefined)
    {
      this.drawHook(false,this.g,this);
    }



  }

  // DownloadCanvasAsImage(canvas,fileName){
  //   let downloadLink = document.createElement('a');
  //   downloadLink.setAttribute('download', fileName);
  //   canvas.toBlob(function(blob) {
  //     let url = URL.createObjectURL(blob);
  //     downloadLink.setAttribute('href', url);
  //     downloadLink.click();
  //   });
  // }
  ctrlLogic() {

    let ctx = this.canvas.getContext('2d');
    if(ctx===null)return;
    //let mmpp = this.rUtil.get_mmpp();
    let worldTransform = this.worldTransform();
    let worldTransform_inv = worldTransform.inverse();

    this.cur_worldTransform=worldTransform;
    this.cur_worldTransform_inv=worldTransform_inv;
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);

    // this.EmitEvent(EV_UI_Canvas_Mouse_Location(mouseOnCanvas2));
    // console.log(ifOnMouseLeftClickEdge,mouseOnCanvas2);

    
    this.g={
      ctx,
      worldTransform,
      worldTransform_inv,
      mouseOnCanvas:mouseOnCanvas2,
      pmouseOnCanvas:pmouseOnCanvas2,

      mouseStatus:this.mouseStatus,
      mouseEdge:ifOnMouseLeftClickEdge,

    }
    if(this.drawHook!==undefined)
    {
      this.drawHook(true,this.g,this);
    }

  }
}


function useDivDimensions(divRef:React.RefObject<HTMLDivElement | undefined>) {
  const [dimensions, setDimensions] = useState([0, 0]);


  const _this = useRef({isInitSable:false,initQueryInterval:-1,bkClientRect:{width:-1,height:-1},bkDim:[0,0]}).current;

  useLayoutEffect(() => {

    function updateSize()
    {
      if (divRef.current) {
        _this.isInitSable=true;
        const { current } = divRef
        const boundingRect = current.getBoundingClientRect()
        const { width, height } = boundingRect
        if(width==0 && height==0)return;
        const rwh  = [  Math.round(width), Math.round(height) ]
        if(rwh[0]==_this.bkDim[0] &&rwh[1]==_this.bkDim[1])return;
        setDimensions(rwh)
        _this.bkDim=rwh;
      }
    }

    const ro =new ResizeObserver(updateSize);
    
    ro.observe(divRef.current as Element);
    return () =>{
      console.log("REMOVE....");
      ro.unobserve(divRef.current as Element);
    }
  }, []);
  return dimensions;
}

export function HookCanvasComponent( {dhook}:{
  dhook:type_DrawHook
}) {

  const divRef = useRef<HTMLDivElement>(null)
  const [width, height] = useDivDimensions(divRef);



  // console.log("<<<<",width, height)
  const pixelRatio = window.devicePixelRatio;
  const canvas = useRef<HTMLCanvasElement>(null);
  const _r = useRef<{canvComp:DrawHook_CanvasComponent|undefined}>({canvComp:undefined});
  let _this=_r.current;

  // responsive width and height
  useEffect(() => {
    if(canvas===null || canvas.current===null)return;
    _this.canvComp=new DrawHook_CanvasComponent(canvas.current);
    
    _this.canvComp.pixelRatio=pixelRatio;
    return () => {
      _this.canvComp=undefined;//delete the canvas component
    };
  }, []);

  useLayoutEffect(() => {
    if(canvas===null || canvas.current===null)return;
    
    if(_this.canvComp===undefined)return;

    _this.canvComp.drawHook=dhook;
    _this.canvComp.ctrlLogic();
    _this.canvComp.draw();
  }, [width, height,dhook]);

  // console.log(width_,height_);
  const displayWidth = Math.floor(pixelRatio * width);
  const displayHeight = Math.floor(pixelRatio * height);
  return (
    <div style={{ width: '100%', height: '100%' }} ref={divRef}>
      <canvas
        style={{ width: '100%', height: '100%' }}
        ref={canvas}
        width={displayWidth}
        height={displayHeight}
        // style={style}
      />
    </div>
  );
};
