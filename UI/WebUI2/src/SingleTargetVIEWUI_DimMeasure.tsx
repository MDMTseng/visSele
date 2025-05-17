import { CompParam_InspTarUI, IMCM_type, InspTarView_basicInfo, TestInputSelectUI, EDIT_PERMIT_FLAG ,CountDownCheckPopup} from "./SingleTargetVIEWUI_UTIL";
import { useRef, useState, useEffect, useCallback ,useMemo} from "react";
import { useDispatch } from "react-redux";
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';

import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { type_CameraInfo, type_IMCM } from './AppTypes';

import { Input, Button, Select, Space,Table } from 'antd';

import type { TableProps } from 'antd';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';

import clone from 'clone';
import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d, threePointToArc,intersectPoint,vecXY_add ,distance_point_point} from './UTIL/MathTools';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { InputNumber, Switch, Popover, Dropdown, Menu ,Divider} from 'antd';

import Color from 'color';

import {
  ExclamationOutlined,PlusOutlined,MinusOutlined,
} from '@ant-design/icons';
import { VerticalAlignTopOutlined, UpOutlined, DownOutlined, VerticalAlignBottomOutlined, DeleteOutlined } from '@ant-design/icons';





const { Column, ColumnGroup } = Table;


function genID_rand(power2:number=29)
{
  return Math.floor(Math.random()*Math.pow(2,power2));
}

interface Reference {
  type: string;
  id: number;
}

interface BaseElement {
  id: number;
  name: string;
  type: string;
}

interface FitElement extends BaseElement {
  margin: number;
  pt1: VEC2D;
  pt2: VEC2D;
  edge_surpress: number;
  blur_size: number;
  from_outer_margin?: boolean;
}

interface LineFitElement extends FitElement {
  type: "LineFit";
}

interface ArcFitElement extends FitElement {
  type: "ArcFit";
  pt3: VEC2D;
  edge_type: "LIGHT_TO_DARK" | "DARK_TO_LIGHT" | "BOTH";
  alpha1?: number;
  alpha2?: number;
}

interface SearchPointElement extends BaseElement {
  type: "SearchPoint";
  margin: number;
  width: number;
  angle: number;
  pt1: VEC2D;
  ref: Reference[];
  edge_surpress: number;
}

interface MeasureElement extends BaseElement {
  disp_pt1?: VEC2D;
  ctrl_pt1: VEC2D;
  ref: Reference[];
}

interface MeasureDistanceElement extends MeasureElement {
  type: "Measure_Distance";
  subtype: string;
  rotate: number;
  ref: Reference[];
}

interface MeasureAngleElement extends MeasureElement {
  type: "Measure_Angle";
  angle_select: number;
  ref: Reference[];
}

interface MeasureDiameterElement extends MeasureElement {
  type: "Measure_Diameter";
  is_radius: boolean;
  ref: Reference[];
}

interface LimitSetup {
  id: number;
  low_limit: number;
  high_limit: number;
  target: number;
  NG_as: "NA" | "OK" |"NG";
}

interface type_CAT_ELE {
  id: number;
  name: string;
  limits_setup?: LimitSetup[];
  ref?: number;
}

interface FeatureInfo {
  // template_angle: number;
  element_list: (LineFitElement | ArcFitElement | SearchPointElement | MeasureDistanceElement | MeasureAngleElement | MeasureDiameterElement)[];
  category_list: type_CAT_ELE[];
}

interface type_DimMeasure_DEF {
  id: string;
  type: string;
  match_tags: (string | string[])[];
  featureInfo: FeatureInfo;
  stream_id: number;
}


type type_Catgory_Element_Edit_param = {
  it_id:string,
  key:string,
  featureInfo: FeatureInfo,
  CatEle: type_CAT_ELE,
  DepInject: any,
  onCatEleUpdate: (CatEle: type_CAT_ELE) => void,
  onExit: () => void,
}


type ColorSetting_type = {

  draw_in_simple_form: boolean,
  main: Color,
  point: Color,
  start_bar: Color,
  start_bar_width: number,

  selected_point: Color,
  hover_point: Color,
  hover_point_size: number,

  primitive_shape_line: Color,
  primitive_shape_line_width: number,
  primitive_shape_line_dash: number[],
  auxiliary_shape_line: Color,
  auxiliary_shape_line_width: number,
  auxiliary_shape_line_dash: number[],

  indication_extended_line: Color,
  indication_extended_line_width: number,
  indication_extended_line_dash: number[],
  indication_line: Color,
  indication_line_width: number,
  indication_line_dash: number[],


  text_color: Color,
  text_stroke_color: Color,
  text_size: number,
}

type ArcFit_Ele_type = {
  id: number,
  name: string,
  pt1: VEC2D,
  pt2: VEC2D,
  pt3: VEC2D,
  margin: number,
  from_outer_margin: boolean,
  is_full_circle: boolean
}

let drawTheme_default: ColorSetting_type = {
  draw_in_simple_form:false,
  main: Color("rgba(255,0,0,0.5)"),
  point: Color("rgba(0,255,0,0.5)"),
  selected_point: Color("rgba(0,255,255,0.8)"),
  start_bar: Color("rgba(0,0,255,0.5)"),
  start_bar_width: 5,
  hover_point: Color("rgba(255,255,0,0.5)"),
  hover_point_size: 5,

  primitive_shape_line: Color("rgba(0,255,0,0.5)"),
  primitive_shape_line_width: 5,
  primitive_shape_line_dash: [],
  auxiliary_shape_line: Color("rgba(0,255,0,0.5)"),
  auxiliary_shape_line_width: 5,
  auxiliary_shape_line_dash: [],

  indication_extended_line: Color("rgba(255,255,0,0.5)"),
  indication_extended_line_width: 2,
  indication_extended_line_dash: [2,0.5],
  indication_line: Color("rgba(255,0,0,0.5)"),
  indication_line_width: 5,
  indication_line_dash: [5,1],

  text_color: Color("rgba(0,255,0,1)"),
  text_stroke_color: Color("rgba(0,0,0,1)"),
  text_size: 5,
}




let drawTheme: { [key: string]: ColorSetting_type } = {
  defDisp: drawTheme_default,

  targetEditingForgroundDisp: {
    ...drawTheme_default,
    main: Color("rgba(255,255,0,0.5)"),
    start_bar_width: 1
  },


  targetEditingBackgroundDisp: {
    ...drawTheme_default,
    main: Color("rgba(100,100,100,0.2)"),
    point: Color("rgba(100,150,100,0.1)"),
    start_bar: Color("rgba(0,0,255,0.2)"),
    start_bar_width: 1,

    indication_line: Color("rgba(0,255,0,0.2)"),
    indication_line_width: 1,
  },


  reportDisp: {
    ...drawTheme_default,
    main: Color("rgba(255,255,0,0.5)"),
  },

}


function FeatureControlPointMinDistance(
  ptsObj: { [key: string]: VEC2D }, mouseOnCanvas: VEC2D,point_key_prefixes:string[]=["pt","ctrl_pt","disp_pt"]
) {
  let minDist = Number.MAX_VALUE;
  let controlPointPath: (string | number)[] = [];
  for (let key in ptsObj) {
    if(key.startsWith("pt")==false && key.startsWith("ctrl_pt")==false && key.startsWith("disp_pt")==false)
      continue;
    let obj = ptsObj[key];
    if (obj === undefined)
      continue;

    //make sure obj is a VEC2D
    if (typeof obj.x !== 'number' || typeof obj.y !== 'number')
      continue;

    let d = Math.abs(mouseOnCanvas.x - obj.x) + Math.abs(mouseOnCanvas.y - obj.y);
    if (d < minDist) {
      minDist = d;
      controlPointPath = [key];
    }
  }

  return {
    distance: minDist, controlPointPath,


  };
}



function useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate,closeRange=10, onControlUpdate = undefined }: {
  featureInfo: any,
  targetFeatureElement: any;
  onFeatureElementUpdate: (featureEle: any) => void;
  closeRange?: number;
  onControlUpdate?: (onUpdateFeatureEle: any, controlPointPath: (string | number)[]) => void;
}) {
  const _this = useRef<any>({
    controlPointPath: undefined,
    controlPointNewLocation: undefined,
    featureInfo: featureInfo,
    featureEle: undefined,
    hoverOnPtPath: undefined,
    canvas_obj: undefined,

  }).current;
  _this.featureInfo = featureInfo;
  _this.featureEle = targetFeatureElement;




  let drawHook = useCallback((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,drawTheme_editing: ColorSetting_type ) => {

    _this.canvas_obj = canvas_obj;
    if (_this.featureEle === undefined) return;

    const onMousePress = canvas_obj.mouseStatus.status === 1 && canvas_obj.mouseStatus.pstatus === 0;
    const onMousePressUP = canvas_obj.mouseStatus.status === 0 && canvas_obj.mouseStatus.pstatus === 1;
    let camMag=canvas_obj.camera.GetCameraScale();
    if (ctrl_or_draw === true) {
      if (_this.controlPointPath === undefined) {
        const mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
        const { distance, controlPointPath } = FeatureControlPointMinDistance(_this.featureEle, mouseOnCanvas);

        if (distance <closeRange/camMag) {
          _this.hoverOnPtPath = controlPointPath;
          if (onMousePress) {
            _this.controlPointNewLocation = undefined;
            _this.controlPointPath = controlPointPath;
            canvas_obj.UserRegionSelect(() => { });

          }
        } else {
          _this.hoverOnPtPath = undefined;
          if (onMousePress) {

            _this.controlPointPath = undefined;

          }
        }
        return;
      }

      if (_this.controlPointPath !== undefined) {
        if (!onMousePressUP) {
          const mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
          _this.controlPointNewLocation = { x: mouseOnCanvas.x, y: mouseOnCanvas.y };

          if (onControlUpdate !== undefined) {

            let tmpFeatureEle = { ..._this.featureEle };

            if (_this.controlPointPath !== undefined && _this.controlPointNewLocation !== undefined) {
              tmpFeatureEle = ObjShellingAssign(tmpFeatureEle, _this.controlPointPath, _this.controlPointNewLocation);
            }
            onControlUpdate(tmpFeatureEle, _this.controlPointPath);
          }
        } else {
          let updatedFeatureEle = { ..._this.featureEle };
          if (_this.controlPointNewLocation !== undefined) {
            updatedFeatureEle = ObjShellingAssign(updatedFeatureEle, _this.controlPointPath, _this.controlPointNewLocation);
            onFeatureElementUpdate(updatedFeatureEle);
          }

          _this.controlPointNewLocation = undefined;
          _this.controlPointPath = undefined;
          canvas_obj.UserRegionSelect(undefined);
        }
      }
    } else {
      let tmpFeatureEle = { ..._this.featureEle };



      let isInEditMode = false;
      if (_this.controlPointPath !== undefined && _this.controlPointNewLocation !== undefined) {
        tmpFeatureEle = ObjShellingAssign(tmpFeatureEle, _this.controlPointPath, _this.controlPointNewLocation);
        isInEditMode = true;
      }

      MUX_Draw_FeatureElement_Edit(featureInfo, tmpFeatureEle,undefined, ctrl_or_draw, g, canvas_obj,drawTheme_editing);

      if (isInEditMode == false) {
        if (_this.hoverOnPtPath !== undefined) {
          let pt = GetObjElement(_this.featureEle, _this.hoverOnPtPath);
          if (pt !== undefined) {
            //draw cadidate circle
            g.ctx.setLineDash([5/camMag,1/camMag]);
            g.ctx.strokeStyle = "rgba(0,255,0,0.5)";
            g.ctx.lineWidth = 5/camMag;
            g.ctx.beginPath();
            g.ctx.arc(pt.x, pt.y, 15/camMag, 0, Math.PI * 2);
            g.ctx.stroke();
          }
        }
      }
    }


  }, [featureInfo])

  return drawHook;




}


type type_ControlPointSelect_DrawHook_param = {
  featureEleList: any[],
  onFeatureSelected?: (featureEle: any, path: (string | number)[], distance: number) => void;
  onFeatureHovered?: (featureEle: any, path: (string | number)[], distance: number) => void;
  distanceThreshold: number;
  colorSetting: ColorSetting_type;
}
function useControlPointSelect_DrawHook(param: type_ControlPointSelect_DrawHook_param | undefined) {
  const { featureEleList, distanceThreshold = 10, colorSetting, onFeatureSelected, onFeatureHovered } = param || {};
  const _this = useRef<any>({
  }).current;


  console.log(">>useControlPointSelect_DrawHook>", param);


  let drawHook = useCallback((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
    if (param === undefined) return;

    // console.log(">>>", param);
    _this.canvas_obj = canvas_obj;
    if (featureEleList === undefined) return;

    if (colorSetting === undefined) return;

    const onMousePress = canvas_obj.mouseStatus.status === 1 && canvas_obj.mouseStatus.pstatus === 0;
    const onMousePressUP = canvas_obj.mouseStatus.status === 0 && canvas_obj.mouseStatus.pstatus === 1;

    if (ctrl_or_draw === true) {//detect
      const mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);


      let hoverEle = undefined;
      let hoverEle_path: (string | number)[] = [];
      let minDist = Number.MAX_VALUE;
      {
        for (let i = 0; i < featureEleList.length; i++) {
          const { distance, controlPointPath } = FeatureControlPointMinDistance(featureEleList[i], mouseOnCanvas);
          if (distance < minDist && distance < distanceThreshold) {
            minDist = distance;
            hoverEle_path = controlPointPath;
            hoverEle = featureEleList[i];
          }
        }
        _this.hoverEle = hoverEle;
        _this.hoverEle_path = hoverEle_path;

      }

      if (_this.hoverEle !== undefined) {
        if (onMousePress) {
          if (onFeatureSelected !== undefined) {
            onFeatureSelected(hoverEle, hoverEle_path, minDist);
          }
        } else {
          if (onFeatureHovered !== undefined) {
            onFeatureHovered(hoverEle, hoverEle_path, minDist);
          }
        }
      }
    } else {//draw
      if (_this.hoverEle === undefined)
        return;
      //draw point
      g.ctx.fillStyle = colorSetting.hover_point.toString();
      g.ctx.strokeStyle = colorSetting.hover_point.toString();
      g.ctx.lineWidth = colorSetting.hover_point_size / 2;
      let pt = GetObjElement(_this.hoverEle, _this.hoverEle_path);
      if (pt !== undefined) {
        g.ctx.beginPath();
        g.ctx.arc(pt.x, pt.y, colorSetting.hover_point_size, 0, Math.PI * 2);
        g.ctx.stroke();
      }
    }


  }, [param, featureEleList, distanceThreshold, onFeatureSelected, onFeatureHovered, colorSetting])

  return drawHook;




}



function drawFeature(featureInfo: any, featureEle: any, colorSetting: ColorSetting_type, drawRefLevel: number, topLevel: number, ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) {

  if (featureEle.ref !== undefined && drawRefLevel > 0) {
    let oriCS = colorSetting;

    let mainColor = oriCS.main.fade(0.2).darken(0.1);
    if (drawRefLevel < topLevel) {
      mainColor = mainColor.darken(0.5);
    }
    let downLevelColorSetting = {
      ...oriCS,
      main: mainColor,
      point: oriCS.point.fade(0.2),
      start_bar: oriCS.start_bar.fade(0.2),
      start_bar_width: oriCS.start_bar_width,
      selected_point: oriCS.selected_point.fade(0.2),

    }

    let refEleList: any[] = featureInfo.element_list.filter((ele: any) => featureEle.ref.some((ref: any) => ref.id == ele.id));
    for (let i = 0; i < refEleList.length; i++) {
      drawFeature(featureInfo, refEleList[i], downLevelColorSetting, drawRefLevel - 1, topLevel, ctrl_or_draw, g, canvas_obj);
    }
  }
  MUX_Draw_FeatureElement_Edit(featureInfo, featureEle,undefined, ctrl_or_draw, g, canvas_obj, colorSetting);
}

type type_DrawFeatureSet = {
  [key:string]:{
    element_list:any[],
    colorSetting:ColorSetting_type,
  }
}


function drawFeatureSet(featureInfo: any, featureset: type_DrawFeatureSet, drawRefLevel: number, ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) {
  if (ctrl_or_draw == true)
    return;
  for (let key in featureset) {
    let featureDrawSet = featureset[key];
    for (let i = 0; i < featureDrawSet.element_list.length; i++) {
      let featureEle = featureDrawSet.element_list[i];
      if(featureEle===undefined)
        continue;
      drawFeature(featureInfo, featureEle, featureDrawSet.colorSetting, drawRefLevel, drawRefLevel, ctrl_or_draw, g, canvas_obj);
    }
  }

}


function PopOverAdjuster({ selectCBs,title,children }: { selectCBs: { [key: string]: () => void },title?:string,children:React.ReactNode }) {
  // console.log(">>PopOverAdjuster",selectCBs);
  return (
    <Popover 
      content={
        <>
          {Object.keys(selectCBs).map((key,idx) => (
            <Button key={idx} onClick={selectCBs[key]}>
              {key}
            </Button>
          ))}
        </>
      } 
      title={title}
    >
      {children}
    </Popover>
  );
}


