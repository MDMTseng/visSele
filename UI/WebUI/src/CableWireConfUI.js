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
  ExpCalcBasic, ExpValidationBasic,defFileGeneration,xstate_GetCurrentMainState
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
  DownOutlined, TrophyOutlined

} from '@ant-design/icons';




const states ={

  neutral:"neutral",

  head_locating:"head_locating",
  head_locating_detail:"head_locating_detail",
  head_locating_simple:"head_locating_simple",

  cable_region_setup:"cable_region_setup",
  cable_region_color_samping:"cable_region_color_samping",
  
}


const actions ={
  ACT_neutral:"ACT_neutral",
  ACT_head_locating:"ACT_head_locating",
  ACT_cable_region_setup:"ACT_cable_region_setup",



  ACT_region_Add:"ACT_region_Add",
  ACT_region_pix_ref_Add:"ACT_region_pix_ref_Add",
  ACT_region_BACK:"ACT_region_BACK"
}


const states_region ={

  neutral:"neutral",
  region_Add:"region_Add",
  pix_ref_Add:"pix_ref_Add",

  
}

const regionSStates = {
  initial: states_region.neutral,
  states: {
    [states_region.neutral]: {
      on: {
        [actions.ACT_region_Add]:states_region.region_Add,
        [actions.ACT_region_pix_ref_Add]: states_region.pix_ref_Add,
      }
    },
    [states_region.region_Add]: {
      on: {
        [actions.ACT_region_BACK]: states_region.neutral,
        [actions.ACT_region_pix_ref_Add]:states_region.pix_ref_Add,
      }
    },
    [states_region.pix_ref_Add]: {
      on: {
        [actions.ACT_region_Add]: states_region.region_Add,
        [actions.ACT_region_BACK]: states_region.neutral,
      }
    },
  }
};

 
const CableSetupState = ({
  id: 'CableSetupState',
  initial:states.neutral,
  states: {
    [states.neutral]: {
      on: {
        // [actions.ACT_neutral]: states.neutral,
        [actions.ACT_head_locating]: states.head_locating,
        [actions.ACT_cable_region_setup]: states.cable_region_setup
      }
    },
    [states.head_locating]: {
      on: {
        [actions.ACT_neutral]: states.neutral,
        // [actions.ACT_head_locating]: states.head_locating,
        [actions.ACT_cable_region_setup]: states.cable_region_setup,
      }
    },
    [states.cable_region_setup]: {
      on: {
        [actions.ACT_neutral]: states.neutral,
        [actions.ACT_head_locating]: states.head_locating,
        // [actions.ACT_cable_region_setup]: states.cable_region_setup,
      },
      ...regionSStates
    }
  }
});


