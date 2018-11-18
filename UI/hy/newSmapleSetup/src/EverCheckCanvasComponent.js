//UIControl

import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';
import {
  distance_arc_point,
  distance_point_point,
  threePointToArc,
  distance_line_point,
  intersectPoint,
  LineCentralNormal,
  PtRotate2d} from 'UTIL/MathTools';


class CameraCtrl
{
  constructor( canvasDOM )
  {
    this.matrix=new DOMMatrix();
    this.tmpMatrix=new DOMMatrix();
    this.identityMat= new  DOMMatrix();
  }

  Scale(scale,center={x:0,y:0})
  {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x,center.y);
    mat.scaleSelf(scale,scale);
    mat.translateSelf(-center.x,-center.y);

    this.matrix.preMultiplySelf(mat);
  }

  StartDrag(vector={x:0,y:0})
  {
    this.tmpMatrix.setMatrixValue(this.identityMat);
    this.tmpMatrix.translateSelf(vector.x,vector.y);
  }
  EndDrag()
  {
    this.matrix.preMultiplySelf(this.tmpMatrix);
    this.tmpMatrix.setMatrixValue(this.identityMat);
  }

  SetOffset(location={x:0,y:0})
  {
    
    this.matrix.m41=0;
    this.matrix.m42=0;
    this.matrix.translateSelf(location.x,location.y);
  }
  
  GetCameraScale(matrix=this.matrix) {//It's an over simplified way to get scale for an matrix, change it if nesessary
    return Math.sqrt(matrix.m11*matrix.m22-matrix.m12*matrix.m21);
  }


  GetCameraOffset(matrix=this.matrix) {
    return {x:matrix.m41,y:matrix.m42};
  }

  CameraTransform(matrix_input) {
    matrix_input.multiplySelf(this.tmpMatrix);
    matrix_input.multiplySelf(this.matrix);
  }


}

class EverCheckCanvasComponent{

  getMousePos(canvas, evt) {
    var rect = canvas.getBoundingClientRect();
    let  mouse = {
      x: evt.clientX - rect.left,
      y: evt.clientY - rect.top
    };
    return mouse;
  }



  constructor( canvasDOM )
  {
    this.canvas = canvasDOM;

    this.canvas.onmousemove=this.onmousemove.bind(this);
    this.canvas.onmousedown=this.onmousedown.bind(this);
    this.canvas.onmouseup=this.onmouseup.bind(this);
    this.canvas.onmouseout=this.onmouseout.bind(this);

    this.canvas.addEventListener('wheel',function(event){
      this.onmouseswheel(event);
      return false; 
    }.bind(this), false);

    this.mouseStatus={x:-1,y:-1,px:-1,py:-1,status:0,pstatus:0};

    this.ReportJSON={};

    this.secCanvas_rawImg=null;

    this.secCanvas = document.createElement('canvas');

    this.identityMat= new  DOMMatrix();
    this.Mouse2SecCanvas= new  DOMMatrix();


    this.mouse_close_dist= 10;


    this.camera= new CameraCtrl();

    this.near_select_obj=null;

    this.onfeatureselected=(ev)=>{};
    
    this.state=UI_SM_STATES.EDIT_MODE_NEUTRAL;
    this.shapeList=[];
    this.inherentShapeList=[];
    
    this.EditShape=null;
    
    this.CandEditPointInfo=null;
    this.EditPoint=null;
    
    this.EmitEvent=(event)=>{console.log(event);};
    this.colorSet={
      unselected:"rgba(100,0,100,0.5)",
      editShape:"rgba(255,0,0,0.7)"
    };
  }

