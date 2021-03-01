//UIControl

import { UI_SM_STATES, UI_SM_EVENT, SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';

import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { xstate_GetCurrentMainState, Exp2PostfixExp, PostfixExpCalc, GetObjElement } from 'UTIL/MISC_Util';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from 'UTIL/MathTools';


import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import * as log from 'loglevel';
import dclone from 'clone';

import { EV_UI_Canvas_Mouse_Location } from 'REDUX_STORE_SRC/actions/UIAct';
import Color from 'color';
import { ConsoleSqlOutlined } from '@ant-design/icons';

export const MEASURE_RESULT_VISUAL_INFO = {
  [MEASURERSULTRESION.UNSET]: { COLOR: "rgba(128,128,128,0.5)", TEXT: MEASURERSULTRESION.UNSET },
  [MEASURERSULTRESION.NA]: { COLOR: "rgba(128,128,128,0.5)", TEXT: MEASURERSULTRESION.NA },
  [MEASURERSULTRESION.UOK]: { COLOR: "rgba(128,200,128,0.7)", TEXT: MEASURERSULTRESION.UOK },
  [MEASURERSULTRESION.LOK]: { COLOR: "rgba(128,200,128,0.7)", TEXT: MEASURERSULTRESION.LOK },
  [MEASURERSULTRESION.UCNG]: { COLOR: "rgba(255,255,0,0.5)", TEXT: MEASURERSULTRESION.UCNG },
  [MEASURERSULTRESION.LCNG]: { COLOR: "rgba(255,255,0,0.5)", TEXT: MEASURERSULTRESION.LCNG },
  [MEASURERSULTRESION.USNG]: { COLOR: "rgba(255,0,0,0.5)", TEXT: MEASURERSULTRESION.USNG },
  [MEASURERSULTRESION.LSNG]: { COLOR: "rgba(255,0,0,0.5)", TEXT: MEASURERSULTRESION.LSNG },
};

export const SHAPE_TYPE_COLOR={
  [SHAPE_TYPE.line]:"hsl(0, 60%, 35%)",
  [SHAPE_TYPE.arc]:"hsl(48, 60%, 35%)",
  [SHAPE_TYPE.search_point]:"hsl(180, 60%, 35%)",

  
  [SHAPE_TYPE.aux_line]:"hsl(48, 60%, 35%)",
  [SHAPE_TYPE.aux_point]:"hsl(220, 60%, 35%)",
  [SHAPE_TYPE.measure]:"hsl(290, 80%, 65%)",
  default:"rgba(100,50,100)"
}

class CameraCtrl {
  constructor(canvasDOM) {
    this.matrix = new DOMMatrix();
    this.tmpMatrix = new DOMMatrix();
    this.identityMat = new DOMMatrix();
    this.Scale(1);
  }

  Scale(scale, center = { x: 0, y: 0 }) {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x, center.y);
    mat.scaleSelf(scale, scale);
    mat.translateSelf(-center.x, -center.y);

    this.matrix.preMultiplySelf(mat);
  }

  Rotate_matrix(theta, center = { x: 0, y: 0 }) {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x, center.y);
    let sinT=Math.sin(theta);
    let cosT=Math.cos(theta);

    
    mat.b = sinT;
    mat.c = -sinT;
    mat.a = 
    mat.d = cosT;
    // mat.is2D=true;
    mat.translateSelf(-center.x, -center.y);
    // console.log(mat);

    let scale = this.GetCameraScale();
    
    this.matrix.b = 0;
    this.matrix.c = 0;
    this.matrix.a = 
    this.matrix.d = scale;

    return mat;
  }
  
  Rotate(theta, center = { x: 0, y: 0 }) {

    this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center ));
  }

  ResetRotate(theta, center = { x: 0, y: 0 }) {

    // this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center ));
  }
  SetRotate(theta, center = { x: 0, y: 0 }) {

    this.matrix=this.Rotate_matrix(theta, center );
  }
  StartDrag(vector = { x: 0, y: 0 }) {
    this.tmpMatrix.setMatrixValue(this.identityMat);
    this.tmpMatrix.translateSelf(vector.x, vector.y);
  }
  EndDrag() {
    this.matrix.preMultiplySelf(this.tmpMatrix);
    this.tmpMatrix.setMatrixValue(this.identityMat);
  }

  SetOffset(location = { x: 0, y: 0 }) {

    this.matrix.m41 = 0;
    this.matrix.m42 = 0;
    this.matrix.translateSelf(location.x, location.y);
  }

  GetCameraRotation(matrix = this.matrix) {
    let rot = Math.atan2( matrix.m21-matrix.m12,matrix.m11 + matrix.m22);
    // console.log(rot);
    return rot;
  }

  GetCameraScale(matrix = this.matrix) {//It's an over simplified way to get scale for an matrix, change it if nesessary
    return Math.sqrt(matrix.m11 * matrix.m22 - matrix.m12 * matrix.m21);
  }


  GetCameraOffset(matrix = this.matrix) {
    return { x: matrix.m41, y: matrix.m42 };
  }

  CameraTransform(matrix_input) {
    matrix_input.multiplySelf(this.tmpMatrix);
    matrix_input.multiplySelf(this.matrix);
  }


}


class renderUTIL {
  constructor(editor_db_obj, cameraCtrl) {
    this.setEditor_db_obj(editor_db_obj);
    this.camCtrl = cameraCtrl;
    this.colorSet = {
      unselected: "rgba(100,0,100,0.5)",
      inspection_Pass: "rgba(0,255,0,0.1)",
      inspection_production_Fail: "rgba(128,128,0,0.1)",
      inspection_Fail: "rgba(255,0,0,0.1)",
      inspection_UNSET: "rgba(128,128,128,0.1)",
      inspection_NA: "rgba(128,128,128,0.1)",
      editShape: "rgba(255,0,0,0.7)",
      measure_info: "rgba(128,128,200,0.7)"
    };
    this.fixedDigit={
      D:4,
      R:4,
      A:2,
      _:4
    }
    this.renderParam = {
      base_Size: 2.5,
      size_Multiplier: 1,
      mmpp: 0.1,
      font_Base_Size: 1,
      font_Style: "bold ",
      

      measureInfoText:{
        name:true,
        showMarginPC:false,
        value:true,
        
        showCur:true,
        showLU:false,
      }
    };
  }
  get_mmpp() {
    return this.renderParam.mmpp;
  }
  getPrimitiveSize() {
    return this.renderParam.base_Size * this.renderParam.size_Multiplier/ this.camCtrl.GetCameraScale();;
  }

  getPointSize() {
    return this.getPrimitiveSize()*2;
  }
  getIndicationLineSize() {
    return this.getPrimitiveSize();
  }
  getSearchDirectionLineSize() {
    return this.getPrimitiveSize();
  }

  getFontHeightPx(size = this.renderParam.font_Base_Size) {
    return 1.5*size * this.renderParam.size_Multiplier*16/ this.camCtrl.GetCameraScale();;
  }

  getFixSizingReg() {
    return 1;//50 / this.camCtrl.GetCameraScale();
  }

  getFontStyle(size_px = this.getFontHeightPx()) {
    return this.renderParam.font_Style + size_px + "px Arial";
  }

  setEditor_db_obj(editor_db_obj) {
    this.db_obj = editor_db_obj;
  }

  setColorSet(colorset) {
    this.colorSet = colorset;
  }
  drawReportLine(ctx, line_obj, offset = { x: 0, y: 0 }) {
    ctx.beginPath();
    ctx.moveTo(line_obj.x0 + offset.x, line_obj.y0 + offset.y);
    ctx.lineTo(line_obj.x1 + offset.x, line_obj.y1 + offset.y);
    //ctx.closePath();
    ctx.stroke();
  }


  drawLine(ctx, line, offset = { x: 0, y: 0 }) {
    ctx.beginPath();
    ctx.moveTo(line.pt1.x + offset.x, line.pt1.y + offset.y);
    ctx.lineTo(line.pt2.x + offset.x, line.pt2.y + offset.y);
    //ctx.closePath();
    ctx.stroke();
  }

  drawHollowLine(ctx, line,boundaryWidth=5, offset = { x: 0, y: 0 }) {
    ctx.beginPath();
    let LineWidth=ctx.lineWidth;
    ctx.lineWidth=boundaryWidth;
    let vec = {
      x:(line.pt2.x-line.pt1.x),
      y:(line.pt2.y-line.pt1.y),
    }
    let vecL=Math.hypot(vec.x,vec.y);
    let normalVec={
      x:-vec.y/vecL,
      y:vec.x/vecL,
    };
    // normalVec
    ctx.moveTo(line.pt1.x + offset.x-normalVec.x*LineWidth/2, line.pt1.y + offset.y-normalVec.y*LineWidth/2);
    ctx.lineTo(line.pt1.x + offset.x+normalVec.x*LineWidth/2, line.pt1.y + offset.y+normalVec.y*LineWidth/2);
    ctx.lineTo(line.pt2.x + offset.x+normalVec.x*LineWidth/2, line.pt2.y + offset.y+normalVec.y*LineWidth/2);
    ctx.lineTo(line.pt2.x + offset.x-normalVec.x*LineWidth/2, line.pt2.y + offset.y-normalVec.y*LineWidth/2);


    ctx.closePath();
    ctx.stroke();
  }



  drawReportArc(ctx, arc_obj, offset = { x: 0, y: 0 }) {
    let r=arc_obj.r;
    if(r<0)r=-r;
    ctx.beginPath();
    if (arc_obj.thetaS === undefined || arc_obj.thetaE === undefined)
      ctx.arc(arc_obj.x + offset.x, arc_obj.y + offset.y, r, 0, Math.PI * 2, false);
    else
      ctx.arc(arc_obj.x + offset.x, arc_obj.y + offset.y, r, arc_obj.thetaS, arc_obj.thetaE, false);
    //ctx.closePath();
    ctx.stroke();
  }

  _drawpoint(ctx, point, type, size = 5) {
    ctx.beginPath();

    if (type == "rect") {
      ctx.rect(point.x - size / 2, point.y - size / 2, size, size);
    }
    else if (type == "cross") {
      ctx.moveTo(point.x - size / 2, point.y);
      ctx.lineTo(point.x + size / 2, point.y);

      ctx.moveTo(point.x, point.y - size / 2);
      ctx.lineTo(point.x, point.y + size / 2);
    }
    else {
      ctx.arc(point.x, point.y, size / 2, 0, Math.PI * 2, false);
    }
    ctx.stroke();
    ctx.closePath();
  }

  drawpoint(ctx, point, type, size = this.getPointSize()) {
    let strokeStyle_bk = ctx.strokeStyle;

    ctx.lineWidth = size * 2;
    ctx.strokeStyle = "rgba(0,0,100,0.5)";
    if (type != "cross")
      this._drawpoint(ctx, point, type, 2 * size);

    ctx.lineWidth = size / 2;
    ctx.strokeStyle = strokeStyle_bk;
    this._drawpoint(ctx, point, type, 2 * size);
  }
  
  draw_aimcross(ctx, point, size = this.getPointSize(),ratio=0.2) {


    
    ctx.beginPath();
    // ctx.moveTo(point.x - size / 2, point.y);
    // ctx.lineTo(point.x + size / 2, point.y);

    ctx.moveTo(point.x, point.y - size / 2);
    ctx.lineTo(point.x, point.y - size / 2*ratio);


    ctx.moveTo(point.x, point.y + size / 2);
    ctx.lineTo(point.x, point.y + size / 2*ratio);

    
    ctx.moveTo(point.x - size / 2, point.y);
    ctx.lineTo(point.x - size / 2*ratio, point.y);
    ctx.moveTo(point.x + size / 2, point.y);
    ctx.lineTo(point.x + size / 2*ratio, point.y);

    ctx.stroke();
    ctx.closePath();
  }

  
  drawcross(ctx, point, size = this.getPointSize()) {
    this._drawpoint(ctx, point, "cross", 2 * size);
  }


  drawInherentShapeList(ctx, inherentShapeList) {
    if (inherentShapeList === undefined || inherentShapeList == null) return;

    inherentShapeList.forEach((ishape) => {
      if (ishape == null) return;

      switch (ishape.type) {
        case SHAPE_TYPE.aux_point:
          {
            let point = this.db_obj.auxPointParse(ishape);
            if (point != null) {
              ctx.strokeStyle = "black";
              this.drawpoint(ctx, point, "rect")
            }
          }
          break;

        case 'aux_line':
          {
          }
          break;
      }
    });
  }
  canvas_arrow(ctx, fromx, fromy, tox, toy, headlen = 10, aangle = Math.PI / 6) {
    var angle = Math.atan2(toy - fromy, tox - fromx);
    ctx.beginPath();
    ctx.moveTo(fromx, fromy);
    ctx.lineTo(tox, toy);

    ctx.moveTo(tox, toy);
    ctx.lineTo(tox - headlen * Math.cos(angle - aangle), toy - headlen * Math.sin(angle - aangle));
    //ctx.moveTo(tox, toy);
    ctx.lineTo(tox - headlen * Math.cos(angle + aangle), toy - headlen * Math.sin(angle + aangle));

    ctx.closePath();
    ctx.stroke();
    ctx.fill();
  }

  drawArcArrow(ctx, x, y, r, sAngle, eAngle, ccw = false) {
    ctx.beginPath();
    //log.debug(ctx,x,y,r,sAngle,eAngle,ccw);
    ctx.arc(x, y, r, sAngle, eAngle, ccw);
    ctx.stroke();
    let ax = Math.cos(eAngle);
    let ay = Math.sin(eAngle);
    x += r * ax;
    y += r * ay;
    let dirSign = (ccw) ? -1 : 1;
    dirSign *= this.getPrimitiveSize();
    let arrowSize = 3 * this.getPrimitiveSize();
    this.canvas_arrow(ctx, x + dirSign * ay, y - dirSign * ax, x, y, arrowSize);

  }
  drawLineArrow(ctx, x1, y1, x2, y2) {
  }