function BasicPrimitiveSettingOption({it_id, featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit,draw_mmpp,onDelete }: type_UI_FeatureElement_Edit_param)
{



  let adj_inc=draw_mmpp;


  function valueUpdate(new_value:number,key:string,roundDigit:number=4)
  {
    onFeatureElementUpdate({ ...targetFeatureElement, [key]: roundDigit>0?Math.round(new_value*10**roundDigit)/10**roundDigit:new_value });
  }
  return <>
  <Input prefix="名稱:" value={targetFeatureElement.name} style={{width:200}} onChange={(e: any) => {
    onFeatureElementUpdate({ ...targetFeatureElement, name: e.target.value });
  }} />

  <CountDownCheckPopup countdown={5} onConfirm={()=>{
    onDelete();
  }}>
  <Button danger>刪除</Button>
  </CountDownCheckPopup>



  
  <PopOverAdjuster selectCBs={{
    "/1.5":()=>{
      valueUpdate(targetFeatureElement.margin / 1.1,"margin");
    },
    "-":()=>{
      valueUpdate(targetFeatureElement.margin - adj_inc,"margin");
    },
    "+":()=>{
      valueUpdate(targetFeatureElement.margin + adj_inc,"margin");
    },
    "X1.5":()=>{
      valueUpdate(targetFeatureElement.margin * 1.1,"margin");
    },
  }} title="範圍">
  
    <Button >範圍</Button>
  </PopOverAdjuster>


  <InputNumber value={targetFeatureElement.margin} onChange={(value: number) => {
    targetFeatureElement.margin = value;
    onFeatureElementUpdate(targetFeatureElement);
  }} />



    強度閾值:
    <InputNumber value={targetFeatureElement.edge_surpress} onChange={(value: number) => {
      targetFeatureElement.edge_surpress = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />
    
    平滑強度:
    <InputNumber value={targetFeatureElement.blur_size} onChange={(value: number) => {
      targetFeatureElement.blur_size = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />

    邊緣形態:
    <Select value={targetFeatureElement.edge_type} onChange={(value: string) => {
      targetFeatureElement.edge_type = value;
      onFeatureElementUpdate(targetFeatureElement);
    }}>
      <Select.Option value="LIGHT_TO_DARK">{"亮=>暗"}</Select.Option>
      <Select.Option value="DARK_TO_LIGHT">{"暗=>亮"}</Select.Option>
      <Select.Option value="BOTH">{"兩邊"}</Select.Option>
    </Select>


    邊緣計算:
    <Select value={targetFeatureElement.center_type} onChange={(value: string) => {
      targetFeatureElement.center_type = value;
      onFeatureElementUpdate(targetFeatureElement);
    }}>
    <Select.Option value="LOCAL_AVG">{"局部平均"}</Select.Option>
    <Select.Option value="GLOBAL_AVG">{"全域平均"}</Select.Option>
  </Select>


    alpha1:
    <InputNumber value={targetFeatureElement.alpha1??0} step={0.01} min={0} max={1} onChange={(value: number) => {
      targetFeatureElement.alpha1 = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />

    alpha2:
    <InputNumber value={targetFeatureElement.alpha2??0} step={0.01} min={0} max={1} onChange={(value: number) => {
      targetFeatureElement.alpha2 = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />
  </>
}


type type_UI_FeatureElement_Edit_param = {
  it_id:string,
  key:string,
  draw_mmpp:number,
  featureInfo: any,
  targetFeatureElement: any,
  Ref_Src_Info: {use_cache:boolean,file_name?:string,folder_path?:string},
  DepInject: any,
  onFeatureElementUpdate: (featureEle: any) => void,
  onExit: () => void,
  onDelete: () => void,
}

//-----------------------------------LineFit
function _Draw_FeatureElement_Edit_LineFit(
  featureInfo: any,
  featureEle: LineFitElement,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
) {
  let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
  if (ctrl_or_draw == true) {
    return;
  }

  g.ctx.setLineDash([]);
  let camMag = canvas_obj.camera.GetCameraScale();


  if(reportEle!==undefined)
  {
    //draw report
    g.ctx.strokeStyle = colorSetting["primitive_shape_line"].toString();
    g.ctx.lineWidth = colorSetting["primitive_shape_line_width"]/camMag;
    g.ctx.beginPath();
    g.ctx.moveTo(reportEle.pt1.x, reportEle.pt1.y);
    g.ctx.lineTo(reportEle.pt2.x, reportEle.pt2.y);
    g.ctx.stroke();
    // console.log(">>>",reportEle);
  }
  else
  {
    //line color

    {

      g.ctx.strokeStyle = colorSetting["main"].toString();
      //draw line
      g.ctx.save();
      g.ctx.beginPath();
      let margin=featureEle.margin;
      if(colorSetting.draw_in_simple_form==true)
      {
        margin=0;
      }
      g.ctx.lineWidth = margin * 2;//positive and negative margin

      g.ctx.moveTo(featureEle.pt1.x, featureEle.pt1.y);
      g.ctx.lineTo(featureEle.pt2.x, featureEle.pt2.y);
      g.ctx.stroke();
      g.ctx.restore();


      {

        //draw start bar
        let start_bar_width = (colorSetting["start_bar_width"] ?? 5)/camMag;
        let start_barColor = colorSetting["start_bar"];
        g.ctx.fillStyle = start_barColor.toString();
        g.ctx.strokeStyle = start_barColor.toString();
        g.ctx.lineWidth = start_bar_width;

        let normal_vec = { x: featureEle.pt2.y - featureEle.pt1.y, y: featureEle.pt1.x - featureEle.pt2.x };
        let len = Math.sqrt(normal_vec.x * normal_vec.x + normal_vec.y * normal_vec.y);
        normal_vec.x = normal_vec.x / len * margin;
        normal_vec.y = normal_vec.y / len * margin;



        g.ctx.beginPath();
        g.ctx.moveTo(featureEle.pt1.x + normal_vec.x, featureEle.pt1.y + normal_vec.y);
        g.ctx.lineTo(featureEle.pt2.x + normal_vec.x, featureEle.pt2.y + normal_vec.y);
        g.ctx.stroke();

      }
    }
    g.ctx.fillStyle = colorSetting["point"].toString();



    //draw dot at pt1 and pt2
    g.ctx.save();
    g.ctx.beginPath();
    let pointSize=5/camMag;
    g.ctx.arc(featureEle.pt1.x, featureEle.pt1.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.beginPath();
    g.ctx.arc(featureEle.pt2.x, featureEle.pt2.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.restore();

  }
}

function _UI_FeatureElement_Edit_LineFit(props: type_UI_FeatureElement_Edit_param) {

  let {draw_mmpp,it_id,key, featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit } = props;
  const dispatch = useDispatch();
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

  const [doLiveInspection,set_doLiveInspection]=useState(false);
  const _this = useRef<any>({
    drawHook:undefined,
    drawTheme_editing:drawTheme.targetEditingForgroundDisp,
  }).current;



  async function Do_DBG_Inspection(Ref_Src_Info:{use_cache:boolean,file_name?:string,folder_path?:string}){

    let pkts = await BPG_API.InspTargetExchange(it_id, { 
      type: "execute_inspection",
      use_cached_input:Ref_Src_Info.use_cache,
      file_name:Ref_Src_Info.file_name,
      folder_path:Ref_Src_Info.folder_path,
      
      // imageQuality:15,
      imageQuality:90,
      dbg_feature_id:targetFeatureElement.id,
      dbg_feature_info:{sdsd:"22222"}
      }) as any[];
      
      console.log(">>>",pkts);
      let IMs=pkts.filter((pkt:any)=>pkt.type=="IM");
      if(IMs.length==0)
      {
      // console.log(">>>",pkts);
      // return;
      }

      let RP=pkts.find((pkt:any)=>pkt.type=="RP");
      if(RP===undefined)
      {
      console.log(">>>",pkts);
      return;
      }

      let featureIdx=featureInfo.element_list.findIndex((feature:any)=>feature.id==targetFeatureElement.id);
      if(featureIdx==-1)
      {
      console.log(">>>",featureInfo.element_list);
      return;
      }


      {
      _this.drawHook=undefined;
      let inspReport=RP?.data?.report?.[0];
      if(inspReport===undefined)
      {
        console.log("no report");
        return;
      }
      let featureReport=inspReport?.element_report?.[featureIdx];
      if(featureReport===undefined)
      {
        console.log("no target feature",inspReport);
        return;
      }

      console.log(">>>",featureReport);
      let dbg_info=featureReport.dbg_info;
      console.log(dbg_info);
      if(dbg_info===undefined)
      {
        console.log("no dbg_info");
        return;
      }
      
      _this.drawHook=(ctrl_or_draw:boolean, g:type_DrawHook_g, canvas_obj:DrawHook_CanvasComponent)=>{

        if(ctrl_or_draw==true)
        {
          return;
        }
        let edge_points=dbg_info?.edge_points;
        let consider_edge_points=dbg_info?.consider_edge_points;
        
        let camMag = canvas_obj.camera.GetCameraScale();

                //find the bounding box of the arc, not circle
        function CRCorner(line:any, margin: number) {
          let pt1=line.pt1;
          let pt2=line.pt2;

          let maxXpt=pt1.x>pt2.x?pt1:pt2;


          return {
            x: maxXpt.x + margin,
            y: (pt1.y+pt2.y)/2,
          };
        }

        {
          let CenterRightC=CRCorner(targetFeatureElement,targetFeatureElement.margin);
          let x_offset=CenterRightC.x+targetFeatureElement.margin;
          let y_offset=CenterRightC.y;
          for(let i=0;i<IMs.length;i++)
          {
            let IM=IMs[i].image_info.image;
            g.ctx.save();
            g.ctx.translate(x_offset,y_offset-IM.height*draw_mmpp/2);

            {//draw left vertical line

              g.ctx.setLineDash([]);
              //draw a frame
              g.ctx.strokeStyle="white";
              g.ctx.lineWidth=3/camMag;
              g.ctx.beginPath();
              g.ctx.rect(0,0,IM.width*draw_mmpp,IM.height*draw_mmpp);
              g.ctx.stroke();


              g.ctx.strokeStyle="red";
              g.ctx.lineWidth=15/camMag;
              g.ctx.beginPath();
              g.ctx.moveTo(0,0);
              g.ctx.lineTo(0,IM.height*draw_mmpp);
              g.ctx.stroke();
            }
            g.ctx.scale(draw_mmpp,draw_mmpp);
            g.ctx.drawImage(IM,0,0);
            g.ctx.restore();
            x_offset+=(IM.width+20)*draw_mmpp;
          }
        }

        // console.log(">>>",_this.tmpDbgInfo?.featureReport,featureReport);
        if(featureReport===undefined||featureReport.error_code!==undefined)
        {
          return;
        }

        {
          g.ctx.strokeStyle="blue";
          g.ctx.beginPath();
          g.ctx.moveTo(featureReport.pt1.x,featureReport.pt1.y);
          g.ctx.lineTo(featureReport.pt2.x,featureReport.pt2.y);
          g.ctx.stroke();
        }

        if(edge_points===undefined||edge_points.length<2)
        {
          return;
        }

        g.ctx.setLineDash([]);
        g.ctx.strokeStyle="red";
        g.ctx.lineWidth=2/camMag;
        g.ctx.beginPath();
        let preloc=edge_points[0];
        g.ctx.moveTo(preloc.x,preloc.y);

        for(let i=1;i<edge_points.length;i++)
        {
          let pt=edge_points[i];
          let dist=Math.hypot(pt.x-preloc.x,pt.y-preloc.y);
          if(dist>2)
          {
            g.ctx.moveTo(pt.x,pt.y);
          }
          else
          {
            g.ctx.lineTo(pt.x,pt.y);
          }
          preloc=pt;
        }
        g.ctx.stroke();


      }



    }
    console.log(">>>",pkts);

    _this.drawTheme_editing={
      ...drawTheme.targetEditingForgroundDisp,
      main:Color("rgba(100,100,100,0.1)"),
    };
    _this.canvas_obj.draw();
  }


  function _onFeatureElementUpdate(featureEle:any){
    onFeatureElementUpdate(featureEle);
    console.log(">>>",featureEle);
    if(doLiveInspection==true)
    {
      Do_DBG_Inspection(props.Ref_Src_Info);
    }
  }


  useEffect(() => {
    if(doLiveInspection==true)
    {
      Do_DBG_Inspection(props.Ref_Src_Info);
    }
  }, [props.Ref_Src_Info]);

  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate:_onFeatureElementUpdate });

  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.canvas_obj=canvas_obj;
        cpe_drawHook(ctrl_or_draw, g, canvas_obj,_this.drawTheme_editing);
        drawFeatureSet(featureInfo,
          {
            bg: {
              element_list: featureInfo.element_list.filter((feature: any) => feature.id != targetFeatureElement.id),
              colorSetting: drawTheme.targetEditingBackgroundDisp
            },
          }, 0, ctrl_or_draw, g, canvas_obj);

        if(_this.drawHook!==undefined)
        {
          _this.drawHook(ctrl_or_draw, g, canvas_obj);
        }
      },
    })
  }, [cpe_drawHook, targetFeatureElement])
  

  return <>

    <BasicPrimitiveSettingOption {...props} onFeatureElementUpdate={_onFeatureElementUpdate} />
    <br/>
    雙頂點貼合<Switch checked={targetFeatureElement.dual_apex_line} onChange={(checked:boolean)=>{
      _onFeatureElementUpdate({...targetFeatureElement,dual_apex_line:checked});

    }} />

    雙頂點長度比例<InputNumber value={targetFeatureElement.dual_apex_line_length_ratio_threshold??0.5} min={0.01} max={1} step={0.01} onChange={(value:number)=>{
      _onFeatureElementUpdate({...targetFeatureElement,dual_apex_line_length_ratio_threshold:value});
    }} />

    頂點貼合<Switch checked={targetFeatureElement.align_to_apex} onChange={(checked:boolean)=>{
      _onFeatureElementUpdate({...targetFeatureElement,align_to_apex:checked});

    }} />

    動態回報
    <Switch checked={doLiveInspection} onChange={(checked:boolean)=>{
      if(checked==true)
      {
        Do_DBG_Inspection(props.Ref_Src_Info);
      }
      else 
      {
        _this.drawHook=undefined;
        _this.drawTheme_editing=drawTheme.targetEditingForgroundDisp;
        _this.canvas_obj.draw();
      }
      set_doLiveInspection(checked);
    }} />



  </>
}

//-----------------------------------ArcFit
function _Draw_FeatureElement_Edit_ArcFit(
  featureInfo: any,
  featureEle: ArcFit_Ele_type,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
) {
  if (ctrl_or_draw == true) {
    return;
  }


  g.ctx.setLineDash([]);

  let camMag = canvas_obj.camera.GetCameraScale();

  let pointColor = colorSetting["point"];
  let arc = threePointToArc(featureEle.pt1, featureEle.pt2, featureEle.pt3);
  let is_full_circle = featureEle.is_full_circle || false;
  if(is_full_circle==true)
  {
    arc.thetaE=arc.thetaS+Math.PI*2;
  }
  if(reportEle!==undefined)
  {
    //draw report
    g.ctx.strokeStyle = colorSetting["primitive_shape_line"].toString();
    g.ctx.lineWidth = colorSetting["primitive_shape_line_width"]/camMag;

    // console.log(">>>",reportEle);
    g.ctx.beginPath();
    g.ctx.arc(reportEle.c.x, reportEle.c.y, reportEle.r, arc.thetaS, arc.thetaE);
    g.ctx.stroke();
    // console.log(">>>",reportEle);
  }
  else
  {
    {
      let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
      let mainColor = colorSetting["main"];
      let start_barColor = colorSetting["start_bar"];
      let start_bar_width = colorSetting["start_bar_width"]/camMag;

      let from_outer_margin = featureEle.from_outer_margin || false;
      //line color
      g.ctx.strokeStyle = mainColor.toString();

      let margin=featureEle.margin;
      if(colorSetting.draw_in_simple_form==true)
      {
        margin=0;
      }

      g.ctx.lineWidth = margin * 2;//positive and negative margin

      //draw arc from three points
      {
        g.ctx.beginPath();
        g.ctx.arc(arc.x, arc.y, arc.r, arc.thetaS, arc.thetaE);
        g.ctx.stroke();


        let nr = arc.r + margin * ((from_outer_margin == true) ? 1 : -1);
        g.ctx.fillStyle =
          g.ctx.strokeStyle = start_barColor.toString();
        if (nr > 0) {
          g.ctx.lineWidth = start_bar_width;
          g.ctx.beginPath();
          g.ctx.arc(arc.x, arc.y, nr, arc.thetaS, arc.thetaE);
          g.ctx.stroke();

        }
        else {
          g.ctx.beginPath();
          g.ctx.arc(arc.x, arc.y, start_bar_width, 0, Math.PI * 2);
          g.ctx.fill();
        }
      }
    }


    g.ctx.fillStyle = pointColor.toString();
    //draw dot at pt1 and pt2
    g.ctx.save();
    let pointSize=5/camMag;
    g.ctx.beginPath();
    g.ctx.arc(featureEle.pt1.x, featureEle.pt1.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.beginPath();
    g.ctx.arc(featureEle.pt2.x, featureEle.pt2.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.beginPath();
    g.ctx.arc(featureEle.pt3.x, featureEle.pt3.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.restore();

  }
}

function _UI_FeatureElement_Edit_ArcFit(props: type_UI_FeatureElement_Edit_param) {

  let {it_id, featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit,draw_mmpp,Ref_Src_Info } = props;

  let _this = useRef<any>({
    controlPointPath: undefined,
    controlPointNewLocation: undefined,
    featureEle: undefined,
    canvas_obj:undefined,
    drawHook:undefined,
    drawTheme_editing:drawTheme.targetEditingForgroundDisp,
    dbg_IMs:[],
  }).current;

  const dispatch = useDispatch();
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);



  async function Do_DBG_Inspection(Ref_Src_Info:{use_cache:boolean,file_name?:string,folder_path?:string}){
    let pkts = await BPG_API.InspTargetExchange(it_id, { 
      type: "execute_inspection",
      imageQuality:90,
      dbg_feature_id:targetFeatureElement.id,
      dbg_feature_info:{sdsd:"22222"},
      use_cached_input:Ref_Src_Info.use_cache,
      file_name:Ref_Src_Info.file_name,
      folder_path:Ref_Src_Info.folder_path,
      }) as any[];

      let IMs=pkts.filter((pkt:any)=>pkt.type=="IM");

      let RP=pkts.find((pkt:any)=>pkt.type=="RP");
      if(RP===undefined)
      {
      console.log(">>>",pkts);
      return;
      }

      let featureIdx=featureInfo.element_list.findIndex((feature:any)=>feature.id==targetFeatureElement.id);
      if(featureIdx==-1)
      {
      console.log(">>>",featureInfo.element_list);
      return;
      }


      {
      _this.drawHook=undefined;
      let inspReport=RP?.data?.report?.[0];
      if(inspReport===undefined)
      {
        console.log("no report");
        return;
      }
      let featureReport=inspReport?.element_report?.[featureIdx];
      if(featureReport===undefined)
      {
        console.log("no target feature",inspReport);
        return;
      }

      console.log(">>>",featureReport);
      let dbg_info=featureReport.dbg_info;
      console.log(dbg_info);
      if(dbg_info===undefined)
      {
        console.log("no dbg_info");
        return;
      }
      
      _this.drawHook=(ctrl_or_draw:boolean, g:type_DrawHook_g, canvas_obj:DrawHook_CanvasComponent)=>{

        if(ctrl_or_draw==true)
        {
          return;
        }
        let edge_points=dbg_info?.edge_points;
        
        let camMag = canvas_obj.camera.GetCameraScale();


        let arc = threePointToArc(targetFeatureElement.pt1, targetFeatureElement.pt2, targetFeatureElement.pt3);


        //find the bounding box of the arc, not circle
        function arcBoundingBox(arc: ReturnType<typeof threePointToArc>, margin: number) {
          const { x: centerX, y: centerY, r: radius, thetaS: startAngle, thetaE: endAngle } = arc;
          
          // Normalize angles to be between 0 and 2π
          let start = startAngle % (2 * Math.PI);
          if (start < 0) start += 2 * Math.PI;
          let end = endAngle % (2 * Math.PI);
          if (end < 0) end += 2 * Math.PI;
          
          // If end is less than start, add 2π to end
          if (end < start) end += 2 * Math.PI;
          
          // Find extrema points by checking start, end, and quadrant boundaries
          let angles = [start, end];
          
          // Add quadrant boundaries (0, π/2, π, 3π/2) if they fall within the arc
          [0, Math.PI/2, Math.PI, 3*Math.PI/2].forEach(angle => {
            if (angle > start && angle < end) {
              angles.push(angle);
            }
          });
          
          // Calculate points on arc at each angle
          let points = angles.map(angle => ({
            x: centerX + radius * Math.cos(angle),
            y: centerY + radius * Math.sin(angle)
          }));
          
          // Find min/max x and y coordinates
          let minX = Math.min(...points.map(p => p.x));
          let maxX = Math.max(...points.map(p => p.x));
          let minY = Math.min(...points.map(p => p.y));
          let maxY = Math.max(...points.map(p => p.y));
          
          // Add margin
          return {
            x: minX - margin,
            y: minY - margin,
            width: (maxX - minX) + 2 * margin,
            height: (maxY - minY) + 2 * margin
          };
        }

        {
          let BBOX=arcBoundingBox(arc,targetFeatureElement.margin);
          let x_offset=BBOX.x+BBOX.width;
          let y_offset=BBOX.y;
          for(let i=0;i<IMs.length;i++)
          {
            let IM=IMs[i].image_info.image;
            g.ctx.save();
            g.ctx.translate(x_offset,y_offset);


            {//draw left vertical line
              g.ctx.strokeStyle="red";
              g.ctx.lineWidth=15/camMag;
              g.ctx.beginPath();
              g.ctx.moveTo(0,0);
              g.ctx.lineTo(0,IM.height*draw_mmpp);
              g.ctx.stroke();
            }
            g.ctx.scale(draw_mmpp,draw_mmpp);
            g.ctx.drawImage(IM,0,0);
            g.ctx.restore();
            x_offset+=(IM.width+20)*draw_mmpp;
          }

        }

        // console.log(">>>",_this.tmpDbgInfo?.featureReport,featureReport);
        if(featureReport===undefined||featureReport.error_code!==undefined)
        {
          return;
        }

        if(1){
          g.ctx.strokeStyle="blue";
          g.ctx.lineWidth=2/camMag;
          g.ctx.beginPath();
          //arc
          g.ctx.arc(featureReport.c.x, featureReport.c.y, featureReport.r, 0, Math.PI*2);
          //line
          g.ctx.moveTo(featureReport.c.x,featureReport.c.y);
          g.ctx.lineTo(featureReport.c.x+ featureReport.r,featureReport.c.y);
          g.ctx.stroke();
        }

        if(edge_points===undefined||edge_points.length<2)
        {
          return;
        }

        g.ctx.setLineDash([]);
        g.ctx.strokeStyle="white";
        g.ctx.lineWidth=2/camMag;
        g.ctx.beginPath();
        let preloc=edge_points[0];
        g.ctx.moveTo(preloc.x,preloc.y);


        let isInGoodRegion=false;
        for(let i=1;i<edge_points.length;i++)
        {
          let pt=edge_points[i];
          let dist=Math.hypot(pt.x-preloc.x,pt.y-preloc.y);
          let _isInGoodRegion=pt.w>0;

          if(dist>2 || isInGoodRegion!=_isInGoodRegion)
          {
            g.ctx.stroke();
            
            g.ctx.strokeStyle=(_isInGoodRegion==true)?"white":"red";
            g.ctx.beginPath();
            g.ctx.moveTo(pt.x,pt.y);
          }
          else
          {
            g.ctx.lineTo(pt.x,pt.y);
          }
          isInGoodRegion=_isInGoodRegion;
          preloc=pt;
        }
        g.ctx.stroke();
      }



    }
    console.log(">>>",pkts);

    _this.drawTheme_editing={
      ...drawTheme.targetEditingForgroundDisp,
      main:Color("rgba(100,100,100,0.1)"),
    };
    _this.canvas_obj.draw();
  }


  function _onFeatureElementUpdate(targetFeatureElement:any)
  {

    onFeatureElementUpdate(targetFeatureElement);

    if(isInActiveCheck==true)
    {
      Do_DBG_Inspection(props.Ref_Src_Info);
    }
   
  }


  useEffect(() => {
    if(isInActiveCheck==true)
    {
      Do_DBG_Inspection(props.Ref_Src_Info);
    }
  }, [props.Ref_Src_Info]);


  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate:_onFeatureElementUpdate });
  const [isInActiveCheck,setIsInActiveCheck]=useState(false);


  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.canvas_obj=canvas_obj;
        cpe_drawHook(ctrl_or_draw, g, canvas_obj,_this.drawTheme_editing);

        if(_this.drawHook!==undefined)
        {
          _this.drawHook(ctrl_or_draw, g, canvas_obj);
        }


      },
    })
  }, [cpe_drawHook])

  _this.featureEle = targetFeatureElement;

  return <>
    <BasicPrimitiveSettingOption {...props} onFeatureElementUpdate={_onFeatureElementUpdate}
    />
    方向:
    <Switch checked={targetFeatureElement.from_outer_margin} onChange={(value: boolean) => {
      targetFeatureElement.from_outer_margin = value;
      _onFeatureElementUpdate(targetFeatureElement);
    }} checkedChildren={"外往內"} unCheckedChildren={"內往外"} />

    弧或圓
    <Switch checked={targetFeatureElement.is_full_circle} onChange={(value: boolean) => {
      targetFeatureElement.is_full_circle = value;
      _onFeatureElementUpdate(targetFeatureElement);
    }} checkedChildren={"弧"} unCheckedChildren={"圓"} />



    <Button onClick={()=>{


      if(isInActiveCheck==false)
        setIsInActiveCheck(true);
      Do_DBG_Inspection(props.Ref_Src_Info);
    }}>驗證</Button>

  </>
}


