//UIControl

import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';
import {
  threePointToArc,
  intersectPoint,
  LineCentralNormal,
  closestPointOnLine} from 'UTIL/MathTools';

  import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';

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


class renderUTIL
{
  constructor(editor_db_obj)
  {
    this.setEditor_db_obj(editor_db_obj);
    this.colorSet={
      unselected:"rgba(100,0,100,0.5)",
      inspection_Pass:"rgba(0,255,0,0.1)",
      inspection_production_Fail:"rgba(128,128,0,0.1)",
      inspection_Fail:"rgba(255,0,0,0.1)",
      inspection_NA:"rgba(128,128,128,0.1)",
      editShape:"rgba(255,0,0,0.7)",
      measure_info:"rgba(128,128,200,0.7)"
    };
  }

  setEditor_db_obj(editor_db_obj)
  {
    this.db_obj = editor_db_obj;
  }

  setColorSet(colorset)
  {
    this.colorSet=colorset;
  }
  drawReportLine(ctx, line_obj, offset={x:0,y:0})
  {
    ctx.beginPath();
    ctx.moveTo(line_obj.x0+offset.x,line_obj.y0+offset.y);
    ctx.lineTo(line_obj.x1+offset.x,line_obj.y1+offset.y);
    //ctx.closePath();
    ctx.stroke();
  }


