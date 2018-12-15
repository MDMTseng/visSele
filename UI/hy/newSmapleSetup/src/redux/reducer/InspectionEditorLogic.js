import {
    distance_point_point,
    threePointToArc,
    intersectPoint,
    closestPointOnLine,
    PtRotate2d_sc,
    vecXY_addin} from 'UTIL/MathTools';
  
import {SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';
import {GetObjElement} from 'UTIL/MISC_Util';
  
export class InspectionEditorLogic
{
  constructor()
  {
    this.shapeCount=0;
    this.shapeList=[];
    this.inherentShapeList=[];
    this.editShape=null;
    this.editPoint=null;
    
    this.state=null;

    
    this.sig360report=null;
    this.inspreport=null;
    this.img=null;
  }

  SetSig360Report(sig360report)
  {
    console.log(sig360report);
    this.sig360report=sig360report;
  }
  SetInspectionReport(inspreport)
  {
    console.log(inspreport);
    this.inspreport=inspreport;
  }
  

  getSig360ReportCenter()
  {
    
    let center = {x:0,y:0};
    try{
      center.x = this.sig360report.reports[0].cx;
      center.y = this.sig360report.reports[0].cy;

    }catch(e)
    {
      center.x = 0;//(this.secCanvas.width / 2)
      center.y = 0;//(this.secCanvas.height / 2)
    }

    return center;
  }


  SetDefInfo(defInfo)
  {
    this.shapeList = defInfo.featureSet[0].features;
    //this.inherentShapeList = defInfo.featureSet[0].inherentShapeList;
    console.log(defInfo);
    let sig360Info = defInfo.featureSet[0].inherentfeatures[0];
    this.SetSig360Report(
      {
        reports:[
          {
            cx:sig360Info.pt1.x,
            cy:sig360Info.pt1.y,
            area:sig360Info.area,
            orientation:sig360Info.orientation,
            signature:sig360Info.signature,
          }
        ]
      }
    );
    let maxId=0;
    this.shapeList.forEach((shape)=>{
      if(maxId<shape.id)
      {
        maxId=shape.id;
      }
    });
    this.shapeCount = maxId;
    this.UpdateInherentShapeList();
  }

  SetState(state)
  {
    if(this.state!=state)
    {
      this.shapeCount=0;
      this.editShape=null;
      this.editPoint=null;
    }
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

  FindShapeIdx( id , shapeList=this.shapeList)
  {
    return this.FindShape( "id" , id ,shapeList);
  }

  FindShapeObject( key , val, shapeList=this.shapeList,inherentShapeList=this.inherentShapeList)
  {
    let idx =this.FindShape( key , val, shapeList);
    if(idx!==undefined)return shapeList[idx];
    idx =this.FindShape( key , val, inherentShapeList);
    if(idx!==undefined)return inherentShapeList[idx];
    return undefined;
  }


  UpdateInherentShapeList()
  {
    this.inherentShapeList=[];

    let setupTarget=this.sig360report.reports[0];
    
    console.log(setupTarget);
    let id=100000;
    let signature_id = id;
    this.inherentShapeList.push({
      id:signature_id,
      type:SHAPE_TYPE.sign360,
      name:"@__SIGNATURE__",
      pt1:{x:setupTarget.cx,y:setupTarget.cy},//The location on the image
      pt2:{x:0,y:0},//The ref location that we use as graphic center

      area:setupTarget.area,
      orientation:0,
      signature:setupTarget.signature
    });
    id = signature_id+1;
    this.inherentShapeList.push({
      id:id++,
      type:SHAPE_TYPE.aux_point,
      name:"@__SIGNATURE__.centre",
      ref:[{
        id:signature_id,
        keyTrace:["pt2"]
      }]
    });
    this.inherentShapeList.push({
      id:id++,
      type:SHAPE_TYPE.aux_line,
      name:"@__SIGNATURE__.orientation",
      ref:[{
        name:"@__SIGNATURE__",
        keyTrace:["orientation"]
      }]
      //ref:"__OBJ_CENTRAL__"
    });
    id=100100;
    this.shapeList.forEach((shape)=>{
      if(shape.type==SHAPE_TYPE.arc)
      {
        this.inherentShapeList.push({
          
          id:id+shape.id*10,
          type:SHAPE_TYPE.aux_point,
          name:shape.name+".centre",
          ref:[{
            //name:shape.name,
            id:shape.id,
            element:"centre"
          }]
        });
      }
    });

    return this.inherentShapeList;
  }

  GenerateEditReport()
  {
    console.log(this.inherentShapeList);
    return {
      type:"binary_processing_group",
      featureSet:[
      {
        "type":"sig360_circle_line",
        "ver":"0.0.0.0",
        "unit":"px",
        "mmpp":0.01,//TODO:The mmpp need to be measured to get more accurate
        features:this.shapeList,
        inherentfeatures:this.inherentShapeList
      }]
    };
  }
  
  SetShape( shape_obj, id )//undefined means add new shape
  {
    let pre_shape = null;
    let pre_shape_idx=undefined;

    if(shape_obj == null)//For delete
    {
      if( id!== undefined)
      {
        pre_shape_idx = this.FindShapeIdx( id );
        console.log("SETShape>",pre_shape_idx);
        if(pre_shape_idx!=undefined)
        {
          pre_shape = this.shapeList[pre_shape_idx];
          this.shapeList.splice(pre_shape_idx, 1);
          if(this.editShape!=null && this.editShape.id == id)
          {
            this.editShape = null;
          }
        }
      }
      //UpdateInherentShapeList();
      return pre_shape;
    }

    console.log("SETShape>",this.shapeList,shape_obj,id);
          

    let ishapeIdx=this.FindShapeIdx( shape_obj.id,this.inherentShapeList);
    //If the id is in the inherentShapeList Exit, no change is allowed
    if(ishapeIdx!=undefined)
    {
      console.log("Error:Shape id:"+id+" name:"+shape_obj.name+" is in inherentShapeList which is not changeable.");
      return null;
    }

    if(id!=undefined)//If the id is assigned, which might exist in the shapelist
    {
      let tmpIdx = this.FindShapeIdx( id );
      let nameIdx = this.FindShape("name",shape_obj.name);

      //Check if the name in shape_obj exits in the list and duplicates with other shape in list(tmpIdx!=nameIdx)
      if(nameIdx!==undefined && tmpIdx!=nameIdx)
      {
        console.log("Error:Shape id:"+id+" Duplicated shape name:"+shape_obj.name+" with idx:"+nameIdx+" ");
        return null;
      }
      console.log("SETShape>",tmpIdx);
      if(tmpIdx!=undefined)
      {
        pre_shape = this.shapeList[tmpIdx];
        pre_shape_idx = tmpIdx;
      }
      else
      {
        console.log("Error:Shape id:"+id+" doesn't exist in the list....");
        return null;
      }
    }
    else{//If the id is undefined, find an available id then append shapelist with this object
      this.shapeCount++;
      id = this.shapeCount;
    }

    console.log("FoundShape>",pre_shape);
    let shape=null;
    if(pre_shape == null)
    {
      shape = Object.assign({id:id},shape_obj);
      if(shape.name === undefined)
      {
        shape.name="@"+shape.type+"_"+id;
      }
      this.shapeList.push(shape);
    }
    else
    {
      shape = Object.assign({id:id},shape_obj);
      if(pre_shape_idx!=undefined)
      {
        this.shapeList[pre_shape_idx] = shape;
      }
    }
    if(this.editShape!== null && this.editShape.id == id)
    {
      this.editShape = shape;
    }
    //UpdateInherentShapeList();
    return shape;

  }


  
  FindInspShapeObject( id , inspReport)
  {
    if(inspReport == undefined)return undefined;
    {
      let inspIdx = this.FindShapeIdx(id,inspReport.detectedCircles);
      if(inspIdx!=undefined)
      {
        return inspReport.detectedCircles[inspIdx];
      }
    }
    

    
    {
      let inspIdx = this.FindShapeIdx(id,inspReport.detectedLines);
      if(inspIdx!=undefined)
      {
        return inspReport.detectedLines[inspIdx];
      }
    }

    
    {
      let inspIdx = this.FindShapeIdx(id,inspReport.auxPoints);
      if(inspIdx!=undefined)
      {
        return inspReport.auxPoints[inspIdx];
      }
    }
    
    {
      let inspIdx = this.FindShapeIdx(id,inspReport.searchPoints);
      if(inspIdx!=undefined)
      {
        return inspReport.searchPoints[inspIdx];
      }
    }
    
    {
      let inspIdx = this.FindShapeIdx(id,inspReport.judgeReports);
      if(inspIdx!=undefined)
      {
        return inspReport.judgeReports[inspIdx];
      }
    }

    return undefined;
  }
  
  ShapeListAdjustsWithInspectionResult(shapeList,InspResult,oriBase=false)
  {
    let cos_v = Math.cos(-InspResult.rotate);
    let sin_v = Math.sin(-InspResult.rotate);
    let flip_f = (InspResult.isFlipped)?-1:1;
    shapeList.forEach((eObject)=>{
      if(eObject==null)return;

      let inspAdjObj= this.FindInspShapeObject( eObject.id , InspResult);
      if(InspResult!=undefined && inspAdjObj == undefined)
      {
        return;
      }
      eObject.inspection_status = inspAdjObj.status;
      ["pt1","pt2","pt3"].forEach((key)=>{
        if(eObject[key]===undefined)return;
        eObject[key] = PtRotate2d_sc(eObject[key],sin_v,cos_v,flip_f);
        eObject[key].x+=InspResult.cx;
        eObject[key].y+=InspResult.cy;
      });

      switch(eObject.type)
      {
        case SHAPE_TYPE.line:
        {
          ["pt1","pt2"].forEach((key)=>{
            eObject[key]=closestPointOnLine(inspAdjObj, eObject[key]);
          });
          if(InspResult.isFlipped)
          {
            let tmp = eObject.pt1;
            eObject.pt1=eObject.pt2;
            eObject.pt2=tmp;
          }
        }
        break;
        
        
        case SHAPE_TYPE.arc:
        {
          ["pt1","pt2","pt3"].forEach((key)=>{
            eObject[key].x-=inspAdjObj.x;
            eObject[key].y-=inspAdjObj.y;
            let mag = Math.hypot(eObject[key].x,eObject[key].y);
            eObject[key].x=eObject[key].x*inspAdjObj.r/mag+inspAdjObj.x;
            eObject[key].y=eObject[key].y*inspAdjObj.r/mag+inspAdjObj.y;
          });
          
        }
        break;

        case SHAPE_TYPE.search_point:
        {
          eObject.pt1.x = inspAdjObj.x;
          eObject.pt1.y = inspAdjObj.y;
          if(InspResult.isFlipped)
            eObject.angleDeg=-eObject.angleDeg;
        }
        break;
      }
      if(oriBase)//rotate back to original orientation
      {
        ["pt1","pt2","pt3"].forEach((key)=>{
          if(eObject[key]===undefined)return;
          eObject[key].x-=InspResult.cx;
          eObject[key].y-=InspResult.cy;
          if(flip_f<0)
          {
            eObject[key] = PtRotate2d_sc(eObject[key],sin_v,cos_v,-1);
          }
          else
          {
            eObject[key] = PtRotate2d_sc(eObject[key],-sin_v,cos_v,1);
          }
        });
      }
    });

    
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
        case SHAPE_TYPE.measure:
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

  auxPointParse(aux_point,shapelist = this.shapeList)
  {
    let point=undefined;
    if(aux_point.type!=SHAPE_TYPE.aux_point)return point;
    
    if(aux_point.ref.length ==1)
    {
      let ref0_shape=this.FindShapeObject( "id" , aux_point.ref[0].id,shapelist);
      if(ref0_shape ===undefined)
      {
        return undefined;
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
      
      let ref0_shape=this.FindShapeObject( "id" , aux_point.ref[0].id,shapelist);
      if(ref0_shape ===undefined)return undefined;
      let ref1_shape=this.FindShapeObject( "id" , aux_point.ref[1].id,shapelist);
      if(ref1_shape ===undefined)return undefined;


      let v0 = this.shapeVectorParse(ref0_shape,shapelist);
      let v1 = this.shapeVectorParse(ref1_shape,shapelist);
      if(v0 === undefined || v1 === undefined)
      {
        return undefined;
      }

      let p0 = this.shapeMiddlePointParse(ref0_shape,shapelist);
      let p1 = this.shapeMiddlePointParse(ref1_shape,shapelist);
      
      if(p0 === undefined || p1 === undefined)
      {
        return undefined;
      }
      vecXY_addin(v0,p0);//Let vx become another point on line
      vecXY_addin(v1,p1);

      let retPt = intersectPoint(p0,v0,p1,v1);
      return retPt;

    }

    return point;
  }
  searchPointParse(search_point,shapelist = this.shapeList)
  {
    let point=undefined;
    if(search_point.type!=SHAPE_TYPE.search_point)return undefined;
    
    if(search_point.ref.length ==1)
    {
      let ref0_shape=this.FindShapeObject( "id" , search_point.ref[0].id);
      if(ref0_shape ===undefined)return undefined;
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



  shapeMiddlePointParse(shape,shapelist = this.shapeList)
  {
    switch(shape.type)
    {
      
      case SHAPE_TYPE.line:
        return {x:(shape.pt1.x+shape.pt2.x)/2,y:(shape.pt1.y+shape.pt2.y)/2};
      case SHAPE_TYPE.arc:
        return threePointToArc(shape.pt1,shape.pt2,shape.pt3);
      case SHAPE_TYPE.aux_point:
        return this.auxPointParse(shape,shapelist);
      case SHAPE_TYPE.search_point:
        return this.searchPointParse(shape,shapelist);
    }
    return undefined;
  }

  shapeVectorParse(shape,shapelist = this.shapeList)
  {
    switch(shape.type)
    {
      
      case SHAPE_TYPE.line:
        return {x:(shape.pt2.x-shape.pt1.x),y:(shape.pt2.y-shape.pt1.y)};
      case SHAPE_TYPE.search_point:
      {
        if(shape.ref===undefined || shape.ref.length!=1)return undefined;

        let refObj = this.FindShapeObject( "id" , shape.ref[0].id,shapelist );
        
        if(refObj===undefined || refObj.type !== SHAPE_TYPE.line)return undefined;
        let lineVec = this.shapeVectorParse(refObj,shapelist);

        if(lineVec===undefined )return undefined;
        let angle=Math.atan2(lineVec.y,lineVec.x)+shape.angleDeg*Math.PI/180;
        return {x:Math.cos(angle),y:Math.sin(angle)};
      }
    }
    return undefined;
  }

}