  drawText(ctx, text, x, y) {
    ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.01;
    ctx.fillText(text, x, y);
    ctx.strokeStyle = "black";
    ctx.lineWidth = 1;//this.getIndicationLineSize();
    ctx.strokeText(text, x, y);
  }

  draw_Text(ctx, text, scale, x, y) {
    ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.013;
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(scale, scale);
    ctx.fillText(text, 0, 0);
    ctx.strokeText(text, 0, 0);
    ctx.restore();
  }

  drawInspMeasureInfoText(ctx,name,value,marginPC,fontPx)
  {
    ctx.strokeStyle = "black";
    ctx.lineWidth = this.getIndicationLineSize() / 3;
    let Y_offset = 0;
    if(this.renderParam.measureInfoText.name==true)
      this.draw_Text(ctx, name, fontPx, 0,0);
    
    Y_offset+=fontPx;
    let text="";
    if(this.renderParam.measureInfoText.value==true)
      text = value;
    else
      text = "";

    if(marginPC==marginPC && isFinite(marginPC) && this.renderParam.measureInfoText.showMarginPC==true)
      text += ":" + (marginPC * 100).toFixed(1) + "%";

    this.draw_Text(ctx, text, fontPx, 0, Y_offset);
  }

  drawDefMeasureInfoText(ctx,name,value,InfoLU,InfoCurVal,fontPx)
  {

    let Y_offset = 0;


        
    if(this.renderParam.measureInfoText.name==true)
      this.draw_Text(ctx, name, fontPx, 0,0);
    

    if(this.renderParam.measureInfoText.value==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, value, fontPx, 0, Y_offset);
    }

    fontPx *=0.7;

