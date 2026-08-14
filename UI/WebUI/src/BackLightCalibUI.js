import React , { useState,useEffect,useContext,useRef } from 'react';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.calib');

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import ComponentBoundary from './component/ComponentBoundary';

import ReactResizeDetector from 'react-resize-detector';
import {useMappedState,useDispatch} from 'redux-react-hook';

import { useSelector,connect } from 'react-redux' 

import Progress from 'antd/lib/progress';

class CanvasComponent extends React.Component {
  constructor(props) {
      super(props);
      this.windowSize={};
  }

  ec_canvas_EmitEvent(event) {
    switch(event.type)
    { 
      case DefConfAct.EVENT.ERROR:
          log.error(event);
          this.props.ACT_ERROR();
      break;
      case "down_samp_level_update":
          // log.error(event);
          // this.props.ACT_ERROR();

          // 儀器尺度改讀 lens_calib.json 的單一來源; camera_calibration
          // report 核心已不再發出。
          let mmpp = this.props.instrument_mmpp;
          if (!(mmpp > 0)) { log.warn("down_samp_level_update: 尚無 lens_calib.json 的 mmpp, 略過"); break; }

          let crop = event.data.crop.map(val=>val/mmpp);
          let down_samp_level = Math.floor(event.data.down_samp_level/mmpp*2)+1;
          if (!Number.isFinite(down_samp_level)) down_samp_level = 1; // R7: NaN escapes both clamps
          else if(down_samp_level<=0)down_samp_level=1;
          else if(down_samp_level>15)down_samp_level=15;
          
          
          //log.info(crop,down_samp_level);
          this.props.BPG_Channel("ST",0,
          {
            CameraSetting:{
              down_samp_level
            },
            ImageTransferSetup:{
              crop
            }
          });
      break;

    }
}


  componentDidMount() {
      this.ec_canvas = new EC_CANVAS_Ctrl.SLCALIB_CanvasComponent(this.refs.canvas);
      this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);
      this.props.onCanvasInit(this.ec_canvas);
      this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
      this.ec_canvas.resourceClean();
  }

  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas === undefined)return;
    this.ec_canvas.EditDBInfoSync(props.edit_info);
    this.ec_canvas.draw();
  }

  onResize(width, height) {
      if(Math.hypot(this.windowSize.width-width,this.windowSize.height-height)<5)return;
      if (this.ec_canvas !== undefined) {
          this.ec_canvas.resize(width, height);
          this.windowSize={
              width,height
          }
          this.updateCanvas(this.props.c_state);
      }
  }

  componentWillUpdate(nextProps, nextState) {
      this.updateCanvas(nextProps.c_state, nextProps);
  }

  render() {

      return (
          <div className={this.props.addClass}>
              <canvas ref="canvas" className="width12 HXF"/>
              <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
          </div>
      );
  }
}




const mapStateToProps_CanvasComponent = (state) => {
  //console.log("mapStateToProps",JSON.stringify(state.UIData.c_state));
  return {
    c_state: state.UIData.c_state,
    edit_info: state.UIData.edit_info,

    camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
    instrument_mmpp: state.UIData.instrument_mmpp,
  }
}



const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => 
{ 
  return{
    ACT_SUCCESS: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.SUCCESS))},
    ACT_Fail: (arg) => {dispatch(UIAct.EV_UI_ACT(DefConfAct.EVENT.FAIL))},
    ACT_EXIT: (arg) => {dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))},
    ACT_EDIT_TAR_UPDATE: (targetObj) => {dispatch(DefConfAct.Edit_Tar_Update(targetObj))},
    ACT_EDIT_TAR_ELE_CAND_UPDATE: (targetObj) =>  {dispatch(DefConfAct.Edit_Tar_Ele_Cand_Update(targetObj))},
    ACT_EDIT_SHAPELIST_UPDATE: (shapeList) => {dispatch(DefConfAct.Shape_List_Update(shapeList))},
    ACT_EDIT_SHAPE_SET: (shape_data) => {dispatch(DefConfAct.Shape_Set(shape_data))},
    ACT_EDIT_TAR_ELE_TRACE_UPDATE: (keyTrace) => {dispatch(DefConfAct.Edit_Tar_Ele_Trace_Update(keyTrace))},
  }
}
const CanvasComponent_rdx = connect(
    mapStateToProps_CanvasComponent,
    mapDispatchToProps_CanvasComponent)(CanvasComponent);


