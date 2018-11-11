import {DISP_EVE_UI} from 'REDUX_STORE_SRC/constant';


import STATE_MACHINE_CORE from 'UTIL/STATE_MACHINE_CORE';  
import { Machine } from 'xstate';
import {UI_SM_STATES,UI_SM_EVENT,SHAPE_TYPE} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';


let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;
const EditStates = {
  initial: UISTS.EDIT_MODE_NEUTRAL,
  states: {
    [UISTS.EDIT_MODE_NEUTRAL]
            :  {on: {[UISEV.Line_Create]: UISTS.EDIT_MODE_LINE_CREATE,
                     [UISEV.Arc_Create]:  UISTS.EDIT_MODE_ARC_CREATE,
                     [UISEV.Aux_Line_Create]: UISTS.EDIT_MODE_AUX_LINE_CREATE,
                     [UISEV.Aux_Point_Create]: UISTS.EDIT_MODE_AUX_POINT_CREATE,
                     [UISEV.Shape_Edit]:  UISTS.EDIT_MODE_SHAPE_EDIT,
                    }},
    [UISTS.EDIT_MODE_AUX_LINE_CREATE]
               :{on: {[UISEV.EDIT_MODE_SUCCESS]: UISTS.EDIT_MODE_SHAPE_EDIT,
                      [UISEV.EDIT_MODE_FAIL]:    UISTS.EDIT_MODE_NEUTRAL}},
    [UISTS.EDIT_MODE_AUX_POINT_CREATE]
               :{on: {[UISEV.EDIT_MODE_SUCCESS]: UISTS.EDIT_MODE_SHAPE_EDIT,
                      [UISEV.EDIT_MODE_FAIL]:    UISTS.EDIT_MODE_NEUTRAL}},
    [UISTS.EDIT_MODE_LINE_CREATE]
               :{on: {[UISEV.EDIT_MODE_SUCCESS]: UISTS.EDIT_MODE_SHAPE_EDIT,
                      [UISEV.EDIT_MODE_FAIL]:    UISTS.EDIT_MODE_NEUTRAL}},
    [UISTS.EDIT_MODE_ARC_CREATE]
               :{on: {[UISEV.EDIT_MODE_SUCCESS]: UISTS.EDIT_MODE_SHAPE_EDIT,
                      [UISEV.EDIT_MODE_FAIL]:    UISTS.EDIT_MODE_NEUTRAL}},
    [UISTS.EDIT_MODE_SHAPE_EDIT]
               :{on: {[UISEV.EDIT_MODE_SUCCESS]: UISTS.EDIT_MODE_NEUTRAL,
                      [UISEV.EDIT_MODE_FAIL]:    UISTS.EDIT_MODE_NEUTRAL}}
  }
};

