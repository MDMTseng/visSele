'use strict'
   

import { connect } from 'react-redux'
import React from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';
import $CSSTG  from 'react-addons-css-transition-group';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {round as roundX} from 'UTIL/MISC_Util';
import 'antd/dist/antd.css';


import EC_zh_TW from './languages/zh_TW';

import Table from 'antd/lib/table';

import * as logX from 'loglevel';
let log = logX.getLogger("AnalysisUI");


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }
  ec_canvas_EmitEvent(event){
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.DEFCONF_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent=this.ec_canvas_EmitEvent.bind(this);
    this.props.onCanvasInit(this.ec_canvas);
    this.updateCanvas(this.props.c_state);
  }
  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }
  updateCanvas(ec_state,props=this.props) {
    if(this.ec_canvas  !== undefined)
    {
      console.log("updateCanvas>>",props.edit_info);
      //if(props.edit_info.session_lock!=null && props.edit_info.session_lock.start == false)
      {
        this.ec_canvas.EditDBInfoSync(props.edit_info);
        this.ec_canvas.SetState(ec_state);
        //this.ec_canvas.ctrlLogic();
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width,height){
    if(this.ec_canvas  !== undefined)
    {
      this.ec_canvas.resize(width,height);
      this.updateCanvas(this.props.c_state);
      this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
    }
  }
  componentWillUpdate(nextProps, nextState) {
    
    console.log("CanvasComponent render",nextProps.c_state);
    //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];
    
    console.log(nextProps.edit_info.inherentShapeList);
    this.updateCanvas(nextProps.c_state,nextProps);
  }

  render() {
 
    return (
      <div className={this.props.addClass+" HXF"}>
        <canvas ref="canvas" className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
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



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => 
{ 
  return{
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(DefConfAct.Edit_Tar_Update(targetObj))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_SHAPELIST_UPDATE: (shapeList) => {dispatch(DefConfAct.Shape_List_Update(shapeList))},
    ACT_EDIT_SHAPE_SET: (shape_data) => {dispatch(DefConfAct.Shape_Set(shape_data))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);





class DataStatsTable extends React.Component{
  render() {
    let statstate = this.props.reportStatisticState;
    let measureList = statstate.statisticValue.measureList;

    
    let measureReports=measureList.map((measure)=>{

      let valArr = statstate.historyReport.map((reps)=>
        reps.judgeReports.find((rep)=>
          rep.id == measure.id)
        .value);

      let mean = valArr.reduce((sum,val)=>sum+val,0)/valArr.length;
      let sigma = Math.sqrt(valArr.reduce((sum,val)=>sum+(mean-val)*(mean-val),0)/valArr.length);


      let CPU = (measure.USL-mean)/(3*sigma);
      let CPL = (mean-measure.LSL)/(3*sigma);
      let CP = Math.min(CPU,CPL);
      let CA = (mean-measure.value)/((measure.USL-measure.LSL)/2);
      let CPK = CP*(1-Math.abs(CA));

      let id = measure.id;
      let name = measure.name;
      let subtype=measure.subtype;


      CPU=roundX(CPU,0.001);
      CPL=roundX(CPL,0.001);
      CP=roundX(CP,0.001);
      CA=roundX(CA,0.001);
      CPK=roundX(CPK,0.001);
      return {id,name,subtype,measure,valArr,mean,sigma,CPU,CPL,CP,CA,CPK};
    });
    

    log.error(measureReports);

    
    //statstate.historyReport.map((rep)=>rep.judgeReports[0]);
    const dataSource = measureReports;
    
    const columns = ["id","name","subtype","CA","CPU","CPL","CP","CPK"].map((type)=>({title:type,dataIndex:type,key:type}));

    let menuStyle={
      top:"0px",
      width:"100px"
    }
    return(
        <Table dataSource={dataSource} columns={columns} />
    );
  }
}










class APP_ANALYSIS_MODE extends React.Component{


  componentDidMount()
  {
    //this.props.ACT_WS_SEND(this.props.WS_ID,"EX",0,{imgsrc:"data/test1.bmp"});
    let defModelPath = this.props.edit_info.defModelPath;

    //this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{deffile:defModelPath+".json",imgsrc:defModelPath+".bmp"});
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }


  genTarEditUI(edit_tar)
  {
    switch(edit_tar.type)
    {
      case UIAct.SHAPE_TYPE.aux_point:
      case UIAct.SHAPE_TYPE.search_point:
      case UIAct.SHAPE_TYPE.measure:
      {
        return (<BASE_COM.JsonEditBlock object={edit_tar}
        dict={EC_zh_TW}
        dictTheme = {edit_tar.type}
          key="BASE_COM.JsonEditBlock"
          whiteListKey={{
            //id:"div",
            type:"div",
            subtype:"div",
            name:"input",
            //pt1:null,
            angleDeg:"input-number",
            value:"input-number",
            margin:"input-number",
            USL:"input-number",
            LSL:"input-number",
            UCL:"input-number",
            LCL:"input-number",
            quadrant:"div",
            docheck:"checkbox",
            width:"input-number",
            ref:{__OBJ__:"div",
              0:{__OBJ__:"btn",
                id:"div",
                element:"div"},
              1:{__OBJ__:"btn",
                id:"div",
                element:"div"},
            }
          }}
          jsonChange={(original_obj,target,type,evt)=>
            {
              if(type =="btn")
              {
                if(target.keyTrace[0]=="ref")
                {
                  this.props.ACT_EDIT_TAR_ELE_TRACE_UPDATE(target.keyTrace);
                }
              }
              else
              {
                let lastKey=target.keyTrace[target.keyTrace.length-1];
                
                switch(type)
                {
                  case "input-number":
                  {
                    let parseNum =  parseFloat(evt.target.value);
                    if(isNaN(parseNum))return;
                    target.obj[lastKey]=parseNum;
                    if( target.obj.value!== undefined)
                    {
                      //Special case, if USL LSL gets changes then UCL and LCL will be changed as well
                      let val = target.obj.value;
                      if(lastKey == "LSL")
                      {
                        target.obj.LCL=roundX((val+(target.obj.LSL-val)/3),0.001);
                      }
                      else if(lastKey == "USL")
                      {
                        target.obj.UCL=roundX((val+(target.obj.USL-val)/3),0.001);
                      }
                    }
                  }
                  break;
                  case "input":
                  {
                    target.obj[lastKey]=evt.target.value;
                  }
                  break;
                  case "checkbox":
                  {
                    target.obj[lastKey]=evt.target.checked;
                  }
                  break;
                }
                this.ec_canvas.SetShape( original_obj, original_obj.id);
              }
            }}/>);
      }
      break;
      default:
      {
        return (<BASE_COM.JsonEditBlock object={edit_tar}
          dict={EC_zh_TW}
          dictTheme = {edit_tar.type}
          key="BASE_COM.JsonEditBlock"
          jsonChange={(original_obj,target,type,evt)=>
            {
              let lastKey = target.keyTrace[target.keyTrace.length-1];
              if(type == "input-number")
                target.obj[lastKey]=parseFloat(evt.target.value);
              else if(type == "input")
                target.obj[lastKey]=evt.target.value;
    
              console.log(target.obj);
              let updated_obj=original_obj;
              this.ec_canvas.SetShape( updated_obj, updated_obj.id);
            }}
          whiteListKey={{
            //id:"div",
            type:"div",
            subtype:"div",
            name:"input",
            margin:"input-number",
            direction:"input-number",

            /*pt1:{
              __OBJ__:"btn",
              x:"input-number",
              y:"input-number",
            }*/
          }}/>);
      }
      break;
      
    }
  }



  render() {
    let menu_height="HXA";//auto
    let MenuSet=[
      <BASE_COM.IconButton
              dict={EC_zh_TW}
              key="<"
              addClass="layout black vbox"
              text="<" onClick={()=>this.props.ACT_EXIT()}/>];


    let menuStyle={
      top:"0px",
      width:"100px"
    }
    return(
    <div className="HXF">

      <div className="overlayCon s overlayCon width2 HXF">
        <$CSSTG transitionName = "fadeIn">
            <div key="k1" className={"s overlay scroll MenuAnim " + menu_height} style={menuStyle}>
              {MenuSet}
            </div>
        </$CSSTG>
      </div>
      <div className="s overlayCon width10 HXF">
        <DataStatsTable reportStatisticState={this.props.reportStatisticState} />
      </div>
    </div>
    );
  }
}


const mapDispatchToProps_APP_ANALYSIS_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
    
    ACT_Report_Save:(id,fileName,content)=>{
      dispatch(UIAct.EV_WS_SEND(id,"SV",0,
          {filename:fileName},
          content
      ));
    },
    ACT_Cache_Img_Save:(id,fileName)=>{
      dispatch(UIAct.EV_WS_SEND(id,"SV",0,
          {filename:fileName, type:"__CACHE_IMG__"}
      ));
    },
  }
}

const mapStateToProps_APP_ANALYSIS_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    edit_tar_info:state.UIData.edit_info.edit_tar_info,
    shape_list:state.UIData.edit_info.list,
    WS_ID:state.UIData.WS_ID,
    edit_info:state.UIData.edit_info,
    reportStatisticState:state.UIData.edit_info.reportStatisticState,
  }
};

const APP_ANALYSIS_MODE_rdx = connect(
    mapStateToProps_APP_ANALYSIS_MODE,
    mapDispatchToProps_APP_ANALYSIS_MODE)(APP_ANALYSIS_MODE);

export default APP_ANALYSIS_MODE_rdx;