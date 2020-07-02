'use strict'


import { connect } from 'react-redux';
import React from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import { TagOptions_rdx } from './component/rdxComponent.jsx';
import dclone from 'clone';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { websocket_autoReconnect, websocket_reqTrack, copyToClipboard, ConsumeQueue } from 'UTIL/MISC_Util';
import EC_zh_TW from "./languages/zh_TW";
import { SHAPE_TYPE, DEFAULT_UNIT } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'REDUX_STORE_SRC/reducer/InspectionEditorLogic';
import { INSPECTION_STATUS, DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import * as logX from 'loglevel';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {TagDisplay_rdx} from './component/rdxComponent.jsx';
//import Plot from 'react-plotly.js';
//import {Doughnut} from 'react-chartjs-2';

import { round } from 'UTIL/MISC_Util';
let localStorage = require('localStorage');
let log = logX.getLogger("InspectionUI");

import Row from 'antd/lib/Row';
import Col from 'antd/lib/Col';
import Slider from 'antd/lib/Slider';

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
  TagsOutlined,
  HeartTwoTone,
  ArrowLeftOutlined,
  FullscreenOutlined,
  PaperClipOutlined,
  SettingOutlined,
  CaretDownOutlined,

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


let _DICT_=EC_zh_TW;
// import Tag from 'antd/lib/tag';
// import Select from 'antd/lib/select';
// import Menu from 'antd/lib/menu';
// import Button from 'antd/lib/button';
// import Icon from 'antd/lib/icon';


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

class RAW_InspectionReportPull extends React.Component {
  constructor(props) {
    super(props);
    this.state = {

    }
    this.pull_skip_count = 0;
    this.WS_DB_Inser = undefined;
    this.WS_DB_Query = undefined;
    this.retryQCount = 0;

    this.cQ = new ConsumeQueue((cQ) => {
      return new Promise((resolve, reject) => {//Implement consume rules
        if (this.WS_DB_Insert === undefined ||
          this.WS_DB_Insert.readyState !== WebSocket.OPEN ||
          cQ.size() == 0
        ) {//If no 
          reject();
          if (this.props.onDBInsertFail !== undefined)
            this.props.onDBInsertFail(undefined, "DB/Connection issue/Data empty");
          return;
        }


        let data = cQ.deQ();//get the latest element
        if (data === undefined)//try next data
        {
          resolve();

          if (this.props.onDBInsertFail !== undefined)
            this.props.onDBInsertFail(undefined, "Data empty");
          return;
        }
        var msg_obj = {
          dbcmd: { "db_action": "insert", "checked": true },
          data
        };
        let timeoutFlag = setTimeout(() => {
          timeoutFlag = undefined;
          console.log("consumeQueue>>timeout");
          reject("Timeout");
          if (this.props.onDBInsertFail !== undefined)
            this.props.onDBInsertFail(data, "Timeout");
        }, 3000);

        //The second param is replacer for stringify, and we replace any value that has toFixed(basically 'Number') to replace it to toFixed(5)
        this.WS_DB_Insert.send_obj(msg_obj, (key, val) => val.toFixed ? Number(val.toFixed(5)) : val).
          then((ret) => {
            clearTimeout(timeoutFlag);
            this.retryQCount = 0;
            resolve();
            this.props.onDBInsertSuccess(data, ret);
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
              cQ.enQ(data);//failed.... put back
              resolve();
            }

            if (this.props.onDBInsertFail !== undefined)
              this.props.onDBInsertFail(data, e);
          });
      })
    });
  }
  componentWillUnmount() {

    this.websocketClose();
  }
  componentWillMount() {

    this.websocketConnect(this.props.url);

  }

  componentDidUpdate(prevProps, prevState, snapshot) {
    if (this.props.reportStatisticState.newAddedReport.length > 0) {
      if (this.pull_skip_count == 0) {
        // let x=this.props.reportStatisticState.newAddedReport.map(e=>e);
        let x = this.props.reportStatisticState.newAddedReport;
        //this.send2WS_Insert(x);
        // this.WS_DB_Insert.send(BSON.serialize(x));

        if (!this.cQ.enQ(x))//If enQ NOT success
        {
          //Just print
          log.error("enQ failed size()=" + this.cQ.size());
          if (this.props.onDBInsertFail !== undefined)
            this.props.onDBInsertFail(x, "Cannot enQ the data");
        }
        if (this.cQ.size() > 0)
          this.cQ.kick();//kick transmission
      }
      if (this.props.pull_skip !== undefined) {
        this.pull_skip_count++;
        if (this.pull_skip_count >= this.props.pull_skip) {
          this.pull_skip_count = 0;
        }
      }
    }
  }

  handleLocalStorage(insertWhat) {
    if (localStorage) {
      log.error("Local Storage: Supported");
      localStorage.setItem("HYVision", JSON.stringify(insertWhat));
      return localStorage.length;
    } else {
      console.log("Local Storage: Unsupported");
    }
    return 0;
  }
  send2WS_Query(msg) {
    //this.state.WS_DB_Query.send("{date:2019}");
  }


  websocketClose() {
    if(this.WS_DB_Insert!==undefined)
      this.WS_DB_Insert.close();
    if(this.WS_DB_Query!==undefined)
      this.WS_DB_Query.close();
  }

  onConnectionStateUpdate(cur, pre) {
    //log.info("dufkhiuefhirhgspiosfjoipfsjvoisfjd",cur,pre);
    if (this.props.onConnectionStateUpdate !== undefined) {
      this.props.onConnectionStateUpdate(cur, pre);
    }

  }
  websocketConnect(url) {
    if(url===undefined)
    {
      return;
    }

    if (this.WS_DB_Insert === undefined) {
      log.info("[init][WS]" +urlConcat(url,"insert/insp"));
      let _ws = new websocket_autoReconnect(urlConcat(url,"insert/insp"), 10000);
      _ws.onStateUpdate = this.onConnectionStateUpdate.bind(this);
      this.WS_DB_Insert = new websocket_reqTrack(_ws);

      this.WS_DB_Insert.onreconnection = (reconnectionCounter) => {
        log.info("onreconnection" + reconnectionCounter);
        return true;
      };
      this.WS_DB_Insert.onconnectiontimeout = () => log.info("onconnectiontimeout");


      this.WS_DB_Insert.onopen = this.onOpen.bind(this);
      this.WS_DB_Insert.onmessage = this.onMessage.bind(this);
      this.WS_DB_Insert.onclose = () => log.info("WS_DB_Insert:onclose");
      this.WS_DB_Insert.onerror = () => this.onError.bind(this);

    }

    if (this.WS_DB_Query === undefined) {
      
      log.info("[init][WS]" + urlConcat(url,"query/insp"));
      let _ws = new websocket_autoReconnect(urlConcat(url,"query/insp"), 10000);
      this.WS_DB_Query = new websocket_reqTrack(_ws);


      this.WS_DB_Query.onreconnection = (reconnectionCounter) => {
        log.info("onreconnection" + reconnectionCounter);
        return true;
      };
      this.WS_DB_Query.onconnectiontimeout = () => log.info("onconnectiontimeout");


      this.WS_DB_Query.onopen = this.onOpen.bind(this);
      this.WS_DB_Query.onmessage = this.onMessage.bind(this);
      this.WS_DB_Query.onclose = () => log.info("WS_DB_Query:onclose");
      this.WS_DB_Query.onerror = () => this.onError.bind(this);
    }

  }



  onError(ev) {
    //this.websocketConnect();
    log.error("onError RAW_InspectionReportPull");
  }
  onOpen(ev) {
    log.info("onOpen RAW_InspectionReportPull");

  }
  onMessage(ev) {
    log.debug(ev);
  }

  render() {

    return null;
  }
}

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
    //console.log(rep);
    return <div className="s black" style={{ "borderBottom": "6px solid #A9A9A9", height: 70 }}>
      <div className="s width8  HXF">
        <div className="s vbox height4">
          <FullscreenOutlined onClick={this.clickFullScreen.bind(this)} />
          {rep.name}
        </div>
        <div className="s vbox  height8" style={{ 'fontSize': 25 }}>
          {this.showResultValueCheck(rep) + DEFAULT_UNIT[rep.subtype]}

        </div>
      </div>
      <div className="s vbox width4 HXF">
        <Tag style={{ 'fontSize': 18 }}
          color={OK_NG_BOX_COLOR_TEXT[rep.detailStatus].COLOR}>
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
    console.log(this.props.detailStatus)
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
    log.info("[XLINX2]fullScreen=" + this.state.fullScreen);
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

      reportDetail = judgeReports.map((rep, idx_) => {
        return <InspectionResultDisplay key={"i" + idx + rep.name} key={idx_} singleInspection={rep} fullScreenToggleCallback={this.toggleFullscreen.bind(this)} />
        // return <InspectionResultDisplay_FullScren key={"i"+idx+rep.name} key={idx_} singleInspection = {rep}/>
      }
      );


      let finalResult = judgeReports.reduce((res, obj) => {
        return MEASURERSULTRESION_reducer(res, obj.detailStatus);
      }
        , undefined);

      return (
        <SubMenu style={{ 'textAlign': 'left' }} key={"sub1" + idx}
          title={
            <span>
              <PaperClipOutlined />
              <span>
                {idx} <OK_NG_BOX detailStatus={finalResult} />
              </span>
            </span>}>
          {reportDetail}
        </SubMenu>
      )
    }
    )


    let fullScreenMODAL = <InspectionResultDisplay_FullScren {...this.state} resultMenuCopy={resultMenu} IR={reports} visible={this.state.fullScreen}
      onCancel={this.toggleFullscreen.bind(this)} width="90%" />;

    return (
      <div>
        <Menu
          onClick={this.handleClick}
          // selectedKeys={[this.current]}
          selectable={true}
          // style={{align: 'left', width: 200}}
          defaultSelectedKeys={['functionMenu']}
          // defaultOpenKeys={['functionMenu']}
          mode="inline">
          <SubMenu style={{ 'textAlign': 'left' }} key="functionMenu"
            title={<span><SettingOutlined />平台功能操作</span>}>
            <MicroFullInspCtrl_rdx
              url={"ws://192.168.2.43:5213"}
            />
          </SubMenu>

          {resultMenu}

        </Menu>
        {fullScreenMODAL}
      </div>
    );
  }
}

