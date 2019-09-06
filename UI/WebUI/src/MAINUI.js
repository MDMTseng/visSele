
import 'antd/dist/antd.less';
import { connect } from 'react-redux'
import React from 'react';
import * as BASE_COM from './component/baseComponent.jsx';
 
import {DEF_EXTENSION} from 'UTIL/BPG_Protocol';
import QRCode from 'qrcode'
//import {XSGraph} from './xstate_visual';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import APP_DEFCONF_MODE_rdx from './DefConfUI';
import APP_INSP_MODE_rdx from './InspectionUI';
import APP_ANALYSIS_MODE_rdx from './AnalysisUI';

import {xstate_GetCurrentMainState,GetObjElement} from 'UTIL/MISC_Util';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import ReactResizeDetector from 'react-resize-detector';

import {BPG_FileBrowser} from './component/baseComponent.jsx';
// import fr_FR from 'antd/lib/locale-provider/fr_FR';

import  {default as AntButton}  from 'antd/lib/button';

import  PageHeader  from 'antd/lib/page-header';
import  Typography  from 'antd/lib/typography';
import  Collapse  from 'antd/lib/collapse';
import  Icon  from 'antd/lib/icon';
import  Menu  from 'antd/lib/menu';
import  Button  from 'antd/lib/button';
import  Layout  from 'antd/lib/layout';
import  Input  from 'antd/lib/input';
import  Tag  from 'antd/lib/tag';
const { Header, Content, Footer, Sider } = Layout;
const SubMenu = Menu.SubMenu;
const { Paragraph,Title } = Typography;
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
  
class QR_Canvas extends React.Component{

  constructor(props) {
    super(props);
    this.QR_Content="";
    this.state={
      canvas:undefined,
      fUpdateC:0
    };
  }
  
  componentDidMount() {
    this.setState({...this.state,canvas:this.refs.canvas});
  }
  onResize(width,height){
    this.refs.canvas.width=width;
    this.refs.canvas.height=height;
    this.setState({...this.state,fUpdateC:this.state.fUpdateC++});
  }
  /*shouldComponentUpdate(nextProps, nextState)
  {
    return nextProps.QR_Content!=this.QR_Content || this.refs.canvas==undefined;
  }*/
  componentDidUpdate(prevProps, prevState) {
    this.QR_Content = this.props.QR_Content;
  }

