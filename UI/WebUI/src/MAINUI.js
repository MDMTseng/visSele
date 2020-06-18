
import 'antd/dist/antd.less';
import { connect } from 'react-redux'
import React, { useState, useEffect } from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx,TagDisplay_rdx, essentialTags, CustomDisplaySelectUI } from './component/rdxComponent.jsx';

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
import Popover from 'antd/lib/popover';

import Row from 'antd/lib/Row';
import Col from 'antd/lib/Col';
const { Meta } = Card;

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
  FundOutlined } from '@ant-design/icons';


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



class DefFileLoader extends React.Component {

  constructor(props) {
    super(props);
    this.QR_Content = "";
    this.state = {
    };
  }

  componentDidMount() {
    this.setState({ ...this.state, canvas: this.refs.canvas });
  }
  onResize(width, height) {
  }
  componentDidUpdate(prevProps, prevState) {
  }

  render() {

    let InspectionMonitor_URL;

    if (isString(this.props.defModelHash) && this.props.defModelHash.length > 5) {
      InspectionMonitor_URL = this.props.InspectionMonitor_URL +
        "?v=" + 0 + "&name=" + this.props.defModelName + "&hash=" + this.props.defModelHash;
      InspectionMonitor_URL = encodeURI(InspectionMonitor_URL);
    }
    let DefFileFolder = this.props.defModelPath.substr(0, this.props.defModelPath.lastIndexOf('/') + 1);


    return(
      
      <Row gutter={18} className="height8 width12 ">
      <Col span={12}>
      
        <Card 
          cover={null}
          hoverable
          className="small_padding_card"
          actions={[
            <FolderOpenOutlined key="setting" onClick={() => {
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
                  this.setState({ fileSelectedCallBack: undefined });
                  this.props.ACT_Def_Model_Path_Update(filePath);
                  this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath });
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
              this.setState({ fileSelectedCallBack, fileSelectFilter, fileGroups });
            }}/>,
            <Popover content={
              <QR_Canvas className="veleX" style={{height:"100%"}}
                      onClick={() => window.open(InspectionMonitor_URL)} QR_Content={InspectionMonitor_URL} />} title="QR" trigger="hover">
              <QrcodeOutlined key="edit"/>
            </Popover>
          ]}
          >
          {
            <CanvasComponent_rdx className="s HXF"  style={{height:"500px","a":1}}/>
          }
          {/* {(this.props.defModelName===undefined)?"OPEN":this.props.defModelName} */}
        </Card>
      </Col>
      <Col span={12}>
        <Card title="Card title" bordered={false}
        
        cover={
          [
            <TagDisplay_rdx className="s width12 HXA" />,
            <Button size="large" onClick={() => {

              let popUpUIInfo = {
                title: "機台設定",
                onOK: () => { },
                onCancel: () => { },
                content: <CustomDisplaySelectUI onSelect={(cusDispInfo) => {

                  let tarDef = cusDispInfo.targetDeffiles[0];
                  let filePath = tarDef.path;
                  if (filePath === undefined) return;
                  filePath = filePath.replace("." + DEF_EXTENSION, "");
                  this.setState({ popUpUIInfo: undefined });
                  this.props.ACT_Def_Model_Path_Update(filePath);
                  this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0, { deffile: filePath + '.' + DEF_EXTENSION, imgsrc: filePath });

                  let setTags = [];
                  try {
                    setTags = tarDef.tags.split(",");

                  }
                  catch (e) {
                    setTags = [];
                  }
                  this.props.ACT_InspOptionalTag_Update(setTags)

                }} />
              }
              this.setState({ popUpUIInfo });
              }}>機台設定選擇</Button>,

              <TagOptions_rdx className="s width12 HXA" />]} >
          
        </Card>
      </Col>
      
      <BPG_FileBrowser key="BPG_FileBrowser"
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
        
        fileGroups={this.state.fileGroups}
        fileFilter={this.state.fileSelectFilter} />


      </Row>

    );
  }
}

