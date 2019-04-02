'use strict'


import 'antd/dist/antd.css';
import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import * as BASE_COM from './component/baseComponent.jsx';
 
import BPG_Protocol from 'UTIL/BPG_Protocol.js'; 
import {DEF_EXTENSION} from 'UTIL/BPG_Protocol';

import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import {MWWS_EVENT} from "REDUX_STORE_SRC/middleware/MWWebSocket";

import  LocaleProvider  from 'antd/lib/locale-provider';

import APPMain_rdx from './MAINUI';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import zh_TW from 'antd/lib/locale-provider/zh_TW';
import EC_zh_TW from './languages/zh_TW';
import * as log from 'loglevel';

import  {default as AntButton}  from 'antd/lib/button';
import  Collapse  from 'antd/lib/collapse';
import  Menu  from 'antd/lib/menu';

log.setLevel("info");
log.getLogger("InspectionEditorLogic").setLevel("INFO");
log.getLogger("UICtrlReducer").setLevel("INFO");


let zhTW = Object.assign({},zh_TW,EC_zh_TW);
// import moment from 'moment';
// import 'moment/locale/fr';
// moment.locale('fr');


let StoreX= ReduxStoreSetUp({});
    
function map_BPG_Packet2Act(parsed_packet)
{
  let acts=[];
  let req_id="";
  switch(parsed_packet.type )
  {
    case "HR":
    {
      /*//log.info(this.props.WS_CH);
      this.props.ACT_WS_SEND(this.props.WS_ID,"HR",0,{a:["d"]});
      
      this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{filename:"data/default_camera_param.json"});
      break;*/
    }

    case "SS":
    {
      let SS =parsed_packet;
      req_id=SS.data.req_id;
      break;
    }
    case "IM":
    {
      let pkg = parsed_packet;
      let img = new ImageData(pkg.image, pkg.width);
      
      acts.push(UIAct.EV_WS_Image_Update(img));
      break;
    }
    case "IR":
    case "RP":
    {
      let report =parsed_packet;
      acts.push(UIAct.EV_WS_Inspection_Report(report.data));
      break;
    }
    case "DF":
    {
      let report =parsed_packet;
      req_id=report.data.req_id;
      log.debug(report.type,report);
      
      acts.push(UIAct.EV_WS_Define_File_Update(report.data));
      break;
    }
    case "FL":
    {
      let report =parsed_packet;
      req_id=report.data.req_id;
      log.error(report.type,report);
      if(report.data.type === "binary_processing_group" )
      {
        
        acts.push(UIAct.EV_WS_Inspection_Report(report.data));
      }
      break;
    }
    case "SG":
    {
      let report =parsed_packet;
      req_id=report.data.req_id;
      log.debug(report.type,report);
      acts.push(UIAct.EV_WS_SIG360_Report_Update(report.data));
      break;
    }
    default:
    {
      let report =parsed_packet;
      req_id=report.data.req_id;
      
      act = (report);
    }



  }
  return acts[0];
}

class APPMasterX extends React.Component{

  static mapDispatchToProps(dispatch, ownProps){
    return {
      ACT_Ctrl_SM_Panel: (args) => dispatch({type:UIAct.UI_SM_EVENT.Control_SM_Panel,data:args}),
      ACT_WS_CONNECT: (id,url,obj) => dispatch({type:MWWS_EVENT.CONNECT,data:Object.assign({id:id,url:url,binaryType:"arraybuffer"},obj)}),
      ACT_WS_DISCONNECT:(id)=> dispatch({type:MWWS_EVENT.DISCONNECT,data:{id:id}}),
      DISPATCH:(act)=>{
        dispatch(act)
      },
      DISPATCH_flush:(act)=>{
        act.ActionThrottle_type="flush";
        dispatch(act)
      },
      ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
    }
  }
  static mapStateToProps(state){
    return {
      showSplash: state.UIData.showSplash,
      showSM_graph: state.UIData.showSM_graph,
      stateMachine:state.UIData.sm,
      WS_CH:state.UIData.WS_CH,
      WS_ID:state.UIData.WS_ID
    }
  }
  static connect(){
    return connect(APPMasterX.mapStateToProps,APPMasterX.mapDispatchToProps)(APPMasterX);
  }