class MicroFullInspCtrl extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      settingPanelVisible: false
    }
    this.prevConnectedState = 0;
  }
  componentWillMount() {
    this.ping_interval = setInterval(() => {

      if (false && this.props.uInspData.connected) {
        this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
          { msg: { type: "PING", id: 443 } });
      }
    }, 3000);
  }

  componentWillUnmount() {
    clearInterval(this.ping_interval);
    this.ping_interval = undefined;
  }

  componentDidUpdate(prevProps, prevState, snapshot) {
    if (this.props.uInspData.connected !== prevProps.uInspData.connected) {
      if (this.props.uInspData.connected) {
        this.LoaduInspSettingToMachine();

        setTimeout(() => {
          this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
            { msg: { type: "get_setup", id: 4423 } });
        }, 100);//to separate messages

      }
      else {

      }
    }
  }

  LoaduInspSettingToMachine(filePath = "data/uInspSetting.json") {
    log.info("LoaduInspSettingToMachine step1");
    new Promise((resolve, reject) => {

      log.info("LoaduInspSettingToMachine step2");
      this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
        { filename: filePath },
        undefined, { resolve, reject }
      );
      setTimeout(() => reject("Timeout"), 1000)
    }).then((pkts) => {

      log.info("LoaduInspSettingToMachine>> step3", pkts);
      if (pkts[0].type != "FL") return;
      let machInfo = pkts[0].data;
      this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
        { msg: { ...machInfo, type: "set_setup", id: 356 } });
    }).catch((err) => {

      log.info("LoaduInspSettingToMachine>> step3-error", err);
    })
  }
  render() {

    let ctrlPanel = [];
    console.log(this.props.uInspMachineInfo,this.props.uInspData);
    if (this.props.uInspMachineInfo !== undefined && this.props.uInspData.connected) {
      let machineInfo = this.props.uInspMachineInfo;

      ctrlPanel.push(
        <Button key="ping uInsp"
          onClick={() => {
            this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
              { msg: { type: "PING", id: 443 } });
          }}>
            <HeartTwoTone twoToneColor={this.props.uInspData.alive == 0 ?undefined:"#eb2f96"}/>
            
        </Button>
      );


      ctrlPanel.push(
        <Button key="opt uInsp" icon="setting"
          onClick={() => {
            this.setState({ ...this.state, settingPanelVisible: true })
          }} />);
      <br />
      ctrlPanel.push(
        <Divider orientation="left" key="divi">uInsp</Divider>);


      if (this.props.res_count !== undefined) {
        ctrlPanel.push(<Tag style={{ 'fontSize': 30 }}
          color={OK_NG_BOX_COLOR_TEXT["OK"].COLOR}>{this.props.res_count.OK}
        </Tag>);
        ctrlPanel.push(<Tag style={{ 'fontSize': 30 }}
          color={OK_NG_BOX_COLOR_TEXT["NG"].COLOR}>{this.props.res_count.NG}
        </Tag>);
        ctrlPanel.push(<Tag style={{ 'fontSize': 30 }}
          color={OK_NG_BOX_COLOR_TEXT["NA"].COLOR}>{this.props.res_count.NA}
        </Tag>);
      }


      if (machineInfo.pulse_hz !== undefined) {
        ctrlPanel.push(
          <Slider key="speedSlider"
            min={0}
            max={20000}
            onChange={(value) => {
              let xobj = { pulse_hz: value };
              this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                { msg: { ...xobj, type: "set_setup", id: 356 } });
              this.props.ACT_Machine_Info_Update(xobj);
            }}
            value={machineInfo.pulse_hz}
            step={100}
          />);
      }

      if (machineInfo.state_pulseOffset !== undefined) {

        ctrlPanel.push(
          machineInfo.state_pulseOffset.map((pulseC, idx) =>
            <InputNumber value={pulseC} size="small" key={"poff" + idx} onChange={(value) => {
              let state_pulseOffset = dclone(machineInfo.state_pulseOffset);
              state_pulseOffset[idx] = value;

              this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                { msg: { state_pulseOffset, type: "set_setup", id: 356 } });

              this.props.ACT_Machine_Info_Update({ state_pulseOffset });

            }} />))
      }

      ctrlPanel.push(
        <Modal
          title="" key="settingModal"
          visible={this.state.settingPanelVisible}
          onCancel={() => this.setState({ ...this.state, settingPanelVisible: false })}
          onOk={() => this.setState({ ...this.state, settingPanelVisible: false })}
          footer={null}
        >

          <div style={{ height: "600px" }}>
            <Button.Group key="GGGG">
              <Button
                key="L_ON"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "MISC/BACK_LIGHT/ON" } })
                }>
                ON
            </Button>

              <Button
                key="L_OFF"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "MISC/BACK_LIGHT/OFF" } })
                }>OFF
            </Button>

              <Button
                icon="camera"
                key="CAM"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "MISC/CAM_TRIGGER" } })
                } />

              <Button
                icon="save"
                key="SaveToFile"
                onClick={() => {
                  var enc = new TextEncoder();
                  this.props.ACT_Report_Save(this.props.WS_ID, "data/uInspSetting.json",
                    enc.encode(JSON.stringify(this.props.uInspMachineInfo, null, 4)));
                }}>Save machine setting</Button>


              <Button
                key="MachineSet"
                onClick={() => {

                  this.LoaduInspSettingToMachine();
                }}>MachineSet</Button>
            </Button.Group>



            <Divider orientation="left" key="ERROR">ERROR</Divider>

            <Button.Group key="ERRORG">
              <Button
                key="error_get"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "error_get" } })
                }>
                error_get:{this.props.error_codes}
              </Button>

              <Button
                key="error_clear"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "error_clear" } })
                }>error_clear
            </Button>


              <Button
                key="speed_set"
                onClick={() => {
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    {
                      msg: {
                        pulse_hz: machineInfo.pulse_hz,
                        type: "set_setup",
                        id: 356
                      }
                    });
                }
                }>speed_set
            </Button>
            </Button.Group>




            <Divider orientation="left">MISC</Divider>
            <Button.Group key="MISC_BB">

              <Button
                key="res_count_clear"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "res_count_clear" } })
                }>res_count_clear
            </Button>

            </Button.Group>



            <Divider orientation="left">MODE</Divider>
            <Button.Group key="MODE_G">
              <Button
                key="MODE:TEST"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "mode_set", mode: "TEST" } })
                }>TEST
            </Button>
              <Button
                key="MODE:NORMAL"
                onClick={() =>
                  this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                    { msg: { type: "mode_set", mode: "NORMAL" } })
                }>NORMAL
            </Button>

              <Button type="danger" key="Disconnect uInsp"
                icon="disconnect"
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
          </div>
        </Modal>);

    }

    return (
      <div>
        {
          this.props.uInspData.connected ?
            null
            :
            <Button type="primary" key="Connect uInsp" disabled={this.props.uInspData.connected}
              icon="link"
              onClick={() => {
                this.props.ACT_WS_SEND(this.props.WS_ID, "PD", 0,
                  { ip: "192.168.2.43", port: 5213 });
              }}>{_DICT_.connection.connect}</Button>
        }


        {ctrlPanel}

      </div>
    );
  }
}



