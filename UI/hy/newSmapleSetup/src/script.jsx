'use strict'
   


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
 
import BPG_Protocol from 'UTIL/BPG_Protocol.js'; 

import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import APP_INSP_MODE_rdx from './InspectionUI';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import {MWWS_EVENT} from "REDUX_STORE_SRC/middleware/MWWebSocket";

import { LocaleProvider } from 'antd';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import zh_TW from 'antd/lib/locale-provider/zh_TW';
import EC_zh_TW from './languages/zh_TW';
import * as log from 'loglevel';

log.setLevel("INFO");
log.getLogger("InspectionEditorLogic.js").setLevel("ERROR");


let zhTW = Object.assign({},zh_TW,EC_zh_TW);
// import moment from 'moment';
// import 'moment/locale/fr';
// moment.locale('fr');


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

    log.debug("APPMain render",this.props);
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

      UI.push(
        <BASE_COM.Button
          key="CAM calib" addClass="lblue width4"
          text="CAM calib" onClick={()=>{

            this.props.ACT_WS_SEND(this.props.WS_ID,"II",0,{
              deffile:"data/cameraCalibration.json",
              imgsrc:"data/BMP_carousel_test/calibration.bmp"
            });
                
          }}/>);
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.DEFCONF_MODE)
    {
      UI = <APP_DEFCONF_MODE_rdx/>;
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.INSP_MODE)
    {
      UI = <APP_INSP_MODE_rdx/>;
      
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
    EV_UI_Insp_Mode: () => {dispatch(UIAct.EV_UI_Insp_Mode())},
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
  }
}
const mapStateToProps_APPMain = (state) => {
  return { 
    c_state: state.UIData.c_state,
    WS_CH:state.UIData.WS_CH,
    WS_ID:state.UIData.WS_ID
  }
}
const APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);

class APPMasterX extends React.Component{

  constructor(props) {
    super(props);
    //this.state={};
    //this.state.do_splash=true;


    this.BPG_WS={
      onopen:(ev,ws_obj)=>{
    
        StoreX.dispatch(UIAct.EV_WS_Connected(ws_obj));
        //StoreX.dispatch(UIAct.EV_WS_ChannelUpdate(bpg_ws));
      },
      onmessage:(evt,ws_obj)=>{
        log.debug("onMessage:::");
        log.debug(evt);
        if (!(evt.data instanceof ArrayBuffer)) return;

        let header = BPG_Protocol.raw2header(evt);
        log.debug("onMessage:["+header.type+"]");
        switch(header.type )
        {
          case "HR":
          {
            //log.info(this.props.WS_CH);
            this.props.ACT_WS_SEND(this.props.WS_ID,"HR",0,{a:["d"]});
            break;
          }

          case "SS":
          {
            let SS =BPG_Protocol.raw2obj(evt);
            // log.debug(header);
            log.debug("Session start:",SS);
            this.props.DISPATCH(UIAct.EV_WS_Session_Lock(SS.data));
            break;
          }
          case "IM":
          {
            let pkg = BPG_Protocol.raw2Obj_IM(evt);
            let img = new ImageData(pkg.image, pkg.width);
            this.props.DISPATCH(UIAct.EV_WS_Image_Update(img));
            break;
          }
          case "IR":
          case "RP":
          {
            let report =BPG_Protocol.raw2obj(evt);
            //log.info(header.type,report);
            this.props.DISPATCH(UIAct.EV_WS_Inspection_Report(report.data));
            break;
          }
          case "DF":
          {
            let report =BPG_Protocol.raw2obj(evt);
            log.debug(header.type,report);
            this.props.DISPATCH(UIAct.EV_WS_Define_File_Update(report.data));
            break;
          }
          case "SG":
          {
            let report =BPG_Protocol.raw2obj(evt);
            log.debug(header.type,report);
            this.props.DISPATCH(UIAct.EV_WS_SIG360_Report_Update(report.data));
            break;
          }

        }
      },
      onclose:(ev,ws_obj)=>{
        this.props.DISPATCH(UIAct.EV_WS_Disconnected(ev));
        setTimeout(()=>{
          this.props.ACT_WS_CONNECT(this.props.WS_ID,"ws://localhost:4090",this.BPG_WS);
        },3000);
      },
      onerror:(ev,ws_obj)=>{
      },
      send:(data,ws_obj)=>{
        if(data.data instanceof Uint8Array)
        {
          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl,data.prop,null,data.data));
        }
        else
        {
          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl,data.prop,data.data,data.uintArr));
        }
      }
    }
    
    this.props.ACT_WS_CONNECT(this.props.WS_ID,"ws://localhost:4090",this.BPG_WS);
    

    
  }
  render() {
    log.debug("APPMasterX render",this.props);

    let xstateG;
    if (this.props.stateMachine!=null) {
      xstateG = 
      <XSGraph addClass="width12 height12" 
      state_machine={this.props.stateMachine.config}/>;
    } else {
      xstateG = null;
    }
    return(
    <div className="HXF">
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
    </div>
    );
  }
}

const mapDispatchToProps_APPMasterX = (dispatch, ownProps) => {
  return {
    ACT_Ctrl_SM_Panel: (args) => dispatch({type:UIAct.UI_SM_EVENT.Control_SM_Panel,data:args}),
    ACT_WS_CONNECT: (id,url,obj) => dispatch({type:MWWS_EVENT.CONNECT,data:Object.assign({id:id,url:url,binaryType:"arraybuffer"},obj)}),
    ACT_WS_DISCONNECT:(id)=> dispatch({type:MWWS_EVENT.DISCONNECT,data:{id:id}}),
    DISPATCH:(act)=>dispatch(act),
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
  }
}
const mapStateToProps_APPMasterX = (state) => {
  return {
    showSplash: state.UIData.showSplash,
    showSM_graph: state.UIData.showSM_graph,
    stateMachine:state.UIData.sm,
    WS_CH:state.UIData.WS_CH,
    WS_ID:state.UIData.WS_ID
  }
}
const APPMasterX_rdx = connect(mapStateToProps_APPMasterX,mapDispatchToProps_APPMasterX)(APPMasterX);

log.info(zhTW);
ReactDOM.render(

  <Provider store={StoreX}>
    <LocaleProvider locale={zhTW}>
      <APPMasterX_rdx/>
    </LocaleProvider>

  </Provider>,document.getElementById('container'));

