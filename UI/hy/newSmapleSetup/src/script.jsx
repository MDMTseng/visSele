'use strict'
   


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
 
import BPG_Protocol from 'UTIL/BPG_Protocol.js'; 
import BPG_WEBSOCKET from 'UTIL/BPG_WebSocket.js';  

import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';




let StoreX= ReduxStoreSetUp({});


    
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
    if(this.props.c_state==null)return null;
    let stateObj = xstate_GetCurrentMainState(this.props.c_state);
    if(stateObj.state === UIAct.UI_SM_STATES.MAIN)
    {
      UI.push(
        <BASE_COM.Button
          key="EDIT MODE" addClass="lgreen width4"
          text="EDIT MODE" onClick={this.props.EV_UI_Edit_Mode}/>);
          
      UI.push(
        <BASE_COM.Button
          key="INSP MODE" addClass="lblue width4"
          text="INSP MODE" onClick={this.props.EV_UI_Insp_Mode}/>);
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.DEFCONF_MODE)
    {
      UI = <APP_DEFCONF_MODE_rdx EC_WS_CH={bpg_ws}/>;
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.INSP_MODE)
    {
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
    EV_UI_Insp_Mode: () => {dispatch(UIAct.EV_UI_Insp_Mode())}
  }
}
const mapStateToProps_APPMain = (state) => {
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

    let xstateG;
    if (this.props.stateMachine!=null) {
      xstateG = 
      <XSGraph addClass="width12 height12" 
      state_machine={this.props.stateMachine.config}/>;
    } else {
      xstateG = null;
    }
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      <APPMain_rdx key="APP"/>

      <BASE_COM.CardFrameWarp addClass={"width7 height10 overlay SMGraph "+((this.props.showSM_graph)?"":"hide")} fixedFrame={true}>
        <div className="layout width11 height12">
          {xstateG}  
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
        this.binary_ws.send(BPG_Protocol.objbarr2raw("HR",0,{a:["d"]}));

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
      else if(header.type === "IR" || header.type === "RP" )
      {
        let report =BPG_Protocol.raw2obj(evt);
        console.log(header.type,report);
        StoreX.dispatch(UIAct.EV_WS_Inspection_Report(report.data));

      }
      else if(header.type === "DF")
      {
        let report =BPG_Protocol.raw2obj(evt);
        console.log(header.type,report);
        StoreX.dispatch(UIAct.EV_WS_Define_File_Update(report.data));

      }
      else if(header.type === "SG")
      {
        let report =BPG_Protocol.raw2obj(evt);
        console.log(header.type,report);
        StoreX.dispatch(UIAct.EV_WS_SIG360_Report_Update(report.data));

      }
    }
  }
  let ws_onopen=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Connected(evt));
    StoreX.dispatch(UIAct.EV_WS_ChannelUpdate(bpg_ws));
  }

  let ws_onclose=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Disconnected(evt));
  }
  this.send=(tl,prop=0,data={},uintArr=null)=>
  {
    if(data instanceof Uint8Array)
    {
      this.binary_ws.send(BPG_Protocol.objbarr2raw(tl,prop,null,data));
    }
    else
    {
      this.binary_ws.send(BPG_Protocol.objbarr2raw(tl,prop,data,uintArr));
    }
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
