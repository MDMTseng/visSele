export enum  EXT_API_TYPES{
  REGISTER="EXT_API_REGISTER",
  CONNECTED="EXT_API_CONNECTED",
  DISCONNECTED="EXT_API_DISCONNECTED",
  ACCESS_API="EXT_API_ACCESS_API",
  UNREGISTER="EXT_API_UNREGISTER",
  UPDATE="EXT_API_UPDATE",
  ERROR="EXT_API_ERROR",

};

export function EXT_API_CONNECTED(id:any) {
  return ({
    type:EXT_API_TYPES.CONNECTED,
    id
  });
}
export function EXT_API_DISCONNECTED(id:any) {
  return ({
    type:EXT_API_TYPES.DISCONNECTED,
    id
  });
}

export function EXT_API_UPDATE(id:any, info:any) {
  return ({
    type:EXT_API_TYPES.UPDATE,
    id,
    info
  });
}

export function EXT_API_REGISTER(id:any, api:any) {
  return ({
    type:EXT_API_TYPES.REGISTER,
    id,
    api
  });
}


export function EXT_API_UNREGISTER(id:any, return_cb:((api:any)=>any)|undefined=undefined) {
  return ({
    type:EXT_API_TYPES.UNREGISTER,
    id,
    return_cb
  });
}




export function EXT_API_ACCESS(id:any, return_cb:((api:any)=>any)|undefined=undefined){
  return ({
    type:EXT_API_TYPES.ACCESS_API,
    id,
    return_cb
  });
}





export function EXT_API_ERROR(id:any, info:string,data:any=undefined){
  return ({
    type:EXT_API_TYPES.ERROR,
    id,info,data
  });
}



export type EXT_API_ACTION_TYPES = 
(ReturnType< typeof EXT_API_UPDATE>&{type: EXT_API_TYPES.UPDATE}) |//tagged union
(ReturnType< typeof EXT_API_REGISTER>&{type: EXT_API_TYPES.REGISTER}) |
(ReturnType< typeof EXT_API_UNREGISTER>&{type: EXT_API_TYPES.UNREGISTER}) |

(ReturnType< typeof EXT_API_CONNECTED>&{type: EXT_API_TYPES.CONNECTED}) |
(ReturnType< typeof EXT_API_DISCONNECTED>&{type: EXT_API_TYPES.DISCONNECTED}) |

(ReturnType< typeof EXT_API_UNREGISTER>&{type: EXT_API_TYPES.UNREGISTER}) |
(ReturnType< typeof EXT_API_ACCESS>&{type: EXT_API_TYPES.ACCESS_API}) |
(ReturnType< typeof EXT_API_ERROR>&{type: EXT_API_TYPES.ERROR})


