import React , { useState,useEffect } from 'react';
import logo from './logo.svg';
import './App.css';
import APP_ANALYSIS_MODE from "./AnalysisUI";
import * as DB_Query from './UTIL/DB_Query';
import  Modal  from 'antd/lib/modal';
import QrScanner from 'qr-scanner';
import jsonp from 'jsonp';

import  Typography  from 'antd/lib/typography';
import Input from 'antd/lib/input';
import Table from 'antd/lib/table';
import Col from 'antd/lib/col';
import Row from 'antd/lib/row';
import Layout from 'antd/lib/layout';
QrScanner.WORKER_PATH = "./qr-scanner-worker.min.js";

const { Title, Paragraph, Text } = Typography;
class WebCam_SQScan extends React.Component{

  constructor(props) {
    super(props);

  }
   
  componentDidMount() {
    let video =this.refs.QR_WebCam;
    
    QrScanner.hasCamera().then(hasCamera => console.log(hasCamera));
    this.qrScanner = new QrScanner(video, this.props.onScanResult);
    
    this.qrScanner.start();
    console.log(this.qrScanner);
  }
  componentWillUnmount() {
    this.qrScanner.destroy();
    this.qrScanner = undefined;
  }

  render() {
    return (
      <video ref="QR_WebCam" className={this.props.className} style={this.props.style} muted playsInline/>
    );
  }
}

function getAllUrlParams(url) {

  // get query string from url (optional) or window
  var queryString = url ? url.split('?')[1] : window.location.search.slice(1);

  queryString = decodeURI(queryString);
  // we'll store the parameters here
  var obj = {};

  // if query string exists
  if (queryString) {

    // stuff after # is not part of query string, so get rid of it
    queryString = queryString.split('#')[0];

    // split our query string into its component parts
    var arr = queryString.split('&');

    for (var i = 0; i < arr.length; i++) {
      // separate the keys and the values
      var a = arr[i].split('=');

      // set parameter name and value (use 'true' if empty)
      var paramName = a[0];
      var paramValue = typeof (a[1]) === 'undefined' ? true : a[1];

      // (optional) keep case consistent
      //paramName = paramName.toLowerCase();
      //if (typeof paramValue === 'string') paramValue = paramValue.toLowerCase();

      // if the paramName ends with square brackets, e.g. colors[] or colors[2]
      if (paramName.match(/\[(\d+)?\]$/)) {

        // create key if it doesn't exist
        var key = paramName.replace(/\[(\d+)?\]/, '');
        if (!obj[key]) obj[key] = [];

        // if it's an indexed array e.g. colors[2]
        if (paramName.match(/\[\d+\]$/)) {
          // get the index value and add the entry at the appropriate position
          var index = /\[(\d+)\]/.exec(paramName)[1];
          obj[key][index] = paramValue;
        } else {
          // otherwise add the value to the end of the array
          obj[key].push(paramValue);
        }
      } else {
        // we're dealing with a string
        if (!obj[paramName]) {
          // if it doesn't exist, create property
          obj[paramName] = paramValue;
        } else if (obj[paramName] && typeof obj[paramName] === 'string'){
          // if property does exist and it's a string, convert it to an array
          obj[paramName] = [obj[paramName]];
          obj[paramName].push(paramValue);
        } else {
          // otherwise add the property
          obj[paramName].push(paramValue);
        }
      }
    }
  }

  return obj;
}

function isString(val){
  return (typeof val === 'string' || val instanceof String)
}

function pjsonp(url,timeout=5000,timeoutErrMsg="TIMEOUT")
{
  return new Promise((res,rej)=>{
    let timeoutFlag=undefined;
    if(timeout>0)
    {
      timeoutFlag = setTimeout(()=>{
        timeoutFlag=undefined;
        rej(timeoutErrMsg)
      },timeout);
    }

    jsonp(url,  (err,data)=>{
      clearTimeout(timeoutFlag);
      if(err === null)
          res(data);
      else
          rej(err)
    });
  });
}

