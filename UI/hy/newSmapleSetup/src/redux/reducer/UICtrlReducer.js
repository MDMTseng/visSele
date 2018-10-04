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
                     [UISEV.Shape_Edit]:  UISTS.EDIT_MODE_SHAPE_EDIT
                    }},

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
      list:[],
      edit_tar_info:null,
    },
    sm:toggleSplashMachine,
    c_state:toggleSplashMachine.initialState
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
    return StateReducer(newState,action);
  }

  return newState;
}
export default UICtrlReducer