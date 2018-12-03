'use strict'

import { connect } from 'react-redux'
import React from 'react';
import $CSSTG  from 'react-addons-css-transition-group';
import * as BASE_COM from './component/baseComponent.jsx';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';


class APP_INSP_MODE extends React.Component{


  componentDidMount()
  {
    
  }
  constructor(props) {
    super(props);
    this.ec_canvas = null;
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }




  render() {

    let MenuSet=[];
    let menu_height="HXA";//auto
    console.log("CanvasComponent render");
      MenuSet=[
          <BASE_COM.Button
          key="<"
          addClass="layout black vbox"
          text="<" onClick={()=>this.props.ACT_EXIT()}/>,
          
        ];

    return(
    <div className="HXF">
        <$CSSTG transitionName = "fadeIn">
          <div key={"MENU"} className={"s overlay scroll MenuAnim " + menu_height}>
            {MenuSet}
          </div>
        </$CSSTG>
      
    </div>
    );
  }
}


const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps) => 
{ 
  return{
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    
    ACT_WS_SEND:(id,tl,prop,data,uintArr)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr)),
    
  }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
  return { 
    c_state: state.UIData.c_state,
    shape_list:state.UIData.edit_info.list,
    WS_ID:state.UIData.WS_ID,
    edit_info:state.UIData.edit_info

  }
};

const APP_INSP_MODE_rdx = connect(
    mapStateToProps_APP_INSP_MODE,
    mapDispatchToProps_APP_INSP_MODE)(APP_INSP_MODE);

export default APP_INSP_MODE_rdx;