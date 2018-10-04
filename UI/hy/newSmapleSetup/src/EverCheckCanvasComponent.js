//UIControl

import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import {distance_arc_point,distance_point_point,threePointToArc,distance_line_point} from 'UTIL/MathTools';

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

    this.canvas.addEventListener('wheel',function(event){
      this.onmouseswheel(event);
      return false; 
    }.bind(this), false);

    this.mouseStatus={x:-1,y:-1,px:-1,py:-1,status:0,pstatus:0};

    this.ReportJSON={};

    this.secCanvas_rawImg=null;

    this.secCanvas = document.createElement('canvas');

    this.transformMat= new  DOMMatrix();
    this.cameraMat= new  DOMMatrix();
    this.dragMat= new  DOMMatrix();
    this.Mouse2SecCanvas= new  DOMMatrix();


    this.mouse_close_dist= 10;
    this.camera={
      scale: 1,
      scaleCenter:{x:0,y:0},
      rotate: 0,
      translate: {x:0,y:0,dx:0,dy:0}
    };

    this.near_select_obj=null;

    this.onfeatureselected=(ev)=>{};
    
    this.state=UI_SM_STATES.NEUTRAL;
    this.shapeList=[];
    
    this.EditShape=null;
    this.EditShape_color="rgba(255,0,0,0.7)";
    
    this.ShapeCount=0;
    this.EditPoint=null;
    
    this.EmitEvent=(event)=>{console.log(event);};
  }

  SetState(state)
  {
    if(this.state!=state)
    {
      this.state=state;
      this.EditShape=null;
      this.EditPoint=null;
      this.near_select_obj=null;
    }
  }
  SetReport( report )
  {

    if(report == this.ReportJSON)return;
    this.ReportJSON = report;
    //this.draw();
  }
  
  FindShapeIdx( id )
  {
    if(id>=0)
    {
      for(let i=0;i<this.shapeList.length;i++)
      {
        if(this.shapeList[i].id == id)
        {
          return i;
        }
      }
    }
    return -1;
  }
  SetShape( shape_obj, id=-1 )
  {
    let shape = null;

    if(shape_obj == null)//For delete
    {
      if( id>=0)
      {
        let tmpIdx = this.FindShapeIdx( id );
        console.log("SETShape>",tmpIdx);
        if(tmpIdx>=0)
        {
          shape = this.shapeList[tmpIdx];
          this.shapeList.splice(tmpIdx, 1);
          
          this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Shape_List_Update,data:this.shapeList});
        }
      }
      if(this.EditShape.id == id)
        this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:{}});
      return shape;
    }

    console.log("SETShape>",this.shapeList,shape_obj,id);
          
    //shape_obj.color="rgba(100,0,100,0.5)";
    let targetIdx=-1;
    if(id>=0)
    {
      let tmpIdx = this.FindShapeIdx( id );
      console.log("SETShape>",tmpIdx);
      if(tmpIdx>=0)
      {
        shape = this.shapeList[tmpIdx];
        targetIdx = tmpIdx;
      }
    }
    else{
      this.ShapeCount++;
      id = this.ShapeCount;
    }

    console.log("SETShape>",shape);
    if(shape == null)
    {
      shape = Object.assign({id:id},shape_obj);
      this.shapeList.push(shape);

      
      this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Shape_List_Update,data:this.shapeList});
    }
    else
    {
      shape = Object.assign({id:id},shape_obj);
      if(targetIdx!=-1)
      {
        this.shapeList[targetIdx] = shape;
        this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Shape_List_Update,data:this.shapeList});
      }
    }
    
    if(this.EditShape.id == id)
      this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:shape});
    return shape;

  }
  SetImg( img )
  {
    if(img == null || img == this.secCanvas_rawImg)return;
    console.log(img);
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


    this.cameraMat.scaleSelf(scale,scale);
    this.camera.scale*=scale;
    if(this.camera.scale<0.1)this.camera.scale=0.1;
    else if(this.camera.scale>10)this.camera.scale=10;
    this.draw();
  }
  onmousemove(evt)
  {
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;

    //console.log("this.state:"+this.state+"  "+this.mouseStatus.status);
    switch(this.state)
    {
      case UI_SM_STATES.EDIT_MODE_SHAPE_EDIT:
        
        if(this.EditPoint!=null)break;
      case UI_SM_STATES.EDIT_MODE_NEUTRAL:
        //console.log("this.state:>>>");
        if(this.mouseStatus.status==1)
        {
          this.setDOMMatrixIdentity(this.dragMat);
          this.dragMat.translateSelf(pos.x-this.mouseStatus.px,pos.y-this.mouseStatus.py);


          this.camera.translate.dx=(pos.x-this.mouseStatus.px)/this.camera.scale;
          this.camera.translate.dy=(pos.y-this.mouseStatus.py)/this.camera.scale;

          this.rotate2d_dxy(this.camera.translate, this.camera.translate, -this.camera.rotate);
        }
      break;
    }

    //this.setDOMMatrixIdentity(this.cameraMat);
    //this.cameraMat.translateSelf(pos.x,pos.y);


    this.camera.scaleCenter.x = pos.x-this.canvas.width/2;
    this.camera.scaleCenter.y = pos.y-this.canvas.width/2;
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
    this.draw();
  }

  onmouseup(evt)
  {
    console.log("onmouseup");
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 0;

    this.camera.translate.x+=this.camera.translate.dx;
    this.camera.translate.y+=this.camera.translate.dy;
    this.camera.translate.dx = 0;
    this.camera.translate.dy = 0;

    this.cameraMat.multiplySelf(this.dragMat);
    this.setDOMMatrixIdentity(this.dragMat);

    this.draw();
  }

  resize(width,height)
  {
    this.canvas.width=width;
    this.canvas.height=height;
    this.draw();
  }


  rotate2d_dxy(coord_dst, coord_src, theta) {
    let sin_v = Math.sin(theta);
    let cos_v = Math.cos(theta);
    let tmp_dx= coord_src.dx;

    coord_dst.dx = tmp_dx * cos_v - coord_src.dy * sin_v;
    coord_dst.dy = tmp_dx * sin_v + coord_src.dy * cos_v;
  }

  getCameraMat()
  {
    return this.cameraMat;
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

  drawpoint(ctx, point,size=5)
  {
    ctx.beginPath();
    ctx.strokeRect(point.x-size/2,point.y-size/2, size, size);
    //ctx.closePath();
    ctx.stroke();
  }
  drawEditObject(ctx, eObjects)
  {
    eObjects.forEach((eObject)=>{
      if(eObject==null)return;
      if(this.EditShape!=null && eObject.id == this.EditShape.id)
      {
        ctx.strokeStyle=this.EditShape_color; 
      }
      else
      {
        ctx.strokeStyle=eObject.color; 
      }
      switch(eObject.type)
      {
        case 'line':


        ctx.lineWidth=eObject.margin*2;
        this.drawReportLine(ctx, {
          x0:eObject.pt1.x,y0:eObject.pt1.y,
          x1:eObject.pt2.x,y1:eObject.pt2.y,
        });
        ctx.lineWidth=2;
        ctx.strokeStyle="black";  
        this.drawpoint(ctx, eObject.pt1);
        this.drawpoint(ctx, eObject.pt2);

        break;
        
        
        case 'arc':
        
          //ctx.strokeStyle=eObject.color; 
          let arc = threePointToArc(eObject.pt1,eObject.pt2,eObject.pt3);
          ctx.lineWidth=eObject.margin*2; 
          this.drawReportArc(ctx, arc);
          
          ctx.lineWidth=2;
          ctx.strokeStyle="black";  
          this.drawpoint(ctx, eObject.pt1);
          this.drawpoint(ctx, eObject.pt2);
          this.drawpoint(ctx, eObject.pt3);
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


  setDOMMatrixIdentity(mat)
  {
    mat.a=1;
    mat.b=0;
    mat.c=0;
    mat.d=1;
    mat.e=0;
    mat.f=0;
  }


  VecX2DMat(vec,mat)
  {

    let XX= vec.x * mat.a + vec.y * mat.c + mat.e;
    let YY= vec.x * mat.b + vec.y * mat.d + mat.f;
    return {x:XX,y:YY};
  }

  worldTransform()
  {
    let ctx = this.canvas.getContext('2d');

    ctx.setTransform(1,0,0,1,0,0); 

    ctx.translate((this.canvas.width / 2), (this.canvas.height / 2));

    //mat.multiplySelf(this.getCameraMat());
    //mat.multiplySelf(this.dragMat);

    //ctx.translate(this.camera.scaleCenter.x, this.camera.scaleCenter.y);

    ctx.scale(this.camera.scale, this.camera.scale);
    //ctx.translate(-this.camera.scaleCenter.x/this.camera.scale, -this.camera.scaleCenter.y/this.camera.scale);

    ctx.rotate(this.camera.rotate);

    ctx.translate(this.camera.translate.x+this.camera.translate.dx, this.camera.translate.y+this.camera.translate.dy);

    ctx.translate(-(this.secCanvas.width / 2), -(this.secCanvas.height / 2));

    return ctx.getTransform();

  }
 
  draw()
  {
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = 2;
    ctx.setTransform(1,0,0,1,0,0); 
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    this.Mouse2SecCanvas = this.worldTransform().invertSelf();

    let invMat =this.Mouse2SecCanvas;
    //this.Mouse2SecCanvas = invMat;
    let mPos = this.mouseStatus;
    let mouseOnCanvas2=this.VecX2DMat(mPos,invMat);

    {



      ctx.drawImage(this.secCanvas,0,0);

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
    



    
    if(this.mouseStatus.status==1)
    {
      let pmPos = {x:this.mouseStatus.px,y:this.mouseStatus.py};
      let pmouseOnCanvas2=this.VecX2DMat(pmPos,invMat);

      if(this.state == UI_SM_STATES.EDIT_MODE_LINE_CREATE)
      {
        let line_obj = {x0:mouseOnCanvas2.x,y0:mouseOnCanvas2.y,
                        x1:pmouseOnCanvas2.x,y1:pmouseOnCanvas2.y,};
        
        this.EditShape=line_obj;

        
        this.EditShape={
          type:"line",
          pt1:mouseOnCanvas2,
          pt2:pmouseOnCanvas2,
          margin:5,
          color:"rgba(255,0,0,0.5)"
        };

        this.drawEditObject(ctx, [this.EditShape]);
      }
      else if(this.state == UI_SM_STATES.EDIT_MODE_ARC_CREATE)
      {
        let midPosition = {
          x:(mouseOnCanvas2.x+pmouseOnCanvas2.x)/2,
          y:(mouseOnCanvas2.y+pmouseOnCanvas2.y)/2};//Find middle
        midPosition.x+=(mouseOnCanvas2.y-pmouseOnCanvas2.y)/1000;
        midPosition.y+=-(mouseOnCanvas2.x-pmouseOnCanvas2.x)/1000;
        //Add normal vec to make sure it's not exactly on straight line

        
        this.EditShape={
          type:"arc",
          pt1:mouseOnCanvas2,
          pt2:midPosition,
          pt3:pmouseOnCanvas2,
          margin:5,
          color:"rgba(255,0,0,0.5)"
        };
        this.drawEditObject(ctx, [this.EditShape]);
      }      
      else if(this.state == UI_SM_STATES.EDIT_MODE_SHAPE_EDIT)
      {
        if(this.mouseStatus.pstatus==0)
        {
          console.log(">>>>>>>>>>",this.EditShape);
          this.EditPoint=null;
          let EditShape_tmp=null;
          let minDist=Number.POSITIVE_INFINITY;
  
          this.shapeList.forEach((shape)=>{
            let tmpDist;
            switch(shape.type)
            {
              case "line":
              tmpDist = distance_point_point(shape.pt1,mouseOnCanvas2);
              if(minDist>tmpDist)
              {
  
                EditShape_tmp=shape;
                minDist = tmpDist;
                this.EditPoint = shape.pt1;
              }
              tmpDist = distance_point_point(shape.pt2,mouseOnCanvas2);
              if(minDist>tmpDist)
              {
                EditShape_tmp=shape;
                minDist = tmpDist;
                this.EditPoint = shape.pt2;
              }
              break;
              
              case "arc":
              
              tmpDist = distance_point_point(shape.pt1,mouseOnCanvas2);
              if(minDist>tmpDist)
              {
  
                EditShape_tmp=shape;
                minDist = tmpDist;
                this.EditPoint = shape.pt1;
              }
              tmpDist = distance_point_point(shape.pt2,mouseOnCanvas2);
              if(minDist>tmpDist)
              {
  
                EditShape_tmp=shape;
                minDist = tmpDist;
                this.EditPoint = shape.pt2;
              }
              tmpDist = distance_point_point(shape.pt3,mouseOnCanvas2);
              if(minDist>tmpDist)
              {
                EditShape_tmp=shape;
                minDist = tmpDist;
                this.EditPoint = shape.pt3;
              }
              break;
            }
          });
  
          if(this.EditPoint!=null&& minDist<this.mouse_close_dist/this.camera.scale)
          {
            
            ctx.lineWidth=3;
            ctx.strokeStyle="green";  
            this.drawpoint(ctx, this.EditPoint);
          }
          else
          {
            this.EditPoint=null;
  
            EditShape_tmp=null;
          }
          
          if(this.EditShape!=EditShape_tmp)
          {
            this.EditShape=EditShape_tmp;
            this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
          }
        }
        else
        {
          if(this.EditPoint!=null)
          {
            this.EditPoint.x = mouseOnCanvas2.x;
            this.EditPoint.y = mouseOnCanvas2.y;
          }
          this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
        }
      }
    }
    else
    {
      if(this.state == UI_SM_STATES.EDIT_MODE_LINE_CREATE || this.state == UI_SM_STATES.EDIT_MODE_ARC_CREATE)
      {
        if(this.EditShape!=null)
        {
          this.EditShape.color="rgba(100,0,100,0.5)";
          this.SetShape( this.EditShape);
          
          this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_SUCCESS});
          
        }
        this.EditShape=null;
      }
      else if(this.state == UI_SM_STATES.EDIT_MODE_SHAPE_EDIT)
      {

      }
      
      
    }

    this.drawEditObject(ctx, this.shapeList);

    ctx.restore();

    this.mouseStatus.pstatus = this.mouseStatus.status;
    
  }
}


export default { EverCheckCanvasComponent }
