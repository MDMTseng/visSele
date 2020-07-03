
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

import Row from 'antd/lib/Row';
import Col from 'antd/lib/Col';
import Steps from 'antd/lib/Steps';
import EC_zh_TW from './languages/zh_TW';

import { useSelector,useDispatch } from 'react-redux';
let LANG_DICT=EC_zh_TW;
const { Meta } = Card;
const { Step } = Steps;

import { 
  FolderOpenOutlined,
  InfoCircleOutlined,
  EditOutlined,
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
  CloseCircleTwoTone } from '@ant-design/icons';

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
        <canvas ref="canvas" className="width12 HXF" />
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

function SingleDisplayUI({ displayInfo }) {
  const canvasRef = React.useRef(null)


  return <div>
    <Title level={2}>{displayInfo.name}</Title>
    Name:{displayInfo.name}
    <br />
    Cat:{displayInfo.cat}
    <br />
    <pre>
      {displayInfo.targetDeffiles.map((dfs, id) =>
        "Def[" + id + "]:" + JSON.stringify(displayInfo.targetDeffiles[id], null, 2)
      )}
    </pre>
  </div>
}

function SingleDisplayEditUI({ displayInfo, onUpdate, onCancel, BPG_Channel }) {
  const [displayEditInfo, setDisplayEditInfo] = useState(undefined);

  let fileSelectFilter = (fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes("." + DEF_EXTENSION);
  const [fileBrowserInfo, setFileBrowserInfo] = useState(undefined);

  useEffect(() => {
    setDisplayEditInfo(dclone(displayInfo));
  }, [displayInfo]);
  if (displayEditInfo === undefined) return null;
  return <div>
    <Input addonBefore="Name:" className="s width4" value={displayEditInfo.name}
      onChange={e => {
        let dcEI = dclone(displayEditInfo)
        dcEI.name = e.target.value;
        setDisplayEditInfo(dcEI)
      }} />
    <div className="s width6" />
    <Input addonBefore="Cat:" className="s width2" value={displayEditInfo.cat}
      onChange={e => {
        let dcEI = dclone(displayEditInfo)
        dcEI.cat = e.target.value;
        setDisplayEditInfo(dcEI)
      }} />
    {displayEditInfo.targetDeffiles.map((dfs, id) => {

      return [
        <Input addonBefore="Tag:" value={dfs.tags} key={id + "_input"}
          onChange={e => {

            let dcEI = dclone(displayEditInfo);
            let tarDef = dcEI.targetDeffiles[id];
            tarDef.tags = e.target.value

            setDisplayEditInfo(dcEI)
          }} />,
        "Def[" + id + "]:" + dfs.name + ":" + dfs.featureSet_sha1 + "  " + dfs.path,
        <Button key={id + "_Button"}
          onClick={() => {
            let fileS =
            {
              path: "data/",
              selected: (path, info) => {
                console.log(path, info);

                let PromArr = [
                  new Promise((resolve, reject) => {
                    BPG_Channel("LD", 0,
                      { filename: path },
                      undefined, { resolve, reject }
                    );
                    setTimeout(() => reject("Timeout"), 5000)
                  }),
                  new Promise((resolve, reject) => {
                    BPG_Channel("FB", 0, {//"FB" is for file browsing
                      path: "./",
                      depth: 0,
                    }, undefined, { resolve, reject });
                    setTimeout(() => reject("Timeout"), 5000)
                  })
                ];
                Promise.all(PromArr).
                  then((pkts) => {
                    console.log(pkts);
                    if (pkts[0][0].type != "FL") return;
                    if (pkts[1][0].type != "FS") return;
                    let dataFolderPath = pkts[1][0].data.path;
                    let machInfo = pkts[0][0].data;
                    //console.log(pkts[0].data);
                    let dcEI = dclone(displayEditInfo);
                    let tarDef = dcEI.targetDeffiles[id];
                    tarDef.hash = machInfo.featureSet_sha1;
                    tarDef.featureSet_sha1 = machInfo.featureSet_sha1;
                    tarDef.featureSet_sha1_pre = machInfo.featureSet_sha1_pre;
                    tarDef.featureSet_sha1_root = machInfo.featureSet_sha1_root;


                    tarDef.path = path.replace(dataFolderPath, "").replace(/^\//, "");
                    tarDef.name = machInfo.name
                    //console.log(dcEI);
                    setDisplayEditInfo(dcEI)

                  })
                  .catch((err) => {
                    console.log(err);
                  })

              },
              filter: fileSelectFilter
            }
            setFileBrowserInfo(fileS);
          }}
          style={{ width: '30%' }}
        >
          loadDefFile
        </Button>]
    }
    )}
    {fileBrowserInfo === undefined ? null :
      <BPG_FileBrowser key="BPG_FileBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileBrowserInfo.path}
        visible={true}
        BPG_Channel={BPG_Channel}
        onFileSelected={(filePath, fileInfo) => {
          fileBrowserInfo.selected(filePath, fileInfo);
          setFileBrowserInfo(undefined);
        }}
        onCancel={() => {
          setFileBrowserInfo(undefined);
        }}
        fileFilter={fileBrowserInfo.filter} />
    }



    <Button key="onUpdate_btn"
      onClick={() => onUpdate(displayEditInfo)}
      style={{ width: '30%' }}
    >
      OK
    </Button>


    <Button key="onCancel_btn"
      onClick={onCancel}
      style={{ width: '30%' }}
    >
      Cancel
    </Button>
  </div>
}

function CustomDisplayUI({ BPG_Channel, defaultFolderPath }) {
  const [displayInfo, setDisplayInfo] = useState(undefined);
  const [displayEle, setDisplayEle] = useState(undefined);

  useEffect(() => {
    setDisplayInfo(undefined);
    CusDisp_DB.read(".").then(data => {
      setDisplayInfo(data.prod);
    }).catch(e => {
      console.log(e);
    });
    return () => {
      console.log("1,didUpdate ret::");
    };
  }, []);

  // useEffect(() => {
  //   console.log("2,count->useEffect>>");
  // },[displayInfo]);

  let UI = [];
  if (displayInfo !== undefined) {
    console.log(displayInfo);
    UI = [];


    if (displayEle === undefined) {

      UI.push(displayInfo.map(info =>
        <div>
          <SingleDisplayUI displayInfo={info} />
          <Button
            onClick={() => {

              setDisplayEle(info);
            }}
            style={{ width: '30%' }}
          >
            EDIT
          </Button>
          <Button type="dashed"
            onClick={() => {
              setDisplayInfo(undefined);
              CusDisp_DB.delete(info._id).then(() => {

                CusDisp_DB.read(".").then(data => {
                  console.log(displayInfo);
                  setDisplayInfo(data.prod);
                }).catch(e => {
                  console.log(e);
                });
              });
            }}
            style={{ width: '60%' }}
          >
            Ｘ
          </Button>
        </div>
      ))

      UI.push(
        <Button type="dashed"
          onClick={() => {
            CusDisp_DB.create({ name: "新設定", targetDeffiles: [{}] }, undefined).then(() => {
              CusDisp_DB.read(".").then(data => {
                console.log(displayInfo);
                setDisplayInfo(data.prod);

                setDisplayEle(data.prod[data.prod.length - 1]);
              }).catch(e => {
                console.log(e);
              });
            });
          }}
          style={{ width: '60%' }}
        >
          Add field
        </Button>)

    }
    else {


      UI.push(
        <div>
          <SingleDisplayEditUI displayInfo={displayEle} BPG_Channel={BPG_Channel}
            onUpdate={(updatedEle) => {
              console.log(updatedEle);
              CusDisp_DB.update(updatedEle, displayEle._id).then(() => {
                CusDisp_DB.read(".").then(data => {
                  console.log(displayInfo);
                  setDisplayInfo(data.prod);
                  setDisplayEle();
                }).catch(e => {
                  console.log(e);
                });
              });
            }}

            onCancel={() => {
              setDisplayEle();
            }} />

        </div>
      )

      // UI.push(
      // <Button  
      //   onClick={() => {
      //     CusDisp_DB.update({name:"OKOK",targetDeffiles:[{hash:""}]},displayEle._id).then(()=>{

      //       CusDisp_DB.read(".").then(data=>{
      //         console.log(displayInfo);
      //         setDisplayInfo(data.prod);
      //         setDisplayEle(undefined);
      //       }).catch(e=>{
      //         console.log(e);
      //       });
      //     });
      //   }}
      //   style={{ width: '30%' }}
      // >
      //   mod
      // </Button>);


    }

  }
  return (

    <Layout style={{ height: "100%" }}>
      {/* <Layout.Header style={{ color: '#FFFFFF' }} >Header</Layout.Header> */}
      {/* <Layout.Sider  style={{ color: '#FFFFFF' }} collapsible collapsed={collapsed} 
        onCollapse={()=>setCollapsed(!collapsed)}
        onMouseOut={()=>(collapsed)?null:setCollapsed(true)}
        onMouseOver={()=>(collapsed)?setCollapsed(false):null}
        >
          Sider
        </Layout.Sider> */}
      <Layout>
        <Layout.Content style={{ padding: '50px 50px', overflow: "scroll" }}>{UI}</Layout.Content>
        <Layout.Footer>Custom UI v0.0.0</Layout.Footer>
      </Layout>

    </Layout>
  );
}



const DefFileLoadUI=()=>{

}

const InspectionDataPrepare = ({onPrepareOK}) => {
  const caruselRef = useRef(undefined);

  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);

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
  const [fileSelectorInfo,setFileSelectorInfo]=useState(undefined);
  
  const [stepIdx,setStepIdx]=useState(0);

  let DefFileFolder=undefined;

  useEffect(()=>{
    ACT_WS_SEND(WS_ID, "LD", 0, { deffile: defModelPath + '.' + DEF_EXTENSION, imgsrc: defModelPath });

  },[])

  let InspectionMonitor_URL_w_info=InspectionMonitor_URL;
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
        filePath = filePath.replace("." + DEF_EXTENSION, "");
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
              ACT_InspOptionalTag_Update(setTags)
              ACT_Def_Model_Path_Update(filePath);
              action_channal(stacked_pkts);
              
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
    
    isOK=DefFileHash!==undefined;
    if(!isOK)isStillOK=false;
    else if(isStillOK)
      OKJumpTo++;
    UI_Stack.push(
      <BASE_COM.CardFrameWarp key="UI_Step0" addClass="width12 height12 overlayCon" fixedFrame={true}>
        
        <CanvasComponent_rdx className="s height12" />
        
        <div className="overlay" style={{left:"15px",top:"15px"}}>

          <TagDisplay_rdx closable className="s width12 HXA" />
        </div>
        <div className="overlay" style={{right:"15px",bottom:"15px"}}>
          {/* <Button style={{"pointerEvents": "auto"}}>120px to affix top</Button> */}
          
          
          <Button className={"antd-icon-sizing "+(isOK?"HW50":"HW100")} size="large"
            style={{"pointerEvents": "auto"}} icon={<FolderOpenOutlined/> } type="text"
            onClick={() => {
            let fileSelectedCallBack =
              (filePath, fileInfo) => {
                if (localStorage !== undefined) {
                  let LocalS_RecentDefFiles = localStorage.getItem("RecentDefFiles");
                  try {
                    LocalS_RecentDefFiles = JSON.parse(LocalS_RecentDefFiles);
                  } catch (e) {
                    LocalS_RecentDefFiles = [];
                  }
                  if (!(LocalS_RecentDefFiles instanceof Array)) {
                    LocalS_RecentDefFiles = [];
                  }
                  //console.log(LocalS_RecentDefFiles);
                  LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter((ls_fileInfo) =>
                    (ls_fileInfo.name != fileInfo.name || ls_fileInfo.path != fileInfo.path));

                  LocalS_RecentDefFiles.unshift(fileInfo);
                  LocalS_RecentDefFiles = LocalS_RecentDefFiles.slice(0, 100);
                  localStorage.setItem("RecentDefFiles", JSON.stringify(LocalS_RecentDefFiles));
                  //console.log(localStorage.getItem("RecentDefFiles"));
                }

                filePath = filePath.replace("." + DEF_EXTENSION, "");
                setFileSelectorInfo(undefined);
                ACT_Def_Model_Path_Update(filePath);
                ACT_WS_SEND(WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath });
              }


            let LocalS_RecentDefFiles = localStorage.getItem("RecentDefFiles");
            try {
              LocalS_RecentDefFiles = JSON.parse(LocalS_RecentDefFiles);
            } catch (e) {
              LocalS_RecentDefFiles = [];
            }
            let fileGroups = [
              { name: "history", list: LocalS_RecentDefFiles }
            ];
            let fileSelectFilter = (fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes("." + DEF_EXTENSION);

            setFileSelectorInfo({
              callBack:fileSelectedCallBack,
              filter:fileSelectFilter,
              groups:fileGroups
            });
          }}/>


          
          <Button className={"antd-icon-sizing "+(isOK?"HW50":"HW100")} size="large"
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
            style={{"pointerEvents": "auto","color":(isOK?"#5191a5":"__")}} icon={<CaretRightOutlined/> } type="text" disabled={!isOK}
            onClick={()=>stepInc()}/>



        </div>
      </BASE_COM.CardFrameWarp>
    );
  }

  //console.log(stepIdx,UI_Stack.length,isOK);

  {  
    isOK=isTagFulFillRequrement(inspOptionalTag,tagGroupsPreset);
    
    if(!isOK)isStillOK=false;
    else if(isStillOK)
      OKJumpTo++;
    UI_Stack.push(
      <BASE_COM.CardFrameWarp key="UI_Step1" addClass="width12 height12 overlayCon" fixedFrame={true}>

        <TagDisplay_rdx closable className="s WXA HXA" />
        <Button size="large" onClick={loadMachineSettingPopUp}>機台設定選擇</Button>

        <TagOptions_rdx className="s width12 HXA" />
        <div className="overlay" style={{right:"15px",bottom:"15px"}}>

            
          <Button className={"antd-icon-sizing  "+(isOK?"HW100":"HW50")} size="large"
            style={{"pointerEvents": "auto","color":(isOK?"#5191a5":"__")}} icon={<CaretRightOutlined/> } type="text" disabled={!isOK}
            onClick={()=>{
              stepInc()
              }}/>



        </div>
      </BASE_COM.CardFrameWarp>
    );
  }
      
  
  if(false)isStillOK=false;
  else if(isStillOK)
    OKJumpTo++;
  UI_Stack.push(
      <BASE_COM.CardFrameWarp key="UI_Step2" addClass="width12 height12" fixedFrame={true}>
        {/* <Title className="veleXY">GO GO GO</Title> */}
        <ScanOutlined  className="veleXY antd-icon-sizing" style={{width:"100px",height:"100px"}}/>
      </BASE_COM.CardFrameWarp>
  );

  // if(stepIdx>=1)
  // {
  //   UI_Stack.push(
  //     <Card bordered={false}
  //       key="UI_Step2"
  //       className="small_padding_card overlay height12"
  //       style={{background:"#FFF"}}
  //       cover={
  //         [
  //           ]} >
          
  //     </Card>
  //   );
  // }
  console.log(caruselRef)
  return(
    
  <div style={{ padding: 24, background: '#fff', height: "100%",
    display: "flex",
    flexFlow: "column"
   }} >
      
    <Steps current={stepIdx} size="small"  onChange={stepJump} style={{flex:" 0 1 auto"}}>
      <Step title={LANG_DICT.mainui.select_deffile} description={LANG_DICT.mainui.select_deffile_detail}/>
      <Step title={LANG_DICT.mainui.set_insp_tags} description={LANG_DICT.mainui.set_insp_tags_detail} />
      <Step title={LANG_DICT.mainui.GOGOGO} description={LANG_DICT.mainui.GOGOGO_detail} />
    </Steps>


    <div className=" width12 ant-carousel_Con_WH100" style={{flex:" 1 1 auto"}} >
      
      <Carousel ref={caruselRef} className="width12 height12 " 
      dots={false} draggable={false}
      afterChange={(current)=>{
        console.log(current,stepIdx);
        if(current>stepIdx)
        {
          //caruselRef.current.goTo(stepIdx);

          caruselRef.current.slick.slickGoTo(stepIdx)
          return;
        }
        if(current==2&&onPrepareOK!==undefined)
        {
          onPrepareOK()
          
          setStepIdx(0);
          
          return;
        }
        if(current<stepIdx)
        {
          setStepIdx(current);
          caruselRef.current.goTo(current);
        }
      }}>
      {UI_Stack}
      </Carousel>
    </div>



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
        fileFilter={(fileSelectorInfo !== undefined)?fileSelectorInfo.filter:undefined} />

      <Modal
        title={InfoPopUp === undefined ? "" : InfoPopUp.title}
        visible={InfoPopUp !== undefined}
        
        footer={(InfoPopUp!==undefined && InfoPopUp.onOK===undefined &&  InfoPopUp.onCancel===undefined)?null:undefined}
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

  </div>

  );
};



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
      calibCalcInfo: {
        curMea1: 0.8,
        calibMea1: 0.82,
        curMea2: 0.4,
        calibMea2: 0.44,
      }
    }
  }


  componentDidMount() {
    let defModelPath = this.props.defModelPath;

  }

  shouldComponentUpdate(nextProps, nextState) {
    
    return true;
  }

  FrontDoor() {
    const content = (
      <div className="content">
        <Paragraph>
          HYVision 2019 is the most famous vision-check system in the world.
                </Paragraph>
        <Paragraph>
          <p>HYVision 2019 - v1 (http://hyv.idcircle.me)</p>
          <p>HYVision 2019 - v2 (http://hyv.idcircle.me)</p>
          <p>HYVision 2019 - v3 (http://hyv.idcircle.me)</p>
        </Paragraph>
        <p className="contentLink">
          <a>
            <img
              src="https://gw.alipayobjects.com/zos/rmsportal/MjEImQtenlyueSmVEfUD.svg"
              alt="start"
            /> Quick Start
                    </a>
          <a>
            <img src="https://gw.alipayobjects.com/zos/rmsportal/NbuDUAuBlIApFuDvWiND.svg" alt="info" />
                         Product Info
                    </a>
          <a>
            <img src="https://gw.alipayobjects.com/zos/rmsportal/ohOEPSYdDTNnyMbGuyLb.svg" alt="doc" />
                        Product Doc
                    </a>
        </p>
      </div>
    );

    const extraContent = (
      <img
        src="https://gw.alipayobjects.com/mdn/mpaas_user/afts/img/A*KsfVQbuLRlYAAAAAAAAAAABjAQAAAQ/original"
        alt="content"
      />
    );
    const routes = [
      {
        path: 'index',
        breadcrumbName: 'HYV',
      },
      {
        path: 'first',
        breadcrumbName: 'HOME',
      },
      {
        path: 'second',
        breadcrumbName: 'Manual',
      },
    ];
    return (<PageHeader title="HYVision 2019" breadcrumb={{ routes }}>
      <div className="wrap">
        <div className="content">{content}</div>
        <div className="extraContent">{extraContent}</div>
      </div>
    </PageHeader>);
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
    if (stateObj.state === UIAct.UI_SM_STATES.MAIN) {
      let DefFileFolder = this.props.defModelPath.substr(0, this.props.defModelPath.lastIndexOf('/') + 1);
      let genericMenuItemCBsCB = (selectInfo) => { this.setState({ ...this.state, menuSelect: selectInfo.key }) }

      let mmpp = undefined;
      if (this.props.camera_calibration_report !== undefined) {
        let camParam = this.props.isp_db.cameraParam;
        mmpp = camParam.mmpb2b / camParam.ppb2b;
      }



      let calibInfo = this.state.calibCalcInfo;
      console.log(calibInfo);
      let newCalibData = Calibration_MMPP_offset(
        calibInfo.curMea1,
        calibInfo.calibMea1,
        calibInfo.curMea2,
        calibInfo.calibMea2,
        mmpp, 0);
      let MenuItem = {
        // HOME:{
        //     icon:"home",
        //     content:this.FrontDoor(),
        //     onSelected: genericMenuItemCBsCB
        // },
        Overview: {
          icon: <InfoCircleOutlined />,
          content: <InspectionDataPrepare onPrepareOK={this.props.EV_UI_Insp_Mode}/>,
          onSelected: genericMenuItemCBsCB
        },
        EDIT: {
          icon: <EditOutlined />,
          content: null,
          onSelected: this.props.EV_UI_Edit_Mode
        },
        // Inspect: {
        //   icon: <ScanOutlined />,
        //   content: null,
        //   onSelected: () => {
        //     this.props.EV_UI_Insp_Mode();
        //   }
        // },

        SDD: {
          icon: <DatabaseOutlined />,
          content: <CustomDisplayUI
            BPG_Channel={(...args) => this.props.ACT_WS_SEND(this.props.WS_ID, ...args)} />,
          onSelected: genericMenuItemCBsCB
        },
        // STA:{
        //     icon:"bar-chart",
        //     content:null,
        //     onSelected:this.props.EV_UI_Analysis_Mode
        // },
        Setting: {
          icon: <SettingOutlined />,
          content: <div style={{ padding: 24, background: '#fff', minHeight: 360 }}>

            <Divider orientation="left">MISC</Divider>
            <AntButton key="Reconnect CAM"
              onClick={() => {
                this.props.ACT_WS_SEND(this.props.WS_ID, "RC", 0, {
                  target: "camera_ez_reconnect"
                });
              }}>Reconnect CAM</AntButton>
              &ensp;
              <AntButton key="camera Calib"
              onClick={() => {
                let fileSelectedCallBack =
                  (filePath, fileInfo) => {
                    console.log(filePath, fileInfo);
                    this.props.ACT_WS_SEND(this.props.WS_ID, "II", 0, {
                      deffile: "data/cameraCalibration.json",
                      imgsrc: filePath
                    });

                  }
                this.setState({ ...this.state, fileSelectedCallBack });

              }}>camera Calib</AntButton>

            <Divider orientation="left">µInsp</Divider>
            <Button.Group>

              <Button type="primary" key="Connect uInsp" disabled={this.props.uInspData.connected}
                icon={<LinkOutlined />}
                onClick={() => {
                  new Promise((resolve, reject) => {
                    this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
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
              <Button type="danger" key="Disconnect uInsp" disabled={!this.props.uInspData.connected}
                icon={<DisconnectOutlined />}
                onClick={() => {
                  new Promise((resolve, reject) => {
                    this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
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
              <Button key="ping uInsp" disabled={!this.props.uInspData.connected}
              onClick={() => {
                new Promise((resolve, reject) => {
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
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
              PING:{this.props.uInspData.alive}
            </Button>

              &ensp;
              <Button key="get_setup" disabled={!this.props.uInspData.connected}
              onClick={() => {
                new Promise((resolve, reject) => {
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
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
              <Button key="set_setup" disabled={this.props.uInspData.machineInfo === undefined}
              onClick={() => {
                let machInfo = dclone(this.props.uInspData.machineInfo);
                //machInfo.state_pulseOffset[0]+=1;
                new Promise((resolve, reject) => {
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
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



            <Button key="save_setup" disabled={this.props.uInspData.machineInfo === undefined}
              onClick={() => {
                var enc = new TextEncoder();
                this.props.ACT_Report_Save(this.props.WS_ID, "data/uInspSetting.json",
                  enc.encode(JSON.stringify(this.props.uInspData.machineInfo, null, 4)));
              }}>
              save_setup
              </Button>




            <Button key="file_set_setup" disabled={this.props.uInspData.machineInfo === undefined}
              onClick={() => {
                new Promise((resolve, reject) => {

                  this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
                    { filename: "data/uInspSetting.json" },
                    undefined, { resolve, reject }
                  );

                  setTimeout(() => reject("Timeout"), 5000)

                })
                  .then((pkts) => {
                    if (pkts[0].type != "FL") return;
                    let machInfo = pkts[0].data;
                    this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                      { msg: { ...machInfo, type: "set_setup", id: 356 } });
                  })
                  .catch((err) => {

                  })


              }}>
              file_set_setup
              </Button>

            {
              //JSON.stringify(this.props.uInspData)
            }

            <Divider orientation="left">SolveCalib</Divider>

              CurMea1: <InputNumber size="large" defaultValue={this.state.calibCalcInfo.curMea1} step={0.001}
              onChange={(val) => this.calibInfoUpdate({ curMea1: val })} />
              &ensp;&ensp;CalibMea1:<InputNumber size="large" defaultValue={this.state.calibCalcInfo.calibMea1} step={0.001}
              onChange={(val) => this.calibInfoUpdate({ calibMea1: val })} />
            <br />
              CurMea2:<InputNumber size="large" defaultValue={this.state.calibCalcInfo.curMea2} step={0.001}
              onChange={(val) => this.calibInfoUpdate({ curMea2: val })} />
              &ensp;&ensp;CalibMea2:<InputNumber size="large" defaultValue={this.state.calibCalcInfo.calibMea2} step={0.001}
              onChange={(val) => this.calibInfoUpdate({ calibMea2: val })} />
            <br />
              --------------------------------
              <br />
              MMPP:{newCalibData.mmpp}
            <br />
              OFFSET:{newCalibData.offset}


            <BPG_FileBrowser key="BPG_FileBrowser"
              className="width8 modal-sizing"
              searchDepth={4}
              path={DefFileFolder} visible={this.state.fileSelectedCallBack !== undefined}
              BPG_Channel={(...args) => this.props.ACT_WS_SEND(this.props.WS_ID, ...args)}
              onFileSelected={(filePath, fileInfo) => {
                this.setState({ ...this.state, fileSelectedCallBack: undefined });
                this.state.fileSelectedCallBack(filePath, fileInfo);
              }}
              onCancel={() => {
                this.setState({ ...this.state, fileSelectedCallBack: undefined });
              }}
              fileFilter={this.state.fileSelectFilter} />

            <BPG_FileSavingBrowser key="BPG_FileSavingBrowser"
              className="width8 modal-sizing"
              searchDepth={4}
              path={DefFileFolder} visible={this.state.fileSavingCallBack !== undefined}
              defaultName={""}
              BPG_Channel={(...args) => this.props.ACT_WS_SEND(this.props.WS_ID, ...args)}

              onOk={(folderInfo, fileName, existed) => {
                this.state.fileSavingCallBack(folderInfo, fileName, existed);

              }}
              onCancel={() => {
                this.setState({ ...this.state, fileSavingCallBack: undefined });
              }}
              fileFilter={this.state.fileSelectFilter}
            />
          </div>,
          onSelected: genericMenuItemCBsCB
        },
        
        BackLightCalib: {
          icon:<ScanOutlined />,
          content: <BackLightCalibUI_rdx
            BPG_Channel={(...args) => this.props.ACT_WS_SEND(this.props.WS_ID, ...args)}
            onCalibFinished={(finalReport) => {
              console.log(">>>>>>>>>",finalReport)
              if(finalReport===undefined)return;
              var enc = new TextEncoder();
              this.props.ACT_WS_SEND(this.props.WS_ID, "SV", 0,
                { filename: "data/stageLightReport.json" },
                enc.encode(JSON.stringify(finalReport, null, 2)))
              console.log(finalReport)
            }} />,
          onSelected: genericMenuItemCBsCB
        },
        Collapse: {
          icon: this.state.menuCollapsed ? <RightOutlined /> :<LeftOutlined />,
          content: null,
          onSelected: () => this.setState({ ...this.state, menuCollapsed: !this.state.menuCollapsed })
        }
      };


      let ver_map = this.props.version_map_info;
      let recommend_URL = GetObjElement(ver_map, ["recommend_info", "url"]);

      if (recommend_URL !== undefined && (recommend_URL.indexOf()) == -1) {
        MenuItem.UPDATE = {
          icon: <CloudDownloadOutlined />,
          content: null,
          onSelected: () => {
            window.location.href = recommend_URL;
          }
        };
      }


      UI.push(
        <Layout className="HXF">
          <Sider
            trigger={null}
            collapsible
            collapsed={this.state.menuCollapsed}
          //collapsed={this.state.collapsed}
          >
            <Menu theme="dark" mode="inline" defaultSelectedKeys={[this.state.menuSelect]}
              onClick={(select) => MenuItem[select.key].onSelected(select)}>
              {
                Object.keys(MenuItem).map(itemKey => (
                  <Menu.Item key={itemKey} >
                    {MenuItem[itemKey].icon}
                    <span>{itemKey}</span>
                  </Menu.Item>
                ))
              }
            </Menu>
          </Sider>

          <Layout>
            <Content>
              {

                MenuItem[this.state.menuSelect].content
              }
            </Content>
          </Layout>
        </Layout>
      )
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
      <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
        {UI}
      </BASE_COM.CardFrameWarp>

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
    ACT_Report_Save: (id, fileName, content) => {
      let act = UIAct.EV_WS_SEND(id, "SV", 0,
        { filename: fileName },
        content
      )
      console.log(act);
      dispatch(act);
    }
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
    InspectionMonitor_URL: state.UIData.InspectionMonitor_URL,
    uInspData: state.Peripheral.uInsp,

    statSetting: state.UIData.edit_info.statSetting,
    inspMode: state.UIData.inspMode,
    machine_custom_setting: state.UIData.machine_custom_setting,
  }
}

let APPMain_rdx = connect(mapStateToProps_APPMain, mapDispatchToProps_APPMain)(APPMain);
export default APPMain_rdx;

