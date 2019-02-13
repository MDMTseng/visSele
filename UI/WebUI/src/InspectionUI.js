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
import G2 from '@antv/g2';
import DataSet from '@antv/data-set';

import {SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';


import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';

import * as logX from 'loglevel';

let log = logX.getLogger(__filename);

import {
    Tag, Input, Select, Upload, Button, Menu, Dropdown, Icon,
} from 'antd';

const ButtonGroup = Button.Group;


const FileProps = {
    name: 'file',

    onChange(info) {
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

class OK_NG_BOX extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            // OK_NG: undefined,
        }
    }

    render() {
        return (
            <div>
                <Tag style={{'font-size': 20}}
                     color={this.props.OK_NG ? "#87d068" : "#f50"}>{this.props.OK_NG ? "OK" : "NG"}
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
                reportDetail = judgeReports.map((rep,idxx)=>
                    <Menu.Item key={"i"+idx+rep.name}>
                        <OK_NG_BOX OK_NG={rep.status==INSPECTION_STATUS.SUCCESS} >
                            {"#"+rep.value.toFixed(4)+"mm"}
                        </OK_NG_BOX>
                    </Menu.Item>
                );
                return (
                    <SubMenu style={{'text-align': 'left'}} key={"sub1"+idx}
                             title={<span><Icon type="paper-clip"/><span>{idx}</span></span>}>
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
                    selectable={true}
                    // style={{align: 'left', width: 200}}
                    defaultSelectedKeys={['i1']}
                    defaultOpenKeys={['sub1']}
                    mode="inline">
                    <SubMenu style={{'text-align': 'left'}} key="functionMenu"
                             title={<span><Icon type="setting"/><span>平台功能操作</span></span>}>
                        <AirControl
                            url={"ws://192.168.2.2:5213"}
                            checkResult2AirAction={this.props.checkResult2AirAction}

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
            transCameraImage: false
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

    getCameraImage_StartStop() {
        log.INFO("fun getCameraImage_StartStop click");
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
            return <BASE_COM.IconButton
                dict={EC_zh_TW}
                addClass="layout black vbox"
                text="Reconnect" onClick={() => {
                this.websocketConnect(this.props.url);
            }}/>;
        }

        return (

            <div>
                <Button block style={{marginTop: 2, marginBottom: 2}} type="primary" size="small"
                        onClick={() => this.getCameraImage_StartStop()}>
                    傳輸相機影像(I): {this.state.transCameraImage ? " 暫停" : " 啟動"}
                </Button>
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

class G2PointJitter extends React.Component {
    getInitialState() {
        return {
            data: [],
            forceFit: true,
            width: 500,
            height: 450,
            plotCfg: {
                margin: [80, 80, 0],
                background: {
                    stroke: '#ccc'
                }
            }
        };
    }

    componentDidMount() {
        const self = this;
        axios.get('../../../static/data/dv-grades.json').then(function (response) {
            self.setState({
                data: response.data
            });
        }).catch(function (error) {
            console.log(error);
        });
    }

    render() {
        return (
            <div>
                <Chart
                    data={this.state.data}
                    width={this.state.width}
                    height={this.state.height}
                    plotCfg={this.state.plotCfg}
                    forceFit={this.state.forceFit}/>
            </div>
        );
    }
}

class G2HOT extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            G2Chart: undefined,
            divID: "asdcsdacadsc" + Math.round(Math.random() * 10000),

            jdata: [{"name": "n1", "carat": 1000, "cut": "Ideal", "depth": 61.4, "table": 57, "price": 10},
                {"name": "n2", "carat": 2000, "cut": "Good", "depth": 64, "table": 57, "price": 20},
                {"name": "n3", "carat": 3000, "cut": "Ideal", "depth": 59.2, "table": 60, "price": 30},
                {"name": "n4", "carat": 4000, "cut": "Ideal", "color": "J", "depth": 63, "table": 56, "price": 40}]
        }

    }

    componentWillUpdate(nextProps, nextState) {

        this.state.jdata.push({"carat": 2000 + (Math.random() - 0.5) * 400, "price": 20 + (Math.random() - 0.5) * 1});
        let dv = new DataSet.View().source(this.state.jdata);
        dv.transform({
            type: 'kernel-smooth.density',
            fields: ['carat', 'price'],
            as: ['carat', 'price', 'density']
        });

        this.state.view.source(dv);

        this.state.G2Chart.source(this.state.jdata);
        this.state.G2Chart.render();
    }

    componentDidMount() {
        this.state.G2Chart = new G2.Chart({
            container: this.state.divID,
            //forceFit:true
        });

        this.state.G2Chart.legend({
            offset: 45
        });
        this.state.G2Chart.point().position('carat*price');

        this.state.view = this.state.G2Chart.view();
        this.state.view.axis(false);
        this.state.view.heatmap().position('carat*price').color('density', 'blue-cyan-lime-yellow-red');

    }

    onResize(width, height) {
        log.debug("G2HOT resize>>", width, height);
        this.state.G2Chart.changeSize(width, height);

    }

    render() {

        return <div className={this.props.className} id={this.state.divID}>

            <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
        </div>
    }

}

class G2LINE extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            G2Chart: undefined,
            divID: "asdcsdacadsc" + Math.round(Math.random() * 10000),

        }

    }

    componentWillUpdate(nextProps, nextState) {
        // if(nextProps.data===undefined)
        //   return;
        this.state.G2Chart.source(nextProps.data);
        this.state.G2Chart.interval().position('genre*sold').color('genre');
        this.state.G2Chart.render();
    }

    componentDidMount() {

        this.state.G2Chart = new G2.Chart({
            container: this.state.divID,
            //forceFit:true
        });
    }

    onResize(width, height) {
        log.debug("G2HOT resize>>", width, height);
        this.state.G2Chart.changeSize(width, height);

    }

    render() {

        return <div className={this.props.className} id={this.state.divID}>

            <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
        </div>
    }

}

