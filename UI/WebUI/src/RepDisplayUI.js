import React , { useState,useEffect,useContext,useRef } from 'react';
import * as logX from 'loglevel';
let log = logX.getLogger("InspectionUI");
import html2canvas from 'html2canvas';

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
    switch (event.type) {
      case "down_samp_level_update":
        if(this.props.ALLOW_CONTROL_DOWN_SAMPLING_LEVEL!=true || this.props.BPG_Channel===undefined)
          break;
        // log.error(event);
        // this.props.ACT_ERROR();

        
        let cam_param = this.props.edit_info._obj.cameraParam;
        let mmpp = cam_param.mmpb2b / cam_param.ppb2b;

        let crop = event.data.crop.map(val => val / mmpp);
        let downSampleFactor=this.props.downSampleFactor||1;
        let down_samp_level = Math.floor(event.data.down_samp_level*downSampleFactor / mmpp) + 1;
        if (down_samp_level <= 0) down_samp_level = 1;
        else if (down_samp_level > 15) down_samp_level = 15;


        //log.info(crop,down_samp_level);
        this.props.BPG_Channel("ST", 0,
          {
            CameraSetting: {
              down_samp_level
            },
            ImageTransferSetup: {
              crop
            },
            LAST_FRAME_RESEND:true
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

    // log.info(this.ec_canvas,props.edit_info);
    if (this.ec_canvas !== undefined && props.edit_info!==undefined) {

      {
        this.ec_canvas.EditDBInfoSync(props.edit_info);
        // log.info("props.edit_info>>", props.edit_info);
        

        // this.ec_canvas.SetState({value:{[UI_SM_STATES.INSP_MODE]:UI_SM_STATES.INSP_MODE_NEUTRAL}});
        this.ec_canvas.draw();
      }
    }
  }

  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 20) return;
    // console.log(this.windowSize.width ,this.windowSize.height,width, height );
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
    // console.log(">>>");
  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" id="RepDisplay_canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}




export function RepDisplay({def,camera_param, reports,image,IGNORE_IMAGE_FIT_TO_SCREEN=false,ALLOW_CONTROL_DOWN_SAMPLING_LEVEL=false,BPG_Channel,downSampleFactor=1 }) {

  
  const [editInfo,setEditInfo]=useState(Edit_info_Empty());

  
  const _REF = React.useRef({firstImageSet:true});
  let _this=_REF.current;
  // const [editInfo,setEditInfo]=useState(Edit_info_Empty());
  
  
  function updateCamParam(newEditInfo,cam_param)
  {
    newEditInfo._obj.SetCameraParamInfo(cam_param);
    return newEditInfo;
  }
  function updateDef(newEditInfo,_def)
  {
    newEditInfo =newEditInfo._obj.rootDefInfoLoading(_def,newEditInfo)
    // console.log(newEditInfo);
    return newEditInfo;
  }

  function updateReport(newEditInfo,_reports)
  {
    if(Array.isArray(_reports)==false)
      _reports=[_reports]
    newEditInfo.reportStatisticState.trackingWindow=_reports;
    return newEditInfo;
  }
  
  function updateImage(newEditInfo,image)
  {
    return {...newEditInfo,img:image};
  }

  // useEffect(()=>{
  //   let newEditInfo = editInfo;


  //   if(def!==undefined)
  //     newEditInfo = updateDef(newEditInfo,def);
  //   if(image!==undefined)
  //     newEditInfo = updateImage(newEditInfo,image);
  //   if(camera_param!==undefined)
  //     newEditInfo = updateCamParam(newEditInfo,camera_param);
  //   if(reports!==undefined)
  //     newEditInfo = updateReport(newEditInfo,reports);


  //   setEditInfo(newEditInfo);
  // },[])



  useEffect(()=>{
    let newEditInfo = editInfo;
    if(newEditInfo._obj===undefined )
      newEditInfo._obj=new InspectionEditorLogic();
    if(def!==undefined && _this.def!=def)
    {
      delete def["featureSet_sha1"]
      newEditInfo = updateDef(newEditInfo,def);
    }

      
    if(image!==undefined)// && _this.image!=image)
    {
      
      // if(IGNORE_IMAGE_FIT_TO_SCREEN==true)
      // {
      //   image.IGNORE_IMAGE_FIT_TO_SCREEN=true;
      // }
      // else if(IGNORE_IMAGE_FIT_TO_SCREEN==false)
      // {
      //   image.IGNORE_IMAGE_FIT_TO_SCREEN=false;
      // }
      // if(_this.firstImageSet==true)
      // {
      //   image.IGNORE_IMAGE_FIT_TO_SCREEN=false;
      // }
      newEditInfo = updateImage(newEditInfo,image);
    }

    console.log(newEditInfo);
    if(camera_param!==undefined && _this.camera_param!=camera_param)
      newEditInfo = updateCamParam(newEditInfo,camera_param);
    if(reports!==undefined && _this.reports!=reports)
      newEditInfo = updateReport(newEditInfo,reports);
    
    _this.def=def;
    _this.image=image;
    _this.camera_param=camera_param;
    _this.reports=reports;
    setEditInfo(newEditInfo);
  },[def,image,camera_param,reports])

  if(_this.image!==undefined)
  {

    if(IGNORE_IMAGE_FIT_TO_SCREEN==true)
    {
      _this.image.IGNORE_IMAGE_FIT_TO_SCREEN=true;
    }
    else if(IGNORE_IMAGE_FIT_TO_SCREEN==false)
    {
      _this.image.IGNORE_IMAGE_FIT_TO_SCREEN=false;
    }
    if(_this.firstImageSet==true)
    {
      _this.firstImageSet=false;
      _this.image.IGNORE_IMAGE_FIT_TO_SCREEN=false;
    }
    // console.log(editInfo);
  
  }


  return (<div  className="s width12 height12">
    <CanvasComponent 
      addClass="height12" 
      edit_info={editInfo} 
      ALLOW_CONTROL_DOWN_SAMPLING_LEVEL={ALLOW_CONTROL_DOWN_SAMPLING_LEVEL} 
      BPG_Channel={BPG_Channel}
      downSampleFactor={downSampleFactor}/>
  </div>);
}
 




