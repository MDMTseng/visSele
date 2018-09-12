'use strict'
   
import styles from '../style/basis.css'
import sp_style from '../style/sp_style.css'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';

import ReactResizeDetector from 'react-resize-detector';
 
import Websocket from 'react-websocket';  
import BPG_Protocol from './UTIL/BPG_Protocol.js'; 
import BPG_WEBSOCKET from './UTIL/BPG_WebSocket.js';  




class MenuComponent extends React.Component{

    constructor(props) {
      super(props);
    }

    componentWillMount()
    {

    }
    componentWillUnmount()
    {
    }
    handleDropDownClick(event,caller) {

      //Store.dispatch(UIAct.UIACT_SetMENU_EXPEND(!Store.getState().UIData.MENU_EXPEND))
      //this.setState({ifShowDropDown:!this.state.ifShowDropDown});
    }
    handlePageTabClick(event,caller) {

      //Store.dispatch(UIAct.UIACT_SetMENU_EXPEND(!Store.getState().UIData.MENU_EXPEND))
      //this.setState({ifShowDropDown:!this.state.ifShowDropDown});
    }

    shouldComponentUpdate(nextProps, nextState) {

      return true;
    }
    render() {

      return(
        <BASE_COM.CardFrameWarp addClass={this.props.className} fixedFrame={true}>
          <div className=" HXF width8"></div>
          <BASE_COM.Button
            addClass="lgreen HXF width4"
            text="..."
            onClick={this.handlePageTabClick}/>
        </BASE_COM.CardFrameWarp>
      );
    }
}

class EverCheckCanvasComponent{

  getMousePos(canvas, evt) {
    var rect = canvas.getBoundingClientRect();
    let  mouse = {
      x: evt.clientX - rect.left,
      y: evt.clientY - rect.top
    };
    return mouse;
  }



  constructor( canvasDOM )
  {
    this.canvas = canvasDOM;

    this.canvas.onmousemove=this.onmousemove.bind(this);

    this.canvas.onmousedown=this.onmousedown.bind(this);

    this.canvas.onmouseup=this.onmouseup.bind(this);


    this.canvas.addEventListener('wheel',function(event){
      this.onmouseswheel(event);
      return false; 
    }.bind(this), false);

    this.mouseStatus={x:-1,y:-1,px:-1,py:-1,status:0};

    this.ReportJSON={};

    this.secCanvas_rawImg=null;

    this.secCanvas = document.createElement('canvas');

    this.transformMat= new  DOMMatrix();
    this.cameraMat= new  DOMMatrix();
    this.dragMat= new  DOMMatrix();
    this.Mouse2SecCanvas= new  DOMMatrix();


    this.camera={
      scale: 1,
      scaleCenter:{x:0,y:0},
      rotate: 1,
      translate: {x:0,y:0,dx:0,dy:0}
    };


  }


  SetReport( report )
  {
    this.ReportJSON = report;
    this.draw();
  }
  SetImg( img )
  {
    if(img == null)return;
    console.log(img);
    this.secCanvas.width = img.width;
    this.secCanvas.height = img.height;

    this.secCanvas_rawImg=img;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(img, 0, 0);
  }

  onmouseswheel(evt)
  {
    console.log("onmouseswheel",evt);

    let scale = 1.05;

    scale = Math.pow(scale,evt.deltaY);


    this.cameraMat.scaleSelf(scale,scale);
    this.camera.scale*=scale;
    this.draw();
  }
  onmousemove(evt)
  {
    //console.log("onmousemove");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    if(this.mouseStatus.status==1)
    {
      this.setDOMMatrixIdentity(this.dragMat);
      this.dragMat.translateSelf(pos.x-this.mouseStatus.px,pos.y-this.mouseStatus.py);


      this.camera.translate.dx=(pos.x-this.mouseStatus.px)/this.camera.scale;
      this.camera.translate.dy=(pos.y-this.mouseStatus.py)/this.camera.scale;

      this.rotate2d_dxy(this.camera.translate, this.camera.translate, -this.camera.rotate);
    }

    //this.setDOMMatrixIdentity(this.cameraMat);
    //this.cameraMat.translateSelf(pos.x,pos.y);


    this.camera.scaleCenter.x = pos.x-this.canvas.width/2;
    this.camera.scaleCenter.y = pos.y-this.canvas.width/2;
    this.draw();

  }

