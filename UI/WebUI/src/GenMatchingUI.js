'use strict'

import React, { useState, useEffect, useRef } from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from 'UTIL/MathTools';



import Ajv from "ajv"
const ajv = new Ajv()

let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
let BPG_FileBrowser_varify_info = BASE_COM.BPG_FileBrowser_varify_info;
import DragSortableList from 'react-drag-sortable'
import ReactResizeDetector from 'react-resize-detector';
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import {Schema_GenMatchingDefFile} from './GenMatchingUTIL';

import {
  round as roundX, websocket_autoReconnect,
  websocket_reqTrack, dictLookUp, undefFallback,
  GetObjElement, Exp2PostfixExp, PostfixExpCalc,
  ExpCalcBasic, ExpValidationBasic,defFileGeneration,xstate_GetCurrentMainState,
  LocalStorageTools
} from 'UTIL/MISC_Util';

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

function appendLocalStorage_RecentFiles(key=LOCALSTORAGE_KEY,fileInfo)
{
  
  return LocalStorageTools.appendlist(key,fileInfo,
    (ls_fileInfo,idx) =>
      (idx<100)&&//Do list length limiting
      (ls_fileInfo.path != fileInfo.path));

}



class DrawHook_CanvasComponent extends EC_CANVAS_Ctrl.EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.reset();
    this.EmitEvent = (event) => { log.info(event); };
    this.tmp_canvas= document.createElement('canvas');
    this.regionSelect=undefined;
    
  }


  canvasResize(canvas,width,height)
  {
    width = Math.floor(width);
    height = Math.floor(height);
    canvas.width = width;
    canvas.height = height;
  }


  reset()
  {
    let mmpp=1;
    this.db_obj = {
      cameraParam:{
        mmpb2b:1,
        ppb2b:1
      }
    };

    
  }

  onmouseup(evt) {
    let pos = this.getMousePos(this.canvas, evt);
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
      {
        let wMat = this.worldTransform();
        //log.debug("this.camera.matrix::",wMat);
        let worldTransform = new DOMMatrix().setMatrixValue(wMat);
        let worldTransform_inv = worldTransform.invertSelf();
        this.regionSelect.pt1= this.VecX2DMat(this.regionSelect.pcvst1, worldTransform_inv);
        this.regionSelect.pt2= this.VecX2DMat(this.regionSelect.pcvst2, worldTransform_inv);


      }



      this.regionSelect.onSelect(this.regionSelect,this);
      this.regionSelect=undefined;
    }
    this.ctrlLogic();
    this.draw();
  }


  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
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
      this.regionSelect.pcvst2={ x: this.mouseStatus.x, y: this.mouseStatus.y};
    }


    if(doDraw)
    {
      this.ctrlLogic();
      this.draw();
    }
  }

  scaleHeadToFitScreen()
  {
    console.log(this.singleReportInfo);

    this.camera.SetOffset(this.canvas.width/2,this.canvas.height/2);
    if(this.singleReportInfo==undefined)return;
    let sr = this.singleReportInfo.report
    {

      let width=Math.hypot(sr.vec_btm.x,sr.vec_btm.y);

      
      let curScale = this.camera.GetCameraScale();
      this.camera.Scale(this.canvas.width/width/curScale);
    }

    
  }
  UserRegionSelect(onSelect)
  {
    this.regionSelect=onSelect!==undefined?{onSelect}:undefined;//able to cancel it
  }
  onmousedown(evt) 
  {

    super.onmousedown(evt);
    // this.doDragging=false;
    if(this.regionSelect!==undefined)
    {
      this.regionSelect.pcvst1=this.regionSelect.pcvst2=
        { x: this.mouseStatus.x, y: this.mouseStatus.y};
    }

  }

  SetIM(IM)
  {
    
    if (IM === undefined || IM == this.IM) return;
    this.IM=IM;


    this.secCanvas.width = IM.width;
    this.secCanvas.height = IM.height;
    this.secCanvas_rawImg = IM.img;
    this.rUtil.renderParam.mmpp = IM.cameraInfo.mmpp;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(this.secCanvas_rawImg, 0, 0);


  }


  SetRP(RP)
  {
    this.RP=RP;
    console.log(RP);
  }


  SetUI_Info(UI_INFO)
  {
    this.UI_INFO=UI_INFO;
    console.log(UI_INFO);
  }


  draw() {

    
    if(this.IM===undefined || this.IM.scale === undefined)
    {
      return;
    }
    // console.log(this.c_state);
    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    };
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);

    ctx.setLineDash([]);
    let scale = this.IM.scale;
    let mmpp=1;
    let mmpp_mult = scale * mmpp;
    this.g={
      ctx,
      base_img:ctx2nd,
      base_img_scale:scale,
      mmpp_mult,
      worldTransform:this.cur_worldTransform,
      worldTransform_inv:this.cur_worldTransform_inv
    }

    // console.log(this.IM,this.RP,this.UI_INFO);


    {
      ctx.imageSmoothingEnabled = false;//scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      let curScale=this.camera.GetCameraScale();
      if(true){

        ctx.scale(mmpp_mult, mmpp_mult);
        if (this.IM.offsetX !== undefined && this.IM.offsetY !== undefined) {
          // ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
          ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
        }
        // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
        ctx.drawImage(this.secCanvas, 0, 0);
        
        ctx.strokeStyle = "rgba(120, 120, 120,30)";
        
        ctx.lineWidth = 10/curScale/mmpp_mult;
        this.rUtil.drawImageBoundaryGrid(ctx,this.IM,100000/curScale);
  
      }
      
      ctx.restore();
    }

    if(this.drawHook!==undefined)
    {
      this.drawHook(false,this.g,this);
    }



  }

  DownloadCanvasAsImage(canvas,fileName){
    let downloadLink = document.createElement('a');
    downloadLink.setAttribute('download', fileName);
    canvas.toBlob(function(blob) {
      let url = URL.createObjectURL(blob);
      downloadLink.setAttribute('href', url);
      downloadLink.click();
    });
  }
  ctrlLogic() {

    //let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();

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







class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  

  componentDidUpdate(prevProps) {
  }

  componentDidMount() {
    this.ec_canvas = new DrawHook_CanvasComponent(this.refs.canvas);

    if(this.props.onCanvasInit!==undefined)
      this.props.onCanvasInit(this);

    this.onResize(500, 500)
    //this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  draw()
  {
    this.ec_canvas.draw();
  }

  update(IM) {

    if (this.ec_canvas !== undefined) {
      {
        this.ec_canvas.SetIM(IM);
        this.draw();
      }
    }
  }


  updateCanvas(ec_state, props = this.props) {
    this.update(props.IM,props.RP,props.UI_Info);
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 20) return;
    console.log(this.windowSize.width ,this.windowSize.height,width, height );
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
    }
  }

  componentWillUpdate(nextProps, nextState) {

  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}


