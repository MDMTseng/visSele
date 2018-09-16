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
import * as UIAct from './redux/actions/UIAct';

let Store= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {

  componentDidMount() {
    this.ec_canvas=new UI_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
    this.ec_canvas.onfeatureselected=this.props.onfeatureselected;
    //this.updateCanvas();
  }
  updateCanvas() {
    if(this.ec_canvas  !== undefined)
    {
      console.log(this.props);
      this.ec_canvas.SetReport(this.props.checkReport.report);
      this.ec_canvas.SetImg(this.props.checkReport.img);
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

class APPMaster extends React.Component{

  eccanvas_onfeatureselected(ev)
  {
    console.log(ev);
    this.state.selectedFeature = ev;
    this.state.MENU_EXPEND=!this.state.MENU_EXPEND;
    this.setState(this.state);
  }
  onMessage(evt)
  {
    console.log("onMessage:::");
    console.log(evt);
    if (evt.data instanceof ArrayBuffer) {
      let header = BPG_Protocol.raw2header(evt);
      console.log("onMessage:["+header.type+"]");
      if(header.type === "HR")
      {
        this.state.ws.send(BPG_Protocol.obj2raw("HR",{a:["d"]}));

        // websocket.send(BPG_Protocol.obj2raw("TG",{}));
        setTimeout(()=>{
            this.state.ws.send(BPG_Protocol.obj2raw("TG",{}));
        },0);
        
        Store.dispatch(UIAct.UIACT_SPLASH_SWITCH(false));
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
        this.state.checkReport.img = new ImageData(pkg.image, pkg.width);
      }
      else if(header.type === "IR")
      {
        let IR =BPG_Protocol.raw2obj(evt);
        console.log("IR",IR);
        this.state.checkReport.report = IR.data;

      }
    }
  }
  ws_onopen(evt)
  {
  }

  ws_onclose(evt)
  {
    Store.dispatch(UIAct.UIACT_SPLASH_SWITCH(true));
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

  constructor(props) {
      super(props);
      let binary_ws = new BPG_WEBSOCKET.BPG_WebSocket("ws://localhost:4090");
      binary_ws.onmessage_bk = this.onMessage.bind(this);
      binary_ws.onopen_bk = this.ws_onopen.bind(this);
      binary_ws.onclose_bk = this.ws_onclose.bind(this);
      this.state ={
        ws:binary_ws,
        checkReport:{
          report:{},
          img:null
        },
        showSlideMenu:false,
        selectedFeature:{},

      };
  }

  componentWillMount()
  {
  }
  componentWillUnmount()
  {
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  render() {
    return(
    <div className="HXF">
      <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
        <CanvasComponent 
          checkReport={this.state.checkReport} 
          onfeatureselected={this.eccanvas_onfeatureselected.bind(this)}/>
      </BASE_COM.CardFrameWarp>


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
      <APPMaster key="APP"/>
      {
        (this.state.showSplash)?
        <div key="LOGO" className="HXF WXF overlay veleXY logoFrame white">
          <div className="veleXY width6 height6">
            <img className="height8 LOGOImg " src="resource/image/NotiMon.svg"></img>
            <div className="HX0_5"/>
            <div>
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



ReactDOM.render(<APPMasterX/>,document.getElementById('container'));