class CableWireDef_CanvasComponent extends EC_CANVAS_Ctrl.EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.reset();
    this.EmitEvent = (event) => { log.info(event); };

  }

  state_transistion(action,data)
  {

    let testState = this.sm.transition("neutral","ACT_head_locating");
    console.log(">>>>>>>>>",testState);


    this.stateData=data;

    console.log(this.c_state.value,"  :::    ",action);
    let newState = this.sm.transition(this.c_state.value,action);
    
    // let state_changed = JSON.stringify(this.c_state.value)!==JSON.stringify(newState.value);
    this.p_state=this.c_state;
    this.c_state=newState;
    console.log(this.p_state.value,">[",action,"]>", this.c_state.value);
  }
  reset()
  {
    this.sm= Machine(CableSetupState);


    const nextState = this.sm.transition("neutral","ACT_head_locating").value;

    console.log(CableSetupState,">>>>>>>>>",nextState);


    this.c_state=this.sm.initialState;

    this.ERROR_LOCK = false;
    let mmpp=1;
    this.db_obj = {
      cameraParam:{
        mmpb2b:1,
        ppb2b:1
      }
    };
    this.db_obj.cameraParam.mmpb2b=mmpp*this.db_obj.cameraParam.ppb2b;
    this.rUtil.renderParam.mmpp = mmpp;
    this.mouse_close_dist = 10;

    this.colorSet =
      Object.assign(this.colorSet,
        {
          inspection_Pass: "rgba(0,255,0,0.1)",
          inspection_production_Fail: "rgba(128,128,0,0.3)",
          inspection_Fail: "rgba(255,0,0,0.1)",
          inspection_UNSET: "rgba(128,128,128,0.1)",
          inspection_NA: "rgba(64,64,64,0.1)",


          color_NA: "rgba(128,128,128,0.5)",

          color_UNSET: "rgba(128,128,128,0.5)",
          color_SUCCESS: this.colorSet.measure_info,
          color_FAILURE_opt: {
            submargin1: "rgba(255,255,0,0.5)",
          },
          color_FAILURE: "rgba(255,0,0,0.5)",
        }
      );


    this.cableRegionInfo={
      regions:[]
    };


    this.cur_editRegion=undefined;

    this.EditInfo=undefined;
    this.objId=1;
    
  }

  onmouseup(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 0;
    this.camera.EndDrag();

    this.debounce_zoom_emit();
    this.ctrlLogic();
    this.draw();
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
      this.camera.Scale(0.5*this.canvas.width/width/curScale);
    }

    
  }

  onmousedown(evt) 
  {

    super.onmousedown(evt);
    // this.doDragging=false;

  }

  onmouseup(evt) 
  {

    super.onmouseup(evt);
    
  }





  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;


    let doDragging = false;
    doDragging=true;
    

    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });

      }

    }
    this.ctrlLogic();
    this.draw();
  }

  SetImg(img_info) {
    if(img_info==null)return;
    super.SetImg(img_info);
    console.log(img_info);
    // if(this.IGNORE_IMAGE_FIT_TO_SCREEN!=true)
    //   this.scaleImageToFitScreen();
    // this.IGNORE_IMAGE_FIT_TO_SCREEN=true;
  }

  SetIM(info_IM) {
    super.SetImg(info_IM); 
  }
  SetRP(info_RP) {
    let sr=GetObjElement(info_RP,["data","reports",0]);
    console.log(sr);
    if(sr!==undefined)
    {

      this.singleReportInfo={
        report:{...sr},
        center:{
          x:sr.pt_cornor.x+(sr.vec_btm.x)/2,
          y:sr.pt_cornor.y+(sr.vec_btm.y)/2,
        },
        rotate:Math.atan2(sr.vec_btm.y,sr.vec_btm.x)

      }

      
      this.cableRegionInfo.anchorInfo=sr;
      // let width=Math.hypot(sr.vec_btm.x,sr.vec_btm.y)
      // console.log(modeInfo,this.singleReportInfo);
      // // this.camera.Rotate(-this.singleReportInfo.angle);
      // this.camera.SetOffset(this.canvas.width/2,this.canvas.height/2);

      
      // let curScale = this.camera.GetCameraScale();
      // this.camera.Scale(0.5*this.canvas.width/width/curScale);
    }
    else
    {
      this.singleReportInfo=undefined;
    }
  }

  SetCableRegionInfo(_cableRegionInfo)//rewrite region info directly
  {
    this.objId = _cableRegionInfo.regions.reduce((max_id,regi)=>regi.id>max_id?regi.id:max_id,0)+1;
    this.cableRegionInfo=dclone(_cableRegionInfo);

  }

  setRegion(id,rinfo){
    if(rinfo===undefined)
    {
      if(id===undefined)return;
      //for deletion
      this.cableRegionInfo.regions = 
        this.cableRegionInfo.regions.filter(regi=>regi.id!==id);
      return;
    }
    if(id===undefined)
    {
      id=this.objId;
      this.objId++;
    }
    let d_rinfo=dclone(rinfo);
    d_rinfo.id=id;
    let idx = undefined;

    if(id!==undefined)
    {
      idx = this.cableRegionInfo.regions.findIndex(regi=>regi.id===id);
    }

    if(idx!==undefined)
    {
      this.cableRegionInfo.regions[idx]=d_rinfo;
    }
    else
    {
      this.cableRegionInfo.regions.push(d_rinfo);
    }
    
  }
  draw() {
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
      
    {//TODO:HACK: 4X4 times scale down for transmission speed


      let mmpp = this.rUtil.get_mmpp();

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;

      
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      // let center = this.db_obj.getsig360infoCenter();
      
      if(xState(this.c_state).state===states.cable_region_setup)
      {

        if(this.singleReportInfo!==undefined)
        {
          ctx.rotate(-this.singleReportInfo.rotate);
          ctx.translate(-this.singleReportInfo.center.x, -this.singleReportInfo.center.y);
          // ctx.Rotate
        }
  
      }

      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX / scale -0.5), (this.img_info.offsetY) / scale -0.5);
      }

      
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      ctx.drawImage(this.secCanvas, 0, 0);
      ctx.restore();
    }
    this.rUtil.drawpoint(ctx,{x:0,y:0});


    // console.log(this.modeInfo);
    
    switch(xState(this.c_state).state) 
    {
      case states.cable_region_setup:
        if(this.closestPtInfo!==undefined)
        {
          let reg = this.closestPtInfo.region;

          let id=reg.id;
          reg=this.cableRegionInfo.regions.find(regi=>regi.id==id);
          if(reg!==undefined)
          {
            ctx.lineWidth = reg.margin;
            ctx.strokeStyle = "rgba(100,0,00,0.5)";
            this.rUtil.drawHollowLine(ctx, reg)
            ctx.lineWidth = reg.margin;
            ctx.strokeStyle = "rgba(200,200,200,0.5)";
            this.rUtil.drawHollowLine(ctx, reg,3)
    
            this.rUtil.drawpoint(ctx,reg.pt1);
            this.rUtil.drawpoint(ctx,reg.pt2);
          }
        }

        else{

          this.cableRegionInfo.regions.forEach(reg=>{

            ctx.lineWidth = reg.margin;
            ctx.strokeStyle = "rgba(0,0,100,0.5)";
            this.rUtil.drawHollowLine(ctx, reg)
            ctx.lineWidth = reg.margin;
            ctx.strokeStyle = "rgba(200,200,200,0.5)";
            this.rUtil.drawHollowLine(ctx, reg,3)

            this.rUtil.drawpoint(ctx,reg.pt1);
            this.rUtil.drawpoint(ctx,reg.pt2);
  
          })

        }

        if(this.tmp_closestPtInfo!==undefined)
        {
          this.rUtil.drawpoint(ctx,this.tmp_closestPtInfo.pt,undefined,this.rUtil.getPointSize()*1.4);
        }
        break;
    }
  }


  isAvailable(state)
  {
    console.log("state:",state);
    if(state==states.neutral)
    {
      return true;
    }

    // console.log("this.img_info:",this.img_info);
    if(state==states.head_locating || 
      state==states.head_locating_detail || 
      state==states.head_locating_simple
      )
    {
      return this.img_info!==undefined;
    }

    if(state==states.cable_region_setup || 
      state==states.cable_region_color_samping 
      )
    {
      
      // return this.img_info!==undefined;
      return this.singleReportInfo!==undefined;
    }

    return false;
  }

  FindClosestPointInfo(location, regionList) {
    let pt_info = {
      pt: undefined,
      keyPath: undefined,
      dist: Number.POSITIVE_INFINITY,
      region:undefined
    };
    regionList.forEach((regi,idx) => {

      let key="pt1";
      let tmpDist  = distance_point_point(regi.pt1, location);
      let tmpDist2 = distance_point_point(regi.pt2, location);

      if(tmpDist>tmpDist2)
      {
        tmpDist=tmpDist2;
        key="pt2";
      }



      if (pt_info.dist > tmpDist) {
        pt_info.pt = regi[key];
        pt_info.region = regi;
        pt_info.keyPath = [idx,key];
        pt_info.dist = tmpDist;
      }

    });

    return pt_info;
  }

  fetchImagePixRGBA(context,x,y,NxN=5)
  {
    let offset = (NxN-1)/2;
    var pixes = context.getImageData(x-offset, y-offset, NxN, NxN).data;

    let RGBA=[0,0,0,0];
    // Loop over each pixel and invert the color.
    for (var i = 0, n = pixes.length; i < n; i += 4) {
      RGBA[0]+=pix[i+0];
      RGBA[1]+=pix[i+1];
      RGBA[2]+=pix[i+2];
      RGBA[3]+=pix[i+3];
    }
    RGBA[0]/=pixes.length;
    RGBA[1]/=pixes.length;
    RGBA[2]/=pixes.length;
    RGBA[3]/=pixes.length;
    return RGBA;
  }

  ctrlLogic() {
    
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);
    
    if(ifOnMouseLeftClickEdge )
    {
      if(this.mouseStatus.status==1)
      {
        // this.mouseOnCanvas={...mouseOnCanvas2};
      }
    }

    // let enableAddNewRegion=false;

    switch(xState(this.c_state).state) 
    {
      case states.cable_region_setup:
        
        // this.EditInfo

        



        if(this.mouseStatus.status==1)
        { 

          if(ifOnMouseLeftClickEdge)
          {
            
            {
              
              let editRegi;
              let closestPtInfo = this.tmp_closestPtInfo;
              // console.log("closestPtInfo>>",closestPtInfo);
              if(closestPtInfo!==undefined && closestPtInfo.dist>20)
              {
                closestPtInfo=undefined;
              }
              this.closestPtInfo = closestPtInfo;
  
  
  
              //If there is no target region create one
              // if(closestPtInfo===undefined && this.doAddNewRegion)
              if(dthis.doAddNewRegion)
              {
                let newRegion = {
                  pt1:{...mouseOnCanvas2},
                  pt2:{...mouseOnCanvas2},
                  margin:30,
                  id:this.objId
                };
                this.objId++;
                this.cableRegionInfo.regions.push(newRegion);
    
    
                let pt_info = {
                  pt: newRegion.pt2,
                  keyPath: [this.cableRegionInfo.regions.length-1,"pt2"],
                  region:newRegion,
                  dist: 0
                };
                this.closestPtInfo = pt_info;
  
              }
              
            }
          }
          else if(this.closestPtInfo!==undefined)
          {
            this.closestPtInfo.pt.x=mouseOnCanvas2.x;
            this.closestPtInfo.pt.y=mouseOnCanvas2.y;
          }
        }
        else{
          
          if(ifOnMouseLeftClickEdge)
          {

            console.log(this.closestPtInfo);
            if(this.closestPtInfo!==undefined)
            {
              console.log(">>>>onRegionInfoUpdate");
              this.EmitEvent({
                type:"onRegionInfoUpdate",
                data:{...this.closestPtInfo,
                  region:this.cableRegionInfo.regions[this.closestPtInfo.keyPath[0]],
                  regionList:this.cableRegionInfo}
              })
              sthis.doAddNewRegion=false;
            }
            else
            {
              this.EmitEvent({
                type:"onRegionInfoUpdate",
                data:undefined
              })
            }
          }
          else
          {

            //just to display the closest point
            let closestPtInfo = this.FindClosestPointInfo(mouseOnCanvas2, this.cableRegionInfo.regions);
            // this.closestPtInfo=undefined;
            if(closestPtInfo.dist<20)
            {
              this.tmp_closestPtInfo=closestPtInfo;
            }
            else
            {
              this.tmp_closestPtInfo=undefined;
            }
            // console.log(x,"<<<");
          }

        }
        break;
    }
    
    this.mouseStatus.pstatus = this.mouseStatus.status;
  }
}







