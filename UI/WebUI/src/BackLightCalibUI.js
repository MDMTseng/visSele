import React , { useState,useEffect,useContext,useRef } from 'react';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionUI");

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';

import ReactResizeDetector from 'react-resize-detector';
import {useMappedState,useDispatch} from 'redux-react-hook';

import { useSelector,connect } from 'react-redux' 

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
      case "asdasdas":
          // log.error(event);
          // this.props.ACT_ERROR();

          let rep = this.props.camera_calibration_report.reports[0];
          let mmpp=rep.mmpb2b/rep.ppb2b;

          let crop = event.data.crop.map(val=>val/mmpp);
          let down_samp_level = Math.floor(event.data.down_samp_level/mmpp*2)+1;
          if(down_samp_level<=0)down_samp_level=1;
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
  console.log(stage_light_report);
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

export default function BackLightCalibUI_rdx({ BPG_Channel , onCalibFinished }) {
  const [imageInfo, setImageInfo] = useState(undefined);
  const [inspReport, setInspReport] = useState(undefined);
  const todoList = useSelector(state => state.todoList);

  let staticObj = useRef({
    targetBri:200,
    briPreDiffSign:0,
    adjAlpha:1,
    fCount:0,
    finalRep:undefined
  });
  let c=staticObj.current;
  function ImgStageBackLightCalib()
  {
    
    clearTimeout(c.triggerTimeout);
    c.triggerTimeout=null;
    console.log(">>>>");
    BPG_Channel( "CI", 0, 
      {
        _PGID_:10004,
        _PGINFO_:{keep:true},
        definfo: {
          "type":"stage_light_report",
          "grid_size":[10,10],
          "nonBG_thres":20,
          "nonBG_spread_thres":180
        }
      },undefined,
      {
        resolve:(darr,mainFlow)=>{
          if(c.triggerTimeout===undefined)return;
          mainFlow(darr);
          console.log(darr);
          let reportInfo = darr.find(data=>data.type==="RP");
          //setInspReport(reportInfo);
          if(reportInfo==undefined)return;
          c.fCount++;
          if((c.fCount%2)!=0)return;
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

          let exposure=reportInfo.data.cam_param.exposure_time;
          if(exposure<100)exposure=100;
          exposure*=
            (((1-c.adjAlpha)*1+(c.adjAlpha)*c.targetBri/maxMean));
          console.log(exposure);
          BPG_Channel("ST",0,{CameraSetting:{exposure}});


          if(false&&reportInfo!==undefined&&  reportInfo.data.frames_left==0)
          {
            //BPG_Channel( "CI", 0, {_PGID_:10004,_PGINFO_:{keep:false}});
            // var enc = new TextEncoder();
            // BPG_Channel("SV",0,
            //     {filename:"data/stageLightReport.json"},
            //     enc.encode(JSON.stringify(finalCalibrationReport, null, 2)))

            // if(onCalibFinished!==undefined)
            // {

            // }
          }
        },
        reject:(e)=>{
          clearTimeout(c.triggerTimeout);
          c.triggerTimeout=null;
        }
      }
      );
  }
  useEffect(() => {
    BPG_Channel("ST",0,{CameraSetting:{exposure:1000}});
    // ImgStageBackLightCalib();
    ImgStageBackLightCalib();
    return ()=>{
      onCalibFinished(c.finalRep);
      //console.log(c.finalRep);
      BPG_Channel( "CI", 0, {_PGID_:10004,_PGINFO_:{keep:false}});
    }

  }, [])


  return (<div  className="s width12 height12">
    <CanvasComponent_rdx  addClass="s width12 height12"
      onCanvasInit={_ => _} BPG_Channel={BPG_Channel}/>
    
  </div>);
}
 