  constructor(props) {
    super(props);
    //this.state={};
    //this.state.do_splash=true;


    this.BPG_WS={
      reqWindow:[],
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
        let req_id=undefined;

        let parsed_pkt=undefined;
        let SS_start = false;

        switch(header.type )
        {
          case "HR":
          {
            //log.info(this.props.WS_CH);
            this.props.ACT_WS_SEND(this.props.WS_ID,"HR",0,{a:["d"]});
            
            this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{filename:"data/default_camera_param.json"});
            break;
          }

          case "SS":
          {
            let SS =BPG_Protocol.raw2obj(evt);
            req_id=SS.data.req_id;
            if(SS.data.start)
            {
              SS_start=true;
            }
            else
            {
            }
            parsed_pkt=SS;
            break;
          }
          case "IM":
          {
            let pkg = BPG_Protocol.raw2Obj_IM(evt);
            parsed_pkt=pkg;
            break;
          }
          case "IR":
          case "RP":
          case "DF":
          case "FL":
          case "SG":
          default:
          {
            let report =BPG_Protocol.raw2obj(evt);
            req_id=report.data.req_id;
            parsed_pkt=report;

            break;
          }

        }

        if(req_id === undefined)
        {//Not in tracking window, just Dispatch it
          if(parsed_pkt!==undefined)
          {
            let act=map_BPG_Packet2Act(parsed_pkt);
            if(act!==undefined)
              this.props.DISPATCH(act);
          }
        }
        else
        {
          
          let req_pkt=this.BPG_WS.reqWindow[req_id];
          
          if(req_pkt!==undefined)//Find the tracking req
          {
            if(parsed_pkt!==undefined)//There is a act, push into the req acts
              req_pkt.pkts.push(parsed_pkt);

            if(!SS_start && header.type=="SS")//Get the termination session[SS] pkt
            {//remove tracking(reqWindow) info and Dispatch the pkt
              delete this.BPG_WS.reqWindow[req_id];
              if(req_pkt.promiseCBs!==undefined)
              {
                req_pkt.promiseCBs.resolve(req_pkt.pkts);
              }
              else
              {
                req_pkt.pkts.forEach((pkt)=>
                {
                  let act=map_BPG_Packet2Act(pkt);
                  if(act!==undefined)
                    this.props.DISPATCH(act)
                });
              }
              //////
            }

          }
          else//No tracking req info in the window
          {
            if(SS_start)//And it's SS start, put the new tracking info
            {
              this.BPG_WS.reqWindow[req_id]={
                time:new Date().getTime(),
                pkts:[parsed_pkt]
              };
            }
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
      send:(data,ws_obj,promiseCBs)=>{
        if(data.data instanceof Uint8Array)
        {
          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl,data.prop,1,null,data.data));
        }
        else
        {
          if(data.data instanceof Object)
          {
            if(data.data.req_id === undefined)
            {
              data.data.req_id = Math.random().toString(36).substring(7);
            }
            this.BPG_WS.reqWindow[data.data.req_id]={
              time:new Date().getTime(),
              pkts:[],
              promiseCBs:promiseCBs,
            };
          }
          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl,data.prop,300,data.data,data.uintArr));
        }
      }
    }
    
    this.props.ACT_WS_CONNECT(this.props.WS_ID,"ws://localhost:4090",this.BPG_WS);
    

    
  }
  render() {
    log.debug("APPMasterX render",this.props);

    let xstateG;
    if (false && this.props.stateMachine!=null) {
      xstateG = 
      <BASE_COM.CardFrameWarp addClass={"width7 height10 overlay SMGraph "+((this.props.showSM_graph)?"":"hide")} fixedFrame={true}>
        <div className="layout width11 height12">
        <XSGraph addClass="width12 height12" 
          state_machine={this.props.stateMachine.config}/>;
        </div>
        <div className="layout button width1 height12" onClick=
          {()=>this.props.ACT_Ctrl_SM_Panel( !this.props.showSM_graph)}></div>
      </BASE_COM.CardFrameWarp>
     
    } else {
      xstateG = null;
    }
    return(
    <div className="HXF">
      <APPMain_rdx key="APP"/>
      {xstateG}

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

let APPMasterX_rdx = APPMasterX.connect();

log.info(zhTW);
ReactDOM.render(

  <Provider store={StoreX}>
    <LocaleProvider locale={zhTW}>
      <APPMasterX_rdx/>
    </LocaleProvider>

  </Provider>,document.getElementById('container'));

