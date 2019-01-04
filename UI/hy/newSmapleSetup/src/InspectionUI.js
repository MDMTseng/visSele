'use strict'


import { connect } from 'react-redux'
import React from 'react';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import EC_zh_TW from "./languages/zh_TW";
import G2 from '@antv/g2';
import DataSet from '@antv/data-set';


class G2HOT extends React.Component{
  constructor(props) {
    super(props);
    this.state={
      G2Chart:undefined,
      divID:"asdcsdacadsc"+Math.round(Math.random()*10000),

      jdata:[{"name":"n1","carat":1000,"cut":"Ideal","depth":61.4,"table":57,"price":10},
        {"name":"n2","carat":2000,"cut":"Good","depth":64,"table":57,"price":20},
        {"name":"n3","carat":3000,"cut":"Ideal","depth":59.2,"table":60,"price":30},
        {"name":"n4","carat":4000,"cut":"Ideal","color":"J","depth":63,"table":56,"price":40}]
    }

  }

  componentWillUpdate(nextProps, nextState) {

    this.state.jdata.push( {"carat":2000+(Math.random()-0.5)*400,"price":20+(Math.random()-0.5)*1});
    let dv = new DataSet.View().source(this.state.jdata);
    dv.transform({
      type: 'kernel-smooth.density',
      fields: ['carat', 'price'],
      as: ['carat', 'price', 'density']
    });

    this.state.view.source(dv);

    this.state.G2Chart.source(this.state.jdata);
    this.state.G2Chart.render();
  }
  componentDidMount() {
    this.state.G2Chart = new G2.Chart({
      container: this.state.divID,
      //forceFit:true
    });

    this.state.G2Chart.legend({
      offset: 45
    });
    this.state.G2Chart.point().position('carat*price');

    this.state.view = this.state.G2Chart.view();
    this.state.view.axis(false);
    this.state.view.heatmap().position('carat*price').color('density', 'blue-cyan-lime-yellow-red');

  }

  onResize(width,height){
    console.log("G2HOT resize>>",width,height);
    this.state.G2Chart.changeSize(width,height);

  }

  render(){

    return <div className={this.props.className} id={this.state.divID}>

      <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
    </div>
  }

}
class G2LINE extends React.Component{
  constructor(props) {
    super(props);
    this.state={
      G2Chart:undefined,
      divID:"asdcsdacadsc"+Math.round(Math.random()*10000),

    }

  }
  componentWillUpdate(nextProps, nextState) {
    // if(nextProps.data===undefined)
    //   return;
    this.state.G2Chart.source(nextProps.data);
    this.state.G2Chart.interval().position('genre*sold').color('genre');
    this.state.G2Chart.render();
  }
  componentDidMount() {

    this.state.G2Chart = new G2.Chart({
      container: this.state.divID,
      //forceFit:true
    });
  }

  onResize(width,height){
    console.log("G2HOT resize>>",width,height);
    this.state.G2Chart.changeSize(width,height);

  }

  render(){

    return <div className={this.props.className} id={this.state.divID}>

      <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
    </div>
  }

}

class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }

  ec_canvas_EmitEvent(event){
    console.log(event);
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.INSP_CanvasComponent(this.refs.canvas);
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
      if(props.edit_info.session_lock!=null && props.edit_info.session_lock.start == false)
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
    this.updateCanvas(nextProps.c_state,nextProps);
  }

  render() {
    return (
      <div className={this.props.addClass}>
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
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);

class APP_INSP_MODE extends React.Component{


  componentDidMount()
  {
    
    this.props.ACT_WS_SEND(this.props.WS_ID,"CI",0,{deffile:"data/test.ic.json"});
           
  }
  componentWillUnmount() {
    this.props.ACT_WS_SEND(this.props.WS_ID,"CI",0,{});
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
  }

  render() {

    let MenuSet=[];
    let menu_height="HXA";//auto
    console.log("CanvasComponent render");
      MenuSet=[
          <BASE_COM.IconButton
              dict={EC_zh_TW}
          key="<"
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          
        ];


    return(
    <div className="HXF">
      <G2HOT className={"height4 width12"} data={this.props.reportStatisticState.measure1}/>
      <CanvasComponent_rdx addClass="layout width12 height8" onCanvasInit={(canvas)=>{this.ec_canvas=canvas}}/>
        
      <$CSSTG transitionName = "fadeIn">
        <div key={"MENU"} className={"s overlay scroll MenuAnim " + menu_height}>
          {MenuSet}
        </div>
      </$CSSTG>
      
    </div>
    );
  }
}


const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
    
  }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    shape_list:state.UIData.edit_info.list,
    WS_ID:state.UIData.WS_ID,
    reportStatisticState:state.UIData.edit_info.reportStatisticState

  }
};

const APP_INSP_MODE_rdx = connect(
    mapStateToProps_APP_INSP_MODE,
    mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;