    if(this.renderParam.measureInfoText.showLU==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, InfoLU, fontPx, 0, Y_offset);
    }


    if(this.renderParam.measureInfoText.showCur==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, InfoCurVal, fontPx,0,Y_offset);
    }
  }

  drawMeasureDistance(ctx, eObject, refObjs, shapeList, unitConvert) {

    let alignLine = null;
    let point_onAlignLine = null;
    let point = null;

    ctx.lineWidth = this.getIndicationLineSize();

    let db_obj = this.db_obj;
    point_onAlignLine = db_obj.shapeMiddlePointParse(refObjs[0], shapeList);
    point = db_obj.shapeMiddlePointParse(refObjs[1], shapeList);

    let main_refObj;
    if (eObject.ref_baseLine !== undefined && eObject.ref_baseLine.id !== undefined) {
      main_refObj = shapeList.find((shape) => shape.id == eObject.ref_baseLine.id);
    }
    if (main_refObj === undefined) main_refObj = refObjs[0];

    let mainObjVec = db_obj.shapeVectorParse(main_refObj, shapeList);
    if (mainObjVec === undefined) {
      mainObjVec = { x: -(point.y - point_onAlignLine.y), y: (point.x - point_onAlignLine.x) };
    }

    alignLine = {
      x1: point_onAlignLine.x, y1: point_onAlignLine.y,
      x2: point_onAlignLine.x + mainObjVec.x, y2: point_onAlignLine.y + mainObjVec.y,
    };


    if (point != null && alignLine != null) {
      //this.canvas_arrow(ctx, point.x, point.y, point_on_line.x, point_on_line.y);

      let point_on_line = closestPointOnLine(alignLine, point);


      let closestPt_disp = closestPointOnLine(alignLine, eObject.pt1);


      let extended_ind_line = {
        x0: closestPt_disp.x, y0: closestPt_disp.y,
        x1: closestPt_disp.x + (point.x - point_on_line.x),
        y1: closestPt_disp.y + (point.y - point_on_line.y),
      }


      ctx.setLineDash([this.getPrimitiveSize(), this.getPrimitiveSize()]);

      this.drawReportLine(ctx, {

        x0: extended_ind_line.x0, y0: extended_ind_line.y0,
        x1: point_onAlignLine.x, y1: point_onAlignLine.y
      });

      this.drawReportLine(ctx, {

        x0: extended_ind_line.x1, y0: extended_ind_line.y1,
        x1: point.x, y1: point.y
      });


      this.drawReportLine(ctx, {

        x0: extended_ind_line.x1, y0: extended_ind_line.y1,
        x1: eObject.pt1.x, y1: eObject.pt1.y
      });
      ctx.setLineDash([]);


      this.drawReportLine(ctx, extended_ind_line);

      this.drawpoint(ctx, eObject.pt1);


      let fontPx = this.getFontHeightPx();
      ctx.font = this.getFontStyle(1);
      ctx.strokeStyle = "black";
      ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.02;

      ctx.save();
      ctx.translate(eObject.pt1.x, eObject.pt1.y);
      
      let measureValue;
      if (eObject.inspection_value !== undefined) {

        let marginPC = (eObject.inspection_value > eObject.value) ?
          (eObject.inspection_value - eObject.value) / (eObject.USL - eObject.value) :
          -(eObject.inspection_value - eObject.value) / (eObject.LSL - eObject.value);
        
        this.drawInspMeasureInfoText(ctx,
          eObject.name,
          "D" + (eObject.inspection_value * unitConvert.mult).toFixed(this.fixedDigit.D) + unitConvert.unit,
          marginPC,fontPx);

        measureValue=eObject.inspection_value;
        // this.draw_Text(ctx, text, fontPx, eObject.pt1.x, eObject.pt1.y + Y_offset);
      }
      else {

        measureValue=Math.hypot(point.x - point_on_line.x, point.y - point_on_line.y);
        
        this.drawDefMeasureInfoText(ctx,
          eObject.name,
          "D" + eObject.value.toFixed(this.fixedDigit.D) + unitConvert.unit,
          "L:" + eObject.LSL * unitConvert.mult.toFixed(this.fixedDigit.D) + unitConvert.unit + 
          " U:" + eObject.USL * unitConvert.mult.toFixed(this.fixedDigit.D) + unitConvert.unit,
          "Now:" + (measureValue * unitConvert.mult).toFixed(this.fixedDigit.D) + unitConvert.unit,
          fontPx)

      }
      ctx.restore();

      return measureValue;

    }
    
    return undefined;
  }


  drawSignature(ctx, signature, pointSkip = 36) {

    ctx.beginPath();
    ctx.moveTo(
      signature.magnitude[0] * Math.cos(signature.angle[0]),
      signature.magnitude[0] * Math.sin(signature.angle[0]));
    for (let i = 1; i < signature.angle.length; i += pointSkip) {

      ctx.lineTo(
        signature.magnitude[i] * Math.cos(signature.angle[i]),
        signature.magnitude[i] * Math.sin(signature.angle[i]));

    }
    ctx.closePath();
    //ctx.stroke();
  }

  drawShapeList(ctx, eObjects, ShapeColor = undefined, skip_id_list = [], shapeList, unitConvert = { unit: "mm", mult: 1 }, drawSubObjs = false,inFullDisplay=true) {
    let next_ShapeColor = null
    if (ShapeColor !== undefined && ShapeColor !== null) {
      next_ShapeColor = Color(ShapeColor).desaturate(0.6).string()
    }
    
    let measureValueCache=[];
    eObjects.forEach((eObject) => {
      if (eObject == null) return;
      var found = skip_id_list.find(function (skip_id) {
        return eObject.id == skip_id;
      });
      if (found !== undefined) {
        return;
      }
      else {
        if (ShapeColor !== undefined && ShapeColor !== null)
          ctx.strokeStyle = ShapeColor;
        else
          ctx.strokeStyle = eObject.color;

      }
      

      let shapeColor=SHAPE_TYPE_COLOR[eObject.type];
      if(shapeColor===undefined)
      {
        shapeColor=SHAPE_TYPE_COLOR.default;
      }
      shapeColor = Color(shapeColor).alpha(0.8);
      switch (eObject.type) {
        case SHAPE_TYPE.line:
          {
            let cnormal = LineCentralNormal(eObject);

            let drawMargin=this.getSearchDirectionLineSize();
            if(inFullDisplay)
            {
              drawMargin=eObject.margin;
            }
            ctx.lineWidth =drawMargin* 2;
            this.drawReportLine(ctx, {
              x0: eObject.pt1.x, y0: eObject.pt1.y,
              x1: eObject.pt2.x, y1: eObject.pt2.y,
            });


            ctx.lineWidth = this.getSearchDirectionLineSize();
            ctx.strokeStyle = shapeColor;
            let marginOffset = drawMargin+ ctx.lineWidth / 2;
            this.drawReportLine(ctx, {
              x0: eObject.pt1.x + cnormal.vx * marginOffset, y0: eObject.pt1.y + cnormal.vy * marginOffset,
              x1: eObject.pt2.x + cnormal.vx * marginOffset, y1: eObject.pt2.y + cnormal.vy * marginOffset,
            });

            ctx.strokeStyle = "gray";
            this.drawpoint(ctx, eObject.pt1);
            this.drawpoint(ctx, eObject.pt2);
          }
          break;

        case SHAPE_TYPE.aux_point:
          {

            if(true||inFullDisplay)
            {
              ctx.lineWidth = this.getSearchDirectionLineSize();
              ctx.strokeStyle = shapeColor.alpha(1);
              let db_obj = this.db_obj;
              let subObjs = eObject.ref
                .map((ref) => db_obj.FindShape("id", ref.id, shapeList))
                .map((idx) => { return idx >= 0 ? shapeList[idx] : null });
              if (drawSubObjs)
                this.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
              if (eObject.id === undefined) break;

              let point = this.db_obj.auxPointParse(eObject, shapeList);
              if (point !== undefined && subObjs.length == 2) {//Draw crosssect line
                ctx.setLineDash([2*this.getPrimitiveSize(), this.getPrimitiveSize()]);

                ctx.beginPath();
                ctx.moveTo(point.x, point.y);

                let closestPt=closestPointOnPoints(point,[subObjs[0].pt1,subObjs[0].pt2]);
                ctx.lineTo(closestPt.x,closestPt.y);
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(point.x, point.y);
                
                closestPt=closestPt=closestPointOnPoints(point,[subObjs[1].pt1,subObjs[1].pt2]);
                ctx.lineTo(closestPt.x,closestPt.y);
                ctx.stroke();
                ctx.setLineDash([]);
                ctx.strokeStyle = "gray";
                this.drawpoint(ctx, point);
              }
            }
          }
          break;


        case SHAPE_TYPE.aux_line:
          {

            let db_obj = this.db_obj;
            let subObjs = eObject.ref
              .map((ref) => db_obj.FindShape("id", ref.id, shapeList))
              .map((idx) => { return idx >= 0 ? shapeList[idx] : null });
            if (drawSubObjs)
              this.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
            if (eObject.id === undefined) break;

            if (subObjs.length == 2) {//Draw crosssect line
              ctx.setLineDash([this.getPrimitiveSize(), this.getPrimitiveSize()]);

              ctx.strokeStyle = "gray";
              ctx.beginPath();
              ctx.moveTo(subObjs[0].pt1.x, subObjs[0].pt1.y);
              ctx.lineTo(subObjs[1].pt1.x, subObjs[1].pt1.y);
              ctx.stroke();
              ctx.setLineDash([]);
              //this.drawpoint(ctx, point);
            }
          }
          break;


        case SHAPE_TYPE.arc:
          {
            let arc = threePointToArc(eObject.pt1, eObject.pt2, eObject.pt3);
            let margin=this.getSearchDirectionLineSize()
            if(inFullDisplay){
              margin=eObject.margin
            }
            //ctx.strokeStyle=eObject.color; 
            ctx.lineWidth = margin * 2;
            this.drawReportArc(ctx, arc);

            ctx.lineWidth = this.getSearchDirectionLineSize();
            ctx.strokeStyle = shapeColor;

            let marginOffset = margin + ctx.lineWidth / 2;
            if (eObject.direction < 0) {
              marginOffset = -marginOffset;
            }
            arc.r += marginOffset;
            //console.log(arc);
            if(arc.r<0.0001)arc.r=0.0001;

            this.drawReportArc(ctx, arc);


            ctx.strokeStyle = "gray";
            this.drawpoint(ctx, eObject.pt1);
            this.drawpoint(ctx, eObject.pt2);
            this.drawpoint(ctx, eObject.pt3);
          }
          break;

        case SHAPE_TYPE.search_point:
          {
            let db_obj = this.db_obj;
            let subObjs = eObject.ref
              .map((ref) => db_obj.FindShape("id", ref.id, shapeList))
              .map((idx) => { return idx >= 0 ? shapeList[idx] : null });

            if (subObjs[0] == null) break;

            let line = subObjs[0];

            let vector = db_obj.shapeVectorParse(eObject, shapeList);
            let cnormal = { x: -vector.y, y: vector.x };
            let mag = eObject.width / 2;
            vector.x *= mag;
            vector.y *= mag;

            let margin=this.getSearchDirectionLineSize()
            if(inFullDisplay){
              margin=eObject.margin
            }

            ctx.lineWidth = margin * 2;
            this.drawReportLine(ctx, {
              x0: eObject.pt1.x - vector.x, y0: eObject.pt1.y - vector.y,
              x1: eObject.pt1.x + vector.x, y1: eObject.pt1.y + vector.y,
            });


            ctx.lineWidth = this.getSearchDirectionLineSize();
            ctx.strokeStyle = shapeColor;
            let marginOffset = margin + ctx.lineWidth / 2;
            this.drawReportLine(ctx, {
              x0: eObject.pt1.x - vector.x + cnormal.x * marginOffset, y0: eObject.pt1.y - vector.y + cnormal.y * marginOffset,
              x1: eObject.pt1.x + vector.x + cnormal.x * marginOffset, y1: eObject.pt1.y + vector.y + cnormal.y * marginOffset,
            });




            if (drawSubObjs)
              this.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);

            ctx.strokeStyle = "gray";
            this.drawpoint(ctx, eObject.pt1);
            if(eObject.locating_anchor)
            {
              ctx.strokeStyle = "red";
              this.draw_aimcross(ctx, eObject.pt1, this.getPointSize()*3,0.3);
            }

          }
          break;
        case SHAPE_TYPE.measure:
          {
            let db_obj = this.db_obj;
            if (eObject.ref === undefined) break;
            let subObjs = eObject.ref
              .map((ref) => db_obj.FindShapeObject("id", ref.id, shapeList));
            let subShapeValues;
            if (drawSubObjs)
            {
              subShapeValues = this.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
            }
            if(subShapeValues===undefined)
            {
              subShapeValues=[];
            }


            let subObjs_valid = subObjs.reduce((acc, cur) => acc && (cur !== undefined), true);
            if (!subObjs_valid) break;

            if (ShapeColor == undefined || ShapeColor == null) {
              if (eObject.color !== undefined) {
                ctx.strokeStyle = eObject.color;
                ctx.fillStyle = eObject.color;
              }
              else {
                ctx.strokeStyle = shapeColor;
                ctx.fillStyle = shapeColor;
              }
            }
            else {
              ctx.strokeStyle = ShapeColor;
              ctx.fillStyle = ShapeColor;

            }
            ctx.lineWidth = this.getIndicationLineSize();
            let measureValue;
            switch (eObject.subtype) {
              case SHAPE_TYPE.measure_subtype.distance:
                {
                  measureValue = this.drawMeasureDistance(ctx, eObject, subObjs, shapeList, unitConvert);

                }
                break;
              case SHAPE_TYPE.measure_subtype.angle:
                {
                  let obj0_pt2=subObjs[0].pt2;

                  if(obj0_pt2===undefined)
                  {
                    
                    obj0_pt2= db_obj.shapeVectorParse(subObjs[0], shapeList);
                    obj0_pt2.x+=subObjs[0].pt1.x;
                    obj0_pt2.y+=subObjs[0].pt1.y;
                  }

                  let obj1_pt2=subObjs[1].pt2;
                  
                  if(obj1_pt2===undefined)
                  {
                    
                    obj1_pt2 = db_obj.shapeVectorParse(subObjs[1], shapeList);
                    obj1_pt2.x+=subObjs[1].pt1.x;
                    obj1_pt2.y+=subObjs[1].pt1.y;
                  }
                  //console.log(eObject,subObjs,obj0_pt2,obj1_pt2);

                  
                  let srcPt =
                    intersectPoint(subObjs[0].pt1, obj0_pt2, subObjs[1].pt1, obj1_pt2);

                  ctx.lineWidth = this.getIndicationLineSize();
                  //ctx.strokeStyle=this.colorSet.measure_info; 

                  ///ctx.fillStyle=this.colorSet.measure_info; 
                  //this.drawpoint(ctx, srcPt,"cross");

                  let sAngle = Math.atan2(subObjs[0].pt1.y - srcPt.y, subObjs[0].pt1.x - srcPt.x);
                  let eAngle = Math.atan2(subObjs[1].pt1.y - srcPt.y, subObjs[1].pt1.x - srcPt.x);
                  //eAngle+=Math.PI;

                  let angleDiff = (eAngle - sAngle) % (2 * Math.PI);
                  if (angleDiff < 0) {
                    angleDiff += Math.PI * 2;
                  }
                  if (angleDiff > Math.PI) {
                    angleDiff -= Math.PI;
                  }


                  let quadrant = 0;

                  //if(eObject.quadrant===undefined)
                  {

                    let midwayAngle = Math.atan2(eObject.pt1.y - srcPt.y, eObject.pt1.x - srcPt.x);//-PI~PI

                    let angleDiff_midway = (midwayAngle - sAngle) % (2 * Math.PI);
                    if (angleDiff_midway < 0) {
                      angleDiff_midway += Math.PI * 2;
                    }

                    if (angleDiff_midway < angleDiff) {
                      quadrant = 1;
                    }
                    else if (angleDiff_midway < Math.PI) {
                      quadrant = 2;
                    }
                    else if (angleDiff_midway < (Math.PI + angleDiff)) {
                      quadrant = 3;
                    }
                    else {
                      quadrant = 4;
                    }


                  }

                  {
                    eObject.quadrant = quadrant;
                  }

                  let dist = Math.hypot(eObject.pt1.y - srcPt.y, eObject.pt1.x - srcPt.x);
                  let margin_deg = eObject.margin * Math.PI / 180;
                  let draw_sAngle = sAngle, draw_eAngle = eAngle;
                  let ext_Angle1 = sAngle, ext_Angle2 = eAngle;
                  switch (quadrant % 4) {
                    case 1:
                      {

                      }
                      break;

                    case 2:
                      {
                        draw_sAngle += angleDiff;
                      }
                      break;
                    case 3:
                      {
                        draw_sAngle += Math.PI;

                      }
                      break;
                    case 0:
                      {
                        draw_sAngle = draw_sAngle + angleDiff + Math.PI;

                      }
                      break;
                  }
                  //log.debug(angleDiff*180/Math.PI,sAngle*180/Math.PI,eAngle*180/Math.PI);
                  if (quadrant % 2 == 0)//if our target quadrant is 2 or 4..., find the complement angle 
                  {
                    angleDiff = Math.PI - angleDiff;
                  }

                  draw_eAngle = draw_sAngle + angleDiff;

                  if (quadrant % 2 == 0) {
                    ext_Angle1 = draw_eAngle;
                    ext_Angle2 = draw_sAngle;
                  }
                  else {
                    ext_Angle1 = draw_sAngle;
                    ext_Angle2 = draw_eAngle;
                  }



                  this.drawArcArrow(ctx, srcPt.x, srcPt.y, dist, draw_sAngle, draw_eAngle);

                  this.drawpoint(ctx, eObject.pt1);

                  let measureDeg = angleDiff * 180 / Math.PI;


                  {

                    ctx.lineWidth = this.getIndicationLineSize();
                    ctx.setLineDash([this.getPrimitiveSize(), 1*this.getPrimitiveSize()])

                    let arcPt={x:srcPt.x + dist * Math.cos(ext_Angle1),y:srcPt.y + dist * Math.sin(ext_Angle1)};
                    let closestPt=closestPointOnPoints(arcPt,[subObjs[0].pt1,obj0_pt2]);
                    this.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    arcPt={x:srcPt.x + dist * Math.cos(ext_Angle2),y:srcPt.y + dist * Math.sin(ext_Angle2)};
                    closestPt=closestPointOnPoints(arcPt,[subObjs[1].pt1,obj1_pt2]);
                    this.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    ctx.setLineDash([]);
                  }

                  let x = eObject.pt1.x + (eObject.pt1.x - srcPt.x) / dist * 4 * this.getPrimitiveSize();
                  let y = eObject.pt1.y + (eObject.pt1.y - srcPt.y) / dist * 4 * this.getPrimitiveSize();



                  let fontPx = this.getFontHeightPx();
                  ctx.font = this.getFontStyle(1);


                  ctx.save();
                  ctx.translate(eObject.pt1.x, eObject.pt1.y);
                  
                  ctx.strokeStyle = "black";
                  if (eObject.inspection_value !== undefined) {
                    
                    let marginPC = (eObject.inspection_value > eObject.value) ?
                      (eObject.inspection_value - eObject.value) / (eObject.USL - eObject.value) :
                      -(eObject.inspection_value - eObject.value) / (eObject.LSL - eObject.value);
                    this.drawInspMeasureInfoText(ctx,
                      eObject.name,
                      (eObject.inspection_value).toFixed(this.fixedDigit.A) + "º",
                      marginPC,fontPx);
                    measureValue=eObject.inspection_value;
                  }
                  else {
            
                    
                    this.drawDefMeasureInfoText(ctx,
                      eObject.name,
                      ""+eObject.value.toFixed(this.fixedDigit.A) + "º",
                      "L:" + eObject.LSL.toFixed(this.fixedDigit.A) + "º U:" + eObject.USL.toFixed(this.fixedDigit.A) + "º",
                      "Now:" + (measureDeg).toFixed(this.fixedDigit.A) + "º",
                      fontPx)
                    
                    measureValue=measureDeg;
                  }
                  ctx.restore();
                }
                break;

              case SHAPE_TYPE.measure_subtype.radius:
                {
                  ctx.lineWidth = this.getIndicationLineSize();
                  //ctx.strokeStyle=this.colorSet.measure_info; 

                  ctx.font = this.getFontStyle(1);
                  let arc = threePointToArc(subObjs[0].pt1, subObjs[0].pt2, subObjs[0].pt3);
                  let dispVec = { x: eObject.pt1.x - arc.x, y: eObject.pt1.y - arc.y };
                  let mag = Math.hypot(dispVec.x, dispVec.y);
                  let dispVec_normalized = { x: dispVec.x / mag, y: dispVec.y / mag };
                  dispVec.x *= arc.r / mag;
                  dispVec.y *= arc.r / mag;//{x:dispVec.x*arc.r/mag,y:dispVec.x*arc.r/mag};

                  /*let lineInfo = {
                    x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                    x1:eObject.pt1.x,y1:eObject.pt1.y,
                  };*/
                  let arrowSize = 3 * this.getPrimitiveSize();
                  this.canvas_arrow(ctx, eObject.pt1.x, eObject.pt1.y, arc.x + dispVec.x, arc.y + dispVec.y, arrowSize);
                  //this.drawReportLine(ctx, lineInfo);

                  this.drawpoint(ctx, eObject.pt1);

                  dispVec_normalized.x *= 5 * this.getPrimitiveSize();
                  dispVec_normalized.y *= 5 * this.getPrimitiveSize();



                  let fontPx = this.getFontHeightPx();
                  ctx.font = this.getFontStyle(1);




                  ctx.save();
                  ctx.translate(eObject.pt1.x, eObject.pt1.y);
                  
                  ctx.strokeStyle = "black";
                  if (eObject.inspection_value !== undefined) {

                    let marginPC = (eObject.inspection_value > eObject.value) ?
                      (eObject.inspection_value - eObject.value) / (eObject.USL - eObject.value) :
                      -(eObject.inspection_value - eObject.value) / (eObject.LSL - eObject.value);
                      
                    this.drawInspMeasureInfoText(ctx,
                      eObject.name,
                      "R" + (eObject.inspection_value * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=eObject.inspection_value;
                  }
                  else {
            
                    
                    this.drawDefMeasureInfoText(ctx,
                      eObject.name,
                      "R" + eObject.value.toFixed(this.fixedDigit.R) + unitConvert.unit,
                      "L:" + (eObject.LSL * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit + " U:" + (eObject.USL * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit,
                      "Now:" + (arc.r * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  break;
                }



              case SHAPE_TYPE.measure_subtype.calc:
                {
                  this.drawpoint(ctx, eObject.pt1);


                  let fontPx = this.getFontHeightPx();
                  ctx.font = this.getFontStyle(1);




                  ctx.save();
                  ctx.translate(eObject.pt1.x, eObject.pt1.y);
                  
                  ctx.strokeStyle = "black";
                  if (eObject.inspection_value !== undefined) {

                    let marginPC = (eObject.inspection_value > eObject.value) ?
                      (eObject.inspection_value - eObject.value) / (eObject.USL - eObject.value) :
                      -(eObject.inspection_value - eObject.value) / (eObject.LSL - eObject.value);
                      
                    this.drawInspMeasureInfoText(ctx,
                      eObject.name,
                      "C" + (eObject.inspection_value),
                      marginPC,fontPx);
                    measureValue=eObject.inspection_value;
                  }
                  else {

                    

                    function ExpCalcBasic(postExp_,funcSet,fallbackFunctionSet) {
                      let postExp=postExp_.filter(exp=>exp!="$")
                      funcSet = {
                        min$: arr => Math.min(...arr),
                        max$: arr => Math.max(...arr),
                        "$+$": vals => vals[0] + vals[1],
                        "$-$": vals => vals[0] - vals[1],
                        "$*$": vals => vals[0] * vals[1],
                        "$/$": vals => vals[0] / vals[1],
                        "$^$": vals => Math.pow(vals[0] , vals[1]),
                        "$": vals => vals,
                        "$,$": vals => vals,
                        "$,$,$": vals => vals,
                        "$,$,$,$": vals => vals,
                        "$,$,$,$,$": vals => vals,
                        "$,$,$,$,$,$": vals => vals,
                        ...funcSet,
                        default:fallbackFunctionSet
                        //default:_=>false
                      };

                      return PostfixExpCalc(postExp, funcSet)[0];
                    }
                    let totalValueList = [...subShapeValues,...measureValueCache];

                    measureValue=ExpCalcBasic(
                      eObject.calc_f.post_exp,
                      totalValueList.reduce((set,ele)=>{
                        set["["+ele.id+"]"]=ele.value;
                        return set;
                      },{}));
                    if(measureValue===undefined)
                      measureValue=NaN;
                    //console.log(measureValueCache,eObject,measureValue);
                    this.drawDefMeasureInfoText(ctx,
                      eObject.name,
                      "C" + eObject.value.toFixed(this.fixedDigit._),
                      "L:" + eObject.LSL.toFixed(this.fixedDigit._) + " U:" + eObject.USL.toFixed(this.fixedDigit._),
                      "Now:" +measureValue.toFixed(this.fixedDigit._),
                      fontPx);
                    

                    // PostfixExpCalc(eObject.post_exp,,,,);
                    // text = "Now:" + ">>";//(Math.hypot(point.x-point_on_line.x,point.y-point_on_line.y)*unitConvert.mult).toFixed(3)+unitConvert.unit;
                    // this.draw_Text(ctx, text, fontPx, eObject.pt1.x, eObject.pt1.y + Y_offset);



                  }
                  ctx.restore();

                  break;
                }
            }
            measureValueCache.push({
              id:eObject.id,
              obj:eObject,
              value:measureValue
            })
          }
          break;
      }
    });
    return measureValueCache;
  }





  drawInspectionShapeList(ctx, eObjects, ShapeColor = undefined, skip_id_list = [], shapeList, unitConvert = { unit: "mm", mult: 1 }, drawSubObjs = false,inFullDisplay=true) {
    let normalRenderGroup = [];
    eObjects.forEach((eObject) => {
      if (eObject == null) return;
      var found = skip_id_list.find(function (skip_id) {
        return eObject.id == skip_id;
      });
      if (found !== undefined) {
        return;
      }
      else {
        if (ShapeColor !== undefined && ShapeColor !== null)
          ctx.strokeStyle = ShapeColor;
        else
          ctx.strokeStyle = eObject.color;
      }
      switch (eObject.type) {
        case SHAPE_TYPE.line:
          {
            ctx.lineWidth = this.getIndicationLineSize();;
            this.drawReportLine(ctx, {
              x0: eObject.pt1.x, y0: eObject.pt1.y,
              x1: eObject.pt2.x, y1: eObject.pt2.y,
            });
          }
          break;


        case SHAPE_TYPE.arc:
          {
            //ctx.strokeStyle=eObject.color; 
            let arc = threePointToArc(eObject.pt1, eObject.pt2, eObject.pt3);
            ctx.lineWidth = this.getIndicationLineSize();
            this.drawReportArc(ctx, arc);


          }
          break;

        case SHAPE_TYPE.search_point:
          {
            ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
            this.drawcross(ctx, eObject.pt1, this.getPointSize()*3);

            ctx.lineWidth = this.getIndicationLineSize();
          }
          break;
        case SHAPE_TYPE.aux_point:
          {

            let db_obj = this.db_obj;
            let subObjs = eObject.ref
              .map((ref) => db_obj.FindShape("id", ref.id, shapeList))
              .map((idx) => { return idx >= 0 ? shapeList[idx] : null });

            if (eObject.id === undefined) break;

            ctx.lineWidth = this.getIndicationLineSize();
            let point = this.db_obj.auxPointParse(eObject, shapeList);
            if (point !== undefined && subObjs.length == 2) {//Draw crosssect line
              ctx.setLineDash([this.getPrimitiveSize(), this.getPrimitiveSize()]);
              
              ctx.beginPath();
              ctx.moveTo(point.x, point.y);
              let closestPt=closestPointOnPoints(point,[subObjs[0].pt1,subObjs[0].pt2]);
              ctx.lineTo(closestPt.x,closestPt.y);
              ctx.stroke();

              ctx.beginPath();
              ctx.moveTo(point.x, point.y);
              closestPt=closestPointOnPoints(point,[subObjs[1].pt1,subObjs[1].pt2]);
              ctx.lineTo(closestPt.x,closestPt.y);
              ctx.stroke();
              ctx.setLineDash([]);
              ctx.strokeStyle = "gray";
              this.drawcross(ctx, point, this.getPointSize()*2);
            }
          }
          break;

        case SHAPE_TYPE.measure:
          normalRenderGroup.push(eObject);
          break;
      }
    });

    this.drawShapeList(ctx, normalRenderGroup, ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);

  }

  drawImageBoundaryGrid(ctx,imgInfo={
    offsetX:0,
    offsetY:0,
    width:undefined,
    height:undefined,
  },extendL=1000)
  {
    if(imgInfo.width===undefined||imgInfo.height===undefined)
    {

      // this.drawLine(ctx, {
      //   pt1:{x:-extendL,y:0},
      //   pt2:{x:extendL,y:0},
      // },offset)
  
      // this.drawLine(ctx, {
      //   pt1:{x:0,y:extendL},
      //   pt2:{x:0,y:-extendL},
      // },offset)

      return;
    }

    this.drawLine(ctx, {
      pt1:{x:-extendL+0,y:0},
      pt2:{x:+extendL+imgInfo.width,y:0},
    })
      
    this.drawLine(ctx, {
      pt1:{x:-extendL+0,y:imgInfo.height},
      pt2:{x:+extendL+imgInfo.width,y:imgInfo.height},
    })


    this.drawLine(ctx, {
      pt1:{x:0,y:-extendL+0},
      pt2:{x:0,y:+extendL+imgInfo.height},
    })
      
    this.drawLine(ctx, {
      pt1:{x:imgInfo.width,y:-extendL+0},
      pt2:{x:imgInfo.width,y:+extendL+imgInfo.height},
    })

    // ctx.beginPath();
    // ctx.moveTo(-10000,0);
    // ctx.lineTo( 10000,0);
    // ctx.closePath();
    
    // ctx.beginPath();
    // ctx.moveTo(0,-10000);
    // ctx.lineTo(0,10000);
    // ctx.closePath();
    // ctx.stroke();


  }
}


class EverCheckCanvasComponent_proto {

  getMousePos(canvas, evt) {
    var rect = canvas.getBoundingClientRect();
    let mouse = {
      x: evt.clientX - rect.left,
      y: evt.clientY - rect.top
    };
    return mouse;
  }

  copyTouchPositionInfo(e) {
    let touches = [];

    Object.keys(e.touches).forEach(key => {
      let t = e.touches[key];
      if (key !== "length") {
        touches.push({
          clientX: t.clientX,
          clientY: t.clientY,
          force: t.force,
          identifier: t.identifier,
          pageX: t.pageX,
          pageY: t.pageY,
          radiusX: t.radiusX,
          radiusY: t.radiusY,
          rotationAngle: t.rotationAngle,
          screenX: t.screenX,
          screenY: t.screenY,
        });
      }
    });
    return touches;
  }

  constructor(canvasDOM) {
    this.canvas = canvasDOM;

    this.canvas.onmousemove = this.onmousemove.bind(this);
    this.canvas.onmousedown = this.onmousedown.bind(this);
    this.canvas.onmouseup = this.onmouseup.bind(this);
    this.canvas.onmouseout = this.onmouseout.bind(this);


    function getTouchPos(canvasDom, touchEvent) {
      var rect = canvasDom.getBoundingClientRect();
      return {
        x: touchEvent.touches[0].clientX - rect.left,
        y: touchEvent.touches[0].clientY - rect.top
      };
    }

    {

      this.multiTouchInfo = {
        ptouchStatus: [],
        touchStatus: [],
        dragInfo: {},
        pinchInfo: {},

      };


      let touchStatus = (e) => {
        let ti = this.multiTouchInfo;
        ti.ptouchStatus = ti.touchStatus;
        ti.touchStatus = this.copyTouchPositionInfo(e);
        let pTL = ti.ptouchStatus.length, TL = ti.touchStatus.length;

        let touchFStat = pTL * 10 + TL;


        switch (touchFStat)//handle gesture leave
        {
          //leave pinch gesture
          case 20://pinch 2 no touch
          case 21://pinch 2 drag
            this.multiTouchInfo.pinchInfo = {};
            break;

          //leave drag gesture
          case 10://drag 2 no touch 
          case 12://drag 2 pinch
            this.multiTouchInfo.dragInfo = {};
            this.onmouseup(ti.ptouchStatus[0]);
            break;
        }

        switch (touchFStat)//handle gesture motion
        {
          case 11://in drag motion
            this.onmousemove(ti.touchStatus[0]);
            break;

          case 22://in pinch motion
          
            var rect = this.canvas.getBoundingClientRect();
            let pts_pre = this.multiTouchInfo.pinchInfo.pts_cur;
            let pts_cur = ti.touchStatus;
            this.multiTouchInfo.pinchInfo = {
              pts_pre: pts_pre,
              pts_cur: ti.touchStatus
            }

            let scale =
              Math.hypot(pts_cur[0].clientX - pts_cur[1].clientX, pts_cur[0].clientY - pts_cur[1].clientY) /
              Math.hypot(pts_pre[0].clientX - pts_pre[1].clientX, pts_pre[0].clientY - pts_pre[1].clientY);

            let center = {
              x: (pts_pre[0].clientX + pts_pre[1].clientX) / 2-rect.left,
              y: (pts_pre[0].clientY + pts_pre[1].clientY) / 2-rect.top,
            }
            this.scaleCanvas(center, 1, scale);
            break;
        }

        switch (touchFStat)//handle enter new gesture
        {
          //from non drag to drag gesture
          case 1://no touch 2 drag
          case 21://pinch 2 drag
            this.multiTouchInfo.dragInfo = {
              start: ti.touchStatus[0]
            }
            this.onmousedown(ti.touchStatus[0]);
            break;

          //from non pinch to pinch gesture
          case 2://no touch 2 pinch
          case 12://drag to pinch
            this.multiTouchInfo.pinchInfo = {
              pts_cur: ti.touchStatus,
              pts_start: ti.touchStatus
            }
            break;
        }

        //console.log(ti);
      }
      this.canvas.addEventListener("touchstart", (e) => {
        touchStatus(e);
      }, false);
      this.canvas.addEventListener("touchmove", (e) => {
        touchStatus(e);
        e.preventDefault();
      }, false);
      this.canvas.addEventListener("touchend", (e) => {
        touchStatus(e);
      }, false);

    }


    this.canvas.addEventListener('wheel', this.onmouseswheel.bind(this), false);

    this.mouseStatus = { x: -1, y: -1, px: -1, py: -1, status: 0, pstatus: 0 };

    this.secCanvas_rawImg = null;

    this.secCanvas = document.createElement('canvas');

    this.identityMat = new DOMMatrix();
    this.Mouse2SecCanvas = new DOMMatrix();
    this.camera = new CameraCtrl();
    this.camera.Scale(100);
    this.colorSet = {
      unselected: "rgba(100,0,100,0.5)",
      inspection_Pass: "rgba(0,255,0,0.1)",
      inspection_Fail: "rgba(255,0,0,0.1)",
      editShape: "rgba(255,0,0,0.7)",
      measure_info: "rgba(128,128,200,0.7)"
    };

    this.rUtil = new renderUTIL(null, this.camera);
    this.rUtil.setColorSet(this.colorSet);


    this.debounce_zoom_emit = this.throttle(this.zoom_emit.bind(this), 100);
  }


  zoomToCurSignature()
  {
    let center = { x: 0, y: 0 };
    this.camera.SetOffset({
      x: -center.x,
      y: -center.y
    });
    
  }

  resourceClean() {
    this.canvas.removeEventListener('wheel', this.onmouseswheel.bind(this));
    log.debug("resourceClean......")
  }

  SetImg(img_info) {
    if (img_info == null || img_info == this.img_info) return;
    //this.zoomToCurSignature();
    // console.log(img_info);
    this.img_info = img_info;
    let img = img_info.img;
    this.secCanvas.width = img.width;
    this.secCanvas.height = img.height;
    this.secCanvas_rawImg = img;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(img, 0, 0);

    log.debug("SetImg::: UPDATE", ctx2nd);
  }

  

  scaleImageToFitScreen(img_info=this.img_info) {
    if(img_info===undefined)return;
    let mmpp = this.rUtil.get_mmpp();
    console.log(img_info.scale*img_info.width*mmpp);

    let curScale = this.camera.GetCameraScale();
    this.camera.Scale(1/curScale);
    this.camera.Scale(this.canvas.width/(img_info.scale*img_info.width*mmpp));
    
    console.log(this.canvas.width,(img_info.scale*img_info.width*mmpp));
    this.camera.SetOffset({ x: -(img_info.scale*img_info.width*mmpp)/2, y: -(img_info.scale*img_info.height*mmpp)/2 });
  }

  debounce(func, wait, immediate) {
    var timeout;
    return function () {
      var context = this, args = arguments;
      var later = function () {
        timeout = null;
        if (!immediate) func.apply(context, args);
      };
      var callNow = immediate && !timeout;
      clearTimeout(timeout);
      timeout = setTimeout(later, wait);
      if (callNow) func.apply(context, args);
    };
  };

  throttle(func, wait, options) {
    var context, args, result;
    var timeout = null;
    var previous = 0;
    if (!options) options = {};
    var later = function () {
      previous = options.leading === false ? 0 : Date.now();
      timeout = null;
      result = func.apply(context, args);
      if (!timeout) context = args = null;
    };
    return function () {
      var now = Date.now();
      if (!previous && options.leading === false) previous = now;
      var remaining = wait - (now - previous);
      context = this;
      args = arguments;
      if (remaining <= 0 || remaining > wait) {
        if (timeout) {
          clearTimeout(timeout);
          timeout = null;
        }
        previous = now;
        result = func.apply(context, args);
        if (!timeout) context = args = null;
      } else if (!timeout && options.trailing !== false) {
        timeout = setTimeout(later, remaining);
      }
      return result;
    };
  };

  zoom_emit() {
    // return this.zoom_full();
    let cW=this.canvas.width;
    let cH=this.canvas.height;

    // let maxEdge=cW>cH?cW:cH;
    // cW=cH=maxEdge;


    let alpha = .2;
    let ViewPort_expand= 0;
    let ViewPortX = -cW * alpha-ViewPort_expand;
    let ViewPortY = -cH * alpha-ViewPort_expand;
    let ViewPortW = cW - 2 * ViewPortX;
    let ViewPortH = cH - 2 * ViewPortY;
    let totalScale = this.camera.GetCameraScale();
    let offset = this.camera.GetCameraOffset();



    let crop = [
      (ViewPortX - offset.x - cW / 2) / totalScale,
      (ViewPortY - offset.y - cH / 2) / totalScale,
      ViewPortW / totalScale,
      ViewPortH / totalScale];
    let down_samp_level = 1.0 * crop[2] / (cW)*0.3;
    this.EmitEvent(
      {
        type: "asdasdas",
        data: {
          down_samp_level,
          crop
        }
      }
    );
  }

  zoom_full() {

    let crop = [0,0,999999,999999];
    let down_samp_level = 0.001;
    this.EmitEvent(
      {
        type: "asdasdas",
        data: {
          down_samp_level,
          crop
        }
      }
    );
  }
  onmouseswheel(evt) {
    //
    let ret_val = this.scaleCanvas(this.mouseStatus, evt.deltaY / 4);
    this.debounce_zoom_emit();
    return ret_val;
  }

  scaleCanvas(scaleCenter, deltaY, scale = 1 / 1.01) {
    if (deltaY > 50) deltaY = 1;//Windows scroll hack => only 100 or -100
    if (deltaY < -50) deltaY = -1;

    scale = Math.pow(scale, deltaY);

    this.camera.Scale(scale,
      {
        x: (scaleCenter.x - (this.canvas.width / 2)),
        y: (scaleCenter.y - (this.canvas.height / 2))
      });

    this.draw();

    return false;
  }

  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = false;

    let doDraw=true;
    //console.log("this.state.substate:",this.state.substate);
    
    // 
    switch (this.state.substate) {
      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        doDraw=true;
        if (this.EditPoint != null) break;
      case UI_SM_STATES.DEFCONF_MODE_NEUTRAL:
      case UI_SM_STATES.INSP_MODE_NEUTRAL:
        doDragging = true;
        break;
      
      case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
        doDraw=true;
        break;
    }

    if (this.state.state === UI_SM_STATES.MAIN) {
      doDragging = true;
    }

    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        
        doDraw=true;
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });
      }

    }
    if(doDraw)
    {
      this.ctrlLogic();
      this.draw();
    }
  }

  onmousedown(evt) {

    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.px = pos.x;
    this.mouseStatus.py = pos.y;
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 1;

    this.ctrlLogic();
    this.draw();
  }

  onmouseup(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 0;
    this.camera.EndDrag();

    this.debounce_zoom_emit();
    this.ctrlLogic();
    this.draw();
  }
  onmouseout(evt) {
    if (this.mouseStatus.status == 1) {
      this.onmouseup(evt);
    }
  }


  resize(width, height) {
    if(Math.abs(this.canvas.height - height)+Math.abs(this.canvas.width - width)<5)return;
    this.canvas.width = width;
    this.canvas.height = height;
    //this.ctrlLogic();
    this.draw();
  }



  worldTransform() {
    let wMat = new DOMMatrix();

    wMat.translateSelf((this.canvas.width / 2), (this.canvas.height / 2));
    this.camera.CameraTransform(wMat);
    //let center = this.getReportCenter();
    //wMat.translateSelf(-center.x, -center.y);

    return wMat;

  }



  VecX2DMat(vec, mat) {

    let XX = vec.x * mat.a + vec.y * mat.c + mat.e;
    let YY = vec.x * mat.b + vec.y * mat.d + mat.f;
    return { x: XX, y: YY };
  }
}


