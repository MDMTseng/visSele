
import 'antd/dist/antd.less';
import { connect } from 'react-redux'
import React, { useState, useEffect,useRef } from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx,TagDisplay_rdx,isTagFulFillRequrement, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
import { DEF_EXTENSION, defFileFilter } from 'UTIL/BPG_Protocol';
import QRCode from 'qrcode'
import JSum from 'jsum'
import dclone from 'clone';
import { CusDisp_DB } from 'UTIL/DB_Query';
import ComponentBoundary from './component/ComponentBoundary';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import APP_INSP_MODE_rdx from './InspectionUI';
import { usePerifConn } from './perif/PerifAPI';
import BackLightCalibUI_rdx from './BackLightCalibUI';
import CalibrationUI_rdx from './CalibrationUI';
import InstInspUI_rdx from './InstInspUI';


import RepDisplayUI_rdx from './RepDisplayUI';
import InputNumber from 'antd/lib/input-number';
import { xstate_GetCurrentMainState, GetObjElement, Calibration_MMPP_offset ,LocalStorageTools,websocket_autoReconnect,websocket_reqTrack, dictLookUp} from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.main');
import { loadDefWithImageFallback } from 'UTIL/DefLoadWithImageFallback';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import ReactResizeDetector from 'react-resize-detector';

import { BPG_FileBrowser, BPG_FileSavingBrowser,BPG_FileBrowser_varify_info } from './component/baseComponent.jsx';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';

import { default as AntButton } from 'antd/lib/button';

import PageHeader from 'antd/lib/page-header';
import Typography from 'antd/lib/typography';
import Collapse from 'antd/lib/collapse';
import Divider from 'antd/lib/divider';
import Card from 'antd/lib/card';
import Carousel from 'antd/lib/carousel';
import Popover from 'antd/lib/popover';
import Affix from 'antd/lib/affix';
import  Table  from 'antd/lib/table';
import Switch from 'antd/lib/switch';
import Row from 'antd/lib/row';
import Col from 'antd/lib/col';
import Steps from 'antd/lib/steps';

import { useSelector,useDispatch } from 'react-redux';
const { Meta } = Card;
const { Step } = Steps;
import { 
  PauseOutlined,
  SendOutlined,
  RollbackOutlined,
  DeleteOutlined,
  MinusOutlined,
  SelectOutlined,
  SaveOutlined,
  MonitorOutlined,
  FolderOpenOutlined,
  InfoCircleOutlined,
  EditOutlined,
  TableOutlined,
  DownOutlined,
  ThunderboltOutlined,
  LeftOutlined,
  RightOutlined,
  LinkOutlined,
  DisconnectOutlined,
  ScanOutlined,
  CaretUpOutlined,
  CaretDownOutlined,
  HistoryOutlined,
  SettingOutlined,
  CameraOutlined,
  DatabaseOutlined,
  QrcodeOutlined,
  PlusSquareOutlined,
  CaretRightOutlined,
  CloudServerOutlined,
  CloseCircleTwoTone,
  LoadingOutlined,
  WarningOutlined } from '@ant-design/icons';
import Menu from 'antd/lib/menu';
import Button from 'antd/lib/button';
import Layout from 'antd/lib/layout';
import Modal from 'antd/lib/modal';
import Input from 'antd/lib/input';
import Tag from 'antd/lib/tag';
import Dropdown from 'antd/lib/dropdown';
import message from 'antd/lib/message';
const { Header, Content, Footer, Sider } = Layout;
const SubMenu = Menu.SubMenu;
const { Paragraph, Title } = Typography;

const IMG_LOAD_DOWNSAMP_LEVEL=1;

var require=require||(()=>undefined);
const electron = require('electron')
const fs = require('fs');
const path = require('path')
// let ELECTRON_IPC = new websocket_reqTrack(new websocket_autoReconnect("ws://localhost:9714/",5*1000));

// ELECTRON_IPC.onreconnection = (reconnectionCounter) => {
//   console.log("onreconnection" + reconnectionCounter);
//   if (reconnectionCounter > 10) return false;
//   return true;
// };
// ELECTRON_IPC.onopen = () => 
// {
//   ELECTRON_IPC.send_obj({"type":"get_UI_url"})
//   .then((data)=>{
//     console.log(data)
//   })
//   .catch((err)=>{
//     console.log(err)
//   })

//   console.log("ELECTRON_IPC:onopen");
// }
// ELECTRON_IPC.onmessage = (msg) => console.log("ELECTRON_IPC:onmessage::", msg);
// ELECTRON_IPC.onconnectiontimeout = () => console.log("ELECTRON_IPC:onconnectiontimeout");
// ELECTRON_IPC.onclose = () => console.log("ELECTRON_IPC:onclose");
// ELECTRON_IPC.onerror = () => console.log("ELECTRON_IPC:onerror");


import { 
  ArrowLeftOutlined,


} from '@ant-design/icons';

class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.counttt==0;
  }
  ec_canvas_EmitEvent(event) {
  }
  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.Preview_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);

    if (this.props.onCanvasInit !== undefined) {
      this.props.onCanvasInit(this.ec_canvas);
    }
    this.updateCanvas(this.props.c_state);
  }
  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }
  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.EditDBInfoSync(props.edit_info);
      this.ec_canvas.SetState(ec_state);
      this.ec_canvas.SetShowInspectionNote(props.showInspectionNote);
      // console.log(props.edit_info,ec_state,props.showInspectionNote);
      //this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
    }
  }

  onResize(width, height) {
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
    }
  }

  componentDidUpdate(prevProps, prevState) {
    this.updateCanvas(this.props.c_state, this.props);
  }

  render() {

    return (
      <div className={this.props.className}  style={this.props.style}>
        <canvas ref="canvas" className="s width12 height12" />
        {(this.props.disable_resize_detector===true)?
          null:
          <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />}
      </div>
    );
  }
}

class QR_Canvas extends React.Component {

  constructor(props) {
    super(props);
    this.QR_Content = "";
    this.state = {
      canvas: undefined,
      fUpdateC: 0
    };
  }

  componentDidMount() {
    this.setState({ ...this.state, canvas: this.refs.canvas });
  }
  onResize(width, height) {
    this.refs.canvas.width = width;
    this.refs.canvas.height = height;
    this.setState({ ...this.state, fUpdateC: this.state.fUpdateC++ });
  }
  /*shouldComponentUpdate(nextProps, nextState)
  {
    return nextProps.QR_Content!=this.QR_Content || this.refs.canvas==undefined;
  }*/
  componentDidUpdate(prevProps, prevState) {
    this.QR_Content = this.props.QR_Content;
  }

  render() {

    if (this.refs.canvas !== undefined)
      QRCode.toCanvas(this.refs.canvas, this.props.QR_Content, { errorCorrectionLevel: 'L' }, function (error) {
        if (error) console.error(error)
      })
    return (
      <div className={this.props.className} style={this.props.style}>
        <canvas ref="canvas" className="width12 HXF veleX" onClick={this.props.onClick} />
        {(this.props.disable_resize_detector===true)?
          null:
          <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />}
      </div>
    );
  }
}

const mapStateToProps_CanvasComponent = (state) => {
  //console.log("mapStateToProps",JSON.stringify(state.UIData.c_state));
  return {
    c_state: state.UIData.c_state,
    edit_info: state.UIData.edit_info,

  }
}

const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => {
  return {}
}
const CanvasComponent_rdx = connect(
  mapStateToProps_CanvasComponent,
  mapDispatchToProps_CanvasComponent)(CanvasComponent);


function isString(data) {
  return (typeof data === 'string' || data instanceof String);
}

function getLocalStorage_RecentFiles()
{
  let LocalS_RecentDefFiles =LocalStorageTools.getlist("RecentDefFiles");
  LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter(BPG_FileBrowser_varify_info);
  return LocalS_RecentDefFiles;
}

function appendLocalStorage_RecentFiles(fileInfo)
{
  
  return LocalStorageTools.appendlist("RecentDefFiles",fileInfo,
    (ls_fileInfo,idx) =>
      (idx<100)&&//Do list length limiting
      (ls_fileInfo.name != fileInfo.name || ls_fileInfo.path != fileInfo.path));

}


