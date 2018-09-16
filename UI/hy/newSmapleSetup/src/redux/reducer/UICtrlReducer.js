import {DISP_EVE_UI} from '../constant';


import STATE_MACHINE_CORE from '../../UTIL/STATE_MACHINE_CORE';  
import {UI_ACT_Type,UI_BodyPage} from '../actions/UIAct';

function Default_UICtrlReducer()
{
  return {
    MENU_EXPEND:false,
    bodyPage:UI_BodyPage.RadarView,
    showSplash:true,
    SM_CB:{
      Splash_ENTER:(event)=>
      {
        console.log(">>>");
      }
    }


  }
}


const UI_TOP_EVENT = {
  Connected:"Connected",
  EDIT_MODE:"EDIT_MODE"
};


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

  
  console.log(">>>>>>>>>>>>",action);
  /*SM_CB.Main_SM=new STATE_MACHINE_CORE(SM_CB,[//example for 
    ["Splash",      UI_TOP_EVENT.Connected,        "MainManu"],
    ["MainManu",    UI_TOP_EVENT.EDIT_MODE,    "Splash","Splash"]
    ]);*/

  if (action.type === UI_ACT_Type.DROPDOWN_EXPEND) {
    let obj={};
    obj.MENU_EXPEND=action.data;
    return Object.assign({},state,obj);
  }

  if (action.type === UI_ACT_Type.BODY_PAGE_SWITCH) {
    let obj={};
    obj.bodyPage=action.data;
    return Object.assign({},state,obj);
  }

  if (action.type === UI_ACT_Type.SPLASH_SWITCH) {
    let obj={};
    obj.showSplash=action.data;
    return Object.assign({},state,obj);
  }
  return state;
}
export default UICtrlReducer