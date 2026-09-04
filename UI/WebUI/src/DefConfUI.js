'use strict'


import { connect } from 'react-redux'
import React, { useState, useEffect, useRef, useMemo } from 'react';
import { createPortal } from 'react-dom';
import * as BASE_COM from './component/baseComponent.jsx';
import ComponentBoundary from './component/ComponentBoundary';
import { TagOptions_rdx, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
import { Shape_Attr_Fill, InspectionEditorLogic } from 'UTIL/InspectionEditorLogic';
import { fieldFor, getShapeModule } from 'JSSRCROOT/shapes';
import { applyFieldChange } from 'JSSRCROOT/shapes/_schemaHelpers';
import { loadDefWithImageFallback } from 'UTIL/DefLoadWithImageFallback';
import { buildSchema } from 'JSSRCROOT/shapes/propertySheet';
let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
import DragSortableList from 'react-drag-sortable'
import ReactResizeDetector from 'react-resize-detector';
import { DEF_EXTENSION, defFileFilter, makeExtensionFilter, BPG_ExpCalc, CameraTransferCtrl as CameraCtrl } from 'UTIL/BPG_Protocol';
import { unsupportedCoreOps } from 'UTIL/expr';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
// v2 runs BESIDE v1, not instead of it. Both buttons stay until the new one
// has been used on a machine for a while; a studio is where a def gets its
// locator, and losing the ability to fall back would mean a bad build blocks
// recipe authoring outright. See the header of SBMStudio2.jsx.
import { SBMSetupView2 } from './SBMStudio2';
import { useDefImages } from 'UTIL/useDefImages';
import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {
  round as roundX, websocket_autoReconnect,
  websocket_reqTrack, dictLookUp,
  GetObjElement, Exp2PostfixExp, PostfixExpCalc,
  defFileGeneration, stampRefImagePath
} from 'UTIL/MISC_Util';

import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.defconf');
import dclone from 'clone';
import Modal from "antd/lib/modal";
import Menu from "antd/lib/menu";
import Button from "antd/lib/button";
import Icon from 'antd/lib/icon';
import Tag from 'antd/lib/tag';
import message from 'antd/lib/message';
import Table  from 'antd/lib/table';
import Checkbox from "antd/lib/checkbox";
import InputNumber from 'antd/lib/input-number';
import Input from 'antd/lib/input';
import Switch from 'antd/lib/switch';
const { CheckableTag } = Tag;
const { TextArea } = Input;
import Divider from 'antd/lib/divider';
import Dropdown from 'antd/lib/dropdown'
import Slider from 'antd/lib/slider';
import Popover from 'antd/lib/popover';


import { useSelector,useDispatch } from 'react-redux';
import { applyInspFrameRate } from 'UTIL/inspRatePolicy.mjs';
import { nextFreeName, takenNamesFrom } from 'UTIL/defNaming.mjs';
import { mmppFromLensCalib } from 'UTIL/mmppRule.mjs';
import { 
  VerticalAlignTopOutlined,
  ThunderboltOutlined,
  StarOutlined,
  LockOutlined,
  CloseOutlined,
  PlusOutlined,
  FormOutlined,
  EditOutlined,
  SaveOutlined,
  ExportOutlined,
  SettingOutlined,
  CameraOutlined,
  ArrowLeftOutlined,
  CaretDownOutlined,
  AimOutlined,


} from '@ant-design/icons';
import {RepDisplay} from './RepDisplayUI.js';



const IMG_LOAD_DOWNSAMP_LEVEL=1;

function toFixedNum(num,digit)
{
  if((typeof num === 'string' || num instanceof String))
  {
    num=parseFloat(num);
  }
  return (parseFloat(num.toFixed(digit)));
}

// Shared compact-layout primitives used by the *Setup renderLib widgets
// (ULRangeSetup, AngleRangeSetup, SimpleSetup). One row = 24px tall, 96px
// label column on the left, number input + small step buttons on the right.
const _COMPACT_ROW = {
  display: 'flex', alignItems: 'center', minHeight: 24, gap: 4,
  fontSize: 12, padding: '1px 0', width: '100%',
};
const _COMPACT_LABEL = {
  flex: '0 0 88px', color: '#333', overflow: 'hidden',
  textOverflow: 'ellipsis', whiteSpace: 'nowrap',
};
const _STEP_BTN = {
  flex: '0 0 auto', height: 20, padding: '0 4px', fontSize: 11,
  border: '1px solid #ccc', borderRadius: 3, background: '#f5f5f5',
  color: '#333', cursor: 'pointer', lineHeight: '18px', minWidth: 22,
};
function CompactRow({ label, children }) {
  return <div style={_COMPACT_ROW}>
    <div style={_COMPACT_LABEL} title={label}>{label}</div>
    <div style={{ flex: 1, minWidth: 0, display: 'flex', alignItems: 'center', gap: 3 }}>
      {children}
    </div>
  </div>;
}
function StepButton({ onClick, title, children }) {
  return <button type="button" style={_STEP_BTN} onClick={onClick} title={title}>{children}</button>;
}
function labelForKey(props, key) {
  const a = GetObjElement(props.dict, [props.dictTheme, key]);
  if (a !== undefined) return a;
  const b = GetObjElement(props.dict, ['_', key]);
  return (b !== undefined) ? b : key;
}

// Compact numeric input used by ULRange/Angle/Simple Setup widgets. Pre-
// rewrite this wrapped a NumPad popup for touch entry; now it's a plain
// HTML number input — 4-decimal round on display, commit on blur/Enter
// (Escape reverts). Mid-typing keystrokes are held in local state so
// external re-renders from the same edit don't clobber the user's input.
function NumberAccInput({ value, className, onChange, style }) {
  const [local, setLocal] = useState(() => '' + toFixedNum(value, 4));
  const editing = useRef(false);
  useEffect(() => { if (!editing.current) setLocal('' + toFixedNum(value, 4)); }, [value]);
  const commit = () => {
    editing.current = false;
    const parsed = parseFloat(local);
    if (Number.isFinite(parsed)) {
      const rounded = toFixedNum(parsed, 4);
      onChange({ target: { value: '' + rounded } });
      setLocal('' + rounded);
    } else {
      setLocal('' + toFixedNum(value, 4));
    }
  };
  return (
    <input
      className={className}
      style={{
        height: 22, fontSize: 12, padding: '0 4px', boxSizing: 'border-box',
        color: '#222', background: 'white', border: '1px solid #ccc',
        borderRadius: 3, width: '100%', ...style,
      }}
      // inputMode brings up the OS numeric on-screen keyboard on a touch
      // screen while changing nothing for a physical keyboard -- this is the
      // whole touch story now that the numpad popup is gone, and unlike the
      // popup it never takes the field away from the keyboard.
      type="number" step="0.0001" inputMode="decimal"
      pattern="^[-+]?[0-9]?(\.[0-9]*){0,1}$"
      value={local}
      onChange={(e) => { editing.current = true; setLocal(e.target.value); }}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        else if (e.key === 'Escape') {
          editing.current = false;
          setLocal('' + toFixedNum(value, 4));
          e.currentTarget.blur();
        }
      }}
    />
  );
}


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);

  }

  ec_canvas_EmitEvent(event) {
    switch (event.type) {
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
        this.props.ACT_EDIT_SHAPELIST_UPDATE(event.data);
        break;
      case DefConfAct.EVENT.Shape_Set:
        this.props.ACT_EDIT_SHAPE_SET(event.data);
        break;
      case DefConfAct.EVENT.Edit_Tar_Ele_Trace_Update:
        this.props.ACT_EDIT_TAR_ELE_TRACE_UPDATE(event.data);
        break;

    }
  }
  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.DEFCONF_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);
    this.props.onCanvasInit(this.ec_canvas);
    this.updateCanvas(this.props.c_state);
  }
  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }
  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas !== undefined) {
      {
        this.ec_canvas.EditDBInfoSync(props.edit_info);
        this.ec_canvas.SetState(ec_state);
        // Mirrored before the draw, exactly as the inspection view does it:
        // per-shape drawInspection reads renderer.show_caliper_hits.
        this.ec_canvas.rUtil.show_caliper_hits = props.showCaliperHits !== false;
        //this.ec_canvas.ctrlLogic();
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width, height) {
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.updateCanvas(this.props.c_state);
      this.ec_canvas.ctrlLogic();
      this.ec_canvas.draw();
    }
  }
  componentWillUpdate(nextProps, nextState) {

    log.debug("[render] CanvasComponent", nextProps.c_state);
    //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];

    //console.log(nextProps.edit_info.inherentShapeList);
    this.updateCanvas(nextProps.c_state, nextProps);
  }

  render() {

    return (
      <div className={this.props.addClass + " HXF"}>
        <canvas ref="canvas" className="width12 HXF" />
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
    // The SAME setting the inspection view reads. renderUTIL defaults
    // show_caliper_hits to true and only InspectionUI ever mirrored it, so the
    // switch turned the overlay off on one screen and left it on here -- with
    // no control on this screen at all.
    showCaliperHits: (state.UIData.System_Setting||{}).SHOW_CALIPER_HITS_INSP,
  }
}



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => {
  return {
    ACT_SUCCESS: (arg) => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS)) },
    ACT_Fail: (arg) => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL)) },
    ACT_EXIT: (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT)) },
    ACT_EDIT_TAR_UPDATE: (targetObj) => { dispatch(DefConfAct.Edit_Tar_Update(targetObj)) },
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) => { dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj)) },
    ACT_EDIT_SHAPELIST_UPDATE: (shapeList) => { dispatch(DefConfAct.Shape_List_Update(shapeList)) },
    ACT_EDIT_SHAPE_SET: (shape_data) => { dispatch(DefConfAct.Shape_Set(shape_data)) },
    ACT_EDIT_TAR_ELE_TRACE_UPDATE: (keyTrace) => { dispatch(DefConfAct.Edit_Tar_Ele_Trace_Update(keyTrace)) },
  }
}
const CanvasComponent_rdx = connect(
  mapStateToProps_CanvasComponent,
  mapDispatchToProps_CanvasComponent)(CanvasComponent);




function ULRangeAcc({ value, lastKey, onChange, RangeCValue, target, props }) {
  const [offsetEditVisible, setOffsetEditVisible] = useState(false);
  function numberSet(num) {
    log.debug("[range]", { num, RangeCValue, sum: RangeCValue + num });
    onChange(target, "input-number", { target: { value: (RangeCValue + num).toFixed(4) } })
  }
  function numberPlus(num) {
    onChange(target, "input-number", { target: { value: num.toFixed(4) } })
  }
  let translateKey = GetObjElement(props.dict, [props.dictTheme, lastKey]);
  if (translateKey === undefined) translateKey = lastKey
  const content =
    <Menu onClick={(ev) => {
    }}>
      <Menu.Item key={4} style={{height:"55px"}}>
        <div className="s height12" style={{ width: "400px" }}>
          <div className="s  height12 width2 vbox black" style={{ color: "white" }} href="#">
            {RangeCValue}+
        </div>
          <NumberAccInput key={"_" + lastKey + "_stxt"} className="s  height12 width5 vbox blackText"
            value={(value - RangeCValue).toFixed(4)}
            onChange={(evt) => {
              
              numberSet(parseFloat(evt.target.value))

            }}
          />

          <div key="acc" className="s width3  height12">
            <Button key="plus100u" className="s height6 width6  black" onClick={_ =>
              numberPlus(value + 0.1)
            }>+.1</Button>
            <Button key="plus10u" className="s  height6 width6  black" onClick={_ =>
              numberPlus(value + 0.01)
            }>+.01</Button>
            <Button key="minus100u" className="s  height6 width6  black" onClick={_ =>
              numberPlus(value - 0.1)
            }>-.1</Button>
            <Button key="minus10u" className="s  height6 width6  black" onClick={_ =>
              numberPlus(value - 0.01)
            }>-.01</Button>
          </div>

          <Button key="zero" className="s  height12 width2  black" onClick={_ =>
            numberPlus(RangeCValue)
          }>R</Button>
        </div>
      </Menu.Item>
    </Menu>
  let dropDownX =
    <Popover content={content} title={null} trigger="click"
      visible={offsetEditVisible}

      onVisibleChange={vis => {
        setOffsetEditVisible(vis)
      }}>

      <a className="s HX1 width4 vbox black" style={{ color: "white" }} href="#">
        {translateKey}
        <Icon type="caret-down" />
      </a>
    </Popover>

  return dropDownX;
}

function AngleDegAcc({ value, onChange,target,lastKey, props }) {
  const [offsetEditVisible, setOffsetEditVisible] = useState(false);
  function numberSet(num) {
    num%=360;
    onChange(target, "input-number", { target: { value: ( num).toFixed(4) } })
  }
  let translateKey = GetObjElement(props.dict, [props.dictTheme, lastKey]);
  //log.info(props.dict,props.dictTheme,lastKey,translateKey);

  if (translateKey === undefined) 
    translateKey = GetObjElement(props.dict, ["_", lastKey]);
  
  if (translateKey === undefined) 
    translateKey = lastKey

  const content =
    <Menu onClick={(ev) => {
    }}>
      <Menu.Item key={4}>
        <div className="s height12" style={{ width: "300px", height:"auto" }}>

          <Button key="plus30u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value + 30)
          }>+30</Button>
          <Button key="minus30u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value - 30)
          }>-30</Button>
          <Button key="plus5u" className="s  height12 width2 vbox black" onClick={_ =>
            numberSet(value + 5)
          }>+5</Button>
          <Button key="minus5u" className="s  height12 width2 vbox black" onClick={_ =>
            numberSet(value - 5)
          }>-5</Button>
          <Button key="zero" className="s  height12 width2 vbox black" onClick={_ =>
            numberSet(0)
          }>0</Button>
        </div>
      </Menu.Item>
    </Menu>
  let dropDownX =
    <Popover content={content} title={null} trigger="click"
      visible={offsetEditVisible}
      onVisibleChange={vis => {
        setOffsetEditVisible(vis)
      }}>

      <a className="s HX1 width4 vbox black" style={{ color: "white" }} href="#">
        {translateKey}
        <Icon type="caret-down" />
      </a>
    </Popover>

  return dropDownX;
}


function SimpleAcc({ value, onChange,target,lastKey, props }) {
  const [offsetEditVisible, setOffsetEditVisible] = useState(false);
  function numberSet(num) {
    onChange(target, "input-number", { target: { value: ( num).toFixed(4) } })
  }
  let translateKey = GetObjElement(props.dict, [props.dictTheme, lastKey]);
  //log.info(props.dict,props.dictTheme,lastKey,translateKey);

  if (translateKey === undefined) 
    translateKey = GetObjElement(props.dict, ["_", lastKey]);
  
  if (translateKey === undefined) 
    translateKey = lastKey

  const content =
    <Menu onClick={(ev) => {
    }}>
      <Menu.Item key={4}>
        <div className="s height12" style={{ width: "300px", height:"auto" }}>

          <Button key="plus30u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value*2)
          }>x2</Button>
          <Button key="minus30u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value/2)
          }>/2</Button>
          <Button key="plus5u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value + 1)
          }>+1</Button>
          <Button key="minus5u" className="s  height12 width3 vbox black" onClick={_ =>
            numberSet(value - 1)
          }>-1</Button>
        </div>
      </Menu.Item>
    </Menu>
  let dropDownX =
    <Popover content={content} title={null} trigger="click"
      visible={offsetEditVisible}
      onVisibleChange={vis => {
        setOffsetEditVisible(vis)
      }}>

      <a className="s HX1 width4 vbox black" style={{ color: "white" }} href="#">
        {translateKey}
        <Icon type="caret-down" />
      </a>
    </Popover>

  return dropDownX;
}




// Two questions, not one: does this expression MEAN anything, and can the
// machine actually run it?
//
// This used to ask only the first, using the UI's own evaluator -- which
// implements a SUPERSET of what judge_CALC does in the core ($^$ being the one
// that exists here and not there). So `a^2` computed a number on screen, saved
// cleanly, and then made the core return -2 for every part. That path leaves
// the judge at STATUS_UNSET and never clears it: the measurement reads NA for
// the rest of the recipe's life, with nothing anywhere naming the operator.
//
// Returns { ok, unsupported } so the caller can say WHICH operator is the
// problem. "Invalid expression" on a formula that evaluates fine in front of
// you is not a message anyone can act on.
function parseCheckExpressionValid(postExp, idArr) {

  const unsupported = unsupportedCoreOps(postExp);
  if (unsupported.length) return { ok: false, unsupported };

  let funcSet = {}

  //the magic 333333+0.00001*idx is to prevent easy calc collision that causes NaN result
  idArr.forEach((id,idx) => { funcSet[id] = 3337333+0.00001*idx });

  let res = BPG_ExpCalc(postExp,funcSet);

  
  // console.log(res);
  res=res.flat();
  // console.log(postExp,res);
  return { ok: (res.length==1)&&res[0]==res[0], unsupported: [] };
}


function completeSingleCtrlMarginInfo(singleMarginInfo,measureInfo)
{
  measureInfo.forEach(shape=>{
    // console.log(singleMarginInfo,measureInfo);
    let singleMeasureMarginInfoIdx=singleMarginInfo.findIndex(mmMeasure=>mmMeasure.id==shape.id);



    let mx={
      id:shape.id,
    }
    if(singleMeasureMarginInfoIdx===-1)
    {
      singleMarginInfo.push({...mx});
    }
    else
    {
      singleMarginInfo[singleMeasureMarginInfoIdx]={...mx,...singleMarginInfo[singleMeasureMarginInfoIdx]};
    }
    // console.log(singleMarginInfo);

  })
}


function DisplayMarginSet({MarginInfo,DICT})
{
  const columns = [
    {
      key:"type",
      title:dictLookUp("type", DICT)
    },{
      key:"value",
      edible:"input",
      title:DICT.measure.value
    },{
      key:"USL",
      edible:"input",
      title:DICT.measure.USL
    },{
      key:"LSL",
      edible:"input",
      title:DICT.measure.LSL
    },{
      key:"UCL",
      edible:"input",
      title:DICT.measure.UCL
    },{
      key:"LCL",
      edible:"input",
      title:DICT.measure.LCL
    }]
    .map((t)=>{

      let render=undefined;
      switch(t.edible)
      {
        case "input":
          render=(dara,A,idx,B) => {
            return(
              <NumberAccInput style={{ width: '100%' }} value={dara} onChange={(nv)=>{
                // console.log("",nv,dara,A,idx,B);
              }} />)
          }
          break;
        default:
          render=(dara,A,idx,B) => (DICT._[dara]===undefined)?dara:DICT._[dara]
          break;
      }

      let title=t.title!==undefined?t.title:
          ((DICT._[t.key]===undefined)?t.key:DICT._[t.key]);
      return {
        title,
        dataIndex: t.key,
        key: t.key,
        width: t.width,
        render
      }
    });

  return  <Table columns={columns} 
  dataSource={MarginInfo} size="small" pagination={false}/>

}

