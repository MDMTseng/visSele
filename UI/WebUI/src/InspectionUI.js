'use strict'


import { connect } from 'react-redux';
import React, { useState, useEffect,useRef } from 'react';
import { useSelector,useDispatch } from 'react-redux';

import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';
var dateFormat = require("dateformat");
import INFO from './info.js';
import { TagOptions_rdx,UINSP_UI } from './component/rdxComponent.jsx';
import dclone from 'clone';
import Color from 'color';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { websocket_autoReconnect, websocket_reqTrack, copyToClipboard, ConsumeQueue ,defFileGeneration,GetObjElement,dictLookUp} from 'UTIL/MISC_Util';
import { SHAPE_TYPE, DEFAULT_UNIT } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { INSPECTION_STATUS, DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import * as logX from 'loglevel';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {TagDisplay_rdx} from './component/rdxComponent.jsx';
// import { PageHeader } from 'antd/lib/page-header';
//import {Doughnut} from 'react-chartjs-2';

import { round } from 'UTIL/MISC_Util';
let localStorage = require('localStorage');
let log = logX.getLogger("InspectionUI");

import Row from 'antd/lib/Row';
import Col from 'antd/lib/Col';
import Slider from 'antd/lib/Slider';
import Checkbox from 'antd/lib/checkbox'
import Popover from 'antd/lib/popover';
import Table from 'antd/lib/table';
import Switch from 'antd/lib/switch';
import Tag from 'antd/lib/tag';
import Input from 'antd/lib/input';
import InputNumber from 'antd/lib/input-number';
import Select from 'antd/lib/select';
import Button, { default as AntButton } from 'antd/lib/button';
import Menu from 'antd/lib/menu';


import { 
  DisconnectOutlined,
  FileOutlined,
  LinkOutlined,
  LineOutlined,
  TagsOutlined,
  CameraOutlined,
  RedoOutlined,
  ExpandOutlined,
  HeartTwoTone,
  ArrowLeftOutlined,
  FullscreenOutlined,
  PaperClipOutlined,
  SettingOutlined,
  CaretDownOutlined,
  BarChartOutlined,
  SaveOutlined,
  BulbOutlined,
  ReloadOutlined

} from '@ant-design/icons';



import Divider from 'antd/lib/divider';

import Chart from 'chart.js';
import 'chartjs-plugin-annotation';
import Modal from "antd/lib/modal";
// import Upload from 'antd/lib/upload';
// import Input from 'antd/lib/Input';
import Dropdown from 'antd/lib/Dropdown'

import Typography from 'antd/lib/typography';
const { Paragraph, Title } = Typography;


// import Tag from 'antd/lib/tag';
// import Select from 'antd/lib/select';
// import Menu from 'antd/lib/menu';
// import Button from 'antd/lib/button';
// import Icon from 'antd/lib/icon';

let stream_PGID_=10004;

function urlConcat(base,add)
{
  let xbase=base;
  while(xbase.charAt(xbase.length-1)=="/")
    xbase=xbase.slice(0, xbase.length-1)
    
  let xadd=add;
  while(xadd.charAt(0)=="/")
    xadd=xadd.slice(1, xbase.length)
  

  return xbase+"/"+xadd;
}

const ButtonGroup = Button.Group;

const Option = Select.Option;

const selectBefore = (
  <Select defaultValue="Http://" style={{ width: 90 }}>
    <Option value="Http://">Http://</Option>
    <Option value="Https://">Https://</Option>
  </Select>
);
const selectAfter = (
  <Select defaultValue=".com" style={{ width: 80 }}>
    <Option value=".com">.com</Option>
    <Option value=".jp">.jp</Option>
    <Option value=".cn">.cn</Option>
    <Option value=".org">.org</Option>
  </Select>
);
const SubMenu = Menu.SubMenu;
const MenuItemGroup = Menu.ItemGroup;


function InspectionReportInsert2DB({reportStatisticState,onDBInsertSuccess,onDBInsertFail,LANG_DICT,insert_skip=0})
        {

  const _s = useRef({sendCounter:0,sendedCounter:0,totalCounter:0,pre_newAddedReport:undefined});

  let _this=_s.current;
  // const c_state = useSelector(state => state.UIData.c_state);
  const dispatch = useDispatch();
  const Insp_DB_W_ID = useSelector(state => state.ConnInfo.Insp_DB_W_ID);
  const Insp_DB_W_ID_CONN_INFO = useSelector(state => state.ConnInfo.Insp_DB_W_ID_CONN_INFO);

  const WS_SEND= (id,data,return_cb) => dispatch(UIAct.EV_WS_SEND_PLAIN(id,data,return_cb));

  let newAddedReport=GetObjElement(reportStatisticState,["newAddedReport"]);

  useEffect(()=>{
    if(newAddedReport===undefined || 
      _this.pre_newAddedReport===newAddedReport || 
      Array.isArray(newAddedReport)==false ||
      newAddedReport.length==0)//there is no new report
    {
      // console.log("no report...");
      return;
    }

    _this.pre_newAddedReport=newAddedReport;
    // console.log(newAddedReport,insert_skip);
    
    let res=_this.totalCounter%insert_skip;
    _this.totalCounter++;
    if(res!=0)
    {
      // console.log("SKIP...");
      return;
    }

    _this.sendCounter++;
    WS_SEND(Insp_DB_W_ID,newAddedReport)
    .then(retInfo=>{
      _this.sendedCounter++;
      onDBInsertSuccess(retInfo);
      console.log(retInfo);
    })
    .catch(err=>{

    })
  },[newAddedReport]);


  // let retx=
  //   this.props.ACT_WS_GET_OBJ(this.props.WS_InspDataBase_W_ID, (obj)=>{

  //     console.log(obj);
  //     return obj.websocket.send_obj({type:"PING"});
  //   })
  //   .then(d=>{
  //     console.log(d);
  //   })
  //   .catch(e=>{
  //     console.log(e);

  //   })

  let isConnected=GetObjElement(Insp_DB_W_ID_CONN_INFO,["type"])==="WS_CONNECTED";

      

  // return null;


  return <Button type="primary" size={"large"} 
    className={ (isConnected ? "blackText lgreen" : "DISCONNECT_Blink")}
    icon={isConnected ? <LinkOutlined /> : <DisconnectOutlined />} >
        {(isConnected ? LANG_DICT.connection.server_connected : LANG_DICT.connection.server_disconnected)
        + " " + _this.sendedCounter+"<"+_this.sendCounter + ":" + _this.totalCounter + "/" + insert_skip}
    </Button>
}



// <InspectionReportInsert2DB 
// reportStatisticState={this.props.reportStatisticState} 

// // DBStatus,
// // DBPushPromise,
// onDBInsertSuccess={(data, info) => {
//   // log.info(data, info);
//   this.setState({ inspUploadedCount: this.state.inspUploadedCount + 1 });
// }}
// onDBInsertFail={(data, info) => {
//   log.error(data, info);
// }}
// insert_skip={InspectionReportPullSkip}/>







class DB extends React.Component {
  constructor(props) {
    super(props);
    this.resultDB = undefined;
    this.state = {
      db_open: false,
      resultX: undefined
    }
  }
  componentDidMount() {

  }
  echoTime() {
    return "" + new Date().getSeconds();
  }
  query() {
    this.resultDB = "[!]";
    fetch('http://127.0.0.1:4000/gui', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
      body: JSON.stringify({ query: "{ hello }" })
    }).then(r => r.json()).then((datax) => {
      log.info('[O]DB data returned:', JSON.stringify(datax));

      this.resultDB = JSON.stringify(datax);
    });
    return this.resultDB;
  }
  render() {
    return (
      <div style={{ 'display': 'inline-block' }}>
        {/*{this.echoTime()}*/}
        <p>
          {/*{this.query()}*/}
        </p>
      </div>
    );
  }
}


