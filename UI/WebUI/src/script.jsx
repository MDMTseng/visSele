'use strict'

import 'regenerator-runtime/runtime' // some legacy deps (react-numpad) expect a global regeneratorRuntime

import 'STYLE/basis.css'
import 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import ReactDOM from 'react-dom';
import * as BASE_COM from './component/baseComponent.jsx';
import {UINSP_UI,SLID_UI,CNC_UI} from './component/rdxComponent.jsx';

import {GetDefaultSystemSetting} from './info.js';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';

import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';

import CSSTransitionGroup from 'react-transition-group/CSSTransitionGroup';
let $CTG=CSSTransitionGroup;
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { websocket_reqTrack, websocket_autoReconnect,xstate_GetCurrentMainState,GetObjElement,websocket_aliveTracking,ConsumeQueue} from 'UTIL/MISC_Util';
import { MW_API } from "REDUX_STORE_SRC/middleware/MW_API";

// import LocaleProvider from 'antd/lib/locale-provider';

import Modal from "antd/lib/modal";
import Divider from 'antd/lib/divider';
import APPMain_rdx from './MAINUI';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';
import * as log from 'loglevel';
import BPG_WS from './comm/BPG_WS';
import { initDiag, downloadDiag, diagCount, diagText } from 'UTIL/diagLog';
import { enqueueFailedInsert, pendingInsertCount } from 'UTIL/inspDBQueue';
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

var require=require||(()=>undefined);

const electron = require('electron')
const fs = require('fs');
const path = require('path')

log.setLevel("info");
log.getLogger("InspectionEditorLogic").setLevel("INFO");
log.getLogger("UICtrlReducer").setLevel("INFO");

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
    StoreX.dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, "LD", 0,
      { deffile: defModelPath + '.' + DEF_EXTENSION, imgsrc: defModelPath, down_samp_level: 1 },
      undefined,
      {
        resolve: (pkts) => {
          StoreX.dispatch({
            type: "ATBundle", ActionThrottle_type: "express",
            data: pkts.map(pkt => { let act = BPG_Protocol.map_BPG_Packet2Act(pkt); if (act) act.IGNORE_DEFCONF_LOCK = true; return act; }).filter(Boolean)
          });
          resolve(true);
        },
        reject
      }));
    setTimeout(() => reject("Timeout"), 8000);
  });
  // Test hooks for the diagnostics ring buffer + local failed-insert queue.
  window.__GP_DIAG__ = { downloadDiag, diagCount, diagText };
  window.__GP_DB_QUEUE__ = { enqueueFailedInsert, pendingInsertCount };
}

