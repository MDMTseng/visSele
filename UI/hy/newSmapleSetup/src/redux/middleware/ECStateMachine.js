
import {BPG_WS_EVENT,UI_SM_EVENT} from '../actions/UIAct';

import { Machine } from 'xstate';


export const ECStateMachine = ECSMData => store => next => action => {
  
  // console.log(action);
  if (ECSMData.initted === undefined || !ECSMData.initted)
  {
    ECSMData.sm = Machine(ECSMData.state_config);
    ECSMData.c_state = ECSMData.sm.initialState;

    next({type:ECSMData.ev_state_update,data:{
        p_state:ECSMData.c_state,
        c_state:ECSMData.c_state,
        sm:ECSMData.sm,
        action:{type:ECSMData.ev_state_update}
    }});
    ECSMData.initted = true;
  }

  if(action.type==null)
  {
    return next(action);
  }

  let newState = ECSMData.sm.transition(ECSMData.c_state, action.type);
  let state_changed = JSON.stringify(ECSMData.c_state.value)!==JSON.stringify(newState.value);
  if(state_changed)
  {
    let oldState = ECSMData.c_state;
    ECSMData.c_state = newState;
    next({type:ECSMData.ev_state_update,data:{
        p_state:oldState,
        c_state:newState,
        sm:ECSMData.sm,
        action:action
    }});
  }
  return next(action);
  
};

