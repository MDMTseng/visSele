'use strict'
   

import React from 'react';

import {round as roundX,GetObjElement} from './UTIL/MISC_Util';
import 'antd/dist/antd.css';

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
const { CheckableTag } = Tag;

const { RangePicker } = DatePicker;
const { Title, Paragraph, Text } = Typography;

let log = logX.getLogger("AnalysisUI");



Chart.pluginService.register({
  afterDraw: function(chart) {
  }
});



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


function convertInspInfo2CSV(measureList,inspRecGroup)
{
  let ci=[];
  console.log(measureList,inspRecGroup);
  function pushDataRow(arr,measureReports,trace,RowName=trace[trace.length-1])
  {
    arr.push(RowName);
    measureReports.forEach((rep)=>{
      let ele = GetObjElement(rep,trace);
      if(ele===undefined)ele='';
      arr.push(","+ele);
    });
    arr.push("\n");
  }
  ["name","subtype"]
    .forEach(field=>pushDataRow(ci,measureList,[field]));

  ["USL","UCL","value","LCL","LSL"]
    .forEach(field=>pushDataRow(ci,measureList,[field]));

  ci.push(",\n");

  inspRecGroup.forEach(s_group=>{
    s_group.group
      .forEach(data=>{
        let rep=data.judgeReports;
        ci.push(data.time_ms);
        measureList.forEach(m=>
          {
            let s_rep = rep.find(s_rep=>s_rep.id==m.id);
            if(s_rep.status!==INSPECTION_STATUS.NA)
              ci.push(","+s_rep.value);
            else
              ci.push(",");
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

      this.state.chartOpt.options.pan={
          enabled: true,
          mode: 'y' // is panning about the y axis neccessary for bar charts?
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
  PropsUpdate(nextProps) {
      

      let LSL=nextProps.targetMeasure["LSL"];
      let USL=nextProps.targetMeasure["USL"];
      let value_target=nextProps.targetMeasure["value"];
      //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
      this.state.chartOpt.data.labels=[];
      this.state.chartOpt.data.datasets.forEach((datInfo)=>{
          datInfo.data=[];
          datInfo.pointBackgroundColor=[];
      });
      let length = nextProps.inspectionRecGroup.length;
      if(length===0)return;
      //let newTime = nextProps.reportArray[length-1].time_ms;
      this.state.chartOpt.options.title.text=nextProps.targetMeasure.name;

      nextProps.inspectionRecGroup
        .reduce((sumG,recG)=>sumG.concat(recG.group),[])//make [{group:[a,b]},{group:[c,d]}] => [a,b,c,d...]
        .reduce((acc_data,rep,idx)=>{
          //acc_data.labels.push(rep.time_ms);
          if(nextProps.xAxisRange!==undefined)
          {
            if(nextProps.xAxisRange[0]>rep.time_ms || 
              nextProps.xAxisRange[1]<rep.time_ms)return acc_data;
          }

          let measureObj = rep.judgeReports.find((jrep)=>jrep.id===nextProps.targetMeasure.id);


          let measureValue=measureObj.value;
          let pointColor=undefined;
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
          
          let val={
            x:new Date(rep.time_ms).toString(),
            y:measureValue,
          };


          acc_data.datasets[0].pointBackgroundColor.push(pointColor);
          //TODO:for now there is only one data set in one chart
          acc_data.datasets[0].data.push(val);
          return acc_data;
      }, this.state.chartOpt.data );

      let measureInfo=nextProps.targetMeasure;

      nextProps.inspectionRecGroup.reduce((acc_data,repG,idx)=>{
        let this_id_stat = repG.stat.
          find((st)=>st.id===nextProps.targetMeasure.id);
        
        let value =this_id_stat.mean;
        let time = repG.group.reduce((sum,rep)=>sum+rep.time_ms,0)/repG.group.length;
        if(nextProps.xAxisRange!==undefined)
        {
          if(nextProps.xAxisRange[0]>time || 
            nextProps.xAxisRange[1]<time)return acc_data;
        }
        acc_data.labels.push(new Date(time));
        let pointColor="rgba(0,255,0,1)";
        let val={
          x:new Date(time).toString(),
          y:value,
          data:repG,
          stat:this_id_stat
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
                      content: annotationTar.type+":"+val
                  },
                  onMouseover: function (e) {
                    document.getElementById("info").innerHTML = 'whatever';
                  },
              });
          });
      
          this.state.chartOpt.options.tooltips={
            callbacks: {
              title: function(tooltipItem, data) {
                let datasetIndex = tooltipItem[0].datasetIndex;
                let index = tooltipItem[0].index;
                
                return data.datasets[datasetIndex].data[index].y
                return data['labels'][tooltipItem[0]['index']];
              },
              label: function(tooltipItem, data) {

                let datasetIndex = tooltipItem.datasetIndex;
                let index = tooltipItem.index;
                let dataOnTip=data.datasets[datasetIndex].data[index];
                let stat = dataOnTip.stat;
                if(stat==undefined)return dataOnTip.x;

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


              },
              afterLabel: function(tooltipItem, data) {
                return "";

              }
            },
            backgroundColor: '#FFF',
            titleFontSize: 16,
            titleFontColor: '#0066ff',
            bodyFontColor: '#000',
            bodyFontSize: 14,
            displayColors: false
          }


      if(LSL!=0 || USL!=0 || value_target!=0)
      {
        this.state.chartOpt.options.scales.yAxes[0].ticks={
          min:1.2*(LSL-value_target)+value_target,
          max:1.2*(USL-value_target)+value_target,
        };
      }
      else
      {

      }


      if(this.charObj!==undefined)    
        this.charObj.update();
      //console.log(this.state.chartOpt.options.scales);
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

  liveQueryInspRec(timeRange,maxResults)
  {
    if(timeRange==undefined)
    {
      if(this.rec.length>0)
      {
        let lastMsInRec = this.rec[this.rec.length-1].time_ms;
        timeRange=[moment(lastMsInRec)._d.getTime(), moment(Date_addDay(new Date(),1))._d.getTime()];
        console.log(moment(lastMsInRec));
      }
      else
      {
        console.log("No existed rec to do live query");
        
        this.timeoutHDL =undefined;
        return false;
      }
    }
    if(maxResults==undefined)
    {
      maxResults=10;
    }
    console.log("timeRange="+timeRange);
    DB_Query.inspectionQuery(this.defFile.featureSet_sha1,timeRange[0],timeRange[1],maxResults)
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
      
    })
    .catch((e)=>{
      console.log(e);
    });
    return true;
  }
  queryInspRec(timeRange=[moment(Date_addDay(new Date(),-7)), moment(Date_addDay(new Date(),1))])
  {
    if(this.defFile===undefined)return false;
    this.passiveQueryRange = timeRange;

    //this.liveFeedMode =(moment().isBefore(timeRange[1]));
    if(!this.liveFeedMode)
    {
      this.resetStreamInfo();
    }
    return this.liveQueryInspRec(timeRange,10000000);
  }


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
      dateRange:[moment(Date_addDay(new Date(),-60)), moment(Date_addDay(new Date(),-30))],
      displayRange:[moment(0), moment(Date_addDay(new Date(),1))],
      inspectionRec:[],
      inspectionRecGroup:[],
      groupInterval:10*60*1000,//10 mins
      liveFeedMode:false
    };
    this.recStream=new InspRecStream();
    //this.state.inspectionRec=dbInspectionQuery;
    //this.props.defFile=defFile;

//let IRG=InspectionRecordGrouping(dbInspectionQuery);
//console.log(IRG,defFile);
  }
  
  shouldComponentUpdate(nextProps, nextState) {
    if(nextProps.defFile!==this.props.defFile)
    {
      this.recStream.setDefFile(nextProps.defFile);
    }
    return true;
  }

  stateUpdate(obj) {
    return this.setState({...this.state,...obj});
  }
  liveFeedMode_ctrl(enable=!this.state.liveFeedMode) {
    //console.log(enable);
    enable = this.recStream.setLiveFeedMode(enable);
    return this.stateUpdate({liveFeedMode:enable});
  }


  inspectionRecGroup_Generate(inspectionRec,groupInterval,measureList)
  {
    let inspectionRecGroup = InspectionRecordGrouping(inspectionRec,groupInterval);
    InspectionRecordGroup_AppendCPK(inspectionRecGroup,measureList);
    return inspectionRecGroup;
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
    
    
    console.log(this.props.defFile);
    document.title = this.props.defFile.name; 
    let HEADER=<Typography>
      <Title>{this.props.defFile.name}</Title>
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
          tipFormatter={(time)=>new Date(time).toString()}
          onChange={(data)=>this.stateUpdate({displayRange:data})}
        />,
        <Checkbox checked={this.state.liveFeedMode} onChange={(ev)=>this.liveFeedMode_ctrl(ev.target.checked)}>LIVE</Checkbox>,
        <TimePicker defaultValue={moment('0:30', 'HH:mm')} format={'HH:mm'} minuteStep={5} allowClear={false} 
          onChange={(t)=>
          {
            if(t===null)return;
            console.log(t);
            let mo=t._d.getTime();
            let day_base=moment(t._d).startOf('date')._d.getTime();
            console.log(mo-day_base)
            let groupInterval = mo-day_base;
            
            let inspectionRecGroup =
              this.inspectionRecGroup_Generate(this.state.inspectionRec ,groupInterval,measureList);
            this.stateUpdate({inspectionRecGroup,groupInterval});
        }}/>
      ]

    }

    
    let graphUI=null;
    graphUI =
    <div  style={{width:"95%"}}> 
      {measureList.map(m=>
      <ControlChart inspectionRecGroup={this.state.inspectionRecGroup} 
        style={{height:"400px"}}
        key={m.name+"_"}
        targetMeasure={m} 
        xAxisRange={this.state.displayRange}/>)}
    </div>
    
    
    return(
    <div className="HXF">
      
      <div className="overlayCon s overlayCon width12 HXF">
        
        {HEADER}
        <div className="s height12">

          <RangePicker key="RP"
            defaultValue={this.state.dateRange} 
            onChange={(date)=>this.stateUpdate({dateRange:date})}/>

          <Button type="primary" icon="search" disabled={!dateRangeReady || !defFileReady} onClick={
            ()=>{
              this.recStream.newFeedCallBack=
                (newStream,fullStream)=>{
                  if(newStream.length>0)
                  {
                    let latestTime=newStream[newStream.length-1].time_ms;
                    let inspectionRecGroup =
                      this.inspectionRecGroup_Generate(fullStream,this.state.groupInterval,measureList);
                    this.stateUpdate({
                      inspectionRec:fullStream,
                      inspectionRecGroup:inspectionRecGroup,
                      displayRange:[this.state.displayRange[0],moment(latestTime+1000)]
                    });

                    console.log("fullStream=",fullStream);
                  }
                }
              this.recStream.queryInspRec(dateRange);
            }} />
            
          <Button type="primary" icon="download" disabled={!dateRangeReady || !defFileReady || this.state.inspectionRec.length===0} 
          onClick={
            ()=>{
              let csv_arr= convertInspInfo2CSV(measureList,this.state.inspectionRecGroup);
              let str = csv_arr.join('');
              //copyStringToClipboard(str);
              downloadString(csv_arr.join(''), "text/csv", DefFileName+"_"+YYYYMMDD(new Date())+".csv");
            }} />
            <hr style={{width:"80%"}}/>
            <RelatedUsageInfo fullStream2Tag={this.state.inspectionRec} />
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
class MyTag extends React.Component {
    state = { checked: true };

    handleChange = checked => {
        this.setState({ checked });

    };

// DB_Query.defFileQuery("","",{"_id":0,"DefineFile.name":1,"DefineFile.tag":1,"createdAt":1}).then((q)=>{
    //         this.setState({"DefFileInfo":q});
    //     console.log("[O]",q);
    // }).catch((e)=> {
    //     console.log("[X]",e);
    // });
    render() {
        return (
            <CheckableTag {...this.props} checked={this.state.checked} onChange={this.handleChange} />
        );
    }
}
class RelatedUsageInfo extends React.Component{
//http://hyv.decade.tw:8080/query/deffile?name=BOS-LT13BH3421&
// http://localhost:3000/hyvision_monitor/0.0.0/?v=0&hash=9fa42a5e990e4da632070e95daf14ec50de8a112&name=BOS-LT13BH3421
    constructor(props){
        super(props);
        this.state={
            fullStream2Tag:[]
            // DefFileInfo:[],
        };
        // this.handleChange = this.handleChange.bind(this);
    }

    componentDidMount(){

    }
    componentWillReceiveProps(nextProps) {
        // if(this.props===nextProps)return;
        this.setState({...this.state,...nextProps});
        console.log("componentWillReceiveProps",nextProps);

    }
    handleTagChange(){
        console.log("handleTagChange");
    }
    updateChart(){
        // let inspectionRecGroup = this.inspectionRecGroup_Generate(this.state.fullStream2Tag,this.state.groupInterval,measureList);

    }
    render() {
        console.log("this.state.fullStream2Tag",this.state.fullStream2Tag);
        const uniSet2 = new Set();
        uniSet2.add("judgeReport Tag");
        if(this.state.fullStream2Tag.length>0){
            this.state.fullStream2Tag.forEach(function(e,i,a){
                console.log("forEach",e.tag);
                uniSet2.add(e.tag);
            });
        }
        return (
            <div>
                <h6 style={{ marginRight: 8, display: 'inline' }}>Uni Categories:</h6>
                {Array.from(uniSet2).map(function(item, index, array) {
                    return (
                        <MyTag key={index+""+item}>
                            {item}
                        </MyTag>
                    );

                })
                }

            </div>
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