const genDef_validator = ajv.compile(Schema_GenMatchingDefFile)





var enc = new TextEncoder();

const GEN_DEF_EXT="genDef";

const configState={
  normal:"normal",
  shape_positioning:"shape_positioning",
  template_matching:"template_matching",
  fine_positioning:"fine_positioning",
  test:"test"
}




function InspRule_positioning_Config({insprule,InspRefImg,ASYNC_WS_SEND_BPG,setCanvasHook,launchRegionSelect,onRuleUpdate,defInfoPath})
{
  let _cur = useRef({
    init:true,
    canvasRenderInfo:{
    }
  });
  let _this=_cur.current;


  const [_insprule,set_insprule]=useState(insprule);

  console.log(_insprule);

  function _setCanvasHook(info)
  {
    _this.canvasRenderInfo=info;
    setCanvasHook(info)

  }

  useEffect(() => {
    setCanvasHook(_this.canvasRenderInfo)
    return () => {
      setCanvasHook(undefined)
      // onRuleUpdate(_insprule)
    };
  }, [])


  function runMatch(ruleInfo,refImgPath)
  {




    ASYNC_WS_SEND_BPG("II", 0, {
      imgsrc:"__CACHE_IMG__",
      camera_id:InspRefImg.cameraInfo.id,
      definfo:{
        ...ruleInfo,
        type:"FM_GenMatching",
        insp_type:"shape_features_matching",
        param_type:"load_template_image",
        image_src_path:refImgPath+"/"+InspRefImg.cameraInfo.refrenceImage,
        
      },
      // img_property:{
      //   down_samp_level:3
      // }
    })
    .then(pkts=>{
      console.log(pkts);
      
      let RP = pkts.find(pkt=>pkt.type=="RP");
      if(RP===undefined)
      {
        throw {
          why:"no report",
          info:pkts
        }
      }
      
      let matchResults = RP.data.matchResults;

      
      _setCanvasHook({
        matchResults,
        draw:(info,g,canvas_obj)=>{
          let ctx = g.ctx;
          let psize = 2*canvas_obj.rUtil.getPointSize();
          ctx.lineWidth = psize/3;

          matchResults.forEach((ms)=>{

            ctx.fillStyle =
            ctx.strokeStyle = "rgba(50, 200, 50,0.8)";
            if(ms.refine_score<0.5)
            {
              ctx.fillStyle =
              ctx.strokeStyle = "rgba(200, 0, 0,0.8)";
            }

            canvas_obj.rUtil.drawcross(ctx, ms,psize);
            let lineLength=100;
            let offsetx=lineLength*Math.cos(ms.angle*3.14159/180);
            let offsety=lineLength*Math.sin(ms.angle*3.14159/180);
            ctx.beginPath();
            ctx.moveTo(ms.x, ms.y);
            ctx.lineTo(ms.x+offsetx, ms.y+offsety);
            ctx.stroke();
            


            ctx.font = '20px serif';
            ctx.fillText('s:'+ms.similarity.toFixed(2) +" cl:"+ms.class_id, ms.x, ms.y);
            ctx.fillText('ang:'+ms.angle.toFixed(2)+" rf_score:"+ms.refine_score, ms.x, ms.y+50);
          })
        }

      })

    })

  }
  function CtrlHook_selectRegion(info,g,canvas_obj)
  {
    // console.log(">>>");
    if(canvas_obj.regionSelect===undefined)
    {
      info.sel_region=undefined
      return;
    }

    if(canvas_obj.regionSelect.pcvst1===undefined || canvas_obj.regionSelect.pcvst2===undefined)
    {
      return;
    }

    let pt1 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
    let pt2 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
      
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
    info.sel_region={
      x,y,w,h
    }
    
  }
  function DrawHook_selectRegionDraw(info,g,canvas_obj)
  {
    if(info.sel_region===undefined)return;
    let ctx = g.ctx;

    ctx.strokeStyle = 'red';
    ctx.lineWidth = 5;

    let x = info.sel_region.x;
    let y = info.sel_region.y;
    let w = info.sel_region.w;
    let h = info.sel_region.h;
    ctx.beginPath();
    let LineSize = canvas_obj.rUtil.getIndicationLineSize();
    ctx.setLineDash([LineSize*10,LineSize*3,LineSize*3,LineSize*3]);
    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = LineSize;
    ctx.rect(x,y,w,h);
    ctx.stroke();
    ctx.closePath();

    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    let psize = 2*canvas_obj.rUtil.getPointSize();
    ctx.lineWidth = psize/3;
    canvas_obj.rUtil.drawcross(ctx, {x:x+w/2,y:y+h/2},psize);

  }
  function DrawHook_Template_py(info,g,canvas_obj)
  {
    let ctx = g.ctx;
    let psize = 2*canvas_obj.rUtil.getPointSize();
    ctx.lineWidth = psize/3;

    let py_mult=1;
    let templateInfo=info.Template_py||[];
    templateInfo.forEach((temp,idx)=>{
      temp.features.forEach((feature)=>{
        
        let x=py_mult*(temp.x+feature.x);
        let y=py_mult*(temp.y+feature.y);

        

        ctx.strokeStyle = "rgba("+0+", "+175/(idx+1)+", 0,0.5)";
        if(info.featureDraw_use_sel_region==true  && info.sel_region!==undefined)
        {
          delete feature._del;
          if(info.sel_region.x<x && info.sel_region.y<y &&
            info.sel_region.x+info.sel_region.w>x && 
            info.sel_region.y+info.sel_region.h>y)//check region includs 
          {
            
            ctx.strokeStyle = "rgba("+175/(idx+1)+", "+0+", 0,0.5)";
            feature._del=true;
          }
        }

        canvas_obj.rUtil.drawcross(ctx, {x,y},psize);
      })
      py_mult*=2;
    });
  
    
  }
  function DrawHook_featureDraw(info,g,canvas_obj)
  {
    let ctx = g.ctx;
    let psize = 2*canvas_obj.rUtil.getPointSize();
    ctx.lineWidth = psize/3;

    let py_mult=1;
    info.Template_py.forEach((temp,idx)=>{
      temp.features.forEach((feature)=>{
        
        let x=py_mult*(temp.x+feature.x);
        let y=py_mult*(temp.y+feature.y);

        

        ctx.strokeStyle = "rgba("+0+", "+175/(idx+1)+", 0,0.5)";
        if(info.featureDraw_use_sel_region==true  && info.sel_region!==undefined)
        {
          delete feature._del;
          if(info.sel_region.x<x && info.sel_region.y<y &&
            info.sel_region.x+info.sel_region.w>x && 
            info.sel_region.y+info.sel_region.h>y)//check region includs 
          {
            
            ctx.strokeStyle = "rgba("+175/(idx+1)+", "+0+", 0,0.5)";
            feature._del=true;
          }
        }

        canvas_obj.rUtil.drawcross(ctx, {x,y},psize);
      })
      py_mult*=2;
    });
  
    
  }


  return<>
    <Button onClick={()=>{
      onRuleUpdate(_insprule)
    }}>{"X"}</Button>

    <Button onClick={()=>{
      
      console.log(_insprule);
      _setCanvasHook({
        Template_py:_insprule.template_pyramid,
        ctrl:CtrlHook_selectRegion,
        draw:[DrawHook_selectRegionDraw,DrawHook_Template_py]
      })

      launchRegionSelect((evt,canvas)=>{
        console.log(evt,canvas);
        
        let x = evt.x;
        let y = evt.y;
        let w = evt.w;
        let h = evt.h;
        let ROI={x,y,w,h};
        let anchor={x:x+w/2,y:y+h/2};

        ASYNC_WS_SEND_BPG("II", 0, {
          imgsrc:"__CACHE_IMG__",
          camera_id:InspRefImg.cameraInfo.id,
          definfo:{
            type:"FM_GenMatching",
            insp_type:"extract_shape_features",
            ROI,anchor
          },
          // img_property:{
          //   down_samp_level:3
          // }
        })
        .then(pkts=>{
          console.log(pkts);

          
          let RP = pkts.find(pkt=>pkt.type=="RP");
          if(RP===undefined)
          {
            throw {
              why:"no report",
              info:pkts
            }
          }

          _this.canvasRenderInfo.Template_py=RP.data.template_pyramid;
          

          set_insprule({
            ..._insprule,
            ROI,anchor,

            template_pyramid:RP.data.template_pyramid
          })
          canvas.draw();
          // onConfigUpdate
        })



      });
    }}>ShapeRegion</Button>




    <Button onClick={()=>{
      _setCanvasHook({
        Template_py:_insprule.template_pyramid||[],
        featureDraw_use_sel_region:true,
        ctrl:CtrlHook_selectRegion,
        draw:[DrawHook_selectRegionDraw,DrawHook_featureDraw]
      })

      launchRegionSelect((evt,canvas)=>{
        let Template_py = _this.canvasRenderInfo.Template_py;
        Template_py.forEach((temp,idx)=>{
          temp.features=temp.features.filter(feature=>feature._del!==true);
        });

        _this.canvasRenderInfo.Template_py=Template_py;

        set_insprule({
          ..._insprule,
          template_pyramid:Template_py
        })
        canvas.draw();



      });
    }}>SelRmTempFeature</Button>









    <Button onClick={()=>{

      let locatingBlocks=_insprule.locatingBlocks||[];
      _setCanvasHook({
        locatingBlocks,
        ctrl:CtrlHook_selectRegion,
        draw:[
          DrawHook_selectRegionDraw,
          (info,g,canvas_obj)=>{
          let ctx = g.ctx;
          let psize = 2*canvas_obj.rUtil.getPointSize();
          ctx.lineWidth = psize/4;

          ctx.setLineDash([]);
          let LineSize = canvas_obj.rUtil.getIndicationLineSize();
          
          info.locatingBlocks.forEach((region)=>{
            ctx.beginPath();
            ctx.strokeStyle = "rgba(179, 100, 0,0.5)";
            ctx.lineWidth = LineSize;
            ctx.rect(region.x, region.y, region.w, region.h);
            ctx.stroke();
            ctx.closePath();

          })
        }]
      })

        
      launchRegionSelect((evt,canvas)=>{
        
        let x=Math.ceil(evt.x);
        let y=Math.ceil(evt.y);
        let w=Math.ceil(evt.w);
        let h=Math.ceil(evt.h);


        let new_locatingBlocks=[...locatingBlocks,{x,y,w,h}];

        set_insprule({
          ..._insprule,
          locatingBlocks:new_locatingBlocks
        })

        _this.canvasRenderInfo.locatingBlocks=new_locatingBlocks;
        canvas.draw();

      });
    }}>LocaBlock</Button>

    <Button onClick={()=>{

      let locatingBlocks=_insprule.locatingBlocks||[];


      _setCanvasHook({
        locatingBlocks,
        ctrl:CtrlHook_selectRegion,
        draw:[
          DrawHook_selectRegionDraw,
          (info,g,canvas_obj)=>{
          let ctx = g.ctx;
          let psize = 2*canvas_obj.rUtil.getPointSize();
          ctx.lineWidth = psize/4;

          ctx.setLineDash([]);
          let LineSize = canvas_obj.rUtil.getIndicationLineSize();
          
          info.locatingBlocks.forEach((region)=>{
            ctx.beginPath();
            
            ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
            
            if(info.sel_region!==undefined)
            {
              delete region._del;
              if(info.sel_region.x<region.x && info.sel_region.y<region.y &&
                info.sel_region.x+info.sel_region.w>region.x+region.w && 
                info.sel_region.y+info.sel_region.h>region.y+region.h)//check region includs 
              {
                
                ctx.strokeStyle = "rgba(200, 0, 0,0.5)";
                region._del=true;
              }
            }

            ctx.lineWidth = LineSize;
            ctx.rect(region.x, region.y, region.w, region.h);
            ctx.stroke();
            ctx.closePath();

          })
        }]
      })

      launchRegionSelect((evt,canvas)=>{
        let lcB=locatingBlocks;
        lcB=lcB.filter(blk=>blk._del===undefined)
        
        _this.canvasRenderInfo.locatingBlocks=lcB;
        set_insprule({
          ..._insprule,
          locatingBlocks:lcB
        })
        canvas.draw();



      });

    }}>SelRmBlock</Button>
    
    <Button onClick={()=>{
      runMatch(_insprule,defInfoPath);
    }}>match</Button>

    
    <Button onClick={()=>{
      runMatch(_insprule,defInfoPath);
    }}>match_refine</Button>
  </>;


}


