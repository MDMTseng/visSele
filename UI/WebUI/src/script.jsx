'use strict'


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React, { useState, useEffect } from 'react';
import ReactDOM from 'react-dom';
import * as BASE_COM from './component/baseComponent.jsx';

import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';

import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';

import CSSTransitionGroup from 'react-transition-group/CSSTransitionGroup';
let $CTG=CSSTransitionGroup;
//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';

import { xstate_GetCurrentMainState, websocket_autoReconnect,
  websocket_reqTrack} from 'UTIL/MISC_Util';
import { MWWS_EVENT } from "REDUX_STORE_SRC/middleware/MWWebSocket";

import LocaleProvider from 'antd/lib/locale-provider';

import APPMain_rdx from './MAINUI';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import zh_TW from 'antd/lib/locale-provider/zh_TW';
import EC_zh_TW from './languages/zh_TW';
import * as log from 'loglevel';
import jsonp from 'jsonp';

import semver from 'semver'
import { default as AntButton } from 'antd/lib/button';
import Collapse from 'antd/lib/collapse';
import Menu from 'antd/lib/menu';

import Button from 'antd/lib/button';
log.setLevel("info");
log.getLogger("InspectionEditorLogic").setLevel("INFO");
log.getLogger("UICtrlReducer").setLevel("INFO");


let zhTW = Object.assign({}, zh_TW, EC_zh_TW);
// import moment from 'moment';
// import 'moment/locale/fr';
// moment.locale('fr');


let StoreX = ReduxStoreSetUp({});
console.log(navigator)