class Preview_CanvasComponent extends EverCheckCanvasComponent_proto {


  constructor(canvasDOM) {
    super(canvasDOM);
    this.edit_DB_info = null;
    this.db_obj = null;
    this.mouse_close_dist = 10;



    this.state = undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;


    this.EditShape = null;
    this.CandEditPointInfo = null;
    this.EditPoint = null;
    this.ShowInspectionNote = false;

    this.EmitEvent = (event) => { log.debug(event); };
  }

  SetShowInspectionNote(doShow=false)
  {
    this.ShowInspectionNote=doShow;
  }
  SetState(state) {
    log.debug(state);
    let stateObj = xstate_GetCurrentMainState(state);
    let stateStr = JSON.stringify(stateObj);
    if (JSON.stringify(this.state) === stateStr) return;

    this.state = JSON.parse(stateStr);
    
    if (
      this.state.state == UI_SM_STATES.DEFCONF_MODE &&
      this.state.substate == UI_SM_STATES.DEFCONF_MODE_NEUTRAL) {
      this.EmitEvent(DefConfAct.Edit_Tar_Update(null));
      this.EditShape = null;
      this.EditPoint = null;
    }

  }


  scaleImageToFitScreen(img_info=this.img_info) {
    if(img_info===undefined)return;
    let mmpp = this.rUtil.get_mmpp();
    let magArr = GetObjElement(this.edit_DB_info.inherentShapeList,[0,"signature","magnitude"]);
    


    let minCanvasWH=this.canvas.width<this.canvas.height?this.canvas.width:this.canvas.height;
    let maxSig=(magArr===undefined)?
      img_info.scale*img_info.width*mmpp:
      magArr.reduce((max,mag)=>mag>max?mag:max,1)*2*1.1;
    // maxSig=img_info.width*mmpp;
    // console.log(maxSig,maxSig/mmpp,img_info.width*mmpp,img_info.width);


    let curScale = this.camera.GetCameraScale();
    this.camera.Scale(1/curScale);
    this.camera.Scale(minCanvasWH/maxSig);

    
    let center = this.db_obj.getsig360infoCenter();

    // console.log(this.canvas.width,(img_info.scale*img_info.width*mmpp));
    this.camera.SetOffset({ x: -center.x, y: -center.y });
  }
  EditDBInfoSync(edit_DB_info) {
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    if (this.db_obj === undefined || this.db_obj == null || this.db_obj.cameraParam === undefined) return;
    this.rUtil.setEditor_db_obj(this.db_obj);
    let imageChanged=edit_DB_info.img!=this.img_info;
    this.SetImg(edit_DB_info.img);
    
    let mmpp = this.db_obj.getsig360info_mmpp();
    this.rUtil.renderParam.mmpp = mmpp;
    if(imageChanged)
      this.scaleImageToFitScreen();

  }



