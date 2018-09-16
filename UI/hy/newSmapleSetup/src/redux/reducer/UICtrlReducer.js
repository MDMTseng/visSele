import {DISP_EVE_UI} from '../constant';


import STATE_MACHINE_CORE from '../../UTIL/STATE_MACHINE_CORE';  
import { Machine } from 'xstate';
import {UI_SM_STATES,UI_SM_EVENT} from '../actions/UIAct';



function Default_UICtrlReducer()
{
  let toggleSplashMachine = Machine({
    initial: UI_SM_STATES.SPLASH,
    states: {
      SPLASH:    { on: { Connected:   UI_SM_STATES.MAIN } },
      MAIN:      { on: { Edit_Mode:    UI_SM_STATES.EDIT_MODE,
                         Disonnected: UI_SM_STATES.SPLASH } },
      EDIT_MODE: { on: { Disonnected: UI_SM_STATES.SPLASH } }
    }
  });
  return {
    MENU_EXPEND:false,


    showSplash:true,
    report:[],
    img:null,

    sm:toggleSplashMachine,
    c_state:toggleSplashMachine.initialState
  }
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

  
  if(action.type === undefined || action.type.includes("@@redux/"))return state;
  let currentState = state.sm.transition(state.c_state, action.type);
  console.log(state.c_state.value+" + "+action.type+" > "+currentState.value);
  state.c_state=currentState;

  let obj={};
  switch(state.c_state.value)
  {
    case UI_SM_STATES.SPLASH:
      obj.showSplash=true;
      return Object.assign({},state,obj);
    case UI_SM_STATES.MAIN:
      obj.showSplash=false;
      return Object.assign({},state,obj);
    case UI_SM_STATES.EDIT_MODE:
    {
      obj.showSplash=false;

      if (action.type === UI_SM_EVENT.Inspection_Report) {
        obj.report=action.data;
      }

      if (action.type  === UI_SM_EVENT.Image_Update) {
        obj.img=action.data;
      }
      return Object.assign({},state,obj);
    }
  }
  
  return state;
}
export default UICtrlReducer