function stage_light_report_maxMean(stage_light_report)
{
  let maxMean=stage_light_report.grid_info.reduce((max,slr)=>{

    return (slr.mean>max)?slr.mean:max;
    if(slr.mean!=slr.mean)return mixMean;
    if(mixMean.mean>max)
    {
      return max;
    }
  },0);
  return maxMean;
}

let BACKLIGHT_CALIB_PGID_=10104;

export default function BackLightCalibUI_rdx({ BPG_Channel ,onExtraCtrlUpdate }) {
  const [imageInfo, setImageInfo] = useState(undefined);
  const [inspReport, setInspReport] = useState(undefined);
  const [curBriDiff, setCurBriDiff] = useState(NaN);
  let staticObj = useRef({
    targetBri:200,
    briPreDiffSign:0,
    adjAlpha:0.9,
    fCount:0,
    finalRep:undefined
  });
  let c=staticObj.current;
  function ImgStageBackLightCalib()
  {
    
    clearTimeout(c.triggerTimeout);
    c.triggerTimeout=null;
    log.debug("[boot]");
    BPG_Channel( "CI", 0, 
      {
        _PGID_:BACKLIGHT_CALIB_PGID_,
        _PGINFO_:{keep:true},
        definfo: {
          "type":"stage_light_report",
          "grid_size":[10,10],
          "nonBG_thres":100,
          "nonBG_spread_thres":180
        },
        IMG_ignore_calib:true
      },undefined,
      {
        resolve:(darr,mainFlow)=>{
          if(c.triggerTimeout===undefined)return;
          mainFlow(darr);
          let reportInfo = darr.find(data=>data.type==="RP");
          //setInspReport(reportInfo);
          if(reportInfo==undefined)return;
          c.fCount++;
          if((c.fCount%5)!=0)return;
          c.finalRep=reportInfo.data;

          let maxMean=
            stage_light_report_maxMean(reportInfo.data);
          if(maxMean<50)maxMean=50;
          //console.log(reportInfo);

          if(c.briPreDiffSign*(maxMean-c.targetBri)<0)
          {//There is a diff sign crossing 
            c.adjAlpha*=0.8;
          }
          c.briPreDiffSign=(maxMean-c.targetBri);
          setCurBriDiff(c.targetBri-maxMean);

          let exposure=reportInfo.data.cam_param.exposure_time;
          if(exposure<1)exposure=1;
          exposure*=
            (((1-c.adjAlpha)*1+(c.adjAlpha)*c.targetBri/maxMean));
          if(exposure>1000*1000)
          {
            exposure=1000*1000;
          }
          if(exposure<1)exposure=1;
          BPG_Channel("ST",0,{CameraSetting:{exposure}});


        },
        reject:(e)=>{
          clearTimeout(c.triggerTimeout);
          c.triggerTimeout=null;
        }
      }
      );
  }
  useEffect(() => {
    onExtraCtrlUpdate({
      currentReportExtract:()=>c.finalRep
    })
    BPG_Channel("ST",0,{CameraSetting:{exposure:1000}});
    
    BPG_Channel( "ST", 0,
    { CameraSetting: { ROI:[0,0,99999,99999] } })

    // ImgStageBackLightCalib();
    ImgStageBackLightCalib();
    return ()=>{
      //onCalibFinished(c.finalRep);
      //console.log(c.finalRep);
      BPG_Channel( "CI", 0, {_PGID_:BACKLIGHT_CALIB_PGID_,_PGINFO_:{keep:true}});
    }

  }, [])

  let diff = curBriDiff>0?curBriDiff:-curBriDiff;
  diff-=5;//within +/- 5 range it's an OK
  if(diff<0)diff=0;
  let progress=(1-diff/150)*100;
  if(progress<0)progress=0;
  return (<div  className="s width12 height12 overlayCon">
    <ComponentBoundary name="BackLightCalibCanvas" fallbackHeight="60vh">
      <CanvasComponent_rdx  addClass="s width12 height12"
        onCanvasInit={_ => _} BPG_Channel={BPG_Channel}/>
    </ComponentBoundary>
    
    <div className={"s overlay"} style={{width:"auto", height:"auto"}}>
      {/* {curBriDiff} */}
      
      <Progress type="circle" percent={progress.toFixed(1)} />
    </div>
  </div>);
}
 