  SetImg(img_info) {
    if(img_info==null)return;
    // if(this.img_info==img_info)return;
    super.SetImg(img_info);
  }
  ctrlLogic() {
  }

  setMatrix(ctx, matrix) {

    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);
  }

  draw() {
    if (this.db_obj === undefined || this.db_obj == null || this.db_obj.cameraParam === undefined) return;
    

    let mmpp = this.rUtil.get_mmpp();


    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);
  
    {
      //TODO:HACK: 4X4 times scale down for transmission speed
      ctx.save();
      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;

      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;
      ctx.scale(scale * mmpp, scale * mmpp);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX) / scale-0.5, (this.img_info.offsetY) / scale-0.5);
      }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      //ctx.translate(-1 * scale * mmpp, -1 * mmpp_mult);
      
      
      ctx.drawImage(this.secCanvas, 0, 0);
      
      ctx.strokeStyle = "rgba(120, 120, 120,30)";
      let curScale=this.camera.GetCameraScale();
      ctx.lineWidth = 200/curScale/scale;
      this.rUtil.drawImageBoundaryGrid(ctx,this.img_info,100000/curScale);
      ctx.restore();
    }
    

    if(this.ShowInspectionNote!=true)
      return
    let center = this.db_obj.getsig360infoCenter();
    ctx.translate(center.x, center.y);




    
    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    }

    {


      this.Mouse2SecCanvas = matrix.invertSelf();
      let invMat = this.Mouse2SecCanvas;
      //this.Mouse2SecCanvas = invMat;
      let mPos = this.mouseStatus;
      let mouseOnCanvas2 = this.VecX2DMat(mPos, invMat);

    }
    ctx.closePath();
    ctx.save();


    let skipDrawIdxs = [];

    this.rUtil.drawShapeList(ctx, this.edit_DB_info.list, null, skipDrawIdxs, this.edit_DB_info.list, unitConvert,false,false);
    this.rUtil.drawInherentShapeList(ctx, this.edit_DB_info.inherentShapeList);

  }

}



