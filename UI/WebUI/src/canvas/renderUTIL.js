// Canvas drawing layer extracted from EverCheckCanvasComponent.js. Self-contained:
// takes a db_obj + CameraCtrl + render config; draws to a 2D context. No redux.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { GetObjElement } from 'UTIL/MISC_Util';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import { INSPECTION_STATUS, BPG_ExpCalc } from 'UTIL/BPG_Protocol';
import { mkLog } from "UTIL/logger";

// How an NA feature is drawn.
//
// grayscale(1) rather than a grey strokeStyle, because a strokeStyle set out
// here does not survive: every shape module's drawInspection sets its own
// colour as its first act (search_point uses rgba(179,0,0,0.5)). Setting grey
// and calling them produced a RED template for a feature that measured nothing
// -- worse than not drawing it, because red is the colour of a real result.
// The canvas filter applies to whatever the module paints, so the module needs
// no cooperation and cannot override it.
//
// Grey, not a faded version of the shape's own colour: a measured feature and a
// template that could not be measured must never differ only in saturation.
const NA_CANVAS_FILTER = 'grayscale(1) opacity(0.7)';

// A RUNTIME SWITCH, because this is a leak suspect and suspicion is not a
// finding. Chromium renders every draw made under a non-'none' ctx.filter
// through a temporary offscreen surface; on an accelerated canvas that surface
// is GPU memory, and a soak measured the GPU process climbing about 15-16 KB
// per inspection report -- with the JS heap flat and a forced collection
// handing it all back at once, which is what "allocated per draw, swept late"
// looks like. The greying arrived the same day as the measurement, so it is the
// first thing to rule in or out.
//
// window.__NA_FILTER_OFF__ = 1 turns it off without a rebuild, so one binary can
// run both halves of the A/B. Absent means on, which is the shipped behaviour.
function naFilterOn() {
  return !(typeof window !== 'undefined' && window.__NA_FILTER_OFF__);
}
const NA_REASON_COLOR  = 'rgba(255, 210, 60, 0.95)';
// How near the pointer has to be, in SCREEN pixels, for a marker to show its
// reason. Generous: the marker is small and the operator is aiming with a mouse
// on a machine, not a stylus.
const NA_HOVER_RADIUS_PX = 18;
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
    // DIAGNOSTIC SWITCH -- off unless window.__DIAG_NO_ICONS__ is set.
    //
    // These two icons are SVG, and an SVG <img> is not a bitmap: Blink keeps a
    // whole document for it and rasterises that document on demand. Drawing one
    // per frame is therefore a candidate for the node count that swings by
    // thousands while the page creates nothing and mutates nothing. Skipping
    // the draw is the only way to ask the question without changing anything
    // else about the render.
    if (typeof window !== 'undefined' && window.__DIAG_NO_ICONS__) return;
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


  // shapeList is optional and is the INSPECTION-ADJUSTED list when there is
  // one. Without it an inherent shape that resolves through refs falls back to
  // the def, and gets drawn at the taught position while everything around it
  // is at the measured one -- see aux_point.drawInherent.
  drawInherentShapeList(ctx, inherentShapeList, shapeList) {
    if (inherentShapeList === undefined || inherentShapeList == null) return;

    inherentShapeList.forEach((ishape) => {
      if (ishape == null) return;
      // Keystone step 3 — inherent-list draw also per-shape. Unregistered or
      // module-without-drawInherent types are no-ops (legacy aux_line case was
      // already an empty block).
      const mod = getShapeModule(ishape.type);
      if (mod && mod.drawInherent) {
        mod.drawInherent(ctx, ishape, this, { shapeList });
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

  // Every label in the app goes through here, which is why the upright-text fix
  // lives here and nowhere else.
  //
  // The rotate-target view turns the whole world so the part appears in its
  // taught orientation. That is right for the geometry and wrong for the
  // writing: the labels turned with it, and at any real angle a measurement
  // reads sideways or upside down.
  //
  // viewRotation / viewFlip are the transform the canvas applied, set by
  // whoever applied it. Undo exactly that, and only that:
  //
  //   not flipped   world has R(rot)          -> rotate(-rot)
  //   flipped       world has R(-rot) . S     -> scale(1,-1) then rotate(rot)
  //
  // (Canvas post-multiplies, so R(-rot).S.S.R(rot) = I.)
  //
  // screenOffset: (x, y) is an offset that should appear as given ON SCREEN,
  // not in world space.
  //
  // Callers use (x, y) two different ways -- some pass an absolute world
  // position (a point's own coordinates), some pass an offset from an anchor
  // they already translated to, and stack lines by varying y. Counter-rotating
  // the glyphs alone left the second kind leaning: each line upright, the STACK
  // tilted with the view. So the offset is mapped back through the view
  // transform first, and only the callers that mean a screen offset ask for it.
  //
  //   not flipped   screen = R(r)·v        -> v = R(-r)·(x,y)
  //   flipped       screen = R(-r)·S·v     -> v = S·R(r)·(x,y)   (S = diag(1,-1))
  draw_Text(ctx, text, scale, x, y, screenOffset = false) {
    ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.013;
    ctx.save();
    if (screenOffset && (this.viewRotation || this.viewFlip)) {
      const r = this.viewRotation || 0;
      let vx, vy;
      if (this.viewFlip) {
        const c = Math.cos(r), sn = Math.sin(r);
        vx = x * c - y * sn;
        vy = -(x * sn + y * c);
      } else {
        const c = Math.cos(-r), sn = Math.sin(-r);
        vx = x * c - y * sn;
        vy = x * sn + y * c;
      }
      ctx.translate(vx, vy);
    } else {
      ctx.translate(x, y);
    }
    const _r = this.viewRotation || 0;
    if (_r || this.viewFlip) {
      if (this.viewFlip) { ctx.scale(1, -1); ctx.rotate(_r); }
      else ctx.rotate(-_r);
    }
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
      this.draw_Text(ctx, name, fontPx, 0, 0, true);
    
    Y_offset+=fontPx;
    let text="";
    if(this.renderParam.measureInfoText.value==true)
      text = value;
    else
      text = "";

    if(marginPC==marginPC && isFinite(marginPC) && this.renderParam.measureInfoText.showMarginPC==true)
      text += ":" + (marginPC * 100).toFixed(1) + "%";

    this.draw_Text(ctx, text, fontPx, 0, Y_offset, true);
  }

  drawDefMeasureInfoText(ctx,name,value,InfoLU,InfoCurVal,fontPx)
  {

    let Y_offset = 0;


        
    if(this.renderParam.measureInfoText.name==true)
      this.draw_Text(ctx, name, fontPx, 0, 0, true);
    

    if(this.renderParam.measureInfoText.value==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, value, fontPx, 0, Y_offset, true);
    }

    fontPx *=0.7;

    if(this.renderParam.measureInfoText.showLU==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, InfoLU, fontPx, 0, Y_offset, true);
    }


    if(this.renderParam.measureInfoText.showCur==true)
    {
      Y_offset += fontPx;
      this.draw_Text(ctx, InfoCurVal, fontPx, 0, Y_offset, true);
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
    // NA shapes, drawn last so a grey template can never overdraw a real result.
    let naRenderGroup = [];
    eObjects.forEach((eObject) => {
      if (eObject == null) return;

      // NA used to `return` here: the feature vanished from the overlay
      // completely, and with it every clue about what was supposed to be
      // measured. An operator could not tell an NA from a def that never had
      // that feature -- which is the difference between "look at the ROI" and
      // "nothing is wrong".
      //
      // It is drawable because ShapeAdjustsWithInspectionResult now forward-
      // transforms the NA shape too: the def geometry placed on the part that
      // was actually found, i.e. where this WOULD have been measured.
      const isNA = (eObject.inspection_status == INSPECTION_STATUS.NA);
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
        (isNA ? naRenderGroup : normalRenderGroup).push(eObject);
      } else {
        const mod = getShapeModule(eObject.type);
        if (mod && mod.drawInspection) {
          if (isNA) {
            const savedFilter = ctx.filter;
            const useF = naFilterOn();
            if (useF) ctx.filter = NA_CANVAS_FILTER;
            mod.drawInspection(ctx, eObject, this, { shapeList });
            if (useF) ctx.filter = savedFilter;
            // The reason is NOT greyed -- it is the one thing on an NA that
            // should catch the eye.
            this.drawNAReason(ctx, eObject);
          } else {
            mod.drawInspection(ctx, eObject, this, { shapeList });
          }
        }
      }
    });

    this.drawShapeList(ctx, normalRenderGroup, ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
    if (naRenderGroup.length) {
      const savedFilter = ctx.filter;
      const useF = naFilterOn();
      if (useF) ctx.filter = NA_CANVAS_FILTER;
      this.drawShapeList(ctx, naRenderGroup, ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs,inFullDisplay);
      if (useF) ctx.filter = savedFilter;
      naRenderGroup.forEach((o) => this.drawNAReason(ctx, o));
    }

  }

  // The core's reason for an NA, written beside the shape.
  //
  // Here rather than in each shape module: only search_point and aux_point ever
  // printed it, so every other type produced a bare NA -- and the difference
  // between "NA" and "NA because the scan window is off-frame" is the
  // difference between an hour of guessing and a fix.
  drawNAReason(ctx, eObject) {
    if (!eObject || !eObject.na_reason) return;
    const anchor = eObject.pt1 || eObject.pt || eObject.center;
    if (!anchor || !isFinite(anchor.x) || !isFinite(anchor.y)) return;
    // The label convention, not drawText().
    //
    // drawText() sets no font and hard-codes ctx.lineWidth = 1 before
    // strokeText. The canvas here is transformed into MILLIMETRES, so that 1 is
    // a one-millimetre stroke -- about 72 px on this bench -- and the glyphs
    // come out as a single black mass with spikes that covers the part. Every
    // working label in this file instead sets the font to one unit and lets
    // draw_Text apply the size as a scale.
    //
    // search_point.js and aux_point.js each print na_reason with the same
    // drawText call and therefore the same defect. It went unnoticed because
    // na_reason was only ever set for a def missing edge.min_strength; a
    // clipped scan window sets it on ordinary parts, and the bug became the
    // whole screen. Both are removed in favour of this one.
    // A MARKER ALWAYS, THE SENTENCE ONLY ON DEMAND.
    //
    // Drawing the reason on every NA feature does not work at any size. Pinned
    // to the screen it is a wall of text when zoomed out; pinned to the
    // workpiece it is either unreadable or -- as measured on this bench with
    // two NA features a millimetre apart -- two sentences overlapping into
    // "nscangwindowrisIoff-frame". The information is worth having and is worth
    // having RARELY: what an operator needs at a glance is "this one is NA",
    // and the reason only when they go looking.
    //
    // So: a small marker at the anchor, always, screen-constant like every
    // other marker. The sentence when the pointer is on it.
    //
    // The hit test is in SCREEN space, via the live canvas transform. Doing it
    // in world coordinates would need this code to know which frame the shape
    // is in, and that assumption has been wrong twice today.
    const saveFill = ctx.fillStyle, saveStroke = ctx.strokeStyle;
    const saveLW = ctx.lineWidth, saveFont = ctx.font;
    const off = this.getPointSize() * 1.2;
    ctx.fillStyle = NA_REASON_COLOR;
    ctx.strokeStyle = "black";
    ctx.lineWidth = this.getPrimitiveSize() * 0.35;
    ctx.beginPath();
    ctx.arc(anchor.x, anchor.y, this.getPointSize() * 0.9, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    let hovered = false;
    try {
      const h = this.hoverScreen;
      if (h) {
        const m = ctx.getTransform();
        const sx = m.a * anchor.x + m.c * anchor.y + m.e;
        const sy = m.b * anchor.x + m.d * anchor.y + m.f;
        const dx = sx - h.x, dy = sy - h.y;
        hovered = (dx * dx + dy * dy) <= NA_HOVER_RADIUS_PX * NA_HOVER_RADIUS_PX;
      }
    } catch (e) { hovered = false; }

    if (hovered) {
      // Screen-constant, because it is now transient and deliberate: the
      // operator asked for it by pointing at it, so it should be as readable as
      // any other label.
      const fontPx = this.getFontHeightPx();
      ctx.font = this.getFontStyle(1);
      ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier * 0.02;
      ctx.save();
      ctx.translate(anchor.x + off, anchor.y - off);
      this.draw_Text(ctx, eObject.na_reason, fontPx, 0, 0);
      ctx.restore();
    }
    ctx.font = saveFont; ctx.lineWidth = saveLW;
    ctx.strokeStyle = saveStroke; ctx.fillStyle = saveFill;
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
