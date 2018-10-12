//UIControl

import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import {distance_arc_point,distance_point_point,threePointToArc,distance_line_point,LineCentralNormal} from 'UTIL/MathTools';

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
    this.dragMat= new  DOMMatrix();
    this.Mouse2SecCanvas= new  DOMMatrix();


    this.mouse_close_dist= 10;
    this.camera={
      scale: 1,
      scaleCenter:{x:0,y:0},
      rotate: 0,
      translate: {x:0,y:0,dx:0,dy:0},
      matrix:new DOMMatrix(),
      tmpMatrix:new DOMMatrix(),
    };

    this.near_select_obj=null;

    this.onfeatureselected=(ev)=>{};
    
    this.state=UI_SM_STATES.NEUTRAL;
    this.shapeList=[];
    
    this.EditShape=null;
    this.EditShape_color="rgba(255,0,0,0.7)";
    
    this.CandEditPointInfo=null;
    this.EditPoint=null;
    
    this.EmitEvent=(event)=>{console.log(event);};
  }

  SetState(state)
  {
    if(this.state!=state)
    {
      this.state=state;
      //this.EditShape=null;
      //this.EditPoint=null;
      this.near_select_obj=null;
    }
  }
  SetReport( report )
  {

    if(report == this.ReportJSON)return;
    this.ReportJSON = report;
    //this.draw();
  }
  

  FindShape( key , val )
  {
    for(let i=0;i<this.shapeList.length;i++)
    {
      if(this.shapeList[i][key] == val)
      {
        return i;
      }
    }
    return -1;
  }

  SetShape( shape_obj, id=-1 )
  {
    this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Shape_Set,data:{shape:shape_obj,id:id}});
  }

  SetEditShape( EditShape )
  {
      this.EditShape = EditShape;
  }

  
  SetShapeList( shapeList )
  {
    this.shapeList = shapeList;
    //console.log("this.EditShape.id:",this.EditShape);
    if(this.EditShape!=null && this.EditShape.id!==undefined)
    {
      let idx = this.FindShape( "id" , this.EditShape.id );
      if(idx ==-1)
      {
        this.EditShape=null;
        this.EditPoint=null;
      }
    }
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

    
    //this.camera.matrix.translateSelf(this.mouseStatus.x-(this.canvas.width / 2),this.mouseStatus.y-(this.canvas.height / 2));
    //
    let mat = new DOMMatrix();
    mat.translateSelf((this.mouseStatus.x-(this.canvas.width / 2)),(this.mouseStatus.y-(this.canvas.height / 2)));
    mat.scaleSelf(scale,scale);
    mat.translateSelf(-(this.mouseStatus.x-(this.canvas.width / 2)),-(this.mouseStatus.y-(this.canvas.height / 2)));

    this.camera.matrix.preMultiplySelf(mat);
    this.camera.scale*=scale;
    if(this.camera.scale<0.1)this.camera.scale=0.1;
    else if(this.camera.scale>10)this.camera.scale=10;
    this.ctrlLogic();
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
          this.camera.tmpMatrix.setMatrixValue(this.identityMat);
          this.camera.tmpMatrix.translateSelf(pos.x-this.mouseStatus.px,pos.y-this.mouseStatus.py);

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

    this.camera.matrix.preMultiplySelf(this.camera.tmpMatrix);
    this.camera.tmpMatrix.setMatrixValue(this.identityMat);
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
    this.ctrlLogic();
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
    ctx.arc(point.x,point.y,size/2,0,Math.PI*2, false);
    //ctx.closePath();
    ctx.stroke();
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
    });
    return pt_info;


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
        case 'line':
        {
          ctx.lineWidth=eObject.margin*2;
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x,y0:eObject.pt1.y,
            x1:eObject.pt2.x,y1:eObject.pt2.y,
          });


          let cnormal =LineCentralNormal(eObject);
          ctx.lineWidth=2;
          ctx.strokeStyle="rgba(0,100,100,0.4)"; 
          let marginOffset = eObject.margin+1;
          if(eObject.direction<0)
          {
            marginOffset=-marginOffset;
          }
          this.drawReportLine(ctx, {
            x0:eObject.pt1.x+cnormal.vx*marginOffset,y0:eObject.pt1.y+cnormal.vy*marginOffset,
            x1:eObject.pt2.x+cnormal.vx*marginOffset,y1:eObject.pt2.y+cnormal.vy*marginOffset,
          });




          ctx.lineWidth=2;
          ctx.strokeStyle="black";  
          this.drawpoint(ctx, eObject.pt1);
          this.drawpoint(ctx, eObject.pt2);
        }
        break;
        
        
        case 'arc':
        {
          //ctx.strokeStyle=eObject.color; 
          let arc = threePointToArc(eObject.pt1,eObject.pt2,eObject.pt3);
          ctx.lineWidth=eObject.margin*2; 
          this.drawReportArc(ctx, arc);
          
          ctx.lineWidth=2;
          ctx.strokeStyle="rgba(0,100,100,0.4)"; 
          
          let marginOffset = eObject.margin+1;
          if(eObject.direction<0)
          {
            marginOffset=-marginOffset;
          }
          arc.r+=marginOffset;
          this.drawReportArc(ctx, arc);

          ctx.lineWidth=2;
          ctx.strokeStyle="black";  
          this.drawpoint(ctx, eObject.pt1);
          this.drawpoint(ctx, eObject.pt2);
          this.drawpoint(ctx, eObject.pt3);
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

  worldTransform()
  {
    let wMat = new DOMMatrix();
    wMat.translateSelf((this.canvas.width / 2), (this.canvas.height / 2));
    wMat.multiplySelf(this.camera.tmpMatrix);
    wMat.multiplySelf(this.camera.matrix);
    
    /*wMat.scaleSelf(this.camera.scale, this.camera.scale);
    wMat.rotateSelf(this.camera.rotate);
    wMat.translateSelf(this.camera.translate.x+this.camera.translate.dx, this.camera.translate.y+this.camera.translate.dy);*/

    wMat.translateSelf(-(this.secCanvas.width / 2), -(this.secCanvas.height / 2));

    return wMat;

  }
 
  draw()
  {
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = 2;
    ctx.setTransform(1,0,0,1,0,0); 
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix  = this.worldTransform();
    ctx.setTransform(
      matrix.a,
      matrix.b,
      matrix.c,
      matrix.d,
      matrix.e,
      matrix.f);  
    


    ctx.drawImage(this.secCanvas,0,0);
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
    
    if(this.EditShape!=null)
    {
      this.drawShapeList(ctx, this.shapeList,true,[this.EditShape.id]);

      ctx.strokeStyle=this.EditShape_color;
      this.drawShapeList(ctx, [this.EditShape],false);
    }
    else
    {
      this.drawShapeList(ctx, this.shapeList);
    }


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
            type:"line",
            pt1:mouseOnCanvas2,
            pt2:pmouseOnCanvas2,
            margin:5,
            color:"rgba(255,0,0,0.5)",
            direction:1
          };
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.EditShape.color="rgba(100,0,100,0.5)";
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
            type:"arc",
            pt1:mouseOnCanvas2,
            pt2:{
              x:cnormal.x+cnormal.vx,
              y:cnormal.y+cnormal.vy},
            pt3:pmouseOnCanvas2,
            margin:5,
            color:"rgba(255,0,0,0.5)",
            direction:1
          };
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.EditShape.color="rgba(100,0,100,0.5)";
            this.SetShape( this.EditShape);
            
            this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_SUCCESS});
            
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
                let pt_info = this.CandEditPointInfo;
                this.CandEditPointInfo=null;
                this.EditShape=JSON.parse(JSON.stringify(pt_info.shape));//Deep copy
                this.EditPoint=this.EditShape[pt_info.key];
                this.EmitEvent({type:UI_SM_EVENT.EDIT_MODE_Edit_Tar_Update,data:this.EditShape});
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

          if(pt_info.pt!=null&& pt_info.dist<this.mouse_close_dist/this.camera.scale)
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
