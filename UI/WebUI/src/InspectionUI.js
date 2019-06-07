'use strict'


import {connect} from 'react-redux';
import React from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {websocket_autoReconnect,websocket_reqTrack} from 'UTIL/MISC_Util';
import EC_zh_TW from "./languages/zh_TW";
import {SHAPE_TYPE,DEFAULT_UNIT} from 'REDUX_STORE_SRC/actions/UIAct';
import {MEASURERSULTRESION,MEASURERSULTRESION_reducer} from 'REDUX_STORE_SRC/reducer/InspectionEditorLogic';
import {INSPECTION_STATUS,DEF_EXTENSION} from 'UTIL/BPG_Protocol';
import * as logX from 'loglevel';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
//import Plot from 'react-plotly.js';
//import {Doughnut} from 'react-chartjs-2';

import {round} from 'UTIL/MISC_Util';
let localStorage = require('localStorage');
let log = logX.getLogger("InspectionUI");

import Row  from 'antd/lib/Row';
import Col  from 'antd/lib/Col';
import Slider  from 'antd/lib/Slider';

import Table from 'antd/lib/table';
import Switch from 'antd/lib/switch';
import  Tag  from 'antd/lib/tag';
import  Input  from 'antd/lib/input';
import  Select  from 'antd/lib/select';
import  Upload  from 'antd/lib/upload';
import Button, {default as AntButton} from 'antd/lib/button';
import  Menu  from 'antd/lib/menu';
import  Icon  from 'antd/lib/icon';

import Chart from 'chart.js';
import 'chartjs-plugin-annotation';
import Modal from "antd/lib/modal";
// import Upload from 'antd/lib/upload';
// import Input from 'antd/lib/Input';
// import Dropdown from 'antd/lib/Dropdown'

// import Tag from 'antd/lib/tag';
// import Select from 'antd/lib/select';
// import Menu from 'antd/lib/menu';
// import Button from 'antd/lib/button';
// import Icon from 'antd/lib/icon';

const ButtonGroup = Button.Group;
const MDB_ATLAS ="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
const MDB_LOCAL="mongodb://localhost:27017/db_hy  ";

const FileProps = {
    name: 'file',

    onChange(info)   {
        console.log(info);

    },
};
const Option = Select.Option;

const selectBefore = (
    <Select defaultValue="Http://" style={{width: 90}}>
        <Option value="Http://">Http://</Option>
        <Option value="Https://">Https://</Option>
    </Select>
);
const selectAfter = (
    <Select defaultValue=".com" style={{width: 80}}>
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
        this.WS_DB_Inser= undefined;
        this.WS_DB_Query= undefined;
    }
    componentWillUnmount() {

        this.websocketClose();
    }
    componentWillMount() {

        this.websocketConnect(this.props.url);

    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if(this.props.reportStatisticState.newAddedReport.length>0)
        {
            // let x=this.props.reportStatisticState.newAddedReport.map(e=>e);
            let x=this.props.reportStatisticState.newAddedReport;
            this.send2WS_Insert(x);
            // this.WS_DB_Insert.send(BSON.serialize(x));
        }
    }

    handleLocalStorage(insertWhat){
        if (localStorage) {
            console.log("Local Storage: Supported");
            localStorage.setItem("HYVision",JSON.stringify(insertWhat));
            return localStorage.length;
        } else {
            console.log("Local Storage: Unsupported");
        }
        return 0;
    }
    send2WS_Query(msg){
        //this.state.WS_DB_Query.send("{date:2019}");
    }
    send2WS_Insert(data){
        console.log("send2WS_Insert");
        if(this.WS_DB_Insert===undefined)return;
        console.log("readyState:"+this.WS_DB_Insert.readyState);
        if(this.WS_DB_Insert.readyState===WebSocket.OPEN){
                
            var msg_obj = {
                dbcmd:{"db_action":"insert","checked":true},
                data
            };
            //The second param is replacer for stringify, and we replace any value that has toFixed(basically 'Number') to replace it to toFixed(5)
            this.WS_DB_Insert.send_obj(msg_obj,(key,val)=>val.toFixed ? Number(val.toFixed(5)) : val).
            then((ret)=>console.log('then',ret)).
            catch((ret)=>console.log("catch",ret));

            //let ls_len=this.handleLocalStorage(msg);
            //console.log("[LocalStorage len=]",ls_len);

        }
        else
            console.log("[X][WS]StatusCode="+ this.WS_DB_Insert.readyState );
    }

    websocketClose()
    {
        this.WS_DB_Insert.close();
        this.WS_DB_Query.close();
    }

    websocketConnect(url = "ws://hyv.decade.tw:8080/") {


        if(this.WS_DB_Insert===undefined)
        {
            console.log("[init][WS]" + url+"insert/insp");
            let _ws=new websocket_autoReconnect(url+"insert/insp",10000);
            this.WS_DB_Insert=new websocket_reqTrack(_ws);

            this.WS_DB_Insert.onreconnection=(reconnectionCounter)=>{
                log.info("onreconnection"+reconnectionCounter);
                return true;
            };
            this.WS_DB_Insert.onconnectiontimeout=()=>log.info("onconnectiontimeout");


            this.WS_DB_Insert.onopen=this.onOpen.bind(this);
            this.WS_DB_Insert.onmessage=this.onMessage.bind(this);
            this.WS_DB_Insert.onclose=()=>log.info("WS_DB_Insert:onclose");
            this.WS_DB_Insert.onerror=()=>this.onError.bind(this);

        }

        if(this.WS_DB_Query===undefined)
        {
            
            console.log("[init][WS]" + url+"query/insp");
            let _ws=new websocket_autoReconnect(url+"query/insp",10000);
            this.WS_DB_Query=new websocket_reqTrack(_ws);

            
            this.WS_DB_Query.onreconnection=(reconnectionCounter)=>{
                log.info("onreconnection"+reconnectionCounter);
                return true;
            };
            this.WS_DB_Query.onconnectiontimeout=()=>log.info("onconnectiontimeout");


            this.WS_DB_Query.onopen=this.onOpen.bind(this);
            this.WS_DB_Query.onmessage=this.onMessage.bind(this);
            this.WS_DB_Query.onclose=()=>log.info("WS_DB_Query:onclose");
            this.WS_DB_Query.onerror=()=>this.onError.bind(this);
        }

    }



    onError(ev) {
        //this.websocketConnect();
        console.log("onError RAW_InspectionReportPull");
    }
    onOpen(ev) {
        console.log("onOpen RAW_InspectionReportPull");

    }
    onMessage(ev) {
        console.log(ev);
    }

    render() {

        return null;
    }
}