export const OK_NG_BOX_COLOR_TEXT = {
  [MEASURERSULTRESION.UNSET]: { COLOR: "#999", TEXT: MEASURERSULTRESION.UNSET },
  [MEASURERSULTRESION.NA]: { COLOR: "#aaa", TEXT: MEASURERSULTRESION.NA },

  [MEASURERSULTRESION.UOK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.UOK },
  [MEASURERSULTRESION.LOK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.LOK },
  [MEASURERSULTRESION.OK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.OK },

  [MEASURERSULTRESION.UCNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.UCNG },
  [MEASURERSULTRESION.LCNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.LCNG },
  [MEASURERSULTRESION.CNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.CNG },

  [MEASURERSULTRESION.USNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.USNG },
  [MEASURERSULTRESION.LSNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.LSNG },
  [MEASURERSULTRESION.SNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.SNG },
  [MEASURERSULTRESION.NG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.SNG },
};

export class InspectionResultDisplay_FullScren extends React.Component {

  constructor(props) {
    super(props);
    this.state = {
      folderStruct: {},
      history: ["./"],
    }
  }
  render() {
    // if(!this.props.visible)return;

    let reports = this.props.IR;
    if (!Array.isArray(reports)) {
      return null;
    }

    // console.log("[XLINX]clone rMenu");
    // console.log(this.props.resultMenuCopy);
    let resultMenu_4fullscreenUse = this.props.resultMenuCopy;

    let titleRender = <div>
      Object List Window
        </div>;
    let openAllsubMenuKeyList = resultMenu_4fullscreenUse.map(function (item, index, array) {
      return "sub1" + index;
    });

    let separateSubMenu = resultMenu_4fullscreenUse.map(function (item) {
      return <Col span={8}>
        <Menu
          // onClick={this.handleClick}
          // selectedKeys={[this.current]}
          selectable={true}
          // style={{align: 'left', width: 200}}
          defaultSelectedKeys={['functionMenu']}
          defaultOpenKeys={openAllsubMenuKeyList}
          mode="inline">
          {item}
        </Menu>
      </Col>;
    });

    return (
      <Modal
        title={titleRender}
        visible={this.props.visible}
        width={this.props.width === undefined ? 900 : this.props.width}
        onCancel={this.props.onCancel}
        onOk={this.props.onOk}
        footer={null}
      >
        <div style={{ height: this.props.height === undefined ? 400 : this.props.height }}>
          <Row type="flex" justify="start" align="top">
            {separateSubMenu}
          </Row>
        </div>
      </Modal>
    );
  }
}

class InspectionResultDisplay extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      // fullScreen:false
    }
  }


  clickFullScreen() {
    // this.setState({
    //     ...this.state,fullScreen:!this.state.fullScreen
    // });
    // console.log("[XLINX]fullScreen="+this.state.fullScreen);
    this.props.fullScreenToggleCallback();
  };

  showResultValueCheck(o) {
    if (o.status == MEASURERSULTRESION.NA)
      return "NaN ";
    else if (o.value === +o.value)
      return o.value.toFixed(3);
    else
      return "NaN ";
  }


  render() {
    let rep = this.props.singleInspection;
    let quality_essential = GetObjElement(rep,["def","quality_essential"]);


    let COLOR=OK_NG_BOX_COLOR_TEXT[rep.detailStatus].COLOR;
    if(quality_essential==false)
      COLOR = Color(COLOR).desaturate(0.6).darken(0.5);

    let height=70;
    
    let fontStyle={fontSize:25};

    if(quality_essential==false)
    {
      height=70;
      fontStyle={
        fontSize:25,
        // textDecoration: "line-through"
        color:"#999"
      }
    }


    let resValue = this.showResultValueCheck(rep);
    let bundary_ratio=resValue>rep.def.value?
    (resValue-rep.def.value)/(rep.def.USL-rep.def.value):
    (resValue-rep.def.value)/(rep.def.LSL-rep.def.value)*-1;

    let detailInfo=<>
    類型:{dictLookUp(rep.def.subtype,this.props.DICT)} <br/>
    目標:{rep.def.value}<br/>
    上限:{rep.def.USL}<br/>
    下限:{rep.def.LSL}<br/>
    檢測值:{resValue}<br/>
    
    界限比例:{bundary_ratio.toFixed(2)}<br/>
    </>


    // console.log(rep);
    return <div className="s black" style={{ "borderBottom": "6px solid #A9A9A9", height:height}}>
      <div className="s width8  HXF">
        <div className="s vbox height4">
          <FullscreenOutlined onClick={this.clickFullScreen.bind(this)} />
          {rep.name}
        </div>
        <Popover content={detailInfo} placement="bottomLeft" trigger={["click","hover"]}>
          <div className="s vbox  height8" style={fontStyle}>
              {resValue + DEFAULT_UNIT[rep.subtype]}
          </div>
        </Popover>
      </div>
      <div className="s vbox width4 HXF">
        <Tag style={{ 'fontSize': 18 }}
          color={COLOR}>
          {OK_NG_BOX_COLOR_TEXT[rep.detailStatus].TEXT}
        </Tag>
      </div>
    </div>;
  }
}

class OK_NG_BOX extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      // OK_NG: undefined,
    }
  }

  render() {
    //console.log(this.props.detailStatus)
    return (
      <div style={{ 'display': 'inline-block' }}>
        <Tag style={{ 'fontSize': 20 }}
          color={OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].COLOR}>
          {OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].TEXT}
        </Tag>
        {this.props.children}
      </div>
    )
  }
}

class CameraCtrl {
  constructor(setting) {
    this.data = {
      DoImageTransfer: true,
      emptyResultCount: 0,
      cameraSpeedMode: -1,
      speedSwitchingCount: 100,
    };
    this.ws_ch = setting.ws_ch;

    this.ev_speedModeChange = setting.ev_speedModeChange;
    if (this.ev_speedModeChange === undefined)
      this.ev_speedModeChange = () => { };

    this.ev_emptyResultCountChange = setting.ev_emptyResultCountChange;
    if (this.ev_emptyResultCountChange === undefined)
      this.ev_emptyResultCountChange = () => { };

    this.setSpeedSwitchingCount(1000);
    this.setCameraSpeed_HIGH();
  }



  setCameraImageTransfer(doTransfer) {
    if (doTransfer === undefined) doTransfer = !this.data.DoImageTransfer;
    this.data.DoImageTransfer = doTransfer;
    this.ws_ch({ DoImageTransfer: doTransfer });
  }

  setCameraSpeedMode(mode) {
    if (this.data.cameraSpeedMode == mode) return;
    log.info("setCameraSpeedMode:" + mode);
    this.data.cameraSpeedMode = mode;

    this.ev_speedModeChange(mode);
    this.ws_ch({ CameraSetting: { framerate_mode: mode } });
  }


  setImageCropParam(cropWindow,downSampleFactor=8) {


    let obj={};
    if(cropWindow!==undefined)
    {
      obj.ImageTransferSetup={
        crop:cropWindow
      };
    }
    
    if(downSampleFactor!==undefined)
    {
      obj.CameraSetting={
        down_samp_level:downSampleFactor
      };
    }
    this.ws_ch(obj);
  }



  setSpeedSwitchingCount(speedSwitchingCount = 1000) {
    this.data.speedSwitchingCount = speedSwitchingCount;
  }

  setCameraSpeed_HIGH() {
    this.setCameraSpeedMode(2);
  }
  setCameraSpeed_LOW() {
    this.setCameraSpeedMode(0);
  }

  updateInspectionReport(report) {
    if (report === undefined || report.reports.length == 0) {
      this.data.emptyResultCount++;
      if (this.data.emptyResultCount > this.data.speedSwitchingCount)
        this.setCameraSpeed_LOW();

      this.ev_emptyResultCountChange(this.data.emptyResultCount);
      return;
    }

    if (this.data.emptyResultCount != 0) {
      this.ev_emptyResultCountChange(this.data.emptyResultCount);
      this.data.emptyResultCount = 0;
      this.setCameraSpeed_HIGH();
    }
  }

}


