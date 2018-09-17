import { applyMiddleware, combineReducers, createStore } from "redux";
import UICtrlReducer from "./reducer/UICtrlReducer";
import * as midware from "./middleware/middleware";
import thunk from 'redux-thunk';

export function ReduxStoreSetUp(presistStore){

  const reducer_C = combineReducers({
    UIData:UICtrlReducer
  })

  const middleware = applyMiddleware(thunk,midware.logger,midware.error_catch);

  return createStore(reducer_C,presistStore,middleware);
}