  render() {

    if(this.refs.canvas!==undefined)
      QRCode.toCanvas(this.refs.canvas, this.props.QR_Content,{ errorCorrectionLevel: 'L' }, function (error) {
        if (error) console.error(error)
        console.log('success!');
      })
    return (
    <div className={this.props.className}>
        <canvas ref="canvas" className="width12 HXF" onClick={this.props.onClick}/>
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


function isString(data)
{
  return (typeof data === 'string' || data instanceof String);
}

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
      
            this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{deffile:defModelPath+'.'+DEF_EXTENSION,imgsrc:defModelPath});
                        
        },1000);
  
    }
    
    shouldComponentUpdate(nextProps, nextState) {
      return true;
    }

    FrontDoor(){
        const content = (
            <div className="content">
                <Paragraph>
                    HYVision 2019 is the most famous vision-check system in the world.
                </Paragraph>
                <Paragraph>
                    <p>HYVision 2019 - v1 (http://hyv.idcircle.me)</p>
                    <p>HYVision 2019 - v2 (http://hyv.idcircle.me)</p>
                    <p>HYVision 2019 - v3 (http://hyv.idcircle.me)</p>
                </Paragraph>
                <p className="contentLink">
                    <a>
                        <img
                            src="https://gw.alipayobjects.com/zos/rmsportal/MjEImQtenlyueSmVEfUD.svg"
                            alt="start"
                        /> Quick Start
                    </a> 
                    <a>
                        <img src="https://gw.alipayobjects.com/zos/rmsportal/NbuDUAuBlIApFuDvWiND.svg" alt="info" />
                         Product Info
                    </a>
                    <a>
                        <img src="https://gw.alipayobjects.com/zos/rmsportal/ohOEPSYdDTNnyMbGuyLb.svg" alt="doc" />
                        Product Doc
                    </a>
                </p>
            </div>
        );

        const extraContent = (
            <img
                src="https://gw.alipayobjects.com/mdn/mpaas_user/afts/img/A*KsfVQbuLRlYAAAAAAAAAAABjAQAAAQ/original"
                alt="content"
            />
        );
        const routes = [
            {
                path: 'index',
                breadcrumbName: 'HYV',
            },
            {
                path: 'first',
                breadcrumbName: 'HOME',
            },
            {
                path: 'second',
                breadcrumbName: 'Manual',
            },
        ];
        return (<PageHeader title="HYVision 2019" breadcrumb={{ routes }}>
            <div className="wrap">
                <div className="content">{content}</div>
                <div className="extraContent">{extraContent}</div>
            </div>
        </PageHeader>);
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


        let InspectionMonitor_URL;

        if(isString(this.props.defModelHash) && this.props.defModelHash.length>5)
        {
          InspectionMonitor_URL = this.props.InspectionMonitor_URL+
            "?v="+0+"&name="+this.props.defModelName+"&hash="+this.props.defModelHash;
          InspectionMonitor_URL =  encodeURI(InspectionMonitor_URL);
        }

        let MenuItem={
          HOME:{
              icon:"home",
              content:this.FrontDoor(),
              onSelected: genericMenuItemCBsCB
          },
          Overview:{
            icon:"info-circle" ,
            content:<div style={{ padding: 24, background: '#fff', height: "100%" }}>
              <div className="s black">{this.props.WebUI_info.version}</div>
              <div className="s width7" style={{height:"500px"}}>
                <Button size="large" 
                  onClick={()=>{
                  let fileSelectedCallBack=
                    (filePath,fileInfo)=>{
                      filePath=filePath.replace("."+DEF_EXTENSION,"");
                      this.setState({...this.state,fileSelectedCallBack:undefined});
                      this.props.ACT_Def_Model_Path_Update(filePath);
                      this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{deffile:filePath+'.'+DEF_EXTENSION,imgsrc:filePath});
                    }
                  this.setState({...this.state,fileSelectedCallBack});
                }}>
                  <Icon type="file-add" />
                  {this.props.defModelPath}
                </Button>
                
                <Title level={2}>
                  {this.props.defModelName}
                </Title>

                {this.props.defFileTag.map(tag=><Tag className="large InspTag fixed">{tag}</Tag>)}
                
                {this.props.inspOptionalTag.map(curTag=>
                  <Tag closable className="large InspTag optional" onClose={(e)=>{
                    e.preventDefault();
                    let tagToDelete=curTag;
                    let NewOptionalTag = this.props.inspOptionalTag.filter(tag=>tag!=tagToDelete);
                    console.log(e.target,NewOptionalTag);
                    this.props.ACT_InspOptionalTag_Update(NewOptionalTag);
                    }}>{curTag}</Tag>)}

                <Input placeholder="新標籤"
                  onChange={(e)=>{
                    let newStr=e.target.value;
                    //e.target.setSelectionRange(0, newStr.length)
                    this.setState({...this.state,newTagStr:newStr});
                  }}
                  onPressEnter={(e)=>{
                    let newTag=e.target.value.split(",");
                    let newTags=[...this.props.inspOptionalTag,...newTag];
                    this.props.ACT_InspOptionalTag_Update(newTags);
                    this.setState({...this.state,newTagStr:""});
                  }}
                  className={(this.props.inspOptionalTag.find((str)=>str==this.state.newTagStr))?"error":""}
                  allowClear
                  value={this.state.newTagStr}
                  prefix={<Icon type="tags"/>}
                />

                <CanvasComponent_rdx className="height11"/>
    
              </div>
  
              {
                (isString(InspectionMonitor_URL))?    
                  <QR_Canvas className="s width5 HX6" 
                  onClick={()=>window.open(InspectionMonitor_URL)}
                  QR_Content={InspectionMonitor_URL}/>:
                  null
              }
              
              {(mmpp===undefined)?null:
                <Button  key="mmpp"  size="large"  onClick={()=>{
                  
                  let cameraCalibPath = "data/default_camera_param.json";
                  this.props.ACT_WS_SEND(this.props.WS_ID,"ST",0,
                    {LoadCameraCalibration:cameraCalibPath});
                
                  this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,
                    {filename:cameraCalibPath});
                }}><Icon type="camera" /> {"mmpp:"+mmpp}</Button>}


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
          STA:{
              icon:"bar-chart",
              content:null,
              onSelected:this.props.EV_UI_Analysis_Mode
          },
          Setting:{
            icon:"setting" ,
            content:<div style={{ padding: 24, background: '#fff', minHeight: 360 }}>
              <AntButton key="Reconnect CAM" 
                onClick={()=>{
                  this.props.ACT_WS_SEND(this.props.WS_ID,"RC",0,{
                    target:"camera_ez_reconnect"});
              }}>Reconnect CAM</AntButton>
  
                
              <AntButton key="____" 
                onClick={()=>{
                  new Promise((resolve, reject) => {
                    this.props.ACT_WS_SEND(this.props.WS_ID,"PD",0,
                    {ip:"192.168.2.2",port:5213},
                    undefined,{resolve,reject});
                    //setTimeout(()=>reject("Timeout"),1000)
                  })
                  .then((data) => {
                    console.log(data);
                  })
                  .catch((err) => {
                    console.log(err);
                  })
              }}>Connect uInsp</AntButton>


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
                fileFilter={(fileInfo)=>fileInfo.type=="DIR"||fileInfo.name.includes(".bmp")||fileInfo.name.includes(".png")}
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

        
        let ver_map = this.props.version_map_info;
        let recommend_URL = GetObjElement(ver_map,["recommend_info","url"]);

        if(recommend_URL!==undefined && (recommend_URL.indexOf())==-1)
        {
          MenuItem.UPDATE={
            icon:"cloud-download",
            content:null,
            onSelected:()=>{
              window.location.href =recommend_URL;
            }
          };
        }

  
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
        ACT_InspOptionalTag_Update:(newTag)=>{dispatch(DefConfAct.InspOptionalTag_Update(newTag))},
        ACT_WS_SEND:(id,tl,prop,data,uintArr,promiseCBs)=>dispatch(UIAct.EV_WS_SEND(id,tl,prop,data,uintArr,promiseCBs)),
    }
}
const mapStateToProps_APPMain = (state) => {
    return { 
        defModelName:state.UIData.edit_info.DefFileName,
        defFileTag:state.UIData.edit_info.DefFileTag,
        inspOptionalTag:state.UIData.edit_info.inspOptionalTag,
        defModelPath: state.UIData.edit_info.defModelPath,
        defModelHash: state.UIData.edit_info.DefFileHash,
        c_state: state.UIData.c_state,
        camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
        isp_db: state.UIData.edit_info._obj,
        WS_CH:state.UIData.WS_CH,
        WS_ID:state.UIData.WS_ID,
        version_map_info:state.UIData.version_map_info,
        WebUI_info:state.UIData.WebUI_info,
        InspectionMonitor_URL:state.UIData.InspectionMonitor_URL
    }
}

let APPMain_rdx = connect(mapStateToProps_APPMain,mapDispatchToProps_APPMain)(APPMain);
export default APPMain_rdx;

