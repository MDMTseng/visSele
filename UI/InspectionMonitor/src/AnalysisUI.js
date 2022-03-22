'use strict'
   

import React, { useState, useEffect,useRef } from 'react';
import {round as roundX,GetObjElement} from './UTIL/MISC_Util';

import {datePrintSimple} from './UTIL/MISC_Util';
import 'antd/dist/antd.css';

import dclone from 'clone';
import moment from 'moment';
import Table from 'antd/lib/table';
import Button from 'antd/lib/button';

import * as logX from 'loglevel';
import 'chartjs-plugin-zoom'

import * as DB_Query from './UTIL/DB_Query';

import Input from 'antd/lib/input';
import Slider from 'antd/lib/slider';
import TimePicker from 'antd/lib/time-picker';
import DatePicker from 'antd/lib/date-picker';
import Checkbox from 'antd/lib/checkbox';

import  Typography  from 'antd/lib/typography';
import ReactResizeDetector from 'react-resize-detector';

import Chart from 'chart.js';
import 'chartjs-plugin-annotation';
import {INSPECTION_STATUS} from './UTIL/BPG_Protocol';
import Tag  from 'antd/lib/tag';

import Modal  from 'antd/lib/modal';



// import { StarOutlined} from '@ant-design/icons';
const { CheckableTag } = Tag;

const { RangePicker } = DatePicker;
const { Title, Paragraph, Text } = Typography;
let log = logX.getLogger("AnalysisUI");


Chart.pluginService.register({
  afterDraw: function(chart) {
  }
});

function branchBeizer(ctx,pt1,pt2,room=5,alpha=0.5)
{

  ctx.beginPath();
  ctx.moveTo(pt1.x, pt1.y);
  let c1x=alpha*(pt1.x-pt2.x)+pt2.x;
  let c2x=alpha*(pt2.x-pt1.x)+pt1.x;
  ctx.bezierCurveTo(c1x, pt1.y, c2x, pt2.y, pt2.x, pt2.y);
  ctx.arc(pt2.x, pt2.y, 5, 0, 2 * Math.PI, true);
  ctx.stroke(); 
}

function calcCpk(mean,sigma,USL,LSL,target)
{
  
  let CPU = (USL-mean)/(3*sigma);
  let CPL = (mean-LSL)/(3*sigma);
  let CP = Math.min(CPU,CPL);
  let CA = (mean-target)/((USL-LSL)/2);
  let CPK = CP*(1-Math.abs(CA));
  return {mean,sigma,CPU,CPL,CP,CA,CPK}
}
function copyStringToClipboard (str) {
  // Create new element
  var el = document.createElement('textarea');
  // Set value (string to be copied)
  el.value = str;
  // Set non-editable to avoid focus and move outside of view
  el.setAttribute('readonly', '');
  el.style = {position: 'absolute', left: '-9999px'};
  document.body.appendChild(el);
  // Select text inside element
  el.select();
  // Copy text to clipboard
  document.execCommand('copy');
  // Remove temporary element
  document.body.removeChild(el);
}

function downloadString(text, fileType, fileName) {
  var blob = new Blob([new Uint8Array([0xef, 0xbb, 0xbf]),text], { type: fileType });
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

function convertInspInfo2CSV(reportName,measureList,inspRecGroup,cur_tagState)
{
  let converterV="0.0.1 Alpha"
  let ci=[];
  
  ci.push('"'+reportName+'"');
  ci.push(",\""+converterV+'"');

  ci.push(',,"tags"');
  ci.push(",\""+Object.keys(cur_tagState).filter(key=>cur_tagState[key]==1).join(",")+'"');
  ci.push(",\""+Object.keys(cur_tagState).filter(key=>cur_tagState[key]==0 || cur_tagState[key]===undefined).join(",")+'"');
  ci.push(",\""+Object.keys(cur_tagState).filter(key=>cur_tagState[key]==-1).join(",")+'"');
  

  ci.push(",\n");
  console.log(measureList,inspRecGroup);
  function pushDataRow(arr,measureReports,trace,RowName=trace[trace.length-1])
  {
    arr.push('"'+RowName+'"');
    measureReports.forEach((rep)=>{
      let ele = GetObjElement(rep,trace);
      if(ele===undefined)ele='';
      arr.push(",\""+ele+'"');
    });
    let additional


    arr.push("\n");
  }
  ["name","subtype"]
    .forEach(field=>pushDataRow(ci,measureList,[field]));

   

  ["USL","LSL","value","UCL","LCL"]
    .forEach(field=>pushDataRow(ci,measureList,[field]));

  ci.push(",\n");

  let additionalColumn=["tag"]
  inspRecGroup.forEach(s_group=>{
    s_group.group
      .forEach(data=>{
        let rep=data.judgeReports;
        ci.push("'"+datePrintSimple(new Date(data.time_ms)));
        //The "'" in the head of time will let Excel left alingn time string, and have better lookup exprience
        measureList.forEach(m=>
          {
            let s_rep = rep.find(s_rep=>s_rep.id==m.id);
            if(s_rep.status!==INSPECTION_STATUS.NA)
              ci.push(","+s_rep.value);
            else
              ci.push(",");
          })
        additionalColumn.forEach(name=>{
          ci.push(",");
          switch(name)
          {
            case "tag":
              let tag=data.tag;
              if(tag===undefined)tag="";
              else
              {
                tag=tag.replace(/^\,+/g, "").replace(/\,{2,}/g, ",")
              }
              ci.push('"'+tag+'"');
              break;
          }
          
        })
        ci.push("\n");
        
      });
  })
  /*
  measureReports.forEach((rep)=>{
    ci.push(",");
  });ci.push("\n");

  let dateL = measureReports[0].valArr.length;
  for(let i=0;i<dateL;i++)
  {
    pushDataRow(ci,measureReports,["valArr",i],insp[i].time_ms);
  }*/


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
    if(new_rec===undefined)return;
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
      frontGroup.endTime=fd[i].time_ms;
    }
    else
    {
      AddNewGroup(fd[i]);
    }
  }

  inspGroups.forEach(frontGroup=>{
    let g= frontGroup.group;
    if(g.length==0)return;

    let stat = g[0].judgeReports.map(x=>({id:x.id}));
    stat=stat.map(s_stat=>{

      let measures=g.map(singleRep=>singleRep.judgeReports.find(measure=>measure.id==s_stat.id));
      let availData= measures.filter((measure)=>measure.status !==INSPECTION_STATUS.NA);
      let mean = availData.reduce((sum,measure)=>sum+measure.value,0)/availData.length;
      
      let minMax = measures.reduce((mM,measure)=>{
        if(measure.status ===INSPECTION_STATUS.NA)
        {
          return mM;
        }


        if(mM.max===undefined || mM.max===null || measure.value>mM.max)
          mM.max = measure.value;
        if(mM.min===undefined || mM.min===null || measure.value<mM.min)
          mM.min = measure.value;
        
        return mM;

      },{
        min:undefined,
        max:undefined
      });
      let sigma = Math.sqrt(availData.reduce((sum,measure)=>sum+(mean-+measure.value)*(mean-+measure.value),0)/availData.length);

      return {
        id:s_stat.id,
        mean,sigma,
        min:minMax.min,
        max:minMax.max
      }
    })


    frontGroup.stat=stat;


  })


  return inspGroups;
}