function InspMarginEditor({measureInfo, control_margin_info ,DICT,onExtraCtrlUpdate, onExitDump}) {
  let _ = useRef({});
  const [inputText, setInputText] = useState("");

  const [_control_margin_info, set_control_margin_info] = useState({});
  const [_measureInfo, set_MeasureInfo] = useState([1]);
  const [displayInfoSet, setDisplayInfoSet] = useState([]);


  _.current.DUMP={measureInfo:_measureInfo,control_margin_info:_control_margin_info,displayInfoSet:displayInfoSet};


  function cleanUpDumpInfo()
  {
    let dump = {..._.current.DUMP};

    {
      //the control_margin_info in dump has complex info update and name property, so clean it up
      let ctrlMarg = {...dump.control_margin_info};
      Object.keys(ctrlMarg).forEach(key=>{
  
        // Copy each ELEMENT before deleting: [...arr] clones the array, not
        // the rows, so `delete m.update` was stripping the live table rows'
        // update closures (and the shared def's rows) every time the dump ran.
        ctrlMarg[key]=(ctrlMarg[key]||[]).map(m=>{
          let c = {...m};
          delete c.name;
          delete c.update;
          return c;
        })
      })
    }

    {
      dump.measureInfo =[...dump.measureInfo].map(m=>{
        let c = {...m};   // same element-copy rule as ctrlMarg above
        delete c.update;
        delete c.__root;  // view state, not def data -- see where it is set
        return c;
      })
    }


    return dump;
  }
  useEffect(() => {
    set_MeasureInfo(dclone(measureInfo));
    //in the following deffile editing, some new measure might appear/delete
    //adjust control_margin_info coording to it

    // dclone like measureInfo just above -- NOT the bare reference. The local
    // edit copy used to BE the redux-held object, so every limit edit mutated
    // the def in place: the dirty check compared an unchanged reference and
    // said "no changes" while the live grading path was already reading the
    // edited numbers. The editor now works on its own copy; the parent pulls
    // it explicitly via getMarginInfo / onExitDump.
    if(control_margin_info!==undefined)
      set_control_margin_info(dclone(control_margin_info));

    if(typeof onExtraCtrlUpdate === "function")
      onExtraCtrlUpdate({
        getMarginInfo:cleanUpDumpInfo
      });
    return () => {
      if(typeof onExitDump === "function")
        onExitDump(cleanUpDumpInfo())
    };
  }, [])



  const columns = [
    {
      key:"name",
      title:dictLookUp("name", DICT)
    },{
      key:"subtype",
      width: '80px',
      title:dictLookUp("subtype", DICT)
    },
    {
      key:"rank",
      edible:"input",
      title:DICT.measure.rank
    },{
      key:"quality_essential",
      width: '110px',
      edible:"tristate",
      title:DICT.measure.quality_essential
    },{
      key:"value",
      edible:"input",
      title:DICT.measure.value
    },{
      key:"USL",
      edible:"input",
      title:DICT.measure.USL
    },{
      key:"LSL",
      edible:"input",
      title:DICT.measure.LSL
    },{
      key:"UCL",
      edible:"input",
      title:DICT.measure.UCL
    },{
      key:"LCL",
      edible:"input",
      title:DICT.measure.LCL
    }]
    .map((col)=>{

      let render=undefined;

      switch(col.edible)
      {
        case "input":
          render=(value,objInfo,idx) => {

            let rootMInfo=_measureInfo.find(m=>m.id===objInfo.id);
            if(objInfo.name!==undefined && objInfo.subtype===undefined && value===undefined)
            {
              let rootValue=rootMInfo[col.key];
              // console.log(_measureInfo,rootMInfo,objInfo,col.key,rootValue);
              return <Button type="dashed" onClick={()=>{
                let new_obj={...objInfo};
                new_obj[col.key]=rootValue;
                objInfo.update(new_obj);
              }}>{rootValue}</Button>;
            }

            if(value===undefined)
            {
              return undefined;
            }

            // NumberAccInput, not the react-numpad popup it used to be.
            //
            // The popup numpad captured the field, so a physical keyboard --
            // which every bench here has -- fought it instead of typing into
            // it, and each edit cost an open/peck/confirm round trip.
            // NumberAccInput is a native input: keyboard types straight in,
            // commit on blur/Enter, Escape reverts, and inputMode="decimal"
            // asks a touch device for its numeric on-screen keyboard, so touch
            // entry still works without owning the field.
            return (
              <NumberAccInput style={{ width: '100%' }} value={value}
                onChange={(evt)=>{
                  let new_obj={...objInfo};
                  let parseNum=toFixedNum(evt.target.value,5);
                  if(parseNum!=parseNum)
                  {
                    if(objInfo.subtype===undefined)
                    {
                      new_obj[col.key]=undefined;
                    }
                  }
                  else
                  {
                    new_obj[col.key]=parseNum;
                  }
                  objInfo.update(new_obj);
                }} />
            );
          }
          break;


        // THREE STATES, BECAUSE AN OVERRIDE TABLE HAS THREE.
        //
        // A switch has two, and a two-state control cannot say "this 製程 does
        // not have an opinion" -- putting one here would silently write an
        // opinion into every row the moment the table rendered, which is the
        // dashed-button bug inside out. Empty means inherit, exactly as it does
        // for the numeric columns beside it, and the inherited value is printed
        // on the 未設定 button so the row still reads as an answer rather than
        // as a blank.
        case "tristate":
          render=(value,objInfo) => {
            const rootMInfo=_measureInfo.find(m=>m.id===objInfo.id);
            if(rootMInfo===undefined)return undefined;
            const rootOn = rootMInfo.quality_essential !== false;
            const set=(v)=>{
              let new_obj={...objInfo};
              if(v===undefined) delete new_obj[col.key];
              else new_obj[col.key]=v;
              objInfo.update(new_obj);
            };
            const btn=(label,v,active)=>(
              <Button size="small" type={active?"primary":"default"}
                style={{ padding:'0 6px', minWidth: 26 }}
                onClick={()=>set(v)}>{label}</Button>);
            // The root row has no row above it to inherit from, so offering
            // "inherit" there is offering a state that cannot mean anything --
            // and it was the state the row always fell back to, which is how
            // the dead control looked like a working one.
            if (objInfo.__root) {
              return (
                <Button.Group>
                  {btn("是", true,  value!==false)}
                  {btn("否", false, value===false)}
                </Button.Group>
              );
            }
            return (
              <Button.Group>
                {btn(rootOn?"—(是)":"—(否)", undefined, value===undefined)}
                {btn("是", true,  value===true)}
                {btn("否", false, value===false)}
              </Button.Group>
            );
          }
          break;

        default:
          render=(dara) => (DICT._[dara]===undefined)?dara:DICT._[dara]
          break;
      }

      let title=col.title!==undefined?col.title:
          ((DICT._[col.key]===undefined)?col.key:DICT._[col.key]);
      return {
        title,
        dataIndex: col.key,
        key: col.key,
        width: col.width,
        render
      }
    });

  let measureX=_measureInfo
    .map((shape,idx)=>{
      let cur_rank=(shape.rank===undefined)?0:shape.rank;
      let SelMarginInfo = Object.keys(displayInfoSet).filter(key=>_control_margin_info[key]!==undefined)
        .map(text=>{
          let info = _control_margin_info[text];

          let obj=info.find(m=>m.id==shape.id);
          if(obj===undefined)
          {
            obj={
              id:shape.id
            }
          }


          obj={...obj};
          if(obj.rank===undefined)
          {
            obj.rank=cur_rank;
          }
          obj.name=<><PlusOutlined/>{text}</>;
          // WHITELIST WHAT GETS PERSISTED. The row object carries display-only
          // members -- obj.name is a React element assigned a few lines up, and
          // obj.update is this very closure -- and newObj is a spread of the
          // whole row, so writing it back stored them in the def.
          //
          // The function vanished in JSON.stringify. The element did not, and
          // downstream the override is merged into the shape with a full spread
          // ({...shape, ...info}) on entering inspection, which replaced the
          // shape's name with an object in the wire def. The core could then
          // not match that feature at all: every part came back NA, the plate
          // ran at 20/s and sorted 0.0/s, and only under the ONE 製程 whose
          // rows had been touched in this editor. Stripping the element and
          // changing nothing else restored 19.6/s.
          //
          // A whitelist rather than `delete obj.name`, because the merge
          // downstream copies every key: anything display-shaped that is added
          // to a row later would land on the shape the same way.
          const PERSIST = ['id', 'rank', 'value', 'USL', 'LSL', 'UCL', 'LCL',
                           'quality_essential'];
          const cleanRow = (o) => PERSIST.reduce((r, k) => {
            if (o[k] !== undefined) r[k] = o[k];
            return r;
          }, {});
          obj.update=(newObj_raw)=>{
            const newObj = cleanRow(newObj_raw);
            let newMarginInfo = {..._control_margin_info};
            // Copy the row array too: the top-level spread above still shares
            // the per-tag arrays, so writing rows[tarIdx] in place edited
            // whatever else holds that array.
            let rows = [...(newMarginInfo[text]||[])];
            let tarIdx=rows.findIndex(m=>m.id==newObj.id);
            if(tarIdx!==-1)
            {
              rows[tarIdx]=newObj;
            }
            else
            {
              // APPEND, do not drop.
              //
              // A tag's array only carries the measures it actually overrides,
              // so a measure without one is shown from a synthetic row built a
              // few lines above ({id} only). That row is not in the array,
              // findIndex returned -1, and this wrote nothing -- while still
              // calling set_control_margin_info, so React re-rendered the
              // identical data and the dashed "take the root value" button
              // looked simply dead.
              //
              // completeSingleCtrlMarginInfo does pre-seed a row per measure,
              // but only when a tag is added from the menu. A tag loaded from
              // an existing def never goes through it -- which is every tag an
              // operator actually opens this editor to change.
              rows.push(newObj);
            }
            newMarginInfo[text]=rows;
            set_control_margin_info(newMarginInfo);
          };
          delete obj.subtype
          // delete obj.rank
          return obj;
        }).filter(_=>_!==undefined);
      
      let arr=[{
        id:shape.id,
        name:shape.name,
        subtype:shape.subtype,
        key:shape.id,
        value:shape.value,
        rank:cur_rank,
        USL:shape.USL,
        LSL:shape.LSL,
        UCL:shape.UCL,
        LCL:shape.LCL,
        // The root row was missing this while the 品質必需 column was rendered
        // on it anyway, and its update() wrote back six keys that did not
        // include it -- so clicking 是/否 here was accepted and discarded, and
        // the button then redrew as "inherit". quality_essential is the single
        // field deciding whether a measurement counts toward the part, so the
        // symptom was "I disabled that measurement and it still rejects parts".
        quality_essential:shape.quality_essential,
        // Marks the root for the tristate renderer: the root has nothing to
        // inherit FROM, so it gets two states, not three. Stripped in
        // cleanUpDumpInfo beside `update` -- a double-underscore key that
        // reached the def would ride the full-spread merge into the wire def,
        // which is the trap documented on the 製程 override rows.
        __root:true,
        
        update:(newObj)=>{
          let newMeasureInfo = [..._measureInfo];
          newMeasureInfo[idx]={...shape,
            value:newObj.value,
            rank:newObj.rank,
            USL:newObj.USL,
            LSL:newObj.LSL,
            UCL:newObj.UCL,
            LCL:newObj.LCL,
            // Explicit, like the five above. An override row may legitimately
            // carry undefined here ("inherit"); the ROOT may not -- it is the
            // thing being inherited from -- so it is normalised to a boolean.
            quality_essential:newObj.quality_essential!==false,
          };
          set_MeasureInfo(newMeasureInfo);
        }
  
      },...SelMarginInfo,{}];
      // console.log(arr)


      return arr;
    }).flat();
    

    const menu_ = (
      <Menu onClick={(ev) => {
        let cmI={..._control_margin_info}
        if(cmI[ev.key]!==undefined)return;
        cmI[ev.key]=[];
        completeSingleCtrlMarginInfo(cmI[ev.key],_measureInfo);

        
        let _={...displayInfoSet};
        _[ev.key]={};
        setDisplayInfoSet(_)
        // console.log(cmI);
        set_control_margin_info(cmI);
      }
      }>
        {tagGroupsPreset[0].tags.map((m, idx) =>
          <Menu.Item key={m} idx={idx}>
            <a target="_blank" rel="noopener noreferrer">
              {m}
            </a>
          </Menu.Item>)}
      </Menu>
    );
    return<>
      {Object.keys(_control_margin_info).map(text=>(
        <Tag color={displayInfoSet[text]===undefined?undefined:"#108ee9"}
          key={text}
          closable
          onClose={() => {
            let cmI={..._control_margin_info};
            delete cmI[text];
            set_control_margin_info(cmI);

          }}
          onClick={() =>{
            let _={...displayInfoSet};
            if(_[text]===undefined)
            {
              _[text]={};
            }
            else
            {
              delete _[text];
            }
            setDisplayInfoSet(_)
          }}>{text}
        </Tag>
        ))}


      <Dropdown overlay={menu_}>
        <Input size="small" placeholder="Add Margin Cat" prefix={<PlusOutlined />} 
          value={inputText} 
          onChange={(text)=>setInputText(text.target.value)}
          onPressEnter={(text)=>{
            let v = text.target.value;
            if(_control_margin_info[v]!==undefined || v.length==0)
              return;
            let cmI={..._control_margin_info}
            cmI[v]=[]
            completeSingleCtrlMarginInfo(cmI[v],_measureInfo);
            
            let _={...displayInfoSet};
            _[v]={};
            setDisplayInfoSet(_)
            set_control_margin_info(cmI);

            setInputText("");
        }}/>
      </Dropdown>

      <Table columns={columns} 
        expandable_={{ 
          expandedRowRender:record => 
          {
            let AA = Object.keys(displayInfoSet)
              .map(text=>_control_margin_info[text])
              .map(info=>info.find(m=>m.id==record.id));
            return <DisplayMarginSet MarginInfo={AA} DICT={DICT}/>;
          }
        }}
        dataSource={measureX} 
        size="small" 
        pagination={false}
      />
    </>;
}

export function Measure_Calc_Editor({ target, onChange, className, renderContext: { measure_list, ref_keyTrace_callback, ref } }) {
  let staticObj = useRef({
    insertIdx: undefined,
    ref_new_idx:9999
  });


  let _this=staticObj.current;
  //console.log(target.obj.calc_f);

  let ele = GetObjElement(target.obj, target.keyTrace);
  let fx = ele;
  const [fxOK, setFxOK] = useState(false);

  const inputEl = useRef(null);
  const [measureIDInfo, setMeasureIDInfo] = useState([]);

  const [fxExp, setFxExp] = useState(fx.exp);

  const DICT = useSelector(state => state.UIData.DICT);

  function translatedExpChangeEvent(newExp) {

    let postExp = [];
    try{
      postExp=Exp2PostfixExp(newExp);
      let aexp_to_del =
      postExp
        .filter(atom_exp => atom_exp.includes('"'));

      if (aexp_to_del.length > 0)//If there is any content with unreplaced '"', replace it
      {
        aexp_to_del.forEach(to_del => {
          newExp = newExp.replace(to_del, "");
        });
        postExp = Exp2PostfixExp(newExp);

      }
      const chk = parseCheckExpressionValid(postExp, measureIDInfo.map(info => info.id_exp));
      if (!chk.ok && chk.unsupported.length) {
        // Named, not just refused. The core's operator set is six entries long
        // (JudgeCALC.cpp); an operator outside it is a permanent NA, so say
        // which one rather than letting it be saved and discovered on the line.
        message.error('核心不支援的運算: ' + chk.unsupported.join(' ')
          + ' (可用: + - * / max min)');
      }
      let isAvail = chk.ok;
      if (isAvail) {
        //onChange();
        onChange(target, "input", {
          target: {
            value: {
              exp: newExp,
              post_exp: postExp
            }
          }
        })
      }
      setFxOK(isAvail);
    }
    catch(e)
    {
      setFxOK(false);
    }
    setFxExp(newExp);

    //
  }
  if (ref.length > staticObj.current.ref_new_idx && staticObj.current.insertIdx !== undefined) {
    let iidx = staticObj.current.insertIdx;
    var nfxExp =  fxExp.slice(0,iidx)+"["+ ref[staticObj.current.ref_new_idx].id+ "]"+fxExp.slice(iidx);
    staticObj.current.ref_new_idx=9999;
    staticObj.current.insertIdx = undefined;
    translatedExpChangeEvent(nfxExp);
  }

  useEffect(() => {
    let idInfo = measure_list.map(m => ({ id: m.id, id_exp: "[" + m.id + "]", name: m.name }));
    setMeasureIDInfo(idInfo);
  }, [measure_list]);

  useEffect(() => {
    let idMap = measure_list.map(m => "[" + m.id + "]");
    let postExp = Exp2PostfixExp(fx.exp);
    // .ok — the check returns { ok, unsupported } now. Reading the object as a
    // boolean would make this mount-time validation always pass, which is worse
    // than the bug it was added for: a def opened with an expression the core
    // cannot run would look healthy.
    let isAvail = parseCheckExpressionValid(postExp, idMap).ok;
    setFxOK(isAvail);
  }, [])


  function translateForward(text_id) {    //[2]*[6] => "OBJAA"*"OBJBB"  note:[{name:"OBJAA",id:2},{name:"OBJBB",id:6}]
    let translatedExp = text_id;
    let regexMatchIdBlock = /\[([^\[^\]]+)\]/g;
    let idErrSet = [];
    let idOKSet = [];
    let matchInfo;
    while ((matchInfo = regexMatchIdBlock.exec(translatedExp)) !== null) {
      let idx_str = matchInfo[1];
      let idx_wBr = matchInfo[0];
      
      let translateInfo = measureIDInfo.find(info => parseInt(idx_str) === info.id)//str is string, id is integer

      let setInfo = {
        match: matchInfo,
        measure: translateInfo
      }
      if (translateInfo === undefined) {
        idErrSet.push(setInfo);
      }
      else {
        idOKSet.push(setInfo)
      }
    }
    //console.log(idOKSet,idErrSet);
    idErrSet.forEach(idErr => {
      translatedExp = translatedExp.replace(idErr.match[0], "");
    });

    if(idOKSet.length>0)
    {

      //{cat:"dog",dog:"goat",goat:"cat"};
      var regexMapList=idOKSet.map(idOK=>{
        let key = idOK.match[0];
        let safekey=key.replace("[","\\[").replace("]","\\]");
        let str='"' + idOK.measure.name + '"';
        return {key,safekey,str}
      })


      var re = new RegExp(regexMapList.map(map=>map.safekey).join("|")
        ,"gi");
      translatedExp = translatedExp.replace(re, matched=>{
        // console.log(matched,regexMapObj[matched]);
        return regexMapList.find(map=>map.key==matched).str;
    });
    }
    return translatedExp;
  }
  let translatedExp = translateForward(fxExp);
  //translate measure id to readable measure name

  function translateBack(text_name) {//"OBJAA"*"OBJBB" => [2]*[6] note:[{name:"OBJAA",id:2},{name:"OBJBB",id:6}]
    measureIDInfo.forEach(idinfo => {//translate readable measure name to measure id
      text_name = text_name.replaceAll('"' + idinfo.name + '"', idinfo.id_exp);
    });
    return text_name;
  }

  function untranslatedIdx(text, idx) {

    var text_wTag = [text.slice(0, idx), "$0", text.slice(idx)].join('');
    text_wTag = translateBack(text_wTag);
    let utidx = text_wTag.indexOf('"');
    if (utidx < 0) utidx = text_wTag.indexOf("$0");
    return utidx;
  }


  return <div className={className + " HXA " + (fxOK ? "" : " error  ")}>
    <TextArea
      value={translatedExp}
      autosize={{ minRows: 1, maxRows: 6 }}
      ref={inputEl}
      onChange={(ev) => {
        let text = translateBack(ev.target.value);
        translatedExpChangeEvent(text);
      }}
      onBlur={(ev)=>{
        // console.log(ev,inputEl.current);
        _this.selectionStart=ev.target.selectionStart;
      }}
    />
    <Button key="xx" className="s vbox black"
      onClick={_ => {
        let true_idx = untranslatedIdx(translatedExp, _this.selectionStart);
        staticObj.current.insertIdx = true_idx;
        staticObj.current.ref_new_idx = ref.length;
        //console.log(translatedExp,selectionStart,fxExp, true_idx);

        ref_keyTrace_callback(["ref", ref.length]);
      }}>{DICT.defConf.calc_add_measure}</Button>
  </div>
}


let renderMethods = {
  Measure_Calc_Editor,
  // Compact selection dropdown. Value read via target.obj[lastKey] so it
  // resolves correctly both at root level AND inside nested sub-objects
  // (target.obj is the immediate parent, keyTrace is the full from-root path).
  Dropdown_List: ({ target, onChange, renderContext: { list }, props }) => {
    const lastKey = target.keyTrace[target.keyTrace.length - 1];
    const current = target.obj[lastKey];
    const label = labelForKey(props, lastKey);
    const menu = (
      <Menu onClick={(ev) => {
        onChange(target, "Dropdown_List", { target: { value: list[ev.key] } });
      }}>
        {list.map((m, idx) => <Menu.Item key={idx}>{m}</Menu.Item>)}
      </Menu>
    );
    return <CompactRow label={label}>
      <Dropdown overlay={menu} trigger={['click']}>
        <a href="#" style={{
          display: 'inline-flex', alignItems: 'center', justifyContent: 'space-between',
          height: 22, padding: '0 8px', fontSize: 12,
          background: '#1565c0', color: 'white', borderRadius: 3,
          minWidth: 96, gap: 6,
        }} onClick={(e) => e.preventDefault()}>
          <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {current ?? '—'}
          </span>
          <CaretDownOutlined />
        </a>
      </Dropdown>
    </CompactRow>;
  },
  // Compact USL/LSL/UCL/LCL setup. Pre-refactor this rendered a popover
  // anchor (ULRangeAcc) with ±0.1 / ±0.01 buttons inside + a NumberAccInput;
  // ergonomics were touch-targeted. New layout: one row = compact label +
  // number input + tiny ±0.1 / ±0.01 inline buttons (Reset goes back to
  // the parent `value` field — same semantics as the old Popover "R" btn).
  ULRangeSetup: ({ onChange, target, props }) => {
    const value = GetObjElement(target.obj, target.keyTrace);
    const lastKey = target.keyTrace[target.keyTrace.length - 1];
    const objM1 = GetObjElement(target.obj, target.keyTrace, target.keyTrace.length - 2);
    let tarExt = "";
    { const idfo = lastKey.lastIndexOf("_"); if (idfo >= 0) tarExt = "_" + lastKey.substr(idfo + 1); }
    const RangeCValue = parseFloat(objM1["value" + tarExt]);
    const label = labelForKey(props, lastKey);
    const set = (v) => onChange(target, "input-number", { target: { value: '' + toFixedNum(v, 4) } });
    return <CompactRow label={label}>
      <NumberAccInput value={value} onChange={(evt) => onChange(target, "input-number", evt)} />
      <StepButton onClick={() => set(value + 0.1)}  title="+0.1">+.1</StepButton>
      <StepButton onClick={() => set(value - 0.1)}  title="−0.1">−.1</StepButton>
      <StepButton onClick={() => set(value + 0.01)} title="+0.01">+.01</StepButton>
      <StepButton onClick={() => set(value - 0.01)} title="−0.01">−.01</StepButton>
      <StepButton onClick={() => set(RangeCValue)}  title="reset to value">R</StepButton>
    </CompactRow>;
  },
  AngleRangeSetup: ({ onChange, target, props }) => {
    const value = GetObjElement(target.obj, target.keyTrace);
    const lastKey = target.keyTrace[target.keyTrace.length - 1];
    const label = labelForKey(props, lastKey);
    const set = (v) => onChange(target, "input-number", { target: { value: '' + toFixedNum(((v % 360) + 360) % 360, 4) } });
    return <CompactRow label={label}>
      <NumberAccInput value={value} onChange={(evt) => onChange(target, "input-number", evt)} />
      <StepButton onClick={() => set(value + 30)} title="+30">+30</StepButton>
      <StepButton onClick={() => set(value - 30)} title="−30">−30</StepButton>
      <StepButton onClick={() => set(value + 5)}  title="+5">+5</StepButton>
      <StepButton onClick={() => set(value - 5)}  title="−5">−5</StepButton>
      <StepButton onClick={() => set(0)}          title="0">0</StepButton>
    </CompactRow>;
  },
  SimpleSetup: ({ onChange, target, props }) => {
    const value = GetObjElement(target.obj, target.keyTrace);
    const lastKey = target.keyTrace[target.keyTrace.length - 1];
    const label = labelForKey(props, lastKey);
    const set = (v) => onChange(target, "input-number", { target: { value: '' + toFixedNum(v, 4) } });
    return <CompactRow label={label}>
      <NumberAccInput value={value} onChange={(evt) => onChange(target, "input-number", evt)} />
      <StepButton onClick={() => set(value * 2)} title="×2">×2</StepButton>
      <StepButton onClick={() => set(value / 2)} title="÷2">÷2</StepButton>
      <StepButton onClick={() => set(value + 1)} title="+1">+1</StepButton>
      <StepButton onClick={() => set(value - 1)} title="−1">−1</StepButton>
    </CompactRow>;
  },
}

// THE STUDIO'S OPENER, PUBLISHED BY THE COMPONENT THAT OWNS THE MODAL.
//
// Migration is two halves -- flip the engine, then go train features -- and the
// second half has to open a modal that only DEFCONF_MODE_NEUTRAL_UI can open,
// because the modal is its state. ACT_Migrate_To_Shape used to just CALL
// openSBM2 from SettingUI, where the name is not in scope: the two dispatches
// before it landed, the call threw ReferenceError, and everything after it --
// including seeding def_image_reg from the sig360 anchor -- never ran. What the
// operator got was the exact half-migrated def the code below warns about: a
// shape_based def with no trained features, which falls back to sig360 and
// looks, from the outside, like the migration never happened.
let sbm2Opener = null;

// MIGRATION, IN ONE PLACE, CALLABLE FROM ANYWHERE THAT HAS A DISPATCH.
//
// Flip the engine + apply the recommended fast coarse scale. anchor_corner and
// every other setting carry over untouched. def_image_reg is already stored at
// save; reference_image is emitted from the def name. Re-SAVE afterwards to
// persist.
// Open the SBM studio without changing anything first. The migration helper
// below also opens it, but it has a def to convert on the way; a def that is
// already shape_based and merely stored in the old format needs the studio and
// nothing else.
export function openShapeStudio() {
  setTimeout(() => { if (sbm2Opener) sbm2Opener(true); }, 0);
}

export function migrateDefToShapeBased(dispatch, edit_info) {
  dispatch(DefConfAct.Locating_Engine_Update('shape_based'));
  dispatch(DefConfAct.Shape_Match_Scale_Update(0.3));

  // SEED def_image_reg FROM THE SIGNATURE ANCHOR.
  //
  // def_image_reg is the object frame's origin and angle, and an absent one is
  // not a neutral default: SBMStudio's drawImage translates by -(cx, cy), so
  // 0,0 puts the frame origin at the IMAGE CORNER. Every caliper is then placed
  // relative to a corner, and the part rotates about a corner -- which measures
  // fine on the reference image, where the rotation is zero, and goes wrong on a
  // part that arrives turned. Nothing reports it.
  //
  // A sig360 def already carries the right answer. Its object frame is anchored
  // at the signature centre, and the two quantities are the SAME one in the same
  // units (EverCheckCanvasComponent's image-align path already uses one as the
  // other's fallback), so migration can carry the frame across rather than
  // dropping it and asking the operator to redraw a frame the def already had.
  //
  // Only when the def has no registration of its own -- a def that has been
  // through the studio has one that was authored deliberately.
  const ei = edit_info || {};
  if (!ei.def_image_reg || typeof ei.def_image_reg.cx !== 'number') {
    // THE ANGLE COMES FROM AN INSPECTION OF THE REFERENCE IMAGE, NOT FROM THE
    // SIGNATURE.
    //
    // This used to read sig360info.reports[0] -- the EXTRACTOR report, i.e. the
    // def's own signature. Its `orientation` is 0 by definition (the signature
    // IS the frame), which EverCheckCanvasComponent already notes, so every
    // migrated def got angle 0. On test2 the part sits at -0.0233 rad in its
    // reference image; the object frame came out turned by 1.33 deg relative
    // to the one every caliper was authored in, and the extracted features'
    // fingerprint went from "ao-1.3334" to "ao0.0000". Measured 2026-09-04.
    //
    // DefConf runs an orientation inspect on entry (sendOrientationInspect) and
    // the TAKE seed already reads its report -- cx, cy, `rotate`, isFlipped.
    // Same source here. The extractor report stays as the fallback for cx/cy
    // only, with the honest angle 0 it always had.
    const insp = ei.inspReport && ei.inspReport.reports && ei.inspReport.reports[0];
    const sig = ei._obj && ei._obj.sig360info && ei._obj.sig360info.reports
                && ei._obj.sig360info.reports[0];
    let seed = null;
    if (insp && Number.isFinite(insp.cx) && Number.isFinite(insp.cy)) {
      seed = { cx: insp.cx, cy: insp.cy,
               angle: Number.isFinite(insp.rotate) ? insp.rotate : 0,
               isFlipped: !!insp.isFlipped };
    } else if (sig && Number.isFinite(sig.cx) && Number.isFinite(sig.cy)) {
      seed = { cx: sig.cx, cy: sig.cy, angle: 0, isFlipped: false };
      log.warn('[migrate] no inspection report yet; def_image_reg seeded from the '
               + 'signature with angle 0 -- redraw 定位 in the studio if the part is not level');
    }
    if (seed) dispatch(DefConfAct.EditInfo_Patch({ def_image_reg: seed }));
  }

  // Straight into the studio afterwards. Converting is only half of it: the def
  // now uses a locator with no trained features, and until 生成特徵點 has been
  // pressed and the def re-saved it falls back to sig360. This is also the only
  // way the SBM surfaces appear at all, so leaving the operator to find them
  // would be leaving them nothing to find.
  //
  // Ordered after the seeding, not before it: the studio reads def_image_reg
  // when it opens.
  setTimeout(() => { if (sbm2Opener) sbm2Opener(true); }, 0);
}

