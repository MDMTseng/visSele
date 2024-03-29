import { createStore } from 'redux';
import { bindConnect } from 'svelte-redux';
 
const store = createStore((state = 0, action) => {
    switch (action.type) {
        case 'INCREMENT':
            return state + 1;
        case 'DECREMENT':
            return state - 1;
        default:
            return state;
    }
});
 
export const connect = bindConnect(store);



import { applyMiddleware, combineReducers, createStore } from "redux";
import UICtrlReducer from "REDUX_STORE_SRC/reducer/UICtrlReducer";
import {ActionThrottle} from "REDUX_STORE_SRC/middleware/ActionThrottle";
import {ECStateMachine} from "REDUX_STORE_SRC/middleware/ECStateMachine";
import {MWWebSocket} from "REDUX_STORE_SRC/middleware/MWWebSocket";

import thunk from 'redux-thunk';


import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';


let UISTS = {

};
let UISEV = UI_SM_EVENT;
const EditStates = {
  initial: UISTS.DEFCONF_MODE_NEUTRAL,
  states: {
    [UISTS.DEFCONF_MODE_NEUTRAL]
            :  {on: {[UISEV.Line_Create]: UISTS.DEFCONF_MODE_LINE_CREATE,
                     [UISEV.Arc_Create]:  UISTS.DEFCONF_MODE_ARC_CREATE,
                     [UISEV.Search_Point_Create]: UISTS.DEFCONF_MODE_SEARCH_POINT_CREATE,
                     [UISEV.Aux_Point_Create]: UISTS.DEFCONF_MODE_AUX_POINT_CREATE,
                     [UISEV.Shape_Edit]:  UISTS.DEFCONF_MODE_SHAPE_EDIT,
                     [UISEV.Measure_Create]:  UISTS.DEFCONF_MODE_MEASURE_CREATE,
                    }},
    [UISTS.DEFCONF_MODE_SEARCH_POINT_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_AUX_POINT_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
  }
};


let ST = {
    initial: UISTS.SPLASH,
    states: {
      [UISTS.SPLASH]:    { on: { [UISEV.Connected]:   UISTS.MAIN } },
      [UISTS.MAIN]:      { on: { [UISEV.Edit_Mode]:   UISTS.DEFCONF_MODE,
                                 [UISEV.Insp_Mode]:   UISTS.INSP_MODE,
                                 [UISEV.Analysis_Mode]:UISTS.ANALYSIS_MODE,
                                 [UISEV.Disonnected]: UISTS.SPLASH, 
                                 [UISEV.EXIT]:        UISTS.SPLASH } },
      [UISTS.DEFCONF_MODE]: Object.assign(
                 { on: { [UISEV.Disonnected]: UISTS.SPLASH , 
                         [UISEV.EXIT]:        UISTS.MAIN }},
                 EditStates),
    }
  };



export function ReduxStoreSetUp(presistStore){

  const reducer_C = combineReducers({
    UIData:UICtrlReducer
  })

  const middleware = applyMiddleware(thunk,
    new MWWebSocket({}),
    new ECStateMachine({ev_state_update:"ev_state_update",state_config:ST}),
    new ActionThrottle({time:100,posEdge:true}),
    );

  return createStore(reducer_C,presistStore,middleware);
}