class DB extends React.Component {
    constructor(props) {
        super(props);
        this.resultDB=undefined;
        this.state = {
            db_open: false,
            resultX:undefined
        }
    }
    componentDidMount() {

    }
    echoTime(){
        return ""+new Date().getSeconds();
    }
    query(){
        this.resultDB="[!]";
        fetch('http://127.0.0.1:4000/gui', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
            },
            body: JSON.stringify({query: "{ hello }"})
        }).then(r => r.json()).then((datax) => {
            console.log('[O]DB data returned:', JSON.stringify(datax));

            this.resultDB=JSON.stringify(datax);
        });
        return this.resultDB;
    }
    render() {
        return (
            <div style={{'display':'inline-block'}}>
                {/*{this.echoTime()}*/}
                <p>
                {/*{this.query()}*/}
                </p>
            </div>
        );
    }
}


export const OK_NG_BOX_COLOR_TEXT = {
    [MEASURERSULTRESION.NA]:{COLOR:"#aaa",TEXT:MEASURERSULTRESION.NA},
    [MEASURERSULTRESION.UOK]:{COLOR:"#87d068",TEXT:MEASURERSULTRESION.UOK},
    [MEASURERSULTRESION.LOK]:{COLOR:"#87d068",TEXT:MEASURERSULTRESION.LOK},
    [MEASURERSULTRESION.UCNG]:{COLOR:"#d0d068",TEXT:MEASURERSULTRESION.UCNG},
    [MEASURERSULTRESION.LCNG]:{COLOR:"#d0d068",TEXT:MEASURERSULTRESION.LCNG},
    [MEASURERSULTRESION.USNG]:{COLOR:"#f50",TEXT:MEASURERSULTRESION.USNG},
    [MEASURERSULTRESION.LSNG]:{COLOR:"#f50",TEXT:MEASURERSULTRESION.LSNG},
};

export class InspectionResultDisplay_FullScren extends React.Component {

