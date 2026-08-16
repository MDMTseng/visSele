'use strict'

import 'regenerator-runtime/runtime' // some legacy deps (react-numpad) expect a global regeneratorRuntime

import 'STYLE/basis.css'
import 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import ReactDOM from 'react-dom';
// The device's setup document is grouped (plate/gate/cam/skip_policy); this UI
// speaks flat. Translating at the wire is the whole fix -- see uinspCfg.js.
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
import SettingOutlined from "@ant-design/icons/SettingOutlined";
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
import { initPerifModule, registerPerifAPI, usePerifLink, pokeLinkHealthNow, uInsp_API, uInspESP32_API, GenPerif_API, SLID_API } from './perif/PerifAPI';
initLogger();
const log = mkLog('ui.main');
const dbLog = mkLog('comm.db'); // DB_WS / SLID API queue chatter

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
  // Live view of the perif link store (a function, so it's always current).
  import('./perif/PerifAPI').then(m => { window.__GP_PERIF_LINKS__ = m.getPerifLinksSnapshot; });
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
      // Module-store link state (perif rows come from usePerifLink now, not
      // Redux). SUSPECT = link up but verdicts in doubt -- the error color,
      // because a machine that may not be sorting deserves red, not grey.
      case "WS_SUSPECT":
        return "color-error-anim";
      default:
        return "color-noresource-anim";
        break;
    }
    
  }
  
  // Peripheral rows read the module link store (usePerifLink), not Redux --
  // that also gives them the SUSPECT state, which the legacy WS_* bridge
  // never carried. The adapter shapes a link into the conn_info the shared
  // row renderer already understands. A link that never connected stays
  // undefined so the row hides, same as before.
  const _linkToConn = (link) => {
    if (!link || (link.state === 'DISCONNECTED' && link.connInfo === undefined)) return undefined;
    const t = link.state === 'CONNECTED' ? 'WS_CONNECTED'
      : link.state === 'SUSPECT' ? 'WS_SUSPECT'
      : 'WS_DISCONNECTED';
    return { type: t, brief_info: link.state === 'SUSPECT' ? '連線異常' : undefined };
  };
  const uInspConn      = _linkToConn(usePerifLink(ConnInfo.uInsp_API_ID));
  const uInspESP32Conn = _linkToConn(usePerifLink(ConnInfo.uInspESP32_API_ID));
  const SLIDConn       = _linkToConn(usePerifLink(ConnInfo.SLID_API_ID));
  const CNCConn        = _linkToConn(usePerifLink(ConnInfo.CNC_API_ID));

  return [
    [dictLookUp("core", DICT),   ConnInfo.CORE_ID_CONN_INFO,        <AimOutlined/>,true],
    [dictLookUp("camera", DICT), ConnInfo.CAM1_ID_CONN_INFO,        <CameraOutlined/>,true],
    ["設定DB",    ConnInfo.DefFile_DB_W_ID_CONN_INFO,<CloudUploadOutlined/>,true],
    ["檢測DB",    ConnInfo.Insp_DB_W_ID_CONN_INFO,   <CloudUploadOutlined/>,true],
    [undefined,            undefined,                  <MinusOutlined />,uInspConn!==undefined || uInspESP32Conn!==undefined || SLIDConn!==undefined||CNCConn!==undefined],//seg line
    ["全檢設備",       uInspConn,   <RobotOutlined />,false],
    ["全檢設備v2",     uInspESP32Conn, <RobotOutlined />,false],
    ["坡檢設備",       SLIDConn,    <StockOutlined />,false],
    ["CNC設備",       CNCConn,    <RobotOutlined />,false],
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

      {/* Exposure simulation. Off by default and driven from HERE, not from
          the shared camera settings: those belong to the real camera, and
          feeding its 50us against this simulator's 5000us reference rendered
          every loaded BMP at 1% brightness -- "the image is super dark" with
          nothing in the fake-camera UI to explain it. A file already has its
          exposure in its pixels; simulating another is opt-in. */}
      <div style={{opacity:0.7, fontSize:12, margin:'6px 0 2px'}}>
        Exposure simulation <span style={{opacity:0.7}}>(100% = {5000}us)</span>
      </div>
      <div style={{display:'flex', alignItems:'center', gap:6}}>
        <Switch size="small" checked={!!eff.expo_sim_en}
          onChange={(v)=>set({expo_sim_en:v})}/>
        <span style={{minWidth:140}}>exposure us</span>
        <InputNumber size="small" disabled={!eff.expo_sim_en}
          value={eff.expo_us} min={0} max={20000} step={100}
          onChange={(v)=>set({expo_us:v})}/>
      </div>
      <div style={{display:'flex', alignItems:'center', gap:6}}>
        <span style={{minWidth:160, marginLeft:34}}>gain x</span>
        <InputNumber size="small" disabled={!eff.expo_sim_en}
          value={eff.expo_gain} min={0} max={64} step={0.1}
          onChange={(v)=>set({expo_gain:v})}/>
      </div>
    </div>
  );
}