const mapDispatchToProps_MicroFullInspCtrl = (dispatch, ownProps) => {
  return {
    ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_Machine_Info_Update: (machineInfo) => dispatch(UIAct.EV_WS_uInsp_Machine_Info_Update(machineInfo)),

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
const mapStateToProps_MicroFullInspCtrl = (state) => {
  return {
    WS_CH: state.UIData.WS_CH,
    WS_ID: state.UIData.WS_ID,
    uInspData: state.Peripheral.uInsp,
    error_codes: state.Peripheral.uInsp.error_codes,
    res_count: state.Peripheral.uInsp.res_count,
    uInspMachineInfo: state.Peripheral.uInsp.machineInfo,
  }
}

let MicroFullInspCtrl_rdx = connect(mapStateToProps_MicroFullInspCtrl, mapDispatchToProps_MicroFullInspCtrl)(MicroFullInspCtrl);




class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }

  ec_canvas_EmitEvent(event) {
    switch (event.type) {
      case DefConfAct.EVENT.ERROR:
        log.error(event);
        this.props.ACT_ERROR();
        break;
      case "asdasdas":
        // log.error(event);
        // this.props.ACT_ERROR();

        let rep = this.props.camera_calibration_report.reports[0];
        let mmpp = rep.mmpb2b / rep.ppb2b;

        let crop = event.data.crop.map(val => val / mmpp);
        let down_samp_level = Math.floor(event.data.down_samp_level / mmpp * 2) + 1;
        if (down_samp_level <= 0) down_samp_level = 1;
        else if (down_samp_level > 15) down_samp_level = 15;


        //log.info(crop,down_samp_level);
        this.props.ACT_WS_SEND(this.props.WS_ID, "ST", 0,
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
        //this.ec_canvas.ctrlLogic();
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 5) return;
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

  componentWillUpdate(nextProps, nextState) {


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
            content: annotationTar.type
          }
        });
      });


    this.charObj.update();
  }


  componentDidMount() {
    var ctx = document.getElementById(this.divID).getContext("2d");

    this.charObj = new Chart(ctx, this.state.chartOpt);
  }
  onResize(width, height) {
    //log.debug("G2HOT resize>>", width, height);
    //this.state.G2Chart.changeSize(width, height);

  }

  render() {
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
  shouldComponentUpdate(nextProps) {
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

    let measureReports = measureList.map((measure) =>
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
      })
    );

    if (measureReports.length == 0) return null;
    //log.error(measureReports);


    //statstate.historyReport.map((rep)=>rep.judgeReports[0]);
    const dataSource = measureReports;

    const columns = ["name", "subtype", "count", "mean", "sigma",
      "OK", "NG", "NA", "WARN",
      "CK", "CP",
      //"CPU","CPL",
      "CPK"]
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
            this.state.drawList[record.id] = val;
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
        <Table key="dat_table" dataSource={dataSource} columns={columns} scroll={{ x: 1000 }} pagination={false} />
        {graphX}
      </div>
    );
  }
}