//-----------------------------------SearchPoint
function _Draw_FeatureElement_Edit_SearchPoint(
  featureInfo: any,
  featureEle: any,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
) {
  if (ctrl_or_draw == true) {
    return;
  }

  g.ctx.setLineDash([]);
  let camMag = canvas_obj.camera.GetCameraScale();

  let error_code:number=reportEle?.error_code??0;
  let error_msg:string=reportEle?.error_msg??"";
  let result_type=reportEle?.result_type??9999;
  if(reportEle!==undefined && error_code==0)
  {
    if(result_type==9999)
    {//HACK hard code for UNSET result_type
      return;
    }
    console.log(">>>",reportEle);
    let rotate_rad = reportObj.angle;
    //draw cross at reportEle.pt1
    g.ctx.strokeStyle = colorSetting.primitive_shape_line.toString();
    g.ctx.lineWidth = colorSetting.primitive_shape_line_width/camMag;
    let cross_size = colorSetting.primitive_shape_line_width/camMag;

    g.ctx.save();
    g.ctx.translate(reportEle.pt1.x, reportEle.pt1.y);
    g.ctx.rotate(-rotate_rad);
    g.ctx.beginPath();


    g.ctx.moveTo(-cross_size, -cross_size);
    g.ctx.lineTo(+cross_size, +cross_size);
    g.ctx.moveTo(-cross_size, +cross_size);
    g.ctx.lineTo(+cross_size, -cross_size);
    g.ctx.stroke();
    g.ctx.restore();
    // console.log(">>>",reportEle);
  }
  else {
    let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);


    // console.log(">>>",colorSetting);
    let mainColor = colorSetting["main"];
    let pointColor = colorSetting["point"];
    let start_barColor = colorSetting["start_bar"];
    let start_bar_width = colorSetting["start_bar_width"]/camMag;
    
    //line color
    g.ctx.strokeStyle = mainColor.toString();

    let margin = featureEle.margin;

    if(colorSetting.draw_in_simple_form==true)
    {
      margin=0;
    }
    g.ctx.lineWidth = margin * 2;//positive and negative margin

    let width_half = featureEle.width / 2;
    let angle_rad = featureEle.angle;


    let refEle = featureInfo.element_list.find((feature: any) => feature.id == featureEle?.ref?.[0]?.id);
    if (refEle !== undefined) {
      angle_rad += Math.atan2(refEle.pt2.y  - refEle.pt1.y, refEle.pt2.x - refEle.pt1.x);
    }


    let line_vec = { x: Math.cos(angle_rad), y: Math.sin(angle_rad) };
    let normal_vec = { x: line_vec.y, y: -line_vec.x };

    //draw arc from three points
    {
      g.ctx.beginPath();
      g.ctx.moveTo(featureEle.pt1.x - line_vec.x * width_half, featureEle.pt1.y - line_vec.y * width_half);
      g.ctx.lineTo(featureEle.pt1.x + line_vec.x * width_half, featureEle.pt1.y + line_vec.y * width_half);
      g.ctx.stroke();

      g.ctx.lineWidth = start_bar_width;
      g.ctx.fillStyle = start_barColor.toString();
      g.ctx.strokeStyle = start_barColor.toString();

      g.ctx.beginPath();
      g.ctx.moveTo(featureEle.pt1.x + normal_vec.x * margin - line_vec.x * width_half, featureEle.pt1.y + normal_vec.y * margin - line_vec.y * width_half);
      g.ctx.lineTo(featureEle.pt1.x + normal_vec.x * margin + line_vec.x * width_half, featureEle.pt1.y + normal_vec.y * margin + line_vec.y * width_half);
      g.ctx.stroke();
    }

    g.ctx.fillStyle = pointColor.toString();
    //draw dot at pt1 and pt2
    g.ctx.save();
    g.ctx.beginPath();
    let pointSize=5/camMag;
    g.ctx.arc(featureEle.pt1.x, featureEle.pt1.y, pointSize, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.restore();

    {
      
      g.ctx.save();
      // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
      g.ctx.translate(featureEle.pt1.x, featureEle.pt1.y);
      draw_feature_text(g,canvas_obj,colorSetting,
        [
          featureEle.name,
          ...(error_code!==0?["E:<"+error_code+">"+error_msg]:[]),
        ]
      );
      g.ctx.restore();

    }

  }
}

function getCurrentRotation(ctx: CanvasRenderingContext2D) {
  // Get the current transform matrix
  const transform = ctx.getTransform();
  
  // Calculate rotation from the matrix components
  // atan2(b, a) where a and b are the first row of the matrix
  return Math.atan2(transform.b, transform.a);
}

function draw_feature_text(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,colorSetting:ColorSetting_type,text:string[])
{

  let camMag = canvas_obj.camera.GetCameraScale();
  g.ctx.save();
  // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
  let size=5/camMag;
  g.ctx.scale(size,size);
  let fontSize=colorSetting.text_size;
  g.ctx.font = "bold " + fontSize + "px Arial";
  g.ctx.lineWidth=0.3;



  g.ctx.strokeStyle=colorSetting.text_stroke_color.toString();
  g.ctx.fillStyle=colorSetting.text_color.toString();
  g.ctx.setLineDash([]);

  let currentRotation=getCurrentRotation(g.ctx);

  let needToRot=false;
  if((currentRotation)>Math.PI/2 || (currentRotation)<-Math.PI/2)//need to rotate 180 degree
  {
    needToRot=true;
  }
  let xOffset=0;
  if(needToRot)
  {
    //max text width
    for(let i=0;i<text.length;i++)
    {
      let textWidth=g.ctx.measureText(text[i]).width;
      if(textWidth>xOffset)
      {
        xOffset=textWidth;
      }
    }
    g.ctx.rotate(Math.PI);
  }

  for(let i=0;i<text.length;i++)
  {
    g.ctx.fillText(text[i], -xOffset, fontSize*i);
    g.ctx.strokeText(text[i], -xOffset, fontSize*i);
  }
  g.ctx.restore();


}

function HidableUI(props:{available?:boolean,defaultHide?:boolean,children:React.ReactNode,title:string})
{
  const {available=false,defaultHide=false,children,title}=props;
  const [hideFlag,setHideFlag]=useState<boolean>(defaultHide);
  useEffect(()=>{
    setHideFlag(available==false || defaultHide==true);
  },[available,defaultHide]);



  return <>
    <Divider key={title} > <span onClick={()=>{
      setHideFlag(!hideFlag);
    }}>{available?(hideFlag==true?<PlusOutlined/>:<MinusOutlined/>):<ExclamationOutlined/>} {title} </span></Divider>
    {(available&&!hideFlag)?children:null}
  </>;
}


function _UI_FeatureElement_Edit_SearchPoint(props: type_UI_FeatureElement_Edit_param) {

  let {it_id,featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate,Ref_Src_Info}=props;
  const dispatch = useDispatch();
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

  const [updateBump, setUpdateBump] = useState<number>(0);


  let _this = useRef<any>({
    drawTheme_editing:drawTheme.targetEditingForgroundDisp
  }).current;


  const [featureDrawSets, setFeatureDrawSets] = useState<type_DrawFeatureSet|undefined>(undefined);
  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate });


  async function Do_DBG_Inspection()
  {

    let pkts = await BPG_API.InspTargetExchange(it_id, { 
      type: "execute_inspection",
      use_cached_input:Ref_Src_Info.use_cache,
      file_name:Ref_Src_Info.file_name,
      folder_path:Ref_Src_Info.folder_path,
      // imageQuality:15,
      imageQuality:90,
      dbg_feature_id:targetFeatureElement.id,
      dbg_feature_info:{sdsd:"22222"}
      }) as any[];

    let IMs=pkts.filter((pkt:any)=>pkt.type=="IM");
    let RP=pkts.find((pkt:any)=>pkt.type=="RP");
    if(RP===undefined)
    {
    console.log(">>>",pkts);
    return;
    }

    let featureIdx=featureInfo.element_list.findIndex((feature:any)=>feature.id==targetFeatureElement.id);
    if(featureIdx==-1)
    {
    console.log(">>>",featureInfo.element_list);
    return;
    }

    console.log(">>>",pkts);
    
    _this.drawHook=undefined;
    let inspReport=RP?.data?.report?.[0];
    if(inspReport===undefined)
    {
      console.log("no report");
      return;
    }
    let featureReport=inspReport?.element_report?.[featureIdx];
    if(featureReport===undefined)
    {
      console.log("no target feature",inspReport);
      return;
    }

    console.log(">>>",featureReport);
    let dbg_info=featureReport.dbg_info;
    console.log(dbg_info);
    if(dbg_info===undefined)
    {
      console.log("no dbg_info");
      return;
    }
    
    _this.drawHook=(ctrl_or_draw:boolean, g:type_DrawHook_g, canvas_obj:DrawHook_CanvasComponent)=>{

        if(ctrl_or_draw==true)
        {
          return;
        }
        let edge_points=dbg_info?.edge_points;
        let consider_edge_points=dbg_info?.consider_edge_points;
        let camMag = canvas_obj.camera.GetCameraScale();


        console.log(">>>",_this.tmpDbgInfo?.featureReport,featureReport);
        if(featureReport===undefined)
        {

          return;
        }


        if(featureReport.pt1===undefined)
        {
          return;
        }

        if(featureReport.error_code!==undefined)
        {
          console.log(">>>",featureReport);
          return;
        }

        {
          g.ctx.strokeStyle="blue";
          //draw cross at pt1
          g.ctx.beginPath();
          let cross_size=15/camMag;
          g.ctx.moveTo(featureReport.pt1.x-cross_size,featureReport.pt1.y);
          g.ctx.lineTo(featureReport.pt1.x+cross_size,featureReport.pt1.y);
          g.ctx.moveTo(featureReport.pt1.x,featureReport.pt1.y-cross_size);
          g.ctx.lineTo(featureReport.pt1.x,featureReport.pt1.y+cross_size);
          g.ctx.stroke();




        }

        if(edge_points===undefined||edge_points.length<2)
        {
          return;
        }

        g.ctx.setLineDash([]);
        g.ctx.strokeStyle="red";
        g.ctx.lineWidth=2/camMag;
        g.ctx.beginPath();
        let preloc=edge_points[0];
        g.ctx.moveTo(preloc.x,preloc.y);

        for(let i=1;i<edge_points.length;i++)
        {
          let pt=edge_points[i];
          if(pt.y-preloc.y>2)
          {
            g.ctx.moveTo(pt.x,pt.y);
          }
          else
          {
            g.ctx.lineTo(pt.x,pt.y);
          }
          preloc=pt;
        }
        g.ctx.stroke();



        console.log(">>>",consider_edge_points);
        if(consider_edge_points!==undefined)
        {
          g.ctx.setLineDash([]);
          g.ctx.strokeStyle="green";
          g.ctx.lineWidth=2/camMag;
          g.ctx.beginPath();
          let preloc=consider_edge_points[0];
          g.ctx.moveTo(preloc.x,preloc.y);
  
          for(let i=1;i<consider_edge_points.length;i++)
          {
            let pt=consider_edge_points[i];
            let dist=Math.hypot(pt.x-preloc.x,pt.y-preloc.y);
            if(dist>2)
            {
              g.ctx.moveTo(pt.x,pt.y);
            }
            else
            {
              g.ctx.lineTo(pt.x,pt.y);
            }
            preloc=pt;
          }
          g.ctx.stroke();
        }

    }


    console.log(">>>",pkts);
    _this.drawTheme_editing={
      ...drawTheme.targetEditingForgroundDisp,
      main:Color("rgba(100,100,100,0.1)"),
    };
    _this.canvas_obj.draw();
  }

  useEffect(() => {
    Do_DBG_Inspection();
  }, [props.Ref_Src_Info]);

  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.canvas_obj=canvas_obj;
        cpe_drawHook(ctrl_or_draw, g, canvas_obj,_this.drawTheme_editing);

        let bgEle: any[] = featureInfo.element_list.filter((feature: any) => feature.id != targetFeatureElement.id);
        let refEle: any[] = bgEle.filter((feature: any) => targetFeatureElement.ref.some((ref: any) => ref.id == feature.id));



        drawFeatureSet(featureInfo,
          {
            bg: {
              element_list: bgEle,
              colorSetting: drawTheme.targetEditingBackgroundDisp
            },
            ref: {
              element_list: featureDrawSets===undefined||Object.keys(featureDrawSets).length == 0 ? refEle : [],
              colorSetting: {
                ...drawTheme.targetEditingForgroundDisp,
                main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5).darken(0.5),
                point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5).darken(0.5),
                start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5).darken(0.5),
                start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
              }
            }
          }, 0, ctrl_or_draw, g, canvas_obj);

        if(featureDrawSets!==undefined)
        {
          drawFeatureSet(featureInfo, featureDrawSets, 0, ctrl_or_draw, g, canvas_obj);
        }

        if(_this.drawHook!==undefined)
        {
          _this.drawHook(ctrl_or_draw, g, canvas_obj);
        }
      },
    })
  }, [cpe_drawHook, featureDrawSets])

  let curRefEle = targetFeatureElement.ref === undefined || targetFeatureElement.ref.length == 0 ? undefined :
    featureInfo.element_list.find((feature: any) => feature.id == targetFeatureElement.ref[0].id);



  let valueUpdate=(new_value:number,key:string,roundDigit:number=4)=>{
    onFeatureElementUpdate({ ...targetFeatureElement, [key]: roundDigit>0?Math.round(new_value*10**roundDigit)/10**roundDigit:new_value });
  }
  
  let adj_inc=0.01
  return <>

    <BasicPrimitiveSettingOption {...props} onFeatureElementUpdate={onFeatureElementUpdate}
    />


      
    <PopOverAdjuster selectCBs={{
        "/1.5":()=>{
          valueUpdate(targetFeatureElement.width / 1.5,"width");
        },
        "-":()=>{
          valueUpdate(targetFeatureElement.width - adj_inc,"width");
        },
        "+":()=>{
          valueUpdate(targetFeatureElement.width + adj_inc,"width");
        },
        "X1.5":()=>{
          valueUpdate(targetFeatureElement.width * 1.5,"width");
        },
      }} title="寬度">
      
        <Button >寬度</Button>
      </PopOverAdjuster>




    <InputNumber value={targetFeatureElement.width} onChange={(value: number) => {
      targetFeatureElement.width = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />

    <br />
    consider_range
    <InputNumber value={targetFeatureElement.consider_range??1} onChange={(value: number) => {
      targetFeatureElement.consider_range = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />

    <Space />
    loc_offset
    <InputNumber value={targetFeatureElement.loc_offset??0} onChange={(value: number) => {
      targetFeatureElement.loc_offset = value;
      onFeatureElementUpdate(targetFeatureElement);
    }} />





    <PopOverAdjuster selectCBs={{
      "-45": () =>{ valueUpdate(targetFeatureElement.angle - 45*Math.PI/180,"angle");},
      "-10": () => { valueUpdate(targetFeatureElement.angle - 10*Math.PI/180,"angle");},
      "0": () => { valueUpdate(0,"angle");},
      "10": () => { valueUpdate(targetFeatureElement.angle + 10*Math.PI/180,"angle");},
      "45": () => { valueUpdate(targetFeatureElement.angle + 45*Math.PI/180,"angle");},
    }} title="角度">
    
    <Button >角度</Button>
    </PopOverAdjuster>



    <InputNumber value={Number((targetFeatureElement.angle*180/Math.PI).toFixed(2))} onChange={(value: number) => {
      targetFeatureElement.angle = value*Math.PI/180;
      onFeatureElementUpdate(targetFeatureElement);
    }} />



    <br/>

    <Button onClick={()=>{
      Do_DBG_Inspection();
    }}>驗證</Button>


    <Dropdown overlay={<Menu
      onMouseLeave={(e: any) => {
        setFeatureDrawSets({});
      }}

      onMouseEnter={() => {

        let backgroundList = featureInfo.element_list.filter((feature: any) => feature.type == "LineFit");
        setFeatureDrawSets({
          background: {
            element_list: backgroundList,
            colorSetting: drawTheme.targetEditingBackgroundDisp
          }
        });
      }}


    >
      {featureInfo.element_list.filter((feature: any) => feature.type == "LineFit").map((feature: any) => {
        return <Menu.Item key={feature.id}
          onClick={() => {
            onFeatureElementUpdate({ ...targetFeatureElement, ref: [{ type: "base_on_angle", id: feature.id }] });
          }}
          onMouseEnter={() => {

            let backgroundList = featureInfo.element_list.filter((feature: any) => feature.type == "LineFit");
            let selectedlist = backgroundList.filter((_feature: any) => _feature.id == feature.id);
            console.log("selectedlist", selectedlist);
            setFeatureDrawSets({
              background: {
                element_list: backgroundList,
                colorSetting: drawTheme.targetEditingBackgroundDisp
              },

              selected: {
                element_list: selectedlist,
                colorSetting: drawTheme.defDisp
              }
            });
          }}
          onMouseLeave={() => {
            let backgroundList = featureInfo.element_list.filter((feature: any) => feature.type == "LineFit");
            setFeatureDrawSets({
              background: {
                element_list: backgroundList,
                colorSetting: drawTheme.targetEditingBackgroundDisp
              }
            });
          }}
        >{feature.name ?? ("ID:" + feature.id)}</Menu.Item>
      })}
    </Menu>}>
      <Button onClick={() => {
      }}>{curRefEle === undefined ? "選擇參考點" : ("參考:" + curRefEle.name)}</Button>
    </Dropdown>

  </>
}

function drawArrowTipToOrigin(g:type_DrawHook_g) { //from -1,0 to 0,0, arrow buttom width is 1
  const ctx = g.ctx;
  
  // Save current context state
  ctx.save();
  
  // Begin new path
  ctx.beginPath();
  
  // Draw arrow tip - triangle from (-1,0) to (0,0)
  ctx.moveTo(-1, -0.5); // Bottom point
  ctx.lineTo(0, 0);     // Tip point
  ctx.lineTo(-1, 0.5);  // Top point
  
  // Close the path to complete the triangle
  ctx.closePath();
  
  // Fill and stroke the arrow
  ctx.fill();
  
  // Restore context state
  ctx.restore();
}
function drawArrowLine(g:type_DrawHook_g,from_pt:VEC2D,to_pt:VEC2D,tip_size:number=1)
{
  g.ctx.beginPath();
  g.ctx.moveTo(from_pt.x,from_pt.y);
  g.ctx.lineTo(to_pt.x,to_pt.y);
  g.ctx.stroke();
  
  let angle=Math.atan2(to_pt.y-from_pt.y,to_pt.x-from_pt.x);
  g.ctx.save();
  g.ctx.translate(to_pt.x,to_pt.y);
  g.ctx.rotate(angle);
  g.ctx.scale(tip_size,tip_size);
  drawArrowTipToOrigin(g);
  g.ctx.restore();

}

function drawTwoHeadArrowLine(g:type_DrawHook_g,from_pt:VEC2D,to_pt:VEC2D,tip_size:number=1)
{

  g.ctx.beginPath();
  g.ctx.moveTo(from_pt.x,from_pt.y);
  g.ctx.lineTo(to_pt.x,to_pt.y);
  g.ctx.stroke();
  
  let angle=Math.atan2(to_pt.y-from_pt.y,to_pt.x-from_pt.x);


  g.ctx.save();
  g.ctx.translate(from_pt.x,from_pt.y);
  g.ctx.rotate(angle+Math.PI);
  g.ctx.scale(tip_size,tip_size);
  drawArrowTipToOrigin(g);
  g.ctx.restore();


  g.ctx.save();
  g.ctx.translate(to_pt.x,to_pt.y);
  g.ctx.rotate(angle);
  g.ctx.scale(tip_size,tip_size);
  drawArrowTipToOrigin(g);
  g.ctx.restore();


}




function vecNormalize(vec:{x:number,y:number})
{
  let len=Math.hypot(vec.x,vec.y);
  return {x:vec.x/len,y:vec.y/len};
}


function get_feature_point_loc(featureEle:any)
{
  switch(featureEle.type)
  {
    case "LineFit":
      return {x:(featureEle.pt1.x+featureEle.pt2.x)/2,y:(featureEle.pt1.y+featureEle.pt2.y)/2};
    case "SearchPoint":
      return featureEle.pt1;
    case "ArcFit":
      {
        let arcParam=threePointToArc(featureEle.pt1,featureEle.pt2,featureEle.pt3);
        return {x:arcParam.x,y:arcParam.y};
      }
  }
  return {x:NaN,y:NaN};
}


function get_feature_point_loc_from_report(featureEle:any,reportObj:any)
{
  if(reportObj!==undefined && reportObj.error_code==undefined)
  {
    switch(featureEle.type)
    {
      case "LineFit":
        return {x:(reportObj.pt1.x+reportObj.pt2.x)/2,y:(reportObj.pt1.y+reportObj.pt2.y)/2};
      case "SearchPoint":
        return reportObj.pt1;
      case "ArcFit":
        {
          return {x:reportObj.c.x,y:reportObj.c.y};
        }
    }

  }
  return {x:NaN,y:NaN};
}


//-----------------------------------Measure_Distance
function _Draw_FeatureElement_Edit_Measure_Distance(
  featureInfo: any,
  featureEle: any,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
) {
  if (ctrl_or_draw == true) {
    return;
  }

  
  let camMag = canvas_obj.camera.GetCameraScale();
  //draw name
  g.ctx.fillStyle = colorSetting.main.toString();

  g.ctx.setLineDash([]);
  let isRefReady=false;


  let refObj1_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj1")?.id;
  let refObj2_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj2")?.id;
  let refObj_project_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj_project")?.id;

  let refObj1_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj1_id);
  let refObj2_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj2_id);
  let refObj_project_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj_project_id);



  let loc1={x:NaN,y:NaN};
  let loc2={x:NaN,y:NaN};
  let projectVec_normalized={x:NaN,y:NaN};


  let refObj1=featureInfo.element_list[refObj1_idx];
  let refObj2=featureInfo.element_list[refObj2_idx];
  let refObj_project=featureInfo.element_list[refObj_project_idx];
  let error_code=0;
  let error_msg="";
  let distance=NaN;
  if(reportEle!==undefined)
  {
    error_code=reportEle?.error_code??0;
    error_msg=reportEle?.error_msg??'';
    let refObjRep1=reportObj?.element_report?.[refObj1_idx];
    let refObjRep2=reportObj?.element_report?.[refObj2_idx];
    let refObjRep_project=reportObj?.element_report?.[refObj_project_idx];


    if(refObjRep1!==undefined&&refObjRep2!==undefined&&refObjRep_project!==undefined)
    {
      isRefReady=true;


      // console.log(">>>",refObj1,refObj2,refObj_project);


      loc1=get_feature_point_loc_from_report(refObj1,refObjRep1);
      loc2=get_feature_point_loc_from_report(refObj2,refObjRep2);


      projectVec_normalized=vecNormalize({x:refObjRep_project.pt2.x-refObjRep_project.pt1.x,y:refObjRep_project.pt2.y-refObjRep_project.pt1.y});



    }
    distance=reportEle?.value??NaN;
    // reportObj.angle=featureInfo.template_angle;
    // console.log(">>>",reportObj);
    // g.ctx.save();
    // g.ctx.translate(featureEle.disp_pt1.x, featureEle.disp_pt1.y);
    // g.ctx.rotate(-reportObj.angle+featureInfo.template_angle);

    // g.ctx.fillText(featureEle.name, 0, 0);
    // g.ctx.restore();
  }
  else {



    if(refObj1!==undefined&&refObj2!==undefined&&refObj_project!==undefined)
    {
      isRefReady=true;


      // console.log(">>>",refObj1,refObj2,refObj_project);
      loc1=get_feature_point_loc(refObj1);
      loc2=get_feature_point_loc(refObj2);


      projectVec_normalized=vecNormalize({x:refObj_project.pt2.x-refObj_project.pt1.x,y:refObj_project.pt2.y-refObj_project.pt1.y});



    }




  }


  let textRotateTheta=0;
  if(isRefReady==true)
  {
    console.log(">>>",featureInfo,featureEle,reportObj,reportEle);
    let projectRotateTheta=featureEle.rotate??Math.PI/2;
    let projectVec_rotated=PtRotate2d(projectVec_normalized,projectRotateTheta);
    let projectVec_rotated_normal={x:-projectVec_rotated.y,y:projectVec_rotated.x};

    let line1={pt1:loc1,pt2:vecXY_add(loc1,projectVec_rotated_normal)};
    let line2={pt1:loc2,pt2:vecXY_add(loc2,projectVec_rotated_normal)};

    let linec={pt1:featureEle.ctrl_pt1,pt2:vecXY_add(featureEle.ctrl_pt1,projectVec_rotated)};



    let intersec1=intersectPoint(line1.pt1,line1.pt2,linec.pt1,linec.pt2);
    let intersec2=intersectPoint(line2.pt1,line2.pt2,linec.pt1,linec.pt2);
    if(reportEle===undefined)
    {//no report, use intersec1 and intersec2 to calculate distance
      distance=distance_point_point(intersec1,intersec2);


      let ds=featureEle.distance_select;

      if(ds==1 || ds==2)
      {
        let vector_to_project={x:intersec2.x-intersec1.x,y:intersec2.y-intersec1.y};
        let vector_to_project_normalized=vecNormalize(vector_to_project);
        let dprod=(vector_to_project_normalized.x*projectVec_rotated.x+vector_to_project_normalized.y*projectVec_rotated.y);
        // console.log("LINE sign>>>",dprod);
        
        if(dprod>0)dprod=1;
        else dprod=-1;

        if(ds==2)dprod*=-1;

        distance*=dprod;
      }
      

    }
    g.ctx.beginPath();
    g.ctx.arc(intersec1.x, intersec1.y, 2/camMag, 0, Math.PI * 2);
    g.ctx.fill();
    g.ctx.beginPath();
    g.ctx.arc(intersec2.x, intersec2.y, 2/camMag, 0, Math.PI * 2);
    g.ctx.fill();

    

    // g.ctx.strokeStyle="blue";
    g.ctx.beginPath();

    g.ctx.strokeStyle = colorSetting.indication_extended_line.toString();
    g.ctx.lineWidth = colorSetting.indication_extended_line_width/camMag;
    g.ctx.setLineDash(colorSetting.indication_extended_line_dash.map(d=>d/camMag));
  
    //line loc1 to intersec1
    g.ctx.moveTo(loc1.x,loc1.y);
    g.ctx.lineTo(intersec1.x,intersec1.y);

    //line loc2 to intersec2
    g.ctx.moveTo(loc2.x,loc2.y);
    g.ctx.lineTo(intersec2.x,intersec2.y);

    g.ctx.stroke();

    g.ctx.fillStyle =
    g.ctx.strokeStyle = colorSetting.indication_line.toString();
    g.ctx.lineWidth = colorSetting.indication_line_width/camMag;
    g.ctx.setLineDash(colorSetting.indication_line_dash.map(d=>d/camMag));

    {
      let isCtrlPt1_in_middle=false;
      let dist12=distance_point_point(intersec1,intersec2);
      let distc1=distance_point_point(featureEle.ctrl_pt1,intersec1);
      let distc2=distance_point_point(featureEle.ctrl_pt1,intersec2);
      isCtrlPt1_in_middle=(distc1+distc2)<(dist12+1);

      if(isCtrlPt1_in_middle==true)
      {
        g.ctx.beginPath();
    
        g.ctx.lineWidth = colorSetting.indication_line_width/camMag;
        //line featureEle.ctrl_pt1 to intersec1
        g.ctx.moveTo(intersec2.x,intersec2.y);
        g.ctx.lineTo(intersec1.x,intersec1.y);
    
        g.ctx.stroke();


      }
      else
      {


        g.ctx.beginPath();

        let close_pt_2_ctrl=distc1<distc2?intersec1:intersec2;
    
        //line featureEle.ctrl_pt1 to intersec2
        // g.ctx.moveTo(featureEle.ctrl_pt1.x,featureEle.ctrl_pt1.y);
        // g.ctx.lineTo(close_pt_2_ctrl.x,close_pt_2_ctrl.y);
        drawArrowLine(g,featureEle.ctrl_pt1,close_pt_2_ctrl,g.ctx.lineWidth*3);
    
    
    
        g.ctx.lineWidth = colorSetting.indication_line_width/camMag*0.7;
        //line featureEle.ctrl_pt1 to intersec1
        // g.ctx.moveTo(intersec2.x,intersec2.y);
        // g.ctx.lineTo(intersec1.x,intersec1.y);
    
        // g.ctx.stroke();
    
        
        drawTwoHeadArrowLine(g,intersec1,intersec2,g.ctx.lineWidth*3);
    
    
      }

      textRotateTheta=Math.atan2(intersec2.y-intersec1.y,intersec2.x-intersec1.x);

    }

  }




  g.ctx.save();
  // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
  g.ctx.translate(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y);
  g.ctx.rotate(textRotateTheta);
  draw_feature_text(g,canvas_obj,colorSetting,
    [
      featureEle.name,
      "D:"+distance.toFixed(4),

      ...(error_code!==0?["E:<"+error_code+">"+error_msg]:[]),
    ]
  );
  g.ctx.restore();




  g.ctx.fillStyle = colorSetting["point"].toString();



  //draw dot at pt1 and pt2
  g.ctx.save();
  g.ctx.beginPath();
  g.ctx.arc(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y, 5/camMag, 0, Math.PI * 2);
  g.ctx.fill();
  g.ctx.restore();
}