function SettingUI({})
{
  
  const defConf_lock_level = useSelector(state => state.UIData.defConf_lock_level);
  
  const edit_info = useSelector(state => state.UIData.edit_info);
  const dispatch = useDispatch();
  const ACT_DefConf_Lock_Level_Update= (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) };
  const ACT_Matching_Angle_Margin_Deg_Update= (deg) => dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) ;
    
  const ACT_Matching_Face_Update=(faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) };//-1(back)/0(both)/1(front)
  const ACT_Matching_Version_Update=(v) => { dispatch(DefConfAct.Matching_Version_Update(v)) };// 1=legacy, 2=phase2 dual-sig
  // NumberAccInput.onChange passes an event-like { target: { value: "<str>" } },
  // not a raw number. Unwrap + parse so the (number-typed) reducers accept it.
  const ACT_Inspection_Downsample_Update=(e) => { const n = parseFloat(e?.target?.value ?? e); if (Number.isFinite(n)) dispatch(DefConfAct.Inspection_Downsample_Update(n)) };// 1..8 (core caps at 4 today)
  const ACT_Sig_Match_Sim_Thres_Update=(e) => { const v = parseFloat(e?.target?.value ?? e); if (Number.isFinite(v)) dispatch(DefConfAct.Sig_Match_Sim_Thres_Update(v)) };// 0..1, core default 0.9
  // Anchor-morph (deformation correction) controls.
  const ACT_Morph_Mode_Update=(m) => dispatch(DefConfAct.Morph_Mode_Update(m));// "tps"|"wls_similarity"|"legacy"
  const ACT_Morph_TPS_Lambda_Update=(e) => { const raw = e?.target?.value ?? e; const v = parseFloat(raw); dispatch(DefConfAct.Morph_TPS_Lambda_Update(Number.isFinite(v) ? v : undefined)) };// core default 0.5
  const ACT_Morph_Max_Iter_Update=(e) => { const raw = e?.target?.value ?? e; const v = parseFloat(raw); dispatch(DefConfAct.Morph_Max_Iter_Update(Number.isFinite(v) ? v : undefined)) };// core default 1
  const ACT_Morph_Alpha_Update=(e) => { const raw = e?.target?.value ?? e; const v = parseFloat(raw); dispatch(DefConfAct.Morph_Alpha_Update(Number.isFinite(v) ? v : undefined)) };// (0,1], core default 1
  const ACT_Shape_Match_Scale_Update=(e) => { const raw = e?.target?.value ?? e; const v = parseFloat(raw); dispatch(DefConfAct.Shape_Match_Scale_Update(Number.isFinite(v) ? v : undefined)) };// (0,1], core default 1
  const ACT_Locating_Engine_Update=(v) => dispatch(DefConfAct.Locating_Engine_Update(v));// "sig360"|"shape_based"
  // One-click migration of a legacy sig360 def to the shape-based localizer:
  // flip the engine + apply the recommended fast coarse scale. anchor_corner and all
  // other settings are carried over untouched. def_image_reg is already stored at save;
  // reference_image is emitted from the def name. Re-SAVE afterwards to persist.
  const ACT_Migrate_To_Shape=() => migrateDefToShapeBased(dispatch, edit_info);

  const DICT = useSelector(state => state.UIData.DICT);
  return [
    <Checkbox
      checked={edit_info.matching_angle_margin_deg == 90}

      onChange={(ev) => {
        if (edit_info.matching_angle_margin_deg == 90)
          ACT_Matching_Angle_Margin_Deg_Update(180);
        else
          ACT_Matching_Angle_Margin_Deg_Update(90);
      }}
    >
      {dictLookUp("matchingAngleLimit180", DICT)}
    </Checkbox>,

    
    <Checkbox
    checked={edit_info.matching_angle_margin_deg == 5}

    onChange={(ev) => {
      if (edit_info.matching_angle_margin_deg == 5)
        ACT_Matching_Angle_Margin_Deg_Update(180);
      else
        ACT_Matching_Angle_Margin_Deg_Update(5);
    }}
    >
    {dictLookUp("matchingAngleLimit10", DICT)}
    </Checkbox>,
    <br />,
    <Checkbox
      checked={edit_info.matching_face == 1}
      onChange={(ev) => {

        if (edit_info.matching_face == 1)
          ACT_Matching_Face_Update(0);
        else
          ACT_Matching_Face_Update(1);

      }
      }
    >
      {dictLookUp("matchingFaceFrontOnly", DICT)}
    </Checkbox>,



    // The intrusion-size gate used to live here. Removed 2026-08-07: it was one
    // number for the whole def that could only say "something somewhere in this
    // image is too big, do not inspect at all". obj_detect clean-space regions
    // say it per region, in mm², and let each region choose whether a trip means
    // the part is bad or the measurement is untrustworthy.

    <Divider orientation="left">localizer</Divider>,
    <span>&nbsp;engine&nbsp;</span>,
    <select
      value={edit_info.locating_engine || 'sig360'}
      onChange={(e) => ACT_Locating_Engine_Update(e.target.value)}
      style={{ height: 24, fontSize: 12 }}
    >
      <option value="sig360">sig360 (contour signature)</option>
      <option value="shape_based">shape_based (line2Dup + ROI refine)</option>
    </select>,
    (edit_info.locating_engine !== 'shape_based') && <Button key="mig2shape" size="small"
      style={{ marginLeft: 8 }}
      onClick={ACT_Migrate_To_Shape}
      title="Switch to the shape-based localizer + set shape_match_scale=0.3, and carry the object frame across: def_image_reg is seeded from the sig360 signature anchor, so the part keeps rotating about the same origin. Re-save to persist. anchor_corner and other settings are kept.">
      → migrate to shape_based (v2)</Button>,

    // Localization regions / registration / ROI are authored in the dedicated
    // full-screen "SBM定位設定" modal (its own canvas, separate from measurement) —
    // opened from the editor toolbar. This panel keeps only engine + perf settings.
    (edit_info.locating_engine === 'shape_based') &&
      <div key="sbmhint" style={{ fontSize: 11, color: '#999', margin: '4px 0' }}>
        定位區域 / 定位線 / ROI 在工具列的「SBM定位設定」全螢幕視窗裡設定。
      </div>,

    // sig360-only perf knobs — hidden for shape_based (its coarse-match scale lives in
    // the SBM定位設定 studio; v2/downsample/sim-thres don't apply to the line2Dup path).
    (edit_info.locating_engine !== 'shape_based') && <React.Fragment key="sig360perf">
      <Divider orientation="left">sig360 perf</Divider>
      <Checkbox
        checked={edit_info.matching_version === 2}
        onChange={() => ACT_Matching_Version_Update(edit_info.matching_version === 2 ? 1 : 2)}
      >v2 matcher (morph-boundary dual-sig)</Checkbox>
      <span>&nbsp;downsample&nbsp;</span>
      <NumberAccInput
        min={1}
        max={8}
        value={edit_info.inspection_downsample || 1}
        onChange={ACT_Inspection_Downsample_Update}
      />
      <br />
      <span>&nbsp;match sim thres&nbsp;</span>
      <NumberAccInput
        min={0}
        max={1}
        value={edit_info.sig_match_sim_thres === undefined ? 0.9 : edit_info.sig_match_sim_thres}
        onChange={ACT_Sig_Match_Sim_Thres_Update}
      />
      <br />
      <span>&nbsp;shape match scale&nbsp;</span>
      <NumberAccInput
        min={0.1}
        max={1}
        value={edit_info.shape_match_scale === undefined ? 1 : edit_info.shape_match_scale}
        onChange={ACT_Shape_Match_Scale_Update}
      />
    </React.Fragment>,

    <Divider orientation="left">anchor morph</Divider>,
    <span>&nbsp;mode&nbsp;</span>,
    <select
      value={edit_info.morph_mode || 'tps'}
      onChange={(e) => ACT_Morph_Mode_Update(e.target.value)}
      style={{ height: 24, fontSize: 12 }}
    >
      <option value="tps">tps (similarity-base RBF, default)</option>
      <option value="wls_similarity">wls_similarity</option>
      <option value="legacy">legacy (polar)</option>
    </select>,
    <br />,
    (edit_info.morph_mode || 'tps') === 'tps' && <span key="ml">&nbsp;rbf λ (bending)&nbsp;</span>,
    (edit_info.morph_mode || 'tps') === 'tps' && <NumberAccInput key="mli"
      min={0}
      max={50}
      value={edit_info.morph_tps_lambda === undefined ? 0.5 : edit_info.morph_tps_lambda}
      onChange={ACT_Morph_TPS_Lambda_Update}
    />,
    (edit_info.morph_mode || 'tps') !== 'legacy' && <span key="mit">&nbsp;max iter&nbsp;</span>,
    (edit_info.morph_mode || 'tps') !== 'legacy' && <NumberAccInput key="miti"
      min={1}
      max={10}
      value={edit_info.morph_max_iter === undefined ? 1 : edit_info.morph_max_iter}
      onChange={ACT_Morph_Max_Iter_Update}
    />,
    ((edit_info.morph_mode || 'tps') !== 'legacy' && edit_info.morph_max_iter > 1) && <span key="maa">&nbsp;alpha(0-1)&nbsp;</span>,
    ((edit_info.morph_mode || 'tps') !== 'legacy' && edit_info.morph_max_iter > 1) && <NumberAccInput key="maai"
      min={0.1}
      max={1}
      value={edit_info.morph_alpha === undefined ? 1 : edit_info.morph_alpha}
      onChange={ACT_Morph_Alpha_Update}
    />,

    <Divider orientation="left"/>,

    <Checkbox
      checked={defConf_lock_level != 0}
      onChange={(ev) => {
        ACT_DefConf_Lock_Level_Update(
          (defConf_lock_level == 0) ? 1 : 0
        );
      }}
    >
      {<Icon type={(defConf_lock_level != 0) ? "lock" : "unlock"} />}
      {DICT.defConf.lock_level +":"+ defConf_lock_level}
    </Checkbox>
  ]
}


// The SBM setup studio lives in SBMStudio2.jsx; SBMStudio.jsx keeps only the
// hook canvas both versions were built on.


// A def load in flight is only valid until something else changes the def.
//
// The load is a round trip: LD goes out, and when the reply lands its packets
// are dispatched as one bundle that RE-APPLIES the whole def -- registration,
// feature cache, image, the lot. If a retake (or another load) happened while
// that was in the air, the bundle silently puts the OLD def back on top of it.
//
// That is not theoretical: TAKE pressed shortly after opening a recipe produced
// a "new object" carrying the previous def's def_image_reg and __shape_cache,
// with __img_fresh_capture back to false -- every measurement then pinned to an
// origin from a frame that no longer exists, and nothing on screen says so. It
// reproduced about one run in three in the regression suite (RESTORED by
// ATBundle, three times, right after Def_Retake).
//
// So each load takes a ticket, and only the current ticket may dispatch.
let defLoadGen = 0;
export function invalidateDefLoads() { return ++defLoadGen; }

function loadDefFile(defModelPath,ACT_DefConf_Lock_Level_Update,ACT_WS_SEND_BPG,CORE_ID,dispatch)
{
  const myGen = ++defLoadGen;
  function actionGen_W_IGNORE_LOCK(pkts)
  {
    return{
      type: "ATBundle",
      ActionThrottle_type: "express",
      
      data: pkts.map(pkt =>{
        let act = BPG_Protocol.map_BPG_Packet2Act(pkt)
        if(act!==undefined)
        {
          act.IGNORE_DEFCONF_LOCK=true;
        }
        return act;
      }).filter(act => act !== undefined)
    }
  }

  ACT_DefConf_Lock_Level_Update(1);
  loadDefWithImageFallback({
    defModelPath,
    defExtension: DEF_EXTENSION,
    downSampLevel: IMG_LOAD_DOWNSAMP_LEVEL,
    send: (payload, promiseCBs) => {
      log.info("[loadDef]", { defModelPath, DEF_EXTENSION, CORE_ID, imgsrc: payload.imgsrc })
      ACT_WS_SEND_BPG(CORE_ID, "LD", 0, payload, undefined, promiseCBs);
    },
  })
    .then(({ pkts }) => {
      if (myGen !== defLoadGen) {
        log.warn('[loadDef] reply dropped -- the def changed while it was in flight',
                 { defModelPath, myGen, now: defLoadGen });
        return;
      }
      dispatch(actionGen_W_IGNORE_LOCK(pkts))

      // new Promise((resolve, reject) => {
      //   ACT_WS_SEND_BPG(CORE_ID, "LD", 0,
      //     {
      //       imgsrc: defModelPath,
      //       down_samp_level:IMG_LOAD_DOWNSAMP_LEVEL
      //     },
      //     undefined, { resolve, reject });
      //   setTimeout(() => reject("Timeout"), 5000)
      // })
      //   .then((pkts) => {
          
      //     dispatch(actionGen_W_IGNORE_LOCK(pkts))
      //   })
      //   .catch((err) => {
      //     log.info(err);
      //   })
    })
    .catch((err) => {
      log.info(err);
    })


}


function modShapeCleanUp(mod_shape)
{
  // measure shapes don't go through point-fitting — the CHECK flow is
  // a no-op for them (downstream code reads judgeReports instead).
  if(mod_shape.type===UIAct.SHAPE_TYPE.measure)
  {
    return undefined;
  }
  // NA: the latest inspection FAILED. We still need to return the shape so
  // the caller can SetShape it through to redux — otherwise stale per-
  // inspection artifacts (cal_hits, _pt1/_pt2, adj_pt1) from a prior
  // SUCCESS run linger on the stored shape and the def-conf overlay shows
  // the OLD green hits. ShapeAdjustsWithInspectionResult already deleted
  // those fields on its NA early-return; persisting the cleared shape
  // makes the WebUI show "no result" correctly.
  if(mod_shape.inspection_status===BPG_Protocol.INSPECTION_STATUS.NA)
  {
    delete mod_shape["inspection_value"];
    delete mod_shape["inspection_status"];
    return mod_shape;
  }

  delete mod_shape["inspection_value"]
  delete mod_shape["inspection_status"]
  if(mod_shape.type==UIAct.SHAPE_TYPE.search_point)
  {
    mod_shape.pt1.x=mod_shape.adj_pt1.x;
    mod_shape.pt1.y=mod_shape.adj_pt1.y;
    delete mod_shape["adj_pt1"]
    delete mod_shape["o_pt1"]
  }
  return mod_shape;
}



// The live preview inside the TAKE dialog.
//
// Same shape as CalibrationUI's PreviewCanvas, and for the same reason: streamed
// frames arrive through the normal BPG pipeline into edit_info.img, and
// Preview_CanvasComponent is the one canvas that renders that without needing a
// def loaded. disableImageAlign because a raw live frame must not be rotated by
// def_image_reg -- the whole point here is to see what the camera sees.
class TakePreviewCanvas extends React.Component {
  componentDidMount() {
    this.ec = new EC_CANVAS_Ctrl.Preview_CanvasComponent(this.refs.cv);
    this.ec.disableImageAlign = true;
    this.ec.SetStandalonePreview(this.props.mmpp);
    this._fitted = false;
    this.update();
  }
  componentWillUnmount() { if (this.ec) this.ec.resourceClean(); }
  componentDidUpdate() { this.update(); }
  update() {
    if (!this.ec) return;
    if (this.props.c_state) this.ec.SetState(this.props.c_state);
    const img = this.props.img;
    if (img) {
      this.ec.SetImg(img);
      // Fit once. Re-fitting on every frame would fight the operator's pan/zoom
      // thirty times a second.
      if (!this._fitted) { this.ec.scaleImageToFitScreen(); this._fitted = true; }
    }
    this.ec.draw();
  }
  onResize(w, h) { if (this.ec) { this.ec.resize(w, h); this.ec.draw(); } }
  render() {
    return <div style={{ width: '100%', height: '100%', position: 'relative' }}>
      <canvas ref="cv" style={{ width: '100%', height: '100%', display: 'block' }} />
      <ReactResizeDetector handleWidth handleHeight
        onResize={(w, h) => this.onResize(w, h)} />
    </div>;
  }
}

// The TAKE dialog: name the object, then choose the frame it will be built from.
//
// The old one was five bare buttons in a modal footer with the string "<<<不重置"
// floating between two of them, so 立即 and 立即 were told apart only by which
// row they sat on -- and every one committed on the first click, so a mis-press
// reset the def.
//
// Nothing here dispatches def state. Name, tags and the keep switch are local;
// the caller gets them in one go when 使用這一幀 is pressed, and cancelling
// leaves the def as it was. The one thing that DOES escape is the live stream,
// because frames land in edit_info.img -- see the restore on cancel.
function TakeSetupDialog({ triggerTimeout, onGo, onCancel, onStreamStart,
                          onStreamStop, onWaitTrigger, loadInstMmpp }) {
  // THIS COMPONENT READS REDUX ITSELF. It must not be handed the live frame.
  //
  // The modal is opened by storing a React ELEMENT in state:
  //   setModal_view({ view: <TakeSetupDialog img={edit_info.img} .../> })
  // That element is a snapshot of the moment it was built. edit_info.img is
  // replaced on every streamed frame, and the element is never rebuilt, so the
  // preview showed the picture that was on screen when the dialog opened and
  // never moved -- while the DefConf canvas behind it, which subscribes for
  // itself, animated. Two views of the same slot disagreeing, with the live one
  // hidden behind the dead one.
  //
  // useSelector subscribes THIS component, independently of whether its parent
  // re-renders, which is the only thing that fixes a view stored in state.
  const edit_info = useSelector((st) => st.UIData.edit_info);
  const c_state = useSelector((st) => st.UIData.c_state);
  const img = edit_info.img;
  const hasImage = !!img;
  // Initial values only: the component mounts once per opening, so reading
  // these here is the same snapshot the props used to carry.
  const initName = edit_info.DefFileName;
  const initTag = edit_info.DefFileTag;
  const [phase, setPhase] = React.useState('name');
  const [name, setName] = React.useState(initName || '');
  const [tag, setTag] = React.useState((initTag || []).join(','));
  // Default OFF. This is 建立新物件 -- starting from blank is the expectation,
  // and keeping the wrong measurements costs more than re-drawing them.
  const [keep, setKeep] = React.useState(false);
  const [streaming, setStreaming] = React.useState(false);
  const [waiting, setWaiting] = React.useState(false);
  // Which of the core's two caches holds the frame the operator is looking at.
  const [streamed, setStreamed] = React.useState(false);
  // WHOSE mm/px this frame is measured in. A camera frame belongs to the
  // MACHINE's scale (lens calibration); the def's own image belongs to the def's.
  // Two different questions from `streamed`, which only picks a cache -- a
  // single-shot trigger is a camera frame but lands in __CACHE_IMG__.
  const [fromCamera, setFromCamera] = React.useState(false);
  // The machine's own mm/px, read once when the dialog opens.
  const [instMmpp, setInstMmpp] = React.useState(undefined);
  React.useEffect(() => {
    if (loadInstMmpp) loadInstMmpp().then(setInstMmpp, () => {});
  }, []);   // eslint-disable-line react-hooks/exhaustive-deps

  // THE PREVIEW'S SCALE FOLLOWS THE PICTURE, like everything else here.
  //
  // The first version read `edit_info.mmpp`, which does not exist -- nothing in
  // the app has ever written that field, and this was its only reader. So the
  // canvas got undefined and drew with no scale at all. The def was fine, which
  // is why it looked right everywhere except in here.
  //
  // A camera frame is measured in the MACHINE's scale (lens_calib.json); the
  // def's own image in the def's, which the editor object can compute. Each
  // falls back to the other so a missing calibration still gives a usable
  // preview rather than none.
  const defMmpp = (edit_info._obj && typeof edit_info._obj.getEditorMmpp === 'function')
    ? edit_info._obj.getEditorMmpp() : undefined;
  const ok = (v) => Number.isFinite(v) && v > 0;
  const mmpp = fromCamera
    ? (ok(instMmpp) ? instMmpp : defMmpp)
    : (ok(defMmpp) ? defMmpp : instMmpp);

  const nameOk = !!name.trim();
  const busy = streaming || waiting;

  if (phase === 'name') {
    return <div style={{ maxWidth: 520 }}>
      <div style={{ fontSize: 12, color: '#999', marginBottom: 4 }}>物件名稱（必填）</div>
      <Input data-testid="take-name" value={name} onChange={(e) => setName(e.target.value)}
        size="large" placeholder="例如 HY-1234-A"
        onPressEnter={() => nameOk && setPhase('capture')} />
      <div style={{ fontSize: 12, color: '#999', margin: '12px 0 4px' }}>標籤（逗號分隔,可空白）</div>
      <Input data-testid="take-tags" value={tag} onChange={(e) => setTag(e.target.value)}
        size="large" placeholder="例如 客戶A,銅件,量產" />
      <div style={{ fontSize: 11.5, color: '#888', margin: '12px 0', lineHeight: 1.7 }}>
        下一步選一張影像。確認之後這就是一個<b>新的物件</b>,跟目前開著的配方不再有關係:
        存檔會存成新檔案,不會蓋掉原本那個。中途取消則什麼都不會改變。
      </div>
      <div style={{ textAlign: 'right', borderTop: '1px solid #333', paddingTop: 12 }}>
        <Button data-testid="take-cancel" onClick={onCancel} style={{ marginRight: 8 }}>取消</Button>
        <Button data-testid="take-next" type="primary" disabled={!nameOk}
          data-enabled={nameOk ? '1' : '0'}
          onClick={() => setPhase('capture')}>下一步:選影像</Button>
      </div>
    </div>;
  }

  // The semantics a test needs, not just handles. Asserting "a button got
  // clicked" proves nothing here; the questions worth asking are which cache the
  // frame will come from, whose scale it will be measured in, and whether the
  // panel is streaming -- and every one of those has already been wrong while
  // the screen looked fine. See TEAM_HANDOFF §13.
  return <div data-testid="take-capture"
    data-phase={streaming ? 'streaming' : waiting ? 'waiting' : 'idle'}
    data-has-image={hasImage ? '1' : '0'}
    data-from-camera={fromCamera ? '1' : '0'}
    data-src={streamed ? 'lastview' : 'cache'}
    data-keep={keep ? '1' : '0'}
    style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 10 }}>
    <div style={{ flex: '1 1 auto', minHeight: 0, background: '#141618',
                  border: '1px solid #333', borderRadius: 6, position: 'relative' }}>
      {hasImage
        ? <TakePreviewCanvas mmpp={mmpp} c_state={c_state} img={img} />
        : <div style={{ position: 'absolute', inset: 0, display: 'flex', alignItems: 'center',
                        justifyContent: 'center', color: '#888', lineHeight: 1.9,
                        textAlign: 'center' }}>
            <div>目前沒有影像<br />
              <span style={{ fontSize: 12 }}>按「開始串流」或「等待觸發」取得一張。</span></div>
          </div>}
      {streaming && <div style={{ position: 'absolute', top: 10, left: 10,
        background: '#a8071a', color: '#fff', padding: '4px 12px', borderRadius: 20,
        fontSize: 13, fontWeight: 600 }}>● 串流中</div>}
      {waiting && <div style={{ position: 'absolute', top: 10, left: 10,
        background: '#d48806', color: '#fff', padding: '4px 12px', borderRadius: 20,
        fontSize: 13, fontWeight: 600 }}>等待觸發訊號…</div>}
    </div>

    <div style={{ flex: '0 0 auto', display: 'flex', alignItems: 'center', gap: 12,
                  flexWrap: 'wrap' }}>
      <div style={{ fontSize: 14, fontWeight: 600, marginRight: 4 }}>{name}</div>

      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <Switch data-testid="take-keep" checked={keep} disabled={busy} onChange={setKeep} />
        <span style={{ fontSize: 13, color: keep ? '#e8eaed' : '#8b929c' }}>
          {keep ? '保留量測設定' : '清除量測設定'}
        </span>
      </div>

      <div style={{ flex: '1 1 auto' }} />

      {streaming
        ? <Button data-testid="take-stream-stop" danger type="primary" size="large"
            style={{ height: 48, minWidth: 140 }}
            onClick={() => { onStreamStop(); setStreaming(false); }}>■ 停止串流</Button>
        : <>
            <Button data-testid="take-stream-start" size="large" style={{ height: 48 }}
              disabled={busy}
              onClick={() => { setStreamed(true); setFromCamera(true);
                               setStreaming(true); onStreamStart(); }}>
              ▶ 開始串流</Button>
            <Button data-testid="take-wait-trigger" size="large" style={{ height: 48 }}
              disabled={busy}
              onClick={() => {
                setWaiting(true);
                onWaitTrigger().then(
                  () => { setWaiting(false); setStreamed(false); setFromCamera(true); },
                  () => { setWaiting(false); });
              }}>⏱ 等待觸發</Button>
          </>}

      <Button data-testid="take-use-frame" type="primary" size="large"
        style={{ height: 48, minWidth: 150 }}
        data-enabled={(busy || !hasImage) ? '0' : '1'}
        disabled={busy || !hasImage}
        onClick={() => onGo({
          name: name.trim(),
          tags: tag.split(',').map((t) => t.trim()).filter((t) => t.length),
          keep,
          srcType: streamed ? '__LAST_DATA_VIEW_CACHE_IMG__' : '__CACHE_IMG__',
          fromCamera,
        })}>✓ 使用這一幀</Button>

      <Button data-testid="take-cancel" size="large" style={{ height: 48 }}
        onClick={onCancel}>取消</Button>
    </div>

    <div style={{ flex: '0 0 auto', fontSize: 11.5, color: '#888', lineHeight: 1.7 }}>
      {streaming
        ? '停止之後,畫面上停住的那一幀就是要用的那一張。'
        : hasImage
          ? '「使用這一幀」會拿畫面上這一張當新物件的樣板影像。'
          : '還沒有影像可以用。'}
      {keep && '　保留量測設定:量測特徵和比對參數會留著,定位、SBM 特徵、特徵範圍仍然要重做。'}
      <div style={{ marginTop: 3 }}>
        比例尺:{fromCamera
          ? <b style={{ color: '#5b9dff' }}>用機台的鏡頭校正（相機實拍）</b>
          : <span>沿用這個 def 原本的 mm/px（使用現有圖像）</span>}
      </div>
    </div>
  </div>;
}