  SetState(state)
  {
    if(this.state!=state)
    {
      this.state=state;
      this.near_select_obj=null;

      if(this.state==UI_SM_STATES.EDIT_MODE_NEUTRAL)
      {
        this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:null});
        this.EditShape=null;
        this.EditPoint=null;
      }
    }
  }
  SetReport( report )
  {

    if(report == this.ReportJSON)return;
    this.ReportJSON = report;
    //this.draw();
  }
  

  FindShape( key , val, shapeList=this.shapeList)
  {
    for(let i=0;i<shapeList.length;i++)
    {
      if(shapeList[i][key] == val)
      {
        return i;
      }
    }
    return undefined;
  }

  SetShape( shape_obj, id)
  {
    this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Shape_Set,data:{shape:shape_obj,id:id}});
  }

  SetEditShape( EditShape )
  {
      this.EditShape = EditShape;
      
      console.log(this.tmp_EditShape_id);
      if(this.EditShape!=null && this.EditShape.id!=undefined && this.tmp_EditShape_id !=this.EditShape.id){
        this.fitCameraToShape(this.EditShape);
        this.tmp_EditShape_id=this.EditShape.id;
      }
  }

  
  SetShapeList( shapeList )
  {
    this.shapeList = shapeList;
    //console.log("this.EditShape.id:",this.EditShape);
    if(this.EditShape!=null && this.EditShape.id!==undefined)
    {
      let idx = this.FindShape( "id" , this.EditShape.id );
      if(idx ==undefined)
      {
        this.EditShape=null;
        this.EditPoint=null;
      }
    }
  }

  SetInherentShapeList( inherentShapeList )
  {
    this.inherentShapeList = inherentShapeList;
  }

  SetImg( img )
  {
    if(img == null || img == this.secCanvas_rawImg)return;
    console.log("SetImg:::");
    this.secCanvas.width = img.width;
    this.secCanvas.height = img.height;

    this.secCanvas_rawImg=img;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(img, 0, 0);
  }

  onmouseswheel(evt)
  {
    //console.log("onmouseswheel",evt);
    let deltaY = evt.deltaY/4;
    if(deltaY>50)deltaY=1;//Windows scroll hack => only 100 or -100
    if(deltaY<-50)deltaY=-1;



    let scale = 1/1.01;

    scale = Math.pow(scale,deltaY);

    this.camera.Scale(scale,
      {x:(this.mouseStatus.x-(this.canvas.width / 2)),
        y:(this.mouseStatus.y-(this.canvas.height / 2))});
    //this.ctrlLogic();
    this.draw();
  }
  onmousemove(evt)
  {
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;

    //console.log("onmousemove_pre:",this.state);
    //console.log("this.state:"+this.state+"  "+this.mouseStatus.status);
    switch(this.state)
    {
      case UI_SM_STATES.EDIT_MODE_SHAPE_EDIT:
        
        if(this.EditPoint!=null)break;
      case UI_SM_STATES.EDIT_MODE_NEUTRAL:
        //console.log("onmousemove");
        if(this.mouseStatus.status==1)
        {
          this.camera.StartDrag({   x:pos.x-this.mouseStatus.px,   y:pos.y-this.mouseStatus.py  });
        }
      break;
    }
    this.ctrlLogic();
    this.draw();

  }

  onmousedown(evt)
  {
    console.log("onmousedown");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.px=pos.x;
    this.mouseStatus.py=pos.y;
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 1;

    if(this.near_select_obj!=null)
    {
      this.onfeatureselected(this.near_select_obj);
    }
    this.ctrlLogic();
    this.draw();
  }

  onmouseup(evt)
  {
    console.log("onmouseup");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 0;
    this.camera.EndDrag();
    this.ctrlLogic();
    this.draw();
  }
  onmouseout(evt)
  {
    if(this.mouseStatus.status==1)
    {
      this.onmouseup(evt);
    }
  }

  resize(width,height)
  {
    this.canvas.width=width;
    this.canvas.height=height;
    //this.ctrlLogic();
    this.draw();
  }

  drawReportJSON_action(context,Report,action,depth=0) {

    if (Report.type == "binary_processing_group")
    {
      //console.log("binary_processing_group>>");
    }
    else if (Report.type == "sig360_circle_line")
    {
      //console.log("sig360_circle_line>>");
    }
    else
    {
      action(Report);
    }

    /*context.lineWidth = 2;
    // context.strokeStyle="rgba(255,0,0,0.5)";
    context.strokeStyle = lerpColor('#ff0000', '#0fff00', i/RXJS.reports[j].reports.length);*/


    if(typeof Report.reports !=='undefined')
    {
      Report.reports.forEach((report)=>{
        this.drawReportJSON_action(context,report,action,depth+1);
      });
    }

  }


  drawReportLine(ctx, line_obj, offset={x:0,y:0})
  {
    ctx.beginPath();
    ctx.moveTo(line_obj.x0+offset.x,line_obj.y0+offset.y);
    ctx.lineTo(line_obj.x1+offset.x,line_obj.y1+offset.y);
    ctx.closePath();
    ctx.stroke();
  }
  drawReportArc(ctx, arc_obj, offset={x:0,y:0})
  {
    ctx.beginPath();
    if(arc_obj.thetaS === undefined||arc_obj.thetaE === undefined )
      ctx.arc(arc_obj.x+offset.x,arc_obj.y+offset.y,arc_obj.r,0,Math.PI*2, false);
    else
      ctx.arc(arc_obj.x+offset.x,arc_obj.y+offset.y,arc_obj.r,arc_obj.thetaS,arc_obj.thetaE, false);
    //ctx.closePath();
    ctx.stroke();
  }

  _drawpoint(ctx, point,type,size=5)
  {
    ctx.beginPath();

    if(type == "rect")
    {
      ctx.rect(point.x-size/2,point.y-size/2,size,size);
    }
    else
    {
      ctx.arc(point.x,point.y,size/2,0,Math.PI*2, false);
    }
    ctx.closePath();
    ctx.stroke();
  }

  drawpoint(ctx, point,type)
  {
    let lineWidth_bk=ctx.lineWidth;
    let strokeStyle_bk=ctx.strokeStyle;

    ctx.lineWidth=lineWidth_bk*2;
    ctx.strokeStyle="rgba(0,0,100,0.5)";  
    this._drawpoint(ctx,point,type);

    ctx.lineWidth=lineWidth_bk;
    ctx.strokeStyle=strokeStyle_bk;  
    this._drawpoint(ctx,point,type);
  }
  
  FindClosestCtrlPointInfo( location)
  {
    let pt_info={
      pt:null,
      key:null,
      shape:null,
      dist:Number.POSITIVE_INFINITY
    };


    this.shapeList.forEach((shape)=>{
      let tmpDist;

      switch(shape.type)
      {
        case SHAPE_TYPE.line:
        case SHAPE_TYPE.arc:
        case SHAPE_TYPE.search_point:
          ["pt1","pt2","pt3"].forEach((key)=>{
            if(shape[key]===undefined)return;
            tmpDist = distance_point_point(shape[key],location);
            if(pt_info.dist>tmpDist)
            {
              pt_info.shape=shape;
              pt_info.key=key;
              pt_info.pt=shape[key];
              pt_info.dist = tmpDist;
            }
          });
        break;

        case SHAPE_TYPE.aux_point:
        {
          let point = this.auxPointParse(shape);
          tmpDist = distance_point_point(point,location);
          if(pt_info.dist>tmpDist)
          {
            pt_info.shape=shape;
            pt_info.key=undefined;
            pt_info.pt=point;
            pt_info.dist = tmpDist;
          }
        }
        break;

      }
    });
    return pt_info;
  }

  FindClosestInherentPointInfo( location,inherentShapeList)
  {
    let pt_info={
      pt:null,
      key:null,
      shape:null,
      dist:Number.POSITIVE_INFINITY
    };
    inherentShapeList.forEach((ishape)=>{
      if(ishape==null)return;
      if(ishape.type!=SHAPE_TYPE.aux_point)return;
      let point = this.auxPointParse(ishape);
      let tmpDist = distance_point_point(point,location);
      if(pt_info.dist>tmpDist)
      {
        pt_info.shape=ishape;
        pt_info.key=null;
        pt_info.pt=point;
        pt_info.dist = tmpDist;
      }

    });

    return pt_info;
  }

  auxPointParse(aux_point)
  {
    let point=null;
    if(aux_point.type!=SHAPE_TYPE.aux_point)return point;
    
    if(aux_point.ref.length ==1)
    {
      let ref0_shape=null;
      let idx = this.FindShape( "id" , aux_point.ref[0].id );
      
      if(idx ===undefined)
      {
        let idx = this.FindShape( "id" , aux_point.ref[0].id ,this.inherentShapeList);
        if(idx ===undefined)return null;
        ref0_shape=this.inherentShapeList[idx];
      }
      else
      {
        ref0_shape=this.shapeList[idx];
      }
      
      if(aux_point.ref[0].keyTrace !== undefined)
      {
        point = GetObjElement(ref0_shape,aux_point.ref[0].keyTrace) ;
        //point.ref = JSON.parse(JSON.stringify(aux_point.ref));//Deep copy
        //point.ref[0]._obj=ref0_shape;
      }
      else 
      {
        switch(ref0_shape.type)
        {
          case SHAPE_TYPE.arc:
          {
            let shape_arc = ref0_shape;
            let arc = threePointToArc(shape_arc.pt1,shape_arc.pt2,shape_arc.pt3);
            point = arc;
            point.ref = JSON.parse(JSON.stringify(aux_point.ref));//Deep copy
            point.ref[0]._obj=shape_arc;
          }
          break;
        }
      }
    }
    else if(aux_point.ref.length ==2)
    {
      let idx0 = this.FindShape( "id" , aux_point.ref[0].id );
      if(idx0 ===undefined)return null;
      let idx1 = this.FindShape( "id" , aux_point.ref[1].id );
      if(idx1 ===undefined)return null;
      
      let ref0_shape=this.shapeList[idx0];
      let ref1_shape=this.shapeList[idx1];
      if(ref0_shape.type == SHAPE_TYPE.line && ref1_shape.type == SHAPE_TYPE.line)
      {
        return intersectPoint(ref0_shape.pt1,ref0_shape.pt2,ref1_shape.pt1,ref1_shape.pt2);
      }

    }

    return point;
  }
  searchPointParse(search_point)
  {
    let point=null;
    if(search_point.type!=SHAPE_TYPE.search_point)return point;
    
    if(search_point.ref.length ==1)
    {
      let idx = this.FindShape( "id" , search_point.ref[0].id );
      if(idx ===undefined)return null;
      let ref0_shape=this.shapeList[idx];
      switch(ref0_shape.type)
      {
        case SHAPE_TYPE.line:
        {
          point = search_point.pt1;
        }
        break;
      }
    }

    return point;
  }
  xPointParse(xpoint)
  {
    let point=null;
    switch(aux_point.type)
    {
      case SHAPE_TYPE.aux_point:
        return this.auxPointParse(xpoint);
      break;
      case SHAPE_TYPE.search_point:
      {
        return this.searchPointParse(xpoint);
      }
      break;
    }
    return point;
  }


  fitCameraToShape(shape)
  {
    if(shape==null || shape===undefined)return;
    let center={x:0,y:0};
    let size=1;
    switch(shape.type)
    {
      case SHAPE_TYPE.line:
      center.x=(shape.pt1.x+shape.pt2.x)/2;
      center.y=(shape.pt1.y+shape.pt2.y)/2;
      break;
      case SHAPE_TYPE.arc:
      let arc = threePointToArc(shape.pt1,shape.pt2,shape.pt3);
      if(arc.r>500)
      {
        center.x=(shape.pt1.x+shape.pt3.x)/2;
        center.y=(shape.pt1.y+shape.pt3.y)/2;
      }
      else
      {
        center.x=arc.x;
        center.y=arc.y;
      }
        
      break;
      case SHAPE_TYPE.aux_point:
        let pt = this.auxPointParse(shape);
        if(pt ==null)return;
        center=pt;
        console.log(shape,pt);
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
      x:-center.x,
      y:-center.y
    });
  }


  drawInherentShapeList(ctx, inherentShapeList)
  {
    if(inherentShapeList===undefined || inherentShapeList ==null )return;
    inherentShapeList.forEach((ishape)=>{
      if(ishape==null)return;
      
      switch(ishape.type)
      {
        case SHAPE_TYPE.aux_point:
        {
          let point = this.auxPointParse(ishape);
          if(point!=null)
          {
            ctx.lineWidth=2;
            ctx.strokeStyle="black";  
            this.drawpoint(ctx,point,"rect")
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
  shapeProp(shape,prop)
  {
    switch(shape.type)
    {
      
      case SHAPE_TYPE.line:
      {
        let line = shape;
        switch(prop)
        {
          case "angle":
          {
            let dx=(line.pt2.x-line.pt1.x);
            let dy=(line.pt2.y-line.pt1.y);
            return Math.atan2(dy,dx);
          }
        }
      }
      break;
      case SHAPE_TYPE.aux_point:
      {
        
      }
      break;
    }
  }
  drawShapeList(ctx, eObjects,useShapeColor=true,skip_id_list=[])
  {
    eObjects.forEach((eObject)=>{
      if(eObject==null)return;
      var found = skip_id_list.find(function(skip_id) {
        return eObject.id == skip_id;
      });
      if(found!==undefined)
      {
        return;
      }
      else if(useShapeColor)
      {
        ctx.strokeStyle=eObject.color; 
      }
      
      
      switch(eObject.type)
      {
        case SHAPE_TYPE.line:
        {
          ctx.lineWidth=eObject.margin*2;
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x,y0:eObject.pt1.y,
            x1:eObject.pt2.x,y1:eObject.pt2.y,
          });


          let cnormal =LineCentralNormal(eObject);
          ctx.lineWidth=4;
          ctx.strokeStyle="rgba(100,50,100,0.8)"; 
          let marginOffset = eObject.margin+ctx.lineWidth/2;
          if(eObject.direction<0)
          {
            marginOffset=-marginOffset;
          }
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x+cnormal.vx*marginOffset,y0:eObject.pt1.y+cnormal.vy*marginOffset,
            x1:eObject.pt2.x+cnormal.vx*marginOffset,y1:eObject.pt2.y+cnormal.vy*marginOffset,
          });


          ctx.lineWidth=2;
          ctx.strokeStyle="gray"; 
          this.drawpoint(ctx,eObject.pt1);
          this.drawpoint(ctx,eObject.pt2);

        }
        break;
        
        case SHAPE_TYPE.aux_point:
        {
          
          let subObjs = eObject.ref
            .map((ref)=> this.FindShape( "id" , ref.id ))
            .map((idx)=>{  return idx>=0?this.shapeList[idx]:null});
          //console.log(eObject.ref);
          this.drawShapeList(ctx, subObjs,useShapeColor,skip_id_list);
          if(eObject.id === undefined)break;

          let point = this.auxPointParse(eObject);
          if(subObjs.length ==2 && subObjs[0].type == SHAPE_TYPE.line && subObjs[1].type == SHAPE_TYPE.line )
          {//Draw crosssect line
            ctx.setLineDash([5, 15]);
  
            ctx.beginPath();
            ctx.moveTo(point.x,point.y);
            ctx.lineTo(subObjs[0].pt1.x,subObjs[0].pt1.y);
            ctx.closePath();
            ctx.stroke();
  
            ctx.beginPath();
            ctx.moveTo(point.x,point.y);
            ctx.lineTo(subObjs[1].pt1.x,subObjs[1].pt1.y);
            ctx.closePath();
            ctx.stroke();
            ctx.setLineDash([]);
          }
          ctx.lineWidth=2;
          ctx.strokeStyle="gray"; 
          this.drawpoint(ctx, point);
        }
        break;
        
        
        
        case SHAPE_TYPE.arc:
        {
          //ctx.strokeStyle=eObject.color; 
          let arc = threePointToArc(eObject.pt1,eObject.pt2,eObject.pt3);
          ctx.lineWidth=eObject.margin*2; 
          this.drawReportArc(ctx, arc);
          
          ctx.lineWidth=4;
          ctx.strokeStyle="rgba(100,50,100,0.8)"; 
          
          let marginOffset = eObject.margin+ctx.lineWidth/2;
          if(eObject.direction<0)
          {
            marginOffset=-marginOffset;
          }
          arc.r+=marginOffset;
          this.drawReportArc(ctx, arc);

                
          ctx.lineWidth=2;
          ctx.strokeStyle="gray";  
          this.drawpoint(ctx,eObject.pt1);
          this.drawpoint(ctx,eObject.pt2);
          this.drawpoint(ctx,eObject.pt3);

        }
        break;

        case SHAPE_TYPE.search_point:
        {
          let subObjs = eObject.ref
            .map((ref)=> this.FindShape( "id" , ref.id ))
            .map((idx)=>{  return idx>=0?this.shapeList[idx]:null});
          
          if(subObjs[0]==null)break;

          let line = subObjs[0];

          let angle=this.shapeProp(line,"angle")+eObject.angle*Math.PI/180;
          let cnormal={x:Math.sin(angle),y:-Math.cos(angle)};
          let vector = {x:cnormal.y,y:-cnormal.x};
          let mag=eObject.width/2;//It starts from center so devide by 2.
          vector.x*=mag;
          vector.y*=mag;


          ctx.lineWidth=eObject.margin*2; 
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x-vector.x,y0:eObject.pt1.y-vector.y,
            x1:eObject.pt1.x+vector.x,y1:eObject.pt1.y+vector.y,
          });


          ctx.lineWidth=4;
          ctx.strokeStyle="rgba(100,50,100,0.8)"; 
          let marginOffset = eObject.margin+ctx.lineWidth/2;
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x-vector.x+cnormal.x*marginOffset,y0:eObject.pt1.y-vector.y+cnormal.y*marginOffset,
            x1:eObject.pt1.x+vector.x+cnormal.x*marginOffset,y1:eObject.pt1.y+vector.y+cnormal.y*marginOffset,
          });


          this.drawShapeList(ctx, subObjs,useShapeColor,skip_id_list);

          ctx.lineWidth=2;
          ctx.strokeStyle="gray";  
          this.drawpoint(ctx,eObject.pt1);

        }
        break;
      }
    });
  }

  drawReportJSON(context,Report,depth=0,draw_obj=null) {

    this.drawReportJSON_action(context,Report,(report_line_cir)=>{
      let Report = report_line_cir;
      let offset_pix = 0.5;
      let offset ={x: offset_pix + Report.cx, y:offset_pix + Report.cy};

      if(Array.isArray(Report.detectedLines))
        Report.detectedLines.forEach((line,idx)=>{
          if(draw_obj==null || draw_obj.line==line)
            this.drawReportLine(context, line, offset);
        });


      if(Array.isArray(Report.detectedCircles))
        Report.detectedCircles.forEach((circle,idx)=>{
          if(draw_obj==null || draw_obj.circle==circle)
            this.drawReportArc(context, circle, offset);
        });
    },depth=0);

  }

  drawReportJSON_closestPoint(ctx,Report,point,minDist=15,depth=0) {

    let closestDist=minDist+1;
    let selectedObject=null;
    let selectedFeature=null;
    let cpointInfo=null;

    this.drawReportJSON_action(ctx,Report,(report_line_cir)=>{
      let Report = report_line_cir;
      let offset = 0.5;
      let x_offset = offset + Report.cx;
      let y_offset = offset + Report.cy;

      if(Array.isArray(Report.detectedLines))
        Report.detectedLines.forEach((line,idx)=>{

          let line_={
            x1:line.x0+x_offset,
            y1:line.y0+y_offset,
            x2:line.x1+x_offset,
            y2:line.y1+y_offset};
          let retDist = distance_line_point(line_, point);
          if(retDist.dist>minDist)
          {
            return;
          }
          if(closestDist>retDist.dist)
          {
            closestDist = retDist.dist;
            selectedObject = Report;
            selectedFeature = {line:line};
            cpointInfo = retDist;
          }

        });


      if(Array.isArray(Report.detectedCircles))
        Report.detectedCircles.forEach((circle,idx)=>{
          let arc={
            x:circle.x+x_offset,
            y:circle.y+y_offset,
            r:circle.r,
            angleFrom:0,
            angleTo:2*Math.PI-0.0001};
          let retDist = distance_arc_point(arc, point);
          if(retDist.dist>minDist)
          {
            return;
          }
          if(closestDist>retDist.dist)
          {
            closestDist = retDist.dist;
            selectedObject = Report;
            selectedFeature = {circle:circle};
            cpointInfo = retDist;
          }


        });
    },depth=0);

    return {
      obj:selectedObject,
      feature:selectedFeature,
      measure:cpointInfo
    };

  }


  VecX2DMat(vec,mat)
  {

    let XX= vec.x * mat.a + vec.y * mat.c + mat.e;
    let YY= vec.x * mat.b + vec.y * mat.d + mat.f;
    return {x:XX,y:YY};
  }

  getReportCenter()
  {
    
    let center = {x:0,y:0};
    try{
      //console.log(this.ReportJSON);
  
      center.x = this.ReportJSON.reports[0].cx;
      center.y = this.ReportJSON.reports[0].cy;

    }catch(e)
    {
      center.x = 0;//(this.secCanvas.width / 2)
      center.y = 0;//(this.secCanvas.height / 2)
    }

    return center;
  }

  worldTransform()
  {
    let wMat = new DOMMatrix();

    wMat.translateSelf((this.canvas.width / 2), (this.canvas.height / 2));
    this.camera.CameraTransform(wMat);
    //let center = this.getReportCenter();
    //wMat.translateSelf(-center.x, -center.y);

    return wMat;

  }
 
  draw()
  {
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = 2;
    ctx.setTransform(this.identityMat);  
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix  = this.worldTransform();
    ctx.setTransform(matrix);  
    


    {
      let center = this.getReportCenter();
      ctx.drawImage(this.secCanvas,-center.x,-center.y);
    }



    {


      this.Mouse2SecCanvas = matrix.invertSelf();
      let invMat =this.Mouse2SecCanvas;
      //this.Mouse2SecCanvas = invMat;
      let mPos = this.mouseStatus;
      let mouseOnCanvas2=this.VecX2DMat(mPos,invMat);
  

      if(false && typeof this.ReportJSON !=='undefined')
      {
        ctx.strokeStyle = '#a00080';
        this.drawReportJSON(ctx,this.ReportJSON);
      }

      if(false && typeof this.ReportJSON !=='undefined')
      {
        var ret = this.drawReportJSON_closestPoint(ctx,this.ReportJSON,mouseOnCanvas2,this.mouse_close_dist/this.camera.scale);

        if(ret.measure!=null)
        {
          let scaleF = this.camera.scale;
          if(scaleF>1)scaleF=1;
          let boxW=5/scaleF;
          ctx.fillStyle = '#999999';
          //ctx.fillRect(ret.measure.x-boxW/2,ret.measure.y-boxW/2, boxW, boxW);
          ctx.beginPath();
          ctx.arc(ret.measure.x,ret.measure.y,boxW, 0,2*Math.PI);
          ctx.fill();


          ctx.strokeStyle = '#0000FF';

          this.drawReportJSON(ctx,ret.obj);

          ctx.lineWidth = 3/scaleF;
          ctx.strokeStyle = '#FF0000';
          this.drawReportJSON(ctx,ret.obj,0,ret.feature) 
          this.near_select_obj = ret;
        }
        else
        {
          this.near_select_obj = null;
        }



      }



    }
    ctx.closePath();
    ctx.save();

    
    let skipDrawIdxs=[];
    if(this.EditShape!=null)
    {
      skipDrawIdxs.push(this.EditShape.id);
      
      ctx.strokeStyle=this.colorSet.editShape;
      this.drawShapeList(ctx, [this.EditShape],false);
    }

    if(this.CandEditPointInfo!=null)
    {
      var candPtInfo = this.CandEditPointInfo;
      var found = skipDrawIdxs.find(function(skip_id) {
        return candPtInfo.shape.id == skip_id;
      }.bind(this));
      
      if( found===undefined )
      {
        skipDrawIdxs.push(candPtInfo.shape.id);

        ctx.strokeStyle="rgba(255,0,255,0.5)";
        this.drawShapeList(ctx, [candPtInfo.shape],false);
      }
    }
    this.drawShapeList(ctx, this.shapeList,true,skipDrawIdxs);
    this.drawInherentShapeList(ctx, this.inherentShapeList);


    if(this.EditPoint!=null)
    {
      ctx.lineWidth=3;
      ctx.strokeStyle="green";  
      this.drawpoint(ctx, this.EditPoint);
    }



    if(this.CandEditPointInfo!=null)
    {
      ctx.lineWidth=3;
      ctx.strokeStyle="rgba(0,255,0,0.3)";  
      this.drawpoint(ctx, this.CandEditPointInfo.pt);
    }



  }


  ctrlLogic()
  {
    

    let wMat = this.worldTransform();
    //console.log("this.camera.matrix::",wMat);
    let worldTransform = new DOMMatrix().setMatrixValue(wMat);
    let worldTransform_inv = worldTransform.invertSelf();
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2=this.VecX2DMat(mPos,worldTransform_inv);

    let pmPos = {x:this.mouseStatus.px,y:this.mouseStatus.py};
    let pmouseOnCanvas2=this.VecX2DMat(pmPos,worldTransform_inv);

    let ifOnMouseLeftClickEdge = (this.mouseStatus.status!=this.mouseStatus.pstatus);
    



    switch(this.state)
    {
      case UI_SM_STATES.EDIT_MODE_LINE_CREATE:
      {
        
        if(this.mouseStatus.status==1)
        {
          this.EditShape={
            type:SHAPE_TYPE.line,
            pt1:mouseOnCanvas2,
            pt2:pmouseOnCanvas2,
            margin:5,
            color:this.colorSet.unselected,
            direction:1
          };
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.SetShape( this.EditShape);
            
            this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_SUCCESS});
            
          }
        }
        break;
      }      
      
      case UI_SM_STATES.EDIT_MODE_ARC_CREATE:
      {
        
        if(this.mouseStatus.status==1)
        {
          let cnormal =LineCentralNormal({
            pt1:mouseOnCanvas2,
            pt2:pmouseOnCanvas2,
          });
  
          this.EditShape={
            type:SHAPE_TYPE.arc,
            pt1:mouseOnCanvas2,
            pt2:{
              x:cnormal.x+cnormal.vx,
              y:cnormal.y+cnormal.vy},
            pt3:pmouseOnCanvas2,
            margin:5,
            color:this.colorSet.unselected,
            direction:1
          };
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.SetShape( this.EditShape);
            
            this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_SUCCESS});
            
          }
        }
        break;
      }


      case UI_SM_STATES.EDIT_MODE_SEARCH_POINT_CREATE:
      case UI_SM_STATES.EDIT_MODE_AUX_POINT_CREATE:
      {
        
        if(this.mouseStatus.status==1)
        {
          if(ifOnMouseLeftClickEdge && this.CandEditPointInfo!=null)
          {
            if(this.state == UI_SM_STATES.EDIT_MODE_SEARCH_POINT_CREATE){
              
              this.EditShape={
                type:SHAPE_TYPE.search_point,
                pt1:{x:0,y:0},
                angle:90,
                margin:10,
                width:40,
                ref:[{
                  id:this.CandEditPointInfo.shape.id,
                  element:this.CandEditPointInfo.shape.type
                }],
                color:this.colorSet.unselected,
              };
              
              this.SetShape( this.EditShape);
              this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_SUCCESS});
            }
            else
            {
              console.log(ifOnMouseLeftClickEdge,this.CandEditPointInfo);
              this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Ele_Cand_Update,data:this.CandEditPointInfo});
  
            }
          }
        }
        else
        {

          let pt_info = this.FindClosestCtrlPointInfo( mouseOnCanvas2);
          let pt_info2 = this.FindClosestInherentPointInfo( mouseOnCanvas2,this.inherentShapeList);
          if(pt_info.dist>pt_info2.dist)
          {
            pt_info = pt_info2;
          }
          if(pt_info.pt!=null&& pt_info.dist<this.mouse_close_dist/this.camera.GetCameraScale())
          {
            this.CandEditPointInfo=pt_info;
          }
          else
          {
            this.CandEditPointInfo=null;
          }
        }
        break;
      }


      case UI_SM_STATES.EDIT_MODE_SHAPE_EDIT:
      {
        
        if(this.mouseStatus.status==1)
        {
          if(ifOnMouseLeftClickEdge)
          {
            if(this.CandEditPointInfo!=null)
            {
              {
                //if(this.EditShape=null ||(this.EditShape!=null && this.EditShape.type!=SHAPE_TYPE.aux_point))
                {
                  let pt_info = this.CandEditPointInfo;
                  this.CandEditPointInfo=null;
                  this.EditShape=JSON.parse(JSON.stringify(pt_info.shape));//Deep copy
                  this.EditPoint=this.EditShape[pt_info.key];
                  this.tmp_EditShape_id = this.EditShape.id;
                  this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
                }
                //else
                {
                  //this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Ele_Cand_Update,data:this.CandEditPointInfo});

                }
              }
            }
            else
            {
              this.EditPoint=null;
              this.EditShape=null;
              this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
            
            }
            
          }
          else
          {
            if(this.EditPoint!=null)
            {
              this.EditPoint.x = mouseOnCanvas2.x;
              this.EditPoint.y = mouseOnCanvas2.y;
              this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
            }
          }
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.SetShape(this.EditShape,this.EditShape.id);
          }

          let pt_info = this.FindClosestCtrlPointInfo( mouseOnCanvas2);
          let pt_info2 = this.FindClosestInherentPointInfo( mouseOnCanvas2,this.inherentShapeList);
          if(pt_info.dist>pt_info2.dist)
          {
            pt_info = pt_info2;
          }

          if(pt_info.pt!=null&& pt_info.dist<this.mouse_close_dist/this.camera.GetCameraScale())
          {
            this.CandEditPointInfo=pt_info;
          }
          else
          {
            this.CandEditPointInfo=null;
          }
        }
        break;
      }
    }
    //ctx.restore();

    this.mouseStatus.pstatus = this.mouseStatus.status;
    
  }
}


export default { EverCheckCanvasComponent }