  onmousedown(evt)
  {
    console.log("onmousedown");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.px=pos.x;
    this.mouseStatus.py=pos.y;
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 1;
  }

  onmouseup(evt)
  {
    console.log("onmouseup");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 0;

    this.camera.translate.x+=this.camera.translate.dx;
    this.camera.translate.y+=this.camera.translate.dy;
    this.camera.translate.dx = 0;
    this.camera.translate.dy = 0;

    this.cameraMat.multiplySelf(this.dragMat);
    this.setDOMMatrixIdentity(this.dragMat);

    this.draw();
  }

  resize(width,height)
  {
    this.canvas.width=width;
    this.canvas.height=height;
    this.draw();
  }


  rotate2d_dxy(coord_dst, coord_src, theta) {
    let sin_v = Math.sin(theta);
    let cos_v = Math.cos(theta);
    let tmp_dx= coord_src.dx;

    coord_dst.dx = tmp_dx * cos_v - coord_src.dy * sin_v;
    coord_dst.dy = tmp_dx * sin_v + coord_src.dy * cos_v;
  }

  getCameraMat()
  {
    return this.cameraMat;
  }


  drawReportJSON(context,Report,depth=0) {

    if (Report.type == "binary_processing_group")
    {
      //console.log("binary_processing_group>>");
    }
    else if (Report.type == "sig360_circle_line")
    {
      //console.log("sig360_circle_line>>");
    }
    else
    {
      //console.log("sig360_circle_line_single_report>>",Report);

      let offset = 0.5;
      let x_offset = offset + Report.cx;
      let y_offset = offset + Report.cy;

      if(Array.isArray(Report.detectedLines))
        Report.detectedLines.forEach((line,idx)=>{
          context.beginPath();
          context.moveTo(line.x0+x_offset,line.y0+y_offset);
          context.lineTo(line.x1+x_offset,line.y1+y_offset);
          context.closePath();
          context.stroke();
        });


      if(Array.isArray(Report.detectedCircles))
        Report.detectedCircles.forEach((circle,idx)=>{
          context.beginPath();
          context.arc(circle.x+x_offset,circle.y+y_offset,circle.r,0,Math.PI*2, false);
          context.closePath();
          context.stroke();
        });
    }

    /*context.lineWidth = 2;
    // context.strokeStyle="rgba(255,0,0,0.5)";
    context.strokeStyle = lerpColor('#ff0000', '#0fff00', i/RXJS.reports[j].reports.length);*/


    if(typeof Report.reports !=='undefined')
    {
      Report.reports.forEach((report)=>{
        this.drawReportJSON(context,report,depth+1);
      });
    }

  }



  setDOMMatrixIdentity(mat)
  {
    mat.a=1;
    mat.b=0;
    mat.c=0;
    mat.d=1;
    mat.e=0;
    mat.f=1;
  }
 
  draw()
  {
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');

    ctx.setTransform(1,0,0,1,0,0); 
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);


    {

      var mat = this.transformMat;
      this.setDOMMatrixIdentity(mat);
      ctx.translate((this.canvas.width / 2), (this.canvas.height / 2));

      //mat.multiplySelf(this.getCameraMat());
      //mat.multiplySelf(this.dragMat);

      //ctx.translate(this.camera.scaleCenter.x, this.camera.scaleCenter.y);

      ctx.scale(this.camera.scale, this.camera.scale);
      //ctx.translate(-this.camera.scaleCenter.x/this.camera.scale, -this.camera.scaleCenter.y/this.camera.scale);

      ctx.rotate(this.camera.rotate);

      ctx.translate(this.camera.translate.x+this.camera.translate.dx, this.camera.translate.y+this.camera.translate.dy);

      ctx.translate(-(this.secCanvas.width / 2), -(this.secCanvas.height / 2));

      //ctx.setTransform(mat.a,mat.b,mat.c,mat.d,mat.e,mat.f);
      ctx.drawImage(this.secCanvas,0,0);

      if(typeof this.ReportJSON !=='undefined')
      {
        this.drawReportJSON(ctx,this.ReportJSON);
      }


      this.Mouse2SecCanvas = ctx.getTransform().invertSelf();

      //ctx.setTransform(1,0,0,1,0,0); 

      let invMat =this.Mouse2SecCanvas;
      let mPos = this.mouseStatus;

      let XX= mPos.x * invMat.a + mPos.y * invMat.c + invMat.e;
      let YY= mPos.x * invMat.b + mPos.y * invMat.d + invMat.f;
      ctx.fillRect(XX,YY, 100, 100);
    }

