import React , { useState,useEffect } from 'react';
import logo from './logo.svg';
import './App.css';
import APP_ANALYSIS_MODE from "./AnalysisUI";
import * as DB_Query from './UTIL/DB_Query';
import  Modal  from 'antd/lib/modal';
import QrScanner from 'qr-scanner';
import jsonp from 'jsonp';
import moment from 'moment';
import INFO from './info.js';

import {datePrintSimple,getAllUrlParams} from './UTIL/MISC_Util';

import  Typography  from 'antd/lib/typography';
import Input from 'antd/lib/input';
import Button from 'antd/lib/button';
import Table from 'antd/lib/table';
import Col from 'antd/lib/col';
import Row from 'antd/lib/row';




import DatePicker from 'antd/lib/date-picker';
// import { MinusCircleOutlined, PlusOutlined } from '@ant-design/icons-react';
//import PlusOutlined from '@ant-design/icons/PlusOutlined';
import Layout from 'antd/lib/layout';
QrScanner.WORKER_PATH = "./qr-scanner-worker.min.js";
const { RangePicker } = DatePicker;
// import { WarningOutlined} from '@ant-design/icons';

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

function isString(val){
  return (typeof val === 'string' || val instanceof String)
}

function pjsonp(url,timeout=10000)
{
  return new Promise((res,rej)=>{
    try{

      jsonp(url,{
        timeout
      },  (err,data)=>{
        if(err === null)
            res(data);
        else
            rej(err)
      });
    }
    catch(e)
    {
      rej(e)
    }
  });
}

function fetchDeffileInfo(name,time_range_ms=[0,9999999999])
{
  let defFileData=undefined;
  
  let url=DB_Query.db_url+'/query/deffile?name='+name+'&limit=1000'+
  '&projection={"DefineFile.name":1,"DefineFile.featureSet_sha1":1,"DefineFile.featureSet_sha1_root":1,"DefineFile.featureSet_sha1_pre":1,"createdAt":1}'

  return pjsonp(url).then((data)=>{

      
    let hashArr = data.map(srec=>srec.DefineFile.featureSet_sha1)
    hashArr = [...new Set(hashArr)];//remove replicated hash

    defFileData=hashArr.map((hash)=>{
      let newest_def = data.filter((defF)=>defF.DefineFile.featureSet_sha1==hash)
                      .reduce((latest,def)=>
                      {
                        let p_createdAt = Date.parse(def.createdAt);
                        if(latest===undefined)
                        {
                          return {p_createdAt,...def};
                        }
                        
                        if(latest.p_createdAt<p_createdAt)
                        {
                          return latest;
                        }
                        return {p_createdAt,...def};
                      });
      
      return {hash,...newest_def,name:newest_def.DefineFile.name}
    });

    defFileData.forEach((def)=>{
      if(def.p_createdAt===undefined)
      {
        def.p_createdAt=Date.parse(def.createdAt);
      }
    })
    let hashRegx = hashArr.reduce((acc,hash)=>acc===undefined?hash:acc+"|"+hash,undefined)

    let url=DB_Query.db_url+'/query/inspection?';
    url+='tStart='+time_range_ms[0]+'&tEnd='+time_range_ms[1]+'&limit=9999999&';
    url+='subFeatureDefSha1='+hashRegx+'&'
    url+='projection={"_id":0,"InspectionData.subFeatureDefSha1":1,"InspectionData.time_ms":1,"InspectionData.tag":1}&'
    url+='agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1",'+
    '"count": {"$sum":1},'+
    '"time_start": {"$min":"$InspectionData.time_ms"},'+
    '"time_end": {"$max":"$InspectionData.time_ms"},'+
    '"tags": {"$addToSet":"$InspectionData.tag"}'+
    '}}]';
    return pjsonp(url)

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


        defF.tags = defF.tags.flat(9)
          .map(tag=>tag.replace(/^\,+/g, "").replace(/\,{2,}/g, ",").split(","))
          .flat(9)
          .filter(tag=>tag.length>0)
        defF.tags = [...new Set(defF.tags)];
      }
    })
    return (defFileData)
  }).catch((err)=>{
    throw err;
  })
}

///query/inspection?tStart=1583942400000&tEnd=1584639131675&subFeatureDefSha1_regex=.&projection={"InspectionData.subFeatureDefSha1":1}&agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1","sum":{"$sum":1}}}]

