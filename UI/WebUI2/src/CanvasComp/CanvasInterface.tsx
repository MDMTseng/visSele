
// import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import { GetObjElement } from '../UTIL/MISC_Util';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine,
  closestPointOnPoints,
  distance_point_point
} from '../UTIL/MathTools';


import {VEC2D,SHAPE_ARC,SHAPE_LINE_seg} from '../UTIL/MathTools';

import dclone from 'clone';


function domMatrixToObj(matrix:DOMMatrix) {
  return ({
    a: matrix.a,
    b: matrix.b,
    c: matrix.c,
    d: matrix.d,
    e: matrix.e,
    f: matrix.f,
  });
}

// Convert JSON to DOMMatrix
function jsonToDomMatrix(json:{a:number,b:number,c:number,d:number,e:number,f:number}) {
  const data = json;
  const { a, b, c, d, e, f } = data;
  const matrix = new DOMMatrix();
  matrix.a = a;
  matrix.b = b;
  matrix.c = c;
  matrix.d = d;
  matrix.e = e;
  matrix.f = f;
  return matrix;
}


class CameraCtrl {
  matrix:DOMMatrix
  tmpMatrix:DOMMatrix
  identityMat:DOMMatrix

  constructor(sobj:any|undefined=undefined) {
    this.matrix = new DOMMatrix();
    this.tmpMatrix = new DOMMatrix();
    this.identityMat = new DOMMatrix();
    this.Scale(1);
    if(sobj!==undefined)
      this.fromSimpleObj(sobj);
  }

  toSimpleObj()
  {
    return {
      matrix:domMatrixToObj(this.matrix),
      tmpMatrix:domMatrixToObj(this.tmpMatrix)
    }
  }

  fromSimpleObj(sobj:any)
  {
    this.matrix=jsonToDomMatrix(sobj.matrix);
    this.tmpMatrix=jsonToDomMatrix(sobj.tmpMatrix);
  }

  Scale(scale:number, center = { x: 0, y: 0 }) {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x, center.y);
    mat.scaleSelf(scale, scale);
    mat.translateSelf(-center.x, -center.y);

    this.matrix.preMultiplySelf(mat);
  }

  Rotate_matrix(theta:number, center = { x: 0, y: 0 }) {
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
  
  Rotate(theta:number, center = { x: 0, y: 0 }) {

    this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center ));
  }

  ResetRotate(theta:number, center = { x: 0, y: 0 }) {

    // this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center ));
  }
  SetRotate(theta:number, center = { x: 0, y: 0 }) {

    this.matrix=this.Rotate_matrix(theta, center );
  }
  StartDrag(vector = { x: 0, y: 0 }) {
    this.tmpMatrix=this.identityMat.translate(vector.x, vector.y);
  }
  EndDrag() {
    this.matrix.preMultiplySelf(this.tmpMatrix);
    this.tmpMatrix = new DOMMatrix();
  }

  SetOffset(location = { x: 0, y: 0 }) {

    this.matrix.m41 = 0;
    this.matrix.m42 = 0;
    this.matrix.translateSelf(location.x, location.y);
  }

  GetOffset() {
    return { x: this.matrix.m41, y: this.matrix.m42 };
    // this.matrix.translateSelf(location.x, location.y);
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

  CameraTransform(matrix_input:DOMMatrix) {
    matrix_input.multiplySelf(this.tmpMatrix);
    matrix_input.multiplySelf(this.matrix);
  }


}