function InspectionRecordGroup_AppendCPK(InspRecGroup,defInspRange)
{
  InspRecGroup.forEach(group=>{
    group.stat.forEach(s_stat=>{
      let defRange = defInspRange.find(s_def=>s_def.id==s_stat.id);
      if(defRange===undefined)return;
      let cpkInfo = calcCpk(s_stat.mean,s_stat.sigma,defRange.USL,defRange.LSL,defRange.value);
      Object.assign(s_stat, cpkInfo);
    })
  })
}


function inspectionRecGroup_Generate(inspectionRec,groupInterval,measureList)
{
    let inspectionRecGroup = InspectionRecordGrouping(inspectionRec,groupInterval);
    InspectionRecordGroup_AppendCPK(inspectionRecGroup,measureList);
    return inspectionRecGroup;
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
                              hour: "M/D h:mm:ss"
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
                      display: true
                  },
                  tooltips: {
                      enabled: true
                  }
              }
          },
          

      };

      this.state.chartOpt.options.pan={
          enabled: true,
          mode: 'y' // is panning about the y axis neccessary for bar charts?
        }
      this.state.chartOpt.options.zoom={
          enabled: true,
          mode: 'y',
      }



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

  genSingleRepPoints(_inspectionRecGroup,_targetMeasure,_xAxisRange,color)
  {
    let value_target=_targetMeasure["value"];
    return _inspectionRecGroup
        .reduce((sumG,recG)=>sumG.concat(recG.group),[])//make [{group:[a,b]},{group:[c,d]}] => [a,b,c,d...]
        .reduce((acc_chart_data,rep,idx)=>{
          //acc_data.labels.push(rep.time_ms);
          if(_xAxisRange!==undefined)
          {
            if(_xAxisRange[0]>rep.time_ms || 
              _xAxisRange[1]<rep.time_ms)return acc_chart_data;
          }

          let measureObj = rep.judgeReports.find((jrep)=>jrep.id===_targetMeasure.id);
            

          let measureValue=measureObj.value;
          let pointColor=color;

          if(pointColor===undefined)
          {
            switch(measureObj.status)
              {
              case INSPECTION_STATUS.SUCCESS:
                pointColor="rgba(0,255,200,0.2)";
              break;
              case INSPECTION_STATUS.FAILURE:
                pointColor="rgba(255,0,200,0.2)";
              break;
              case INSPECTION_STATUS.NA:
                pointColor="#000000";
                measureValue=value_target;
              break;
              }
          }
          else
          {
            if(measureObj.status === INSPECTION_STATUS.NA)
            {
              pointColor="#000000";
              measureValue=value_target;
            }
          }

            let val={
            x:datePrintSimple(new Date(rep.time_ms)),
            y:measureValue,
            tag:rep.tag
            };
            

          acc_chart_data.pointBackgroundColor.push(pointColor);
          //TODO:for now there is only one data set in one chart
          acc_chart_data.data.push(val);
          return acc_chart_data;
      }, { 
        borderColor:"rgba(100, 255, 100,0)",
        lineTension: 0,data:[],
        pointBackgroundColor:[]});
  }

  dataPointNormalizer(value,USL,targetValue,LSL,bestEffert=true)
  {
    if(USL==LSL)return 1/0.0;
    if(bestEffert)
    {
      if(targetValue<=LSL)
      {
        LSL=targetValue-(USL-targetValue);
      }
      else if(targetValue>=USL)
      {
        USL = targetValue+(targetValue-LSL);
      }
      if(USL<=targetValue || LSL >=targetValue)return 1/0.0;
    }

    let mValue;
    //console.log(value,USL,targetValue,LSL);
    if(value>targetValue)
    {
      mValue=(value-targetValue)/(USL-targetValue);
    }
    else
    {
      mValue=(value-targetValue)/(targetValue-LSL);
    }
    return mValue;
  }

  genGroupRepPoints(_inspectionRecGroup,_targetMeasure,_xAxisRange,color)
  {
    return _inspectionRecGroup.reduce((acc_chart_data,repG)=>{
      let this_id_stat = repG.stat.
        find((st)=>st.id===_targetMeasure.id);
      
      let value =this_id_stat.mean;
      let time = repG.group.reduce((sum,rep)=>sum+rep.time_ms,0)/repG.group.length;
      if(_xAxisRange!==undefined)
      {
        if(_xAxisRange[0]>time || 
          _xAxisRange[1]<time)return acc_chart_data;
      }
      let pointColor=(color===undefined)?"rgba(0,255,0,1)":color;
        let val={
          x:datePrintSimple(new Date(time)),
        y:value,
          data:repG,
          stat:this_id_stat
        };

      
      acc_chart_data.pointBackgroundColor.push(pointColor);
      //TODO:for now there is only one data set in one chart
      acc_chart_data.data.push(val);
      return acc_chart_data;
    }, { 
      type: "line",
      borderColor:"rgba(100, 255, 100,1)",
      lineTension: 0,data:[],
      pointBackgroundColor:[],} );
  }


  colorPalette(idx,arrLen,alpha=1,hueOffset=0,sat="100%",light="50%"){
    let hue=360*idx/arrLen+hueOffset;
    return "hsla("+hue+", "+sat+", "+light+","+alpha+")";
  }
  PropsUpdate(nextProps) {
    console.log(">>>>");
    let doMultipleChart=nextProps.targetMeasure.length!=1;
    if(this.targetMeasure_cache != nextProps.targetMeasure ||
      this.inspectionRecGroup != nextProps.inspectionRecGroup)
    {
      this.doChartDataUpdate(nextProps,doMultipleChart);
    }
    this.targetMeasure_cache = nextProps.targetMeasure;
    this.inspectionRecGroup = nextProps.inspectionRecGroup;

    let N_LSL=-100;
    let N_USL=100;
    let N_value_target=0;
    if(doMultipleChart)
    {
      N_LSL=-100;
      N_USL=100;
      N_value_target=0;
    }
    else
    {
      let _targetMeasure = nextProps.targetMeasure[0];
      N_LSL=_targetMeasure["LSL"];
      N_USL=_targetMeasure["USL"];
      N_value_target=_targetMeasure["value"];
    }

    if(N_LSL!=0 || N_USL!=0 || N_value_target!=0)
    {
      this.state.chartOpt.options.scales.yAxes[0].ticks={
        min:1.2*(N_LSL-N_value_target)+N_value_target,
        max:1.2*(N_USL-N_value_target)+N_value_target,
      };
    }
    else
    {

    }

    this.charObj.update();
      
    //console.log(this.state.chartOpt.options.scales);

  }
      
  doChartDataUpdate(nextProps,doMultipleChart){
      
      //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
      this.state.chartOpt.data.labels=[];
      this.state.chartOpt.data.datasets.forEach((datInfo)=>{
          datInfo.data=[];
          datInfo.pointBackgroundColor=[];
        });

        
      let length = nextProps.inspectionRecGroup.length;
      if(length===0)return;


      let _inspectionRecGroup = nextProps.inspectionRecGroup;
      let _xAxisRange = nextProps.xAxisRange;


        
      let N_LSL=-100;
      let N_USL=100;
      let N_value_target=0;
      this.state.chartOpt.data.datasets=[];
      if(doMultipleChart)
      {
        N_LSL=-100;
        N_USL=100;
        N_value_target=0;
      }
      else
      {
        let _targetMeasure = nextProps.targetMeasure[0];
        N_LSL=_targetMeasure["LSL"];
        N_USL=_targetMeasure["USL"];
        N_value_target=_targetMeasure["value"];
      }
      //this.state.chartOpt.options.legend.display=(nextProps.targetMeasure.length>1);
      nextProps.targetMeasure.forEach((_targetMeasure,idx,arr)=>{
        
        let pointColor=arr.length>1?this.colorPalette(idx,arr.length,0.4,100):undefined;
        let chart_data = this.genSingleRepPoints(_inspectionRecGroup,_targetMeasure,_xAxisRange,pointColor);
        let groupColor=this.colorPalette(idx,arr.length,1,100);
        let chart_group_data = this.genGroupRepPoints(_inspectionRecGroup,_targetMeasure,_xAxisRange,groupColor);

        if(doMultipleChart)
        {
          let LSL=_targetMeasure["LSL"];
          let USL=_targetMeasure["USL"];
          let value_target=_targetMeasure["value"];
    
          chart_data.data.forEach(dat=>{

            dat.original_y=dat.y
            dat.y=
              (N_USL)*this.dataPointNormalizer(dat.y,USL,value_target,LSL)});
  
          chart_group_data.data.forEach(dat=>{
            dat.original_y=dat.y
            dat.y=
              (N_USL)*this.dataPointNormalizer(dat.y,USL,value_target,LSL)});
        }
        else
        {
          chart_data.data.forEach(dat=>{dat.original_y=dat.y});
          chart_group_data.data.forEach(dat=>{dat.original_y=dat.y});
        }
        //console.log(chart_data.data);
        let dataSet = this.state.chartOpt.data.datasets;
        dataSet.push(chart_data);
        dataSet.push(chart_group_data);
        
        dataSet[dataSet.length-2].label="";
        dataSet[dataSet.length-1].label=_targetMeasure.name;
        
        //dataSet[dataSet.length-2].borderColor=(pointColor);
        dataSet[dataSet.length-2].backgroundColor=(pointColor);

        dataSet[dataSet.length-1].borderColor=(groupColor);
        dataSet[dataSet.length-1].backgroundColor=(groupColor);
        //dataSet[dataSet.length-1].pointRadius=10;

      })
      this.state.chartOpt.data.labels = 
      _inspectionRecGroup.map(g=>datePrintSimple(new Date((g.startTime+g.endTime)/2)));


      let annotationTargets=this.props.anotationTargets;
      if(annotationTargets===undefined)
      {
          annotationTargets = this.default_annotationTargets
      }

      let annoT={
        USL:N_USL,
        LSL:N_LSL,
        value:N_value_target,
      };//_targetMeasure;

      this.state.chartOpt.options.annotation.annotations = 
          annotationTargets.map((annotationTar) => {
          
              let val = annoT[annotationTar.type];
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
                      content: ""//annotationTar.type+":"+val
                  },
                  onMouseover: function (e) {
                    document.getElementById("info").innerHTML = 'whatever';
                  },
              });
          });
      
      this.state.chartOpt.options.tooltips={
        callbacks: {
          title: function(tooltipItem, data) {
            // let datasetIndex = tooltipItem[0].datasetIndex;
            // let index = tooltipItem[0].index;

            // return data.datasets[datasetIndex].data[index].original_y
            return ""
          },
          label: function(tooltipItem, data) {

            let datasetIndex = tooltipItem.datasetIndex;
            if(datasetIndex===undefined)return ""
            let index = tooltipItem.index;

            console.log(data,data.datasets[datasetIndex]);
            return data.datasets[datasetIndex].data[index].original_y
          },
          afterLabel: function(tooltipItem, data) {
            let datasetIndex = tooltipItem.datasetIndex;
            let index = tooltipItem.index;
            let dataOnTip=data.datasets[datasetIndex].data[index];
            let stat = dataOnTip.stat;
            if(stat===undefined)return dataOnTip.x+"\ntag:"+dataOnTip.tag;

            let groupSize = dataOnTip.data.group.length;
            if(groupSize==0)return dataOnTip.x;
            let str_arr=[
              moment(dataOnTip.data.group[0].time_ms).format("YYYY/MM/DD , h:mm:ss a"),
              moment(dataOnTip.data.group[groupSize-1].time_ms).format("YYYY/MM/DD , h:mm:ss a")
            ];


            let str = Object.keys(stat).map(key=>key+":"+
              ((stat[key]!=null)?stat[key].toFixed(4):"NULL")
            );
            str_arr = str_arr.concat(str);
            if(dataOnTip.data.group!==undefined)
              str_arr.push("count:"+dataOnTip.data.group.length);
            return str_arr;


          }
        },
        backgroundColor: '#FFF',
        titleFontSize: 16,
        titleFontColor: '#0066ff',
        bodyFontColor: '#000',
        bodyFontSize: 14,
        displayColors: true
      }

  }


  componentDidMount() {
      var ctx = document.getElementById(this.divID).getContext("2d");

      this.charObj = new Chart(ctx, this.state.chartOpt);
      this.PropsUpdate(this.props);
  }

  render() {
    return <div className={this.props.className} style={this.props.style}> 
        <canvas id={this.divID}  style={{height: "100%"}} className={this.props.className}/>
    </div>
  }

}

