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
  DownOutlined, TrophyOutlined,
  SubnodeOutlined
} from '@ant-design/icons';




const states ={

  neutral:"neutral",

  head_locating:"head_locating",
  head_locating_detail:"head_locating_detail",
  head_locating_simple:"head_locating_simple",

  cable_region_setup:"cable_region_setup",
  cable_region_color_samping:"cable_region_color_samping",
  
  
  inspection:"inspection",
}


const actions ={
  ACT_neutral:"ACT_neutral",
  ACT_head_locating:"ACT_head_locating",
  ACT_cable_region_setup:"ACT_cable_region_setup",
  ACT_inspection:"ACT_inspection",



  ACT_region_neutral:"ACT_region_neutral",
  ACT_region_Add:"ACT_region_Add",
  ACT_region_Edit:"ACT_region_Edit",
  ACT_region_color_ref_Setup:"ACT_region_color_ref_Setup",
}


const states_region ={

  neutral:"neutral",
  region_Add:"region_Add",
  region_Edit:"region_Edit",
  color_ref_Setup:"color_ref_Setup",
  
}

const regionSStates = {
  initial: states_region.neutral,
  states: {
    [states_region.neutral]: {
      on: {
        [actions.ACT_region_Add]:states_region.region_Add,
        [actions.ACT_region_Edit]:states_region.region_Edit,
      }
    },
    [states_region.region_Add]: {
      on: {
        [actions.ACT_region_neutral]: states_region.neutral,
        [actions.ACT_region_Edit]:states_region.region_Edit,
      }
    },
    [states_region.region_Edit]: {
      on: {
        [actions.ACT_region_neutral]: states_region.neutral,
        [actions.ACT_region_color_ref_Setup]:states_region.color_ref_Setup,
      }
    },
    [states_region.color_ref_Setup]: {
      on: {
        [actions.ACT_region_Edit]:states_region.region_Edit,
        [actions.ACT_region_neutral]: states_region.neutral,
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
        [actions.ACT_cable_region_setup]: states.cable_region_setup,
        [actions.ACT_inspection]: states.inspection,
      }
    },
    [states.head_locating]: {
      on: {
        [actions.ACT_neutral]: states.neutral,
        // [actions.ACT_head_locating]: states.head_locating,
        [actions.ACT_cable_region_setup]: states.cable_region_setup,
        [actions.ACT_inspection]: states.inspection,
      }
    },
    [states.cable_region_setup]: {
      on: {
        [actions.ACT_neutral]: states.neutral,
        [actions.ACT_head_locating]: states.head_locating,
        [actions.ACT_inspection]: states.inspection,
        // [actions.ACT_cable_region_setup]: states.cable_region_setup,
      },
      ...regionSStates
    },
    [states.inspection]: {
      on: {
        [actions.ACT_neutral]: states.neutral,
      }
    }
  }
});