function Default_UICtrlReducer()
{
  let ST = {
    initial: UISTS.SPLASH,
    states: {
      [UISTS.SPLASH]:    { on: { [UISEV.Connected]:   UISTS.MAIN } },
      [UISTS.MAIN]:      { on: { [UISEV.Edit_Mode]:   UISTS.EDIT_MODE,
                                 [UISEV.Disonnected]: UISTS.SPLASH, 
                                 [UISEV.EXIT]:        UISTS.SPLASH } },
      [UISTS.EDIT_MODE]: Object.assign(
                 { on: { [UISEV.Disonnected]: UISTS.SPLASH , 
                         [UISEV.EXIT]:        UISTS.MAIN }},
                 EditStates)
    }
  };
  //ST = d;
  //console.log("ST...",JSON.stringify(ST));
  let toggleSplashMachine = Machine(ST);
  return {
    MENU_EXPEND:false,


    showSplash:true,
    report:[],
    img:null,
    showSM_graph:false,
    edit_info:{
      _obj:new InspectionEditorLogic(),
      list:[],
      inherentShapeList:[],

      edit_tar_info:null,//It's for usual edit target

      //It's the target element in edit target
      //Example 
      //edit_tar_info={iii:0,a:{b:[x,y,z,c]}}
      //And our target is c
      //Then, edit_tar_ele_info={obj:b, keyHist:["a","b",3]}
      edit_tar_ele_info:null,

      //This is the cadidate info for target element content
      edit_tar_ele_cand:null,

    },
    sm:toggleSplashMachine,
    c_state:toggleSplashMachine.initialState
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

    
    this.report=null;
    this.img=null;
  }
  SetCurrentReport(report)
  {
    console.log(report);
    this.report=report;
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

  UpdateInherentShapeList()
  {
    this.inherentShapeList=[];

    let id=100000;
    this.inherentShapeList.push({
      id:id++,
      type:SHAPE_TYPE.aux_point,
      name:"@__SIGNATURE__.centre",
      ref:[{
        name:"@__SIGNATURE__",
        element:"centre"
      }]
    });
    this.inherentShapeList.push({
      id:id++,
      type:SHAPE_TYPE.aux_line,
      name:"@__SIGNATURE__.orientation",
      ref:[{
        name:"@__SIGNATURE__",
        element:"orientation"
      }]
      //ref:"__OBJ_CENTRAL__"
    });

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

  SetShape( shape_obj, id )//undefined means add new shape
  {
    let shape = null;

    if(shape_obj == null)//For delete
    {
      if( id!== undefined)
      {
        let tmpIdx = this.FindShapeIdx( id );
        console.log("SETShape>",tmpIdx);
        if(tmpIdx>=0)
        {
          shape = this.shapeList[tmpIdx];
          this.shapeList.splice(tmpIdx, 1);
          if(this.editShape!=null && this.editShape.id == id)
          {
            this.editShape = null;
          }
        }
      }
      //UpdateInherentShapeList();
      return shape;
    }

    console.log("SETShape>",this.shapeList,shape_obj,id);
          

    let nameIdx=this.FindShapeIdx( shape_obj.id,this.inherentShapeList);
    if(nameIdx!=undefined)
    {
      console.log("Error:Shape id:"+id+" name:"+shape_obj.name+" is in inherentShapeList which is not changeable.");
      return null;
    }

    //shape_obj.color="rgba(100,0,100,0.5)";
    let targetIdx=undefined;
    if(id!=undefined)
    {//id(identification)!=idx(index)
      let tmpIdx = this.FindShapeIdx( id );
      let nameIdx = this.FindShape("name",shape_obj.name);

      //Check if the name in shape_obj exits in the list and duplicates with other shape in list(tmpIdx!=nameIdx)
      if(nameIdx!==undefined && tmpIdx!=nameIdx)
      {
        console.log("Error:Shape id:"+id+" Duplicated shape name:"+shape_obj.name+" with idx:"+nameIdx+" ");
        return null;
      }
      console.log("SETShape>",tmpIdx);
      if(tmpIdx>=0)
      {
        shape = this.shapeList[tmpIdx];
        targetIdx = tmpIdx;
      }
      else
      {
        console.log("Error:Shape id:"+id+" doesn't exist in the list....");
        return null;
      }
    }
    else{
      this.shapeCount++;
      id = this.shapeCount;
    }

    console.log("FoundShape>",shape);
    if(shape == null)
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
      if(targetIdx!=undefined)
      {
        this.shapeList[targetIdx] = shape;
      }
    }
    if(this.editShape!== null && this.editShape.id == id)
    {
      this.editShape = shape;
    }
    //UpdateInherentShapeList();
    return shape;

  }

}

function StateReducer(newState,action)
{


  console.log(newState.c_state,">>",action.type);
  let currentState = newState.sm.transition(newState.c_state, action.type);
  console.log(newState.c_state.value," + ",action.type," > ",currentState.value);
  let state_changed = JSON.stringify(newState.c_state.value)!==JSON.stringify(currentState.value);
  console.log("state change:"+state_changed);
  newState.c_state=currentState;

  
  if (action.type === UISEV.Control_SM_Panel) {
    newState.showSM_graph = action.data;
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
        case UISEV.Inspection_Report:
          newState.report=action.data;
          newState.edit_info._obj.SetCurrentReport(action.data);
        break;

        case UISEV.Image_Update:
          newState.img=action.data;
        break;

        case UISEV.EDIT_MODE_Edit_Tar_Update:
          newState.edit_info.edit_tar_info=
            (action.data == null)? null : Object.assign({},action.data);
        break;

        case UISEV.EDIT_MODE_Shape_List_Update:
          newState.edit_info.list=(action.data == null)? []: action.data;
        break;

        case UISEV.EDIT_MODE_Edit_Tar_Ele_Update:
          newState.edit_info.edit_tar_ele_info=
            (action.data == null)? null : Object.assign({},action.data);
        break;
        case UISEV.EDIT_MODE_Edit_Tar_Ele_Cand_Update:
          newState.edit_info.edit_tar_ele_cand=
            (action.data == null)? null : Object.assign({},action.data);
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
        case UI_SM_STATES.EDIT_MODE_AUX_POINT_CREATE:
        {
          if(state_changed)
          {
            newState.edit_info.edit_tar_info = {
              type:SHAPE_TYPE.aux_point,
              ref:[{},{}]
            };
            newState.edit_info.edit_tar_ele_info=null;
            newState.edit_info.edit_tar_ele_cand=null;
            break;
          }
          
          if(newState.edit_info.edit_tar_ele_info!=null && newState.edit_info.edit_tar_ele_cand!=null)
          {
            let keyHist=newState.edit_info.edit_tar_ele_info.keyHist;
            let obj=GetObjElement(newState.edit_info.edit_tar_info,keyHist,keyHist.length-2);
            let cand=newState.edit_info.edit_tar_ele_cand;

            console.log("GetObjElement",obj,keyHist[keyHist.length-1]);
            obj[keyHist[keyHist.length-1]]={
              id:cand.shape.id,
              element:cand.shape.type
            };

            console.log(newState.edit_info.edit_tar_ele_info,newState.edit_info.edit_tar_ele_cand);
            console.log(obj,newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_info=Object.assign({},newState.edit_info.edit_tar_info);
            newState.edit_info.edit_tar_ele_info=null;
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
    return newState;
  }
  else
  {
    newState = StateReducer(newState,action);
    return newState;
  }

  return newState;
}
export default UICtrlReducer