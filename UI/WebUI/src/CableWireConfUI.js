'use strict'


import { connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
import DragSortableList from 'react-drag-sortable'
import ReactResizeDetector from 'react-resize-detector';
import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import { ReduxStoreSetUp } from 'REDUX_STORE_SRC/redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {
  round as roundX, websocket_autoReconnect,
  websocket_reqTrack, dictLookUp, undefFallback,
  GetObjElement, Exp2PostfixExp, PostfixExpCalc,
  ExpCalcBasic, ExpValidationBasic,defFileGeneration
} from 'UTIL/MISC_Util';

import * as log from 'loglevel';
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
import Dropdown from 'antd/lib/Dropdown'
import Slider from 'antd/lib/Slider';
import Popover from 'antd/lib/Popover';

const SubMenu = Menu.SubMenu;

import { useSelector,useDispatch } from 'react-redux';
import { 
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


} from '@ant-design/icons';



class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  triggerROISelect()
  {

  }

  
  ec_canvas_EmitEvent(event) {
    switch (event.type) {

    }
  }

  componentDidUpdate(prevProps) {
  }

  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.CableWireDef_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);

    if(this.props.onCanvasInit!==undefined)
      this.props.onCanvasInit(this.ec_canvas);

    this.onResize(500, 500)
    //this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  // SetIM(imgInfo)
  // {
  //   this.ec_canvas.SetImg(imgInfo);
  // }
  // SetRP(imgInfo)
  // {
  //   this.ec_canvas.SetImg(imgInfo);
  // }

  updateCanvas(ec_state, props = this.props) {

    if (this.ec_canvas !== undefined && props.IM!==undefined) {
      {
        // console.log(props.IM);
        this.ec_canvas.SetImg(props.IM);
        this.ec_canvas.SetOPModeInfo(props.modeInfo);
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 20) return;
    console.log(this.windowSize.width ,this.windowSize.height,width, height );
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
      this.updateCanvas(this.props.c_state);
    }
  }

  componentWillUpdate(nextProps, nextState) {
    this.updateCanvas(nextProps.c_state, nextProps);
    console.log(">>>");
  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}


const CABLE_DEF_EXT="cabDef";

function debounce(func, delay=250) {
  let timer = null;
 
  return () => {
    let context = this;
    let args = arguments;
 
    clearTimeout(timer);
    timer = setTimeout(() => {
      func.apply(context, args);
    }, delay)
  }
}