class INSP_CanvasComponent extends EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.ERROR_LOCK = false;
    this.edit_DB_info = null;
    this.db_obj = null;
    this.mouse_close_dist = 10;
    this.doImageFitting=true;
    this.colorSet =
      Object.assign(this.colorSet,
        {
          inspection_Pass: "rgba(0,255,0,0.1)",
          inspection_production_Fail: "rgba(128,128,0,0.3)",
          inspection_Fail: "rgba(255,0,0,0.1)",
          inspection_UNSET: "rgba(128,128,128,0.1)",
          inspection_NA: "rgba(64,64,64,0.1)",


          color_NA: "rgba(128,128,128,0.5)",

          color_UNSET: "rgba(128,128,128,0.5)",
          color_SUCCESS: this.colorSet.measure_info,
          color_FAILURE_opt: {
            submargin1: "rgba(255,255,0,0.5)",
          },
          color_FAILURE: "rgba(255,0,0,0.5)",
        }
      );


    this.state = undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;


    this.EditShape = null;
    this.CandEditPointInfo = null;
    this.EditPoint = null;

    this.ROISettingCallBack = undefined;
    this.EmitEvent = (event) => { log.info(event); };
    this.ROISettingInfo=undefined;
  }

  SetROISettingCallBack(callback)
  {
    this.ROISettingCallBack=callback;
    if(this.ROISettingCallBack!==undefined)
    {
      this.ROISettingInfo={
      };
    }
    else
    {
      this.ROISettingInfo=undefined;
    }
  }

  SetMeasureDisplayRank(rank)
  {
    this.measureDisplayRank=rank;
  }
  ScreenCoordTo_mm_pix(coord)
  {
    let mmpp = this.rUtil.get_mmpp();
    //let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mouseOnCanvas = this.VecX2DMat(coord, worldTransform_inv);
    return {
      x_mm:mouseOnCanvas.x,
      y_mm:mouseOnCanvas.y,
      x_pix:mouseOnCanvas.x/mmpp,
      y_pix:mouseOnCanvas.y/mmpp,
    }
  }
  
  onmousemove(evt) 
  {

    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });

        this.ctrlLogic();
        this.draw();
      }

    }


    if(this.ROISettingCallBack!==undefined)
    {
      

      let mm_pix = this.ScreenCoordTo_mm_pix(this.mouseStatus);

      
      this.ROISettingInfo.current={};
      let info = this.ROISettingInfo.current;
      info.mm={
        x:mm_pix.x_mm,
        y:mm_pix.y_mm,
      }
      
      info.pix={
        x:mm_pix.x_pix,
        y:mm_pix.x_pix,
      }
      
      this.draw();
    }


  }

  
  onmousedown(evt) 
  {
    if(this.ROISettingCallBack===undefined)
      super.onmousedown(evt);
    else
    {
      let pos = this.getMousePos(this.canvas, evt);
      
      let mm_pix = this.ScreenCoordTo_mm_pix(pos);
      
      this.ROISettingInfo.start={};
      let info = this.ROISettingInfo.start;
      info.mm={
        x:mm_pix.x_mm,
        y:mm_pix.y_mm,
      }
      
      info.pix={
        x:mm_pix.x_pix,
        y:mm_pix.y_pix,
      }

    }
  }

  onmouseup(evt) 
  {
    if(this.ROISettingCallBack===undefined)
      super.onmouseup(evt);
    else
    {
      let pos = this.getMousePos(this.canvas, evt);
      
      let mm_pix = this.ScreenCoordTo_mm_pix(pos);
      
      this.ROISettingInfo.end={};
      let info = this.ROISettingInfo.end;
      info.mm={
        x:mm_pix.x_mm,
        y:mm_pix.y_mm,
      }
      
      info.pix={
        x:mm_pix.x_pix,
        y:mm_pix.y_pix,
      }

      this.ROISettingCallBack(this.ROISettingInfo);
      this.ROISettingInfo=undefined;
      this.ROISettingCallBack=undefined;

      this.ctrlLogic();
      this.draw();


    }
  }

  SetState(state) {
    log.debug(state);
    let stateObj = xstate_GetCurrentMainState(state);
    let stateStr = JSON.stringify(stateObj);
    if (JSON.stringify(this.state) === stateStr) return;

    this.state = JSON.parse(stateStr);

  }

  EditDBInfoSync(edit_DB_info) {
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg(edit_DB_info.img);
    let mmpp = this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b;
    this.rUtil.renderParam.mmpp = mmpp;

    if(this.doImageFitting!=false)
    {
      
      this.scaleImageToFitScreen();
      this.doImageFitting=false;
    }
  }




  SetShape(shape_obj, id) {
    this.tmp_EditShape_id = id;
    this.EmitEvent(DefConfAct.Shape_Set({ shape: shape_obj, id: id }));
  }



  inspectionResult(objReport) {
    let judgeReports = objReport.judgeReports;
    let ret_status = judgeReports.reduce((res, obj) => {
      if (res == INSPECTION_STATUS.NA || res == INSPECTION_STATUS.UNSET) return res;
      if (res == INSPECTION_STATUS.FAILURE) {
        if (obj.status == INSPECTION_STATUS.NA) return INSPECTION_STATUS.NA;
        return res;
      }
      return obj.status;
    }
      , INSPECTION_STATUS.SUCCESS);

    if (ret_status == undefined) {
      return INSPECTION_STATUS.NA;
    }

    return ret_status;
  }
  rotateVector(vec, ang)
  {
      // not right!
      return {
          x:vec.x * Math.cos(ang) - vec.y * Math.sin(ang),
          y:vec.x * Math.sin(ang) + vec.y * Math.cos(ang)
      };
  };

  draw() {
    this.draw_INSP();
  }
  draw_INSP() {


    let mmpp = this.rUtil.get_mmpp();
    // console.log(">>edit_DB_info>>",this.edit_DB_info );
    if (this.ERROR_LOCK || this.edit_DB_info == null ) {
      return;
    }
    //let inspectionReport = this.edit_DB_info.inspReport;
    //let inspectionReportList = this.edit_DB_info.inspReport.reports;


    let unitConvert;

    unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    };

    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);




    let inspectionReportList = this.edit_DB_info.reportStatisticState.trackingWindow.filter((x) => x.isCurObj);





    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);

    if(false&&inspectionReportList.length==1)
    {
      let line_N=inspectionReportList[0].detectedLines[1];
      let spoint_N=inspectionReportList[0].searchPoints[0];
      
      let centerPt={x:inspectionReportList[0].cx,y:inspectionReportList[0].cx}
      // centerPt={x:(line_N.pt1.x+line_N.pt2.x)/2,y:(line_N.pt1.y+line_N.pt2.y)/2}
      // centerPt={x:(spoint_N.x),y:(spoint_N.y)}
      ctx.translate(centerPt.x,centerPt.y);//Move to the center of the secCanvas
      

      // let rot=inspectionReportList[0].rotate;
      let rot=-Math.atan2(line_N.vy,line_N.vx);

      ctx.rotate(rot);
      if (inspectionReportList[0].isFlipped)
        ctx.scale(1, -1);
      
      // ctx.translate(inspectionReportList[0].cx/mmpp,inspectionReportList[0].cy/mmpp);//Move to the center of the secCanvas
      // console.log(inspectionReportList[0]);
      
      ctx.translate(-centerPt.x,-centerPt.y);//Move to the center of the secCanvas
      
    }
  
    
    
    {//TODO:HACK: 4X4 times scale down for transmission speed


      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX-0.5*(scale)) / scale, (this.img_info.offsetY-0.5*(scale)) / scale);
      }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      ctx.drawImage(this.secCanvas, 0, 0);
      
      ctx.strokeStyle = "rgba(120, 120, 120,30)";
      
      let curScale=this.camera.GetCameraScale();
      ctx.lineWidth = 50/curScale;
      this.rUtil.drawImageBoundaryGrid(ctx,this.img_info,100000/curScale);

      ctx.restore();

      if(this.db_obj.cameraParam.mask_radius!==undefined)
      {
        ctx.save();
        
        ctx.lineWidth = 400/curScale;
        ctx.strokeStyle = "rgba(150, 00, 00,30)";
        ctx.scale(mmpp, mmpp);
        // console.log(this.db_obj.cameraParam.mask_radius);

        ctx.beginPath();
        
        ctx.arc(
          this.img_info.full_width/2,
          this.img_info.full_height/2, 
          this.db_obj.cameraParam.mask_radius/mmpp, 
          0, 2 * Math.PI);


        ctx.stroke();
        ctx.restore();
        
      }


    }
    
    if (true) {
      let sigScale = 1;


      let measureShape = [];
      if (this.edit_DB_info.list !== undefined) {
        measureShape = this.edit_DB_info.list.reduce((measureShape, shape) => {
          if (shape.type == SHAPE_TYPE.measure)
            measureShape.push(shape)
          return measureShape;
        }, []);
      }


    //this.edit_DB_info.inspReport.reports;
      if(true)
      {
        inspectionReportList.forEach((report, idx) => {
          ctx.save();
          ctx.translate(report.cx, report.cy);
          
          // ctx.save();
          // ctx.rotate(-report.rotate);
          // if (report.isFlipped)
          //   ctx.scale(1, -1);
          // this.rUtil.drawSignature(ctx, this.edit_DB_info.inherentShapeList[0].signature, 5);
          // ctx.restore();
          // 
          

          // ctx.scale(sigScale, sigScale);



          // ctx.textAlign = "center";
          // ctx.textBaseline = 'middle';
          // ctx.lineWidth = this.rUtil.renderParam.base_Size * this.rUtil.renderParam.size_Multiplier*0.013;




          // this.rUtil.draw_Text(ctx, idx,1, 0, 0);

          // this.rUtil.drawSignature(ctx, this.edit_DB_info.inherentShapeList[0].signature, 5);

          let ret_res = this.inspectionResult(report);
          //console.log(ret_res, report);
          switch (ret_res) {
            case INSPECTION_STATUS.NA:
              ctx.fillStyle = this.colorSet.inspection_NA;
              break;
            case INSPECTION_STATUS.UNSET:
              ctx.fillStyle = this.colorSet.inspection_UNSET;
              break;
            case INSPECTION_STATUS.SUCCESS:
              {
                ctx.fillStyle = this.colorSet.inspection_Pass;

                {
                  let minViolationIdx = Number.MAX_VALUE;
                  this.edit_DB_info.list.forEach((eObj) => {
                    if (eObj.type === SHAPE_TYPE.measure) {
                      let targetID = eObj.id;
                      let inspMeasureTar = report.judgeReports.find((measure) => (measure.id === targetID));
                      if (inspMeasureTar === undefined) {
                        return;
                      }

                      ctx.fillStyle = MEASURE_RESULT_VISUAL_INFO[inspMeasureTar.detailStatus].COLOR;
                    }
                  });
                }
                //ctx.fillStyle=this.colorSet.inspection_production_Fail;

              }
              break;
            case INSPECTION_STATUS.FAILURE:
              ctx.fillStyle = this.colorSet.inspection_Fail;
              break;

          }
          // let fontPx = this.getFontHeightPx();
          
          
          ctx.strokeStyle = "red";
          this.rUtil.draw_aimcross(ctx, {x:0,y:0});
          // this.rUtil.draw_Text(ctx, idx, this.rUtil.getFontHeightPx(), 0,0);

          {
            let ringR=this.rUtil.getFontHeightPx()*1;
            let ringThickness=this.rUtil.getFontHeightPx()/4;
            ctx.lineWidth = ringThickness;
            ctx.beginPath();

            let startAngle=0, endAngle=2 * Math.PI;
            if(report.headSkipTime>0)
            {
              ctx.strokeStyle = "rgb(255, 0, 0)";
              startAngle=-2 * Math.PI*report.headSkipTime/report.minReportRepeat;
              endAngle=0;
            }
            else
            {
              if(report.repeatTime<report.minReportRepeat)
              {
                ctx.strokeStyle = "rgb(255, 255, 0)";
                endAngle = 2 * Math.PI*report.repeatTime/report.minReportRepeat;
              }
              else if(report.repeatTime<report.minReportRepeat+1)
              {
                ctx.strokeStyle = "rgba(0, 255, 0,0.5)";
              }
              else
              {
                endAngle=0;
              }
            }
            ctx.arc(0, 0,  ringR, startAngle,endAngle);
            ctx.stroke();
          }
          // ctx.fillText(idx, 0, 0);
          // ctx.strokeText(idx, 0, 0);
          // ctx.fill();
          ctx.restore();
          //this.rUtil.drawpoint(ctx, {x:report.cx,y:report.cy},"cross");
        });
      }

    }

    inspectionReportList.forEach((report, idx) => {
      //let ret_res = this.inspectionResult(report);
      //if(ret_res == INSPECTION_STATUS.SUCCESS)
      {
        let listClone = dclone(this.edit_DB_info.list);

        this.db_obj.ShapeListAdjustsWithInspectionResult(listClone, report);

        listClone=listClone.filter(ff=>{
          if(ff.rank===undefined)return true;
          if(ff.rank<=this.measureDisplayRank)return true;
          return false;
        });


        listClone.forEach((eObj) => {
          //log.info(eObj.inspection_status);
          switch (eObj.inspection_status) {
            case INSPECTION_STATUS.NA:
              eObj.color = this.colorSet.color_NA;
              break;
            case INSPECTION_STATUS.UNSET:
              eObj.color = this.colorSet.color_UNSET;
              break;
            case INSPECTION_STATUS.SUCCESS:
              {
                eObj.color = this.colorSet.color_SUCCESS;

                if (eObj.type === SHAPE_TYPE.measure) {
                  let targetID = eObj.id;
                  let inspMeasureTar = report.judgeReports.find((measure) => measure.id === targetID);
                  if (inspMeasureTar === undefined) break;

                  eObj.color = MEASURE_RESULT_VISUAL_INFO[inspMeasureTar.detailStatus].COLOR;

                }

              }
              break;
            case INSPECTION_STATUS.FAILURE:
              eObj.color = this.colorSet.color_FAILURE;
              break;

          }
        });
        
        //console.log(listClone);
        this.rUtil.drawInspectionShapeList(ctx, listClone, null, [], listClone, unitConvert, false);
      }
    });

    
    if(this.ROISettingInfo!==undefined && 
      this.ROISettingInfo.current!==undefined && 
      this.ROISettingInfo.start!==undefined)
    {
      let x=this.ROISettingInfo.start.mm.x;
      let y=this.ROISettingInfo.start.mm.y;
      let w=this.ROISettingInfo.current.mm.x-x;
      let h=this.ROISettingInfo.current.mm.y-y;

      if(w<0){
        x+=w;
        w=-w;
      }
      
      if(h<0){
        y+=h;
        h=-h;
      }
      ctx.beginPath();
      let LineSize = this.rUtil.getIndicationLineSize();
      ctx.setLineDash([LineSize*10,LineSize*3,LineSize*3,LineSize*3]);
      ctx.strokeStyle = "rgba(151, 51, 51,30)";
      ctx.lineWidth = LineSize*2;
      ctx.rect(x, y, w, h);
      ctx.stroke();
      ctx.closePath();
    }
  }

  ctrlLogic() {



    if (
      this.edit_DB_info.inherentShapeList === null ||
      this.edit_DB_info.inherentShapeList === undefined ||
      this.edit_DB_info.inherentShapeList.length == 0) {
      this.ERROR_LOCK = true;
      this.EmitEvent({ type: "ERROR", data: ("Define Config is not valid or the corrupted...") });
      return;
    }
    //let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);

    this.EmitEvent(EV_UI_Canvas_Mouse_Location(mouseOnCanvas2));

  }
}