function _UI_FeatureElement_Edit_Measure_Distance({ featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit }: type_UI_FeatureElement_Edit_param) {

  let props = { featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate };


  let _this = useRef<any>({
    drawHook: undefined,
  }).current;
  const [ControlPointSelect_DrawHook_param, setControlPointSelect_DrawHook_param] = useState<type_ControlPointSelect_DrawHook_param|undefined>(undefined);
  const ControlPointSelect_DrawHook = useControlPointSelect_DrawHook(ControlPointSelect_DrawHook_param);

  const [featureDrawSets, setFeatureDrawSets] = useState<{
    [key: string]: {
      enable?: boolean,
      list: any[],
      drawRefLevel?: number,
      colorSetting: ColorSetting_type
    }
  }>({
    // background: {
    //     list:featureInfo.element_list.filter((feature:any)=>feature.id!=targetFeatureElement.id),
    //     colorSetting:drawTheme.targetEditingBackgroundDisp
    // }
  });

  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate });




  _this.drawHook = (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
    cpe_drawHook(ctrl_or_draw, g, canvas_obj,drawTheme.targetEditingForgroundDisp);

    if(ctrl_or_draw==false)
    {

      let bgEle: any[] = featureInfo.element_list.filter((feature: any) => feature.id != targetFeatureElement.id);
      let ref_ele=targetFeatureElement.ref.map((ref:any)=>featureInfo.element_list.find((feature:any)=>feature.id==ref.id));
      // console.log(">>>", candidateFilter);
  
      drawFeatureSet(featureInfo,
        {
          bg: {
            element_list: bgEle,
            colorSetting: drawTheme.targetEditingBackgroundDisp
          },
          ref: {
            element_list: ControlPointSelect_DrawHook_param===undefined?ref_ele:[],
            colorSetting: {
              ...drawTheme.targetEditingForgroundDisp,
              main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5),
              point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5),
              start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5),
              start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
            }
          },
          ref_select:{
            element_list: ControlPointSelect_DrawHook_param?.featureEleList ?? [],
            colorSetting: {
              ...drawTheme.targetEditingForgroundDisp,
              main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5).darken(0.5),
              point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5).darken(0.5),
              start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5).darken(0.5),
              start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
            }
          }
        }, 0, ctrl_or_draw, g, canvas_obj);
    }

    ControlPointSelect_DrawHook(ctrl_or_draw, g, canvas_obj);
    // drawFeatureSet(featureInfo,featureDrawSets,0,ctrl_or_draw,g,canvas_obj);
  }
  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.drawHook(ctrl_or_draw, g, canvas_obj);
      },
    })
  }, [])

  function findRefObj(type:string)
  {
    let refObjInfo = targetFeatureElement.ref.find((ref:any)=>ref.type==type) as {type:string,id:number};
    return refObjInfo===undefined?undefined:featureInfo.element_list.find((feature:any)=>feature.id==refObjInfo.id);
  }


  function EnterRefSelectMode(type:string,candEleList:any[],onFeatureSelected:(featureEle:any,path:(string|number)[],distance:number)=>void)
  {
    setControlPointSelect_DrawHook_param({
      featureEleList: candEleList,
      distanceThreshold: 10,
      colorSetting: drawTheme.targetEditingForgroundDisp,
      onFeatureSelected: (featureEle: any, path: (string | number)[], distance: number) => {
        console.log(">>>", featureEle, path, distance);
        setControlPointSelect_DrawHook_param(undefined);
          onFeatureSelected(featureEle,path,distance);

       },
      onFeatureHovered: (featureEle: any, path: (string | number)[], distance: number) => {
      }
    });
  }
  function EnterRefSelectMode_autoSetNewRef(type:string,candEleList:any[],onFeatureSelected?:(featureEle:any,path:(string|number)[],distance:number)=>void)
  {
    EnterRefSelectMode(type,candEleList,
      onFeatureSelected!==undefined?onFeatureSelected:
      (featureEle:any,path:(string|number)[],distance:number)=>{
      let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!=type);
      onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:type,id:featureEle.id}]});
    });
  }


  let refObj1=findRefObj("obj1");
  let refObj2=findRefObj("obj2");

  let refObj_project=findRefObj("obj_project");

  function RefEditUI(Obj1CandTypeFilter:string[],Obj2CandTypeFilter:string[],Obj_projectCandTypeFilter:string[],
    onObj1Select?:(featureEle:any,path:(string|number)[],distance:number)=>void,
    onObj2Select?:(featureEle:any,path:(string|number)[],distance:number)=>void,
    onObj_projectSelect?:(featureEle:any,path:(string|number)[],distance:number)=>void
  )
  {

    return <>
      Obj1:
      <Button onClick={() => {

        let candEleList = featureInfo.element_list.filter((feature: any) => Obj1CandTypeFilter.includes(feature.type));
        EnterRefSelectMode_autoSetNewRef("obj1",candEleList,onObj1Select);

      }}>{refObj1=== undefined ? "選擇參考1" : ("參考1:" + refObj1.name)}</Button>


      Obj2:
      <Button onClick={() => {
        let candEleList = featureInfo.element_list.filter((feature: any) => Obj2CandTypeFilter.includes(feature.type));
        EnterRefSelectMode_autoSetNewRef("obj2",candEleList,onObj2Select);
      }}>{refObj2=== undefined ? "選擇參考2" : ("參考2:" + refObj2.name)}</Button>

      *Obj_project:
      <Button onClick={() => {
        let candEleList = featureInfo.element_list.filter((feature: any) => Obj_projectCandTypeFilter.includes(feature.type));
        EnterRefSelectMode_autoSetNewRef("obj_project",candEleList,onObj_projectSelect);
        }}>{refObj_project=== undefined ? "選擇投影參考" : ("參考:" + refObj_project.name)}</Button>
    </>
  }



  let EditUI:React.ReactNode[]=[];

  type type_subtype="point_to_point"|"line_to_point"|"arc_to_point"|"line_to_line"|"line_to_arc"|"arc_to_arc";
  


  {
    //dropdown menu to select subtype
    const menu = (
      <Menu onClick={(e) => {
        // Handle the selection of a subtype
        onFeatureElementUpdate({ ...targetFeatureElement, subtype: e.key,ref:[] });//also clear ref
      }}>
        <Menu.Item key="point_to_point">Point to Point</Menu.Item>
        <Menu.Item key="line_to_point">Line to Point</Menu.Item>
        <Menu.Item key="arc_to_point">Arc to Point</Menu.Item>
        <Menu.Item key="line_to_line">Line to Line</Menu.Item>
        <Menu.Item key="line_to_arc">Line to Arc</Menu.Item>
        <Menu.Item key="arc_to_arc">Arc to Arc</Menu.Item>
      </Menu>
    );

    // Use Dropdown component
    EditUI.push(
      <HidableUI available={true} title="特徵類型">
        <Dropdown overlay={menu}>
          <Button>
          {targetFeatureElement.subtype===undefined?"選擇特徵類型":targetFeatureElement.subtype}
        </Button>
        </Dropdown>
      </HidableUI>
    );

  }
  

  let isRefReady=(refObj1!==undefined && refObj2!==undefined && refObj_project!==undefined);



  // if(targetFeatureElement.subtype!==undefined)
  {


    let refEditUI:React.ReactNode=<></>;
    switch(targetFeatureElement.subtype as type_subtype)
    {
      case "point_to_point":
      {
        refEditUI=(
          RefEditUI(["SearchPoint"],["SearchPoint"],["LineFit","SearchPoint"])
        );
        break;
      }

      case "line_to_point":
      {
        refEditUI=(
          RefEditUI(["LineFit"],["SearchPoint"],["LineFit","SearchPoint"],
            (featureEle:any,path:(string|number)[],distance:number)=>{
              
              let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!="obj1" && ref.type!="obj_project");
              onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:"obj1",id:featureEle.id},{type:"obj_project",id:featureEle.id}]});


            }
          ));
        break;
      }


      case "arc_to_point":
      {
        refEditUI=(
          RefEditUI(["ArcFit"],["SearchPoint"],["LineFit","SearchPoint"])
        );
        break;
      }



      case "line_to_line":
      {
        
        refEditUI=(
          RefEditUI(["LineFit"],["LineFit"],["LineFit","SearchPoint"],
            (featureEle:any,path:(string|number)[],distance:number)=>{
              let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!="obj1" && ref.type!="obj_project");
              onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:"obj1",id:featureEle.id},{type:"obj_project",id:featureEle.id}]});
            }
          )
        );
        break;
      }

      case "line_to_arc":
      {
        
        refEditUI=(
          RefEditUI(["LineFit"],["ArcFit"],["LineFit","SearchPoint"],
            (featureEle:any,path:(string|number)[],distance:number)=>{
              let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!="obj1" && ref.type!="obj_project");
              onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:"obj1",id:featureEle.id},{type:"obj_project",id:featureEle.id}]});
            }
          )
        );
        break;
      }


      case "arc_to_arc":
      {
        refEditUI=(
          RefEditUI(["ArcFit"],["ArcFit"],["LineFit","SearchPoint"]
          )
        );
        break;
      }
    }

    EditUI.push(<HidableUI available={targetFeatureElement.subtype!==undefined} title="參考設定">{refEditUI}</HidableUI>);

  }






  let rotate_input_value=targetFeatureElement.rotate??Math.PI/2;
  EditUI.push(<HidableUI available={isRefReady} title="詳細設定">
    {/* <Button onClick={()=>{
      onFeatureElementUpdate({...targetFeatureElement,subtype:undefined,ref:[]});
    }}>清除特徵</Button> */}
    {/* <Button onClick={()=>{
      
    }}>..dwad.</Button> */}


    距離類型:
    <InputNumber  value={targetFeatureElement.distance_select??0} min={-1} max={3} step={1} onChange={(value)=>{
      if(value<0)value+=3;
      onFeatureElementUpdate({...targetFeatureElement,distance_select:value%3});
    }} />


    <InputNumber value={Number((rotate_input_value*180/Math.PI).toFixed(2))} onChange={(value:number)=>{
      onFeatureElementUpdate({...targetFeatureElement,rotate:value*Math.PI/180});
    }}/>
  </HidableUI>);

    


  return <>
    <Input value={targetFeatureElement.name} onChange={(e: any) => {
      onFeatureElementUpdate({ ...targetFeatureElement, name: e.target.value });
    }} />
    {EditUI}
  </>
}