function fetchDeffileInfo(name)
{
  let defFileData=undefined;
  
  return new Promise((res,rej)=>{
    let url='http://hyv.decade.tw:8080/query/deffile?name='+name+'&limit=1000'
    
    pjsonp(url,null).then((data)=>{

      
      let url='http://hyv.decade.tw:8080/query/inspection?';
      let hashArr = data.map(srec=>srec.DefineFile.featureSet_sha1)
      hashArr = [...new Set(hashArr)];

      defFileData=hashArr.map((hash)=>{
        let name = data.filter((defF)=>defF.DefineFile.featureSet_sha1==hash)
          .map(defF=>defF.DefineFile.name)
        return {hash,name}
      }).map(defFInfo=>{
        defFInfo.name=[...new Set(defFInfo.name)];
        return defFInfo;
      });
      let hashRegx = hashArr.reduce((acc,hash)=>acc===undefined?hash:acc+"|"+hash,undefined)
      
      url+='tStart=0&tEnd=2581663256894&limit=999999999&';
      url+='subFeatureDefSha1='+hashRegx+'&'
      url+='projection={"_id":0,"InspectionData.subFeatureDefSha1":1,"InspectionData.time_ms":1,"InspectionData.tag":1}&'
      url+='agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1",'+
      '"count": {"$sum":1},'+
      '"time_start": {"$min":"$InspectionData.time_ms"},'+
      '"time_end": {"$max":"$InspectionData.time_ms"},'+
      '"tags": {"$addToSet":"$InspectionData.tag"}'+
      '}}]';

      return pjsonp(url,null)

    }).then((dataSet)=>{

      let dataSet_Formatted=
      dataSet.map(data=>{
        data._id=data._id[0]
        return data
      }).reduce((acc,data)=>{
        let id=data._id;
        if(acc[id]===undefined)
        {
          acc[id]=data;
          delete acc[id]._id; 
        }
        else
        {
          acc[id].count+=data.count;
          acc[id].time_start=[Math.min(acc[id].time_start[0],data.time_start[0])];
          acc[id].time_end=[Math.max(acc[id].time_end[0],data.time_end[0])];
        }
        return acc;
      },{})

      //final aggregation
      defFileData.forEach((defF)=>{
        let tar = dataSet_Formatted[defF.hash];
        if(tar!==undefined)
        {
          Object.assign(defF, tar)
        }
      })
      res(defFileData)
    }).catch((err)=>{
      rej(err);
    })
  });
}



function fetchCostomDisplayInfo(name)
{
  let defFileData=undefined;
  
  return new Promise((res,rej)=>{
    let url='http://hyv.decade.tw:8080/QUERY/customDisplay?name='+name
    
    pjsonp(url,null).then((data)=>{
      res(data);
    }).catch((err)=>{
      rej(err);
    })
  });
}