function Date_addDay(date,addDays)
{
  if( date===undefined)date=new Date();

  return date.setDate(date.getDate() + addDays);
}

class InspRecStream
{
  constructor(){
    this.reset();
    this.newFeedCallBack=(newStream,fullStream)=>console.log("newFeedCallBack=>",newStream,fullStream);
    this.liveFeedMode=false;
  }
  
  resetStreamInfo()
  {
    this.rec=[];
    this.liveFeedInterval=10000;
    this.passiveQueryRange=[];
    this.liveQueryRange=[];
    if(this.timeoutHDL!==undefined)
    {
      clearTimeout(this.timeoutHDL);
    }
    this.timeoutHDL=undefined;
  }
  reset()
  {
    this.defFile=undefined;
    this.resetStreamInfo();
  }

  setDefFile(defFile)
  {
    this.resetStreamInfo();
    this.defFile=JSON.parse(JSON.stringify(defFile));
    console.log("setDefFile=",defFile);
  }

  newStreamFeed(inspectionRec)
  {
    inspectionRec.sort(function(a, b) {
      return a.time_ms - b.time_ms;
    })

    if(this.rec.length>0)
    {
      let lastMsInRec = this.rec[this.rec.length-1].time_ms;
      let recAfterLast = inspectionRec.filter((srec)=>srec.time_ms>lastMsInRec);
      let concatArr = this.rec.concat(recAfterLast);
      console.log( this.rec.length,inspectionRec.length,concatArr.length);
      
      this.rec = concatArr;

    }
    else
    {
      this.rec = inspectionRec;
    }
    this.newFeedCallBack(inspectionRec,this.rec);
  }
  setLiveFeedMode(enable){
    console.log(enable);
    if(this.rec.length<0)
    {
      enable=false;
    }

    console.log(this.rec);
    if(enable)
    {
      
      if(this.timeoutHDL==undefined)
      {
        this.liveQueryInspRec();
      }
    }
    else
    {
      if(this.timeoutHDL!==undefined)
      {
        clearTimeout(this.timeoutHDL);
        this.timeoutHDL=undefined;
      }
    }
    return this.liveFeedMode=enable;
  }

