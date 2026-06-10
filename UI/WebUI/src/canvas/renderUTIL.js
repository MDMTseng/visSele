// Canvas drawing layer extracted from EverCheckCanvasComponent.js. Self-contained:
// takes a db_obj + CameraCtrl + render config; draws to a 2D context. No redux.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { GetObjElement } from 'UTIL/MISC_Util';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import { INSPECTION_STATUS, BPG_ExpCalc } from 'UTIL/BPG_Protocol';
import { mkLog } from "UTIL/logger";
const log = mkLog("canvas.draw");
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
    // Inspection overlay: per-caliper hits on line/arc reports. Toggled at
    // runtime via System_Setting.SHOW_CALIPER_HITS_INSP; mirrored here by
    // EverCheckCanvasComponent before each draw so per-shape drawInspection
    // can gate the overlay without reading redux.
    this.show_caliper_hits = true;
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
  // Draw an iconSet image ONLY when it has actually decoded. drawImage() throws
  // a DOMException ("HTMLImageElement is in the 'broken' state") if the icon
  // failed to load -- which crashed the whole DefConf canvas on deployments
  // where resource/image/*.svg wasn't bundled. A missing decorative icon must
  // degrade to "not drawn", never crash the render.
  drawIcon(ctx, name, x, y, w, h) {
    const img = this.iconSet[name];
    if (img && img.complete && img.naturalWidth > 0) {
      ctx.drawImage(img, x, y, w, h);
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
      // Keystone step 3 — inherent-list draw also per-shape. Unregistered or
      // module-without-drawInherent types are no-ops (legacy aux_line case was
      // already an empty block).
      const mod = getShapeModule(ishape.type);
      if (mod && mod.drawInherent) {
        mod.drawInherent(ctx, ishape, this);
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
      

      // Keystone step 3 COMPLETE — every shape's draw lives in shapes/<type>.js.
      // This is now a single uniform dispatch; unregistered types pass through
      // (nothing drawn). Per-shape opts (e.g. measure's measureValueCache) are
      // included in every call — destructuring at each draw picks what it needs.
      const mod = getShapeModule(eObject.type);
      if (mod && mod.draw) {
        mod.draw(ctx, eObject, this, {
          inFullDisplay, shapeList, next_ShapeColor, skip_id_list,
          unitConvert, drawSubObjs,
          ShapeColor, measureValueCache,
        });
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
      // Keystone step 3 — inspection-mode draw also dispatched per-shape.
      // measure has no drawInspection (it's deferred to the editor-style draw
      // via normalRenderGroup below). Unregistered types pass through.
      if (eObject.type === SHAPE_TYPE.measure) {
        normalRenderGroup.push(eObject);
      } else {
        const mod = getShapeModule(eObject.type);
        if (mod && mod.drawInspection) {
          mod.drawInspection(ctx, eObject, this, { shapeList });
        }
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
