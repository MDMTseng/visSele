
import 'antd/dist/antd.css';
import { connect } from 'react-redux'
import React from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
 
import {DEF_EXTENSION} from 'UTIL/BPG_Protocol';

//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import APP_INSP_MODE_rdx from './InspectionUI';
import APP_ANALYSIS_MODE_rdx from './AnalysisUI';

import {xstate_GetCurrentMainState} from 'UTIL/MISC_Util';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import ReactResizeDetector from 'react-resize-detector';

import {BPG_FileBrowser} from './component/baseComponent.jsx';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';

import  {default as AntButton}  from 'antd/lib/button';
import  Collapse  from 'antd/lib/collapse';
import  Icon  from 'antd/lib/icon';
import  Menu  from 'antd/lib/menu';
import  Layout  from 'antd/lib/layout';
const { Header, Content, Footer, Sider } = Layout;
const SubMenu = Menu.SubMenu;



const Panel = Collapse.Panel;


class CanvasComponent extends React.Component {
    constructor(props) {
        super(props);
    }
    ec_canvas_EmitEvent(event){
    }
    componentDidMount() {
        this.ec_canvas=new EC_CANVAS_Ctrl.Preview_CanvasComponent(this.refs.canvas);
        this.ec_canvas.EmitEvent=this.ec_canvas_EmitEvent.bind(this);

        if(this.props.onCanvasInit !== undefined)
        {
            this.props.onCanvasInit(this.ec_canvas);
        }
        this.updateCanvas(this.props.c_state);
    }
    componentWillUnmount() {
        this.ec_canvas.resourceClean();
    }
    updateCanvas(ec_state,props=this.props) {
        if(this.ec_canvas  !== undefined)
        {
            this.ec_canvas.EditDBInfoSync(props.edit_info);
            this.ec_canvas.SetState(ec_state);
            //this.ec_canvas.ctrlLogic();
            this.ec_canvas.draw();
        }
    }

    onResize(width,height){
        if(this.ec_canvas  !== undefined)
        {
            this.ec_canvas.resize(width,height);
        }
    }
    
    componentDidUpdate(prevProps, prevState) {
        
        console.log("CanvasComponent render",this.props.c_state);
        //let substate = nextProps.c_state.value[UIAct.UI_SM_STATES.DEFCONF_MODE];
        
        console.log(this.props.edit_info.inherentShapeList);
        this.updateCanvas(this.props.c_state,this.props);
    }

    render() {

        return (
        <div className={this.props.className}>
            <canvas ref="canvas" className="width12 HXF"/>
            <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
        </div>
        );
    }   
}
  

const mapStateToProps_CanvasComponent = (state) => {
//console.log("mapStateToProps",JSON.stringify(state.UIData.c_state));
return {
    c_state: state.UIData.c_state,
    edit_info: state.UIData.edit_info,

}
}



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => 
{ 
return{}
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);




class APPMain extends React.Component{