  liveQueryInspRec(timeRange,maxResults,sampleCount)
  {
    if(timeRange==undefined)
    {
      if(this.rec.length>0)
      {
        let lastMsInRec = this.rec[this.rec.length-1].time_ms;
        timeRange=[moment(lastMsInRec)._d.getTime(), moment(Date_addDay(new Date(),1))._d.getTime()];
        // console.log(lastMsInRec,moment(lastMsInRec));
      }
      else
      {
        console.log("No existed rec to do live query");
        
        this.timeoutHDL =undefined;
        return undefined;
      }
    }
    console.log("timeRange="+timeRange);
    return DB_Query.inspectionQuery(this.defFile.featureSet_sha1,timeRange[0],timeRange[1],maxResults,sampleCount)
    .then((queryResult)=>{
      let inspectionRec = queryResult.map(res=>res.InspectionData[0]);
      this.newStreamFeed(inspectionRec);
      if(this.liveFeedMode)
      {
        this.timeoutHDL = 
          setTimeout(()=>this.liveQueryInspRec(),this.liveFeedInterval);
      }
      else
      {
        this.timeoutHDL =undefined;
      }
      return inspectionRec;
    })
  }
  queryInspRec(timeRange=[moment(Date_addDay(new Date(),-7)), moment(Date_addDay(new Date(),1))],sampleCount=undefined)
  {
    if(this.defFile===undefined)return false;
    this.passiveQueryRange = timeRange;

    //this.liveFeedMode =(moment().isBefore(timeRange[1]));
    if(!this.liveFeedMode)
    {
      this.resetStreamInfo();
    }
    return this.liveQueryInspRec(timeRange,undefined,sampleCount);
  }


