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

class App extends React.Component{

  constructor(props) {
    super(props);

    this.state={
      DefFileInfo:undefined
    }
  }
  
  onDefFileInfoUpdate(info)
  {
    this.setState({...this.state,DefFileInfo:info});
    console.log(info);
    DB_Query.defFileQuery(info.name,info.hash).
      then((q)=>console.log(q));
  }

  onQRScanResult(result){

    try {
      let res_obj=JSON.parse(result);
      if(res_obj.v===0 && res_obj.hash!==undefined && res_obj.hash.length>5)
        this.onDefFileInfoUpdate(res_obj);
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
            (this.state.DefFileInfo===undefined)?
              <WebCam_SQScan style={{width:"100%",height:"100%"}} onScanResult={this.onQRScanResult.bind(this)}/>:
              null
          }
        </Modal>
        {(this.state.DefFileInfo!==undefined)?
          <div>{JSON.stringify(this.state.DefFileInfo)}</div>:null
        }
      </div>
    );
  }
}

export default App;
