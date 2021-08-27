'use strict'
   

import { connect } from 'react-redux'
import React from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import $CSSTG  from 'react-addons-css-transition-group';

import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {round as roundX,GetObjElement} from 'UTIL/MISC_Util';


import Table from 'antd/lib/table';
import Button from 'antd/lib/button';

import * as logX from 'loglevel';


import * as DB_Query from 'UTIL/DB_Query';

import Input from 'antd/lib/input';
import DatePicker from 'antd/lib/date-picker';
const { RangePicker } = DatePicker;

let log = logX.getLogger("AnalysisUI");

function convertInspInfo_stat(measureList,insp)
{
  let inspectionResults = insp;

  
  let measureReports=measureList.map((measure)=>{
    let valArr = inspectionResults.map((reps)=>
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
  console.log(measureReports);
  return measureReports;
}


class DataStatsTable extends React.Component{
  render() {
    let inspectionResults = this.props.inspectionResults;
    let measureList = this.props.reportStatisticState.statisticValue.measureList;

    
    let measureReports=convertInspInfo_stat(measureList,inspectionResults);
    
    
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

function downloadString(text, fileType, fileName) {
  var blob = new Blob([text], { type: fileType });
// downloadString("a,b,c\n1,2,3", "text/csv", "myCSV.csv")
  var a = document.createElement('a');
  a.download = fileName;
  a.href = URL.createObjectURL(blob);
  a.dataset.downloadurl = [fileType, a.download, a.href].join(':');
  a.style.display = "none";
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  setTimeout(function() { URL.revokeObjectURL(a.href); }, 1500);
}

function isString (value) {
  return typeof value === 'string' || value instanceof String;
}

function convertInspInfo2CSV(measureList,insp)
{
  let measureReports=convertInspInfo_stat(measureList,insp);
  let ci=[];


  let dataL = measureReports.reduce((pL,rep)=>{
    if(pL==-1)return rep.valArr.length;
    if(pL==rep.valArr.length)return pL;
    return NaN;
  },-1);
  if(dataL==-1||dataL!=dataL)return [];


  function pushDataRow(arr,measureReports,trace,RowName)
  {
    if(isString(trace))trace=[trace];
    if(RowName===undefined)RowName=trace[trace.length-1];
    ci.push(RowName+",");
    measureReports.forEach((rep)=>{
      let ele = GetObjElement(rep,trace);
      if(ele===undefined)ele='';
      ci.push(ele+",");
    });
    ci.push("\n");
  }
  ["name","subtype"]
    .forEach(field=>pushDataRow(ci,measureReports,field));

  ["value","LCL","UCL","LSL","USL"]
    .forEach(field=>pushDataRow(ci,measureReports,["measure",field]));

  ["mean","sigma","CA","CPU","CPL","CP","CPK"]
    .forEach(field=>pushDataRow(ci,measureReports,field));

  measureReports.forEach((rep)=>{
    ci.push(",");
  });ci.push("\n");

  let dateL = measureReports[0].valArr.length;
  for(let i=0;i<dateL;i++)
  {
    pushDataRow(ci,measureReports,["valArr",i],insp[i].time_ms);
  }


  return ci;
}

function YYYYMMDD(date)
{
  var mm = date.getMonth() + 1; // getMonth() is zero-based
  var dd = date.getDate();

  return [date.getFullYear(),
          (mm>9 ? '' : '0') + mm,
          (dd>9 ? '' : '0') + dd
         ].join('');
}

class APP_ANALYSIS_MODE extends React.Component{


  componentDidMount()
  {
    let defModelPath = this.props.edit_info.defModelPath;
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.state={
      defFileSearchName:"",
      dateRange:[],
      inspectionRec:[]
    };
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
        dict={this.props.DICT}
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
            importance:"input-number",
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
          dict={this.props.DICT}
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


  stateUpdate(obj) {
    return this.setState({...this.state,...obj});
  }

  render() {
    let menu_height="HXA";//auto
    let MenuSet=[
      <BASE_COM.IconButton
              dict={this.props.DICT}
              key="<"
              addClass="layout black vbox"
              text="<" onClick={()=>this.props.ACT_EXIT()}/>];
    //if()
    /*MenuSet=this.state.dateRange.reduce((menu,date,idx)=>{
      menu.push(<BASE_COM.IconButton
        dict={this.props.DICT}
        key={"<"+idx}
        addClass="layout black vbox"
        text={date._d.getTime()} />);
      return menu;
    },MenuSet);*/
    let menuStyle={
      top:"0px",
      width:"100px"
    }

    let dateRangeReady = 
      (this.state.dateRange.length==2) &&
      (this.state.dateRange.reduce(
        (isReady,date)=> 
          ( isReady) && 
          ( date._d !==undefined) && 
          ( date._d.getTime !==undefined)
      ,true));

    let dateRange=this.state.dateRange.map(dr=>dr._d);
    let DefFileHash = this.props.edit_info.DefFileHash;
    let DefFileName = this.props.edit_info.DefFileName;
    let defFileReady = 
      isString(this.props.edit_info.DefFileHash)&&
      this.props.edit_info.DefFileHash.length>5;
    return(
    <div className="HXF">
      
      <div className="overlayCon s overlayCon width12 HXF">
        
      
        <div className="s height12">
          <RangePicker key="RP" onChange={(date)=>this.stateUpdate({dateRange:date})}/>

          <Button type="primary" icon="search" disabled={!dateRangeReady || !defFileReady} onClick={
            ()=>{

              DB_Query.inspectionQuery(DefFileHash,dateRange[0],dateRange[1])
                .then((queryResult)=>{
                  console.log(queryResult);
                  
                  let inspectionRec = queryResult.map(res=>res.InspectionData[0]);
                  this.stateUpdate({inspectionRec});
                  let csv_arr= convertInspInfo2CSV( this.props.reportStatisticState.statisticValue.measureList,inspectionRec);
                  //downloadString(csv_arr.join(''), "text/csv", "aaa.csv");
                })
                .catch((e)=>{
                  console.log(e);
                });
            }} />
            
          <Button type="primary" icon="download" disabled={!dateRangeReady || !defFileReady || this.state.inspectionRec.length==0} 
          onClick={
            ()=>{

              let csv_arr= convertInspInfo2CSV( this.props.reportStatisticState.statisticValue.measureList,this.state.inspectionRec);
              downloadString(csv_arr.join(''), "text/csv", DefFileName+"_"+YYYYMMDD(new Date())+".csv");
            }} />

          <DataStatsTable 
            reportStatisticState={this.props.reportStatisticState} 
            inspectionResults={this.state.inspectionRec}
            />
        </div>

        
        <$CSSTG transitionName = "fadeIn">
            <div key="k1" className={"s overlay scroll MenuAnim " + menu_height} style={menuStyle}>
              {MenuSet}
            </div>
        </$CSSTG>
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
    
    ACT_WS_SEND_BPG:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND_BPG(id,tl,prop,data,uintArr)),
    
    ACT_Report_Save:(id,fileName,content)=>{
      dispatch(UIAct.EV_WS_SEND_BPG(id,"SV",0,
          {filename:fileName},
          content
      ));
    },
    ACT_Cache_Img_Save:(id,fileName)=>{
      dispatch(UIAct.EV_WS_SEND_BPG(id,"SV",0,
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
    CORE_ID:state.ConnInfo.CORE_ID,
    edit_info:state.UIData.edit_info,
    reportStatisticState:state.UIData.edit_info.reportStatisticState,
    DICT:state.UIData.DICT,
  }
};

const APP_ANALYSIS_MODE_rdx = connect(
    mapStateToProps_APP_ANALYSIS_MODE,
    mapDispatchToProps_APP_ANALYSIS_MODE)(APP_ANALYSIS_MODE);

export default APP_ANALYSIS_MODE_rdx;