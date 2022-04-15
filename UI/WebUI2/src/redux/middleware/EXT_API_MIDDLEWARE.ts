

import {StoreTypes} from '../store';
import {EXT_API_TYPES,EXT_API_ACTION_TYPES,EXT_API_ERROR} from '../actions/EXT_API_ACT';
import { Middleware,MiddlewareAPI, Dispatch, AnyAction } from 'redux';

export const EXT_API_MIDDLEWARE = (WSData:{[key:string]:any}) => 
  (_api: MiddlewareAPI) => 
  (next:Dispatch<AnyAction>) => 
  (action:  EXT_API_ACTION_TYPES) => {

  if(action===undefined || action.type===undefined  || !action.type.startsWith("EXT_API_"))
  {
    return next(action);
  }

  // console.log(action);
  let id = action.id;
  let api_in_store=WSData[id];
  switch (action.type) {
    case EXT_API_TYPES.REGISTER:
    {
      WSData[id]=action.api;
      break;
    }

    case EXT_API_TYPES.ACCESS_API:
    {
      if(action.return_cb!==undefined)
        return action.return_cb(api_in_store);
      return api_in_store;
    }

    case EXT_API_TYPES.UNREGISTER:
    {

      if(api_in_store===undefined)
      {
        // return next(EXT_API_ERROR(id,`The API of id:${id} is undefined`,action));
      }
      else
      {
        let cb=action.return_cb;
        delete WSData[id];
        if(cb!==undefined)
          return cb(api_in_store);
      }

      break;
    }
    default:
      break;
  }
  return next(action);
};