function fetchDeffileInfo_in_insp_time_range(start_ms,end_ms)
{
  let dataSet_Formatted;
  return new Promise((res,rej)=>{
    let url=DB_Query.db_url+"/query/inspection?tStart="+start_ms+"&tEnd="+end_ms+
    '&projection={"InspectionData.subFeatureDefSha1":1,"InspectionData.time_ms":1,"InspectionData.tag":1}'+
    '&agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1",'+
      '"count":{"$sum":1},'+
      '"time_start": {"$min":"$InspectionData.time_ms"},'+
      '"time_end": {"$max":"$InspectionData.time_ms"},'+
      '"tags": {"$addToSet":"$InspectionData.tag"}'+
    '}}]';


    pjsonp(url).then((dataSet)=>{

      dataSet_Formatted=
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
      let hashRegx = Object.keys(dataSet_Formatted).reduce((acc,hash)=>acc===undefined?hash:acc+"|"+hash,undefined)

      console.log(dataSet,dataSet_Formatted,hashRegx);
      //'/query/deffile?featureSet_sha1='+hashRegx
      let url=DB_Query.db_url+'/query/deffile?featureSet_sha1='+hashRegx+
      '&projection={"DefineFile.name":1,"DefineFile.featureSet_sha1":1,"DefineFile.featureSet_sha1_root":1,"DefineFile.featureSet_sha1_pre":1,"createdAt":1}'
      
      return pjsonp(url)

    }).then((defFileData)=>{


      console.log(defFileData);


      let sha1_dict=defFileData.reduce((obj,df)=>{
        let sha1=df.DefineFile.featureSet_sha1;
        let time = Date.parse(df.createdAt);
        df.p_createdAt = time;
        if(obj[sha1]===undefined || obj[sha1].p_createdAt<df.p_createdAt)
        {
          obj[sha1]=df;
        }
        
        return obj;
      },{})//find the latest featureSet_sha1 and ignore the ole one
      console.log(sha1_dict);
      defFileData = Object.values(sha1_dict)

      // console.log(defFileData);
      defFileData.forEach((defF)=>{
        defF.hash=defF.DefineFile.featureSet_sha1;
        defF.name=[defF.DefineFile.name];
        let tar = dataSet_Formatted[defF.hash];
        if(tar!==undefined)
        {
          Object.assign(defF, tar)
          defF.tags = defF.tags.flat(9)
            .map(tag=>tag.replace(/^,+|,+$/g, "").replace(/\,{2,}/g, ",").split(","))
            .flat(9)
            .filter(tag=>tag.length>0)
          defF.tags = [...new Set(defF.tags)];
        }
      })
      console.log(defFileData);



      res(defFileData)
    }).catch((err)=>{
      rej(err);
    })
  });
}


let CusDisp_DB={
  read:(name)=>
  {
    let defFileData=undefined;
    
    return new Promise((res,rej)=>{
      let url=DB_Query.db_url+'/QUERY/customDisplay?name='+name
      url+='&projection={"name":1,"targetDeffiles":1}'
      
      pjsonp(url).then((data)=>{
        res(data);
      }).catch((err)=>{
        rej(err);
      })
    });
  },
  create:(info,id)=>{
    let defFileData=undefined;
    
    return new Promise((res,rej)=>{
      let url=DB_Query.db_url+'/insert/customdisplay?name='+info.name+
        "&targetDeffiles="+JSON.stringify(info.targetDeffiles)
      if(id!==undefined)
      {
        url+="&_id="+id;
        
      }
      pjsonp(url).then((data)=>{
        res(data);
      }).catch((err)=>{
        rej(err);
      })
    });
  },
  delete:(id)=>{

    return new Promise((res,rej)=>{
      let url=DB_Query.db_url+'/delete/customdisplay?_id='+id;
      pjsonp(url).then((data)=>{
        res(data);
      }).catch((err)=>{
        rej(err);
      })
    });
  }
}
CusDisp_DB.update=(info,id)=>{
  return new Promise((res,rej)=>{
    if(id===undefined)
    {
      return rej("Error:No id");
    }
    CusDisp_DB.create(info,id).then((data)=>{
      res(data);
    }).catch((err)=>{
      rej(err);
    })
  });
}
///delete/customDisplay?_id=5e782cddf40281013bedd142
// fetchCustomDisplayInfo("Machine");
//CusDisp_DB.create({name:"><>",targetDeffiles:[{hash:"sdiosdjciojsdoi"}]},undefined);
//CusDisp_DB.delete("5e771c388b25286acf112810");
function getUrlPath()
{
  return window.location.href.substring(window.location.protocol.length).split('?')[0]
}