class MeasureStatList extends React.Component {

  componentDidMount() {
  }

  componentWillUnmount() {
  }

  constructor(props) {
    super(props);
  }

  render() {
    //console.log(this.props.inspection_Info);
    let statisticValue = this.props.inspection_Info.statisticValue;
    if (statisticValue === undefined) {
      return null;
    }
    let info = statisticValue.measureList
      .map(measure => <div>{measure.statistic.mean}</div>)

    return <div className={this.props.addClass}>{info}</div>
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

class APP_INSP_MODE extends React.Component {


  componentDidMount() {


    this.CameraCtrl.setCameraImageTransfer(true);
    
    this.CameraCtrl.setImageCropParam(undefined,4);

    if (this.props.inspMode == "FI") {
      this.props.ACT_WS_SEND(this.props.WS_ID, "FI", 0, { _PGID_: 10004, _PGINFO_: { keep: true }, deffile: this.props.defModelPath + "." + DEF_EXTENSION }, undefined);
      //TODO:HACK HACK make StatSettingParam action slower, 
      //because the cmd "FI"/"CI" will send DefFile("DF") and that will reset StatSettingParam make make this action useless
      setTimeout(() =>
        this.props.ACT_StatSettingParam_Update({
          keepInTrackingTime_ms: 0,
          minReportRepeat: 0,
          headReportSkip: 0,
        }), 2000);
    }
    else if (this.props.inspMode == "CI") {
      this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, { _PGID_: 10004, _PGINFO_: { keep: true }, deffile: this.props.defModelPath + "." + DEF_EXTENSION }, undefined);


      setTimeout(() =>
        this.props.ACT_StatSettingParam_Update({
          keepInTrackingTime_ms: 1000,
          historyReportlimit: 2000,
          minReportRepeat: 4,
          headReportSkip: 4,
        }), 2000);
    }
  }

  componentWillUnmount() {
    this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, { _PGID_: 10004, _PGINFO_: { keep: false } });

  }

  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.checkResult2AirAction = { direction: "none", ver: 0 };
    this.state = {
      GraphUIDisplayMode: 0,
      CanvasWindowRatio: 9,
      ROIs: {},
      ROI_key: undefined,
      DB_Conn_state: undefined,
      inspUploadedCount: 0
    };

