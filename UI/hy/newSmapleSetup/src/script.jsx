'use strict'

import styles from '../style/basis.css'
import sp_style from '../style/sp_style.css'
import React from 'react';
import ReactDOM from 'react-dom';
import $CSSTG  from 'react-addons-css-transition-group';


//import * as BASE_COM from './component/baseComponent';


class APPMaster extends React.Component{

  constructor(props) {
      super(props);
      this.state ={};
  }

  componentWillMount()
  {
  }
  componentWillUnmount()
  {
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {

    return(

    <div className="HXF">
    </div>);
  }
}



class APPMasterX extends React.Component{

  constructor(props) {
    super(props);
    this.state={};
    this.state.isStoreInited=false;

    setTimeout(function(){
      console.log(">>>>");

      this.state.isStoreInited=true;
      this.setState(this.state);
    }.bind(this),2000)

  }

  componentWillMount()
  {
      //
  }

  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }
  render() {
    return(
    <$CSSTG transitionName = "logoFrame" className="HXF">
      {
        (this.state.isStoreInited)?
        <APPMaster key="APP" />:
        <div key="LOGO" className="HXF WXF overlay veleXY logoFrame white">
          <div className="veleXY width6 height6">
            <img className="height8 LOGOImg " src="resource/image/NotiMon.svg"></img>
            <div className="HX0_5"/>
            <div>
              <div className="TitleTextCon showOverFlow HX2">
                <h1 className="Title">GO  !!</h1>
                <h1 className="Title">NOTIMON</h1>
              </div>
            </div>
          </div>
        </div>
      }
    </$CSSTG>
    );
  }
}



ReactDOM.render(<APPMasterX/>,document.getElementById('container'));