class ObjInfoList extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      collapsed: false,
      fullScreen: false,
    }
  }

  toggleCollapsed() {
    this.setState({
      ...this.state,
      collapsed: !this.state.collapsed,
    });
  }
  toggleFullscreen() {
    this.setState({
      ...this.state,
      fullScreen: !this.state.fullScreen,
    });

  }
  render() {

    let resultMenu = [];
    let reports = this.props.IR;
    let IR_decotrator = this.props.IR_decotrator;
    if (!Array.isArray(reports)) {
      return null;
    }


    let curObjList = reports.filter((rep) => rep.isCurObj);
    let optiona_id_order = undefined;
    if (IR_decotrator !== undefined) {
      optiona_id_order = IR_decotrator.list_id_order;
    }

    resultMenu = curObjList.map((singleReport, idx) => {
      let reportDetail = [];
      let judgeReports = singleReport.judgeReports;
      if(judgeReports===undefined||judgeReports.length==0)
      {
        return null;
      }
      if (optiona_id_order !== undefined) {
        judgeReports = optiona_id_order.
          map(id => judgeReports.find(judge => judge.id == id)).
          filter(rep => rep !== undefined);
      }



      let judgeInRank = judgeReports
      .map(rep=>({...rep,def:this.props.shape_def.find(def=>def.id==rep.id)}))
      .filter(rep=>{
        let rdef=rep.def;
        if(rdef.rank===undefined)return true;
        if(rdef.rank<=this.props.measureDisplayRank)return true;
        return false;
      });


      reportDetail =judgeInRank
        // .filter(rep=>rep.def.quality_essential!=false)//do not show quality_essential=false result
        .map((rep, idx_) => (
          <InspectionResultDisplay DICT={this.props.DICT} key={"i" + rep.name} singleInspection={rep} fullScreenToggleCallback={this.toggleFullscreen.bind(this)} />
        )
      );

      // console.log(judgeInRank);
      let finalResult = judgeInRank.reduce((res, rep) => {
        if(rep.def.quality_essential==false)return res;
        return MEASURERSULTRESION_reducer(res, rep.detailStatus);
      }, undefined);
      // console.log(singleReport.isFlipped);
      return (
        <SubMenu style={{ 'textAlign': 'left' }} key={"sub1" + idx}
          title={
            <span>
              <PaperClipOutlined />
              <span>
                {`${idx}   ${singleReport.isFlipped?"反":"正"}`} <OK_NG_BOX detailStatus={finalResult} />
              </span>
            </span>}>
          {reportDetail}
        </SubMenu>
      )
    }
    )


    let fullScreenMODAL = <InspectionResultDisplay_FullScren {...this.state} resultMenuCopy={resultMenu} IR={reports} visible={this.state.fullScreen}
      onCancel={this.toggleFullscreen.bind(this)} width="90%" />;

    let uInspUI=this.props.uInsp_API_ID_CONN_INFO===undefined? null:
    <SubMenu style={{ 'textAlign': 'left' }} key={"uInsp" } className="Antd_Menu_Title_AutoHeight Antd_Menu_Title_Padding_Left_small"
      title={
      <>
        <Divider orientation="center" key="divi" style={{ 'margin': '5px'}} className="Antd_Divider_Small_Text_Tight">全檢設備</Divider>
        <UINSP_UI UI_INSP_Count={true} UI_INSP_Count_font_size={15}/>
      </>}
      > 
        <div style={{margin:"15px"}}>
          <Divider orientation="left" style={{ 'margin': '2px',fontSize: "12px"}} >全檢速度(pcs/s)</Divider>
          <UINSP_UI UI_INSP_Count_Rate={true} UI_INSP_Count_font_size={15}/>
          <Divider orientation="left" style={{ 'margin': '10px 2px 2px 2px',fontSize: "12px"}} >轉盤速度(pulse/s)</Divider>
          <UINSP_UI UI_Speed_Slider={true}/>
          
        </div>
    </SubMenu>

    return (
      <>
        <Menu
          // onClick={this.handleClick}
          // selectedKeys={[this.current]}
          selectable={true}
          // style={{align: 'left', width: 200}}
          defaultSelectedKeys={['functionMenu']}
          // defaultOpenKeys={['functionMenu']}
          mode="inline">
          {uInspUI}
          

          {resultMenu}

        </Menu>
        {fullScreenMODAL}
      </>
    );
  }
}


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  triggerROISelect()
  {

  }

  componentDidUpdate(prevProps) {
    if (prevProps.onROISettingCallBack !== this.props.onROISettingCallBack && this.ec_canvas !== undefined) {

      if( this.props.onROISettingCallBack!==undefined)
      {
        this.ec_canvas.SetROISettingCallBack(this.props.onROISettingCallBack);
      }
      else
      {
        this.ec_canvas.SetROISettingCallBack(undefined);
      }
    }
  }

  ec_canvas_EmitEvent(event) {
    switch (event.type) {
      case DefConfAct.EVENT.ERROR:
        log.error(event);
        this.props.ACT_ERROR();
        break;
      case "down_samp_level_update":
        // log.error(event);
        // this.props.ACT_ERROR();

        let rep = this.props.camera_calibration_report.reports[0];
        let mmpp = rep.mmpb2b / rep.ppb2b;
        // event.data.down_samp_level*=this.props.downSampleFactor;
        let crop = event.data.crop.map(val => val / mmpp);
        // console.log(this.props.downSampleFactor);
        let down_samp_level = Math.floor(event.data.down_samp_level*this.props.downSampleFactor / mmpp ) + 1;
        if (down_samp_level <= 0) down_samp_level = 1;
        else if (down_samp_level > 10) down_samp_level = 10;

        // down_samp_level=1;
        //log.info(crop,down_samp_level);
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
          {
            CameraSetting: {
              down_samp_level
            },
            ImageTransferSetup: {
              crop
            }
          });
        break;

    }
  }

  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.INSP_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);
    this.props.onCanvasInit(this.ec_canvas);
    this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas !== undefined) {
      log.debug("updateCanvas>>", props.edit_info);

      {
        this.ec_canvas.EditDBInfoSync(props.edit_info);
        this.ec_canvas.SetState(ec_state);
        this.ec_canvas.SetMeasureDisplayRank(props.measureDisplayRank);
        //this.ec_canvas.ctrlLogic();
        this.ec_canvas.draw();
        this.ec_canvas.doRotateView=this.props.renderObjAlignRotate;
        
      }
    }
  }

  onResize(width, height) {
    
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 15) return;
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
      this.updateCanvas(this.props.c_state);
    }
  }

  componentWillUpdate(nextProps, nextState) {
    this.updateCanvas(nextProps.c_state, nextProps);
  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}


const mapStateToProps_CanvasComponent = (state) => {
  //log.info("mapStateToProps",JSON.stringify(state.UIData.c_state));
  return {
    c_state: state.UIData.c_state,
    edit_info: state.UIData.edit_info,

  }
}


const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => {
  return {
    ACT_EXIT: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
    },
    ACT_ERROR: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.ERROR))
    }
  }
}
const CanvasComponent_rdx = connect(
  mapStateToProps_CanvasComponent,
  mapDispatchToProps_CanvasComponent)(CanvasComponent);



class ControlChart extends React.Component {
  constructor(props) {
    super(props);
    this.divID = "ControlChart_ID_" + Math.round(Math.random() * 10000);
    this.charObj = undefined;

    this.state = {
      chartOpt: {
        type: 'line',
        data: {
          labels: [], datasets: [{
            type: "line",
            borderColor: "rgb(100, 255, 100)",
            lineTension: 0.2, data: [],
            pointBackgroundColor: []
          }]
        },
        bezierCurve: false,
        options: {
          scales: {
            xAxes: [{
              ticks: {
                callback: function (value, index, values) {
                  return value;
                }
              }
            }]
          },
          elements: {
            line: { fill: false },
            point: { radius: 6 }
          },
          bezierCurve: false,
          animation: {
            duration: 0
          },
          maintainAspectRatio: false,
          responsive: true,
          title: {
            display: true,
            text: ''
          },
          annotation: {
            annotations: []
          },
          legend: {
            display: false
          },
          tooltips: {
            enabled: true
          }
        }
      }
    };

    this.default_annotationTargets = [
      { type: "USL", color: "rgba(200, 0, 0,0.2)" },
      { type: "LSL", color: "rgba(200, 0, 0,0.2)" },
      { type: "UCL", color: "rgba(100, 100, 0,0.2)" },
      { type: "LCL", color: "rgba(100, 100, 0,0.2)" },
      { type: "value", color: "rgba(0, 0, 0,0.2)" },
    ];
  }


  componentWillUpdate(nextProps, nextState)
  {

  }

