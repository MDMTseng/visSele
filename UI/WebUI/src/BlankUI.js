'use strict'


import { connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from 'UTIL/MathTools';

import { Machine } from 'xstate';

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



const LOCALSTORAGE_KEY="CABLE.RecentDefFiles"


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



class CableWireDef_CanvasComponent extends EC_CANVAS_Ctrl.EverCheckCanvasComponent_proto {

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



      this.regionSelect.onSelect(this.regionSelect);
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
    this.regionSelect={onSelect};
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
      this.drawHook(this.g,this);
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
    this.ec_canvas = new CableWireDef_CanvasComponent(this.refs.canvas);

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

  update(IM,RP,UI_Info) {

    if (this.ec_canvas !== undefined) {
      {
        this.ec_canvas.SetIM(IM);
        this.ec_canvas.SetRP(RP);
        this.ec_canvas.SetUI_Info(UI_Info);
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


const CABLE_DEF_EXT="cabDef";



function Blank_rdx({onExtraCtrlUpdate})
{
  let _cur = useRef({
  });
  let _this=_cur.current;

  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  
  const dispatch = useDispatch();
  const ACT_WS_SEND_BPG= (...args) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, ...args));

  const [fileSavingContext,setFileSavingContext]=useState(undefined);
  const [fileBrowsingContext,setFileBrowsingContext]=useState(undefined);

  const [defPath,setDefPath]=useState("data/default."+CABLE_DEF_EXT);

  const [canComp,setCanComp]=useState(undefined);

  const [_defInfo_,set_defInfo_]=useState({
    type:"FM_GenMatching",
  });

  const [editRegionInfo,setEditRegionInfo]=useState(undefined);


  function Reset()
  {
    setFileSavingContext(undefined);
    setFileBrowsingContext(undefined);
    set_defInfo_({
      type:"FM_GenMatching",
    });
    if(canComp!==undefined)
      canComp.reset();
  }

  function fetchParam()
  {
    ACT_WS_SEND_BPG( "ST", 0,
      { 
        InspectionParam:[{
          get_param:true
        }]
      },undefined, { 
        resolve:(pkts)=>{
          let DT=pkts.find(pkt=>pkt.type=="DT");
          console.log("-------DT",DT,"   pkts:",pkts);
          if(DT!==undefined && DT.data!==undefined&& DT.data[0]!==undefined)
          {
            set_defInfo_({..._defInfo_,
              ...DT.data[0]
            });
          }
        }, 
        reject:()=>{} 
      })
  }
  function inspStage(inspectionStage)
  {
    ACT_WS_SEND_BPG( "ST", 0,
      { 
        InspectionParam:[{
          inspectionStage
        }]
      },undefined, { 
        resolve:(pkts)=>{}, 
        reject:()=>{} 
      })
  }

  function TriggerNewResult(doTakeNew,add_defInfo,cur_defInfo=_defInfo_)
  {

    //let defInfo= {...cur_defInfo,...add_defccInfo};
    console.log(add_defInfo);
    let defInfo={...cur_defInfo,...add_defInfo}


    // console.log(defInfo);
    ACT_WS_SEND_BPG("II", 0, {
      imgsrc:(doTakeNew==false)?"__CACHE_IMG__":undefined,
      img_property:{
        down_samp_level:2,
        scale:3
      },
      definfo:defInfo
    }, undefined,
    { resolve:(pkts)=>{
      log.info(pkts);
      let RP = pkts.find(pkt=>pkt.type=="RP");
      let IM = pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM!==undefined)
      {
        _this.RP=RP;

        
        IM.img = new ImageData(IM.image, IM.width);
        _this.IM=IM;
        
        canComp.update(_this.IM,_this.RP,{});
      }

    }, reject:(pkts)=>{
      log.info(pkts);
    }});


    if(doTakeNew==true)
    {
      fetchParam();
    }
  }


  useEffect(() => {
    return () => {
    };
  },[])

  useEffect(() => {
    if(canComp===undefined)return;
    
     
    canComp.ec_canvas.EmitEvent=(event)=>{
  
      console.log(event);
      switch (event.type) {
        case "onRegionInfoUpdate":
          
          if(event.data==undefined )
          {
            setEditRegionInfo(undefined);
          }
          else
          {
            console.log(event)
            setEditRegionInfo(event.data.region);
          }
          break;


        case "onUserRegionSelect":
          
          console.log(event)
          break;


      }
    };
    
    TriggerNewResult();


  }, [canComp])



  if(canComp!==undefined)
  {
    canComp.draw();
  }


  // console.log(editRegionInfo);
  return <div className="overlayCon HXF">
    <CanvasComponent key="kk" addClass="height12" 
    onCanvasInit={setCanComp}/>
    <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      <Button onClick={()=>{
        let i=0;
        let sec1=10;
        for(i=0;i<sec1;i++)
        {
          let ix=i;
          setTimeout(()=>{
            console.log(ix);
            TriggerNewResult(false,{
              thres:ix*255/(sec1-1)
            });
          },ix*100);
        }
      }}>AAA</Button>

      <Button onClick={()=>{
        canComp.ec_canvas.drawHook=(g,canvas_obj)=>{
          let ctx = g.ctx;

          ctx.strokeStyle = 'red';
          ctx.lineWidth = 5;
          if(canvas_obj.regionSelect.pcvst1===undefined)
          {
            return;
          }
          let pt1 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);

          // console.log(pt1,pt2x)
          
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
          ctx.beginPath();
          let LineSize = canvas_obj.rUtil.getIndicationLineSize();
          ctx.setLineDash([LineSize*10,LineSize*3,LineSize*3,LineSize*3]);
          ctx.strokeStyle = "rgba(151, 51, 51,30)";
          ctx.lineWidth = LineSize*2;
          ctx.rect(x, y, w, h);
          ctx.stroke();
          ctx.closePath();


        }
        canComp.ec_canvas.UserRegionSelect((evt)=>{
          console.log(evt)
          
          canComp.ec_canvas.drawHook=undefined;
        });
        canComp.draw();
      }}>BBB</Button>
    </div>
  </div>;
}


export default Blank_rdx;