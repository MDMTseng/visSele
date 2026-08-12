'use strict'

import 'regenerator-runtime/runtime' // some legacy deps (react-numpad) expect a global regeneratorRuntime

import 'STYLE/basis.css'
import 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import ReactDOM from 'react-dom';
// The device's setup document is grouped (plate/gate/cam/skip_policy); this UI
// speaks flat. Translating at the wire is the whole fix -- see uinspCfg.js.
import { regroup as uinspRegroup, flatten as uinspFlatten } from './uinspCfg';
import * as BASE_COM from './component/baseComponent.jsx';
import {UINSP_UI,SLID_UI,CNC_UI} from './component/rdxComponent.jsx';
import {UINSP_ESP32_UI} from './component/uInspESP32_UI.jsx';

import {GetDefaultSystemSetting} from './info.js';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import { initActionRecorder } from 'UTIL/actionRecorder.js';
import { WEBUI_BUILD } from 'virtual:webui-build';   // git/build provenance injected at build time
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';

import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';

import CSSTransitionGroup from 'react-transition-group/CSSTransitionGroup';
let $CTG=CSSTransitionGroup;
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { websocket_reqTrack, websocket_autoReconnect,xstate_GetCurrentMainState,GetObjElement,websocket_aliveTracking,ConsumeQueue,PostfixExpCalc,Exp2PostfixExp,round,dictLookUp,CircularCounter} from 'UTIL/MISC_Util';
import { MW_API } from "REDUX_STORE_SRC/middleware/MW_API";

// import LocaleProvider from 'antd/lib/locale-provider';

import Modal from "antd/lib/modal";
import Divider from 'antd/lib/divider';
import APPMain_rdx from './MAINUI';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import BPG_WS from './comm/BPG_WS';
import { initDiag, downloadDiag, diagCount, diagText } from 'UTIL/diagLog';
import { persistPending, deletePending, getPendingBySource, pendingInsertCount } from 'UTIL/inspDBQueue';
import { applyMeasureLimitCoupling } from 'JSSRCROOT/shapes/measure/index.js';
import { loadDefWithImageFallback } from 'UTIL/DefLoadWithImageFallback';
import { Shape_Attr_Fill } from 'UTIL/InspectionEditorLogic';
initDiag(); // start capturing console output into the diagnostics ring buffer ASAP

import { default as AntButton } from 'antd/lib/button';
import Collapse from 'antd/lib/collapse';
import Menu from 'antd/lib/menu';

import { 
  AimOutlined,
  CameraOutlined,
  MinusOutlined,
  DatabaseOutlined,
  CloudSyncOutlined,
  CloudUploadOutlined,
  RobotOutlined,
  StockOutlined} from '@ant-design/icons';

import { useSelector,useDispatch } from 'react-redux';
import Button from 'antd/lib/button';
import Drawer from 'antd/lib/drawer';
import Input from 'antd/lib/input';
import Switch from 'antd/lib/switch';
import InputNumber from 'antd/lib/input-number';
import CoreLogPanel from './component/CoreLogPanel';

var require=require||(()=>undefined);

const electron = require('electron')
const fs = require('fs');
const path = require('path')

// Logging facade — all per-file `mkLog(ns)` go through here. initLogger reads
// localStorage `logLevel` / `logLevel:<ns>` (or `?logLevel=...` URL param) and
// applies the per-namespace defaults from UTIL/logger.js's NAMESPACES registry.
// Runs AFTER initDiag() above so loglevel rebinds its method refs to the
// diag-wrapped console (the R-quick-wins #7 fix is now built into initLogger
// since every logger is created lazily via mkLog post-initDiag).
import { initLogger, mkLog } from 'UTIL/logger';
initLogger();
const log = mkLog('ui.main');
const dbLog = mkLog('comm.db'); // DB_WS / SLID API queue chatter
const perifLog = mkLog('ui.uinsp2'); // uInspESP32 link health (2nd-gen sorter only)

// import moment from 'moment';
// import 'moment/locale/fr';
// moment.locale('fr');


let StoreX = ReduxStoreSetUp({});
// Dev-only handle for the webctl harness / regression tooling. Not present in production builds.
if (typeof __DEV_MODE__ !== "undefined" && __DEV_MODE__) {
  window.__GP_STORE__ = StoreX;
  window.__GP_DEF__ = () => StoreX.getState().UIData.edit_info?._obj?.GenerateFeature_sig360_circle_line?.();
  // Load a def + its paired image by path through the real core LD flow (mirrors
  // DefConfUI.loadDefFile) — used by tools/webctl/golden.mjs for a faithful oracle.
  window.__GP_LOAD_BY_PATH__ = (defModelPath) => new Promise((resolve, reject) => {
    const CORE_ID = StoreX.getState().ConnInfo.CORE_ID;
    loadDefWithImageFallback({
      defModelPath,
      defExtension: DEF_EXTENSION,
      downSampLevel: 1,
      timeoutMs: 8000,
      send: (payload, promiseCBs) => {
        StoreX.dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, "LD", 0, payload, undefined, promiseCBs));
      },
    }).then(({ pkts }) => {
      StoreX.dispatch({
        type: "ATBundle", ActionThrottle_type: "express",
        data: pkts.map(pkt => { let act = BPG_Protocol.map_BPG_Packet2Act(pkt); if (act) act.IGNORE_DEFCONF_LOCK = true; return act; }).filter(Boolean)
      });
      resolve(true);
    }).catch(reject);
  });
  // Test hooks for the diagnostics ring buffer + local failed-insert queue.
  window.__GP_DIAG__ = { downloadDiag, diagCount, diagText };
  window.__GP_DB_QUEUE__ = { persistPending, deletePending, getPendingBySource, pendingInsertCount };
  window.__GP_BPG__ = BPG_Protocol; // raw framing/decode (raw2header, raw2Obj_IM, ...) for QA
  window.__GP_MEASURE__ = { applyMeasureLimitCoupling, Shape_Attr_Fill }; // pure value<->limit coupling + per-shape defaults for QA
  window.__GP_UTIL__ = { PostfixExpCalc, Exp2PostfixExp, round, GetObjElement, dictLookUp, CircularCounter, ConsumeQueue }; // pure utils for QA
  window.__GP_LOG__ = log; // loglevel module — for QA to verify the diag ring captures loglevel output
}

// Global safety net for async errors that React error boundaries cannot catch
// (timer/event-handler throws, WS callbacks, unhandled promise rejections).
// Use console.error directly (NOT loglevel): diagLog wraps the console methods, but
// loglevel binds its own console reference and can bypass that wrap — so going
// straight to console.error guarantees these land in the diagnostics ring buffer,
// the floor-unit's only signal when no devtools are attached.
if (typeof window !== "undefined") {
  window.addEventListener("error", (e) => {
    console.error("window.onerror:", (e && (e.error || e.message)), (e && e.filename), (e && e.lineno));
  });
  window.addEventListener("unhandledrejection", (e) => {
    console.error("unhandledrejection:", (e && e.reason));
  });
}

function System_Status_Display({ style={}, showText=false,iconSize=50,gridSize,onItemClick=_=>_})
{
  // const Insp_DB_W_ID = useSelector(state => state.UIData.Insp_DB_W_ID);
  // const WS_InspDataBase_conn_info = useSelector(state => state.UIData.WS_InspDataBase_conn_info);
  
  const DICT = useSelector(state => state.UIData.DICT);
  const ConnInfo = useSelector(state => state.ConnInfo);

  // console.log(ConnInfo);

  
  // useEffect(() => {
    
  //   let newStatus={...systemConnectState};
  //   let cameraStatus = GetObjElement(coreStatus, ["camera_info",0,"cam_status"])===0;

  //   if(DEV_MODE==true&&cameraStatus===false)cameraStatus=true; 
  //   newStatus.camera=cameraStatus;


  //   if(systemConnectState.camera!=cameraStatus)//JUST for TEST mode force to use fake camera
  //   {
  //     setSystemConnectState(newStatus);
  //     onStatusChange(newStatus)
  //   }

  //   onStatusTick(newStatus)
  // },[coreStatus])

  
  if(gridSize===undefined)gridSize=iconSize+50;
  // height:auto + whiteSpace:normal so the full label can wrap instead of being
  // clipped by the antd button's default nowrap (運算核心 -> 運算核, HikCam -> HikCa).
  let gridStyle={...style,width:(gridSize)+"px", height:"auto", whiteSpace:"normal" };
  
  let iconStyle={width:iconSize+"px",height:iconSize+"px"};



  
  function connectionStatus2CSSColor(conn_info)
  {
    let connType=GetObjElement(conn_info,["type"]);
    // console.log(conn_info,connType);
    switch(connType)
    {
      case "WS_CONNECTED":
        return "color-online-anim";
        break;
      case "WS_DISCONNECTED":
        return "color-offline-anim";
        break;
      case "WS_ERROR":
        return "color-error-anim";
        break;
      default:
        return "color-noresource-anim";
        break;
    }
    
  }
  
  // console.log(ConnInfo);

  return [
    [dictLookUp("core", DICT),   ConnInfo.CORE_ID_CONN_INFO,        <AimOutlined/>,true],
    [dictLookUp("camera", DICT), ConnInfo.CAM1_ID_CONN_INFO,        <CameraOutlined/>,true],
    ["設定DB",    ConnInfo.DefFile_DB_W_ID_CONN_INFO,<CloudUploadOutlined/>,true],
    ["檢測DB",    ConnInfo.Insp_DB_W_ID_CONN_INFO,   <CloudUploadOutlined/>,true],
    [undefined,            undefined,                  <MinusOutlined />,ConnInfo.uInsp_API_ID_CONN_INFO!==undefined || ConnInfo.uInspESP32_API_ID_CONN_INFO!==undefined || ConnInfo.SLID_API_ID_CONN_INFO!==undefined||ConnInfo.CNC_API_ID_CONN_INFO!==undefined],//seg line
    ["全檢設備",       ConnInfo.uInsp_API_ID_CONN_INFO,   <RobotOutlined />,false],
    ["全檢設備v2",     ConnInfo.uInspESP32_API_ID_CONN_INFO, <RobotOutlined />,false],
    ["坡檢設備",       ConnInfo.SLID_API_ID_CONN_INFO,    <StockOutlined />,false],
    ["CNC設備",       ConnInfo.CNC_API_ID_CONN_INFO,    <RobotOutlined />,false],
    ]
    .filter(([textName, conn_info, icon,froceAppear])=> (froceAppear|| conn_info!==undefined) && !(showText && textName===undefined))
    .map(([textName, conn_info, icon,froceAppear],idx)=>{
      let brief_info= GetObjElement(conn_info,["brief_info"]);
      return(
      <Button size="large" key={`stat ${textName} ${idx}`} style={gridStyle} 
      type="text" //disabled={!systemConnectState.core}
      className={"s HXA "+connectionStatus2CSSColor(conn_info)} 
      onClick={()=>onItemClick(conn_info)}>
        <div 
          className={"antd-icon-sizing veleX"} 
          style={iconStyle}
        >
          {icon}
        </div>
            {(showText==false)?null:
              <>
                <span className="veleX" style={{whiteSpace:"normal", wordBreak:"break-word", textAlign:"center", lineHeight:1.15, display:"block"}}>{textName}<br/>{brief_info}</span>

              </>}
      </Button>)})

}

                    


