'use strict'
   


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';
 
import BPG_Protocol from 'UTIL/BPG_Protocol.js'; 
import BPG_WEBSOCKET from 'UTIL/BPG_WebSocket.js';  

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';

let StoreX= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {

  ec_canvas_EmitEvent(event){
    switch(event.type)
    { 
      case UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS:
        this.props.ACT_SUCCESS();
      break;
      case UIAct.UI_SM_EVENT.EDIT_MODE_FAIL:
        this.props.ACT_Fail();
      break; 
      case UIAct.UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update:
        console.log(event);
        this.props.ACT_EDIT_TAR_UPDATE(event.data);
      break; 
    }
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent=this.ec_canvas_EmitEvent.bind(this);
  }
  updateCanvas(state) {
    if(this.ec_canvas  !== undefined)
    {
      this.ec_canvas.SetReport(this.props.report);
      this.ec_canvas.SetImg(this.props.img);
      this.ec_canvas.SetState(state);
      this.ec_canvas.draw();
    }
  }

  onResize(width,height){
    if(this.ec_canvas  !== undefined)
    {
      this.ec_canvas.resize(width,height);
      this.updateCanvas();
    }
  }
  render() {
    console.log("CanvasComponent render");
    let substate = this.props.c_state.value[UIAct.UI_SM_STATES.EDIT_MODE];
    console.log("substate:"+substate);
    this.updateCanvas(substate);
    switch(substate)
    {
      case UIAct.UI_SM_STATES.EDIT_MODE_NEUTRAL:
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_LINE_CREATE:
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_ARC_CREATE:
      break;
    }
 
    return (
      <div className={this.props.addClass+" HXF"}>
        <canvas ref="canvas" className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }   
}


const mapStateToProps_CanvasComponent = (state) => {
  console.log("mapStateToProps",state);
  return {
    c_state: state.UIData.c_state,
    report: state.UIData.report,
    img: state.UIData.img
  }
}



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => 
{ 
  return{
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Update(targetObj))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);



class APP_EDIT_MODE extends React.Component{


  componentDidMount()
  {
    bpg_ws.send("TG");
  }
  constructor(props) {
      super(props);
  }
  render() {

    let MenuSet=[];
    let menu_height="HXA";//auto
    console.log("CanvasComponent render");
    let substate = this.props.c_state.value[UIAct.UI_SM_STATES.EDIT_MODE];
    switch(substate)
    {
      case UIAct.UI_SM_STATES.EDIT_MODE_NEUTRAL:
      MenuSet=[
          <BASE_COM.Button
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            text="LINE" onClick={()=>this.props.ACT_Line_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            text="ARC" onClick={()=>this.props.ACT_Arc_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            text="Edit" onClick={()=>this.props.ACT_Shape_Edit_Mode()}/>,
        ];
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_LINE_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div className="s lred vbox">LINE</div>,
      ];
      
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_ARC_CREATE:          
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div className="s lred vbox">ARC</div>
      ];
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_SHAPE_EDIT: 
      menu_height = "HXF";         
      MenuSet=[
        <div className="s height6">
          <BASE_COM.Button
            addClass="layout black vbox"
            text="<" onClick={()=>this.props.ACT_Fail()}/>
          <div className="s lred vbox">EDIT</div>
        </div>]
      if(this.props.edit_tar_info!=null)
      {
        let shape = this.props.edit_tar_info;
        MenuSet.push(
          <div className="s height6">
            <div className="s lred vbox">{shape.type}</div>
            {
              (shape.type=="line")?
                <div className="s lred vbox">{shape.pt1.x}</div>
              :
              {}
            }
          </div>);
      }
      break;
    }
 

    console.log("APP_EDIT_MODE render");
    return(
    <div className="HXF">
      <CanvasComponent_rdx addClass="layout width12"/>
        <$CSSTG transitionName = "fadeIn">
          <div key={substate} className={"s width2 overlay scroll MenuAnim " + menu_height}>
            {MenuSet}
          </div>
        </$CSSTG>
      
    </div>
    );
  }
}


const mapDispatchToProps_APP_EDIT_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_Line_Add_Mode: (arg) =>  {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Line_Create))},
    ACT_Arc_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Arc_Create))},
    ACT_Shape_Edit_Mode:(arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Shape_Edit))},
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))}
  }
}

const mapStateToProps_APP_EDIT_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    edit_tar_info:state.UIData.edit_tar_info
  }
};
const APP_EDIT_MODE_rdx = connect(
    mapStateToProps_APP_EDIT_MODE,
    mapDispatchToProps_APP_EDIT_MODE)(APP_EDIT_MODE);


class APPMain extends React.Component{