function getRandom(min = 0, max = 1000000) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
};
function Boot_CTRL_UI({URL,triggerHide}) {
  const comp_info = React.useRef({});
  
  const [BOOT_DAEMON_readyState, setBOOT_DAEMON_readyState] = useState(WebSocket.CLOSED);
  
  const boot_daemon_ws = comp_info.current._boot_daemon_ws;

  const [UI_url, setUI_url] = useState(undefined);
  const [hidePanel, setHidePanel] = useState(false);
  
  const [latestReleaseInfo, setLatestReleaseInfo] = useState(undefined);

  const [updateRunning, setUpdateRunning] = useState(false);


  const coreStates={
    UNKNOWN:"UNKNOWN",
    RUNNING:"RUNNING",
    EXIT:"EXIT",
    TERMINATED:"TERMINATED"
  }
  const [coreState, setCoreState] = useState(coreStates.UNKNOWN);

  useEffect(() => {
    setHidePanel(true);
  },[triggerHide])

  useEffect(() => {


    comp_info.current.queryRunningTimer=setInterval(()=>{

      if(comp_info.current._boot_daemon_ws.readyState!==WebSocket.OPEN)
      {
        if(coreState!==coreStates.UNKNOWN)
          setCoreState(coreStates.UNKNOWN);
        return
      }
      comp_info.current._boot_daemon_ws.send_obj({"type":"poll_core"})
      .then((data)=>{
        //if(data.)
        //console.log("poll.then:",data)
        if(data.ACK)
        {
          if(data.poll_code===undefined||data.poll_code===null)
          {//The code will be set if the program exit with return code
            
            if(coreState!==coreStates.RUNNING)
              setCoreState(coreStates.RUNNING);
          }
          else
          {
            if(coreState!==coreStates.EXIT)
             setCoreState(coreStates.EXIT);
          }
        }
        else
        {
          if(coreState!==coreStates.TERMINATED)
            setCoreState(coreStates.TERMINATED);
        }
      })
      .catch((err)=>{
        console.log(err)
      })
    },5000)

    
    let url = "http://hyv.idcircle.me"
    url += "/version.jsonp?rand=" + getRandom();

    jsonp(url, { name: "hyv_version_map" }, (err, data) => {
      console.log(data)
    });




    let rec_ws=new websocket_autoReconnect(URL,3000);
    rec_ws.onreconnection = (reconnectionCounter) => {
      //log.info("onreconnection" + reconnectionCounter);
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
      
      return true;
    }; 
    // rec_ws.onconnectiontimeout = () =>{ 
    //   log.info("boot_daemon_ws:onconnectiontimeout");
    //   this.setState({BOOT_DAEMON_readyState:_boot_daemon_ws.readyState});
    // }
    let _boot_daemon_ws = new websocket_reqTrack(rec_ws,"cmd_id");

    _boot_daemon_ws.onmessage =(data)=>{
      log.info(data)
    };

    _boot_daemon_ws.onopen = (obj) => {
      
      setUpdateRunning(false)
      log.info("boot_daemon_ws.onopen", obj);
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)


      Promise.all([
        _boot_daemon_ws.send_obj({"type":"get_version"}),
        _boot_daemon_ws.send_obj({"type":"http_get","url":"https://api.github.com/repos/MDMTseng/visSele/releases/latest"})])
        .then((data)=>{
          console.log(" >>>>>>:",data);
          if(data[0].ACK ==false || data[1].ACK ==false )
          {
            return;
          }
          data[1].obj=JSON.parse(data[1].text);


          
          let plat = navigator.platform.toLowerCase();
          let tar_asset=undefined;
          if(plat=="macintel")
          {
            tar_asset=data[1].obj.assets.find(asset=>asset.name.endsWith("mac.zip"))
          }
          else if(plat=="win32" || plat=="win64")
          {
            tar_asset=data[1].obj.assets.find(asset=>asset.name.endsWith("win.zip"))
          }

          if(tar_asset===undefined)
          {
            //No supported OS
            return;
          }
          data[1].obj.tar_asset=tar_asset;
          


          let lv=data[0].version;
          if(lv===undefined)
          {
            lv="0.0.0"
          }
          let rv=semver.clean(data[1].obj.name);

          console.log(" local_version:",lv)
          console.log("remote_version:",rv)
          console.log("remote_info:",data[1].obj)
          console.log("gt lt:",semver.gt(rv, lv),semver.lt(rv, lv))
          
          if(semver.gt(rv, lv))
          {
            setLatestReleaseInfo(data[1].obj);
          }
          return
          // let maxV = versions
          //   .map(ver => semver.clean(ver))
          //   .reduce((maxV, ver) => semver.gt(maxV, ver) ? maxV : ver);
          // let hasNewVer = semver.gt(maxV, localV);


        })
        .catch((err)=>{
          console.log(err)
        })


      _boot_daemon_ws.send_obj({"type":"get_UI_url"})
        .then((data)=>{
          console.log("get_UI_url:",data)
          let url="file:///"+data.url;
          
          console.log(window.location.href+">>>"+url)
          let curUrl=window.location.href;
          let dstUrl=url;
          curUrl=curUrl.replace(/file:\/+/, "").replace(/\\/g, "/");
          dstUrl=dstUrl.replace(/file:\/+/, "").replace(/\\/g, "/");

          if(curUrl!==dstUrl)
          {
            setUI_url(url);
            window.location.href=url
          }
        })
        .catch((err)=>{
          console.log(err)
        })

      return true;
    };
    _boot_daemon_ws.onclose = (e) =>{
      log.info("boot_daemon_ws:onclose,",e);
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
    }
    _boot_daemon_ws.onerror = () => {
      //log.info("boot_daemon_ws:onerror");
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
    }
    comp_info.current._boot_daemon_ws=_boot_daemon_ws;
    return () => {
      _boot_daemon_ws.close()
      setBOOT_DAEMON_readyState(WebSocket.CLOSED)
      comp_info.current._boot_daemon_ws = (undefined)
      clearInterval(comp_info.current.queryRunningTimer);
      comp_info.current.queryRunningTimer=undefined
    }
    // Your code here
  }, []);

  let wopn=BOOT_DAEMON_readyState==WebSocket.OPEN;



  let APPLaunchCtrlBtn=null;
  switch(coreState)
  {
    case coreStates.RUNNING:
    case coreStates.EXIT:
      
      APPLaunchCtrlBtn=<Button key={"APP_TERMINATION_Button"}  danger
        onClick={() => {
          boot_daemon_ws.send_obj({"type":"kill_core"})
          setCoreState(coreStates.UNKNOWN)
        }}>TERMINATION</Button>
      break;
    case coreStates.TERMINATED:
    
      APPLaunchCtrlBtn=<Button key={"APP_LAUNCH_Button"}  type="primary"
        onClick={() => {
          boot_daemon_ws.send_obj({"type":"launch_core", "env_path":"./"})
          setCoreState(coreStates.UNKNOWN)
        }}>APP RUN</Button>
      break;
    
    case coreStates.UNKNOWN:
  
      APPLaunchCtrlBtn=<Button key={"APP_UNKNOWN_Button"}  loading
        onClick={() => {
        }}>APP UNKNOWN</Button>
      break;
  }
  return (
  <BASE_COM.CardFrameWarp 
    addClass={"width7 height10 overlay SMGraph "+((!hidePanel)?"":"hide")} 
    fixedFrame={true}>
    <div className={"s width11 height12"}>
    {
      wopn?
        [
          // <div className="layout width3 height2">
          //   readyState:{BOOT_DAEMON_readyState}
          // </div>,
          
          APPLaunchCtrlBtn,

          
            <Button key={"1_Button"}   loading={updateRunning}
            onClick={() => {
            setUpdateRunning(true)
            let current_datetime = new Date()
            let formatted_date = 
              current_datetime.getDate() + "_" + 
              (current_datetime.getMonth() + 1) + "_" + 
              current_datetime.getFullYear()
              

              
          
            let plat = navigator.platform.toLowerCase();
            let updateFileName="update"
            if(plat=="macintel")
            {
              updateFileName+="_mac.zip";
            }
            else if(plat=="win32" || plat=="win64")
            {
              updateFileName+="_win.zip";
            }
            
            let x = {"type":"update", 
              "bk_name_append":formatted_date,
              "update_URL":updateFileName
            }
            boot_daemon_ws.send_obj(x)
              .then((data)=>{
                
                setUpdateRunning(false)
                console.log("Update:",data)
                if(data.ACK)
                {
                  let x = {"type":"reload"}
                  boot_daemon_ws.send_obj(x)
                    .then((data)=>{
                      console.log("reload:",data)
                    })
                    .catch((err)=>{
                      console.log(err)
                    })
      
                }
              })
              .catch((err)=>{
                console.log(err)
              })
          }}>Local UPDATE</Button>,

          latestReleaseInfo===undefined?null:
          <Button key={"Remote UPDATE"} loading={updateRunning}
            onClick={() => {
            setUpdateRunning(true)
            console.log(latestReleaseInfo)
            let url=latestReleaseInfo.tar_asset.browser_download_url;
            //if(latestReleaseInfo.)
            let current_datetime = new Date()
            let x = {"type":"update", 
              "bk_name_append":"",
              "update_URL":url
            }
            boot_daemon_ws.send_obj(x)
              .then((data)=>{
                
                setUpdateRunning(false)
                console.log("Update:",data)
                if(data.ACK)
                {
                  let x = {"type":"reload"}
                  boot_daemon_ws.send_obj(x)
                    .then((data)=>{
                      console.log("reload:",data)
                    })
                    .catch((err)=>{
                      console.log(err)
                    })
      
                }
              })
              .catch((err)=>{
                console.log(err)
              })
              
            }}>Remote UPDATE {latestReleaseInfo.name}</Button>
        ]
        :
        <div className="layout width11 height12">
          CLOSED
        </div>
    }
    </div>
    <div className="layout button width1 height12" onClick=
      {() => setHidePanel(!hidePanel)}></div>
    {/* <div className="layout width11 height12">
      readyState:{BOOT_DAEMON_readyState}
    </div> */}
  </BASE_COM.CardFrameWarp>)

}