    this.CameraCtrl = new CameraCtrl({
      ws_ch: (STData, promiseCBs) => {
        this.props.ACT_WS_SEND(this.props.WS_ID, "ST", 0, STData, undefined, promiseCBs)
      },
      ev_speedModeChange: (mode) => {
        console.log(mode);
      }
    });
    // this.IR = undefined;



    new Promise((resolve, reject) => {
      this.props.ACT_WS_SEND(this.props.WS_ID, "LD", 0,
        { filename: "data/default_camera_setting.json" },
        undefined, { resolve, reject }
      );
      setTimeout(() => reject("Timeout"), 2000)
    }).then((pkts) => {
      if (pkts[0].type != "FL") return;
      let cam_setting = pkts[0].data;
      if (typeof cam_setting.ROIs !== 'object') return;
      let ROIs = cam_setting.ROIs;
      console.log(">>>>", ROIs);
      this.setState({ ROIs, ROI_key: undefined });
    }).catch((err) => { })

  }


  componentDidUpdate() {
    this.CameraCtrl.updateInspectionReport(this.props.inspectionReport);
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

        if (ret_status == INSPECTION_STATUS.SUCCESS) {
          this.checkResult2AirAction = { direction: "right", ver: this.checkResult2AirAction.ver + 1 };
        } else if (ret_status == INSPECTION_STATUS.FAILURE) {
          this.checkResult2AirAction = { direction: "left", ver: this.checkResult2AirAction.ver + 1 };
        } else {
          //log.error("result NA...");
        }
        //
      }
    }
    let MenuSet = [];
    let menu_height = "HXA";//auto
    log.debug("CanvasComponent render");
    let CanvasWindowRatio = 12;
    let menuOpacity = 1;

    let MenuSet_2nd = [];



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

    let shortedModelName=this.props.defModelName.length<23?
      this.props.defModelName.length:
      this.props.defModelName.substring(0, 20)+"..."
    console.log(">>>>defModelName>>>>>"+this.props.defModelName);
    MenuSet = [
      <BASE_COM.IconButton
        iconType={<ArrowLeftOutlined />}
        dict={_DICT_}
        key="<"
        addClass="layout black vbox"
        onClick={this.props.ACT_EXIT} />
      ,
      
      <Popover content={<div>{this.props.defModelName} </div>}  placement="bottomLeft"  trigger="hover">
        <div style={{backgroundColor:"#444"}}> <FileOutlined/> {shortedModelName} </div>
       
      </Popover>

      ,
      <BASE_COM.IconButton
        iconType={this.state.DB_Conn_state == 1 ? <LinkOutlined/>:<DisconnectOutlined/>}
        key="LOADDef"
        addClass={"blockS layout gray-1 vbox " + ((this.state.DB_Conn_state == 1) ? "blackText lgreen" : "BK_Blink")}
        text={this.state.DB_Conn_state == 1 ? _DICT_.connection.server_connected: _DICT_.connection.server_disconnected}
        onClick={() => { }} />
      ,

      <div className="s black width12 HXA">
        <TagDisplay_rdx/>
        
        {/* {<Tag className="large" color="gray" onClick={() => onTagEdit()}><TagsOutlined /></Tag>} */}
      </div>

      ,
      this.state.additionalUI
    ];


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


    MenuSet.push(
      <div className="s black width12 HXA">
        <Row>
          <Col span={6}>
            <Paragraph style={{ color: "white" }}>已上傳：</Paragraph>
          </Col>

          <Col span={24 - 6}>
            <Tag className="width9 large" color="gray" key="upC"
              onClick={() => this.setState({ inspUploadedCount: 0 })}>{this.state.inspUploadedCount}</Tag>
          </Col>
        </Row></div>);


    MenuSet_2nd.push(
      <BASE_COM.IconButton
        dict={_DICT_}
        iconType="up-square"
        key="DoImageTransfer"
        addClass="layout palatte-blue-8 vbox"
        text={"傳輸相機影像(I): " + ((this.CameraCtrl.data.DoImageTransfer) ? "暫停" : "啟動")}
        onClick={() =>
          this.CameraCtrl.setCameraImageTransfer()

        } />);
    MenuSet_2nd.push(
      <BASE_COM.IconButton
        dict={_DICT_}
        iconType="bar-chart"
        key="Info Graphs"
        addClass="layout black vbox"
        text="Info Graphs" onClick={() => {
          this.state.GraphUIDisplayMode = (this.state.GraphUIDisplayMode + 1) % 3;
          this.setState(Object.assign({}, this.state));
        }} />);

    MenuSet_2nd.push(
      <BASE_COM.IconButton
        dict={_DICT_}
        iconType="up-square"
        key="ZOOM OUT"
        iconType="zoom-out"
        addClass="layout palatte-blue-8 vbox width6"
        text={""}
        onClick={() =>
          this.props.ACT_WS_SEND(this.props.WS_ID, "ST", 0,
            { CameraSettingFromFile: "data/" })
        } />);

    const menu_ = (
      <Menu onClick={(ev) => {
        console.log(ev);
        let ROI = this.state.ROIs[ev.key];
        this.props.ACT_WS_SEND(this.props.WS_ID, "ST", 0,
          { CameraSetting: { ROI } })

        this.setState({ ROI_key: ev.key });
      }
      }>
        {Object.keys(this.state.ROIs)
          .map((ROI_key, idx) =>
            <Menu.Item key={ROI_key}>
              <a target="_blank" rel="noopener noreferrer">
                {ROI_key}
              </a>
            </Menu.Item>)}
      </Menu>
    );
    MenuSet_2nd.push(<Dropdown overlay={menu_}>
      <a className="HX1 layout palatte-blue-8 vbox width6" href="#">
        {this.state.ROI_key}
        <CaretDownOutlined />
      </a>
    </Dropdown>);

    MenuSet_2nd.push(<AngledCalibrationHelper className="s width12 HXA"
      reportStatisticState={this.props.reportStatisticState} shape_list={this.props.shape_list}
      camera_calibration_report={this.props.camera_calibration_report} />);

    let trackingWindowInfo = this.props.reportStatisticState.trackingWindow;

    MenuSet.push(
      <ObjInfoList
        IR={trackingWindowInfo}
        IR_decotrator={this.props.info_decorator}
        checkResult2AirAction={this.checkResult2AirAction}
        key="ObjInfoList"
        WSCMD_CB={(tl, prop, data, uintArr) => { this.props.ACT_WS_SEND(this.props.WS_ID, tl, prop, data, uintArr); }}
      />);
    return (
      <div className="overlayCon HXF">

        {(CanvasWindowRatio <= 0) ? null :
          <CanvasComponent_rdx addClass={"layout WXF" + " height" + CanvasWindowRatio}
            ACT_WS_SEND={this.props.ACT_WS_SEND}
            WS_ID={this.props.WS_ID}
            onCanvasInit={(canvas) => { this.ec_canvas = canvas }}
            camera_calibration_report={this.props.camera_calibration_report} />}

        {(CanvasWindowRatio >= 12) ? null :
          <DataStatsTable className={"s scroll WXF" + " height" + (12 - CanvasWindowRatio)}
            reportStatisticState={this.props.reportStatisticState} />}
        <RAW_InspectionReportPull
          reportStatisticState={this.props.reportStatisticState}
          onConnectionStateUpdate={(cur, pre) => {
            //console.log(">>>>>>>>>>",cur,pre);
            this.setState({ DB_Conn_state: cur });
          }}
          onDBInsertSuccess={(data, info) => {
            log.info(data, info);

            this.setState({ inspUploadedCount: this.state.inspUploadedCount + 1 });

          }}

          onDBInsertFail={(data, info) => {
            log.error(data, info);
          }}
          url={this.props.machine_custom_setting.inspection_db_ws_url}
          pull_skip={(this.props.inspMode == "FI") ? 10 : 1} />
        <$CSSTG transitionName="fadeIn">
          <div key={"MENU"} className={"s overlay shadow1 scroll MenuAnim " + menu_height}
            style={{ opacity: menuOpacity, width: "250px" }}>
            {MenuSet}
          </div>
        </$CSSTG>


        <Menu
          onClick={this.handleClick}
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

            <div key={"MENU"} className={"s HXA"}
              style={{ width: "250px" }}>
              {MenuSet_2nd}
            </div>

          </SubMenu>


        </Menu>


      </div>
    );
  }
}



const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps) => {
  return {
    ACT_EXIT: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
    },
    ACT_WS_SEND: (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_StatSettingParam_Update: (arg) => dispatch(UIAct.EV_StatSettingParam_Update(arg)),
  }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
  return {
    c_state: state.UIData.c_state,
    shape_list: state.UIData.edit_info.list,
    info_decorator: state.UIData.edit_info.__decorator,
    defModelName: state.UIData.edit_info.DefFileName,
    
    defModelTag: state.UIData.edit_info.DefFileTag,
    machine_custom_setting: state.UIData.machine_custom_setting,
    machTag: state.UIData.MachTag,
    inspOptionalTag: state.UIData.edit_info.inspOptionalTag,
    defModelPath: state.UIData.edit_info.defModelPath,
    WS_ID: state.UIData.WS_ID,
    inspectionReport: state.UIData.edit_info.inspReport,
    reportStatisticState: state.UIData.edit_info.reportStatisticState,

    inspMode: state.UIData.inspMode,

    camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
    //reportStatisticState:state.UIData.edit_info.reportStatisticState
  }
};

const APP_INSP_MODE_rdx = connect(
  mapStateToProps_APP_INSP_MODE,
  mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;