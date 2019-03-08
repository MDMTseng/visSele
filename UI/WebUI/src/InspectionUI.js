'use strict'


import {connect} from 'react-redux'
import React from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import EC_zh_TW from "./languages/zh_TW";
import {SHAPE_TYPE,DEFAULT_UNIT} from 'REDUX_STORE_SRC/actions/UIAct';
import {MEASURERSULTRESION,MEASURERSULTRESION_reducer} from 'REDUX_STORE_SRC/reducer/InspectionEditorLogic';
import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';
import * as logX from 'loglevel';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
//import Plot from 'react-plotly.js';
import {Doughnut} from 'react-chartjs-2';

import {round} from 'UTIL/MISC_Util';

let log = logX.getLogger("InspectionUI");

import Table from 'antd/lib/table';
import Switch from 'antd/lib/switch';

import  Tag  from 'antd/lib/tag';
import  Select  from 'antd/lib/select';
import  Upload  from 'antd/lib/upload';
import  Button  from 'antd/lib/button';
import  Menu  from 'antd/lib/menu';
import  Icon  from 'antd/lib/icon';

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
                <Tag style={{'font-size': 20}}
                     color={OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].COLOR}>{OK_NG_BOX_COLOR_TEXT[this.props.detailStatus].TEXT}
                </Tag>
                {this.props.children}
            </div>
        )
    }
}