    constructor(props) {
        super(props);
        this.state = {
            folderStruct: {},
            history: ["./"],
        }
    }
    render()
    {
        // if(!this.props.visible)return;

        let reports = this.props.IR;
        if(!Array.isArray(reports) )
        {
            return null;
        }

        // console.log("[XLINX]clone rMenu");
        // console.log(this.props.resultMenuCopy);
        let resultMenu_4fullscreenUse=this.props.resultMenuCopy;

        let titleRender=<div>
            Object List Window
        </div>;
        let openAllsubMenuKeyList=resultMenu_4fullscreenUse.map(function(item, index, array){
            return "sub1"+index;
        });

        let separateSubMenu=resultMenu_4fullscreenUse.map(function(item){
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
                width={this.props.width===undefined?900:this.props.width}
                onCancel={this.props.onCancel}
                onOk={this.props.onOk}
                footer={null}
            >
                <div style= {{ height:this.props.height===undefined?400:this.props.height}}>
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

    showResultValueCheck(o){
        if(o.status==MEASURERSULTRESION.NA)
            return "NaN ";
        else if(o.value===+o.value)
            return o.value.toFixed(3);
        else
            return "NaN ";
    }


    render() {
        let rep = this.props.singleInspection;
        //console.log(rep);
        return <div className="s black" style={{"borderBottom": "6px solid #A9A9A9",height: 70}}>
            <div className="s width8  HXF">
                <div className="s vbox height4">
                    <Icon type="fullscreen" onClick={this.clickFullScreen.bind(this)}/>
                    {rep.name}
                </div>
                <div className="s vbox  height8"  style={{'fontSize': 25}}>
                    {this.showResultValueCheck(rep)+DEFAULT_UNIT[rep.subtype]}

                </div>
            </div>
            <div className="s vbox width4 HXF">
                <Tag style={{'fontSize': 18}}
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
        return (
            <div style={{'display':'inline-block'}}>
                <Tag style={{'fontSize': 20}}
                     color={OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].COLOR}>{OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].TEXT}
                </Tag>
                {this.props.children}
            </div>
        )
    }
}

class CameraCtrl  {
    constructor(setting) {
        this.data={
            DoImageTransfer:true,
            emptyResultCount:0,
            cameraSpeedMode:-1,
            speedSwitchingCount:100,
        };
        this.ws_ch=setting.ws_ch;

        this.ev_speedModeChange=setting.ev_speedModeChange;
        if(this.ev_speedModeChange===undefined)
            this.ev_speedModeChange=()=>{};

        this.ev_emptyResultCountChange=setting.ev_emptyResultCountChange;
        if(this.ev_emptyResultCountChange===undefined)
            this.ev_emptyResultCountChange=()=>{};

        this.setSpeedSwitchingCount(1000);
        this.setCameraSpeed_HIGH();
    }

    

    setCameraImageTransfer(doTransfer) {
        if(doTransfer===undefined)doTransfer=!this.data.DoImageTransfer;
        this.data.DoImageTransfer = doTransfer;
        this.ws_ch({DoImageTransfer: doTransfer});
    }

    setCameraSpeedMode(mode) {
        if(this.data.cameraSpeedMode == mode)return;
        log.info("setCameraSpeedMode:"+mode);
        this.data.cameraSpeedMode=mode;
        
        this.ev_speedModeChange(mode);
        this.ws_ch({CameraSetting: {framerate_mode:mode}});
    }

    
    
    setSpeedSwitchingCount(speedSwitchingCount=1000)
    {
        this.data.speedSwitchingCount=speedSwitchingCount;
    }

    setCameraSpeed_HIGH()
    {
        this.setCameraSpeedMode(2);
    }
    setCameraSpeed_LOW()
    {
        this.setCameraSpeedMode(0);
    }