//-----------------------------------Measure_Angle-----------------------------------
function _Draw_FeatureElement_Edit_Measure_Angle(
  featureInfo: any,
  featureEle: any,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
) {
  if (ctrl_or_draw == true) {
    return;
  }
  let camMag = canvas_obj.camera.GetCameraScale();
  //draw name
  g.ctx.fillStyle = colorSetting.main.toString();

  g.ctx.setLineDash([]);
  let isRefReady=false;


  let refObj1_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj1")?.id;
  let refObj2_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj2")?.id;

  let refObj1_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj1_id);
  let refObj2_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj2_id);



  let line1={pt1:{x:NaN,y:NaN},pt2:{x:NaN,y:NaN}};
  let line2={pt1:{x:NaN,y:NaN},pt2:{x:NaN,y:NaN}};

  let error_code=0;
  let error_msg="";
  let angle=NaN;


  let angle_select=featureEle.angle_select??0;
  if(reportEle!==undefined)
  {
    error_code=reportEle?.error_code??0;
    error_msg=reportEle?.error_msg??'';
    let refObj1=reportObj?.element_report?.[refObj1_idx];
    let refObj2=reportObj?.element_report?.[refObj2_idx];


    if(refObj1!==undefined&&refObj2!==undefined)
    {
      isRefReady=true;
      line1={pt1:{...refObj1.pt1},pt2:{...refObj1.pt2}};
      line2={pt1:{...refObj2.pt1},pt2:{...refObj2.pt2}};
    }
    angle=reportEle?.value??NaN;
    // reportObj.angle=featureInfo.template_angle;
    // console.log(">>>",reportObj);
    // g.ctx.save();
    // g.ctx.translate(featureEle.disp_pt1.x, featureEle.disp_pt1.y);
    // g.ctx.rotate(-reportObj.angle+featureInfo.template_angle);

    // g.ctx.fillText(featureEle.name, 0, 0);
    // g.ctx.restore();
    // console.log(">rep>>",refObj1,refObj2);
  }
  else {


    let refObj1=featureInfo.element_list[refObj1_idx];
    let refObj2=featureInfo.element_list[refObj2_idx];

    if(refObj1!==undefined&&refObj2!==undefined)
    {
      isRefReady=true;


      // console.log(">>>",refObj1,refObj2,refObj_project);

      line1={pt1:{...refObj1.pt1},pt2:{...refObj1.pt2}};
      line2={pt1:{...refObj2.pt1},pt2:{...refObj2.pt2}};




    }

    // console.log(">def>>",refObj1,refObj2);


  }

  let textRotateTheta=0;

  if(isRefReady==true)
  { 
    let vec1=vecNormalize({x:line1.pt2.x-line1.pt1.x,y:line1.pt2.y-line1.pt1.y});
    let vec2=vecNormalize({x:line2.pt2.x-line2.pt1.x,y:line2.pt2.y-line2.pt1.y});
    
    let intersect_pt=intersectPoint(line1.pt1,line1.pt2,line2.pt1,line2.pt2);

    let as=Math.atan2(vec1.y,vec1.x);
    let ae=Math.atan2(vec2.y,vec2.x);


    {
      let spand=Math.floor(angle_select/4);
      if(spand%2==1)
      {
        ae+=Math.PI;
      }
    }


    switch(angle_select%4)
    {
      case 0:
        break;
      
      case 1://
      {
        let at=as;
        as=ae;
        ae=at;
        ae+=Math.PI;
      }
        break;
      case 2://
        ae+=Math.PI;
        as+=Math.PI;
        break;
      case 3://
      {
        as+=Math.PI;
        ae+=Math.PI;

        let at=as;
        as=ae;
        ae=at;
        ae+=Math.PI;
      }
        break;

        
      break;
    }


    let draw_UI_calc_angle=ae-as;
    {//make angle in [0,2*Math.PI] at any range
      draw_UI_calc_angle=draw_UI_calc_angle%(Math.PI*2);
      if(draw_UI_calc_angle<0)
      {
        draw_UI_calc_angle+=Math.PI*2;
      }


      ae=as+draw_UI_calc_angle;
      if(featureEle.is_principal_range==true)
      {
        if(draw_UI_calc_angle>Math.PI)
        {
          draw_UI_calc_angle-=Math.PI*2;
        }
      }


      // ae=as+angle;
    }


    let draw_radius=distance_point_point(intersect_pt,featureEle.ctrl_pt1);

    {//angle ctrl
      let angle_ctrl_pt=featureEle.ctrl_pt1;
      let angle_ctrl_pt_vec=vecNormalize({x:angle_ctrl_pt.x-intersect_pt.x,y:angle_ctrl_pt.y-intersect_pt.y});
      let ctrl_pt_angle=Math.atan2(angle_ctrl_pt_vec.y,angle_ctrl_pt_vec.x);

      textRotateTheta=ctrl_pt_angle;
      let angle_from_as=ctrl_pt_angle-as;
      {
        angle_from_as=angle_from_as%(Math.PI*2);
        if(angle_from_as<0)
        {
          angle_from_as+=Math.PI*2;
        }
        ctrl_pt_angle=as+angle_from_as;
      }
      if(angle_from_as<draw_UI_calc_angle)
      {//in range

      }
      else
      {
        g.ctx.strokeStyle = colorSetting.indication_extended_line.toString();
        g.ctx.lineWidth = colorSetting.indication_extended_line_width/camMag;
        g.ctx.setLineDash(colorSetting.indication_extended_line_dash.map(d=>d/camMag));
        let angle_middle=ae+draw_UI_calc_angle/2;
        g.ctx.beginPath();

        if(angle_from_as-angle_middle>Math.PI)
        {
          g.ctx.arc(intersect_pt.x, intersect_pt.y, draw_radius, ctrl_pt_angle, as);
        }
        else
        {
          g.ctx.arc(intersect_pt.x, intersect_pt.y, draw_radius, ae, ctrl_pt_angle);
        }
        g.ctx.stroke();
      }

    }

    if(reportEle!==undefined)
    {//if there is report, use report value
      angle=reportEle?.value??NaN;
    }
    else
    {//if there is no report, use calculated value
      angle=draw_UI_calc_angle;
    }


    g.ctx.fillStyle =
    g.ctx.strokeStyle = colorSetting.indication_line.toString();
    g.ctx.lineWidth = colorSetting.indication_line_width/camMag*0.7;
    g.ctx.setLineDash(colorSetting.indication_line_dash.map(d=>d/camMag));

    g.ctx.beginPath();
    g.ctx.arc(intersect_pt.x, intersect_pt.y, draw_radius, as, ae);
    g.ctx.stroke();

  }


  g.ctx.fillStyle =
  g.ctx.strokeStyle = colorSetting.indication_line.toString();
  g.ctx.lineWidth = colorSetting.indication_line_width/camMag;
  g.ctx.setLineDash(colorSetting.indication_line_dash.map(d=>d/camMag));

  


  g.ctx.fillStyle = colorSetting["point"].toString();
  //draw dot at pt1 and pt2
  g.ctx.save();
  g.ctx.beginPath();
  g.ctx.arc(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y, 5/camMag, 0, Math.PI * 2);
  g.ctx.fill();
  g.ctx.restore();


    g.ctx.save();
    // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
    g.ctx.translate(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y);
    g.ctx.rotate(textRotateTheta);
    draw_feature_text(g,canvas_obj,colorSetting,
      [
        featureEle.name,
        "A:"+(angle*180/Math.PI).toFixed(2),

        ...(error_code!==0?["E:<"+error_code+">"+error_msg]:[]),
      ]
    );
    g.ctx.restore();

}

function _UI_FeatureElement_Edit_Measure_Angle({ featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit }: type_UI_FeatureElement_Edit_param) {

  let props = { featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate };


  let _this = useRef<any>({
    drawHook: undefined,
  }).current;
  const [ControlPointSelect_DrawHook_param, setControlPointSelect_DrawHook_param] = useState<type_ControlPointSelect_DrawHook_param|undefined>(undefined);
  const ControlPointSelect_DrawHook = useControlPointSelect_DrawHook(ControlPointSelect_DrawHook_param);

  const [featureDrawSets, setFeatureDrawSets] = useState<{
    [key: string]: {
      enable?: boolean,
      list: any[],
      drawRefLevel?: number,
      colorSetting: ColorSetting_type
    }
  }>({
    // background: {
    //     list:featureInfo.element_list.filter((feature:any)=>feature.id!=targetFeatureElement.id),
    //     colorSetting:drawTheme.targetEditingBackgroundDisp
    // }
  });

  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate });




  _this.drawHook = (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
    cpe_drawHook(ctrl_or_draw, g, canvas_obj,drawTheme.targetEditingForgroundDisp);

    if(ctrl_or_draw==false)
    {

      let bgEle: any[] = featureInfo.element_list.filter((feature: any) => feature.id != targetFeatureElement.id);
      let ref_ele=targetFeatureElement.ref.map((ref:any)=>featureInfo.element_list.find((feature:any)=>feature.id==ref.id));
      // console.log(">>>", candidateFilter);
  
      drawFeatureSet(featureInfo,
        {
          bg: {
            element_list: bgEle,
            colorSetting: drawTheme.targetEditingBackgroundDisp
          },
          ref: {
            element_list: ControlPointSelect_DrawHook_param===undefined?ref_ele:[],
            colorSetting: {
              ...drawTheme.targetEditingForgroundDisp,
              main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5),
              point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5),
              start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5),
              start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
            }
          },
          ref_select:{
            element_list: ControlPointSelect_DrawHook_param?.featureEleList ?? [],
            colorSetting: {
              ...drawTheme.targetEditingForgroundDisp,
              main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5).darken(0.5),
              point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5).darken(0.5),
              start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5).darken(0.5),
              start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
            }
          }
        }, 0, ctrl_or_draw, g, canvas_obj);
    }

    ControlPointSelect_DrawHook(ctrl_or_draw, g, canvas_obj);
    // drawFeatureSet(featureInfo,featureDrawSets,0,ctrl_or_draw,g,canvas_obj);
  }
  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.drawHook(ctrl_or_draw, g, canvas_obj);
      },
    })
  }, [])

  function findRefObj(type:string)
  {
    let refObjInfo = targetFeatureElement.ref?.find((ref:any)=>ref.type==type) as {type:string,id:number};
    return refObjInfo===undefined?undefined:featureInfo.element_list.find((feature:any)=>feature.id==refObjInfo.id);
  }


  function EnterRefSelectMode(type:string,candEleList:any[],onFeatureSelected:(featureEle:any,path:(string|number)[],distance:number)=>void)
  {
    setControlPointSelect_DrawHook_param({
      featureEleList: candEleList,
      distanceThreshold: 10,
      colorSetting: drawTheme.targetEditingForgroundDisp,
      onFeatureSelected: (featureEle: any, path: (string | number)[], distance: number) => {
        console.log(">>>", featureEle, path, distance);
        setControlPointSelect_DrawHook_param(undefined);
          onFeatureSelected(featureEle,path,distance);

       },
      onFeatureHovered: (featureEle: any, path: (string | number)[], distance: number) => {
      }
    });
  }
  function EnterRefSelectMode_autoSetNewRef(type:string,candEleList:any[],onFeatureSelected?:(featureEle:any,path:(string|number)[],distance:number)=>void)
  {
    EnterRefSelectMode(type,candEleList,
      onFeatureSelected!==undefined?onFeatureSelected:
      (featureEle:any,path:(string|number)[],distance:number)=>{
      let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!=type);
      onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:type,id:featureEle.id}]});
    });
  }


  let refObj1=findRefObj("obj1");
  let refObj2=findRefObj("obj2");

  function RefEditUI(Obj1CandTypeFilter:string[],Obj2CandTypeFilter:string[],
    onObj1Select?:(featureEle:any,path:(string|number)[],distance:number)=>void,
    onObj2Select?:(featureEle:any,path:(string|number)[],distance:number)=>void
  )
  {

    return <>
      Obj1:
      <Button onClick={() => {

        let candEleList = featureInfo.element_list.filter((feature: any) => Obj1CandTypeFilter.includes(feature.type));
        EnterRefSelectMode_autoSetNewRef("obj1",candEleList,onObj1Select);

      }}>{refObj1=== undefined ? "選擇參考1" : ("參考1:" + refObj1.name)}</Button>


      Obj2:
      <Button onClick={() => {
        let candEleList = featureInfo.element_list.filter((feature: any) => Obj2CandTypeFilter.includes(feature.type));
        EnterRefSelectMode_autoSetNewRef("obj2",candEleList,onObj2Select);
      }}>{refObj2=== undefined ? "選擇參考2" : ("參考2:" + refObj2.name)}</Button>

    </>
  }



  let EditUI:React.ReactNode[]=[];

  type type_subtype="line_to_line";
  
  let subtype=targetFeatureElement.subtype??"line_to_line";

  // {
  //   //dropdown menu to select subtype
  //   const menu = (
  //     <Menu onClick={(e) => {
  //       // Handle the selection of a subtype
  //       onFeatureElementUpdate({ ...targetFeatureElement, subtype: e.key,ref:[] });//also clear ref
  //     }}>
  //       <Menu.Item key="line_to_line">Line to Line</Menu.Item>
  //     </Menu>
  //   );

  //   // Use Dropdown component
  //   EditUI.push(
  //     <HidableUI available={true} title="特徵類型">
  //       <Dropdown overlay={menu}>
  //         <Button>
  //         {targetFeatureElement.subtype===undefined?"選擇特徵類型":targetFeatureElement.subtype}
  //       </Button>
  //       </Dropdown>
  //     </HidableUI>
  //   );

  // }
  

  let isRefReady=(refObj1!==undefined && refObj2!==undefined);



  // if(targetFeatureElement.subtype!==undefined)
  {


    let refEditUI:React.ReactNode=<></>;
    
    switch(subtype as type_subtype)
    {


      case "line_to_line":
      {
        
        refEditUI=(
          RefEditUI(["LineFit"],["LineFit"])
        );
        break;
      }

    }

    EditUI.push(<HidableUI available={subtype} title="參考設定">{refEditUI}</HidableUI>);

  }






  let rotate_input_value=targetFeatureElement.rotate??Math.PI/2;
  EditUI.push(<HidableUI available={isRefReady} title="詳細設定">
    <Button onClick={()=>{
      onFeatureElementUpdate({...targetFeatureElement,ref:[]});
    }}>清除特徵</Button>

    <InputNumber prefix="類型0~7:"  value={targetFeatureElement.angle_select??0} min={-1} max={8} step={1} onChange={(value)=>{
      if(value<0)value+=8;
      onFeatureElementUpdate({...targetFeatureElement,angle_select:value%8});
    }} />

    <Switch checkedChildren="-180~180" unCheckedChildren="0~360" checked={targetFeatureElement.is_principal_range??false} onChange={(checked)=>{
      onFeatureElementUpdate({...targetFeatureElement,is_principal_range:checked});
    }} />

  </HidableUI>);

    


  return <>
    <Input value={targetFeatureElement.name} onChange={(e: any) => {
      onFeatureElementUpdate({ ...targetFeatureElement, name: e.target.value });
    }} />
    {EditUI}
  </>
}



function _Draw_FeatureElement_Edit_Measure_Diameter(
  featureInfo: any,
  featureEle: any,
  reportObj: any,
  reportEle: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type
)
{
  if (ctrl_or_draw == true) {
    return;
  }
  let camMag = canvas_obj.camera.GetCameraScale();

  let is_radius=featureEle.is_radius??false;

  g.ctx.fillStyle = colorSetting.main.toString();

  g.ctx.setLineDash([]);
  let isRefReady=false;


  let refObj1_id=featureEle?.ref?.find((ref:any)=>ref.type=="obj1")?.id;

  let refObj1_idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==refObj1_id);


  if(refObj1_idx!==-1)
  {
    isRefReady=true;
  }


  if(isRefReady)
  {

    let obj1Def=featureInfo.element_list[refObj1_idx];

    let def_arc=threePointToArc(obj1Def.pt1,obj1Def.pt2,obj1Def.pt3);

    let obj1Rep=reportObj?.element_report?.[refObj1_idx];


    let centre=obj1Rep?.c ?? {x:def_arc.x,y:def_arc.y};
    let radius=obj1Rep?.r ?? def_arc.r;


    let ctrl_pt1_vec={x:featureEle.ctrl_pt1.x-centre.x,y:featureEle.ctrl_pt1.y-centre.y};
    let ctrl_pt_mag=Math.hypot(ctrl_pt1_vec.x,ctrl_pt1_vec.y);
    ctrl_pt1_vec.x/=ctrl_pt_mag;  
    ctrl_pt1_vec.y/=ctrl_pt_mag;

    let angle=Math.atan2(ctrl_pt1_vec.y,ctrl_pt1_vec.x);

    if(ctrl_pt_mag<=radius)
    {//in radius
      g.ctx.fillStyle=colorSetting.indication_line.toString();
      g.ctx.strokeStyle=colorSetting.indication_line.toString();
      g.ctx.setLineDash(colorSetting.indication_line_dash.map(d=>d/camMag));
      g.ctx.lineWidth=colorSetting.indication_line_width/camMag;

    }
    else
    {
      
      g.ctx.fillStyle=colorSetting.indication_line.toString();
      g.ctx.strokeStyle=colorSetting.indication_line.toString();
      g.ctx.setLineDash(colorSetting.indication_line_dash.map(d=>d/camMag));
      g.ctx.lineWidth=colorSetting.indication_line_width/camMag;
    }


    let temp_value=radius;
    if(is_radius)
    {

      drawTwoHeadArrowLine(g,
        {x:ctrl_pt1_vec.x*radius+centre.x,y:ctrl_pt1_vec.y*radius+centre.y},
        {x:centre.x,y:centre.y},
        10/camMag
      );
     

    }
    else
    {
      temp_value=radius*2;
      drawTwoHeadArrowLine(g,
        {x:ctrl_pt1_vec.x*radius+centre.x,y:ctrl_pt1_vec.y*radius+centre.y},
        {x:-ctrl_pt1_vec.x*radius+centre.x,y:-ctrl_pt1_vec.y*radius+centre.y},
        10/camMag
      );
    }



    let value=reportEle?(reportEle?.value):temp_value;
    if(ctrl_pt_mag<=radius)
    {
      //inside

      //draw arrow line from ctrl_pt1 to centre
    }
    else
    {

      g.ctx.fillStyle=colorSetting.indication_extended_line.toString();
      g.ctx.strokeStyle=colorSetting.indication_extended_line.toString();
      g.ctx.setLineDash(colorSetting.indication_extended_line_dash.map(d=>d/camMag));
      g.ctx.lineWidth=colorSetting.indication_extended_line_width/camMag;

      g.ctx.beginPath();
      g.ctx.moveTo(ctrl_pt1_vec.x*radius+centre.x,ctrl_pt1_vec.y*radius+centre.y);
      g.ctx.lineTo(featureEle.ctrl_pt1.x,featureEle.ctrl_pt1.y);
      g.ctx.stroke();
    }
    
    // console.log(">>>",obj1Def,obj1Rep);
    g.ctx.save();

    // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
    g.ctx.translate(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y);
    g.ctx.rotate(angle);
    draw_feature_text(g,canvas_obj,colorSetting,
      [
        featureEle.name,
        (is_radius?"R:":"Ø:")+value?.toFixed(4)

      ]
    );
    g.ctx.restore();


  }
  else
  {
    g.ctx.save();

    // g.ctx.rotate(featureEle.rotate??0-featureInfo.template_angle);
    g.ctx.translate(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y);
    // g.ctx.rotate(0);
    draw_feature_text(g,canvas_obj,colorSetting,
      [
        featureEle.name,
        (is_radius?"R:":"Ø:")+":!UNSET!"

      ]
    );
    g.ctx.restore();
  }
  

  g.ctx.fillStyle = colorSetting["point"].toString();

  g.ctx.save();
  g.ctx.beginPath();
  g.ctx.arc(featureEle.ctrl_pt1.x, featureEle.ctrl_pt1.y, 5/camMag, 0, Math.PI * 2);
  g.ctx.fill();
  g.ctx.restore();
}


