
import JSum from 'jsum'
import {GetObjElement} from '../../UTIL/MISC_Util';

import {EXT_API_TYPES,EXT_API_ACTION_TYPES,EXT_API_ERROR} from '../actions/EXT_API_ACT';



export enum EXT_API_STATUS
{
  UNSET,
  CONNECTED,
  DISCONNECTED,
  ERROR
}


type EXP_API_STATE_TYPE={[key: string]: {
  state:EXT_API_STATUS,
  [key: string]:any
}}
//TODO: Move inspection report reducer logic to here
let StateReducer = (state:EXP_API_STATE_TYPE, action:EXT_API_ACTION_TYPES) => {


  switch(action.type)
  {
    
    case EXT_API_TYPES.CONNECTED:
      {
        
        if( state[action.id]===undefined) return state;
        let newInfo={...state[action.id],state:EXT_API_STATUS.CONNECTED};
        return {...state,[action.id]:newInfo}
      }
      
    case EXT_API_TYPES.DISCONNECTED:
      {
        if( state[action.id]===undefined) return state;
        let newInfo={...state[action.id],state:EXT_API_STATUS.DISCONNECTED};
        return {...state,[action.id]:newInfo}
      }


    case EXT_API_TYPES.REGISTER:
      {
        return {...state,[action.id]:{
          state:EXT_API_STATUS.UNSET
        }}
      }
      
    case EXT_API_TYPES.UNREGISTER:
      {
        let newState={...state};
        delete newState[action.id];
        return newState
      }

    case EXT_API_TYPES.ERROR:
      {
        if( state[action.id]===undefined) return state;
        let newInfo={...state[action.id]};
        newInfo.err_log=[...newInfo.err_log,action];
        let newState={...state,[action.id]:newInfo};
        
        return newState
      }

    case EXT_API_TYPES.UPDATE:
      {
        if( state[action.id]===undefined) return state;
        let newInfo={...state[action.id],...action};
        let newState={...state,[action.id]:newInfo};
        return newState
      }
  
    default:
      break;

  }
  // logX.info(action)
  return state;
}

let EXT_API_Reducer = (state:EXP_API_STATE_TYPE= {}, action:EXT_API_ACTION_TYPES) => {
  if (action.type === undefined || action.type.includes("@@redux/")) return state;

  //console.log(action);
  let newState = state;
  newState = StateReducer(newState, action);
  //log.debug(newState);
  return newState;
}

export default EXT_API_Reducer