function insertCostomDisplayInfo(id,info)
{
  let defFileData=undefined;
  
  return new Promise((res,rej)=>{
    let url='http://hyv.decade.tw:8080/insert/customdisplay?name='+info.name+
      "&targetDeffiles="+JSON.stringify(info.targetDeffiles)
    if(id!==undefined)
    {
      url+="&_id="+id;
      
    }
    pjsonp(url,null).then((data)=>{
      console.log(data);
    }).catch((err)=>{
      rej(err);
    })
  });
}
// fetchCostomDisplayInfo("Machine");
// insertCostomDisplayInfo("5e66719724f4fc4638dd3603",{name:"><>",targetDeffiles:[{hash:"sdiosdjciojsdoi"}]});
function getUrlPath()
{
  return window.location.href.substring(window.location.protocol.length).split('?')[0]
}
function XQueryInput({ onQueryRes,onQueryRej,placeholder,defaultValue }) {
  const [fetchedRecord, setFetchedRecord] = useState([]);
  let searchBox=<Input placeholder={placeholder} defaultValue={defaultValue}
    onPressEnter={(e)=>{
    console.log(e.target.value)
    setFetchedRecord();
    fetchDeffileInfo(e.target.value).
      then((res)=>{

        setFetchedRecord(res);
        onQueryRes(res);
      }).catch((e)=>{
        setFetchedRecord([]);
        if(onQueryRej!==undefined)
          onQueryRej(e)
      });
  }} ></Input>
  let fetchBtn=fetchedRecord===undefined?null:
  <div>

  </div>


  let displayInfo=null
  if(fetchedRecord!==undefined)
  {
    // displayInfo = fetchedRecord.map(fetchRec=>{

    //   let text = fetchRec.name+" ";
    //   if(fetchRec.count!==undefined)
    //   {
    //     text+=",count:"+fetchRec.count;
    //     var dateStart = new Date(fetchRec.time_start[0]);
    //     var dateEnd = new Date(fetchRec.time_end[0]);
    //     text+=","+datePrintSimple(dateStart)+"~"+datePrintSimple(dateEnd);
    //     text+=",tags:"+fetchRec.tags;
    //   }

    //   return [
    //     <a href={getUrlPath()+"?v=0&hash="+fetchRec.hash}>{text}</a>,
    //     <br/>]})
    const columns = [
      {
        title: 'Name',
        dataIndex: 'name',
        key: 'name',
        render: fetchRec => <a href={getUrlPath()+"?v=0&hash="+fetchRec.hash}>{fetchRec.name}</a>,
      },
      {
        title: 'count',
        dataIndex: 'count',
        key: 'count',
        sorter: (a, b) => a.count - b.count,
      },
      {
        title: 'Date_Start',
        dataIndex: 'Date_Start',
        key: 'Date_Start',
        render: milliSec => <div>{new Date(milliSec)+""}</div>,
        sorter: (a, b) => a.Date_Start - b.Date_Start,
      },
      {
        title: 'Date_End',
        dataIndex: 'Date_End',
        key: 'Date_End',
        render: milliSec => <div>{new Date(milliSec)+""}</div>,
        sorter: (a, b) => a.Date_End - b.Date_End,
      },
      {
        title: 'Tags',
        dataIndex: 'Tags',
        key: 'Tags',
      }]

      let dataSource = fetchedRecord.filter(fetchRec=>fetchRec.count!==undefined).map(fetchRec=>{
        let retSrc={
          name:fetchRec
        }
        if(fetchRec.count!==undefined)
        {
          retSrc.count=fetchRec.count;
          // var dateStart = new Date(fetchRec.time_start[0]);
          // var dateEnd = new Date(fetchRec.time_end[0]);

          retSrc.Date_Start=fetchRec.time_start[0];
          retSrc.Date_End=fetchRec.time_end[0];
          retSrc.Tags=fetchRec.tags;
        }
  
        return retSrc
      })
    console.log(dataSource);
    displayInfo=<Table columns={columns} dataSource={dataSource} pagination={false}/>;
  }
  return (

    <div>
      {searchBox}
      {displayInfo}
    </div>
  );
}

function SingleDisplayUI({ displayInfo})
{
  const canvasRef = React.useRef(null)


  return <div>
    <Title level={2}>{displayInfo.name}</Title>
    {JSON.stringify(displayInfo)}

    <canvas  key="canv"
      ref={canvasRef}
      onClick={e => {        
        const canvas = canvasRef.current
        const ctx = canvas.getContext('2d')
        // implement draw on ctx here
      }}
    />
  </div>
}

