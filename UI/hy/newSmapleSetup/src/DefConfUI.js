'use strict'
   

import { connect } from 'react-redux'
import React from 'react';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import ReactResizeDetector from 'react-resize-detector';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';  
import {ReduxStoreSetUp} from 'REDUX_STORE_SRC/redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { DatePicker,Icon } from 'antd';
import 'antd/dist/antd.css';
let StoreX= ReduxStoreSetUp({});


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }

  ec_canvas_EmitEvent(event){
    switch(event.type)
    { 
      case DefConfAct.EVENT.SUCCESS:
        this.props.ACT_SUCCESS();
      break;
      case DefConfAct.EVENT.FAIL:
        this.props.ACT_Fail();
      break; 
      case DefConfAct.EVENT.Edit_Tar_Update:
        //console.log(event);
        this.props.ACT_EDIT_TAR_UPDATE(event.data);
      break; 
      case DefConfAct.EVENT.Edit_Tar_Ele_Cand_Update:
        //console.log(event);
        this.props.ACT_EDIT_TAR_ELE_CAND_UPDATE(event.data);
      break; 
      case DefConfAct.EVENT.Shape_List_Update:
        console.log(event);
        this.props.ACT_EDIT_SHAPELIST_UPDATE(event.data);
      break; 
      case DefConfAct.EVENT.Shape_Set:
        console.log(event);
        this.props.ACT_EDIT_SHAPE_SET(event.data);
      break; 
    }
  }
  componentDidMount() {
    this.ec_canvas=new EC_CANVAS_Ctrl.DEFCONF_CanvasComponent(this.refs.canvas);
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
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(DefConfAct.Edit_Tar_Update(targetObj))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_SHAPELIST_UPDATE: (shapeList) => {dispatch(DefConfAct.Shape_List_Update(shapeList))},
    ACT_EDIT_SHAPE_SET: (shape_data) => {dispatch(DefConfAct.Shape_Set(shape_data))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);


class APP_DEFCONF_MODE extends React.Component{


  componentDidMount()
  {
    this.props.ACT_WS_SEND(this.props.WS_ID,"EX",0,{imgsrc:"data/test1.bmp"});
    
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
        return (<BASE_COM.JsonEditBlock object={edit_tar} 
          key="BASE_COM.JsonEditBlock"
          whiteListKey={{
            //id:"div",
            type:"div",
            subtype:"div",
            name:"input",
            //pt1:null,
            angleDeg:"input-number",
            value:"input-number",
            margin:"input-number",
            quadrant:"div",
            docheck:"checkbox",
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
                
                switch(type)
                {
                  case "input-number":
                  {
                    let parseNum =  parseFloat(evt.target.value);
                    if(isNaN(parseNum))return;
                    target.obj[lastKey]=parseNum;
                  }
                  break;
                  case "input":
                  {
                    target.obj[lastKey]=evt.target.value;
                  }
                  break;
                  case "checkbox":
                  {
                    target.obj[lastKey]=evt.target.checked;
                  }
                  break;
                }
                this.ec_canvas.SetShape( original_obj, original_obj.id);
              }
            }}/>);
      }
      break;
      default:
      {
        return (<BASE_COM.JsonEditBlock object={edit_tar} 
          key="BASE_COM.JsonEditBlock"
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
    let substate = this.props.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];
    switch(substate)
    {
      case UIAct.UI_SM_STATES.DEFCONF_MODE_NEUTRAL:
      MenuSet=[
          <BASE_COM.Button
          key="<"
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          <BASE_COM.Button
            addClass="layout palatte-blue-8 vbox"
            key="LINE"
            text="線/LINE" onClick={()=>this.props.ACT_Line_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout palatte-blue-8 vbox"
            key="ARC"
            text="弧/ARC" onClick={()=>this.props.ACT_Arc_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout palatte-blue-6 vbox"
            key="APOINT"
            text="交點/APOINT" onClick={()=>this.props.ACT_Aux_Point_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout palatte-blue-6 vbox"
            key="SPOINT"
            text="搜尋點/SPOINT" onClick={()=>this.props.ACT_Search_Point_Add_Mode()}/>,
        <BASE_COM.IconButton
            iconType="scan"
            addClass="layout palatte-gold-6 "
            key="MEASURE"
            text="測量定義/MEASURE" onClick={()=>this.props.ACT_Measure_Add_Mode()}/>,
          <BASE_COM.Button
            addClass="layout palatte-gold-6 vbox"
            key="EDIT"
            text="編輯測量定義/Edit" onClick={()=>this.props.ACT_Shape_Edit_Mode()}/>,
          <BASE_COM.Button
            addClass="layout palatte-gold-7 vbox"
            key="SAVE"
            text="儲存定義/SAVE" onClick={()=>{
                var enc = new TextEncoder();
                let report = this.props.edit_info._obj.GenerateEditReport();
                this.props.ACT_Report_Save(this.props.WS_ID,"data/test.ic.json",enc.encode(JSON.stringify(report, null, 2)));
            }}/>,
          /*<BASE_COM.Button
            addClass="layout lred vbox"
            key="TRIGGER"
            text="TRIGGER" onClick={()=>{this.props.ACT_Trigger_Inspection({deffile:"data/test.ic.json",imgsrc:"data/test1.bmp"})}}/>,*/
            <BASE_COM.Button
            addClass="layout palatte-purple-8 vbox"
            key="LOAD"
            text="讀取定義/LOAD" onClick={()=>{
                
                this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{deffile:"data/test.ic.json",imgsrc:"data/test1.bmp"});
                
            }}/>,
            <BASE_COM.Button
              addClass="layout palatte-purple-8 vbox"
              key="TAKE"
              text="重新設定/TAKE" onClick={()=>{
                  this.props.ACT_WS_SEND(this.props.WS_ID,"EX",0,{});
                  this.props.ACT_Shape_List_Reset();
              }}/>,
        ];
      break;
      case UIAct.UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="MEASURE" className="s width8 lblue vbox">MEASURE</div>,
      ];

      
      if(this.props.edit_tar_info!=null)
      {
        console.log("BASE_COM.JsonEditBlock:",this.props.edit_tar_info);

        
        MenuSet.push(<BASE_COM.JsonEditBlock object={this.props.edit_tar_info} dict={ {line:"LINE線"} } 
          key="BASE_COM.JsonEditBlock"
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
      
      case UIAct.UI_SM_STATES.DEFCONF_MODE_LINE_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="LINE" className="s width8 lred vbox">LINE</div>,
      ];
      
      break;
      case UIAct.UI_SM_STATES.DEFCONF_MODE_ARC_CREATE:          
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox width4"
          key="<"
          text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="ARC" className="s width8 lred vbox">ARC</div>
      ];
      break;

      case UIAct.UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:         
      MenuSet=[
        <BASE_COM.Button
          addClass="layout black vbox"
          key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
        <div key="SEARCH_POINT" className="s lred vbox">SPOINT</div>,
      ];
      if(this.props.edit_tar_info!=null)
      {
        console.log("BASE_COM.JsonEditBlock:",this.props.edit_tar_info);

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

      case UIAct.UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:  
      {       
        MenuSet=[
          <BASE_COM.Button
            addClass="layout black vbox"
            key="<" text="<" onClick={()=>this.props.ACT_Fail()}/>,
          <div key="AUX_POINT" className="s lred vbox">APOINT</div>,
        ];

      
        if(this.props.edit_tar_info!=null)
        {
          console.log("BASE_COM.JsonEditBlock:",this.props.edit_tar_info);

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

      case UIAct.UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT: 
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
 

    console.log("APP_DEFCONF_MODE render");
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


const mapDispatchToProps_APP_DEFCONF_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_Line_Add_Mode: (arg) =>  {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Line_Create))},
    ACT_Arc_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Arc_Create))},
    ACT_Search_Point_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Search_Point_Create))},
    ACT_Aux_Point_Add_Mode: (arg) =>   {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Aux_Point_Create))},
    ACT_Shape_Edit_Mode:(arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Shape_Edit))},
    ACT_Measure_Add_Mode:(arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Measure_Create))},
   
    ACT_EDIT_TAR_ELE_TRACE_UPDATE: (keyTrace) => {dispatch(DefConfAct.Edit_Tar_Ele_Trace_Update(keyTrace))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(DefConfAct.Edit_Tar_Update(targetObj))},
    ACT_Shape_List_Reset:() => {dispatch(DefConfAct.Shape_List_Update([]))},
   
    ACT_Save_Def_Config: (info) => {dispatch(UIAct.EV_UI_EC_Save_Def_Config(info))},
    ACT_Trigger_Inspection: (info) => {dispatch(UIAct.EV_UI_EC_Trigger_Inspection(info))},
    ACT_Load_Def_Config: (info) => {dispatch(UIAct.EV_UI_EC_Load_Def_Config(info))},
   
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
    
    ACT_Report_Save:(id,fileName,content)=>{
        dispatch(UIAct.EV_WS_SEND(id,"SV",0,
            {filename:fileName},
            content
        ));
    },
  }
}

const mapStateToProps_APP_DEFCONF_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    edit_tar_info:state.UIData.edit_info.edit_tar_info,
    shape_list:state.UIData.edit_info.list,
    WS_ID:state.UIData.WS_ID,
    edit_info:state.UIData.edit_info

  }
};

const APP_DEFCONF_MODE_rdx = connect(
    mapStateToProps_APP_DEFCONF_MODE,
    mapDispatchToProps_APP_DEFCONF_MODE)(APP_DEFCONF_MODE);

export default APP_DEFCONF_MODE_rdx;