    constructor(props) {
        super(props);
        this.state={
          fileSelectedCallBack:undefined,
          menuSelect:"Overview",
          menuCollapsed:true
        }
    }
  
  
    componentDidMount()
    {
        setTimeout(()=>{

            let defModelPath = this.props.defModelPath;
      
            this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{deffile:defModelPath+'.'+DEF_EXTENSION,imgsrc:defModelPath+".bmp"});
                        
        },1000);
  
    }
    
    shouldComponentUpdate(nextProps, nextState) {
      return true;
    }
  
    render() {
      let UI=[];
  
      if(this.props.c_state==null)return null;
      let stateObj = xstate_GetCurrentMainState(this.props.c_state);
      if(stateObj.state === UIAct.UI_SM_STATES.MAIN)
      {
        let DefFileFolder= this.props.defModelPath.substr(0,this.props.defModelPath.lastIndexOf('/') + 1);
        let genericMenuItemCBsCB=(selectInfo)=>{this.setState({...this.state,menuSelect:selectInfo.key})}
  
        let mmpp = undefined;
        if(this.props.camera_calibration_report!==undefined)
        {
          let camParam = this.props.isp_db.cameraParam;
          mmpp= camParam.mmpb2b/camParam.ppb2b;
        }
  
        let MenuItem={
          Overview:{
            icon:"info-circle" ,
            content:<div style={{ padding: 24, background: '#fff', height: "100%" }}>
              <CanvasComponent_rdx className="HX3 WX3"/>
              <AntButton onClick={()=>{
                let fileSelectedCallBack=
                  (filePath,fileInfo)=>{
                    filePath=filePath.replace("."+DEF_EXTENSION,"");
                    this.setState({...this.state,fileSelectedCallBack:undefined});
                    this.props.ACT_Def_Model_Path_Update(filePath);
                  }
                this.setState({...this.state,fileSelectedCallBack});
  
              }} key="1">{"defModelPath:"+this.props.defModelPath}</AntButton>
  
              {(mmpp===undefined)?null:
              <AntButton  key="mmpp" onClick={()=>{
                
                let cameraCalibPath = "data/default_camera_param.json";
                this.props.ACT_WS_SEND(this.props.WS_ID,"ST",0,
                  {LoadCameraCalibration:cameraCalibPath});
              
                this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,
                  {filename:cameraCalibPath});
              }}>{"mmpp:"+mmpp}</AntButton>}
  
  
              <BPG_FileBrowser key="BPG_FileBrowser"
                path={DefFileFolder} visible={this.state.fileSelectedCallBack!==undefined}
                BPG_Channel={(...args)=>this.props.ACT_WS_SEND(this.props.WS_ID,...args)} 
                onFileSelected={(filePath,fileInfo)=>
                { 
                  this.setState({...this.state,fileSelectedCallBack:undefined});
                  this.state.fileSelectedCallBack(filePath,fileInfo);
                }}
                onCancel={()=>
                { 
                  this.setState({...this.state,fileSelectedCallBack:undefined});
                }}
                fileFilter={(fileInfo)=>fileInfo.type=="DIR"||fileInfo.name.includes("."+DEF_EXTENSION)}
                />
            </div>,
            onSelected:genericMenuItemCBsCB
          },
          EDIT:{
            icon:"edit",
            content:null,
            onSelected:this.props.EV_UI_Edit_Mode
          },
          Inspect:{
            icon:"scan",
            content:null,
            onSelected:this.props.EV_UI_Insp_Mode
          },
          Setting:{
            icon:"setting" ,
            content:<div style={{ padding: 24, background: '#fff', minHeight: 360 }}>
                
  
                
              <AntButton key="Reconnect CAM" 
                onClick={()=>{
                  this.props.ACT_WS_SEND(this.props.WS_ID,"RC",0,{
                    target:"camera_ez_reconnect"});
              }}>Reconnect CAM</AntButton>
  
                
              <AntButton key="camera Calib" 
                onClick={()=>{
                  let fileSelectedCallBack=
                  (filePath,fileInfo)=>{
                    console.log(filePath,fileInfo);
                    this.props.ACT_WS_SEND(this.props.WS_ID,"II",0,{
                      deffile:"data/cameraCalibration.json",
                      imgsrc:filePath
                    });
  
                  }
                this.setState({...this.state,fileSelectedCallBack});
  
              }}>camera Calib</AntButton>
  
  
              <BPG_FileBrowser key="BPG_FileBrowser"
                path={DefFileFolder} visible={this.state.fileSelectedCallBack!==undefined}
                BPG_Channel={(...args)=>this.props.ACT_WS_SEND(this.props.WS_ID,...args)} 
                onFileSelected={(filePath,fileInfo)=>
                { 
                  this.setState({...this.state,fileSelectedCallBack:undefined});
                  this.state.fileSelectedCallBack(filePath,fileInfo);
                }}
                onCancel={()=>
                { 
                  this.setState({...this.state,fileSelectedCallBack:undefined});
                }}
                fileFilter={(fileInfo)=>fileInfo.type=="DIR"||fileInfo.name.includes(".bmp")}
                />
            </div>,
            onSelected:genericMenuItemCBsCB
          },
          Collapse:{
            icon:this.state.menuCollapsed?"right":"left" ,
            content:null,
            onSelected:()=>this.setState({...this.state,menuCollapsed:!this.state.menuCollapsed})
          }
        };
  
        UI.push(
          <Layout className="HXF">
            <Sider
              trigger={null}
              collapsible
              collapsed={this.state.menuCollapsed}
              //collapsed={this.state.collapsed}
            >
              <Menu theme="dark" mode="inline" defaultSelectedKeys={[this.state.menuSelect]} 
                onClick={(select)=>MenuItem[select.key].onSelected(select)}>
                {
                  Object.keys(MenuItem).map(itemKey=>(
                  <Menu.Item key={itemKey} >
                    <Icon type={MenuItem[itemKey].icon} />
                    <span>{itemKey}</span>
                  </Menu.Item>
                  ))
                }
              </Menu>
            </Sider>
  
            <Layout>
              <Content>
                {
  
                  MenuItem[this.state.menuSelect].content
                }
              </Content>
            </Layout>
          </Layout>
  
  
  
        )
  
      }
      else if(stateObj.state === UIAct.UI_SM_STATES.DEFCONF_MODE)
      {
        UI = <APP_DEFCONF_MODE_rdx/>;
      }
      else if(stateObj.state === UIAct.UI_SM_STATES.INSP_MODE)
      {
        UI = <APP_INSP_MODE_rdx/>;
        
      }
      else if(stateObj.state === UIAct.UI_SM_STATES.ANALYSIS_MODE)
      {
        UI = <APP_ANALYSIS_MODE_rdx/>;
      }
  
      return(
      <BASE_COM.CardFrameWarp addClass="width12 height12" fixedFrame={true}>
        {UI}
      </BASE_COM.CardFrameWarp>
  
      );
    }
}
const mapDispatchToProps_APPMain = (dispatch, ownProps) => {
    return {
        EV_UI_Edit_Mode: (arg) => {dispatch(UIAct.EV_UI_Edit_Mode())},
        EV_UI_Insp_Mode: () => {dispatch(UIAct.EV_UI_Insp_Mode())},
        EV_UI_Analysis_Mode:()=>{dispatch(UIAct.EV_UI_Analysis_Mode())},
        ACT_Def_Model_Path_Update:(path)=>{dispatch(UIAct.Def_Model_Path_Update(path))},
        ACT_WS_SEND:(id,tl,prop,data,uintArr,promiseCBs)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr,promiseCBs)),
    }
}
const mapStateToProps_APPMain = (state) => {
    return { 
        defModelPath: state.UIData.edit_info.defModelPath,
        c_state: state.UIData.c_state,
        camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
        isp_db: state.UIData.edit_info._obj,
        WS_CH:state.UIData.WS_CH,
        WS_ID:state.UIData.WS_ID
    }
}

let APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);
export default APPMain_rdx;