  updateChart(nextProps)
  {
    if(this.charObj===undefined)return;

    //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
    this.state.chartOpt.data.labels = [];
    this.state.chartOpt.data.datasets.forEach((datInfo) => {
      datInfo.data = [];
      datInfo.pointBackgroundColor = [];
    });
    let length = nextProps.reportArray.length;
    if (length == 0) return;
    let newTime = nextProps.reportArray[length - 1].time_ms;
    this.state.chartOpt.options.title.text = nextProps.reportArray[0].judgeReports.find((jrep) => jrep.id == nextProps.targetMeasure.id).name;

    nextProps.reportArray.reduce((acc_data, rep, idx) => {
      acc_data.labels.push((newTime - rep.time_ms) / 1000);

      let measureObj = rep.judgeReports.find((jrep) => jrep.id == nextProps.targetMeasure.id);

      let pointColor = undefined;
      let val = measureObj.value;
      pointColor = OK_NG_BOX_COLOR_TEXT[measureObj.detailStatus].COLOR;
      if (pointColor === undefined) {
        pointColor = "#AA0000";
      }
      //console.log(measureObj.detailStatus);
      if (measureObj.detailStatus === MEASURERSULTRESION.NA) {
        val = undefined;
      }



      acc_data.datasets[0].pointBackgroundColor.push(pointColor);
      //TODO:for now there is only one data set in one chart
      acc_data.datasets[0].data.push(val);
      return acc_data;
    }, this.state.chartOpt.data);



    let annotationTargets = this.props.nnotationTargets;
    if (annotationTargets === undefined) {
      annotationTargets = this.default_annotationTargets
    }

    this.state.chartOpt.options.annotation.annotations =
      annotationTargets.map((annotationTar) => {

        let val = nextProps.targetMeasure[annotationTar.type];
        return ({
          type: 'line',
          mode: 'horizontal',
          scaleID: 'y-axis-0',
          value: val,
          borderColor: annotationTar.color,
          borderWidth: 4,
          borderDash: [12, 12],
          label: {
            position: "right",
            enabled: true,
            content: val
          }
        });
      });


    this.charObj.update();
  }


  componentDidMount() {
    var ctx = document.getElementById(this.divID).getContext("2d");

    this.charObj = new Chart(ctx, this.state.chartOpt);
    this.updateChart(this.props);
  }
  onResize(width, height) {
    //log.debug("G2HOT resize>>", width, height);
    //this.state.G2Chart.changeSize(width, height);

  }

  render() {
    this.updateChart(this.props);
    return <div className={this.props.className}>
      <canvas id={this.divID} style={{ height: "400px" }} />
      <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
    </div>
  }

}



class DataStatsTable extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      drawList: []
    };
  }
  shouldComponentUpdate(nextProps,nextState) {
    
    if(nextState!=this.state)
    {
      return true;
    }

    let statstate = nextProps.reportStatisticState;
    if (statstate.historyReport === undefined) {
      return false;
    }
    let historyReport = statstate.historyReport;
    if (historyReport.length == 0) {
      return false;
    }
    if (historyReport.length > 0 && this.pre_last_rep == historyReport[historyReport.length - 1]) {
      return false;
    }
    this.pre_last_rep = historyReport[historyReport.length - 1];
    return true;

  }
  render() {
    let statstate = this.props.reportStatisticState;
    //console.log(statstate);
    if (statstate.statisticValue === undefined) {
      return null;
    }
    let measureList = statstate.statisticValue.measureList;

    // console.log(measureList);
    let measureReports = measureList.filter(m=>m.rank===undefined || m.rank<=this.props.measureDisplayRank).map((measure) =>
      ({
        id: measure.id,
        name: measure.name,
        subtype: measure.subtype,
        count: measure.statistic.count,
        mean: round(measure.statistic.mean, 0.001),
        sigma: round(measure.statistic.sigma, 0.0001),


        OK: ["UOK", "LOK", "UCNG", "LCNG"].reduce((sum, tag) => sum + measure.statistic.count_stat[tag], 0),
        NG: ["USNG", "LSNG"].reduce((sum, tag) => sum + measure.statistic.count_stat[tag], 0),
        NA: measure.statistic.count_stat.NA,
        WARN: ["UCNG", "LCNG"].reduce((sum, tag) => sum + measure.statistic.count_stat[tag], 0),

        CK: round(measure.statistic.CK, 0.001),
        // CPU:round(measure.statistic.CPU,0.001),
        // CPL:round(measure.statistic.CPL,0.001),
        CP: round(measure.statistic.CP, 0.001),
        CPK: round(measure.statistic.CPK, 0.001),
        MIN: round(measure.statistic.MIN, 0.001),
        MAX: round(measure.statistic.MAX, 0.001),
      })
    );

    if (measureReports.length == 0) return null;
    //log.error(measureReports);


    //statstate.historyReport.map((rep)=>rep.judgeReports[0]);
    const dataSource = measureReports;
    const columns = ["name", "subtype", "count", "mean", 
      "MIN", "MAX","OK", "NG", "CPK","sigma",
      "NA", "WARN",
      "CK", "CP",
      //"CPU","CPL",
      ]
      .map((type) => ({ title: type, dataIndex: type, key: type }))
      .map((col) => (typeof measureReports[0][col.title] == 'number') ?//Find the first dataset and if it's number then add a sorter
        Object.assign(col, { sorter: (a, b) => a[col.title] - b[col.title] }) : col)
    /*columns[0].fixed="left";
    columns[0].width=100;

    columns[columns.length-1].fixed="right";
    columns[columns.length-1].width=100;*/
    columns.push(
      {
        title: "Draw Toggle", key: "draw", fixed: "right",
        render: (text, record) => {
          return <Switch onChange={(val) => {
            // this.state.drawList[record.id] = val;
            let newDrawList = [...this.state.drawList];
            newDrawList[record.id]=val;
            this.setState({drawList:newDrawList});
            
            console.log("TRIGGER");
          }
          } />
        }
      }
    );

    let graphX = Object.keys(this.state.drawList).map((key, idx) => {
      if (this.state.drawList[key] == true) {
        let targetMeasure = measureList.find((m) => m.id == key);

        let lastN = 500;
        let lastNArr = statstate.historyReport.slice(Math.max(statstate.historyReport.length - lastN, 1));
        return <ControlChart reportArray={lastNArr} targetMeasure={targetMeasure} />
      }
      return null;
    });
    //console.log(graphX);

    return (
      <div className={this.props.className}>
        <Table key="dat_table" className="antd-table-small" dataSource={dataSource} columns={columns} size="small" scroll={{ x: 1000 }} pagination={false} />
        {graphX}
      </div>
    );
  }
}

let skip_counter = 0;

