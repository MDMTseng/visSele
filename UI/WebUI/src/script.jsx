'use strict'


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import ReactDOM from 'react-dom';
import * as BASE_COM from './component/baseComponent.jsx';

import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';

import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';

import CSSTransitionGroup from 'react-transition-group/CSSTransitionGroup';
let $CTG=CSSTransitionGroup;
//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { xstate_GetCurrentMainState, websocket_autoReconnect,GetObjElement,
  websocket_reqTrack} from 'UTIL/MISC_Util';
import { MWWS_EVENT } from "REDUX_STORE_SRC/middleware/MWWebSocket";

// import LocaleProvider from 'antd/lib/locale-provider';

import Modal from "antd/lib/modal";
import Divider from 'antd/lib/divider';
import APPMain_rdx from './MAINUI';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import * as log from 'loglevel';
import jsonp from 'jsonp';

import semver from 'semver'
import { default as AntButton } from 'antd/lib/button';
import Collapse from 'antd/lib/collapse';
import Menu from 'antd/lib/menu';

import { 
  LaptopOutlined,
  CameraOutlined,
  DatabaseOutlined,
  CloudUploadOutlined} from '@ant-design/icons';

import { useSelector,useDispatch } from 'react-redux';
import Button from 'antd/lib/button';
import Drawer from 'antd/lib/drawer';

log.setLevel("info");
log.getLogger("InspectionEditorLogic").setLevel("INFO");
log.getLogger("UICtrlReducer").setLevel("INFO");

// import moment from 'moment';
// import 'moment/locale/fr';
// moment.locale('fr');


let StoreX = ReduxStoreSetUp({});
console.log(navigator)

function getRandom(min = 0, max = 1000000) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
};


function checkUpdateInfo(updateInfo)
{
  console.log(updateInfo);
  let plat = navigator.platform.toLowerCase();
  let tar_asset=undefined;
  if(plat=="macintel")
  {
    tar_asset=updateInfo.assets.find(asset=>asset.name.endsWith("mac.zip"))
  }
  else if(plat=="win32" || plat=="win64")
  {
    tar_asset=updateInfo.assets.find(asset=>asset.name.endsWith("win.zip"))
  }
  return tar_asset;
}


function isNewVersionExist(latestReleaseInfo,currentVersionInfo)
{
  if(latestReleaseInfo===undefined || currentVersionInfo===undefined)
    return false;
  let lv=currentVersionInfo.version;
  if(lv===undefined)
  {
    lv="0.0.0"
  }
  let rv=semver.clean(latestReleaseInfo.name);

  console.log(" local_version:",lv)
  console.log("remote_version:",rv)
  console.log("gt lt:",semver.gt(rv, lv),semver.lt(rv, lv))
  
  return semver.gt(rv, lv)

}


function SystemServicePanel_UI()
{
  
  const dispatch = useDispatch();
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(...args));



  return <div>
    <Button onClick={()=>{


      ACT_WS_SEND(WS_ID, "RC", 0, {
          target: "camera_ez_reconnect"
        });
      }}>EX_RECONN</Button>
  </div>
}


