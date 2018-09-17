'use strict'
   
import styles from '../style/basis.css'
import sp_style from '../style/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';
 
import BPG_Protocol from './UTIL/BPG_Protocol.js'; 
import BPG_WEBSOCKET from './UTIL/BPG_WebSocket.js';  

import UI_Ctrl from './UI_Ctrl.js';  
import {ReduxStoreSetUp} from './redux/redux';
import {XSGraph} from './xstate_visual';
import * as UIAct from './redux/actions/UIAct';

let StoreX= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {

  componentDidMount() {
    this.ec_canvas=new UI_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
  }
  updateCanvas() {
    if(this.ec_canvas  !== undefined)
    {
      this.ec_canvas.SetReport(this.props.report);
      this.ec_canvas.SetImg(this.props.img);
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
    this.updateCanvas();
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
const CanvasComponent_rdx = connect(mapStateToProps_CanvasComponent)(CanvasComponent);



class APP_EDIT_MODE extends React.Component{


  componentDidMount()
  {
    bpg_ws.send("TG");
  }
  constructor(props) {
      super(props);
  }
  render() {

    console.log("APP_EDIT_MODE render");
    return(
    <div className="HXF">
      <CanvasComponent_rdx addClass="layout width11"/>
      <div className="layout width1 HXF scroll ">
          <BASE_COM.Button
            addClass="layout black"
            text="line"/>
          <BASE_COM.Button
            addClass="layout black"
            text="arc"/>
          <BASE_COM.Button
            addClass="layout black"
            text="List"/>
      </div>


    </div>
    );
  }
}


const APP_EDIT_MODE_rdx = connect()(APP_EDIT_MODE);


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
    //TODO: ugly logic, find a way to deal with it
    if(typeof this.props.c_state.value === "string") // displays "string")
    {
      if(this.props.c_state.value === UIAct.UI_SM_STATES.MAIN)
      {
        UI = 
          <BASE_COM.Button
            addClass="lgreen width4"
            text="EDIT MODE" onClick={this.props.EV_UI_Edit_Mode}/>;
        
      }
    }
    else if(UIAct.UI_SM_STATES.EDIT_MODE in this.props.c_state.value)
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
    EV_UI_Edit_Mode: (arg) => {dispatch(UIAct.EV_UI_Edit_Mode())},
  }
}
const mapStateToProps_APPMain = (state) => {
  console.log("mapStateToProps",state);
  return {
    c_state: state.UIData.c_state
  }
}
const APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);


const lightMachine = {
  id: 'light',
  initial: 'green',
  states: {
      green: {
      on: {
          TIMER: 'yellow'
      }
      },
      yellow: {
      on: {
          TIMER: 'red'
      }
      },
      red: {
      on: {
          TIMER: 'green'
      }
      }
  }
};
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