class AngledCalibrationHelper extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      target_line: undefined,
      target_measure: undefined,
      target_spoint: undefined,
      angledOffsetTable: {},
      angleStatTable: [],
      updateCount: 0,
      shape_list: undefined,
      camera_calibration_report: undefined,//just for comparison
      mmpp: -1
    };


    for (let i = 0; i < 6; i++) {
      this.state.angleStatTable.push({
        w: 0,
        value: 0
      });
    }
  }

  static getDerivedStateFromProps(props, state) {
    let newState = state;
    if (newState.shapeList === undefined) {
      let shape_list = dclone(props.shape_list);
      newState = { ...newState, shape_list };
    }

    if (newState.shape_list === undefined) return newState;


    if (props.camera_calibration_report != state.camera_calibration_report) {
      let rep = props.camera_calibration_report.reports[0];
      let mmpp = rep.mmpb2b / rep.ppb2b;
      newState = {
        ...newState
        , camera_calibration_report: props.camera_calibration_report
        , mmpp
      };
    }


    if (
      newState.target_measure !== undefined &&
      newState.target_line !== undefined
      && props.reportStatisticState.newAddedReport !== undefined
      && props.reportStatisticState.newAddedReport.length !== 0) {
      let historyReport = props.reportStatisticState.newAddedReport;
      //console.log(historyReport);
      historyReport.forEach(rep => {


        let measure = rep.judgeReports.find(jrep => jrep.id == state.target_measure.id);
        //let spoint=rep.searchPoints.find(jrep=>jrep.id==state.target_spoint.id);
        let line = rep.detectedLines.find(jrep => jrep.id == state.target_line.id);



        let ang =
          Math.atan2(line.vy, line.vx) * 180 / Math.PI
          + 90;

        if (state.target_spoint !== undefined) {
          ang += state.target_spoint.angleDeg - 180;
        }
        //if(ang<0)ang+=360;


        function fmod(a, b) {
          return a - (Math.floor(a / b) * b);
        }
        ang = fmod(ang, 360);

        let pixOffset = measure.value / newState.mmpp;

        function addStatValue(id, portion, value) {
          let tab = newState.angleStatTable[id];
          if (tab === undefined) return;
          tab.w += portion;
          tab.value += portion * value;
        }

        function AddAngleOffsetRec(angleDeg, pixOffset) {
          angleDeg = fmod(angleDeg, 360);
          let section_f = angleDeg / (360 / newState.angleStatTable.length);
          let section = Math.floor(section_f);
          let portion = 1 - (section_f - section);

          let lowID = section;
          let highID = (section + 1) % newState.angleStatTable.length;

          addStatValue(lowID, portion, pixOffset);
          addStatValue(highID, 1 - portion, pixOffset);


        }

        //console.log(ang,pixOffset);
        AddAngleOffsetRec(ang, pixOffset);
        AddAngleOffsetRec(ang + 180, pixOffset);


      })

      if ((skip_counter++) % 10 == 0) {
        let angleStatTable = dclone(newState.angleStatTable);

        //averaging
        if (true) {
          for (let i = 0; i < angleStatTable.length; i++) {
            let i_warp = i + angleStatTable.length;
            let pre_stat_1 = newState.angleStatTable[(i_warp - 1) % angleStatTable.length];
            let pre_stat_2 = newState.angleStatTable[(i_warp - 0) % angleStatTable.length];
            let pre_stat_3 = newState.angleStatTable[(i_warp + 1) % angleStatTable.length];

            let new_stat = angleStatTable[i];
            let beta = 4;
            new_stat.w = (pre_stat_1.w + beta * pre_stat_2.w + pre_stat_3.w) / (2 + beta);
            new_stat.value = (pre_stat_1.value + beta * pre_stat_2.value + pre_stat_3.value) / (2 + beta);
          }
        }

        let mean = angleStatTable.reduce((sum, stat) => sum + stat.value / (stat.w + 0.0001), 0) / angleStatTable.length;

        let offsetMap = angleStatTable.map(stat => stat.value / (stat.w + 0.0001) - mean);
        let tableX = {};
        offsetMap.forEach((ele, idx) => {
          let key = "" + 360 * idx / offsetMap.length;
          tableX[key] = -round(ele, 0.00001)
        });
        console.log(JSON.stringify(tableX, null, 4));
        // angledOffsetTable
        let updateCount = (newState.updateCount === undefined) ? 0 : newState.updateCount + 1;
        newState = {
          ...newState
          , angledOffsetTable: tableX
          , updateCount
        };
      }
    }

    return newState;
  }

  render() {


    let measure_dist_list = this.state.shape_list
      .filter(shape => shape.type == "measure" && shape.subtype == "distance")


    const menu = (
      <Menu
        onClick={(ev) => {
          if (ev.key == -1) {

            this.setState({
              target_measure: undefined, target_line: undefined, target_spoint: undefined,
              angledOffsetTable: {}, updateCount: 0
            })
            return;
          }
          let slist = this.state.shape_list;
          let target_measure = slist.find(shape => shape.id == ev.key);
          let middleObject = target_measure;

          let ref_id = middleObject.ref[0].id;
          middleObject = slist.find(shape => shape.id == ref_id);
          let target_spoint = undefined;
          let target_line = middleObject;
          console.log(middleObject, ref_id);

          if (middleObject.type == "search_point") {
            target_spoint = middleObject;
            ref_id = middleObject.ref[0].id;
            middleObject = slist.find(shape => shape.id == ref_id);
            target_line = middleObject;
          }


          let angleStatTable = dclone(this.state.angleStatTable);
          angleStatTable.forEach(st => { st.w = st.value = 0; })
          this.setState({
            target_measure, target_line, target_spoint,
            angledOffsetTable: {}, angleStatTable, updateCount: 0
          })
        }}>
        {measure_dist_list.concat([{ id: -1, name: "CANCEL" }])
          .map((mea, idx) =>
            <Menu.Item key={mea.id}>
              <a target="_blank" rel="noopener noreferrer">
                {mea.id + ":  " + mea.name}
              </a>
            </Menu.Item>)}
      </Menu>
    );



    let displayDropDown = <Dropdown overlay={menu}>
      <a className="ant-dropdown-link HX1" href="#">
        {this.state.target_measure === undefined ?
          "SELECT" :
          ">>>" + this.state.target_measure.name}
        <CaretDownOutlined />
      </a>
    </Dropdown>
    return <div className={this.props.className}>
      <div className="s HXA width12" style={{ padding: "10px" }}>
        <Divider orientation="left" key="divi">Angled Calib</Divider>
        {displayDropDown}
        <Button type="primary" icon="download"
          onClick={() => { copyToClipboard(JSON.stringify(this.state.angledOffsetTable)) }}>
          {this.state.updateCount}
        </Button>

      </div>

    </div>
  }

}

function MeasureRankEdit ({shape_list,info_decorator,initRank=0,onRankChange}){
      
  const [sliderV,setSliderV]=useState(0);
  let measureList = shape_list
    .filter(shape=>shape.type===UIAct.SHAPE_TYPE.measure);
  let rankMax=measureList.reduce((max,m)=>max<m.rank?m.rank:max,0);
  let rankMin=measureList.reduce((min,m)=>min>m.rank?m.rank:min,0);
  console.log(shape_list,"----",info_decorator);
  // console.log(rankMin,"----",rankMax);
  if(rankMin==1000)
  {
    rankMin=0;
    rankMax=0;
  }
  if(rankMin==rankMax)
  {
    rankMin=0;
  }
  let marks = {};


  marks[rankMin]=rankMin+"";
  marks[rankMax]=rankMax+"";
  measureList.forEach((m)=>{
    if(m.rank!==undefined)
    {
      marks[m.rank]=m.rank+"";
    }
  });


  useEffect(()=>{
    setSliderV(initRank);
    
  },[])
  useEffect(()=>{
    if(onRankChange!==undefined)
    {
      onRankChange(sliderV);
    }
  },[sliderV])

  return <>
  
    <Slider
      min={rankMin}
      max={rankMax}
      onChange={setSliderV}
      marks={marks}
      value={sliderV}
    />
    {measureList.filter(m=>m.rank<=sliderV||m.rank===undefined).map(m=><Tag>{m.name}</Tag>)}
  
  </>;
}



function RestrictiveCircleREdit ({initR,onRChanged}){
      
  let rankMin=0;
  let rankMax=1000;
  const [sliderV,setSliderV]=useState(initR===undefined?rankMax:initR);

  
  useEffect(()=>{
    if(onRChanged!==undefined)
    {
      onRChanged(sliderV);
    }
  },[sliderV])

  return <>
  
    <Slider
      step={10}
      min={rankMin}
      max={rankMax}
      onChange={setSliderV}
      value={sliderV}
    />
  </>;
}