function RefSelectUI({featureInfo,refSeleInfo,onRefSelected,onDrawHookUpdate}:{
  featureInfo:any,
  refSeleInfo:{type:string,cands:any[],curRef:any}[],
  onRefSelected:(refKey:string,feature_id:number)=>void,
  onDrawHookUpdate:(drawHook?:(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)=>void)=>void})
{

  let _this = useRef<any>({
    cacheDrawHook: undefined,
  }).current;

  const [ControlPointSelect_DrawHook_param, setControlPointSelect_DrawHook_param] = useState<type_ControlPointSelect_DrawHook_param|undefined>(undefined);
  const ControlPointSelect_DrawHook = useControlPointSelect_DrawHook(ControlPointSelect_DrawHook_param);

  

  _this.cacheDrawHook=(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)=>{
    if(ControlPointSelect_DrawHook_param===undefined)
      return;

    
    ControlPointSelect_DrawHook(ctrl_or_draw, g, canvas_obj);



    drawFeatureSet(featureInfo,
      {
        // ref: {
        //   list: ControlPointSelect_DrawHook_param===undefined?ref_ele:[],
        //   colorSetting: {
        //     ...drawTheme.targetEditingForgroundDisp,
        //     main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5),
        //     point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5),
        //     start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5),
        //     start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
        //   }
        // },
        ref_select:{
          element_list: ControlPointSelect_DrawHook_param?.featureEleList ?? [],
          colorSetting: {
            ...drawTheme.targetEditingForgroundDisp,
            main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5).darken(0.5),
            point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5).darken(0.5),
            start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5).darken(0.5),
            start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
          }
        }
      }, 0, ctrl_or_draw, g, canvas_obj);
  }
  
  return <>
  RefSelectUI  
  {refSeleInfo.map((info,idx)=>{
    return <Button key={info.type} onClick={()=>{
      // onRefSelected(key,feature_id);
      
      setControlPointSelect_DrawHook_param({
        featureEleList: info.cands,
        distanceThreshold: 10,
        colorSetting: drawTheme.targetEditingForgroundDisp,
        onFeatureSelected: (featureEle: any, path: (string | number)[], distance: number) => {
          onRefSelected(info.type,featureEle.id);
          setControlPointSelect_DrawHook_param(undefined);
          onDrawHookUpdate(undefined);
         },
        onFeatureHovered: (featureEle: any, path: (string | number)[], distance: number) => {
        }
      });
      onDrawHookUpdate((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)=>{
        _this.cacheDrawHook(ctrl_or_draw, g, canvas_obj);
      });

      
    }}>{info.curRef===undefined?"選擇參考":info.curRef.name}</Button>
  })}
  
  
  
  </>;
}


function findRefObj(featureInfo:any,targetFeatureElement:any,type:string)
{
  let refObjInfo = targetFeatureElement.ref.find((ref:any)=>ref.type==type) as {type:string,id:number};
  return refObjInfo===undefined?undefined:featureInfo.element_list.find((feature:any)=>feature.id==refObjInfo.id);
}




function _UI_FeatureElement_Edit_Measure_Diameter({ featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate, onExit }: type_UI_FeatureElement_Edit_param) {

  let props = { featureInfo, targetFeatureElement, DepInject, onFeatureElementUpdate };


  let _this = useRef<any>({
    drawHook: undefined,
    canvas_obj:undefined,
  }).current;

  const cpe_drawHook = useControlPointEdit_DrawHook({ featureInfo, targetFeatureElement, onFeatureElementUpdate });




  let refObj1=findRefObj(featureInfo,targetFeatureElement,"obj1");

  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {

        _this.canvas_obj=canvas_obj;
        if(_this.drawHook!==undefined)
        {
          _this.drawHook(ctrl_or_draw, g, canvas_obj);
        }
        else
        {
          cpe_drawHook(ctrl_or_draw, g, canvas_obj,drawTheme.targetEditingForgroundDisp);


          {
            // let bgEle: any[] = featureInfo.element_list.filter((feature: any) => feature.id != targetFeatureElement.id);
            let ref_ele=targetFeatureElement.ref.map((ref:any)=>featureInfo.element_list.find((feature:any)=>feature.id==ref.id));
            // console.log(">>>", candidateFilter);
        
            drawFeatureSet(featureInfo,
              {
                // bg: {
                //   list: bgEle,
                //   colorSetting: drawTheme.targetEditingBackgroundDisp
                // },
                ref: {
                  element_list: ref_ele,
                  colorSetting: {
                    ...drawTheme.targetEditingForgroundDisp,
                    main: drawTheme.targetEditingForgroundDisp.main.alpha(0.5),
                    point: drawTheme.targetEditingForgroundDisp.point.alpha(0.5),
                    start_bar: drawTheme.targetEditingForgroundDisp.start_bar.alpha(0.5),
                    start_bar_width: drawTheme.targetEditingForgroundDisp.start_bar_width * 0.5,
                  }
                }
              }, 0, ctrl_or_draw, g, canvas_obj);
          }
        }
        
      },
    })
  }, [targetFeatureElement])


  let isRefReady=(refObj1!==undefined);



  let EditUI:React.ReactNode[]=[];

  EditUI.push(<Input value={targetFeatureElement.name} onChange={(e: any) => {
    onFeatureElementUpdate({ ...targetFeatureElement, name: e.target.value });
  }} />);

  {

    EditUI.push(<HidableUI available={true} title="參考設定">
      <RefSelectUI 
      featureInfo={featureInfo}
      refSeleInfo={[
        {
          type:"obj1",
          cands:featureInfo.element_list.filter((feature:any)=>feature.type=="ArcFit"),
          curRef:refObj1
        },
      ]}
      onRefSelected={(refType,feature_id)=>{
        let newRefList = targetFeatureElement.ref.filter((ref:any)=>ref.type!=refType);

        onFeatureElementUpdate({...targetFeatureElement,ref:[...newRefList,{type:refType,id:feature_id}]});
        _this.drawHook=undefined;
      }}
      onDrawHookUpdate={(drawHook)=>{
        console.log(">>>",drawHook);
        _this.drawHook=drawHook;
        _this.canvas_obj.draw();
      }} />
      </HidableUI>);
  }

  {
    EditUI.push(<HidableUI available={true} title="詳細設定">
      <>
      <Switch checkedChildren={"R"} unCheckedChildren={"Ø"}  checked={targetFeatureElement.is_radius} onChange={(checked) => {
        onFeatureElementUpdate({ ...targetFeatureElement, is_radius: checked });
      }} />
      </>
    </HidableUI>);
  }

  return <>
  <p>_UI_FeatureElement_Edit_Measure_Diameter</p>
    {EditUI}
  </>;
}





let LineFit_UI = {
  draw: _Draw_FeatureElement_Edit_LineFit,
  UI: _UI_FeatureElement_Edit_LineFit,
  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "LineFit",
      margin: 20,
      pt1: {
        "x": 0,
        "y": 0
      },
      "pt2": {
        "x": 100,
        "y": 100
      }
    };
  }
}



let ArcFit_UI = {
  draw: _Draw_FeatureElement_Edit_ArcFit,
  UI: _UI_FeatureElement_Edit_ArcFit,

  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "ArcFit",
      from_outer_margin: false,
      margin: 20,
      pt1: {
        x: 0,
        y: 0
      },
      pt2: {
        x: 100,
        y: 100
      },
      pt3: {
        x: 100,
        y: 200
      }
    }
  }

}


let SearchPoint_UI = {
  draw: _Draw_FeatureElement_Edit_SearchPoint,
  UI: _UI_FeatureElement_Edit_SearchPoint,
  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "SearchPoint",
      margin: 20,
      width: 100,
      angle: 0,
      pt1: {
        x: 0,
        y: 0
      },
      "ref": [
      ]
    }
  }
}


let Measure_Distance_UI = {
  draw: _Draw_FeatureElement_Edit_Measure_Distance,
  UI: _UI_FeatureElement_Edit_Measure_Distance,
  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "Measure_Distance",
      disp_pt1: {
        x: 0,
        y: 0
      },
      ctrl_pt1: {
        x: 0,
        y: 0
      },
      "ref": [
      ]
    }
  }
}


let Measure_Angle_UI = {
  draw: _Draw_FeatureElement_Edit_Measure_Angle,
  UI: _UI_FeatureElement_Edit_Measure_Angle,
  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "Measure_Angle",
      subtype: "line_to_line",
      disp_pt1: {
        x: 0,
        y: 0
      },
      ctrl_pt1: {
        x: 0,
        y: 0
      },
    }
  }
}


let Measure_Diameter_UI = {
  draw: _Draw_FeatureElement_Edit_Measure_Diameter,
  UI: _UI_FeatureElement_Edit_Measure_Diameter,
  GenNewElement: (id:number|undefined)=>{
    return {
      id: id??genID_rand(),
      name: "XXX",
      type: "Measure_Diameter",
      ctrl_pt1: {
        x: 0,
        y: 0
      },
      "ref": [
      ]
    }
  }
}


let FeatureEle_UI = {
  LineFit: LineFit_UI,
  ArcFit: ArcFit_UI,
  SearchPoint: SearchPoint_UI,
  Measure_Distance: Measure_Distance_UI,
  Measure_Angle: Measure_Angle_UI,
  Measure_Diameter: Measure_Diameter_UI,
}

function MUX_Draw_FeatureElement_Edit(
  featureInfo: any,
  featureEle: any,
  reportObject: any,
  ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent,
  colorSetting: ColorSetting_type = drawTheme.defDisp
) {

  let idx=featureInfo.element_list.findIndex((ele:any)=>ele.id==featureEle.id);
  if (featureEle.type in FeatureEle_UI) {
    let type = featureEle.type as keyof typeof FeatureEle_UI;
    let reportEle = reportObject?.element_report?.[idx];
    FeatureEle_UI[type].draw(featureInfo, featureEle, reportObject, reportEle, ctrl_or_draw, g, canvas_obj, colorSetting);
  }



  
}



function MUX_UI_FeatureElement_Edit(props: type_UI_FeatureElement_Edit_param) {
  let { featureInfo, targetFeatureElement, DepInject } = props;
  // return <>{featureEle.id+" "+featureEle.type+" "+featureEle.name}</>
  if (targetFeatureElement === undefined)
    return null;

  if (targetFeatureElement.type in FeatureEle_UI) {
    let type = targetFeatureElement.type as keyof typeof FeatureEle_UI;
    let UISet = FeatureEle_UI[type];
    return <UISet.UI {...props} />
  }
  return null;
}





// type NewFeatureType="LineFit"|"ArcFit"|"SearchPoint"|"Measure_Distance"|"Measure_Angle";
function NewFeatureAddUI({ onOK }: { onOK: (newFeature: any) => void }) {
  const [newFeatureName, setNewFeatureName] = useState("");
  const [newFeatureType, setNewFeatureType] = useState<string | undefined>(undefined);
  return <>
    <Input placeholder="新的特徵名稱" value={newFeatureName} onChange={(e) => { setNewFeatureName(e.target.value) }} />
    <Select style={{ width: 120 }} value={newFeatureType} onChange={(value) => { setNewFeatureType(value as string) }}>
      {Object.keys(FeatureEle_UI).map((type) => {
        return <Select.Option value={type}>{type}</Select.Option>
      })}
    </Select>
    <Button onClick={() => {
      if (newFeatureName.length == 0 || newFeatureType === undefined)
        return;

      let id = genID_rand();

      if (newFeatureType in FeatureEle_UI) {
        let type = newFeatureType as keyof typeof FeatureEle_UI;
        let UISet = FeatureEle_UI[type];
        onOK({ ...UISet.GenNewElement(id), id, name: newFeatureName });
      }

    }}>OK</Button>
  </>
}




function checkRefLoop(CATList: type_CAT_ELE[], curEle: type_CAT_ELE, MasterEle: type_CAT_ELE): boolean {
  // Use Set for O(1) lookups
  const visited = new Set<number>();
  
  // Start from current element
  let current = curEle as type_CAT_ELE|undefined;
  
  while (true) {
    if(current===undefined)break;
    // If we've seen this ID before or reached master, we found a loop
    if (visited.has(current.id) || current.id === MasterEle.id) {
      return true;
    }
    
    // Mark current ID as visited
    visited.add(current.id);
    
    // Move to referenced element if it exists
    if (current.ref === undefined) {
      break;
    }
    
    // Find next element in chain
    current = CATList.find(ele =>current && ele.id === current.ref);
    if (!current) {
      break;
    }
  }
  
  return false;
}

function Cat_ref_array(CATList:type_CAT_ELE[],CatEle:type_CAT_ELE):type_CAT_ELE[]
{
  if(CatEle.ref===undefined)return [CatEle];

  let ref_cat=CATList.find((ele:any)=>ele.id==CatEle.ref);
  if(ref_cat===undefined)return [CatEle];
  console.log(">>Cat_ref_array>",CatEle.id,ref_cat);
  let ref_array=Cat_ref_array(CATList,ref_cat);

  ref_array.push(CatEle);
  return ref_array;
}

// function refed_limit_setup_reducer(refArray:type_CAT_ELE[])
// {
//   return refArray.reduce((acc:type_CAT_ELE['limits_setup'],ele:type_CAT_ELE)=>{
//     Object.keys(ele.limits_setup).forEach((key)=>{
//       acc[key]=acc[key]??0;
//       acc[key]+=ele.limits_setup[key];
//     });
//     return acc;
//   },{});
// }


function _UI_Category_Edit(props: type_Catgory_Element_Edit_param) {

  let { featureInfo, CatEle, DepInject, onCatEleUpdate, onExit } = props;
  let measure_list=featureInfo.element_list.filter((ele:any)=>ele.type.startsWith("Measure_"));
  let other_list=featureInfo.element_list.filter((ele:any)=>!ele.type.startsWith("Measure_"));

  const [showRecManipulation,setShowRecManipulation]=useState(false);
  let _this=useRef<any>({}).current;

  const refArray=useMemo(()=>{

    let refArray=Cat_ref_array(featureInfo.category_list,CatEle);

    // let compile_refed_limit_setup=refed_limit_setup_reducer(featureInfo);
    // console.log(">>compile_refed_limit_setup>",compile_refed_limit_setup);
    return refArray;
  },[CatEle.ref]);



  useEffect(() => {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.canvas_obj=canvas_obj;
        if(_this.drawHook!==undefined)
        {
          _this.drawHook(ctrl_or_draw, g, canvas_obj);
          return;
        }
        if(ctrl_or_draw==true)return;
        
        drawFeatureSet(featureInfo,
          {
            // bg: {
            //   list: bgEle,
            //   colorSetting: drawTheme.targetEditingBackgroundDisp
            // },
            measure: {
              element_list:measure_list,
              colorSetting:{...drawTheme.targetEditingForgroundDisp,
                indication_extended_line:drawTheme.targetEditingForgroundDisp.indication_extended_line.alpha(0.5),
              
                indication_line:drawTheme.targetEditingForgroundDisp.indication_line.alpha(0.3),
              },
            },
            other: {
              element_list: other_list,
              colorSetting:drawTheme.targetEditingBackgroundDisp
            }
          }, 0, ctrl_or_draw, g, canvas_obj);
      },
    })

    // return  () => { DepInject(undefined) };
  }, [])

  console.log(">>CatEle>",CatEle);

  type DataType = {
    key: React.Key;
    src: any;
    id:number;
    name:string;
    not_in_record:boolean;
    not_in_measure:boolean;
    valueMultFactor:number;
  } & LimitSetup;

  const exist_in_record: DataType[]=CatEle.limits_setup?.map((ele:any)=>{
    let measure_index=measure_list.findIndex((measure_ele:any)=>measure_ele.id==ele.id);
    return {
      key:ele.id,
      id:ele.id,
      src:measure_list?.[measure_index],
      name:measure_list?.[measure_index]?.name??"==DELETED==",
      not_in_record:false,
      not_in_measure:measure_index===-1,
      valueMultFactor:1,
      ...ele,
    } as DataType
  })??[]

  const rest_in_measure: DataType[]=measure_list.filter((measure_ele:any)=>{
    return exist_in_record.findIndex((ele:any)=>ele.id==measure_ele.id)===-1;//not in exist_in_record
  }).map((measure_ele:any)=>{
    return {
      key:measure_ele.id,
      id:measure_ele.id,
      src:measure_ele,
      name:measure_ele.name,
      not_in_record:true,
      not_in_measure:false,

      valueMultFactor:1,
      low_limit:NaN,
      high_limit:NaN,
      target:NaN,
      NG_as:"NG",
    } as DataType
  })


  const data: DataType[] =[...exist_in_record,...rest_in_measure];

  data.forEach((ele:any)=>{
    ele.valueMultFactor=ele.src?.type?.startsWith("Measure_Angle")?180/Math.PI:1;
  });

  console.log(">refArray>",refArray);

  
  function moveFeatureElement(src:any,id:number,direction:"top"|"up"|"down"|"bottom")
  {
    let idx=CatEle?.limits_setup?.findIndex((ele:any)=>ele.id==id);
    if(idx===undefined || idx<0)return;//not in record
    let new_limits_setup=[...CatEle.limits_setup??[]];

    let record=new_limits_setup[idx];
    new_limits_setup.splice(idx,1);

    let new_idx=0;
    if(direction==="top")
    {
      new_limits_setup.unshift(record);
    }
    else if(direction==="up")
    {
      new_limits_setup.splice(idx-1,0,record);
    }
    else if(direction==="down")
    {
      new_limits_setup.splice(idx+1,0,record);
    }
    else if(direction==="bottom")
    {
      new_limits_setup.push(record);
    }

    onCatEleUpdate({...CatEle,limits_setup:new_limits_setup});
  }

  function deleteFeatureElement(src:any,id:number)
  {
    let newCatEle={...CatEle,limits_setup:[...CatEle.limits_setup??[]].filter((ele:any)=>ele.id!==id)};
    onCatEleUpdate(newCatEle);
  }

  function updateFeatureElement(src:any,id:number,low_limit:number,target:number,high_limit:number,NG_as:"OK"|"NG"|"NA")
  {

    // let newCatEle={...CatEle,limits_setup:[...CatEle.limits_setup??[]]};
    let findElement=CatEle?.limits_setup?.findIndex((ele:any)=>ele.id==id) as number;
    
    let newCatEle={...CatEle,limits_setup:[...CatEle.limits_setup??[]]};
    if(findElement===-1)//append new element
    {
      findElement=newCatEle.limits_setup.length;
      newCatEle.limits_setup.push({id,low_limit,target,high_limit,NG_as});
    }
    else//update existing element
    {
      newCatEle.limits_setup[findElement]={...newCatEle.limits_setup[findElement],low_limit,target,high_limit,NG_as};
    }
    
    onCatEleUpdate(newCatEle);
  }

  


  let measureTypeNameDict:any={
    "Measure_Distance":"距離",
    "Measure_Angle":"角度",
    "Measure_Diameter":"圓徑",
  }
  const columns: TableProps<DataType>['columns'] = [
    {
      title: '名',
      dataIndex: 'name',
      key: 'name',
      render: (_, data) =>(<p style={{color:data.not_in_measure?"red":data.not_in_record?"rgba(200,200,200,1)":"black"}}>{data.name}</p>)
    },

    {
      title: '類型',
      dataIndex: 'type',
      key: 'type',
      render: (_, data) =>(<>{measureTypeNameDict?.[data.src?.type as string]??data?.src?.type}</>)
    },


    {
      title: '下界',
      dataIndex: 'low_limit',
      key: 'low_limit',
      render: (_, data) =>(<>
        <InputNumber value={Number((data.low_limit*data.valueMultFactor).toFixed(5))} onChange={(value)=>{
          updateFeatureElement(data.src,data.id,value/data.valueMultFactor,data.target,data.high_limit,data.NG_as);
        }} />
        </>
      )
    },


    {
      title: '目標',
      dataIndex: 'target',
      key: 'target',
      render: (_, data) =>(<>
        <InputNumber value={Number((data.target*data.valueMultFactor).toFixed(5))} onChange={(value)=>{
          updateFeatureElement(data.src,data.id,data.low_limit,value/data.valueMultFactor,data.high_limit,data.NG_as);
        }} />
      </>
    ),
    },
    {
      title: '上界',
      dataIndex: 'high_limit',
      key: 'high_limit',
      render: (_, data) => (<>
          <InputNumber value={Number((data.high_limit*data.valueMultFactor).toFixed(5))} onChange={(value)=>{
            updateFeatureElement(data.src,data.id,data.low_limit,data.target,value/data.valueMultFactor,data.NG_as);
            }} />
        </>
      ),
    },
    {
      title: '出界為',
      dataIndex: 'NG_as',
      key: 'NG_as',
      render: (_, data) => (<>
        <Select value={data.NG_as} style={{color:data.NG_as==="OK"?"green":data.NG_as==="NG"?"red":"gray" }} onChange={(value)=>{
          updateFeatureElement(data.src,data.id,data.low_limit,data.target,data.high_limit,value);
        }}>
          <Select.Option value="OK" style={{ color: 'green' }}>OK</Select.Option>
          <Select.Option value="NG" style={{ color: 'red' }}>NG</Select.Option>
          <Select.Option value="NA" style={{ color: 'gray' }}>NA</Select.Option>
        </Select>
      </>)
    },
    ...showRecManipulation?[{
      title: '更動',
      dataIndex: 'move',
      key: 'move',
      render: (_:any, data:DataType) => (<>

        <Button onClick={() => {
          moveFeatureElement(data.src, data.id, "top");
        }}>
          <VerticalAlignTopOutlined />
        </Button>
        <Button onClick={() => {
          moveFeatureElement(data.src, data.id, "up");
        }}>
          <UpOutlined />
        </Button>
        <Button onClick={() => {
          moveFeatureElement(data.src, data.id, "down");
        }}>
          <DownOutlined />
        </Button>
        <Button onClick={() => {
          moveFeatureElement(data.src, data.id, "bottom");
        }}>
          <VerticalAlignBottomOutlined />
        </Button>

        <CountDownCheckPopup countdown={3} onConfirm={()=>{
          deleteFeatureElement(data.src, data.id);
        }}>
          <Button danger type="primary">
            <DeleteOutlined />
          </Button>
        </CountDownCheckPopup>
      </>)
    }]:[]
  ];


  return <>

    <Input prefix="名稱:" style={{width:120}} placeholder="新的特徵名稱" value={CatEle.name} onChange={(e) => { onCatEleUpdate({...CatEle,name:e.target.value}) }} />
    {/* Ref:
    <Select style={{width:120}} value={CatEle.ref} onChange={(value)=>{
      if(value===-1)value=undefined;
      onCatEleUpdate({...CatEle,ref:value});
    }}>
      {featureInfo.category_list.filter((ele:any)=>checkRefLoop(featureInfo.category_list,ele,CatEle,5)===false).map((ele:any)=>{
        return <Select.Option value={ele.id}>{ele.name}</Select.Option>
      })}
      <Select.Option value={-1}>NO REF</Select.Option>
    </Select>

    {
      refArray.map((ele:any)=>{
        return <>{">"+ele.name}</>
      })
    } */}
{/* 
    <RefSelectUI 
      featureInfo={featureInfo}
      refSeleInfo={[
        {
          type:"obj1",
          cands:featureInfo.element_list.filter((ele:any)=>ele.type.startsWith("Measure_")),
          curRef:undefined
        },
      ]}
      onRefSelected={(refType,feature_id)=>{
        _this.drawHook=undefined;
      }}
      onDrawHookUpdate={(drawHook)=>{
        console.log(">>>",drawHook);
        _this.drawHook=drawHook;
        _this.canvas_obj.draw();
      }} />
     */}
    
    
    <Switch checked={showRecManipulation} checkedChildren="顯示順序操作" unCheckedChildren="隱藏順序操作" onChange={(checked)=>{
      setShowRecManipulation(checked);
    }} />
    <Table<DataType> dataSource={data} columns={columns}  size="small" pagination={false}/>
      
    {/* {JSON.stringify(measure_list,null,2)} */}
    </>
}