class DEFCONF_CanvasComponent extends EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.edit_DB_info = null;
    this.db_obj = null;
    this.mouse_close_dist = 20;



    this.state = undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;


    this.EditShape = null;
    this.CandEditPointInfo = null;
    this.EditPoint = null;

    this.EmitEvent = (event) => { log.debug(event); };
  }

  SetState(state) {
    log.debug(state);
    let stateObj = xstate_GetCurrentMainState(state);
    let stateStr = JSON.stringify(stateObj);
    if (JSON.stringify(this.state) === stateStr) return;

    this.state = JSON.parse(stateStr);

    if (
      this.state.state == UI_SM_STATES.DEFCONF_MODE &&
      this.state.substate == UI_SM_STATES.DEFCONF_MODE_NEUTRAL) {
      this.EmitEvent(DefConfAct.Edit_Tar_Update(null));
      this.EditShape = null;
      this.EditPoint = null;
    }

  }

  EditDBInfoSync(edit_DB_info) {
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg(edit_DB_info.img);

    this.SetEditShape(edit_DB_info.edit_tar_info);

    let mmpp = this.db_obj.getsig360info_mmpp();
    this.rUtil.renderParam.mmpp = mmpp;
  }

  SetShape(shape_obj, id) {
    // console.log();
    this.tmp_EditShape_id = id;
    this.EmitEvent(DefConfAct.Shape_Set({ shape: shape_obj, id: id }));
  }

  SetEditShape(EditShape) {
    this.EditShape = EditShape;

    log.debug(this.tmp_EditShape_id);
    if (this.EditShape != null && this.EditShape.id != undefined && this.tmp_EditShape_id != this.EditShape.id) {
      if (this.tmp_EditShape_id != undefined) {
        this.fitCameraToShape(this.EditShape);
      }
      this.tmp_EditShape_id = this.EditShape.id;
    }
  }

  AvailableShapeFilter(shapeList) {
    let type;
    let subtype;

    switch (this.state.substate) {
      case UI_SM_STATES.DEFCONF_MODE_LINE_CREATE:
        type = SHAPE_TYPE.line;
        break;
      case UI_SM_STATES.DEFCONF_MODE_ARC_CREATE:
        type = SHAPE_TYPE.arc;
        break;
      case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
        type = SHAPE_TYPE.aux_point;
        break;
      case UI_SM_STATES.DEFCONF_MODE_AUX_LINE_CREATE:
        type = SHAPE_TYPE.aux_line;
        break;
      case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
        type = SHAPE_TYPE.search_point;
        break;
      case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
        type = SHAPE_TYPE.measure;
        if (this.EditShape !== undefined && this.EditShape !== null)
          subtype = this.EditShape.subtype;
        break;

      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:

        let tar_ele_trace = this.edit_DB_info.edit_tar_ele_trace;
        let edit_tar_info = this.edit_DB_info.edit_tar_info;
        if (edit_tar_info !== undefined && tar_ele_trace !== null) {
          subtype = edit_tar_info.subtype;
          type = edit_tar_info.type;
        }
        break;
    }
    switch (type) {
      case SHAPE_TYPE.line:
      case SHAPE_TYPE.arc:
        return [];
        break;
      case SHAPE_TYPE.aux_point:
        return shapeList.filter((shape) =>
          shape.type === SHAPE_TYPE.line ||
          shape.type === SHAPE_TYPE.search_point
        );
        break;
      case SHAPE_TYPE.aux_line:
        return shapeList.filter((shape) =>
          shape.type === SHAPE_TYPE.arc ||
          shape.type === SHAPE_TYPE.search_point
        );
        break;
      case SHAPE_TYPE.search_point:
        return shapeList.filter((shape) => shape.type === SHAPE_TYPE.line);
        break;
      case SHAPE_TYPE.measure:
        switch (subtype) {
          case SHAPE_TYPE.measure_subtype.distance:
            return shapeList.filter((shape) => shape.type !== SHAPE_TYPE.measure);
            break;
          case SHAPE_TYPE.measure_subtype.angle:
            return shapeList.filter((shape) => shape.type === SHAPE_TYPE.line || shape.type === SHAPE_TYPE.search_point);
            break;
          case SHAPE_TYPE.measure_subtype.radius:
            return shapeList.filter((shape) => shape.type === SHAPE_TYPE.arc);
            break;
          case SHAPE_TYPE.measure_subtype.calc:
            return shapeList.filter((shape) => shape.type === SHAPE_TYPE.measure);
            break;
        }
        break;
    }
    return shapeList;
  }

  fitCameraToShape(shape) {
    if (shape == null || shape === undefined) return;
    let center = { x: 0, y: 0 };
    switch (shape.type) {
      case SHAPE_TYPE.line:
        center.x = (shape.pt1.x + shape.pt2.x) / 2;
        center.y = (shape.pt1.y + shape.pt2.y) / 2;
        break;
      case SHAPE_TYPE.arc:
        let arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
        if (arc.r > 500) {
          center.x = (shape.pt1.x + shape.pt3.x) / 2;
          center.y = (shape.pt1.y + shape.pt3.y) / 2;
        }
        else {
          center.x = arc.x;
          center.y = arc.y;
        }

        break;
      case SHAPE_TYPE.aux_point:
        let pt = this.db_obj.auxPointParse(shape);
        if (pt == null) return;
        center = pt;
        break;
      case SHAPE_TYPE.search_point:
        {
          center = shape.pt1;
        }
        break;
      default:
        return;
    }

    this.camera.SetOffset({
      x: -center.x,
      y: -center.y
    });
  }


  SetImg(img_info) {
    if(img_info==null)return;
    if(this.img_info==img_info)return;
    super.SetImg(img_info);

    // this.scaleImageToFitScreen();
    // this.camera.SetOffset({ x:0, y: 0});
  
  }


  draw() {
    this.draw_DEFCONF();
  }
  ctrlLogic() {
    this.ctrlLogic_DEFCONF();
  }

  setMatrix(ctx, matrix) {

    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);
  }

  draw_DEFCONF() {

    let mmpp = this.rUtil.get_mmpp();
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    this.setMatrix(ctx, this.identityMat);
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix = this.worldTransform();
    this.setMatrix(ctx, matrix);

    {
      let center = this.db_obj.getsig360infoCenter();
      ctx.save();
      ctx.translate(-center.x, -center.y);

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;

      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;
      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX-0.5*(scale)) / scale, (this.img_info.offsetY-0.5*(scale)) / scale);
      }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      //ctx.translate(-1 * scale * mmpp, -1 * mmpp_mult);
      
      
      ctx.drawImage(this.secCanvas, 0, 0);

      
      ctx.strokeStyle = "rgba(120, 120, 120,30)";
      let curScale=this.camera.GetCameraScale();
      ctx.lineWidth = 200/curScale/scale;
      this.rUtil.drawImageBoundaryGrid(ctx,this.img_info,100000/curScale);
      ctx.restore();
    }

    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    }

    {


      this.Mouse2SecCanvas = matrix.invertSelf();
      let invMat = this.Mouse2SecCanvas;
      //this.Mouse2SecCanvas = invMat;
      let mPos = this.mouseStatus;
      let mouseOnCanvas2 = this.VecX2DMat(mPos, invMat);

    }
    ctx.closePath();
    ctx.save();

    let displayShape = this.AvailableShapeFilter(this.edit_DB_info.list);

    let drawFocusItem = false;
    let skipDrawIdxs = [];
    if (this.EditShape != null) {

      drawFocusItem = true;
      skipDrawIdxs.push(this.EditShape.id);

      ctx.strokeStyle = this.colorSet.editShape;
      this.rUtil.drawShapeList(ctx, [this.EditShape], this.colorSet.editShape, [], this.edit_DB_info.list, unitConvert, true,true);
    }

    if (this.CandEditPointInfo != null) {
      drawFocusItem = true;
      var candPtInfo = this.CandEditPointInfo;
      var found = skipDrawIdxs.find(function (skip_id) {
        return candPtInfo.shape.id == skip_id;
      }.bind(this));

      if (found === undefined) {
        skipDrawIdxs.push(candPtInfo.shape.id);
        let dcolor = "rgba(255,0,255,0.5)"
        ctx.strokeStyle = dcolor;
        this.rUtil.drawShapeList(ctx, [candPtInfo.shape], dcolor, [], this.edit_DB_info.list, unitConvert, true,true);
      }
    }

    if (displayShape != this.edit_DB_info.list)//draw all
    {
      this.rUtil.drawShapeList(ctx, displayShape, null, skipDrawIdxs, this.edit_DB_info.list, unitConvert,false,false);
    }
    else if (!drawFocusItem) {
      this.rUtil.drawShapeList(ctx, this.edit_DB_info.list, null, skipDrawIdxs, this.edit_DB_info.list, unitConvert,false,false);
    }

    this.rUtil.drawInherentShapeList(ctx, this.edit_DB_info.inherentShapeList);


    if (this.EditPoint != null) {
      //ctx.lineWidth=3*this.rUtil.getPrimitiveSize();
      ctx.strokeStyle = "green";
      this.rUtil.drawpoint(ctx, this.EditPoint, 2 * this.rUtil.getPointSize());
    }



    if (this.CandEditPointInfo != null) {
      //ctx.lineWidth=3*this.rUtil.getPrimitiveSize();
      ctx.strokeStyle = "rgba(0,255,0,0.3)";
      this.rUtil.drawpoint(ctx, this.CandEditPointInfo.pt, 2 * this.rUtil.getPointSize());
    }



  }

  round_number_to_significant(number,after_significant=0,countMax=10)
  {
    let idx_N=1.0;
    let count=0;
    for(idx_N=1.0;;idx_N/=10)
    {
      if(idx_N<number)
      {
        break;
      }

      count++;
      if(count>countMax)return 0;
    }

    // console.log(number,count,number.toFixed(count),after_significant);
    //return parseFloat(number.toFixed(count+after_significant));
    return idx_N*10;
  }
  ctrlLogic_DEFCONF() {

    let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);


    switch (this.state.substate) {
      case UI_SM_STATES.DEFCONF_MODE_LINE_CREATE:
        {
          let mmpp_round=this.round_number_to_significant(mmpp);

          if (this.mouseStatus.status == 1) {
            this.EditShape = {
              type: SHAPE_TYPE.line,
              pt1: mouseOnCanvas2,
              pt2: pmouseOnCanvas2,
              margin: mmpp_round*20,
              color: this.colorSet.unselected
            };
          }
          else {
            if (this.EditShape != null && ifOnMouseLeftClickEdge) {
              this.SetShape(this.EditShape);
              this.EmitEvent(DefConfAct.SUCCESS());
            }
          }
          break;
        }

      case UI_SM_STATES.DEFCONF_MODE_ARC_CREATE:
        {

          let mmpp_round=this.round_number_to_significant(mmpp);
          if (this.mouseStatus.status == 1) {
            let cnormal = LineCentralNormal({
              pt1: mouseOnCanvas2,
              pt2: pmouseOnCanvas2,
            });

            this.EditShape = {
              type: SHAPE_TYPE.arc,
              pt1: mouseOnCanvas2,
              pt2: {
                x: cnormal.x + cnormal.vx,
                y: cnormal.y + cnormal.vy
              },
              pt3: pmouseOnCanvas2,
              margin: mmpp_round*20,
              color: this.colorSet.unselected,
              direction: 1
            };
          }
          else {
            if (this.EditShape != null && ifOnMouseLeftClickEdge) {
              this.SetShape(this.EditShape);
              this.EmitEvent(DefConfAct.SUCCESS());
            }
          }
          break;
        }


      case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_AUX_LINE_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
        {
          let mmpp_round=this.round_number_to_significant(mmpp);

          if (this.mouseStatus.status == 1) {
            if (ifOnMouseLeftClickEdge && this.CandEditPointInfo != null) {
              if (this.state.substate == UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE) {

                this.EditShape = {
                  type: SHAPE_TYPE.search_point,
                  pt1: { x: 0, y: 0 },
                  angleDeg: 90,
                  search_far:false,
                  margin: mmpp_round*30,
                  width: mmpp_round*500,
                  ref: [{
                    id: this.CandEditPointInfo.shape.id,
                    element: this.CandEditPointInfo.shape.type
                  }],
                  color: this.colorSet.unselected,
                };

                this.SetShape(this.EditShape);
                this.EmitEvent(DefConfAct.SUCCESS());

              }
              else {
                log.debug(ifOnMouseLeftClickEdge, this.CandEditPointInfo);
                this.EmitEvent(DefConfAct.Edit_Tar_Ele_Cand_Update(this.CandEditPointInfo));
              }
            }
          }
          else {

            let displayShape = this.AvailableShapeFilter(this.edit_DB_info.list);
            let pt_info = this.db_obj.FindClosestCtrlPointInfo(mouseOnCanvas2, displayShape);
            let displayiShape = this.AvailableShapeFilter(this.edit_DB_info.inherentShapeList);
            let pt_info2 = this.db_obj.FindClosestInherentPointInfo(mouseOnCanvas2, displayiShape);
            if (pt_info.dist > pt_info2.dist) {
              pt_info = pt_info2;
            }
            if (pt_info.pt != null && pt_info.dist < this.mouse_close_dist / this.camera.GetCameraScale()) {
              this.CandEditPointInfo = pt_info;
            }
            else {
              this.CandEditPointInfo = null;
            }
          }
          break;
        }


      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        {

          let tar_ele_trace = this.edit_DB_info.edit_tar_ele_trace;

          {
              if ((tar_ele_trace === null || tar_ele_trace === undefined)
              && this.EditShape != null && ifOnMouseLeftClickEdge) {

              this.SetShape(this.EditShape, this.EditShape.id);
            }

            let displayShape = this.AvailableShapeFilter(this.edit_DB_info.list);

            let pt_info = this.db_obj.FindClosestCtrlPointInfo(mouseOnCanvas2, displayShape);

            let displayiShape = this.AvailableShapeFilter(this.edit_DB_info.inherentShapeList);
            let pt_info2 = this.db_obj.FindClosestInherentPointInfo(mouseOnCanvas2, displayiShape);
            if (pt_info.dist > pt_info2.dist) {
              pt_info = pt_info2;
            }

            if (pt_info.pt != null && pt_info.dist < this.mouse_close_dist / this.camera.GetCameraScale()) {
              this.CandEditPointInfo = pt_info;
            }
            else {
              this.CandEditPointInfo = null;
            }
            // console.log(this.CandEditPointInfo);
          }



          if (this.mouseStatus.status == 1) {
            if (ifOnMouseLeftClickEdge) {


              if (tar_ele_trace == null || tar_ele_trace === undefined) {
                //If there is no tar_ele_trace was set,ie. if user didn't select ref
                if (this.CandEditPointInfo != null) {
                  let pt_info = this.CandEditPointInfo;
                  this.CandEditPointInfo = null;
                  this.EditShape = dclone(pt_info.shape);//Deep copy
                  this.EditPoint = this.EditShape[pt_info.key];
                  this.tmp_EditShape_id = this.EditShape.id;
                }
                else {
                  this.EditPoint = null;
                  this.EditShape = null;
                }
                this.EmitEvent(DefConfAct.Edit_Tar_Update(this.EditShape));
              }
              else {
                if (this.CandEditPointInfo != null) {
                  this.EmitEvent(DefConfAct.Edit_Tar_Ele_Cand_Update(this.CandEditPointInfo));
                }
                else {//If there is a edit_tar_ele_trace set, then it will cancel the edit_tar_ele_trace
                  this.EmitEvent(DefConfAct.Edit_Tar_Ele_Trace_Update(null));
                }
              }
            }
            else {
              if (this.EditPoint != null) {
                this.EditPoint.x = mouseOnCanvas2.x;
                this.EditPoint.y = mouseOnCanvas2.y;
                this.EmitEvent(DefConfAct.Edit_Tar_Update(this.EditShape));
              }
            }
          }
          else {
            //the SetShape here is for EditPoint modification, so if the operation is for tar_ele_trace, skip it

          }
          break;
        }
    }
    //ctx.restore();

    this.mouseStatus.pstatus = this.mouseStatus.status;

  }
}