class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  triggerROISelect()
  {

  }

  

  componentDidUpdate(prevProps) {
  }

  componentDidMount() {
    this.ec_canvas = new CableWireDef_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent =()=>{};

    if(this.props.onCanvasInit!==undefined)
      this.props.onCanvasInit(this.ec_canvas);

    this.onResize(500, 500)
    //this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  updateCanvas(ec_state, props = this.props) {

    if (this.ec_canvas !== undefined && props.IM!==undefined) {
      {
        // console.log(props.IM);
        this.ec_canvas.SetImg(props.IM);
        // this.ec_canvas.SetOPModeInfo(props.modeInfo);
        this.ec_canvas.draw();
      }
    }
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
    // this.ec_canvas.EmitEvent=nextProps.onCanvasEvent;
    // this.updateCanvas(nextProps.c_state, nextProps);
    // console.log(">>>");
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

function debounce(func, delay=250) {
  let timer = null;
 
  return () => {
    let context = this;
    let args = arguments;
 
    clearTimeout(timer);
    timer = setTimeout(() => {
      func.apply(context, args);
    }, delay)
  }
}


function RegionEditor({regionInfo,onRegionInfoUpdate,onRegionRefPixSet})
{

  console.log(regionInfo);
  const [_regionInfo,set_regionInfo]=useState();
  useEffect(() => {
    console.log(regionInfo);
    set_regionInfo(dclone(regionInfo));
  },[regionInfo]);


  // const menu =<Menu>{ 
  //   Object.keys(CableWireDef_regionType).map(key=>
  //     <Menu.Item onClick={()=>{
  //       let newInfo={..._regionInfo,type:CableWireDef_regionType[key]};
  //       set_regionInfo(newInfo);
  //       onRegionInfoUpdate(newInfo.id,newInfo);
  //     }}>
  //       {key}
  //     </Menu.Item>)}
  //   </Menu>;
  if(_regionInfo===undefined)return null;
  return <>
    {/* <Dropdown overlay={menu}>
      <a className="ant-dropdown-link" onClick={e =>{

      }}>
       種類 <DownOutlined />
      </a>
    </Dropdown> */}
    
    <Divider orientation="left" plain>
      偵測寬度
    </Divider>
    <Slider key={"margin_slider"}
        min={20}
        max={150}
        onChange={(value) => {
          let newInfo={..._regionInfo,margin:value};
          set_regionInfo(newInfo);
          onRegionInfoUpdate(newInfo.id,newInfo);
        }}
        value={_regionInfo.margin}
        step={1}
      />
    
    <Button type="primary" shape="round" icon={<CloseOutlined />} danger
      onClick={()=>{
        set_regionInfo(undefined);
        onRegionInfoUpdate(_regionInfo.id,undefined);
      }}>
      刪除
    </Button>


    <Button shape="round" icon={<CloseOutlined />}
      onClick={()=>{
        onRegionRefPixSet();
      }}>
      增加點
    </Button>
     
  </>
}

function CABLE_WIRE_CONF_MODE_rdx({onExtraCtrlUpdate})
{
  let _cur = useRef({});
  let _this=_cur.current;
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  
  const dispatch = useDispatch();
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(WS_ID, ...args));
  
  const [CurIM,setCurIM]=useState(undefined);
  const [CurRP,setCurRP]=useState(undefined);
  const [fileSavingContext,setFileSavingContext]=useState(undefined);
  const [fileBrowsingContext,setFileBrowsingContext]=useState(undefined);

  const [defPath,setDefPath]=useState("data/default."+CABLE_DEF_EXT);

  const [ecCanvas,setECCanvas]=useState(undefined);

  const [_defInfo_,set_defInfo_]=useState({
    type:"gen",
  });

  const [forceUpdate,setForceUpdate]=useState(0);


  const [editRegionInfo,setEditRegionInfo]=useState(undefined);

  const [menuKey,setMenuKey]=useState([]);

  function Reset()
  {
    setCurIM(undefined);
    setCurRP(undefined);
    setFileSavingContext(undefined);
    setFileBrowsingContext(undefined);
    set_defInfo_({
      type:"gen",
    });
    ecCanvas.reset();
  }

  function fetchParam()
  {
    ACT_WS_SEND( "ST", 0,
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
  function TriggerNewResult(doTakeNew,add_defInfo,cur_defInfo=_defInfo_)
  {

    //let defInfo= {...cur_defInfo,...add_defccInfo};

    let defInfo={...cur_defInfo,regionInfo:ecCanvas.cableRegionInfo,...add_defInfo}
    // ACT_WS_SEND("ST", 0, {
    //   CameraTriggerShutter:true
    // }, undefined);

    console.log(defInfo);
    ACT_WS_SEND("II", 0, {
      imgsrc:(doTakeNew==false)?"__CACHE_IMG__":undefined,
      img_property:{
        down_samp_level:2,
        scale:3,
        calibInfo:{
          type:"NA"
        }
      },
      definfo:defInfo
    }, undefined,
    { resolve:(pkts)=>{
      log.info(pkts);
      let RP = pkts.find(pkt=>pkt.type=="RP");
      let IM = pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM!==undefined)
      {

        setCurRP(RP);
        let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
        setCurIM(img_pros.data);
        // setInterval(()=>{
        //   TriggerNewResult();
        // },10000)
      }

    }, reject:(pkts)=>{
      log.info(pkts);
    }});


    if(doTakeNew==true)
    {
      fetchParam();
    }
  }

  function InpectionAgain(inspectionStage=1,defInfo)
  {
    _this.curStage=inspectionStage;
    TriggerNewResult(false,{inspectionStage},defInfo);
  }
  function TakeNew(inspectionStage=1,defInfo)
  {
    _this.curStage=inspectionStage;
    TriggerNewResult(true,{inspectionStage},defInfo);
  }

  useEffect(() => {

    if(onExtraCtrlUpdate!==undefined)
      onExtraCtrlUpdate({
        save:(path)=>{
          setFileSavingContext({
            onOK:(folderInfo, fileName, existed) => {
              let fileNamePath = folderInfo.path + "/" + fileName.replace("."+CABLE_DEF_EXT, "");
              setFileSavingContext(undefined);
              var enc = new TextEncoder();


              let combDefFile={..._defInfo_,regionInfo:ecCanvas.cableRegionInfo}
              console.log(_defInfo_);

              set_defInfo_(combDefFile);


              ACT_BIN_Save(fileNamePath+"."+CABLE_DEF_EXT, enc.encode(JSON.stringify(combDefFile, null, 2)));
              console.log("ACT_Cache_Img_Save");
              ACT_Cache_Img_Save(fileNamePath+".png");
  
            },
            path:"data/",
            defaultName:"default."+CABLE_DEF_EXT
          });
        },
        open:(path)=>{

          Reset();
          setFileBrowsingContext({

            onFileSelected:(file_path,file) => {
              Reset();
              // console.log(">>",file_path,file);
  
              let defModelPath = file_path.replace("."+CABLE_DEF_EXT, "");
  
              ACT_WS_SEND("LD", 0,
                {
                  deffile: defModelPath + '.' + CABLE_DEF_EXT,
                  imgsrc: defModelPath,
                  down_samp_level:10
                },
                undefined, { 
                resolve:(pkts)=>{
                  let DF = pkts.find(pkt=>pkt.type=="DF");
                  let IM = pkts.find(pkt=>pkt.type=="IM");
                  if(DF!==undefined && IM!=undefined)
                  {
  
                    set_defInfo_(DF.data);
                    // let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
                    // setCurIM(img_pros.data);
                    setFileBrowsingContext(undefined);
                    InpectionAgain(-1,DF.data);
                    ecCanvas.SetCableRegionInfo(DF.data.regionInfo);
                  }
                }, reject:()=>{} });
            },
            path:"data/"
          });
        },
        take_new:()=>{

          Reset();
          TakeNew(0);
        }
        // InpectionAgain,
        // paramAdjust_Info:{
          
        // },
        // paramAdjust:(key,value)=> set_defInfo_({..._defInfo_,[key]:value}),
      })
    return () => {
      
    };
  }, [_defInfo_,editRegionInfo,ecCanvas])


  if(ecCanvas!==undefined)
  {
    ecCanvas.SetRP(CurRP);
    ecCanvas.SetIM(CurIM);

  }

  useEffect(() => {
    if(ecCanvas===undefined)return;

    ecCanvas.EmitEvent=(event)=>{
  
      console.log(event);
      switch (event.type) {
        case "onRegionInfoUpdate":
          
          if(event.data==undefined )
          {
            if(editRegionInfo!==undefined)
              setEditRegionInfo(undefined);
          }
          else
          {
            console.log(event)
            setEditRegionInfo(event.data.region);
          }


          // let combDefFile={..._defInfo_,regionInfo:ecCanvas.cableRegionInfo}
          // set_defInfo_(combDefFile);
          break;
      }
    }
  
  }, [ecCanvas])




  const ACT_Cache_Img_Save= (fileName) => ACT_WS_SEND( "SV", 0,{ filename: fileName, type: "__CACHE_IMG__" } )
  
  const ACT_BIN_Save=( fileName, content)  => ACT_WS_SEND( "SV", 0,{ filename: fileName},content );


  // console.log(_defInfo_);
  let sliders_stage1_gen=()=>//make it lazy
  ["HFrom","HTo","SMax","SMin","VMax","VMin","boxFilter1_Size","boxFilter1_thres","boxFilter2_Size","boxFilter2_thres"].map(key=>[
    <div>
      {key+":"+_defInfo_[key]}
      
      <Slider key={key+"_slider"}
        min={0}
        max={255}
        onChange={(value) => {
          if(_defInfo_[key]===undefined)return;
          let newDef = {..._defInfo_,[key]:value};
          // this.MatchingEnginParamSet(key,value);
          set_defInfo_(newDef);

          InpectionAgain( _this.curStage,newDef)
        }}
        value={_defInfo_[key]}
        step={1}
      />
    </div>
    ])




  let CableRegionUI_gen=()=>{

    let stateInfo=xState(ecCanvas.c_state);
    if(stateInfo.state!==states.cable_region_setup)return null;
    console.log(stateInfo);
    switch(stateInfo.substate)
    {
      case states_region.neutral:
        return <>
          <Button shape="round" key="ACT_region_Add" icon={<PlusOutlined />}
            onClick={()=>{
              ecCanvas.state_transistion(actions.ACT_region_Add)
              setForceUpdate(forceUpdate+1);
            }}>
              ACT_region_Add
          </Button>
          <Button shape="round" key="ACT_region_pix_ref_Add" icon={<PlusOutlined />}
            onClick={()=>{
              ecCanvas.state_transistion(actions.ACT_region_pix_ref_Add)
              setForceUpdate(forceUpdate+1);
            }}>
              ACT_region_pix_ref_Add
          </Button>
          <Button shape="round" key="rangeAdd" icon={<PlusOutlined />}
            onClick={()=>{
              ecCanvas.TriggerAddNewRegion();
            }}>
              增加檢測範圍
          </Button>
          <RegionEditor regionInfo={editRegionInfo}  key="rangeEdit"
            onRegionInfoUpdate={(id,rinfo)=>{
              // console.log(info);
              ecCanvas.setRegion(id,rinfo);
              ecCanvas.draw();
            }}
            onRegionRefPixSet={()=>{
              ecCanvas.TriggerAddRefPix();
            }}
            />
        </>
    
      case states_region.region_Add:
        return <>
          <Button shape="round" key="ACT_region_BACK" icon={<PlusOutlined />}
            onClick={()=>{
              ecCanvas.state_transistion(actions.ACT_region_BACK)
              setForceUpdate(forceUpdate+1);
            }}>
              BACK
          </Button>
      </>

      case states_region.pix_ref_Add:
        return <>
          
          <Button shape="round" key="ACT_region_BACK" icon={<PlusOutlined />}
            onClick={()=>{
              ecCanvas.state_transistion(actions.ACT_region_BACK)
              setForceUpdate(forceUpdate+1);
            }}>
              BACK
          </Button>
        </>
    }

    return null;
  }

  // console.log(editRegionInfo);
  return <div className="overlayCon HXF">
    <CanvasComponent key="kk" addClass="height12" IM={CurIM} 
    onCanvasInit={setECCanvas}/>
    {fileSavingContext===undefined?null:<BPG_FileSavingBrowser key="BPG_FileSavingBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileSavingContext.path} visible={true}
        defaultName={fileSavingContext.defaultName}
        BPG_Channel={(...args) => ACT_WS_SEND(...args)}

        onOk={(folderInfo, fileName, existed) => {
          fileSavingContext.onOK(folderInfo, fileName, existed);

        }}
        onCancel={() => {
          if(fileSavingContext.onCancel!==undefined)
            fileSavingContext.onCancel(folderInfo, fileName, existed);
          else
          { 
            setFileSavingContext(undefined);
          }
          // setFileSavingCallBack(undefined);
        }}
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.'+CABLE_DEF_EXT)}
      />}

    {fileBrowsingContext===undefined?null:<BPG_FileBrowser key="BPG_FileBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileBrowsingContext.path} visible={true}
        BPG_Channel={(...args) => ACT_WS_SEND(...args)}
        onFileSelected={fileBrowsingContext.onFileSelected}
        onCancel={() => {
          if(fileBrowsingContext.onCancel!==undefined)
            fileBrowsingContext.onCancel(folderInfo, fileName, existed);
          else
          { 
            setFileBrowsingContext(undefined);
          }
          // setFileSavingCallBack(undefined);
        }}
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.'+CABLE_DEF_EXT)}
      />}



    {  ecCanvas===undefined ?null:
    <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      
      <Button onClick={()=>InpectionAgain(0)}>TAKE0</Button>
      <Button onClick={()=>InpectionAgain(2)}>TAKE2</Button>
      <Button onClick={()=>InpectionAgain(3)}>TAKE3</Button>
      <Button onClick={()=>InpectionAgain(-1)}>TAKEX</Button>


      <Menu
        style={{ width: 256 }}
        defaultSelectedKeys={['1']}
        // defaultOpenKeys={['sub1']}
        openKeys={menuKey}
        mode="inline"  selectable={false}
        onOpenChange={(openKeys)=>{
          // console.log(openKeys);

          let lastKey = undefined
          if(openKeys.length>0)
            lastKey = openKeys[openKeys.length-1];
          setMenuKey([lastKey]);


          switch(lastKey)
          {
            case states.head_locating:
              ecCanvas.state_transistion(actions.ACT_head_locating);
              ecCanvas.scaleImageToFitScreen();
              break;
            case states.cable_region_setup:
              ecCanvas.state_transistion(actions.ACT_cable_region_setup);
              ecCanvas.scaleHeadToFitScreen();
              break;
            default:
              ecCanvas.state_transistion(actions.ACT_neutral);
              break;

          }


          ecCanvas.draw();

          // SetCableRegionInfo(_cableRegionInfo)
        }}
      >
        <SubMenu

          disabled={!ecCanvas.isAvailable(states.head_locating)}
          key={states.head_locating}
          title={states.head_locating}
        >
          
          {sliders_stage1_gen()}
        </SubMenu>

        <SubMenu 
          disabled={!ecCanvas.isAvailable(states.cable_region_setup)}
          key={states.cable_region_setup} 
          title={states.cable_region_setup}>

          <CableRegionUI_gen/>

        </SubMenu>
      </Menu>
      {



      }
    </div>
    }
  </div>;
}


export default CABLE_WIRE_CONF_MODE_rdx;