class renderUTIL {
  camCtrl:CameraCtrl
  renderParam:{
    base_Size:number,
    size_Multiplier:number,
    mmpp:number
    font_Base_Size:number,
    font_Style:string

  }
  iconSet:{
    [name:string]:HTMLImageElement
  }
  constructor( cameraCtrl:CameraCtrl) {
    this.camCtrl = cameraCtrl;
    this.renderParam = {
      base_Size: 2.5,
      size_Multiplier: 1,
      mmpp: 0.1,
      font_Base_Size: 1,
      font_Style: "bold "
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

  drawLine(ctx:CanvasRenderingContext2D, line_obj:SHAPE_LINE_seg) {
    ctx.beginPath();
    ctx.moveTo(line_obj.x1, line_obj.y1);
    ctx.lineTo(line_obj.x2, line_obj.y2);
    //ctx.closePath();
    ctx.stroke();
  }


  drawHollowLine(ctx:CanvasRenderingContext2D, line:SHAPE_LINE_seg,boundaryWidth=5) {
    ctx.beginPath();
    let LineWidth=ctx.lineWidth;
    ctx.lineWidth=boundaryWidth;
    let vec = {
      x:(line.x2-line.x1),
      y:(line.y2-line.y1),
    }
    let vecL=Math.hypot(vec.x,vec.y);
    let normalVec={
      x:-vec.y/vecL,
      y:vec.x/vecL,
    };


    ctx.moveTo(line.x1 -normalVec.x*LineWidth/2, line.y1 -normalVec.y*LineWidth/2);
    ctx.lineTo(line.x1 +normalVec.x*LineWidth/2, line.y1 +normalVec.y*LineWidth/2);
    ctx.lineTo(line.x2 +normalVec.x*LineWidth/2, line.y2 +normalVec.y*LineWidth/2);
    ctx.lineTo(line.x2 -normalVec.x*LineWidth/2, line.y2 -normalVec.y*LineWidth/2);

    // normalVec
    ctx.moveTo(line.x1 -normalVec.x*LineWidth/2, line.y1 -normalVec.y*LineWidth/2);
    ctx.lineTo(line.x1 +normalVec.x*LineWidth/2, line.y1 +normalVec.y*LineWidth/2);
    ctx.lineTo(line.x2 +normalVec.x*LineWidth/2, line.y2 +normalVec.y*LineWidth/2);
    ctx.lineTo(line.x2 -normalVec.x*LineWidth/2, line.y2 -normalVec.y*LineWidth/2);


    ctx.closePath();
    ctx.stroke();
  }



  drawArc(ctx:CanvasRenderingContext2D, arc_obj:SHAPE_ARC) {
    ctx.beginPath();
    ctx.arc(arc_obj.x, arc_obj.y , arc_obj.r, arc_obj.angleFrom, arc_obj.angleTo, false);
    ctx.stroke();
  }

  _drawpoint(ctx:CanvasRenderingContext2D, point:VEC2D, type:string, size = 5) {
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

  drawPoint(ctx:CanvasRenderingContext2D, point:VEC2D, type:string, size = this.getPointSize()) {
    let strokeStyle_bk = ctx.strokeStyle;

    ctx.lineWidth = size * 2;
    ctx.strokeStyle = "rgba(0,0,100,0.5)";
    if (type != "cross")
      this._drawpoint(ctx, point, type, 2 * size);

    ctx.lineWidth = size / 2;
    ctx.strokeStyle = strokeStyle_bk;
    this._drawpoint(ctx, point, type, 2 * size);
  }
  
  drawAimCross(ctx:CanvasRenderingContext2D, point:VEC2D, size = this.getPointSize(),ratio=0.2) {


    
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

  
  drawCross(ctx:CanvasRenderingContext2D, point:VEC2D, size = this.getPointSize()) {
    this._drawpoint(ctx, point, "cross", 2 * size);
  }

  drawArrow(ctx:CanvasRenderingContext2D,pt1:VEC2D,pt2:VEC2D, headlen = 10, aangle = Math.PI / 6) {
    var angle = Math.atan2(pt2.y - pt1.y, pt2.x - pt1.x);
    ctx.beginPath();
    ctx.moveTo(pt1.x, pt1.y);
    ctx.lineTo(pt2.x, pt2.y);

    ctx.moveTo(pt2.x, pt2.y);
    ctx.lineTo(pt2.x - headlen * Math.cos(angle - aangle), pt2.y - headlen * Math.sin(angle - aangle));
    //ctx.moveTo(pt2.x, pt2.y);
    ctx.lineTo(pt2.x - headlen * Math.cos(angle + aangle), pt2.y - headlen * Math.sin(angle + aangle));

    ctx.closePath();
    ctx.stroke();
    ctx.fill();
  }

  drawArcArrow(ctx:CanvasRenderingContext2D,arc:SHAPE_ARC, arrowside:number=0,) {

    this.drawArc(ctx, arc);
    if(arrowside>=0)
    {
      let ax = Math.cos(arc.angleTo);
      let ay = Math.sin(arc.angleTo);
      let pt2 ={
        x:arc.x+ arc.r * ax,
        y:arc.y+ arc.r * ay}
      let dirSign = 1;
      dirSign *= this.getPrimitiveSize();
      let arrowSize = 3 * this.getPrimitiveSize();

      let pt1 ={
        x:pt2.x+ dirSign * ay,
        y:pt2.y- dirSign * ax}
      this.drawArrow(ctx, pt1,pt2, arrowSize);
    }

    if(arrowside<=0)
    {
      let ax = Math.cos(arc.angleFrom);
      let ay = Math.sin(arc.angleFrom);
      let pt2 ={
        x:arc.x+ arc.r * ax,
        y:arc.y+ arc.r * ay}
      let dirSign = 1;
      dirSign *= this.getPrimitiveSize();
      let arrowSize = 3 * this.getPrimitiveSize();

      let pt1 ={
        x:pt2.x+ dirSign * ay,
        y:pt2.y- dirSign * ax}
      this.drawArrow(ctx, pt1,pt2, arrowSize);
    }

  }
  drawText(ctx:CanvasRenderingContext2D, text:string,point:VEC2D) {
    ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.01;
    ctx.fillText(text, point.x, point.y);
    ctx.strokeStyle = "black";
    ctx.lineWidth = 1;//this.getIndicationLineSize();
    ctx.strokeText(text, point.x, point.y);
  }
  
  draw_Text(ctx:CanvasRenderingContext2D, text:string,point:VEC2D,scale:number) {
    ctx.lineWidth = this.renderParam.base_Size * this.renderParam.size_Multiplier*0.013;
    ctx.save();
    ctx.translate(point.x, point.y);
    ctx.scale(scale, scale);
    ctx.fillText(text, 0, 0);
    ctx.strokeText(text, 0, 0);
    ctx.restore();
  }
}


export class EverCheckCanvasComponent_proto {

  EmitEvent: (param:any) => void;
  canvas:HTMLCanvasElement
  multiTouchInfo:{
    ptouchStatus: any[],
    touchStatus: any[],
    dragInfo: {[key:string]:any},
    pinchInfo: {[key:string]:any},
  }

  mouseStatus:{
    x:number,
    y:number,
    px:number,
    py:number,
    status:number,
    pstatus:number
  }
  camera:CameraCtrl
  rUtil:renderUTIL
  debounce_zoom_emit:any
  allow_camera_drag:boolean
  pixelRatio:number
  
  getMousePos(evt:MouseEvent) {
    this.pixelRatio=window.devicePixelRatio;
    var rect = this.canvas.getBoundingClientRect();
    let mouse = {
      x: (evt.clientX - rect.left)*this.pixelRatio,
      y: (evt.clientY - rect.top)*this.pixelRatio
    };
    
    return mouse;
  }

  copyTouchPositionInfo(e:TouchEvent) {
    let touches :{
      clientX:number,
      clientY:number,
      force:boolean
      identifier:number,
      pageX:number,
      pageY:number,
      radiusX:number,
      radiusY:number,
      rotationAngle:number,
      screenX: number,
      screenY: number
    }[]=[];

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

  constructor(canvasDOM:HTMLCanvasElement) {
    this.canvas = canvasDOM;
    this.pixelRatio=window.devicePixelRatio;
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
    
    this.EmitEvent = (event) => { console.log(event); };
    this.canvas.onmouseout = this.onmouseout.bind(this);

    {

      this.multiTouchInfo = {
        ptouchStatus: [],
        touchStatus: [],
        dragInfo: {},
        pinchInfo: {},

      };


      let touchStatus = (e:TouchEvent) => {
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

    // this.secCanvas_rawImg = null;

    // this.secCanvas = document.createElement('canvas');

    this.camera = new CameraCtrl();
    this.camera.Scale(1);
    this.allow_camera_drag=true

    this.rUtil = new renderUTIL(this.camera);
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
    console.log("resourceClean......")
  }


  debounce(func:()=>void, wait:number, immediate:boolean) {
    let timeout:number=-1;
    return function () {
      var later = function () {
        timeout = 0;
        if (!immediate) func();
      };
      var callNow = immediate && !timeout;
      window.clearTimeout(timeout);
      timeout = window.setTimeout(later, wait);
      if (callNow) func();
    };
  };

  throttle(func:()=>void, wait:number) {

    // Executes the func after delay time.
    let  timerId:number =  -1;

    return function () {
      // console.log(timerId,wait);
      if(timerId!==-1)
      window.clearTimeout(timerId);
      timerId = window.setTimeout(()=>{
        // console.log("CALLLLL");
        timerId=-1;
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

  onmouseswheel(evt:WheelEvent) {
    //
    let ret_val = this.scaleCanvas(this.mouseStatus, evt.deltaY / 4);
    this.debounce_zoom_emit();
    evt.preventDefault();
    return ret_val;
  }

  scaleCanvas(scaleCenter:VEC2D, deltaY:number, scale = 1 / 1.01) {
    if (deltaY > 50) deltaY = 1;//Windows scroll hack => only 100 or -100
    if (deltaY < -50) deltaY = -1;

    scale = Math.pow(scale, deltaY);

    this.camera.Scale(scale,
      {
        x: (scaleCenter.x - (this.canvas.width / 2)),
        y: (scaleCenter.y - (this.canvas.height / 2))
      });

    this.ctrlLogic();
    this.draw();

    return false;
  }
  draw()
  {
    
  }
  ctrlLogic()
  {
    
  }
  onmousemove(evt:MouseEvent) {
    let pos = this.getMousePos(evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;

    let doDraw=true;
    //console.log("this.state.substate:",this.state.substate);
    
    if (this.allow_camera_drag) {
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

  onmousedown(evt:MouseEvent) {

    let pos = this.getMousePos(evt);
    this.mouseStatus.px = pos.x;
    this.mouseStatus.py = pos.y;
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 1;

    this.ctrlLogic();
    this.draw();
  }

  onmouseup(evt:MouseEvent) {
    let pos = this.getMousePos(evt);
    this.mouseStatus.x = pos.x;
    this.mouseStatus.y = pos.y;
    this.mouseStatus.status = 0;
    this.camera.EndDrag();

    this.debounce_zoom_emit();
    this.ctrlLogic();
    this.draw();
  }
  onmouseout(evt:MouseEvent) {
    if (this.mouseStatus.status == 1) {
      this.onmouseup(evt);
    }
  }



  worldTransform() {
    let wMat = new DOMMatrix();

    wMat.translateSelf((this.canvas.width / 2), (this.canvas.height / 2));
    this.camera.CameraTransform(wMat);
    //let center = this.getReportCenter();
    //wMat.translateSelf(-center.x, -center.y);

    return wMat;

  }



  VecX2DMat(vec:VEC2D, mat:DOMMatrix) {

    let XX = vec.x * mat.a + vec.y * mat.c + mat.e;
    let YY = vec.x * mat.b + vec.y * mat.d + mat.f;
    return { x: XX, y: YY };
  }
}