let Category_UI = {
  UI: _UI_Category_Edit,
  GenNewElement: (id:number|undefined)=>{
    if(id===undefined)
      id=genID_rand();
    let newEle={
      id: NaN,
      name: "XXX"
    }
    return {
      ...newEle,
      id,
    }
  }
}


type type_State_Info = {
  state: number,
  data: any
}
function useStateMachine(initState: number, stateAction: (stateTriple: type_State_Info[]) => void) {



  const [editState, _setEditState] = useState<type_State_Info[]>([{ "state": initState, data: undefined }]);

  let currentState = editState[editState.length - 1];


  function popEditState() {
    if (editState.length == 1)
      return;
    let state3Ev: type_State_Info[] = [];//3 elements, leave,stay,enter
    let preState = editState[editState.length - 2];
    state3Ev = [currentState, { state: NaN, data: undefined }, preState]
    stateAction(state3Ev);

    let newStateList = [...editState];
    newStateList.pop();
    _setEditState(newStateList);
  }

  function pushEditState(newEditState: type_State_Info) {

    // _this.sel_region = 
    // _this.sel_region_type = undefined;
    // if (_this.canvasComp == undefined) return;
    //     _this.canvasComp.UserRegionSelect(undefined)

    let state3Ev: type_State_Info[] = [];//3 elements, leave,stay,enter
    if (newEditState?.state != currentState.state) {
      state3Ev = [currentState, { state: NaN, data: undefined }, newEditState]
    }
    else {
      state3Ev = [{ state: NaN, data: undefined }, newEditState, { state: NaN, data: undefined }]
    }
    stateAction(state3Ev);
    if (newEditState?.state != currentState.state)//new state
      _setEditState([...editState, newEditState]);
    else //update/replace latest same state with different data
    {
      let newStateList = [...editState];
      newStateList[newStateList.length - 1] = newEditState;
      _setEditState(newStateList);
    }
  }


  return {
    states: editState,
    pushState: pushEditState,
    popState: popEditState,
  }
}
function DPadControl({onMove, onRotate,onScale}: {
  onMove: (dx: number, dy: number) => void,
  onRotate: (theta: number) => void,
  onScale: (scale: number) => void
}) {
  const [stepSize, setStepSize] = useState(1); // Default step size
  const theta = 5 * Math.PI / 180; // Rotation delta (5 degrees)

  return (
    <div>
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 40px)',
        gap: '4px',
        margin: '8px'
      }}>
        {/* Rotation controls */}
        <Button onClick={() => onRotate(-theta)}>↺</Button>
        <Button onClick={() => onMove(0, -stepSize)}>↑</Button>
        <Button onClick={() => onRotate(theta)}>↻</Button>

        {/* Movement controls */}
        <Button onClick={() => onMove(-stepSize, 0)}>←</Button>
        <Button>•</Button>
        <Button onClick={() => onMove(stepSize, 0)}>→</Button>

        {/* Empty space and down button */}
        <Button onClick={() => onScale(1-0.01*stepSize)}>-</Button>
        <Button onClick={() => onMove(0, stepSize)}>↓</Button>
        <Button onClick={() => onScale(1+0.01*stepSize)}>+</Button>
        <div />
      </div>

      {/* Step size selector */}
      <div style={{ 
        display: 'flex',
        gap: '4px',
        justifyContent: 'center',
        marginTop: '4px'
      }}>
        {[1, 3, 10].map((size) => (
          <Button 
            key={size}
            type={stepSize === size ? 'primary' : 'default'}
            onClick={() => setStepSize(size)}
          >
            {size}
          </Button>
        ))}
      </div>
    </div>
  );
}


function DEF_Edit_UI(props: CompParam_InspTarUI & { DepInject: any, onExit: () => void }) {
  let { display, fsPath, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, defDoReload, UIOption, onUIOptionUpdate, showUIOptionConfigUI = false, APIExport, DepInject, onExit } = props;


  const _this = useRef<{
    drawHook: any,
    tempImg:any,
    canvas_obj: DrawHook_CanvasComponent | undefined,
    _mouseHoverInfo: any
  }>({
    drawHook: undefined,
    canvas_obj: undefined,
    _mouseHoverInfo: undefined,
    tempImg:undefined
  }).current;

  const [Ref_Src_Info,set_Ref_Src_Info]=
  useState<{use_cache:boolean,file_name?:string,folder_path?:string}>
    ({use_cache:false,file_name:"template",folder_path:""});
  const [templateInfo, setTemplateInfo] = useState<any>(undefined);
  const [cacheDef, setCacheDef] = useState<any>(def);
  const dispatch = useDispatch();
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);


  console.log(">>>wdwdsd", def);

  useEffect(() => {
    console.log(">>>", def);
    setCacheDef(def);

    _this.canvas_obj?.draw();
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
    });

  }, [def]);




  enum EditState {
    Def_Edit = 1,
    Add_New_Element = 2,
    FeatureElement_Edit = 100,
    CategoryElement_Edit = 300,
    NA = -99999
  }

  const { states, pushState, popState } = useStateMachine(EditState.Def_Edit, (stateTriple: type_State_Info[]) => {
    stateTriple.forEach((st, idx) => {

      switch (st.state)//current state
      {
        case EditState.Def_Edit:
          if (idx == 2)//enter
          {

          }
          else if (idx == 0)//leave
          {

          }
          break;

        case EditState.FeatureElement_Edit:
          if (idx == 2)//enter
          {
            setTimeout(() => {//HACK: wait for drawHook
              _this.canvas_obj?.draw();
            }, 100);
          }
          else if (idx == 0)//leave
          {
            setTimeout(() => {
              _this.canvas_obj?.draw();
            }, 100);
          }
          break;
      }
    })
  })


  function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

    if (updatedDef === undefined) {
      // onDefChange(undefined,false);
      return;
    }
    console.log(updatedDef);
    setCacheDef(updatedDef);

    

    // (async () => {
    //     await BPG_API.InspTargetUpdate(updatedDef)
    // })()
    onDefChange(updatedDef, doTakeNewImage);
  }


  console.log(">>>", def);

  let drawHook = (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
    // DEF_Edit(def,g);
    // console.log(">>>",ctrl_or_draw);

    _this.canvas_obj = canvas_obj;
    let ctx = g.ctx;
    let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);

    let camMag = canvas_obj.camera.GetCameraScale();
    let featureEleList = cacheDef.featureInfo?.element_list || [];

    let onMousePress = g.mouseStatus.status == 1 && g.mouseStatus.pstatus == 0;
    if (ctrl_or_draw == true) {
      if (currentState.state == EditState.Def_Edit) {

        let minDistance = Infinity;
        let minFeatureEle: any = undefined;
        let pointPath: (string | number)[] | undefined;
        let tarEleIndex: number = -1;

        let idx = 0;
        for (let featureEle of featureEleList) {

          let { distance, controlPointPath } = FeatureControlPointMinDistance(featureEle, mouseOnCanvas);

          if (distance < minDistance) {
            minDistance = distance;
            minFeatureEle = featureEle;
            pointPath = controlPointPath;
            tarEleIndex = idx;
          }
          idx++;
        }
        if (minDistance < 10/camMag  && minFeatureEle !== undefined) {
          _this._mouseHoverInfo = {
            distance: minDistance,
            ele: minFeatureEle,
            path: pointPath,
            index: tarEleIndex,
          }
          if (onMousePress) {
            pushState({ state: EditState.FeatureElement_Edit, data: { id: minFeatureEle.id, index: tarEleIndex } });


          }
          // console.log(`>>>${minDistance}>>`,minFeatureEle,pointPath,onMousePress);
        }
        else {
          _this._mouseHoverInfo = undefined;
        }
      }
    }
    else {

      //draw imgCanvas



      g.ctx.save();
      // let scale = _this.featureInfoExt.IM.image_info.scale;
      // g.ctx.scale(scale, scale);
      
      console.log(">>>",templateInfo);
      let template_mmpp=templateInfo?.mmpp??1;
      g.ctx.scale(template_mmpp,template_mmpp);
      g.ctx.translate(-0.5, -0.5);
      if (templateInfo !== undefined && templateInfo.report !== undefined) {
        if (templateInfo.report.length > 0) {
          // console.log(">>>",templateInfo.report);
          let first_loc = templateInfo.report[0];
          g.ctx.rotate(-first_loc.angle);
          g.ctx.translate(-first_loc.center.x, -first_loc.center.y);

        }
      }
      if(_this.tempImg!==undefined)
      {
        g.ctx.drawImage(_this.tempImg, 0, 0);
      }
      g.ctx.restore();
      {
        //draw rect at 0,0
        g.ctx.strokeStyle = "red";
        let size = 10 / camMag;
        g.ctx.lineWidth = 2 / camMag;
        g.ctx.strokeRect(-size / 2, -size / 2, size, size);
      }



      //draw feature

      if (currentState.state == EditState.Def_Edit) {

        if (_this._mouseHoverInfo !== undefined)//draw hover element
        {
          drawFeature(cacheDef.featureInfo, _this._mouseHoverInfo.ele, drawTheme.targetEditingForgroundDisp, 99, 99, ctrl_or_draw, g, canvas_obj);
        }
        else {
          for (let featureEle of featureEleList) {
            if (_this._mouseHoverInfo !== undefined) {
              if (_this._mouseHoverInfo.ele.id == featureEle.id) {
                continue;//skip draw
              }
            }
            MUX_Draw_FeatureElement_Edit(cacheDef.featureInfo, featureEle,undefined, ctrl_or_draw, g, canvas_obj, 
              {
                ...drawTheme.defDisp,
                draw_in_simple_form:true
              });
          }


        }

      }
      else {

      }


    }


    {
      // console.log(">>>",currentState.data);
      if (currentState.data?.injectData?.drawHook !== undefined) {
        currentState.data.injectData.drawHook(ctrl_or_draw, g, canvas_obj);
      }
    }

  }

  function Refresh_Render()
  {
    DepInject({
      drawHook: (ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
        _this.drawHook(ctrl_or_draw, g, canvas_obj);
      }
    })
  }
  async function Load_EDIT_ref_Src(Ref_Src_Info:{use_cache:boolean,file_name?:string,folder_path?:string})
  {
    let pkts = await BPG_API.InspTargetExchange(cacheDef.id, { type: "load_orientation_info",imageQuality:80,
      use_cached_input:Ref_Src_Info.use_cache,
      file_name:Ref_Src_Info.file_name,
      folder_path:Ref_Src_Info.folder_path,
    }) as any[];

    let IM = pkts.find((p: any) => p.type == "IM");
    if (IM === undefined) return;
    let IP = pkts.find((p: any) => p.type == "IP");
    if (IP === undefined) return;
    console.log("++++++++\n", IM, IP);


    {
      _this.tempImg=IM.image_info.image;
    }

    console.log(">>>",IP.data,cacheDef);
    setTemplateInfo(IP.data);
    // let firstLoc = IP.data?.report?.[0];
    // if(firstLoc!==undefined)
    // {

    //   setCacheDef(ObjShellingAssign(cacheDef,["featureInfo","template_angle"],firstLoc.angle));
    // }

    console.log(">>>", def);

    Refresh_Render();

  }


  _this.drawHook = drawHook;
  useEffect(() => {

    Load_EDIT_ref_Src(Ref_Src_Info);


  }, [])









  let EDIT_UI = null;
  console.log("editState", states);
  let currentState = states[states.length - 1];


  function Element_Move(ele:any,move_func:(pt:VEC2D)=>VEC2D)
  {
    let new_ele={...ele};
    for (let key in ele) {
      if(key.startsWith("pt")==false && key.startsWith("ctrl_pt")==false && key.startsWith("disp_pt")==false)
        continue;
      let point = ele[key];
      new_ele[key]=move_func(point);
    }
    return new_ele;
  }
  function ElementList_Move(eleList:any[],move_func:(pt:VEC2D)=>VEC2D)
  {
    let new_featureEleList=[];
    for(let ele of eleList)
    {
      let new_ele=Element_Move(ele,move_func);
      new_featureEleList.push(new_ele);
    }
    return new_featureEleList;
  }


  function Ref_Src_Select_UI()
  {
  }


  switch (currentState.state) {

    case EditState.Def_Edit:


      let delta=5;
      let theta=5*Math.PI/180;
      let featureEleList = cacheDef.featureInfo?.element_list || [];


      let measure_ele_list=featureEleList.filter((ele:any)=>ele.type.startsWith("Measure_"));
      let primitive_ele_list=featureEleList.filter((ele:any)=>!ele.type.startsWith("Measure_"));

      EDIT_UI = <>
        <Button danger type="primary" onClick={() => {
          onDefChange(cacheDef, false);
          onExit();
        }}>{"<"}


        </Button>


        <Popover content={<DPadControl 
        
        onMove={(dx,dy)=>{
          let new_featureEleList=ElementList_Move(cacheDef.featureInfo.element_list,(pt:VEC2D)=>({x:pt.x+dx,y:pt.y+dy}));
          onCacheDefChange(ObjShellingAssign(cacheDef,["featureInfo","element_list"],new_featureEleList));
        }} 
        
        onRotate={(theta)=>{
          let new_featureEleList=ElementList_Move(cacheDef.featureInfo.element_list,(pt:VEC2D)=>(PtRotate2d(pt,theta)));
          onCacheDefChange(ObjShellingAssign(cacheDef,["featureInfo","element_list"],new_featureEleList));
        }} 
        
        onScale={(scale)=>{
          let new_featureEleList=ElementList_Move(cacheDef.featureInfo.element_list,(pt:VEC2D)=>({x:pt.x*scale,y:pt.y*scale}));
          onCacheDefChange(ObjShellingAssign(cacheDef,["featureInfo","element_list"],new_featureEleList));
        }} 
        
        />}>
          <Button>{"移動"}</Button>
        </Popover>

        <Button type="primary" onClick={() => {
          pushState({ state: EditState.Add_New_Element, data: undefined });
        }}>增加新要素與量測</Button>

        <HidableUI title={"要素群組[" + (primitive_ele_list.length) + "]"} defaultHide={true} available={true}>
        {
          primitive_ele_list.map((ele: any, idx: number) => {
            return <Button key={ele.id} onClick={() => {
              pushState({ state: EditState.FeatureElement_Edit, data: { id: ele.id, index: idx } });
            }}>{ele.name}</Button>
          })
        }
        <br/>
        </HidableUI>
        <HidableUI title={"量測群組[" + (measure_ele_list.length) + "]"} defaultHide={true} available={true}>
        
        {
          measure_ele_list.map((ele: any, idx: number) => {
            return <Button key={ele.id} onClick={() => {
              pushState({ state: EditState.FeatureElement_Edit, data: { id: ele.id, index: idx } });
            }}>{ele.name}</Button>
          })
        }
        <br/>
        </HidableUI>

        <Button key={"量測標準設定"} onClick={()=>{
            pushState({ state: EditState.CategoryElement_Edit, data: { id: cacheDef.featureInfo?.category_list } });
          }}>量測標準設定</Button>


        <br/>

        {/* <Button key={"使用註冊圖像測試"} onClick={()=>{
          let new_Ref_Src_Info={use_cache:false,file_name:"template",folder_path:""};
          set_Ref_Src_Info(new_Ref_Src_Info);
          Load_EDIT_ref_Src(new_Ref_Src_Info);
          }}>使用註冊圖像</Button>

        <Button key={"使用最新圖像測試"} onClick={()=>{
          let new_Ref_Src_Info={use_cache:true,file_name:undefined,folder_path:undefined};
          set_Ref_Src_Info(new_Ref_Src_Info);
          Load_EDIT_ref_Src(new_Ref_Src_Info);
          }}>使用最新圖像</Button> */}
      </>

      break;

    case EditState.Add_New_Element:
      EDIT_UI = <>
        <Button danger type="primary" onClick={() => {
          popState();
        }}>{"<"}</Button>

        <NewFeatureAddUI onOK={(newFeature: any) => {
          console.log(">>>", newFeature);
          let newFeatureList = [...cacheDef?.featureInfo?.element_list??[]];
          newFeatureList.push(newFeature);
          onCacheDefChange(ObjShellingAssign(cacheDef, ["featureInfo", "element_list"], newFeatureList));
          popState();

          pushState({ state: EditState.FeatureElement_Edit, data: { id: newFeature.id } });

        }} />
      </>
      break;


    case EditState.FeatureElement_Edit:
      console.log(">>>",cacheDef.featureInfo.element_list);
      let index = cacheDef.featureInfo.element_list.findIndex((f: any) => f.id == currentState.data.id);
      let targetFeatureElement = cacheDef.featureInfo.element_list[index];
      console.log(">>>", targetFeatureElement);
      EDIT_UI = <>
        <Button type="primary" onClick={() => {
          popState();
        }}>{"<"}</Button>


        <Button key={"使用註冊圖像測試"} onClick={()=>{
          let new_Ref_Src_Info={use_cache:false,file_name:"template",folder_path:""};
          set_Ref_Src_Info(new_Ref_Src_Info);
          Load_EDIT_ref_Src(new_Ref_Src_Info);
          }}>使用註冊圖像</Button>

        <Button key={"使用最新圖像測試"} onClick={()=>{
          let new_Ref_Src_Info={use_cache:true,file_name:undefined,folder_path:undefined};
          set_Ref_Src_Info(new_Ref_Src_Info);
          Load_EDIT_ref_Src(new_Ref_Src_Info);
          }}>使用最新圖像</Button>
        <br/>

        <MUX_UI_FeatureElement_Edit
          onDelete={()=>{
            let new_featureEleList=cacheDef.featureInfo.element_list.filter((ele:any)=>ele.id!=targetFeatureElement.id);
            onCacheDefChange(ObjShellingAssign(cacheDef,["featureInfo","element_list"],new_featureEleList));
            popState();
          }}
          Ref_Src_Info={Ref_Src_Info}
          it_id={cacheDef.id}
          key={targetFeatureElement.id}
          featureInfo={cacheDef.featureInfo}
          targetFeatureElement={targetFeatureElement}
          DepInject={(injectData: any) => {
            console.log(">>>", injectData);
            pushState({ state: EditState.FeatureElement_Edit, data: { ...currentState.data, injectData } });
          }}
          draw_mmpp={templateInfo?.mmpp??1}
          onFeatureElementUpdate={(featureEle: LineFitElement | ArcFitElement | SearchPointElement | MeasureDistanceElement | MeasureAngleElement | MeasureDiameterElement) => {
            console.log(">>>", featureEle);
            // cacheDef.featureInfo.element_list[index] = featureEle;

            let newDef = ObjShellingAssign(cacheDef, ["featureInfo", "element_list", index], featureEle);

            onCacheDefChange(newDef);
            // _this.canvas_obj?.draw();
            setTimeout(() => {//HACK: wait for drawHook
              _this.canvas_obj?.draw();
            }, 100);
          }}
          onExit={() => {
            popState();
          }}
        />
      </>
      break;
    

    
    case EditState.CategoryElement_Edit:
      {

        let index = cacheDef.featureInfo?.category_list?.findIndex((f: any) => f.id == currentState.data.id) ?? -1;
        let targetCategoryElement =index>=0? cacheDef.featureInfo?.category_list?.[index]:undefined;
        console.log(">>>", targetCategoryElement);
        EDIT_UI = <>
          <Button type="primary" onClick={() => {
            popState();
          }}>{"<"}</Button>
          {" "}
          {(cacheDef.featureInfo.category_list??[]).map((ele:any,idx:number)=>{
            return <Button key={ele.id} type={idx==index?"primary":undefined} onClick={()=>{
              pushState({ state: EditState.CategoryElement_Edit, data: { id: ele.id } });
            }}>{ele.name}</Button>
          })}
          <Button type="primary" style={{marginLeft:"10px"}} onClick={() => {
            let newCatList = [...cacheDef.featureInfo.category_list??[]];
            let newCatEle=Category_UI.GenNewElement(undefined);
            newCatList.push(newCatEle);
            onCacheDefChange(ObjShellingAssign(cacheDef, ["featureInfo", "category_list"], newCatList));
            popState();

            pushState({ state: EditState.CategoryElement_Edit, data: { id: newCatEle.id } });



          }}>新增量測標準</Button>
          <br/>
          {
            targetCategoryElement!==undefined&&
            <Category_UI.UI
              it_id={cacheDef.id}
              key={targetCategoryElement.id}
              featureInfo={cacheDef.featureInfo}
              CatEle={targetCategoryElement}
              DepInject={(injectData: any) => {
                console.log(">>>", injectData);
                pushState({ state: EditState.CategoryElement_Edit, data: { ...currentState.data, injectData } });
              }}
              onCatEleUpdate={(newCatEle: any) => {
                console.log(">>>", newCatEle);
    
    
                // cacheDef.featureInfo.element_list[index] = featureEle;
    
                let newDef = ObjShellingAssign(cacheDef, ["featureInfo", "category_list", index], newCatEle);
    
                onCacheDefChange(newDef);
    
              }}
              
              onExit={() => {
                popState();
              }}
            />
          }
        </>
      }
      break;
  }








  return <>




    {EDIT_UI}




  </>
}