function DEFCONF_MODE_NEUTRAL_UI({})
{
  const DICT = useSelector(state => state.UIData.DICT);
  
  const [fileSavingCallBack,setFileSavingCallBack]=useState(undefined);
  const [waitForSnapFlag,setWaitForSnapFlag]=useState(false);
  const dispatch = useDispatch();
  const ACT_EXIT=(arg) =>dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT)) ;

  const ACT_Shape_List_Update=(newlist)=>dispatch(DefConfAct.Shape_List_Update(newlist));
  
  const ACT_Line_Add_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Line_Create)) };
  const ACT_Arc_Add_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Arc_Create)) };
  const ACT_Search_Point_Add_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Search_Point_Create)) };
  const ACT_Aux_Point_Add_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Aux_Point_Create)) };
  const ACT_Shape_Edit_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Shape_Edit)) };
  const ACT_Measure_Add_Mode= (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.Measure_Create)) };

  const ACT_Shape_List_Reset= () => { dispatch(DefConfAct.Shape_List_Update([])) };
  // WHICH cached frame, because the core has two and they are not the same one.
  //
  // __CACHE_IMG__ is the image loaded on DefConf entry -- the def's own .png --
  // and a CI/FI stream does NOT update it. __LAST_DATA_VIEW_CACHE_IMG__ is the
  // last frame that went through the data view, i.e. what the live preview is
  // showing. Defaulting to the former and forgetting to say so is how a
  // "capture" ends up saving the PREVIOUS recipe's picture, with nothing on
  // screen looking any different (see saveAlternateImage, which hit this).
  // A FAILED TEMPLATE WRITE MUST NOT LOOK LIKE A SUCCESSFUL ONE EITHER.
  //
  // This is how TAKE gives the shape locator a template before the def has a
  // name: the captured frame is written to a scratch sidecar and its path is
  // stamped into the def-info. It was fire-and-forget, so an unwritable path or
  // an empty core cache landed nowhere -- and the first thing the operator saw
  // was 生成特徵失敗 in the SBM studio, several steps later, pointing at
  // thresholds and regions that were never the problem.
  //
  // Same treatment as ACT_Report_Save above, for the same reason.
  const ACT_Cache_Img_Save= (id, fileName, srcType) =>
    dispatch(UIAct.EV_WS_SEND_BPG(id, "SV", 0,
      { filename: fileName, type: srcType || "__CACHE_IMG__" },
      undefined,
      { resolve: (darr) => {
          const ack = (darr || []).map((p) => p && p.data)
            .find((d) => d && d.cmd === 'SV');
          if (ack && ack.ACK === false) {
            log.error('[action] template-save REFUSED', fileName, ack.errMsg);
            Modal.error({ title: '暫存樣板影像寫入失敗',
              content: (ack.errMsg || '核心沒有給原因') + '　（' + fileName + '.png）'
                     + ' — 接下來的「生成特徵點」會因為讀不到樣板而失敗。' });
          }
        },
        reject: (e) => log.error('[action] template-save no reply', fileName, e) }
    ))


  const ACT_DefConf_Lock_Level_Update= (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) };
  const ACT_DefFileName_Update=(newName) => { dispatch(DefConfAct.DefFileName_Update(newName)) };
  const ACT_DefFileTag_Update=(newInfo) => { dispatch(DefConfAct.DefFileTag_Update(newInfo)) };
  const ACT_DefFileHash_Update= (hash) => { dispatch(DefConfAct.DefFileHash_Update(hash)) };

  const ACT_Def_Model_Path_Update= (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) };
    
  // A FAILED SAVE MUST NOT LOOK LIKE A SAVE.
  //
  // This fired and forgot: no promise callbacks, so every refusal from the core
  // -- a full disk, an unwritable path, and now "this machine may not write into
  // the shared def folder" -- landed nowhere and the operator watched the dialog
  // close as if it had worked. The core carries the reason in errMsg; show it.
  const ACT_Report_Save=(id, fileName, content) => {
    let act = UIAct.EV_WS_SEND_BPG(id, "SV", 0,
      { filename: fileName },
      content,
      { resolve: (darr) => {
          const ack = (darr || []).map((p) => p && p.data)
            .find((d) => d && d.cmd === 'SV');
          if (ack && ack.ACK === false) {
            log.error('[action] report-save REFUSED', fileName, ack.errMsg);
            Modal.error({ title: '存檔被拒絕',
              content: (ack.errMsg || '核心沒有給原因') + '　（' + fileName + '）' });
          }
        }, reject: (e) => {
          log.error('[action] report-save no reply', fileName, e);
          Modal.error({ title: '存檔沒有回應',
            content: '核心沒有回覆存檔結果,檔案可能沒有寫入。' });
        } }
    )
    dispatch(act);
  };
  const ACT_Matching_Angle_Margin_Deg_Update= (deg) => { dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) };
  const ACT_Matching_Face_Update=(faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) };//-1(back)/0(both)/1(front)
    
  const ACT_WS_SEND_BPG= (...args) => dispatch(UIAct.EV_WS_SEND_BPG(...args));


  // Opening v2, from the toolbar button AND from the end of a TAKE.
  //
  // A fresh capture has no registration -- the def-scoped keys were just
  // cleared -- and every later action (generate, inspect, save) is measured
  // against a registration that is not there yet. Rather than let that be
  // discovered later, the studio is opened at the moment the picture arrives.
  // Published for migrateDefToShapeBased, which needs to open this modal from
  // outside. No dep array on purpose: openSBM2 closes over this render's
  // dispatch/setModal_view, so a stale one would set state on an old closure.
  useEffect(() => {
    sbm2Opener = openSBM2;
    return () => { if (sbm2Opener === openSBM2) sbm2Opener = null; };
  });

  const openSBM2 = (auto) => {
        dispatch(DefConfAct.Locating_Engine_Update('shape_based'));   // this surface implies shape_based
        setModal_view({
          title: auto ? "新物件 — 先設定定位" : "Shape-based 定位設定（v2）",
          footer: null,
          width: "96vw",
          style: { top: 12 },
          bodyStyle: { padding: 8, height: "86vh" },
          // The X is guarded too, because the studio applies LIVE: closing it does
          // not discard anything, so leaving by the corner commits exactly the
          // same broken state as 完成 would. Greying one and leaving the other
          // open would just be theatre.
          //
          // It asks rather than refuses. A def whose reference image cannot be
          // read can never regenerate, and a modal with no way out is worse than
          // the state it is protecting against -- so "仍要離開" stays, and the
          // save path asks once more before anything reaches disk.
          // Marked, because both buttons are in the toolbar and a screenshot from
          // the line has to say which one it came from.
          onCancel: () => {
            // Leaving the studio re-locates the object.
            //
            // The studio is where the registration line, the extraction region
            // and the trained features are set -- all three change where the
            // core thinks the part IS. The def canvas rectifies the image
            // against the last inspection report, so without a fresh one it
            // goes on drawing the picture aligned to the pose from BEFORE the
            // edits: overlays that sit next to the part instead of on it.
            //
            // Same call the image switcher already makes for the same reason
            // (useDefImages afterLoad). It lives in another component, so this
            // goes through the window event that component listens for --
            // the pattern defconf-images-changed already uses.
            const closeIt = () => {
              dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS));
              setModal_view(undefined);
              window.dispatchEvent(new Event('defconf-orient-now'));
            };
            // Auto-opened after a TAKE and still no registration: say so before
            // letting it close. Not a refusal -- there is a way out, because a
            // modal that cannot be left is worse than the state it guards -- but
            // leaving here silently is how someone spends ten minutes drawing
            // regions and measurements against an origin that is not there.
            if (auto && !edit_info.def_image_reg) {
              Modal.confirm({
                title: '還沒設定定位', width: 500,
                content: '這是新擷取的物件,還沒有定位原點和 0° 軸。'
                       + '接下來畫的範圍、抽的特徵、量的尺寸全部都是相對於它 —— '
                       + '現在離開的話,這些之後都要重做。',
                okText: '留下來設定', cancelText: '仍要離開',
                onCancel: closeIt,
              });
              return;
            }
            const lg = edit_info.__shape_lastGood;
            if (!edit_info.__shape_stale) { closeIt(); return; }
            // SAY WHAT ACTUALLY HAPPENS.
            //
            // This used to say the def would fall back to sig360 and that the
            // screen would not show it. That was true while the core refused a
            // cache whose fingerprint had moved; it is not true now -- the
            // cache loads regardless, and the def still locates with SBM.
            //
            // The real consequence is narrower and worth stating exactly: the
            // features, the crop and the origin all come from the cache, so a
            // registration edited after the last generation is simply not in
            // effect. Nothing is broken and nothing is lost; a setting is
            // waiting for a generation. That is a warning, not a trap, so the
            // dialog no longer stands between anyone and the door.
            Modal.confirm({
              title: '定位設定尚未生效', width: 500,
              content: '定位設定改過,但特徵還是上一次生成的。SBM 定位照常運作,'
                     + '不過它用的是舊的定位原點——新的設定要按「生成特徵點」才會生效。',
              okText: lg ? '還原上一版並離開' : '回去生成',
              cancelText: '仍要離開',
              onOk: () => {
                if (lg) {
                  dispatch(DefConfAct.EditInfo_Patch({
                    def_image_reg: lg.def_image_reg, roi_refine_points: lg.roi_refine_points,
                    __shape_cache: lg.cache, __shape_stale: undefined, __shape_lastGood: undefined,
                  }));
                  closeIt();
                }
                // No last-good version: stay in the studio, where 生成特徵點 is.
              },
              onCancel: closeIt,
            });
          },
          view: <SBMSetupView2
            sendBPG={(...a) => ACT_WS_SEND_BPG(CORE_ID, ...a)}
            onClose={() => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS)); setModal_view(undefined);
                             window.dispatchEvent(new Event('defconf-orient-now')); }}
            onSave={() => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS)); setModal_view(undefined); triggerSave();
                            window.dispatchEvent(new Event('defconf-orient-now')); }}
          />,
        });
  };

  // Save the current def (opens the file picker, then writes the .hydef + <def>.png on
  // a NEW save). Extracted from the SAVE button so the SBM studio can save in-modal too.
  const triggerSave = () => {
    if (defConf_lock_level > 2) return;
    setFileSavingCallBack((prevs, props) => (folderInfo, fileName, existed) => {
      log.debug("[file-exists]", { folderInfo, fileName, existed });
      let fileNamePath = folderInfo.path + "/" + fileName.replace('.' + DEF_EXTENSION, "");
      var enc = new TextEncoder();
      // SEED def_image_reg BEFORE generating, never after.
      //
      // It used to be written onto the finished report, which was fine while the
      // field sat at the def top level and outside the hash. Now it lives in
      // featureSet[0] and IS hashed, and defFileGeneration digests deliberately
      // BEFORE adding the __-prefixed keys -- so re-hashing out here would fold
      // __decorator and __shape_cache into the digest and quietly change what
      // featureSet_sha1 means. Seeding the input instead keeps one hash, computed
      // in one place, over one definition of the def.
      //
      // AND IT ONLY SEEDS AN ABSENT FIELD. This branch used to fire on every save
      // the file browser reported as new, overwriting a measured registration
      // with whatever the last inspection happened to say -- including a run that
      // used the sig360 FALLBACK, which is how a recipe field ends up sourced
      // from whichever locator was running at the time. Seeding what is missing
      // is the legitimate job; overwriting what somebody measured is not.
      let _ei = edit_info;
      if (!edit_info.def_image_reg) {
        const reg = edit_info.inspReport && edit_info.inspReport.reports && edit_info.inspReport.reports[0];
        if (reg && typeof reg.cx === 'number' && typeof reg.cy === 'number') {
          const seed = { cx: reg.cx, cy: reg.cy, angle: reg.rotate, isFlipped: !!reg.isFlipped };
          _ei = { ...edit_info, def_image_reg: seed };
          log.info("[action] def_image_reg seeded (was absent)", seed);
        }
      }
      let report = defFileGeneration(_ei);
      if (report.name === undefined || report.name.length == 0) {
        report.name = fileName;
        ACT_DefFileName_Update(fileName);
      }
      // The def goes to the database as well as to disk, and a failure here used
      // to be a console.log nobody was reading.
      //
      // That silence is expensive in a specific way: an inspection REPORT does
      // not carry the settings it was judged against. The def in the database is
      // what makes an archived report interpretable later -- without it there is
      // a verdict on record and no way to say what it meant. The local .def file
      // is already written by the time this runs, so the save genuinely did
      // succeed on this machine; only the shared half is missing. Say exactly
      // that, and offer the retry, because a dropped socket is usually over by
      // the time someone reads the dialog.
      const pushDefToDB = (rep, pathForMsg, attempt = 1) => {
        DefFile_DB_SEND(rep)
          .then((ret) => log.info("[def-db] uploaded", { path: pathForMsg, attempt }))
          .catch((err) => {
            const why = (err && err.message) ? err.message
              : (typeof err === 'string' && err.length) ? err : '沒有回應';
            log.warn("[def-db] upload failed", { path: pathForMsg, attempt, err: String(err) });
            Modal.confirm({
              title: '設定檔沒有上傳到資料庫',
              content: (<div style={{ lineHeight: 1.9 }}>
                <div><b>本機檔案已經存好了</b>（{pathForMsg}.{DEF_EXTENSION}），只有資料庫那一份沒有送出。</div>
                <div style={{ marginTop: 8 }}>原因：{why}</div>
                <div style={{ marginTop: 8, color: '#a8071a' }}>
                  檢驗報告不包含當時的檢驗設定,要靠資料庫裡的設定檔才能還原一筆報告是依據什麼判定的。
                  少了這一份,之後查這段時間的報告會查不出判定依據。
                </div>
                <div style={{ marginTop: 8, color: '#888', fontSize: 12 }}>
                  可以先確認右上角「設定DB」的連線狀態,再重試。
                </div>
              </div>),
              okText: '重試上傳', cancelText: '先不上傳',
              onOk: () => pushDefToDB(rep, pathForMsg, attempt + 1),
            });
          });
      };

      const commitSave = () => {
        ACT_DefFileHash_Update(report.featureSet_sha1);
        log.info("[action] report-save");
        ACT_Report_Save(CORE_ID, fileNamePath + '.' + DEF_EXTENSION, enc.encode(JSON.stringify(report, null, 2)));
        // The sidecar is also rewritten when the picture has been RE-TAKEN onto
        // an existing name. Skipping it there kept a <def>.png of the previous
        // part while the def described the new one -- and that file is what the
        // shape locator trains from, so the recipe would have gone on matching
        // the old part under the new name.
        const _retaken = !!edit_info.__img_fresh_capture;
        if (!existed || _retaken) {
          log.info("[action] cache-img-save", { existed, retaken: _retaken });
          ACT_Cache_Img_Save(CORE_ID, fileNamePath);
        }
        else { log.info("[action] cache-img-save skipped (def exists; keeping original image)"); }
        // The real sidecar now exists under the def's own name, so the scratch
        // one is no longer the thing to point at.
        dispatch(DefConfAct.EditInfo_Patch({
          __tmp_ref_image_path: undefined, __img_fresh_capture: false }));
        ACT_Def_Model_Path_Update(fileNamePath);
        pushDefToDB(report, fileNamePath);
      };
      setFileSavingCallBack(undefined);

      // Conflict check before overwriting the def THIS SESSION LOADED: if the
      // on-disk featureSet_sha1 no longer matches the hash recorded at load
      // (edit_info.DefFileHash), someone else -- another browser, a hand
      // edit, a core-side migration -- saved it since. Silently overwriting
      // resurrects this session's stale view of every field it never edited
      // (defFileGeneration spreads loadedDefFile wholesale). Surface it and
      // let the operator decide. LD {filename} is a pure file read (FL
      // reply), no core-state side effects.
      //
      // Scope: only when the target IS the loaded def's path. Overwriting a
      // DIFFERENT existing file is the file picker's own confirm; our
      // load-time hash says nothing about that file.
      // SAVING INTO THE SHARED FOLDER IS A SAVE ON EVERY MACHINE.
      //
      // Every def lives in a Resilio-synced folder shared by the fleet, so this
      // write lands on all of them within a sync window. That is usually not
      // what somebody adjusting one machine's threshold means to do, and today
      // nothing anywhere says it is happening.
      //
      // This CONFIRMS, it does not block. Blocking needs somewhere else to save
      // -- the machine runs the def straight out of the share, so refusing the
      // write refuses the adjustment as well. That is stage 1 (a local
      // workspace and a deliberate publish); until it exists, turning an
      // accident into a decision is the whole of what can be done honestly.
      const shareRoot = machine_custom_setting && machine_custom_setting.def_share_root;
      const underShare = !!(shareRoot && pathIsUnder(fileNamePath, shareRoot));
      const proceed = () => {
        const sameFile2 = existed && edit_info.defModelPath === fileNamePath;
        if (!sameFile2 || edit_info.DefFileHash === undefined) { commitSave(); return; }
        checkHashThenSave();
      };
      // A DEF MUST NOT LEAVE THE STUDIO UNABLE TO LOCATE WITH ITS OWN LOCATOR.
      //
      // If the registration or the ROI points changed, the trained features no
      // longer match and the core will refuse them -- the def then silently
      // falls back to sig360, which on a good image still locates, so nothing
      // looks wrong until it does. This is the last moment it can be fixed, so
      // it is asked here rather than left to be discovered on a line.
      //
      // Three ways out, and the third is deliberately available: an operator
      // who knows the def is being handed off mid-edit should not be trapped.
      // Only for the shape locator. sig360 finds its own object frame from the
      // silhouette and needs no registration line, so asking there is asking
      // about something the def does not have and does not want.
      const isShapeEngine = edit_info.locating_engine === 'shape_based';
      // A NEW OBJECT WITHOUT A REGISTRATION IS NOT A HALF-FINISHED DEF, IT IS A
      // WRONG ONE.
      //
      // Every measurement in the def is authored in object-frame mm, i.e.
      // relative to def_image_reg. Save it unset and the whole feature set is
      // pinned to a default origin that has nothing to do with the part -- and
      // it still inspects, because sig360 locates on the silhouette, so the
      // numbers come out plausible and wrong.
      //
      // Only for a capture that has never been saved (__img_fresh_capture): an
      // OLD def legitimately may not carry one, and refusing to save those would
      // block editing a recipe that has been running for months.
      //
      // Asked, not refused: the third button exists because a modal with no way
      // out is worse than the state it protects against, and someone handing a
      // def over mid-edit has a real reason to write it as-is.
      if (isShapeEngine && edit_info.__img_fresh_capture && !edit_info.def_image_reg) {
        Modal.confirm({
          title: '這個新物件還沒設定定位',
          width: 520,
          content: '量測全部是相對於定位原點和 0° 軸寫下來的,現在還沒有。'
                 + '這樣存下去,特徵和量測會釘在一個跟零件無關的原點上——'
                 + '而且它照樣檢驗得出數字,看起來不會有錯。',
          okText: '去設定定位',
          cancelText: '仍要存檔',
          onOk: () => { openSBM2(true); },
          onCancel: () => { log.warn('[save] new capture saved with NO def_image_reg'); proceed(); },
        });
        return;
      }

      const staleWhy = edit_info.__shape_stale;
      const lastGood = edit_info.__shape_lastGood;
      if (isShapeEngine && staleWhy) {
        Modal.confirm({
          title: '定位設定尚未生效',
          width: 520,
          content: '改了' + (staleWhy === 'def_image_reg' ? '定位'
                          : staleWhy === 'roi_refine_points' ? 'ROI 點' : '定位和 ROI 點')
                 + ',但特徵還是上一次生成的。存檔沒問題,SBM 定位也照常運作——'
                 + '只是它會用舊的定位原點,新的設定要按「生成特徵點」才會生效。',
          okText: '還原上一版定位設定',
          cancelText: '仍要存檔',
          // Restoring all three together makes the cache valid again by
          // construction: it is exactly the settings it was trained against.
          onOk: () => {
            dispatch(DefConfAct.EditInfo_Patch({
              def_image_reg: lastGood && lastGood.def_image_reg,
              roi_refine_points: lastGood && lastGood.roi_refine_points,
              __shape_cache: lastGood && lastGood.cache,
              __shape_stale: undefined, __shape_lastGood: undefined,
            }));
            Modal.info({ title: '已還原', content:
              '定位設定與特徵都回到上一個一致的版本。請重新存檔。' });
          },
          onCancel: () => { log.warn('[save] saving with features older than the registration'); proceed(); },
        });
        return;
      }

      if (underShare) {
        const n = machine_custom_setting.def_share_machines;
        Modal.confirm({
          title: '這個資料夾是共享的',
          content: n ? `存檔會同步到 ${n} 台機器,不只這一台。確定要存嗎?`
                     : '存檔會同步到共享資料夾的所有機器,不只這一台。確定要存嗎?',
          okText: '存檔', cancelText: '取消',
          onOk: proceed,
        });
        return;
      }
      proceed();

      function checkHashThenSave() {
      const sameFile = existed && edit_info.defModelPath === fileNamePath;
      if (!sameFile || edit_info.DefFileHash === undefined) { commitSave(); return; }
      ACT_WS_SEND_BPG(CORE_ID, "LD", 0, { filename: fileNamePath + '.' + DEF_EXTENSION }, undefined, {
        resolve: (pkts) => {
          const fl = (pkts || []).find(p => p.type == "FL");
          const onDiskSha1 = fl && fl.data && fl.data.featureSet_sha1;
          // Legacy def without a sha1, or unreadable reply: nothing to compare
          // against -- keep the old behaviour rather than block every save.
          if (onDiskSha1 === undefined || onDiskSha1 === edit_info.DefFileHash) { commitSave(); return; }
          log.warn("[save-conflict] on-disk def changed since load", { onDiskSha1, loadedHash: edit_info.DefFileHash });
          setModal_view({
            title: dictLookUp("WARNING", DICT),
            view: "此定義檔在載入後已被其他人修改（sha1 不符）。覆蓋會丟失對方的變更；建議先取消、重新載入檢視。",
            okText: "仍要覆蓋",
            onOk: () => { commitSave(); setModal_view(undefined); },
            onCancel: () => { log.info("[save-conflict] save cancelled"); setModal_view(undefined); },
          });
        },
        reject: () => {
          // Cannot read the on-disk file (deleted since load?). The save
          // recreates it; nothing to conflict with.
          log.warn("[save-conflict] could not re-read on-disk def; saving");
          commitSave();
        },
      });
      }
    });
  };

  const edit_info = useSelector(state => state.UIData.edit_info);
  const FILE_default_camera_setting = useSelector(state => state.UIData.FILE_default_camera_setting);
  const defConf_lock_level = useSelector(state => state.UIData.defConf_lock_level);
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const DefFile_DB_W_ID = useSelector(state => state.ConnInfo.DefFile_DB_W_ID);
  const DefFile_DB_SEND= (data,return_cb) => dispatch(UIAct.EV_WS_SEND_PLAIN(DefFile_DB_W_ID,data,return_cb));
  const shape_list = useSelector(state => state.UIData.edit_info._obj.shapeList);
  const defModelPath = edit_info.defModelPath;
  const machine_custom_setting = useSelector(state => state.UIData.machine_custom_setting);

  // The station, in full-sensor pixels, for drawing. Same two keys the core
  // reads out of machine_setting.json -- not the def's, and not converted:
  // the station is mechanics and lives in sensor pixels at every step.
  const stationOverlay = React.useMemo(() => {
    const ms = machine_custom_setting || {};
    const r = ms.inspection_region;
    const cl = Array.isArray(ms.clean_regions) ? ms.clean_regions : [];
    if (!(r && r.w > 0 && r.h > 0) && !cl.length) return undefined;
    return { region: (r && r.w > 0 && r.h > 0) ? r : undefined, clean: cl };
  }, [machine_custom_setting]);

  const [fileSelectedCallBack,setFileSelectedCallBack]=useState(undefined);
  // Where that browser opens and what it will show. The def picker is the
  // default because it was the only caller; 載入 xrep points it at the snapshot
  // folder with an .xreps filter instead. Both go through ONE browser -- two
  // would be two states that can both be open.
  const [fileSelectCfg,setFileSelectCfg]=useState(undefined);


  const [modal_view,setModal_view]=useState(undefined);

  const [cacheDef,setCacheDef]=useState(undefined);
  const [nowInspdata,setNowInspdata]=useState(undefined);
  // 快速驗證 only: is the machine's station filter (inspection_region +
  // clean_regions) being enforced for this session? Default ENFORCED, so the
  // quick check shows what production shows unless somebody says otherwise.
  const [stationEnforced,setStationEnforced]=useState(true);
  // NOTE: orientation auto-inspect + multi-image switching moved to the
  // persistent <DefConfImageSwitcher/> (rendered by APP_DEFCONF_MODE) so the
  // floating switcher + orientation survive across edit submodes (this neutral
  // menu component unmounts when you enter a shape-edit menu).

  let MenuSet= [
    <BASE_COM.IconButton
      iconType={<ArrowLeftOutlined/>}
      dict={DICT}
      key="<"
      addClass="layout black vbox"
      onClick={() =>{
        let defFile_New=defFileGeneration(edit_info);
        if(defFile_New.featureSet_sha1===edit_info.DefFileHash)
        {
          ACT_EXIT();
        }
        else
        {
          // SAY WHICH FIELDS MOVED.
          //
          // "變更尚未儲存" with nothing behind it is unfalsifiable: someone who
          // opened the def and touched nothing cannot tell a real edit from the
          // round-trip failing to reproduce the saved featureSet -- and both
          // happen. The hash is over featureSet, so the diff is too.
          const _loaded = GetObjElement(edit_info, ["loadedDefFile", "featureSet", 0]) || {};
          const _now = GetObjElement(defFile_New, ["featureSet", 0]) || {};
          const _skip = { __decorator: 1 };   // per-session UI state, not hashed
          const _same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
          const _keys = [...new Set([...Object.keys(_loaded), ...Object.keys(_now)])]
            .filter((k) => !_skip[k] && !_same(_loaded[k], _now[k]));
          // String() around the stringify, because JSON.stringify(undefined)
          // returns undefined -- not "undefined" -- and .slice() on that throws.
          // A key that exists on one side and not the other is the COMMONEST
          // case here (that is what a field being added or moved looks like), so
          // this threw on nearly every dirty exit. The throw came out of the
          // button's onClick, so the back button simply did nothing: no dialog,
          // no error on screen, no way to leave the page.
          const _cut = (v) => String(JSON.stringify(v)).slice(0, 120);
          if (_keys.length) log.warn("[exit-dirty] featureSet fields differ", _keys.map((k) => ({
            key: k, was: _cut(_loaded[k]), now: _cut(_now[k]),
          })));
          else log.warn("[exit-dirty] hash differs but no featureSet field does", {
            loadedHash: edit_info.DefFileHash, nowHash: defFile_New.featureSet_sha1 });
          setModal_view({
            onOk: () => {
              ACT_EXIT();
              
              setModal_view(undefined);
            },
            onCancel: () => { console.log("onCancel");setModal_view(undefined); },
            title: dictLookUp("WARNING", DICT),
            view: <>
              {DICT.defConf.exit_warning_change_is_made}
              <div style={{ marginTop: 10, fontSize: 12, color: '#888', lineHeight: 1.8 }}>
                {_keys.length
                  ? <>變更的欄位:{_keys.map((k) => <code key={k} style={{ marginRight: 6 }}>{k}</code>)}</>
                  : '雜湊不同,但沒有任何欄位不同 —— 這是存檔與重新載入之間的不一致,不是你改了什麼。'}
              </div>
            </>
          })
        }
      }} />,

    <BASE_COM.JsonEditBlock object={{ DefFileName: edit_info.DefFileName }}
      dict={DICT}
      key="this.props.edit_info.DefFileName"
      jsonChange={(original_obj, target, type, evt) => {
        ACT_DefFileName_Update(evt.target.value);
      }}
      whiteListKey={{ DefFileName: "input", }} />,

    <BASE_COM.JsonEditBlock object={{ DefFileTag: edit_info.DefFileTag.join(",") }}
      dict={DICT}
      key="this.props.edit_info.DefFileTag"
      jsonChange={(original_obj, target, type, evt) => {
        ACT_DefFileTag_Update(evt.target.value.split(","));
      }}
      whiteListKey={{ DefFileTag: "input", }} />
    ]
  if(defConf_lock_level==0)
  MenuSet=MenuSet.concat(
    [
    <BASE_COM.IconButton
      dict={DICT}
      addClass="layout vbox  btn-swipe"
      style={{backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.line]}}
      key="LINE"
      text="line" onClick={() => ACT_Line_Add_Mode()} />,
    <BASE_COM.IconButton
      dict={DICT}
      addClass="layout palatte-blue-8 vbox  btn-swipe"
      style={{backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.arc]}}
      key="ARC"
      text="arc" onClick={() => ACT_Arc_Add_Mode()} />,
    <BASE_COM.IconButton
      dict={DICT}
      addClass="layout palatte-blue-8 vbox  btn-swipe"
      style={{backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.aux_point]}}
      key="APOINT"
      text="apoint" onClick={() =>  ACT_Aux_Point_Add_Mode()} />,

    <BASE_COM.IconButton
      dict={DICT}
      addClass="layout palatte-blue-8 vbox  btn-swipe"
      style={{backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.search_point]}}
      key="SPOINT"
      text="spoint" onClick={() => ACT_Search_Point_Add_Mode()} />,
    <BASE_COM.IconButton
      //iconType={<FormOutlined/>}
      addClass="layout palatte-blue-8  btn-swipe"
      key="MEASURE"
      style={{backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.measure]}}
      dict={DICT}
      text="measure"
      onClick={() => ACT_Measure_Add_Mode()}>
    </BASE_COM.IconButton>,
    // 物件偵測 (obj_detect) is no longer offered.
    //
    // The check it was for -- "this space must be clean" -- is done at MACHINE
    // level by clean_regions in machine_setting.json (eval_clean_regions),
    // which is where it belongs: it describes the station, not the product, so
    // it does not want re-drawing per def. The def-level feature duplicated
    // that against the def's own frame, and its own design doc records the
    // half that was never finished ("P2 ... OBJ_DETECT is declared in the
    // FEATURETYPE enum and has no users").
    //
    // Only the CREATE path is gone. Drawing, editing and deleting stay: a def
    // in the field may still carry obj_detect regions, and those fold into the
    // part verdict (UINSP_VERDICT_PATH 3.5). Removing the shape module too
    // would leave them invisible on the canvas while still condemning parts,
    // which is worse than leaving them visible and removable.
    ]);
      

  // VERIFY AGAINST A SAVED RECORD, with the calibration it was taken under.
  //
  // The image switcher already loads a sibling .png into the core's cache, and
  // that is enough to LOOK at a def on another sample. It is not enough to
  // measure one: a bare image carries no mm-per-pixel, so the frame gets
  // interpreted with whatever calibration the editor happens to be holding --
  // which, for a picture taken on another machine or before a re-calibration,
  // is the wrong ruler and produces numbers that look entirely ordinary.
  //
  // A .xreps record is the pair: the report AND the camera_param of the frame
  // it was taken from. So this is the same LD the playback screen sends
  // (filename + imgsrc), and the camera_param that comes back goes into the
  // editor through SetCameraParamInfo -- the same entry point an inspection
  // report uses.
  //
  // It does NOT touch the def: since cam_param generation takes the file's
  // values first, looking at a record cannot rewrite the recipe's calibration.
  // That mattered enough to be a separate fix; without it, opening a record
  // here would quietly re-scale the def to the record's camera.
  function loadXrepForVerify(xrepPath, fileInfo)
  {
    const stem = String(xrepPath).replace(/\.xreps$/i, "");
    const slash = Math.max(stem.lastIndexOf('/'), stem.lastIndexOf('\\'));
    const dir = slash >= 0 ? stem.substring(0, slash) : '.';
    const base = slash >= 0 ? stem.substring(slash + 1) : stem;

    // The picture's EXTENSION has to be looked up, not guessed: the core's
    // automatic NG snapshots write .jpg and the manual 檢測快照 writes .png,
    // and the core hands imgsrc straight to cv::imread without appending
    // anything -- so a guess that is wrong loads the report with no image and
    // the overlay draws over a blank canvas.
    const IMG_EXTS = ["png", "jpg", "jpeg", "bmp"];
    ACT_WS_SEND_BPG(CORE_ID, 'FB', 0, { path: dir, depth: 1 }, undefined, {
      resolve: (darr) => {
        const fsInfo = darr && darr[0] && darr[0].data;
        const files = (fsInfo && fsInfo.files) || [];
        const fdir = (fsInfo && fsInfo.path) || dir;
        const hit = files.find((f) => f && f.type === 'REG' && typeof f.name === 'string'
          && IMG_EXTS.some((e) => f.name.toLowerCase() === (base + '.' + e).toLowerCase()));
        const imgPath = hit ? (hit.path || (fdir + '/' + hit.name)) : undefined;
        if (!imgPath) log.warn('[xrep] no image beside ' + stem + ' -- report only');
        sendXrepLD(stem, imgPath);
      },
      reject: (e) => { log.warn('[xrep] folder listing failed', e); sendXrepLD(stem, undefined); }
    });
  }

  function sendXrepLD(stem, imgPath)
  {
    ACT_WS_SEND_BPG(CORE_ID, 'LD', 0,
      { filename: stem + '.xreps',
        ...(imgPath ? { imgsrc: imgPath } : {}),
        down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL },
      undefined,
      { resolve: (pkts) => {
          const FL = (pkts || []).find((p) => p.type === 'FL');
          const IM = (pkts || []).find((p) => p.type === 'IM');

          // The image first, so the canvas is showing the frame the numbers
          // will be about. IGNORE_DEFCONF_LOCK because the post-load display
          // lock drops image actions and this one is a deliberate operator act.
          if (IM !== undefined) {
            const a = BPG_Protocol.map_BPG_Packet2Act(IM);
            if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
            else log.error('[xrep] an IM packet produced no action');
          } else {
            log.warn('[xrep] LD returned no IM; the canvas keeps the previous image');
          }

          const camParam = FL && FL.data && FL.data.camera_param;
          if (camParam !== undefined && edit_info && edit_info._obj) {
            edit_info._obj.SetCameraParamInfo(camParam);
            log.info('[xrep] camera_param adopted from the record', camParam);
          } else {
            // Worth saying out loud rather than silently measuring with the
            // editor's current ruler: an old record may predate the field.
            log.warn('[xrep] the record carries no camera_param -- '
                   + 'the frame will be measured with the calibration already loaded');
          }

          setNowInspdata(undefined);   // any previous result was another frame
          setTimeout(() => runXrepVerify(camParam), 60);
        },
        reject: (e) => { log.warn('[xrep] load failed', e); }
      });
  }

  // Measure the loaded frame with the CURRENT def and the FRAME's ruler.
  //
  // This does not reuse the shared 'defconf-orient-now' event, and the reason is
  // the whole feature: that path sends calibInfo.mmpp from the DEF, so the
  // camera_param just adopted from the record would be carried around and never
  // actually used -- the frame would be measured with the def's scale, which is
  // the thing loading a record was supposed to avoid. mm-per-pixel is a property
  // of how a frame was captured, so it comes from the record.
  //
  // What comes from the def is everything else: the features, the tolerances,
  // the regions. That is the point -- this verifies the def you have open
  // against a saved frame, and it deliberately ignores the defInfo stored in the
  // record (FL.data.defInfo), which describes the def as it was back then.
  function runXrepVerify(camParam)
  {
    if (!edit_info || !edit_info._obj) return;
    let deffile = defFileGeneration(edit_info);
    stampRefImagePath(deffile, edit_info);
    const defMmpp = deffile.featureSet[0].mmpp;
    // mmpb2b/ppb2b is how the rest of the code turns a cam_param into a scale
    // (see the sig360 report path). Guarded: a zero or missing ppb2b would make
    // this Infinity or NaN and measure the part to a nonsense scale rather than
    // failing, so fall back to the def and say which one was used.
    // The SOURCE is tracked, not inferred from the value. A bench whose record
    // was taken under the same calibration as the def produces two identical
    // numbers, and a label derived by comparing them then reports "def" for a
    // scale that came from the record -- which is exactly the question this
    // line exists to answer.
    let mmpp = defMmpp, mmppFrom = 'def';
    if (camParam && camParam.mmpb2b > 0 && camParam.ppb2b > 0) {
      mmpp = camParam.mmpb2b / camParam.ppb2b;
      mmppFrom = 'record';
    } else {
      log.warn('[xrep] no usable mmpb2b/ppb2b in the record -- measuring with the def mmpp', defMmpp);
    }
    log.info('[xrep] verifying with mmpp=' + mmpp + ' (' + mmppFrom + ')');

    ACT_WS_SEND_BPG(CORE_ID, 'II', 0,
      { definfo: deffile, imgsrc: '__CACHE_IMG__',
        img_property: { calibInfo: { type: 'disable', mmpp: mmpp } } },
      undefined,
      { resolve: (darr) => {
          const RP = (darr || []).find((p) => p.type === 'RP');
          if (RP !== undefined) {
            const a = BPG_Protocol.map_BPG_Packet2Act(RP);
            if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
          }
          const IM = (darr || []).find((p) => p.type === 'IM');
          if (IM !== undefined) {
            const a = BPG_Protocol.map_BPG_Packet2Act(IM);
            if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
          }
          // AFTER the dispatch, deliberately. Handling a sig360_circle_line
          // report calls SetCameraParamInfo with the REPORT's cam_param
          // (UICtrlReducer), which is the def's -- so dispatching the result of
          // this verification silently undoes the adoption that made it
          // meaningful. Measured: exposure_time:50 went in and came back gone.
          if (camParam !== undefined) edit_info._obj.SetCameraParamInfo(camParam);
        },
        reject: (e) => { log.warn('[xrep] verify failed', e); }
      });
  }

  function startQuickInsp(inspMode=machine_custom_setting.InspectionMode||"CI")
  {//FI/CI


    let _CameraCtrl = new CameraCtrl({
      ws_ch: (STData, promiseCBs) => {
        ACT_WS_SEND_BPG(CORE_ID, "ST", 0, STData, undefined,promiseCBs);
      },
      ev_frameRateChange: (fps) => {
      }
    });
    // One definition, shared with InspectionUI. These were 8 here and 10 there,
    // and nobody had chosen either number.
    applyInspFrameRate(_CameraCtrl, inspMode);





    let deffile = defFileGeneration(edit_info);
    stampRefImagePath(deffile, edit_info);   // shape locator: ref-image path for the core
    setCacheDef(deffile);

    // The station filter, stated per session rather than inherited.
    //
    // InspAreaBypass turns off BOTH machine-level area gates -- the station
    // inspection_region and the clean_regions -- for the life of the process,
    // and the core logs an ERROR on any session that starts with it latched:
    // "this is the way it ends up on in production". So it is sent explicitly
    // every time, both ways, and cleared unconditionally when the session ends
    // rather than only when it was turned on here.
    ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { InspAreaBypass: !stationEnforced });

    let _PGID_=11004;
    ACT_WS_SEND_BPG(CORE_ID, inspMode, 0, 
    { _PGID_: _PGID_, 
      _PGINFO_: { keep: true }, 
      definfo: deffile     
    }, undefined,{
      resolve:(pkts,mainFlow)=>{
        // console.log(pkts);

        // nowInspdata

        let RP=pkts.find(pkt=>pkt.type=="RP");
        let IM=pkts.find(pkt=>pkt.type=="IM");
        
        let reports = GetObjElement(RP,["data","reports",0,"reports"]);
        
        

        // let root_MarginInfo=edit_info._obj.shapeList;
        // rep.reports.forEach(rep=>{
        //   rep.judgeReports.forEach(jdg=>{
        //     jdg.
        //   })
        // })
        // console.log(rep.reports);
        let image = undefined;
        if(IM!==undefined)
          image=BPG_Protocol.map_BPG_Packet2Act(IM).data;

        
        setNowInspdata({
          cam_param:edit_info._obj.cameraParam,
          reports:reports,
          image:image,
        });
      },
      reject:(e)=>{
      }
    });

    function CancelNowInsp()
    {
      // Unconditional: a bypass that outlives the screen that set it is a
      // machine that has silently stopped enforcing its station.
      ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { InspAreaBypass: false });
      // keep:false, and no definfo.
      //
      // This was keep:true with definfo:undefined, which does not stop
      // anything: keep:true is the flag that PRESERVES the subscription,
      // and a CI carrying neither deffile nor definfo is rejected outright
      // ("nothing to inspect against"), so the packet never reached the
      // group logic at all. Measured on the bench: 6.0 fps before the
      // cancel, 6.0 fps after it, 0.0 fps after a real keep:false.
      //
      // The stream therefore outlived the screen and ran until the core
      // process exited, and anything opened afterwards added its own on
      // top -- which is what a rising image rate that nobody configured
      // looks like.
      ACT_WS_SEND_BPG(CORE_ID, "CI", 0,
        { _PGID_: _PGID_, _PGINFO_: { keep: false } });
      // And stop the camera, the way InspectionUI does on its way out. Leaving
      // it in free run keeps frames flowing into the pipeline for whatever
      // subscribes next.
      ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { CameraSetting: { trigger_mode: 1 } });

    }

    // Save the frame currently streaming in this 立即測試 modal as an ALTERNATE
    // sibling image for the def. Use __LAST_DATA_VIEW_CACHE_IMG__ (the last
    // INSPECTED frame = the live streamed image), NOT __CACHE_IMG__ -- the latter
    // holds the def image loaded on DefConf entry (the original <def>.png) and is
    // NOT updated by the CI/FI stream, so it saved a duplicate of <def>.png. We
    // name it "<defModelPath>_<stamp>" (defModelPath is the extension-less base path),
    // so the core writes "<base>_<stamp>.png", which DefConfImageSwitcher
    // discovers as a sibling (filter: name.indexOf(base)===0 && /\.(png|jpe?g|bmp)$/).
    // On success we fire a window event so the (separate, persistent) switcher
    // re-scans and the new image appears in its dropdown immediately.
    function saveAlternateImage()
    {
      const base = edit_info.defModelPath;
      if (!CORE_ID || !base) return;
      const dt = new Date();
      const p2 = (n) => ("0" + n).slice(-2);
      const stamp = `${dt.getFullYear()}${p2(dt.getMonth()+1)}${p2(dt.getDate())}_${p2(dt.getHours())}${p2(dt.getMinutes())}${p2(dt.getSeconds())}`;
      const filename = `${base}_${stamp}`;
      ACT_WS_SEND_BPG(CORE_ID, "SV", 0,
        { filename, type: "__LAST_DATA_VIEW_CACHE_IMG__" }, undefined,
        {
          resolve: (darr) => {
            try { window.dispatchEvent(new CustomEvent('defconf-images-changed')); } catch (e) {}
          },
          reject: (e) => { log.info("[save-alt-image] failed", e); }
        });
    }


    setModal_view({
      onOk: () => {
        CancelNowInsp()
        setModal_view(undefined);
      },
      onCancel: () => {
        CancelNowInsp()
        setModal_view(undefined);
      },

      height:"80%",
      width:"95%",
      style:{top:"30px"},

      className:"modal-sizing size95",
      footer:<>
          <span style={{ float:'left', display:'flex', alignItems:'center', gap:8 }}>
            <Switch size="small" checked={stationEnforced}
              onChange={(v)=>{ setStationEnforced(v);
                ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { InspAreaBypass: !v }); }} />
            <span style={{ fontSize:12, color: stationEnforced ? '#8b929c' : '#d4380d',
                           fontWeight: stationEnforced ? 400 : 700 }}>
              {stationEnforced
                ? '站點範圍過濾:啟用(和生產一致)'
                : '⚠ 站點範圍過濾已關閉 —— 站點外的物件也會被判定,clean_regions 也沒在檢查'}
            </span>
          </span>
          <Button key="save-alt" type="primary" onClick={saveAlternateImage}>儲存為替代影像</Button>
          <Button key="close" danger onClick={()=>{ CancelNowInsp(); setModal_view(undefined); }}>關閉</Button>
        </>,
      title: null,
      ext_sec:"INST_Inspection"
    })



  }



  MenuSet=MenuSet.concat([
    <BASE_COM.IconButton
      iconType={<EditOutlined/>}
      dict={DICT}
      addClass="layout palatte-blue-5 vbox "
      key="EDIT"
      text="edit" onClick={() => ACT_Shape_Edit_Mode()} />,
    (defConf_lock_level > 2) ? null :
    <BASE_COM.IconButton
      iconType={<SaveOutlined/>}
      dict={DICT}
      addClass="layout palatte-gold-7 vbox"
      key="SAVE"
      text="save" onClick={() => triggerSave()} />,
    <BASE_COM.IconButton
      iconType={<ExportOutlined/>}
      dict={DICT}
      addClass="layout palatte-gold-7 vbox"
      key="LOAD"
      text="load" onClick={() => {
        setFileSelectedCallBack(()=>(filePath) => {
          let fileNamePath = filePath.replace("." + DEF_EXTENSION, "");

          loadDefFile(fileNamePath,ACT_DefConf_Lock_Level_Update,ACT_WS_SEND_BPG,CORE_ID,dispatch);
          ACT_Def_Model_Path_Update(fileNamePath);
          setFileSelectedCallBack(undefined);
        })

      }} />,
    (defConf_lock_level !=0) ? null :
    <BASE_COM.IconButton
      dict={DICT}
      iconType={<SettingOutlined/>}
      addClass="layout palatte-grey-8 vbox"
      key="setting"
      text="setting" onClick={() => setModal_view({
          title: DICT.defConf.setup,
          view: <SettingUI></SettingUI>
      })} />,
    <BASE_COM.IconButton
      iconType={<CameraOutlined/>}
      dict={DICT}
      addClass="layout palatte-purple-8 vbox"
      data-testid="take"
      key="TAKE"
      text="take" onClick={() => {

        // OPENING THIS DIALOG DISCARDS UNSAVED EDITS. That is the contract, not
        // an accident, and cancelling does not exempt you: cancel reloads the
        // def from disk.
        //
        // Building a take on top of unsaved work would mean deciding, for every
        // path through here, which half of the editor survives -- and that is a
        // set of states nobody would enumerate correctly. One rule instead:
        // starting a new object starts from the last saved state.
        //
        // Same dirtiness test as the back button (featureSet_sha1 vs the hash
        // the def was loaded with), deliberately: two different answers to "is
        // this dirty" is worse than either answer.
        const _now = defFileGeneration(edit_info);
        const _dirty = _now.featureSet_sha1 !== edit_info.DefFileHash;
        const _openTake = () => {
          // CLEAR THE POST-LOAD DISPLAY LOCK, ONCE, BEFORE THE DIALOG EXISTS.
          //
          // While it is non-zero the reducer drops DefConf actions that are not
          // on a three-entry whitelist -- including IMAGE actions, which is why
          // useDefImages tags its own with IGNORE_DEFCONF_LOCK. A viewfinder
          // streaming into that reducer throws every frame away and keeps
          // showing the picture it opened with, and Def_Retake at the end is
          // discarded the same way.
          //
          // It happens HERE and not in startStream because changing the lock
          // re-renders this menu, which remounts the component whose state
          // holds the modal -- so clearing it mid-stream closes the dialog. The
          // door is the one moment where a remount costs nothing.
          ACT_DefConf_Lock_Level_Update(0);
        // One fixed name, deliberately. A per-capture name would leave a file
        // behind for every retake in data/, and nothing would ever delete them;
        // reusing one means the previous scratch frame is overwritten by the
        // next capture, which is exactly the lifetime this needs. The leading
        // underscores keep it out of the way of the recipe names beside it.
        const TMP_REF_BASE = "data/__retake_ref";
        const triggerTimeout = 10000;
        // Its own group id, so stopping this stream cannot cancel somebody
        // else's subscription (快速驗證 uses 11004, calibration 10105).
        const TAKE_STREAM_PGID = 11007;

        // A NEW OBJECT MUST NOT BE ABLE TO OVERWRITE THE DEF IT CAME FROM.
        //
        // After this the def is a different part, so the save dialog must not
        // open pre-filled with the previous recipe's file name. Same folder --
        // that is where its siblings live -- new name, and a [N] suffix if
        // something with that name is already there, picked by actually listing
        // the folder rather than by hoping.
        //
        // Best effort on purpose: a failed listing falls back to the plain name
        // and the save browser's own exists-prompt. A listing must not be able
        // to block a capture.
        const claimNewDefPath = (name) => new Promise((resolve) => {
          const dir = defModelPath.substr(0, defModelPath.lastIndexOf('/') + 1) || 'data/';
          const done = (taken) => resolve(dir + nextFreeName(name, taken));
          try {
            ACT_WS_SEND_BPG(CORE_ID, 'FB', 0, { path: dir, depth: 1 }, undefined, {
              resolve: (darr) => {
                let taken = new Set();
                try {
                  const files = [];
                  for (const pkt of (darr || [])) {
                    const d = pkt && pkt.data;
                    const list = d && (d.files || d.list || d.content);
                    if (list) files.push(...list);
                  }
                  taken = takenNamesFrom(files);
                } catch (e) { log.warn('[take] folder listing unreadable', e); }
                done(taken);
              },
              reject: () => done(new Set()),
            });
          } catch (e) { done(new Set()); }
        });

        // The machine's own mm/px, from the file lens calibration writes.
        // um_per_px is what it produces; m (px/mm) is the same number inverted
        // and is kept as a fallback for older files -- both straight out of
        // CalibrationUI's loadInstMmpp, which is the authority for this number.
        const loadInstrumentMmpp = () => new Promise((resolve) => {
          try {
            ACT_WS_SEND_BPG(CORE_ID, "LD", 0, { filename: "data/lens_calib.json" },
              undefined, {
                resolve: (pkts) => {
                  const fl = (pkts || []).find(p => p.type === "FL");
                  resolve(mmppFromLensCalib(fl && fl.data));
                },
                reject: () => resolve(undefined),
              });
          } catch (e) { resolve(undefined); }
        });

        // LIVE PREVIEW, WITHOUT RUNNING AN INSPECTION.
        //
        // The core streams frames only while a CI subscription is open, and CI
        // rejects a request carrying neither deffile nor definfo. Sending the
        // real def would work but would run the measurement engine on a part
        // that has no features yet -- a screenful of NA over the picture the
        // operator is trying to judge. stage_light_report is the lightweight def
        // type CalibrationUI already streams with: raw frames, no measurement.
        //
        // Frames arrive through the normal pipeline into edit_info.img, which is
        // ALSO where the def's own image lives. That is why cancelling has to put
        // the def image back -- see restoreDefImage.
        const startStream = () => {
          ACT_WS_SEND_BPG(CORE_ID, "ST", 0,
            { CameraSetting: { trigger_mode: 0, down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL } });
          ACT_WS_SEND_BPG(CORE_ID, "CI", 0, {
            _PGID_: TAKE_STREAM_PGID,
            _PGINFO_: { keep: true },
            definfo: { type: "stage_light_report", grid_size: [10, 10],
                       nonBG_thres: 100, nonBG_spread_thres: 180 },
            IMG_ignore_calib: true,
          });
        };
        // TWO things, and leaving out the second is why frames kept arriving
        // after 停止串流: cancelling the subscription stops the core PUSHING, but
        // the camera was put into free run by startStream and stays there,
        // producing frames into the pipeline for anything else that is looking.
        // CalibrationUI's cleanup does both; this did only the first.
        //
        // trigger_mode 1 is software trigger, i.e. the camera produces nothing
        // until asked -- the state a static editor wants, and the same one
        // calibration restores.
        const stopStream = () => {
          ACT_WS_SEND_BPG(CORE_ID, "CI", 0,
            { _PGID_: TAKE_STREAM_PGID, _PGINFO_: { keep: false } });
          ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { CameraSetting: { trigger_mode: 1 } });
        };

        // CANCEL RELOADS THE DEF. It does not try to put the editor back the way
        // it was.
        //
        // The contract for this whole dialog is deliberately blunt: opening it
        // means unsaved edits are gone, and cancelling returns to the last SAVED
        // state. The alternative -- restore the picture, keep the edits -- needs
        // the streamed frame swapped out without disturbing edit_info, and a
        // reply dispatched through only its IM packet the way switchImage does
        // it. That is a third state ("cancelled, but still dirty") that nothing
        // else in this screen has, and states nobody enumerated are where the
        // bugs live. One rule, two outcomes, no half-restored editor.
        //
        // Skipped when there is nothing to go back to: after a take that has
        // never been saved, defModelPath names a file that does not exist yet,
        // and a failed load would leave a wiped def under a streamed frame --
        // strictly worse than leaving the frame alone.
        const reloadSavedDef = () => {
          if (!defModelPath || edit_info.__img_fresh_capture) return;
          try {
            loadDefFile(defModelPath, ACT_DefConf_Lock_Level_Update,
                        ACT_WS_SEND_BPG, CORE_ID, dispatch);
          } catch (e) { log.warn('[take] could not reload the def', e); }
        };

        const closeTake = () => {
          stopStream();
          reloadSavedDef();
          setModal_view(undefined);
        };

        // Wait for a plate trigger and keep the single frame it returns. The
        // screen is frozen while it waits, which is what was asked for: this is
        // the existing EX path, one frame, no stream.
        const waitForTrigger = () => new Promise((resolve, reject) => {
          ACT_DefConf_Lock_Level_Update(0);
          ACT_WS_SEND_BPG(CORE_ID, "EX", 0, {
            trigger_type: 2, timeout: triggerTimeout,
            img_property: { down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL }
          }, undefined, {
            resolve: (pkts) => {
              const SS = pkts.find(pkt => pkt.type == "SS");
              if (SS && SS.data.ACK == true) {
                const acts = pkts.map(pkt => BPG_Protocol.map_BPG_Packet2Act(pkt))
                                 .filter(a => a !== undefined);
                dispatch({ type: "ATBundle", ActionThrottle_type: "express", data: acts });
                resolve();
              } else { message.error("沒有等到觸發訊號"); reject(); }
            },
            reject: (e) => { log.info(e); message.error("取像異常"); reject(); },
          });
        });

        // Everything the confirm path shares.
        //
        // Order matters and is not obvious: Def_Retake clears def-scoped keys, so
        // the name, the tags, the engine and the scratch template path are all
        // written AFTER it.
        const finishTake = (opt) => {
          stopStream();
          // CLEAR THE EDITOR LOCK FIRST, or none of this happens.
          //
          // While defConf_lock_level is non-zero the reducer drops every
          // DefConf action that is not on a three-entry whitelist -- silently,
          // by `break`ing out of the do-block before the switch. Def_Retake is
          // not on that list, so it was discarded and the "new object" kept the
          // previous def's registration, feature cache and name while its path
          // and engine changed around them. Nothing reported an error; the
          // studio simply opened with every step already green.
          //
          // Entering DefConf leaves the lock at 1 (the post-load display lock).
          // The original TAKE cleared it inside triggerSnapExam, so the
          // single-shot path worked and the stream and reuse-image paths, added
          // later, did not. Doing it here covers all three by construction.
          ACT_DefConf_Lock_Level_Update(0);
          // Anything still in flight belongs to the def we are leaving behind.
          // Without this the reply lands after the retake and puts it back.
          invalidateDefLoads();
          dispatch(DefConfAct.Def_Retake(!!opt.keep));
          ACT_Cache_Img_Save(CORE_ID, TMP_REF_BASE, opt.srcType);
          dispatch(DefConfAct.EditInfo_Patch({ __tmp_ref_image_path: TMP_REF_BASE + ".png" }));

          // PUT THE CAPTURED FRAME INTO THE CORE'S CACHE TOO.
          //
          // Everything downstream inspects __CACHE_IMG__: the studio's 跑一次檢驗
          // and its robustness sweep, the orientation re-inspect fired when the
          // studio closes, and therefore what the def canvas rectifies against.
          // A stream never updates that cache -- the captured frame only reached
          // the scratch sidecar -- so all of them were measuring the PREVIOUS
          // recipe's picture while the features came from the new part. On a
          // bench where the two look alike the scores stay high and only the
          // reported ORIENTATION gives it away: it is the old image's part, at
          // the old image's angle. The sweep cannot catch it either, because it
          // degrades and measures that same wrong image and stays perfectly
          // self-consistent.
          //
          // Dispatching ONLY the IM packet, the way useDefImages.switchImage
          // does. A fire-and-forget LD has no promiseCBs, so BPG_WS hands the
          // whole reply to WSDataDispatch -- including the sig360_extractor
          // report, whose reducer case calls Edit_info_reset and would wipe the
          // name, tags and engine set two lines above.
          ACT_WS_SEND_BPG(CORE_ID, "LD", 0,
            { imgsrc: TMP_REF_BASE + ".png", down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL },
            undefined, {
              resolve: (darr) => {
                const IM = (darr || []).find((p) => p.type === 'IM');
                if (!IM) { log.warn('[take] LD returned no IM; the core may not hold the new frame'); return; }
                const a = BPG_Protocol.map_BPG_Packet2Act(IM);
                if (a) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
              },
              reject: (e) => log.warn('[take] could not load the captured frame into the core', e),
            });
          dispatch(DefConfAct.DefFileName_Update(opt.name));
          dispatch(DefConfAct.DefFileTag_Update(opt.tags));
          // TAKE means "this is an SBM object". It is the one surface where that
          // is not a guess: the operator just said they are starting a new part
          // and picked the frame to build it from.
          dispatch(DefConfAct.Locating_Engine_Update('shape_based'));

          // SCALE FOLLOWS THE PICTURE'S OWNER.
          //
          // A camera frame is measured in the MACHINE's mm/px; the def's own
          // image is measured in the def's. Def_Retake does not clear
          // _obj.sig360info, and getEditorMmpp reads that first -- so without
          // this a new part captured here keeps the PREVIOUS def's scale, every
          // dimension comes out at a consistent wrong ratio, and nothing on
          // screen looks any different. The single-shot path happened to be
          // rescued by its own sig360 report; the stream, which runs
          // stage_light_report with IMG_ignore_calib, produces no report at all
          // and was silently wrong.
          //
          // Reusing the existing image deliberately does NOT do this: that
          // picture really does belong to the def's scale.
          if (opt.fromCamera) {
            loadInstrumentMmpp().then((mmpp) => {
              if (Number.isFinite(mmpp) && mmpp > 0) {
                dispatch(DefConfAct.Instrument_Mmpp_Set(mmpp));
                log.info('[take] instrument scale', { mmpp });
              } else {
                // Not silent. Without a lens calibration the def has no honest
                // scale, and a def that measures in the wrong unit is worse than
                // one that refuses to measure.
                log.warn('[take] no lens_calib.json -- scale falls back to the camera param');
                message.warning('讀不到鏡頭校正（data/lens_calib.json）,比例尺可能不正確');
              }
            });
          }

          claimNewDefPath(opt.name).then((newPath) => {
            ACT_Def_Model_Path_Update(newPath);
            setModal_view(undefined);
            // Deferred a tick so every dispatch above has landed; the studio
            // reads edit_info on mount and would otherwise see the old def.
            setTimeout(() => openSBM2(true), 0);
          });
        };

        setModal_view({
          title: "建立新物件",
          footer: null,
          width: "96vw",
          style: { top: 12 },
          bodyStyle: { padding: 10, height: "86vh" },
          onCancel: () => closeTake(),
          view: <TakeSetupDialog
            triggerTimeout={triggerTimeout}
            loadInstMmpp={loadInstrumentMmpp}
            onStreamStart={startStream}
            onStreamStop={stopStream}
            onWaitTrigger={waitForTrigger}
            onCancel={() => closeTake()}
            onGo={finishTake}
          />,
        });
        };

        if (!_dirty) { _openTake(); return; }
        Modal.confirm({
          title: '目前的變更還沒存檔',
          width: 520,
          content: '建立新物件會從「上次存檔的狀態」開始,目前編輯中還沒存的內容會消失。'
                 + '中途按「取消」也一樣 —— 取消是把 def 重新載入回上次存檔的樣子,'
                 + '不是回到你現在的編輯狀態。要保留的話,先回去存檔。',
          okText: '先回去存檔',
          cancelText: '丟掉變更,繼續建立新物件',
          onCancel: () => { log.warn('[take] discarding unsaved def edits'); _openTake(); },
        });
      }} />,
    (defConf_lock_level !=0) ? null :
    <BASE_COM.IconButton
      iconType={<VerticalAlignTopOutlined />}
      dict={DICT}
      addClass="layout palatte-purple-8 vbox"
      key="INST_CHECK"
      text="INST_CHECK" onClick={() => {
        let deffile = defFileGeneration(edit_info);
        stampRefImagePath(deffile, edit_info);   // shape locator: ref-image path for the core
        console.log("deffile",deffile);
            console.log("INST_CHECK");
        ACT_WS_SEND_BPG(CORE_ID,"II", 0, 
        {
          definfo:deffile,
          imgsrc:"__CACHE_IMG__",
          img_property:{
            calibInfo:{
              type:"disable",
              mmpp:deffile.featureSet[0].mmpp
            },
            //down_samp_level:1,
          }
        },undefined,
        {
          resolve:(darr,mainFlow)=>{
            let RP=darr.find(pkt=>pkt.type=="RP");

            if(RP!==undefined)
            {
              // Feed the inspection report into redux (sets edit_info.inspReport)
              // so the def-conf canvas can rectify the image to the object frame
              // and show cal_hits. INST_CHECK previously dispatched only the IM
              // packet, so inspReport (hence the canvas `single`) was never set.
              let rpAct = BPG_Protocol.map_BPG_Packet2Act(RP);
              if (rpAct !== undefined) dispatch(rpAct);

              let insp_reports = GetObjElement(RP,["data","reports",0,"reports"]);
              if(insp_reports!==undefined&&  insp_reports.length>0)
              {
                console.log(insp_reports);
                let insp_rep = insp_reports[0];
                let modList = shape_list.map((shape,idx)=>{
                  let mod_shape=dclone(shape);
                  
                  edit_info._obj.ShapeAdjustsWithInspectionResult(mod_shape,shape_list, insp_rep,true);
                  // A measurement that did not happen must not move the def.
                  mod_shape = InspectionEditorLogic.KeepDefGeometryIfNotMeasured(shape, mod_shape);

                  mod_shape=modShapeCleanUp(mod_shape);
                  if(mod_shape!==undefined)
                    return mod_shape;
                  else
                    return dclone(shape)
                });

                ACT_Shape_List_Update(modList);
                // dispatch(DefConfAct.Shape_List_Update([]))
                // modList.forEach((adj_shape,index)=>{
                //   // console.log(adj_shape,adj_shape.id);
                //   // setTimeout(()=>
                //   //   SetShape(adj_shape,adj_shape.id)
                //   // ,100*index)
                //   SetShape(adj_shape,adj_shape.id);
                // })
              }
            }
            
            {

              let IM=darr.find(pkt=>pkt.type=="IM");
              if(IM!==undefined)
              {
                let act = BPG_Protocol.map_BPG_Packet2Act(IM);
                if (act !== undefined)
                  dispatch(act);
              }
                
            }

          },
          reject:(e)=>{
          }
        }
        );
      }} />,

    
    // THE SBM SURFACE ONLY EXISTS FOR A DEF THAT USES THE SBM LOCATOR, AND
    // ONLY WHILE THE DEF IS EDITABLE.
    //
    // It used to be unconditional, and pressing it silently switched the def to
    // shape_based -- so a sig360 recipe could be converted by somebody who only
    // meant to look. Conversion has consequences (features must be re-trained,
    // the def re-saved), so it belongs to the one control that says so:
    // -> migrate to shape_based, in the localizer settings.
    //
    // The lock is the second half of the same thought. defConf_lock_level != 0
    // means the reducer drops DefConf actions that are not on a three-entry
    // whitelist -- silently -- so the studio would open, accept a registration
    // line, redraw itself as though it had taken it, and change nothing. An
    // editor that cannot edit should not be reachable at all.
    (edit_info.locating_engine !== 'shape_based' || defConf_lock_level != 0) ? null :
    <BASE_COM.IconButton
      iconType={<AimOutlined />}
      dict={DICT}
      addClass="layout palatte-geekblue-8 vbox width12"
      data-testid="sbm-studio-v2"
      key="SBMSETUP2"
      text="SBM定位設定 2" onClick={() => {
        openSBM2(false);
            }} />,

    <BASE_COM.IconButton
      iconType={<ThunderboltOutlined />}
      dict={DICT}
      addClass="layout palatte-purple-8 vbox width12"
      key="NOW"
      data-testid="quick-verify"
      text="快速驗證" onClick={() => {

        let InspectionModeOption={
          CI:"檢驗",
          FI:"全檢",
        }
        
        setModal_view({
          onOk: () => {
            setModal_view(undefined);
          },
          onCancel: () => { 
            setModal_view(undefined); 
          },

          footer:null,
          title: "快速驗證",
          view:<>

            選擇模式
            <Button key="CI_MODE" data-testid="quick-verify-ci" onClick={_ => startQuickInsp("CI")}>
              檢驗{machine_custom_setting.InspectionMode=="CI"?<StarOutlined />:null}
            </Button>
            <Button key="FI_MODE" data-testid="quick-verify-fi" onClick={_ => startQuickInsp("FI")}>
              全檢{machine_custom_setting.InspectionMode=="FI"?<StarOutlined />:null}
            </Button>
            <div style={{ marginTop: 12, borderTop: '1px solid #333', paddingTop: 10 }}>
              <Button key="XREP" data-testid="quick-verify-xrep"
                onClick={_ => {
                  setModal_view(undefined);
                  setFileSelectCfg({
                    filter: makeExtensionFilter('xreps'),
                    path: machine_custom_setting.InspSampleSavePath || 'data/',
                  });
                  setFileSelectedCallBack(() => (filePath, fileInfo) => {
                    setFileSelectedCallBack(undefined);
                    setFileSelectCfg(undefined);
                    loadXrepForVerify(filePath, fileInfo);
                  });
                }}>
                載入 xrep
              </Button>
              <div style={{ fontSize: 12, color: '#888', marginTop: 6, lineHeight: 1.7 }}>
                用存下來的檢驗記錄當輸入:載入它的影像,並改用<b>那張影像的相機參數</b>來量。
                不會動到這份設定檔的校正值。
              </div>
            </div>
          </>
        })



      }} />,



  ]);


  let DefFileFolder = defModelPath.substr(0, defModelPath.lastIndexOf('/') + 1);
  if (fileSelectedCallBack !== undefined) {
    MenuSet.push(
      <BPG_FileBrowser key="BPG_FileBrowser"
        searchDepth={4}
        className="width8 modal-sizing"
        path={(fileSelectCfg && fileSelectCfg.path) || DefFileFolder}
        visible={fileSelectedCallBack !== undefined}
        BPG_Channel={(...args) => ACT_WS_SEND_BPG(CORE_ID, ...args)}
        onFileSelected={(filePath, fileInfo) => {
          fileSelectedCallBack(filePath, fileInfo);
        }}
        onOk={(folderPath) => {
        }}
        onCancel={() => {
          setFileSelectedCallBack(undefined);
          setFileSelectCfg(undefined);
        }}
        fileFilter={(fileSelectCfg && fileSelectCfg.filter) || defFileFilter}
      />);

  }
  if (fileSavingCallBack !== undefined) {
    let defaultName = defModelPath.substr(defModelPath.lastIndexOf('/') + 1);
    log.debug("[browser] BPG_FileSavingBrowser open");
    MenuSet.push(
      <BPG_FileSavingBrowser key="BPG_FileSavingBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={DefFileFolder} visible={fileSavingCallBack !== undefined}
        defaultName={defaultName}
        BPG_Channel={(...args) => ACT_WS_SEND_BPG(CORE_ID, ...args)}

        onOk={(folderInfo, fileName, existed) => {
          fileSavingCallBack(folderInfo, fileName, existed);

        }}
        onCancel={() => {
          setFileSavingCallBack(undefined);
        }}
        fileFilter={defFileFilter}
      />);

  }

  let modal_view_sec=null;

  if(modal_view !== undefined && modal_view.ext_sec!==undefined)
  {
    switch(modal_view.ext_sec)
    {
      case "INST_Inspection":
        let fallback_nowInspdata=nowInspdata||{}
        // console.log(cacheDef,edit_info._obj.cameraParam,fallback_nowInspdata.reports,fallback_nowInspdata.image);
        modal_view_sec=
          <RepDisplay 
            def={cacheDef} 
            camera_param={fallback_nowInspdata.cam_param}  
            reports={fallback_nowInspdata.reports} 
            image={fallback_nowInspdata.image}
            stationOverlay={stationOverlay}
            IGNORE_IMAGE_FIT_TO_SCREEN={true}
            ALLOW_CONTROL_DOWN_SAMPLING_LEVEL={true}
            BPG_Channel={(...args)=>ACT_WS_SEND_BPG(CORE_ID, ...args) }
            downSampleFactor={1}
            />;
        // modal_view_sec="dd"
        break;
    }
  }
  MenuSet.push(
    <Modal
      {...modal_view}
      visible={modal_view !== undefined}
      onCancel={(param) => {
        if (modal_view!==undefined && modal_view.onCancel !== undefined) {
          modal_view.onCancel(param);
        }
        else
        {
          setModal_view(undefined);
        }
      }}

      onOk={(param) => {
        if (modal_view!==undefined && modal_view.onOk !== undefined) {
          modal_view.onOk(param);
        }
        else
        {
          setModal_view(undefined);
        }
      }}>
      {modal_view === undefined ? null : (typeof modal_view.view === 'function'? modal_view.view():modal_view.view )}
      {modal_view_sec}
    </Modal>);

  return MenuSet;
}