class SLCALIB_CanvasComponent extends EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.ERROR_LOCK = false;
    this.edit_DB_info = null;
    this.db_obj = null;
    this.mouse_close_dist = 10;

    this.colorSet =
      Object.assign(this.colorSet,
        {
          inspection_Pass: "rgba(0,255,0,0.1)",
          inspection_production_Fail: "rgba(128,128,0,0.3)",
          inspection_Fail: "rgba(255,0,0,0.1)",
          inspection_UNSET: "rgba(128,128,128,0.1)",
          inspection_NA: "rgba(64,64,64,0.1)",


          color_NA: "rgba(128,128,128,0.5)",

          color_UNSET: "rgba(128,128,128,0.5)",
          color_SUCCESS: this.colorSet.measure_info,
          color_FAILURE_opt: {
            submargin1: "rgba(255,255,0,0.5)",
          },
          color_FAILURE: "rgba(255,0,0,0.5)",
        }
      );


    this.state = undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;

    this.EmitEvent = (event) => { log.info(event); };
  }

  EditDBInfoSync(edit_DB_info) {
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    this.stage_light_report = edit_DB_info.stage_light_report;
    this.SetImg(edit_DB_info.img);
    let mmpp = this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b;
    this.rUtil.renderParam.mmpp = mmpp;
  }

  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });

        this.ctrlLogic();
        this.draw();
      }

    }
  }





  drawpoint(ctx, point, size = this.getPointSize(), color = "rgba(0,0,100,0.5)") {
    ctx.lineWidth = size * 2;
    ctx.strokeStyle = color;

    ctx.beginPath();

    ctx.rect(point.x - size / 2, point.y - size / 2, size, size);
    ctx.stroke();
    ctx.closePath();

  }


  draw() {

    //console.log(this.ERROR_LOCK, this.edit_DB_info);
    if (this.ERROR_LOCK || this.edit_DB_info == null) {
      return;
    }
    //let inspectionReport = this.edit_DB_info.inspReport;
    //let inspectionReportList = this.edit_DB_info.inspReport.reports;


    let inspectionReportList = this.edit_DB_info.reportStatisticState.trackingWindow.filter((x) => x.isCurObj);
    //this.edit_DB_info.inspReport.reports;


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
    {//TODO:HACK: 4X4 times scale down for transmission speed

      let mmpp = this.rUtil.get_mmpp();

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX / scale -0.5), (this.img_info.offsetY) / scale -0.5);
      }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      ctx.drawImage(this.secCanvas, 0, 0);
      ctx.restore();
    }

    {
      let mmpp = this.rUtil.get_mmpp();
      //this.stage_light_report
      let grid_info = GetObjElement(this, ["stage_light_report", "grid_info"]);
      //console.log(this.stage_light_report,grid_info,"<")
      if (grid_info !== undefined) {
        grid_info.forEach(node => {
          //console.log(node.location,"<")
          let pos = {
            x: node.location.x * mmpp,
            y: node.location.y * mmpp,
          }

          let sigma = node.sigma;
          if (sigma > 8)
            sigma = 8;
          this.drawpoint(ctx, pos, this.rUtil.getPointSize() * (2 * sigma),
            "rgb(" + node.mean + "," + node.mean + "," + node.mean + ")")
          //console.log(this.stage_light_report)
        })
      }
    }
    //this.stage_light_report

  }

  ctrlLogic() {

  }
}





class InstInsp_CanvasComponent extends EverCheckCanvasComponent_proto {

  constructor(canvasDOM,cameraParam) {
    super(canvasDOM);
    this.ERROR_LOCK = false;
    this.cameraParam = cameraParam;

    this.markPoints=[];

    this.colorSet =
      Object.assign(this.colorSet,
        {
          inspection_Pass: "rgba(0,255,0,0.1)",
          inspection_production_Fail: "rgba(128,128,0,0.3)",
          inspection_Fail: "rgba(255,0,0,0.1)",
          inspection_UNSET: "rgba(128,128,128,0.1)",
          inspection_NA: "rgba(64,64,64,0.1)",


          color_NA: "rgba(128,128,128,0.5)",

          color_UNSET: "rgba(128,128,128,0.5)",
          color_SUCCESS: this.colorSet.measure_info,
          color_FAILURE_opt: {
            submargin1: "rgba(255,255,0,0.5)",
          },
          color_FAILURE: "rgba(255,0,0,0.5)",
        }
      );

    this.EmitEvent = (event) => { log.info(event); };


    this.removeOneMarkSet=()=>
    {
      if(this.markPoints.length==0)return;
      let removeCount=1;
      // if(this.markPoints.length%2==1)
      // {
      //   removeCount=1;
      // }
      // else
      // {
      //   removeCount=2;
      // }
      this.markPoints.splice(this.markPoints.length-removeCount,removeCount);
    }
  
    this.distanceType=0;
    this.setDistanceType=(type=0)=>
    {
      this.distanceType=type;
      //type 0 is XY distance
      //type 1 is X distance
      //type 2 is Y distance
    }
  
    this.clearMarkSet=()=>
    {
      this.markPoints=[];
    }
  }


  SetImg(img_info) {
    if(img_info==null)return;
    if(this.img_info==img_info)return;
    super.SetImg(img_info);
  
  }

  EditDBInfoSync(edit_DB_info) {
    if(edit_DB_info._obj===undefined)return;
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg(edit_DB_info.img);
    if(this.db_obj.cameraParam!==undefined)
    {
      let mmpp = this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b;
      this.rUtil.renderParam.mmpp = mmpp;
    }
  }

  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    let doDraw=false;
    //console.log("this.state.substate:",this.state.substate);
    
    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        
        doDraw=true;
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });
      }

    }
    if(doDraw)
    {
      this.ctrlLogic();
      this.draw();
    }
  }

  draw() {
    //let inspectionReport = this.edit_DB_info.inspReport;
    //let inspectionReportList = this.edit_DB_info.inspReport.reports;


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
    {//TODO:HACK: 4X4 times scale down for transmission speed

      let mmpp = this.rUtil.get_mmpp();

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX / scale -0.5), (this.img_info.offsetY) / scale -0.5);
      }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      ctx.drawImage(this.secCanvas, 0, 0);
      ctx.restore();
    }


    this.markPoints.map(pt=>{
      this.rUtil.drawpoint(ctx, pt);
    })

    for(let i=0;i<this.markPoints.length-1;i+=2)
    {
      let pt1 = this.markPoints[i];
      let pt2 = this.markPoints[i+1];
      
      ctx.lineWidth = this.rUtil.getPrimitiveSize()*2;
      
      ctx.strokeStyle = new Color(this.colorSet.inspection_Pass).alpha(0.5);
      ctx.setLineDash([this.rUtil.getPrimitiveSize(), this.rUtil.getPrimitiveSize()]);

      let diatance=Math.NaN;
      switch(this.distanceType)
      {
        case 1:
          diatance=Math.abs(pt1.x-pt2.x);
          this.rUtil.drawReportLine(ctx, {
    
            x0:pt1.x, y0: pt2.y,
            x1:pt2.x, y1: pt2.y,
          });
          
          ctx.strokeStyle = new Color(this.colorSet.inspection_NA).alpha(0.5);
          this.rUtil.drawReportLine(ctx, {
            x0:pt1.x, y0: pt1.y,
            x1:pt1.x, y1: pt2.y,
          });
          break;
        case 2:
          diatance=Math.abs(pt1.y-pt2.y);
          this.rUtil.drawReportLine(ctx, {
            x0:pt2.x, y0: pt1.y,
            x1:pt2.x, y1: pt2.y,
          });
          ctx.strokeStyle = new Color(this.colorSet.inspection_NA).alpha(0.5);
          this.rUtil.drawReportLine(ctx, {
            x1:pt1.x, y1: pt1.y,
            x0:pt2.x, y0: pt1.y,
          });
          break;
        
        case 0:
        default:
          diatance=Math.hypot(pt1.x-pt2.x,pt1.y-pt2.y);
          this.rUtil.drawReportLine(ctx, {
    
            x0:pt1.x, y0: pt1.y,
            x1:pt2.x, y1: pt2.y,
          });
          break;
      }

      ctx.setLineDash([]);
      
      
      let fontPx = this.rUtil.getFontHeightPx();
      ctx.font = this.rUtil.getFontStyle(1);
      ctx.strokeStyle = "black";
      ctx.lineWidth = this.rUtil.renderParam.base_Size * this.rUtil.renderParam.size_Multiplier*0.02;

      this.rUtil.draw_Text(ctx, diatance.toFixed(5)+"mm",  fontPx,pt2.x, pt2.y);

    }
    //this.stage_light_report

  }

  
  // onmouseup(evt) {
  //   let pos = this.getMousePos(this.canvas, evt);
  //   this.mouseStatus.x = pos.x;
  //   this.mouseStatus.y = pos.y;
  //   this.mouseStatus.status = 0;
  //   this.camera.EndDrag();

  //   this.debounce_zoom_emit();
  //   this.ctrlLogic();
  //   this.draw();
  // }

  ctrlLogic() {
    
    let mmpp = this.rUtil.get_mmpp();
    let wMat = this.worldTransform();
    //log.debug("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2 = this.VecX2DMat(mPos, worldTransform_inv);

    let pmPos = { x: this.mouseStatus.px, y: this.mouseStatus.py };
    let pmouseOnCanvas2 = this.VecX2DMat(pmPos, worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status != this.mouseStatus.pstatus);
    
    if(ifOnMouseLeftClickEdge )
    {
      if(this.mouseStatus.status==1)
      {
        this.bk_mPos={...mPos};
      }
      else if(Math.hypot(this.bk_mPos.x-mPos.x,this.bk_mPos.y-mPos.y)<5)
      {
        // if(this.markPoints.length>=2)
        // {
        //   this.markPoints=[];
        // }
        this.markPoints.push(mouseOnCanvas2);
        console.log(this.markPoints);
      }
    }
    console.log(mouseOnCanvas2,pmouseOnCanvas2);

    
    this.mouseStatus.pstatus = this.mouseStatus.status;
  }
}






class RepDisplay_CanvasComponent extends EverCheckCanvasComponent_proto {

  constructor(canvasDOM) {
    super(canvasDOM);
    this.ERROR_LOCK = false;
    this.edit_DB_info = null;
    this.db_obj = null;
    this.mouse_close_dist = 10;

    this.colorSet =
      Object.assign(this.colorSet,
        {
          inspection_Pass: "rgba(0,255,0,0.1)",
          inspection_production_Fail: "rgba(128,128,0,0.3)",
          inspection_Fail: "rgba(255,0,0,0.1)",
          inspection_UNSET: "rgba(128,128,128,0.1)",
          inspection_NA: "rgba(64,64,64,0.1)",


          color_NA: "rgba(128,128,128,0.5)",

          color_UNSET: "rgba(128,128,128,0.5)",
          color_SUCCESS: this.colorSet.measure_info,
          color_FAILURE_opt: {
            submargin1: "rgba(255,255,0,0.5)",
          },
          color_FAILURE: "rgba(255,0,0,0.5)",
        }
      );


    this.state = undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;

    this.EmitEvent = (event) => { log.info(event); };
  }

  EditDBInfoSync(edit_DB_info) {
    if(edit_DB_info._obj===undefined)return;
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg(edit_DB_info.img);
    if(this.db_obj.cameraParam!==undefined)
    {
      let mmpp = this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b;
      this.rUtil.renderParam.mmpp = mmpp;
    }
  }

  onmousemove(evt) {
    let pos = this.getMousePos(this.canvas, evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    let doDragging = true;

    if (doDragging) {
      if (this.mouseStatus.status == 1) {
        this.camera.StartDrag({ x: pos.x - this.mouseStatus.px, y: pos.y - this.mouseStatus.py });

        this.ctrlLogic();
        this.draw();
      }

    }

  }

  SetImg(img_info) {
    if(img_info==null)return;
    super.SetImg(img_info);
    console.log(img_info);
    if(img_info.IGNORE_IMAGE_FIT_TO_SCREEN!=true)
      this.scaleImageToFitScreen();
  }


  draw() {
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
    {//TODO:HACK: 4X4 times scale down for transmission speed

      let mmpp = this.rUtil.get_mmpp();

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;

      
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        ctx.translate((this.img_info.offsetX / scale -0.5), (this.img_info.offsetY) / scale -0.5);
      }

      
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      ctx.drawImage(this.secCanvas, 0, 0);
      ctx.restore();
    }


    if(this.edit_DB_info!=null)
    {

      let inspectionReportList = this.edit_DB_info.reportStatisticState.trackingWindow.filter((x) => x.isCurObj);


      inspectionReportList.forEach((report, idx) => 
        {
          let listClone = dclone(this.edit_DB_info.list);
  
          this.db_obj.ShapeListAdjustsWithInspectionResult(listClone, report);
  
          listClone.forEach((eObj) => {
            //log.info(eObj.inspection_status);
            switch (eObj.inspection_status) {
              case INSPECTION_STATUS.NA:
                eObj.color = this.colorSet.color_NA;
                break;
              case INSPECTION_STATUS.UNSET:
                eObj.color = this.colorSet.color_UNSET;
                break;
              case INSPECTION_STATUS.SUCCESS:
                {
                  eObj.color = this.colorSet.color_SUCCESS;
  
                  if (eObj.type === SHAPE_TYPE.measure) {
                    let targetID = eObj.id;
                    let inspMeasureTar = report.judgeReports.find((measure) => measure.id === targetID);
                    if (inspMeasureTar === undefined) break;
  
                    eObj.color = MEASURE_RESULT_VISUAL_INFO[inspMeasureTar.detailStatus].COLOR;
  
                  }
  
                }
                break;
              case INSPECTION_STATUS.FAILURE:
                eObj.color = this.colorSet.color_FAILURE;
                break;
  
            }
          });
          this.rUtil.drawInspectionShapeList(ctx, listClone, null, [], listClone, unitConvert, false);
        });
      //this.stage_light_report
    }

  }

  ctrlLogic() {

  }
}


export default { EverCheckCanvasComponent_proto,
  Preview_CanvasComponent, INSP_CanvasComponent, SLCALIB_CanvasComponent, DEFCONF_CanvasComponent,
  SHAPE_TYPE_COLOR,InstInsp_CanvasComponent,RepDisplay_CanvasComponent}