const InspectionDataPrepare = ({onPrepareOK}) => {
  const caruselRef = useRef(undefined);

  const DICT = useSelector(state => state.UIData.DICT);
  const System_Setting = useSelector(state => state.UIData.System_Setting);
  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);

  const Info_decorator = useSelector(state => state.UIData.edit_info.__decorator);
  const shapeListForRank = useSelector(state => state.UIData.edit_info._obj.shapeList);
  const CAM1_ID_CONN_INFO = useSelector(state => state.ConnInfo.CAM1_ID_CONN_INFO);
  const uInsp_API_ID_CONN_INFO = usePerifConn(useSelector(state => state.ConnInfo.uInsp_API_ID));
  const SLID_API_ID_CONN_INFO = usePerifConn(useSelector(state => state.ConnInfo.SLID_API_ID));

  
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const defModelPath = useSelector(state => state.UIData.edit_info.defModelPath);
  const DefFileName = useSelector(state => state.UIData.edit_info.DefFileName);
  const DefFileHash = useSelector(state => state.UIData.edit_info.DefFileHash);
  const db_server_url = "localhost:8085";
  
  const InspectionMonitor_URL= useSelector(state => state.UIData.InspectionMonitor_URL);
  const dispatch = useDispatch();
  const ACT_Def_Model_Path_Update= (path) => dispatch(UIAct.Def_Model_Path_Update(path));
  const ACT_WS_SEND_BPG= (tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, tl, prop, data, uintArr, promiseCBs));
  const ACT_InspOptionalTag_Update= (newTags) => dispatch(DefConfAct.InspOptionalTag_Update(newTags));
  
  const [InfoPopUp,setInfoPopUp]=useState(undefined);
  const [ErrorInfo,setErrorInfo]=useState(undefined);

  
  const [showInspectionNote,setShowInspectionNote]=useState(false);

  
  const _mus = useSelector(state => state.UIData.machine_custom_setting);
  const [fileSelectorInfo,setFileSelectorInfo]=useState(undefined);
  
  const [stepIdx,setStepIdx]=useState(0);
  const [isVertical,setIsVertical]=useState(false);
  // Tracks whether we've already pushed the camera-setting + calib reload for
  // the CURRENT camera connection. The effect below re-runs on every
  // CAM1_ID_CONN_INFO reference change (the conn channel emits a fresh object
  // on each ~2.6s status heartbeat), but the reload is only meaningful on the
  // disconnected->connected transition -- without this guard the heavy
  // CameraSettingFromFile + lens/field calib reload (~2s of camera I/O) re-ran
  // every heartbeat, throttling live inspection.
  const camReloadSentRef = useRef(false);
  let DefFileFolder=undefined;
  // console.log(uInsp_API_ID_CONN_INFO);
  useEffect(()=>{
    let is_Cam_Ready=GetObjElement(CAM1_ID_CONN_INFO,["type"])=="WS_CONNECTED";
    let is_uInsp_Ready=GetObjElement(uInsp_API_ID_CONN_INFO,["type"])=="WS_CONNECTED";
    let is_SLID_Ready=GetObjElement(SLID_API_ID_CONN_INFO,["type"])=="WS_CONNECTED";
    if(uInsp_API_ID_CONN_INFO===undefined)
    {
      is_uInsp_Ready=true;//if the uInsp is not set(maybe not the full inspection machine) ig nore it
    }
    // if(SLID_API_ID_CONN_INFO===undefined)
    {
      is_SLID_Ready=true;//if the uInsp is not set(maybe not the full inspection machine) ig nore it
    }

    let CamInfo=is_Cam_Ready?undefined:dictLookUp("camera_reconnection_caption", DICT);
    let uInspInfo=is_uInsp_Ready?undefined:dictLookUp("uInsp_reconnection_caption", DICT);
    let SLIDInfo=is_SLID_Ready?undefined:"坡檢設備重連中";


    let isSystemReadyForInsp=is_Cam_Ready && is_uInsp_Ready && is_SLID_Ready;
    
    // Whenever the core (camera channel) reaches a connected state, tell it
    // to (re)load camera setting + lens calib + field calib so the sampler's
    // mmpp is primed. Without this, any mode that bypasses CalibrationUI
    // (e.g. DefConfUI -> triggerSnapExam -> EX -> mmpP_ideal()) would get
    // the default calibPpB=1, calibmmpB=1 -> mmpp = 1.0 mm/px (wildly wrong).
    if (is_Cam_Ready) {
      // Fire ONCE per connection (on the disconnected->connected edge). The
      // conn-info object churns every heartbeat; re-sending each time would
      // re-run the camera's full setting + calib reload and starve streaming.
      if (!camReloadSentRef.current) {
        camReloadSentRef.current = true;
        ACT_WS_SEND_BPG("RC", 0, {
          target: "calib_files_load",
          camera_setting_dir: "data/",
          lens_calib_path: "data/lens_calib.json",
          field_calib_path: "data/field_calib.json",
        });
      }
    } else {
      // Camera dropped -- arm the reload for the next reconnection.
      camReloadSentRef.current = false;
    }

    if(!isSystemReadyForInsp)
    {
      setErrorInfo({
        content:<div>
          <div className="antd-icon-sizing" style={{height:"50px"}}>
            <LoadingOutlined/>
          </div>
          {[CamInfo,uInspInfo,SLIDInfo]
            .filter(info=>info!==undefined)
            .map(info=>
            <Title level={2} style={{textAlign:"center"}} >
              {info}
            </Title>
          )}

          <Button size="small" type="primary" onClick={()=>{
            // ACT_WS_SEND_BPG( "LD", 0, { deffile: defModelPath + '.' + DEF_EXTENSION, imgsrc: defModelPath ,down_samp_level},undefined,{
            //   resolve:(pkts,WSDataDispatch)=>{
            //     console.log(pkts);
            //     WSDataDispatch(pkts);
            //   }
            // })
            dispatch({type:"System_Setting_Update",data:{...System_Setting,ALLOW_SOFT_CAM:true}})
          }}
          // Found by its Chinese label in three harnesses. This modal sits over
          // MAIN after a WS bounce and intercepts every click, so failing to
          // dismiss it does not look like "the button moved" -- it looks like
          // the whole page stopped responding, which is how it was first
          // diagnosed as a network fault.
          data-testid="cam-reconnect-skip"
          >跳過相機連線</Button>
        </div>,
        onCancel:()=>{},
        onOK:()=>{}
      });
    }
    else
    {
      if(ErrorInfo!==undefined)
      {
        setErrorInfo(undefined);
      }
    }
  },[CAM1_ID_CONN_INFO,uInsp_API_ID_CONN_INFO])

  
  useEffect(()=>{
    setTimeout(()=>{
      
      let down_samp_level=IMG_LOAD_DOWNSAMP_LEVEL;
      if(down_samp_level>3)down_samp_level=3;

      loadDefWithImageFallback({
        defModelPath, defExtension: DEF_EXTENSION, downSampLevel: down_samp_level,
        send: (payload, promiseCBs) => ACT_WS_SEND_BPG("LD", 0, payload, undefined, promiseCBs),
      })
        .then(({ pkts, actionChannel }) => { actionChannel(pkts); })
        .catch((err) => log.warn("[auto-load]", { defModelPath, err: String(err) }));
    },50);

  },[])


  function SignatureTargetMatching(fileInfoList,onResultJudge,onMatchingResult,trigger_type=0,timeout=-1)
  {
    if(fileInfoList==undefined||fileInfoList.length==0)
    {
      onMatchingResult({files:[]});
      return;
    }
    ACT_WS_SEND_BPG( "ST", 0,
    { CameraSetting: { ROI:[0,0,99999,99999] } })
    ACT_WS_SEND_BPG( "EX", 0, {
      
        trigger_type,
        timeout,
      },
      undefined, { 
      resolve:(pkts)=>{
        
        let signature = GetObjElement(pkts,[0,"data","reports",0,"signature"]);

        if(onResultJudge(pkts)!=true)
        {
          return;
        }
        // console.log(signature);
        ACT_WS_SEND_BPG( "SC", 0, {
          type:"signature_files_matching",
          signature: signature,
          files:fileInfoList.map(fileInfo=>fileInfo.path)
          },undefined,{
          resolve:(pkts,defaultFlow)=>{
            if(pkts[0].data.files===undefined)pkts[0].data.files=[];
            pkts[0].data.files.forEach((fileSigMatchingInfo)=>{
              fileSigMatchingInfo.error = fileSigMatchingInfo.p_error<fileSigMatchingInfo.n_error?fileSigMatchingInfo.p_error:fileSigMatchingInfo.n_error;
              fileSigMatchingInfo.file=fileInfoList[fileSigMatchingInfo.idx]
            })
            let sortedErrorList = pkts[0].data.files.sort((a, b)=> {
              return a.error - b.error;
            })
            onMatchingResult(pkts[0].data);
          }
        })  

      }, reject:(e)=>{
        onResultJudge(undefined);
      } });
  }

  // inspection_monitor_url comes from the core (machine_custom_setting). encodeURI
  // does NOT strip `javascript:` / `data:` schemes — so a malicious core could
  // make window.open() execute. Allow only http(s)/ws(s); anything else → ignore.
  const _safeMonitorScheme = (u) => typeof u === 'string' && /^(https?|wss?):/i.test(u);
  let InspectionMonitor_URL_overvall = _safeMonitorScheme(_mus.inspection_monitor_url) ? _mus.inspection_monitor_url : undefined;
  let InspectionMonitor_URL_w_info   = InspectionMonitor_URL_overvall;
  if (InspectionMonitor_URL_overvall!==undefined && isString(DefFileHash) && DefFileHash.length > 5) {

    if(InspectionMonitor_URL_overvall.includes("?")==false)
    {
      InspectionMonitor_URL_overvall+="?"
    }
    InspectionMonitor_URL_overvall+="v=" + 0;


    InspectionMonitor_URL_w_info= InspectionMonitor_URL_overvall + "&search_name=" + DefFileName;

    InspectionMonitor_URL_overvall = encodeURI(InspectionMonitor_URL_overvall);
    InspectionMonitor_URL_w_info = encodeURI(InspectionMonitor_URL_w_info);
  }
  DefFileFolder = defModelPath.substr(0, defModelPath.lastIndexOf('/') + 1);

  if(caruselRef.current!==undefined)
  {
    caruselRef.current.goTo(stepIdx);
    
    caruselRef.current.slick.innerSlider.swipeMove=()=>{};
  }
  const stepInc=()=>{
    setStepIdx(stepIdx+1);
  }

  
  function loadMachineSettingPopUp()
  {
    
    let popUpUIInfo = {
      title: "機台設定",
      onOK: undefined,
      onCancel: undefined,//make sure it reloads everytime
      content: <CustomDisplaySelectUI key={"CustomDisplaySelectUI_"+(new Date().getMilliseconds())} onSelect={(cusDispInfo) => {

        let tarDef = cusDispInfo.targetDeffiles[0];
        let filePath = tarDef.path;
        if (filePath === undefined) return;
        filePath = filePath.replace("." + DEF_EXTENSION, "").replaceAll("\\" , "/");
        setInfoPopUp(undefined);

        loadDefWithImageFallback({
          defModelPath: filePath, defExtension: DEF_EXTENSION, downSampLevel: IMG_LOAD_DOWNSAMP_LEVEL,
          send: (payload, promiseCBs) => ACT_WS_SEND_BPG("LD", 0, payload, undefined, promiseCBs),
        })
          .then(({ pkts, actionChannel }) => {
            let setTags = [];
            try { setTags = tarDef.tags.split(","); } catch (e) { setTags = []; }
            ACT_Def_Model_Path_Update(filePath);
            actionChannel(pkts);
            ACT_InspOptionalTag_Update(setTags);
          })
          .catch((err) => {
            log.warn("[load-custom-disp]", { filePath, err: String(err) });
            let errPopUpUIInfo = {
              title: "錯誤",
              onOK: undefined,
              onCancel: undefined,
              content:<div style={{width:"100%",height:"200px"}}><Title className="veleXY">
                <CloseCircleTwoTone twoToneColor="#FF0000"/>找不到檔案:{filePath}
                </Title></div>
            };
            setTimeout(()=>setInfoPopUp(errPopUpUIInfo),100);
          });

      }} />
    }
    setInfoPopUp(popUpUIInfo);
  }

  let UI_Stack=[];

  let isSystemReadyForInsp=GetObjElement(CAM1_ID_CONN_INFO,["type"])=="WS_CONNECTED";
  let isOK;
  let isStillOK=true;
  
  let OKJumpTo=0;
  const stepJump=(toIdx)=>{
    if(toIdx>OKJumpTo)
      return false;
    setStepIdx(toIdx);
    return true;
  }
  {//1st page
    
    let new_tagGroupsPreset=tagGroupsPreset;

    if(Info_decorator!==undefined && Info_decorator.control_margin_info!==undefined)
    {
      new_tagGroupsPreset=[
        {
          name:"已設定範圍",
          maxCount:1,
          tags:Object.keys(Info_decorator.control_margin_info)
        },...new_tagGroupsPreset]
    }

    // 檢測等級, offered as tags because that is how every other per-part choice
    // is already made here -- and because the level has to travel with the
    // part, not sit in a slider. A rankN tag folds itself into
    // quality_essential when inspection starts (see componentDidMount in
    // InspectionUI), so the level reaches the wire def and the core reduces on
    // the same field the screen does. A slider could never do that: the core
    // has no notion of rank and cannot be told about one after the def is sent.
    //
    // Generated from the ranks the recipe actually uses, so the list cannot
    // offer a level that selects nothing, and omitted entirely when every
    // measurement sits at the same rank -- there is no choice to make then.
    {
      // THE UNION OF ROOT AND OVERRIDE RANKS.
      //
      // Asking only the root shapeList found nothing: in a real recipe the
      // measures carry no rank of their own and every level is expressed
      // per-製程, in the control margin rows -- which is where an operator sets
      // them, in the margin editor. Reading one source offered an empty list
      // and no way to choose a level at all.
      const mids = new Set((shapeListForRank||[])
        .filter(sh => sh && sh.type === UIAct.SHAPE_TYPE.measure).map(sh => sh.id));
      const cmi = (Info_decorator||{}).control_margin_info || {};
      const ranks = [...new Set([
        ...(shapeListForRank||[])
          .filter(sh => sh && sh.type === UIAct.SHAPE_TYPE.measure && sh.rank !== undefined)
          .map(sh => sh.rank),
        ...Object.values(cmi).flat()
          .filter(r => r && r.rank !== undefined && mids.has(r.id))
          .map(r => r.rank),
      ])].sort((a,b)=>a-b);
      if (ranks.length > 1) {
        new_tagGroupsPreset=[
          {
            name:"檢測等級",
            maxCount:1,
            tags:ranks.map(r=>"rank"+r)
          },...new_tagGroupsPreset]
      }
    }

    let isFileOK=(DefFileHash!==undefined&&isSystemReadyForInsp) ;
    
    isOK=isFileOK && isTagFulFillRequrement(inspOptionalTag,tagGroupsPreset);
    // Why play is refusing, for the probes. data-ready=0 alone cannot tell a
    // correct refusal from a wrong one -- "no recipe loaded" and "a tag group
    // is unsatisfied" look identical, and so does "ready when it should not
    // be". Deliberately reported against BOTH lists: readiness is computed
    // from tagGroupsPreset while the picker renders new_tagGroupsPreset (the
    // margin group is in the second and not the first), so `tags_shown`
    // disagreeing with `tags_checked` is exactly the gap, and now it is
    // visible rather than inferred. Reporting only; nothing here changes what
    // the button does.
    const playReason =
      !isSystemReadyForInsp ? 'system-not-ready'
      : DefFileHash === undefined ? 'no-def'
      : !isTagFulFillRequrement(inspOptionalTag, tagGroupsPreset) ? 'tags'
      : !isTagFulFillRequrement(inspOptionalTag, new_tagGroupsPreset) ? 'tags-shown-only'
      : 'ok';
    let twoPanelClass1="s height12 width4";
    let twoPanelClass2="s height12 width8";
    if(isVertical)
    {
      twoPanelClass1="s height5 width12"
      twoPanelClass2="s height7 width12"
    }

    function matchingAUTO_AskTriggerType(fileInfoList,displayInfo)
    {

      let triggerTimeout=5000;
      let errPopUpUIInfo = {
        title: displayInfo,
        onOK: undefined,
        onCancel: undefined,
        content:<div style={{width:"100%",height:"400px"}}>
          選擇觸發模式<br/>
          <Button key="back" onClick={()=>{matchingAUTO_UI(fileInfoList)}}>
            立即
          </Button>
          <Button key="trigger5S" type="primary" onClick={()=>{matchingAUTO_UI(fileInfoList,2,triggerTimeout)}}>
            {triggerTimeout/1000}s內觸發
          </Button>
          <Button danger onClick={()=>setInfoPopUp(undefined)}>
            取消
          </Button>
        </div>
      }
      setInfoPopUp(errPopUpUIInfo)
    }




    function matchingAUTO_UI(fileInfoList,trigger_type=0,timeout=-1)
    {

      let errPopUpUIInfo = {
        title: "MATCH ing...",
        onOK: undefined,
        onCancel: undefined,
        content:<div style={{width:"100%",height:"400px"}} className="scroll">
          
          <div className="antd-icon-sizing" style={{height:"50px"}}>
            <LoadingOutlined/>
          </div>
          <Title level={2} style={{textAlign:"center"}} >
            {DICT.mainui.FUNC_auto_recognition_running}
          </Title>
        </div>
      }
      setInfoPopUp(errPopUpUIInfo)

      SignatureTargetMatching(fileInfoList,
        
        (pkts)=>{

          if(pkts===undefined)
          {

            log.warn("[capture] error")
            
            setInfoPopUp({content:<>
              <div className="antd-icon-sizing" style={{height:"50px"}}>
                <WarningOutlined/>
              </div>
              <Title level={2} style={{textAlign:"center"}} >
                圖像獲取異常
              </Title></>})
            return false;
          }
          let SS=pkts.find(pkt=>pkt.type=="SS");

          if(SS===undefined || SS.data.ACK!==true)
          {   
            log.warn("[capture] nak")
            setInfoPopUp({content:<>
              <div className="antd-icon-sizing" style={{height:"50px"}}>
                <WarningOutlined/>
              </div>
              <Title level={2} style={{textAlign:"center"}} >
                圖像獲取失敗
              </Title></>})
            return false;
          }

          let signature = GetObjElement(pkts,[0,"data","reports",0,"signature"]);
          
          if(signature===undefined)
          {
            setInfoPopUp({content:<>
              <div className="antd-icon-sizing" style={{height:"50px"}}>
                <WarningOutlined/>
              </div>
              <Title level={2} style={{textAlign:"center"}} >
                圖像無目標
              </Title></>})
            log.warn("[capture] empty signature")
            return false;
          }


          return true;
        },
        (matchingList)=>{



        let columns = ['name','score','path'].map((info)=>({
          title: info,
          dataIndex: info,
          key:info,
        }));

        const dataSource = matchingList.files
        .map(mat_info=>{
          mat_info.score = (1-(mat_info.error)/(mat_info.mean/4));
          return mat_info}).
        filter(mat_info=>mat_info.score>0)
        .map(mat_info=>{
          return {
            key:mat_info.file.path,
            name:mat_info.name,
            score:(100*mat_info.score).toFixed(1)+"%",
            path:mat_info.file.path,
            matchingInfo:mat_info
          }
        })
        
        
        let errPopUpUIInfo = {
          title: "MATCH",
          onOK: undefined,
          onCancel: undefined,
          content:<div style={{width:"100%",height:"400px"}} className="scroll">
        
            <Table 
              onRow={(file) => ({
                onClick: (evt) => { 
                  //console.log(file,evt);

                  appendLocalStorage_RecentFiles(file.matchingInfo.file);

                  let filePath = file.path.replace("." + DEF_EXTENSION, "");
                  setInfoPopUp(undefined);
                  ACT_Def_Model_Path_Update(filePath);

                  loadDefWithImageFallback({
                    defModelPath: filePath, defExtension: DEF_EXTENSION, downSampLevel: IMG_LOAD_DOWNSAMP_LEVEL,
                    send: (payload, promiseCBs) => ACT_WS_SEND_BPG("LD", 0, payload, undefined, promiseCBs),
                  })
                    .then(({ pkts, actionChannel }) => actionChannel(pkts))
                    .catch((err) => log.warn("[load-recent]", { filePath, err: String(err) }));

                  setFileSelectorInfo(undefined);

                }})} 
              dataSource={dataSource} 
              columns={columns} 
              pagination={false}/>

          </div>
        }
        setInfoPopUp(errPopUpUIInfo)
        },
        trigger_type,timeout
      )

    }


    UI_Stack.push(
      <div key="UI_Step0" className="s width12 height12 overlayCon" style={{background: "rgb(250,250,250)"}}>
        
        <div className={twoPanelClass1} style={{padding: "10px",overflow:"scroll"}}>
          
          <TagDisplay_rdx closable/>
          <TagOptions_rdx className="s width12 HXA" size="middle" tagGroups={new_tagGroupsPreset}/>
        </div>
        <ComponentBoundary name="MainCanvas" fallbackHeight="60vh">
          <CanvasComponent_rdx className={twoPanelClass2} showInspectionNote={showInspectionNote} />
        </ComponentBoundary>
        
        <ReactResizeDetector handleWidth handleHeight onResize={(width, height)=>{
          if(width>height)//landscape
          {
            if(isVertical!=false)setIsVertical(false);
          }
          else
          {
            if(isVertical!=true)setIsVertical(true);
          }
        }} />


        <div className="overlay vbox" style={{
          padding: "10px",
          boxShadow:"inset 0px 0px 15px rgba(0,0,0,0.1), -0.5px 0.5px 2px 1px rgba(20,20,20,0.1)",
          background: "rgba(255,255,255,.6)",
          backdropFilter:" blur(5px)",
          textAlign:"end",
          right:"15px",
          bottom:"15px"}}>

          <Switch checkedChildren="測線" unCheckedChildren="純圖" checked={showInspectionNote} onChange={setShowInspectionNote} />
          <Button className={"antd-icon-sizing HW50"} size="large"
            style={{"pointerEvents": "auto"}} icon={<MonitorOutlined/> } type="text"
            onClick={() =>matchingAUTO_AskTriggerType(getLocalStorage_RecentFiles(),"近期檔案比對")}/>
          
          <Button className={"antd-icon-sizing "+(isFileOK?"HW50":"HW100")} size="large"
            style={{"pointerEvents": "auto"}} icon={<FolderOpenOutlined/> } type="text"
            onClick={() => {
            let fileSelectedCallBack =
              (filePath, fileInfo) => {


                filePath = filePath.replace("." + DEF_EXTENSION, "");
                setFileSelectorInfo(undefined);

                loadDefWithImageFallback({
                  defModelPath: filePath, defExtension: DEF_EXTENSION, downSampLevel: IMG_LOAD_DOWNSAMP_LEVEL,
                  send: (payload, promiseCBs) => ACT_WS_SEND_BPG("LD", 0, payload, undefined, promiseCBs),
                })
                  .then(({ pkts, actionChannel }) => {
                    appendLocalStorage_RecentFiles(fileInfo);
                    ACT_Def_Model_Path_Update(filePath);
                    actionChannel(pkts);
                  })
                  .catch((err) => {
                    log.warn("[load-file-selector]", { filePath, err: String(err) });
                    let errPopUpUIInfo = {
                      title: dictLookUp("ERROR", DICT),
                      onOK: undefined,
                      onCancel: undefined,
                      content:<div style={{width:"100%",height:"200px"}}><Title className="veleXY">
                        <CloseCircleTwoTone twoToneColor="#FF0000"/>{DICT.mainui.FILE_NOT_FOUND}:{filePath}
                        </Title></div>
                    };
                    setInfoPopUp(errPopUpUIInfo);
                  });
                }


            let fileGroups = [
              { name: "history", list: getLocalStorage_RecentFiles() },
              
            ];
            let fileSelectFilter = defFileFilter;

            setFileSelectorInfo({
              callBack:fileSelectedCallBack,
              filter:fileSelectFilter,
              groups:fileGroups,
              additionalFuncs:[
                { icon: <MonitorOutlined/>, 
                  name:"資料夾比對",
                  key:"matching",
                  action: (state,props)=>{
                  let files=state.folderStruct.files.filter(fileInfo=>fileInfo.type=="REG"&&props.fileFilter(fileInfo))
                  if(files!==undefined && files.length>0)
                  {
                    // console.log(state.folderStruct);
                    matchingAUTO_AskTriggerType(files,<>資料夾比對<br/> {state.folderStruct.path}</>);
                    setFileSelectorInfo();
                  }
                }},
              ]
            });
          }}/>


          
          {_mus.cusdisp_db_fetch_url!==undefined?<Button className={"antd-icon-sizing "+(isFileOK?"HW50":"HW100")} size="large"
            style={{"pointerEvents": "auto"}} icon={<CloudServerOutlined/> } type="text"
            onClick={loadMachineSettingPopUp}
            ></Button>:null}
          <Popover 
            style={{"pointerEvents": "auto"}}
            content={<>
              <Button onClick={() => window.open(InspectionMonitor_URL_overvall)} >完整資料庫搜尋</Button>
              <Button onClick={() => window.open(InspectionMonitor_URL_w_info)} >檢測資料搜尋</Button>
              <QR_Canvas className="veleX" style={{height:"100%"}}
                    onClick={() => window.open(InspectionMonitor_URL_w_info)} QR_Content={InspectionMonitor_URL_w_info} />
            </>} 
            trigger={"hover|click"}>
            <Button type="text" className="antd-icon-sizing HW50" size="large" disabled={false} icon={<QrcodeOutlined/> }/>
          </Popover>

          
          
          {/* data-testid="main-play": the control that enters the Inspection
              UI. Three harnesses find it today as "the widest button in the
              bottom-right corner", because it shares its class with its 50x50
              neighbours -- geometry standing in for identity. That is one
              layout change away from clicking the QR popover or the file
              browser instead, and it silently would: they are all icon-only
              text buttons. data-ready publishes whether pressing it will do
              anything, which no amount of DOM reading recovers -- the disabled
              styling is a colour. */}
          <Button className={"antd-icon-sizing  "+(isOK?"HW100":"HW50")} size="large"
            data-testid="main-play" data-ready={isOK ? '1' : '0'} data-reason={playReason}
            style={{"pointerEvents": "auto","color":(isOK?"#5191a5":"__")}} icon={<CaretRightOutlined/> } type="text"
            disabled={!isOK}
            onClick={onPrepareOK}/>



        </div>
      </div>
    );
  }

  return(
    
  <>
    {UI_Stack}


    <BPG_FileBrowser key="BPG_FileBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={DefFileFolder} visible={fileSelectorInfo !== undefined}
        BPG_Channel={(...args) => ACT_WS_SEND_BPG( ...args)}
        onFileSelected={(filePath, fileInfo) => {
          // Read the callback BEFORE clearing the state, and tolerate it being
          // gone. setFileSelectorInfo(undefined) does not change this closure's
          // captured value, but a selection can still arrive after the dialog
          // has been dismissed some other way -- and then this threw
          // "Cannot read properties of undefined (reading 'callBack')",
          // which surfaces as a page error with no hint that a file was being
          // opened at the time.
          const cb = fileSelectorInfo && fileSelectorInfo.callBack;
          setFileSelectorInfo(undefined);
          if (cb) cb(filePath, fileInfo);
        }}
        onCancel={() => {
          setFileSelectorInfo(undefined);
        }}
        
        fileGroups={(fileSelectorInfo !== undefined)?fileSelectorInfo.groups:undefined}
        additionalFuncs={(fileSelectorInfo !== undefined)?fileSelectorInfo.additionalFuncs:undefined}
        fileFilter={(fileSelectorInfo !== undefined)?fileSelectorInfo.filter:undefined} />

      <Modal
        title={InfoPopUp === undefined ? "" : InfoPopUp.title}
        visible={InfoPopUp !== undefined}
        
        footer={(InfoPopUp===undefined || (InfoPopUp.onOK===undefined &&  InfoPopUp.onCancel===undefined))?null:undefined}
        onOk={() => {
          if(InfoPopUp.onOK!==undefined)InfoPopUp.onOK();
          setInfoPopUp(undefined);
        }}
        onCancel={() => {
          if(InfoPopUp.onCancel!==undefined)InfoPopUp.onCancel();
          setInfoPopUp(undefined);
        }}
      >
        {InfoPopUp === undefined ?
          null : InfoPopUp.content}
      </Modal>

    <Modal
      closable={false}
      visible={ErrorInfo !== undefined}
      centered
      title={ErrorInfo!=undefined?ErrorInfo.title:null}
      footer={null}
      onOk={() => {
        if(ErrorInfo!==undefined)
          ErrorInfo.onOK();
      }}
      onCancel={() => {
        if(ErrorInfo!==undefined)
          ErrorInfo.onCancel();
      }}
    >
      {ErrorInfo === undefined ?
        null : ErrorInfo.content}
    </Modal>
  </>

  );
};



