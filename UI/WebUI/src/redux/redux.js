import { applyMiddleware, combineReducers, createStore } from "redux";
import UICtrlReducer from "REDUX_STORE_SRC/reducer/UICtrlReducer";
import uInspReducer from "REDUX_STORE_SRC/reducer/uInspReducer";
import InspDataReducer from "REDUX_STORE_SRC/reducer/InspDataReducer";
import {ActionThrottle} from "REDUX_STORE_SRC/middleware/ActionThrottle";
import {ECStateMachine} from "REDUX_STORE_SRC/middleware/ECStateMachine";
import {MWWebSocket} from "REDUX_STORE_SRC/middleware/MWWebSocket";

import thunk from 'redux-thunk';


import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';


let UISTS = UI_SM_STATES;
let UISEV = UI_SM_EVENT;
const EditStates = {
  initial: UISTS.DEFCONF_MODE_NEUTRAL,
  states: {
    [UISTS.DEFCONF_MODE_NEUTRAL]
            :  {on: {[UISEV.Line_Create]: UISTS.DEFCONF_MODE_LINE_CREATE,
                     [UISEV.Arc_Create]:  UISTS.DEFCONF_MODE_ARC_CREATE,
                     [UISEV.Search_Point_Create]: UISTS.DEFCONF_MODE_SEARCH_POINT_CREATE,
                     [UISEV.Aux_Point_Create]: UISTS.DEFCONF_MODE_AUX_POINT_CREATE,
                     [UISEV.Aux_Line_Create]: UISTS.DEFCONF_MODE_AUX_LINE_CREATE,
                     [UISEV.Shape_Edit]:  UISTS.DEFCONF_MODE_SHAPE_EDIT,
                     [UISEV.Measure_Create]:  UISTS.DEFCONF_MODE_MEASURE_CREATE,
                    }},
    [UISTS.DEFCONF_MODE_SEARCH_POINT_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_AUX_POINT_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},

    [UISTS.DEFCONF_MODE_AUX_LINE_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},

    [UISTS.DEFCONF_MODE_LINE_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_ARC_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
                  
    //Result formula calculation
    // [UISTS.DEFCONF_MODE_AUX_LINE_CREATE]
    //           :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
    //                   [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},


    [UISTS.DEFCONF_MODE_SHAPE_EDIT]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_NEUTRAL,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_MEASURE_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}}
  }
};

const InspectionStates = {
  initial: UISTS.INSP_MODE_NEUTRAL,
  states: {
    [UISTS.INSP_MODE_NEUTRAL]
            :  {on: {}}
  }
};

const AnalysisStates = {
  initial: UISTS.ANALYSIS_MODE_NEUTRAL,
  states: {
    [UISTS.ANALYSIS_MODE_NEUTRAL]
            :  {on: {}}
  }
};



const InstInspStates = {
  initial: UISTS.INSTINSP_MODE_NEUTRAL,
  states: {
    [UISTS.INSTINSP_MODE_NEUTRAL]
            :  {on: {}}
  }
};

let ST = {
    initial: UISTS.SPLASH,
    states: {
      [UISTS.SPLASH]:    { on: { [UISEV.REMOTE_SYSTEM_READY]:   UISTS.MAIN } },
      [UISTS.MAIN]:      { on: { [UISEV.Edit_Mode]:   UISTS.DEFCONF_MODE,
                                 [UISEV.Insp_Mode]:   UISTS.INSP_MODE,
                                 [UISEV.Analysis_Mode]:UISTS.ANALYSIS_MODE,
                                 [UISEV.InstInsp_Mode]:UISTS.INSTINSP_MODE,
                                 [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH, 
                                 [UISEV.EXIT]:        UISTS.SPLASH } },
      [UISTS.DEFCONF_MODE]: Object.assign(
                 { on: { [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH , 
                         [UISEV.EXIT]:        UISTS.MAIN }},
                 EditStates),
      [UISTS.INSP_MODE]: Object.assign(
                 { on: { [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH , 
                         [UISEV.ERROR]:       UISTS.MAIN , 
                         [UISEV.EXIT]:        UISTS.MAIN }},
                 InspectionStates),
    
      [UISTS.ANALYSIS_MODE]: Object.assign(
                 { on: { [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH , 
                    [UISEV.ERROR]:       UISTS.MAIN , 
                    [UISEV.EXIT]:        UISTS.MAIN }},
                  AnalysisStates),



      [UISTS.INSTINSP_MODE]: Object.assign(
                  { on: { [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH , 
                      [UISEV.ERROR]:       UISTS.MAIN , 
                      [UISEV.EXIT]:        UISTS.MAIN }},
                    InstInspStates)
    }
  };



export function ReduxStoreSetUp(presistStore){

  const reducer_C = combineReducers({
    UIData:UICtrlReducer,
    InspData:InspDataReducer,
    Peripheral:combineReducers({
      uInsp:uInspReducer
    })
  })

  const middleware = applyMiddleware(thunk,
    new MWWebSocket({}),
    new ECStateMachine({ev_state_update:"ev_state_update",state_config:ST}),
    new ActionThrottle({time:100,posEdge:true}),
    );

  return createStore(reducer_C,presistStore,middleware);
}
