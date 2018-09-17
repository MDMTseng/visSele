import {DISP_EVE_UI} from '../constant';


import STATE_MACHINE_CORE from '../../UTIL/STATE_MACHINE_CORE';  
import { Machine } from 'xstate';
import {UI_SM_STATES,UI_SM_EVENT} from '../actions/UIAct';

const EditStates = {
  initial: 'NEUTRAL',
  states: {
    NEUTRAL:  {on: {Line_Create: UI_SM_STATES.EDIT_MODE_LINE_CREATE,
                    Arc_Create:  UI_SM_STATES.EDIT_MODE_ARC_CREATE}},

    LINE_CREATE:{on: {PED_TIMER: UI_SM_STATES.EDIT_MODE_NEUTRAL}},
    ARC_CREATE: {on: {PED_TIMER: UI_SM_STATES.EDIT_MODE_NEUTRAL}},
    LINE_EDIT:{on: {PED_TIMER: UI_SM_STATES.EDIT_MODE_NEUTRAL}},
    ARC_EDIT: {on: {PED_TIMER: UI_SM_STATES.EDIT_MODE_NEUTRAL}}
  }
};

function Default_UICtrlReducer()
{
  let ST = {
    initial: UI_SM_STATES.SPLASH,
    states: {
      SPLASH:    { on: { Connected:   UI_SM_STATES.MAIN } },
      MAIN:      { on: { Edit_Mode:   UI_SM_STATES.EDIT_MODE,
                         Disonnected: UI_SM_STATES.SPLASH, 
                         EXIT:        UI_SM_STATES.SPLASH } },
      EDIT_MODE: Object.assign(
                 { on: { Disonnected: UI_SM_STATES.SPLASH , 
                         EXIT:        UI_SM_STATES.SPLASH }},
                 EditStates)
    }
  };
  //ST = d;
  console.log("ST...",JSON.stringify(ST));
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

  if (action.type === UI_SM_EVENT.Control_SM_Panel) {
      return Object.assign({},state,{showSM_graph:action.data});
  }
  let obj={};
  switch(state.c_state.value)
  {
    case UI_SM_STATES.SPLASH:
      obj.showSplash=true;
      return Object.assign({},state,obj);
    case UI_SM_STATES.MAIN:
      obj.showSplash=false;
      return Object.assign({},state,obj);
    
  }

  switch(state.c_state.value)
  {
    case UI_SM_STATES.SPLASH:
      obj.showSplash=true;
      return Object.assign({},state,obj);
    case UI_SM_STATES.MAIN:
      obj.showSplash=false;
      return Object.assign({},state,obj);
    
  }

  if(UI_SM_STATES.EDIT_MODE in state.c_state.value)
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
  
  return state;
}
export default UICtrlReducer