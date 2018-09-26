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

    sm:toggleSplashMachine,
    c_state:toggleSplashMachine.initialState
  }
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

  
  if(action.type === undefined || action.type.includes("@@redux/"))return state;
  console.log(state.c_state,">>",action.type);
  let currentState = state.sm.transition(state.c_state, action.type);
  console.log(state.c_state.value," + ",action.type," > ",currentState.value);
  state.c_state=currentState;

  if (action.type === UISEV.Control_SM_Panel) {
      return Object.assign({},state,{showSM_graph:action.data});
  }
  let obj={};
  switch(state.c_state.value)
  {
    case UISTS.SPLASH:
      obj.showSplash=true;
      return Object.assign({},state,obj);
    case UISTS.MAIN:
      obj.showSplash=false;
      return Object.assign({},state,obj);
    
  }

  let stateObj = xstate_GetCurrentMainState(state.c_state);

  switch(stateObj.state)
  {
    case UISTS.SPLASH:
      obj.showSplash=true;
      return Object.assign({},state,obj);
    case UISTS.MAIN:
      obj.showSplash=false;
      return Object.assign({},state,obj);
    case UISTS.EDIT_MODE:
    {
      obj.showSplash=false;

      if (action.type === UISEV.Inspection_Report) {
        obj.report=action.data;
      }
  
      if (action.type  === UISEV.Image_Update) {
        obj.img=action.data;
      }
      return Object.assign({},state,obj);
    }
  }

  return state;
}
export default UICtrlReducer