// Fake-camera (BMP_carousel) live control panel. Reads camera info from
// Redux directly so the file list & current index stay in sync between
// renders (CAM1_ID_CONN_INFO[0].carousel is refreshed by _queryCam).
const BMP_CAROUSEL_FOLDER_LSKEY = "bmp_carousel_folder_path";
const BMP_CAROUSEL_FPS_LSKEY    = "bmp_carousel_fps";
const BMP_CAROUSEL_AUG_LSKEY    = "bmp_carousel_aug";

// Headless companion: pushes the saved BMP_carousel folder + fps + aug to core
// once the camera is detected as a BMP_carousel -- independent of whether the
// fake-camera Drawer has been opened. Renders nothing. autoAppliedRef + the
// per-folder/fps/aug guards prevent re-pushing on every camera_info refresh.
function BMPCarouselAutoBoot({ camInfo, coreId, ws_send_bpg }) {
  const car  = camInfo?.data?.[0]?.carousel;
  const type = camInfo?.data?.[0]?.type;
  const sentRef = React.useRef({ folder: false, fps: false, aug: false });
  React.useEffect(() => {
    if (type !== "CameraLayer_BMP_carousel") return;
    if (!car || !coreId || !ws_send_bpg) return;
    const send = (action, extra) => ws_send_bpg(coreId, "RC", 0,
      { target: "bmp_carousel", action, ...extra }, undefined,
      { resolve: () => {}, reject: () => {} });

    if (!sentRef.current.folder) {
      const saved = (localStorage.getItem(BMP_CAROUSEL_FOLDER_LSKEY) || "").trim();
      if (saved && saved !== car.folder) send("setfolder", { folder: saved });
      sentRef.current.folder = true;
    }
    if (!sentRef.current.fps) {
      const fps = parseFloat(localStorage.getItem(BMP_CAROUSEL_FPS_LSKEY));
      if (Number.isFinite(fps) && fps > 0 && fps !== car.fps) send("setfps", { fps });
      sentRef.current.fps = true;
    }
    if (!sentRef.current.aug) {
      try {
        const aug = JSON.parse(localStorage.getItem(BMP_CAROUSEL_AUG_LSKEY) || "{}");
        if (aug && Object.keys(aug).length > 0) send("setaug", { aug });
      } catch {}
      sentRef.current.aug = true;
    }
  }, [type, car?.folder, car?.fps, coreId]);
  return null;
}