  drawLine(ctx, line, offset={x:0,y:0})
  {
    ctx.beginPath();
    ctx.moveTo(line.pt1.x+offset.x,line.pt1.y+offset.y);
    ctx.lineTo(line.pt2.x+offset.x,line.pt2.y+offset.y);
    //ctx.closePath();
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
    //ctx.closePath();
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

  drawInherentShapeList(ctx, inherentShapeList)
  {
    if(inherentShapeList===undefined || inherentShapeList ==null )return;
    
    inherentShapeList.forEach((ishape)=>{
      if(ishape==null)return;
      
      switch(ishape.type)
      {
        case SHAPE_TYPE.aux_point:
        {
          let point = this.db_obj.auxPointParse(ishape);
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
  canvas_arrow(ctx, fromx, fromy, tox, toy,headlen = 10,aangle=Math.PI/6){
    var angle = Math.atan2(toy-fromy,tox-fromx);
    ctx.beginPath();
    ctx.moveTo(fromx, fromy);
    ctx.lineTo(tox, toy);
    
    ctx.moveTo(tox, toy);
    ctx.lineTo(tox-headlen*Math.cos(angle-aangle),toy-headlen*Math.sin(angle-aangle));
    ctx.moveTo(tox, toy);
    ctx.lineTo(tox-headlen*Math.cos(angle+aangle),toy-headlen*Math.sin(angle+aangle));
    ctx.stroke();
  }
  
  drawArcArrow(ctx,x,y,r,sAngle,eAngle,ccw=false)
  {
    ctx.beginPath();
    //console.log(ctx,x,y,r,sAngle,eAngle,ccw);
    ctx.arc(x,y,r,sAngle,eAngle,ccw);
    ctx.stroke();
    let ax= Math.cos(eAngle);
    let ay= Math.sin(eAngle);
    x+=r*ax;
    y+=r*ay;
    let dirSign=(ccw)?-1:1;
    this.canvas_arrow(ctx, x+dirSign*ay, y-dirSign*ax, x, y);
    
  }
  drawLineArrow(ctx,x1,y1,x2,y2)
  {
  }

  drawMeasureDistance(ctx,eObject,refObjs,shapeList,unitConvert)
  {
    ctx.lineWidth=2;
              
    ctx.font="30px Arial";

    let alignLine=null;
    let point_onAlignLine=null;
    let point=null;

    
    let db_obj = this.db_obj;
    point_onAlignLine = db_obj.shapeMiddlePointParse(refObjs[0],shapeList);
    point = db_obj.shapeMiddlePointParse(refObjs[1],shapeList);
    
    let mainObjVec= db_obj.shapeVectorParse(refObjs[0],shapeList);
    if(mainObjVec===undefined)
    {
      mainObjVec = {x:-(point.y-point_onAlignLine.y),y:(point.x-point_onAlignLine.x)};
    }

    alignLine={
      x1:point_onAlignLine.x,y1:point_onAlignLine.y,
      x2:point_onAlignLine.x+mainObjVec.x,y2:point_onAlignLine.y+mainObjVec.y,
    };

    if(point!=null && alignLine!=null)
    {
      //this.canvas_arrow(ctx, point.x, point.y, point_on_line.x, point_on_line.y);

      let point_on_line = closestPointOnLine(alignLine,point);


      let closestPt_disp = closestPointOnLine(alignLine,eObject.pt1);


      let extended_ind_line = {
        x0:closestPt_disp.x,y0:closestPt_disp.y,
        x1:closestPt_disp.x+(point.x-point_on_line.x),
        y1:closestPt_disp.y+(point.y-point_on_line.y),
      }


      ctx.setLineDash([5, 15]);
      
      this.drawReportLine(ctx, {
        
        x0:extended_ind_line.x0,y0:extended_ind_line.y0,
        x1:point_onAlignLine.x,y1:point_onAlignLine.y
      });

      this.drawReportLine(ctx, {
        
        x0:extended_ind_line.x1,y0:extended_ind_line.y1,
        x1:point.x,y1:point.y
      });

      
      this.drawReportLine(ctx, {
        
        x0:extended_ind_line.x1,y0:extended_ind_line.y1,
        x1:eObject.pt1.x,y1:eObject.pt1.y
      });
      ctx.setLineDash([]);
      

      this.drawReportLine(ctx, extended_ind_line);

      
      this.drawpoint(ctx,eObject.pt1);

      this.drawText(ctx,"D"+(Math.hypot(point.x-point_on_line.x,point.y-point_on_line.y)*unitConvert.mult).toFixed(4)+"±"+(eObject.margin*unitConvert.mult).toFixed(4)+unitConvert.unit,
      eObject.pt1.x,eObject.pt1.y);
    }
  }


  drawSignature(ctx,signature,pointSkip=36)
  {
    
    ctx.beginPath();
    ctx.moveTo(
      signature.magnitude[0]*Math.cos(signature.angle[0]),
      signature.magnitude[0]*Math.sin(signature.angle[0]));
    for(let i=1;i<signature.angle.length;i+=pointSkip)
    {

      ctx.lineTo(
        signature.magnitude[i]*Math.cos(signature.angle[i]),
        signature.magnitude[i]*Math.sin(signature.angle[i]));

    }
    ctx.closePath();
    //ctx.stroke();
  }
  drawText(ctx,text,x,y)
  {
    ctx.font="bold 30px  Arial";
    ctx.fillText(text,x,y);
    ctx.strokeStyle="white";
    ctx.lineWidth=2;
    ctx.strokeText(text,x,y);
  }

  drawShapeList(ctx, eObjects,useShapeColor=true,skip_id_list=[],shapeList,unitConvert={unit:"px",mult:1})
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
          
          let db_obj = this.db_obj;
          let subObjs = eObject.ref
            .map((ref)=> db_obj.FindShape( "id" , ref.id, shapeList ))
            .map((idx)=>{  return idx>=0?shapeList[idx]:null});

          this.drawShapeList(ctx, subObjs,useShapeColor,skip_id_list,shapeList);
          if(eObject.id === undefined)break;

          let point = this.db_obj.auxPointParse(eObject,shapeList);
          if(point !== undefined && subObjs.length ==2 )
          {//Draw crosssect line
            ctx.setLineDash([5, 15]);
  
            ctx.beginPath();
            ctx.moveTo(point.x,point.y);
            ctx.lineTo(subObjs[0].pt1.x,subObjs[0].pt1.y);
            ctx.stroke();
  
            ctx.beginPath();
            ctx.moveTo(point.x,point.y);
            ctx.lineTo(subObjs[1].pt1.x,subObjs[1].pt1.y);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.lineWidth=2;
            ctx.strokeStyle="gray"; 
            this.drawpoint(ctx, point);
          }
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
          let db_obj = this.db_obj;
          let subObjs = eObject.ref
            .map((ref)=> db_obj.FindShape( "id" , ref.id,shapeList ))
            .map((idx)=>{  return idx>=0?shapeList[idx]:null});
          
          if(subObjs[0]==null)break;

          let line = subObjs[0];

          let vector = db_obj.shapeVectorParse(eObject,shapeList);
          let cnormal={x:-vector.y,y:vector.x};
          let mag=eObject.width/2;
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


          this.drawShapeList(ctx, subObjs,useShapeColor,skip_id_list,shapeList);

          ctx.lineWidth=2;
          ctx.strokeStyle="gray";  
          this.drawpoint(ctx,eObject.pt1);

        }
        break;
        case SHAPE_TYPE.measure:
        {
          let db_obj = this.db_obj;
          if(eObject.ref===undefined)break;
          let subObjs = eObject.ref
            .map((ref)=> db_obj.FindShapeObject( "id" , ref.id,shapeList ));
          let subObjs_valid=subObjs.reduce((acc, cur) => acc && (cur!==undefined),true);
          if(!subObjs_valid)break;

          if(useShapeColor)
          {
            if(eObject.color!==undefined)
            {
              ctx.strokeStyle=eObject.color; 
              ctx.fillStyle=eObject.color;  
            }
            else
            {
              ctx.strokeStyle=this.colorSet.measure_info; 
              ctx.fillStyle=this.colorSet.measure_info;  
            }
          }

          switch(eObject.subtype)
          {
            case SHAPE_TYPE.measure_subtype.distance:
            {
              this.drawMeasureDistance(ctx,eObject,subObjs,shapeList,unitConvert);
            }
            break;
            case SHAPE_TYPE.measure_subtype.angle:
            {
              let srcPt = 
                intersectPoint(subObjs[0].pt1,subObjs[0].pt2,subObjs[1].pt1,subObjs[1].pt2);
                  
              ctx.lineWidth=2;
              //ctx.strokeStyle=this.colorSet.measure_info; 
                        
              ///ctx.fillStyle=this.colorSet.measure_info; 
              //this.drawpoint(ctx, srcPt,"rect");
              
              let sAngle = Math.atan2(subObjs[0].pt1.y - srcPt.y,subObjs[0].pt1.x - srcPt.x);
              let eAngle = Math.atan2(subObjs[1].pt1.y - srcPt.y,subObjs[1].pt1.x - srcPt.x);
              //eAngle+=Math.PI;

              let angleDiff = (eAngle - sAngle)%(2*Math.PI);
              if(angleDiff<0)
              {
                angleDiff+=Math.PI*2;
              }
              if(angleDiff>Math.PI)
              {
                angleDiff-=Math.PI;
              }

              
              let quadrant=0;

              //if(eObject.quadrant===undefined)
              {

                let midwayAngle = Math.atan2(eObject.pt1.y - srcPt.y,eObject.pt1.x - srcPt.x);//-PI~PI
              
                let angleDiff_midway = (midwayAngle - sAngle)%(2*Math.PI);
                if(angleDiff_midway<0)
                {
                  angleDiff_midway+=Math.PI*2;
                }

                if(angleDiff_midway<angleDiff)
                {
                  quadrant=1;
                }
                else if(angleDiff_midway<Math.PI)
                {
                  quadrant=2;
                }
                else if(angleDiff_midway<(Math.PI+angleDiff))
                {
                  quadrant=3;
                }
                else{
                  quadrant=4;
                }
                

              }

              {
                eObject.quadrant = quadrant;
              }

              let dist = Math.hypot(eObject.pt1.y - srcPt.y,eObject.pt1.x - srcPt.x);
              let margin_deg = eObject.margin*Math.PI/180;
              let draw_sAngle=sAngle,draw_eAngle=eAngle;
              let ext_Angle1=sAngle,ext_Angle2=eAngle;
              switch(quadrant%4)
              {
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
                  draw_sAngle = draw_sAngle+angleDiff+Math.PI;

                }
                break;
              }
              //console.log(angleDiff*180/Math.PI,sAngle*180/Math.PI,eAngle*180/Math.PI);
              if(quadrant%2==0)//if our target quadrant is 2 or 4..., find the complement angle 
              {
                angleDiff=Math.PI-angleDiff;
              }

              draw_eAngle = draw_sAngle + angleDiff;

              if(quadrant%2==0)
              {
                ext_Angle1=draw_eAngle;
                ext_Angle2=draw_sAngle;
              }
              else
              {
                ext_Angle1=draw_sAngle;
                ext_Angle2=draw_eAngle;
              }




              this.drawArcArrow(ctx,srcPt.x,srcPt.y,dist,draw_sAngle,draw_eAngle);


              this.drawpoint(ctx,eObject.pt1);
              
              let measureDeg = angleDiff*180/Math.PI;


              {
                    
                ctx.setLineDash([5, 15]);
                
                this.drawReportLine(ctx, {
                  x0:subObjs[0].pt1.x,y0:subObjs[0].pt1.y,
                  x1:srcPt.x+dist*Math.cos(ext_Angle1),y1:srcPt.y+dist*Math.sin(ext_Angle1)
                });

                this.drawReportLine(ctx, {
                  x0:subObjs[1].pt1.x,y0:subObjs[1].pt1.y,
                  x1:srcPt.x+dist*Math.cos(ext_Angle2),y1:srcPt.y+dist*Math.sin(ext_Angle2)
                });

                ctx.setLineDash([]);
              }

              let text = ""+(measureDeg).toFixed(2)+"º ±"+(eObject.margin).toFixed(2);
              let x = eObject.pt1.x+(eObject.pt1.x - srcPt.x)/dist*4;
              let y = eObject.pt1.y+(eObject.pt1.y - srcPt.y)/dist*4;
              this.drawText(ctx,text,x,y);
              //this.drawArcArrow(ctx,srcPt.x,srcPt.y,100,1,0,true);
            }
            break;
            
            case SHAPE_TYPE.measure_subtype.radius:
            {
              ctx.lineWidth=2;
              //ctx.strokeStyle=this.colorSet.measure_info; 

              ctx.font="30px Arial";
              let arc = threePointToArc(subObjs[0].pt1,subObjs[0].pt2,subObjs[0].pt3);
              let dispVec = {x:eObject.pt1.x - arc.x,y:eObject.pt1.y - arc.y};
              let mag = Math.hypot(dispVec.x,dispVec.y);
              let dispVec_normalized ={x:dispVec.x/mag,y:dispVec.y/mag};
              dispVec.x*=arc.r/mag;
              dispVec.y*=arc.r/mag;//{x:dispVec.x*arc.r/mag,y:dispVec.x*arc.r/mag};

              /*let lineInfo = {
                x0:arc.x+dispVec.x,y0:arc.y+dispVec.y,
                x1:eObject.pt1.x,y1:eObject.pt1.y,
              };*/
              
              this.canvas_arrow(ctx, eObject.pt1.x, eObject.pt1.y, arc.x+dispVec.x, arc.y+dispVec.y);
              //this.drawReportLine(ctx, lineInfo);
              this.drawpoint(ctx,eObject.pt1);

              dispVec_normalized.x*=40;
              dispVec_normalized.y*=40;
              this.drawText(ctx,"R"+(arc.r*unitConvert.mult).toFixed(4)+"±"+(eObject.margin*unitConvert.mult).toFixed(4)+unitConvert.unit,
                eObject.pt1.x+dispVec_normalized.x,
                eObject.pt1.y+dispVec_normalized.y);
                
            
            }
          }
        }
        break;
      }
    });
  }


}