    updateInspectionReport(report)
    {
        if(report===undefined || report.reports.length==0)
        {
            this.data.emptyResultCount++;
            if(this.data.emptyResultCount>this.data.speedSwitchingCount)
                this.setCameraSpeed_LOW();

            this.ev_emptyResultCountChange(this.data.emptyResultCount);
            return;
        }

        if(this.data.emptyResultCount!=0)
        {
            this.ev_emptyResultCountChange(this.data.emptyResultCount);
            this.data.emptyResultCount=0;
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
        this.setState({...this.state,
            collapsed: !this.state.collapsed,
        });
    }
    toggleFullscreen(){
        console.log("[XLINX2]fullScreen="+this.state.fullScreen);
        this.setState({...this.state,
            fullScreen: !this.state.fullScreen,
        });

    }
    render() {

        let resultMenu = [];
        let reports = this.props.IR;
        if(!Array.isArray(reports) )
        {
            return null;
        }
            resultMenu = reports.filter((rep)=>rep.isCurObj).map((singleReport,idx) => {
                let reportDetail=[];
                let judgeReports = singleReport.judgeReports;
                reportDetail = judgeReports.map((rep,idx_)=>
                    {
                        return <InspectionResultDisplay key={"i"+idx+rep.name} key={idx_} singleInspection = {rep} fullScreenToggleCallback={this.toggleFullscreen.bind(this)}/>
                        // return <InspectionResultDisplay_FullScren key={"i"+idx+rep.name} key={idx_} singleInspection = {rep}/>
                    }
                );


                let finalResult = judgeReports.reduce((res, obj) => {
                        return MEASURERSULTRESION_reducer(res, obj.detailStatus);
                    }
                    , undefined);

                // log.info("judgeReports>>", judgeReports);
                return (
                    <SubMenu style={{'textAlign': 'left'}} key={"sub1"+idx}
                             title={<span><Icon type="paper-clip"/><span>{idx} <OK_NG_BOX detailStatus={ finalResult } ></OK_NG_BOX></span>

                             </span>}>
                        {reportDetail}
                    </SubMenu>


                    )
                }
            )

        let fullScreenMODAL =<InspectionResultDisplay_FullScren {...this.state} resultMenuCopy={resultMenu} IR={reports} visible={this.state.fullScreen}
                                                                onCancel={ this.toggleFullscreen.bind(this)} width="90%" />;

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
                    <SubMenu style={{'textAlign': 'left'}} key="functionMenu"
                             title={<span><Icon type="setting"/><span>平台功能操作</span></span>}>
                        <AirControl_rdx
                            url={"ws://192.168.2.2:5213"}
                            checkResult2AirAction={this.props.checkResult2AirAction}
                            WSCMD_CB={this.props.WSCMD_CB}
                        />
                    </SubMenu>

                    {resultMenu}

                </Menu>
                {fullScreenMODAL}
            </div>
        );
    }
}

class AirControl extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            loading: false,
            iconLoading: false,
            websocketAirTime: 10,
            STOP: true,
        }
        this.websocketAir=undefined;
        this.heartBeat={
            timer:undefined,
            PINGcount:0,
            PONGcount:0,
            PONGPace:0,
        };
    }

    keyEventX(event) {
        let key = event.keyCode || event.which;
        let keychar = String.fromCharCode(key);
        log.info("keyEvent>>", key, keychar);
        switch (keychar) {
            case 'R':
                this.blowAir_RIGHTa();
                break;
            case 'L':
                this.blowAir_LEFTa();
                break;
            case 'B':
                this.blowAir_StartStop();
                break;
            case '1':
                this.blowAir_TIMEADD(10);
                break;
            case '2':
                this.blowAir_TIMESUB(10);
                break;
            case 'T':
                this.blowAir_TEST();
                break;
            case 'I':
                this.getCameraImage_StartStop();
                break;
        }

    }

    blowAir_TEST() {
        console.log("[WS]/cue/TEST");
        if (this.websocketAir.readyState === 1) {
            console.log("[O][WS]/cue/TEST");
            this.websocketAir.send("/cue/TEST");
        } else {
            console.log("[X][WS]/cue/TEST");
        }

    }

    blowAir_LEFTa() {
        if (this.websocketAir.readyState === this.websocketAir.OPEN) {
            this.websocketAir.send("/cue/LEFT");
        } else {
            console.log("[X][WS]/cue/LEFT");
        }

    }

    blowAir_RIGHTa() {
        if (this.websocketAir.readyState === this.websocketAir.OPEN) {
            this.websocketAir.send("/cue/RIGHT");
        } else {
            console.log("[X][WS]/cue/RIGHT");
        }
    }

    blowAir_TIMEUpdate() {
        this.setState(Object.assign({}, this.state));
        if (this.websocketAir.readyState === this.websocketAir.OPEN)
            this.websocketAir.send("/cue/TIME/" + this.state.websocketAirTime);
    }

    blowAir_StartStop() {
        this.state.STOP = !this.state.STOP;
        this.setState(Object.assign({}, this.state));
    }

    blowAir_TIMEADD(val) {
        this.state.websocketAirTime += val;
        this.blowAir_TIMEUpdate();
    }

    blowAir_TIMESUB(val) {
        this.state.websocketAirTime -= val;
        if (this.state.websocketAirTime < 10)
            this.state.websocketAirTime = 10;
        this.blowAir_TIMEUpdate();
    }

    enterLoading() {
        this.setState({...this.state,loading: true});
    }


    sendHeartBeat()
    {
        if (this.websocketAir===undefined ||
            this.websocketAir.readyState != this.websocketAir.OPEN) return;

        log.info("sendHeartBeat: PING:"+this.heartBeat.PINGcount, 
            " PONG:"+this.heartBeat.PONGPace+"/"+this.heartBeat.PONGcount);
        if(this.heartBeat.PINGcount>this.heartBeat.PONGPace+2)
        {
            log.error("Heart beat ERROR: PINGcount > PONGPace+2");
            this.websocketAir.reconnect();
            this.heartBeat.PINGcount=this.heartBeat.PONGPace=0;
            return;
        }

        
        this.websocketAir.send("/cue/PING");
        this.heartBeat.PINGcount++;
    }
    componentWillMount() {
        this._keyEventX = this.keyEventX.bind(this);
        console.log("[init][componentWillMount]");
        this.websocketConnect(this.props.url);
        
        this.websocketAir=new websocket_autoReconnect(this.props.url,10000);
        this.websocketAir.onreconnection=(count)=>console.log(count);
        this.websocketAir.onmessage = this.onMessage.bind(this);
        this.websocketAir.onerror = this.onError.bind(this);
        this.websocketAir.onopen = (ev)=>{
                this.setState({...this.state,loading: false});
                console.log("onopen:",ev);
                this.props.ACT_MinRepeatInspReport_Update(1);
                this.heartBeat.PINGcount=0;
                this.heartBeat.PONGcount=0;
    
            };
        this.websocketAir.onclose =(evt) => {
                this.props.ACT_MinRepeatInspReport_Update();
                this.setState({...this.state,loading: true});
                if (evt.code == 3001) {
                    console.log('ws closed',evt);
                } else {
                    console.log('ws connection error',evt);
                }
            };

        document.addEventListener('keydown', this._keyEventX);
        
        this.heartBeat.timer=setInterval(this.sendHeartBeat.bind(this),3000);
    }

    componentWillUnmount() {
        // log.info("componentWillUnmount1")
        if(this.websocketAir!==undefined)
        {
            this.websocketAir.close();
            this.websocketAir = undefined;
        }
        document.removeEventListener('keydown', this._keyEventX);
        // log.info("componentWillUnmount2")

        clearInterval(this.heartBeat.timer);
        this.heartBeat.timer=undefined;
    }
    

    websocketConnect(url = "ws://192.168.2.2:5213") {

        console.log("[init][WS][OK]");
        console.log(this.websocketAir);
    }

    onError(ev) {
        this.setState({...this.state,loading: false});
        //this.websocketConnect();
        console.log("onError");
    }

    onMessage(ev) {
        console.log(ev);
        let tstamp = ev.timeStamp;
        let data = ev.data;
        if(data === "/rsp/PONG")
        {
            this.heartBeat.PONGcount++;
            this.heartBeat.PONGPace=this.heartBeat.PINGcount;
        }
    }

    enterIconLoading() {
        //this.setState({ iconLoading: true });
    }


    componentWillReceiveProps(nextProps) {
        if (this.state.STOP) return;
        //log.info(nextProps.checkResult2AirAction.ver, "222");
        // console.log(this.websocketAir.OPEN,this.websocketAir.readyState,"XXX");
        // if(this.websocketAir.readyState != this.websocketAir.OPEN)return;
        //log.error(nextProps.checkResult2AirAction.ver,this.props.checkResult2AirAction.ver);
        if (nextProps.checkResult2AirAction.ver == this.props.checkResult2AirAction.ver) return;

        if (nextProps.checkResult2AirAction.direction === "left") {
            this.blowAir_LEFTa();
        } else if (nextProps.checkResult2AirAction.direction === "right") {
            this.blowAir_RIGHTa();
        }


    }

    render() {
        if (this.websocketAir===undefined ||
            this.websocketAir.readyState != this.websocketAir.OPEN) {
            return(
            <BASE_COM.AButton block text="ReconnectAirDevice" type="primary" shape="round" icon="loading" size="large" dict={EC_zh_TW}
                    onClick={() => {this.websocketConnect(this.props.url);
                    }}/>
            );
        }

        return (

            <div>
                <Button block style={{marginTop: 2, marginBottom: 2}} type="primary" size="small"
                        onClick={() => this.blowAir_StartStop()}>
                    噴氣功能(B): {this.state.STOP ? " 暫停=" : " 啟動="}{this.state.websocketAirTime}ms
                </Button>
                <ButtonGroup>
                    <Button style={{backgroundColor: "#c41d7f", marginBottom: 2}} type="primary"
                            size="large"
                            onClick={this.blowAir_LEFTa.bind(this)}>
                        <Icon type="double-left"/>NG左
                    </Button>

                    <Button style={{backgroundColor: "#7cb305", marginBottom: 2}} type="primary"
                            size="large"
                            onClick={() => this.blowAir_RIGHTa()}>
                        右OK<Icon type="double-right"/>
                    </Button>
                </ButtonGroup>
                <Button block style={{marginBottom: 2}} type="primary" size="small"
                        onClick={() => this.blowAir_TIMEADD(10)}>
                    <Icon type="up-circle"/>
                    增加噴氣時間(1)
                </Button>
                <Button block style={{marginBottom: 2}} type="primary" size="small"
                        onClick={() => this.blowAir_TIMESUB(10)}>
                    <Icon type="down-circle"/>
                    漸少噴氣時間(2)
                </Button>

                <Button block style={{marginBottom: 2}} type="primary" size="small" onClick={() => this.blowAir_TEST()}>
                    <Icon type="swap"/>
                    測試左右噴氣(T)
                    <Icon type="swap"/>
                </Button>


                <br/>
                <Upload {...FileProps}>
                    <Button block style={{marginBottom: 3}} type="primary" size="small">
                        <Icon type="thunderbolt"/>
                        快速切換特徵檔案
                        <Icon type="thunderbolt"/>
                    </Button>

                </Upload>

                <div>
                </div>

            </div>
        );
    }
}


