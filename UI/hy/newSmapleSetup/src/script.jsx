'use strict'
   
import styles from '../style/basis.css'
import sp_style from '../style/sp_style.css'
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

let Store= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {

  componentWillMount()
  {
    this.unSubscribe=Store.subscribe(()=>
    {
      this.setState(Store.getState().UIData);
    });
  }

  componentWillUnmount()
  {
    this.unSubscribe();
    this.unSubscribe=null;
  }


  constructor(props) {
    super(props);
    this.state =Store.getState().UIData;
    console.log(this.state);
  }
  componentDidMount() {
    this.ec_canvas=new UI_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
  }
  updateCanvas() {
    if(this.ec_canvas  !== undefined)
    {
      console.log(this.state);
      this.ec_canvas.SetReport(this.state.report);
      this.ec_canvas.SetImg(this.state.img);
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
    this.updateCanvas();
    return (
      <div className="width12 HXF">
        <canvas ref="canvas" className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }   
}

class SideMenu extends React.Component{

  //<JSONTree data={this.state.selectedFeature}/>
  render() {
    return(
      <BASE_COM.CardFrameWarp addClass="overlay width5 height10 overlayright sideCtrl scroll" fixedFrame={true}>
        <div className="HXF scroll ">
          <BASE_COM.Button
            addClass="lgreen width4"
            text="..."/>
        </div>
      </BASE_COM.CardFrameWarp>
    );
  }

}

class APP_EDIT_MODE extends React.Component{


  componentWillMount()
  {
    this.unSubscribe=Store.subscribe(()=>
    {
      this.setState(Store.getState().UIData);
    });
  }
  componentDidMount()
  {
    bpg_ws.send("TG");
  }
  componentWillUnmount()
  {
    this.unSubscribe();
    this.unSubscribe=null;
  }

  constructor(props) {
      super(props);
      this.state =Store.getState().UIData;
      console.log(this.state);
  }
  render() {

    console.log(this.state);
    return(
    <div className="HXF">
      <CanvasComponent/>


      <$CSSTG transitionName = "fadeIn"  className="width0">

        {(this.state.MENU_EXPEND)?
          <SideMenu/>
          :[]
        }
      </$CSSTG>

    </div>
    );
  }
}

class APPMain extends React.Component{


  componentWillMount()
  {
    this.unSubscribe=Store.subscribe(()=>
    {
      this.setState(Store.getState().UIData);
    });
  }

  componentWillUnmount()
  {
    this.unSubscribe();
    this.unSubscribe=null;
  }

  constructor(props) {
      super(props);
      this.state =Store.getState().UIData;
      console.log(this.state);
  }


  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  render() {
    let UI=[];
    console.log(this.state);

    //TODO: ugly logic, find a way to deal with it
    if(typeof this.state.c_state.value === "string") // displays "string")
    {
      if(this.state.c_state.value === UIAct.UI_SM_STATES.MAIN)
      {
        UI = 
          <BASE_COM.Button
            addClass="lgreen width4"
            text="EDIT MODE" onClick={(event)=>{Store.dispatch(UIAct.EV_UI_Edit_Mode())}}/>;
        
      }
    }
    else if(UIAct.UI_SM_STATES.EDIT_MODE in this.state.c_state.value)
    {
      UI = <APP_EDIT_MODE/>;
    }

    return(
    <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
      {UI}
    </BASE_COM.CardFrameWarp>

    );
  }
}

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

    this.state =Store.getState().UIData;
    console.log(this.state);
  }
  componentWillMount()
  {
    this.unSubscribe=Store.subscribe(()=>
    {
      this.setState(Store.getState().UIData);
    });
  }

  componentWillUnmount()
  {
    this.unSubscribe();
    this.unSubscribe=null;
  }
  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      <APPMain key="APP"/>

      <BASE_COM.CardFrameWarp addClass={"width6 height10 overlay SMGraph "+((this.state.showSM_graph)?"":"hide")} fixedFrame={true}>
        <div className="layout width11 height12">
          <XSGraph addClass="width12 height12" state_machine={this.state.sm.config}/>
        </div>
        <div className="layout button width1 height12" onClick=
          {()=>Store.dispatch({type:UIAct.UI_SM_EVENT.Control_SM_Panel,data: !this.state.showSM_graph})}></div>
      </BASE_COM.CardFrameWarp>

      {
        (this.state.showSplash)?
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
        Store.dispatch(UIAct.EV_WS_Image_Update(img));
      }
      else if(header.type === "IR")
      {
        let IR =BPG_Protocol.raw2obj(evt);
        console.log("IR",IR);
        Store.dispatch(UIAct.EV_WS_Inspection_Report(IR.data));

      }
    }
  }
  let ws_onopen=(evt)=>
  {
    Store.dispatch(UIAct.EV_WS_Connected(evt));
  }

  let ws_onclose=(evt)=>
  {
    Store.dispatch(UIAct.EV_WS_Disconnected(evt));
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

ReactDOM.render(<APPMasterX/>,document.getElementById('container'));