class ObjInfoList extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            collapsed: false,
        }
    }

    toggleCollapsed() {
        this.setState({
            collapsed: !this.state.collapsed,
        });
    }

    render() {

        let resultMenu = [];
        if (this.props.IR != undefined) {



            resultMenu = this.props.IR.reports.map((singleReport,idx) => {
                let reportDetail=[];
                let judgeReports = singleReport.judgeReports;
                reportDetail = judgeReports.map((rep,idx_)=>
                    {
                        return <Menu.Item key={"i"+idx+rep.name} key={idx_}>
                            <OK_NG_BOX detailStatus={rep.detailStatus} >
                                {""+rep.value.toFixed(3)+DEFAULT_UNIT[rep.subtype]}
                            </OK_NG_BOX>
                        </Menu.Item>
                    }
                );

                let finalResult = judgeReports.reduce((res, obj) => {
                        return MEASURERSULTRESION_reducer(res, obj.detailStatus);
                    }
                    , undefined);

                //log.info("judgeReports>>", judgeReports);
                return (
                    <SubMenu style={{'text-align': 'left'}} key={"sub1"+idx}
                             title={<span><Icon type="paper-clip"/><span>{idx} <OK_NG_BOX detailStatus={ finalResult } ></OK_NG_BOX></span>

                             </span>}>
                        {reportDetail}
                    </SubMenu>


                    )
                }
            )
        }


        return (
            <div>
                <Menu
                    onClick={this.handleClick}
                    // selectedKeys={[this.current]}
                    selectable={true}
                    // style={{align: 'left', width: 200}}
                    defaultSelectedKeys={['functionMenu']}
                    defaultOpenKeys={['functionMenu']}
                    mode="inline">
                    <SubMenu style={{'textAlign': 'left'}} key="DBMenu"
                             title={<span><Icon type="setting"/><span>資料庫操作</span></span>}>
                        <DB
                            url={MDB_ATLAS}
                            checkResult2AirAction={this.props.checkResult2AirAction}

                        />
                    </SubMenu>
                    <SubMenu style={{'textAlign': 'left'}} key="functionMenu"
                             title={<span><Icon type="setting"/><span>平台功能操作</span></span>}>
                        <AirControl
                            url={"ws://192.168.2.2:5213"}
                            checkResult2AirAction={this.props.checkResult2AirAction}
                            WSCMD_CB={this.props.WSCMD_CB}
                        />
                    </SubMenu>

                    {resultMenu}

                </Menu>
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
            websocketAir: undefined,
            websocketAirTime: 10,
            STOP: true,
        }
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
        if (this.state.websocketAir.readyState === 1) {
            console.log("[O][WS]/cue/TEST");
            this.state.websocketAir.send("/cue/TEST");
        } else {
            console.log("[X][WS]/cue/TEST");
        }

    }

    blowAir_LEFTa() {
        console.log("[WS]/cue/LEFT");
        if (this.state.websocketAir.readyState === this.state.websocketAir.OPEN) {
            console.log("[O][WS]/cue/LEFT");
            this.state.websocketAir.send("/cue/LEFT");
        } else {
            console.log("[X][WS]/cue/LEFT");
        }

    }

    blowAir_RIGHTa() {
        console.log("[WS]/cue/RIGHT");
        if (this.state.websocketAir.readyState === this.state.websocketAir.OPEN) {
            console.log("[O][WS]/cue/RIGHT");
            this.state.websocketAir.send("/cue/RIGHT");
        } else {
            console.log("[X][WS]/cue/RIGHT");
        }
    }

    blowAir_TIMEUpdate() {
        this.setState(Object.assign({}, this.state));
        if (this.state.websocketAir.readyState === this.state.websocketAir.OPEN)
            this.state.websocketAir.send("/cue/TIME/" + this.state.websocketAirTime);
    }

    blowAir_StartStop() {
        this.state.STOP = !this.state.STOP;
        this.setState(Object.assign({}, this.state));
    }

    blowAir_TIMEADD(val) {
        this.state.websocketAirTime += val;
        console.log("[WS]/cue/RIGHT");
        this.blowAir_TIMEUpdate();
    }

    blowAir_TIMESUB(val) {
        this.state.websocketAirTime -= val;
        if (this.state.websocketAirTime < 10)
            this.state.websocketAirTime = 10;
        console.log("[WS]/cue/RIGHT");
        this.blowAir_TIMEUpdate();
    }

    enterLoading() {
        this.setState({loading: true});
    }

    componentWillMount() {
        this._keyEventX = this.keyEventX.bind(this);
        console.log("[init][componentWillMount]");
        this.websocketConnect(this.props.url);
        document.addEventListener('keydown', this._keyEventX);

    }

    componentWillUnmount() {
        log.info("componentWillUnmount1")
        this.state.websocketAir.close();
        this.state.websocketAir = undefined;
        document.removeEventListener('keydown', this._keyEventX);
        log.info("componentWillUnmount2")
    }


    websocketConnect(url = "ws://192.168.2.2:5213") {
        console.log("[init][WS]" + url);
        this.state.websocketAir = new WebSocket(url);
        this.state.websocketAir.onmessage = this.onMessage.bind(this);
        this.state.websocketAir.onerror = this.onError.bind(this);


        this.state.websocketAir.onclose = (evt) => {
            if (evt.code == 3001) {
                console.log('ws closed');
            } else {
                console.log('ws connection error');
            }
        };
        console.log("[init][WS][OK]");
        console.log(this.state.websocketAir);
    }

    onError(ev) {
        //this.websocketConnect();
        console.log("onError");
    }

    onMessage(ev) {
        // console.log(ev);
    }

    enterIconLoading() {
        //this.setState({ iconLoading: true });
    }


    componentWillReceiveProps(nextProps) {
        if (this.state.STOP) return;
        log.info(nextProps.checkResult2AirAction.ver, "222");
        // console.log(this.state.websocketAir.OPEN,this.state.websocketAir.readyState,"XXX");
        // if(this.state.websocketAir.readyState != this.state.websocketAir.OPEN)return;
        //log.error(nextProps.checkResult2AirAction.ver,this.props.checkResult2AirAction.ver);
        if (nextProps.checkResult2AirAction.ver == this.props.checkResult2AirAction.ver) return;

        if (nextProps.checkResult2AirAction.direction === "left") {
            this.blowAir_LEFTa();
        } else if (nextProps.checkResult2AirAction.direction === "right") {
            this.blowAir_RIGHTa();
        }


    }

    render() {

        if (this.state.websocketAir.readyState != this.state.websocketAir.OPEN) {
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
            //if (props.edit_info.session_lock != null && props.edit_info.session_lock.start == false) 
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
                data:{labels:[],datasets:[{lineTension: 0,data:[]}]},
                bezierCurve : false,
                options: {
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
                tooltips: {
                    mode: 'index',
                    intersect: true
                },
                annotation: {
                    annotations: [{
                    type: 'line',
                    mode: 'horizontal',
                    scaleID: 'y-axis-0',
                    value: 5,
                    borderColor: 'rgb(75, 192, 192)',
                    borderWidth: 4,
                    label: {
                        enabled: false,
                        content: 'Test label'
                    }
                    }]
                }
                }
            }
        };
    } 

    componentWillUpdate(nextProps, nextState) {

        
        //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
        this.state.chartOpt.data.labels=[];
        this.state.chartOpt.data.datasets.forEach((datInfo)=>{
            datInfo.data=[];
        });
        let length = nextProps.reportArray.length;
        if(length==0)return;
        let newTime = nextProps.reportArray[length-1].time_ms;
        this.state.chartOpt.options.title.text=nextProps.reportArray[0].judgeReports.find((jrep)=>jrep.id==nextProps.targetMeasureId).name;

        nextProps.reportArray.reduce((acc_data,rep,idx)=>{
            if((idx-(length-1))%10==0)
                acc_data.labels.push((newTime-rep.time_ms)/1000);
            else
                acc_data.labels.push("");

            //TODO:for now there is only one data set in one chart
            acc_data.datasets[0].data.push(
                rep.judgeReports.find((jrep)=>jrep.id==nextProps.targetMeasureId).value);
            return acc_data;
        }, this.state.chartOpt.data );

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
            <canvas id={this.divID}  style={{height: "200px"}}/>
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
        columns[0].fixed="left";
        columns[0].width=100;

        columns[columns.length-1].fixed="right";
        columns[columns.length-1].width=100;
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
                return <ControlChart reportArray={statstate.historyReport} targetMeasureId={key}/>
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

        this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, {deffile: this.props.defModelPath + ".json"});
        this.getCameraImage_StartStop(false);

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
            DoImageTransfer:true
        };
        // this.IR = undefined;
    }

    

    getCameraImage_StartStop(doTransfer) {
        log.info("fun getCameraImage_StartStop click");
        

        this.state.DoImageTransfer = (doTransfer===undefined)?!this.state.DoImageTransfer:doTransfer;
        this.setState(Object.assign({}, this.state));


        
        this.props.ACT_WS_SEND(this.props.WS_ID,"ST", 0, {DoImageTransfer: this.state.DoImageTransfer});
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
            CanvasWindowRatio=10;
            menuOpacity=1;

            
            MenuSet.push(
                <BASE_COM.IconButton
                dict={EC_zh_TW}
                iconType="up-square"
                key="DoImageTransfer"
                addClass="layout palatte-blue-8 vbox"
                text={"傳輸相機影像(I): "+ ((this.state.DoImageTransfer) ?"暫停": "啟動")}
                onClick={() => this.getCameraImage_StartStop()}/>);
            
            MenuSet.push(
                <ObjInfoList IR={inspectionReport} checkResult2AirAction={this.checkResult2AirAction}
                key="ObjInfoList"
                WSCMD_CB={(tl, prop, data, uintArr)=>{this.props.ACT_WS_SEND(this.props.WS_ID,tl, prop, data, uintArr);}}
                />);
            break;
            case 1:
            CanvasWindowRatio=3;
            menuOpacity=0.3;
            break;
            
            case 2:
            CanvasWindowRatio=0;
            menuOpacity=0.3;

            
            MenuSet.push(
                <BASE_COM.IconButton
                dict={EC_zh_TW}
                iconType="up-square"
                key="DoImageTransfer"
                addClass="layout palatte-blue-8 vbox"
                text={"傳輸相機影像(I): "+ ((this.state.DoImageTransfer) ?"暫停": "啟動")}
                onClick={() => this.getCameraImage_StartStop()}/>);
            
            MenuSet.push(
                <ObjInfoList IR={inspectionReport} checkResult2AirAction={this.checkResult2AirAction}
                key="ObjInfoList"
                WSCMD_CB={(tl, prop, data, uintArr)=>{this.props.ACT_WS_SEND(this.props.WS_ID,tl, prop, data, uintArr);}}
                />);
            break;
            case 3:
            CanvasWindowRatio=3;
            menuOpacity=0.3;
            break;
        }

        return (
            <div className="HXF">
                
                <CanvasComponent_rdx addClass={"layout WXF"+" height"+CanvasWindowRatio} 
                    onCanvasInit={(canvas) => {this.ec_canvas = canvas}}/>
                <DataStatsTable className={"s scroll WXF"+" height"+(12-CanvasWindowRatio)} reportStatisticState={this.props.reportStatisticState}/>
                <$CSSTG transitionName="fadeIn">
                    <div key={"MENU"} className={"s overlay shadow1 scroll MenuAnim " + menu_height} 
                        style={{opacity:menuOpacity}}> 
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

        ACT_WS_SEND: (id, tl, prop, data, uintArr) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr)),

    }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
    return {
        c_state: state.UIData.c_state,
        shape_list: state.UIData.edit_info.list,
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