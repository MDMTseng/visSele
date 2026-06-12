//UIControl

import { UI_SM_STATES, UI_SM_EVENT, SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { getShapeModule } from 'JSSRCROOT/shapes';

import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { xstate_GetCurrentMainState, GetObjElement } from 'UTIL/MISC_Util';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point,
} from 'UTIL/MathTools';


import { INSPECTION_STATUS,BPG_ExpCalc } from 'UTIL/BPG_Protocol';
import { mkLog } from 'UTIL/logger';
const log = mkLog('canvas.ctrl');
import dclone from 'clone';

import Color from 'color';
import { ConsoleSqlOutlined } from '@ant-design/icons';
import { CameraCtrl } from './canvas/CameraCtrl';
import renderUTIL from './canvas/renderUTIL';
import { MEASURE_RESULT_VISUAL_INFO, SHAPE_TYPE_COLOR } from './canvas/renderConst';
export { MEASURE_RESULT_VISUAL_INFO, SHAPE_TYPE_COLOR };

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
    this.canvas.onmousedown = (ev)=>{
      ev.preventDefault();
      // console.log("MD_CB:");
      this.onmousedown(ev);
    };
    this.canvas.onmouseup = (ev)=>{
      ev.preventDefault();

      // console.log("MU_CB:");
      this.onmouseup(ev);
    };
    
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
        e.preventDefault();
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
      measure_info: "rgba(128,128,255,0.7)"
    };

    this.rUtil = new renderUTIL(null, this.camera);
    this.rUtil.setColorSet(this.colorSet);


    this.debounce_zoom_emit = this.throttle(this.zoom_emit.bind(this), 500);
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
    this.img_info = img_info;

    // JPEG path (core a7cd253d, opt-in via GS IMG_STREAMING_JPEG_QUALITY=N):
    // payload is a Blob; decode via createImageBitmap. Order matters here —
    // resizing secCanvas before the bitmap is ready CLEARS it, leaving any
    // draw() that fires during the decode-async-gap with a blank frame
    // (visible as flicker on live streams). Decode FIRST, then atomically
    // swap (resize + drawImage + cache). The previous bitmap stays visible
    // on secCanvas until that swap. Token guards against an older in-flight
    // decode clobbering a newer frame.
    if (img_info.jpegBlob) {
      const w = img_info.width, h = img_info.height;
      const token = (this._jpegToken = (this._jpegToken || 0) + 1);
      createImageBitmap(img_info.jpegBlob).then((bmp) => {
        if (this._jpegToken !== token) { if (bmp.close) bmp.close(); return; }
        // TEMP probe — confirm decoded bitmap intrinsic size matches header.
        if (bmp.width !== w || bmp.height !== h) {
          console.warn("JPEG size mismatch: header="+w+"x"+h+
            " bitmap="+bmp.width+"x"+bmp.height+
            " full="+img_info.full_width+"x"+img_info.full_height+
            " scale="+img_info.scale+" fmt="+img_info.format);
        }
        this.secCanvas.width = bmp.width;
        this.secCanvas.height = bmp.height;
        const ctx2nd = this.secCanvas.getContext('2d');
        ctx2nd.drawImage(bmp, 0, 0);
        this.secCanvas_rawImg = bmp;
        // Decode is async: the draw() that ran synchronously on mount had no
        // image yet, so the canvas stayed blank until a mousemove forced a
        // redraw. Redraw now that secCanvas holds the bitmap.
        if (typeof this.draw === 'function') this.draw();
      }).catch((err) => {
        log.warn("SetImg: JPEG decode failed", err);
      });
      return;
    }

    // Legacy raw RGBA: img is an ImageData.
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
    // console.log(img_info.scale*img_info.width*mmpp);

    let curScale = this.camera.GetCameraScale();
    this.camera.Scale(1/curScale);
    this.camera.Scale(this.canvas.width/(img_info.scale*img_info.width*mmpp));
    
    // console.log(this.canvas.width,(img_info.scale*img_info.width*mmpp));
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

  throttle(func, wait) {

    // Executes the func after delay time.
    let  timerId =  undefined;

    return function () {
      // console.log(timerId,wait);
      if(timerId!==undefined)
        clearTimeout(timerId);
      timerId = setTimeout(()=>{
        // console.log("CALLLLL");
        timerId=undefined;
        func();
      }, wait);
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
    let down_samp_level = 1.0 * crop[2] / (cW);
    this.EmitEvent(
      {
        type: "down_samp_level_update",
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
        type: "down_samp_level_update",
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

    
    // let center = this.db_obj.getsig360infoCenter();

    // console.log(this.canvas.width,(img_info.scale*img_info.width*mmpp));
    // this.camera.SetOffset({ x: -center.x, y: -center.y });
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
    if(this.img_info===undefined)
    {
      return;
    }

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
      // if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
      //   ctx.translate((this.img_info.offsetX) / scale-0.5, (this.img_info.offsetY) / scale-0.5);
      // }
      // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
      //ctx.translate(-1 * scale * mmpp, -1 * mmpp_mult);




      // ALIGN <def>.png TO THE DEF FEATURES.
      // The features live in the reference/init sig360 frame. If this def recorded
      // the saved image's OWN sig360 (def_image_reg, see DefConfUI save), map that
      // image's sig360 onto the reference so its content registers with the
      // features even when it isn't the original init image. Guarded so it is an
      // exact identity for an init-image save (img reg == reference) and skipped
      // entirely for legacy defs -> zero change to existing rendering.
      // NOTE: coordinate frame/sign (and isFlipped) pending visual verification.
      try {
        // The calibration preview (disableImageAlign) shows a raw live frame, not
        // a def image, so it must NOT apply def_image_reg rotation -- ignore reg
        // and fall through to the centering-only branch below.
        const reg = this.disableImageAlign ? null : (this.edit_DB_info && this.edit_DB_info.def_image_reg);
        const ref = this.db_obj && this.db_obj.sig360info && this.db_obj.sig360info.reports
                    && this.db_obj.sig360info.reports[0];
        if (reg && ref && typeof reg.cx === 'number' && typeof reg.cy === 'number') {
          const refAngle = ref.orientation || 0;
          const imgAngle = reg.angle || 0;
          const flip = reg.isFlipped || false;
          const differs = reg.cx !== ref.cx || reg.cy !== ref.cy || imgAngle !== refAngle;
          if (differs) {
            flip ? ctx.scale(1, -1) : ctx.scale(1, 1);
            //ctx.translate(ref.cx, ref.cy);          // -> reference sig360 position
            ctx.rotate(imgAngle);        // -> reference orientation
            ctx.translate(-reg.cx/mmpp, -reg.cy/mmpp); 
                  // image sig360 -> origin
            //ctx.translate(-reg.cx, -reg.cy);
          }
        }
        else
        {

          let center = this.db_obj.getsig360infoCenter();
          ctx.translate(-center.x/mmpp, -center.y/mmpp);
        }
      } catch (e) { 



       }

      ctx.drawImage(this.secCanvas, 0, 0);
      
      ctx.strokeStyle = "rgba(120, 120, 120,30)";
      let curScale=this.camera.GetCameraScale();
      ctx.lineWidth = 200/curScale/scale;
      this.rUtil.drawImageBoundaryGrid(ctx,this.img_info,100000/curScale);

      


    
      ctx.restore();
    }
    

    //draw box 0,0 to 100,100
    // ctx.strokeStyle = "rgba(255,0,0,1)";
    // ctx.lineWidth = 3;
    // ctx.strokeRect(0, 0, 100, 100);
    // ctx.closePath();


    let skipDrawIdxs = [];


    if(this.ShowInspectionNote!=true)
      return
   




    
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
      // console.log("mouseOnCanvas2",mouseOnCanvas2);

    }

    ctx.closePath();
    ctx.save();




    this.rUtil.drawShapeList(ctx, this.edit_DB_info._obj.shapeList, null, skipDrawIdxs, this.edit_DB_info._obj.shapeList, unitConvert,false,false);
    this.rUtil.drawInherentShapeList(ctx, this.edit_DB_info.inherentShapeList);

    
  }

  // Sample the image pixel under the mouse. Rebuilds the SAME secCanvas->canvas
  // transform draw() uses (worldTransform . scale(scale*mmpp) . translate(offset))
  // and inverts it to map a canvas point back to a secCanvas (image) pixel, then
  // reads it. Returns {ix,iy,r,g,b,lum} (lum = Rec.601 luma), or undefined if
  // off-image / not ready.
  brightnessAtEvent(evt) {
    if (this.img_info === undefined || this.secCanvas === undefined) return undefined;
    if (this.secCanvas.width === 0 || this.secCanvas.height === 0) return undefined;
    let mmpp = this.rUtil.get_mmpp();
    let scale = (this.img_info.scale !== undefined) ? this.img_info.scale : 1;
    let M = this.worldTransform();                       // fresh DOMMatrix per call
    M.scaleSelf(scale * mmpp, scale * mmpp);
    if (this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined)
      M.translateSelf(this.img_info.offsetX / scale - 0.5, this.img_info.offsetY / scale - 0.5);
    let pos = this.getMousePos(this.canvas, evt);         // canvas-pixel coords
    let p = this.VecX2DMat(pos, M.inverse());             // -> secCanvas pixel
    let ix = Math.floor(p.x), iy = Math.floor(p.y);
    if (!Number.isFinite(ix) || !Number.isFinite(iy)) return undefined;
    if (ix < 0 || iy < 0 || ix >= this.secCanvas.width || iy >= this.secCanvas.height) return undefined;
    let d = this.secCanvas.getContext('2d').getImageData(ix, iy, 1, 1).data;
    let lum = Math.round(0.299 * d[0] + 0.587 * d[1] + 0.114 * d[2]);
    return { ix, iy, r: d[0], g: d[1], b: d[2], lum };
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
    this.doRotateView=true;
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

    // this.stream_img = new Image(); 
    // this.stream_img.src = "http://localhost:7603/CAM1.mjpg";
  }

  SetStreamImageSrc(url)
  {
    // this.stream_img.src = url;
  }
 
  resourceClean() {
    this.canvas.removeEventListener('wheel', this.onmouseswheel.bind(this));
    this.stream_img=null;
    log.debug("resourceClean......")
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

  EditDBInfoSync(edit_DB_info,updateImgOnly=false) {
    if(updateImgOnly==false)
    {
      this.edit_DB_info = edit_DB_info;
      this.db_obj = edit_DB_info._obj;
      this.rUtil.setEditor_db_obj(this.db_obj);
    }
    this.SetImg(edit_DB_info.img);
    // cameraParam can be undefined when the def has no embedded cam_param
    // (e.g. fresh def, or report not yet attached). Default to identity so
    // the canvas still renders rather than crashing the boundary.
    let mmpp = (this.db_obj.cameraParam && this.db_obj.cameraParam.mmpb2b != null && this.db_obj.cameraParam.ppb2b != null)
      ? this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b
      : 1;
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
      log.debug("[inspectionResult] judge", obj);
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
    
    let down_samp_level = 1.0 * crop[2] / (cW);
    if(this.doRotateView==true)
    {
      let mmpp = this.rUtil.get_mmpp();
      if(this.img_info!==undefined)
      {
        crop= [
          0,0,
          (this.img_info.full_height+100)*mmpp,
          (this.img_info.full_width+100)*mmpp];
          
        let new_down_samp_level = 1.0 * crop[2] / (cW);
        if(down_samp_level<new_down_samp_level)
          down_samp_level=new_down_samp_level;
      }
    }


    this.EmitEvent(
      {
        type: "down_samp_level_update",
        data: {
          down_samp_level,
          crop
        }
      }
    );
  }


  draw_INSP() {

    if(this.img_info===undefined)
    {
      return;
    }
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
    
    ctx.font = this.rUtil.getFontStyle(1);
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);




    let inspectionReportList = this.edit_DB_info.reportStatisticState.trackingWindow.filter((x) => x.isCurObj);





    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);

    // console.log(">>>>>>>",this.doRotateView,inspectionReportList.length,this.img_info);
    if(this.doRotateView==true && inspectionReportList.length>=1 && this.img_info!==undefined)
    {
      // let line_N=inspectionReportList[0].detectedLines[1];
      // let spoint_N=inspectionReportList[0].searchPoints[0];
      
      let centerPt={x:inspectionReportList[0].cx,y:inspectionReportList[0].cy}
      // let centerPt={x:(line_N.pt1.x+line_N.pt2.x)/2,y:(line_N.pt1.y+line_N.pt2.y)/2}
      // centerPt={x:(spoint_N.x),y:(spoint_N.y)}
      // ctx.translate(centerPt.x,centerPt.y);//Move to the center of the secCanvas
      

      let rot=inspectionReportList[0].rotate;
      // let rot=-Math.atan2(line_N.vy,line_N.vx);

      ctx.translate(this.img_info.full_width/2*mmpp, this.img_info.full_height/2*mmpp);//Move to the center of the secCanvas

      if (inspectionReportList[0].isFlipped)
      {
        
        ctx.rotate(-rot);
        ctx.scale(1, -1);
        
      }
      else
      {

        ctx.rotate(rot);
      }
      
      // ctx.translate(inspectionReportList[0].cx/mmpp,inspectionReportList[0].cy/mmpp);//Move to the center of the secCanvas
      // console.log(inspectionReportList[0]);
      
      ctx.translate(-centerPt.x,-centerPt.y);//Move to the center of the secCanvas
      
    }
    
    
    {
      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;
      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;

      //ctx.translate(-this.secCanvas.width*mmpp_mult/2,-this.secCanvas.height*mmpp_mult/2);//Move to the center of the secCanvas
      ctx.save();

      let curScale=this.camera.GetCameraScale();
      if(true){

        ctx.scale(mmpp_mult, mmpp_mult);
        if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
          ctx.translate((this.img_info.offsetX-0.5*(scale)) / scale, (this.img_info.offsetY-0.5*(scale)) / scale);
        }
        // ctx.translate(-1 * mmpp_mult, -1 * mmpp_mult);
        ctx.drawImage(this.secCanvas, 0, 0);
        
        ctx.strokeStyle = "rgba(120, 120, 120,30)";
        
        ctx.lineWidth = 50/curScale;
        this.rUtil.drawImageBoundaryGrid(ctx,this.img_info,100000/curScale);
  
      }
      else
      {
        try{
          ctx.imageSmoothingEnabled=false;
          ctx.scale(mmpp, mmpp);
          ctx.drawImage(this.stream_img, 0, 0);
        } catch (error) {
          log.error("[stream-drawImage]", { stream_img: this.stream_img, error });
        }
        
        
      }
      ctx.restore();

      if(false && this.db_obj.cameraParam && this.db_obj.cameraParam.mask_radius!==undefined)
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
    inspectionReportList.forEach((report, idx) => {
      //let ret_res = this.inspectionResult(report);
      //if(ret_res == INSPECTION_STATUS.SUCCESS)


      // let finalStatus=this.colorSet.color_SUCCESS
      {
        let listClone = dclone(this.edit_DB_info._obj.shapeList);

        this.db_obj.ShapeListAdjustsWithInspectionResult(listClone, report);

        listClone=listClone.filter(ff=>{
          if(ff.rank===undefined)return true;
          if(ff.rank<=this.measureDisplayRank)return true;
          return false;
        });


        listClone.forEach((eObj) => {
          // log.info(eObj,report);
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
                  // try{
                  eObj.detailStatus=inspMeasureTar.detailStatus;
                  let dStatus=inspMeasureTar.detailStatus;
                  // console.log(dStatus);
                  let tarColor= MEASURE_RESULT_VISUAL_INFO[dStatus].COLOR;
                  eObj.color = tarColor;

                  // }
                  // catch(e)
                  // {
                  //   console.log(e);
                  //   ctx.fillStyle=MEASURE_RESULT_VISUAL_INFO[MEASURERSULTRESION.NA].COLOR;
                  // }

                }

              }
              break;
            case INSPECTION_STATUS.FAILURE:
              eObj.color = this.colorSet.color_FAILURE;
              break;

          }

          if(eObj.quality_essential==false)
          {
            eObj.color=Color(eObj.color).mix(Color("rgba(128,128,128,1)"),0.8).fade(0.5).string()
          }

        });

        
        ctx.save();
        ctx.translate(report.cx, report.cy);
        {
          let overallStat = listClone
          .reduce((res, listEle) => {
            if( listEle.type!==SHAPE_TYPE.measure || listEle.quality_essential==false)return res;
            return MEASURERSULTRESION_reducer(res, listEle.detailStatus);
          }, MEASURERSULTRESION.UOK);

        
          // try{
           
          // console.log(overallStat);
          if(overallStat===undefined)
            ctx.fillStyle =MEASURE_RESULT_VISUAL_INFO[MEASURERSULTRESION.NA].COLOR;
          else
          {
            ctx.fillStyle =MEASURE_RESULT_VISUAL_INFO[overallStat].COLOR;
          }

          // }
          // catch(e)
          // {
          //   console.log(e);
          //   ctx.fillStyle=MEASURE_RESULT_VISUAL_INFO[MEASURERSULTRESION.NA].COLOR;
          // }
          // console.log(overallStat,ctx.fillStyle );

          ctx.strokeStyle = "black";
          this.rUtil.draw_Text(ctx, `${idx} ${report.isFlipped?"反":"正"}`, this.rUtil.getFontHeightPx(), 0,0);


          ctx.strokeStyle = "red";
          this.rUtil.draw_aimcross(ctx, {x:0,y:0},this.rUtil.getPointSize()*3,0.1);

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
        ctx.restore();
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
    this.mouseTriggeredUpdate=false;
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
    this.mouseTriggeredUpdate=(this.mouseStatus.status != this.mouseStatus.pstatus);
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
    // Keystone phase C: per-shape canvasCtrl. Each shape module owns the rule
    // for what other shapes can be referenced when creating/editing it (measure
    // further dispatches by subtype). Unregistered types fall through to the
    // unfiltered list (matches legacy default).
    const mod = getShapeModule(type);
    if (mod && mod.availableRefShapes) return mod.availableRefShapes(shapeList, subtype);
    return shapeList;
  }

  fitCameraToShape(shape) {
    if (shape == null || shape === undefined) return;
    // Keystone phase C: per-shape canvasCtrl. Each shape module returns its
    // "fit camera" center point (or null/undefined to skip). Unregistered types
    // and shapes without fitCameraCenter (e.g. aux_line, measure) skip — same
    // as the legacy default branch's early return.
    const mod = getShapeModule(shape.type);
    if (!mod || !mod.fitCameraCenter) return;
    const center = mod.fitCameraCenter(shape, this.db_obj);
    if (!center) return;
    // A new/incomplete arc (undefined or collinear pt1/pt2/pt3) makes
    // threePointToArc return NaN, so fitCameraCenter yields {x:NaN,y:NaN} -- a
    // truthy object the !center guard misses. Feeding NaN to SetOffset corrupts
    // the camera DOMMatrix and crashes the next worldTransform. Skip non-finite.
    if (!Number.isFinite(center.x) || !Number.isFinite(center.y)) return;

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

    // Build {shape_id → cal_hits} from the latest inspection report. Core
    // emits cal_hits in OBJECT-FRAME mm (the def's own coord system) so
    // they overlay the def shape directly — no inverse transform needed.
    //
    // Staleness gate: the reducer snapshots per-shape matching-relevant
    // fields (pt1/pt2/pt3/margin/locating/caliper/edge) into
    // inspReport.shape_fingerprints at inspect-time. If the user edits the
    // def afterward, the current fingerprint won't match — skip those hits
    // rather than showing data that doesn't reflect the current params.
    this.rUtil.cal_hits_by_id = {};
    const rpt = this.edit_DB_info && this.edit_DB_info.inspReport;
    const single = rpt && rpt.reports && rpt.reports[0];
    if (single) {
      const fps = rpt.shape_fingerprints || {};
      const fpOf = (s) => JSON.stringify({
        pt1: s.pt1, pt2: s.pt2, pt3: s.pt3,
        margin: s.margin, locating: s.locating,
        caliper: s.caliper, edge: s.edge,
      });
      const shapeList = this.edit_DB_info._obj && this.edit_DB_info._obj.shapeList || [];
      const byId = new Map(shapeList.map((s) => [s.id, s]));
      const collect = (arr) => {
        if (!arr) return;
        for (const r of arr) {
          if (!(r && r.cal_hits && r.id !== undefined)) continue;
          const cur = byId.get(r.id);
          if (cur && fps[r.id] !== undefined && fpOf(cur) !== fps[r.id]) continue; // stale
          // Pass per-hit status through unchanged so each caliper renders
          // its own outcome: 0=missed → gray box + no X, 1=outlier → red X,
          // 2=inlier → green X. Whole-fit success/fail does NOT override
          // per-hit state — a failed fit with no found edges should look
          // distinctly different from a failed fit with many outliers.
          this.rUtil.cal_hits_by_id[r.id] = r.cal_hits;
        }
      };
      collect(single.detectedLines);
      collect(single.detectedCircles);
      collect(single.searchPoints);
    }

    {
      let center = this.db_obj.getsig360infoCenter();
      ctx.save();
      // Option 2: rectify the displayed image into the def's OBJECT frame using
      // the last INST_CHECK result (detected center + matched rotation), so the
      // part lines up with the object-frame overlays (shapes / cal_hits). Because
      // the image and the hits then share the SAME transform, the matched angle's
      // residual error cancels and the marks sit pixel-precise on the edges.
      // Falls back to the plain centered (unrotated) view when there's no result.
      // NOTE: verify the rotate SIGN and flip AXIS on target -- if the image
      // rotates the wrong way, flip the sign of `single.rotate`; if a flipped
      // match mirrors wrong, change `ctx.scale(1, flip)` to `ctx.scale(flip, 1)`.
      console.log("single",single);
      if (single && typeof single.rotate === 'number') {
        let flip = single.isFlipped ? -1 : 1;
        ctx.scale(1, flip);                       // mirror (flipped match) first
        ctx.rotate(single.rotate);               // undo the part's rotation
        ctx.translate(-single.cx, -single.cy);    // detected center -> object origin

      } else {
        ctx.translate(-center.x, -center.y);
      }

      let scale = 1;
      if (this.img_info !== undefined && this.img_info.scale !== undefined)
        scale = this.img_info.scale;

      ctx.imageSmoothingEnabled = scale!=1;
      ctx.webkitImageSmoothingEnabled = scale!=1;
      let mmpp_mult = scale * mmpp;
      
      ctx.scale(mmpp_mult, mmpp_mult);
      if (this.img_info !== undefined && this.img_info.offsetX !== undefined && this.img_info.offsetY !== undefined) {
        //ctx.translate((this.img_info.offsetX / scale -0.5), (this.img_info.offsetY) / scale -0.5);
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

    let displayShape = this.AvailableShapeFilter(this.edit_DB_info._obj.shapeList);

    let drawFocusItem = false;
    let skipDrawIdxs = [];
    if (this.EditShape != null) {

      drawFocusItem = true;
      skipDrawIdxs.push(this.EditShape.id);

      ctx.strokeStyle = this.colorSet.editShape;
      this.rUtil.drawShapeList(ctx, [this.EditShape], this.colorSet.editShape, [], this.edit_DB_info._obj.shapeList, unitConvert, true,true);
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
        this.rUtil.drawShapeList(ctx, [candPtInfo.shape], dcolor, [], this.edit_DB_info._obj.shapeList, unitConvert, true,true);
      }
    }

    if (displayShape != this.edit_DB_info._obj.shapeList)//draw all
    {
      this.rUtil.drawShapeList(ctx, displayShape, null, skipDrawIdxs, this.edit_DB_info._obj.shapeList, unitConvert,false,false);
    }
    else if (!drawFocusItem) {
      this.rUtil.drawShapeList(ctx, this.edit_DB_info._obj.shapeList, null, skipDrawIdxs, this.edit_DB_info._obj.shapeList, unitConvert,false,false);
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
              margin: mmpp_round*5,
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
              margin: mmpp_round*5,
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
          {
            
            let displayShape = this.AvailableShapeFilter(this.edit_DB_info._obj.shapeList);
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
          if (this.mouseStatus.status == 1) {
            if (ifOnMouseLeftClickEdge && this.CandEditPointInfo != null) {
              if (this.state.substate == UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE) {

                this.EditShape = {
                  type: SHAPE_TYPE.search_point,
                  pt1: { x: 0, y: 0 },
                  angleDeg: 90,
                  search_far:false,
                  margin: mmpp_round*3,
                  width: mmpp_round*10,
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
          break;
        }


      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        {

          let tar_ele_trace = this.edit_DB_info.edit_tar_ele_trace;

          {
            if ((tar_ele_trace === null || tar_ele_trace === undefined)
              && this.EditShape != null && ifOnMouseLeftClickEdge) {
              if(this.mouseTriggeredUpdate==false)
                this.SetShape(this.EditShape, this.EditShape.id);
            }

            let displayShape = this.AvailableShapeFilter(this.edit_DB_info._obj.shapeList);

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
          this.mouseTriggeredUpdate=false;



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
    // cameraParam can be undefined when the def has no embedded cam_param
    // (e.g. fresh def, or report not yet attached). Default to identity so
    // the canvas still renders rather than crashing the boundary.
    let mmpp = (this.db_obj.cameraParam && this.db_obj.cameraParam.mmpb2b != null && this.db_obj.cameraParam.ppb2b != null)
      ? this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b
      : 1;
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
    ctx.lineWidth = size/5;
    ctx.fillStyle = color;
    ctx.strokeStyle = "black";

    ctx.beginPath();

    ctx.rect(point.x - size / 2, point.y - size / 2, size, size);
    ctx.stroke();
    ctx.closePath();
    ctx.fill();

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

      if(this.markPoints.length%2==0)//a point pair
      this.EmitEvent(
        {
          type: "point_pair_update",
          data: {
            pts:this.markPoints.filter((pt,idx)=>idx%2==0).map((_,idx)=>[this.markPoints[2*idx],this.markPoints[2*idx+1]]),
            pt:this.markPoints.slice(-2)
          }
        }
      );
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
    
    if(img_info.IGNORE_IMAGE_FIT_TO_SCREEN==false)
      this.scaleImageToFitScreen();
  
  }

  EditDBInfoSync(edit_DB_info) {
    if(edit_DB_info._obj===undefined)return;
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg(edit_DB_info.img);
    if(this.db_obj.cameraParam!==undefined)
    {
      // cameraParam can be undefined when the def has no embedded cam_param
    // (e.g. fresh def, or report not yet attached). Default to identity so
    // the canvas still renders rather than crashing the boundary.
    let mmpp = (this.db_obj.cameraParam && this.db_obj.cameraParam.mmpb2b != null && this.db_obj.cameraParam.ppb2b != null)
      ? this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b
      : 1;
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
        // console.log(this.markPoints);

        if(this.markPoints.length%2==0)//a point pair
          this.EmitEvent(
            {
              type: "point_pair_update",
              data: {
                pts:this.markPoints.filter((pt,idx)=>idx%2==0).map((_,idx)=>[this.markPoints[2*idx],this.markPoints[2*idx+1]]),
                pt:this.markPoints.slice(-2)
      }
    }
          );
      }
    }
    // console.log(mouseOnCanvas2,pmouseOnCanvas2);

    
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
      // cameraParam can be undefined when the def has no embedded cam_param
    // (e.g. fresh def, or report not yet attached). Default to identity so
    // the canvas still renders rather than crashing the boundary.
    let mmpp = (this.db_obj.cameraParam && this.db_obj.cameraParam.mmpb2b != null && this.db_obj.cameraParam.ppb2b != null)
      ? this.db_obj.cameraParam.mmpb2b / this.db_obj.cameraParam.ppb2b
      : 1;
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
    // console.log(img_info);
    if(img_info.IGNORE_IMAGE_FIT_TO_SCREEN!=true)
      this.scaleImageToFitScreen();
  }


  draw() {
    let unitConvert = {
      unit: "mm",//"μm",
      mult: 1
    };

    let ctx = this.canvas.getContext('2d');
    ctx.font = this.rUtil.getFontStyle(1);
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = this.rUtil.getIndicationLineSize();
    ctx.resetTransform();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix = this.worldTransform();
    ctx.setTransform(matrix.a, matrix.b, matrix.c,
      matrix.d, matrix.e, matrix.f);
    {
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

      let inspectionReportList = this.edit_DB_info.reportStatisticState.trackingWindow;

      inspectionReportList.forEach((report, idx) => 
        {
          let listClone = dclone(this.edit_DB_info._obj.shapeList);
  
          this.db_obj.ShapeListAdjustsWithInspectionResult(listClone, report);
          

          let curStatus=INSPECTION_STATUS.SUCCESS;
          let finalColor=MEASURE_RESULT_VISUAL_INFO[MEASURERSULTRESION.UOK].COLOR;

          listClone.forEach((eObj) => {
            let tmpStatus=curStatus;
            let tmpFinalColor=finalColor;
            // log.info(eObj.inspection_status);
            switch (eObj.inspection_status) {
              case INSPECTION_STATUS.NA:
                eObj.color = this.colorSet.color_NA;
                tmpStatus=eObj.inspection_status;
                tmpFinalColor=eObj.color;
                break;
              case INSPECTION_STATUS.UNSET:
                eObj.color = this.colorSet.color_UNSET;
                
                break;
              case INSPECTION_STATUS.SUCCESS:
                {
                  eObj.color = MEASURE_RESULT_VISUAL_INFO[MEASURERSULTRESION.UOK].COLOR;

                  // if (eObj.type === SHAPE_TYPE.measure) {
                  //   let targetID = eObj.id;
                  //   let inspMeasureTar = report.judgeReports.find((measure) => measure.id === targetID);
                  //   if (inspMeasureTar === undefined) break;
  
                  //   console.log(inspMeasureTar);
                  //   eObj.color = MEASURE_RESULT_VISUAL_INFO[inspMeasureTar.detailStatus].COLOR;
  
                  // }
  
                }
                break;
              case INSPECTION_STATUS.FAILURE:
                eObj.color = this.colorSet.color_FAILURE;
                if(curStatus==INSPECTION_STATUS.SUCCESS)
                {
                  tmpStatus=eObj.inspection_status;
                  tmpFinalColor=eObj.color;
                }
                break;
  
            }

            

            if(eObj.quality_essential==false)
            {
              eObj.color=Color(eObj.color).mix(Color("rgba(128,128,128,1)"),0.8).fade(0.5).string()
            }
            else if(eObj.type==SHAPE_TYPE.measure)
            {
              // console.log(eObj,tmpStatus,tmpFinalColor,curStatus,finalColor)
              curStatus=tmpStatus;
              finalColor=tmpFinalColor;
            }

          });


          {

            ctx.save();
            ctx.translate(report.cx, report.cy);

            
            ctx.strokeStyle = "black";
            ctx.fillStyle = finalColor;
            this.rUtil.draw_Text(ctx, `${idx} ${report.isFlipped?"反":"正"}`, this.rUtil.getFontHeightPx(), 0,0);
          
            ctx.strokeStyle = "red";
            this.rUtil.draw_aimcross(ctx, {x:0,y:0},this.rUtil.getPointSize()*3,0.1);
  
            ctx.restore();
          }
          
          
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