function CABLE_WIRE_CONF_MODE_rdx({onExtraCtrlUpdate})
{
  let _cur = useRef({});
  let _this=_cur.current;
  const edit_info = useSelector(state => state.UIData.edit_info);
  const dispatch = useDispatch();
  const ACT_DefConf_Lock_Level_Update= (level) => { dispatch(DefConfAct.DefConf_Lock_Level_Update(level)) };
    
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  const ACT_WS_SEND= (...args) => dispatch(UIAct.EV_WS_SEND(WS_ID, ...args));
  
  const [CurIM,setCurIM]=useState(undefined);
  const [CurRP,setCurRP]=useState(undefined);
  const [fileSavingContext,setFileSavingContext]=useState(undefined);
  const [fileBrowsingContext,setFileBrowsingContext]=useState(undefined);

  const [defPath,setDefPath]=useState("data/default."+CABLE_DEF_EXT);


  const [canvasModeInfo,setCanvasModeInfo]=useState({
    type:EC_CANVAS_Ctrl.CableWireDef_Canvas_mode.neutral,
    data:undefined
  });

  const [_defInfo_,set_defInfo_]=useState({
    type:"gen",
    SMax:255,
    SMin:0,
    VMax:255,
    VMin:0,
  });

  function fetchParam()
  {
    ACT_WS_SEND( "ST", 0,
      { 
        InspectionParam:[{
          get_param:true
        }]
      },undefined, { 
        resolve:(pkts)=>{
          let DT=pkts.find(pkt=>pkt.type=="DT");
          console.log("-------DT",DT,"   pkts:",pkts);
          if(DT!==undefined && DT.data!==undefined&& DT.data[0]!==undefined)
          {
            set_defInfo_({..._defInfo_,
              ...DT.data[0]
            });
          }
        }, 
        reject:()=>{} 
      })
  }
  function TriggerNewResult(doTakeNew,add_defInfo,cur_defInfo=_defInfo_)
  {

    let defInfo= {...cur_defInfo,...add_defInfo};
    // ACT_WS_SEND("ST", 0, {
    //   CameraTriggerShutter:true
    // }, undefined);

    console.log(defInfo);
    ACT_WS_SEND("II", 0, {
      imgsrc:(doTakeNew==false)?"__CACHE_IMG__":undefined,
      img_property:{
        down_samp_level:2,
        scale:3,
        calibInfo:{
          type:"NA"
        }
      },
      definfo:defInfo
    }, undefined,
    { resolve:(pkts)=>{
      log.info(pkts);
      let RP = pkts.find(pkt=>pkt.type=="RP");
      let IM = pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM!==undefined)
      {

        setCurRP(RP);
        let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
        setCurIM(img_pros.data);
        // setInterval(()=>{
        //   TriggerNewResult();
        // },10000)
      }

    }, reject:(pkts)=>{
      log.info(pkts);
    }});


    if(doTakeNew==true)
    {
      fetchParam();
    }
  }

  function InpectionAgain(inspectionStage=1,defInfo)
  {
    _this.curStage=inspectionStage;
    TriggerNewResult(false,{inspectionStage},defInfo);
  }
  function TakeNew(inspectionStage=1,defInfo)
  {
    _this.curStage=inspectionStage;
    TriggerNewResult(true,{inspectionStage},defInfo);
  }

  useEffect(() => {


    // TriggerNewResult(_defInfo_);
    if(onExtraCtrlUpdate!==undefined)
      onExtraCtrlUpdate({
        save:(path)=>{
          setFileSavingContext({
            onOK:(folderInfo, fileName, existed) => {
              let fileNamePath = folderInfo.path + "/" + fileName.replace("."+CABLE_DEF_EXT, "");
              setFileSavingContext(undefined);
              var enc = new TextEncoder();
              ACT_BIN_Save(fileNamePath+"."+CABLE_DEF_EXT, enc.encode(JSON.stringify(_defInfo_, null, 2)));
              console.log("ACT_Cache_Img_Save");
              ACT_Cache_Img_Save(fileNamePath+".png");
  
            },
            path:"data/",
            defaultName:"default."+CABLE_DEF_EXT
          });
        },
        open:(path)=>{

          setFileBrowsingContext({

            onFileSelected:(file_path,file) => {
              // console.log(">>",file_path,file);
  
              let defModelPath = file_path.replace("."+CABLE_DEF_EXT, "");
  
              ACT_WS_SEND("LD", 0,
                {
                  deffile: defModelPath + '.' + CABLE_DEF_EXT,
                  imgsrc: defModelPath,
                  down_samp_level:10
                },
                undefined, { 
                resolve:(pkts)=>{
                  let DF = pkts.find(pkt=>pkt.type=="DF");
                  let IM = pkts.find(pkt=>pkt.type=="IM");
                  if(DF!==undefined && IM!=undefined)
                  {
  
                    set_defInfo_(DF.data);
                    let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
                    setCurIM(img_pros.data);
                    setFileBrowsingContext(undefined);
                  }
                }, reject:()=>{} });
            },
            path:"data/"
          });
        },
        take_new:()=>{
          TakeNew(0);
        }
        // InpectionAgain,
        // paramAdjust_Info:{
          
        // },
        // paramAdjust:(key,value)=> set_defInfo_({..._defInfo_,[key]:value}),
      })
    return () => {
      
      // ACT_WS_SEND("CI", 0, { _PGID_: 10004, _PGINFO_: { keep: false } });
    };
  }, [])



  const ACT_Cache_Img_Save= (fileName) => ACT_WS_SEND( "SV", 0,{ filename: fileName, type: "__CACHE_IMG__" } )
  
  const ACT_BIN_Save=( fileName, content)  => ACT_WS_SEND( "SV", 0,{ filename: fileName},content );


  // console.log(_defInfo_);
  let sliders_stage1_gen=()=>//make it lazy
  ["HFrom","HTo","SMax","SMin","VMax","VMin","boxFilter1_Size","boxFilter1_thres","boxFilter2_Size","boxFilter2_thres"].map(key=>[
    <div>
      {key+":"+_defInfo_[key]}
      
      <Slider key={key+"_slider"}
        min={0}
        max={255}
        onChange={(value) => {
          if(_defInfo_[key]===undefined)return;
          let newDef = {..._defInfo_,[key]:value};
          // this.MatchingEnginParamSet(key,value);
          set_defInfo_(newDef);

          InpectionAgain( _this.curStage,newDef)
        }}
        value={_defInfo_[key]}
        step={1}
      />
    </div>
    ])

  return <div className="overlayCon HXF">
    {/* sss */}
    <CanvasComponent key="kk" addClass="height12" IM={CurIM} modeInfo={canvasModeInfo}/>
    {fileSavingContext===undefined?null:<BPG_FileSavingBrowser key="BPG_FileSavingBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileSavingContext.path} visible={true}
        defaultName={fileSavingContext.defaultName}
        BPG_Channel={(...args) => ACT_WS_SEND(...args)}

        onOk={(folderInfo, fileName, existed) => {
          fileSavingContext.onOK(folderInfo, fileName, existed);

        }}
        onCancel={() => {
          if(fileSavingContext.onCancel!==undefined)
            fileSavingContext.onCancel(folderInfo, fileName, existed);
          else
          { 
            setFileSavingContext(undefined);
          }
          // setFileSavingCallBack(undefined);
        }}
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.'+CABLE_DEF_EXT)}
      />}

    {fileBrowsingContext===undefined?null:<BPG_FileBrowser key="BPG_FileBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileBrowsingContext.path} visible={true}
        BPG_Channel={(...args) => ACT_WS_SEND(...args)}
        onFileSelected={fileBrowsingContext.onFileSelected}
        onCancel={() => {
          if(fileBrowsingContext.onCancel!==undefined)
            fileBrowsingContext.onCancel(folderInfo, fileName, existed);
          else
          { 
            setFileBrowsingContext(undefined);
          }
          // setFileSavingCallBack(undefined);
        }}
        fileFilter={(fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes('.'+CABLE_DEF_EXT)}
      />}




    <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      
      <Button onClick={()=>InpectionAgain(0)}>TAKE0</Button>
      <Button onClick={()=>InpectionAgain(2)}>TAKE2</Button>
      <Button onClick={()=>InpectionAgain(3)}>TAKE3</Button>
      <Button onClick={()=>InpectionAgain(-1)}>TAKEX</Button>




      <Menu
        style={{ width: 256 }}
        defaultSelectedKeys={['1']}
        // defaultOpenKeys={['sub1']}
        openKeys={['sub1','subSecBlock']}
        mode="inline"  selectable={false}
      >
        <SubMenu
          key="sub1"
          title={
            <span>
              <span>Stage1</span>
            </span>
          }
        >
          
          {sliders_stage1_gen()}
        </SubMenu>
        <SubMenu key="subSecBlock" title="SecM">
          <Menu.Item key="5" onClick={()=>{
            console.log("sdsdsdd");
            setCanvasModeInfo({
              type:EC_CANVAS_Ctrl.CableWireDef_Canvas_mode.cableRegionSetUp,
              data:undefined
            });
            }}>cableRegionSetUp</Menu.Item>
        </SubMenu>
        <SubMenu key="subX" title="Sec2">
          <Menu.Item key="5">Option 5</Menu.Item>
          <Menu.Item key="6">Option 6</Menu.Item>
          <SubMenu key="sub3" title="Submenu">
            <Menu.Item key="7">Option 7</Menu.Item>
            <Menu.Item key="8">Option 8</Menu.Item>
          </SubMenu>
        </SubMenu>
        <SubMenu
          key="sub4"
          title={
            <span>
              <SettingOutlined />
              <span>Navigation Three</span>
            </span>
          }
        >
          <Menu.Item key="9">Option 9</Menu.Item>
          <Menu.Item key="10">Option 10</Menu.Item>
          <Menu.Item key="11">Option 11</Menu.Item>
          <Menu.Item key="12">Option 12</Menu.Item>
        </SubMenu>
      </Menu>
      {



      }
    </div>

  </div>;
}


export default CABLE_WIRE_CONF_MODE_rdx;