const mapStateToProps_AirControl = (state) => {
    //log.info("mapStateToProps",JSON.stringify(state.UIData.c_state));
    return {}
}
const mapDispatchToProps_AirControl = (dispatch, ownProps) => {
    return {
        ACT_MinRepeatInspReport_Update: (arg) => {
            dispatch(UIAct.EV_WS_MinRepeatInspReport_Update(arg))
        },
    }
}
const AirControl_rdx = connect(
    mapStateToProps_AirControl,
    mapDispatchToProps_AirControl)(AirControl);




class CanvasComponent extends React.Component {
    constructor(props) {
        super(props);
        this.windowSize={};
    }

    ec_canvas_EmitEvent(event) {
        switch(event.type)
        { 
        case DefConfAct.EVENT.ERROR:
            log.error(event);
            this.props.ACT_ERROR();
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
        if(Math.hypot(this.windowSize.width-width,this.windowSize.height-height)<5)return;
        if (this.ec_canvas !== undefined) {
            this.ec_canvas.resize(width, height);
            this.windowSize={
                width,height
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
                <canvas ref="canvas" className="width12 HXF"/>
                <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
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
        ACT_ERROR:(arg) => {
            dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.ERROR))
        },
    }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);



class ControlChart extends React.Component {
    constructor(props) {
        super(props);
        this.divID="ControlChart_ID_" + Math.round(Math.random() * 10000);
        this.charObj = undefined;

        this.state={
            chartOpt:{
                type: 'line',
                data:{labels:[],datasets:[{            
                    type: "line",
                    borderColor:"rgb(100, 255, 100)",
                    lineTension: 0.2,data:[],
                    pointBackgroundColor:[]}]},
                bezierCurve : false,
                options: {
                    scales: {
                        xAxes: [{
                            ticks: {
                                callback: function(value, index, values) {
                                    return value;
                                }
                            }
                        }]   
                    },
                    elements: {
                        line: {fill: false},
                        point: { radius: 6 } 
                    },
                    bezierCurve : false,
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

        this.default_annotationTargets=[
            {type:"USL",color:"rgba(200, 0, 0,0.2)"},
            {type:"LSL",color:"rgba(200, 0, 0,0.2)"},
            {type:"UCL",color:"rgba(100, 100, 0,0.2)"},
            {type:"LCL",color:"rgba(100, 100, 0,0.2)"},
            {type:"value",color:"rgba(0, 0, 0,0.2)"},
        ];
    } 

    componentWillUpdate(nextProps, nextState) {

        
        //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
        this.state.chartOpt.data.labels=[];
        this.state.chartOpt.data.datasets.forEach((datInfo)=>{
            datInfo.data=[];
            datInfo.pointBackgroundColor=[];
        });
        let length = nextProps.reportArray.length;
        if(length==0)return;
        let newTime = nextProps.reportArray[length-1].time_ms;
        this.state.chartOpt.options.title.text=nextProps.reportArray[0].judgeReports.find((jrep)=>jrep.id==nextProps.targetMeasure.id).name;

        nextProps.reportArray.reduce((acc_data,rep,idx)=>{
            acc_data.labels.push((newTime-rep.time_ms)/1000);

            let measureObj = rep.judgeReports.find((jrep)=>jrep.id==nextProps.targetMeasure.id);

            let pointColor=undefined;
            let val=measureObj.value;
            pointColor=OK_NG_BOX_COLOR_TEXT[measureObj.detailStatus].COLOR;
            if(pointColor===undefined)
            {
                pointColor="#AA0000";
            }
            //console.log(measureObj.detailStatus);
            if(measureObj.detailStatus===MEASURERSULTRESION.NA)
            {
                val=undefined;
            }



            acc_data.datasets[0].pointBackgroundColor.push(pointColor);
            //TODO:for now there is only one data set in one chart
            acc_data.datasets[0].data.push(val);
            return acc_data;
        }, this.state.chartOpt.data );



        let annotationTargets=this.props.nnotationTargets;
        if(annotationTargets===undefined)
        {
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
            <canvas id={this.divID}  style={{height: "400px"}}/>
            <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
        </div>
    }

}

    

class DataStatsTable extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            drawList:[]
        };
    }

    render() {
        let statstate = this.props.reportStatisticState;
        //console.log(statstate);
        if(statstate.statisticValue===undefined)
        {
            return null;
        }
        let measureList = statstate.statisticValue.measureList;
        
        let measureReports=measureList.map((measure)=>
            ({
            id:measure.id,
            name:measure.name,
            subtype:measure.subtype,
            count:measure.statistic.count,
            mean:round(measure.statistic.mean,0.001),
            sigma:round(measure.statistic.sigma,0.001),
            CK:round(measure.statistic.CK,0.001),
            CPU:round(measure.statistic.CPU,0.001),
            CPL:round(measure.statistic.CPL,0.001),
            CP:round(measure.statistic.CP,0.001),
            CPK:round(measure.statistic.CPK,0.001),
        })
        );
        
        if(measureReports.length==0)return null;
        //log.error(measureReports);
    
        
        //statstate.historyReport.map((rep)=>rep.judgeReports[0]);
        const dataSource = measureReports;
        
        const columns = ["name","subtype","count","mean","sigma","CK","CP","CPU","CPL","CPK"]
            .map((type)=>({title:type,dataIndex:type,key:type}))
            .map((col)=>(  typeof measureReports[0][col.title]  == 'number')?//Find the first dataset and if it's number then add a sorter
                Object.assign(col,{sorter: (a, b) => a[col.title] - b[col.title]}):col)
        /*columns[0].fixed="left";
        columns[0].width=100;

        columns[columns.length-1].fixed="right";
        columns[columns.length-1].width=100;*/
        columns.push(
            {title:"Draw Toggle",key:"draw",fixed:"right",
            render: (text, record) => {
                return <Switch  onChange={(val)=>{
                    this.state.drawList[record.id]=val;
                    }
                } />
            }}
        );

        let graphX =Object.keys(this.state.drawList).map((key,idx)=>
        {
            if(this.state.drawList[key]==true)
            {
                let targetMeasure = measureList.find((m)=>m.id==key);

                let lastN=500;
                let lastNArr = statstate.historyReport.slice(Math.max(statstate.historyReport.length - lastN, 1));
                return <ControlChart reportArray={lastNArr} targetMeasure={targetMeasure}/>
            }
            return null;
        });
        //console.log(graphX);
      
        return(
            <div className={this.props.className}>
                <Table key="dat_table" dataSource={dataSource} columns={columns} scroll={{ x: 1000 }} pagination={false}/>
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
        if(statisticValue===undefined)
        {
            return null;
        }
        let info = statisticValue.measureList
            .map(measure=><div>{measure.statistic.mean}</div>)

        return <div className={this.props.addClass}>{info}</div>
    }
}

class APP_INSP_MODE extends React.Component {


    componentDidMount() {
        this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, {deffile: this.props.defModelPath + "."+DEF_EXTENSION});
        this.CameraCtrl.setCameraImageTransfer(false);
    }

    componentWillUnmount() {
        this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, {});

    }

