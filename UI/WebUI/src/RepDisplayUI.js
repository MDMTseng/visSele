import React , { useState,useEffect,useContext,useRef } from 'react';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionUI");

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';

import ReactResizeDetector from 'react-resize-detector';
import { useSelector,connect,useDispatch } from 'react-redux' 

import { InspectionEditorLogic,Edit_info_Empty } from 'UTIL/InspectionEditorLogic';
import { BPG_FileBrowser, BPG_FileSavingBrowser } from './component/baseComponent.jsx';

import { UI_SM_STATES, UI_SM_EVENT, SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';

import BPG_Protocol from 'UTIL/BPG_Protocol.js';
class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  triggerROISelect()
  {

  }

  componentDidUpdate(prevProps) {
  }

  ec_canvas_EmitEvent(event) {
    return;
    switch (event.type) {
      case "asdasdas":
        // log.error(event);
        // this.props.ACT_ERROR();

        let rep = this.props.camera_calibration_report.reports[0];
        let mmpp = rep.mmpb2b / rep.ppb2b;

        let crop = event.data.crop.map(val => val / mmpp);
        let down_samp_level = Math.floor(event.data.down_samp_level / mmpp * 2) + 1;
        if (down_samp_level <= 0) down_samp_level = 1;
        else if (down_samp_level > 15) down_samp_level = 15;


        //log.info(crop,down_samp_level);
        this.props.ACT_WS_SEND_BPG(this.props.WS_ID, "ST", 0,
          {
            CameraSetting: {
              down_samp_level
            },
            ImageTransferSetup: {
              crop
            }
          });
        break;

    }
  }

  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.RepDisplay_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);

    if(this.props.onCanvasInit!==undefined)
      this.props.onCanvasInit(this.ec_canvas);

    this.onResize(500, 500)
    //this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  updateCanvas(ec_state, props = this.props) {

    log.info(this.ec_canvas,props.edit_info);
    if (this.ec_canvas !== undefined && props.edit_info!==undefined) {

      {
        this.ec_canvas.EditDBInfoSync(props.edit_info);
        log.info("props.edit_info>>", props.edit_info);
        

        // this.ec_canvas.SetState({value:{[UI_SM_STATES.INSP_MODE]:UI_SM_STATES.INSP_MODE_NEUTRAL}});
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 20) return;
    console.log(this.windowSize.width ,this.windowSize.height,width, height );
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
      this.updateCanvas(this.props.c_state);
    }
  }

  componentWillUpdate(nextProps, nextState) {
    this.updateCanvas(nextProps.c_state, nextProps);
    console.log(">>>");
  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}



export default function RepDisplayUI_rdx({ BPG_Channel , onExtraCtrlUpdate }) {

  
  // const DICT = useSelector(state => state.UIData.DICT);
  
  const [fileSelectorInfo,setFileSelectorInfo]=useState(undefined);
  const [editInfo,setEditInfo]=useState(Edit_info_Empty());

  const [TON,setTON]=useState(false);
  
  // const _REF = React.useRef({
  //   iel: new InspectionEditorLogic(),
  // });
  let xreps="xreps";
  
  function LoadNewFile(filePath)
  {


    let newEditInfo = {...editInfo}
    newEditInfo._obj=new InspectionEditorLogic();
    setEditInfo(newEditInfo)
    filePath = filePath.replace("." + xreps, "");
    BPG_Channel( "LD", 0,{ filename: filePath+"." + xreps,imgsrc: filePath,down_samp_level:6 },undefined,
    { resolve:(pkts,action_channal)=>{
      let SS=pkts.find(pkt=>pkt.type=="SS");
      let FL=pkts.find(pkt=>pkt.type=="FL");
      let IM=pkts.find(pkt=>pkt.type=="IM");
      // console.log(pkts,newEditInfo);
      newEditInfo._obj.SetCameraParamInfo(FL.data.camera_param);
      newEditInfo =newEditInfo._obj.rootDefInfoLoading(FL.data.defInfo,newEditInfo)

      let reports = FL.data.reports;
      if(Array.isArray(reports)==false)
        reports=[reports]
      newEditInfo.reportStatisticState.trackingWindow=reports;
      
      let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);

      newEditInfo.img=img_pros.data;
      // console.log(IM,img_pros);
      setEditInfo({...newEditInfo})





      setTON(true);
    }, reject:(e)=>{

    } });
  

    BPG_Channel( "LD", 0,{ imgsrc: filePath,down_samp_level:1 },undefined,
    { resolve:(pkts,action_channal)=>{
      let IM=pkts.find(pkt=>pkt.type=="IM");
      let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
      newEditInfo.img=img_pros.data;
      newEditInfo.img.IGNORE_IMAGE_FIT_TO_SCREEN=true;
      setEditInfo({...newEditInfo})
      setTON(true);
    }, reject:(e)=>{

    } });

  }
  function BrowseNewFileToLoad()
  {
    let fileGroups = [
      // { name: "history", list: getLocalStorage_RecentFiles() }
    ];
    let fileSelectFilter = (fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes("."+xreps);

    setFileSelectorInfo({
      filter:fileSelectFilter,
      groups:fileGroups,
      callBack:(filePath, fileInfo) => {
        console.log(filePath, fileInfo,">>>");

        LoadNewFile(filePath);

      }
    });

  }


  useEffect(()=>{
    onExtraCtrlUpdate({
      browseNewFileToLoad:()=>BrowseNewFileToLoad(),
      LoadNewFile:(filePath)=>LoadNewFile(filePath)
    })
    BrowseNewFileToLoad();
  },[]);

  return (<div  className="s width12 height12">

    <BPG_FileBrowser key="BPG_FileBrowser"
      className="width8 modal-sizing"
      searchDepth={4}
      path="data/" visible={fileSelectorInfo !== undefined}
      BPG_Channel={BPG_Channel}

      onFileSelected={(filePath, fileInfo) => {
        setFileSelectorInfo(undefined);
        fileSelectorInfo.callBack(filePath, fileInfo);
      }}

      onCancel={() => {
        setFileSelectorInfo(undefined);
      }}
      
      fileGroups={(fileSelectorInfo !== undefined)?fileSelectorInfo.groups:undefined}
      fileFilter={(fileSelectorInfo !== undefined)?fileSelectorInfo.filter:undefined} />
    <CanvasComponent addClass="height12" edit_info={editInfo}/>
  </div>);
}
 