const Setui_UI=({machCusSetting,onMachCusSettingUpdate,onExtraCtrlUpdate})=>{

  const dispatch = useDispatch();
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const ACT_WS_SEND_BPG= (tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, tl, prop, data, uintArr, promiseCBs));
  const ACT_Report_Save = (filename, content,promiseCBs) => {ACT_WS_SEND_BPG("SV", 0,{ filename},content,promiseCBs)};

  const Platform_API_ID = useSelector(state => state.ConnInfo.Platform_API_ID);
  const Platform_API_ID_CONN_INFO = useSelector(state => state.ConnInfo.Platform_API_ID_CONN_INFO);
  const ACT_PLAT_OBJ= (callback)=>dispatch(UIAct.EV_WS_GET_OBJ(Platform_API_ID,callback));
  
  

  const [st_machine_custom_setting ,_set_st_machine_custom_setting] = useState(machCusSetting);
  const [origin_machine_custom_setting,set_origin_machine_custom_setting] = useState(machCusSetting);
  function set_st_machine_custom_setting(new_setting)
  {
    _set_st_machine_custom_setting(new_setting);
    if(onMachCusSettingUpdate!==undefined)
      onMachCusSettingUpdate(new_setting);
  }

  function isUpdated()
  {
    if(origin_machine_custom_setting===undefined || st_machine_custom_setting===undefined)
    {
      return true;
    }
    // console.log(origin_machine_custom_setting,st_machine_custom_setting);
    return JSum.digest(origin_machine_custom_setting, 'sha1', 'hex')!==JSum.digest(st_machine_custom_setting, 'sha1', 'hex');
  }

  
  // Re-seed from the store, but never over unsaved work.
  //
  // This used to overwrite both copies unconditionally, so anything that
  // touched machine_custom_setting -- the station panel finishing a save is
  // the everyday one -- silently discarded whatever was half-typed here. No
  // warning, no undo: the fields just reverted mid-edit.
  //
  // With edits pending, keep them and leave `origin` alone too: origin is the
  // baseline the save diffs against, so moving it under a dirty form would
  // make the operator's own changes look like they were already applied and
  // drop them from the write.
  const dirtyRef = useRef(false);
  dirtyRef.current = isUpdated();
  useEffect(() => {
    if (dirtyRef.current) {
      log.info('[machine-setting] store changed while this form has unsaved edits -- keeping the edits');
      return;
    }
    set_origin_machine_custom_setting(machCusSetting);
    _set_st_machine_custom_setting(machCusSetting);
  }, [machCusSetting]);
  useEffect(() => {

    if(onExtraCtrlUpdate!==undefined)
    {
      let ctrlInfo={};

      if(isUpdated())
      {
        ctrlInfo.fetchSetting=()=>st_machine_custom_setting;
        // The seed this panel was opened with. The save needs it to work out
        // WHICH keys the operator actually touched -- without it the only
        // thing it can write is the whole cached file, which is how this
        // panel reverts changes it never saw. isUpdated() already compares
        // these two; this just exposes the other half.
        ctrlInfo.fetchOrigin=()=>origin_machine_custom_setting;
      }

      ctrlInfo.isUpdated=isUpdated;


      onExtraCtrlUpdate(ctrlInfo)
    }
  }, [st_machine_custom_setting]);



  let InspectionModeOption={
    CI:"檢驗",
    FI:"全檢",
    FI_C:"觸發檢驗",
  }
  
  const InspectionModeOptionMenu = (
    <Menu>
      {Object.keys(InspectionModeOption).map((key,idx)=>
      <Menu.Item key={"m_"+InspectionModeOption[key]} onClick={()=>{
        set_st_machine_custom_setting({...st_machine_custom_setting,InspectionMode:key});
      }}>
        {InspectionModeOption[key]}
      </Menu.Item>)}

    </Menu>
  );


  // const ACT_PLAT_OBJ= (callback)=>dispatch(UIAct.EV_WS_GET_OBJ(Platform_API_ID,callback));

  let isPlatAPI_ready=Platform_API_ID_CONN_INFO!==undefined && Platform_API_ID_CONN_INFO.type=="WS_CONNECTED"

  // Native folder picker, two ways in.
  //
  //   window.launcher.pickFolder   the launcher's contextBridge (UI/Launcher)
  //   platform_api WS              the old Electron shell's WebSocket
  //
  // The WS route existed only for THIS ONE CALL -- a whole express + ws server
  // running so a button could open a directory dialog -- and it is not
  // configured on this machine (machine_setting.json has no
  // platform_api_conn_info, so the log says "platform_api not configured" and
  // the button has been disabled all along). The launcher exposes the same
  // capability as a direct IPC call.
  //
  // Both are kept: the WS path still works for anyone running the old shell,
  // and neither exists in a plain browser (the Vite dev server), where the
  // button stays disabled exactly as it does today.
  const nativePick = (typeof window !== 'undefined' && window.launcher
                      && typeof window.launcher.pickFolder === 'function')
                     ? window.launcher.pickFolder : undefined;
  const canPickFolder = nativePick !== undefined || isPlatAPI_ready;

  const pickSnapshotFolder = () => {
    const apply = (filePaths) => {
      // A cancelled dialog returns an empty list. Writing filePaths[0] without
      // checking would blank the configured path on every cancel.
      if (!filePaths || !filePaths.length) return;
      set_st_machine_custom_setting({...st_machine_custom_setting, InspSampleSavePath: filePaths[0]});
    };
    const opts = { title: "Select Directory", defaultPath: "",
                   properties: ['openDirectory','createDirectory'] };
    if (nativePick) {
      nativePick(opts)
        .then((r) => apply(r && r.filePaths))
        .catch((e) => log.error("[pickFolder] " + (e && e.message ? e.message : e)));
      return;
    }
    ACT_PLAT_OBJ((obj)=>{
      obj.showOpenDialog(opts)
        .then((result) => apply(result && result.filePaths))
        .catch((e) => log.error("[pickFolder/platform_api] " + (e && e.message ? e.message : e)));
    });
  };

  return <div style={{ padding: 24, background: '#fff', minHeight: 360 }}>
    
    測量模式：
    <Dropdown overlay={InspectionModeOptionMenu} trigger={['click']}>
      <Button>
        {InspectionModeOption[st_machine_custom_setting.InspectionMode]} <DownOutlined />
      </Button>
    </Dropdown>

    <br/>

    檢測快照儲存位置：
    <Button size="large" icon={<MonitorOutlined/> } disabled={!canPickFolder}
        onClick={pickSnapshotFolder}>{st_machine_custom_setting.InspSampleSavePath}</Button>
    <br/>


    全檢儲存NG： <Switch checked={st_machine_custom_setting.FI_INSP_NG_SNAP==true} onChange={(check)=>{
      
      set_st_machine_custom_setting({...st_machine_custom_setting,FI_INSP_NG_SNAP:check});
    }} />

    <br/>
    全檢儲存NG最大數量： 
    {/* The InputNumber that used to sit inside the numpad popup, promoted to
        BE the control: keyboard types directly, inputMode brings the numeric
        on-screen keys on touch. */}
    <InputNumber min={0} precision={0} inputMode="numeric"
      value={st_machine_custom_setting.FI_INSP_NG_SNAP_MAX_NUM}
      onChange={(v)=>{ const n=parseInt(v);
        if(Number.isFinite(n))
          set_st_machine_custom_setting({...st_machine_custom_setting,FI_INSP_NG_SNAP_MAX_NUM:n});
      }} />

    <Divider>RAW</Divider>
    <pre>
    {JSON.stringify(st_machine_custom_setting, null, 4)}
    </pre>

            
  </div>
}

