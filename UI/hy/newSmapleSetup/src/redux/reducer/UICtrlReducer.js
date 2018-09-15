import {DISP_EVE_UI} from '../constant';


import {UI_ACT_Type,UI_BodyPage} from '../actions/UIAct';

function Default_UICtrlReducer()
{
  return {
    MENU_EXPEND:false,
    bodyPage:UI_BodyPage.RadarView,
    showSplash:true,
  }
}


let UICtrlReducer = (state = Default_UICtrlReducer(), action) => {

/*  if (action.type === "ajaxGET") {
    console.log("UICtrlReducer>>>",action);
    return state;
  }*/
  

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