class APPMasterX extends React.Component {

  static mapDispatchToProps(dispatch, ownProps) {
    return {
      ACT_Ctrl_SM_Panel: (args) => dispatch({ type: UIAct.UI_SM_EVENT.Control_SM_Panel, data: args }),
      ACT_WS_CONNECT: (id, url, obj) => dispatch({ type: MWWS_EVENT.CONNECT, data: Object.assign({ id: id, url: url, binaryType: "arraybuffer" }, obj) }),
      ACT_WS_DISCONNECT: (id) => dispatch({ type: MWWS_EVENT.DISCONNECT, data: { id: id } }),
      DISPATCH: (act) => {
        dispatch(act)
      },
      DISPATCH_flush: (act) => {
        act.ActionThrottle_type = "flush";
        dispatch(act)
      },
      ACT_CAMERA_INFO_UPDATE: (camera_info) => dispatch(UIAct.EV_UI_Version_Map_Update(camera_info)),
      ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),

      ACT_Version_Map_Update: (mapInfo) => dispatch(UIAct.EV_UI_Version_Map_Update(mapInfo)),

    }
  }
  static mapStateToProps(state) {
    return {
      showSplash: state.UIData.showSplash,
      showSM_graph: state.UIData.showSM_graph,
      stateMachine: state.UIData.sm,
      WS_CH: state.UIData.WS_CH,
      WS_ID: state.UIData.WS_ID
    }
  }
  static connect() {
    return connect(APPMasterX.mapStateToProps, APPMasterX.mapDispatchToProps)(APPMasterX);
  }


  WSDataDispatch(pkts) {
    let acts = {
      type: "ATBundle",
      ActionThrottle_type: "express",
      data: pkts.map(pkt => BPG_Protocol.map_BPG_Packet2Act(pkt)).filter(act => act !== undefined),
      //rawData:req_pkt
    };
    //console.log(pkts,acts);
    this.props.DISPATCH(acts)
  }

  constructor(props) {
    super(props);
    //this.state={};
    //this.state.do_splash=true;
    
    this.WSDataDispatch = this.WSDataDispatch.bind(this);
    this.BPG_WS = {
      reqWindow: {},
      pgIDCounter: 0,
      onopen: (ev, ws_obj) => {

        StoreX.dispatch(UIAct.EV_WS_Connected(ws_obj));

        //StoreX.dispatch(UIAct.EV_WS_ChannelUpdate(bpg_ws));
      },
      onmessage: (evt, ws_obj) => {
        //log.info("onMessage:::");
        //log.info(evt);
        if (!(evt.data instanceof ArrayBuffer)) return;

        let header = BPG_Protocol.raw2header(evt);
        //log.info("onMessage:["+header.type+"]");
        let pgID = header.pgID;

        let parsed_pkt = undefined;
        let SS_start = false;

        switch (header.type) {
          case "HR":
            {
              {
                let HR = BPG_Protocol.raw2obj(evt);
                let url = HR.data.webUI_resource;
                if (url === undefined) url = "http://hyv.idcircle.me"
                url += "/version.jsonp?rand=" + getRandom();

                jsonp(url, { name: "hyv_version_map" }, (err, data) => {
                  data.core_info = HR.data;
                  this.props.ACT_Version_Map_Update(data);
                });
              }

              this.props.ACT_WS_SEND(this.props.WS_ID, "HR", 0, { a: ["d"] });
              //Just for teesting

              setTimeout(() => {
                this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0, { filename: "data/default_camera_param.json" });
              }, 1000);


              this.props.ACT_WS_SEND(this.props.WS_ID, "GS", 0, { items: ["data_path","binary_path","camera_info"] },
                undefined, {
                resolve: (data) => {
                  console.log(data)
                  if (data[0].type == "GS") {

                    let path = data[0].data["binary_path"];
                    if(path!==undefined)
                    {
                      
                    }

                    let camera_info = data[0].data["camera_info"];
                    if(camera_info!==undefined)
                    {
                      this.props.ACT_CAMERA_INFO_UPDATE(camera_info);
                    }

                  }
                }, reject: (err) => {
                  log.error(err);
                }
              });
            }

          case "SS":
            {
              let SS = BPG_Protocol.raw2obj(evt);

              if (SS.data.start) {
                SS_start = true;
              }
              else {
              }
              parsed_pkt = SS;
              break;
            }
          case "IM":
            {
              let pkg = BPG_Protocol.raw2Obj_IM(evt);
              parsed_pkt = pkg;
              break;
            }
          case "IR":
          case "RP":
          case "DF":
          case "FL":
          case "SG":
          case "PD":
          default:
            {
              let report = BPG_Protocol.raw2obj(evt);
              parsed_pkt = report;

              break;
            }

        }
        if (header.type == "PD") {
          //log.info(parsed_pkt,pgID);
          if (pgID == 0)
            pgID = -1;
        }
        //log.info(parsed_pkt,pgID,this.BPG_WS.reqWindow);
        if (pgID === -1) {//Not in tracking window, just Dispatch it
          if (parsed_pkt !== undefined) {
            let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
            if (act !== undefined)
              this.props.DISPATCH(act);
          }
        }
        else {
          let req_pkt = this.BPG_WS.reqWindow[pgID];

          if (req_pkt !== undefined)//Find the tracking req
          {
            if (parsed_pkt !== undefined)//There is a act, push into the req acts
              req_pkt.pkts.push(parsed_pkt);

            if (!SS_start && header.type == "SS")//Get the termination session[SS] pkt
            {//remove tracking(reqWindow) info and Dispatch the pkt
              let stacked_pkts = this.BPG_WS.reqWindow[pgID].pkts;
              if (req_pkt._PGINFO_ === undefined || req_pkt._PGINFO_.keep !== true) {
                delete this.BPG_WS.reqWindow[pgID];
              }
              else {
                this.BPG_WS.reqWindow[pgID].pkts = [];
              }
              if (req_pkt.promiseCBs !== undefined) {
                req_pkt.promiseCBs.resolve(stacked_pkts, this.WSDataDispatch);
              }
              else {
                // let acts={
                //   type:"ATBundle",
                //   ActionThrottle_type:"express",
                //   data:stacked_pkts.map(pkt=>BPG_Protocol.map_BPG_Packet2Act(pkt)).filter(act=>act!==undefined),
                //   rawData:req_pkt
                // };
                // this.props.DISPATCH(acts)
                this.WSDataDispatch(stacked_pkts);
                // req_pkt.pkts.forEach((pkt)=>
                // {
                //   let act=map_BPG_Packet2Act(pkt);
                //   if(act!==undefined)
                //     this.props.DISPATCH(act)
                // });

              }
              //////
            }

          }
          else//No tracking req info in the window
          {
            if (SS_start)//And it's SS start, put the new tracking info
            {
              this.BPG_WS.reqWindow[pgID] = {
                time: new Date().getTime(),
                pkts: [parsed_pkt]
              };
            }
            else {
              let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
              if (act !== undefined)
                this.props.DISPATCH(act);
            }
          }

        }
      },
      onclose: (ev, ws_obj) => {
        this.props.DISPATCH(UIAct.EV_WS_Disconnected(ev));
        setTimeout(() => {
          this.props.ACT_WS_CONNECT(this.props.WS_ID, "ws://localhost:4090", this.BPG_WS);
        }, 3000);
      },
      onerror: (ev, ws_obj) => {
      },
      send: (data, ws_obj, promiseCBs) => {


        let PGID = undefined;
        let PGINFO = undefined;
        if (data.data instanceof Object) {
          PGID = data.data._PGID_;
          PGINFO = data.data._PGINFO_;
          delete data.data["_PGID_"];
          delete data.data["_PGINFO_"];
        }
        if (PGID === undefined) {
          PGID = this.BPG_WS.pgIDCounter;
          this.BPG_WS.pgIDCounter++;
          if (this.BPG_WS.pgIDCounter > 10000) this.BPG_WS.pgIDCounter = 0;
        }


        if (data.data instanceof Uint8Array) {
          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl, data.prop, PGID, null, data.data));
        }
        else {
          this.BPG_WS.reqWindow[PGID] = {
            time: new Date().getTime(),
            pkts: [],
            promiseCBs: promiseCBs,
            _PGINFO_: PGINFO
          };

          ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl, data.prop, PGID, data.data, data.uintArr));
        }
      }
    }


    setTimeout(() =>
      this.props.ACT_WS_CONNECT(this.props.WS_ID, "ws://localhost:4090", this.BPG_WS)
      , 100);


  }
  render() {
    log.debug("APPMasterX render", this.props);

    let xstateG=null;
    if (false && this.props.stateMachine != null) {
      xstateG =
        <BASE_COM.CardFrameWarp addClass={"width7 height10 overlay SMGraph " + ((this.props.showSM_graph) ? "" : "hide")} fixedFrame={true}>
          <div className="layout width11 height12">
            <XSGraph addClass="width12 height12"
              state_machine={this.props.stateMachine.config} />;
        </div>
          <div className="layout button width1 height12" onClick=
            {() => this.props.ACT_Ctrl_SM_Panel(!this.props.showSM_graph)}></div>
        </BASE_COM.CardFrameWarp>

    }
    else
    {
      xstateG = <Boot_CTRL_UI triggerHide={this.props.showSplash} URL="ws://localhost:5678"/>
    }


    return (
      <div className="HXF sp_Style">

        <APPMain_rdx key="APP" />
        <CSSTransitionGroup
          transitionName={"logoFrame"}
          transitionEnter={true}
          transitionLeave={true}
          transitionEnterTimeout={1500}
          transitionLeaveTimeout={1500}
          >
        {
          (this.props.showSplash) ?
            <div key="LOGO" className="s HXF WXF overlay veleXY logoFrame white">
              <div className="veleXY width6 height6">
                <img className="height8 LOGOImg" src="resource/image/Ｃ_LOGO.svg"/>
                <div className="HX0_5" />
                <div className="s HX2">
                  <div className="TitleTextCon showOverFlow HX4">
                    <h1 className="Title HX2">By Xception</h1>
                    <h1 className="Title HX2">SAMP</h1>
                  </div>
                </div>
              </div>
            </div>
            : null
        }
        </CSSTransitionGroup>
        {xstateG}
      </div>
    );
  }
}

let APPMasterX_rdx = APPMasterX.connect();

log.info(zhTW);
ReactDOM.render(

  <Provider store={StoreX}>
    <LocaleProvider locale={zhTW}>
      <APPMasterX_rdx />
    </LocaleProvider>

  </Provider>, document.getElementById('container'));