// Global safety net for async errors that React error boundaries cannot catch
// (timer/event-handler throws, WS callbacks, unhandled promise rejections).
// Routed through loglevel so a future diagnostics ring-buffer can capture them;
// this is the floor-unit's only signal when no devtools are attached.
if (typeof window !== "undefined") {
  window.addEventListener("error", (e) => {
    log.error("window.onerror:", (e && (e.error || e.message)), (e && e.filename), (e && e.lineno));
  });
  window.addEventListener("unhandledrejection", (e) => {
    log.error("unhandledrejection:", (e && e.reason));
  });
}
console.log(navigator)

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
  let gridStyle={...style,width:(gridSize)+"px" };
  
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
    [DICT._.core,   ConnInfo.CORE_ID_CONN_INFO,        <AimOutlined/>,true],
    [DICT._.camera, ConnInfo.CAM1_ID_CONN_INFO,        <CameraOutlined/>,true],
    ["設定資料庫",    ConnInfo.DefFile_DB_W_ID_CONN_INFO,<CloudUploadOutlined/>,true],
    ["檢測資料庫",    ConnInfo.Insp_DB_W_ID_CONN_INFO,   <CloudUploadOutlined/>,true],
    [undefined,            undefined,                  <MinusOutlined />,ConnInfo.uInsp_API_ID_CONN_INFO!==undefined || ConnInfo.SLID_API_ID_CONN_INFO!==undefined||ConnInfo.CNC_API_ID_CONN_INFO!==undefined],//seg line
    ["全檢設備",       ConnInfo.uInsp_API_ID_CONN_INFO,   <RobotOutlined />,false],
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
                <span className="veleX">{textName}<br/>{brief_info}<br/></span>
                
              </>}
      </Button>)})

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
      modal_view:undefined
    };

    console.log("electron:",electron);
    console.log("fs:",fs);
    console.log("path:",path);


    let localUrl =window.location.href;
    let rootUrl="localhost";
    if(!localUrl.startsWith("file"))
    {
      let matchRes = (/[\/]+(.+)(\:|\?.+)/gm).exec(localUrl);
      console.log(matchRes);
      rootUrl=matchRes[1];
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



      _insertOK(data,msg)
      {
        // console.log("OK",data,msg);
      }
      _insertFailed(data,msg)
      {
        // console.log("FAILED",data,msg);
        // Don't silently drop a record that failed to reach the traceability DB:
        // buffer it to local IndexedDB (capped, oldest-dropped). Some failure
        // paths have no record (connection/empty) — only buffer real data.
        if (data !== undefined && data !== null) {
          enqueueFailedInsert(data, msg).catch((e) => log.error("enqueueFailedInsert failed", e));
        }
      }

      constructor(id)
      {
        this.id=id;
        this.QWindow= {};
        this.pgIDCounter= 0;
        this.websocket=undefined;
        this.dataSentCount=0;
        console.log(">>>>");
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
            console.log(ns,"<=",os,"(",act,")")
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
          // this.onmessage(ev);
          console.log(ev);
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
              console.log("consumeQueue>>timeout");
              reject("Timeout");
              
              _this._insertFailed(data, "Timeout");
            }, 5000);
            
            //The second param is replacer for stringify, and we replace any value that has toFixed(basically 'Number') to replace it to toFixed(5)
            console.log("SEND::::",msg_obj);
            _this.websocket.send_obj(msg_obj, (key, val) => typeof val === 'number' ? Number(val.toFixed(5)) : val).
              then((ret) => {
                clearTimeout(timeoutFlag);
                this.retryQCount = 0;
                _this.dataSentCount++;
                cQ.deQ();//pop one data out
                resolve();
                _this._insertOK(data, ret);
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
            console.log("Termination dump",cQ);
            while(true)
            {
              let data = cQ.deQ();
              // console.log("dump deQ",data);
              if(data===undefined)
              {break;}
              console.log(data)
              data.reject("The Q is teminated....");
            }
          }
        );


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
        console.log(">>>>",info);
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
        console.log(info);
        let data = info.data;
        
        // if(isInQueue==true)
        {
          let prom=new Promise((resolve, reject) => {
            
            if (!this.cQ.enQ({data,resolve,reject}))//If enQ NOT success
            {
              //Just print
              log.error("enQ failed size()=" + this.cQ.size());
              this._insertFailed(data, "Cannot enQ the data"); // bugfix: was undefined `x` → ReferenceError, reject() never ran (promise hung)
              reject("send insert failed");
            }
            else
            {
              console.log("QQQ.len:",this.cQ.size());
            }
            if (this.cQ.size() > 0)
              this.cQ.kick();//kick transmission


          });
          console.log("ENQ send",info);
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
        console.log("queryCam System_Setting",comp.props.System_Setting);
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
            console.log(e);
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
            console.log(ret);
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




    class  uInsp_API
    {

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

      
      saveMachineSetupIntoFile(filename = "data/uInspSetting.json")
      {
        
        let act = comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID,"SV", 0,
          { filename: filename },
          new TextEncoder().encode(JSON.stringify(this.machineSetup, null, 4)),
          {
            resolve:(res)=>{
              console.log(res);
              let default_pulse_hz = this.machineSetup.pulse_hz;
              StoreX.dispatch({type:"WS_UPDATE",id:comp.props.uInsp_API_ID,default_pulse_hz:default_pulse_hz});
            }, 
            reject:(res)=>{
              console.log(res);
            }, 
          }
        )
      }

      

      LoadFileToMachine(filename = "data/uInspSetting.json") {
        new Promise((resolve, reject) => {

          log.info("LoaduInspSettingToMachine step2");
          comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID,"LD", 0,
            { filename },
            undefined, { resolve, reject }
          );
          setTimeout(() => reject("Timeout"), 1000)
        }).then((pkts) => {

          log.info("LoaduInspSettingToMachine>> step3", pkts);
          if (pkts[0].type != "FL")
          {
            return;
          }
          let machInfo = pkts[0].data;
          
          this.machineSetupUpdate(machInfo,true);
          this.default_pulse_hz = machInfo.pulse_hz;
          StoreX.dispatch({type:"WS_UPDATE",id:comp.props.uInsp_API_ID,default_pulse_hz:this.default_pulse_hz});
        }).catch((err) => {

          log.info("LoaduInspSettingToMachine>> step3-error", err);
        })
      }

      machineSetupUpdate(newMachineInfo,doReplace=false)
      {

        this.machineSetup=doReplace==true?newMachineInfo:{...this.machineSetup,...newMachineInfo};
        // console.log(this.machineSetup);
        StoreX.dispatch({type:"WS_UPDATE",id:comp.props.uInsp_API_ID,machineSetup:this.machineSetup});
        this.send({type:"set_setup",...newMachineInfo},
        (ret)=>{
          //HACK: just assume it will work
          // this.machineSetup={...this.machineSetup,...newMachineInfo};
          // console.log(ret);
        },(e)=>console.log(e));
      }
      
      machineSetupReSync() {
        this.send({type:"get_setup"},
        (ret)=>{
          delete ret["type"];
          delete ret["id"];
          delete ret["st"];
          this.machineSetup=ret;
          // console.log(ret);
          this.machineSetupUpdate(this.machineSetup,true);
        },(e)=>console.log(e));
      }

      connect(connInfo)
      {
        if(this.inReconnection==true)
        {//still in reconnection state, return
          return false;
        }
        
        StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.uInsp_API_ID,data:undefined});
        this.connInfo=connInfo;
        this.inReconnection=true;
        this.LoadFileToMachine();
        comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "PD", 0, {type:"CONNECT",...connInfo, _PGID_: this.pg_id_channel, _PGINFO_: { keep: true }},undefined,
        {
          resolve: (stacked_pkts,action_channal) => {
            // console.log(stacked_pkts);
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
                  StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.uInsp_API_ID,data:PD});
                  break;
                case "CONNECT":
                  this.CONN_ID=PD_data.CONN_ID;
                  StoreX.dispatch({type:"WS_CONNECTED",id:comp.props.uInsp_API_ID,data:PD});

                  if(this.machineSetup!==undefined)
                  {
                    // this.default_pulse_hz = machInfo.pulse_hz;
                    StoreX.dispatch({type:"WS_UPDATE",id:comp.props.uInsp_API_ID,default_pulse_hz:this.default_pulse_hz})
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
            console.log(e);
            StoreX.dispatch({type:"WS_DISCONNECTED",id:comp.props.uInsp_API_ID,data:undefined});
            
          }
        });
      }

      constructor(id,pg_id_channel=10024)
      {
        this.CONN_ID=undefined;
        this.pg_id_channel=pg_id_channel;
        this.id=id;
        this.connInfo=undefined;
        this.inReconnection=false;
        this.checkReconnectionInterval=setInterval(()=>this.checkReConnection(),3000);//watch dog to do reconnection
        this.runPINGInterval=setInterval(()=>this._sendPing(),3000);//watch dog to do reconnection

        this.trackingWindow={};
        this.idCounter=10;
        this.PINGCount=0;

        this.machineInfo=undefined;


        this.pre_res_count=undefined;
        this.res_count_start_time=undefined;
        this.res_count_pre_time=undefined;

        this.res_count_rate_overall=undefined;
        this.res_count_rate_recent=undefined;


        
      } 
      checkReConnection()
      {
        // console.log(this.connInfo,this.connected,this.inReconnection);
        if(this.connInfo===undefined ||this.CONN_ID!==undefined || this.inReconnection==true)
        {
          return;
        }
        this.connect(this.connInfo);
        // this.checkReconnectionTimeout=setTimeout(,);
      }

      getMachineSetup()
      {
        return this.machineSetup;
      }

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
        // console.log(this.CONN_ID);

        this.triggerPing();
        // this.machineSetupUpdate({pulse_hz:0});
      }
      triggerPing()
      {
        
        this.send({type:"PING"},(ret)=>{
          // console.log(ret);
          delete ret["type"]
          delete ret["id"]
          delete ret["st"]
          let machineStatus={...ret};
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
              // console.log(currentTime_ms,"<ms->>",OK_rate,NG_rate,NA_rate, period_overall_s,period_s)
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
          // console.log(this.res_count_rate_overall,this.res_count_rate_recent);

          StoreX.dispatch({type:"WS_UPDATE",id:comp.props.uInsp_API_ID,machineStatus,result_count_rate_recent:this.res_count_rate_recent});
          this.PINGCount=0;
        },errorInfo=>console.log(errorInfo));

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
      
    }
    this.props.ACT_WS_REGISTER(this.props.uInsp_API_ID,new uInsp_API(this.props.uInsp_API_ID));




    class  GenPerif_API
    {

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

      
      saveMachineSetupIntoFile(filename = this.settingFilePath)
      {
        
        let act = comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID,"SV", 0,
          { filename: filename },
          new TextEncoder().encode(JSON.stringify(this.machineSetup, null, 4)),
          {
            resolve:(res)=>{
              console.log(res);
            }, 
            reject:(res)=>{
              console.log(res);
            }, 
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
        }).catch((err) => {

          log.info("LoadSettingToMachine>> step3-error", err);
        })
      }

      machineSetupUpdate(newMachineInfo,doReplace=false)
      {

        this.machineSetup=doReplace==true?newMachineInfo:{...this.machineSetup,...newMachineInfo};
        // console.log(this.machineSetup);
        StoreX.dispatch({type:"WS_UPDATE",id:this.id,machineSetup:this.machineSetup});
        this.send({type:"set_setup",...newMachineInfo},
        (ret)=>{
          console.log("<<<<<<*************>>>>>>",ret);
          //HACK: just assume it will work
          // this.machineSetup={...this.machineSetup,...newMachineInfo};
          // console.log(ret);
        },(e)=>console.log(e));
      }
      
      machineSetupReSync() {
        console.log("*************");
        this.send({type:"get_setup"},
        (ret)=>{
          if(ret["ack"]!=true)
          {
            console.log("get setup failed:",ret);
            return;
          }
          delete ret["type"];
          delete ret["id"];
          delete ret["st"];
          delete ret["ack"];
          this.machineSetup=ret;
          this.machineSetupUpdate(this.machineSetup,true);
        },(e)=>console.log(e));
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
                    // this.default_pulse_hz = machInfo.pulse_hz;
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
            console.log(e);
            StoreX.dispatch({type:"WS_DISCONNECTED",id:this.id,data:undefined});
            
          }
        });
      }

      constructor(id,settingFilePath,pg_id_channel=10025)
      {
        this.settingFilePath=settingFilePath;
        this.CONN_ID=undefined;
        this.pg_id_channel=pg_id_channel;
        this.id=id;
        this.connInfo=undefined;
        this.inReconnection=false;
        this.checkReconnectionInterval=setInterval(()=>this.checkReConnection(),3000);//watch dog to do reconnection
        this.runPINGInterval=setInterval(()=>this._sendPing(),3000);//watch dog to do reconnection

        this.trackingWindow={};
        this.idCounter=10;
        this.PINGCount=0;

        this.machineInfo=undefined;

        
      } 
      checkReConnection()
      {
        // console.log(this.connInfo,this.connected,this.inReconnection);
        if(this.connInfo===undefined ||this.CONN_ID!==undefined || this.inReconnection==true)
        {
          return;
        }
        this.connect(this.connInfo);
        // this.checkReconnectionTimeout=setTimeout(,);
      }

      getMachineSetup()
      {
        return this.machineSetup;
      }

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
        // console.log(this.CONN_ID);

        this.triggerPing();
        // this.machineSetupUpdate({pulse_hz:0});
      }
      triggerPing()
      {
        
        this.send({type:"PING"},(ret)=>{
          // console.log(ret);
          delete ret["type"]
          delete ret["id"]
          delete ret["st"]
          let machineStatus={...ret};
          let res_count=machineStatus.res_count||{OK:0,NG:0,NA:0};

          let currentTime_ms=new Date().getTime();

          this.PINGCount=0;
        },errorInfo=>console.log(errorInfo));

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

        return {...defaultRule,...JSON.parse(readRule)};
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
          console.log(reportStatisticState)

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

        console.log(this.showOpenDialog);
        this.connected=false;
        comp.props.DISPATCH({type:"WS_DISCONNECTED",id,data:undefined})
        this.websocket.onopen = (e)=> {
          console.log(this.showOpenDialog);
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
        
        console.log(this.showOpenDialog);
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
          title=""//{this.props.DICT._.system_status+" "+localVersion}
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
          </div>

          <System_Status_Display showText iconSize={30} gridSize={90}
            onItemClick={(connInfo)=>{
              console.log(connInfo);
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
            <Divider>V{this.props.System_Setting.version}</Divider>
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

