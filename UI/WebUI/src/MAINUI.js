
import 'antd/dist/antd.less';
import { connect } from 'react-redux'
import React, { useState, useEffect,useRef } from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx,TagDisplay_rdx,isTagFulFillRequrement, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';

import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import QRCode from 'qrcode'
import dclone from 'clone';
import { CusDisp_DB } from 'UTIL/DB_Query';
//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import APP_INSP_MODE_rdx from './InspectionUI';
import APP_ANALYSIS_MODE_rdx from './AnalysisUI';
import BackLightCalibUI_rdx from './BackLightCalibUI';
import InstInspUI_rdx from './InstInspUI';

import RepDisplayUI_rdx from './RepDisplayUI';
import InputNumber from 'antd/lib/input-number';
import { xstate_GetCurrentMainState, GetObjElement, Calibration_MMPP_offset } from 'UTIL/MISC_Util';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import ReactResizeDetector from 'react-resize-detector';

import { BPG_FileBrowser, BPG_FileSavingBrowser } from './component/baseComponent.jsx';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';

import { default as AntButton } from 'antd/lib/button';

import PageHeader from 'antd/lib/page-header';
import Typography from 'antd/lib/typography';
import Collapse from 'antd/lib/collapse';
import Divider from 'antd/lib/divider';
import Card from 'antd/lib/card';
import Carousel from 'antd/lib/carousel';
import Popover from 'antd/lib/popover';
import Affix from 'antd/lib/Affix';
import  Table  from 'antd/lib/table';
import Switch from 'antd/lib/switch';
import Row from 'antd/lib/Row';
import Col from 'antd/lib/Col';
import Steps from 'antd/lib/Steps';

import { useSelector,useDispatch } from 'react-redux';
const { Meta } = Card;
const { Step } = Steps;

import { 
  RollbackOutlined,
  DeleteOutlined,
  MinusOutlined,
  SelectOutlined,
  MonitorOutlined,
  FolderOpenOutlined,
  InfoCircleOutlined,
  EditOutlined,
  TableOutlined,
  ThunderboltOutlined,
  CloudDownloadOutlined,
  LeftOutlined,
  RightOutlined,
  LinkOutlined,
  DisconnectOutlined,
  ScanOutlined,
  SettingOutlined,
  DatabaseOutlined,
  QrcodeOutlined,
  FundOutlined,
  CaretRightOutlined,
  CloudServerOutlined,
  CloseCircleTwoTone,
  LoadingOutlined } from '@ant-design/icons';

import Menu from 'antd/lib/menu';
import Button from 'antd/lib/button';
import Layout from 'antd/lib/layout';
import Modal from 'antd/lib/modal';
import Input from 'antd/lib/input';
import Tag from 'antd/lib/tag';
import Dropdown from 'antd/lib/dropdown';
const { Header, Content, Footer, Sider } = Layout;
const SubMenu = Menu.SubMenu;
const { Paragraph, Title } = Typography;
const Panel = Collapse.Panel;

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

    console.log("CanvasComponent render", this.props.c_state);
    //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];

    console.log(this.props.edit_info.inherentShapeList);
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
        console.log('success!');
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

function checkFileInfo(fileInfo)
{

  if (!(typeof fileInfo.name === 'string' || fileInfo.name instanceof String))
  {
    return false;
  }
  if (!(typeof fileInfo.path === 'string' || fileInfo.path instanceof String))
  {
    return false;
  }
  if (!(typeof fileInfo.type === 'string' || fileInfo.type instanceof String))
  {
    return false;
  }

  if(typeof fileInfo.ctime_ms !== 'number'){
    return false;
  }
  if(typeof fileInfo.mtime_ms !== 'number'){
    return false;
  }
  if(typeof fileInfo.size_bytes !== 'number'){
    return false;
  }
  return true;
}

function getLocalStorage_RecentFiles()
{

  if(localStorage===undefined)return [];
  let LocalS_RecentDefFiles = localStorage.getItem("RecentDefFiles");
  try {
    LocalS_RecentDefFiles = JSON.parse(LocalS_RecentDefFiles);
    if (!(LocalS_RecentDefFiles instanceof Array)) {
      LocalS_RecentDefFiles = [];
    }

      
    LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter((fileInfo) =>checkFileInfo(fileInfo));
  } catch (e) {
    LocalS_RecentDefFiles = [];
  }
  return LocalS_RecentDefFiles;
}

