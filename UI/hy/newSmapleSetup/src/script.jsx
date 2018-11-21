'use strict'
   


import styles from 'STYLE/basis.css'
import sp_style from 'STYLE/sp_style.css'
import { Provider, connect } from 'react-redux'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';
 
import BPG_Protocol from 'UTIL/BPG_Protocol.js'; 
import BPG_WEBSOCKET from 'UTIL/BPG_WebSocket.js';  

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';

let StoreX= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }

  ec_canvas_EmitEvent(event){
    switch(event.type)
    { 
      case UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS:
        this.props.ACT_SUCCESS();
      break;
      case UIAct.UI_SM_EVENT.EDIT_MODE_FAIL:
        this.props.ACT_Fail();
      break; 
      case UIAct.UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update:
        //console.log(event);
        this.props.ACT_EDIT_TAR_UPDATE(event.data);
      break; 
      case UIAct.UI_SM_EVENT.EDIT_MODE_Edit_Tar_Ele_Cand_Update:
        //console.log(event);
        this.props.ACT_EDIT_TAR_ELE_CAND_UPDATE(event.data);
      break; 
      case UIAct.UI_SM_EVENT.EDIT_MODE_Shape_List_Update:
        console.log(event);
        this.props.ACT_EDIT_SHAPELIST_UPDATE(event.data);
      break; 
      case UIAct.UI_SM_EVENT.EDIT_MODE_Shape_Set:
        console.log(event);
        this.props.ACT_EDIT_SHAPE_SET(event.data);
      break; 
    }
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.EverCheckCanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent=this.ec_canvas_EmitEvent.bind(this);
    this.props.onCanvasInit(this.ec_canvas);
  }
  updateCanvas(state,props=this.props) {
    if(this.ec_canvas  !== undefined)
    {
      console.log("updateCanvas>>",props.edit_info.inherentShapeList);
      
      
      this.ec_canvas.SetShapeList(props.edit_info.list);
      this.ec_canvas.SetInherentShapeList(props.edit_info.inherentShapeList);
      this.ec_canvas.SetEditShape(props.edit_info.edit_tar_info);
      
      this.ec_canvas.SetReport(props.report);
      this.ec_canvas.SetImg(props.img);
      this.ec_canvas.SetState(state);
      //this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
    }
  }

  onResize(width,height){
    if(this.ec_canvas  !== undefined)
    {
      let stateObj = xstate_GetCurrentMainState(this.props.c_state);
      this.ec_canvas.resize(width,height);
      this.updateCanvas(stateObj.substate);
      this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
    }
  }
  componentWillUpdate(nextProps, nextState) {
    
    console.log("CanvasComponent render",nextProps.c_state);
    //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.EDIT_MODE];
    
    let stateObj = xstate_GetCurrentMainState(nextProps.c_state);
    let substate = stateObj.substate;
    console.log("substate:"+substate,stateObj);
    console.log(nextProps.edit_info.inherentShapeList);
    this.updateCanvas(substate,nextProps);
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
    report: state.UIData.report,
    img: state.UIData.img,
    edit_info: state.UIData.edit_info,

  }
}



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => 
{ 
  return{
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Update(targetObj))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_SHAPELIST_UPDATE: (shapeList) => {dispatch(UIAct.EV_UI_EDIT_MODE_Shape_List_Update(shapeList))},
    ACT_EDIT_SHAPE_SET: (shape_data) => {dispatch(UIAct.EV_UI_EDIT_MODE_Shape_Set(shape_data))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);



class JsonElement extends React.Component{

  render()
  {
 
    if(this.props.type.type !== undefined)
    {
      return this.renderComplex();
    }
    else
    {
      //if(this.props.type instanceof String)
      
      return this.renderSimple();
      
    }
  }


  renderComplex()
  {
    switch(this.props.type.type)
    {
      case "droplist":
      return <div key={this.props.id} className={this.props.className} >
        {this.props.children}
      </div>
    }
  }
  renderSimple()
  {
    switch(this.props.type)
    {
      case "input-number":
        return <input key={this.props.id} className={this.props.className} type="number" value={this.props.children}  
          onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
      case "input":
        return <input key={this.props.id} className={this.props.className} value={this.props.children}  
          onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
      case "btn":
        return <button
          key={this.props.id}
          className={this.props.className}
          onClick={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}>
          {this.props.children}</button>
      case "div":
      default:
        return <div key={this.props.id} className={this.props.className} >{this.props.children}</div>
      
    }
  }
}

class JsonEditBlock extends React.Component{
  

  constructor(props) {
      super(props);
      this.tmp={
        object:{}
      };
  }

  onChangeX(target,type,evt) {
    this.props.jsonChange(this.tmp.object,target,type,evt);
    return true;
  }
  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  composeObject(obj,whiteListKey=null,idHeader="",keyTrace=[])
  {
    var rows = [];
    let keyList = (whiteListKey==null )?obj:whiteListKey;
    for (var key in keyList) {
        let ele = obj[key];
        let displayMethod=(whiteListKey==null )?null:whiteListKey[key];

        //console.log(key,keyList[key],obj,obj[key]);
        if((ele === undefined) || displayMethod=== undefined)continue;
        //console.log(key,ele,typeof ele);
        
        let newkeyTrace = keyTrace.slice();
        newkeyTrace.push(key);
        switch(typeof ele)
        {
          case "string":
            if(displayMethod==null)displayMethod="input";
            rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width3 vbox black">{key}</div>);
            rows.push(<JsonElement key={idHeader+"_"+key+"_ele"} className="s HX1 width9 vbox blackText" type={displayMethod}
              target={{obj:obj,keyTrace:newkeyTrace}} 
              onChange={this.onChangeX.bind(this)}>{(ele)}</JsonElement>);
            
          break;
          case "number":
            if(displayMethod==null)displayMethod="input-number";
            rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width3 vbox black">{key}</div>);
            rows.push(<JsonElement key={idHeader+"_"+key+"_ele"} className="s HX1 width9 vbox blackText" type={displayMethod} 
              target={{obj:obj,keyTrace:newkeyTrace}}  
              onChange={this.onChangeX.bind(this)}>{(ele).toFixed(4)}</JsonElement>);
          break;
          case "object":
          {

            rows.push(<div key={idHeader+"_"+key+"_HL"} className="s HX0_1 WXF  vbox"></div>);
            let obj_disp_type = (displayMethod==null)?"div":displayMethod.__OBJ__;
            if(obj_disp_type == undefined)obj_disp_type="div";
            rows.push(<JsonElement key={idHeader+"_"+key+"_ele"} 
              className="s HX1 WXF vbox black" type={obj_disp_type} 
              onChange={this.onChangeX.bind(this)}
              target={{obj:obj,keyTrace:newkeyTrace}}>{key}</JsonElement>);

            rows.push(<div key={idHeader+"_"+key+"__"} className="s HX1 width1"></div>);
            rows.push(<div key={idHeader+"_"+key+"_C"} className="s HXA width11">{
              this.composeObject(ele,displayMethod,idHeader+"_"+key,newkeyTrace)
            }</div>);
            rows.push(<div key={idHeader+"_"+key+"_HL2"} className="s HX0_1 WXF  vbox"></div>);
          }
          break;
          default:
            rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width3 vbox black">{key}</div>);
            rows.push(<div key={idHeader+"_"+key+"_XXX"} className="s HX1 width9 vbox lblue">Not supported</div>);
          break;
        }
    }
    return rows
  }

  render() {
    this.tmp.object = JSON.parse(JSON.stringify(this.props.object));
   //console.log("this.props.object:",this.props.object,this.tmp.object);
    var rows = this.composeObject(this.tmp.object,this.props.whiteListKey);
    return(
    <div className="WXF HXA">
      {rows}
    </div>
    );
  }
}

class APP_EDIT_MODE extends React.Component{


  componentDidMount()
  {
    bpg_ws.send("TG");
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }


  genTarEditUI(edit_tar)
  {
    switch(edit_tar.type)
    {
      case UIAct.SHAPE_TYPE.aux_point:
      case UIAct.SHAPE_TYPE.search_point:
      case UIAct.SHAPE_TYPE.measure:
      {
        return (<JsonEditBlock object={edit_tar} 
          key="JsonEditBlock"
          whiteListKey={{
            //id:"div",
            type:"div",
            subtype:"div",
            name:"input",
            //pt1:null,
            angle:"input-number",
            value:"input-number",
            margin:"input-number",
            width:"input-number",
            ref:{__OBJ__:"div",
              0:{__OBJ__:"btn",
                id:"div",
                element:"div"},
              1:{__OBJ__:"btn",
                id:"div",
                element:"div"},
            }
          }}
          jsonChange={(original_obj,target,type,evt)=>
            {
              if(type =="btn")
              {
                if(target.keyTrace[0]=="ref")
                {
                  this.props.ACT_EDIT_TAR_ELE_TRACE_UPDATE(target.keyTrace);
                }
              }
              else
              {
                let lastKey=target.keyTrace[target.keyTrace.length-1];
                
                if(type == "input-number")
                {
                  let parseNum =  parseFloat(evt.target.value);
                  if(isNaN(parseNum))return;
                  target.obj[lastKey]=parseNum;
                }
                else if(type == "input")
                  target.obj[lastKey]=evt.target.value;
                this.ec_canvas.SetShape( original_obj, original_obj.id);
              }
            }}/>);
      }
      break;
      default:
      {
        return (<JsonEditBlock object={edit_tar} 
          key="JsonEditBlock"
          jsonChange={(original_obj,target,type,evt)=>
            {
              let lastKey = target.keyTrace[target.keyTrace.length-1];
              if(type == "input-number")
                target.obj[lastKey]=parseFloat(evt.target.value);
              else if(type == "input")
                target.obj[lastKey]=evt.target.value;
    
              console.log(target.obj);
              let updated_obj=original_obj;
              this.ec_canvas.SetShape( updated_obj, updated_obj.id);
            }}
          whiteListKey={{
            //id:"div",
            type:"div",
            subtype:"div",
            name:"input",
            margin:"input-number",
            direction:"input-number",

            /*pt1:{
              __OBJ__:"btn",
              x:"input-number",
              y:"input-number",
            }*/
          }}/>);
      }
      break;
      
    }
  }



  render() {

    let MenuSet=[];
    let menu_height="HXA";//auto
    console.log("CanvasComponent render");
    let substate = this.props.c_state.value[UIAct.UI_SM_STATES.EDIT_MODE];
    switch(substate)
    {
      case UIAct.UI_SM_STATES.EDIT_MODE_NEUTRAL:
      MenuSet=[
          <BASE_COM.Button
          key="<"
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            key="LINE"
            text="LINE" onClick={()=>this.props.ACT_Line_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            key="ARC"
            text="ARC" onClick={()=>this.props.ACT_Arc_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            key="APOINT"
            text="APOINT" onClick={()=>this.props.ACT_Aux_Point_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            key="SPOINT"
            text="SPOINT" onClick={()=>this.props.ACT_Search_Point_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lgreen vbox"
            key="EDIT"
            text="Edit" onClick={()=>this.props.ACT_Shape_Edit_Mode()}/>,
          <BASE_COM.Button
            addClass="layout lred vbox"
            key="SAVE"
            text="SAVE" onClick={()=>{this.props.ACT_Save_Edit_Info()}}/>,
          <BASE_COM.Button
            addClass="layout lblue vbox"
            key="MEASURE"
            text="MEASURE" onClick={()=>this.props.ACT_Measure_Add_Mode()}/>,
        ];
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_MEASURE_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="MEASURE" className="s width8 lblue vbox">MEASURE</div>,
      ];

      
      if(this.props.edit_tar_info!=null)
      {
        console.log("JsonEditBlock:",this.props.edit_tar_info);

        
        MenuSet.push(<JsonEditBlock object={this.props.edit_tar_info} 
          key="JsonEditBlock"
          whiteListKey={{
            //id:"div",
            name:"input",
            //pt1:null,
            subtype:"div",
            ref:{__OBJ__:"div",
              0:{__OBJ__:"btn",
                id:"div",
                element:"div"},
              1:{__OBJ__:"btn",
                id:"div",
                element:"div"},
            }
          }}
          jsonChange={(original_obj,target,type,evt)=>
            {
              if(type =="btn")
              {
                if(target.keyTrace[0]=="ref")
                {
                  this.props.ACT_EDIT_TAR_ELE_TRACE_UPDATE(target.keyTrace);
                }
              }
              else
              {
                let lastKey=target.keyTrace[target.keyTrace.length-1];
                
                if(type == "input-number")
                  target.obj[lastKey]=parseFloat(evt.target.value);
                else if(type == "input")
                  target.obj[lastKey]=evt.target.value;
                this.ec_canvas.SetShape( original_obj, original_obj.id);
              }
            }}/>);
        if(this.props.edit_tar_info.subtype == UIAct.SHAPE_TYPE.measure_subtype.NA)
        {
          for (var key in UIAct.SHAPE_TYPE.measure_subtype) {
            if(key == "NA")continue;
            MenuSet.push(<BASE_COM.Button
              key={"MSUB__"+key}
              addClass="layout red vbox"
              text={key} onClick={(data,btn)=>
                {
                  console.log(btn.props.text);
                  this.props.ACT_EDIT_TAR_ELE_CAND_UPDATE(btn.props.text);
                }}/>);
          }
        }
      
        let tar_info = this.props.edit_tar_info;
        console.log(tar_info.ref);
        if(tar_info.ref!==undefined)
        {
          let notFullSet=false;
          tar_info.ref.forEach((ref_data)=>{
            if(ref_data.id === undefined)notFullSet=true;
          });
          if(!notFullSet)
          {
            MenuSet.push(<BASE_COM.Button
              key="ADD_BTN"
              addClass="layout red vbox"
              text="ADD" onClick={()=>
                {
                  this.ec_canvas.SetShape( this.props.edit_tar_info);
                  this.props.ACT_SUCCESS();
                }}/>);
          }
        }
      }
      break;
      
      case UIAct.UI_SM_STATES.EDIT_MODE_LINE_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="LINE" className="s width8 lred vbox">LINE</div>,
      ];
      
      break;
      case UIAct.UI_SM_STATES.EDIT_MODE_ARC_CREATE:          
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<"
          text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="ARC" className="s width8 lred vbox">ARC</div>
      ];
      break;

      case UIAct.UI_SM_STATES.EDIT_MODE_SEARCH_POINT_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="SEARCH_POINT" className="s lred vbox">SPOINT</div>,
      ];
      if(this.props.edit_tar_info!=null)
      {
        console.log("JsonEditBlock:",this.props.edit_tar_info);

        MenuSet.push(this.genTarEditUI(this.props.edit_tar_info));

        let tar_info = this.props.edit_tar_info;
        if(tar_info.ref[0].id !==undefined )
        {
          MenuSet.push(<BASE_COM.Button
            key="ADD_BTN"
            addClass="layout red vbox"
            text="ADD" onClick={()=>
              {
                this.ec_canvas.SetShape( this.props.edit_tar_info);
                this.props.ACT_SUCCESS();
              }}/>);
        }
      
      }
      break;

      case UIAct.UI_SM_STATES.EDIT_MODE_AUX_POINT_CREATE:  
      {       
        MenuSet=[
          <BASE_COM.Button
            addClass="layout black vbox"
            key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
          <div key="AUX_POINT" className="s lred vbox">APOINT</div>,
        ];

      
        if(this.props.edit_tar_info!=null)
        {
          console.log("JsonEditBlock:",this.props.edit_tar_info);

          MenuSet.push(this.genTarEditUI(this.props.edit_tar_info));

          let tar_info = this.props.edit_tar_info;
          console.log(tar_info.ref);
          if(tar_info.ref[0].id !==undefined && 
            tar_info.ref[1].id !==undefined &&
            tar_info.ref[0].id !=tar_info.ref[1].id 
            )
          {
            MenuSet.push(<BASE_COM.Button
              key="ADD_BTN"
              addClass="layout red vbox"
              text="ADD" onClick={()=>
                {
                  this.ec_canvas.SetShape( this.props.edit_tar_info);
                  this.props.ACT_SUCCESS();
                }}/>);
          }
        
        }
      }
      break;

      case UIAct.UI_SM_STATES.EDIT_MODE_SHAPE_EDIT: 
      menu_height = "HXA";         
      MenuSet=[
        <BASE_COM.Button
          key="<"
          addClass="layout black vbox width4"
          text="<" onClick={()=>this.props.ACT_Fail()}/>,
          
        <div key="EDIT_Text" className="s width8 lblue vbox">EDIT</div>,
        <div key="HLINE" className="s HX0_1"></div>
      ]
      
      if(this.props.edit_tar_info!=null)
      {
        MenuSet.push(this.genTarEditUI(this.props.edit_tar_info));

        let on_DEL_Tar=(id)=>
        {
          this.ec_canvas.SetShape( null, id);
        }
        if(this.props.edit_tar_info.id !== undefined)
        {
          MenuSet.push(<BASE_COM.Button
            key="DEL_BTN"
            addClass="layout red vbox"
            text="DEL" onClick={()=>on_DEL_Tar(this.props.edit_tar_info.id)}/>);
        }
        
      }
      else
      {
        console.log(this.props.shape_list);
        this.props.shape_list.forEach((shape)=>
        {
          MenuSet.push(<BASE_COM.Button
            key={"shape_listing_"+shape.id}
            addClass="layout lgreen vbox"
            text={shape.name} onClick={()=>this.props.ACT_EDIT_TAR_UPDATE(shape)}/>);
        });
      }
      break;
    }
 

    console.log("APP_EDIT_MODE render");
    return(
    <div className="HXF">
      <CanvasComponent_rdx addClass="layout width12" onCanvasInit={(canvas)=>{this.ec_canvas=canvas}}/>
        <$CSSTG transitionName = "fadeIn">
          <div key={substate} className={"s overlay scroll MenuAnim " + menu_height}>
            {MenuSet}
          </div>
        </$CSSTG>
      
    </div>
    );
  }
}


const mapDispatchToProps_APP_EDIT_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_Line_Add_Mode: (arg) =>  {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Line_Create))},
    ACT_Arc_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Arc_Create))},
    ACT_Search_Point_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Search_Point_Create))},
    ACT_Aux_Point_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Aux_Point_Create))},
    ACT_Shape_Edit_Mode:(arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Shape_Edit))},
    ACT_Measure_Add_Mode:(arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Measure_Create))},
   
    ACT_EDIT_TAR_ELE_TRACE_UPDATE: (keyTrace) => {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Ele_Trace_Update(keyTrace))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(UIAct.EV_UI_EDIT_MODE_Edit_Tar_Update(targetObj))},
   
    ACT_Save_Edit_Info: (arg) => {dispatch(UIAct.EV_UI_EDIT_MODE_Save_Edit_Info())},
   
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EDIT_MODE_FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))}
  }
}

