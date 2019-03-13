
import {BPG_WS_EVENT,UI_SM_EVENT} from '../actions/UIAct';

import * as logX from 'loglevel';
let log = logX.getLogger("ActionThrottle");


export const ActionThrottle = ATData => store => next => action => {
  //console.log("ActionThrottle", ATData.ATID,action.ATID);
  

  if(ATData.ATID!=action.ATID)//Add ATID for different packet throttle setup
  {
    return next(action);
  }

  if (ATData.initted === undefined)
  {
    if (typeof(ATData.time ) != typeof(0))
    {
      ATData.time=30;
    }

    if (typeof(ATData.posEdge ) != typeof(true))
    {
      ATData.posEdge=true;
    }
    ATData.actions=[];
    ATData.initted=true;
    ATData.timeout_obj=null;
  }

  switch(action.ActionThrottle_type)
  {
    case "express":
    action.ATID=ATData.ExATID;
    return next(action);

    case "flush":
    {
      clearTimeout(ATData.timeout_obj);
      ATData.timeout_obj=null;
      if(ATData.actions.length==0)return;
      log.debug("Trigger.....",ATData.ATID);
      ATData.actions.push(action);
      let actions = ATData.actions;
      ATData.actions=[];
      console.log(actions);
      actions.forEach((act=>{
        if(act.type!==undefined)
        {
          next(act)
        }
      }));
      return null;
    }
    break;
    default:
      if(ATData.timeout_obj == null)
      {
        ATData.timeout_obj = setTimeout(()=>{
          store.dispatch({ActionThrottle_type:"flush"});
        },ATData.time);

        if(ATData.posEdge)
        {
          action.ATID=ATData.ExATID;
          return next(action);
        }
        else
          ATData.actions.push(action);
        return;
      }
      else
      {
        ATData.actions.push(action);
      }
    return;
    
  }
  action.ATID=ATData.ExATID;
  return next(action);
  
};