function Boot_CTRL_UI({URL,doPopUpUpdateWindow=true,onReadyStateChange=()=>{}}) {
  
  const comp_info = React.useRef({});
  
  const DICT = useSelector(state => state.UIData.DICT);
  const [BOOT_DAEMON_readyState, setBOOT_DAEMON_readyState] = useState(WebSocket.CLOSED);


  const dispatch = useDispatch();
  const ACT_Update_Status_Update= (upd_status) => dispatch(UIAct.EV_Update_Status_Update(upd_status));
  
  const boot_daemon_ws = comp_info.current._boot_daemon_ws;

  const [UI_url, setUI_url] = useState(undefined);
  
  const [updateInfo, _setUpdateInfo] = useState(undefined);
  const setUpdateInfo=(uinfo)=>{
    _setUpdateInfo(uinfo);
    ACT_Update_Status_Update(uinfo);
  }


  const [modalInfo, setModalInfo] = useState(undefined);

  const [updateRunning, setUpdateRunning] = useState(false);
  const update_info_url="https://api.github.com/repos/MDMTseng/visSele/releases/latest";

  const coreStates={
    UNKNOWN:"UNKNOWN",
    RUNNING:"RUNNING",
    EXIT:"EXIT",
    TERMINATED:"TERMINATED"
  }
  const [coreState, setCoreState] = useState(coreStates.UNKNOWN);
  
  function fetchUpdateInfo(update_url)
  {

    const boot_daemon_ws = comp_info.current._boot_daemon_ws;
    return Promise.all([
      boot_daemon_ws.send_obj({"type":"get_version"}),
      boot_daemon_ws.send_obj({"type":"http_get","url":update_url})])
      .then((data)=>{

        return new Promise((resolve, reject) => {
          console.log(" >>>>>>:",data);
          if(data[0].ACK !=true || data[1].ACK !=true )
          {
            reject({
              data,
              info:"query has a nak"
            });
          }

          let cverinfo=data[0];
          let rinfo=JSON.parse(data[1].text);
          let tar_asset=checkUpdateInfo(rinfo);
          if(tar_asset===undefined)
          {
            reject({
              data,
              info:"No supported OS"
            });
          }

          let newUpdateExist = isNewVersionExist(rinfo,cverinfo);
          let _updateInfo = {
            local:cverinfo,
            localVersion:cverinfo.version,
            remote:rinfo,
            remoteVersion:rinfo.name,
            remote_res:tar_asset,
            newUpdateExist:newUpdateExist
          }
          resolve(_updateInfo)
        })

        
      })
  }

  function updateTrigger(url)
  {
    setUpdateRunning(true)
    // console.log(setUpdateInfo)
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
      
  }


  useEffect(() => {


    comp_info.current.queryRunningTimer=setInterval(()=>{
      if(comp_info.current._boot_daemon_ws===undefined)return;
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

    
    let rec_ws=new websocket_autoReconnect(URL,3000);
    rec_ws.onreconnection = (reconnectionCounter) => {
      //log.info("onreconnection" + reconnectionCounter);
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
      onReadyStateChange(_boot_daemon_ws.readyState);
      return true;
    }; 

    let _boot_daemon_ws = new websocket_reqTrack(rec_ws,"cmd_id");

    _boot_daemon_ws.onmessage =(data)=>{
      log.info(data)
    };

    _boot_daemon_ws.onopen = (obj) => {
      
      comp_info.current._boot_daemon_ws=_boot_daemon_ws;
      setUpdateRunning(false)
      log.info("boot_daemon_ws.onopen", obj);
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
      onReadyStateChange(_boot_daemon_ws.readyState);


      fetchUpdateInfo(update_info_url)
        .then((info)=>{
          console.log(info)
          setUpdateInfo(info);

          if(info.newUpdateExist)
          {
            setModalInfo({
              title:"New Update!!!",
              onCancel:()=>setModalInfo(),
              onOk:()=>setModalInfo(),
              child:"New update:"+info.remoteVersion+ "   current ver:"+info.localVersion
            }) 
          }
        })
        .catch(err=>{
          console.log(err);
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
          window.location.href=url
          setUI_url(url);
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
      onReadyStateChange(_boot_daemon_ws.readyState);
    }
    _boot_daemon_ws.onerror = () => {
      //log.info("boot_daemon_ws:onerror");
      setBOOT_DAEMON_readyState(_boot_daemon_ws.readyState)
      onReadyStateChange(_boot_daemon_ws.readyState);
    }
    return () => {
      _boot_daemon_ws.close()
      setBOOT_DAEMON_readyState(WebSocket.CLOSED)
      onReadyStateChange(WebSocket.CLOSED);
      comp_info.current._boot_daemon_ws = (undefined)
      clearInterval(comp_info.current.queryRunningTimer);
      comp_info.current.queryRunningTimer=undefined
    }
    // Your code here
  }, []);

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

  let localUpdateBtn=null;
  {
    localUpdateBtn=  
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
    }}>Local UPDATE</Button>

  }
  //console.log(updateInfo);


  let popUpModal=modalInfo===undefined?null:
  <Modal
    key="UpdatePopModal"
    title={modalInfo.title}
    visible={true}
    onCancel={modalInfo.onCancel}
    onOk={modalInfo.onok}
    footer={modalInfo.footer}
  >
    <div style={{height:"100%"}}>
      {modalInfo.child}
    </div>
  </Modal>


  if(BOOT_DAEMON_readyState!==WebSocket.OPEN)return null
  return ([
      popUpModal,
      //APPLaunchCtrlBtn,
      //localUpdateBtn,
      <Button key={"refresh_Button"}   loading={updateRunning}
        onClick={() => {
          

          setUpdateInfo(undefined);
          fetchUpdateInfo(update_info_url)
          .then((info)=>{
            //console.log(info)
            setUpdateInfo(info);
          })
          .catch(err=>{
            console.log(err);
          })
        }}>refresh_Button</Button>,

      updateInfo!==undefined?
      <Button key={"Force Remote UPDATE"} loading={updateRunning}
        icon={<CloudUploadOutlined />} danger={updateInfo.newUpdateExist!==true}
        onClick={() => {
          if(updateInfo.newUpdateExist==true)
          {
            updateTrigger(updateInfo.remote_res.browser_download_url);
          }
          else
          {
            
          }
        }}>{updateInfo.newUpdateExist==true?DICT.fallback.normal_update:DICT.fallback.force_update} {updateInfo.remote.name}</Button>
      :null
    ]);

}

function System_Status_Display({ style={}, showText=false,iconSize=50,gridSize,onStatusChange=(status)=>{},onStatusTick=(status)=>{},
  onClick_Core=(sys_state)=>{},onClick_Camera=(sys_state)=>{},onClick_UploadDataBase=(sys_state)=>{}})
{
  
  const DICT = useSelector(state => state.UIData.DICT);
  const coreConnected = useSelector(state => state.UIData.coreConnected);
  const coreStatus = useSelector(state => state.UIData.Core_Status);
  
  const [systemConnectState,setSystemConnectState] = useState({
    boot_daemon:false,
    core:false,
    camera:false,
    upload_database:false,
  });

  
  useEffect(() => {
    let newStatus={...systemConnectState,core:coreConnected};
    if(systemConnectState.core!=coreConnected)
    {
      setSystemConnectState(newStatus);
      onStatusChange(newStatus)
    }
    onStatusTick(newStatus)
  },[coreConnected])


  
  useEffect(() => {
    let cameraStatus = GetObjElement(coreStatus, ["camera_info",0,"cam_status"])===0;
    //console.log(cameraStatus,systemConnectState);
    let newStatus={...systemConnectState,camera:cameraStatus};
    if(systemConnectState.camera!=cameraStatus)
    {
      setSystemConnectState(newStatus);
      onStatusChange(newStatus)
    }
    onStatusTick(newStatus)
  },[coreStatus])

  
  if(gridSize===undefined)gridSize=iconSize+50;
  let gridStyle={...style,width:(gridSize)+"px" };
  
  let iconStyle={width:iconSize+"px",height:iconSize+"px"};
  return [
    <Button size="large" key="core_stat" style={gridStyle} 
    type="text" //disabled={!systemConnectState.core}
    className={"s HXA "+(systemConnectState.core?"color-online-anim":"color-offline-anim")} 
    onClick={()=>onClick_Core(systemConnectState)}>
      <div 
        className={"antd-icon-sizing veleX"} 
        style={iconStyle}
      >
        <LaptopOutlined/>
      </div>
      {(showText)?DICT.fallback.core:null}
      <br/>{systemConnectState.core?null:DICT.fallback.disconnected}
    </Button>,

    <Button size="large"  key="cam_stat" style={gridStyle} 
    type="text" //disabled={!systemConnectState.camera}
        className={"s HXA "+(systemConnectState.camera?"color-online-anim":"color-offline-anim")} 
        onClick={()=>onClick_Camera(systemConnectState)}>
          <div 
            className={"antd-icon-sizing veleX"} 
            style={iconStyle}
          >
            <CameraOutlined/>
          </div>
          {(showText)?DICT.fallback.camera:null}
          <br/>{systemConnectState.camera?null:DICT.fallback.disconnected}
        </Button>,

    <Button size="large"  key="db_stat" style={gridStyle} 
        type="text" //disabled={!systemConnectState.upload_database}
        className={"s HXA "+(systemConnectState.upload_database?"color-online-anim":"color-offline-anim")} 
        onClick={()=>onClick_UploadDataBase(systemConnectState)}>
          <div 
            className={"antd-icon-sizing veleX"} 
            style={iconStyle}
          >
            <DatabaseOutlined/>
          </div>
          {(showText)?DICT.fallback.upload_database:null}
          <br/>{systemConnectState.upload_database?null:DICT.fallback.disconnected}
        </Button>


  ];
}

function Query_Camera_Info(ACT_WS_SEND,WS_ID)
{
  return new Promise((resolve, reject) => {
    ACT_WS_SEND(WS_ID, "GS", 0, { items: ["data_path","binary_path","camera_info"] },
      undefined, {resolve, reject})
  });
}

function Side_Boot_CTRL_UI({URL,triggerHide}){

  const DICT = useSelector(state => state.UIData.DICT);

  let _DDD = useRef({});
  let s_=_DDD.current;
  //const [BOOT_DAEMON_readyState, setBOOT_DAEMON_readyState] = useState(WebSocket.CLOSED);
  
  const [sys_state, setSys_state] = useState(undefined);
  const dispatch = useDispatch();
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const cur_state = useSelector(state => state.UIData.c_state);
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(...args));

  const ACT_EXIT= _ => dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT));
  const ACT_System_Connection_Status_Update= (st) => dispatch(UIAct.EV_UI_System_Connection_Status_Update(st));


  const ACT_CAMERA_RECONNECT=()=>  new Promise((resolve, reject) => {
    ACT_WS_SEND(WS_ID, "RC", 0, {
      target: "camera_ez_reconnect"
    },
      undefined, { resolve, reject });
  })
  

  function Try_CAMERA_RECONNECT()
  {
    if(s_.in_progress!=true)
    {
      s_.in_progress=true;
      ACT_CAMERA_RECONNECT()
      .then((pkts)=>{
        console.log(pkts);
        s_.in_progress=false;
      })
      .catch((err)=>{
        s_.in_progress=false;
      })
      return true;
    }
    
    return false;
  }

  return (
    <div className="layout width12 height12">
      
      <Divider>{DICT.fallback.connection_status}</Divider>
      <System_Status_Display showText iconSize={30} gridSize={90} 
        onStatusTick={(sys_state)=>{
          if(sys_state.camera==false)
          {
            Try_CAMERA_RECONNECT();
          }
        }}
        onStatusChange={(sys_state)=>{
          
          setSys_state(sys_state);
          ACT_System_Connection_Status_Update(sys_state);
          let curState_EX=xstate_GetCurrentMainState(cur_state);
          console.log(cur_state,curState_EX.state,UIAct.UI_SM_STATES.INSP_MODE);
          //console.log(sys_state);
          if(sys_state.camera==false&&curState_EX.state==UIAct.UI_SM_STATES.INSP_MODE)
          {
            ACT_EXIT();
          }
          
        }}
        onClick_Core={(sys_state)=>{
          console.log(sys_state)
        }}
        onClick_Camera={(sys_state)=>{
          if(sys_state.camera==false)
          {
            Try_CAMERA_RECONNECT();
          }
        }}
        onClick_UploadDataBase={(sys_state)=>{}}/>
      <Divider>Update</Divider>
      <Boot_CTRL_UI URL={URL}/>
      <Divider>ServicePanel</Divider>
      <SystemServicePanel_UI/>
    </div>)
}


