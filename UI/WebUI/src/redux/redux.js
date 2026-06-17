import { applyMiddleware, combineReducers, createStore } from "redux";
import { mkLog } from 'UTIL/logger';
const log = mkLog('editor.reducer');
import UICtrlReducer from "REDUX_STORE_SRC/reducer/UICtrlReducer";
import ConnectionInfoReducer from "REDUX_STORE_SRC/reducer/ConnectionInfoReducer";
import {ActionThrottle} from "REDUX_STORE_SRC/middleware/ActionThrottle";
import {ECStateMachine} from "REDUX_STORE_SRC/middleware/ECStateMachine";
import {MW_API} from "REDUX_STORE_SRC/middleware/MW_API";

import thunk from 'redux-thunk';


import {UI_SM_STATES,UI_SM_EVENT} from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

import reduxCatch from 'redux-catch';

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
                     [UISEV.Loc_Include_Create]: UISTS.DEFCONF_MODE_LOC_INCLUDE_CREATE,
                     [UISEV.Loc_Exclude_Create]: UISTS.DEFCONF_MODE_LOC_EXCLUDE_CREATE,
                     [UISEV.Loc_Reg_Create]: UISTS.DEFCONF_MODE_LOC_REG_CREATE,
                     [UISEV.Obj_Detect_Create]: UISTS.DEFCONF_MODE_OBJ_DETECT_CREATE,
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
    [UISTS.DEFCONF_MODE_LOC_INCLUDE_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_LOC_EXCLUDE_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_SHAPE_EDIT,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    // loc_reg sets def_image_reg (not a persisted shape) -> back to NEUTRAL on commit.
    [UISTS.DEFCONF_MODE_LOC_REG_CREATE]
               :{on: {[DefConfAct.EVENT.SUCCESS]: UISTS.DEFCONF_MODE_NEUTRAL,
                      [DefConfAct.EVENT.FAIL]:    UISTS.DEFCONF_MODE_NEUTRAL}},
    [UISTS.DEFCONF_MODE_OBJ_DETECT_CREATE]
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
    



      [UISTS.INSTINSP_MODE]: Object.assign(
                  { on: { [UISEV.REMOTE_SYSTEM_NOT_READY]: UISTS.SPLASH , 
                      [UISEV.ERROR]:       UISTS.MAIN , 
                      [UISEV.EXIT]:        UISTS.MAIN }},
                    InstInspStates)
    }
  };




 
  
function errorHandler(error, getState, lastAction, dispatch) {
  log.error("[middleware-error]", error);
  log.debug("[middleware-error] state", getState());
  log.debug("[middleware-error] last-action", lastAction);
}



export function ReduxStoreSetUp(presistStore){

  const reducer_C = combineReducers({
    UIData:UICtrlReducer,
    ConnInfo:ConnectionInfoReducer
  })

  // These middlewares are curried factory functions (cfg => store => next => action).
  // (Previously called with `new`, which only worked because Babel transpiled the
  //  arrows to constructable functions; esbuild keeps arrows, so call them directly.)
  const middleware = applyMiddleware(thunk,
    MW_API({}),
    ECStateMachine({ev_state_update:"ev_state_update",state_config:ST}),
    ActionThrottle({time:100,posEdge:true}),
    reduxCatch(errorHandler)
    );

  return createStore(reducer_C,presistStore,middleware);
}