    constructor(props) {
        super(props);
        this.ec_canvas = null;
        this.checkResult2AirAction = {direction: "none", ver: 0};
        this.state={
            GraphUIDisplayMode:0,
            CanvasWindowRatio:9,
        };

        this.CameraCtrl=new CameraCtrl({
            ws_ch:(STData,promiseCBs)=>
            {
                this.props.ACT_WS_SEND(this.props.WS_ID,"ST", 0, STData,undefined,promiseCBs)
            },
            ev_speedModeChange:(mode)=>{
                console.log(mode);
            }
        });
        // this.IR = undefined;
    }

    
    componentDidUpdate()
    {
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
                    this.checkResult2AirAction = {direction: "right", ver: this.checkResult2AirAction.ver + 1};
                } else if (ret_status == INSPECTION_STATUS.FAILURE) {
                    this.checkResult2AirAction = {direction: "left", ver: this.checkResult2AirAction.ver + 1};
                } else {
                    log.error("result NA...");
                }
                //
            }
        }
        let MenuSet = [];
        let menu_height = "HXA";//auto
        log.debug("CanvasComponent render");
        let CanvasWindowRatio = 12;
        let menuOpacity = 1;


        MenuSet = [
            <BASE_COM.IconButton
                dict={EC_zh_TW}
                key="<"
                addClass="layout black vbox"
                text="<" onClick={this.props.ACT_EXIT}/>
            ,
            <BASE_COM.IconButton
            iconType="file"
            key="LOADDef"
            addClass="layout gray-1 vbox"
            text={this.props.defModelName}
            onClick={() => {}}/>
            ,

            <div className="s black width12 HXA">
                {this.props.defModelTag.map(tag=><Tag color="red">{tag}</Tag>)}
                {this.props.inspOptionalTag.map(tag=><Tag color="green">{tag}</Tag>)}
            </div>
            ,
            <BASE_COM.IconButton
                dict={EC_zh_TW}
                iconType="bar-chart"
                key="Info Graphs"
                addClass="layout black vbox"
                text="Info Graphs" onClick={()=>{
                    this.state.GraphUIDisplayMode=(this.state.GraphUIDisplayMode+1)%3;
                    this.setState(Object.assign({},this.state));
                }}/>
            
            ,
        ];


        switch(this.state.GraphUIDisplayMode)
        {
            case 0:
            CanvasWindowRatio=12;
            menuOpacity=1;
            break;

            case 1:
            CanvasWindowRatio=3;
            menuOpacity=0.3;
            break;
            
            case 2:
            CanvasWindowRatio=0;
            menuOpacity=0.3;
            break;

            case 3:
            CanvasWindowRatio=3;
            menuOpacity=0.3;
            break;
        }
        
        MenuSet.push(
            <BASE_COM.IconButton
            dict={EC_zh_TW}
            iconType="up-square"
            key="DoImageTransfer"
            addClass="layout palatte-blue-8 vbox"
            text={"傳輸相機影像(I): "+ ((this.state.DoImageTransfer) ?"暫停": "啟動")}
            onClick={() => this.CameraCtrl.setCameraImageTransfer()}/>);

            
        MenuSet.push(
            <ObjInfoList IR={this.props.reportStatisticState.trackingWindow} checkResult2AirAction={this.checkResult2AirAction}
            key="ObjInfoList"
            WSCMD_CB={(tl, prop, data, uintArr)=>{this.props.ACT_WS_SEND(this.props.WS_ID,tl, prop, data, uintArr);}}
            />);

        return (
            <div className="overlayCon HXF">
                
                {(CanvasWindowRatio<=0)?null:
                    <CanvasComponent_rdx addClass={"layout WXF"+" height"+CanvasWindowRatio} 
                        onCanvasInit={(canvas) => {this.ec_canvas = canvas}}/>}
                
                {(CanvasWindowRatio>=12)?null:
                    <DataStatsTable className={"s scroll WXF"+" height"+(12-CanvasWindowRatio)} 
                        reportStatisticState={this.props.reportStatisticState}/>}
                <RAW_InspectionReportPull 
                    reportStatisticState={this.props.reportStatisticState} 
                    url= "ws://hyv.decade.tw:8080/"/>
                <$CSSTG transitionName="fadeIn">
                    <div key={"MENU"} className={"s overlay shadow1 scroll MenuAnim " + menu_height} 
                        style={{opacity:menuOpacity,width:"250px"}}> 
                        {MenuSet}
                    </div>
                </$CSSTG>

            </div>
        );
    }
}



const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps) => {
    return {
        ACT_EXIT: (arg) => {
            dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
        },
        ACT_WS_SEND: (id, tl, prop, data, uintArr,promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr,promiseCBs)),
    }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
    return {
        c_state: state.UIData.c_state,
        shape_list: state.UIData.edit_info.list,
        defModelName:state.UIData.edit_info.DefFileName,
        defModelTag:state.UIData.edit_info.DefFileTag,
        inspOptionalTag:state.UIData.edit_info.inspOptionalTag,
        defModelPath: state.UIData.edit_info.defModelPath,
        WS_ID: state.UIData.WS_ID,
        inspectionReport: state.UIData.edit_info.inspReport,
        reportStatisticState:state.UIData.edit_info.reportStatisticState
        //reportStatisticState:state.UIData.edit_info.reportStatisticState
    }
};

const APP_INSP_MODE_rdx = connect(
    mapStateToProps_APP_INSP_MODE,
    mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;