function Date_addDay(date,addDays)
{
  if( date===undefined)date=new Date();

  return date.setDate(date.getDate() + addDays);
}

function DateRange(dayCount1,dayCount2)
{
  return [moment(Date_addDay(new Date(),dayCount1)), moment(Date_addDay(new Date(),dayCount2))];
}


function XQueryInput({ onQueryRes,onQueryRej,placeholder,defaultValue }) {
  const [fetchedRecord, setFetchedRecord] = useState([]);
  const [searchDateRange, setSearchDateRange] = useState(DateRange(-7,1));//by default one week
  
  console.log(searchDateRange);
  const [modal_view,setModal_view]=useState(undefined);
  function pop_dataRetrive()
  {

    setModal_view({
      view_fn:()=>"取得資料中",
    })
  }
  function pop_dataRetriveFailed()
  {

    setModal_view({
      view_fn:()=>"取得資料失敗",
      title:"!",
    })
  }
  function pop_disable()
  {
    
    setModal_view();
  }

  function recentQuery(dateRange=searchDateRange)
  {
    
    return fetchDeffileInfo_in_insp_time_range(dateRange[0]._d.getTime(),dateRange[1]._d.getTime()).
    then((res)=>{

      setFetchedRecord(res);
      onQueryRes(res);
    }).catch((e)=>{
      setFetchedRecord([]);
      console.log(e);
      if(onQueryRej!==undefined)
        onQueryRej(e)
      throw e;
    });
  }



  useEffect(() => {
    console.log("1,didUpdate");
    pop_dataRetrive();

    try{
      recentQuery(searchDateRange)
      .then(_=>{
        console.log(_)
        pop_disable();
      })
      .catch(e=>{
        console.log(e)
        pop_dataRetriveFailed();
      });
    }
    catch(e)
    {
      pop_dataRetriveFailed();
      console.log(e)
    }

    return () => {
      console.log("1,didUpdate ret::");
    };
  },[]);

  let searchBox=<Input placeholder={placeholder} defaultValue={defaultValue}
    onPressEnter={(e)=>{
    console.log(e.target.value)
    if(e.target.value=="")
    {
      pop_dataRetrive();
      recentQuery(searchDateRange)
      .then(_=>{
        pop_disable();
      })
      .catch(_=>{
        pop_dataRetriveFailed();
      });
    }
    else if(e.target.value.length<2)
    {
      
      setModal_view({
        view_fn:()=>"請輸入大於兩個字",
        title:"!",
      })
    }
    else
    {
      
      pop_dataRetrive();
      setFetchedRecord();
      fetchDeffileInfo(e.target.value,[searchDateRange[0]._d.getTime(),searchDateRange[1]._d.getTime()]).
        then((res)=>{
          pop_disable();
          setFetchedRecord(res);
          onQueryRes(res);
        }).catch((e)=>{
          
          pop_dataRetriveFailed();
          setFetchedRecord([]);
          if(onQueryRej!==undefined)
            onQueryRej(e)
        });
    }
  }} ></Input>
  let fetchBtn=fetchedRecord===undefined?null:
  <div>

  </div>


  let displayInfo=null
  if(fetchedRecord!==undefined)
  {
    const columns = [
      {
        title: 'Name',
        dataIndex: 'name',
        key: 'name',
        render: (name,fetchRec) => <>
          {fetchRec.level==0?null:"⮑"}
          {fetchRec.link===undefined?name:<a href={fetchRec.link} target="_blank">{name}</a>}  
          {fetchRec.children===undefined?null:`:+${fetchRec.children.length}`}
          </>,
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
        render: milliSec => datePrintSimple(new Date(milliSec)),
        sorter: (a, b) => a.Date_Start - b.Date_Start,
      },
      {
        title: 'Date_End',
        dataIndex: 'Date_End',
        key: 'Date_End',
        render: milliSec => datePrintSimple(new Date(milliSec)),
        sorter: (a, b) => a.Date_End - b.Date_End,
        defaultSortOrder:'descend'
      },
      {
        title: 'Tags',
        dataIndex: 'Tags',
        key: 'Tags',
        render: tags => tags.join(","),
      }]


    console.log(fetchedRecord);
    let defFileGroup={};
    fetchedRecord.forEach((defInfo)=>{
      let sha1_root=defInfo.DefineFile.featureSet_sha1_root||"_";
      if(defFileGroup[sha1_root]===undefined)
      {
        defFileGroup[sha1_root]=[];
      }

      defFileGroup[sha1_root].push(defInfo);
      
    });

    console.log(defFileGroup);

    let dataSource = Object.keys(defFileGroup).map((g,idx)=>{
      
      
      let urlParam = getAllUrlParams();

      let groupInfo = defFileGroup[g].filter(fetchRec=>fetchRec.count!==undefined).map(fetchRec=>
        ({
          level:1,
          name:fetchRec.name,
          count:fetchRec.count,
          Date_Start:fetchRec.time_start[0],
          Date_End:fetchRec.time_end[0],
          Tags:fetchRec.tags,
          info:fetchRec,
          hash:fetchRec.hash,
          key:fetchRec.hash,
          link:getUrlPath()+"?v=0&hash="+fetchRec.hash+"&db_server="+urlParam.db_server,
        })
      )

      let _ginfo = groupInfo.reduce((sumInfo,fetchRec)=>{
        let newInfo={...sumInfo};
        if(newInfo.Date_End<fetchRec.Date_End)
        {
          newInfo.name=fetchRec.name;
          newInfo.Date_End=fetchRec.Date_End;
        }
        if(newInfo.Date_Start>fetchRec.Date_Start)
        {
          newInfo.Date_Start=fetchRec.Date_Start;
        }
        
        newInfo.Tags=[...newInfo.Tags,...fetchRec.Tags];
        newInfo.hash=[...newInfo.hash,fetchRec.hash];
        newInfo.count+=fetchRec.count;
        newInfo.count+=fetchRec.count;
        return newInfo;
      },{
        level:0,
        name:undefined,
        count:0,
        Date_End:0,
        Date_Start:Number.MAX_VALUE,
        hash:[],
        Tags:[],
        info:groupInfo,
        children:groupInfo.length>1?groupInfo:undefined
      })
      _ginfo.Tags = [...new Set(_ginfo.Tags)];//remove replicated hash]
      _ginfo.hash = [...new Set(_ginfo.hash)];//remove replicated hash]
      _ginfo.key="-"+_ginfo.hash[0];

      if(_ginfo.hash.length==1)
        _ginfo.link=getUrlPath()+"?v=0&hash="+_ginfo.hash[0]+"&db_server="+urlParam.db_server
      return _ginfo
    }).filter(fetchRec=>fetchRec.count!==undefined &&fetchRec.count>0 )
    console.log(dataSource);
    displayInfo=<Table columns={columns} dataSource={dataSource} pagination={false}/>;
  }
  return (

    <>
      {searchBox}
      
      <Button key="_month"
        onClick={() => {setSearchDateRange(DateRange(-30,1))}}>
        月(-30)
      </Button>

      <Button key="half_month"
        onClick={() => {setSearchDateRange(DateRange(-15,1))}}>
        半月(-15)
      </Button>
      <Button key="week"
        onClick={() => {setSearchDateRange(DateRange(-7,1))}}>
        星期(-7)
      </Button>


      <RangePicker key="RP"
            value={searchDateRange} 
            onChange={(date)=>setSearchDateRange(date)}/>
      {displayInfo}
      
      <Modal
          footer={null}
          onCancel={()=>setModal_view()}
          onOk={()=>setModal_view()}
          {...modal_view}
          visible={modal_view !== undefined}>
          {modal_view === undefined ? null : modal_view.view_fn()}
      </Modal>
      version:{INFO.version}
    </>
  );
}