function default_infoDispParam()
{
  return{
    color:"white",
    background:"rgba(0,0,0,0.2)",
    hideInfoDetail:false,
    hideDefDetail:true,
    reportIdxHide:[],
  }
}


export default function RepDisplayUI_rdx({ BPG_Channel , onExtraCtrlUpdate }) {

  
  // const DICT = useSelector(state => state.UIData.DICT);

  let _this= useRef({}).current;

  const [fileSelectorInfo,setFileSelectorInfo]=useState(undefined);
  const [repImgInfo,SetRepImgInfo]=useState(undefined);
  const [cachedXREPList,setCachedXREPList]=useState(undefined);
  const [curIdx,setCurIdx]=useState(-1);
  const [repDispInfo,setRepDispInfo]=useState(undefined);

  const [infoDispParam,setInfoDispParam]=useState(default_infoDispParam());


    
  const machine_custom_setting = useSelector(state => state.UIData.machine_custom_setting);

  const [curFolderPath,setCurFolderPath]=useState(machine_custom_setting.InspSampleSavePath||"data/");
  // const _REF = React.useRef({
  //   iel: new InspectionEditorLogic(),
  // });
  let xreps="xreps";
  
  function LoadNewFile(filePath,fileInfo)
  {
    filePath = filePath.replace("." + xreps, "");
    // console.log(filePath);
    // console.log(fileInfo);
    _this.latest_filename=fileInfo.name.replace("." + xreps, "");
    BPG_Channel( "LD", 0,{ filename: filePath+"." + xreps,imgsrc: filePath,down_samp_level:1 },undefined,
    { resolve:(pkts,action_channal)=>{
      let SS=pkts.find(pkt=>pkt.type=="SS");
      let FL=pkts.find(pkt=>pkt.type=="FL");
      let IM=pkts.find(pkt=>pkt.type=="IM");
      // console.log(pkts,newEditInfo);

      let reports = FL.data.reports;
      if(Array.isArray(reports)==false)
        reports=[reports]
      let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);

      setRepDispInfo({
        camera_param:FL.data.camera_param,
        reports:reports,
        def:FL.data.defInfo
      })
      
      setInfoDispParam(default_infoDispParam())

      SetRepImgInfo(img_pros.data);

            
    }, reject:(e)=>{

    } });
  

    // BPG_Channel( "LD", 0,{ imgsrc: filePath,down_samp_level:1 },undefined,
    // { resolve:(pkts,action_channal)=>{
    //   let IM=pkts.find(pkt=>pkt.type=="IM");
    //   let img_pros= BPG_Protocol.map_BPG_Packet2Act(IM);
    //   // newEditInfo.img=img_pros.data;
    //   // newEditInfo.img.IGNORE_IMAGE_FIT_TO_SCREEN=true;
    //   // setEditInfo({...newEditInfo})

    //   SetRepImgInfo(img_pros.data);

    // }, reject:(e)=>{

    // } });

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

        // _this.Latest_file_path=
        LoadNewFile(filePath,fileInfo);

      }
    });

  }


  useEffect(()=>{
    BrowseNewFileToLoad();
  },[]);
  function downloadImage(data, filename) {
    var a = document.createElement('a');
    a.href = data;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
  }
  function getLines(ctx, text, maxWidth,lineBreakChars=[" "]) {
    // var sections = word_char_based_split?text.split(" "):text.split("");
    let sections=[];
    let idxHead=0;
    for (let i=0; i < text.length; i++) {
      let chr=String.fromCharCode(text.charCodeAt(i));
      
      if(lineBreakChars.includes(chr)||i==text.length-1)
      {
        sections.push(text.substring(idxHead,i+1));
        idxHead=i+1;
      }
    }
    // console.log(sections);

    let lines = [];
    let currentLine = sections[0];

    for (let i = 1; i < sections.length; i++) {
      let section = sections[i];
      let width = ctx.measureText(currentLine + section).width;
      // console.log(width)
      if (width < maxWidth) {
          currentLine +=  section;
      } else {
          lines.push(currentLine);
          currentLine = section;
      }
    }
    lines.push(currentLine);
    return lines;
}
  useEffect(()=>{
    onExtraCtrlUpdate({
      browseNewFileToLoad:()=>BrowseNewFileToLoad(),
      LoadNewFile:(filePath)=>LoadNewFile(filePath),
      loadNext:()=>{
        let newIdx=(curIdx+1)%cachedXREPList.length;
        setCurIdx(newIdx);
        console.log(cachedXREPList,newIdx);
        LoadNewFile(cachedXREPList[newIdx].path,cachedXREPList[newIdx]);
      },
      loadPrev:()=>{ 
        let newIdx=(curIdx-1);
        if(newIdx<0)newIdx+=cachedXREPList.length;
        setCurIdx(newIdx);
        // console.log(cachedXREPList,newIdx);
        LoadNewFile(cachedXREPList[newIdx].path,cachedXREPList[newIdx]);
      },
      imageSave:()=>{
        console.log("SAVE...");


        // var ctx = canvas.getContext('2d');
        // ctx.resetTransform();
        // let textHeight=14;
        // ctx.font = textHeight+"px Arial";
        // ctx.fillStyle = "red";
        // let text="PATH:"+_this.latest_filename;

        // let tlines=getLines(ctx,text,canvas.width,[" ","/","\\",":"]);
        // let drawY=50;
        // tlines.forEach(line=>{
        //   ctx.fillText(line, 10,drawY);
        //   drawY+=textHeight;
        // })


        let RepDisplayUI_ele = document.getElementById("RepDisplayUI");
        html2canvas(RepDisplayUI_ele).then(function(canvas) {
            // document.body.appendChild(canvas);
            let dataURL = canvas.toDataURL();

            // var dataURL = canvas.toDataURL("image/jpeg", 1.0);
    
            downloadImage(dataURL, _this.latest_filename+'.png');
        });
        // var dataURL = canvas.toDataURL("image/jpeg", 1.0);

        // downloadImage(dataURL, 'my-canvas.png');

      },
      canvasSave:()=>{
        console.log("SAVE...");

        let canvas = document.getElementById("RepDisplay_canvas");

        var ctx = canvas.getContext('2d');
        ctx.resetTransform();
        let textHeight=14;
        ctx.font = textHeight+"px Arial";
        ctx.fillStyle = "red";
        let text="PATH:"+_this.latest_filename;

        let tlines=getLines(ctx,text,canvas.width,[" ","/","\\",":"]);
        let drawY=50;
        tlines.forEach(line=>{
          ctx.fillText(line, 10,drawY);
          drawY+=textHeight;
        })


        var dataURL = canvas.toDataURL();//"image/jpeg", 1.0);

        downloadImage(dataURL, _this.latest_filename+'.png');

      }
    })
  },[cachedXREPList,curIdx]);

  // console.log(repDispInfo,repImgInfo,infoDispParam);

  let infoTableUI=null;
  
  if(repDispInfo!==undefined)
  {
    try{

    let curFileInfo=cachedXREPList[curIdx]
    infoTableUI=
    <table style={{color:infoDispParam.color,background:infoDispParam.background}}>
      <tbody>


        <tr>
          <td >檔名:</td>
          <td>{curFileInfo.name}</td>
        </tr>
        <tr>
          <td >檔案創建時間:</td>
          <td>{new Date(curFileInfo.ctime_ms).toLocaleString('en-US',{ hour12: false })}</td>
        </tr>
            
        <tr onClick={()=>{setInfoDispParam({...infoDispParam,hideInfoDetail:!infoDispParam.hideInfoDetail})}}>
          <td style={{textAlign:"left"}}>{infoDispParam.hideInfoDetail?"+":"-"}詳細資訊</td>
        </tr>
        {infoDispParam.hideInfoDetail?null:<>
          <tr onClick={()=>{setInfoDispParam({...infoDispParam,hideDefDetail:!infoDispParam.hideDefDetail})}}>
            <td >{infoDispParam.hideDefDetail?"+":"-"}檢測檔名稱:</td>
            <td>{repDispInfo.def.name}</td>
          </tr>
          {infoDispParam.hideDefDetail?null:<>
            <tr><td >sha1:</td>
            <td>{repDispInfo.def.featureSet_sha1}</td></tr>
            <tr><td >pre_sha1:</td>
            <td>{repDispInfo.def.featureSet_sha1_pre}</td></tr>
            <tr><td >root_sha1:</td>
            <td>{repDispInfo.def.featureSet_sha1_root}</td></tr>
          </>}
          
          {/* <tr>
          <td>原始檔路徑:</td>
          <td>{_this.latest_filename}</td>
          </tr> */}

          {repDispInfo.reports.map((rep,idx)=>{
            let repDispUI=null;
            if(infoDispParam.reportIdxHide.includes(idx)==false)
            {
              let jReps=rep.judgeReports;
              repDispUI=[]
              // repDispUI.push(<>
              //   <tr idx={idx+"_time"}>
              //   <td >時間</td>
              //   <td>{new Date(rep.add_time_ms).toLocaleString()}</td>
              //   </tr>
              
              // </>)


              repDispUI.push(jReps.map((jrep,jidx)=>(
                <tr idx={idx+"jrep"+jidx}>
                <td >{jrep.name}</td>
                <td>{jrep.value.toFixed(3)}</td>
                </tr>
              )))
            }

            return <>
              <tr onClick={()=>{
                let new_reportIdxHide;
                if(repDispUI==null)
                {
                  new_reportIdxHide=infoDispParam.reportIdxHide.filter(_idx=>_idx!=idx);
                }
                else
                {
                  new_reportIdxHide=[...infoDispParam.reportIdxHide,idx]
                }
                console.log(new_reportIdxHide);
              
                setInfoDispParam({...infoDispParam,reportIdxHide:new_reportIdxHide})
              }}>
                <td >{(repDispUI==null?"+":"-")+" "+idx}</td>
                <td >----------------</td>
              </tr>

              {repDispUI}
            </>
          })}
        </>}
      </tbody>
    </table>
    }
    catch(e)
    {

    }
  }




  return (<div  className="s width12 height12 overlayCon" id="RepDisplayUI">

    <BPG_FileBrowser key="BPG_FileBrowser"
      className="width8 modal-sizing"
      searchDepth={4}
      path={curFolderPath} visible={fileSelectorInfo !== undefined}
      BPG_Channel={BPG_Channel}

      onFileSelected={(filePath, fileInfo,folderStruct) => {
        console.log(fileInfo);
        let xrepLists=folderStruct.files
          .filter(f=>(f.type=="REG"&&f.name.endsWith(xreps)))
          .sort((f1,f2) => f1.ctime_ms>f2.ctime_ms);
        setCurFolderPath(folderStruct.path);
        setCachedXREPList(xrepLists);
        setFileSelectorInfo(undefined);
        
        let idx = xrepLists.findIndex((xrp)=>xrp.name==fileInfo.name);
        setCurIdx(idx);
        fileSelectorInfo.callBack(filePath, fileInfo);
      }}

      onCancel={() => {
        setFileSelectorInfo(undefined);
      }}
      
      fileGroups={(fileSelectorInfo !== undefined)?fileSelectorInfo.groups:undefined}
      fileFilter={(fileSelectorInfo !== undefined)?fileSelectorInfo.filter:undefined} />
    {/* <CanvasComponent addClass="height12" edit_info={editInfo}/> */}
    <RepDisplay {...repDispInfo} image={repImgInfo} IGNORE_IMAGE_FIT_TO_SCREEN/>
    <div className={"overlay"} >


      {infoTableUI}






    </div>
  </div>);
}
 