const MainUI=()=>{


  const DICT = useSelector(state => state.UIData.DICT);
  
  const _REF = React.useRef({
    statesTable:{

      RootSelect:{
        type:"RootSelect",
        name:DICT.mainui.MODE_SELECT_MAIN_MENU,
      },
      
      Inspection:{
        type:"Inspection",
        name:DICT.mainui.MODE_SELECT_INSP_PREP,
      },
      DeConf:{
        type:"DeConf",
        name:DICT.mainui.MODE_SELECT_DEFCONF
      },
      InstInsp:{
        type:"InstInsp",
        name:DICT.mainui.MODE_SELECT_PRECISION_VALIDATION
      },
      // BackLightCalib:{
      //   type:"BackLightCalib",
      //   name:DICT.mainui.MODE_SELECT_BACKLIGHT_CALIB,
      // },
      Calibration:{
        type:"Calibration",
        name:DICT.mainui.MODE_SELECT_CALIBRATION,
      },
      RepDisplay:{
        type:"RepDisplay",
        name:DICT.mainui.MODE_SELECT_REP_DISPLAY,
      },
      // PrecisionValidation:{
      //   type:"PrecisionValidation",
      //   name:DICT.mainui.MODE_SELECT_PRECISION_VALIDATION,
      // },
      Setting:{
        type:"Setting",
        name:DICT.mainui.MODE_SELECT_SETTING,
      },
  
    }

  });
  let s_statesTable=_REF.current.statesTable;
  
  const dispatch = useDispatch();
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  
  const [siderCollapse,setSiderCollapse] = useState(true);
  
  const RDX_machine_custom_setting = useSelector(state => state.UIData.machine_custom_setting);
  const ACT_machine_custom_setting_Update= (setting) => dispatch(UIAct.EV_machine_custom_setting_Update(setting));


  const EV_UI_Edit_Mode=()=>dispatch(UIAct.EV_UI_Edit_Mode());
  const EV_UI_Insp_Mode= () =>dispatch(UIAct.EV_UI_Insp_Mode());
  const ACT_WS_SEND_BPG= (tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, tl, prop, data, uintArr, promiseCBs));
  const ACT_File_Save = (filePath, content,promiseCBs) => {
    let act = UIAct.EV_WS_SEND_BPG(CORE_ID, "SV", 0,
      {filename:filePath},
      content,promiseCBs
    )
    dispatch(act);
  }
  
  const [popUpInfo,setPopUpInfo] = useState(undefined);
  const [hideMachineSetting,setHideMachineSetting] = useState(false);
  
  const [UI_state, _setUI_state] = useState(s_statesTable.RootSelect);
  const [extraSideUI, setExtraSideUI] = useState([]);

  function setUI_state(newUI_state)
  {
    if(s_statesTable.RootSelect==newUI_state)
    {
      setExtraSideUI([]);
    }
    _setUI_state(newUI_state);
  }
  let UI=[];
  
  let siderUI_info=undefined;
  //let siderUI=null;

  let card_width=250;
  let style_obj={
    width: card_width,
    height: card_width,
    float: "left",
    margin: "10px"
  }
  let bodyStyle={
    padding:"3px",
    width:"100%",
    height:"100%",
  }


  // UI.push(<div style={{}} className="s HXA WXA veleXY" >

  // <div className="neumorphic variation2" onClick={()=>EV_UI_Edit_Mode()}>
  //   <span><strong>{s_statesTable.DeConf.name}</strong></span>
  // </div>
  // <div className="neumorphic variation2" onClick={()=>setUI_state(s_statesTable.Inspection)}>
  //   <span><strong>{DICT.mainui.MODE_SELECT_INSP_PREP}</strong></span>
  // </div>
  // <br/>
  // <div className="neumorphic variation2" onClick={()=>setUI_state(s_statesTable.BackLightCalib)}>
  //   <span><strong>{DICT.mainui.MODE_SELECT_BACKLIGHT_CALIB}</strong></span>
  // </div>

  // {/* <div className="neumorphic variation2" onClick={()=>setUI_state(s_statesTable.RepDisplay)}>
  //   <span><strong>{DICT.mainui.MODE_SELECT_REP_DISPLAY}</strong></span>
  // </div> */}
  // <div className="neumorphic variation2" onClick={()=>setUI_state(s_statesTable.InstInsp)}>
  //   <span><strong>{DICT.mainui.MODE_SELECT_INST_INSP}</strong></span>
  // </div>

  // {/* <div className="neumorphic variation2" onClick={()=>setUI_state(s_statesTable.BackLightCalib)}>
  //   <span><strong>{DICT.mainui.MODE_SELECT_INST_INSP}</strong></span>
  // </div> */}
  
  // </div>)

  switch(UI_state)
  {
    case s_statesTable.RootSelect:


    
      siderUI_info={
        title:UI_state.name,
        menu:[
          {
            icon:<EditOutlined />,
            text:DICT.mainui.MODE_SELECT_DEFCONF,
            onClick:_=>EV_UI_Edit_Mode()
          },
          {
            icon:<HistoryOutlined />,
            text:DICT.mainui.RepDisplay,
            onClick:_=>setUI_state(s_statesTable.RepDisplay)
          },


        ],
      }


      if(hideMachineSetting==false)
      {
        siderUI_info.menu=siderUI_info.menu.concat([


          {
            icon:<SettingOutlined />,
            text:DICT.mainui.MODE_SELECT_SETTING,
            onClick:_=>setUI_state(s_statesTable.Setting)
          },
          // {
          //   icon:<TableOutlined />,
          //   text:DICT.mainui.MODE_SELECT_BACKLIGHT_CALIB,
          //   onClick:_=>setUI_state(s_statesTable.BackLightCalib)
          // },
          {
            icon:<TableOutlined />,
            text:DICT.mainui.MODE_SELECT_CALIBRATION,
            onClick:_=>setUI_state(s_statesTable.Calibration)
          },
          // {
          //   icon:<PlusSquareOutlined />,
          //   // text:DICT.mainui.MODE_SELECT_INST_INSP,
          //   text:DICT.mainui.MODE_SELECT_PRECISION_VALIDATION,
          //   onClick:_=>setUI_state(s_statesTable.InstInsp)
          // }


        ]);
      }

      siderUI_info.menu.push({
        icon:hideMachineSetting?<CaretDownOutlined />:<CaretUpOutlined />,
        text:hideMachineSetting?"打開機器設定":"隱藏機器設定",
        onClick:_=>{
          setHideMachineSetting(!hideMachineSetting)
        }
      });

      UI.push(<InspectionDataPrepare key="InspectionDataPrepare" onPrepareOK={EV_UI_Insp_Mode}/>);
      
      break;
  
    case  s_statesTable.DeConf:
      //UI=<EV_UI_Edit_Mode  onPrepareOK={EV_UI_Insp_Mode}/>;
      break;
    case  s_statesTable.Inspection:
      
      break;
    case  s_statesTable.Calibration:
      UI.push(<CalibrationUI_rdx key="CalibrationUI" />);
      siderUI_info={
        title:UI_state.name,
        menu:[
          {
            icon:<ArrowLeftOutlined />,
            text:DICT._["<"],
            onClick:_=>setUI_state(s_statesTable.RootSelect)
          },
          ...extraSideUI
        ],
      }
      break;
    case  s_statesTable.BackLightCalib:
      UI.push(<BackLightCalibUI_rdx
        BPG_Channel={(...args) => ACT_WS_SEND_BPG(...args)}
        onExtraCtrlUpdate={extraCtrls=>{
          let extraCtrlUI=[];
          // 舊的「儲存校正」按鈕已移除。
          //
          // 它把 stage_light_report 存成 data/stageLightReport.json, 而那個檔案
          // 沒有任何人讀 -- 核心只在 wiringPanel.cpp 的註解裡提到它, 說校正已改
          // 由 lens_calib.json + field_calib.json 提供 (load_lens_calib /
          // load_field_calib, 由 calib_files_load 觸發)。BackLightCalibUI 自己
          // 的讀取端也早就註解掉了。
          //
          // 也就是說操作員按下去、UI 回報成功、檔案確實寫進磁碟, 但機台行為完全
          // 沒變 -- 這比按鈕不存在更糟。實際生效的背光/場地校正在 CalibrationUI
          // 的 field calib(產生 data/field_calib.json)。
          setExtraSideUI(extraCtrlUI);
        }}

         />);

      
      siderUI_info={
        title:UI_state.name,
        
        menu:[
          {
            icon:<ArrowLeftOutlined />,
            text:DICT._["<"],
            onClick:_=>
            {
              setUI_state(s_statesTable.RootSelect)
              
              ACT_WS_SEND_BPG("RC", 0, {
                target: "camera_setting_refresh"
              });
            }
            // subMenu:[]
          },
          ...extraSideUI
        ],
      }
      break;    

    case  s_statesTable.RepDisplay:
    
      UI.push(<RepDisplayUI_rdx key="RepDisplayUI_rdx"
        BPG_Channel={(...args) => ACT_WS_SEND_BPG(...args)}
        onCalibFinished={(finalReport) => {
    log.debug("[final-report]", finalReport)        }} 
        onExtraCtrlUpdate={extraCtrls=>{

          let extraCtrlUI=[];
          if(extraCtrls.browseNewFileToLoad!==undefined)
          {
            extraCtrlUI.push({
              key:"save_calibration",
              icon:<FolderOpenOutlined />,
              text:"開啟",
              onClick:_=>extraCtrls.browseNewFileToLoad()
              // subMenu:[]
            })
          }

          if(extraCtrls.loadPrev!==undefined)
          {
            extraCtrlUI.push({
              key:"loadPrev",
              icon:<CaretUpOutlined />,
              text:"loadPrev",
              onClick:_=>extraCtrls.loadPrev()
              // subMenu:[]
            })
          }
          if(extraCtrls.loadNext!==undefined)
          {
            extraCtrlUI.push({
              key:"loadNext",
              icon:<CaretDownOutlined />,
              text:"loadNext",
              onClick:_=>extraCtrls.loadNext()
              // subMenu:[]
            })
          }

          
          if(extraCtrls.imageSave!==undefined)
          {
            extraCtrlUI.push({
              key:"imageSave",
              icon:<SaveOutlined />,
              text:"imageSave",
              onClick:_=>extraCtrls.imageSave()
              // subMenu:[]
            })
          }


          setExtraSideUI(extraCtrlUI);
        }}/>
        
        );
      
      
        siderUI_info={
          title:UI_state.name,
          
          menu:[
            {
              icon:<ArrowLeftOutlined />,
              text:DICT._["<"],
              onClick:_=>setUI_state(s_statesTable.RootSelect)
              // subMenu:[]
            },
            ...extraSideUI
          ],
        }
      break;  
    case  s_statesTable.InstInsp:
      UI.push(<InstInspUI_rdx
        BPG_Channel={(...args) => ACT_WS_SEND_BPG( ...args)}

        onExtraCtrlUpdate={extraCtrls=>{

          let extraCtrlUI=[];
          // if(extraCtrls.takeNewImage!==undefined)
          // {
          //   //
          //   extraCtrlUI.push(
          //     <div className="antd-icon-sizing" key={"icon_s"} style={{height:30,color:"#FFF"}} onClick={_=>extraCtrls.takeNewImage()}>
          //       dddd
          //       {/* <MinusOutlined onClick={_=>extraCtrls.takeNewImage()}/> */}
          //     </div>
          //   );
          // }
          if(extraCtrls.clearMeasureSet!==undefined)
          {
            extraCtrlUI.push({
              icon:<DeleteOutlined />,
              text:"Remove",
              onClick:_=>extraCtrls.removeOneMeasureSet()
              // subMenu:[]
            })
          }
          
          if(extraCtrls.togglePointPairMMPPAdjust!==undefined)
          {
            extraCtrlUI.push({
              icon:<SettingOutlined />,
              text:"校正設定",
              onClick:_=>extraCtrls.togglePointPairMMPPAdjust()
              
              // subMenu:[]
            })
          }
          if(extraCtrls.saveCameraParam!==undefined)
          {
            extraCtrlUI.push({
              icon:<SaveOutlined />,
              text:dictLookUp("save_calibration", DICT),
              onClick:_=>extraCtrls.saveCameraParam()
            });
          }
          
          
          
          // if(extraCtrls.removeOneMeasureSet!==undefined)
          // {
          //   extraCtrlUI.push({
          //     icon:<MinusOutlined />,
          //     text:"",
          //     onClick:_=>extraCtrls.removeOneMeasureSet()
          //     // subMenu:[]
          //   })
          // }

          setExtraSideUI(extraCtrlUI);
        }}

         />);

      
      siderUI_info={
        title:UI_state.name,
        menu:[
          {
            icon:<ArrowLeftOutlined />,
            text:DICT._["<"],
            onClick:_=>{
              
              ACT_WS_SEND_BPG("RC", 0, {
                target: "camera_setting_refresh"
              });

              // The default_camera_param.json load that used to follow is gone --
              // see the note in comm/BPG_WS.js. Nothing consumed the reply, and
              // the file's mmpb2b disagreed with lens_calib.json. The camera
              // setting refresh above is the part of this action that does work.
              setUI_state(s_statesTable.RootSelect)
            }
            // subMenu:[]
          },
          ...extraSideUI
        ],
      }
      break;
    case  s_statesTable.Setting:
      // console.log(RDX_machine_custom_setting)
      UI=<Setui_UI machCusSetting={RDX_machine_custom_setting} 
        
        onExtraCtrlUpdate={extraCtrls=>{

          let path =GetObjElement(RDX_machine_custom_setting,["__priv","path"]);

          if(path===undefined)
          {
            path="data/machine_setting.json";
          }
          


          // Read-merge-write, not write-the-cache.
          //
          // This wrote `setting` -- the panel's copy of the WHOLE file, seeded
          // when the panel last re-seeded from the store -- straight over the
          // file. Anything that reached the disk without going through the
          // redux store was therefore reverted by the next save here, silently
          // and in full. The store only learns of a change on two paths: the
          // LD at connect, and StationRegionPanel calling
          // machine_custom_setting_Update after it saves. A field edited by
          // hand, or written by any future panel that does not dispatch, is
          // invisible to this one and gets erased.
          //
          // StationRegionPanel already solved this for its two keys (see
          // InspectionUI.js) after browser B was observed reverting browser A's
          // InspectionMode. Same fix, generalised: this panel does not own a
          // fixed key set, so the keys it may write are the ones the operator
          // actually CHANGED -- the difference between the seed it opened with
          // and what is in the form now. Everything else on disk is left alone.
          function saveSetting(saveToFilePath, setting, origin)
          {
            const enc = new TextEncoder();
            const clean = (o) => {
              const c = { ...(o || {}) };
              Object.keys(c).forEach((k) => { if (k.startsWith("_")) delete c[k]; });
              return c;
            };
            const cur = clean(setting);
            const org = clean(origin);
            const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

            // What this panel is asking to change, and nothing else.
            const touched = Object.keys(cur).filter((k) => !same(cur[k], org[k]));
            const removed = Object.keys(org).filter((k) => !(k in cur));

            // No origin means the panel could not tell us its baseline, so the
            // change set is unknowable. Refuse rather than fall back to writing
            // the whole cache -- that fallback IS the bug.
            if (origin === undefined) {
              log.error("[machine-setting] no baseline for the diff -- NOT saving");
              message.error("無法判斷變更範圍，設定未儲存");
              return;
            }
            if (touched.length === 0 && removed.length === 0) {
              log.info("[machine-setting] nothing changed -- not writing");
              return;
            }

            const writeMerged = (base) => {
              const merged = clean(base);
              touched.forEach((k) => { merged[k] = cur[k]; });
              removed.forEach((k) => { delete merged[k]; });
              log.info("[machine-setting] saving keys:", touched.join(",") || "(none)",
                       removed.length ? " removing: " + removed.join(",") : "");
              ACT_File_Save(saveToFilePath,
                enc.encode(JSON.stringify(merged, null, 2)),
                {
                  resolve: () => {
                    // Publish what is now ON DISK, not the panel's copy: the
                    // merge may have brought in keys this panel never had, and
                    // the store is what everything else re-seeds from.
                    ACT_machine_custom_setting_Update({ ...setting, ...merged });
                    // Tell the RUNNING core the same thing, from here.
                    //
                    // This used to fire unconditionally in saveSettingPopUp,
                    // right after calling saveSetting -- so once saveSetting
                    // learned to refuse, a refused write still pushed the
                    // panel's stale cache into the live core. The operator
                    // read "設定未儲存" while the machine had already taken the
                    // values, with nothing on disk to show for it. Sending it
                    // from the write's own success path makes the file and the
                    // running core the same decision, carrying the same
                    // merged document.
                    ACT_WS_SEND_BPG("ST", 0, { MachineSetting: merged });
                  },
                  reject: (e) => {
                    log.error("[machine-setting] save failed", e);
                    message.error("設定儲存失敗");
                  }
                });
            };

            // Re-read first. On failure, refuse -- a save that silently reverts
            // someone else's work is worse than one the operator can retry.
            ACT_WS_SEND_BPG("LD", 0, { filename: saveToFilePath }, undefined, {
              resolve: (pkts) => {
                const fl = (pkts || []).find((p) => p.type == "FL");
                const base = fl && fl.data;
                if (base && typeof base === "object" && !Array.isArray(base)) writeMerged(base);
                else {
                  log.error("[machine-setting] could not re-read " + saveToFilePath + " -- NOT saving");
                  message.error("無法讀取 " + saveToFilePath + "，設定未儲存 — 請重試");
                }
              },
              reject: () => {
                log.error("[machine-setting] re-read failed -- NOT saving");
                message.error("無法讀取設定檔，設定未儲存 — 請重試");
              }
            });
          }
          function saveSettingPopUp(saveToFilePath,setting,onOK,onCancel,origin)
          {
            setPopUpInfo({
              title:"CHECK",
              onOK:()=>{
                // The ST push moved INTO saveSetting's write-success path. Here
                // it fired unconditionally with the panel's unmerged cache, so
                // a refused write still handed the running core the stale
                // document -- "設定未儲存" on screen, values already in force on
                // the machine, and nothing on disk to reconcile against.
                saveSetting(saveToFilePath,setting,origin);

                setPopUpInfo();
                if(onOK!==undefined)onOK();
              },
              onCancel:()=>{
                setPopUpInfo();
                if(onCancel!==undefined)onOK();
              },
              content:"確定存檔？",

              okText:"OK",
              cancelText:"NO"
            });
          }
          let extraCtrlUI=[];


          
          if(extraCtrls.isUpdated!==undefined)
          {
            extraCtrlUI.push({
              icon:<ArrowLeftOutlined />,
              text:DICT._["<"],
              onClick:_=>{
                if(extraCtrls.isUpdated()==true)
                {
                  let setting = extraCtrls.fetchSetting();
                  let origin  = extraCtrls.fetchOrigin && extraCtrls.fetchOrigin();
                  saveSettingPopUp(path,setting,
                    ()=>{
                    setUI_state(s_statesTable.RootSelect)
                  },()=>{
                    setUI_state(s_statesTable.RootSelect)
                  },origin);
                  
                }
                else
                {
                  
                  setUI_state(s_statesTable.RootSelect);
                }
              }
            });
          }

          if(extraCtrls.fetchSetting!==undefined)
          {


            extraCtrlUI.push({
              icon:<SaveOutlined />,
              text:"SAVE",
              onClick:_=>{
                let setting = extraCtrls.fetchSetting();
                let origin  = extraCtrls.fetchOrigin && extraCtrls.fetchOrigin();
                saveSettingPopUp(path,setting,undefined,undefined,origin);

              }
              // subMenu:[]
            })
          }
          setExtraSideUI(extraCtrlUI);
        }}/>;

      siderUI_info={
        title:UI_state.name,
        menu:extraSideUI,
      }
      break;
  }

  let siderUI=[];
  if(siderUI_info!==undefined)
  {
    if(siderUI_info.title!==undefined)
    {
      siderUI.push(<div key="title" 
        style={{height:"auto",background: "#FFF",margin: "5px",
        writingMode: "vertical-rl",textOrientation: "mixed",
        alignItems: "center",display: "flex"}}>
        <Title level={2} style={{margin: "15px"}}  className="theme_color_2" onClick={()=>setSiderCollapse(!siderCollapse)}>{siderUI_info.title}</Title>
      </div>)
    }


    if(siderUI_info.menu!==undefined)
    {
      siderUI.push(<Menu mode="inline" defaultSelectedKeys={['1']}    selectable={false} key="MENU.."
      style={{
        boxShadow: "inset -1px 0 9px -2px rgba(0,0,0,0.4)",
        border: "0px"}}>
        {siderUI_info.menu.map((item,idx)=> <Menu.Item key={(item.key===undefined)?("idx:"+idx):item.key} icon={item.icon} onClick={item.onClick}>{item.text}</Menu.Item>)}
        </Menu>)
      // siderUI_info.menu.forEach((menu,idx)=>{
      //   siderUI.push(menu)
      // })
    }
    
  }

  return <Layout className="HXF">
    {siderUI==null?null:
    <Sider collapsed={siderCollapse} className="theme_background_2">
      {siderUI}
    </Sider>}
    

    <Layout>
      <Content>
        {UI}
      </Content>
    </Layout>



    <Modal
      closable={false}
      visible={popUpInfo !== undefined}
      centered
      title={popUpInfo!=undefined?popUpInfo.title:null}
      onOk={() => {
        popUpInfo.onOK();
      }}
      onCancel={() => {
        popUpInfo.onCancel();
      }}
      okText={popUpInfo!=undefined?popUpInfo.okText:undefined}
      cancelText={popUpInfo!=undefined?popUpInfo.cancelText:undefined}
    >
      {popUpInfo === undefined ?
        null : popUpInfo.content}
    </Modal>
  </Layout>;
}