function BMPCarouselFPS({ curFps, send }) {
  const saved = parseFloat(localStorage.getItem(BMP_CAROUSEL_FPS_LSKEY));
  const [fps, setFps] = React.useState(
    () => Number.isFinite(saved) && saved > 0 ? saved : (curFps || 1));
  React.useEffect(() => {
    if (Number.isFinite(saved) && saved > 0) send("setfps", { fps: saved });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  const apply = (v) => {
    if (!v || v <= 0) return;
    setFps(v);
    localStorage.setItem(BMP_CAROUSEL_FPS_LSKEY, String(v));
    send("setfps", { fps: v });
  };
  return (
    <div style={{display:'flex', alignItems:'center', gap:6}}>
      <span style={{minWidth:60}}>FPS</span>
      <InputNumber size="small" value={fps} min={0.1} max={120} step={0.5}
        onChange={apply}/>
      <span style={{opacity:0.6, fontSize:12}}>core: {(curFps ?? 0).toFixed?.(2)}</span>
    </div>
  );
}

// Fake-camera augmentation knob block. Reads server-current aug from
// camInfo (so first paint reflects core defaults), then user-edits are
// merged into localStorage AND pushed to core via setaug.
function BMPCarouselAugPanel({ aug, send }) {
  const [override, setOverride] = React.useState(() => {
    try { return JSON.parse(localStorage.getItem(BMP_CAROUSEL_AUG_LSKEY) || "{}"); }
    catch { return {}; }
  });
  // On first mount, re-push saved overrides so core matches the UI.
  React.useEffect(() => {
    if (override && Object.keys(override).length > 0) send("setaug", { aug: override });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  const eff = { ...aug, ...override };
  const set = (patch) => {
    const next = { ...override, ...patch };
    setOverride(next);
    localStorage.setItem(BMP_CAROUSEL_AUG_LSKEY, JSON.stringify(next));
    send("setaug", { aug: patch });
  };
  const Row = ({ enKey, en, valKey, val, label, min, max, step }) => (
    <div style={{display:'flex', alignItems:'center', gap:6}}>
      <Switch size="small" checked={!!en} onChange={(v)=>set({[enKey]:v})}/>
      <span style={{minWidth:140}}>{label}</span>
      <InputNumber size="small" disabled={!en}
        value={val} min={min} max={max} step={step}
        onChange={(v)=>set({[valKey]:v})}/>
    </div>
  );
  return (
    <div style={{display:'flex', flexDirection:'column', gap:4,
                 padding:6, border:'1px dashed #555'}}>
      <div style={{opacity:0.7, fontSize:12, marginBottom:2}}>Augmentations</div>
      <Row enKey="brightness_jitter_en"  en={eff.brightness_jitter_en}
           valKey="brightness_jitter_pct" val={eff.brightness_jitter_pct}
           label="brightness ±%" min={0} max={100} step={1}/>
      <Row enKey="rotate_en" en={eff.rotate_en}
           valKey="rotate_step_deg" val={eff.rotate_step_deg}
           label="rotate deg/frame" min={0} max={10} step={0.05}/>
      <Row enKey="noise_en" en={eff.noise_en}
           valKey="noise_range" val={eff.noise_range}
           label="pixel noise ±" min={0} max={64} step={1}/>
      <Row enKey="y_offset_en" en={eff.y_offset_en}
           valKey="y_offset_r" val={eff.y_offset_r}
           label="y-wobble radius px" min={0} max={500} step={1}/>
    </div>
  );
}

function BMPCarouselPanel({ camInfo, coreId, cam1Id, ws_send_bpg }) {
  const car = camInfo?.data?.[0]?.carousel;
  const send = (action, extra={}) => ws_send_bpg(coreId, "RC", 0,
    { target: "bmp_carousel", action, ...extra }, undefined, {
      resolve: () => {}, reject: () => {},
    });
  const [folderInput, setFolderInput] = React.useState(
    () => localStorage.getItem(BMP_CAROUSEL_FOLDER_LSKEY) || (car?.folder ?? ""));
  // Auto-apply the saved folder once when the drawer mounts, but only if it
  // actually differs from the core's currently-loaded folder. Guarded by a
  // ref so we don't re-push on every camera_info refresh.
  const autoAppliedRef = React.useRef(false);
  React.useEffect(() => {
    if (autoAppliedRef.current) return;
    const saved = (localStorage.getItem(BMP_CAROUSEL_FOLDER_LSKEY) || "").trim();
    if (!saved) return;
    if (car && car.folder === saved) { autoAppliedRef.current = true; return; }
    autoAppliedRef.current = true;
    send("setfolder", { folder: saved });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [car?.folder]);
  const applyFolder = () => {
    const f = (folderInput || "").trim();
    if (!f) return;
    localStorage.setItem(BMP_CAROUSEL_FOLDER_LSKEY, f);
    send("setfolder", { folder: f });
  };
  if (!car) return (
    <div style={{display:'flex', flexDirection:'column', gap:8}}>
      <div>(no carousel info yet — waiting for camera_info)</div>
      <Input.Search
        placeholder="image folder path"
        enterButton="Set"
        value={folderInput}
        onChange={(e)=>setFolderInput(e.target.value)}
        onSearch={applyFolder} />
    </div>
  );
  const files = car.files || [];
  const shortName = (p) => (p||"").split("/").pop();
  return (
    <div style={{display:'flex', flexDirection:'column', gap:8}}>
      <Input.Search
        placeholder="image folder path"
        enterButton="Set"
        value={folderInput}
        onChange={(e)=>setFolderInput(e.target.value)}
        onSearch={applyFolder} />
      <div style={{opacity:0.7, fontSize:12}}>folder: {car.folder}</div>
      <div>
        <span style={{marginRight:8}}>frame</span>
        <b>{car.index + 1}</b> / {files.length}
        <div style={{fontSize:12, opacity:0.7}}>{shortName(car.file)}</div>
      </div>
      <BMPCarouselFPS curFps={car.fps} send={send} />
      <div style={{display:'flex', gap:6, flexWrap:'wrap'}}>
        <Button onClick={()=>send("prev")}>◀ Prev</Button>
        <Button onClick={()=>send("replay")}>↻ Replay</Button>
        <Button onClick={()=>send("next")}>Next ▶</Button>
        <Button onClick={()=>send("pause")}>⏸ Pause</Button>
        <Button onClick={()=>send("resume")}>▶ Resume</Button>
      </div>
      <BMPCarouselAugPanel aug={car.aug} send={send} />
      <div style={{maxHeight:'40vh', overflowY:'auto', border:'1px solid #333', padding:6}}>
        {files.map((f, i) => (
          <div key={f}
            onClick={()=>send("jump", { index: i })}
            style={{
              cursor:'pointer',
              padding:'2px 6px',
              background: i===car.index ? '#2a5' : 'transparent',
              color: i===car.index ? 'white' : 'inherit',
            }}>
            {i + 1}. {shortName(f)}
          </div>
        ))}
      </div>
    </div>
  );
}

class APPMasterX extends React.Component {

  static mapDispatchToProps(dispatch, ownProps) {
    return {
      ACT_Ctrl_SM_Panel: (args) => dispatch({ type: UIAct.UI_SM_EVENT.Control_SM_Panel, data: args }),
      ACT_WS_REGISTER: (id, api)=>dispatch(UIAct.EV_WS_REGISTER(id,api)),
      ACT_WS_GET_OBJ: (id, callback)=>dispatch(UIAct.EV_WS_GET_OBJ(id,callback)),
      ACT_WS_CONNECT: (id, url,return_cb) =>dispatch(UIAct.EV_WS_CONNECT(id, url,return_cb)),
      ACT_WS_CLOSE: (id)=>dispatch(UIAct.EV_WS_CLOSE(id)),
      DISPATCH: (act) => {
        dispatch(act)
      },
      DISPATCH_flush: (act) => {
        act.ActionThrottle_type = "flush";
        dispatch(act)
      },
      ACT_WS_SEND_BPG: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(id, tl, prop, data, uintArr, promiseCBs)),
      ACT_MachTag_Update: (machTag) => { dispatch(DefConfAct.MachTag_Update(machTag)) },
      ACT_Machine_Custom_Setting_Update: (info) => dispatch(UIAct.EV_machine_custom_setting_Update(info)),
      ACT_System_Setting_Update: (sysSetting) => dispatch({type:"System_Setting_Update",data:sysSetting}),
    }
  }
  static mapStateToProps(state) {
    return {
      showSM_graph: state.UIData.showSM_graph,
      stateMachine: state.UIData.sm,
      CORE_ID: state.ConnInfo.CORE_ID,
      Insp_DB_W_ID: state.ConnInfo.Insp_DB_W_ID,
      DefFile_DB_W_ID:state.ConnInfo.DefFile_DB_W_ID,
      CAM1_ID:state.ConnInfo.CAM1_ID,
      CAM1_ID_CONN_INFO:state.ConnInfo.CAM1_ID_CONN_INFO,
      CORE_ID_CONN_INFO:state.ConnInfo.CORE_ID_CONN_INFO,
      uInsp_API_ID:state.ConnInfo.uInsp_API_ID,

      uInspESP32_API_ID:state.ConnInfo.uInspESP32_API_ID,
      uInspESP32_API_ID_CONN_INFO:state.ConnInfo.uInspESP32_API_ID_CONN_INFO,

      SLID_API_ID:state.ConnInfo.SLID_API_ID,
      SLID_API_ID_CONN_INFO:state.ConnInfo.SLID_API_ID_CONN_INFO,


      CNC_API_ID:state.ConnInfo.CNC_API_ID,
      CNC_API_ID_CONN_INFO:state.ConnInfo.CNC_API_ID_CONN_INFO,


      Platform_API_ID:state.ConnInfo.Platform_API_ID,

      System_Setting:state.UIData.System_Setting,
      C_STATE: state.UIData.c_state,
      
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
      show_system_panel:true,
      modal_view:undefined,
      carousel_drawer_open:false,
    };

    log.debug("[boot] electron/fs/path host probe", { electron: !!electron, fs: !!fs, path: !!path });


    let localUrl =window.location.href;
    let rootUrl="localhost";
    if(!localUrl.startsWith("file"))
    {
      let matchRes = (/[\/]+(.+)(\:|\?.+)/gm).exec(localUrl);
      // matchRes is null on port-less + query-less URLs (e.g. http://host/)
      // — used to throw and blank-screen the app. Fall back to the hostname.
      if (matchRes && matchRes[1]) rootUrl = matchRes[1];
      else if (window.location && window.location.hostname) rootUrl = window.location.hostname;
    }
    // core ws port: 4090 = latest dev backend (default); override via
    // localStorage.setItem("coreport","4190") (stable old-backup backend) for
    // verification when the dev backend is down. (Avoid a URL query — it breaks the
    // rootUrl parse above.)
    let corePort = (typeof localStorage !== "undefined" && localStorage.getItem("coreport")) || "4090";
    this.coreUrl="ws://"+rootUrl+":"+corePort;
    this.sideBootUrl="ws://"+rootUrl+":5678";
    //this.state.do_splash=true;
    
    this.WSDataDispatch = this.WSDataDispatch.bind(this);

    this.props.ACT_System_Setting_Update(GetDefaultSystemSetting());

    let comp=this;


    

    class DB_WS
    {



      _insertOK(dataInfo,msg)
      {
        // console.log("OK",dataInfo,msg);
        // Insert confirmed by the remote DB — drop its durable copy. dataInfo
        // carries the IndexedDB key promise stamped in send().
        if (dataInfo && dataInfo.__idbSeqP) {
          dataInfo.__idbSeqP
            .then((seq) => deletePending(seq))
            .catch((e) => log.error("deletePending failed", e));
        }
      }
      _insertFailed(data,msg)
      {
        // console.log("FAILED",data,msg);
        // The record stays in IndexedDB (persisted in send(), unconfirmed) and in
        // the in-memory queue for retry — nothing to do here. A reload-orphaned
        // record is replayed at construction via getPendingBySource().
      }

      constructor(id)
      {
        this.id=id;
        this.QWindow= {};
        this.pgIDCounter= 0;
        this.websocket=undefined;
        this.dataSentCount=0;
        this.curr_ws_state=undefined;
        
        this.websocket=new websocket_aliveTracking({
          onStateChange:(ns,os,act)=>
          {
            this.curr_ws_state=ns;
            let info={
              URL:this.websocket.getURL(),
              ns,os,act
            }
            if(ns=="ERROR")
            {
              info.errorInfo=this.websocket.getErrorInfo();
              comp.props.DISPATCH({type:"WS_ERROR",id,data:info})
            }
            else if(ns=="CONNECTED")//enter connection state
            {
              this.cQ.kick();//when connected kick start
              comp.props.DISPATCH({type:"WS_CONNECTED",id,data:info})
            }
            else if(os=="CONNECTED")//exit connection state
            {
              comp.props.DISPATCH({type:"WS_DISCONNECTED",id,data:info})
            }
            dbLog.debug("[state]", { id, from: os, to: ns, via: act });
          },
          binaryType:"arraybuffer"
        });

        setInterval(()=>{
          let info=`${this.getDataQueueCount()}>${this.getDataSentCount()}`;

          // console.log(info);
          comp.props.DISPATCH({type:"WS_UPDATE",id,brief_info:info});
        },5000)

          
        // this.websocket.onopen=(ev)=>{
        //   console.log(ev);
        // }
        // this.websocket.onclose=(ev)=>{
        //   console.log(ev);
        //   comp.props.DISPATCH({type:"WS_DISCONNECTED",id:comp.props.Insp_DB_W_ID})
        // }
        // this.websocket.error=(ev)=>{
        //   console.log(ev);
        //   comp.props.DISPATCH({type:"WS_DISCONNECTED",id:comp.props.Insp_DB_W_ID})
        // }
        this.websocket.onmessage=(ev)=>{
          dbLog.debug("[onmessage]", ev && ev.data);
        }

        let _this=this;
        this.cQ = new ConsumeQueue(
          (cQ) => 
          new Promise((resolve, reject) => {//Implement consume rules
            //resolve() will kick next consume
            //reject will stop kick next consume you will need to do it manually

            // this.websocket.send_obj(data)
            if(_this.curr_ws_state!=="CONNECTED" || cQ.size() == 0)
            {
              reject();
              _this._insertFailed(undefined, "DB/Connection issue/Data empty");
              return;
            }

            // console.log(cQ.size());
    
            let dataInfo = cQ.head();//get the latest element
            if (dataInfo === undefined)//try next data
            {
              resolve();
              _this._insertFailed(undefined,  "Data empty");
              return;
            }


            let data=dataInfo.data;

            
            var msg_obj = {
              dbcmd: { "db_action": "insert", "checked": true },
              data
            };
            let timeoutFlag = setTimeout(() => {//insert timeout will not continue push
              timeoutFlag = undefined;
              dbLog.warn("[insert-timeout]", { id });
              reject("Timeout");

              _this._insertFailed(data, "Timeout");
            }, 5000);

            //The second param is replacer for stringify, and we replace any value that has toFixed(basically 'Number') to replace it to toFixed(5)
            dbLog.debug("[insert-send]", msg_obj);
            _this.websocket.send_obj(msg_obj, (key, val) => typeof val === 'number' ? Number(val.toFixed(5)) : val).
              then((ret) => {
                clearTimeout(timeoutFlag);
                this.retryQCount = 0;
                _this.dataSentCount++;
                cQ.deQ();//pop one data out
                resolve();
                _this._insertOK(dataInfo, ret);
                dataInfo.resolve(ret)
              }).catch((e) => {//Failed retry....
                clearTimeout(timeoutFlag);
                this.retryQCount++;
                // if(this.retryQCount>10)
                // {
                //   resolve();
                //   //reject();
                // }
                // else
                {
                  // console.log("REQ::::",msg_obj);
                  // cQ.enQ(dataInfo);//failed.... put it back and try again
                  resolve();
                }
    
                _this._insertFailed(dataInfo  , e);
              });
          })
          ,1000,
          (cQ)=>{//onTerminationState
            //dump the data out
            dbLog.warn("[queue-terminated] dumping", { size: cQ.size() });
            while(true)
            {
              let data = cQ.deQ();
              if(data===undefined)
              {break;}
              data.reject("The Q is teminated....");
            }
          }
        );

        // Replay records left over from a previous session/reload that were never
        // confirmed: re-queue them (without re-persisting) so they send once the
        // socket connects. Live disconnect retries are handled by cQ.kick() above.
        getPendingBySource(this.id)
          .then((items) => {
            if (items.length) log.info("DB_WS[" + this.id + "] replaying " + items.length + " buffered inserts");
            items.forEach((it) => { try { this.send({ data: it.record, __replaySeq: it.seq }); } catch (e) { log.error("replay send failed", e); } });
          })
          .catch((e) => log.error("getPendingBySource failed", e));


        // setInterval(()=>{
        //   this.send({
        //     data:{type:"PING"}
        //   })
        //   .then(ret=>{
        //     console.log(ret);
        //   })
        //   .catch(e=>{
        //     console.log(e);
        //   })
        // },1000);


        // setTimeout(()=>{
        //   this.cQ.termination();
        // },20000)
      }

      connect(info)
      {

        let url = info.url;
        dbLog.info("[connect]", { id: this.id, url });
        this.websocket.RESET(url);//the url may be undefined
      }
      onmessage(evt){

      }

      getDataSentCount()
      {
        return this.dataSentCount;
      }
      getDataQueueCount()
      {
        return this.cQ.size();
      }

      send(info)
      {
        dbLog.debug("[send-info]", info);
        let data = info.data;

        // Durable mirror: persist the record before queuing so it survives a
        // disconnect or page reload; the key promise is stamped on the queue
        // entry and deleted on insert-OK. On replay (info.__replaySeq set) the
        // record is already persisted, so reuse its key instead of re-persisting.
        let __idbSeqP = (info.__replaySeq != null)
          ? Promise.resolve(info.__replaySeq)
          : persistPending(data, this.id).catch((e) => { log.error("persistPending failed", e); return null; });

        // if(isInQueue==true)
        {
          let prom=new Promise((resolve, reject) => {

            if (!this.cQ.enQ({data,resolve,reject,__idbSeqP}))//If enQ NOT success
            {
              //Just print
              log.error("enQ failed size()=" + this.cQ.size());
              this._insertFailed(data, "Cannot enQ the data"); // bugfix: was undefined `x` → ReferenceError, reject() never ran (promise hung)
              reject("send insert failed");
            }
            else
            {
              dbLog.debug("[queue-size]", this.cQ.size());
            }
            if (this.cQ.size() > 0)
              this.cQ.kick();//kick transmission


          });
          dbLog.debug("[enq]", info);
          return prom;
        }
        // else
        // {
        //   return this.websocket.send_obj(data);
        // }
      }


      close()
      {
        this.websocket.close();
      }
    }

    this.props.ACT_WS_REGISTER(this.props.Insp_DB_W_ID,new DB_WS(this.props.Insp_DB_W_ID));
    this.props.ACT_WS_REGISTER(this.props.DefFile_DB_W_ID,new DB_WS(this.props.DefFile_DB_W_ID));



    this.props.ACT_WS_REGISTER(this.props.CORE_ID,new BPG_WS(comp, StoreX));
    this.props.ACT_WS_CONNECT(this.props.CORE_ID, this.coreUrl)




    class Cam_Stat_Query{
      camDisconnectionAction()
      {
        // if(cur_state==null)return;
        // let curState_EX=xstate_GetCurrentMainState(cur_state);
        // console.log(cur_state,curState_EX.state,UIAct.UI_SM_STATES.INSP_MODE);
        // //console.log(sys_state);
        // if(sys_state.camera==false&&curState_EX.state==UIAct.UI_SM_STATES.INSP_MODE)
        // {
        //   ACT_EXIT();
        // }
          
      }

      
      _queryCam(resolve,reject)
      {
        log.debug("[queryCam] System_Setting", comp.props.System_Setting);
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "GS", 0, { items: ["camera_info"] },
        undefined, 
        {
          resolve: (stacked_pkts,P) => {
            
            let GS=stacked_pkts.find(pkt=>pkt.type=="GS");
            if(GS!==undefined)
            {
              let camInfo = GetObjElement(GS,["data","camera_info"]);


              let cam0=GetObjElement(camInfo,[0,"type"]);


              let isInOperation=true;

              if(cam0===undefined || (comp.props.System_Setting.ALLOW_SOFT_CAM==false && cam0.includes("CameraLayer_BMP")))
              {
                isInOperation=false;
              }

              
              if(GetObjElement(camInfo,[0,"cam_status"])!=0)
              {
                isInOperation=false;
              }

              if(!isInOperation)
              {
                this.isConnected=false;
                StoreX.dispatch({type:"WS_ERROR",id:comp.props.CAM1_ID,data:camInfo});
                
                this.camDisconnectionAction();
                
                this.reconnection();
                reject(stacked_pkts,P);
                // this.queryTimeOut=setTimeout(()=>{
                //   this.queryCam(timeout_ms);
                // },timeout_ms*2);
              }
              else
              {
                
                let camName=GetObjElement(camInfo,[0,"name"]);
                // StoreX.dispatch({type:"WS_CONNECTED",id:comp.props.CAM1_ID,data:camInfo});
                let ev_type=(this.isConnected===false)?"WS_CONNECTED":"WS_UPDATE";
                comp.props.DISPATCH({type:ev_type,id:comp.props.CAM1_ID,data:camInfo,brief_info:camName});
                this.isConnected=true;

                resolve(stacked_pkts,P);

              }
              // console.log(camInfo);
              
            }
            
          },
          reject:(e)=>{
            StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.CAM1_ID,data:e});

            this.isConnected=false;
            this.camDisconnectionAction();

            resolve(e);
            // this.queryTimeOut=setTimeout(()=>{
            //   this.queryCam(timeout_ms);
            // },1000);
          }
        });

      }

      reconnection()
      {
        if(this.isInReconn==true)
        {
          return false;
        }
        this.isInReconn=true;



        StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.CAM1_ID});
        this.isConnected=false;
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "RC", 0, {
          target: "camera_ez_reconnect"
        },
        undefined, { 
          resolve:(ret)=>{
            this.isInReconn=false;

            
            this._queryCam(
              ()=>{},
              ()=>{})
          }, 
          reject:()=>{
            this.isInReconn=false;
          } })

      }
      queryCam(timeout_ms=2000)
      {
        if(this.isInReconn==true)
        {
          //wait until reconnection action over
          this.queryTimeOut=setTimeout(()=>{
            this.queryCam(timeout_ms);
          },timeout_ms*2);
          return;
        }
        // comp.props.DISPATCH({
        //   type:"MW_API_CALL",id,method:"send",
        //   param:{
            
        //   }
        // });

        this._queryCam(()=>{
          this.queryTimeOut=setTimeout(()=>{
            this.queryCam(timeout_ms);
          },timeout_ms);
        },
        ()=>{
          this.queryTimeOut=setTimeout(()=>{
            this.queryCam(timeout_ms);
          },timeout_ms/2);
        })



      }
      constructor(id)
      {
        this.id=id;
        this.isInReconn=false;
        this.isConnected=false;

        this.queryCam(2000);
      }


      

    }
    this.props.ACT_WS_REGISTER(this.props.CAM1_ID,new Cam_Stat_Query(this.props.CAM1_ID));




    // Transport plumbing shared by every serial peripheral reached through the
    // core's PD channel: connect/reconnect, request/response tracking, the PING
    // watchdog, setting-file load/save and the comm latency probe. Subclasses
    // supply the device's protocol dialect, not the transport.
    class  Perif_API_Base
    {
      constructor(id,settingFilePath,pg_id_channel)
      {
        this.id=id;
        this.settingFilePath=settingFilePath;
        this.pg_id_channel=pg_id_channel;

        this.CONN_ID=undefined;
        this.connInfo=undefined;
        this.inReconnection=false;

        this.trackingWindow={};
        this.idCounter=10;
        this.PINGCount=0;

        this.machineInfo=undefined;

        this.checkReconnectionInterval=setInterval(()=>this.checkReConnection(),3000);//watch dog to do reconnection
        this.runPINGInterval=setInterval(()=>this._sendPing(),3000);//watch dog to do reconnection
      }

      // ---- subclass hooks ------------------------------------------------

      // uInspMEGA's get_setup reply carries no "ack" field, so the check cannot
      // be unconditional -- devices that do send one opt in and avoid storing a
      // nak as if it were settings.
      resyncRequiresAck(){ return false; }

      // PING reply with the envelope fields already stripped.
      onPingStatus(machineStatus)
      {
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,machineStatus});
      }

      onSetupFileLoaded(machInfo){}
      onSetupFileSaved(){}
      onBeforeSetupPush(){}

      // ---- connection ----------------------------------------------------

      cleanUpTrackingWindow()
      {
        let keyList = Object.keys(this.trackingWindow);
        keyList.forEach(key=>{
          let reject = this.trackingWindow[key].reject;
          if(reject !==undefined)
          {
            reject("CONNECTION ERROR");
          }
          delete this.trackingWindow[key]
        })
      }

      cleanUpConnection()
      {
        this.cleanUpTrackingWindow();
      }

      connect(connInfo)
      {
        if(this.inReconnection==true)
        {//still in reconnection state, return
          return false;
        }

        StoreX.dispatch({type:"WS_DISCONNECTED",id:this.id,data:undefined});
        this.connInfo=connInfo;
        this.inReconnection=true;
        this.LoadFileToMachine();
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "PD", 0, {type:"CONNECT",...connInfo, _PGID_: this.pg_id_channel, _PGINFO_: { keep: true }},undefined,
        {
          resolve: (stacked_pkts,action_channal) => {
            let PD=stacked_pkts.find(pkt=>pkt.type=="PD");
            this.inReconnection=false;
            if(PD!==undefined)
            {
              let PD_data=PD.data;
              switch(PD_data.type)
              {
                case "MESSAGE":
                {
                  let CONN_ID = PD_data.CONN_ID;
                  let msg = PD_data.msg;
                  let msg_id = msg!==undefined? msg.id:undefined;
                  let trwin=this.trackingWindow[msg_id];
                  if(trwin!==undefined)
                  {
                    if(trwin.resolve!==undefined)
                      trwin.resolve(msg);
                    delete this.trackingWindow[msg_id];
                  }
                }
                  break;
                case "DISCONNECT":
                  this.CONN_ID=undefined;
                  this.cleanUpConnection();
                  StoreX.dispatch({type:"WS_DISCONNECTED",id:this.id,data:PD});
                  break;
                case "CONNECT":
                  this.CONN_ID=PD_data.CONN_ID;
                  StoreX.dispatch({type:"WS_CONNECTED",id:this.id,data:PD});

                  if(this.machineSetup!==undefined)
                  {
                    this.onBeforeSetupPush();
                    this.send({type:"set_setup",...this.machineSetup},
                    (ret)=>{
                      this.machineSetupReSync();
                    },(e)=>console.log(e));
                  }
                  else
                  {
                    this.machineSetupReSync();
                  }

                  break;
              }
            }
          },
          reject:(e)=>{
            this.CONN_ID=undefined;
            this.inReconnection=false;
            this.cleanUpConnection();
            StoreX.dispatch({type:"WS_DISCONNECTED",id:this.id,data:undefined});
          }
        });
      }

      checkReConnection()
      {
        if(this.connInfo===undefined ||this.CONN_ID!==undefined || this.inReconnection==true)
        {
          return;
        }
        this.connect(this.connInfo);
      }

      // ---- setting file ----------------------------------------------------

      saveMachineSetupIntoFile(filename = this.settingFilePath)
      {
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID,"SV", 0,
          { filename: filename },
          new TextEncoder().encode(JSON.stringify(this.machineSetup, null, 4)),
          {
            resolve:(res)=>{ this.onSetupFileSaved(); },
            reject:(res)=>{ },
          }
        )
      }

      LoadFileToMachine(filename = this.settingFilePath) {
        new Promise((resolve, reject) => {

          log.info("LoadSettingToMachine step2");
          comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID,"LD", 0,
            { filename },
            undefined, { resolve, reject }
          );
          setTimeout(() => reject("Timeout"), 1000)
        }).then((pkts) => {

          log.info("LoadSettingToMachine>> step3", pkts);
          if (pkts[0].type != "FL")
          {
            return;
          }
          let machInfo = pkts[0].data;

          this.machineSetupUpdate(machInfo,true);
          this.onSetupFileLoaded(machInfo);
        }).catch((err) => {

          log.info("LoadSettingToMachine>> step3-error", err);
        })
      }

      // ---- machine setup ---------------------------------------------------

      machineSetupUpdate(newMachineInfo,doReplace=false)
      {
        this.machineSetup=doReplace==true?newMachineInfo:{...this.machineSetup,...newMachineInfo};
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,machineSetup:this.machineSetup});
        this.send(uinspRegroup({type:"set_setup",...newMachineInfo}),
        (ret)=>{
          log.debug("[machine-setup] set_setup ack", ret);
          //HACK: just assume it will work
        },(e)=>log.warn("[machine-setup] set_setup failed", e));
      }

      machineSetupReSync() {
        log.debug("[machine-setup] resync request");
        this.send({type:"get_setup"},
        (ret)=>{
          if(this.resyncRequiresAck() && ret["ack"]!=true)
          {
            log.warn("[machine-setup] get_setup nak", ret);
            return;
          }
          delete ret["type"];
          delete ret["id"];
          delete ret["st"];
          delete ret["ack"];
          // Flatten before anything downstream looks at it: SETTABLE_KEYS is a
          // list of flat names, so a grouped reply matched none of them and the
          // entire configuration was filed as read-only device state.
          this.machineSetup=uinspFlatten(ret);
          this.machineSetupUpdate(this.machineSetup,true);
        },(e)=>console.log(e));
      }

      getMachineSetup()
      {
        return this.machineSetup;
      }

      // ---- request / response ---------------------------------------------

      findAvailableID()
      {
        let id=this.idCounter;
        while(this.trackingWindow[id]!==undefined)
        {
          this.idCounter++;
          if(this.idCounter>999999)
          {
            this.idCounter=0;
          }
          id=this.idCounter;
        }
        return id;
      }

      send(data,resolve,reject)
      {
        if(this.CONN_ID===undefined)
        {
          reject("CONN ID is not set");
          return ;
        }

        if(data.id!==undefined )
        {
          if(this.trackingWindow[data.id]!==undefined)
            reject(`ID ${data.id} collision`);
        }
        else
        {
          data.id=this.findAvailableID();
        }
        this.trackingWindow[data.id]={resolve,reject};

        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "PD", 0, //just send
        {
          msg:data,
          CONN_ID:this.CONN_ID,
          type:"MESSAGE"
        },undefined, {
          resolve:d=>d,
          reject:d=>console.log(d)
        });
      }

      // Promise flavour of send(), for the command helpers subclasses add.
      sendP(data)
      {
        return new Promise((resolve,reject)=>this.send(data,resolve,reject));
      }

      // ---- PING watchdog ---------------------------------------------------

      _sendPing()
      {
        if(this.CONN_ID===undefined)return ;

        if(this.PINGCount>=2)
        {
          //time to disconnect
          this.PINGCount=0;

          this.connect(this.connInfo);
          return;
        }
        this.PINGCount++;

        this.triggerPing();
      }

      triggerPing()
      {
        this.send({type:"PING"},(ret)=>{
          delete ret["type"]
          delete ret["id"]
          delete ret["st"]
          this.onPingStatus({...ret});
          this.PINGCount=0;
        },errorInfo=>console.log(errorInfo));
      }

      // Round-trip latency probe for the peripheral link: WebUI -> core PD packet
      // -> perifCH serial -> device -> reply -> back. Sends `count` SEQUENTIAL
      // PINGs (same transport a light/PIN_CONF command takes) and times each
      // resolve, then reports min/avg/p95/max + per-sample list via onUpdate.
      // Drives the WebUI "通訊診斷" button so field comm-delay can be measured.
      diagnoseComm(count,onUpdate)
      {
        count = count || 20;
        onUpdate = onUpdate || (()=>{});
        let self=this;
        let samples=[], fails=0, i=0, startedAt=new Date().getTime();
        function finish(){
          let sorted=[...samples].sort((a,b)=>a-b);
          let sum=samples.reduce((a,b)=>a+b,0);
          onUpdate({
            done:true, total:count, n:samples.length, fails:fails,
            min: sorted.length?sorted[0]:null,
            max: sorted.length?sorted[sorted.length-1]:null,
            avg: samples.length?Math.round(sum/samples.length):null,
            p95: sorted.length?sorted[Math.min(sorted.length-1,Math.floor(sorted.length*0.95))]:null,
            elapsed: new Date().getTime()-startedAt,
            samples: samples,
          });
        }
        function next(){
          if(i>=count || self.CONN_ID===undefined){ finish(); return; }
          i++;
          let t0=new Date().getTime(), settled=false;
          let to=setTimeout(()=>{ if(settled)return; settled=true; fails++;
            onUpdate({done:false,i:i,total:count,last:null,timeout:true}); next(); },3000);
          self.send({type:"PING"},()=>{
            if(settled)return; settled=true; clearTimeout(to);
            samples.push(new Date().getTime()-t0);
            onUpdate({done:false,i:i,total:count,last:samples[samples.length-1]});
            next();
          },(e)=>{
            if(settled)return; settled=true; clearTimeout(to); fails++;
            onUpdate({done:false,i:i,total:count,last:null,error:String(e)});
            next();
          });
        }
        next();
      }
    }


    // uInspMEGA (Arduino MEGA + W5500). Speaks pulse_hz / res_count and reports
    // sorting throughput derived from successive PING replies.
    class  uInsp_API extends Perif_API_Base
    {
      constructor(id,pg_id_channel=10024)
      {
        super(id,"data/uInspSetting.json",pg_id_channel);

        this.pre_res_count=undefined;
        this.res_count_start_time=undefined;
        this.res_count_pre_time=undefined;

        this.res_count_rate_overall=undefined;
        this.res_count_rate_recent=undefined;
      }

      onSetupFileLoaded(machInfo)
      {
        this.default_pulse_hz = machInfo.pulse_hz;
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,default_pulse_hz:this.default_pulse_hz});
      }

      onSetupFileSaved()
      {
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,default_pulse_hz:this.machineSetup.pulse_hz});
      }

      onBeforeSetupPush()
      {
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,default_pulse_hz:this.default_pulse_hz});
      }

      onPingStatus(machineStatus)
      {
        let res_count=machineStatus.res_count||{OK:0,NG:0,NA:0};

        let currentTime_ms=new Date().getTime();
        if(this.pre_res_count!==undefined)
        {
          if( this.pre_res_count.OK <= res_count.OK &&
            this.pre_res_count.NG <= res_count.NG &&
            this.pre_res_count.NA <= res_count.NA &&
            this.res_count_pre_time!==undefined&&
            this.res_count_start_time!==undefined
            )
          {
            let period_s = (currentTime_ms-this.res_count_pre_time)/1000;
            let period_overall_s = (currentTime_ms-this.res_count_start_time)/1000;
            let period_pre_s=period_overall_s-period_s;
            let OK_rate=(res_count.OK-this.pre_res_count.OK)/period_s;
            let NG_rate=(res_count.NG-this.pre_res_count.NG)/period_s;
            let NA_rate=(res_count.NA-this.pre_res_count.NA)/period_s;
            this.res_count_rate_overall={
              OK:(this.res_count_rate_overall.OK*period_pre_s+OK_rate*period_s)/period_overall_s,
              NG:(this.res_count_rate_overall.NG*period_pre_s+NG_rate*period_s)/period_overall_s,
              NA:(this.res_count_rate_overall.NA*period_pre_s+NA_rate*period_s)/period_overall_s,
            }

            let maxRange=20;
            let offset=1;
            let alpha=period_s>maxRange?1:((period_s+offset)/(maxRange+offset));
            this.res_count_rate_recent={
              OK:(this.res_count_rate_recent.OK*(1-alpha)+OK_rate*alpha),
              NG:(this.res_count_rate_recent.NG*(1-alpha)+NG_rate*alpha),
              NA:(this.res_count_rate_recent.NA*(1-alpha)+NA_rate*alpha),
            }
            this.res_count_pre_time=currentTime_ms;
            this.pre_res_count={...res_count};
          }
          else
          {
            this.pre_res_count=undefined;
          }
        }

        if(this.pre_res_count===undefined)
        {
          this.pre_res_count={...res_count};
          this.res_count_start_time=
          this.res_count_pre_time=currentTime_ms;

          this.res_count_rate_overall={OK:0,NG:0,NA:0};
          this.res_count_rate_recent={OK:0,NG:0,NA:0};
        }

        StoreX.dispatch({type:"WS_UPDATE",id:this.id,machineStatus,result_count_rate_recent:this.res_count_rate_recent});
      }
    }
    this.props.ACT_WS_REGISTER(this.props.uInsp_API_ID,new uInsp_API(this.props.uInsp_API_ID));


    // uInspESP32 (Peripheral/uInspESP32). Distinct protocol from uInspMEGA:
    // plateFreq rather than pulse_hz, stage_pulse_offset for the per-machine
    // camera/light/selector timing, and an explicit inspection-mode state
    // machine. get_setup answers with ack, so the resync check is worth having.
    // Consecutive RESET attempts before falling back to a port reopen. Two is
    // enough for a one-off line glitch; more would just delay the real recovery
    // when the device is genuinely gone (unplugged, crashed, powered down).
    const LINK_RESYNC_MAX = 2;

    class  uInspESP32_API extends Perif_API_Base
    {
      constructor(id,settingFilePath="data/uInspESP32Setting.json",pg_id_channel=10027)
      {
        super(id,settingFilePath,pg_id_channel);
        this.runningStat=undefined;
        this.deviceState={};
        this.cfg={};
        this._resyncTries=0;
        this._linkResyncTries=0;
      }

      resyncRequiresAck(){ return true; }

      // QA/bring-up: flip the frame<->object pairing algorithm at runtime.
      // The switch exists so the two can be compared on the same machine with
      // the same input; reading it only at CONNECT made that comparison cost a
      // board reset, which is why it had never been run.
      // Ignore the real gate sensor while phantom pulses still register.
      // Lets a calibration pulse be fired into a clean lane -- no real part can
      // land in the middle of the measurement. Parts are not lost; they ride
      // round again.
      setGateDisable(on){ return this.sendP({type:"set_gate_disable", on:!!on}); }
      trigPhantomPulse(){ return this.sendP({type:"trig_phantom_pulse"}); }

      setPairingMode(mode)
      {
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "PD", 0,
          { type:"PAIRING_MODE", mode, CONN_ID:this.CONN_ID },
          undefined, { resolve:d=>d, reject:d=>d });
      }

      // Try to un-wedge the serial link before resorting to reopening the port.
      //
      // The base watchdog reconnects after two unanswered PINGs, and reconnect
      // reopens the port, and opening the port toggles DTR, which hard-resets
      // the ESP32. So one corrupted byte on the wire costs a board reset, every
      // object in flight, and the whole run. Observed exactly that: a bad frame,
      // two silent PINGs, channel torn down 9s later, run over at 196 parts.
      //
      // The device parser leaves its framing-error state on one thing only --
      // the RESET_PACKET byte sequence -- which is why a reconnect works: it
      // sends RESET on the way up. The core can send that alone, without
      // touching the port, so try that first and give the link a cycle to come
      // back. Only if it stays silent is the expensive reset justified.
      //
      // Overridden here rather than in Perif_API_Base because that watchdog is
      // shared with the 1st-gen peripherals, whose devices do not necessarily
      // treat RESET the same way.
      _sendPing()
      {
        if(this.CONN_ID===undefined)return;

        if(this.PINGCount>=2)
        {
          if(this._linkResyncTries<LINK_RESYNC_MAX)
          {
            this._linkResyncTries++;
            perifLog.warn("[link] PING unanswered -- trying RESET before reconnect",
                          { attempt:this._linkResyncTries, max:LINK_RESYNC_MAX });
            comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "PD", 0,
              { type:"RESYNC", CONN_ID:this.CONN_ID },
              undefined, { resolve:d=>d, reject:d=>d });
            this.PINGCount=0;      // give the link one more cycle to answer
            this.triggerPing();
            return;
          }
          perifLog.error("[link] RESET did not recover the link -- reconnecting "
                         + "(this resets the board and ends the run)");
          this._linkResyncTries=0;
        }
        else if(this.PINGCount===0)
        {
          this._linkResyncTries=0;   // link is healthy again
        }

        super._sendPing();
      }

      // Opening the serial port toggles DTR, which hard-resets the ESP32. The
      // host then has a live port to a board that is still booting, and the
      // first command out of connect() lands in that window: the board reports
      // it as recv_ERROR:2 with the frame mangled a dozen bytes in
      // ({'type':'get_s<garbage>), because its UART is not up yet. PING survives
      // this by accident -- it repeats every 3s, so one eventually lands. The
      // one-shot config resync does not, which is why the machine panel stayed
      // blank forever while the link itself looked perfectly healthy.
      //
      // So retry until the config actually arrives instead of trusting the first
      // attempt. Cheap (one small command), bounded, and self-cancelling the
      // moment cfg is populated.
      static RESYNC_RETRY_MS=1000;
      static RESYNC_MAX_TRIES=10;

      machineSetupReSync()
      {
        this._resyncTries=0;
        this._resyncKick();
      }

      _resyncKick()
      {
        if(Object.keys(this.cfg||{}).length>0)return;   // got it, stop
        if(this.CONN_ID===undefined)return;             // a reconnect will restart this
        if(this._resyncTries>=uInspESP32_API.RESYNC_MAX_TRIES)
        {
          log.warn("[uInspESP32] config resync gave up after",this._resyncTries,"tries");
          return;
        }
        this._resyncTries++;
        super.machineSetupReSync();
        setTimeout(()=>this._resyncKick(),uInspESP32_API.RESYNC_RETRY_MS);
      }

      // Everything the firmware's set_setup handler actually consumes
      // (LegacyFirmware.cpp, the JSON_SETIF_ABLE block). Anything else in a
      // get_setup reply is read-only runtime state.
      static SETTABLE_KEYS=[
        "plate_freq","plate_accel","speed_band_pct","min_detect_sep_us",
        "pulse_min_width","pulse_max_width",
        "gate_debounce_rise","gate_debounce_fall",
        // The gate's distance rejection. It was mapped in uinspCfg but missing
        // here, so the panel could display it and never write it.
        "min_detect_dist_um",
        "stepper_en_active","stepper_dir",
        "unanswered_stop_after",
        "host_timeout_ms","pulses_per_rev","plate_diameter_mm",
        "stage_pulse_offset","io_on_level",
        // Per-station widths in MICROSECONDS -- the device converts to ticks
        // against its own plate_freq, so a recipe survives a speed change.
        "stage_pulse_width_us",
        // Window CENTRES in ticks. 0 = that station keeps the forward-only
        // shape, where the offset is the window's start.
        "stage_pulse_center",
        // Calibration trigger pulse width, us.
        "cal_pulse_us",
        // The camera clock: match window and the recal/drift settings. All are
        // JSON_SETIF_ABLE targets in set_setup and none of them were listed, so
        // the panel could display them and never write one.
        "cam_match_window_us","cam_recal_idle_ms","cam_drift_comp",
        // The match window expressed as what it actually is -- a position
        // tolerance. Settable on the device since 2026-08-11, unreachable here.
        "cam_match_tolerance_mm",
        "report_match_ts","report_match_pcnt",
        // `auto_rate` and `unanswered_policy` used to live here. The firmware
        // dropped both flat keys on 2026-08-08 when the decision was collapsed
        // into ONE name -- so the UI kept writing two keys that no longer
        // existed and was told ack:true both times. skip_policy_mode is that
        // one name, now "stop_only" | "none" after the slow half was removed.
        "skip_policy_mode",
        "machine_id","CAM1_Tags","CAM2_Tags","persist",
      ];

      // This board owns its own configuration: it keeps a copy in NVS and comes
      // up on it (cfg_from_nvs), so the host has no business re-pushing settings
      // just because it connected. Perif_API_Base does exactly that -- it echoes
      // the whole get_setup reply straight back as set_setup, stripping only the
      // 4 envelope fields, so ver/name/cur_state/step_count/error_hist/cfg_crc/
      // reset_reason/xtal_mhz all get sent to a board that has no use for them.
      //
      // So this override makes the sync READ-ONLY: values are stored for display
      // and never echoed. Settable keys land in machineSetup, runtime state in
      // deviceState (getDeviceState). Writing config is an explicit act --
      // machineSetupUpdateAndPersist() / setMachineId() -- not a side effect of
      // connecting, which also means the NVS copy stays authoritative instead of
      // being silently overwritten by whatever the host happened to cache.
      //
      // Bonus: it keeps the wire quiet. A full config push is ~500-670 bytes and
      // used to arrive corrupted (see the RX-buffer fix in LegacyFirmware.cpp);
      // not sending it at all is the better answer than sending it carefully.
      machineSetupUpdate(newMachineInfo,doReplace=false,push=false)
      {
        let settable={}, readonly={};
        Object.keys(newMachineInfo||{}).forEach(k=>{
          if(uInspESP32_API.SETTABLE_KEYS.indexOf(k)>=0) settable[k]=newMachineInfo[k];
          else readonly[k]=newMachineInfo[k];
        });
        this.deviceState=doReplace?readonly:{...this.deviceState,...readonly};
        this.cfg=doReplace?settable:{...this.cfg,...settable};

        // Actively clear, not merely "leave unset": machineSetupReSync() assigns
        // `this.machineSetup = ret` (the whole get_setup reply) BEFORE it calls
        // this method, so the field is already populated by the time we get here.
        // Perif_API_Base.connect()
        // pushes `send({type:"set_setup",...this.machineSetup})` directly -- not
        // through this method -- whenever the field is set, so the only way to
        // guarantee a reconnect stays read-only is to keep the field empty. The
        // UI is unaffected: it reads machineSetup out of redux, which is fed
        // here, not off the instance.
        this.machineSetup=undefined;

        StoreX.dispatch({type:"WS_UPDATE",id:this.id,
                         machineSetup:this.cfg,
                         deviceState:this.deviceState});
        if(push!==true)return;
        this.send(uinspRegroup({type:"set_setup",...settable}),
          (ret)=>log.debug("[machine-setup] set_setup ack",ret),
          (e)=>log.warn("[machine-setup] set_setup failed",e));
      }

      getMachineSetup(){ return this.cfg; }
      getDeviceState(){ return this.deviceState; }

      // ---- inspection mode -------------------------------------------------

      enterInspMode(){ return this.sendP({type:"enter_insp_mode"}); }
      exitInspMode(){ return this.sendP({type:"exit_insp_mode"}); }
      clearError(){ return this.sendP({type:"clear_error"}); }
      clearErrorHistory(){ return this.sendP({type:"clear_error_history"}); }

      // ---- sorting ---------------------------------------------------------

      // cat 1 -> SEL1, 2 -> SEL2. tid comes from the bT trigger message that
      // announced the object, so a late result cannot be applied to the wrong
      // part.
      report(tid,cat){ return this.sendP({type:"report",tid,cat}); }

      // count of -1 disables the countdown; reaching zero faults the machine.
      setSel1Countdown(count){ return this.sendP({type:"set_sel1_cd",count}); }
      getSel1Countdown(){ return this.sendP({type:"get_sel1_cd"}); }

      // ---- stepper / diagnostics -------------------------------------------

      // Steady backlight for camera setup (exposure / gain / focus need a stable
      // image; the stage machine only ever strobes it for ~600us). Polarity-aware
      // on the firmware side -- do NOT use pin_on/pin_off, those are raw writes
      // and this machine's io_on_level makes ON active-low, so they invert.
      // The board auto-drops the hold on timeout or when it leaves IDLE.
      light(ch,on,timeout_ms){ return this.sendP({type:"light",ch,on,timeout_ms}); }

      // One camera trigger, no pipeline object behind it. This is the shutter,
      // not the sorter: it fires the camera line directly and does NOT create a
      // part, so nothing gets counted, tracked or blown.
      trigCamPulse(){ return this.sendP({type:"trig_cam_pulse"}); }

      // A single lit frame, for setting up on a stopped plate.
      //
      // The backlight and the camera trigger are separate things and the stage
      // machine normally strobes the light for ~600us around each trigger. With
      // the plate stopped nothing strobes it, so a bare trig_cam_pulse returns a
      // BLACK frame -- which reads as "the inspection found nothing" and sends
      // you looking for a fault that is not there.
      //
      // Light first, settle, shoot, then drop it. The timeout is a backstop: if
      // this promise chain dies between on and off, the board drops the hold by
      // itself rather than leaving the panel lit.
      camSnapWithLight(ch="L1A", settle_ms=120){
        return this.light(ch,true,4000)
          .then(()=> new Promise(r=>setTimeout(r,settle_ms)))
          .then(()=> this.trigCamPulse())
          .then((r)=> new Promise(res=>setTimeout(()=>res(r),settle_ms)))
          .then((r)=> this.light(ch,false,0).then(()=>r),
                (e)=> this.light(ch,false,0).then(()=>{throw e;}));
      }

      stepperEnable(){ return this.sendP({type:"stepper_enable"}); }
      stepperDisable(){ return this.sendP({type:"stepper_disable"}); }

      // Station placement. Catch the next part at the gate, then drive it to an
      // absolute offset until it sits where the station should be; the number
      // that comes back is in stage_pulse_offset units and goes straight into
      // SEL1_on / CAM1_on / whatever is being placed.
      //
      // jogGoto is ABSOLUTE on purpose: the device computes the relative move.
      // A browser cannot know where the plate stopped -- braking distance is not
      // predictable from here -- so anything relative would have to read back,
      // subtract, and race the machine.
      jogArm(freq){ return this.sendP({type:"jog_arm", ...(freq?{freq}:{})}); }
      // freq is a CEILING, not a demand: the device still lowers it for a short
      // move, because braking takes f^2/a ticks and a move shorter than twice
      // that would be all deceleration.
      jogGoto(offset,freq){ return this.sendP({type:"jog", offset, ...(freq?{freq}:{})}); }
      jogEnd(){ return this.sendP({type:"jog_end"}); }

      resetRunningStat(){ return this.sendP({type:"reset_running_stat"}); }
      getRunningStat()
      {
        return this.sendP({type:"get_running_stat"}).then(ret=>{
          this.runningStat=ret;
          StoreX.dispatch({type:"WS_UPDATE",id:this.id,runningStat:ret});
          return ret;
        });
      }

      // The enum lives in the firmware, so its text should come from there too.
      // A copy of the table in the UI is a copy that goes stale, and it did:
      // the panel knew five of the ten states, so a machine sitting in CAL
      // showed "state 102". Asked once when the panel opens; old firmware that
      // does not know the command simply leaves the built-in table in place.
      getStateNames(){ return this.sendP({type:"get_state_names"}); }

      // Promise flavour of get_setup, for callers that need to CONFIRM a write
      // landed rather than fire and hope. machineSetupReSync() does the same
      // round trip but swallows the reply into this.machineSetup, so it cannot
      // be awaited or checked.
      //
      // Worth knowing which plate_freq you are reading: get_setup returns
      // PLATE_FREQ_SETPOINT (the configuration, what set_setup writes), while
      // get_running_stat returns PLATE_FREQ_TARGET (the ramp\'s current goal,
      // which stays 0 in IDLE). They are different variables and only one of
      // them can confirm a set_setup.
      // Flattened, like machineSetupReSync's copy. Callers read flat names off
      // this -- the Inspection UI's start gate checks `s.plate_freq > 0` to
      // confirm the speed reached the device -- and a grouped reply makes every
      // one of those checks read undefined. That is the whole of
      // "轉速沒有寫進裝置,未進入檢測模式": the write had in fact landed.
      getSetupP(){ return this.sendP({type:"get_setup"}).then(uinspFlatten); }

      // ---- persistence -----------------------------------------------------

      // The board keeps its own copy in NVS, so a machine that is moved or
      // reflashed still comes up on its own timing rather than whatever the
      // host happened to cache. Persisting is deliberate, not automatic --
      // offset probing during setup should not burn flash cycles.
      saveSetupToDevice(){ return this.sendP({type:"save_setup"}); }
      clearSavedSetupOnDevice(){ return this.sendP({type:"clear_saved_setup"}); }

      // push=true: this is the explicit write path. Everything else -- connect,
      // resync, file load -- is read-only, see machineSetupUpdate.
      machineSetupUpdateAndPersist(newMachineInfo)
      {
        this.machineSetupUpdate(newMachineInfo,false,true);
        return this.saveSetupToDevice();
      }

      setMachineId(machine_id){ return this.sendP({type:"set_setup",machine_id,persist:true}); }
      getMachineId(){ return (this.machineSetup||{}).machine_id; }

      // True when the running config came from the board's NVS rather than the
      // compiled fallback -- an unconfigured board is worth flagging loudly
      // before it starts flinging parts at the wrong bin.
      // cfg_from_nvs is read-only state, so it lives in deviceState now, not in
      // machineSetup -- see machineSetupUpdate above.
      isConfigFromNVS(){ return this.deviceState.cfg_from_nvs===true; }
    }
    {
      let _uInspESP32 = new uInspESP32_API(this.props.uInspESP32_API_ID);
      this.props.ACT_WS_REGISTER(this.props.uInspESP32_API_ID,_uInspESP32);
      // QA/bring-up handle, same spirit as __GP_MEASURE__/__GP_UTIL__ above.
      // Driving the board by hand from the console is the only way to probe one
      // command in isolation -- the periodic PING/resync traffic otherwise makes
      // it impossible to tell which request a reply (or a silence) belongs to.
      if(typeof window!=="undefined") window.__GP_PERIF__ = _uInspESP32;
    }


    // Generic serial peripheral with no device-specific status handling.
    class  GenPerif_API extends Perif_API_Base
    {
      constructor(id,settingFilePath,pg_id_channel=10025)
      {
        super(id,settingFilePath,pg_id_channel);
      }

      resyncRequiresAck(){ return true; }

      // Deliberately does NOT publish machineStatus: the pre-refactor
      // GenPerif_API decoded the PING reply and dropped it on the floor, and
      // SLID_API inherits from here. Publishing it would be a behaviour change
      // to a machine in production, so it stays opt-in per device.
      onPingStatus(machineStatus){}
    }

    
    class  SLID_API extends GenPerif_API
    {
      constructor(id,settingFilePath,pg_id_channel=10025)
      {
        super(id,settingFilePath,pg_id_channel);
        this.checkInfoInterval=undefined;
        this.checkInfoListenerDict={};
        // this.pause_EM_STOP=false;
        this.is_in_EM_STOP=false;
        this.EM_STOP_src_list=[];
        this.no_obj_detected_time_ms=-1;
        this.no_ava_detected_time_ms=-1;
        this.EM_STOP_Rule=this.readLocalstorage_SLID_EM_STOP_RULE({//set default value
          enable_EM_STOP:false,
          no_obj_detected_time_max_ms:60*5*1000,
          no_ava_detected_time_max_ms:60*5*1000,
          SNG_Max:10,
          CNG_Max:20,

          
          consecutive_SNG_Max:3,
          consecutive_CNG_Max:6,

          
          fuzzy_consecutive_SNG_Max:-1,
          fuzzy_consecutive_CNG_Max:-1,

          
        })

      }
      readLocalstorage_SLID_EM_STOP_RULE(defaultRule)
      {
        let readRule = window.localStorage.getItem('SLID_EM_STOP_RULE');
        if(readRule===null)
        {
          this.saveLocalstorage_SLID_EM_STOP_RULE(defaultRule);
          return defaultRule;
        }

        try { return {...defaultRule,...JSON.parse(readRule)}; }
        catch (e) { return defaultRule; }
      }
      saveLocalstorage_SLID_EM_STOP_RULE(rule=this.EM_STOP_Rule)
      {
        window.localStorage.setItem('SLID_EM_STOP_RULE', JSON.stringify(rule));
      }
      connect(connInfo)
      {
        if(this.checkInfoInterval===undefined)
        {//start scan loop
          this.checkInfoInterval=window.setInterval(()=>this.checkInfoState(),1000);//watch dog to do reconnection
        }
        super.connect(connInfo)
      }




      reload_EM_STOP_RULE()
      {
        this.EM_STOP_Rule=this.readLocalstorage_SLID_EM_STOP_RULE();
      }
      update_EM_STOP_RULE(newRule)
      {
        this.EM_STOP_Rule={...this.EM_STOP_Rule,...newRule};
        this.saveLocalstorage_SLID_EM_STOP_RULE(this.EM_STOP_Rule)
        
        let reportStatisticState=GetObjElement(StoreX.getState(),["UIData","edit_info","reportStatisticState"]);
        Object.keys(this.checkInfoListenerDict).forEach(key=>{
          this.checkInfoListenerDict[key](this,reportStatisticState);
        })
      }
      tmp_EM_STOP_RULE(newRule)
      {
        this.EM_STOP_Rule={...this.EM_STOP_Rule,...newRule};

      }
      

      checkInfoListenerKeyUsed(key)
      {
        return this.checkInfoListenerDict[key]!==undefined
      }
      checkInfoListenerAdd(key,cb)
      {
        this.checkInfoListenerDict[key]=cb
      }
      
      checkInfoListenerRemove(key)
      {
        delete this.checkInfoListenerDict[key]
      }


      checkInfoState()
      {
        let reportStatisticState=GetObjElement(StoreX.getState(),["UIData","edit_info","reportStatisticState"]);
        let c_state=GetObjElement(StoreX.getState(),["UIData","c_state"]);
        let m_state=xstate_GetCurrentMainState(c_state);

        // console.log(this.p_state,m_state.state)
        if(this.p_state!=m_state.state)
        {
          if(m_state.state==UIAct.UI_SM_STATES.INSP_MODE)
            this.clear_EM_STOP_state();
          this.p_state=m_state.state;
        }


        if(m_state.state==UIAct.UI_SM_STATES.INSP_MODE && this.EM_STOP_Rule.enable_EM_STOP==true)
        {
          this.no_obj_detected_time_ms=-1;
          if(this.reportCount==reportStatisticState.reportCount)//no change
          {
            if(this.noreport_timestamp===undefined)
              this.noreport_timestamp=Date.now();
            else
            {
              
              this.no_obj_detected_time_ms= Date.now()-this.noreport_timestamp;
              
            }
          }
          else
          {
            this.noreport_timestamp=undefined
            this.reportCount=reportStatisticState.reportCount;
          }

          this.no_ava_detected_time_ms=-1;
          let ava_report_count=reportStatisticState.reportCount-reportStatisticState.emptyReportCount;
          if(this.ava_report_count==ava_report_count)//no change
          {
            if(this.latest_ava_report_timestamp===undefined)
              this.latest_ava_report_timestamp=Date.now();
            else
            {
              
              this.no_ava_detected_time_ms= Date.now()-this.latest_ava_report_timestamp;
              
            }
          }
          else
          {
            this.latest_ava_report_timestamp=undefined
            this.ava_report_count=ava_report_count;
          }

          ////


          if(this.is_in_EM_STOP==false)
          {//check status to EM_STOP

            let needToTrigEM_STOP=false;
            let EM_STOP_src_list=[];
            if(this.EM_STOP_Rule.no_obj_detected_time_max_ms>0 && this.no_obj_detected_time_ms>=this.EM_STOP_Rule.no_obj_detected_time_max_ms)
            {
              EM_STOP_src_list.push("no_obj_detected_time_ms");
              needToTrigEM_STOP=true
            }
            if(this.EM_STOP_Rule.no_ava_detected_time_max_ms>0 && this.no_ava_detected_time_ms>=this.EM_STOP_Rule.no_ava_detected_time_max_ms)
            {
              EM_STOP_src_list.push("no_ava_detected_time_ms");
              needToTrigEM_STOP=true
            }
            else
            {
            reportStatisticState.statisticValue.measureList.forEach(msure=>{
              let stat_sp=msure.statistic.sp;//find every 

              if(this.EM_STOP_Rule.SNG_Max>0 && stat_sp.SNG_count>=this.EM_STOP_Rule.SNG_Max)
              {
                EM_STOP_src_list.push("SNG_count");
                needToTrigEM_STOP=true;return;
              }
              if(this.EM_STOP_Rule.CNG_Max>0 && stat_sp.CNG_count>=this.EM_STOP_Rule.CNG_Max)
              {
                EM_STOP_src_list.push("CNG_count");
                needToTrigEM_STOP=true;return;
              }

              if(this.EM_STOP_Rule.consecutive_SNG_Max>0 && stat_sp.consecutive_SNG_count>=this.EM_STOP_Rule.consecutive_SNG_Max)
              {
                EM_STOP_src_list.push("consecutive_SNG_count");
                needToTrigEM_STOP=true;return;
              }
              if(this.EM_STOP_Rule.consecutive_CNG_Max>0 && stat_sp.consecutive_CNG_count>=this.EM_STOP_Rule.consecutive_CNG_Max)
              {
                EM_STOP_src_list.push("consecutive_CNG_count");
                needToTrigEM_STOP=true;return;
              }



              if(this.EM_STOP_Rule.fuzzy_consecutive_SNG_Max>0 && stat_sp.fuzzy_consecutive_SNG_count>=this.EM_STOP_Rule.fuzzy_consecutive_SNG_Max)
              {
                EM_STOP_src_list.push("fuzzy_consecutive_SNG_count");
                needToTrigEM_STOP=true;return;
              }
              if(this.EM_STOP_Rule.fuzzy_consecutive_CNG_Max>0 && stat_sp.fuzzy_consecutive_CNG_count>=this.EM_STOP_Rule.fuzzy_consecutive_CNG_Max)
              {
                EM_STOP_src_list.push("fuzzy_consecutive_CNG_count");
                needToTrigEM_STOP=true;return;
              }
              
            })

            }
            if(needToTrigEM_STOP)
            {
              this.EM_STOP_src_list=EM_STOP_src_list;
              this.trigger_EM_STOP();
            }
          }



        }
        else
        {
          this.noreport_timestamp=undefined;
        }


          
        // console.log(this.checkInfoListenerDict)
        Object.keys(this.checkInfoListenerDict).forEach(key=>{
          
          this.checkInfoListenerDict[key](this,reportStatisticState);
        })

      }
      
      clear_EM_STOP_state()
      {
        this.noreport_timestamp=undefined;
        this.latest_ava_report_timestamp=undefined;
        this.is_in_EM_STOP=false;
        this.EM_STOP_src_list=[];
      }
      trigger_EM_STOP(keep_ms)
      {
        this.is_in_EM_STOP=true;

        if(keep_ms===undefined)
        {
          keep_ms=this.machineSetup.EM_STOP_keep_ms;//try to find the keep time from setup first
          
        }
        if(keep_ms===undefined)keep_ms=2000;//if still unset use 2s keep time
        // this.is_in_EM_STOP_src="TRIG";
        this.send({"type":"EM_STOP","keep_ms":keep_ms},
        (ret)=>{
          // console.log(ret);
        },(e)=>console.log(e));
      }
    }

    this.props.ACT_WS_REGISTER(this.props.SLID_API_ID, new SLID_API(this.props.SLID_API_ID,"data/SLID_Setting.json",10025));



    this.props.ACT_WS_REGISTER(this.props.CNC_API_ID, new GenPerif_API(this.props.CNC_API_ID,"data/CNC_Setting.json",10026));


    

    class  Platform_API
    {
      constructor(id)
      {
        this.id=id;
        this.websocket=undefined;
        this.connected=false;

      }

      connect(info)
      {
        let id = this.id;
        let url = info.url;
        // console.log(">>>>",info);
        if(this.websocket===undefined)
        {
          this.close();
          this.websocket=undefined;
        }


        this.websocket=new websocket_reqTrack(new WebSocket(url));

        this.connected=false;
        comp.props.DISPATCH({type:"WS_DISCONNECTED",id,data:undefined})
        this.websocket.onopen = (e)=> {
          comp.props.DISPATCH({type:"WS_CONNECTED",id,data:undefined})
          this.connected=true;
        };
        
        this.websocket.onclose=(e)=>{
          
          this.connected=false;
          comp.props.DISPATCH({type:"WS_DISCONNECTED",id,data:undefined})
        };
        
        this.websocket.onerror=(e)=>{

          this.connected=false;
          comp.props.DISPATCH({type:"WS_DISCONNECTED",id,data:undefined})
        };
      }

      showOpenDialog(option={ title: "Select Directory",defaultPath:"", properties: ['openDirectory','createDirectory'] })
      {
        if(this.connected==false)return false;
        return this.websocket.send_obj({type:"showOpenDialog",option});
      }

      

      close()
      {
        
        if(this.websocket!==undefined)
          this.websocket.close();
        
        this.connected=false;
      }
    }

    this.props.ACT_WS_REGISTER(this.props.Platform_API_ID,new Platform_API(this.props.Platform_API_ID));





    
    // StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.uInsp_API_ID,data:undefined});




    // setInterval(()=>{
    //   let retx=
    //     this.props.ACT_WS_GET_OBJ(this.props.Insp_DB_W_ID, (obj)=>{
          
    //       console.log(obj);
    //       return obj.websocket.send_obj({type:"PING"});
    //     })
    //     .then(d=>{
    //       console.log(d);
    //     })
    //     .catch(e=>{
    //       console.log(e);

    //     })
    // },5000);



    // let dd=new websocket_aliveTracking({
    //   onStateChange:(ns,os,act)=>console.log(ns,"<=",os,"(",act,")"),
    //   url:"ws://db.xception.tech:8080/insert/insp"
    // });



    // this.props.ACT_WS_CONNECT(this.props.Insp_DB_W_ID, "ws://db.xception.tech:8080/insert/insp", new MW_CORE())

  



  }



  render() {
    log.debug("APPMasterX render", this.props);


    // console.log(this.props.C_STATE,this.props.CORE_ID_CONN_INFO);

    return (
      <div className="HXF sp_Style xception-theme">
        {/* <NullDOM_SystemStatusQuery onStatusChange={(status)=>{
          //console.log(status)
        }}/> */}
        <APPMain_rdx key="APP" />
        <CSSTransitionGroup //Splash Cover
          transitionName={"logoFrame"}
          transitionEnter={true}
          transitionLeave={true}
          transitionEnterTimeout={1500}
          transitionLeaveTimeout={1500}
          >
        {
          (GetObjElement(this.props.CORE_ID_CONN_INFO,["type"])=="WS_CONNECTED") ?null:
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
          title=""//{this.props.dictLookUp("system_status", DICT)+" "+localVersion}
          placement="right"
          closable={true}
          onClose={()=>{
            this.setState({show_system_panel:false});
          }}
          visible={this.state.show_system_panel}
        >

          <div style={{ padding: "8px 0", textAlign: "center" }}>
            <AntButton icon={<CloudUploadOutlined />} onClick={() => downloadDiag()}>
              下載診斷紀錄 / Download Diagnostics
            </AntButton>
            <AntButton style={{ marginLeft: 8 }} onClick={() => this.setState({show_core_log_panel:true})}>
              Core Logs
            </AntButton>
          </div>
          <Drawer
            title="Core Logs (inspd_log)"
            placement="right"
            width="90vw"
            closable={true}
            destroyOnClose={true}
            onClose={() => this.setState({show_core_log_panel:false})}
            visible={!!this.state.show_core_log_panel}
          >
            {this.state.show_core_log_panel && <CoreLogPanel height="calc(100vh - 100px)" />}
          </Drawer>

          <System_Status_Display showText iconSize={30} gridSize={100}
            onItemClick={(connInfo)=>{
              switch(connInfo.id)
              {
                
                case this.props.CORE_ID:
                {
                  
                  this.setState({
                    modal_view:{
                      view_fn:()=>
                      {
                        let cinfo=this.props.CORE_ID_CONN_INFO;
                        let info=GetObjElement(cinfo,["info"]);  
                        let snap_queue_skip_count=GetObjElement(info,["snap_queue_skip_count"]);  
                        let save_snap_folder_full_delete_count=GetObjElement(info,["save_snap_folder_full_delete_count"]);  

                        return <pre>
                        檢驗NG儲存略過數量：{snap_queue_skip_count}<br/>
                        檢驗NG儲存資料夾滿後刪舊：{save_snap_folder_full_delete_count}<br/>
                        {JSON.stringify(info,null,2)}
                        </pre>
                      },
                      title:"Core",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });

                  break;
                }



                
                
                case this.props.CAM1_ID:
                {
                  // BMP_carousel fake-camera: open right-side drawer with live ctrls
                  let camType = GetObjElement(this.props.CAM1_ID_CONN_INFO, ["data", 0, "type"]);
                  if (camType === "CameraLayer_BMP_carousel") {
                    this.setState({ carousel_drawer_open: true });
                    break;
                  }
                  this.setState({
                    modal_view:{
                      view_fn:()=><pre>
                        {JSON.stringify(this.props.CAM1_ID_CONN_INFO,null,2)}

                        <Button onClick={()=>{

                          this.props.ACT_WS_GET_OBJ(this.props.CAM1_ID, (obj)=>{
                            return obj.reconnection();
                          })
                        }}>重連</Button>

                      </pre>,
                      title:"Camera",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });

                  break;
                }



                case this.props.uInsp_API_ID:
                {
                  this.setState({
                    modal_view:{
                      view_fn:()=><>
                      檢測數<UINSP_UI UI_INSP_Count={true}/><br/>
                      檢測速<UINSP_UI UI_INSP_Count_Rate={true}/><br/>
                      <UINSP_UI UI_Speed_Slider={true} UI_detail={true}/>
                      
                      </>,
                      title:"uInsp_API",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });
                  break;
                }
                
                case this.props.uInspESP32_API_ID:
                {
                  this.setState({
                    modal_view:{
                      view_fn:()=><UINSP_ESP32_UI/>,
                      title:"全檢設備 v2 (uInspESP32)",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });
                  break;
                }

                case this.props.SLID_API_ID:
                {
                  this.setState({
                    modal_view:{
                      view_fn:()=><>
                      <SLID_UI key="SIMP_UI" SIMPLE_CTRL_UI/><br/>


                      {/* <SLID_UI key="STOP_UI" UI_EM_STOP_UI 
                      on_EM_STOP_triggered={(api,report_stat)=>{
                        if(this.state.modal_view!==undefined)return;
                        this.setState({
                          modal_view:{
                            view_fn:()=>
                            <SLID_UI key="STOP_UI" UI_EM_STOP_UI on_EM_STOP_triggered={(api,report_stat)=>{}}/>
                            ,
                            title:"SLID_API_EM_STOP",
                            onCancel:()=>this.setState({modal_view:undefined}),
                            onOk:()=>this.setState({modal_view:undefined}),
                            footer:null
                          }
                        });
                      }}/> */}

                      </>
                      ,
                      title:"SLID_API",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });
                  break;
                }
                
                case this.props.CNC_API_ID:
                {
                  this.setState({
                    modal_view:{
                      view_fn:()=><>
                      <CNC_UI/><br/>
                      </>
                      ,
                      title:"CNC_API",
                      onCancel:()=>this.setState({modal_view:undefined}),
                      onOk:()=>this.setState({modal_view:undefined}),
                      footer:null
                    }
                  });
                  break;
                }
                


              }
            }}
          />
          
          <>
            <Divider>Build Info</Divider>
            {(() => {
              const cb = GetObjElement(this.props.CORE_ID_CONN_INFO, ["data","data","build"]) || {};
              const cv = GetObjElement(this.props.CORE_ID_CONN_INFO, ["data","data","version"]) || this.props.System_Setting.version;
              const wb = WEBUI_BUILD || {};
              return (
                <pre style={{fontSize:11, lineHeight:1.4, margin:"4px 0"}}>
{`Core   v${cv || '?'}   ${cb.config || ''}
  build  ${cb.time || 'unknown'}
  git    ${cb.git_hash || 'unknown'}  (${cb.git_branch || '?'})
  tag    ${cb.git_describe || 'unknown'}

WebUI
  build  ${wb.time || 'unknown'}
  git    ${wb.git_hash || 'unknown'}  (${wb.git_branch || '?'})
  tag    ${wb.git_describe || 'unknown'}`}
                </pre>
              );
            })()}
            <Divider></Divider>
            <pre>
              {JSON.stringify(this.props.System_Setting, null, 1)}
            </pre>
            <Divider></Divider>
          </>
        </Drawer>

        <Button className="overlay" 
          style={{
            background: "rgba(255,255,255,0.4)",
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

        <Modal
          {...this.state.modal_view}
          visible={this.state.modal_view !== undefined}>
          {this.state.modal_view === undefined ? null : this.state.modal_view.view_fn()}
        </Modal>

        <BMPCarouselAutoBoot
          camInfo={this.props.CAM1_ID_CONN_INFO}
          coreId={this.props.CORE_ID}
          ws_send_bpg={this.props.ACT_WS_SEND_BPG}
        />

        <Drawer
          title="Fake Camera (BMP carousel)"
          placement="right"
          width={360}
          visible={this.state.carousel_drawer_open}
          onClose={()=>this.setState({carousel_drawer_open:false})}>
          <BMPCarouselPanel
            camInfo={this.props.CAM1_ID_CONN_INFO}
            coreId={this.props.CORE_ID}
            cam1Id={this.props.CAM1_ID}
            ws_send_bpg={this.props.ACT_WS_SEND_BPG}
          />
        </Drawer>
      </div>
    );
  }
}

let APPMasterX_rdx = APPMasterX.connect();

// Top-level error boundary: a render/lifecycle throw anywhere in the tree would
// otherwise white-screen the whole operator UI for the rest of the shift. Catch
// it, log it, and show a recoverable reload panel instead. Fallback uses only
// plain DOM (no antd/redux) so it can't itself re-crash if the cause is there.
class RootErrorBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = { error: null };
  }
  static getDerivedStateFromError(error) {
    return { error };
  }
  componentDidCatch(error, info) {
    log.error("RootErrorBoundary caught a render crash:", error, info && info.componentStack);
  }
  render() {
    if (this.state.error) {
      const msg = String((this.state.error && this.state.error.message) || this.state.error);
      return (
        <div className="HXF WXF overlay veleXY white"
          style={{ flexDirection: "column", padding: "5%", textAlign: "center", boxSizing: "border-box" }}>
          <h1 className="Title">系統發生錯誤 / Something went wrong</h1>
          <div style={{ margin: "1em 0", maxWidth: "90%", wordBreak: "break-word", opacity: 0.7, fontSize: "0.5cm" }}>
            {msg}
          </div>
          <button onClick={() => window.location.reload()}
            style={{ padding: "0.4cm 1cm", fontSize: "0.5cm", borderRadius: "4px", cursor: "pointer" }}>
            重新載入 / Reload
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}

// Hard block for def-file integrity failures: when rootDefInfoLoading detects a
// featureSet_sha1 mismatch it sets edit_info.defIntegrityError; the def is already
// refused (not loaded). This watcher pops a blocking modal so the operator cannot
// silently proceed with a corrupt/tampered inspection definition.
function DefIntegrityGuard() {
  const err = useSelector(s => s.UIData.edit_info && s.UIData.edit_info.defIntegrityError);
  const shownRef = useRef(null);
  useEffect(() => {
    if (err) {
      if (shownRef.current !== err.actual) {
        shownRef.current = err.actual;
        Modal.error({
          title: "定義檔完整性驗證失敗 / Definition integrity check failed",
          content:
            "此定義檔的 SHA1 與內容不符，已拒絕載入，避免使用受損或被竄改的檢測定義。\n" +
            "The definition file's SHA1 does not match its content; loading was refused.\n\n" +
            "expected: " + err.expected + "\nactual: " + err.actual,
          okText: "確定 / OK",
        });
      }
    } else {
      shownRef.current = null;
    }
  }, [err]);
  return null;
}

ReactDOM.render(

  <RootErrorBoundary>
    <Provider store={StoreX}>
        <APPMasterX_rdx />
        <DefIntegrityGuard />

    </Provider>
  </RootErrorBoundary>, document.getElementById('container'));

// UI action recorder: OFF by default. Opt in with localStorage.__rec_on = "1"
// (then reload) to capture a repro sequence for replay.
if (typeof localStorage !== 'undefined' && localStorage.__rec_on === '1')
  initActionRecorder();
