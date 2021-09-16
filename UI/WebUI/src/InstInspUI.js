import React , { useState,useEffect,useContext,useRef } from 'react';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionUI");

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';

import ReactResizeDetector from 'react-resize-detector';

import { useSelector,connect } from 'react-redux' 

import { xstate_GetCurrentMainState, GetObjElement, isString } from 'UTIL/MISC_Util';
import BPG_Protocol from 'UTIL/BPG_Protocol.js';

import Modal from "antd/lib/modal";
import  Table  from 'antd/lib/table';
import InputNumber from 'antd/lib/input-number';
let stream_PGID_=10005;


function CanvasComponent({ image,addClass,BPG_Channel,onExtraCtrlUpdate}) 
{

  const rdx_edit_info = useSelector(state => state.UIData.edit_info);
  const FILE_default_camera_setting = useSelector(state => state.UIData.FILE_default_camera_setting);
  let _ = useRef({
    canvas: undefined,
    windowSize:{width:0,height:0},
    isFirstImage:true,
    currentValue:[],
    targetValue:[],
    adjCalibInfo:{}
  });


  let _this=_.current;
  const canvasRef = useRef(null);
  
  const [measurePointPair,setMeasurePointPair]=useState([]);
  const [updateKickCount,setUpdateKickCount]=useState(0);

  const [modal_view,setModal_view]=useState();
  
  function BUMP_KICK()
  {
    setUpdateKickCount(updateKickCount+1);
  }

  function ec_canvas_EmitEvent(event) {
    switch(event.type)
    { 
      case DefConfAct.EVENT.ERROR:
          log.error(event);
          this.props.ACT_ERROR();
      break;
      case "point_pair_update":
        // _.current.cachedPoints=event.data.pts;
        let pts = event.data.pts;
        console.log(event);

        _this.currentValue=pts.map((pt,idx)=>{
          let pt0=pt[0];
          let pt1=pt[1];
          return Math.hypot(pt[0].x-pt[1].x,pt[0].y-pt[1].y);
        });
        _this.targetValue=pts.map((pt,idx)=>_this.targetValue[idx]);
        // setUpdateKick({});
        setMeasurePointPair(pts);

      break;
      case "down_samp_level_update":
          // log.error(event);
          // this.props.ACT_ERROR();

          let rep = rdx_edit_info.camera_calibration_report.reports[0];
          let mmpp=rep.mmpb2b/rep.ppb2b;

          let crop = event.data.crop.map(val=>val/mmpp);

          
          let downSampleFactor=FILE_default_camera_setting.downSampleFactor||1;
          let down_samp_level = Math.floor(event.data.down_samp_level*downSampleFactor/mmpp)+1;
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
    console.log("UPDATE");

    let ExtraControl=
    {
      clearMeasureSet:()=>{
        _.current.ec_canvas.clearMarkSet();
        _.current.ec_canvas.draw();
      },
      removeOneMeasureSet:()=>{
        _.current.ec_canvas.removeOneMarkSet();
        _.current.ec_canvas.draw();
      },
      
      togglePointPairMMPPAdjust:()=>{
        if(modal_view===undefined)
        {

          // <Table columns={columns} dataSource={data} />

          setModal_view({
            onCancel:()=>setModal_view(),
            onOk:()=>setModal_view(),
            footer:null,
            // view:<>dddd</>
            view_sele:"modifyTable"
          })
        }
        else
        {
          setModal_view();
        }
      },
    }

    function ModalPopup(msg)
    {
      
      setModal_view({
        onCancel:()=>setModal_view(),
        onOk:()=>{
          setModal_view();
        },
        view_con:msg
      })
    }

    // if(_this.adjCalibInfo.adj_mmpb2b!==undefined && _this.adjCalibInfo.adj_ppb2b!==undefined)
    {
      ExtraControl.saveCameraParam=()=>{
        if(_this.adjCalibInfo.adj_mmpb2b!==undefined && _this.adjCalibInfo.adj_ppb2b!==undefined)
        {
          setModal_view({
            onCancel:()=>setModal_view(),
            onOk:()=>{
              
              let targetPath = "data/default_camera_param.json";
              BPG_Channel("LD",0,
              {filename: targetPath},
              undefined,
              { 
                resolve:(pkts,action_channal)=>{

                  let FL=pkts.find(pkt=>pkt.type=="FL");
                  if(FL===undefined)
                  {
                    ModalPopup(`檔案:${targetPath} 找不到!`);
                    return;
                  }
                  try{

                    FL.data.reports[0].mmpb2b=_this.adjCalibInfo.adj_mmpb2b;
                    FL.data.reports[0].ppb2b=_this.adjCalibInfo.adj_ppb2b;
                  }
                  catch(e)
                  {
                    ModalPopup(`修改檔案時錯誤:${e}`);
                    return;
                  }

                  var enc = new TextEncoder();

                  BPG_Channel("SV",0,
                  {filename: targetPath},
                  enc.encode(JSON.stringify(FL.data, null, 2),
                  { 
                    resolve:(pkts,action_channal)=>{
                      
                      ModalPopup(`OK`);
                    }, 
                    reject:(e)=>{
                      ModalPopup(`錯誤:${e}`);
                    } 
                  }));


                }, 
                reject:(e)=>{
                  
                  console.log("ERROR",e);
                  ModalPopup(`錯誤:${e}`);
                  return;
                } 
              });
  
              setModal_view();
            },
            // view:<>dddd</>
            view_con:<>
              確定更新校正參數?<br/> 
              <div>
              factor:{_this.adjCalibInfo.mmppFactor}=<br/> 
              mmpb2b:{_this.adjCalibInfo.adj_mmpb2b}<br/>
              ppb2b:{_this.adjCalibInfo.adj_ppb2b}
              </div>
            </>
          })
        }
        else
        {
          ModalPopup("請先使用設定以取得校正參數")
        }
       
      };
    }



    onExtraCtrlUpdate(ExtraControl)
    return ()=>{
    }

  }, [updateKickCount])


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
      // BPG_Channel( "CI", 0, {_PGID_:stream_PGID_,_PGINFO_:{keep:false}});
    }

  }, [canvasRef])


  useEffect(() => {
    if(_.current.ec_canvas===undefined || image===undefined)return;
    // console.log(image);
    
    image.IGNORE_IMAGE_FIT_TO_SCREEN=true;
    if(_this.isFirstImage==true)
    {
      image.IGNORE_IMAGE_FIT_TO_SCREEN=false;
      _this.isFirstImage=false;
    }
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

  function updateCalcMMPP(dataX)
  {
    let camInfo=GetObjElement(rdx_edit_info,["camera_calibration_report","reports",0]);


    let mmpb2b=NaN;
    let ppb2b=NaN;
    if(camInfo!==undefined)
    {
      mmpb2b=camInfo.mmpb2b;
      ppb2b=camInfo.ppb2b;
    }

    // rdx_edit_info
    let adjInfo=dataX.reduce((avg,dat)=>{
      if(dat.tarDist===undefined)return avg;
      let W = dat.tarDist;
      return {
        W:avg.W+W,
        sum:avg.sum+W*dat.tarDist/dat.curDist
      }
    },{
      W:0,
      sum:0
    });
    let mmppFactor = (adjInfo.sum/adjInfo.W);
    console.log(mmpb2b,ppb2b/mmppFactor,dataX);


    
    _this.adjCalibInfo={
      cur_mmpb2b:mmpb2b,
      cur_ppb2b:ppb2b,
      mmppFactor,
      adj_mmpb2b:mmpb2b,
      adj_ppb2b:ppb2b/mmppFactor,
    }
    
  }

  let modalContent=null;
  let modalTitle=null;
  console.log(rdx_edit_info);
  if(modal_view!==undefined)
  switch(modal_view.view_sele)
  {
    case "modifyTable":
    {
      console.log(modal_view);
      modalTitle=<div>
        factor:{_this.adjCalibInfo.mmppFactor}=<br/> 
        mmpb2b:{_this.adjCalibInfo.adj_mmpb2b}<br/>
        ppb2b:{_this.adjCalibInfo.adj_ppb2b}
      </div>;
      const data = 
      measurePointPair.map((ptp,i)=>({
        key:i,
        curDist:_this.currentValue[i],
        tarDist:_this.targetValue[i],
        ptp:ptp
      }))

      const columns = [
        {
          title: '目前長度',
          dataIndex: 'curDist',
          key: 'curDist',
        },
        {
          title: '目標長度',
          dataIndex: 'tarDist',
          key: 'tarDist',
          render:(text, record, index)=>{
            // console.log(text, record, index);
            return <InputNumber 
              onChange={(e)=>{
                // console.log(e);
                _this.targetValue[index]=e;


                updateCalcMMPP(measurePointPair.map((ptp,i)=>({
                  key:i,
                  curDist:_this.currentValue[i],
                  tarDist:_this.targetValue[i],
                  ptp:ptp
                })))



                BUMP_KICK();
              }}
              
              value={record.tarDist}

              >
              
            </InputNumber>;
          }
        }
      ];



      modalContent = <Table columns={columns} dataSource={data} />
      break;
    }
    default:
      modalContent = modal_view.view_con;
  }
  
  return (
    <div className={addClass}>
        <canvas ref={canvasRef} className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={onResize}/>
        
        <Modal 
        title={modalTitle}
        {...modal_view} 
        visible={modal_view !== undefined}>
          {modalContent}
        </Modal>
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
        _PGID_:stream_PGID_,
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
          if(IM===undefined)
          {
            throw "IM is undefined!";
          }
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
      BPG_Channel( "CI", 0, {_PGID_:stream_PGID_,_PGINFO_:{keep:true}});
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
 