// keystone step 4: GenTarEditUI extracted from the class to a top-level function
// component. Was a class METHOD using React Hooks rendered as <this.GenTarEditUI/>,
// which technically works (React calls it as a component) but is fragile against
// Rules-of-Hooks tooling and confuses readers. As a top-level function component
// it's idiomatic, lint-friendly, and easier to extract further (next: shape-slice).
function GenTarEditUI({ edit_tar_info, shape_list, Info_decorator, ec_canvas,
                       ACT_EDIT_TAR_ELE_TRACE_UPDATE,
                       ACT_WS_SEND_BPG, CORE_ID, edit_info }) {
  const dispatch = useDispatch();
  // ONE primitive's edge profile, from the image already on screen.
  //
  // II + __CACHE_IMG__ is the request the CHECK button makes: one inspection of
  // the frame the editor is holding. No camera, no subscription, nothing to
  // stop afterwards.
  //
  // This started as a CI stream, which was wrong in three ways and each one
  // cost something. A CI subscription is started with keep:true and ended by a
  // separate keep:false -- sending keep:false WITH definfo does not make a
  // one-shot, it registers like any other, so the first version left five
  // streams running and the camera never stopped. Ending them properly still
  // left a live stream during the measurement, and a live stream rewrites the
  // editor's shape list every frame, so the slider's own writes were being
  // overwritten as fast as they were made. Reaching for CancelNowInsp's
  // trigger_mode reset to tidy up made it worse again: that is a machine-wide
  // setting changed by a panel-local probe, and after it the property sheet
  // stopped writing to the store at all.
  //
  // None of it was needed. The threshold is being set against a picture the
  // operator is looking at, so the right frame is that one -- which also makes
  // the probe repeatable, and makes "no object" a fact about the def rather
  // than about the instant the button was pressed.
  const probeEdgeProfile = (shape) => {
    let deffile;
    try { deffile = defFileGeneration(edit_info); stampRefImagePath(deffile, edit_info); }
    catch (e) { return Promise.reject(new Error('def 產生失敗')); }

    const off = () => ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { DEBUG_EMIT: { edge_profile: false } });
    ACT_WS_SEND_BPG(CORE_ID, "ST", 0, { DEBUG_EMIT: { edge_profile: true } });

    return new Promise((resolve, reject) => {
      ACT_WS_SEND_BPG(CORE_ID, "II", 0,
        {
          definfo: deffile,
          imgsrc: "__CACHE_IMG__",
          img_property: {
            calibInfo: { type: "disable", mmpp: deffile.featureSet[0].mmpp },
          },
        },
        undefined,
        {
          resolve: (pkts) => {
            const RP = (pkts || []).find((p) => p.type === "RP");
            // Put the report on the CANVAS too, the way CHECK and
            // sendOrientationInspect do. A threshold change moves which edge
            // each caliper picks, and the hits and the fitted line move with
            // it -- so the answer the operator is judging should be the one on
            // screen, not the one from before the drag. IGNORE_DEFCONF_LOCK
            // because a locked def still gets to be looked at.
            const put = (pkt) => {
              if (pkt === undefined) return;
              const a = BPG_Protocol.map_BPG_Packet2Act(pkt);
              if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
            };
            put(RP);
            put((pkts || []).find((p) => p.type === "IM"));
            const reports = GetObjElement(RP, ["data", "reports", 0, "reports"]);
            const one = reports && reports[0];
            if (!one) {
              // The core says why when it can; CHECK surfaces the same field.
              const why = GetObjElement(RP, ["data", "reports", 0, "locate", "reason"]);
              reject(new Error(why || '這張影像沒有偵測到物件'));
              return;
            }
            // Match by id, not by index: an NA primitive is absent from the
            // list, so position means nothing.
            const pools = [].concat(one.detectedLines || [], one.detectedCircles || [],
                                    one.searchPoints || []);
            const hit = pools.find((e) => e && e.id === shape.id);
            if (!hit) { reject(new Error('這個 primitive 沒有回報（可能是 NA）')); return; }
            // A caliper sends a curve (g), a search point sends candidates
            // (p/s). Accept either; the panel branches on `kind`.
            const prof = hit.extra && hit.extra.edge_profile;
            const usable = prof && ((prof.g && prof.g.length) || (prof.p && prof.p.length));
            if (!usable) {
              // Say what DID arrive. "The core did not send it" is a guess, and
              // it was wrong the first time it was read: the payload was there
              // and in a shape this check did not recognise.
              reject(new Error('沒有可用的 edge_profile（extra: '
                + Object.keys((hit && hit.extra) || {}).join(',') + '）'));
              return;
            }
            resolve(prof);
          },
          reject: (e) => reject(e instanceof Error ? e : new Error(String(e))),
        });
    }).then((p) => { off(); return p; },
            (e) => { off(); throw e; });
  };


    const DICT = useSelector(state => state.UIData.DICT);
    // New-version (shape_based) defs: measurement primitives must use caliper locating
    // (the raw-gray path has no contour to follow) — hide the contour/caliper choice.
    const lockCaliper = useSelector(state => state.UIData.edit_info.locating_engine === 'shape_based');
  
    let edit_tar = edit_tar_info;
    let decorator = Info_decorator;

    let UIArr = [];
    useEffect(
      () => {
        //console.log("GenTarEditUI effect");
        return () => {
          //console.log("GenTarEditUI cleaned up");
        };
      }, []
    );

    {
      function refChainHasLoop(tar1, tar2, infoList, treeDepth = 0, treeDepthMax = infoList.length + 1)//when treeDepth over max, consider it has loop
      {
        //console.log("refChainHasLoop:",tar1,tar2,"treeDepth:",treeDepth)
        if ((tar1.id == tar2.id) || (treeDepth >= treeDepthMax)) return true;
        if (tar2.ref === undefined || tar2.ref.length == 0) return false;

        let id2RefTars = tar2.ref
          .map(ref => infoList.find(infoInList => ref.id == infoInList.id))
          .filter(tar => tar !== undefined);
        //console.log("id2RefTars:",id2RefTars)

        let retR = id2RefTars.reduce((hasLoop, refTar) => hasLoop ? hasLoop :
          refChainHasLoop(tar1, refTar, infoList, treeDepth + 1, treeDepthMax), false);
        //console.log("retR:",retR,"  treeDepth:",treeDepth)
        return retR;
      }
      edit_tar = Shape_Attr_Fill(edit_tar);

      // Always compute the legacy whiteListKey (hooks must be unconditional).
      // Used only by the JsonEditBlock fallback below — wasted on shapes
      // that have a dedicated PropertySheet, but cheap and Rules-of-Hooks
      // compliant.
      const whiteListKey = useMemo(
        () => buildSchema(edit_tar, {
          shape_list, renderMethods, refChainHasLoop,
          ACT_EDIT_TAR_ELE_TRACE_UPDATE,
        }),
        [edit_tar.id, edit_tar.type, edit_tar.subtype, edit_tar.ref, shape_list,
         ACT_EDIT_TAR_ELE_TRACE_UPDATE]
      );

      // Per-shape PropertySheet: dedicated React component per shape type.
      // The PropertySheet receives the shape + onUpdate (dispatches
      // Shape_Set) + onTracePick (ref-pick mode) + shapeList. Shapes
      // without a PropertySheet export fall back to the legacy
      // JsonEditBlock path until they're migrated.
      const PSheetMod = getShapeModule(edit_tar.type);
      const PSheet = PSheetMod && PSheetMod.PropertySheet;
      if (PSheet) {
        // WHAT THE MACHINE JUST MEASURED, for the fields that want to be set
        // from it rather than typed. Read from the report rather than from the
        // shape: the shape carries the DEF's target, and the two must not be
        // confused. Absent until a CHECK has run, and then the button that uses
        // it simply does not offer itself.
        const _measured = (() => {
          const j = GetObjElement(edit_info, ['inspReport', 'reports', 0, 'judgeReports']);
          if (!Array.isArray(j)) return undefined;
          const hit = j.find((e) => e && e.id === edit_tar.id);
          return (hit && Number.isFinite(hit.value)) ? hit.value : undefined;
        })();
        UIArr.push(<PSheet
          key="propertySheet"
          measured={_measured}
          shape={edit_tar}
          shapeList={shape_list}
          dict={DICT}
          dictTheme={edit_tar.type}
          lockCaliper={lockCaliper}
          onProbeEdges={probeEdgeProfile}
          onUpdate={(next) => ec_canvas.SetShape(next, next.id)}
          onTracePick={(keyTrace) => ACT_EDIT_TAR_ELE_TRACE_UPDATE(keyTrace)}
        />);
      } else {
        UIArr.push(<BASE_COM.JsonEditBlock
          key="BASE_COM.JsonEditBlock"
          object={edit_tar}
          dict={DICT}
          additionalData={{ shape_list }}
          dictTheme={edit_tar.type}
          renderLib={renderMethods}
          whiteListKey={whiteListKey}
          jsonChange={(original_obj, target, type, evt) => {
            if (type == "btn") {
              if (target.keyTrace[0] == "ref" || target.keyTrace[0] == "ref_baseLine") {
                ACT_EDIT_TAR_ELE_TRACE_UPDATE(target.keyTrace);
              }
              return;
            }
            const lastKey = target.keyTrace[target.keyTrace.length - 1];
            const field = fieldFor(edit_tar, lastKey);
            if (!applyFieldChange(field, target, type, evt)) return;
            ec_canvas.SetShape(original_obj, original_obj.id);
          }}
        />);
      }
    }




    return UIArr;
}


