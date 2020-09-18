import React , { useState,useEffect,useContext,useRef } from 'react';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionUI");

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';

import ReactResizeDetector from 'react-resize-detector';

import { useSelector,connect } from 'react-redux' 

import BPG_Protocol from 'UTIL/BPG_Protocol.js';




function CanvasComponent({ image,addClass,BPG_Channel,onExtraCtrlUpdate}) 
{

  const rdx_edit_info = useSelector(state => state.UIData.edit_info);
  let _ = useRef({
    canvas: undefined,
    windowSize:{width:0,height:0}
  });
  const canvasRef = useRef(null);
  
  function ec_canvas_EmitEvent(event) {
    switch(event.type)
    { 
      case DefConfAct.EVENT.ERROR:
          log.error(event);
          this.props.ACT_ERROR();
      break;
      case "asdasdas":
          // log.error(event);
          // this.props.ACT_ERROR();

          let rep = rdx_edit_info.camera_calibration_report.reports[0];
          let mmpp=rep.mmpb2b/rep.ppb2b;

          let crop = event.data.crop.map(val=>val/mmpp);
          let down_samp_level = Math.floor(event.data.down_samp_level/mmpp*2)+1;
          if(down_samp_level<=0)down_samp_level=1;
          else if(down_samp_level>15)down_samp_level=15;
          
          
          //log.info(crop,down_samp_level);
          BPG_Channel("ST",0,
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

  useEffect(() => {
    onExtraCtrlUpdate(
      {
        clearMeasureSet:()=>{
          _.current.ec_canvas.clearMarkSet();
          _.current.ec_canvas.draw();
        },
        removeOneMeasureSet:()=>{
          _.current.ec_canvas.removeOneMarkSet();
          _.current.ec_canvas.draw();
        },
        setDistanceType:(type)=>{
          _.current.ec_canvas.setDistanceType(type);
          _.current.ec_canvas.draw();
        },
      }
    )
    return ()=>{
    }

  }, [])


  useEffect(() => {
    if(_.current.ec_canvas!==undefined)return;
    _.current.ec_canvas=new EC_CANVAS_Ctrl.InstInsp_CanvasComponent(canvasRef.current);

    _.current.ec_canvas.EditDBInfoSync(rdx_edit_info);
    _.current.ec_canvas.EmitEvent =ec_canvas_EmitEvent;
    
    _.current.ec_canvas.resize(_.current.windowSize.width, _.current.windowSize.height);
    _.current.ec_canvas.draw();
    
    // 
    return ()=>{
      //console.log(c.finalRep);
      // BPG_Channel( "CI", 0, {_PGID_:10004,_PGINFO_:{keep:false}});
    }

  }, [canvasRef])


  useEffect(() => {
    if(_.current.ec_canvas===undefined)return;
    // console.log(image);
    _.current.ec_canvas.SetImg(image);
    _.current.ec_canvas.draw();

  }, [image])

  
  function onResize(width, height) {
    _.current.windowSize={width,height};

    console.log("onResize::::::::");
    if(_.current.ec_canvas!==undefined)
    {
      _.current.ec_canvas.resize(_.current.windowSize.width, _.current.windowSize.height);
      _.current.ec_canvas.draw();
    }

    // if(Math.hypot(_.current.windowSize.width-width,_.current.windowSize.height-height)<5)return;
    // if (this.ec_canvas !== undefined) {
    //     this.ec_canvas.resize(width, height);
    //     this.windowSize={
    //         width,height
    //     }
    //     this.updateCanvas(this.props.c_state);
    // }
  }

  
  return (
    <div className={addClass}>
        <canvas ref={canvasRef} className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={onResize}/>
    </div>
  );

}

export default function InstInspUI_rdx({ BPG_Channel,onExtraCtrlUpdate  }) {
  const [inspReport, setInspReport] = useState(undefined);
  const [imageInfo, setImageInfo] = useState(undefined);
  const [canvExCtrl, setCanvExCtrl] = useState(undefined);
  

  // const ec_canvasRef = useRef(null);
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
    
    BPG_Channel( "CI", 0, 
      {
        _PGID_:10004,
        _PGINFO_:{keep:true},
        definfo: {
          "type":"nop",
        }
      },undefined,
      {
        resolve:(pkts,mainFlow)=>{
          // console.log(pkts);

          let SS=pkts.find(pkt=>pkt.type=="SS");
          let FL=pkts.find(pkt=>pkt.type=="FL");
          let IM=pkts.find(pkt=>pkt.type=="IM");

          let img_act= BPG_Protocol.map_BPG_Packet2Act(IM);
          setImageInfo(img_act.data);
          // newEditInfo.img=img_pros.data;
          
        },
        reject:(e)=>{
        }
      }
      );
  }


  useEffect(() => {
    ImgStageBackLightCalib();
  
    return ()=>{
      BPG_Channel( "CI", 0, {_PGID_:10004,_PGINFO_:{keep:false}});
    }}, [])


  useEffect(() => {
    onExtraCtrlUpdate({
      takeNewImage:()=>{
        console.log("takeNewImage")
      },
      ...canvExCtrl
    })
  }, [canvExCtrl])

  const FancyButton = React.forwardRef((props, ref) => (
    <button ref={ref} className="FancyButton">
      {props.children}
    </button>
  ));
  // useEffect(() => {
  //   console.log(ec_canvasRef);

  //   onExtraCtrlUpdate({
  //     takeNewImage:()=>{
  //       console.log("takeNewImage")
  //     },

  //     clearMeasureSet:()=>{
  //       console.log("clearMeasureSet",ec_canvasRef)
  //       ec_canvasRef.current.clearMeasureSet();
  //     },

  //     removeOneMeasureSet:()=>{
  //       console.log("removeOneMeasureSet",ec_canvasRef)
  //       ec_canvasRef.current.removeOneMeasureSet();
  //     }
  //   })
  // }, [ec_canvasRef])

  return (<div  className="s width12 height12">
    <CanvasComponent  addClass="s width12 height12"
     image={imageInfo} BPG_Channel={BPG_Channel} 
     onExtraCtrlUpdate={setCanvExCtrl}
      onCanvasInit={_ => _} BPG_Channel={BPG_Channel}/>
    
  </div>);
}
 

