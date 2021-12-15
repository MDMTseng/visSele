'use strict'


import { connect } from 'react-redux'
import React, { useState, useEffect, useRef } from 'react';
import $CSSTG from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import { TagOptions_rdx, tagGroupsPreset, CustomDisplaySelectUI } from './component/rdxComponent.jsx';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from 'UTIL/MathTools';

import { Machine } from 'xstate';

let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
let BPG_FileSavingBrowser = BASE_COM.BPG_FileSavingBrowser;
let BPG_FileBrowser_varify_info = BASE_COM.BPG_FileBrowser_varify_info;
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
  ExpCalcBasic, ExpValidationBasic,defFileGeneration,xstate_GetCurrentMainState,
  LocalStorageTools
} from 'UTIL/MISC_Util';

let xState=xstate_GetCurrentMainState;

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
  DownOutlined, TrophyOutlined,
  SubnodeOutlined
} from '@ant-design/icons';



const LOCALSTORAGE_KEY="CABLE.RecentDefFiles"


function getLocalStorage_RecentFiles(key=LOCALSTORAGE_KEY)
{
  let LocalS_RecentDefFiles =LocalStorageTools.getlist(key);
  // LocalS_RecentDefFiles = LocalS_RecentDefFiles.filter(BPG_FileBrowser_varify_info);
  // console.log(LocalS_RecentDefFiles);
  return LocalS_RecentDefFiles;
}

function appendLocalStorage_RecentFiles(key=LOCALSTORAGE_KEY,fileInfo)
{
  
  return LocalStorageTools.appendlist(key,fileInfo,
    (ls_fileInfo,idx) =>
      (idx<100)&&//Do list length limiting
      (ls_fileInfo.path != fileInfo.path));

}



class CableWireDef_CanvasComponent extends EC_CANVAS_Ctrl.EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.reset();
    this.EmitEvent = (event) => { log.info(event); };
    this.tmp_canvas= document.createElement('canvas');
    this.regionSelect=undefined;
    
  }


  canvasResize(canvas,width,height)
  {
    width = Math.floor(width);
    height = Math.floor(height);
    canvas.width = width;
    canvas.height = height;
  }


  reset()
  {
    let mmpp=1;
    this.db_obj = {
      cameraParam:{
        mmpb2b:1,
        ppb2b:1
      }
    };

    
  }

  onmouseup(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 0;

    if(this.regionSelect===undefined)
    {
      this.camera.EndDrag();
  
      this.debounce_zoom_emit();
    }
    else
    {
      // console.log(this.regionSelect);
      {
        let wMat = this.worldTransform();
        //log.debug("this.camera.matrix::",wMat);
        let worldTransform = new DOMMatrix().setMatrixValue(wMat);
        let worldTransform_inv = worldTransform.invertSelf();
        this.regionSelect.pt1= this.VecX2DMat(this.regionSelect.pcvst1, worldTransform_inv);
        this.regionSelect.pt2= this.VecX2DMat(this.regionSelect.pcvst2, worldTransform_inv);


      }



      this.regionSelect.onSelect(this.regionSelect);
      this.regionSelect=undefined;
    }
    this.ctrlLogic();
    this.draw();
  }


  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    let doDraw=true;
    if(this.regionSelect===undefined)
    {
      if (doDragging) {
        if (this.mouseStatus.status == 1) {
          
          doDraw=true;
          this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });
        }
  
      }
    }
    else
    {
      this.regionSelect.pcvst2={ x: this.mouseStatus.x, y: this.mouseStatus.y};
    }


    if(doDraw)
    {
      this.ctrlLogic();
      this.draw();
    }
  }

  scaleHeadToFitScreen()
  {
    console.log(this.singleReportInfo);

    this.camera.SetOffset(this.canvas.width/2,this.canvas.height/2);
    if(this.singleReportInfo==undefined)return;
    let sr = this.singleReportInfo.report
    {

      let width=Math.hypot(sr.vec_btm.x,sr.vec_btm.y);

      
      let curScale = this.camera.GetCameraScale();
      this.camera.Scale(this.canvas.width/width/curScale);
    }

    
  }
  UserRegionSelect(onSelect)
  {
    this.regionSelect=onSelect!==undefined?{onSelect}:undefined;//able to cancel it
  }
  onmousedown(evt) 
  {

    super.onmousedown(evt);
    // this.doDragging=false;
    if(this.regionSelect!==undefined)
    {
      this.regionSelect.pcvst1=this.regionSelect.pcvst2=
        { x: this.mouseStatus.x, y: this.mouseStatus.y};
    }

  }

  SetIM(IM)
  {
    
    if (IM === undefined || IM == this.IM) return;
    this.IM=IM;


    this.secCanvas.width = IM.width;
    this.secCanvas.height = IM.height;
    this.secCanvas_rawImg = IM.img;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(this.secCanvas_rawImg, 0, 0);

  }


  SetRP(RP)
  {
    this.RP=RP;
    console.log(RP);
  }


  SetUI_Info(UI_INFO)
  {
    this.UI_INFO=UI_INFO;
    console.log(UI_INFO);
  }


  draw() {

    
    if(this.IM===undefined || this.IM.scale === undefined)
    {
      return;
    }
    // console.log(this.c_state);
    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    };
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);

    ctx.setLineDash([]);
    let scale = this.IM.scale;
    let mmpp=1;
    let mmpp_mult = scale * mmpp;
    this.g={
      ctx,
      base_img:ctx2nd,
      base_img_scale:scale,
      mmpp_mult,
      worldTransform:this.cur_worldTransform,
      worldTransform_inv:this.cur_worldTransform_inv
    }

    // console.log(this.IM,this.RP,this.UI_INFO);


    {
      ctx.imageSmoothingEnabled = false;//scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      let curScale=this.camera.GetCameraScale();
      if(true){

        ctx.scale(mmpp_mult, mmpp_mult);
        if (this.IM.offsetX !== undefined && this.IM.offsetY !== undefined) {
          // ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
          ctx.translate((this.IM.offsetX-0.5*(scale)) / scale, (this.IM.offsetY-0.5*(scale)) / scale);
        }
        // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
        ctx.drawImage(this.secCanvas, 0, 0);
        
        ctx.strokeStyle = "rgba(120, 120, 120,30)";
        
        ctx.lineWidth = 10/curScale/mmpp_mult;
        this.rUtil.drawImageBoundaryGrid(ctx,this.IM,100000/curScale);
  
      }
      
      ctx.restore();
    }

    if(this.drawHook!==undefined)
    {
      this.drawHook(false,this.g,this);
    }



  }

  DownloadCanvasAsImage(canvas,fileName){
    let downloadLink = document.createElement('a');
    downloadLink.setAttribute('download', fileName);
    canvas.toBlob(function(blob) {
      let url = URL.createObjectURL(blob);
      downloadLink.setAttribute('href', url);
      downloadLink.click();
    });
  }
  ctrlLogic() {

    //let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();

    this.cur_worldTransform=worldTransform;
    this.cur_worldTransform_inv=worldTransform_inv;
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);

    // this.EmitEvent(EV_UI_Canvas_Mouse_Location(mouseOnCanvas2));
    // console.log(ifOnMouseLeftClickEdge,mouseOnCanvas2);

    
    this.g={
      worldTransform,
      worldTransform_inv,
      mouseOnCanvas:mouseOnCanvas2,
      pmouseOnCanvas:pmouseOnCanvas2,

      mouseStatus:this.mouseStatus,
      mouseEdge:ifOnMouseLeftClickEdge,

    }
    if(this.drawHook!==undefined)
    {
      this.drawHook(true,this.g,this);
    }

  }
}