class APPMain extends React.Component {


  constructor(props) {
    super(props);
    this.state = {
      fileSelectedCallBack: undefined,
      fileSelectFilter: undefined,
      fileStaticList: undefined,

      popUpUIInfo: undefined,
      menuSelect: "Overview",
      additionalUI: [],
      menuCollapsed: true,
    }
  }


  componentDidMount() {
    let defModelPath = this.props.defModelPath;
    if(defModelPath===undefined)
    {
      let recent = getLocalStorage_RecentFiles();
      
      if(recent.length==0)
      {
        this.props.ACT_Def_Model_Path_Update("data/DEFAULT");
      }
      else
      {
        
        let fileNamePath =recent[0].path.replace('.' + DEF_EXTENSION, "");
        this.props.ACT_Def_Model_Path_Update(fileNamePath);
      }
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    
    return true;
  }

  calibInfoUpdate(newAddInfo) {
    this.setState({ calibCalcInfo: { ...this.state.calibCalcInfo, ...newAddInfo } });
  }
  render() {
    let UI = [];
    if (this.props.c_state == null) return null;




    let stateObj = xstate_GetCurrentMainState(this.props.c_state);
    if (stateObj.state === UIAct.UI_SM_STATES.MAIN) 
    {
      UI=<MainUI/>;
    }
    else if (stateObj.state === UIAct.UI_SM_STATES.DEFCONF_MODE) {
      UI = <APP_DEFCONF_MODE_rdx />;
    }
    else if (stateObj.state === UIAct.UI_SM_STATES.INSP_MODE) {
      UI = <APP_INSP_MODE_rdx />;

    }

    return (
      <>
      {/* // <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}> */}
        {UI}
      {/* // </BASE_COM.CardFrameWarp> */}
      </>

    );
  }
}
const mapDispatchToProps_APPMain = (dispatch, ownProps) => {
  return {
    EV_UI_Edit_Mode: (arg) => { dispatch(UIAct.EV_UI_Edit_Mode()) },
    EV_UI_Insp_Mode: () => { dispatch(UIAct.EV_UI_Insp_Mode()) },
    
    ACT_WS_SEND_BPG: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_Def_Model_Path_Update: (path) => dispatch(UIAct.Def_Model_Path_Update(path)),
  }
}
const mapStateToProps_APPMain = (state) => {
  return {
    defFileTag: state.UIData.edit_info.DefFileTag,
    inspOptionalTag: state.UIData.edit_info.inspOptionalTag,
    defModelPath: state.UIData.edit_info.defModelPath,
    c_state: state.UIData.c_state,
    camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
    isp_db: state.UIData.edit_info._obj,
    CORE_ID: state.ConnInfo.CORE_ID,
    version_map_info: state.UIData.version_map_info,
    WebUI_info: state.UIData.WebUI_info,

    statSetting: state.UIData.edit_info.statSetting,
    machine_custom_setting: state.UIData.machine_custom_setting,
  }
}

let APPMain_rdx = connect(mapStateToProps_APPMain, mapDispatchToProps_APPMain)(APPMain);
export default APPMain_rdx;