// Persistent orientation + multi-image switcher. Rendered by APP_DEFCONF_MODE
// (NOT the per-submode menu), so the floating image dropdown and the
// auto-orientation survive across every edit menu (the neutral menu unmounts
// when you enter a shape-edit submode). On entry it runs ONE orientation-only
// inspection so the canvas rectifies the reference image to the def's object
// frame; it discovers the def's sibling images (<base>*.{png,...}) and lets the
// editor swap the background ON THE FLY (image-only load + re-orient) without
// ever touching the deffile / shapes / edit mode.
// Is `p` inside `root`? Mirrors the core's path_is_under (wiringPanel.cpp) --
// normalised separators, a trailing one on the root so "defs_old" is not
// treated as inside "defs", and case-insensitive because these are Windows
// paths. The core is the one that ENFORCES; this only decides whether to warn.
export function pathIsUnder(p, root) {
  if (!p || !root) return false;
  const n = (x) => String(x).split('\\').join('/').replace(/[/]{2,}/g, '/').toLowerCase();
  let t = n(p), r = n(root);
  if (!r.endsWith('/')) r += '/';
  return t.startsWith(r);
}

function DefConfImageSwitcher() {
  const dispatch = useDispatch();
  const edit_info = useSelector(state => state.UIData.edit_info);
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const ACT_WS_SEND_BPG = (...args) => dispatch(UIAct.EV_WS_SEND_BPG(...args));

  // Orientation-only inspection on the core's current cached image -> feed RP/IM
  // to redux so the canvas rectifies. Never modifies the deffile. IGNORE_DEFCONF_LOCK
  // so the display actions pass the post-load lock filter.
  const sendOrientationInspect = () => {
    if (!CORE_ID || !edit_info || !edit_info._obj || !edit_info._obj.sig360info) return;
    let deffile = defFileGeneration(edit_info);
    stampRefImagePath(deffile, edit_info);   // shape locator: ref-image path for the core
    ACT_WS_SEND_BPG(CORE_ID, "II", 0,
      { definfo: deffile, imgsrc: "__CACHE_IMG__",
        img_property: { calibInfo: { type: "disable", mmpp: deffile.featureSet[0].mmpp } } },
      undefined,
      { resolve: (darr) => {
          let RP = darr.find(p => p.type == "RP");
          if (RP !== undefined) { let a = BPG_Protocol.map_BPG_Packet2Act(RP); if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); } }
          let IM = darr.find(p => p.type == "IM");
          if (IM !== undefined) { let a = BPG_Protocol.map_BPG_Packet2Act(IM); if (a !== undefined) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); } }
        }, reject: (e) => { } });
  };

  // Auto-orientation on entry, debounced past the def-load action bundle (whose
  // sig360_extractor Edit_info_reset clears inspReport).
  const orientSentRef = useRef(null);
  useEffect(() => {
    if (!CORE_ID) return;
    if (!edit_info || !edit_info._obj || !edit_info._obj.sig360info) return;
    if (!edit_info.img) return;
    const key = edit_info.defModelPath || 'def';
    if (orientSentRef.current === key) return;
    const t = setTimeout(() => {
      if (orientSentRef.current === key) return;
      orientSentRef.current = key;
      sendOrientationInspect();
    }, 400);
    return () => clearTimeout(t);
  }, [CORE_ID, edit_info.defModelPath, edit_info.img, edit_info.sig360info]);

  // Fired when the SBM studio closes. Debounced by a tick so the studio's own
  // last EditInfo_Patch (a region, a registration, a fresh cache) is in the
  // store before the def is generated from it -- otherwise this would locate
  // against the state the operator just changed.
  useEffect(() => {
    const h = () => setTimeout(() => sendOrientationInspect(), 60);
    window.addEventListener('defconf-orient-now', h);
    return () => window.removeEventListener('defconf-orient-now', h);
  });

  // The scan/LD/IM half lives in useDefImages -- the SBM studio needs the same
  // switch and a second copy of it would be a screen that shows one image while
  // the core measures another.
  const { imageList, currentImagePath, switchImage } =
    useDefImages({ afterLoad: () => sendOrientationInspect() });

  if (imageList.length <= 1) return null;
  return createPortal(
    <div style={{
      position: 'fixed', right: 12, bottom: 12, zIndex: 100000,
      background: 'rgba(30,30,30,0.85)', color: '#eee',
      padding: '6px 8px', borderRadius: 6,
      boxShadow: '0 2px 8px rgba(0,0,0,0.4)',
      display: 'flex', alignItems: 'center', gap: 6, fontSize: 12
    }}>
      <span style={{ color: '#bbb' }}>image</span>
      <select value={currentImagePath || ''}
        onChange={(e) => switchImage(e.target.value)}
        style={{ height: 24, fontSize: 12, maxWidth: 240, color: '#000', background: '#fff' }}>
        {imageList.map(im => <option key={im.path} value={im.path} style={{ color: '#000', background: '#fff' }}>{im.name}</option>)}
      </select>
    </div>,
    document.body
  );
}