  constructor(props) {
      super(props);
  }


  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  render() {
    let UI=[];

    console.log("APPMain render",this.props);

    let stateObj = xstate_GetCurrentMainState(this.props.c_state);
    if(stateObj.state === UIAct.UI_SM_STATES.MAIN)
    {
      UI = 
        <BASE_COM.Button
          addClass="lgreen width4"
          text="EDIT MODE" onClick={this.props.EV_UI_Edit_Mode}/>;
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.EDIT_MODE)
    {
      UI = <APP_EDIT_MODE_rdx/>;
    }

    return(
    <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
      {UI}
    </BASE_COM.CardFrameWarp>

    );
  }
}
const mapDispatchToProps_APPMain = (dispatch, ownProps) => {
  return {
    EV_UI_Edit_Mode: (arg) => {dispatch(UIAct.EV_UI_Edit_Mode())}
  }
}
const mapStateToProps_APPMain = (state) => {
  console.log("mapStateToProps",state);
  return { 
    c_state: state.UIData.c_state
  }
}
const APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);

class APPMasterX extends React.Component{

  constructor(props) {
    super(props);
    //this.state={};
    //this.state.do_splash=true;

  }
  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {
    console.log("APPMasterX render",this.props);
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      <APPMain_rdx key="APP"/>

      <BASE_COM.CardFrameWarp addClass={"width7 height10 overlay SMGraph "+((this.props.showSM_graph)?"":"hide")} fixedFrame={true}>
        <div className="layout width11 height12">
          <XSGraph addClass="width12 height12" state_machine={this.props.stateMachine.config}/>
        </div>
        <div className="layout button width1 height12" onClick=
          {()=>this.props.ACT_Ctrl_SM_Panel( !this.props.showSM_graph)}></div>
      </BASE_COM.CardFrameWarp>

      {
        (this.props.showSplash)?
        <div key="LOGO" className="s HXF WXF overlay veleXY logoFrame white">
          <div className="veleXY width6 height6">
            <img className="height8 LOGOImg " src="resource/image/NotiMon.svg"></img>
            <div className="HX0_5"/>
            <div className="s">
              <div className="TitleTextCon showOverFlow HX2">
                <h1 className="Title">GO  !!</h1>
                <h1 className="Title">NOTIMON</h1>
              </div>
            </div>
          </div>
        </div>
        :[]
      }
    </$CSSTG>
    );
  }
}

const mapDispatchToProps_APPMasterX = (dispatch, ownProps) => {
  return {
    ACT_Ctrl_SM_Panel: (args) => dispatch({type:UIAct.UI_SM_EVENT.Control_SM_Panel,data:args}),
  }
}
const mapStateToProps_APPMasterX = (state) => {
  console.log("mapStateToProps",state);
  return {
    showSplash: state.UIData.showSplash,
    showSM_graph: state.UIData.showSM_graph,
    stateMachine:state.UIData.sm
  }
}
const APPMasterX_rdx = connect(mapStateToProps_APPMasterX,mapDispatchToProps_APPMasterX)(APPMasterX);

function BPG_WS (url){
  
  this.binary_ws = new BPG_WEBSOCKET.BPG_WebSocket(url);//"ws://localhost:4090");

  let onMessage=(evt)=>
  {
    console.log("onMessage:::");
    console.log(evt);
    if (evt.data instanceof ArrayBuffer) {
      let header = BPG_Protocol.raw2header(evt);
      console.log("onMessage:["+header.type+"]");
      if(header.type === "HR")
      {
        this.binary_ws.send(BPG_Protocol.obj2raw("HR",{a:["d"]}));

        // websocket.send(BPG_Protocol.obj2raw("TG",{}));
        //setTimeout(()=>{this.state.ws.send(BPG_Protocol.obj2raw("TG",{}));},0);
        
      }
      else if(header.type === "SS")
      {
        let SS =BPG_Protocol.raw2obj(evt);
        // console.log(header);
        console.log("Session start:",SS);
      }
      else if(header.type === "IM")
      {
        let pkg = BPG_Protocol.raw2Obj_IM(evt);
        let img = new ImageData(pkg.image, pkg.width);
        StoreX.dispatch(UIAct.EV_WS_Image_Update(img));
      }
      else if(header.type === "IR")
      {
        let IR =BPG_Protocol.raw2obj(evt);
        console.log("IR",IR);
        StoreX.dispatch(UIAct.EV_WS_Inspection_Report(IR.data));

      }
    }
  }
  let ws_onopen=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Connected(evt));
  }

  let ws_onclose=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Disconnected(evt));
  }
  this.send=(tl,data={})=>
  {
    this.binary_ws.send(BPG_Protocol.obj2raw(tl,data));
  }
  this.binary_ws.onmessage_bk = onMessage.bind(this);
  this.binary_ws.onopen_bk = ws_onopen.bind(this);
  this.binary_ws.onclose_bk = ws_onclose.bind(this);
}

let bpg_ws = new BPG_WS("ws://localhost:4090");

ReactDOM.render(
  
  <Provider store={StoreX}>
    <APPMasterX_rdx/>
  </Provider>,document.getElementById('container'));