function InspSet_Config({InspInfo,InspRefImg,ASYNC_WS_SEND_BPG,onInfoUpdate,defInfoPath})
{
  let _cur = useRef({
    canvasRenderInfo:{
    }
  });
  let _this=_cur.current;

  const [_inspinfo,set_inspInfo]=useState(InspInfo);

  const [MODE,setMODE]=useState("NORMAL");
  
  const [canComp,setCanComp]=useState(undefined);
  


  let launchRegionSelect=(onSelectComplete,drawRegionRect=true)=>
  {
    if(onSelectComplete===undefined)
    {
      canComp.ec_canvas.UserRegionSelect(undefined);
      return;
    }

      
    canComp.ec_canvas.UserRegionSelect((evt,canvas)=>{
      // console.log(evt)

      let x = evt.pt1.x;
      let y = evt.pt1.y;

      let w = evt.pt2.x-x;
      let h = evt.pt2.y-y;
      if(w<0)
      {
        x+=w;
        w=-w;
      }
      if(h<0)
      {
        y+=h;
        h=-h;
      }

      evt.x=x;
      evt.y=y;
      evt.w=w;
      evt.h=h;

      onSelectComplete(evt,canvas);
      // canComp.ec_canvas.drawHook=undefined;
    });




  }
  
  let launchPointSelect=(onSelectComplete)=>
  {
    if(onSelectComplete===undefined)
    {
      canComp.ec_canvas.UserRegionSelect(undefined);
      return;
    }

    {
      _this.canvasRenderInfo.pointSelect={draw:(info,g,canvas_obj)=>{
        let ctx = g.ctx;

        ctx.strokeStyle = 'red';
        ctx.lineWidth = 5;
        if(canvas_obj.regionSelect.pcvst2===undefined)
        {
          return;
        }
        let pt2 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);

        ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

        let psize = 2*canvas_obj.rUtil.getPointSize();
        
        ctx.lineWidth = psize/3;
        canvas_obj.rUtil.drawcross(ctx, pt2,psize);
      }}
    }
    canComp.ec_canvas.UserRegionSelect((evt)=>{
      // console.log(evt)
      onSelectComplete(evt);
      delete _this.canvasRenderInfo.pointSelect;
    });
    canComp.draw();

  }




  useEffect(() => {
    if(canComp===undefined)return;
    
     
    canComp.ec_canvas.EmitEvent=(event)=>{
  
    };

    canComp.ec_canvas.drawHook=(ctrl_or_draw,g,canvas_obj)=>{
      if(ctrl_or_draw==true)
      {
        Object.keys(_this.canvasRenderInfo).forEach(id=>{
          let idInfo=_this.canvasRenderInfo[id];
          if(idInfo.ctrl===undefined)return;
          if(Array.isArray(idInfo.ctrl))
          {
            idInfo.ctrl.forEach(ctrlh=>ctrlh(idInfo,g,canvas_obj))
          }
          else
            idInfo.ctrl(idInfo,g,canvas_obj);
        })
        return;
      }

      Object.keys(_this.canvasRenderInfo).forEach(id=>{
        let idInfo=_this.canvasRenderInfo[id];
        if(idInfo.draw===undefined)return;
        
        if(Array.isArray(idInfo.draw))
        {
          idInfo.draw.forEach(drawh=>drawh(idInfo,g,canvas_obj))
        }
        else
          idInfo.draw(idInfo,g,canvas_obj);
      })
    }
    canComp.ec_canvas.SetIM(InspRefImg);
    canComp.ec_canvas.scaleImageToFitScreen(InspRefImg)

  }, [canComp])


  if(canComp!==undefined)
  {
    canComp.draw();
  }


  function updateRule(newRule)
  {
    let rules=_inspinfo.inspRules||[];
    
    rules=[...rules];

    let idx=rules.findIndex(rule=>rule.id==newRule.id);
    if(idx==-1)
    {
      rules.push(newRule);
    }
    else
    {
      rules[idx]=newRule;
    }
    set_inspInfo({..._inspinfo,inspRules:rules});
  }


  let EditUI;
  
  switch(MODE)
  {

    case "NORMAL":
      
      EditUI=(
        <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
          
          <Button onClick={()=>{
            onInfoUpdate(_inspinfo)
          }}>X</Button>
          <Button onClick={()=>{
            setMODE("POSE_EST")
          }}>{"POSE_EST"}</Button>
        </div>);

      
      break;

    case "POSE_EST":
      let rules=_inspinfo.inspRules||[];
      let posi_rule=rules.find(rule=>rule.type="pose_est")||{
        type:"pose_est",
        id:30,
        ROI:{x:0,y:0,w:100,h:100}
      }
    
      EditUI=(
        <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      
          <InspRule_positioning_Config ASYNC_WS_SEND_BPG={ASYNC_WS_SEND_BPG} launchRegionSelect={launchRegionSelect} defInfoPath={defInfoPath}
            insprule={posi_rule}
            InspRefImg={InspRefImg}
            onRuleUpdate={(new_rule)=>{
              console.log(new_rule);

              updateRule(new_rule);
              setMODE("NORMAL")
            }}

            setCanvasHook={(hook)=>{
              if(hook===undefined)
              {
                delete _this.canvasRenderInfo.inspRule_positioning_Config;
              }
              else 
                _this.canvasRenderInfo.inspRule_positioning_Config=hook;
              
              console.log(hook);
              canComp.draw();
            }}
            />
        </div>);
      break;



  }




  return  <div className="overlayCon HXF">
    <CanvasComponent key="kk" addClass="height12" onCanvasInit={setCanComp}/>
    {EditUI}
  </div>;
}