    //ctx.fillRect(this.mouseStatus.x,this.mouseStatus.y, 100, 100);
  }
}

class CanvasComponent extends React.Component {

  componentDidMount() {
    this.ec_canvas=new EverCheckCanvasComponent(this.refs.canvas);
    //this.updateCanvas();
  }
  updateCanvas() {
    if(this.ec_canvas  !== undefined)
    {
      console.log(this.props);
      this.ec_canvas.SetReport(this.props.checkReport.report);
      this.ec_canvas.SetImg(this.props.checkReport.img);
      this.ec_canvas.draw();
    }
  }

  onResize(width,height){
    if(this.ec_canvas  !== undefined)
    {
      this.ec_canvas.resize(width,height);
      this.updateCanvas();
    }
  }
  render() {
    this.updateCanvas();
    return (
      <div className="width12 HXF">
        <canvas ref="canvas" className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }   
}

class APPMaster extends React.Component{

  onMessage(evt)
  {
    console.log("onMessage:::");
    console.log(evt);
    if (evt.data instanceof ArrayBuffer) {
      let header = BPG_Protocol.raw2header(evt);
      console.log("onMessage:["+header.type+"]");
      if(header.type === "HR")
      {
          this.state.ws.send(BPG_Protocol.obj2raw("HR",{a:["d"]}));

          // websocket.send(BPG_Protocol.obj2raw("TG",{}));
          setTimeout(()=>{
              this.state.ws.send(BPG_Protocol.obj2raw("TG",{}));
          },0);
      }
      else if(header.type === "SS")
      {
          let SS =BPG_Protocol.raw2obj(evt);
          // console.log(header);
          console.log("Session start:",SS);
      }
      else if(header.type === "IM")
      {
          let pkg = BPG_Protocol.raw2Obj_IM(evt);
          this.state.checkReport.img = new ImageData(pkg.image, pkg.width);
      }
      else if(header.type === "IR")
      {
          let IR =BPG_Protocol.raw2obj(evt);
          console.log("IR",IR);
          this.state.checkReport.report = IR.data;

      }


    }
    // RXMSG.listAll();

  }


  constructor(props) {
      super(props);
      let binary_ws = new BPG_WEBSOCKET.BPG_WebSocket("ws://localhost:4090");
      binary_ws.onmessage_bk = this.onMessage.bind(this);
      this.state ={
        buttonCount:1,
        ws:binary_ws,
        checkReport:{
          report:{},
          img:null
        }
      };
  }

  componentWillMount()
  {
  }
  componentWillUnmount()
  {
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  render() {
    return(
    <div className="HXF">
      <BASE_COM.CardFrameWarp addClass="width12 height10" fixedFrame={true}>
        <CanvasComponent checkReport={this.state.checkReport}/>
      </BASE_COM.CardFrameWarp>
      <MenuComponent className="height2"/>
    </div>
    );
  }
}



class APPMasterX extends React.Component{

  constructor(props) {
    super(props);
    this.state={};
    this.state.isStoreInited=true;

    setTimeout(function(){
      console.log(">>>>");

      this.state.isStoreInited=true;
      this.setState(this.state);
    }.bind(this),1000)

  }

  componentWillMount()
  {
      //
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      {
        (this.state.isStoreInited)?
        <APPMaster key="APP" />:
        <div key="LOGO" className="HXF WXF overlay veleXY logoFrame white">
          <div className="veleXY width6 height6">
            <img className="height8 LOGOImg " src="resource/image/NotiMon.svg"></img>
            <div className="HX0_5"/>
            <div>
              <div className="TitleTextCon showOverFlow HX2">
                <h1 className="Title">GO  !!</h1>
                <h1 className="Title">NOTIMON</h1>
              </div>
            </div>
          </div>
        </div>
      }
    </$CSSTG>
    );
  }
}



ReactDOM.render(<APPMasterX/>,document.getElementById('container'));