class APP_DEFCONF_MODE extends React.Component {

  componentDidMount() {
    
    let defModelPath = this.props.edit_info.defModelPath;
    loadDefFile(defModelPath,this.props.ACT_DefConf_Lock_Level_Update,this.props.ACT_WS_SEND_BPG,this.props.CORE_ID,this.props.DISPATCH);

    
    // DO NOT flip trigger_mode here — DefConf must NOT free-run the
    // camera, or it floods frames the user isn't actively viewing.
    // Trigger only flips to continuous in InspMode (see APP_INSP_MODE
    // mount in InspectionUI.js).
    //
    // JPEG streaming (quality 85) for the full-res template stream —
    // ~10x smaller than raw RGBA, visually lossless. Sticky in the
    // core but now safe for cross-mode use: the receiver
    // (UTIL/BPG_Protocol.raw2Obj_IM + map_BPG_Packet2Act) accepts
    // both format=1 (BGR JPEG) and format=2 (grayscale JPEG).
    this.props.ACT_WS_SEND_BPG(this.props.CORE_ID, "ST", 0,
    {
      CameraSetting: { ROI:[0,0,99999,99999] },
      IMG_STREAMING_JPEG_QUALITY: 85,
    });
  }

  componentWillUnmount() {
    this.props.ACT_ClearImage();

    this.props.ACT_DefConf_Lock_Level_Update(0);
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
    this.state = {
      fileSelectedCallBack: undefined,
      fileSavingCallBack: undefined,
      modal_view: undefined
    }

  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }




  render() {

    let MenuSet = [];
    let menu_height = "HXA";//auto
    log.debug("[render] CanvasComponent");
    let substate = this.props.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];

    let defModelPath = this.props.edit_info.defModelPath;
    switch (substate) {
      case UIAct.UI_SM_STATES.DEFCONF_MODE_NEUTRAL:
        
        menu_height = "HXA";
        MenuSet=<DEFCONF_MODE_NEUTRAL_UI/>

        break;
      case UIAct.UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
        MenuSet = [
          <BASE_COM.IconButton
            iconType={<ArrowLeftOutlined/>}
            addClass="layout black vbox width4"
            key="<" onClick={() => this.props.ACT_Fail()} />,
          // Dark ink on the pale header. It inherits the panel's white, which was
          // right while the panel floated over the camera image and is wrong now
          // that it has a ground of its own -- and was already wrong here, since
          // .lblue is rgb(204,204,238). Set locally rather than on the panel:
          // 複製 / 刪除 / CHECK sit on dark bars and need the white they inherit.
          <div key="MEASURE" className="s width8 lblue vbox" style={{ color: '#333' }}>MEASURE</div>,
        ];


        if (this.props.edit_tar_info != null) {
          let subType = this.props.edit_tar_info.subtype;
          log.debug("[JsonEditBlock]", { edit_tar_info: this.props.edit_tar_info, subType });
          MenuSet.push(<BASE_COM.JsonEditBlock object={this.props.edit_tar_info} dict={this.props.DICT}
            key="BASE_COM.JsonEditBlock"
            renderLib={renderMethods}
            whiteListKey={{
              //id:"div",
              name: "input",
              //pt1:null,
              subtype: "div",
              // calc_f:{
              //   __OBJ__:renderMethods.Measure_Calc_Editor,
              //   measure_list:this.props.shape_list.filter(s=>s.type==UIAct.SHAPE_TYPE.measure)
              // },
              ref: (subType === UIAct.SHAPE_TYPE.measure_subtype.calc) ?
                undefined :
                {
                  __OBJ__: "div",
                  ...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].reduce((acc, key) => {
                    acc[key + ""] =
                    {
                      __OBJ__: "btn",
                      id: "div",
                      element: "div"
                    };
                    return acc;
                  }, {})
                },
              ref_baseLine: {
                __OBJ__: "btn",
                id: "div",
                element: "div"
              }
            }}
            jsonChange={(original_obj, target, type, evt) => {
              if (type == "btn") {
                if (target.keyTrace[0] == "ref" || target.keyTrace[0] == "ref_baseLine") {
                  this.props.ACT_EDIT_TAR_ELE_TRACE_UPDATE(target.keyTrace);
                }
              }
              else {
                let lastKey = target.keyTrace[target.keyTrace.length - 1];

                if (type == "input-number") {
                  let parseNum = parseFloat(evt.target.value);
                  target.obj[lastKey] = Number.isFinite(parseNum) ? parseNum : target.obj[lastKey];
                }
                else if (type == "input")
                  target.obj[lastKey] = evt.target.value;
                this.ec_canvas.SetShape(original_obj, original_obj.id);
              }
            }} />);
          if (this.props.edit_tar_info.subtype == UIAct.SHAPE_TYPE.measure_subtype.NA) {

            for (var key in UIAct.SHAPE_TYPE.measure_subtype) {
              if (key == "NA") continue;
              MenuSet.push(<BASE_COM.IconButton
                dict={this.props.DICT}
                key={"MSUB__" + key}
                addClass="layout red vbox btn-swipe"
                style={{ backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[UIAct.SHAPE_TYPE.measure] }}


                text={key} onClick={(data, btn) => {
                  this.props.ACT_EDIT_TAR_ELE_CAND_UPDATE(btn.props.text);
                }} />);
            }
          }

          let tar_info = this.props.edit_tar_info;

          if (tar_info.ref !== undefined) {
            MenuSet.push(<BASE_COM.Button
              key="ADD_BTN"
              addClass="layout red vbox"
              text="ADD" onClick={() => {
                this.ec_canvas.SetShape(this.props.edit_tar_info);
                this.props.ACT_SUCCESS();
              }} />);
          }
        }
        break;

      case UIAct.UI_SM_STATES.DEFCONF_MODE_LINE_CREATE:
        MenuSet = [
          <BASE_COM.IconButton
            iconType={<ArrowLeftOutlined/>}
            dict={this.props.DICT}
            addClass="layout black vbox width4"
            key="<" onClick={() => this.props.ACT_Fail()} />,
          <div key="LINE" className="s width8 lred vbox">LINE</div>,
        ];

        break;
      case UIAct.UI_SM_STATES.DEFCONF_MODE_ARC_CREATE:
        MenuSet = [
          <BASE_COM.IconButton
            iconType={<ArrowLeftOutlined/>}
            dict={this.props.DICT}
            addClass="layout black vbox width4"
            key="<"
            onClick={() => this.props.ACT_Fail()} />,
          <div key="ARC" className="s width8 lred vbox">ARC</div>
        ];
        break;

      case UIAct.UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
        MenuSet = [
          <BASE_COM.IconButton
            dict={this.props.DICT}
            addClass="layout black vbox"
            key="<" 
            iconType={<ArrowLeftOutlined/>}
            onClick={() => this.props.ACT_Fail()} />,
          <div key="SEARCH_POINT" className="s lred vbox">SPOINT</div>,
        ];
        if (this.props.edit_tar_info != null) {
          log.debug("[JsonEditBlock]", this.props.edit_tar_info);
          MenuSet.push(<GenTarEditUI key="tarEditUI" ec_canvas={this.ec_canvas} {...this.props} />);


          let tar_info = this.props.edit_tar_info;
          if (tar_info.ref[0].id !== undefined) {
            MenuSet.push(<BASE_COM.Button
              key="ADD_BTN"
              addClass="layout red vbox"
              text="ADD" onClick={() => {
                this.ec_canvas.SetShape(this.props.edit_tar_info);
                this.props.ACT_SUCCESS();
              }} />);
          }

        }
        break;


      case UIAct.UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
        {
          MenuSet = [
            <BASE_COM.IconButton
              addClass="layout black vbox"
              key="<" 
            iconType={<ArrowLeftOutlined/>} onClick={() => this.props.ACT_Fail()} />,
            <div key="AUX_POINT" className="s lred vbox">APOINT</div>,
          ];


          if (this.props.edit_tar_info != null) {
            log.debug("[JsonEditBlock]", this.props.edit_tar_info);


            MenuSet.push(<GenTarEditUI key="tarEditUI" ec_canvas={this.ec_canvas} {...this.props} />);

            let tar_info = this.props.edit_tar_info;
            if (tar_info.ref[0].id !== undefined &&
              tar_info.ref[1].id !== undefined &&
              tar_info.ref[0].id != tar_info.ref[1].id
            ) {
              MenuSet.push(<BASE_COM.Button
                key="ADD_BTN"
                addClass="layout red vbox"
                text="ADD" onClick={() => {
                  this.ec_canvas.SetShape(this.props.edit_tar_info);
                  this.props.ACT_SUCCESS();
                }} />);
            }

          }
        }
        break;



      case UIAct.UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        MenuSet = [
          <BASE_COM.IconButton
            key="<"
            addClass="layout black vbox width4"
            
            iconType={<ArrowLeftOutlined/>}
            onClick={() => this.props.ACT_Fail()} />,

          <div key="EDIT_Text" className="s width8 lblue vbox" style={{ color: '#333' }}>EDIT</div>,
          <div key="HLINE" className="s HX0_1"></div>
        ]