const mapStateToProps_APP_EDIT_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    edit_tar_info:state.UIData.edit_info.edit_tar_info,
    shape_list:state.UIData.edit_info.list,
  }
};
const APP_EDIT_MODE_rdx = connect(
    mapStateToProps_APP_EDIT_MODE,
    mapDispatchToProps_APP_EDIT_MODE)(APP_EDIT_MODE);


class APPMain extends React.Component{


  constructor(props) {
      super(props);
  }


  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  render() {
    let UI=[];

    console.log("APPMain render",this.props);
    if(this.props.c_state==null)return null;
    let stateObj = xstate_GetCurrentMainState(this.props.c_state);
    if(stateObj.state === UIAct.UI_SM_STATES.MAIN)
    {
      UI = 
        <BASE_COM.Button
          addClass="lgreen width4"
          text="EDIT MODE" onClick={this.props.EV_UI_Edit_Mode}/>;
    }
    else if(stateObj.state === UIAct.UI_SM_STATES.EDIT_MODE)
    {
      UI = <APP_EDIT_MODE_rdx/>;
    }

    return(
    <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
      {UI}
    </BASE_COM.CardFrameWarp>

    );
  }
}
const mapDispatchToProps_APPMain = (dispatch, ownProps) => {
  return {
    EV_UI_Edit_Mode: (arg) => {dispatch(UIAct.EV_UI_Edit_Mode())}
  }
}
const mapStateToProps_APPMain = (state) => {
  return { 
    c_state: state.UIData.c_state
  }
}
const APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);

