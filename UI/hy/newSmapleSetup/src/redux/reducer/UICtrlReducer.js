
import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';

import {
  distance_point_point,
  threePointToArc,
  intersectPoint} from 'UTIL/MathTools';


let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;
function Default_UICtrlReducer()
{
  //ST = d;
  //console.log("ST...",JSON.stringify(ST));
  return {
    MENU_EXPEND:false,


    showSplash:true,
    showSM_graph:false,
    WS_CH:undefined,
    edit_info:{
      _obj:new InspectionEditorLogic(),
      defInfo:[],
      inspReport:[],
      sig360report:[],
      img:null,

      list:[],
      inherentShapeList:[],

      edit_tar_info:null,//It's for usual edit target

      //It's the target element in edit target
      //Example 
      //edit_tar_info={iii:0,a:{b:[x,y,z,c]}}
      //And our target is c
      //Then, edit_tar_ele_trace={obj:b, keyHist:["a","b",3]}
      edit_tar_ele_trace:null,

      //This is the cadidate info for target element content
      edit_tar_ele_cand:null,

    },
    sm:null,
    c_state:null,
    p_state:null,
    state_count:0
  }
}

class InspectionEditorLogic
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

  FindShapeObject( key , val)
  {
    let idx =this.FindShape( key , val, this.shapeList);
    if(idx!==undefined)return this.shapeList[idx];
    idx =this.FindShape( key , val, this.inherentShapeList);
    if(idx!==undefined)return this.inherentShapeList[idx];
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
        "unit":"mm",
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

  auxPointParse(aux_point)
  {
    let point=null;
    if(aux_point.type!=SHAPE_TYPE.aux_point)return point;
    
    if(aux_point.ref.length ==1)
    {
      let ref0_shape=this.FindShapeObject( "id" , aux_point.ref[0].id);
      if(ref0_shape ===undefined)
      {
        return null;
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
      
      let ref0_shape=this.FindShapeObject( "id" , aux_point.ref[0].id);
      if(ref0_shape ===undefined)return null;
      let ref1_shape=this.FindShapeObject( "id" , aux_point.ref[1].id);
      if(ref1_shape ===undefined)return null;


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
      let ref0_shape=this.FindShapeObject( "id" , search_point.ref[0].id);
      if(ref0_shape ===undefined)return null;
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



  shapeMiddlePointParse(shape)
  {
    switch(shape.type)
    {
      
      case SHAPE_TYPE.line:
        return {x:(shape.pt1.x+shape.pt2.x)/2,y:(shape.pt1.y+shape.pt2.y)/2};
      case SHAPE_TYPE.arc:
        return threePointToArc(shape.pt1,shape.pt2,shape.pt3);
      case SHAPE_TYPE.aux_point:
        return this.auxPointParse(shape);
      case SHAPE_TYPE.search_point:
        return this.searchPointParse(shape);
    }
    return undefined;
  }

  shapeVectorParse(shape)
  {
    switch(shape.type)
    {
      
      case SHAPE_TYPE.line:
        return {x:(shape.pt2.x-shape.pt1.x),y:(shape.pt2.y-shape.pt1.y)};
      case SHAPE_TYPE.search_point:
      {
        if(shape.ref===undefined || shape.ref.length!=1)return undefined;

        let refObj = this.FindShapeObject( "id" , shape.ref[0].id );
        
        if(refObj===undefined || refObj.type !== SHAPE_TYPE.line)return undefined;
        let lineVec = this.shapeVectorParse(refObj);

        if(lineVec===undefined )return undefined;
        let angle=Math.atan2(lineVec.y,lineVec.x)+shape.angleDeg*Math.PI/180;
        return {x:Math.cos(angle),y:Math.sin(angle)};
      }
    }
    return undefined;
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

}

function StateReducer(newState,action)
{

  newState.state_count++;
  if(action.type == "ev_state_update")
  {
    newState.c_state = action.data.c_state;
    newState.p_state = action.data.p_state;
    newState.sm = action.data.sm;
    newState.state_count=0;
    console.log(newState.p_state.value," + ",action.data.action," > ",newState.c_state.value);
  }
  
  if (action.type === UISEV.Control_SM_Panel) {
    newState.showSM_graph = action.data;
    return newState;
  }


  switch(action.type)
  {
    case UISEV.Control_SM_Panel:
      newState.showSM_graph = action.data;
    return newState;

    case UISEV.WS_channel:
      newState.WS_CH=action.data;
    return newState;
    
  }
  switch(newState.c_state.value)
  {
    case UISTS.SPLASH:
      newState.showSplash=true;
      return newState;
    case UISTS.MAIN:
      newState.showSplash=false;
      return newState;
    
  }

  let stateObj = xstate_GetCurrentMainState(newState.c_state);
  console.log()
  let substate = stateObj.substate;
  switch(stateObj.state)
  {
    case UISTS.SPLASH:
      newState.showSplash=true;
      return newState;
    case UISTS.MAIN:
      newState.showSplash=false;
      return newState;
    case UISTS.EDIT_MODE:
    {


      newState.showSplash=false;
      switch(action.type)
      {
        case UISEV.Image_Update:
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info.img=action.data;
        break;


        case UISEV.Inspection_Report:
          newState.edit_info=Object.assign({},newState.edit_info);
          //newState.report=action.data;
          newState.edit_info._obj.SetInspectionReport(action.data);
          //newState.edit_info.inherentShapeList=newState.edit_info._obj.UpdateInherentShapeList();
        break;

        case UISEV.Define_File_Update:
          
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info._obj.SetDefInfo(action.data);
          
          newState.edit_info.edit_tar_info = null;
          
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          
          //newState.edit_info.inherentShapeList=
            //newState.edit_info._obj.UpdateInherentShapeList();
        break;
        case UISEV.SIG360_Report_Update:
          
          newState.edit_info=Object.assign({},newState.edit_info);
          newState.edit_info._obj.SetSig360Report(action.data);
          newState.edit_info.sig360report = newState.edit_info._obj.sig360report;
        break;


        

        case UISEV.EDIT_MODE_Shape_List_Update:
          newState.edit_info.list=(action.data == null)? []: action.data;
        break;

        case UISEV.EDIT_MODE_Edit_Tar_Update:
          newState.edit_info.edit_tar_info=
            (action.data == null)? null : Object.assign({},action.data);
          
          newState.edit_info.edit_tar_ele_trace=null;
          newState.edit_info.edit_tar_ele_cand=null;
        break;
        case UISEV.EDIT_MODE_Edit_Tar_Ele_Trace_Update:
          newState.edit_info.edit_tar_ele_trace=
            (action.data == null)? null : action.data.slice();
        break;
        case UISEV.EC_Save_Edit_Info:
        {
          if(newState.WS_CH==undefined)break;
          var enc = new TextEncoder();

          let report = newState.edit_info._obj.GenerateEditReport();
          console.log(newState.WS_CH);
          newState.WS_CH.send("SV",0,
            {filename:"test.ic.json"},
            enc.encode(JSON.stringify(report, null, 2))
          );
        }
        break;
        case UISEV.EC_Trigger_Inspection:
        {
          if(newState.WS_CH==undefined)break;
          let IIData=action.data;
          if(IIData === undefined)
          {
            IIData={

            };
          }
          newState.WS_CH.send("II",0,IIData);
        }
        break;
        case UISEV.EDIT_MODE_Edit_Tar_Ele_Cand_Update:
          newState.edit_info.edit_tar_ele_cand=
            (action.data == null)? null :(action.data instanceof Object)? Object.assign({},action.data):action.data;
            console.log("EDIT_MODE_Edit_Tar_Ele_Cand_Update",newState.edit_info.edit_tar_ele_cand);
        break;

        case UISEV.EDIT_MODE_Shape_Set:
        {
          //Three cases
          //ID undefined but shaped is defiend -Add new shape
          //ID is defined and shaped is defiend - Modify an existed shape if it's in the list
          //ID is defined and shaped is null   - delete  an existed shape if it's in the list

          let newID=action.data.id;
          console.log("newID:",newID);
          let shape = newState.edit_info._obj.SetShape(action.data.shape,newID);
          newState.edit_info.list=newState.edit_info._obj.shapeList;
          
          newState.edit_info.inherentShapeList=
            newState.edit_info._obj.UpdateInherentShapeList();
          if(newID!==undefined)
          {//If this time it's not for adding new shape(ie, newID is not undefined)
            let tmpTarIdx=
            newState.edit_info._obj.FindShapeIdx( newID );
            console.log(tmpTarIdx);
            if(tmpTarIdx === undefined)//In this case we delete the shape in the list 
            {
              newState.edit_info.edit_tar_info=null;
            }
            else
            {//Otherwise, we deepcopy the shape
              newState.edit_info.edit_tar_info = 
                JSON.parse(JSON.stringify(newState.edit_info.list[tmpTarIdx]));
            }
          }
          else
          {//We just added a shape, set it as an edit target
            newState.edit_info.edit_tar_info = 
              JSON.parse(JSON.stringify(shape));
          }

          newState.edit_info=Object.assign({},newState.edit_info);
        }
        break;
      }

      
      switch(substate)
      {
        case UI_SM_STATES.EDIT_MODE_SEARCH_POINT_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            /*newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.search_point,
              pt1:{x:0,y:0},
              angle:90,
              margin:10,
              width:40,
              ref:[{}]
            };*/
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
            break;
          }
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            console.log(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
          break;
        }
        case UI_SM_STATES.EDIT_MODE_AUX_POINT_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.aux_point,
              ref:[{},{}]
            };
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
            break;
          }
          console.log(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
            obj[keyTrace[keyTrace.length-1]]={
              id:cand.shape.id,
              type:cand.shape.type
            };

            console.log(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
        }
        break;


        
        case UI_SM_STATES.EDIT_MODE_MEASURE_CREATE:
        {
          if(newState.edit_info.edit_tar_info==null)
          {
            newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.measure,
              subtype:SHAPE_TYPE.measure_subtype.NA,
              //ref:[{},{}]
            };
            newState.edit_info.edit_tar_ele_trace=["subtype"];
            newState.edit_info.edit_tar_ele_cand=null;
            //break;
          }
          console.log(newState.edit_info.edit_tar_ele_trace,newState.edit_info.edit_tar_ele_cand);
          
          if(newState.edit_info.edit_tar_ele_trace!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyTrace=newState.edit_info.edit_tar_ele_trace;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyTrace,keyTrace.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;
            
            
            if(keyTrace[0]=="ref" && cand.shape!==undefined)
            {
              let acceptData=true;
              let subtype = newState.edit_info.edit_tar_info.subtype;
              switch(subtype)
              {
                case SHAPE_TYPE.measure_subtype.sigma:break;
                case SHAPE_TYPE.measure_subtype.distance://No specific requirement
                  if(cand.shape.type==SHAPE_TYPE.search_point || 
                    cand.shape.type==SHAPE_TYPE.aux_point || 
                    cand.shape.type==SHAPE_TYPE.arc )
                  {
                    //We allow these three
                  }
                  else if(cand.shape.type==SHAPE_TYPE.line)
                  {//Might need to check the angle if both are lines

                  }
                  else
                  {
                    console.log("Error: "+ subtype+ 
                      " doesn't accept "+cand.shape.type);
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.radius://Has to be an arc
                  if(cand.shape.type!=SHAPE_TYPE.arc)
                  {
                    console.log("Error: "+ subtype+ 
                      " Only accepts arc");
                    acceptData=false;
                  }
                break;
                case SHAPE_TYPE.measure_subtype.angle://Has to be an line to measure
                if(cand.shape.type!=SHAPE_TYPE.line)
                {
                  console.log("Error: "+ subtype+ 
                    " Only accepts line");
                  acceptData=false;
                }
              break;
                default :
                  console.log("Error: "+ subtype+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              if(acceptData)
              {
                console.log("GetObjElement",obj,keyTrace[keyTrace.length-1]);
                obj[keyTrace[keyTrace.length-1]]={
                  id:cand.shape.id,
                  type:cand.shape.type
                };
              }
            }
            else if(keyTrace[0] == "subtype")
            {
              let acceptData=true;
              switch(cand)
              {
                case SHAPE_TYPE.measure_subtype.sigma:
                case SHAPE_TYPE.measure_subtype.radius:
                  newState.edit_info.edit_tar_info.ref=[{}];
                break;
                case SHAPE_TYPE.measure_subtype.distance:
                case SHAPE_TYPE.measure_subtype.angle:
                  newState.edit_info.edit_tar_info.ref=[{},{}];
                break;
                default :
                  console.log("Error: "+ cand+ " is not in the measure_subtype list");
                  acceptData=false;
              }
              newState.edit_info.edit_tar_info = 
                Object.assign(newState.edit_info.edit_tar_info,
                  {
                    pt1:{x:0,y:0},
                    value:0,
                    margin:1
                  });
              if(acceptData)
                obj[keyTrace[keyTrace.length-1]] = cand;
            }

            console.log(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_trace=null;
            newState.edit_info.edit_tar_ele_cand=null;
          }
        }
        break;
      }



      return newState;
    }
  }
  return newState;
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

  
  if(action.type === undefined || action.type.includes("@@redux/"))return state;

  let newState = Object.assign({},state);

  if(action.type==="ATBundle")
  {
    for( let i=0 ;i<action.data.length;i++)
    {
      newState = StateReducer(newState,action.data[i]);
    }

    console.log(newState);
    return newState;
  }
  else
  {
    newState = StateReducer(newState,action);
    console.log(newState);
    return newState;
  }

  return newState;
}
export default UICtrlReducer