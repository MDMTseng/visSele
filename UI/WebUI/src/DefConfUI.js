'use strict'


import { connect } from 'react-redux'
import React, { useState, useEffect, useRef, useMemo } from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
import ComponentBoundary from './component/ComponentBoundary';
import { TagOptions_rdx, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
import { Shape_Attr_Fill } from 'UTIL/InspectionEditorLogic';
import { fieldFor, getShapeModule } from 'JSSRCROOT/shapes';
import { applyFieldChange } from 'JSSRCROOT/shapes/_schemaHelpers';
import { loadDefWithImageFallback } from 'UTIL/DefLoadWithImageFallback';
import { buildSchema } from 'JSSRCROOT/shapes/propertySheet';
let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
import DragSortableList from 'react-drag-sortable'
import ReactResizeDetector from 'react-resize-detector';
import { DEF_EXTENSION, BPG_ExpCalc, CameraTransferCtrl as CameraCtrl } from 'UTIL/BPG_Protocol';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {
  round as roundX, websocket_autoReconnect,
  websocket_reqTrack, dictLookUp,
  GetObjElement, Exp2PostfixExp, PostfixExpCalc,
  defFileGeneration
} from 'UTIL/MISC_Util';

import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.defconf');
import dclone from 'clone';
import Modal from "antd/lib/modal";
import Menu from "antd/lib/menu";
import Button from "antd/lib/button";
import Icon from 'antd/lib/icon';
import Tag from 'antd/lib/tag';
import Table  from 'antd/lib/table';
import Checkbox from "antd/lib/checkbox";
import InputNumber from 'antd/lib/input-number';
import Input from 'antd/lib/input';
const { CheckableTag } = Tag;
const { TextArea } = Input;
import Divider from 'antd/lib/divider';
import Dropdown from 'antd/lib/dropdown'
import Slider from 'antd/lib/slider';
import Popover from 'antd/lib/popover';


import NumPad from 'react-numpad';
import { useSelector,useDispatch } from 'react-redux';
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
      type="number" step="0.0001" pattern="^[-+]?[0-9]?(\.[0-9]*){0,1}$"
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




function parseCheckExpressionValid(postExp, idArr) {

  let funcSet = {}

  //the magic 333333+0.00001*idx is to prevent easy calc collision that causes NaN result
  idArr.forEach((id,idx) => { funcSet[id] = 3337333+0.00001*idx });

  let res = BPG_ExpCalc(postExp,funcSet);

  
  // console.log(res);
  res=res.flat();
  // console.log(postExp,res);
  return (res.length==1)&&res[0]==res[0];
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
  
        ctrlMarg[key]=[...ctrlMarg[key]].map(m=>{
          delete m.name;
          delete m.update;
          return m;
        })
      })
    }

    {
      dump.measureInfo =[...dump.measureInfo].map(m=>{
        delete m.update;
        return m;
      })
    }


    return dump;
  }
  useEffect(() => {
    set_MeasureInfo(dclone(measureInfo));
    //in the following deffile editing, some new measure might appear/delete
    //adjust control_margin_info coording to it

    if(control_margin_info!==undefined)
      set_control_margin_info(control_margin_info);

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

            return (
              <NumPad.Number onChange={(value)=>{
                
                let new_obj={...objInfo};
                let parseNum =value;
                parseNum=toFixedNum(parseNum,5);
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
                // objInfo.update(new_obj);
              }} value={value}>
                <Input style={{ width: '100%' }} value={value}/>
              </NumPad.Number>
            );
            return 
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
          obj.update=(newObj)=>{
            let newMarginInfo = {..._control_margin_info};
            
            let tarIdx=newMarginInfo[text].findIndex(m=>m.id==newObj.id);
            if(tarIdx!==-1)
            {
              newMarginInfo[text][tarIdx]=newObj;
            }
            // console.log(_control_margin_info);
            // console.log(newMarginInfo);
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
        
        update:(newObj)=>{
          let newMeasureInfo = [..._measureInfo];
          newMeasureInfo[idx]={...shape,
            value:newObj.value,
            rank:newObj.rank,
            USL:newObj.USL,
            LSL:newObj.LSL,
            UCL:newObj.UCL,
            LCL:newObj.LCL,
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
      let isAvail = parseCheckExpressionValid(postExp, measureIDInfo.map(info => info.id_exp));
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
    let isAvail = parseCheckExpressionValid(postExp, idMap);
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

function SettingUI({})
{
  
  const defConf_lock_level = useSelector(state => state.UIData.defConf_lock_level);
  
  const edit_info = useSelector(state => state.UIData.edit_info);
  const dispatch = useDispatch();
  const ACT_DefConf_Lock_Level_Update= (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) };
  const ACT_Matching_Angle_Margin_Deg_Update= (deg) => dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) ;
  const ACT_IntrusionSizeLimitRatio_Update= (ratio) => { dispatch(DefConfAct.IntrusionSizeLimitRatio_Update(ratio)) };//0~1
    
  const ACT_Matching_Face_Update=(faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) };//-1(back)/0(both)/1(front)
  const ACT_Matching_Version_Update=(v) => { dispatch(DefConfAct.Matching_Version_Update(v)) };// 1=legacy, 2=phase2 dual-sig
  const ACT_Inspection_Downsample_Update=(n) => { dispatch(DefConfAct.Inspection_Downsample_Update(n)) };// 1..8 (core caps at 4 today)

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



    <Divider orientation="left">{DICT.defConf.intrusion_size_limit_ratio}</Divider>,
    <Slider
      min={0}
      step={0.01}
      max={1}
      included={true}
      marks={
        {
          0: '',
          0.01: '',
          0.05: '',
          0.1: '0.1',
          0.3: '0.2',
          0.5: '0.5',
          1: '',
        }
      }
      onChange={ACT_IntrusionSizeLimitRatio_Update}
      value={edit_info.intrusionSizeLimitRatio}
    />,
    <NumberAccInput
      min={0}
      max={1}
      value={edit_info.intrusionSizeLimitRatio}
      onChange={ACT_IntrusionSizeLimitRatio_Update}
    />,

    <Divider orientation="left">sig360 perf</Divider>,
    <Checkbox
      checked={edit_info.matching_version === 2}
      onChange={() => ACT_Matching_Version_Update(edit_info.matching_version === 2 ? 1 : 2)}
    >v2 matcher (morph-boundary dual-sig)</Checkbox>,
    <span>&nbsp;downsample&nbsp;</span>,
    <NumberAccInput
      min={1}
      max={8}
      value={edit_info.inspection_downsample || 1}
      onChange={ACT_Inspection_Downsample_Update}
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


  
function loadDefFile(defModelPath,ACT_DefConf_Lock_Level_Update,ACT_WS_SEND_BPG,CORE_ID,dispatch)
{
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
  const ACT_Cache_Img_Save= (id, fileName) =>
    dispatch(UIAct.EV_WS_SEND_BPG(id, "SV", 0,
      { filename: fileName, type: "__CACHE_IMG__" }
    ))


  const ACT_DefConf_Lock_Level_Update= (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) };
  const ACT_DefFileName_Update=(newName) => { dispatch(DefConfAct.DefFileName_Update(newName)) };
  const ACT_DefFileTag_Update=(newInfo) => { dispatch(DefConfAct.DefFileTag_Update(newInfo)) };
  const ACT_DefFileHash_Update= (hash) => { dispatch(DefConfAct.DefFileHash_Update(hash)) };

  const ACT_Def_Model_Path_Update= (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) };
  const ACT_IntrusionSizeLimitRatio_Update= (ratio) => { dispatch(DefConfAct.IntrusionSizeLimitRatio_Update(ratio)) };//0~1
    
  const ACT_Report_Save=(id, fileName, content) => {
    let act = UIAct.EV_WS_SEND_BPG(id, "SV", 0,
      { filename: fileName },
      content
    )
    dispatch(act);
  };
  const ACT_Matching_Angle_Margin_Deg_Update= (deg) => { dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) };
  const ACT_Matching_Face_Update=(faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) };//-1(back)/0(both)/1(front)
    
  const ACT_WS_SEND_BPG= (...args) => dispatch(UIAct.EV_WS_SEND_BPG(...args));

  const edit_info = useSelector(state => state.UIData.edit_info);
  const FILE_default_camera_setting = useSelector(state => state.UIData.FILE_default_camera_setting);
  const defConf_lock_level = useSelector(state => state.UIData.defConf_lock_level);
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  const DefFile_DB_W_ID = useSelector(state => state.ConnInfo.DefFile_DB_W_ID);
  const DefFile_DB_SEND= (data,return_cb) => dispatch(UIAct.EV_WS_SEND_PLAIN(DefFile_DB_W_ID,data,return_cb));
  const shape_list = useSelector(state => state.UIData.edit_info._obj.shapeList);
  const defModelPath = edit_info.defModelPath;
  const machine_custom_setting = useSelector(state => state.UIData.machine_custom_setting);

  const [fileSelectedCallBack,setFileSelectedCallBack]=useState(undefined);
  
  
  const [modal_view,setModal_view]=useState(undefined);

  const [cacheDef,setCacheDef]=useState(undefined);
  const [nowInspdata,setNowInspdata]=useState(undefined);



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
          setModal_view({
            onOk: () => {
              ACT_EXIT();
              
              setModal_view(undefined);
            },
            onCancel: () => { console.log("onCancel");setModal_view(undefined); },
            title: dictLookUp("WARNING", DICT),
            view: DICT.defConf.exit_warning_change_is_made
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
    </BASE_COM.IconButton>]);
      

  function startQuickInsp(inspMode=machine_custom_setting.InspectionMode||"CI")
  {//FI/CI


    let _CameraCtrl = new CameraCtrl({
      ws_ch: (STData, promiseCBs) => {
        ACT_WS_SEND_BPG(CORE_ID, "ST", 0, STData, undefined,promiseCBs);
      },
      ev_frameRateChange: (fps) => {
      }
    });
    if(inspMode=="CI")
    {
      _CameraCtrl.setCameraFrameRate(15);
    }
    else if(inspMode=="FI")
    {
      _CameraCtrl.setCameraSpeed_HIGHEST();
    }





    let deffile = defFileGeneration(edit_info);
    deffile.intrusionSizeLimitRatio=1;
    setCacheDef(deffile);

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
      ACT_WS_SEND_BPG(CORE_ID, "CI", 0, 
      { 
        _PGID_: _PGID_, 
        _PGINFO_: { keep: true }, 
        definfo: undefined     
      }, undefined,
      {
        resolve:(darr,mainFlow)=>{
        },
        reject:(e)=>{
        }
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
      footer:null,
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
      text="save" onClick={() => {
        if (defConf_lock_level > 2) return;
        setFileSavingCallBack((prevs,props)=> (folderInfo, fileName, existed) => {
            log.debug("[file-exists]", { folderInfo, fileName, existed });
            
            let fileNamePath = folderInfo.path + "/" + fileName.replace('.' + DEF_EXTENSION, "");

            var enc = new TextEncoder();
            let report = defFileGeneration(edit_info);
            if(report.name===undefined || report.name.length==0)
            {
              report.name=fileName;
              ACT_DefFileName_Update(fileName)
            }
            ACT_DefFileHash_Update(report.featureSet_sha1);
            log.info("[action] report-save");
            ACT_Report_Save(CORE_ID, fileNamePath + '.' + DEF_EXTENSION, enc.encode(JSON.stringify(report, null, 2)));
            log.info("[action] cache-img-save");
            ACT_Cache_Img_Save(CORE_ID, fileNamePath);


            ACT_Def_Model_Path_Update(fileNamePath);
            setFileSavingCallBack(undefined);

            DefFile_DB_SEND(report).
            then((ret) => console.log('then', ret)).
            catch((ret) => console.log("catch", ret));

          });
      }} />,
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
      key="TAKE"
      text="take" onClick={() => {

        function setModal_viewAsWait()
        {
          setModal_view({
            onCancel:()=>{},//disable the cancel
            footer:[],
              
            view:"請稍後...."
            })
        }

        function triggerSnapExam(trigger_type=0,timeout=-1,doReset=true)
        {
          
          setModal_viewAsWait();
          ACT_DefConf_Lock_Level_Update(0);
          new Promise((resolve, reject) => {
            ACT_WS_SEND_BPG(CORE_ID, "EX", 0, {
              trigger_type,
              timeout,
              img_property:{
                down_samp_level:IMG_LOAD_DOWNSAMP_LEVEL
              }
            },
              undefined, { resolve, reject });
            //setTimeout(() => reject("Timeout"), 3000)
          })
            .then((pkts) => {
              
              let SS=pkts.find(pkt=>pkt.type=="SS");
              if(SS.data.ACK==true)
              {              
                let acts=pkts.map(pkt => BPG_Protocol.map_BPG_Packet2Act(pkt)).filter(act => act !== undefined);
                dispatch({
                  type: "ATBundle",
                  ActionThrottle_type: "express",
                  data: acts
                })
                setModal_view(undefined);
              }
              else
              {
                setModal_view({
                  footer:[],
                  view:"圖像獲取失敗"
                  })
              }

            })
            .catch((err) => {
              log.info(err);

              setModal_view({
                footer:[],
                view:"圖像獲取異常"
                })
            })
          if(doReset)
            ACT_Shape_List_Reset();
        }

        let triggerTimeout=10000;
        let doRESET=true
        setModal_view({
          footer:<>
              <Button key="back" onClick={()=>{triggerSnapExam(0,-1,false)}}>
                立即
              </Button>
              <Button key="trigger5S" type="primary" onClick={()=>{triggerSnapExam(2,triggerTimeout,false)}}>
                {triggerTimeout/1000}s內觸發
              </Button>
              {"<<<不重置"}
              <br/>
              <Button key="back" onClick={()=>{triggerSnapExam(0,-1,true)}}>
                立即
              </Button>
              <Button key="trigger5S" type="primary" onClick={()=>{triggerSnapExam(2,triggerTimeout,true)}}>
                {triggerTimeout/1000}s內觸發
              </Button>
              <Button danger onClick={()=>setModal_view(undefined)}>
                取消
              </Button>
              </>,
            onOk: () => {
              setModal_view(undefined);

              
            },
            onCancel: () => { console.log("onCancel");setModal_view(undefined);},
            title: dictLookUp("WARNING", DICT),
            view: DICT.defConf.do_you_want_to_reset_def
          })
      }} />,
    (defConf_lock_level !=0) ? null :
    <BASE_COM.IconButton
      iconType={<VerticalAlignTopOutlined />}
      dict={DICT}
      addClass="layout palatte-purple-8 vbox"
      key="INST_CHECK"
      text="INST_CHECK" onClick={() => {
        let deffile = defFileGeneration(edit_info);
        deffile.intrusionSizeLimitRatio=1;
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
              let insp_reports = GetObjElement(RP,["data","reports",0,"reports"]);
              if(insp_reports!==undefined&&  insp_reports.length>0)
              {
                let insp_rep = insp_reports[0];
                edit_info._obj.setsig360infoCenter({x:insp_rep.cx,y:insp_rep.cy});
                let modList = shape_list.map((shape,idx)=>{
                  let mod_shape=dclone(shape);
                  
                  edit_info._obj.ShapeAdjustsWithInspectionResult(mod_shape,shape_list, insp_rep,true);

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

    
    <BASE_COM.IconButton
      iconType={<ThunderboltOutlined />}
      dict={DICT}
      addClass="layout palatte-purple-8 vbox width12"
      key="NOW"
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
            <Button key="CI_MODE" onClick={_ => startQuickInsp("CI")}>
              檢驗{machine_custom_setting.InspectionMode=="CI"?<StarOutlined />:null}
            </Button>
            <Button key="FI_MODE" onClick={_ => startQuickInsp("FI")}>
              全檢{machine_custom_setting.InspectionMode=="FI"?<StarOutlined />:null}
            </Button>
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
        path={DefFileFolder} visible={fileSelectedCallBack !== undefined}
        BPG_Channel={(...args) => ACT_WS_SEND_BPG(CORE_ID, ...args)}
        onFileSelected={(filePath, fileInfo) => {
          fileSelectedCallBack(filePath, fileInfo);
        }}
        onOk={(folderPath) => {
        }}
        onCancel={() => {
          setFileSelectedCallBack(undefined)
        }}
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.' + DEF_EXTENSION)}
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
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.' + DEF_EXTENSION)}
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
            IGNORE_IMAGE_FIT_TO_SCREEN={true}
            ALLOW_CONTROL_DOWN_SAMPLING_LEVEL={true}
            BPG_Channel={(...args)=>ACT_WS_SEND_BPG(CORE_ID, ...args) }
            downSampleFactor={FILE_default_camera_setting.down_samp_factor||1}
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
function GenTarEditUI({ edit_tar_info, shape_list, Info_decorator, ec_canvas, ACT_EDIT_TAR_ELE_TRACE_UPDATE }) {
    
    const DICT = useSelector(state => state.UIData.DICT);
  
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
        UIArr.push(<PSheet
          key="propertySheet"
          shape={edit_tar}
          shapeList={shape_list}
          dict={DICT}
          dictTheme={edit_tar.type}
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


class APP_DEFCONF_MODE extends React.Component {

  componentDidMount() {
    
    let defModelPath = this.props.edit_info.defModelPath;
    loadDefFile(defModelPath,this.props.ACT_DefConf_Lock_Level_Update,this.props.ACT_WS_SEND_BPG,this.props.CORE_ID,this.props.DISPATCH);

    
    this.props.ACT_WS_SEND_BPG(this.props.CORE_ID, "ST", 0,
    {
      CameraSetting: { ROI:[0,0,99999,99999] },
      // JPEG-encode the streamed dataview frames (full-res, uncropped — the
      // downsamp_level is already 1 on the IM request). Quality 85 is
      // visually lossless and ~10x smaller than raw RGBA. 0 disables (raw).
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
          <div key="MEASURE" className="s width8 lblue vbox">MEASURE</div>,
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

          <div key="EDIT_Text" className="s width8 lblue vbox">EDIT</div>,
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

              let mmpp = this.props.edit_info._obj.getsig360info_mmpp();
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
                deffile.intrusionSizeLimitRatio=1;
  

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
                      let insp_reports = GetObjElement(RP,["data","reports",0,"reports"]);
                      if(insp_reports.length>0)
                      {
                        let insp_rep = insp_reports[0];
                        let mod_shape=dclone(this.props.edit_tar_info);
                        
                        this.props.edit_info._obj.ShapeAdjustsWithInspectionResult(mod_shape,this.props.shape_list, insp_rep,true);

                        
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

        <div key={substate} className={"s overlay scroll shadow1 MenuAnim " + menu_height}>
          {MenuSet}
        </div>

        {AddtionalInfo}



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

    ACT_Def_Model_Path_Update: (path) => { dispatch(UIAct.Def_Model_Path_Update(path)) },
    ACT_WS_SEND_BPG: (...args) => dispatch(UIAct.EV_WS_SEND_BPG(...args)),
    ACT_ClearImage: () => { dispatch(UIAct.EV_WS_Image_Update(null)) },
    ACT_Shape_Decoration_ID_Order_Update: (shape_id_order) => { dispatch(DefConfAct.Shape_Decoration_ID_Order_Update(shape_id_order)) },

    ACT_Shape_Decoration_Control_Margin_Info_Update: (extra_info) => { dispatch(DefConfAct.Shape_Decoration_Control_Margin_Info_Update(extra_info)) },
    ACT_Matching_Angle_Margin_Deg_Update: (deg) => { dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(deg)) },
    ACT_Matching_Face_Update: (faceSetup) => { dispatch(DefConfAct.Matching_Face_Update(faceSetup)) },//-1(back)/0(both)/1(front)
    ACT_IntrusionSizeLimitRatio_Update: (ratio) => { dispatch(DefConfAct.IntrusionSizeLimitRatio_Update(ratio)) },//0~1
    ACT_DefFileHash_Update: (hash) => { dispatch(DefConfAct.DefFileHash_Update(hash)) },
    ACT_Report_Save: (id, fileName, content) => {
      let act = UIAct.EV_WS_SEND_BPG(id, "SV", 0,
        { filename: fileName },
        content
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
