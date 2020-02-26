import {
    distance_point_point,
    threePointToArc,
    intersectPoint,
    closestPointOnLine,
    PtRotate2d_sc,
    vecXY_addin} from 'UTIL/MathTools';
  
import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';
import {SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';
import {GetObjElement} from 'UTIL/MISC_Util';
import dclone from 'clone';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionEditorLogic");

export const MEASURERSULTRESION=
{
  NA:"NA",
  UOK:"UOK",
  LOK:"LOK",
  OK:"OK",
  
  UCNG:"UCNG",
  LCNG:"LCNG",
  CNG:"CNG",

  USNG:"USNG",
  LSNG:"LSNG",
  SNG:"SNG",
  NG:"NG",
};


export const MEASURERSULTRESION_priority=
{
  [MEASURERSULTRESION.NA]:0,

  [MEASURERSULTRESION.LSNG]:1,
  [MEASURERSULTRESION.USNG]:1,
  [MEASURERSULTRESION.SNG]:1,
  [MEASURERSULTRESION.NG]:1,
  [MEASURERSULTRESION.UCNG]:2,
  [MEASURERSULTRESION.LCNG]:2,
  [MEASURERSULTRESION.CNG]:2,

  [MEASURERSULTRESION.UOK]:3,
  [MEASURERSULTRESION.LOK]:3,
  [MEASURERSULTRESION.OK]:3,
  

};

export function MEASURERSULTRESION_reducer(res, measure_result_region)
{
  if (res == MEASURERSULTRESION.NA) return res;

  if (res == MEASURERSULTRESION.USNG || res == MEASURERSULTRESION.LSNG ) {
      if (measure_result_region == MEASURERSULTRESION.NA)
          return measure_result_region;
      return res;
  }

  if (res == MEASURERSULTRESION.UCNG || res == MEASURERSULTRESION.LCNG ) {
    if (measure_result_region == MEASURERSULTRESION.NA || 
      measure_result_region == MEASURERSULTRESION.USNG||
      measure_result_region == MEASURERSULTRESION.LSNG
      )
        return measure_result_region;
    return res;
  }
  //If the res is undefined/UOK/LOK then the new result is the return value
  return measure_result_region;
}


export class InspectionEditorLogic
{
  constructor()
  {
    this.reset();
  }

  reset()
  {
    this.shapeCount=0;
    this.shapeList=[];
    this.inherentShapeList=[];
    this.editShape=null;
    this.editPoint=null;
    
    this.state=null;

    
    this.sig360info=null;
    this.inspreport=null;
    this.img=null;
  }

  Setsig360info(sig360info)
  {
    log.info(sig360info);
    this.sig360info=sig360info;
  }
  SetInspectionReport(inspreport)
  {
    this.inspreport=inspreport;
    if(this.inspreport.reports===undefined || this.inspreport.reports.length==0)
    {
      return;
    }
    this.inspreport.reports.forEach(report=>report.judgeReports.forEach((measure)=>{
      let measureDef = this.shapeList.find((feature)=>feature.id ==measure.id);
      if(measureDef===undefined || measure.status === INSPECTION_STATUS.NA)
      {
        measure.detailStatus=MEASURERSULTRESION.NA;
      }
      else if(measure.value<measureDef.LSL)
      {
        measure.detailStatus=MEASURERSULTRESION.LSNG;
      }
      else if(measure.value<measureDef.LCL)
      {
        measure.detailStatus=MEASURERSULTRESION.LCNG;
      }
      else if(measure.value<measureDef.value)
      {
        measure.detailStatus=MEASURERSULTRESION.LOK;
      }
      else if(measure.value<measureDef.UCL)
      {
        measure.detailStatus=MEASURERSULTRESION.UOK;
      }
      else if(measure.value<measureDef.USL)
      {
        measure.detailStatus=MEASURERSULTRESION.UCNG;
      }
      else
      {
        measure.detailStatus=MEASURERSULTRESION.USNG;
      }
      
    })
    );

  }

  getsig360info_mmpp()
  {
    try{
      //console.log(this.sig360info);
      return this.sig360info.reports[0].mmpp;

    }catch(e)
    {
    }

    return 1;
  }

  getsig360infoCenter()
  {
    
    let center = {x:0,y:0};
    try{
      center.x = this.sig360info.reports[0].cx;
      center.y = this.sig360info.reports[0].cy;

    }catch(e)
    {
      center.x = 0;//(this.secCanvas.width / 2)
      center.y = 0;//(this.secCanvas.height / 2)
    }

    return center;
  }

  SetShapeList(shapeList)
  {
    this.shapeList = shapeList;
    let maxId=0;
    this.shapeList.forEach((shape)=>{
      if(maxId<shape.id)
      {
        maxId=shape.id;
      }
    });
    this.shapeCount = maxId;
  }

  SetDefInfo(defInfo)
  {
    this.SetShapeList(defInfo.features);

    //this.inherentShapeList = defInfo.featureSet[0].inherentShapeList;
    //log.info(defInfo);
    let sig360info = defInfo.inherentfeatures[0];

    sig360info.signature.magnitude=sig360info.signature.magnitude.map((val)=>Math.round(val * 1000) / 1000);//most 3 decimal places //to 0.001mm/1um
    sig360info.signature.angle=sig360info.signature.angle.map((val)=>Math.round(val * 1000) / 1000);//most 3 decimal places// 0.001*180/pi=0.057 deg
    
    this.Setsig360info(
      {
        reports:[
          {
            cx:sig360info.pt1.x,
            cy:sig360info.pt1.y,
            area:sig360info.area,
            orientation:sig360info.orientation,
            signature:sig360info.signature,
            mmpp:defInfo.mmpp,
            cam_param:defInfo.cam_param,
          }
        ]
      }
    );
    this.UpdateInherentShapeList();
    
    let lostRefObjs = this.findLostRefShapes();

    this.shapeList=this.shapeList.filter((shape)=>!lostRefObjs.includes(shape));
    this.UpdateInherentShapeList();
  }

  SetCameraParamInfo(cameraParam)
  {
    this.cameraParam = cameraParam;
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
    let idx = shapeList.findIndex((shape)=>shape[key]==val);
    return (idx<0)?undefined:idx;
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
    if(this.sig360info===null || this.sig360info === undefined)return;
    let setupTarget=this.sig360info.reports[0];
    
    log.debug(setupTarget);
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

  GenerateFeature_sig360_circle_line()
  {
    return {
      "type":"sig360_circle_line",
      "ver":"0.0.1.0",
      "unit":"px",
      "mmpp":this.sig360info.reports[0].mmpp,
      cam_param:this.sig360info.reports[0].cam_param,
      features:this.shapeList,
      inherentfeatures:this.inherentShapeList
    };
    
  }
  
  findLostRefShapes(shapeList=this.shapeList,inherentShapeList=this.inherentShapeList)
  {
    let totalList=shapeList.concat(inherentShapeList);
    let lostRefShape = totalList.filter(shape=>{
      if(shape.ref===undefined && shape.ref_baseLine===undefined)
        return false;
      let totalRef = shape.ref;
      if(GetObjElement(shape,["ref_baseLine","id"])!==undefined)
      {
        totalRef=[...totalRef,shape.ref_baseLine];
      }
      let lostRef = totalRef.reduce((lostRef,ref)=>{
        if(lostRef)return lostRef;
        return totalList.find((shape)=>ref.id ==shape.id )==undefined;
      },false);
      if(lostRef)return true;


      return false;
    });
    return lostRefShape;
  }
  
  FindShapeRefTree(id,shapeList=this.shapeList,inherentShapeList=this.inherentShapeList)
  {
    let totalList=shapeList.concat(inherentShapeList);
    let ref_layer = totalList.filter(shape=>{
      if(shape.ref===undefined && shape.ref_baseLine===undefined)
        return false;
      let hasRef = shape.ref.find(ref=>ref.id==id)!==undefined;
      if(hasRef)return true;

      if(GetObjElement(shape,["ref_baseLine","id"])==id)
        return true;

      return false;
    }).map(ref_shape=>{
      let ref_tree = this.FindShapeRefTree(ref_shape.id,shapeList,inherentShapeList);
      if(ref_tree.length ==0)
        return {id:ref_shape.id,shape:ref_shape};
      return {id:ref_shape.id,shape:ref_shape,ref_tree};
    });

    return ref_layer;
  }
  FlatRefTree(refTree)
  {
    let idList=[];
    refTree.forEach(refShapeInfo=>{
      idList.push(refShapeInfo);
      if(refShapeInfo.ref_tree!==undefined)
        idList=idList.concat(this.FlatRefTree(refShapeInfo.ref_tree));
    });
    return idList;
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
        log.debug("SETShape>",pre_shape_idx);
        if(pre_shape_idx!=undefined)
        {
          let refTree = this.FindShapeRefTree(id);
          console.log("refTree",refTree);
          let flatRefTree = this.FlatRefTree(refTree);
          console.log("flatRefTree",flatRefTree);
          this.shapeList=this.shapeList
            .filter((shape)=>flatRefTree.find(fRef=>shape.id==fRef.id)===undefined)
            .filter((shape)=>id!=shape.id);

        }
      }
      //UpdateInherentShapeList();
      return pre_shape;
    }

    log.info("SETShape>",this.shapeList,shape_obj,id);
          

    let ishapeIdx=this.FindShapeIdx( shape_obj.id,this.inherentShapeList);
    //If the id is in the inherentShapeList Exit, no change is allowed
    if(ishapeIdx!=undefined)
    {
      log.error("Error:Shape id:"+id+" name:"+shape_obj.name+" is in inherentShapeList which is not changeable.");
      return null;
    }

    if(id!=undefined)//If the id is assigned, which might exist in the shapelist
    {
      let tmpIdx = this.FindShapeIdx( id );
      let nameIdx = this.FindShape("name",shape_obj.name);

      //Check if the name in shape_obj exits in the list and duplicates with other shape in list(tmpIdx!=nameIdx)
      if(nameIdx!==undefined && tmpIdx!=nameIdx)
      {
        log.error("Error:Shape id:"+id+" Duplicated shape name:"+shape_obj.name+" with idx:"+nameIdx+" ");
        return null;
      }
      log.info("SETShape>",tmpIdx);
      if(tmpIdx!=undefined)
      {
        pre_shape = this.shapeList[tmpIdx];
        pre_shape_idx = tmpIdx;
      }
      else
      {
        log.error("Error:Shape id:"+id+" doesn't exist in the list....");
        return null;
      }
    }
    else{//If the id is undefined, find an available id then append shapelist with this object
      this.shapeCount++;
      id = this.shapeCount;
    }

    //log.info("FoundShape>",pre_shape);
    let shape=null;
    shape = {...shape_obj,id};
    if(pre_shape == null)
    {
      if(shape.name === undefined)
      {
        shape.name="@"+shape.type+"_"+id;
      }
      this.shapeList.push(shape);
    }
    else
    {
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

        
        case SHAPE_TYPE.measure:
        {
          eObject.inspection_value=inspAdjObj.value;
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

    
  FindClosestCtrlPointInfo( location,shapeList=this.shapeList)
  {
    let pt_info={
      pt:null,
      key:null,
      shape:null,
      dist:Number.POSITIVE_INFINITY
    };

    shapeList.forEach((shape)=>{
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
        //point.ref = dclone(aux_point.ref);//Deep copy
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
            point.ref = dclone(aux_point.ref);//Deep copy
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