function NullDOM_SystemStatusQuery({onStatusChange}){

  
  let _DDD = useRef({});
  let s_=_DDD.current;
  const dispatch = useDispatch();
  const ACT_CAMERA_STATUS_UPDATE= (camera_info) => dispatch(UIAct.EV_Core_Status_Update(camera_info));
  
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(...args));
  useEffect(() => {
    //return 
    //trigger start
    setInterval(()=>
    {
      if(s_.inProgress==true)return;
      s_.inProgress=true;
      Query_Camera_Info(ACT_WS_SEND,WS_ID)
        .then((pkts) => {
          s_.inProgress=false;
          let GS=pkts.find(pkt=>pkt.type=="GS")
          if (GS!==undefined) {

            onStatusChange(GS.data);
            ACT_CAMERA_STATUS_UPDATE(GS.data);
          }
        })
        .catch(err => {
          s_.inProgress=false;
          log.error(err);
        })
    }
      
    ,2000);

  },[])

  return null;
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

      ACT_MachTag_Update: (machTag) => { dispatch(DefConfAct.MachTag_Update(machTag)) },
      ACT_Machine_Custom_Setting_Update: (info) => dispatch(UIAct.EV_machine_custom_setting_Update(info)),
    }
  }
  static mapStateToProps(state) {
    return {
      coreConnected: state.UIData.coreConnected,
      showSM_graph: state.UIData.showSM_graph,
      stateMachine: state.UIData.sm,
      WS_CH: state.UIData.WS_CH,
      WS_ID: state.UIData.WS_ID,
      C_STATE: state.UIData.c_state,
      
      Update_Status:state.UIData.Update_Status,
      DICT :state.UIData.DICT
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
    this.state={
      show_system_panel:true
    };
    //this.state.do_splash=true;
    
    this.WSDataDispatch = this.WSDataDispatch.bind(this);
    this.BPG_WS = {
      reqWindow: {},
      pgIDCounter: 0,
      onopen: (ev, ws_obj) => {
        StoreX.dispatch(UIAct.EV_WS_REMOTE_SYSTEM_READY(ws_obj));
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
                // let url = HR.data.webUI_resource;
                // if (url === undefined) url = "http://hyv.idcircle.me"
                // url += "/version.jsonp?rand=" + getRandom();

                // jsonp(url, { name: "hyv_version_map" }, (err, data) => {
                //   data.core_info = HR.data;
                //   this.props.ACT_Version_Map_Update(data);
                // });
              }

              this.props.ACT_WS_SEND(this.props.WS_ID, "HR", 0, { a: ["d"] });
              
              setTimeout(()=>{

                this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0, { filename: "data/default_camera_param.json" });

  
              
                  
                this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
                { filename: "data/machine_setting.json" },
                undefined, 
                {resolve: (data) => {
                  console.log(data);
                  if (data[0].type == "FL") {
                    let info = data[0].data;
                    this.props.ACT_Machine_Custom_Setting_Update(info);
                  }
                }});
            
                this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
                  { filename: "data/machine_info" },
                  undefined, 
                  {resolve: (data) => {
                    console.log(data);
                    if (data[0].type == "FL") {
                      let info = data[0].data;
                      if (info.length < 16) {
                        this.props.ACT_MachTag_Update(info);
                      }
                    }
                  }});
              },1000)

              
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
        StoreX.dispatch(UIAct.EV_WS_REMOTE_SYSTEM_NOT_READY(ev));
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

        //console.log(data, ws_obj, promiseCBs);

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
      xstateG =null// <Side_Boot_CTRL_UI triggerHide={this.props.coreConnected} URL="ws://localhost:5678"/>
    }


    console.log(this.props.C_STATE);

    let localVersion=(this.props.Update_Status!==undefined)?this.props.Update_Status.localVersion:null;
    return (
      <div className="HXF sp_Style">
        <NullDOM_SystemStatusQuery onStatusChange={(status)=>{
          //console.log(status)
        }}/>
        <APPMain_rdx key="APP" />
        <CSSTransitionGroup //Splash Cover
          transitionName={"logoFrame"}
          transitionEnter={true}
          transitionLeave={true}
          transitionEnterTimeout={1500}
          transitionLeaveTimeout={1500}
          >
        {
          (this.props.coreConnected) ?null:
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
        }
        </CSSTransitionGroup>
        <Drawer
          title={this.props.DICT.fallback.system_status+" "+localVersion}
          placement="right"
          closable={true}
          onClose={()=>{
            this.setState({show_system_panel:false});
          }}
          visible={this.state.show_system_panel}
        >
          <Side_Boot_CTRL_UI URL="ws://localhost:5678"/>
        </Drawer>

        <Button className="overlay" 
          style={{
            background: "white",
            right:this.state.show_system_panel?"-50px":"-10px",
            margin:"10px",
            top:"100px",
            height: "auto",
            width:"50px",
            borderRadius: "12px 0px 0px 12px"}} 
          onClick={()=>{this.setState({show_system_panel:true})}} key="sec">
          <System_Status_Display 
            showText={false} iconSize={20} gridSize={30}/>
        </Button>
      </div>
    );
  }
}

let APPMasterX_rdx = APPMasterX.connect();

ReactDOM.render(

  <Provider store={StoreX}>
      <APPMasterX_rdx />

  </Provider>, document.getElementById('container'));