class APP_INSP_MODE extends React.Component {

  
  componentDidMount() {


    this.CameraCtrl.setCameraImageTransfer(true);
    
    this.CameraCtrl.setImageCropParam(undefined,4);

    {
      console.log(this.props.shape_list,this.props.info_decorator,this.props.inspOptionalTag);


      let ctrlMarginInfos=GetObjElement(this.props.info_decorator,["control_margin_info"]);

      if(ctrlMarginInfos!==undefined)
      {
        let curMarginInfo_name=this.props.inspOptionalTag.find(tag=>ctrlMarginInfos[tag]!==undefined)//find the tag name that is in ctrlMarginInfo
        let curMarginInfo=ctrlMarginInfos[curMarginInfo_name];
        console.log(curMarginInfo_name,curMarginInfo);

        if(curMarginInfo!==undefined)
        {
          let newShapeList = [...this.props.shape_list];
          curMarginInfo.forEach(info=>{
            let cur_shape_idx = newShapeList.findIndex(shape=>shape.id==info.id);
            if(cur_shape_idx!==-1)
            {
              newShapeList[cur_shape_idx]=
              {
                ...newShapeList[cur_shape_idx],
                ...info
              }
            }


          });
          this.props.ACT_Shape_List_Update(newShapeList);
          console.log(newShapeList)
        }

      }

    }



    let deffile = defFileGeneration(this.props.edit_info);
    if (this.props.machine_custom_setting.InspectionMode== "FI") {

      
      //deffile.intrusionSizeLimitRatio=0.001;//By default, the intrusionSizeLimitRatio for Full insp should be as small as possible
      deffile.featureSet[0].matching_angle_margin_deg=180;//By default, match whole round -180~180
      deffile.featureSet[0].matching_face=0;//By default, match two sides

      this.props.ACT_WS_SEND_CORE_BPG( "FI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: deffile}, undefined);

      this.props.ACT_StatSettingParam_Update(this.props.System_Setting.FI_MODE_StatSettingParam)
    }
    else if (this.props.machine_custom_setting.InspectionMode == "CI") {
      
      // deffile.featureSet[0].single_result_area_ratio=0.9;
      this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: deffile     
       }, undefined);


      // this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
      // { CameraSetting: { down_samp_w_calib:false } });

      // this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: {
      //   type:"gen"
      // }
      // }, undefined);

      this.props.ACT_StatSettingParam_Update(this.props.System_Setting.CI_MODE_StatSettingParam)

    }