  queryInspRecSize(timeRange=[moment(Date_addDay(new Date(),-7)), moment(Date_addDay(new Date(),1))])
  {
    return DB_Query.inspectionQuery(this.defFile.featureSet_sha1,timeRange[0],timeRange[1],100*10000,undefined,
      {
        "_id":1,
      },
      {
        $count: "count"
      })
  }



}

function ListInfoEditUI ({shape_list,initRank=0,onListInfoChange}){
      
  const [sliderV,setSliderV]=useState(initRank);
  const [listInfo,setListInfo]=useState({});
  let measureList = shape_list
    .filter(shape=>shape.type==="measure");
  let rankMax=measureList.reduce((max,m)=>max<m.rank?m.rank:max,0);
  let rankMin=measureList.reduce((min,m)=>min>m.rank?m.rank:min,0);

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
    if(onListInfoChange==undefined)return;
    let new_list_info={};
    shape_list.forEach((s)=>{
      new_list_info[s.id]={
        name:s.name,
        selected:(s.rank===undefined)||(s.rank<=sliderV)};
    });
    setListInfo(new_list_info);
    onListInfoChange(new_list_info);
  },[sliderV]);


  if(rankMin==0 && rankMax==0)
  {
    return null;
  }

  return <div>
    <div>Rank:</div>
    <Slider
      min={rankMin}
      max={rankMax}
      onChange={setSliderV}
      marks={marks}
      value={sliderV}
      style={{width:"300px",    
      marginLeft: "auto",
      marginRight:"auto"}}
    />
    
    {Object.keys(listInfo).map(key=><CheckableTag
    checked={listInfo[key].selected}
    onChange={(checked)=>{
      let nLInfo={...listInfo};
      nLInfo[key]={ ...nLInfo[key], selected:checked}
      
      setListInfo(nLInfo);
      onListInfoChange(nLInfo);
    }}>{listInfo[key].name}</CheckableTag>)}
  
  </div>;
}