function SingleDisplayUI({ displayInfo})
{
  const canvasRef = React.useRef(null)


  return <div>
    <Title level={2}>{displayInfo.name}</Title>
    Name:{displayInfo.name}
    {displayInfo.targetDeffiles.map((dfs,id)=>
      "Def["+id+"]:"+JSON.stringify(displayInfo.targetDeffiles[id])
    )}
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

function CustomDisplayUI({ }) {
  const [displayInfo, setDisplayInfo] = useState(undefined);

  const [collapsed, setCollapsed] = useState(true);
  useEffect(() => {
    setDisplayInfo(undefined);
    CusDisp_DB.read(".").then(data=>{
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

  let UI=[];
  if(displayInfo!==undefined)
  {
    console.log(displayInfo);
    UI=[];
    UI.push(<div key="Reset" onClick={()=>setDisplayInfo(undefined)}>RESET....</div>)
    UI.push(<Row  key="table"  style={{height:"50%"}}>{displayInfo.map(info=>
      <Col key={info._id} span={12}  style={{height:"100%"}}>
        <SingleDisplayUI displayInfo={info}/>

        <Button key="sdsdfk"
                onClick={() => {
                  info.ddd="sss";
                  CusDisp_DB.update({name:"OKOK",targetDeffiles:[{hash:""}]},info._id).then(()=>{

                    CusDisp_DB.read(".").then(data=>{
                      console.log(displayInfo);
                      setDisplayInfo(data.prod);
                    }).catch(e=>{
                      console.log(e);
                    });
                  });
                }}
                style={{ width: '60%' }}
        >
          mod
        </Button>
        <Button type="dashed"
                onClick={() => {
                  setDisplayInfo(undefined);
                  CusDisp_DB.delete(info._id).then(()=>{

                    CusDisp_DB.read(".").then(data=>{
                      console.log(displayInfo);
                      setDisplayInfo(data.prod);
                    }).catch(e=>{
                      console.log(e);
                    });
                  });
                }}
                style={{ width: '60%' }}
        >
          Ｘ
        </Button>
      </Col>
    )}</Row>)


  }
  UI.push(<Row  key="table2"  style={{height:"50%"}}>
    <Col key={"sdsd"} span={12}  style={{height:"100%"}}>
      
      <Button type="dashed"
              onClick={() => {
                CusDisp_DB.create({name:"><>",targetDeffiles:[{hash:"sdiosdjciojsdoi"}]},undefined).then(()=>{
                  CusDisp_DB.read(".").then(data=>{
                    console.log(displayInfo);
                    setDisplayInfo(data.prod);
                  }).catch(e=>{
                    console.log(e);
                  });
                });
              }}
              style={{ width: '60%' }}
      >
        Add field
      </Button>

    </Col></Row>)


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
          <Layout.Footer>Custom UI v0.0.0</Layout.Footer>
        </Layout>
      </Layout>
    </Layout> 
  );
}


function DBDupMan({ }) {
  const [displayInfo, setDisplayInfo] = useState(undefined);

  const [collapsed, setCollapsed] = useState(true);


  function fetchDeffileInfo_in_insp_time_range(start_ms,end_ms)
  {
    let dataSet_Formatted;
    return new Promise((res,rej)=>{

    let url=DB_Query.db_url+'/query/deffile?name='+""+'&limit=1000'+
      '&agg=[{"$group":{"_id":"$DefineFile.featureSet_sha1",'+
        '"count":{"$sum":1}'+
      '}}]';
  
  
      pjsonp(url).then((dataSet)=>{
        console.log(dataSet);
      }).catch((err)=>{
        rej(err);
      })
    });
  }
  
  fetchDeffileInfo_in_insp_time_range();


  return ">>";
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
      dbDupMan:"dbDupMan",
      analysis:"analysis",
      search:"search",
    }
  }
  componentDidMount() {

    let urlParam = getAllUrlParams();
    console.log(urlParam);


    if(urlParam.db_server!=undefined)
    {
      let url = urlParam.db_server;
      DB_Query.setupDBUrl("http://"+url);
    }


    if(urlParam.UI==="customDisplay")
    {
      this.setState({UI:this.UI_type.customDisplay});
    }
    if(urlParam.UI==="dbDupMan")
    {
      this.setState({UI:this.UI_type.dbDupMan});
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
          this.setState({...this.state,defFile:q[q.length-1].DefineFile});
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
          let urlParam = getAllUrlParams();
          window.location.href = 'http:' + window.location.href.substring(window.location.protocol.length).split('?')[0]+
            "?v="+res_obj.v+"&hash="+res_obj.hash+"&db_server="+urlParam.db_server;
          
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
        UI=<CustomDisplayUI/>;
        break;
      case this.UI_type.dbDupMan:
        UI=<DBDupMan/>;
        break;
      case this.UI_type.analysis:
        UI=<APP_ANALYSIS_MODE 
          DefFileHash={this.state.DefFileInfo.hash} 
          DefFileName={this.state.DefFileInfo.name}
          defFile={this.state.defFile}/>;
        break;
      case this.UI_type.search:
        UI=<XQueryInput onQueryRes={(res)=>{console.log(res)}} defaultValue="" placeholder="輸入名稱"/>;
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