class EverCheckCanvasComponent_proto{
  
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

    this.canvas.addEventListener('wheel',this.onmouseswheel.bind(this), false);

    this.mouseStatus={x:-1,y:-1,px:-1,py:-1,status:0,pstatus:0};

    this.secCanvas_rawImg=null;

    this.secCanvas = document.createElement('canvas');

    this.identityMat= new  DOMMatrix();
    this.Mouse2SecCanvas= new  DOMMatrix();
    this.camera= new CameraCtrl();
    
    this.colorSet={
      unselected:"rgba(100,0,100,0.5)",
      inspection_Pass:"rgba(0,255,0,0.1)",
      inspection_Fail:"rgba(255,0,0,0.1)",
      editShape:"rgba(255,0,0,0.7)",
      measure_info:"rgba(128,128,200,0.7)"
    };

    this.rUtil=new renderUTIL(null);
    this.rUtil.setColorSet(this.colorSet);
  }


  resourceClean()
  {
    this.canvas.removeEventListener('wheel',this.onmouseswheel.bind(this));
    console.log("resourceClean......")
  }

  SetImg( img )
  {
    console.log("SetImg:::");
    if(img == null || img == this.secCanvas_rawImg)return;
    this.secCanvas.width = img.width;
    this.secCanvas.height = img.height;

    this.secCanvas_rawImg=img;
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx2nd.putImageData(img, 0, 0);

    console.log("SetImg::: UPDATE",ctx2nd);
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

    return false;
  }


  
  onmousemove(evt)
  {
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;

    //console.log("onmousemove_pre:",this.state);
    //console.log("this.state:"+this.state+"  "+this.mouseStatus.status);

    
    switch(this.state.substate)
    {
      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
        
        if(this.EditPoint!=null)break;
      case UI_SM_STATES.DEFCONF_MODE_NEUTRAL:
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
    
    let pos = this.getMousePos(this.canvas,evt);
    this.mouseStatus.px=pos.x;
    this.mouseStatus.py=pos.y;
    this.mouseStatus.x=pos.x;
    this.mouseStatus.y=pos.y;
    this.mouseStatus.status = 1;

    this.ctrlLogic();
    this.draw();
  }

  onmouseup(evt)
  {
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



  worldTransform()
  {
    let wMat = new DOMMatrix();

    wMat.translateSelf((this.canvas.width / 2), (this.canvas.height / 2));
    this.camera.CameraTransform(wMat);
    //let center = this.getReportCenter();
    //wMat.translateSelf(-center.x, -center.y);

    return wMat;

  }
 


  VecX2DMat(vec,mat)
  {

    let XX= vec.x * mat.a + vec.y * mat.c + mat.e;
    let YY= vec.x * mat.b + vec.y * mat.d + mat.f;
    return {x:XX,y:YY};
  }
}




class INSP_CanvasComponent extends EverCheckCanvasComponent_proto{

  constructor( canvasDOM )
  {
    super(canvasDOM);
    this.edit_DB_info =null;
    this.db_obj = null;
    this.mouse_close_dist= 10;

    this.colorSet=
    Object.assign(this.colorSet,
      {
        inspection_Pass:"rgba(0,255,0,0.1)",
        inspection_production_Fail:"rgba(128,128,0,0.3)",
        inspection_Fail:"rgba(255,0,0,0.1)",
        inspection_NA:"rgba(64,64,64,0.1)",

          
        color_NA:"rgba(128,128,128,0.5)",
        color_SUCCESS:this.colorSet.measure_info,
        color_FAILURE_opt:{
          submargin1:"rgba(255,255,0,0.5)",
        },
        color_FAILURE:"rgba(255,0,0,0.5)",
      }
    );


    this.state=undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;


    this.EditShape=null;
    this.CandEditPointInfo=null;
    this.EditPoint=null;
    
    this.EmitEvent=(event)=>{console.log(event);};
  }

  SetState(state)
  {
    console.log(state);
    let stateObj = xstate_GetCurrentMainState(state);
    let stateStr = JSON.stringify(stateObj);
    if(JSON.stringify(this.state) === stateStr)return;

    this.state = JSON.parse(stateStr);

    if(
      this.state.state ==  UI_SM_STATES.DEFCONF_MODE&&
      this.state.substate==UI_SM_STATES.DEFCONF_MODE_NEUTRAL)
    {
      this.EmitEvent(DefConfAct.Edit_Tar_Update(null));
      this.EditShape=null;
      this.EditPoint=null;
    }
    
  }

  EditDBInfoSync(edit_DB_info)
  {
    console.log(">>>>>>>>>>>>>>>>>>>>>",edit_DB_info);
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg( edit_DB_info.img );

  }

  SetShape( shape_obj, id)
  {
    this.tmp_EditShape_id=id;
    this.EmitEvent(DefConfAct.Shape_Set({shape:shape_obj,id:id}));
  }



  inspectionResult(objReport)
  {
    let judgeReports = objReport.judgeReports;
    let ret_status = judgeReports.reduce((res,obj)=>{
      if(res==INSPECTION_STATUS.NA)return res;
      if(res==INSPECTION_STATUS.FAILURE)
      {
        if(obj.status==INSPECTION_STATUS.NA)return INSPECTION_STATUS.NA;
        return res;
      }
      return obj.status;
    }
    ,INSPECTION_STATUS.SUCCESS);

    if(ret_status==undefined)
    {
      return INSPECTION_STATUS.NA;
    }

    return ret_status;
  }

  featureInspectionMarginTest(featureMeasure,target,marginSet)
  {
    let subtype = featureMeasure.subtype;


    
    let error = featureMeasure.value - target;
    if(error<0)error=-error;
    if(subtype === "angle")
    {
      if(error>=90)error=180-error;
    }


    let minViolation=Number.MAX_VALUE;
    let violationInfo={
      subtype:subtype,
      target:target,
      value:featureMeasure.value,
      error:error,
      minViolationKey:undefined,
      minViolationIdx:Number.MAX_VALUE,
      minViolationValue:Number.MAX_VALUE
    };

    let idx=0;
    for (var key in marginSet)
    {
      if(marginSet[key] == 0)continue;
      let violation = error-marginSet[key];
      if(violation>0)
      {
        if(minViolation>violation)
        {
          minViolation = violation;
          violationInfo.minViolationValue = violation;
          violationInfo.minViolationKey = key;
          violationInfo.minViolationIdx = idx;
        }
      }
      idx++;

    }
    return violationInfo;
  }


  draw()
  {
      this.draw_INSP();
  }
  draw_INSP()
  {
    if(this.edit_DB_info.inspReport==null || this.edit_DB_info.inspReport.reports==undefined)
    {
      return;
    }
    let inspReportGroup= this.edit_DB_info.inspReport.reports[0];
    let inspectionReport = inspReportGroup.reports;
    let mmpp = inspReportGroup.mmpp;
    let unitConvert;

    if(!isNaN(mmpp) )
    {
      unitConvert={
        unit:"mm",//"μm",
        mult:mmpp
      }
    } 
    else
    {
      unitConvert={
        unit:"px",
        mult:1
      }
    }
    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = 2;
    ctx.resetTransform();  
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix  = this.worldTransform();
    ctx.setTransform(matrix.a,matrix.b,matrix.c,
      matrix.d,matrix.e,matrix.f);  
    
    {//TODO:HACK: 4X4 times scale down for transmission speed
      
      ctx.translate(-this.secCanvas.width*4/2,-this.secCanvas.height*4/2);//Move to the center of the secCanvas
      ctx.save();
      ctx.scale(4,4);
      ctx.drawImage(this.secCanvas,0,0);
      ctx.restore();
    }

    if(true)
    {
      let sigScale = 1;

      
      let measureShape =[];
      if(this.edit_DB_info.list !== undefined)
      {
        measureShape = this.edit_DB_info.list.reduce((measureShape,shape)=>{
          if(shape.type == SHAPE_TYPE.measure)
            measureShape.push(shape)
          return measureShape;
        },[]);
      }

      inspectionReport.forEach((report,idx)=>{
        ctx.save();
        ctx.translate(report.cx,report.cy);
        ctx.rotate(-report.rotate);
        if(report.isFlipped)
          ctx.scale(1,-1);
        
        ctx.scale(sigScale,sigScale);
        this.rUtil.drawSignature(ctx, this.edit_DB_info.inherentShapeList[0].signature,5);
        
        let ret_res = this.inspectionResult(report);
        switch(ret_res)
        {
          case INSPECTION_STATUS.NA:
            ctx.fillStyle=this.colorSet.inspection_NA;
          break;
          case INSPECTION_STATUS.SUCCESS:
          {
            ctx.fillStyle=this.colorSet.inspection_Pass;

            {
              let minViolationIdx = Number.MAX_VALUE;
              this.edit_DB_info.list.forEach((eObj)=>{
                if(eObj.type === "measure")
                {
                  let targetID = eObj.id;
                  let inspMeasureTar = report.judgeReports.reduce((tar,measure)=>
                    (tar===undefined && measure.id === targetID)?measure:tar
                    ,undefined);
                  let measureSet={
                    submargin1:eObj.submargin1
                  };
                  
                  let submargin = this.featureInspectionMarginTest(inspMeasureTar,eObj.value,measureSet);
                  if(submargin.minViolationKey != undefined && minViolationIdx>submargin.minViolationIdx)
                  {
                    minViolationIdx=submargin.minViolationIdx;
                    ctx.fillStyle=this.colorSet.color_FAILURE_opt[submargin.minViolationKey];
                  }

                }
              });
            }
            //ctx.fillStyle=this.colorSet.inspection_production_Fail;
            
          }
          break;
          case INSPECTION_STATUS.FAILURE:
            ctx.fillStyle=this.colorSet.inspection_Fail;
          break;

        }
        ctx.fill();
        ctx.restore();
        ctx.strokeStyle = "black";
        this.rUtil.drawpoint(ctx, {x:report.cx,y:report.cy},"rect");
      });
    }


    inspectionReport.forEach((report,idx)=>{
      //let ret_res = this.inspectionResult(report);
      //if(ret_res == INSPECTION_STATUS.SUCCESS)
      {
        let listClone = JSON.parse(JSON.stringify(this.edit_DB_info.list)); 
        this.db_obj.ShapeListAdjustsWithInspectionResult(listClone,report);
        
        listClone.forEach((eObj)=>{
          //console.log(eObj);
          switch(eObj.inspection_status)
          {
            case INSPECTION_STATUS.NA:
              eObj.color=this.colorSet.color_NA;
            break;
            case INSPECTION_STATUS.SUCCESS:
            {
              eObj.color=this.colorSet.color_SUCCESS;
              if(eObj.type === "measure")
              {
                let targetID = eObj.id;
                let inspMeasureTar = report.judgeReports.reduce((tar,measure)=>
                  (tar===undefined && measure.id === targetID)?measure:tar
                  ,undefined);
                let measureSet={
                  submargin1:eObj.submargin1
                };
                
                let submargin = this.featureInspectionMarginTest(inspMeasureTar,eObj.value,measureSet);
                if(submargin.minViolationKey != undefined)
                {
                  eObj.color=this.colorSet.color_FAILURE_opt[submargin.minViolationKey];
                }

              }

            }
            break;
            case INSPECTION_STATUS.FAILURE:
              eObj.color=this.colorSet.color_FAILURE;
            break;

          }
        });
        this.rUtil.drawShapeList(ctx,listClone,true,[],listClone,unitConvert);
      }
    });

  }

  ctrlLogic()
  {
    this.ctrlLogic_INSP();
  }
  
  ctrlLogic_INSP()
  {

  }
}


class DEFCONF_CanvasComponent extends EverCheckCanvasComponent_proto{

  constructor( canvasDOM )
  {
    super(canvasDOM);
    this.edit_DB_info =null;
    this.db_obj = null;
    this.mouse_close_dist= 10;



    this.state=undefined;//UI_SM_STATES.DEFCONF_MODE_NEUTRAL;


    this.EditShape=null;
    this.CandEditPointInfo=null;
    this.EditPoint=null;
    
    this.EmitEvent=(event)=>{console.log(event);};
  }

  SetState(state)
  {
    console.log(state);
    let stateObj = xstate_GetCurrentMainState(state);
    let stateStr = JSON.stringify(stateObj);
    if(JSON.stringify(this.state) === stateStr)return;

    this.state = JSON.parse(stateStr);

    if(
      this.state.state ==  UI_SM_STATES.DEFCONF_MODE&&
      this.state.substate==UI_SM_STATES.DEFCONF_MODE_NEUTRAL)
    {
      this.EmitEvent(DefConfAct.Edit_Tar_Update(null));
      this.EditShape=null;
      this.EditPoint=null;
    }
    
  }

  EditDBInfoSync(edit_DB_info)
  {
    console.log(">>>>>>>>>>>>>>>>>>>>>",edit_DB_info);
    this.edit_DB_info = edit_DB_info;
    this.db_obj = edit_DB_info._obj;
    this.rUtil.setEditor_db_obj(this.db_obj);
    this.SetImg( edit_DB_info.img );

    this.SetEditShape( edit_DB_info.edit_tar_info );
  }

  SetShape( shape_obj, id)
  {
    this.tmp_EditShape_id=id;
    this.EmitEvent(DefConfAct.Shape_Set({shape:shape_obj,id:id}));
  }

  SetEditShape( EditShape )
  {
      this.EditShape = EditShape;
      
      console.log(this.tmp_EditShape_id);
      if(this.EditShape!=null && this.EditShape.id!=undefined && this.tmp_EditShape_id !=this.EditShape.id){
        if(this.tmp_EditShape_id!=undefined)
        {
          this.fitCameraToShape(this.EditShape);
        }
        this.tmp_EditShape_id=this.EditShape.id;
      }
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
        let pt = this.db_obj.auxPointParse(shape);
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



  draw()
  {
    this.draw_DEFCONF();
  }
  ctrlLogic()
  {
    this.ctrlLogic_DEFCONF();
  }
  

  draw_DEFCONF()
  {

    let ctx = this.canvas.getContext('2d');
    let ctx2nd = this.secCanvas.getContext('2d');
    ctx.lineWidth = 2;
    ctx.setTransform(this.identityMat);  
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    let matrix  = this.worldTransform();
    ctx.setTransform(matrix);  
    
    {
      let center = this.db_obj.getSig360ReportCenter();
      //TODO:HACK: 4X4 times scale down for transmission speed
      ctx.save();
      ctx.translate(-center.x,-center.y);
      ctx.scale(4,4);
      ctx.drawImage(this.secCanvas,0,0);
      ctx.restore();
    }



    {


      this.Mouse2SecCanvas = matrix.invertSelf();
      let invMat =this.Mouse2SecCanvas;
      //this.Mouse2SecCanvas = invMat;
      let mPos = this.mouseStatus;
      let mouseOnCanvas2=this.VecX2DMat(mPos,invMat);
  
    }
    ctx.closePath();
    ctx.save();

    
    let skipDrawIdxs=[];
    if(this.EditShape!=null)
    {
      skipDrawIdxs.push(this.EditShape.id);
      
      ctx.strokeStyle=this.colorSet.editShape;
      this.rUtil.drawShapeList(ctx, [this.EditShape],false,[],this.edit_DB_info.list);
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
        this.rUtil.drawShapeList(ctx, [candPtInfo.shape],false,[],this.edit_DB_info.list);
      }
    }
    
    this.rUtil.drawShapeList(ctx, this.edit_DB_info.list,true,skipDrawIdxs,this.edit_DB_info.list);
    this.rUtil.drawInherentShapeList(ctx, this.edit_DB_info.inherentShapeList);


    if(this.EditPoint!=null)
    {
      ctx.lineWidth=3;
      ctx.strokeStyle="green";  
      this.rUtil.drawpoint(ctx, this.EditPoint);
    }



    if(this.CandEditPointInfo!=null)
    {
      ctx.lineWidth=3;
      ctx.strokeStyle="rgba(0,255,0,0.3)";  
      this.rUtil.drawpoint(ctx, this.CandEditPointInfo.pt);
    }



  }

  ctrlLogic_DEFCONF()
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
    
    switch(this.state.substate)
    {
      case UI_SM_STATES.DEFCONF_MODE_LINE_CREATE:
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
            this.SetShape(this.EditShape);
            this.EmitEvent(DefConfAct.SUCCESS());
          }
        }
        break;
      }      
      
      case UI_SM_STATES.DEFCONF_MODE_ARC_CREATE:
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
            this.EmitEvent(DefConfAct.SUCCESS());
          }
        }
        break;
      }


      case UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_AUX_POINT_CREATE:
      case UI_SM_STATES.DEFCONF_MODE_MEASURE_CREATE:
      {
        
        if(this.mouseStatus.status==1)
        {
          if(ifOnMouseLeftClickEdge && this.CandEditPointInfo!=null)
          {
            if(this.state.substate == UI_SM_STATES.DEFCONF_MODE_SEARCH_POINT_CREATE){
              
              this.EditShape={
                type:SHAPE_TYPE.search_point,
                pt1:{x:0,y:0},
                angleDeg:90,
                margin:10,
                width:40,
                ref:[{
                  id:this.CandEditPointInfo.shape.id,
                  element:this.CandEditPointInfo.shape.type
                }],
                color:this.colorSet.unselected,
              };
              
              this.SetShape( this.EditShape);
              this.EmitEvent(DefConfAct.SUCCESS());

            }
            else
            {
              console.log(ifOnMouseLeftClickEdge,this.CandEditPointInfo);
              this.EmitEvent(DefConfAct.Edit_Tar_Ele_Cand_Update(this.CandEditPointInfo));
  
            }
          }
        }
        else
        {

          let pt_info = this.db_obj.FindClosestCtrlPointInfo( mouseOnCanvas2);
          let pt_info2 = this.db_obj.FindClosestInherentPointInfo( mouseOnCanvas2,this.edit_DB_info.inherentShapeList);
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


      case UI_SM_STATES.DEFCONF_MODE_SHAPE_EDIT:
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
                  this.EmitEvent(DefConfAct.Edit_Tar_Update(this.EditShape));
                  
                }
                //else
                {
                  //this.EmitEvent({type:DefConfAct.EVENT.Tar_Ele_Cand_Update,data:this.CandEditPointInfo});

                }
              }
            }
            else
            {
              this.EditPoint=null;
              this.EditShape=null;
              this.EmitEvent(DefConfAct.Edit_Tar_Update(this.EditShape));
            
            }
            
          }
          else
          {
            if(this.EditPoint!=null)
            {
              this.EditPoint.x = mouseOnCanvas2.x;
              this.EditPoint.y = mouseOnCanvas2.y;
              this.EmitEvent(DefConfAct.Edit_Tar_Update(this.EditShape));
            }
          }
        }
        else
        {
          if(this.EditShape!=null && ifOnMouseLeftClickEdge)
          {
            this.SetShape(this.EditShape,this.EditShape.id);
          }

          let pt_info = this.db_obj.FindClosestCtrlPointInfo( mouseOnCanvas2);
          let pt_info2 = this.db_obj.FindClosestInherentPointInfo( mouseOnCanvas2,this.edit_DB_info.inherentShapeList);
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


export default { INSP_CanvasComponent,DEFCONF_CanvasComponent }
