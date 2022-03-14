import { applyMiddleware, combineReducers, createStore } from "redux";
import EXT_API_Reducer from "./reducer/EXT_API_Reducer";
import {EXT_API_MIDDLEWARE} from "./middleware/EXT_API_MIDDLEWARE";
import * as EXT_API_ACT from "./actions/EXT_API_ACT";

// import { configureStore } from '@reduxjs/toolkit'

// import reduxCatch from 'redux-catch';



function storeGen()//just so I can get the type of the reducers
{
  return combineReducers({
    EXT_API:EXT_API_Reducer
  });
}
// EXT_API_ACT.
export function ReduxStoreSetUp(presistStore:any){

  const middleware = applyMiddleware(
     EXT_API_MIDDLEWARE({}),
    // new ECStateMachine({ev_state_update:"ev_state_update",state_config:ST}),
    // new ActionThrottle({time:100,posEdge:true}),
    // reduxCatch(errorHandler)
    );

  return createStore(storeGen(),presistStore,middleware);
}

export type StoreTypes = ReturnType< ReturnType<typeof storeGen> >