class CanvasComponent extends React.Component {
    constructor(props) {
        super(props);

    }

    ec_canvas_EmitEvent(event) {
        log.debug(event);
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
            if (props.edit_info.session_lock != null && props.edit_info.session_lock.start == false) {
                this.ec_canvas.EditDBInfoSync(props.edit_info);
                this.ec_canvas.SetState(ec_state);
                //this.ec_canvas.ctrlLogic();
                this.ec_canvas.draw();
            }
        }
    }

    onResize(width, height) {
        if (this.ec_canvas !== undefined) {
            this.ec_canvas.resize(width, height);
            this.updateCanvas(this.props.c_state);
            this.ec_canvas.ctrlLogic();
            this.ec_canvas.draw();
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
    }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);

class APP_INSP_MODE extends React.Component {


    componentDidMount() {

        this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, {deffile: this.props.defModelPath + ".json"});

    }

    componentWillUnmount() {
        this.props.ACT_WS_SEND(this.props.WS_ID, "CI", 0, {});

    }

    constructor(props) {
        super(props);
        this.ec_canvas = null;
        this.checkResult2AirAction = {direction: "none", ver: 0};
        // this.IR = undefined;
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
        let InfoSet = [];
        let menu_height = "HXA";//auto
        log.debug("CanvasComponent render");
        MenuSet = [
            <BASE_COM.IconButton
                dict={EC_zh_TW}
                key="<"
                addClass="layout black vbox"
                text="<" onClick={this.props.ACT_EXIT}/>
            ,
            <ObjInfoList IR={inspectionReport} checkResult2AirAction={this.checkResult2AirAction}/>

        ];

        return (
            <div className="HXF">

                <CanvasComponent_rdx addClass="layout width12 height12" onCanvasInit={(canvas) => {
                    this.ec_canvas = canvas
                }}/>

                <$CSSTG transitionName="fadeIn">
                    <div key={"MENU"} className={"s overlay scroll MenuAnim " + menu_height}>
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
        //reportStatisticState:state.UIData.edit_info.reportStatisticState

    }
};

const APP_INSP_MODE_rdx = connect(
    mapStateToProps_APP_INSP_MODE,
    mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;