function appendLocalStorage_RecentFiles(fileInfo)
{
  if(checkFileInfo(fileInfo)==false)
  {
    return false;
  }
  let LocalS_RecentDefFiles = getLocalStorage_RecentFiles();

  //console.log(LocalS_RecentDefFiles);
  LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter((ls_fileInfo) =>
    (ls_fileInfo.name != fileInfo.name || ls_fileInfo.path != fileInfo.path));

  LocalS_RecentDefFiles.unshift(fileInfo);
  LocalS_RecentDefFiles = LocalS_RecentDefFiles.slice(0, 100);
  localStorage.setItem("RecentDefFiles", JSON.stringify(LocalS_RecentDefFiles));
  //console.log(localStorage.getItem("RecentDefFiles"));

  return true;
}


const InspectionDataPrepare = ({onPrepareOK}) => {
  const caruselRef = useRef(undefined);

  const DICT = useSelector(state => state.UIData.DICT);
  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);

  const Info_decorator = useSelector(state => state.UIData.edit_info.__decorator);
  const System_Connection_Status = useSelector(state => state.UIData.System_Connection_Status);
  
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const defModelPath = useSelector(state => state.UIData.edit_info.defModelPath);
  const DefFileName = useSelector(state => state.UIData.edit_info.DefFileName);
  const DefFileHash = useSelector(state => state.UIData.edit_info.DefFileHash);
  
  const InspectionMonitor_URL= useSelector(state => state.UIData.InspectionMonitor_URL);
  const dispatch = useDispatch();
  const ACT_Def_Model_Path_Update= (path) => dispatch(UIAct.Def_Model_Path_Update(path));
  const ACT_WS_SEND= (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs));
  const ACT_InspOptionalTag_Update= (newTags) => dispatch(DefConfAct.InspOptionalTag_Update(newTags));
  
  const [InfoPopUp,setInfoPopUp]=useState(undefined);
  const [ErrorInfo,setErrorInfo]=useState(undefined);

  
  const [showInspectionNote,setShowInspectionNote]=useState(false);

  
  const _mus = useSelector(state => state.UIData.machine_custom_setting);
  const [fileSelectorInfo,setFileSelectorInfo]=useState(undefined);
  
  const [stepIdx,setStepIdx]=useState(0);
  const [isVertical,setIsVertical]=useState(false);
  let DefFileFolder=undefined;

  useEffect(()=>{
    let isSystemReadyForInsp=GetObjElement(System_Connection_Status,["camera"])==true;
    if(!isSystemReadyForInsp)
    {
      setErrorInfo({
        content:<div>
          <div className="antd-icon-sizing" style={{height:"50px"}}>
            <LoadingOutlined className="veleX"/>
          </div>
          <Title level={2} style={{textAlign:"center"}} >{
            DICT._.camera_reconnection_caption
          }</Title>
        </div>
      });
    }
    else
    {
      if(ErrorInfo!==undefined)
      {
        setErrorInfo(undefined);
      }
    }
  },[System_Connection_Status])

  
  useEffect(()=>{
    ACT_WS_SEND(WS_ID, "LD", 0, { deffile: defModelPath + '.' + DEF_EXTENSION, imgsrc: defModelPath });

  },[])



  function SignatureTargetMatching(fileInfoList,onMatchingResult)
  {
    if(fileInfoList==undefined||fileInfoList.length==0)
    {
      onMatchingResult({files:[]});
      return;
    }
    console.log(fileInfoList);
    ACT_WS_SEND(WS_ID, "ST", 0,
    { CameraSetting: { ROI:[0,0,99999,99999] } })
    ACT_WS_SEND(WS_ID, "EX", 0, {},
      undefined, { 
      resolve:(pkts)=>{
        let signature = GetObjElement(pkts,[0,"data","reports",0,"signature"]);
        
        ACT_WS_SEND(WS_ID, "SC", 0, {
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

      }, reject:(pkts,__)=>{
        
      } });
  }

  let InspectionMonitor_URL_w_info=_mus.inspection_monitor_url;
  if (isString(DefFileHash) && DefFileHash.length > 5) {
    InspectionMonitor_URL_w_info = InspectionMonitor_URL_w_info +
      "?v=" + 0 + "&name=" + DefFileName + "&hash=" + DefFileHash;
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
      onCancel: undefined,
      content: <CustomDisplaySelectUI onSelect={(cusDispInfo) => {

        let tarDef = cusDispInfo.targetDeffiles[0];
        let filePath = tarDef.path;
        if (filePath === undefined) return;
        filePath = filePath.replace("." + DEF_EXTENSION, "").replaceAll("\\" , "/");
        setInfoPopUp(undefined);
        ACT_WS_SEND(WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath },undefined,{
          resolve:(stacked_pkts,action_channal)=>{
            let SS=stacked_pkts.find(pkt=>pkt.type=="SS");
            console.log(stacked_pkts,SS);
            if(SS===undefined)return;
            let DF=stacked_pkts.find(pkt=>pkt.type=="DF");
            let IM=stacked_pkts.find(pkt=>pkt.type=="IM");
            console.log(SS,DF,IM);
            if(SS.data.ACK==true && DF!==undefined && IM!==undefined)
            {
              let setTags = [];
              try {
                setTags = tarDef.tags.split(",");

              }
              catch (e) {
                setTags = [];
              }
              
              ACT_Def_Model_Path_Update(filePath);
              action_channal(stacked_pkts);
              
              // console.log(setTags);
              ACT_InspOptionalTag_Update(setTags)
            }
            else
            {
              
              let errPopUpUIInfo = {
                title: "錯誤",
                onOK: undefined,
                onCancel: undefined,
              content:<div style={{width:"100%",height:"200px"}}><Title className="veleXY">
                <CloseCircleTwoTone twoToneColor="#FF0000"/>找不到檔案:{filePath}
                </Title></div>
              }
              setTimeout(()=>setInfoPopUp(errPopUpUIInfo),100);
            }
          }, 
          reject:()=>{

          }
        });

      }} />
    }
    setInfoPopUp(popUpUIInfo);
  }

  let UI_Stack=[];

  let isSystemReadyForInsp=GetObjElement(System_Connection_Status,["camera"])==true;
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

    let isFileOK=(DefFileHash!==undefined&&isSystemReadyForInsp) ;
    
    isOK=isFileOK && isTagFulFillRequrement(inspOptionalTag,tagGroupsPreset);
    let twoPanelClass1="s height12 width4";
    let twoPanelClass2="s height12 width8";
    if(isVertical)
    {
      twoPanelClass1="s height5 width12"
      twoPanelClass2="s height7 width12"
    }

    function matchingAUTO_UI(fileInfoList)
    {

      let errPopUpUIInfo = {
        title: "MATCH ing...",
        onOK: undefined,
        onCancel: undefined,
        content:<div style={{width:"100%",height:"400px"}} className="scroll">
          
          <div className="antd-icon-sizing" style={{height:"50px"}}>
            <LoadingOutlined className="veleX"/>
          </div>
          <Title level={2} style={{textAlign:"center"}} >
            {DICT.mainui.FUNC_auto_recognition_running}
          </Title>
        </div>
      }
      setInfoPopUp(errPopUpUIInfo)


      SignatureTargetMatching(fileInfoList,(matchingList)=>{

        console.log(matchingList);


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
                  ACT_WS_SEND(WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath });

                  setFileSelectorInfo(undefined);

                }})} 
              dataSource={dataSource} 
              columns={columns} 
              pagination={false}/>

          </div>
        }
        setInfoPopUp(errPopUpUIInfo)
      })

    }


    UI_Stack.push(
      <div key="UI_Step0" className="s width12 height12 overlayCon" fixedFrame={true}  style={{background: "rgb(250,250,250)"}}>
        
        <div className={twoPanelClass1} style={{padding: "10px"}}>
          
          <TagDisplay_rdx closable/>
          <TagOptions_rdx className="s width12 HXA" size="middle" tagGroups={new_tagGroupsPreset}/>
        </div>
        <CanvasComponent_rdx className={twoPanelClass2} showInspectionNote={showInspectionNote} />
        
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
            onClick={() =>matchingAUTO_UI(getLocalStorage_RecentFiles())}/>
          
          <Button className={"antd-icon-sizing "+(isFileOK?"HW50":"HW100")} size="large"
            style={{"pointerEvents": "auto"}} icon={<FolderOpenOutlined/> } type="text"
            onClick={() => {
            let fileSelectedCallBack =
              (filePath, fileInfo) => {


                filePath = filePath.replace("." + DEF_EXTENSION, "");
                setFileSelectorInfo(undefined);
                ACT_WS_SEND(WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath },
                  undefined, { resolve:(pkts,action_channal)=>{
                    let SS=pkts.find(pkt=>pkt.type=="SS");
                    console.log(pkts);
                    if(SS==undefined || SS.data.ACK==false)
                    {
                      
                      let errPopUpUIInfo = {
                        title: DICT._.ERROR,
                        onOK: undefined,
                        onCancel: undefined,
                        content:<div style={{width:"100%",height:"200px"}}><Title className="veleXY">
                          <CloseCircleTwoTone twoToneColor="#FF0000"/>{DICT.mainui.FILE_NOT_FOUND}:{filePath}
                          </Title></div>
                      }
                      setInfoPopUp(errPopUpUIInfo)
                      return;
                    }
                      
                    appendLocalStorage_RecentFiles(fileInfo);
                    ACT_Def_Model_Path_Update(filePath);
                    action_channal(pkts);
                  }, reject:(e)=>{

                  } });
                }


            let fileGroups = [
              { name: "history", list: getLocalStorage_RecentFiles() },
              
            ];
            let fileSelectFilter = (fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes("." + DEF_EXTENSION);

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
                  console.log(files);
                  if(files!==undefined && files.length>0)
                    matchingAUTO_UI(files);
                }},
              ]
            });
          }}/>


          
          <Button className={"antd-icon-sizing "+(isFileOK?"HW50":"HW100")} size="large"
            style={{"pointerEvents": "auto"}} icon={<CloudServerOutlined/> } type="text"
            onClick={loadMachineSettingPopUp}
            ></Button>
          <Popover 
            style={{"pointerEvents": "auto"}}
            content={
            !isOK?null:
            <QR_Canvas className="veleX" style={{height:"100%"}}
                    onClick={() => window.open(InspectionMonitor_URL_w_info)} QR_Content={InspectionMonitor_URL_w_info} />} 
            trigger={!isOK?"??":"hover"}>
            <Button type="text" className="antd-icon-sizing HW50" size="large" disabled={!isOK} icon={<QrcodeOutlined/> }/>
          </Popover>

          
          
          <Button className={"antd-icon-sizing  "+(isOK?"HW100":"HW50")} size="large"
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
        BPG_Channel={(...args) => ACT_WS_SEND(WS_ID, ...args)}
        onFileSelected={(filePath, fileInfo) => {
          setFileSelectorInfo(undefined);
          fileSelectorInfo.callBack(filePath, fileInfo);
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
        ErrorInfo.onOK();
      }}
      onCancel={() => {
        ErrorInfo.onCancel();
      }}
    >
      {ErrorInfo === undefined ?
        null : ErrorInfo.content}
    </Modal>
  </>

  );
};