class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  

  componentDidUpdate(prevProps) {
  }

  componentDidMount() {
    this.ec_canvas = new CableWireDef_CanvasComponent(this.refs.canvas);

    if(this.props.onCanvasInit!==undefined)
      this.props.onCanvasInit(this);

    this.onResize(500, 500)
    //this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  draw()
  {
    this.ec_canvas.draw();
  }

  update(IM,RP,UI_Info) {

    if (this.ec_canvas !== undefined) {
      {
        this.ec_canvas.SetIM(IM);
        this.ec_canvas.SetRP(RP);
        this.ec_canvas.SetUI_Info(UI_Info);
        this.draw();
      }
    }
  }


  updateCanvas(ec_state, props = this.props) {
    this.update(props.IM,props.RP,props.UI_Info);
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 20) return;
    console.log(this.windowSize.width ,this.windowSize.height,width, height );
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
    }
  }

  componentWillUpdate(nextProps, nextState) {

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


var enc = new TextEncoder();

const CABLE_DEF_EXT="cabDef";

const configState={
  normal:"normal",
  shape_positioning:"shape_positioning",
  template_matching:"template_matching",
  fine_positioning:"fine_positioning",
  test:"test"
}


function GenMatching_rdx({onExtraCtrlUpdate})
{
  let _cur = useRef({
    prjRootPath:"data/TMP/",
    init:true,
    canvasRenderInfo:{
      test:{
        pt:{x:100,y:100},
        draw:(info,g,canvas_obj)=>{

          // let info = _this.canvasRenderInfo.test;
          // let ctx = g.ctx;
          // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          // let psize = 2*canvas_obj.rUtil.getPointSize();
          // ctx.lineWidth = psize/3;
          
          // canvas_obj.rUtil.drawcross(ctx, info.pt,psize);

        }
      }
    }
  });
  let _this=_cur.current;

  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  
  const dispatch = useDispatch();
  const ACT_WS_SEND_BPG= (...args) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, ...args));

  const [fileSavingContext,setFileSavingContext]=useState(undefined);
  const [fileBrowsingContext,setFileBrowsingContext]=useState(undefined);

  const [defPath,setDefPath]=useState("data/default."+CABLE_DEF_EXT);

  const [canComp,setCanComp]=useState(undefined);
  const [confState,setConfState]=useState(configState.shape_positioning);

  const [_defInfo_,set_defInfo_]=useState({
    type:"FM_GenMatching",
    "template_pyramid":	[{
			"x":	738,
			"y":	644,
			"w":	116,
			"h":	100,
			"pyramid_level":	0,
			"angle":	0,
			"anchor":	{
				"x":	798.763916015625,
				"y":	691.59228515625
			},
			"features":	[{
					"label":	4,
					"theta":	275.19363403320312,
					"x":	1,
					"y":	54
				}, {
					"label":	4,
					"theta":	273.5960693359375,
					"x":	7,
					"y":	54
				}, {
					"label":	4,
					"theta":	94.599334716796875,
					"x":	14,
					"y":	27
				}, {
					"label":	4,
					"theta":	93.944488525390625,
					"x":	24,
					"y":	28
				}, {
					"label":	4,
					"theta":	273.94448852539062,
					"x":	11,
					"y":	54
				}, {
					"label":	4,
					"theta":	95.3137435913086,
					"x":	7,
					"y":	27
				}, {
					"label":	4,
					"theta":	95.3137435913086,
					"x":	10,
					"y":	27
				}, {
					"label":	4,
					"theta":	275.31375122070312,
					"x":	15,
					"y":	55
				}, {
					"label":	4,
					"theta":	93.990203857421875,
					"x":	2,
					"y":	26
				}, {
					"label":	4,
					"theta":	276.25823974609375,
					"x":	2,
					"y":	15
				}, {
					"label":	4,
					"theta":	94.058631896972656,
					"x":	39,
					"y":	29
				}, {
					"label":	4,
					"theta":	276.0081787109375,
					"x":	23,
					"y":	17
				}, {
					"label":	4,
					"theta":	275.2615966796875,
					"x":	12,
					"y":	16
				}, {
					"label":	5,
					"theta":	294.197021484375,
					"x":	36,
					"y":	57
				}, {
					"label":	4,
					"theta":	274.57315063476562,
					"x":	8,
					"y":	16
				}, {
					"label":	6,
					"theta":	325.45895385742188,
					"x":	40,
					"y":	60
				}, {
					"label":	4,
					"theta":	95.4765625,
					"x":	1,
					"y":	71
				}, {
					"label":	4,
					"theta":	94.697914123535156,
					"x":	5,
					"y":	72
				}, {
					"label":	4,
					"theta":	94.697914123535156,
					"x":	8,
					"y":	72
				}, {
					"label":	4,
					"theta":	275.6298828125,
					"x":	37,
					"y":	18
				}, {
					"label":	4,
					"theta":	94.898307800292969,
					"x":	17,
					"y":	73
				}, {
					"label":	4,
					"theta":	94.204627990722656,
					"x":	25,
					"y":	74
				}, {
					"label":	4,
					"theta":	276.53134155273438,
					"x":	49,
					"y":	19
				}, {
					"label":	3,
					"theta":	237.75070190429688,
					"x":	90,
					"y":	4
				}, {
					"label":	3,
					"theta":	251.26278686523438,
					"x":	96,
					"y":	1
				}, {
					"label":	4,
					"theta":	261.46978759765625,
					"x":	100,
					"y":	0
				}, {
					"label":	2,
					"theta":	216.7669677734375,
					"x":	83,
					"y":	10
				}, {
					"label":	4,
					"theta":	279.16192626953125,
					"x":	86,
					"y":	99
				}, {
					"label":	0,
					"theta":	182.48905944824219,
					"x":	75,
					"y":	53
				}, {
					"label":	7,
					"theta":	167.67985534667969,
					"x":	75,
					"y":	66
				}, {
					"label":	4,
					"theta":	275.19363403320312,
					"x":	5,
					"y":	92
				}, {
					"label":	4,
					"theta":	274.59933471679688,
					"x":	9,
					"y":	92
				}, {
					"label":	0,
					"theta":	181.97453308105469,
					"x":	75,
					"y":	60
				}, {
					"label":	4,
					"theta":	275.43951416015625,
					"x":	38,
					"y":	93
				}, {
					"label":	4,
					"theta":	271.3636474609375,
					"x":	29,
					"y":	93
				}, {
					"label":	4,
					"theta":	270,
					"x":	26,
					"y":	93
				}, {
					"label":	4,
					"theta":	274.82000732421875,
					"x":	18,
					"y":	93
				}, {
					"label":	4,
					"theta":	272.06961059570312,
					"x":	22,
					"y":	93
				}, {
					"label":	4,
					"theta":	272.06961059570312,
					"x":	33,
					"y":	93
				}, {
					"label":	4,
					"theta":	98.325057983398438,
					"x":	90,
					"y":	80
				}, {
					"label":	4,
					"theta":	272.79217529296875,
					"x":	43,
					"y":	94
				}, {
					"label":	4,
					"theta":	272.79217529296875,
					"x":	46,
					"y":	94
				}, {
					"label":	4,
					"theta":	272.79217529296875,
					"x":	52,
					"y":	94
				}, {
					"label":	4,
					"theta":	272.79217529296875,
					"x":	55,
					"y":	95
				}, {
					"label":	4,
					"theta":	278.53021240234375,
					"x":	67,
					"y":	96
				}, {
					"label":	4,
					"theta":	278.53021240234375,
					"x":	72,
					"y":	97
				}, {
					"label":	4,
					"theta":	280.75103759765625,
					"x":	75,
					"y":	97
				}, {
					"label":	4,
					"theta":	275.70977783203125,
					"x":	62,
					"y":	95
				}]
		}, {
			"x":	369,
			"y":	322,
			"w":	58,
			"h":	50,
			"pyramid_level":	1,
			"angle":	0,
			"anchor":	{
				"x":	399.3819580078125,
				"y":	345.796142578125
			},
			"features":	[{
					"label":	4,
					"theta":	274.15057373046875,
					"x":	2,
					"y":	27
				}, {
					"label":	4,
					"theta":	94.288421630859375,
					"x":	1,
					"y":	13
				}, {
					"label":	4,
					"theta":	93.337844848632812,
					"x":	12,
					"y":	14
				}, {
					"label":	4,
					"theta":	95.05645751953125,
					"x":	3,
					"y":	36
				}, {
					"label":	4,
					"theta":	277.91351318359375,
					"x":	0,
					"y":	7
				}, {
					"label":	4,
					"theta":	275.60845947265625,
					"x":	9,
					"y":	8
				}, {
					"label":	3,
					"theta":	237.788818359375,
					"x":	45,
					"y":	2
				}, {
					"label":	4,
					"theta":	277.176513671875,
					"x":	43,
					"y":	49
				}, {
					"label":	4,
					"theta":	273.15423583984375,
					"x":	26,
					"y":	47
				}, {
					"label":	4,
					"theta":	273.69070434570312,
					"x":	18,
					"y":	46
				}, {
					"label":	4,
					"theta":	273.69070434570312,
					"x":	21,
					"y":	47
				}, {
					"label":	4,
					"theta":	270,
					"x":	14,
					"y":	46
				}, {
					"label":	4,
					"theta":	95.709785461425781,
					"x":	45,
					"y":	40
				}, {
					"label":	4,
					"theta":	274.76287841796875,
					"x":	3,
					"y":	46
				}, {
					"label":	4,
					"theta":	273.81338500976562,
					"x":	7,
					"y":	46
				}, {
					"label":	4,
					"theta":	276.88095092773438,
					"x":	54,
					"y":	50
				}, {
					"label":	4,
					"theta":	94.573158264160156,
					"x":	58,
					"y":	42
				}]
		}],
	  "locatingBlocks":	[{
			"x":	756,
			"y":	408,
			"w":	117,
			"h":	107
		}, {
			"x":	749,
			"y":	651,
			"w":	109,
			"h":	107
		}, {
			"x":	980,
			"y":	408,
			"w":	63,
			"h":	79
		}],

  });

  const [editRegionInfo,setEditRegionInfo]=useState(undefined);




  function Reset()
  {
    setFileSavingContext(undefined);
    setFileBrowsingContext(undefined);
    set_defInfo_({
      type:"FM_GenMatching",
    });
    if(canComp!==undefined)
      canComp.reset();
  }

  function fetchParam()
  {
    ACT_WS_SEND_BPG( "ST", 0,
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
  function inspStage(inspectionStage)
  {
    ACT_WS_SEND_BPG( "ST", 0,
      { 
        InspectionParam:[{
          inspectionStage
        }]
      },undefined, { 
        resolve:(pkts)=>{}, 
        reject:()=>{} 
      })
  }

  function TriggerNewResult(doTakeNew,add_defInfo,cur_defInfo=_defInfo_,resolve=_=>_,reject=_=>_)
  {

    //let defInfo= {...cur_defInfo,...add_defccInfo};
    console.log(add_defInfo);
    let defInfo={...cur_defInfo,...add_defInfo}


    // console.log(defInfo);
    ACT_WS_SEND_BPG("II", 0, {
      imgsrc:(doTakeNew==false)?"__CACHE_IMG__":undefined,
      img_property:{
        down_samp_level:3
      },
      definfo:defInfo
    }, undefined,
    { resolve:(pkts)=>{
      log.info(pkts);
      let RP = pkts.find(pkt=>pkt.type=="RP");
      let IM = pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM!==undefined)
      {
        _this.RP=RP;

        
        IM.img = new ImageData(IM.image, IM.width);
        _this.IM=IM;
        resolve({RP,IM});
        canComp.update(_this.IM,_this.RP,{});
      }

      }, reject:(pkts)=>{
        log.info(pkts);
        reject(pkts);
      }});


    if(doTakeNew==true)
    {
      fetchParam();
    }
  }



  const ACT_File_Save = (filePath, content,promiseCBs) => {
    let act = UIAct.EV_WS_SEND_BPG(CORE_ID, "SV", 0,
      {filename:filePath},
      content,promiseCBs
    )
    dispatch(act);
  }
  function FileSave(filename,type, encoded_content,resolve=_=>_,reject=_=>_)
  {

    // console.log(defInfo);
    ACT_WS_SEND_BPG("SV", 0, {
      filename,type,
      make_dir:true
    }, encoded_content,
    { resolve:(pkts)=>{
        let OBJ= pkts.reduce((obj,pkt)=>{
          obj[pkt.type]=pkt;
          return obj;
        },{});
        resolve(OBJ);
      }, reject:(pkts)=>{
        log.info(pkts);
        reject(pkts);
      }
    });

  }

  useEffect(() => {
    return () => {
    };
  },[])

  useEffect(() => {
    if(canComp===undefined)return;
    
     
    canComp.ec_canvas.EmitEvent=(event)=>{
  
      console.log(event);
      switch (event.type) {
        case "onRegionInfoUpdate":
          
          if(event.data==undefined )
          {
            setEditRegionInfo(undefined);
          }
          else
          {
            console.log(event)
            setEditRegionInfo(event.data.region);
          }
          break;


        case "onUserRegionSelect":
          
          console.log(event)
          break;


      }
    };

    canComp.ec_canvas.drawHook=(ctrl_or_draw,g,canvas_obj)=>{
      if(ctrl_or_draw==true)
      {
        Object.keys(_this.canvasRenderInfo).forEach(id=>{
          let idInfo=_this.canvasRenderInfo[id];
          if(idInfo.ctrl===undefined)return;
          if(Array.isArray(idInfo.ctrl))
          {
            idInfo.ctrl.forEach(ctrlh=>ctrlh(idInfo,g,canvas_obj))
          }
          else
            idInfo.ctrl(idInfo,g,canvas_obj);
        })
        return;
      }

      Object.keys(_this.canvasRenderInfo).forEach(id=>{
        let idInfo=_this.canvasRenderInfo[id];
        if(idInfo.draw===undefined)return;
        
        if(Array.isArray(idInfo.draw))
        {
          idInfo.draw.forEach(drawh=>drawh(idInfo,g,canvas_obj))
        }
        else
          idInfo.draw(idInfo,g,canvas_obj);
      })
    }
    
    TriggerNewResult(true,
      {insp_type:"NOP"},_defInfo_,
      (ret_info)=>{
        if(_this.init==true && ret_info.IM!==undefined)
        {
          //save 
          FileSave(_this.prjRootPath+"IMG1.png","__CACHE_IMG__",undefined,
          (obj)=>{
            console.log(">>>>",obj);
          });
          _this.init=false;
        }
      },
      ()=>{
        
      });


  }, [canComp])



  if(canComp!==undefined)
  {
    canComp.draw();
  }

  let launchRegionSelect=(onSelectComplete,drawRegionRect=true)=>
  {
    if(onSelectComplete===undefined)
    {
      canComp.ec_canvas.UserRegionSelect(undefined);
      return;
    }

      
    canComp.ec_canvas.UserRegionSelect((evt)=>{
      // console.log(evt)

      let x = evt.pt1.x;
      let y = evt.pt1.y;

      let w = evt.pt2.x-x;
      let h = evt.pt2.y-y;
      if(w<0)
      {
        x+=w;
        w=-w;
      }
      if(h<0)
      {
        y+=h;
        h=-h;
      }

      evt.x=x;
      evt.y=y;
      evt.w=w;
      evt.h=h;

      onSelectComplete(evt);
      // canComp.ec_canvas.drawHook=undefined;
    });



    canComp.draw();

  }
  
  let launchPointSelect=(onSelectComplete)=>
  {
    if(onSelectComplete===undefined)
    {
      canComp.ec_canvas.UserRegionSelect(undefined);
      return;
    }

    {
      _this.canvasRenderInfo.pointSelect={draw:(info,g,canvas_obj)=>{
        let ctx = g.ctx;

        ctx.strokeStyle = 'red';
        ctx.lineWidth = 5;
        if(canvas_obj.regionSelect.pcvst2===undefined)
        {
          return;
        }
        let pt2 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);

        ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

        let psize = 2*canvas_obj.rUtil.getPointSize();
        
        ctx.lineWidth = psize/3;
        canvas_obj.rUtil.drawcross(ctx, pt2,psize);
      }}
    }
    canComp.ec_canvas.UserRegionSelect((evt)=>{
      // console.log(evt)
      onSelectComplete(evt);
      delete _this.canvasRenderInfo.pointSelect;
    });
    canComp.draw();

  }



  let confUI=null;

  switch(confState)
  {
    case configState.normal:
      confUI=<>

        <Button onClick={()=>{
          setConfState(configState.shape_positioning);
          
          TriggerNewResult(false,{
            insp_type:"NOP",
          });
        }}>Edit rough positioning</Button>


        <Button onClick={()=>{
          setConfState(configState.template_matching);
          
          TriggerNewResult(false,{
            insp_type:"NOP",
          });
        }}>template_matching</Button>
        <br/>

      </>;
    break;
    case configState.test:
      confUI=<>
        <Button onClick={()=>{
          setConfState(configState.normal);
        }}>{"<"}</Button>

        <Button onClick={()=>{
          let i=0;
          let sec1=10;
          for(i=0;i<sec1;i++)
          {
            let ix=i;
            setTimeout(()=>{
              console.log(ix);
              TriggerNewResult(false,{
                insp_type:"image_binarization",
                thres:ix*255/(sec1-1)
              });
            },ix*100);
          }
        }}>AAA</Button>

        <Button onClick={()=>{
          launchRegionSelect((evt)=>{
          console.log(evt);
          });
        }}>BBB</Button>

        
        <Button onClick={()=>{
          launchPointSelect((evt)=>{
            console.log(evt);
          });
        }}>Point</Button>
      </>;




    break;
    case configState.shape_positioning:


      function runMatch(doFineAdj=false)
      {

        let addedDef={
          insp_type:"shape_features_matching",
          param_type:"load_template_image",
          image_src_path:_this.prjRootPath+"IMG1.png"
        }
        
        if(doFineAdj==false)
        {
          addedDef.locatingBlocks=[];//if NOT do fine adj, then overwrite the locatingBlocks as empty
        }


        TriggerNewResult(false,addedDef,_defInfo_,
        (obj)=>{
          console.log(obj);
          let matchResults = obj.RP.data.matchResults;
          
          // set_defInfo_({..._defInfo_,matchResults});
          _this.canvasRenderInfo.shape_positioning={
            matchResults,
            draw:(info,g,canvas_obj)=>{
              let ctx = g.ctx;
              let psize = 2*canvas_obj.rUtil.getPointSize();
              ctx.lineWidth = psize/3;

              matchResults.forEach((ms)=>{

                ctx.fillStyle =
                ctx.strokeStyle = "rgba(50, 200, 50,0.8)";
                if(ms.refine_score<0.5)
                {
                  ctx.fillStyle =
                  ctx.strokeStyle = "rgba(200, 0, 0,0.8)";
                }

                canvas_obj.rUtil.drawcross(ctx, ms,psize);
                let lineLength=100;
                let offsetx=lineLength*Math.cos(ms.angle*3.14159/180);
                let offsety=lineLength*Math.sin(ms.angle*3.14159/180);
                ctx.beginPath();
                ctx.moveTo(ms.x, ms.y);
                ctx.lineTo(ms.x+offsetx, ms.y+offsety);
                ctx.stroke();
                


                ctx.font = '20px serif';
                ctx.fillText('s:'+ms.similarity.toFixed(2) +" cl:"+ms.class_id, ms.x, ms.y);
                ctx.fillText('ang:'+ms.angle.toFixed(2)+" rf_score:"+ms.refine_score, ms.x, ms.y+50);
              })
            }

          }





        },
        ()=>{

        });
      }

      function CtrlHook_selectRegion(info,g,canvas_obj)
      {
        if(canvas_obj.regionSelect===undefined)
        {
          info.sel_region=undefined
          return;
        }

        if(canvas_obj.regionSelect.pcvst1===undefined || canvas_obj.regionSelect.pcvst2===undefined)
        {
          return;
        }

        let pt1 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
        let pt2 = canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
          
        let x,y,w,h;

        x=pt1.x;
        w=pt2.x-pt1.x;

        y=pt1.y;
        h=pt2.y-pt1.y;

    
        if(w<0){
          x+=w;
          w=-w;
        }
        
        if(h<0){
          y+=h;
          h=-h;
        }
        info.sel_region={
          x,y,w,h
        }
        
      }

      
      function DrawHook_selectRegionDraw(info,g,canvas_obj)
      {
        if(info.sel_region===undefined)return;
        let ctx = g.ctx;

        ctx.strokeStyle = 'red';
        ctx.lineWidth = 5;

        let x = info.sel_region.x;
        let y = info.sel_region.y;
        let w = info.sel_region.w;
        let h = info.sel_region.h;
        ctx.beginPath();
        let LineSize = canvas_obj.rUtil.getIndicationLineSize();
        ctx.setLineDash([LineSize*10,LineSize*3,LineSize*3,LineSize*3]);
        ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
        ctx.lineWidth = LineSize;
        ctx.rect(x,y,w,h);
        ctx.stroke();
        ctx.closePath();

        ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
        let psize = 2*canvas_obj.rUtil.getPointSize();
        ctx.lineWidth = psize/3;
        canvas_obj.rUtil.drawcross(ctx, {x:x+w/2,y:y+h/2},psize);

      }
      function DrawHook_featureDraw(info,g,canvas_obj)
      {
        let ctx = g.ctx;
        let psize = 2*canvas_obj.rUtil.getPointSize();
        ctx.lineWidth = psize/3;

        let py_mult=1;
        info.Template_py.forEach((temp,idx)=>{
          temp.features.forEach((feature)=>{
            
            let x=py_mult*(temp.x+feature.x);
            let y=py_mult*(temp.y+feature.y);

            

            ctx.strokeStyle = "rgba("+0+", "+175/(idx+1)+", 0,0.5)";
            if(info.featureDraw_use_sel_region==true  && info.sel_region!==undefined)
            {
              delete feature._del;
              if(info.sel_region.x<x && info.sel_region.y<y &&
                info.sel_region.x+info.sel_region.w>x && 
                info.sel_region.y+info.sel_region.h>y)//check region includs 
              {
                
                ctx.strokeStyle = "rgba("+175/(idx+1)+", "+0+", 0,0.5)";
                feature._del=true;
              }
            }

            canvas_obj.rUtil.drawcross(ctx, {x,y},psize);
          })
          py_mult*=2;
        });
      
        
      }



      confUI=<>
        <Button onClick={()=>{
          setConfState(configState.normal);
          delete _this.canvasRenderInfo.shape_positioning;
        }}>{"<"}</Button>

        <Button onClick={()=>{
          
          _this.canvasRenderInfo.shape_positioning={
            Template_py:_defInfo_.template_pyramid,
            ctrl:CtrlHook_selectRegion,
            draw:[DrawHook_selectRegionDraw,DrawHook_featureDraw]
          }

          launchRegionSelect((evt)=>{
            console.log(evt);
            
            let x = evt.x;
            let y = evt.y;
            let w = evt.w;
            let h = evt.h;

            TriggerNewResult(false,{
              insp_type:"extract_shape_features",
              ROI:{x,y,w,h},
              anchor:{x:x+w/2,y:y+h/2}
            },_defInfo_,
            (obj)=>{
              console.log(obj);

              _this.canvasRenderInfo.shape_positioning.Template_py=obj.RP.data.TemplatePyramid;

              set_defInfo_({..._defInfo_,template_pyramid:obj.RP.data.TemplatePyramid});
            },
            ()=>{

            });
          });
        }}>ShapeRegion</Button>




        <Button onClick={()=>{
          let Template_py = _defInfo_.template_pyramid;
          
          _this.canvasRenderInfo.shape_positioning={
            Template_py:_defInfo_.template_pyramid,
            featureDraw_use_sel_region:true,
            ctrl:CtrlHook_selectRegion,
            draw:[DrawHook_selectRegionDraw,DrawHook_featureDraw]
          }

          launchRegionSelect((evt)=>{
            let Template_py = _this.canvasRenderInfo.shape_positioning.Template_py;
            Template_py.forEach((temp,idx)=>{
              temp.features=temp.features.filter(feature=>feature._del!==true);
            });

            _this.canvasRenderInfo.shape_positioning.Template_py=Template_py;

            set_defInfo_({..._defInfo_,template_pyramid:Template_py});

          });
        }}>SelRmTempFeature</Button>









        <Button onClick={()=>{

          _this.canvasRenderInfo.shape_positioning={
            defInfo:_defInfo_,
            ctrl:CtrlHook_selectRegion,
            draw:[
              DrawHook_selectRegionDraw,
              (info,g,canvas_obj)=>{
              let ctx = g.ctx;
              let psize = 2*canvas_obj.rUtil.getPointSize();
              ctx.lineWidth = psize/4;

              ctx.setLineDash([]);
              let LineSize = canvas_obj.rUtil.getIndicationLineSize();
              
              let locatingBlocks=info.defInfo.locatingBlocks||[];
              locatingBlocks.forEach((region)=>{
                ctx.beginPath();
                ctx.strokeStyle = "rgba(179, 100, 0,0.5)";
                ctx.lineWidth = LineSize;
                ctx.rect(region.x, region.y, region.w, region.h);
                ctx.stroke();
                ctx.closePath();

              })
            }]
          }
            
          launchRegionSelect((evt)=>{
            
            let locatingBlocks=_defInfo_.locatingBlocks||[];
            let x=Math.ceil(evt.x);
            let y=Math.ceil(evt.y);
            let w=Math.ceil(evt.w);
            let h=Math.ceil(evt.h);


            let new_locatingBlocks=[...locatingBlocks,
              {x,y,w,h}];
            let new_defInfo={..._defInfo_,locatingBlocks:new_locatingBlocks};
            set_defInfo_(new_defInfo);
            _this.canvasRenderInfo.shape_positioning.defInfo=new_defInfo;

          });
        }}>LocaBlock</Button>

        <Button onClick={()=>{

          _this.canvasRenderInfo.shape_positioning={
            locatingBlocks:_defInfo_.locatingBlocks||[],
            ctrl:CtrlHook_selectRegion,
            draw:[
              DrawHook_selectRegionDraw,
              (info,g,canvas_obj)=>{
              let ctx = g.ctx;
              let psize = 2*canvas_obj.rUtil.getPointSize();
              ctx.lineWidth = psize/4;

              ctx.setLineDash([]);
              let LineSize = canvas_obj.rUtil.getIndicationLineSize();
              
              let locatingBlocks=info.locatingBlocks||[];
              locatingBlocks.forEach((region)=>{
                ctx.beginPath();
                
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                
                if(info.sel_region!==undefined)
                {
                  delete region._del;
                  if(info.sel_region.x<region.x && info.sel_region.y<region.y &&
                    info.sel_region.x+info.sel_region.w>region.x+region.w && 
                    info.sel_region.y+info.sel_region.h>region.y+region.h)//check region includs 
                  {
                    
                    ctx.strokeStyle = "rgba(200, 0, 0,0.5)";
                    region._del=true;
                  }
                }

                ctx.lineWidth = LineSize;
                ctx.rect(region.x, region.y, region.w, region.h);
                ctx.stroke();
                ctx.closePath();

              })
            }]
          }

          launchRegionSelect((evt)=>{
            let lcB=_this.canvasRenderInfo.shape_positioning.locatingBlocks;
            lcB=lcB.filter(blk=>blk._del===undefined)
            _this.canvasRenderInfo.shape_positioning.locatingBlocks=lcB;
            let new_defInfo={..._defInfo_,locatingBlocks:lcB};
            set_defInfo_(new_defInfo);

          });

        }}>SelRmBlock</Button>
        
        <Button onClick={()=>{
          runMatch(false);
        }}>match</Button>

        
        <Button onClick={()=>{
          runMatch(true);
        }}>match_refine</Button>
      </>;
    break;
    case configState.template_matching:
      confUI=<>
        <Button onClick={()=>{
          setConfState(configState.normal);
          delete _this.canvasRenderInfo.template_matching;
        }}>{"<"}</Button>


        <Button onClick={()=>{
          TriggerNewResult(false,{
            insp_type:"shape_features_matching",
          },_defInfo_,
          (obj)=>{
            console.log(obj);
            let matchResults = obj.RP.data.matchResults;
            
            // set_defInfo_({..._defInfo_,matchResults});
            _this.canvasRenderInfo.template_matching={
              matchResults,
              draw:(info,g,canvas_obj)=>{
                let ctx = g.ctx;
                ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
                let psize = 2*canvas_obj.rUtil.getPointSize();
                ctx.lineWidth = psize/3;

                matchResults.forEach((ms)=>{
                  canvas_obj.rUtil.drawcross(ctx, ms,psize);
                  let lineLength=100;
                  let offsetx=lineLength*Math.cos(ms.angle*3.14159/180);
                  let offsety=lineLength*Math.sin(ms.angle*3.14159/180);
                  ctx.beginPath();
                  ctx.moveTo(ms.x, ms.y);
                  ctx.lineTo(ms.x+offsetx, ms.y+offsety);
                  ctx.stroke();

                  ctx.font = '50px serif';
                  ctx.fillText('s:'+ms.similarity, 0, 0);
                  
                })
              }

            }





          },
          ()=>{

          });
        }}>match</Button>





      </>;
    break;
  }



  
  // console.log(editRegionInfo);
  return <div className="overlayCon HXF">
    <CanvasComponent key="kk" addClass="height12" 
    onCanvasInit={setCanComp}/>
    <div className="s overlay overlay scroll HXA WXA" style={{top:0,width:255,background:"#EEE"}}>
      {confUI}
    </div>
  </div>;
}


export default GenMatching_rdx;