class APP_ANALYSIS_MODE extends React.Component{


  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.state={
      defFileSearchName:"",
      dateRange:[moment(Date_addDay(new Date(),-1)), moment(Date_addDay(new Date(),1))],
      displayRange:[moment(0), moment(Date_addDay(new Date(),1))],
      inspectionRec:[],
        inspectionRec_TagFiltered:[],
      inspectionRecGroup:[],
      groupInterval:2*60*1000,//10 mins
      liveFeedMode:false,
      dataInSync:false,
      controlChartOverlap:false,
      displayListInfo:{},
      tagState:{},
      modalInfo:undefined
    };
    this.recStream=new InspRecStream();
  }
  

  stateUpdate(obj) {
    return this.setState({...this.state,...obj});
  }
  liveFeedMode_ctrl(enable=!this.state.liveFeedMode) {
    //console.log(enable);
    enable = this.recStream.setLiveFeedMode(enable);
    return this.stateUpdate({liveFeedMode:enable});
  }

  shouldComponentUpdate(nextProps, nextState) {
    if(nextProps.defFile!==this.props.defFile)
    {
      console.log("shouldComponentUpdate");
      this.recStream.setDefFile(nextProps.defFile);
    }
    return true;
  }
  static getDerivedStateFromProps(nextProps, prevState)
  {
    if(nextProps.defFile===undefined)return null;
    if(nextProps.defFile===prevState.__cached_defFile)return prevState;
    let defFile = dclone(nextProps.defFile);
    let features=defFile.featureSet[0].features;
    let __decorator=defFile.featureSet[0].__decorator;

    //order the feature array according to the __decorator
    let featureInOrder=__decorator.list_id_order.map(id=>features.find(f=>f.id==id));
    defFile.featureSet[0].features=featureInOrder;

    if(__decorator.control_margin_info===undefined)__decorator.control_margin_info={};

    let __control_margin_info=__decorator.control_margin_info;

    {//back up 
      __control_margin_info["__DEFAULT__"]=dclone(defFile.featureSet[0].features);
      // console.log(defFile);


      
    }



    // console.log(features,__decorator.list_id_order,featureInOrder);
    return {...prevState,defFile,__cached_defFile:nextProps.defFile};
  }

  popUp_downloading()
  {
    this.stateUpdate({modalInfo:{
      title:null,
      visible:true,
      onOK:()=>{
        // this.stateUpdate({modalInfo:undefined});
      },
      onCancel:()=>{
        // this.stateUpdate({modalInfo:undefined});
      },
      content:"下載中....",
      footer:null,
    },dataInSync:true});
  }
  SetDateRange(dayCount1,dayCount2)
  {
    
    function DateRange(dayCount1,dayCount2)
    {
      return [moment(Date_addDay(new Date(),dayCount1)), moment(Date_addDay(new Date(),dayCount2))];
    }
    return this.setState({dateRange:DateRange(dayCount1,dayCount2)});
  }
  render() {
    console.log(">>>>",this.state);
    if(this.state.defFile===undefined)return null;
    let menu_height="HXA";//auto
    let MenuSet=[];
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
    // console.log( this.state.defFile);
    let measureList = this.state.defFile.featureSet[0].features
      .filter(feature=>
        (this.state.displayListInfo[feature.id]!==undefined) &&
        (this.state.displayListInfo[feature.id].selected) &&
        (feature.type==="measure"));
    
    // console.log(this.state.defFile);
    document.title = this.state.defFile.name; 
    let HEADER=<Typography>
      <Title>{this.state.defFile.name}</Title>
    </Typography>;
    
    let graphCtrlUI=null;

    if(this.state.inspectionRec.length>0)
    {    
      graphCtrlUI = [
        <Slider range 
          min={this.state.inspectionRec[0].time_ms-1000}
          max={this.state.inspectionRec[this.state.inspectionRec.length-1].time_ms+1000}
          defaultValue={
              [this.state.inspectionRec[0].time_ms-1000, 
              this.state.inspectionRec[this.state.inspectionRec.length-1].time_ms+1000]} 
          step={1000*60*5}
          tipFormatter={(time)=>datePrintSimple(new Date(time))}
          onChange={(data)=>this.stateUpdate({displayRange:data})}
        />,
        <Checkbox checked={this.state.liveFeedMode} onChange={(ev)=>this.liveFeedMode_ctrl(ev.target.checked)}>LIVE</Checkbox>,
        "  抽檢間隔",
        <TimePicker defaultValue={moment('0:'+(this.state.groupInterval/60000), 'HH:mm')} format={'HH:mm'} hourStep={3} minuteStep={1} allowClear={false} 
          onChange={(t)=>
          {
            if(t===null)return;
            console.log(t);
            let mo=t._d.getTime();
            let day_base=moment(t._d).startOf('date')._d.getTime();
            console.log(mo-day_base)
            let groupInterval = mo-day_base;
            console.log("TimePicker",this.state.inspectionRec_TagFiltered);
            let inspectionRecGroup =
              inspectionRecGroup_Generate(this.state.inspectionRec_TagFiltered,groupInterval,measureList);

            this.stateUpdate({inspectionRecGroup,groupInterval});
        }}/>,
        <Checkbox checked={this.state.controlChartOverlap} onChange={(ev)=>this.setState({controlChartOverlap:ev.target.checked})}>重疊顯示</Checkbox>,
      ]

    }

    
    console.log(measureList);
    let graphUI=null;
    if(this.state.controlChartOverlap)
    {
      graphUI =
      <div  style={{width:"95%",height:"100%"}}> 
        <ControlChart inspectionRecGroup={this.state.inspectionRecGroup} 
          style={{height:"100%"}}
          key={"_"}
          targetMeasure={measureList} 
          xAxisRange={this.state.displayRange}/>
      </div>
    }
    else
    {
      graphUI =
      <div  style={{width:"95%"}}> 
        {measureList.map(m=>
        <ControlChart inspectionRecGroup={this.state.inspectionRecGroup} 
          style={{height:"400px"}}
          key={m.name+"_"}
          targetMeasure={[m]} 
          xAxisRange={this.state.displayRange}/>)}
      </div>
    }


    let modalDisplay=this.state.modalInfo===undefined?null:
    <Modal title={this.state.modalInfo.title} visible={this.state.modalInfo.visible}  
      maskClosable={this.state.modalInfo.maskClosable==true} closable={this.state.modalInfo.closable==true}
      onOk={this.state.modalInfo.onOK} okText={this.state.modalInfo.okText}
      onCancel={this.state.modalInfo.onCancel} cancelText={this.state.modalInfo.cancelText} footer={this.state.modalInfo.footer}> 
      {this.state.modalInfo.content}
    </Modal>;
    

    


    return(
    <div className="HXF">
      
      <div className="overlayCon s overlayCon width12 HXF">
        
        {HEADER}
        <div className="s height12">

          <Button key="_month"
            onClick={() => this.SetDateRange(-30,1)}>
            月(-30)
          </Button>

          <Button key="half_month"
            onClick={() => this.SetDateRange(-15,1)}>
            半月(-15)
          </Button>
          <Button key="week"
            onClick={() => this.SetDateRange(-7,1)}>
            星期(-7)
          </Button>
          <Button key="day"
            onClick={() => this.SetDateRange(-1,1)}>
            一天(-1)
          </Button>
          <RangePicker key="RP"
            value={this.state.dateRange} 
            onChange={(date)=>{
              let date_start_n_end=[date[0].startOf('day'),date[1].endOf('day')]
              // console.log(date_start_n_end);
              this.stateUpdate({dateRange:date_start_n_end})
            }}/>

          <Button type="primary" icon="search" disabled={ (!dateRangeReady || !defFileReady) || (this.state.dataInSync)} onClick={
            ()=>{

              this.popUp_downloading();


              this.recStream.newFeedCallBack=
                (newStream,fullStream)=>{
                  if(newStream.length>0)
                  {
                    let latestTime=newStream[newStream.length-1].time_ms;
                    let inspectionRecGroup =
                      inspectionRecGroup_Generate(fullStream,this.state.groupInterval,measureList);
                    this.stateUpdate({
                      inspectionRec:fullStream,
                      inspectionRecGroup:inspectionRecGroup,
                        inspectionRec_TagFiltered:fullStream,
                      displayRange:[this.state.displayRange[0],moment(latestTime+1000)]
                    });

                    //console.log("fullStream=",fullStream);
                  }
                }

              this.recStream.queryInspRecSize(dateRange.map(d=>d.getTime())).then(result=>{

                let dataCount=result[0].count;
                console.log(dataCount)
                
                let sample1=5000;
                let sample2=1000;
                let sample3=500;
                let warn_limit=sample1;
                if(dataCount<warn_limit)
                {
                  this.recStream.queryInspRec(dateRange.map(d=>d.getTime())).then(result=>{
                    this.stateUpdate({dataInSync:false,modalInfo:undefined});
                  }).catch(err=>{
                    this.stateUpdate({dataInSync:false,modalInfo:undefined});
                  });
                }
                else
                {
                  
                  this.stateUpdate({modalInfo:{
                    title:"警告",
                    visible:true,
                    onOK:()=>{
                      
                      this.stateUpdate({dataInSync:false,modalInfo:undefined});
        
                    },
                    onCancel:()=>{
                      // this.stateUpdate({modalInfo:undefined,dataInSync:false});
                      
                      this.stateUpdate({dataInSync:false,modalInfo:undefined});
        
                    },
                    content:"查詢結果數量:"+dataCount+" > "+warn_limit+" 取樣下載? 或是仍要全下載?",
                    okText:"全下載",

                    footer:<>
                      <Button key="_month"
                        onClick={() =>{
                          this.recStream.queryInspRec(dateRange.map(d=>d.getTime())).then(result=>{
                            this.stateUpdate({dataInSync:false,modalInfo:undefined});
                          }).catch(err=>{
                            this.stateUpdate({dataInSync:false,modalInfo:undefined});
                          });
                          this.popUp_downloading();    
                        }}>
                        全下載
                      </Button>
                      <br/>
                      取樣下載:
                      {[sample1,sample2,sample3].map(count=>(
                          <Button key={`samp_${count}`}
                          onClick={() =>{
                            this.recStream.queryInspRec(dateRange.map(d=>d.getTime()),count).then(result=>{
                              this.stateUpdate({dataInSync:false,modalInfo:undefined});
                            }).catch(err=>{
                              this.stateUpdate({dataInSync:false,modalInfo:undefined});
                            });
                            this.popUp_downloading();
              
                          }}>
                          {count}
                        </Button>
                      ))}
                    </>,
                    maskClosable:true
                  }});
                  
                }



                // this.stateUpdate({dataInSync:false});
              }).catch(err=>{
                this.stateUpdate({dataInSync:false,modalInfo:undefined});
              });

            }} />
            
          <Button type="primary" icon="download" disabled={!dateRangeReady || !defFileReady || this.state.inspectionRec.length===0} 
            onClick={()=>{
              let ReportName=this.state.defFile.name+"_"+YYYYMMDD(new Date());
              let csv_arr= convertInspInfo2CSV(ReportName,measureList,this.state.inspectionRecGroup,this.state.tagState);
              let str = csv_arr.join('');
              //copyStringToClipboard(str);
              downloadString(csv_arr.join(''), "text/csv", ReportName+".csv");
            }} />

          {modalDisplay}
          資料數:{this.state.inspectionRec_TagFiltered.length}
            <hr style={{width:"80%"}}/>
            <RelatedUsageInfo fullStream2Tag={this.state.inspectionRec}
              control_margin_set={this.state.defFile.featureSet[0].__decorator.control_margin_info}
              onTagStateChange={(tagState)=>{
                  

                  let updatedDefFile ={...this.state.defFile};
                  // updatedDefFile
                  let updatedDefFile_feature=updatedDefFile.featureSet[0];
                  let __deco_control_margin_info=updatedDefFile_feature.__decorator.control_margin_info;
                  let new_feature=dclone(__deco_control_margin_info["__DEFAULT__"]);

                  console.log(updatedDefFile,new_feature);


                  let mustList = Object.keys(tagState).filter(key=>tagState[key]==1);
                  let mustNotList = Object.keys(tagState).filter(key=>tagState[key]==-1);

                  let selKey=mustList.find(mustKey=>__deco_control_margin_info[mustKey]!==undefined);
                  if(selKey!==undefined)
                  {
                    let curMargInfo = __deco_control_margin_info[selKey];
                    new_feature = new_feature.map((fea)=>{
                      let matched_cmi=curMargInfo.find(info=>info.id==fea.id);
                      if(matched_cmi===undefined)return fea;
                      
                      return {...fea,...matched_cmi}
                    });

                  }
                  updatedDefFile.featureSet[0].features=new_feature;

                  let filterTagsBoolean = 
                    this.state.inspectionRec.filter(function(item, index, array){
                        let tArr=item.tag.split(",");
                        let everyMust=mustList.every((item)=>tArr.includes(item));
                        let someMustNot=mustNotList.some((item)=>tArr.includes(item));
                        return everyMust && (!someMustNot);
                        //return selectedTrueTags.every((item)=>tArr.includes(item));
                    });

                  console.log(updatedDefFile);
                  let inspectionRecGroup =
                      inspectionRecGroup_Generate(filterTagsBoolean,this.state.groupInterval,measureList);
                  //console.log(filterTagsBoolean,inspectionRecGroup);
                  this.stateUpdate({
                    tagState,
                    inspectionRecGroup:inspectionRecGroup,
                    inspectionRec_TagFiltered:filterTagsBoolean,
                    defFile:updatedDefFile
                  });


              }}/>


            <ListInfoEditUI shape_list={this.state.defFile.featureSet[0].features.filter(feature=>(feature.type==="measure"))}
            onListInfoChange={(listInfo)=>{
              console.log(listInfo);
              this.setState({displayListInfo:listInfo})
            }}/>
            
            <hr style={{width:"80%"}}/>
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


class RelatedUsageInfo extends React.Component{
///query/deffile?name=BOS-LT13BH3421&
// http://localhost:3000/hyvision_monitor/0.0.0/?v=0&hash=9fa42a5e990e4da632070e95daf14ec50de8a112&name=BOS-LT13BH3421
    constructor(props){
        super(props);
        this.state={
            tags:{
                //"name":false
            },
            init:true
            // DefFileInfo:[],
        };
        
        this.checkInspectionRec=undefined;
        // this.handleChange = this.handleChange.bind(this);
    }

    componentDidMount(){

    }
    componentWillReceiveProps(nextProps) {
        if(this.checkInspectionRec===nextProps.fullStream2Tag)
        {
            return;
        }
        this.checkInspectionRec=nextProps.fullStream2Tag;
        console.log("props.fullStream2Tag",nextProps.fullStream2Tag);
        const uniSet2 = new Set();
        // uniSet2.add("judgeReport Tag");
        if(nextProps.fullStream2Tag.length>0){
            nextProps.fullStream2Tag.forEach(function(e,i,a){
                //console.log("e.tag=",e.tag);
                let tagSplit=e.tag.split(",");
                //console.log("e.tag.split=",e.tagSplit);
                tagSplit.forEach(function(e2,i2,a2){
                    //console.log("forEach2",e2);
                    if(e2.length!=0)
                        uniSet2.add(e2);
                });
            });
            let tags2={};
            Array.from(uniSet2).forEach(function(key){
                tags2[key]=0;
            });

            this.setState( {tags:tags2});

            this.props.onTagStateChange(tags2);
        }
    }
    handleTagChange(){
        console.log("handleTagChange");
    }

    static getDerivedStateFromProps(props, prevState)
    {
        console.log("props.fullStream2Tag",props.fullStream2Tag);
        const uniSet2 = new Set();
        // uniSet2.add("judgeReport Tag");
        if(props.fullStream2Tag.length>0){
            props.fullStream2Tag.forEach(function(e,i,a){
                //console.log("e.tag=",e.tag);
                let tagSplit=e.tag.split(",");
                //console.log("e.tag.split=",e.tagSplit);
                tagSplit.forEach(function(e2,i2,a2){
                    //console.log("forEach2",e2);
                    if(e2.length!=0)
                        uniSet2.add(e2);
                });
            });
            let tags2={...prevState.tags};
            Array.from(uniSet2).forEach(function(key){
              if(tags2[key]===undefined)
                tags2[key]=0;
            });
            if(prevState.init==true)
            {
              props.onTagStateChange(tags2);
            }
            return {tags:tags2,init:false};

        }
        return null;
    }

    handleTagChange = (key,onoff) =>
    {
      let tags2={...this.state.tags};
      //if the special tag is set to SET, unset other special tags.
      if(onoff==1 && this.props.control_margin_set!==undefined)
      {
        if(this.props.control_margin_set[key]!==undefined)
        {
          Object.keys(tags2).forEach((key)=>{
            if(this.props.control_margin_set[key]!==undefined)
            {
              tags2[key]=0;
            }
          });
        }
      }

        if(key===undefined)
        {
          return;
        }
        if(key==null)
        {
          if(Object.keys(tags2).every(key=>tags2[key]==onoff))
          {
            onoff=0;
          }
          Object.keys(tags2).forEach((key)=>{
            tags2[key]=onoff;
          });
        }
        else
        {
          tags2[key]=onoff;
        }
        this.props.onTagStateChange(tags2);
        this.setState( {tags:tags2});
    }

    render() {
        // console.log("this.state.tags",this.state.tags);
        // control_margin_set
        return (
          <>
            <Tag color="#87d068" 
            onClick={_=>this.handleTagChange(null,1)}
            >O</Tag>:
            {Object.keys(this.state.tags).map((key)=>{
              let tagName=key;
              if(this.props.control_margin_set!==undefined && this.props.control_margin_set[key]!==undefined)
              {
                tagName="["+key+"]";
              }
                return (
                  <Tag 
                  color={(this.state.tags[key]==1)?"#87d068":undefined}
                  key={key}
                  onClick={_=>this.handleTagChange(key,(this.state.tags[key]!=1)?1:0)}
                  >{tagName}</Tag>
                );
            })
            
            }

            <br/>
            <Tag color="#f50"
            onClick={_=>this.handleTagChange(null,-1)}
            >X</Tag>:
            {Object.keys(this.state.tags).map((key, index, array)=>{
                let tagName=key;
                if(this.props.control_margin_set!==undefined && this.props.control_margin_set[key]!==undefined)
                {
                  tagName="["+key+"]";
                }
                return (
                  <Tag color={(this.state.tags[key]==-1)?"#f50":undefined}
                  key={key}
                  onClick={_=>this.handleTagChange(key,(this.state.tags[key]!=-1)?-1:0)}
                  >{tagName}</Tag>
                );
            })
            
            }
          </>
        );

    }


}
// function getDef(q_info) {
//     let result
//     console.log("q_info",q_info);
//     DB_Query.defFileQuery(q_info.name,q_info.hash).
//     then((q)=>{
//         if(q.length>0){
//
//             result=q;
//         }else{
//             console.log("[X]No result.");
//         }
//
//     });
//     return result;
// }
export default APP_ANALYSIS_MODE;