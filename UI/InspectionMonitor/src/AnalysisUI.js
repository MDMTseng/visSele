'use strict'
   

import React from 'react';

import {round as roundX,GetObjElement} from './UTIL/MISC_Util';
import 'antd/dist/antd.css';

import moment from 'moment';
import Table from 'antd/lib/table';
import Button from 'antd/lib/button';

import * as logX from 'loglevel';


import * as DB_Query from './UTIL/DB_Query';

import Input from 'antd/lib/input';
import Slider from 'antd/lib/slider';
import TimePicker from 'antd/lib/time-picker';
import DatePicker from 'antd/lib/date-picker';

import ReactResizeDetector from 'react-resize-detector';

import Chart from 'chart.js';
import 'chartjs-plugin-annotation';


const { RangePicker } = DatePicker;

let log = logX.getLogger("AnalysisUI");

function convertInspInfo_stat(measureList,insp)
{
  let inspectionResults = insp;

  
  let measureReports=measureList.map((measure)=>{
    let valArr = inspectionResults.map((reps)=>
      reps.judgeReports.find((rep)=>
        rep.id === measure.id)
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
    if(pL===-1)return rep.valArr.length;
    if(pL===rep.valArr.length)return pL;
    return NaN;
  },-1);
  if(dataL===-1||dataL!==dataL)return [];


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

function InspectionRecordGrouping(InspectionRecord,largestInterval=2*60*1000)
{
  let fd = InspectionRecord;
  let inspGroups=[];
  let frontGroup=undefined;
  function AddNewGroup(new_rec)
  {
    frontGroup={
      group:[new_rec],
      startTime:new_rec.time_ms,
      endTime:new_rec.time_ms,
    }
    inspGroups.push(frontGroup);
  }
  AddNewGroup(fd[0]);
  for(let i=1;i<fd.length;i++)
  {
    if(fd[i].time_ms-frontGroup.endTime<largestInterval)
    {
      frontGroup.group.push(fd[i]);
      frontGroup.group.endTime=fd[i].time_ms;
    }
    else
    {
      AddNewGroup(fd[i]);
    }
  }
  return inspGroups;
}


const MEASURERSULTRESION=
{
  NA:"NA",
  UOK:"UOK",
  LOK:"LOK",
  
  UCNG:"UCNG",
  LCNG:"LCNG",

  USNG:"USNG",
  LSNG:"LSNG",
};


export const OK_NG_BOX_COLOR_TEXT = {
  [MEASURERSULTRESION.NA]:{COLOR:"#aaa",TEXT:MEASURERSULTRESION.NA},
  [MEASURERSULTRESION.UOK]:{COLOR:"#87d068",TEXT:MEASURERSULTRESION.UOK},
  [MEASURERSULTRESION.LOK]:{COLOR:"#87d068",TEXT:MEASURERSULTRESION.LOK},
  [MEASURERSULTRESION.UCNG]:{COLOR:"#d0d068",TEXT:MEASURERSULTRESION.UCNG},
  [MEASURERSULTRESION.LCNG]:{COLOR:"#d0d068",TEXT:MEASURERSULTRESION.LCNG},
  [MEASURERSULTRESION.USNG]:{COLOR:"#f50",TEXT:MEASURERSULTRESION.USNG},
  [MEASURERSULTRESION.LSNG]:{COLOR:"#f50",TEXT:MEASURERSULTRESION.LSNG},
};
function newDate(time_ms) {
  return moment(time_ms).toDate();
}
class ControlChart extends React.Component {
  constructor(props) {
      super(props);
      this.divID="ControlChart_ID_" + Math.round(Math.random() * 10000);
      this.charObj = undefined;
      this.state={
          chartOpt:{
              type: 'line',
              data:{labels:[],datasets:[{            
                  
                  borderColor:"rgba(100, 255, 100,0)",
                  lineTension: 0,data:[],
                  pointBackgroundColor:[]},
                {            
                  type: "scatter",
                  borderColor:"rgba(100, 255, 100)",
                  lineTension: 0,data:[],
                  pointBackgroundColor:[]}
                
                ]},
              bezierCurve : false,
              options: {
                  responsive: true,
                  scales: {
                      xAxes: [
                        {
                          display: true,
                          type: 'time',
                          distribution: "linear",
                          ticks: {
                            callback: function(value, index, values) {
                              return value.toString();
                            },
                            source: 'labels',
                            autoSkip: true
                          },
                          time:{
                            displayFormat:true,
                            displayFormats:{
                              hour: "MMM D, h:mm:ss"
                            },
                            minUnit:"hour"
                          }
                        }] ,
                      yAxes: [{
                        display: true,
                        scaleLabel: {
                          display: true,
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
          },
          

      };

      this.default_annotationTargets=[
          {type:"USL",color:"rgba(200, 0, 0,0.2)"},
          {type:"LSL",color:"rgba(200, 0, 0,0.2)"},
          {type:"UCL",color:"rgba(100, 100, 0,0.2)"},
          {type:"LCL",color:"rgba(100, 100, 0,0.2)"},
          {type:"value",color:"rgba(0, 0, 0,0.2)"},
      ];
  } 
  
  componentWillMount(nextProps, nextState){
  }
  componentWillUpdate(nextProps, nextState){
    this.PropsUpdate(nextProps);
  }
  PropsUpdate(nextProps) {
      
      //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
      this.state.chartOpt.data.labels=[];
      this.state.chartOpt.data.datasets.forEach((datInfo)=>{
          datInfo.data=[];
          datInfo.pointBackgroundColor=[];
      });
      let length = nextProps.reportArray.length;
      if(length===0)return;
      //let newTime = nextProps.reportArray[length-1].time_ms;
      this.state.chartOpt.options.title.text=nextProps.targetMeasure.name;

      nextProps.reportArray.reduce((acc_data,rep,idx)=>{
          //acc_data.labels.push(rep.time_ms);
          if(nextProps.xAxisRange!==undefined)
          {
            if(nextProps.xAxisRange[0]>rep.time_ms || 
              nextProps.xAxisRange[1]<rep.time_ms)return acc_data;
          }

          let measureObj = rep.judgeReports.find((jrep)=>jrep.id===nextProps.targetMeasure.id);
          if(measureObj.status===-128)return acc_data;
          let pointColor=undefined;
          let val={
            x:new Date(rep.time_ms).toString(),
            y:measureObj.value,
          };


          acc_data.datasets[0].pointBackgroundColor.push(pointColor);
          //TODO:for now there is only one data set in one chart
          acc_data.datasets[0].data.push(val);
          return acc_data;
      }, this.state.chartOpt.data );

      let inspGroup=InspectionRecordGrouping(nextProps.reportArray,this.props.groupInterval);



      inspGroup.reduce((acc_data,repG,idx)=>{

        let measureGList = repG.group.
          map(rep=>rep.judgeReports.
            find((jrep)=>
              jrep.id===nextProps.targetMeasure.id &&
              jrep.status!==-128
            ));
        

        let value = measureGList.reduce((sum,mg)=>sum+mg.value,0)/measureGList.length;
        let time = repG.group.reduce((sum,rep)=>sum+rep.time_ms,0)/measureGList.length;
        if(nextProps.xAxisRange!==undefined)
        {
          if(nextProps.xAxisRange[0]>time || 
            nextProps.xAxisRange[1]<time)return acc_data;
        }
        acc_data.labels.push(new Date(time));
        let pointColor=undefined;
        let val={
          x:new Date(time).toString(),
          y:value,
        };


        acc_data.datasets[1].pointBackgroundColor.push(pointColor);
        //TODO:for now there is only one data set in one chart
        acc_data.datasets[1].data.push(val);
        return acc_data;
    }, this.state.chartOpt.data );


      let annotationTargets=this.props.anotationTargets;
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
      if(this.charObj!==undefined)    
        this.charObj.update();
      console.log(this.state.chartOpt.options.scales);
  }


  componentDidMount() {
      var ctx = document.getElementById(this.divID).getContext("2d");

      this.charObj = new Chart(ctx, this.state.chartOpt);
      this.PropsUpdate(this.props);
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

function Date_addDay(date,addDays)
{
  if( date===undefined)date=new Date();

  return date.setDate(date.getDate() + addDays);
}

class APP_ANALYSIS_MODE extends React.Component{


  componentDidMount()
  {
  }

  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.state={
      defFileSearchName:"",
      dateRange:[moment(Date_addDay(new Date(),-7)), moment(Date_addDay(new Date(),1))],
      inspectionRec:[],
      groupInterval:10*60*1000//10 mins
    };
    //this.state.inspectionRec=dbInspectionQuery;
    //this.props.defFile=defFile;

//let IRG=InspectionRecordGrouping(dbInspectionQuery);
//console.log(IRG,defFile);
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }


  stateUpdate(obj) {
    return this.setState({...this.state,...obj});
  }

  render() {
    if(this.props.defFile===undefined)return null;
    let menu_height="HXA";//auto
    let MenuSet=[];
    //if()
    /*MenuSet=this.state.dateRange.reduce((menu,date,idx)=>{
      menu.push(<BASE_COM.IconButton
        dict={EC_zh_TW}
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
      (this.state.dateRange.length===2) &&
      (this.state.dateRange.reduce(
        (isReady,date)=> 
          ( isReady) && 
          ( date._d !==undefined) && 
          ( date._d.getTime !==undefined)
      ,true));

    let dateRange=this.state.dateRange.map(dr=>dr._d);
    let DefFileHash = this.props.DefFileHash;
    let DefFileName = this.props.DefFileName;
    let defFileReady = 
      isString(this.props.DefFileHash)&&
      this.props.DefFileHash.length>5;

    const dateFormat = 'YYYY/MM/DD';
    console.log( this.props.defFile);
    let measureList = this.props.defFile.featureSet[0].features.filter(feature=>feature.type==="measure");
    
    
    
    
    let graphCtrlUI=null;

    if(this.state.inspectionRec.length>0)
    {    
      graphCtrlUI = [
        <Slider range 
          min={this.state.inspectionRec[0].time_ms}
          max={this.state.inspectionRec[this.state.inspectionRec.length-1].time_ms}
          defaultValue={
              [this.state.inspectionRec[0].time_ms, 
              this.state.inspectionRec[this.state.inspectionRec.length-1].time_ms]} 
          step={1000*60*60*2}
          tipFormatter={(time)=>new Date(time).toString()}
          onChange={(data)=>this.stateUpdate({displayRange:data})}
        />,
      
        <TimePicker defaultValue={moment('0:10', 'HH:mm')} format={'HH:mm'} minuteStep={5} allowClear={false} 
          onChange={(t)=>
          {
            if(t===null)return;
            console.log(t);
            let mo=t._d.getTime();
            let day_base=moment(t._d).startOf('date')._d.getTime();
            console.log(mo-day_base)
            this.stateUpdate({groupInterval:mo-day_base})
        }}/>
      ]

    }

    
    let graphUI=null;
    graphUI = measureList.map(m=>
      <ControlChart reportArray={this.state.inspectionRec} 
        key={m.name+"_"}
        targetMeasure={m} 
        xAxisRange={this.state.displayRange}
        groupInterval={this.state.groupInterval}/>)
    
    
    return(
    <div className="HXF">
      
      <div className="overlayCon s overlayCon width12 HXF">
        
      
        <div className="s height12">
          <RangePicker key="RP" 
            defaultValue={this.state.dateRange} 
            onChange={(date)=>this.stateUpdate({dateRange:date})}/>

          <Button type="primary" icon="search" disabled={!dateRangeReady || !defFileReady} onClick={
            ()=>{

              DB_Query.inspectionQuery(DefFileHash,dateRange[0],dateRange[1],10000000)
                .then((queryResult)=>{
                  console.log(queryResult);
                  
                  let inspectionRec = queryResult.map(res=>res.InspectionData[0]);
                  
                  inspectionRec.sort(function(a, b) {
                    return a.time_ms - b.time_ms;
                  })
                  this.stateUpdate({inspectionRec});
                  //downloadString(JSON.stringify(inspectionRec), "text/csv", DefFileName+"_"+YYYYMMDD(new Date())+".csv");
                  //let csv_arr= convertInspInfo2CSV( this.props.reportStatisticState.statisticValue.measureList,inspectionRec);
                  //downloadString(csv_arr.join(''), "text/csv", "aaa.csv");
                })
                .catch((e)=>{
                  console.log(e);
                });
            }} />
            
          <Button type="primary" icon="download" disabled={!dateRangeReady || !defFileReady || this.state.inspectionRec.length===0} 
          onClick={
            ()=>{

              //let csv_arr= convertInspInfo2CSV( this.props.reportStatisticState.statisticValue.measureList,this.state.inspectionRec);
              //downloadString(csv_arr.join(''), "text/csv", DefFileName+"_"+YYYYMMDD(new Date())+".csv");
            }} />
        </div>

        {graphCtrlUI}
        {graphUI}
        <div key="k1" className={"s overlay scroll MenuAnim " + menu_height} style={menuStyle}>
          {MenuSet}
        </div>
      </div>
      
    </div>
    );
  }
}


export default APP_ANALYSIS_MODE;