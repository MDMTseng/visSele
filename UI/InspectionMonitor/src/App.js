import React from 'react';
import logo from './logo.svg';
import './App.css';
import APP_ANALYSIS_MODE from "./AnalysisUI";
import * as DB_Query from './UTIL/DB_Query';
import  Modal  from 'antd/lib/modal';
import QrScanner from 'qr-scanner';

QrScanner.WORKER_PATH = "./qr-scanner-worker.min.js";

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
      paramName = paramName.toLowerCase();
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

class App extends React.Component{

  constructor(props) {
    super(props);


    console.log(getAllUrlParams());
    this.state={
      DefFileInfo:undefined,
      allowQRScan:false
      //{v: 0, name: "BOS-LT13BH3421", hash: "9fa42a5e990e4da632070e95daf14ec50de8a112"}
    }
  }
  componentDidMount() {

    let urlParam = getAllUrlParams();
    if(urlParam.v!==undefined && urlParam.name!==undefined && urlParam.hash!==undefined)
    {
      this.onQRScanResult(JSON.stringify(urlParam));
    }
    else
    {
      this.setState({...this.state,allowQRScan:true});
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
            "?v="+res_obj.v+"&hash="+res_obj.hash+"&name="+res_obj.name;
          
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
    return (
      <div className="App">
        
        <Modal
          title={null}
          visible={this.state.DefFileInfo===undefined}
          onCancel={this.props.onCancel}
          onOk={this.props.onOk}
        >
          {
            (this.state.DefFileInfo===undefined && this.state.allowQRScan)?
              <WebCam_SQScan style={{width:"100%",height:"100%"}} 
              onScanResult={this.onQRScanResult.bind(this)}/>:
              null
          }
        </Modal>
        {(this.state.DefFileInfo!==undefined)?
          <APP_ANALYSIS_MODE 
            DefFileHash={this.state.DefFileInfo.hash} 
            DefFileName={this.state.DefFileInfo.name}
            defFile={this.state.defFile}/>:
          null
        }
      </div>
    );
  }
}

export default App;