    this.exitGate=false;
  }

  componentWillUnmount() {
    this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: false } });

  }

  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.checkResult2AirAction = { direction: "none", ver: 0 };
    this.state = {
      GraphUIDisplayMode: 0,
      CanvasWindowRatio: 9,
      onROISettingCallBack:undefined,
      measureDisplayRank:0,
      isInSettingUI:false,
      SettingParamInfo:undefined,
      modalInfo:undefined,
      renderObjAlignRotate:false
    };

    

    new Promise((resolve, reject) => {
      this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
      { 
        InspectionParam:[{
          get_param:true
        }]
      },undefined, { resolve, reject })

    }).then((pkts) => {
      let DT=pkts.find(pkt=>pkt.type=="DT");
      console.log("-------DT",DT,"   pkts:",pkts);
      if(DT!==undefined && DT.data!==undefined&& DT.data[0]!==undefined)
      {
        this.setState({SettingParamInfo:DT.data[0]});
      }
      else
      {
        this.setState({SettingParamInfo:undefined});
      }
    })


    this.CameraCtrl = new CameraCtrl({
      ws_ch: (STData, promiseCBs) => {
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0, STData, undefined, promiseCBs)
      },
      ev_speedModeChange: (mode) => {
        console.log(mode);
      }
    });
    // this.IR = undefined;



    // new Promise((resolve, reject) => {
    //   this.props.ACT_WS_SEND_CORE_BPG( "LD", 0,
    //     { filename: "data/default_camera_setting.json" },
    //     undefined, { resolve, reject }
    //   );
    //   setTimeout(() => reject("Timeout"), 2000)
    // }).then((pkts) => {
    //   if (pkts[0].type != "FL") return;
    //   let cam_setting = pkts[0].data;
    //   if (typeof cam_setting.ROIs !== 'object') return;
    //   let ROIs = cam_setting.ROIs;
    //   // console.log(">>>>", ROIs);
    //   let down_samp_factor =cam_setting.down_samp_factor===undefined? 1:cam_setting.down_samp_factor;
    //   this.setState({ ROIs, ROI_key: undefined,down_samp_factor});
    // }).catch((err) => { })





  }

  EXIT()
  {
    if(this.exitGate==false)
    {//prevent double exit
      this.exitGate=true;
      this.props.ACT_EXIT();
    }
  }

  componentDidUpdate() {
    this.CameraCtrl.updateInspectionReport(this.props.inspectionReport);

    if(this.props.uInsp_API_ID_CONN_INFO!==undefined)
    {
      if(this.props.uInsp_API_ID_CONN_INFO.type!=="WS_CONNECTED")
      {
        this.EXIT();
      }
    }
    if(this.props.CAM1_ID_CONN_INFO!==undefined)
    {
      if(this.props.CAM1_ID_CONN_INFO.type!=="WS_CONNECTED")
      {
        this.EXIT();
      }
    }
  }
  shouldComponentUpdate(nextProps, nextState)
  {
    let pre_reportCount=nextProps.reportStatisticState.reportCount;
    let isReportInc=pre_reportCount!==this.pre_reportCount
    this.pre_reportCount=pre_reportCount;
    if(this.state!==nextState){
      return true;
    }
    // console.log("///",isReportInc);
    return isReportInc;
  }
  setInspectionRankUI()
  {
    this.setState({
      additionalUI: [
        <Modal
        title={""}
        visible={true}
        onOk={() => {
          this.setState({ additionalUI: [] });
          this.props.EV_UI_inspMode();
        }}
        onCancel={() => {
          this.setState({ additionalUI: [] });
        }}
      >
        
        
        <Divider orientation="left" key="div1">檢測等級</Divider>

        <MeasureRankEdit shape_list={this.props.shape_list} 
          info_decorator = {this.props.info_decorator}
          initRank={this.state.measureDisplayRank}
          onRankChange={(rank)=>{
            this.setState({measureDisplayRank: rank });
          }}/>
        
        <Divider orientation="left" key="div2"></Divider>

        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {
            this.props.ACT_StatInfo_Clear();
          }} >清空統計數據</Button>
        <br/>
        {/* SAVE:
        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              INSP_NG_SNAP:true
            })

          }} >O</Button>
          
        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              INSP_NG_SNAP:false
            })

          }} >X</Button> */}
      </Modal>]
    })
  }


  MatchingEnginParamSet(key,value)
  {
    this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
              { 
                InspectionParam:[{
                  [key]:value
                }]
              })
  }

  notifyPopUp(title,msg)
  {
    this.setState({
      modalInfo:{
        title:title,
        onOk:()=>this.setState({modalInfo:undefined}),
        onCancel:()=>this.setState({modalInfo:undefined}),
        footer:null,
        children:msg
      }
    })
  }
  warnPopUp(msg)
  {
    this.notifyPopUp("警告",msg)
  }
  render() {
    
    let inspectionReport = undefined;
    if (this.props.inspectionReport !== undefined) {
      inspectionReport = this.props.inspectionReport;
      if (inspectionReport.reports.length > 0) {
        let groupResult = inspectionReport.reports.map((single_rep) => {

          // [1,2,3,4].reduce((sum,ele)=>{return sum+ele},0);
          // [1,2,3,4].map((ele)=>2*ele);

          let judgeReports = single_rep.judgeReports;
          let ret_status = judgeReports.reduce((res, obj) => {
            if (res == INSPECTION_STATUS.NA) return res;
            if (res == INSPECTION_STATUS.FAILURE) {
              if (obj.status == INSPECTION_STATUS.NA)
                return INSPECTION_STATUS.NA;
              return res;
            }
            return obj.status;
          }
            , INSPECTION_STATUS.SUCCESS);
          return ret_status;
        });

        let ret_status = groupResult.reduce((gresult, result) => {
          if (gresult === undefined)
            return result;

          if (gresult == INSPECTION_STATUS.NA || result == INSPECTION_STATUS.NA)
            return INSPECTION_STATUS.NA;

          if (gresult != result)
            return INSPECTION_STATUS.NA;

          return result;
        }, undefined);

        // if (ret_status == INSPECTION_STATUS.SUCCESS) {
        //   this.checkResult2AirAction = { direction: "right", ver: this.checkResult2AirAction.ver + 1 };
        // } else if (ret_status == INSPECTION_STATUS.FAILURE) {
        //   this.checkResult2AirAction = { direction: "left", ver: this.checkResult2AirAction.ver + 1 };
        // } else {
        //   //log.error("result NA...");
        // }
        //
      }
    }
    let MenuSet = [];
    let menu_height = "HXA";//auto
    log.debug("CanvasComponent render");
    let CanvasWindowRatio = 12;
    let menuOpacity = 1;

    // let MenuSet_2nd = [];



    let onTagEdit = () =>
      this.setState({
        additionalUI: [
          <Modal
            title={"警告"}
            visible={true}
            onOk={() => {
              this.setState({ additionalUI: [] });
              this.props.EV_UI_inspMode();
            }}
            onCancel={() => {
              this.setState({ additionalUI: [] });
            }}
          >
            <div style={{ height: "500px" }}>
              <TagOptions_rdx className="s width12 HXA" />
            </div>
          </Modal>
        ]
      });

    let maxTextLength=20;
    let text_more="...";
    let shortedModelName=this.props.defModelName.length<(maxTextLength+text_more.length)?
      this.props.defModelName:
      this.props.defModelName.substring(0, maxTextLength)+text_more
    //console.log(">>>>defModelName>>>>>"+this.props.defModelName);
    MenuSet = [

    ];

    // MenuSet.push(
      
    // );

    
    MenuSet.push(

    //   <Button type="primary" icon={<SearchOutlined />}>
    //   Search
    // </Button>
      <Button
        icon={<SaveOutlined />}
        key="SVX"
        style={{width:"100%"}}
        type="primary"
        onClick={() =>{



          

          let curList = this.props.reportStatisticState.trackingWindow.filter(rep=>rep.isCurObj==true);

          
          let tag_str = (curList.length==0)?"":curList[0].tag;

          this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
          { stacking_count:5, type: "__START_STACKING_IMG__"},undefined,
          {
            resolve:(pkts,action_ch)=>{
              console.log(pkts);
            },
            reject:(e)=>{

              console.log(e);
            }
          })

          let default_dst_Path=this.props.machine_custom_setting.Sample_Saving_Path;
          
          if(default_dst_Path===undefined)
          {
            default_dst_Path="data"
          }

          let targetName=this.props.edit_info.DefFileName+"-["+tag_str+"]-"+dateFormat(new Date(), "yyyymmdd-hh:mm:ss_l");
          this.setState({
            modalInfo:{
              title:"快照命名",
              onOk:()=>{

                this.setState({
                  modalInfo:{...this.state.modalInfo,confirmLoading:true}})

                
                let name = this.state.modalInfo.targetName;
                let path_name = default_dst_Path+"/"+name;
                
                this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
                { filename: path_name+".png",make_dir:true, type: "__STACKING_IMG__" },undefined,
                {
                  resolve:(pkts,action_ch)=>{

                    
                    let SS=pkts.find(pkt=>pkt.type=="SS");
                    console.log(SS)
                    if(SS.data.ACK==true)
                    {
                      let deffile = defFileGeneration(this.props.edit_info);

                      let reportSave = {
                        reports:JSON.parse(JSON.stringify(curList,(key, val) => val.toFixed ? Number(val.toFixed(6)) : val  )),
                        defInfo:deffile,
                        camera_param:this.props.edit_info._obj.cameraParam
                      }
                      var enc = new TextEncoder();

                      
                
          
                      this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
                      { filename: path_name+".xreps" },enc.encode(JSON.stringify(reportSave)),
                      {
                        resolve:(pkts,action_ch)=>{
                          let SS=pkts.find(pkt=>pkt.type=="SS");
                          if(SS.data.ACK==true)
                          {
                            // this.setState({modalInfo:undefined})

                            // this.notifyPopUp(null,`儲存快照  ${ path_name }  成功`);
                            
                            this.setState({
                              modalInfo:{...this.state.modalInfo,confirmLoading:false,onOk:_=>_,onCancel:_=>_,okText:"存檔成功"}})
                            
                            setTimeout(()=>{//close after 1s
                              this.setState({modalInfo:undefined})
                            },1000);

                          }
                          else
                          {
                            this.warnPopUp(`儲存檔案  ${ path_name+".xreps" }   失敗`);
                          }
                          // 
                          
                        },
                        reject:(e)=>{
                          this.warnPopUp(`儲存檔案  ${ path_name+".xreps" }   失敗`);
                          // this.setState({modalInfo:undefined})
                        }
                      }
                      
                      )
                    }
                    else
                    {
                      this.warnPopUp(`儲存圖像  ${ path_name+".png" }   失敗`);
                    }


                    console.log(pkts);
                  },
                  reject:(e)=>{
      
                    this.warnPopUp(`儲存圖像  ${ path_name+".png" }   失敗`);
                    console.log(e);
                  }
                })


              },
              onCancel:()=>this.setState({modalInfo:undefined}),

              targetName:targetName,
              children:(modalInfo)=><>
              路徑:{default_dst_Path}<br/>
              名稱:
              <Input size="small"
                value={modalInfo.targetName} 
                onChange={(ev)=> this.setState({
                  modalInfo:{...modalInfo,targetName:ev.target.value}})}
              />
              
              </>
            }
          })
          return;
        }} >檢測快照</Button>);
        


    {//if the FLAGS.CI_INSP_SEND_REP_TO_DB_SKIP is undefined it will use the default number
    }
    let InspectionReportPullSkip=(this.props.machine_custom_setting.InspectionMode == "CI") ? 
    this.props.System_Setting.CI_MODE_UPLOAD_SKIP:this.props.System_Setting.FI_MODE_UPLOAD_SKIP;
    // console.log(this.props.inspMode,InspectionReportPullSkip);
    if(!this.state.isInSettingUI)
    {

      let trackingWindowInfo = this.props.reportStatisticState.trackingWindow;
      //console.log(">>>>>>inspection_db_ws_url:",this.props.machine_custom_setting);
      MenuSet.push(
        <ObjInfoList
          IR={trackingWindowInfo}
          DICT={this.props.DICT}
          measureDisplayRank={this.state.measureDisplayRank}
          IR_decotrator={this.props.info_decorator}
          checkResult2AirAction={this.checkResult2AirAction}
          uInsp_peripheral_conn_info={this.props.machine_custom_setting.uInsp_peripheral_conn_info}
          shape_def={this.props.shape_list}
          key="ObjInfoList"
          uInsp_API_ID_CONN_INFO={this.props.uInsp_API_ID_CONN_INFO}
          WSCMD_CB={(tl, prop, data, uintArr) => { this.props.ACT_WS_SEND_CORE_BPG( tl, prop, data, uintArr); }}
        />);
    }
    else
    {
      if(this.state.SettingParamInfo!==undefined)
      {
        let paramSet = this.state.SettingParamInfo;
        //["HFrom","HTo","VMax","VMin","SMax","SMin","boxFilter1_Size","boxFilter1_thres","boxFilter2_Size","boxFilter2_thres"]
        MenuSet.push(
          Object.keys(paramSet).map(key=>[
          <Divider orientation="left" key={key+"_div"}>{key+":"+paramSet[key]}</Divider>,
          
          <Slider key={key+"_slider"}
            className="layout width12"
            min={0}
            max={255}
            onChange={(value) => {
              if(paramSet[key]===undefined)return;
              this.MatchingEnginParamSet(key,value);

              this.setState({SettingParamInfo:{...paramSet,[key]:value}});
              
            }}
            value={paramSet[key]}
            step={1}
          />
          ])
        
        );
      }
    }
    switch (this.state.GraphUIDisplayMode) {
      case 0:
        CanvasWindowRatio = 12;
        menuOpacity = 1;
        break;

      case 1:
        CanvasWindowRatio = 4;
        menuOpacity = 0.3;
        break;

      case 2:
        CanvasWindowRatio = 0;
        menuOpacity = 0.3;
        break;

      case 3:
        CanvasWindowRatio = 3;
        menuOpacity = 0.3;
        break;
    }



    let headerUI = 
    <>
      
      <Button type="primary" size={"large"} onClick={()=>this.EXIT()}>
        <ArrowLeftOutlined />
      </Button>

      <Popover content={<div>{this.props.defModelName}<br />{this.props.defModelPath} </div>} placement="bottomLeft" trigger="click">
        <span style={{margin:"10px"}} ><FileOutlined /> {shortedModelName}</span>
      </Popover>
      <TagDisplay_rdx size="middle"/>
      
      <Tag className="large" color="gray" onClick={() =>{
            this.setInspectionRankUI()
          }}><SettingOutlined /></Tag>
      {this.state.additionalUI}




      
      {/* <Button type="primary" size={"large"} 
      className={ ((this.state.DB_Conn_state == 1) ? "blackText lgreen" : "DISCONNECT_Blink")}
      icon={this.state.DB_Conn_state == 1 ? <LinkOutlined /> : <DisconnectOutlined />} >
          {(this.state.DB_Conn_state == 1 ? this.props.DICT.connection.server_connected : this.props.DICT.connection.server_disconnected)
          + " " + this.state.inspUploadedCount + ":" + this.props.reportStatisticState.historyReport.length + "/" + InspectionReportPullSkip}
      </Button> */}


      <InspectionReportInsert2DB 
        // newAddedReport={this.props.reportStatisticState.newAddedReport} 
        reportStatisticState={this.props.reportStatisticState}
        LANG_DICT={this.props.DICT}
        // DBStatus,
        // DBPushPromise,
        onDBInsertSuccess={(data, info) => {
          // log.info(data, info);
        }}
        onDBInsertFail={(data, info) => {
          log.error(data, info);
        }}
        insert_skip={InspectionReportPullSkip}/>



      <Button size={"large"} type={this.state.renderObjAlignRotate==true?"primary":"dashed"} onClick={()=>this.setState({renderObjAlignRotate:!this.state.renderObjAlignRotate})}>
        <RedoOutlined/>
        {this.state.renderObjAlignRotate==true?"旋轉標的":"不轉原圖"}
      </Button>






