// Canvas drawing layer extracted from EverCheckCanvasComponent.js. Self-contained:
// takes a db_obj + CameraCtrl + render config; draws to a 2D context. No redux.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { GetObjElement } from 'UTIL/MISC_Util';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import { INSPECTION_STATUS, BPG_ExpCalc } from 'UTIL/BPG_Protocol';
import * as log from 'loglevel';
import dclone from 'clone';
import Color from 'color';
import { MEASURE_RESULT_VISUAL_INFO, SHAPE_TYPE_COLOR } from './renderConst';
import { getShapeModule } from 'JSSRCROOT/shapes';

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
      measure_info: "rgba(158,158,200,1)"
    };
    this.fixedDigit={
      D:3,
      R:3,
      A:2,
      C:6,
      _:3
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

    this.iconSet={};

    {
      let image = new Image();
      image.src = "resource/image/antd-compass.svg";
      this.iconSet.compass=image;
    }
    {
      let image = new Image();
      image.src = "resource/image/antd-eye-invisible.svg";
      this.iconSet["eye_invisible"]=image;
    }
  }
  get_mmpp() {
    return this.renderParam.mmpp;
  }
  getPrimitiveSize() {
    return this.renderParam.base_Size * this.renderParam.size_Multiplier/ this.camCtrl.GetCameraScale();
  }

  getPointSize() {
    return this.getPrimitiveSize()*2;
  }
  getIndicationLineSize() {
    return this.getPrimitiveSize()*2;
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
  drawMeasureDistance(ctx, eObject, refObjs, shapeList, unitConvert,measValueAdjStr="") {

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
          "Now:" + (measureValue * unitConvert.mult).toFixed(this.fixedDigit.D) + unitConvert.unit + measValueAdjStr,
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
    ctx.stroke();
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
            // Keystone step 3 — per-shape draw owns its rendering. shapes/line.js
            // gets the renderer (this) for helpers it needs (drawReportLine, drawpoint,
            // getSearchDirectionLineSize). Falls back to legacy inline if the module
            // somehow disappears — defensive, should never trigger.
            const mod = getShapeModule(SHAPE_TYPE.line);
            if (mod && mod.draw) {
              mod.draw(ctx, eObject, this, { inFullDisplay });
            }
          }
          break;

        case SHAPE_TYPE.aux_point:
        case SHAPE_TYPE.aux_line:
        case SHAPE_TYPE.arc:
          {
            // Keystone step 3 — per-shape draw owns its rendering.
            const mod = getShapeModule(eObject.type);
            if (mod && mod.draw) {
              mod.draw(ctx, eObject, this, {
                inFullDisplay, shapeList, next_ShapeColor, skip_id_list,
                unitConvert, drawSubObjs,
              });
            }
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

            
            let imgWH=this.getPointSize()*4;
            let offsetR=this.getPointSize()*5;

            if(eObject.orientation_essential)
            {
              let theta=180*Math.PI/180;
              let compassOffset={x:offsetR*Math.cos(theta),y:offsetR*Math.sin(theta)};
              ctx.drawImage(this.iconSet["compass"], eObject.pt1.x-imgWH/2+compassOffset.x,eObject.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
            }
            
            if(eObject.quality_essential==false)
            {
              let theta=(180+45)*Math.PI/180;
              let compassOffset={x:offsetR*Math.cos(theta),y:offsetR*Math.sin(theta)};
              ctx.drawImage(this.iconSet["eye_invisible"], eObject.pt1.x-imgWH/2+compassOffset.x,eObject.pt1.y-imgWH/2+compassOffset.y,imgWH,imgWH);
              // this.draw_aimcross(ctx, eObject.pt1, this.getPointSize()*3,0.3);
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
            if (eObject.inspection_value !== undefined) {
              if(typeof eObject.inspection_value === 'number'){
                //good
              }
              else
              {
                eObject.inspection_value=Number.NaN;
              }
            }
            let measValueAdjStr = "";
            
            if(eObject.value_A!==undefined && eObject.value_B!==undefined && eObject.value_X!==undefined && eObject.value_Y!==undefined)
            {

              measValueAdjStr+=" "+eObject.value_A+"~"+eObject.value_B+" => "+eObject.value_X+"~"+eObject.value_Y;
            }

            // console.log(eObject);
            switch (eObject.subtype) {
              case SHAPE_TYPE.measure_subtype.distance:
                {
                  measureValue = this.drawMeasureDistance(ctx, eObject, subObjs, shapeList, unitConvert,measValueAdjStr);

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
                      "Now:" + (measureDeg).toFixed(this.fixedDigit.A) + "º" + measValueAdjStr,
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
                      "Now:" + (arc.r * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  break;
                  
                
            
                }


                
              case SHAPE_TYPE.measure_subtype.circle_info:
              
                {
                  ctx.lineWidth = this.getIndicationLineSize();
                  //ctx.strokeStyle=this.colorSet.measure_info; 

                  ctx.font = this.getFontStyle(1);
                  let arc = threePointToArc(subObjs[0].pt1, subObjs[0].pt2, subObjs[0].pt3);

                  /*let lineInfo = {
                    x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                    x1:eObject.pt1.x,y1:eObject.pt1.y,
                  };*/
                  let arrowSize = 3 * this.getPrimitiveSize();
                  this.canvas_arrow(ctx, eObject.pt1.x, eObject.pt1.y, arc.x, arc.y, arrowSize);
                  //this.drawReportLine(ctx, lineInfo);

                  this.drawpoint(ctx, eObject.pt1);

                  let fontPx = this.getFontHeightPx();
                  ctx.font = this.getFontStyle(1);

                  ctx.save();
                  ctx.translate(eObject.pt1.x, eObject.pt1.y);
                  

                  let tagName="CI.";
                  switch(eObject.info_type)
                  {
                    case SHAPE_TYPE._circle_info_type.max_diameter:
                      tagName+="maxD";
                      break;
                      
                    case SHAPE_TYPE._circle_info_type.min_diameter:
                      tagName+="minD";

                      break;
                      
                    case SHAPE_TYPE._circle_info_type.roughness_max:
                      tagName+="roughnessMax";

                      break;
                      
                    case SHAPE_TYPE._circle_info_type.roughness_min:
                      tagName+="roughnessMin";

                      break;
                      
                      
                    case SHAPE_TYPE._circle_info_type.roughness_rmse:
                      tagName+="roughnessRMSE";
                      break;
                    default:
                      tagName+="NA";
                      break;

                  }

                  ctx.strokeStyle = "black";
                  if (eObject.inspection_value !== undefined) {

                    let marginPC = (eObject.inspection_value > eObject.value) ?
                      (eObject.inspection_value - eObject.value) / (eObject.USL - eObject.value) :
                      -(eObject.inspection_value - eObject.value) / (eObject.LSL - eObject.value);
                      
                    this.drawInspMeasureInfoText(ctx,
                      eObject.name,
                      tagName + (eObject.inspection_value * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit,
                      marginPC,fontPx);
                    measureValue=eObject.inspection_value;
                  }
                  else {
            
                    
                    this.drawDefMeasureInfoText(ctx,
                      eObject.name,
                      tagName + eObject.value.toFixed(this.fixedDigit.R) + unitConvert.unit,
                      "L:" + (eObject.LSL * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit + " U:" + (eObject.USL * unitConvert.mult).toFixed(this.fixedDigit.R) + unitConvert.unit,
                      "?" + measValueAdjStr,
                      fontPx);

                    measureValue=arc.r;
                  }
                  ctx.restore();

                  break;
                  
                
            
                }

              break;

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
                      "C" + (eObject.inspection_value).toFixed(this.fixedDigit.C),
                      marginPC,fontPx);
                    measureValue=eObject.inspection_value;
                  }
                  else {

                    let totalValueList = [...subShapeValues,...measureValueCache];

                    measureValue=BPG_ExpCalc(
                      eObject.calc_f.post_exp,
                      totalValueList.reduce((set,ele)=>{
                        set["["+ele.id+"]"]=ele.value;
                        return set;
                      },{}))[0];
                    
                    if(measureValue===undefined)
                      measureValue=NaN;
                    //console.log(measureValueCache,eObject,measureValue);
                    this.drawDefMeasureInfoText(ctx,
                      eObject.name,
                      "C" + eObject.value.toFixed(this.fixedDigit.C),
                      "L:" + eObject.LSL.toFixed(this.fixedDigit.C) + " U:" + eObject.USL.toFixed(this.fixedDigit.C),
                      "Now:" +measureValue.toFixed(this.fixedDigit.C + measValueAdjStr),
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
      
      if(eObject.inspection_status==INSPECTION_STATUS.NA )
      {
        return;
      }
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

export default renderUTIL;