        if (this.props.edit_tar_info != null) {
          MenuSet.push(<GenTarEditUI key="tarEditUI" ec_canvas={this.ec_canvas} {...this.props} />);

          let on_DEL_Tar = (id) => {
            this.ec_canvas.SetShape(null, id);
          }
          let on_COPY_Tar = (targetShape) => {
            let copy_shape = dclone(targetShape);
            copy_shape.id = undefined;//the undefined id will let reducer append a new feature
            //console.log(copy_shape);
            ["pt1", "pt2", "pt3"].forEach((pt_key) => {
              if (copy_shape[pt_key] === undefined) return;

              // Same reason as the preview canvas: a pure-SBM def has no
              // signature, and the raw call returns 1 -- so a copied shape
              // would be offset by 100 MILLIMETRES instead of 100 pixels and
              // land somewhere off the part.
              let mmpp = this.props.edit_info._obj.getEditorMmpp();
              copy_shape[pt_key].x += mmpp*100;
              copy_shape[pt_key].y += mmpp*100;
            });
            let reNameCount = 1;

            let tmpName = copy_shape.name + "[" + reNameCount + "]";
            while (this.props.shape_list.find(shape => shape.name === tmpName) !== undefined) {
              reNameCount++;
              tmpName = copy_shape.name + "[" + reNameCount + "]";
            }
            copy_shape.name = tmpName;
            this.ec_canvas.SetShape(copy_shape, undefined);
          }
          if (this.props.edit_tar_info.id !== undefined) {
            MenuSet.push(<BASE_COM.Button
              key="COPY_BTN"
              addClass="layout blue vbox"
              text="複製" onClick={() => on_COPY_Tar(this.props.edit_tar_info)} />);


            MenuSet.push(<BASE_COM.Button
              key="DEL_BTN"
              addClass="layout red vbox"
              text="刪除" onClick={() => {
                let tarInfo = this.props.edit_tar_info;
                let warningUI = "確定要刪除:" + tarInfo.name + " ?";

                let refTree = this.props.edit_info._obj.FindShapeRefTree(tarInfo.id)
                let flatTree = this.props.edit_info._obj.FlatRefTree(refTree);

                //The flat tree contains shapes in inherentShapeList, 
                //We only need the one in shapelist
                flatTree = flatTree.filter((refedShape) =>
                  this.props.shape_list.find(shape => shape.id == refedShape.id));

                if (flatTree.length !== 0) {
                  warningUI = <div>
                    {warningUI}<br />
                  相關連之物件如下
                  {flatTree.map(fShape => [<br />, fShape.shape.name])}
                  </div>
                }


                this.setState({
                  ...this.state, modal_view: {

                    title: "WARNING",
                    onOk: () => {
                      on_DEL_Tar(tarInfo.id);
                      log.debug("[onOK]")
                    },
                    onCancel: () => { console.log("onCancel") },
                    view_update: () => warningUI
                  }
                })
              }} />);


            MenuSet.push(<BASE_COM.IconButton
            
              iconType={<VerticalAlignTopOutlined />}
              key="CHECK"
              addClass="layout blue vbox"
              text="CHECK" onClick={() =>{

                let deffile = defFileGeneration(this.props.edit_info);
                stampRefImagePath(deffile, this.props.edit_info);   // shape locator: ref-image path
              

                this.props.ACT_WS_SEND_BPG(this.props.CORE_ID,"II", 0, 
                {
                  definfo:deffile,
                  imgsrc:"__CACHE_IMG__",
                  img_property:{
                    calibInfo:{
                      type:"disable",
                      mmpp:deffile.featureSet[0].mmpp
                    },
                    //down_samp_level:1,
                  }
                },undefined,
                {
                  resolve:(darr,mainFlow)=>{
                    let RP=darr.find(pkt=>pkt.type=="RP");
                    if(RP!==undefined)
                    {
                      // Feed the inspection report into redux (sets edit_info.inspReport)
                      // so the def-conf canvas can rectify the image + show cal_hits.
                      // Same gap as INST_CHECK: only the manual shape adjust ran here.
                      let rpAct = BPG_Protocol.map_BPG_Packet2Act(RP);
                      if (rpAct !== undefined) this.props.DISPATCH(rpAct);

                      let insp_reports = GetObjElement(RP,["data","reports",0,"reports"]);
                      // A CHECK THAT DOES NOTHING MUST SAY SO.
                      //
                      // Four ways this used to end in silence, and from the
                      // outside all four look like a dead button: no object in
                      // the report, this shape absent from the object, the
                      // shape measured NA, or the request itself rejected. The
                      // first is the common one and the least guessable -- the
                      // core DROPS a whole detection when an orientation-
                      // essential judge fails, so dragging one line past its
                      // caliper margin (often 0.2mm) deletes every result in
                      // the frame, not just that line's.
                      if(insp_reports.length === 0)
                      {
                        const why = GetObjElement(RP,["data","reports",0,"locate","reason"]);
                        Modal.warning({
                          title: 'CHECK:這一幀沒有偵測到物件',
                          content: why
                            ? why
                            : '核心回報 0 個物件,且沒有附上原因。定位失敗或某個判定否決了這次偵測。',
                        });
                      }
                      if(insp_reports.length>0)
                      {
                        let insp_rep = insp_reports[0];
                        console.log(insp_rep);
                        let mod_shape=dclone(this.props.edit_tar_info);

                        // Is THIS shape in the report at all? ShapeAdjusts...
                        // returns without a word when it is not, which is how a
                        // feature that the core never even reached looks
                        // identical to one that measured perfectly.
                        const _mine = (insp_rep && this.props.edit_info._obj
                                       .FindInspShapeObject)
                          ? this.props.edit_info._obj.FindInspShapeObject(mod_shape && mod_shape.id, insp_rep)
                          : undefined;
                        if (mod_shape && _mine === undefined) {
                          Modal.warning({
                            title: 'CHECK:報告裡沒有這個特徵',
                            content: '物件有被定位到,但 ' + (mod_shape.name || ('id ' + mod_shape.id))
                              + ' 不在回報中,所以畫面沒有更新。',
                          });
                        }

                        this.props.edit_info._obj.ShapeAdjustsWithInspectionResult(mod_shape,this.props.shape_list, insp_rep,true);

                        // NA is a real answer, and the reason is in the report.
                        // Without this the shape greys out and the operator is
                        // left to work out which knob did it.
                        if (mod_shape && mod_shape.inspection_status === BPG_Protocol.INSPECTION_STATUS.NA) {
                          Modal.warning({
                            title: 'CHECK:' + (mod_shape.name || ('id ' + mod_shape.id)) + ' 量不到',
                            content: mod_shape.na_reason
                              || '核心沒有給原因。線被拖離邊緣超過 margin 時,caliper 掃不到邊就是這個結果。',
                          });
                        }

                        // Same rule as INST_CHECK: NA keeps the def's geometry
                        // and carries only the status, the reason and the hits.
                        mod_shape = InspectionEditorLogic.KeepDefGeometryIfNotMeasured(
                          this.props.edit_tar_info, mod_shape);

                        mod_shape=modShapeCleanUp(mod_shape);
                        if(mod_shape!==undefined)
                        {
                          this.ec_canvas.SetShape(mod_shape, mod_shape.id);
                        }
                        //Make sure the status is not NA
                        // if(mod_shape.inspection_status!==BPG_Protocol.INSPECTION_STATUS.NA)
                        // {
                        //   //this.props.shape_list[idx]=mod_shape;
                          
                        //   delete mod_shape["inspection_value"]
                        //   delete mod_shape["inspection_status"]
                        //   this.ec_canvas.SetShape(mod_shape, mod_shape.id);
                        // }
                        
                        //console.log(shape,mod_shape);
                        
                      }
                    }
                  },
                  reject:(e)=>{
                    // Was empty. A rejected round trip is the one case where
                    // the operator has no way at all to tell that anything
                    // happened.
                    Modal.error({
                      title: 'CHECK 沒有送出去',
                      content: '核心沒有回覆這次檢驗:' + ((e && e.message) || e || '連線中斷'),
                    });
                  }
                });
              }} />);
          }

        }
        else {
          //console.log(this.props.shape_list);

          let shapeListInOrder = this.props.shape_list;
          //console.log(this.props.Info_decorator.list_id_order);
          if (this.props.Info_decorator.list_id_order.length == shapeListInOrder.length) {
            shapeListInOrder = this.props.Info_decorator.list_id_order.map(id => this.props.shape_list.find(shape => shape.id == id));
          }
          // The localizer's extraction regions are not measurement features and
          // this list is how measurement features are selected -- clicking one
          // opens it in the property sheet with 複製 / 刪除 / CHECK, none of
          // which mean anything for a region. They are authored in the SBM
          // studio and drawn only there.
          //
          // AFTER the ordering, not before: list_id_order is compared for
          // LENGTH against the full list, and filtering first makes that
          // comparison fail, silently dropping the operator's ordering.
          shapeListInOrder = shapeListInOrder.filter(
            (sh) => sh && sh.type !== 'loc_include' && sh.type !== 'loc_exclude');
          MenuSet.push(<BASE_COM.Button
            key="setAdditional"
            addClass="layout black vbox HX0_5"
            text="..." onClick={() => {


              let measureShape=shapeListInOrder
                .filter((shape)=>shape.type===UIAct.SHAPE_TYPE.measure);
              this.setState({
                ...this.state, modal_view: {

                  title: "GOGOGO",
                  onOk: () => {
                    log.debug("[onOK]")
                  },
                  onCancel: () => { console.log("onCancel") },
                  view_update: () => {
                    return <>

                      <InspMarginEditor  
                        control_margin_info={this.props.Info_decorator.control_margin_info}
                        measureInfo={measureShape}
                        DICT={this.props.DICT}
                        onExitDump={(dumpInfo)=>{
                          this.props.ACT_Shape_Decoration_Control_Margin_Info_Update(dumpInfo.control_margin_info);
                          let originList = this.props.shape_list;
                          let newList = originList.map(oshape=>{
                            let newShape=dumpInfo.measureInfo.find(dshape=>dshape.id==oshape.id);
                            if(newShape===undefined)return oshape;
                            return {...oshape,...newShape};
                          });
                          this.props.ACT_Shape_List_Update(newList);
                        }}
                      />  
                    </>
                  }
                }
              })
            }} />);
          MenuSet.push(<div className="s HXA" key="DragSortableList_con" >
            <DragSortableList
              items={shapeListInOrder.map((shape, id) =>{ 
                return{
                  content: (
                    <div
                      key={"shape_listing_" + shape.id}
                      className="button   btn-swipe"
                      style={{ height: "40px",backgroundColor:EC_CANVAS_Ctrl.SHAPE_TYPE_COLOR[shape.type] }}
                      onClick={() => this.props.ACT_EDIT_TAR_UPDATE(shape)}>
                      {shape.name}
                    </div>),
                  shape_id: shape.id
                }
              })}

              onSort={(newContentOrder) => {
                let idOrder = newContentOrder.map(ele => ele.shape_id);
                this.props.ACT_Shape_Decoration_ID_Order_Update(idOrder);
                log.debug("[onSort]", { newContentOrder, idOrder })
              }}
              dropBackTransitionDuration={0.3}
              type="vertical" />
          </div>);
        }
        break;
    }

    let AddtionalInfo = null;
    if (this.props.defConf_lock_level != 0)
      AddtionalInfo =
        <div key="AddtionalInfo" className={"s overlay overlayright HXA"} style={{ width: "100px", backgroundColor: "black" }}
          onClick={()=>this.props.ACT_DefConf_Lock_Level_Update(0)}>
          {<LockOutlined />}
          {" 鎖等級:" + this.props.defConf_lock_level}
        </div>



    log.debug("[render] APP_DEFCONF_MODE");
    return (
      <div className="overlayCon HXF">
        {this.state.modal_view === undefined?null:
        <Modal
          key="<<>>O"
          {...this.state.modal_view}
          visible={true}
          onCancel={(param) => {
            if (this.state.modal_view!==undefined && 
              this.state.modal_view.onCancel !== undefined) {
              this.state.modal_view.onCancel(param);
            }
            this.setState({ ...this.state, modal_view: undefined });
          }}
          height={"95%"}
          width={"95%"}
          style={{top:"30px"}}
          onOk={(param) => {
            if (this.state.modal_view.onOk !== undefined) {
              this.state.modal_view.onOk(param);
            }
            this.setState({ ...this.state, modal_view: undefined });
          }}>
          {this.state.modal_view === undefined ? null : this.state.modal_view.view_update()}
        </Modal>}
        <ComponentBoundary name="DefConfCanvas" fallbackHeight="60vh">
          <CanvasComponent_rdx addClass="layout width12" onCanvasInit={(canvas) => { this.ec_canvas = canvas }} />
        </ComponentBoundary>

        <DefConfImageSwitcher />

        {/* THE PANEL PAINTS ITS OWN GROUND.
            .overlay is `background: none` by design -- it is the class every
            floating panel in the app uses, and most of them sit over something
            plain. This one sits over the CAMERA IMAGE, so the frame read
            straight through the labels: on a dark or busy part of the part
            being inspected, "min_strength" and the section headers simply were
            not there. Reported twice from the bench.
            Scoped here rather than in basis.css: the fix belongs to the screen
            whose background is a photograph, not to every overlay. */}
        <div key={substate} className={"s overlay scroll shadow1 MenuAnim " + menu_height}
             style={{ background: '#f2f2f2' }}>
          {MenuSet}
        </div>

        {AddtionalInfo}

        {/* AN OLD DEF SAYS SO, ON THE PICTURE, WHERE THE WORK HAPPENS.
            A def still on the sig360 localizer is not broken -- it inspects --
            so nothing anywhere said it was the old one. The migration button
            existed, in the localizer section of a scrolling settings panel,
            which is not somewhere anyone looks unless they already know to.
            Top centre, over the image, because that is where the operator is
            looking and because the banner has to be impossible to mistake for
            part of the recipe.
            Only in NEUTRAL and only unlocked: in a drawing substate it would
            cover the work, and under a lock the reducer drops the DefConf
            actions the migration is made of -- silently. A button that quietly
            does nothing is worse than no button. */}
        {substate === UIAct.UI_SM_STATES.DEFCONF_MODE_NEUTRAL
          && defModelPath
          && this.props.defConf_lock_level == 0
          && (this.props.edit_info.locating_engine || 'sig360') !== 'shape_based' &&
          <div key="oldver" style={{
                 // Below the timing caption, not on top of it. At 8 it covered
                 // the second status line, which is where the per-phase
                 // breakdown lands -- the one number being read while a def is
                 // being worked on.
                 position: 'absolute', top: 44, left: '50%', transform: 'translateX(-50%)',
                 zIndex: 20, display: 'flex', alignItems: 'center', gap: 10,
                 background: '#a8071a', color: '#fff', borderRadius: 4,
                 padding: '6px 12px', fontSize: 13,
                 boxShadow: '0 2px 8px rgba(0,0,0,0.35)' }}>
            <span>這是舊版定位（sig360）</span>
            <Button size="small" danger type="primary" data-testid="upgrade-def"
              style={{ background: '#fff', color: '#a8071a', borderColor: '#fff' }}
              onClick={() => Modal.confirm({
                title: '升級到 shape-based 定位（v2）',
                width: 520,
                content: (<div style={{ lineHeight: 1.9 }}>
                  <div>定位引擎換成 shape_based,量測設定、anchor_corner 等其他設定<b>原封不動</b>。</div>
                  <div style={{ marginTop: 8 }}>升級後<b>還沒有特徵點</b> —— 接著會直接打開
                    「SBM定位設定」,在裡面按<b>生成特徵點</b>,然後<b>重新存檔</b>。</div>
                  <div style={{ marginTop: 8, color: '#a8071a' }}>
                    這兩步沒做完,這個 def 會退回用 sig360 檢驗,而畫面上看不出差別。</div>
                  <div style={{ marginTop: 8, color: '#888', fontSize: 12 }}>
                    存檔前都還沒有寫到磁碟,不想要的話直接離開不要存就好。</div>
                </div>),
                okText: '升級並開始設定', cancelText: '先不要',
                onOk: () => this.props.ACT_Migrate_To_Shape(this.props.edit_info),
              })}>升級</Button>
          </div>}

        {/* SHAPE_BASED, BUT STORED THE OLD WAY.
            A def whose features were saved before the format carried its own
            ROI windows keeps the coarse feature levels and nothing else. The
            core no longer loads that -- it refuses and asks for a regenerate --
            but without this the operator finds out when the machine will not
            run, which is the worst moment and the least informative place.
            One key tells them apart: a cache written before the change has no
            `roi`. edit_info.__shape_cache is the def's own, carried in by the
            load (InspectionEditorLogic reads it out of @__SBM_INFO__), so this
            is what the file says and not what some later step recomputed. */}
        {/* NOT gated on the lock, unlike the migration banner above.
            That one is hidden under a lock because pressing it dispatches
            DefConf actions the reducer would drop -- a button that quietly does
            nothing. This one states a fact about the FILE: the machine will not
            load it. That is true whether or not the def is locked for editing,
            and it is exactly what someone looking at a locked recipe needs to
            know. The button is the part that needs an unlocked def, so the
            button is what the lock hides. */}
        {substate === UIAct.UI_SM_STATES.DEFCONF_MODE_NEUTRAL
          && defModelPath
          && (this.props.edit_info.locating_engine || 'sig360') === 'shape_based'
          && this.props.edit_info.__shape_cache
          && !this.props.edit_info.__shape_cache.roi &&
          <div key="oldfmt" data-testid="oldfmt-banner" style={{
                 position: 'absolute', top: 44, left: '50%', transform: 'translateX(-50%)',
                 zIndex: 20, display: 'flex', alignItems: 'center', gap: 10,
                 background: '#a8071a', color: '#fff', borderRadius: 4,
                 padding: '6px 12px', fontSize: 13,
                 boxShadow: '0 2px 8px rgba(0,0,0,0.35)' }}>
            <span>這個 def 只有粗定位特徵(舊格式),機台會跑但精度只有幾個像素</span>
            {this.props.defConf_lock_level != 0
              ? <span style={{ opacity: 0.85 }}>（解鎖後可重新產生）</span>
              : <Button size="small" danger type="primary" data-testid="oldfmt-def"
              style={{ background: '#fff', color: '#a8071a', borderColor: '#fff' }}
              onClick={() => Modal.confirm({
                title: '重新產生特徵點（舊格式）',
                width: 540,
                content: (<div style={{ lineHeight: 1.9 }}>
                  <div>這個 def 存的是<b>舊格式的特徵</b>:只有粗比對用的特徵層,
                    沒有 ROI 精修要用的視窗和選點。</div>
                  <div style={{ marginTop: 8 }}>機台會載入它,但<b>只做粗定位</b>(誤差幾個像素,
                    不是 sub-pixel)。檢驗畫面會一直顯示「只有粗定位」的提示,報告裡
                    <code>locate.code</code> 是 <code>coarse_only</code>。</div>
                  <div style={{ marginTop: 8 }}>接著會打開「SBM定位設定」,在裡面按
                    <b>生成特徵點</b>,然後<b>重新存檔</b>。存好之後 def 會自己帶著需要的
                    像素,連參考影像都不用放在旁邊。</div>
                  <div style={{ marginTop: 8, color: '#888', fontSize: 12 }}>
                    整批轉換用 tools/webctl/upgrade_defs.mjs —— 它走的是同一條路,
                    而且會檢查重抽的特徵跟原本存的一致才寫回。</div>
                </div>),
                okText: '開始設定', cancelText: '先不要',
                onOk: () => this.props.ACT_Open_Shape_Studio(),
              })}>重新產生</Button>}
          </div>}



      </div>
    );
  }
}


const mapDispatchToProps_APP_DEFCONF_MODE = (dispatch, ownProps) => {
  return {
    ACT_DefFileName_Update: (newName) => { dispatch(DefConfAct.DefFileName_Update(newName)) },
    ACT_DefFileTag_Update: (newInfo) => { dispatch(DefConfAct.DefFileTag_Update(newInfo)) },

    ACT_EDIT_TAR_ELE_TRACE_UPDATE: (keyTrace) => { dispatch(DefConfAct.Edit_Tar_Ele_Trace_Update(keyTrace)) },
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) => { dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj)) },
    ACT_EDIT_TAR_UPDATE: (targetObj) => { dispatch(DefConfAct.Edit_Tar_Update(targetObj)) },
    ACT_Shape_List_Reset: () => { dispatch(DefConfAct.Shape_List_Update([])) },
    ACT_Shape_List_Update:(newlist)=>dispatch(DefConfAct.Shape_List_Update(newlist)),

    ACT_SUCCESS: (arg) => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS)) },
    ACT_Fail: (arg) => { dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL)) },
    ACT_EXIT: (arg) => { dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT)) },

    ACT_DefConf_Lock_Level_Update: (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) },

    ACT_Migrate_To_Shape: (edit_info) => migrateDefToShapeBased(dispatch, edit_info),
    ACT_Open_Shape_Studio: () => openShapeStudio(),
    ACT_Def_Model_Path_Update: (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) },
    ACT_WS_SEND_BPG: (...args) => dispatch(UIAct.EV_WS_SEND_BPG(...args)),
    ACT_ClearImage: () => { dispatch(UIAct.EV_WS_Image_Update(null)) },
    ACT_Shape_Decoration_ID_Order_Update: (shape_id_order) => { dispatch(DefConfAct.Shape_Decoration_ID_Order_Update(shape_id_order)) },

    ACT_Shape_Decoration_Control_Margin_Info_Update: (extra_info) => { dispatch(DefConfAct.Shape_Decoration_Control_Margin_Info_Update(extra_info)) },
    ACT_Matching_Angle_Margin_Deg_Update: (deg) => { dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) },
    ACT_Matching_Face_Update: (faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) },//-1(back)/0(both)/1(front)
    ACT_DefFileHash_Update: (hash) => { dispatch(DefConfAct.DefFileHash_Update(hash)) },
    // Same as the hook copy above: a refusal must reach the operator. Two
    // definitions of "save a def" is one too many, but they are wired into
    // different components; if a third appears, extract it.
    ACT_Report_Save: (id, fileName, content) => {
      let act = UIAct.EV_WS_SEND_BPG(id, "SV", 0,
        { filename: fileName },
        content,
        { resolve: (darr) => {
            const ack = (darr || []).map((p) => p && p.data)
              .find((d) => d && d.cmd === 'SV');
            if (ack && ack.ACK === false)
              Modal.error({ title: '存檔被拒絕',
                content: (ack.errMsg || '核心沒有給原因') + '　（' + fileName + '）' });
          }, reject: () => {
            Modal.error({ title: '存檔沒有回應',
              content: '核心沒有回覆存檔結果,檔案可能沒有寫入。' });
          } }
      )
      dispatch(act);
    },
    ACT_Cache_Img_Save: (id, fileName) => {
      dispatch(UIAct.EV_WS_SEND_BPG(id, "SV", 0,
        { filename: fileName, type: "__CACHE_IMG__" }
      ));
    },
    ACT_SIG360_Extraction: (report) => dispatch(UIAct.EV_WS_SIG360_Extraction(report))
    ,
    DISPATCH: (act) => {
      dispatch(act)
    },
  }
}

const mapStateToProps_APP_DEFCONF_MODE = (state) => {
  return {
    c_state: state.UIData.c_state,
    edit_tar_info: state.UIData.edit_info.edit_tar_info,
    shape_list: state.UIData.edit_info._obj.shapeList,
    Info_decorator: state.UIData.edit_info.__decorator,
    CORE_ID: state.ConnInfo.CORE_ID,
    edit_info: state.UIData.edit_info,
    defConf_lock_level: state.UIData.defConf_lock_level,
    DICT:state.UIData.DICT,
  }
};

const APP_DEFCONF_MODE_rdx = connect(
  mapStateToProps_APP_DEFCONF_MODE,
  mapDispatchToProps_APP_DEFCONF_MODE)(APP_DEFCONF_MODE);

export default APP_DEFCONF_MODE_rdx;