const mapStateToProps_DefFileLoader = (state) => {
  //console.log("mapStateToProps",JSON.stringify(state.UIData.c_state));
  return {
    WS_CH: state.UIData.WS_CH,
    WS_ID: state.UIData.WS_ID,
    defModelPath: state.UIData.edit_info.defModelPath,
    defModelName: state.UIData.edit_info.DefFileName,
    defModelHash: state.UIData.edit_info.DefFileHash,

  }
}
const mapDispatchToProps_DefFileLoader = (dispatch, ownProps) => {
  return {
    
    ACT_Def_Model_Path_Update: (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) },
    ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),
    
  }
}
const DefFileLoader_rdx = connect(
  mapStateToProps_DefFileLoader,
  mapDispatchToProps_DefFileLoader)(DefFileLoader);




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

    this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0, { deffile: defModelPath + '.' + DEF_EXTENSION, imgsrc: defModelPath });


    setTimeout(()=>
      new Promise((resolve, reject) => {
        console.log(">>>")
        this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
          { filename: "data/machine_setting.json" },
          undefined, { resolve, reject });
      }).then((data) => {
        if (data[0].type == "FL") {
          let info = data[0].data;
          
          this.props.ACT_Machine_Custom_Setting_Update(info);
        }
      }).catch((err) => {
        console.log(err);
      }),1000);
    
    new Promise((resolve, reject) => {
      this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
        { filename: "data/machine_info" },
        undefined, { resolve, reject });
    }).then((data) => {
      if (data[0].type == "FL") {
        let info = data[0].data;
        if (info.length < 16) {
          this.props.ACT_MachTag_Update(info);
        }
        //console.log(info);
      }
    }).catch((err) => {
      console.log(err);
    })

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
          content: <div style={{ padding: 24, background: '#fff', height: "100%" }}>
            {/* <div className="s black">{this.props.WebUI_info.version}</div> */}
            <DefFileLoader_rdx/>

            {/*               
              {(mmpp===undefined)?null:
                <Button  key="mmpp"  size="large"  onClick={()=>{
                  
                  let CameraSettingFromFile_Path = "data/";
                  //CameraSettingFromFile_Path+="S0886/";
                  
                  this.props.ACT_WS_SEND(this.props.WS_ID,"ST",0,
                    {CameraSettingFromFileSetup:CameraSettingFromFileSetup_Path});
                    
                    
                  this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,
                    {filename:CameraSettingFromFileSetup_Path+"default_camera_param.json"});

                }}><Icon type="camera" /> {"mmpp:"+mmpp}</Button>}
 */}


            <Modal
              title={this.state.popUpUIInfo === undefined ? "" : this.state.popUpUIInfo.title}
              visible={this.state.popUpUIInfo !== undefined}
              onOk={() => {
                this.state.popUpUIInfo.onOK();
                this.setState({ popUpUIInfo: undefined });
              }}
              onCancel={() => {
                this.state.popUpUIInfo.onCancel();
                this.setState({ popUpUIInfo: undefined });
              }}
            >
              {this.state.popUpUIInfo === undefined ?
                null : this.state.popUpUIInfo.content}
            </Modal>
            {this.state.additionalUI}
          </div>,
          onSelected: genericMenuItemCBsCB
        },
        EDIT: {
          icon: <EditOutlined />,
          content: null,
          onSelected: this.props.EV_UI_Edit_Mode
        },
        Inspect: {
          icon: <ScanOutlined />,
          content: null,
          onSelected: () => {

            console.log(this.state.menuSelect);
            if (this.state.menuSelect !== "Overview") return;
            if (this.props.inspOptionalTag.reduce((hasEss, tag) => hasEss || essentialTags.includes(tag), false)) {
              this.props.EV_UI_Insp_Mode();
            }
            else {
              this.setState({
                additionalUI: [
                  <Modal
                    title={"警告"}
                    visible={true}
                    onOk={() => {
                      this.setState({ additionalUI: [] });
                      //this.props.EV_UI_Insp_Mode();
                    }}
                    onCancel={() => {
                      this.setState({ additionalUI: [] });
                      //this.props.EV_UI_Insp_Mode();
                    }}
                    cancelButtonProps={{ style: { display: 'none' } }}
                  //okButtonProps={{ disabled: true }}
                  // onCancel={()=>{
                  //   this.setState({additionalUI:[]});
                  // }}
                  >
                    <div style={{ height: "auto" }}>
                      警告：沒有設定 必要Tag
                    <br />
                    必要Tag 如下
                    <br />

                      {
                        essentialTags.map((ele, idx, arr) =>
                          <Tag className=" InspTag optional fixed" key={ele + "_ele"}>{ele}</Tag>)
                      }

                      {/* <TagOptions_rdx className="s width12 HXA"/> */}
                    </div>
                  </Modal>
                ]
              });
            }

          }
        },

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
              setTimeout(() => {
                var enc = new TextEncoder();
                this.props.ACT_WS_SEND(this.props.WS_ID, "SV", 0,
                  { filename: "data/stageLightReport.json" },
                  enc.encode(JSON.stringify(finalReport, null, 2)))
                console.log(finalReport)
              }, 100)
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
    ACT_Def_Model_Path_Update: (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) },
    ACT_MachTag_Update: (machTag) => { dispatch(DefConfAct.MachTag_Update(machTag)) },

    ACT_InspOptionalTag_Update: (newTag) => { dispatch(DefConfAct.InspOptionalTag_Update(newTag)) },
    ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_StatSettingParam_Update: (arg) => dispatch(UIAct.EV_StatSettingParam_Update(arg)),
    ACT_WS_DISCONNECT: (id) => dispatch(UIAct.EV_WS_Disconnect(id)),
    ACT_Insp_Mode_Update: (mode) => dispatch(UIAct.EV_UI_Insp_Mode_Update(mode)),
    ACT_Machine_Custom_Setting_Update: (info) => dispatch(UIAct.EV_machine_custom_setting_Update(info)),
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
    defModelName: state.UIData.edit_info.DefFileName,
    defFileTag: state.UIData.edit_info.DefFileTag,
    inspOptionalTag: state.UIData.edit_info.inspOptionalTag,
    defModelPath: state.UIData.edit_info.defModelPath,
    defModelHash: state.UIData.edit_info.DefFileHash,
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