class APPMasterX extends React.Component{

  constructor(props) {
    super(props);
    //this.state={};
    //this.state.do_splash=true;

  }
  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {
    console.log("APPMasterX render",this.props);

    let xstateG;
    if (this.props.stateMachine!=null) {
      xstateG = 
      <XSGraph addClass="width12 height12" 
      state_machine={this.props.stateMachine.config}/>;
    } else {
      xstateG = null;
    }
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      <APPMain_rdx key="APP"/>

      <BASE_COM.CardFrameWarp addClass={"width7 height10 overlay SMGraph "+((this.props.showSM_graph)?"":"hide")} fixedFrame={true}>
        <div className="layout width11 height12">
          {xstateG}  
        </div>
        <div className="layout button width1 height12" onClick=
          {()=>this.props.ACT_Ctrl_SM_Panel( !this.props.showSM_graph)}></div>
      </BASE_COM.CardFrameWarp>

      {
        (this.props.showSplash)?
        <div key="LOGO" className="s HXF WXF overlay veleXY logoFrame white">
          <div className="veleXY width6 height6">
            <img className="height8 LOGOImg " src="resource/image/NotiMon.svg"></img>
            <div className="HX0_5"/>
            <div className="s">
              <div className="TitleTextCon showOverFlow HX2">
                <h1 className="Title">GO  !!</h1>
                <h1 className="Title">NOTIMON</h1>
              </div>
            </div>
          </div>
        </div>
        :[]
      }
    </$CSSTG>
    );
  }
}