function GenMatching_rdx({onExtraCtrlUpdate,defInfoPath="data/TMP/"})
{
  let _cur = useRef({
    bk_cam1_conn_info:undefined
  });
  let _this=_cur.current;

  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const [MODE,setMODE] = useState("NORMAL");
  // const CAM1_ID_CONN_INFO = useSelector(state =>state.ConnInfo.CAM1_ID_CONN_INFO);

  const CAM1_ID_CONN_INFO = useSelector(state =>{
    let sCAM1_ID_CONN_INFO=state.ConnInfo.CAM1_ID_CONN_INFO
    // console.log(sCAM1_ID_CONN_INFO,_this.bk_cam1_conn_info);
    if(GetObjElement(sCAM1_ID_CONN_INFO,["data",0,"name"])!=GetObjElement(_this.bk_cam1_conn_info,["data",0,"name"]))
    {
      _this.bk_cam1_conn_info=sCAM1_ID_CONN_INFO;
    }

    return _this.bk_cam1_conn_info;
  });
  console.log(CAM1_ID_CONN_INFO);



  const dispatch = useDispatch();
  const ACT_WS_SEND_BPG= (...args) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, ...args));
  const ASYNC_WS_SEND_BPG=(tl,prop,data,uintArr)=> new Promise((resolve, reject) => {
    
    ACT_WS_SEND_BPG(tl,prop,data,uintArr, { resolve:(pkts)=>{
      if(pkts.length==1)
      {
        let data0 = pkts[0].data;
        if(data0.ACK!=true)
        {
          reject(pkts)
        }
      }

      resolve(pkts)
    }, reject });
    setTimeout(() => reject("Timeout"), 5000)
  });

  const [confState,setConfState]=useState(configState.shape_positioning);



  const [_defInfo_,set_defInfo_]=useState();
  const [_imgSet_,set_imgSet_]=useState();


  async function LoadDefInfo(path)
  {
    console.log(path);
    let pkts=await ASYNC_WS_SEND_BPG("LD", 0, {
      filename: path+"/info" + '.' + GEN_DEF_EXT,
      down_samp_level:3
    });
    
    
    let FL = pkts.find(pkt=>pkt.type=="FL");
    let info=GetObjElement(FL,["data"]);

    if(info===undefined)
    {
      throw {
        why: "Info File is empty"
      }
    }
    var result = genDef_validator(info);


    console.log(result);
    console.log('Errors: ', genDef_validator.errors);

    if(result==false)
    {
      throw {
        why:"def info is not valid",
        info:genDef_validator.errors
      }
    }


    let imageSet=await Promise.all(info.inspectionSet.map( insp => ASYNC_WS_SEND_BPG("LD", 0, {
        imgsrc: path+"/"+insp.cameraInfo.refrenceImage,
        down_samp_level:3
      }).
      then(pkts=>{
        let IM = pkts.find(pkt=>pkt.type=="IM");
        if(IM===undefined)throw "Image not found"
        IM.cameraInfo=insp.cameraInfo;
        IM.img = new ImageData(IM.image, IM.width);
        return IM;
      })
    ));


    console.log(imageSet);

    return {
      def:info,
      imageSet
    };
  }

  
  async function SaveReport(path,defFile,imgset)
  {
    let img1=imgset[0];
    let defFile_cam_info=defFile.inspectionSet[0].cameraInfo;
    await ASYNC_WS_SEND_BPG("SV", 0, {
      make_dir:true,
      filename:path+"/"+defFile_cam_info.refrenceImage,
      type: "__CACHE_IMG__"
    });

    await ASYNC_WS_SEND_BPG("SV", 0, {
      make_dir:true,
      filename:path+"/info" + '.' + GEN_DEF_EXT,
    },
    enc.encode(JSON.stringify(defFile, null, 2))
    );

    
  }

  async function constructDefInfo(cameraSet)
  {
    let cam=cameraSet[0];


    
    let cameraInfo={
      "name": cam.data[0].name,
      "id": cam.id,
      "serial_number": cam.id,
      "refrenceImage":cam.data[0].name+"_IMG",
      "mmpp":cam.data[0].mmpp
    };

    let info={
      "type": "FM_GenMatching",
      "version": "0.0.1",
      "inspectionSet": [
        {
          "cameraInfo":cameraInfo
        }
      ]
    }
    

    
    let result=genDef_validator(info);
    console.log(result,genDef_validator.errors);
    if(result ==false)
    {
      throw {
        why:"info construct failed",
        info:genDef_validator.errors
      }
    }
    
    let pkts=await ASYNC_WS_SEND_BPG("II", 0, {
      imgsrc:undefined,
      camera_id:cam.id,
      definfo:{
        type:"FM_GenMatching",
        insp_type:"NOP",
      },
      img_property:{
        down_samp_level:3
      },
    });
    let IM = pkts.find(pkt=>pkt.type=="IM");
    if(IM===undefined)throw "Image not found"
    

    IM.cameraInfo=cameraInfo;
    
    IM.img = new ImageData(IM.image, IM.width);
    let imageSet=[IM]

    console.log(pkts);



    // if(true)
    // {    
    //   await SaveReport(defInfoPath,info,[IM])


    // }

    
    return {
      def:info,
      imageSet
    };

  }


  

  // useEffect(() => {
  //   if(typeof onExtraCtrlUpdate === "function")
  //     onExtraCtrlUpdate({
  //       save:MODE!="NORMAL"?undefined:()=>
  //       {
  //         SaveReport(defInfoPath,_defInfo_,_imgSet_).
  //         then(()=>{
  //           console.log("SAVE OK!!!");
  //         }).
  //         catch((err)=>{
  //           console.log("ERR....",err);
  //         })
  //       }
  //     });
  //   return () => {
  //   };
  

  // }, [_defInfo_,_imgSet_,MODE])




  // useEffect(() => {    

  //   console.log(">>>>>>>>>>>>>>>>",_defInfo_);
  //   if(_defInfo_!==undefined)
  //   {
  //     return;
  //   }




  //   constructDefInfo([CAM1_ID_CONN_INFO]).then((info)=>{
  //     console.log(info);
      
  //     set_defInfo_(info.def)
  //     set_imgSet_(info.imageSet)
      
      
  //     // setSetUpCamID(info.imageSet[0].cameraInfo.id);

  //   }).catch((err)=>{
  //     console.log(err);
  //     // ConnInfo.CAM1_ID_CONN_INFO
  //     set_defInfo_(undefined)
  //     set_imgSet_(undefined)
  //   })



  // }, [CAM1_ID_CONN_INFO])




  switch(MODE)
  {
    case "NORMAL":

      return <>
      <Button onClick={()=>{
        setMODE("InfoEDIT")
      }}>InfoEDIT</Button>




      <Button onClick={()=>{
        

        constructDefInfo([CAM1_ID_CONN_INFO]).then((info)=>{
          console.log(info);
          
          set_defInfo_(info.def)
          set_imgSet_(info.imageSet)
          
          
          // setSetUpCamID(info.imageSet[0].cameraInfo.id);

        }).catch((err)=>{
          console.log(err);
          // ConnInfo.CAM1_ID_CONN_INFO
          set_defInfo_(undefined)
          set_imgSet_(undefined)
        })


      }}>NEW</Button>

      <Button onClick={()=>{


        LoadDefInfo(defInfoPath).
        then((info)=>{
          console.log(info);
          
          set_defInfo_(info.def)
          set_imgSet_(info.imageSet)


          
        }).catch(err=>{
          console.log(err)
        })



      }}>LOAD</Button>


      <Button onClick={()=>{

        SaveReport(defInfoPath,_defInfo_,_imgSet_).
        then(()=>{
          console.log("SAVE OK!!!");
        }).
        catch((err)=>{
          console.log("ERR....",err);
        })
      }}>SAVE</Button>




      </>
      break;
    case "InfoEDIT":

      if(_defInfo_===undefined || _imgSet_===undefined)
      {
        return "No def info";
      }
      let InspInfo=_defInfo_.inspectionSet[0]
      let InspRefImg=_imgSet_[0]
      
      return <InspSet_Config ASYNC_WS_SEND_BPG={ASYNC_WS_SEND_BPG} InspInfo={InspInfo} InspRefImg={InspRefImg} defInfoPath={defInfoPath}
      onInfoUpdate={(newInfo)=>{
        // _defInfo_.inspectionSet=[newInfo];
        set_defInfo_({..._defInfo_,inspectionSet:[newInfo]});
        setMODE("NORMAL")
      }}/>;


      break;
  }

  return NULL;
}


export default GenMatching_rdx;