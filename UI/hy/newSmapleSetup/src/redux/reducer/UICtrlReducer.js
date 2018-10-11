import {DISP_EVE_UI} from 'REDUX_STORE_SRC/constant';


import STATE_MACHINE_CORE from 'UTIL/STATE_MACHINE_CORE';  
import { Machine } from 'xstate';
import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';


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
      edit_tar_info:null,
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
    this.editShape=null;
    this.editPoint=null;
    
    this.state=null;
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

  FindShapeIdx( id )
  {
    return this.FindShape( "id" , id );
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
          if(this.editShape!=null && this.editShape.id == id)
          {
            this.editShape = null;
          }
        }
      }
      return shape;
    }

    console.log("SETShape>",this.shapeList,shape_obj,id);
          
    //shape_obj.color="rgba(100,0,100,0.5)";
    let targetIdx=-1;
    if(id>=0)
    {//id(identification)!=idx(index)
      let tmpIdx = this.FindShapeIdx( id );
      let nameIdx = this.FindShape("name",shape_obj.name);

      //Check if the name in shape_obj exits in the list and duplicates with other shape in list(tmpIdx!=nameIdx)
      if(nameIdx!=-1 && tmpIdx!=nameIdx)
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
      if(targetIdx!=-1)
      {
        this.shapeList[targetIdx] = shape;
      }
    }
    if(this.editShape!== null && this.editShape.id == id)
    {
      this.editShape = shape;
    }
    return shape;

  }

}


function StateReducer(newState,action)
{


  console.log(newState.c_state,">>",action.type);
  let currentState = newState.sm.transition(newState.c_state, action.type);
  console.log(newState.c_state.value," + ",action.type," > ",currentState.value);
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
        break;
        case UISEV.Image_Update:
          newState.img=action.data;
        break;
        case UISEV.EDIT_MODE_Edit_Tar_Update:
          if(action.data == null)
          {
            newState.edit_info.edit_tar_info=null;
          }
          else
          {
            newState.edit_info.edit_tar_info=Object.assign({},action.data);
          }
        break;
        case UISEV.EDIT_MODE_Shape_List_Update:
          if(action.data == null)
          {
            newState.edit_info.list=[];
          }
          else
          {
            newState.edit_info.list=action.data;
          }
        break;
        case UISEV.EDIT_MODE_Shape_Set:
        {
          newState.edit_info._obj.SetShape(action.data.shape,action.data.id);

          newState.edit_info.list=newState.edit_info._obj.shapeList;
          let tmpTarIdx=
            newState.edit_info._obj.FindShapeIdx( action.data.id );
          console.log(tmpTarIdx);
          if(tmpTarIdx == -1)
          {
            newState.edit_info.edit_tar_info=null;
          }
          else
          {
            newState.edit_info.edit_tar_info = 
              JSON.parse(JSON.stringify(newState.edit_info.list[tmpTarIdx]));
          }

          newState.edit_info=Object.assign({},newState.edit_info);
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