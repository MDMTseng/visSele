
import { websocket_reqTrack } from 'UTIL/MISC_Util';

export const MW_API_EVENT = {
  REGISTER:"MW_API_REGISTER",
  CALL:"MW_API_CALL",
  GET_OBJ:"MW_API_GET_OBJ",
  UNREGISTER:"MW_API_UNREGISTER",

  ERROR:"MW_API_ERROR",

};

export const MW_API = WSData => store => next => action => {

  if(action===undefined || action.type===undefined  || !action.type.startsWith("MW_API_"))
  {
    return next(action);
  }

  // console.log(action);
  let api = action.api;
  let id = action.id;
  let api_in_store=WSData[id];
  switch (action.type) {
    case MW_API_EVENT.REGISTER:
    {
      
      if(id===undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The id:${id} cannot be undefined`,action}});
      }

      if(api===undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The register action id:${id} has undefined api`,action}});
      }
      if(api_in_store!==undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The id:${id} is occupied`,action}});
      }

      api_in_store=
      WSData[id]=api;

      break;
    }

    case MW_API_EVENT.GET_OBJ:
    {
      let return_cb = action.return_cb;
      return return_cb(api_in_store);
      break;
    }
    case MW_API_EVENT.CALL:
    {
      if(api_in_store===undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The API of id:${id} is undefined`,action}});
      }
      let param = action.param;
      let method=action.method;
      if(method===undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The API of id:${id}. The target method is undefined`,action}});
      }
      let ret = api_in_store[method](param);
      
      return ret;
      // return next({type:MW_API_EVENT.CALL,data:ret,action});
      break;
    }



    case MW_API_EVENT.UNREGISTER:
    {

      if(api_in_store===undefined)
      {
        return next({type:MW_API_EVENT.ERROR,data:{msg:`The API of id:${id} is undefined`,action}});
      }
      delete WSData[id];

      break;
    }
  }
  return next(action);
};