export function SingleTargetVIEWUI_DimMeasure(props: CompParam_InspTarUI) {
  let { display, fsPath, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, defDoReload, UIOption, onUIOptionUpdate, showUIOptionConfigUI = false, APIExport } = props;
  const _this = useRef<{
    imgCanvas: HTMLCanvasElement,
    canvasComp: DrawHook_CanvasComponent | undefined,
    cameraBackup: any,
    canvasHook: any,
    drawHooks: any[],
    ctrlHooks: any[],
    extDrawHook: any,
    API_PARAM:any,
    EXT_API_REFRESH_LISTENER:{[key:string]:any},
    tmp_select_object:number,
    fetchedPixInfo:any,
  }>({

    imgCanvas: document.createElement('canvas'),
    canvasComp: undefined,
    canvasHook: undefined,
    cameraBackup: undefined,
    drawHooks: [],
    ctrlHooks: [],
    extDrawHook: undefined,
    API_PARAM:{},
    EXT_API_REFRESH_LISTENER:{},
    tmp_select_object:-1,
  }).current;



  const dispatch = useDispatch();
  const [cacheDef, setCacheDef] = useState<any>(def);

  const [defReport, setDefReport] = useState<any>(report);
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [Local_IMCM, setLocal_IMCM] =
    useState<IMCM_type | undefined>(undefined);
  const [cur_select_object,setCur_select_object]=useState<number>(-1);
  const [category_id,setCategory_id]=useState<number|undefined>(undefined);

  console.log("Local_IMCM", Local_IMCM, "defReport", defReport);

  enum EditState {
    Normal_Show = 0,
    Def_Edit = 1,
    Add_New_Element = 2,
    FeatureElement_Edit = 100,
    Test_Saved_Files = 3,


    MISC_Settings = 999,
    NA = -99999
  }

  const { states, pushState, popState } = useStateMachine(EditState.Normal_Show, (stateTriple: type_State_Info[]) => {
    //stateTriple[0] is leave
    //stateTriple[1] is stay
    //stateTriple[2] is enter
    stateTriple.forEach((st, idx) => {

      switch (st.state)//current state
      {
        case EditState.Normal_Show:
          if (idx == 2)//enter
          {

          }
          else if (idx == 0)//leave
          {

          }
          break;

        case EditState.Test_Saved_Files:
          if (idx == 2)//enter
          {
          }
          else if (idx == 0)//leave
          {
          }
          break;
        case EditState.Def_Edit:
          if (idx == 2)//enter
          {
            console.log(">>>", _this.canvasComp?.camera);
            //backup camera
            _this.cameraBackup = _this.canvasComp?.camera.toSimpleObj();
            _this.canvasComp?.camera.SetOffset({x:0,y:0});
            // BPG_API.InspTargetExchange(cacheDef.id,{type:"load_template"});
          }
          else if (idx == 0)//leave
          {
            if(_this.cameraBackup!==undefined)
            {
              _this.canvasComp?.camera.fromSimpleObj(_this.cameraBackup);
              _this.cameraBackup=undefined;
            }
          }
          break;

        case EditState.FeatureElement_Edit:
          if (idx == 2)//enter
          {
            console.log(">>>", st.data);
          }
          else if (idx == 0)//leave
          {
          }
          break;

      }
    })

  });
  let currentState = states[states.length - 1];








  useEffect(() => {
    console.log(">>>", def);
    setCacheDef(def);
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
    });

  }, [def]);


  _this.API_PARAM={
    category_id:category_id,
    cur_select_object:cur_select_object,
  }
  useEffect(() => {
      console.log(APIExport)

      if(APIExport!==undefined)
      {
          APIExport({
              id:def.id,
              api1:()=>"hello world",
              get_cur_category_id:()=>_this.API_PARAM.category_id,
              set_cur_category_id:(id:number)=>{
                setCategory_id(id);
              },
              get_cur_select_object:()=>_this.API_PARAM.cur_select_object, 
              set_cur_select_object:(index:number)=>{
                
                setCur_select_object(index);
              },
              register_refresh_listener:(key:string,listener:any)=>{
                _this.EXT_API_REFRESH_LISTENER[key]=listener;
              },
              unregister_refresh_listener:(key:string)=>{
                delete _this.EXT_API_REFRESH_LISTENER[key];
              }
          })
      }



      return (() => {
        _this.EXT_API_REFRESH_LISTENER={};
        if(APIExport!==undefined)
          {
              APIExport({});
          }
    
      });

  }, [APIExport]);

  Object.keys(_this.EXT_API_REFRESH_LISTENER).forEach(key=>{
    _this.EXT_API_REFRESH_LISTENER[key]();
  })


  useEffect(() => {


    BPG_API.InspTargetExchange(def.id, {
      type: "stream_info",
      downsample: display ? 1 : 10,
      stream_id: def.stream_id
    });

    console.log(display,def.id);
    return (() => {
    });

  }, [display]);





  useEffect(() => {//////////////////////

    let cbsKey = "_" + Math.random();
    (async () => {

      let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
      console.log(ret);

      // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
      await BPG_API.send_cbs_attach(
        cacheDef.stream_id, cbsKey, {

        resolve: (pkts) => {
          // console.log(pkts);
          let IM = pkts.find((p: any) => p.type == "IM");
          if (IM === undefined) return;
          let CM = pkts.find((p: any) => p.type == "CM");
          if (CM === undefined) return;
          let RP = pkts.find((p: any) => p.type == "RP");
          if (RP === undefined) return;
          console.log("++++++++\n", IM, CM, RP);


          setDefReport(RP.data)
          let IMCM = {
            image_info: IM.image_info,
            camera_id: CM.data.camera_id,
            trigger_id: CM.data.trigger_id,
            trigger_tag: CM.data.trigger_tag,
          } as type_IMCM

          _this.imgCanvas.width = IMCM.image_info.width;
          _this.imgCanvas.height = IMCM.image_info.height;

          let ctx2nd = _this.imgCanvas.getContext('2d');

          // console.log(IMCM.image_info);
          if (IMCM.image_info.image instanceof ImageData)
            ctx2nd?.putImageData(IMCM.image_info.image, 0, 0);
          else if (IMCM.image_info.image instanceof HTMLImageElement)
            ctx2nd?.drawImage(IMCM.image_info.image, 0, 0);

          setLocal_IMCM(IMCM)
          // console.log(IMCM)
          //console.log(def.id)

        },
        reject: (pkts) => {

        }
      }

      )

    })()
    return (() => {
      (async () => {
        await BPG_API.send_cbs_detach(
          cacheDef.stream_id, cbsKey);

        // await BPG_API.InspTargetSetStreamChannelID(
        //   cacheDef.id,0,
        //   {
        //     resolve:(pkts)=>{
        //     },
        //     reject:(pkts)=>{

        //     }
        //   }
        // )
      })()

    })
  }, []);

  function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

    if (updatedDef === undefined) {
      onDefChange(undefined, false);
      return;
    }
    console.log(updatedDef);
    setCacheDef(updatedDef);



    (async () => {
      await BPG_API.InspTargetUpdate(updatedDef)
    })()
    onDefChange(updatedDef, doTakeNewImage);
  }


  console.log("SingleTargetVIEWUI_DataTransfer", cacheDef, def);




  let EDIT_UI = null;
  console.log("editState", states);
  switch (currentState.state) {

    case EditState.Normal_Show:
    {
      let Category_UI_List=cacheDef.featureInfo?.category_list?.map((catEle:any)=>{
      // console.log(">>>",catEle,defReport);

        return <Button type={category_id==catEle.id?"primary":undefined} key={catEle.id} onClick={()=>{
          setCategory_id(catEle.id);
        }}>{catEle.name}</Button>
      })
      EDIT_UI = <>
        <Input maxLength={100} value={cacheDef.id} disabled
          style={{ width: "200px" }}
          onChange={(e) => {
          }} /> 

        {((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0) ? null :
          <>
            <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
              onCacheDefChange(newDef, ddd);
            }}

              defDoReload={() => defDoReload()}

            />


          </>
        }

        <Button onClick={() => {
          console.log(">>>")
          pushState({ state: EditState.Test_Saved_Files, data: undefined });
        }}>測試儲存圖檔</Button>




        <Button onClick={async() => {

          let pkts=await BPG_API.InspTargetExchange(cacheDef.id, { type: "save_cached_orientation_info",file_name:"template.png" });
          console.log(pkts);
        }}>儲存最近定位圖像</Button>


        <Button onClick={() => {

          BPG_API.InspTargetExchange(cacheDef.id, { type: "load_orientation_info" });
        }}>載入最近定位圖像</Button>



        <Button onClick={() => {

        BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
        }}>重測</Button>




        <Button onClick={() => {
          console.log(">>>")
          pushState({ state: EditState.Def_Edit, data: undefined });
        }}>特徵編輯</Button>

        <br/>
        {Category_UI_List}
      </>
      break;
    }
    case EditState.Test_Saved_Files:

      let folderPath = cacheDef.testInputFolder || fsPath;
      let result_InspTar_stream_id = 51001;//HACK hard coded
      EDIT_UI = <>
        <TestInputSelectUI def={cacheDef} testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
      </>
      break;

    case EditState.Def_Edit:
      {
        let featureInfo = cacheDef?.featureInfo || {};
        let defList = featureInfo?.element_list || [];
        console.log("Def_Edit", defList);
        EDIT_UI = <DEF_Edit_UI
          {...props}
          def={cacheDef}
          DepInject={(injectData: any) => {
            console.log(">>>", injectData);
            pushState({ ...currentState, data: { ...currentState.data, injectData } });
          }}

          onDefChange={(newDef, ddd) => {
            // console.log(">>>",newDef);
            onCacheDefChange(newDef, ddd);
          }}
          onExit={() => {
            popState();
          }}
        />
      }

      break;

  }

  if (display == false) return null;



  return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll"} style={{backgroundColor:"white"}}>
      {/* {currentState.state!=EditState.Normal_Show?<Button danger type="primary" onClick={() => {

            popEditState();
        }}>{"<"}</Button>:null} */}
      {EDIT_UI}



    </div>

    <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
      _this.canvasComp = canvas_obj;
      // console.log(ctrl_or_draw);
      if (_this.extDrawHook !== undefined && _this.extDrawHook.preDraw !== undefined) {
        _this.extDrawHook.preDraw(ctrl_or_draw, g, canvas_obj);
      }


      let ctx = g.ctx;
      let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);

      let camMag = canvas_obj.camera.GetCameraScale();
      let featureEleList = cacheDef.featureInfo?.element_list || [];

      let onMousePress = g.mouseStatus.status == 1 && g.mouseStatus.pstatus == 0;
      if (ctrl_or_draw == true)//ctrl
      {

  
        const imageData = g.ctx.getImageData(g.mouseStatus.x-2, g.mouseStatus.y-2, 1, 1);
        _this.fetchedPixInfo = imageData;

        if (currentState.state == EditState.Normal_Show)
        {
          _this.tmp_select_object=-1;
          let minDist=Number.MAX_VALUE;
          let minIndex=-1;
          if (defReport?.report !== undefined)
          {
            let report = defReport.report
            for (let [index, reportEle] of report.entries()) {
              let { center, angle, flip } = reportEle;

              let dist=Math.hypot(center.x-mouseOnCanvas.x,center.y-mouseOnCanvas.y);
              if(dist<minDist)
              {
                minDist=dist;
                minIndex=index;
              }
            }
            _this.tmp_select_object=minIndex;
            if(onMousePress)
            {
              if(minIndex!==cur_select_object)
              {
                setCur_select_object(minIndex);
              }
            }
          }
        }
        if (currentState.state == EditState.Def_Edit) {//find the nearest feature


        }
        if (currentState.state == EditState.FeatureElement_Edit) {
          // let tarEleIndex=currentState.data.index;
          // let tarEle=featureEleList[tarEleIndex];
          // Draw_FeatureElement_Edit(tarEle,(distance,controlPointPath)=>
          //     {

          //     },ctrl_or_draw,g,canvas_obj);
          // console.log(currentState,">>>",tarEle);
        }
      }
      else//draw
      {
        {
          // Draw arrow from screen center towards origin (0,0)
          {
            ctx.save();
            


            let centerOnCanvas = canvas_obj.VecX2DMat({x:ctx.canvas.width/2,y:ctx.canvas.height/2}, g.worldTransform_inv);
            ctx.lineWidth=1/camMag;
            // Calculate arrow head size based on line length

            let vec={...centerOnCanvas};
            let vecMag=Math.hypot(vec.x,vec.y);
            vec.x=vec.x/vecMag;
            vec.y=vec.y/vecMag;

            let nvec={x:-vec.y,y:vec.x};

            let sizeArrow=30/camMag;
            // Draw main line
            ctx.beginPath();
            ctx.moveTo(centerOnCanvas.x+nvec.x*sizeArrow, centerOnCanvas.y+nvec.y*sizeArrow);
            ctx.lineTo(centerOnCanvas.x-nvec.x*sizeArrow, centerOnCanvas.y-nvec.y*sizeArrow);
            ctx.lineTo(centerOnCanvas.x, centerOnCanvas.y);
            ctx.lineTo(0, 0);//toward camera transform origin
            ctx.strokeStyle = 'rgba(0,0,0,0.5)';
            ctx.stroke();
        
            ctx.restore();
          }
        }

        if (currentState.state == EditState.Normal_Show) {

          let template_angle=0;//cacheDef?.featureInfo?.template_angle;

          if (defReport?.report !== undefined) {
            // console.log(">>>",cacheDef.featureInfo,defReport);
            let report = defReport.report
            let mmpp=defReport?.mmpp??1;


            if (Local_IMCM !== undefined) {
              g.ctx.save();
              let scale = Local_IMCM.image_info.scale*mmpp;
              g.ctx.scale(scale, scale);
              g.ctx.translate(-0.5, -0.5);
              g.ctx.drawImage(_this.imgCanvas, 0, 0);
              g.ctx.restore();
            }



            for (let [index, reportEle] of report.entries()) {
             
              let { center, angle, flip } = reportEle;
              // console.log(">>>",center,angle,flip, index);
              //draw a circle



              g.ctx.save();
              g.ctx.translate(center.x*mmpp, center.y*mmpp);


              let rotate_angle=angle-template_angle;
              //rotate angle
              g.ctx.rotate(rotate_angle);
              
              if(cur_select_object==index)
              {
                //draw primitive feature
                for (let featureEle of featureEleList) {
                  if(featureEle.type.startsWith("Measure_"))continue;
                  MUX_Draw_FeatureElement_Edit(cacheDef.featureInfo, featureEle,reportEle, ctrl_or_draw, g, canvas_obj, 
                    {
                      ...drawTheme.defDisp
                    }
                  
                  );
                }


                for (let featureEle of featureEleList) {
                  if(!featureEle.type.startsWith("Measure_"))continue;
                  MUX_Draw_FeatureElement_Edit(cacheDef.featureInfo, featureEle,reportEle, ctrl_or_draw, g, canvas_obj, 
                    {
                      ...drawTheme.defDisp
                    }
                  
                  );
                }
  
              }

              g.ctx.rotate(-rotate_angle);
              // g.ctx.scale(1.5,1.5);
              //text align center
              g.ctx.textAlign="center";
              // let alpha=(index==_this.tmp_select_object || index==cur_select_object)?1:0.3;

              let alpha=(index==_this.tmp_select_object)?1:0.3;
              let text_color=Color("rgba(100,255,100,"+alpha+")");
              // console.log(">>>",reportEle);
              let text_stroke_color=Color("rgba(0,0,0,"+alpha+")");
              g.ctx.textAlign="center";
              draw_feature_text(g,canvas_obj,{
                ...drawTheme.defDisp,
                text_color:text_color,
                text_stroke_color,
                text_size:5,
              },["["+(index+1)+"]"]);



              // g.ctx.strokeStyle = "green";
              // g.ctx.beginPath();
              // g.ctx.moveTo(0, 0);
              // g.ctx.lineTo(100, 0);
              // g.ctx.stroke();

              // //stroke red
              // g.ctx.strokeStyle = "red";
              // g.ctx.beginPath();
              // g.ctx.arc(0, 0, 10, 0, Math.PI * 2);
              // g.ctx.stroke();


              g.ctx.restore();

            }


            {
              ctx.save();
              ctx.resetTransform();
              ctx.font = "20px Arial";
              ctx.fillStyle = "rgba(150,100, 100,0.5)";
              ctx.fillText("ProcessTime:" + (defReport.process_time_us / 1000).toFixed(2) + " ms", 20, 400)
              

              let tagsStr="";
              if(defReport.tags!==undefined)
              {
                  tagsStr=defReport.tags.join(",");
              }
              ctx.fillText("tags:"+tagsStr, 20, 400+20*(1))



              ctx.restore();
          }
          }


        }

        // {//draw line 00 to 100,100
        //     g.ctx.save();
        //     //line width 2
        //     g.ctx.lineWidth=20;
        //     g.ctx.beginPath();
        //     g.ctx.moveTo(0,0);
        //     g.ctx.lineTo(100,100);
        //     g.ctx.stroke();
        //     g.ctx.restore();

        // }


      }

      if (currentState.state == EditState.FeatureElement_Edit || currentState.state == EditState.Def_Edit) {
        // console.log(">>>",currentState.data);
        if (currentState.data?.injectData?.drawHook !== undefined) {
          currentState.data.injectData.drawHook(ctrl_or_draw, g, canvas_obj);
        }
      }

      if(ctrl_or_draw==false)
      {

        {//draw cursor location text on location
          g.ctx.fillStyle = "red";
          g.ctx.font = "12px Arial";
          g.ctx.save();
          g.ctx.translate(mouseOnCanvas.x, mouseOnCanvas.y);
          g.ctx.scale(1/camMag,1/camMag);
          g.ctx.fillText(mouseOnCanvas.x.toFixed(2)+","+mouseOnCanvas.y.toFixed(2), 0, 0);


          
          // console.log(">>>",g.mouseStatus);
          const imageData = _this.fetchedPixInfo
          let data=imageData.data;

          g.ctx.fillText(data[0]+","+data[1]+","+data[2]+","+data[3], 0, 20);
          
          g.ctx.restore();
        }

      }
    }
    } />


  </div>;
}