function CostomDisplayUI({ }) {
  const [displayInfo, setDisplayInfo] = useState(undefined);

  const [collapsed, setCollapsed] = useState(true);
  useEffect(() => {
    setDisplayInfo(undefined);
    fetchCostomDisplayInfo("Machine").then(data=>{
      console.log(displayInfo);
      setDisplayInfo(data.prod);
    }).catch(e=>{
      console.log(e);
    });
    return () => {
      console.log("1,didUpdate ret::");
    };
  },[]);

  useEffect(() => {
    console.log("2,count->useEffect>>");
  },[displayInfo]);

  let UI=null;
  if(displayInfo!==undefined)
  {
    console.log(displayInfo);
    UI=[];
    UI.push(<div key="Reset" onClick={()=>setDisplayInfo(undefined)}>RESET....</div>)
    UI.push(<Row  key="table"  style={{height:"50%"}}>{displayInfo.map(info=>
      <Col key={info.name} span={12}  style={{height:"100%"}}>
        <SingleDisplayUI displayInfo={info}/>
      </Col>
    )}</Row>)


  }


  return (

    <Layout style={{height:"100%"}}>
      <Layout.Header style={{ color: '#FFFFFF' }} >Header</Layout.Header>
      <Layout>
        <Layout.Sider  style={{ color: '#FFFFFF' }} collapsible collapsed={collapsed} 
        onCollapse={()=>setCollapsed(!collapsed)}
        onMouseOut={()=>(collapsed)?null:setCollapsed(true)}
        onMouseOver={()=>(collapsed)?setCollapsed(false):null}
        >
          Sider
        </Layout.Sider>
        <Layout>
          <Layout.Content  style={{ padding: '50px 50px' }}>{UI}</Layout.Content>
          <Layout.Footer>Costom UI v0.0.0</Layout.Footer>
        </Layout>
      </Layout>
    </Layout> 
  );
}


class App extends React.Component{

  constructor(props) {
    super(props);


    console.log(getAllUrlParams());
    this.state={
      DefFileInfo:undefined,
      allowQRScan:false,
      UI:undefined

      //{v: 0, name: "BOS-LT13BH3421", hash: "9fa42a5e990e4da632070e95daf14ec50de8a112"}
    }
    this.UI_type={
      customDisplay:"customDisplay",
      analysis:"analysis",
      search:"search",
    }
  }
  componentDidMount() {

    let urlParam = getAllUrlParams();
    console.log(urlParam);
    if(urlParam.UI==="customDisplay")
    {
      this.setState({UI:this.UI_type.customDisplay});
    }
    else if(urlParam.v!==undefined && urlParam.hash!==undefined)
    {

      this.onQRScanResult(JSON.stringify(urlParam));
      this.setState({UI:this.UI_type.analysis});
    }
    else
    {
      this.setState({allowQRScan:true,UI:this.UI_type.search});
    }
  }
  
  onDefFileInfoUpdate(info)
  {
    this.setState({...this.state,DefFileInfo:info});
    console.log(info);
    DB_Query.defFileQuery(info.name,info.hash).
      then((q)=>{
        if(q.length>0)
          this.setState({...this.state,defFile:q[0].DefineFile});
        console.log(q)
      });
  }

  onQRScanResult(result){

    try {
      let res_obj=JSON.parse(result);
      if(isString(res_obj.v))
      {
         res_obj.v = Number(res_obj.v);
      }
      if(res_obj.v===0 && res_obj.hash!==undefined && res_obj.hash.length>5)
      {
        console.log(window.location.protocol);
        if(window.location.protocol === 'https:')
        {
          window.location.href = 'http:' + window.location.href.substring(window.location.protocol.length).split('?')[0]+
            "?v="+res_obj.v+"&hash="+res_obj.hash;
          
          console.log(window.location.href);
          this.setState({...this.state,allowQRScan:false});
        }
        else
        {
          this.onDefFileInfoUpdate(res_obj);
        }
      }
    } catch(e) {
        alert(e); // error in the above string (in this case, yes)!
    }
  }

  render() {
    let UI;
    switch(this.state.UI)
    {
      case this.UI_type.customDisplay:
        UI=<CostomDisplayUI/>;
        break;
      case this.UI_type.analysis:
        UI=<APP_ANALYSIS_MODE 
          DefFileHash={this.state.DefFileInfo.hash} 
          DefFileName={this.state.DefFileInfo.name}
          defFile={this.state.defFile}/>;
        break;
      case this.UI_type.search:
        UI=<XQueryInput onQueryRes={(res)=>{console.log(res)}} defaultValue="3421" placeholder="輸入名稱"/>;
        break;
    }

    return (
      <div className="App"  style={{height:"100%"}}>
        {UI}
      </div>
    );
  }
}

export default App;