const Setui_UI=()=>{

  const dispatch = useDispatch();
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const ACT_WS_SEND= (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs));
  const ACT_Report_Save = (id, fileName, content) => {
    let act = UIAct.EV_WS_SEND(id, "SV", 0,
      { filename: fileName },
      content
    )
    dispatch(act);
  }
  const uInspData = useSelector(state => state.Peripheral.uInsp);

  const [calibCalcInfo, setCalibCalcInfo] = useState({
    curMea1: 0.8,
    calibMea1: 0.82,
    curMea2: 0.4,
    calibMea2: 0.44,
  });



  console.log(calibCalcInfo);
  let newCalibData = Calibration_MMPP_offset(
    calibCalcInfo.curMea1,
    calibCalcInfo.calibMea1,
    calibCalcInfo.curMea2,
    calibCalcInfo.calibMea2,
    mmpp, 0);

  const isp_db = useSelector(state => state.UIData.edit_info._obj);

  let mmpp = undefined;
  if (isp_db !== undefined) {
    let camParam = isp_db.cameraParam;
    mmpp = camParam.mmpb2b / camParam.ppb2b;
  }





  return <div style={{ padding: 24, background: '#fff', minHeight: 360 }}>

    <Divider orientation="left">MISC</Divider>
    <AntButton key="Reconnect CAM"
      onClick={() => {
        ACT_WS_SEND(WS_ID, "RC", 0, {
          target: "camera_ez_reconnect"
        });
      }}>Reconnect CAM</AntButton>
      &ensp;


    <Divider orientation="left">µInsp</Divider>
    <Button.Group>

      <Button type="primary" key="Connect uInsp" disabled={uInspData.connected}
        icon={<LinkOutlined />}
        onClick={() => {
          new Promise((resolve, reject) => {
            ACT_WS_SEND(WS_ID, "PD", 0,
              { ip: "192.168.2.2", port: 5213 },
              undefined, { resolve, reject });
            //setTimeout(()=>reject("Timeout"),1000)
          })
            .then((data) => {
              console.log(data);
            })
            .catch((err) => {
              console.log(err);
            })
        }}>(re)Connect</Button>
      <Button type="danger" key="Disconnect uInsp" disabled={!uInspData.connected}
        icon={<DisconnectOutlined />}
        onClick={() => {
          new Promise((resolve, reject) => {
            ACT_WS_SEND(WS_ID, "PD", 0,
              {},
              undefined, { resolve, reject });
            //setTimeout(()=>reject("Timeout"),1000)
          })
            .then((data) => {
              console.log(data);
            })
            .catch((err) => {
              console.log(err);
            })
        }}>Disconnect</Button>

    </Button.Group>

      &ensp;
      <Button key="ping uInsp" disabled={!uInspData.connected}
      onClick={() => {
        new Promise((resolve, reject) => {
          ACT_WS_SEND(WS_ID, "PD", 0,
            { msg: { type: "PING", id: 443 } },
            undefined, { resolve, reject });
        })
          .then((data) => {
            console.log(data);
          })
          .catch((err) => {
            console.log(err);
          })
      }}>
      PING:{uInspData.alive}
    </Button>

      &ensp;
      <Button key="get_setup" disabled={!uInspData.connected}
      onClick={() => {
        new Promise((resolve, reject) => {
          ACT_WS_SEND(WS_ID, "PD", 0,
            { msg: { type: "get_setup", id: 4423 } },
            undefined, { resolve, reject });
        })
          .then((data) => {
            console.log(data);
          })
          .catch((err) => {
            console.log(err);
          })
      }}>
      get_setup
      </Button>

      &ensp;
      <Button key="set_setup" disabled={uInspData.machineInfo === undefined}
      onClick={() => {
        let machInfo = dclone(uInspData.machineInfo);
        //machInfo.state_pulseOffset[0]+=1;
        new Promise((resolve, reject) => {
          ACT_WS_SEND(WS_ID, "PD", 0,
            { msg: { ...machInfo, type: "set_setup", id: 356 } },
            undefined, { resolve, reject });
        })
          .then((data) => {
            console.log(data);
          })
          .catch((err) => {
            console.log(err);
          })
      }}>
      set_setup
      </Button>



    <Button key="save_setup" disabled={uInspData.machineInfo === undefined}
      onClick={() => {
        var enc = new TextEncoder();
        ACT_Report_Save(WS_ID, "data/uInspSetting.json",
          enc.encode(JSON.stringify(uInspData.machineInfo, null, 4)));
      }}>
      save_setup
      </Button>




    <Button key="file_set_setup" disabled={uInspData.machineInfo === undefined}
      onClick={() => {
        new Promise((resolve, reject) => {

          ACT_WS_SEND(WS_ID, "LD", 0,
            { filename: "data/uInspSetting.json" },
            undefined, { resolve, reject }
          );

          setTimeout(() => reject("Timeout"), 5000)

        })
          .then((pkts) => {
            if (pkts[0].type != "FL") return;
            let machInfo = pkts[0].data;
            ACT_WS_SEND(WS_ID, "PD", 0,
              { msg: { ...machInfo, type: "set_setup", id: 356 } });
          })
          .catch((err) => {

          })


      }}>
      file_set_setup
      </Button>

    {
      //JSON.stringify(uInspData)
    }

    {/* <Divider orientation="left">SolveCalib</Divider>

      CurMea1: <InputNumber size="large" defaultValue={calibCalcInfo.curMea1} step={0.001}
      onChange={(val) => this.calibInfoUpdate({ curMea1: val })} />
      &ensp;&ensp;CalibMea1:<InputNumber size="large" defaultValue={calibCalcInfo.calibMea1} step={0.001}
      onChange={(val) => this.calibInfoUpdate({ calibMea1: val })} />
    <br />
      CurMea2:<InputNumber size="large" defaultValue={calibCalcInfo.curMea2} step={0.001}
      onChange={(val) => this.calibInfoUpdate({ curMea2: val })} />
      &ensp;&ensp;CalibMea2:<InputNumber size="large" defaultValue={calibCalcInfo.calibMea2} step={0.001}
      onChange={(val) => this.calibInfoUpdate({ calibMea2: val })} />
    <br />
      --------------------------------
      <br />
      MMPP:{newCalibData.mmpp}
    <br />
      OFFSET:{newCalibData.offset} */}

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
        name:DICT.mainui.MODE_SELECT_INST_INSP
      },
  
      BackLightCalib:{
        type:"BackLightCalib",
        name:DICT.mainui.MODE_SELECT_BACKLIGHT_CALIB,
      },
      RepDisplay:{
        type:"RepDisplay",
        name:DICT.mainui.MODE_SELECT_REP_DISPLAY,
      },
      PrecisionValidation:{
        type:"PrecisionValidation",
        name:DICT.mainui.MODE_SELECT_PRECISION_VALIDATION,
      },
      Setting:{
        type:"Setting",
        name:DICT.mainui.MODE_SELECT_SETTING,
      },
  
    }

  });
  let s_statesTable=_REF.current.statesTable;
  
  const dispatch = useDispatch();
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  
  const [siderCollapse,setSiderCollapse] = useState(true);
  
  const EV_UI_Edit_Mode=()=>dispatch(UIAct.EV_UI_Edit_Mode());
  const EV_UI_Insp_Mode= () =>dispatch(UIAct.EV_UI_Insp_Mode());
  const ACT_WS_SEND= (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs));
  const ACT_Report_Save = (id, fileName, content) => {
    let act = UIAct.EV_WS_SEND(id, "SV", 0,
      { filename: fileName },
      content
    )
    dispatch(act);
  }
  const uInspData = useSelector(state => state.Peripheral.uInsp);

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
            icon:<TableOutlined />,
            text:DICT.mainui.MODE_SELECT_BACKLIGHT_CALIB,
            onClick:_=>setUI_state(s_statesTable.BackLightCalib)
          },
          {
            icon:<ThunderboltOutlined />,
            text:DICT.mainui.MODE_SELECT_INST_INSP,
            onClick:_=>setUI_state(s_statesTable.InstInsp)
          }
           
        ],
      }
      UI.push(<InspectionDataPrepare  onPrepareOK={EV_UI_Insp_Mode}/>);
      
      break;
  
    case  s_statesTable.DeConf:
      //UI=<EV_UI_Edit_Mode  onPrepareOK={EV_UI_Insp_Mode}/>;
      break;
    case  s_statesTable.Inspection:
      
      break;
    case  s_statesTable.BackLightCalib:
      UI.push(<BackLightCalibUI_rdx
        BPG_Channel={(...args) => ACT_WS_SEND(WS_ID, ...args)}
        onExtraCtrlUpdate={extraCtrls=>{
          let extraCtrlUI=[];
          if(extraCtrls.currentReportExtract!==undefined)
          {
            extraCtrlUI.push({
              icon:<SelectOutlined />,
              text:DICT._.save_calibration,
              onClick:_=>{

                let report = extraCtrls.currentReportExtract();
                if(report===undefined)return;
                var enc = new TextEncoder();
                ACT_WS_SEND(WS_ID, "SV", 0,
                  { filename: "data/stageLightReport.json" },
                  enc.encode(JSON.stringify(report, null, 2)),
                  {
                    resolve:(stacked_pkts,action_channal)=>{
                      
                      // ACT_WS_SEND(WS_ID, "RC", 0, {
                      //   target: "camera_setting_refresh"
                      // });
  
                    }
                  })
              }
            });
          }
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
              
              ACT_WS_SEND(WS_ID, "RC", 0, {
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
        BPG_Channel={(...args) => ACT_WS_SEND(WS_ID, ...args)}
        onCalibFinished={(finalReport) => {
          console.log(">>>>>>>>>",finalReport)
        }} 
        onExtraCtrlUpdate={extraCtrls=>{

          let extraCtrlUI=[];
          if(extraCtrls.browseNewFileToLoad!==undefined)
          {
            extraCtrlUI.push({
              icon:<FolderOpenOutlined />,
              text:DICT._.save_calibration,
              onClick:_=>extraCtrls.browseNewFileToLoad()
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
        BPG_Channel={(...args) => ACT_WS_SEND(WS_ID, ...args)}

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
          
          if(extraCtrls.setDistanceType!==undefined)
          {
            extraCtrlUI.push({
              icon:<MinusOutlined />,
              text:"距離模式",
              onClick:_=>
              {
                if(_REF.current.distType===undefined)
                {
                  _REF.current.distType=0;
                }
                _REF.current.distType++;
                _REF.current.distType%=3;
                extraCtrls.setDistanceType(_REF.current.distType);
              }
              // subMenu:[]
            })
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
            onClick:_=>setUI_state(s_statesTable.RootSelect)
            // subMenu:[]
          },
          ...extraSideUI
        ],
      }
      break;
    case  s_statesTable.Setting:
      UI=<Setui_UI/>;
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
      siderUI.push(<Menu mode="inline" defaultSelectedKeys={['1']} 
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
      
      console.log(recent);
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
    console.log(newAddInfo);
    this.setState({ calibCalcInfo: { ...this.state.calibCalcInfo, ...newAddInfo } });
  }
  render() {
    let UI = [];
    if (this.props.c_state == null) return null;

    if (this.props.inspMode === undefined) {
      return <div>

        <Button
          size="large"
          key="<"
          onClick={() => {
            this.props.ACT_Insp_Mode_Update("FI");
          }}>全檢</Button>


        <Button
          size="large"
          key=">"
          onClick={() => {
            this.props.ACT_Insp_Mode_Update("CI");
          }}>檢測</Button>



        {/* <Button
          size="large"
          key="DISS"
          onClick={() => {
            this.props.ACT_WS_DISCONNECT(this.props.WS_ID);
          }}>DISS..</Button> */}
      </div>
    }




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
    else if (stateObj.state === UIAct.UI_SM_STATES.ANALYSIS_MODE) {
      UI = <APP_ANALYSIS_MODE_rdx />;
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
    EV_UI_Analysis_Mode: () => { dispatch(UIAct.EV_UI_Analysis_Mode()) },
    
    ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_WS_DISCONNECT: (id) => dispatch(UIAct.EV_WS_Disconnect(id)),
    ACT_Insp_Mode_Update: (mode) => dispatch(UIAct.EV_UI_Insp_Mode_Update(mode)),
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
    WS_CH: state.UIData.WS_CH,
    WS_ID: state.UIData.WS_ID,
    version_map_info: state.UIData.version_map_info,
    WebUI_info: state.UIData.WebUI_info,
    uInspData: state.Peripheral.uInsp,

    statSetting: state.UIData.edit_info.statSetting,
    inspMode: state.UIData.inspMode,
    machine_custom_setting: state.UIData.machine_custom_setting,
  }
}

let APPMain_rdx = connect(mapStateToProps_APPMain, mapDispatchToProps_APPMain)(APPMain);
export default APPMain_rdx;