{/* 
      <Checkbox  checked={this.CameraCtrl.data.DoImageTransfer}
      onChange={(ev)=>
          {
            this.CameraCtrl.setCameraImageTransfer(ev.target.checked);
            this.setState({});//just to kick update
          }
        } >{
          "相機影像更新"
        }</Checkbox> */}
      
      <Button type="primary" key="Info Graphs" size={"large"} icon={<BarChartOutlined />}
      onClick={() => {
        this.state.GraphUIDisplayMode = (this.state.GraphUIDisplayMode + 1) % 3;
        this.setState(Object.assign({}, this.state));
      }}
      >資料圖表</Button>

      <Button type={"primary"} danger={this.state.onROISettingCallBack!==undefined} key="Manual ZOOM" size={"large"}
        onClick={() => {
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
        { CameraSetting: { ROI:[0,0,99999,99999] } });
        this.setState({ onROISettingCallBack:(ROI_setting)=>{
          
          let x = ROI_setting.start.pix.x;
          let y = ROI_setting.start.pix.y;
          
          let w = ROI_setting.end.pix.x-x;
          let h = ROI_setting.end.pix.y-y;
          if(w<0)
          {
            x+=w;
            w=-w;
          }
          if(h<0)
          {
            y+=h;
            h=-h;
          }
          let ROI = [x,y,w,h];
          this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
          {CameraSetting: { ROI}});

        
          console.log(ROI_setting,ROI);
          this.setState({onROISettingCallBack:undefined});
        }})
      }} ><ExpandOutlined />
        {this.state.onROISettingCallBack===undefined?"設定ROI":"選擇ROI中"}</Button>
    </>

/*
    </>;*/


    // console.log(this.props.FILE_default_camera_setting)

    // MenuSet_2nd.push(<AngledCalibrationHelper className="s width12 HXA"
    //   reportStatisticState={this.props.reportStatisticState} shape_list={this.props.shape_list}
    //   camera_calibration_report={this.props.camera_calibration_report} />);

    return (
      <div className="HXF flex_section">
        <div className="flex_header">{headerUI}</div>



        <div className="flex_div">
          {/* <$CSSTG transitionName="fadeIn"> */}
            <div key={"MENU"} className={"s overlay shadow1 scroll MenuAnim " + menu_height}
              style={{ opacity: menuOpacity, width: "250px" }}>
              {MenuSet}
            </div>
          {/* </$CSSTG> */}

          {(CanvasWindowRatio <= 0) ? null :
            <CanvasComponent_rdx addClass={"layout WXF " + " height" + CanvasWindowRatio}
              onROISettingCallBack={this.state.onROISettingCallBack}
              measureDisplayRank={this.state.measureDisplayRank}
              ACT_WS_SEND_CORE_BPG={this.props.ACT_WS_SEND_CORE_BPG}
              downSampleFactor={this.props.FILE_default_camera_setting.down_samp_factor||1}
              onCanvasInit={(canvas) => { this.ec_canvas = canvas }}
              renderObjAlignRotate={this.state.renderObjAlignRotate}
              camera_calibration_report={this.props.camera_calibration_report} />}

          {(CanvasWindowRatio >= 12) ? null :
            <DataStatsTable className={"s scroll WXF" + " height" + (12 - CanvasWindowRatio)}
              reportStatisticState={this.props.reportStatisticState} measureDisplayRank={this.state.measureDisplayRank}/>}



        </div>
        <Modal {...this.state.modalInfo} visible={this.state.modalInfo!==undefined}> 
          {this.state.modalInfo===undefined?null:
            ((typeof this.state.modalInfo.children === 'function')?
            this.state.modalInfo.children(this.state.modalInfo):
            this.state.modalInfo.children)
          }
        </Modal>
        
        <>  

          {/* <Menu
            // onClick={this.handleClick}
            // selectedKeys={[this.current]}
            selectable={true}
            // style={{align: 'left', width: 200}}
            defaultSelectedKeys={['functionMenu']}
            // defaultOpenKeys={['functionMenu']}
            mode="inline">

            <SubMenu key="sss"
              className="s overlay overlayright scroll HXA WXA"
              style={{ color: '#333' }}
              title={<SettingOutlined />}>

              <div key={"MENU"}
                style={{ width: "250px" }}>
                {MenuSet_2nd}
              </div>

            </SubMenu>


          </Menu> */}
        </>
      
      </div>
    );
  }
}



const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps,ff) => {
  return {
    ACT_EXIT: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
    },
    ACT_WS_SEND_BPG: (id,tl, prop, data, uintArr, promiseCBs) => 
      dispatch(UIAct.EV_WS_SEND_BPG(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_StatSettingParam_Update: (arg) => dispatch(UIAct.EV_StatSettingParam_Update(arg)),
    ACT_StatInfo_Clear:()=>dispatch(UIAct.EV_StatInfo_Clear()),
    
    ACT_Shape_List_Update:(newlist)=>dispatch(DefConfAct.Shape_List_Update(newlist)),
  }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
  return {
    
    edit_info :state.UIData.edit_info,
    c_state: state.UIData.c_state,
    shape_list: state.UIData.edit_info.list,
    info_decorator: state.UIData.edit_info.__decorator,
    defModelName: state.UIData.edit_info.DefFileName,
    FILE_default_camera_setting:state.UIData.FILE_default_camera_setting,
    
    defModelTag: state.UIData.edit_info.DefFileTag,
    machine_custom_setting: state.UIData.machine_custom_setting,
    machTag: state.UIData.MachTag,
    inspOptionalTag: state.UIData.edit_info.inspOptionalTag,
    defModelPath: state.UIData.edit_info.defModelPath,
    CORE_ID: state.ConnInfo.CORE_ID,
    WS_InspDataBase_W_ID: state.UIData.WS_InspDataBase_W_ID,
    inspectionReport: state.UIData.edit_info.inspReport,
    reportStatisticState: state.UIData.edit_info.reportStatisticState,
    
    uInsp_API_ID_CONN_INFO:state.ConnInfo.uInsp_API_ID_CONN_INFO,
    CAM1_ID_CONN_INFO:state.ConnInfo.CAM1_ID_CONN_INFO,
    
    camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
    DICT:state.UIData.DICT,
    
    System_Setting:state.UIData.System_Setting,
  }
};


const mergeProps_APP_INSP_MODE = (ownProps, mapProps, dispatchProps) => {
  // console.log(ownProps, mapProps, dispatchProps);
  return ({
    ...ownProps,
    ...mapProps,
    ...dispatchProps,
    ACT_WS_SEND_CORE_BPG: (tl, prop, data, uintArr, promiseCBs) => 
      mapProps.ACT_WS_SEND_BPG(ownProps.CORE_ID, tl, prop, data, uintArr, promiseCBs)
  })
}

const APP_INSP_MODE_rdx = connect(
  mapStateToProps_APP_INSP_MODE,
  mapDispatchToProps_APP_INSP_MODE,
  mergeProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;