const mapDispatchToProps_APPMasterX = (dispatch, ownProps) => {
  return {
    ACT_Ctrl_SM_Panel: (args) => dispatch({type:UIAct.UI_SM_EVENT.Control_SM_Panel,data:args}),
  }
}
const mapStateToProps_APPMasterX = (state) => {
  return {
    showSplash: state.UIData.showSplash,
    showSM_graph: state.UIData.showSM_graph,
    stateMachine:state.UIData.sm
  }
}
const APPMasterX_rdx = connect(mapStateToProps_APPMasterX,mapDispatchToProps_APPMasterX)(APPMasterX);

function BPG_WS (url){
  
  this.binary_ws = new BPG_WEBSOCKET.BPG_WebSocket(url);//"ws://localhost:4090");

  let onMessage=(evt)=>
  {
    console.log("onMessage:::");
    console.log(evt);
    if (evt.data instanceof ArrayBuffer) {
      let header = BPG_Protocol.raw2header(evt);
      console.log("onMessage:["+header.type+"]");
      if(header.type === "HR")
      {
        this.binary_ws.send(BPG_Protocol.objbarr2raw("HR",0,{a:["d"]}));

        // websocket.send(BPG_Protocol.obj2raw("TG",{}));
        //setTimeout(()=>{this.state.ws.send(BPG_Protocol.obj2raw("TG",{}));},0);
        
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
        let img = new ImageData(pkg.image, pkg.width);
        StoreX.dispatch(UIAct.EV_WS_Image_Update(img));
      }
      else if(header.type === "IR")
      {
        let IR =BPG_Protocol.raw2obj(evt);
        console.log("IR",IR);
        StoreX.dispatch(UIAct.EV_WS_Inspection_Report(IR.data));

      }
    }
  }
  let ws_onopen=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Connected(evt));
    StoreX.dispatch(UIAct.EV_WS_ChannelUpdate(bpg_ws));
  }

  let ws_onclose=(evt)=>
  {
    StoreX.dispatch(UIAct.EV_WS_Disconnected(evt));
  }
  this.send=(tl,prop=0,data={},uintArr=null)=>
  {
    if(data instanceof Uint8Array)
    {
      this.binary_ws.send(BPG_Protocol.objbarr2raw(tl,prop,null,data));
    }
    else
    {
      this.binary_ws.send(BPG_Protocol.objbarr2raw(tl,prop,data,uintArr));
    }
  }
  this.binary_ws.onmessage_bk = onMessage.bind(this);
  this.binary_ws.onopen_bk = ws_onopen.bind(this);
  this.binary_ws.onclose_bk = ws_onclose.bind(this);
}

let bpg_ws = new BPG_WS("ws://localhost:4090");

ReactDOM.render(
  
  <Provider store={StoreX}>
    <APPMasterX_rdx/>
  </Provider>,document.getElementById('container'));