class CableWireDef_CanvasComponent extends EC_CANVAS_Ctrl.EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.reset();
    this.EmitEvent = (event) => { log.info(event); };
    this.tmp_canvas= document.createElement('canvas');
  }


  canvasResize(canvas,width,height)
  {
    width = Math.floor(width);
    height = Math.floor(height);
    canvas.width = width;
    canvas.height = height;
  }

  SetState(newState,data)
  {
    if(this.c_state===undefined || JSON.stringify(this.c_state.value) !==JSON.stringify(newState.value) )
    {
      this.color_ref_region_info=undefined;
    }
    this.c_state=newState;
  }
  reset()
  {
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

    if(xState(this.c_state).state!==states.cable_region_setup)
      doDragging=true;
    if(this.stopDragging==true)
    {
      doDragging=false;
    }

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
    let srs=GetObjElement(info_RP,["data","reports"]);


    let sr=undefined;


    if(Array.isArray(srs) && srs.length>0)
    {
      let largeObjArea=-1;
      for(let i=0;i<srs.length;i++)
      {
        let vec_btm=srs[i].vec_btm;
        let vec_side=srs[i].vec_side;
        let area=Math.abs(vec_btm.x*vec_side.y-vec_btm.y*vec_side.x);
        console.log(area);
        if(largeObjArea<area)
        {
          largeObjArea=area;
          sr=srs[i];
        }
      }

    }

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

  SetInspRP(RP) 
  {
    // console.log(info_RP  );
    this.inspRP=RP;
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
      if(GetObjElement(this.closestPtInfo,['region','id'])==id)
        this.closestPtInfo=undefined;
        
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

  getRegion(id){
    return this.cableRegionInfo.regions.find(regi=>regi.id===id);
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
      
    let stateX=this.c_state===undefined?{state:states.neutral}:xState(this.c_state);
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
      if(this.c_state===undefined)return;
      if(stateX.state===states.cable_region_setup)
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

      ctx.scale(1/mmpp_mult, 1/mmpp_mult);
      if(this.singleReportInfo!==undefined && this.singleReportInfo.report!==undefined && stateX.state!==states.inspection)
      {
        let rp =this.singleReportInfo.report;
        // console.log(rp,">>>>",rp.pt_cornor,"  ",rp.vec_btm);


        
        ctx.strokeStyle = "rgba(120,0,0,0.5)";
        this.rUtil.drawLine(ctx, {
          pt1:{x:rp.pt_cornor.x,y:rp.pt_cornor.y},
          pt2:{x:rp.pt_cornor.x+rp.vec_btm.x,y:rp.pt_cornor.y+rp.vec_btm.y}
        });


        ctx.strokeStyle = "rgba(0,120,0,0.5)";
        this.rUtil.drawLine(ctx, {
          pt1:{x:rp.pt_cornor.x,y:rp.pt_cornor.y},
          pt2:{x:rp.pt_cornor.x+rp.vec_side.x,y:rp.pt_cornor.y+rp.vec_side.y}
        });
      }
      ctx.restore();
    }




    // console.log(this.modeInfo);
    // console.log(stateX,this.c_state);
    switch(stateX.state) 
    {
      case states.head_locating:
        if(this.rectSamplingInfo!==undefined)
        {

          let rect = this.rectSamplingInfo.rect;
          if(rect!==undefined)
          {
            let x=rect.pt1.x,w=rect.pt2.x-rect.pt1.x;
            if(w<0)
            {
              x+=w;
              w=-w;
            }

            let y=rect.pt1.y,h=rect.pt2.y-rect.pt1.y;
            if(h<0)
            {
              y+=h;
              h=-h;
            }

            ctx.beginPath();
            ctx.rect(x,y,w,h);
            ctx.stroke();
            ctx.closePath();
          }
        }
        break;
      case states.cable_region_setup:
        this.rUtil.drawpoint(ctx,{x:0,y:0});
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
            // console.log(reg);
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


        if(this.color_ref_region_info!==undefined)
        {

          ctx.lineWidth = this.color_ref_region_info.margin;
          ctx.strokeStyle = "rgba(0,0,100,0.5)";
          this.rUtil.drawHollowLine(ctx, this.color_ref_region_info)
          ctx.lineWidth = this.color_ref_region_info.margin;
          ctx.strokeStyle = "rgba(200,200,200,0.5)";
          this.rUtil.drawHollowLine(ctx, this.color_ref_region_info,3)
        }


        if(this.tmp_closestPtInfo!==undefined)
        {
          this.rUtil.drawpoint(ctx,this.tmp_closestPtInfo.pt,undefined,this.rUtil.getPointSize()*1.4);
        }
        break;
      case states.inspection:
        // console.logthis.inspRP
        let reports=GetObjElement(this.inspRP,["data","reports"]);
        let defData=GetObjElement(this.singleReportInfo,["report"]);

        if(reports===undefined || defData===undefined)break;

        let defNF_btm=Math.hypot(defData.vec_btm.x,defData.vec_btm.y);
        let defNF_side=Math.hypot(defData.vec_side.x,defData.vec_side.y);



        // console.log(">>>",this.singleReportInfo,norMF_btm,norMF_side);

        // console.log("sss",reports,this.singleReportInfo);

        reports.forEach((srep)=>{

          

          ctx.strokeStyle = "rgba(120,0,0,0.5)";
          this.rUtil.drawLine(ctx, {
            pt1:{x:srep.pt_cornor.x,y:srep.pt_cornor.y},
            pt2:{x:srep.pt_cornor.x+srep.vec_btm.x,y:srep.pt_cornor.y+srep.vec_btm.y}
          });


          ctx.strokeStyle = "rgba(0,120,0,0.5)";
          this.rUtil.drawLine(ctx, {
            pt1:{x:srep.pt_cornor.x,y:srep.pt_cornor.y},
            pt2:{x:srep.pt_cornor.x+srep.vec_side.x,y:srep.pt_cornor.y+srep.vec_side.y}
          });


          let singleReportInfo={
            center:{
              x:srep.pt_cornor.x+(srep.vec_btm.x)/2,
              y:srep.pt_cornor.y+(srep.vec_btm.y)/2,
            },
            rotate:Math.atan2(srep.vec_btm.y,srep.vec_btm.x)

          }
          ctx.save();

          ctx.translate(singleReportInfo.center.x, singleReportInfo.center.y);
          ctx.rotate(singleReportInfo.rotate);
          
          let NF_btm=Math.hypot(srep.vec_btm.x,srep.vec_btm.y)/defNF_btm;
          let NF_side=Math.hypot(srep.vec_side.x,srep.vec_side.y)/defNF_side;


          // ctx.strokeStyle = "rgba(0,0,100,0.5)";
          // this.rUtil.drawHollowLine(ctx, reg)
          // ctx.lineWidth = reg.margin;
          // ctx.strokeStyle = "rgba(200,200,200,0.5)";
          // this.rUtil.drawHollowLine(ctx, reg,3)

          let PASSRate=0;
          this.cableRegionInfo.regions.forEach(reg=>{
            // let x_normalizer

            // 
            
            let defRep = this.cableRegionInfo.anchorInfo.inspection_report.find(defRep=>defRep.id===reg.id);
            let curRep = srep.inspection_report.find(curRep=>curRep.id===reg.id);
            let localPassRate=Number.NaN;
            if(defRep!==undefined && curRep!==undefined)
            {
              // defRep

              // console.log(reg,defRep,curRep);

              let min_W2W_RR=0;//=defRep
              if(defRep.max_window2wire_width_ratio > curRep.max_window2wire_width_ratio)
              {
                min_W2W_RR=curRep.max_window2wire_width_ratio/defRep.max_window2wire_width_ratio;
              }
              else
              {
                min_W2W_RR=defRep.max_window2wire_width_ratio/curRep.max_window2wire_width_ratio;
              }

              if(min_W2W_RR>0.8)
              {
                // 
                localPassRate=1.0/this.cableRegionInfo.regions.length;
              }
              else
              {
                // console.log("NG:"+reg.id,curRep.max_window2wire_width_ratio,defRep.max_window2wire_width_ratio);
                localPassRate=0;
              }

            }

            PASSRate+=localPassRate;

            let margin=reg.margin*NF_btm;
            let reg_line={
              pt1:{
                x:reg.pt1.x*NF_btm,
                y:reg.pt1.y*NF_side
              },
              pt2:{
                x:reg.pt2.x*NF_btm,
                y:reg.pt2.y*NF_side
              },
            }



            // console.log(reg);
            ctx.lineWidth = margin;
            ctx.strokeStyle = "rgba(0,0,100,0.5)";
            this.rUtil.drawHollowLine(ctx, reg_line)
            ctx.lineWidth = margin;



            if(localPassRate!=localPassRate)
            {
              ctx.strokeStyle = "rgba(0,0,0,0.5)";
            }
            else if(localPassRate==0)
            {
              ctx.strokeStyle = "rgba(200,0,0,0.5)";
            }
            else 
            {
              ctx.strokeStyle = "rgba(0,200,200,0.5)";
            }
            this.rUtil.drawHollowLine(ctx, reg_line,3)

          })




          ctx.restore();


          let headPT1={
            x:srep.pt_cornor.x+(srep.vec_btm.x)/2,
            y:srep.pt_cornor.y+(srep.vec_btm.y)/2};
          let headPT2={
            x:srep.pt_cornor.x+srep.vec_side.x+srep.vec_btm.x/2,
            y:srep.pt_cornor.y+srep.vec_side.y+srep.vec_btm.y/2};

          
          ctx.lineWidth =defNF_btm;
          if(PASSRate!=PASSRate)
          {
            ctx.strokeStyle = "rgba(100,100,100,0.5)";
          }
          else if(PASSRate>0.999)//basically 1 but since we do add up so...
            ctx.strokeStyle = "rgba(0,120,0,0.5)";
          else
            ctx.strokeStyle = "rgba(120,0,0,0.5)";

          this.rUtil.drawLine(ctx, {
            pt1:headPT1,
            pt2:headPT2
          });
  

        });


        
        break;
    }
  }


  isAvailable(state)
  {
    // console.log("state:",state);
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


  fetchImagePixRGBA(context,x,y,w,h)
  {
    var pixes = context.getImageData(x, y, w, h).data;

    let RGBA=[0,0,0,0];
    let sigma=[0,0,0,0];
    // Loop over each pixel and invert the color.
    for (var i = 0, n = pixes.length; i < n; i += 4) {
      RGBA[0]+=pixes[i+0];
      RGBA[1]+=pixes[i+1];
      RGBA[2]+=pixes[i+2];
      RGBA[3]+=pixes[i+3];


      sigma[0]+=pixes[i+0]*pixes[i+0];
      sigma[1]+=pixes[i+1]*pixes[i+1];
      sigma[2]+=pixes[i+2]*pixes[i+2];
      sigma[3]+=pixes[i+3]*pixes[i+3];
    }
    let pixCount=pixes.length/4;
    RGBA[0]/=pixCount;
    RGBA[1]/=pixCount;
    RGBA[2]/=pixCount;
    RGBA[3]/=pixCount;


    sigma[0]/=pixCount;
    sigma[1]/=pixCount;
    sigma[2]/=pixCount;
    sigma[3]/=pixCount;

    sigma[0]=Math.sqrt(sigma[0]-RGBA[0]*RGBA[0]);
    sigma[1]=Math.sqrt(sigma[1]-RGBA[1]*RGBA[1]);
    sigma[2]=Math.sqrt(sigma[2]-RGBA[2]*RGBA[2]);
    sigma[3]=Math.sqrt(sigma[3]-RGBA[3]*RGBA[3]);
    return {
      RGBA,
      sigma:sigma,
      area:w*h,
    };
  }
  

  rectSampling(info,callback)
  {
    let stateInfo=xState(this.c_state);
    if(stateInfo.state!==states.head_locating)
      return;
    this.stopDragging=true;
    this.rectSamplingInfo={
      info,
      callback:(rect,RGBA)=>{
        this.stopDragging=false;
        callback(rect,RGBA);
        this.rectSamplingInfo=undefined;
      },
    }
    this.ctrlLogic();
    this.draw();


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
    

    // let enableAddNewRegion=false;
    let subState = xState(this.c_state).substate;
    switch(xState(this.c_state).state) 
    {


      case states.head_locating:

        if(this.rectSamplingInfo!==undefined)
        {

          if(ifOnMouseLeftClickEdge )
          {
            if(this.mouseStatus.status==1)
            {
              this.rectSamplingInfo.rect={
                pt1:mouseOnCanvas2,
                pt2:mouseOnCanvas2
              }
            }
            else
            {
              
              if(this.rectSamplingInfo.rect!==undefined)
              {
                this.rectSamplingInfo.rect.pt2=mouseOnCanvas2;
                let rect=this.rectSamplingInfo.rect;
                let x=rect.pt1.x,w=rect.pt2.x-rect.pt1.x;
                if(w<0)
                {
                  x+=w;
                  w=-w;
                }
                let y=rect.pt1.y,h=rect.pt2.y-rect.pt1.y;
                if(h<0)
                {
                  y+=h;
                  h=-h;
                }

                let scale=this.img_info.scale;
                let ctx2nd = this.secCanvas.getContext('2d');
                let fetchColorInfo = this.fetchImagePixRGBA(ctx2nd,x/scale,y/scale,w/scale,h/scale);

                this.rectSamplingInfo.callback(this.rectSamplingInfo.rect,fetchColorInfo);
              }
            }
  
          }
          else
          {
            if(this.rectSamplingInfo.rect!==undefined)
              this.rectSamplingInfo.rect.pt2=mouseOnCanvas2;
            
          }
  
        }



        break;


      case states.cable_region_setup:
        
        // this.EditInfo

        if(subState==states_region.region_Edit)
        {



          if(this.mouseStatus.status==1)
          { 
  
            if(ifOnMouseLeftClickEdge)
            {
              
              {
                
                let closestPtInfo = this.tmp_closestPtInfo;
                // // console.log("closestPtInfo>>",closestPtInfo);
                // if(closestPtInfo!==undefined && closestPtInfo.dist>20)
                // {
                //   closestPtInfo=undefined;
                // }
                this.closestPtInfo = closestPtInfo;
                
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
        }
        else if(subState==states_region.region_Add)
        {

          if(this.mouseStatus.status==1 && ifOnMouseLeftClickEdge)
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
            
            let onAdded=GetObjElement(this.c_state,['extData','onAdded']);
            if(onAdded!==undefined)
            {
              onAdded();
            }
          }
        }
        else if(subState==states_region.color_ref_Setup)
        {

          if(this.mouseStatus.status==1)
          { 
            if(ifOnMouseLeftClickEdge)//press
            {
              this.color_ref_region_info={
                pt1:mouseOnCanvas2,
                margin:10,
              }
            }

            this.color_ref_region_info.pt2=mouseOnCanvas2;
            
          }
          else
          {
            
            if(ifOnMouseLeftClickEdge)//press
            {

              let diffX = this.color_ref_region_info.pt2.x-this.color_ref_region_info.pt1.x;
              let diffY = this.color_ref_region_info.pt2.y-this.color_ref_region_info.pt1.y;
              this.canvasResize(this.tmp_canvas, Math.hypot(diffX,diffY),this.color_ref_region_info.margin);

              let tmp_ctx = this.tmp_canvas.getContext('2d');
              

              let mmpp = this.rUtil.get_mmpp();


              let angle = Math.atan2(diffY,diffX);

              tmp_ctx.translate(0,this.color_ref_region_info.margin/2);
              tmp_ctx.rotate(-angle);

              tmp_ctx.translate(-this.color_ref_region_info.pt1.x,-this.color_ref_region_info.pt1.y);
              tmp_ctx.rotate(-this.singleReportInfo.rotate);
              console.log(this.singleReportInfo);
              tmp_ctx.translate(-this.singleReportInfo.center.x, -this.singleReportInfo.center.y);

              tmp_ctx.scale(this.img_info.scale,this.img_info.scale);

              // 
              

              
              tmp_ctx.drawImage(this.secCanvas, 0, 0);
              let fetchColorInfo = this.fetchImagePixRGBA(tmp_ctx,0,0,this.tmp_canvas.width,this.tmp_canvas.height);
              console.log(fetchColorInfo);

              fetchColorInfo.region_id=this.closestPtInfo.region.id;
              // this.DownloadCanvasAsImage(this.tmp_canvas,"pretty image.png");

              let onColorFetched=GetObjElement(this.c_state,['extData','onColorFetched']);
              if(onColorFetched!==undefined)
              {
                onColorFetched(fetchColorInfo);
              }
            }
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

     
  </>
}

function CABLE_WIRE_CONF_MODE_rdx({onExtraCtrlUpdate})
{
  let _cur = useRef({
    sm: Machine(CableSetupState)
  });
  let _this=_cur.current;

  const WS_ID = useSelector(state => state.UIData.WS_ID);
  
  const dispatch = useDispatch();
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(WS_ID, ...args));
  
  const [CurIM,setCurIM_]=useState(undefined);
  const [CurRP,setCurRP_]=useState(undefined);
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

  const [c_state,set_c_state]=useState(_this.sm.initialState);


  const [insp_state,set_insp_state]=useState(false);
  
  

  function setCurRP(RP)
  {
    setCurRP_(RP)
    if(ecCanvas!==undefined)
      ecCanvas.SetRP(RP);
  }


  function setCurIM(IM)
  {
    setCurIM_(IM)
    if(ecCanvas!==undefined)
      ecCanvas.SetIM(IM);
  }

  function state_transistion(action,extData)
  {
    let newState = _this.sm.transition(c_state.value,action);
    
    let p_state=c_state;
    newState.extData=extData;
    set_c_state(newState);
    console.log(p_state.value,">[",action,"]>", newState.value);
    
  }


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
  function inspStage(inspectionStage)
  {
    ACT_WS_SEND( "ST", 0,
      { 
        InspectionParam:[{
          inspectionStage
        }]
      },undefined, { 
        resolve:(pkts)=>{}, 
        reject:()=>{} 
      })
  }

  function camera_set_once_WB()
  {
    ACT_WS_SEND( "ST", 0,
      { 
        CameraSetting:{
          set_once_WB:true
        }
      },undefined, { 
        resolve:(pkts)=>{
          console.log(pkts);
        }, 
        reject:()=>{} 
      })
  }


  function TriggerNewResult(doTakeNew,add_defInfo,cur_defInfo=_defInfo_)
  {

    //let defInfo= {...cur_defInfo,...add_defccInfo};

    let defInfo={...cur_defInfo,regionInfo:ecCanvas.cableRegionInfo,...add_defInfo}


    // console.log(defInfo);
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

        ecCanvas.SetIM(img_pros.data);
        // ecCanvas.scaleImageToFitScreen();

        set_c_state(_this.sm.initialState);
        // setMenuKey([]);
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



  function LiveInspection(defInfo)
  {

    
    // this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, { _PGID_: 10004, _PGINFO_: { keep: true }, definfo: deffile     
    // }, undefined);
    console.log(defInfo);
    if(defInfo==undefined)
    {
      
      reloadIM(defPath);
      ACT_WS_SEND("CI", 0, { _PGID_: 10007, _PGINFO_: { keep: false } });
      return;
    }

    console.log(defInfo);
    ACT_WS_SEND("CI", 0, {_PGID_: 10007, _PGINFO_: { keep: true },
      
      definfo:defInfo
    }, undefined,
    { resolve:(pkts)=>{
      // log.info(pkts);
      let RP = pkts.find(pkt=>pkt.type=="RP");
      let IM = pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM!==undefined)
      {

        // s etCurRP(RP);

// eee

        let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
        setCurIM(img_pros.data);
        ecCanvas.SetInspRP(RP);
        ecCanvas.SetIM(img_pros.data);
        // ecCanvas.scaleImageToFitScreen();

        // set_c_state(_this.sm.initialState);
        // setMenuKey([]);
        // setInterval(()=>{
        //   TriggerNewResult();
        // },10000)
      }

    }, reject:(pkts)=>{
      log.info(pkts);
    }});
  }


  function InpectAgain(inspectionStage=1,defInfo)
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
    return () => {
      LiveInspection();
    };
  },[])
    // useEffect(() => {
  //   return () => {
  //     LiveInspection();
  //   };
  // })
  function open(file_path)
  {

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
          Reset();

          setDefPath(file_path);
          set_defInfo_(DF.data);
          // let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
          // setCurIM(img_pros.data);
          setFileBrowsingContext(undefined);
          ecCanvas.SetCableRegionInfo(DF.data.regionInfo);
          InpectAgain(-1,DF.data);
        }
      }, reject:()=>{} });
  }

  function reloadIM(file_path)
  {

    let defModelPath = file_path.replace("."+CABLE_DEF_EXT, "");
  
    ACT_WS_SEND("LD", 0,
      {
        imgsrc: defModelPath,
        down_samp_level:10
      },
      undefined, { 
      resolve:(pkts)=>{
        let IM = pkts.find(pkt=>pkt.type=="IM");
        if(IM!=undefined)
        {
          InpectAgain(-1);
        }
      }, reject:()=>{} });
  }

  useEffect(() => {

    if(onExtraCtrlUpdate!==undefined)
    {
      let ctrlInfo={

        set_insp_state:(on_off)=>{
          if(on_off!==true)on_off=false;
          set_insp_state(on_off);
          if(on_off==true)
          {
            
            state_transistion(actions.ACT_inspection);
            let combDefFile={..._defInfo_,regionInfo:ecCanvas.cableRegionInfo}
            LiveInspection(combDefFile);
          }
          else
          {
            state_transistion(actions.ACT_neutral);
            LiveInspection();


          }
        },
        insp_state
        // InpectAgain,
        // paramAdjust_Info:{
          
        // },
        // paramAdjust:(key,value)=> set_defInfo_({..._defInfo_,[key]:value}),
      }
      if(insp_state==false)
      {

        ctrlInfo.open=(path)=>{

          setFileBrowsingContext({

            onFileSelected:(file_path,file) => {
              // console.log(">>",file_path,file);
              open(file_path);
            },
            path:"data/"
          });
        },

        ctrlInfo.save=(path)=>{
          setFileSavingContext({
            onOK:(folderInfo, fileName, existed) => {
              let fileNamePath = folderInfo.path + "/" + fileName.replace("."+CABLE_DEF_EXT, "");
              setFileSavingContext(undefined);
              var enc = new TextEncoder();
  
  
              let combDefFile={..._defInfo_,regionInfo:ecCanvas.cableRegionInfo}
              console.log(_defInfo_);
  
              set_defInfo_(combDefFile);
  
  
              ACT_BIN_Save(fileNamePath+"."+CABLE_DEF_EXT, enc.encode(JSON.stringify(combDefFile, null, 2)));
              // console.log("ACT_Cache_Img_Save");
              ACT_Cache_Img_Save(fileNamePath+".png");
  
            },
            path:"data/",
            defaultName:"default."+CABLE_DEF_EXT
          });
        };
        ctrlInfo.take_new=()=>{
  
          Reset();
          TakeNew(0);
        }
  
      }

      onExtraCtrlUpdate(ctrlInfo)
    }
    return () => {
      
    };
  }, [_defInfo_,editRegionInfo,ecCanvas,insp_state])



  useEffect(() => {
    if(ecCanvas===undefined)return;

    ecCanvas.EmitEvent=(event)=>{
  
      // console.log(event);
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

          InpectAgain( _this.curStage,newDef)
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
              state_transistion(actions.ACT_region_Add,{onAdded:()=>{
                state_transistion(actions.ACT_region_Edit)
              }})
            }}>
              增加檢測範圍
          </Button>
          <Button shape="round" key="ACT_region_Edit" icon={<PlusOutlined />}
            onClick={()=>{
              state_transistion(actions.ACT_region_Edit)
            }}>
              檢測設定
          </Button>
        </>
    
      case states_region.region_Add:
        return <>
          <Button shape="round" key="ACT_region_neutral" icon={<ArrowLeftOutlined />}
            onClick={()=>{
              state_transistion(actions.ACT_region_neutral)
            }}/>
      </>

      case states_region.region_Edit:{
        let RGBA=GetObjElement(editRegionInfo,['colorInfo','RGBA']);
        let iconStyleObj=(RGBA===undefined)?undefined: { color: 'rgb('+RGBA[0]+','+RGBA[1]+','+RGBA[2]+')' };

        return <>
          <Button shape="round" key="ACT_region_neutral" icon={<ArrowLeftOutlined />}
            onClick={()=>{
              state_transistion(actions.ACT_region_neutral)
            }}/>


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



          {editRegionInfo===undefined?null:
          <Button shape="round" key="ACT_region_color_ref_Setup"  icon={<SubnodeOutlined   style={iconStyleObj} />}
            onClick={()=>{
              state_transistion(actions.ACT_region_color_ref_Setup,{onColorFetched:(colorInfo)=>{
                let region_id=colorInfo.region_id;
                let region_get  = ecCanvas.getRegion(region_id);
                region_get={...region_get,colorInfo}
                ecCanvas.setRegion(region_id,region_get);
                setEditRegionInfo(region_get);
                // console.log("colorInfo:",colorInfo);

              }})
            }}>
              顏色擷取
          </Button>}
      </>
      }

      case states_region.color_ref_Setup:{
        let RGBA=GetObjElement(editRegionInfo,['colorInfo','RGBA']);
        let styleColorString=(RGBA===undefined)?undefined: 'rgb('+RGBA[0]+','+RGBA[1]+','+RGBA[2]+')';
        return <>
          
          <Button shape="round" key="ACT_region_neutral"  style={{background:styleColorString}} 
            icon={<ArrowLeftOutlined />}
            onClick={()=>{
              state_transistion(actions.ACT_region_Edit)
            }}/>
        </>
      }
    }

    return null;
  }

  if(ecCanvas!==undefined)
  {
    ecCanvas.SetState(c_state);
    ecCanvas.draw();

  }

  function RGB2HSV(RGB)
  {
        //0 V ~255
    //1 S ~255
    //2 H ~251

    let InDataR=RGB[0]|0;
    let InDataG=RGB[1]|0;
    let InDataB=RGB[2]|0;

    let Mod,Max,Min,D1,D2;
    Max=Min=InDataR;
    D1=InDataG;
    D2=InDataB;
    Mod=6;
    if(InDataG>Max)
    {
        Max=InDataG;
        Mod=2;       //
        D1=InDataB;
        D2=InDataR;
    }
    else
    {
        Min=InDataG;

    }

    if(InDataB>Max)
    {
        Max=InDataB;
        Mod=4;
        D1=InDataR;
        D2=InDataG;
    }
    else if(InDataB<Min)
    {
        Min=InDataB;

    }
    let H,S,V;
    V=Max;
    if(Max==0)
    {
      S=
      H=0;
      
      return [H,S,V];
    }
    else
      S=255-Min*255/Max;
    Max-=Min;
    if(Max)
    {
      H=(Mod*(Max)+D1-D2)*42/(Max);
      if(H<42||H>=252)H+=4;
    }
    else
      H=0;

    return [H,S,V];
    
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




    
    <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      
      <Button onClick={camera_set_once_WB}>camera_set_once_WB</Button>

      <Button onClick={()=>insp_state?inspStage(0):InpectAgain(0)}>T0</Button>
      <Button onClick={()=>insp_state?inspStage(2 ):InpectAgain(2)}>T2</Button>
      <Button onClick={()=>insp_state?inspStage(3):InpectAgain(3)}>T3</Button>
      <Button onClick={()=>insp_state?inspStage(-1):InpectAgain(-1)}>TX</Button>
      {ecCanvas===undefined ||insp_state==true ?null:<>


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
              ecCanvas.scaleImageToFitScreen();
              state_transistion(actions.ACT_head_locating);
              break;
            case states.cable_region_setup:
              ecCanvas.scaleHeadToFitScreen();
              state_transistion(actions.ACT_cable_region_setup);
              break;
            default:
              state_transistion(actions.ACT_neutral);
              break;

          }



          // SetCableRegionInfo(_cableRegionInfo)
        }}
      >
        <SubMenu

          disabled={!ecCanvas.isAvailable(states.head_locating)}
          key={states.head_locating}
          title={states.head_locating}
        >
          <Button onClick={()=>{
            ecCanvas.rectSampling({},(rect,colorInfo)=>{
              let HSV=RGB2HSV(colorInfo.RGBA);
              console.log(rect,colorInfo,HSV);

              let H=HSV[0];
              let S=HSV[1];
              let V=HSV[2];
              let Hrange=50*255/(V+0.1);
              let Vrange=colorInfo.sigma.reduce((M,s)=>M>s?M:s,0);
              let Srange=Vrange*300/(V+0.1);//if V is small S would have larger range due to the static noise
              
              let HSVThres={
                HTo:H+Hrange,
                HFrom:H-Hrange,
                SMax:S+Srange,
                SMin:S-Srange,
                VMax:V+Vrange,
                VMin:V-Vrange,
                boxFilter1_Size:17,
                boxFilter1_thres:26,
                boxFilter2_Size:37,
                boxFilter2_thres:179
              }
              if(Hrange>(250/2))
              {
                HSVThres.HFrom=0;
                HSVThres.HTo=251;

              }
              else
              {
                HSVThres.HFrom=(HSVThres.HFrom+251)%251;
                HSVThres.HTo=(HSVThres.HTo+251)%251;
              }

              if(HSVThres.SMax>255)HSVThres.SMax=255;
              if(HSVThres.SMin<0)HSVThres.SMin=0;

              if(HSVThres.VMax>255)HSVThres.VMax=255;
              if(HSVThres.VMin<0)HSVThres.VMin=0;
              let newDef = {..._defInfo_,...HSVThres};
              // this.MatchingEnginParamSet(key,value);
              set_defInfo_(newDef);
    
              InpectAgain( _this.curStage,newDef)



            })
          }}>SELECT</Button>
          
          {sliders_stage1_gen()}
        </SubMenu>

        <SubMenu 
          disabled={!ecCanvas.isAvailable(states.cable_region_setup)}
          key={states.cable_region_setup} 
          title={states.cable_region_setup}>

          <CableRegionUI_gen/>

        </SubMenu>
      </Menu>
      </>}
    </div>
      
  </div>;
}


export default CABLE_WIRE_CONF_MODE_rdx;