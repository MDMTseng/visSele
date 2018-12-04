'use strict'


import { connect } from 'react-redux'
import React from 'react';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';





class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }

  ec_canvas_EmitEvent(event){
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
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
      
      this.ec_canvas.EditDBInfoSync(props.edit_info);
      this.ec_canvas.SetState(ec_state);
      //this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
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
    
    console.log("CanvasComponent render",nextProps.c_state);
    //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];
    
    console.log(nextProps.edit_info.inherentShapeList);
    this.updateCanvas(nextProps.c_state,nextProps);
  }

  render() {
    return (
      <div className={this.props.addClass+" HXF"}>
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
    
    this.props.ACT_WS_SEND(this.props.WS_ID,"II",0,{deffile:"data/test.ic.json", imgsrc:"data/testInsp.bmp"});
           
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
          <BASE_COM.Button
          key="<"
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          
        ];

    return(
    <div className="HXF">
      <CanvasComponent_rdx addClass="layout width12" onCanvasInit={(canvas)=>{this.ec_canvas=canvas}}/>
        
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
    edit_info:state.UIData.edit_info

  }
};

const APP_INSP_MODE_rdx = connect(
    mapStateToProps_APP_INSP_MODE,
    mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;