function BMPCarouselPanel({ camInfo, coreId, cam1Id, ws_send_bpg }) {
  const car = camInfo?.data?.[0]?.carousel;
  const send = (action, extra={}) => ws_send_bpg(coreId, "RC", 0,
    { target: "bmp_carousel", action, ...extra }, undefined, {
      resolve: () => {}, reject: () => {},
    });
  const [augOpen, setAugOpen] = React.useState(false);
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
      <div style={{display:'flex', justifyContent:'flex-end'}}>
        <Button size="small" icon={<SettingOutlined />} title="模擬參數 / simulation"
          onClick={()=>setAugOpen(true)}>模擬參數</Button>
      </div>
      {/* `visible`, not `open`. antd renamed this prop in 4.23.0 and this
          project is pinned at 4.22.8, where Modal.js reads props.visible and
          ignores `open` entirely -- so the button set the state, the state was
          correct, and nothing appeared. Nothing warns: an unknown prop is just
          dropped. Every other Modal/Drawer in this codebase uses `visible`. */}
      <Modal title="Fake camera — 模擬參數 / simulation" visible={augOpen}
        onCancel={()=>setAugOpen(false)} footer={null} destroyOnClose={false}>
        <BMPCarouselAugPanel aug={car.aug} send={send} />
      </Modal>
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

      SLID_API_ID:state.ConnInfo.SLID_API_ID,


      CNC_API_ID:state.ConnInfo.CNC_API_ID,


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
    // Camera-state doorbell (core CamStateWatchThread): the core pushes this
    // tiny GS batch whenever the camera's health CHANGES. It is a doorbell,
    // not a data update -- re-run the normal camera_info query immediately so
    // every policy (soft-cam, reconnect) stays in Cam_Stat_Query. This is
    // what lets the steady-state poll run at 6s instead of 2s.
    const doorbell = pkts.find(pkt => pkt && pkt.type == "GS" && pkt.data && pkt.data.camera_state);
    if (doorbell) {
      log.info("[cam-doorbell] camera state changed, re-querying", doorbell.data.camera_state);
      if (this.camStatQuery) this.camStatQuery.pokeNow();
    }
    // Perif-link doorbell (core pgID 0xCA12): the link summary changed --
    // counters moving, suspect flip, channel gone. Re-read perif_pairing now
    // instead of waiting out the 30s safety-net poll.
    const perifBell = pkts.find(pkt => pkt && pkt.type == "GS" && pkt.data && pkt.data.perif_state);
    if (perifBell) {
      log.info("[perif-doorbell] link state changed, re-querying", perifBell.data.perif_state);
      pokeLinkHealthNow();
    }
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




    const CAM_POLL_MS = 6000;
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

              // A soft camera when soft cameras are not allowed is a POLICY
              // mismatch, not a fault -- and reconnecting cannot fix it.
              //
              // This is what produced the reconnect livelock: the core hands
              // back the same CameraLayer_BMP_carousel every time (it is what
              // the machine has, or what FORCE_BMP_CAROUSEL pins), the UI reads
              // it as "not in operation" again, and asks again. The core's 3s
              // rate limit turned that into `camera_ez_reconnect: ignored` on a
              // loop, so the reconnect could never even run, the blocking
              // 相機重連中 modal never cleared, and the whole UI was unusable
              // against a fake camera. Retrying a deterministic answer is the
              // bug; the rate limit was only hiding its cost.
              //
              // Kept separate from the fault case below because the two want
              // opposite handling: a fault should be retried, this should be
              // shown to a human. The modal already carries the one control
              // that resolves it -- 跳過相機連線 sets ALLOW_SOFT_CAM.
              let softCamBlocked = (cam0!==undefined
                && comp.props.System_Setting.ALLOW_SOFT_CAM==false
                && cam0.includes("CameraLayer_BMP"));

              if(cam0===undefined || softCamBlocked)
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

                if(softCamBlocked)
                {
                  // Say it once per transition, not once per 2s poll.
                  if(this.softCamWarned!==true)
                  {
                    this.softCamWarned=true;
                    log.warn("[queryCam] soft camera (" + cam0 + ") with ALLOW_SOFT_CAM=false"
                      + " -- NOT reconnecting: the core would return the same camera."
                      + " Press 跳過相機連線 to work against it.");
                  }
                }
                else
                {
                  this.softCamWarned=false;
                  this.reconnection();
                }
                reject(stacked_pkts,P);
                // this.queryTimeOut=setTimeout(()=>{
                //   this.queryCam(timeout_ms);
                // },timeout_ms*2);
              }
              else
              {
                
                this.softCamWarned=false;
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

        // Never ask faster than the core will answer.
        //
        // wiringPanel's RC handler drops any camera_ez_reconnect inside 3s of
        // the last one, so a client that retries faster gets nothing but
        // `ignored, less than 3s since the last one` -- the reconnect it is
        // waiting for never actually runs. Mirroring the interval here (with a
        // margin) means every request we send is one the core will act on.
        // The core's limit stays where it is: it guards against ALL clients,
        // and this one only governs itself.
        const RC_MIN_INTERVAL_MS = 3500;
        const now = Date.now();
        if(this.lastReconnAt!==undefined && (now-this.lastReconnAt) < RC_MIN_INTERVAL_MS)
        {
          return false;
        }
        this.lastReconnAt = now;
        this.isInReconn=true;



        StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.CAM1_ID});
        this.isConnected=false;
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "RC", 0, {
          target: "camera_ez_reconnect"
        },
        undefined, {
          resolve:(ret)=>{
            this.isInReconn=false;
            // No immediate re-query here.
            //
            // It used to call _queryCam() straight from this callback, which
            // re-entered the not-in-operation branch and fired the next
            // reconnect with ZERO delay -- the tight half of the livelock, and
            // the reason requests arrived inside the core's 3s window. The
            // poll loop in queryCam() already re-queries on its own schedule,
            // so the reconnect result is picked up there.
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

        // One chain, ever. The timer is only re-armed inside these callbacks,
        // so between _queryCam firing and its reply there is NO pending timer
        // -- a pokeNow() in that window used to clearTimeout a stale handle
        // and start a SECOND in-flight query, whose reply armed a second 6s
        // chain; chains only ever accumulated (each doorbell-during-flight
        // added one). inFlight makes that window explicit, and a poke that
        // lands inside it is coalesced into pokePending, honoured here when
        // the reply arrives.
        // A poke that arrived BEFORE this query starts is satisfied by this
        // query itself (it fetches the fresh state); only pokes landing while
        // it is in flight need the immediate follow-up in _next.
        this.pokePending = false;
        this.inFlight = true;
        const _next = (delay_ms) => {
          this.inFlight = false;
          if (this.pokePending === true)
          {
            this.pokePending = false;
            this.queryCam(timeout_ms);   // immediate re-query, same chain
            return;
          }
          this.queryTimeOut = setTimeout(() => {
            this.queryCam(timeout_ms);
          }, delay_ms);
        };
        this._queryCam(()=>{ _next(timeout_ms); },
                       ()=>{ _next(timeout_ms/2); });
      }
      // The core's camera-state doorbell says "re-query NOW". Three cases:
      // a timer is pending -> collapse it into an immediate query; a query
      // is in flight -> remember the poke and let its reply re-query (never
      // start a parallel chain); reconnecting -> remember the poke too (the
      // old silent drop meant a "camera back" doorbell arriving inside the
      // RC round trip was only noticed by the 12s fallback timer).
      pokeNow()
      {
        if (this.inFlight === true || this.isInReconn === true)
        {
          this.pokePending = true;
          return;
        }
        clearTimeout(this.queryTimeOut);
        this.queryCam(CAM_POLL_MS);
      }
      constructor(id)
      {
        this.id=id;
        this.isInReconn=false;
        this.isConnected=false;

        // 6s, not 2s: steady-state changes are pushed by the core's
        // CamStateWatchThread (the doorbell above), so the poll is only a
        // safety net for missed pushes, non-primary peers (only the default
        // peer is stream-subscribed) and older cores without the watcher.
        this.queryCam(CAM_POLL_MS);
      }


      

    }
    this.camStatQuery = new Cam_Stat_Query(this.props.CAM1_ID);
    this.props.ACT_WS_REGISTER(this.props.CAM1_ID, this.camStatQuery);




    // Transport plumbing shared by every serial peripheral reached through the
    // core's PD channel: connect/reconnect, request/response tracking, the PING
    // watchdog, setting-file load/save and the comm latency probe. Subclasses
    // supply the device's protocol dialect, not the transport.
    // Peripheral device APIs (uInspMEGA / uInspESP32 / SLID / CNC) live in
    // src/perif/PerifAPI.js now, with their own link-state store (no Redux)
    // -- see the header there for the design. The ACT_WS_REGISTER calls are
    // the legacy GET_OBJ bridge and go away when the last consumer uses
    // getPerifAPI().
    initPerifModule({
      sendBPG: (tl, prop, obj, bin, cbs) => comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, tl, prop, obj, bin, cbs),
      getState: () => StoreX.getState(),
    });
    {
      let _u = new uInsp_API(this.props.uInsp_API_ID);
      registerPerifAPI(this.props.uInsp_API_ID, _u);
      this.props.ACT_WS_REGISTER(this.props.uInsp_API_ID, _u);
    }
    {
      let _uInspESP32 = new uInspESP32_API(this.props.uInspESP32_API_ID);
      registerPerifAPI(this.props.uInspESP32_API_ID, _uInspESP32);
      this.props.ACT_WS_REGISTER(this.props.uInspESP32_API_ID, _uInspESP32);
      // QA/bring-up handle -- driving the board by hand from the console is
      // the only way to probe one command in isolation.
      if (typeof window !== "undefined") window.__GP_PERIF__ = _uInspESP32;
    }
    {
      let _slid = new SLID_API(this.props.SLID_API_ID, "data/SLID_Setting.json", 10025);
      registerPerifAPI(this.props.SLID_API_ID, _slid);
      this.props.ACT_WS_REGISTER(this.props.SLID_API_ID, _slid);
    }
    {
      let _cnc = new GenPerif_API(this.props.CNC_API_ID, "data/CNC_Setting.json", 10026);
      registerPerifAPI(this.props.CNC_API_ID, _cnc);
      this.props.ACT_WS_REGISTER(this.props.CNC_API_ID, _cnc);
    }


    

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
