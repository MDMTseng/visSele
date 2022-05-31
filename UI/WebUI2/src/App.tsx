import React from 'react';
import { useState, useEffect,useRef,useMemo } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio } from 'antd';
const { SubMenu } = Menu;

import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,MoreOutlined,PlayCircleOutlined } from '@ant-design/icons';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement,ID_debounce,ID_throttle,ObjShellingAssign} from './UTIL/MISC_Util';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col,Input,Tag,Modal,message } from 'antd';


import './basic.css';

// import ReactJsoneditor from 'jsoneditor-for-react';

// declare module 'jsoneditor-react'jsoneditor-for-react"

// import 'jsoneditor-react/es/editor.min.css';

let INSP1_REP_PGID_ = 51000

let HACK_do_Camera_Check=false;
const { TabPane } = Tabs;
const { Header, Content, Footer,Sider } = Layout;
type type_InspDef={
  name:string,
  
  cameraList:{id:string,[key:string]:any}[]

  rules:{
    name:string,
    id:string
    [key:string]:any
  }[]
}

let tar_camera_id="BMP_carousel_0";//"MindVision-040010720303";

function CameraDiscover({onCameraSelected}:{onCameraSelected:(...param:any)=>void})
{
  
  const _ = useRef({});
  let _this=_.current;
  const dispatch = useDispatch();
  const CORE_API_INFO = useSelector((state:StoreTypes) => state.EXT_API[CORE_ID]);
  const ACT_EXT_API_REGISTER= (...p:Parameters<typeof EXT_API_REGISTER>) => dispatch(EXT_API_REGISTER(...p));
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const ACT_EXT_API_UPDATE= (...p:Parameters<typeof EXT_API_UPDATE>) => dispatch(EXT_API_UPDATE(...p));
  const ACT_EXT_API_CONNECTED= (...p:Parameters<typeof EXT_API_CONNECTED>) => dispatch(EXT_API_CONNECTED(...p));
  const ACT_EXT_API_DISCONNECTED= (...p:Parameters<typeof EXT_API_DISCONNECTED>) => dispatch(EXT_API_DISCONNECTED(...p));
  const [camList,setCamList]=useState<{id:string,driver_name:string,name:string,model:string}[]|undefined>(undefined);

  useEffect(()=>{
    ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
      let api=_api as BPG_WS;//cast
      api.cameraDiscovery().then((ret:any)=>{
        console.log(ret);
        setCamList(ret[0].data)
      })
    })
  },[])

  if(camList===undefined)
  {
    return <>
    
     掃描中
  
    </>
  }

  if(camList.length==0)
  {
    return <>
    
     無相機
  
    </>
  }

  return <>
    
    {camList.map(cam=><Button key={"id_"+cam.id} onClick={()=>{
      onCameraSelected(cam);
      }}>{cam.id}</Button>)}
  
  
  
  </>
}


type IMCM_type=
{
  camera_id:string,
  trigger_id:number,
  trigger_tag:string,
  image_info:{
    full_height: number
    full_width: number
    height: number
    image: ImageData
    offsetX: number
    offsetY: number
    scale: number
    width: number
  }
}


function ColorRegionLocating_SingleRegion({srule,onRuleChange,canvas_obj}:
  {
    srule:any,
    onRuleChange:(...param:any)=>void,
    canvas_obj:DrawHook_CanvasComponent
  })
{
  
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);
  let srule_Filled={
    
    region:[0,0,0,0],
    colorThres:10,
    hough_circle:{
      dp:1,
      minDist:5,
      param1:100,
      param2:30,
      minRadius:5,
      maxRadius:15
    },
    hsv:{
      rangeh:{
        h:180,s:255,v:255
      },
      rangel:{
        h:0,s:0,v:0
      },
    },

    ...srule
  }
  const _this = useRef<any>({}).current;
  return <>
    <Button key={">>>"} onClick={()=>{
        canvas_obj.UserRegionSelect((info,state)=>{
          if(state==2)
          {
            console.log(info);
            
            let x,y,w,h;

            x=info.pt1.x;
            w=info.pt2.x-info.pt1.x;
        
            y=info.pt1.y;
            h=info.pt2.y-info.pt1.y;
        
        
            if(w<0){
              x+=w;
              w=-w;
            }
            
            if(h<0){
              y+=h;
              h=-h;
            }

            let newRule={...srule_Filled,region:[Math.round(x),Math.round(y),Math.round(w),Math.round(h)]};
            onRuleChange(newRule)

            canvas_obj.UserRegionSelect(undefined)
          }
        })
      }}>設定範圍</Button>
    
      <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onRuleChange(undefined);
                // onRuleChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
{/* 
    <br/>hough_circle
    <Slider defaultValue={srule_Filled.hough_circle.minRadius} max={100} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,minRadius:v}};
      onRuleChange(newRule)
    },()=>_this.trigTO=undefined,500);

    }}/>
    
    <Slider defaultValue={srule_Filled.hough_circle.maxRadius} max={100} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,maxRadius:v}};
        onRuleChange(newRule)
      },()=>_this.trigTO=undefined,500);

    }}/>  */}




    <>
    
    
    <br/>結果顯示
    <Slider defaultValue={srule_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onRuleChange(ObjShellingAssign(srule_Filled,["resultOverlayAlpha"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>

    <br/>HSV
    <Slider defaultValue={srule_Filled.hsv.rangeh.h} max={180} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","h"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.h} max={180} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangel","h"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>



    <Slider defaultValue={srule_Filled.hsv.rangeh.s} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","s"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.s} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangel","s"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>




    <Slider defaultValue={srule_Filled.hsv.rangeh.v} max={255} onChange={(v)=>{

    _this.trigTO=
    ID_debounce(_this.trigTO,()=>{
      onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangeh","v"],v));
    },()=>_this.trigTO=undefined,500);

    }}/>
    <Slider defaultValue={srule_Filled.hsv.rangel.v} max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{

        onRuleChange(ObjShellingAssign(srule_Filled,["hsv","rangel","v"],v));
      },()=>_this.trigTO=undefined,500);

    }}/>





    </>

    <br/>colorThres
    <Slider defaultValue={srule_Filled.colorThres}  max={255} onChange={(v)=>{

      _this.trigTO=
      ID_throttle(_this.trigTO,()=>{
        onRuleChange(ObjShellingAssign(srule_Filled,["colorThres"],v));
      },()=>_this.trigTO=undefined,200);

    }}/>




  </>

}



function SingleTargetVIEWUI_ColorRegionLocating({readonly,width,height,style=undefined,renderHook,IMCM_group,rule,report,onRuleChange}:{
  readonly:boolean,
  style?:any,
  width:string,height:string,
  renderHook:((ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,rule:any)=>void)|undefined,
  IMCM_group:{[trigID:string]:IMCM_type},
  rule:any,
  report:any,
  onRuleChange:(updatedRule:any,doInspUpdate:boolean)=>void}){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined,
    drawHooks:[],
    ctrlHooks:[]


  });
  let _this=_.current;
  let c_report:any = undefined;
  if(_this.cache_report!==report)
  {
    if(report!==undefined)
    {
      _this.cache_report=report;
    }
  }
  c_report=_this.cache_report;


  useEffect(() => {

    _this.cache_report=undefined;
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, [rule]); 
  // console.log(IMCM_group,report);
  // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  
  enum editState {
    Normal_Show = 0,
    Region_Edit = 1,
    CalibDataCollection = 100,
  }
  
  const [stateInfo,setStateInfo]=useState<{st:editState,info:any}[]>([{
    st:editState.Normal_Show,
    info:undefined
  }]);

  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);

  
  const [queryCameraList,setQueryCameraList]=useState<any[]|undefined>(undefined);
  const [delConfirmCounter,setDelConfirmCounter]=useState(0);

  
  let stateInfo_tail=stateInfo[stateInfo.length-1];

  // function pushInSendGCodeQ()
  // {
  //   if(_this.isSendWaiting==true || _this.gcodeSeq.length==0)
  //   {
  //     return;
  //   }
  //   const gcode = _this.gcodeSeq.shift();
  //   if(gcode==undefined || gcode==null)return;
  //   _this.isSendWaiting=true;
  //   ACT_WS_GET_OBJ((api)=>{
  //     api.send({"type":"GCODE","code":gcode},
  //     (ret)=>{
  //       console.log(ret);
  //       _this.isSendWaiting=false;
  //       pushInSendGCodeQ(_this.gcodeSeq);

  //     },(e)=>console.log(e));
  //   })
  // }


  useEffect(() => {
    let newIMCM=IMCM_group[rule.trigger_tag+rule.camera_id];
    // console.log(newIMCM,rule.trigger_tag);
    if(Local_IMCM===newIMCM)return;
    
    if(newIMCM===undefined)return;
    _this.imgCanvas.width = newIMCM.image_info.width;
    _this.imgCanvas.height = newIMCM.image_info.height;
    // console.log(IMCM.image_info);
    let ctx2nd = _this.imgCanvas.getContext('2d');
    ctx2nd.putImageData(newIMCM.image_info.image, 0, 0);
    setLocal_IMCM(newIMCM);
    if(_this.canvasComp!==undefined)
    {
      _this.canvasComp.draw();
    }
  }, [IMCM_group]); 

  function drawRegion(g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent,region:{x:number,y:number,w:number,h:number},lineWidth:number)
  {
    let ctx = g.ctx;
    // ctx.lineWidth = 5;

    let x = region.x;
    let y = region.y;
    let w = region.w;
    let h = region.h;
    ctx.beginPath();
    ctx.setLineDash([lineWidth*10,lineWidth*3,lineWidth*3,lineWidth*3]);
    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth;
    ctx.rect(x,y,w,h);
    ctx.stroke();
    ctx.closePath();

    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth*2/3;
    canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2}, lineWidth*2/3);



  }





  let EDIT_UI= null;
  
  switch(stateInfo_tail.st)
  {
    
    case editState.Normal_Show:


      let EditUI=null;
      if(readonly==false)
      {
        EditUI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let newRule={...rule};
          newRule.regionInfo.push({region:[0,0,0,0],colorThres:10});
          onRuleChange(newRule,false)

          
          setStateInfo([...stateInfo,{
            st:editState.Region_Edit,
            info:{
              idx:newRule.regionInfo.length-1
            }
          }])

        }}>+</Button>

        

        {rule.regionInfo.map((region:any,idx:number)=>{
          return <Button key={"_"+idx} onClick={()=>{
            if(_this.canvasComp===undefined)return;


            setStateInfo([...stateInfo,{
              st:editState.Region_Edit,
              info:{
                idx:idx
              }
            }])




          }}>{"idx:"+idx}</Button>
        })}
        </>
      }

      EDIT_UI=<>
        
        <Input maxLength={100} value={rule.id} 
          style={{width:"100px"}}
          onChange={(e)=>{
            console.log(e.target.value);

            let newRule={...rule};
            newRule.id=e.target.value;
            onRuleChange(newRule,false)


          }}/>

        <Input maxLength={100} value={rule.type} disabled
          style={{width:"100px"}}
          onChange={(e)=>{
            
          }}/>

        <Input maxLength={100} value={rule.sampleImageFolder}  disabled
          style={{width:"100px"}}
          onChange={(e)=>{
          }}/>


        <Dropdown
          overlay={<>
            <Menu>
              {
                queryCameraList===undefined?
                  <Menu.Item disabled danger>
                  <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                    Press to update
                  </a>
                  </Menu.Item>
                  :
                  queryCameraList.map(cam=><Menu.Item key={cam.id} 
                  onClick={()=>{
                    let newRule={...rule};
                    newRule.camera_id=cam.id;
                    HACK_do_Camera_Check=true;
                    onRuleChange(newRule,true)
                  }}>
                    {cam.id}
                  </Menu.Item>)

              }
            </Menu>
          </>}
        >
          <Button onClick={()=>{
            // queryCameraList
            setQueryCameraList(undefined);
            BPG_API.cameraDiscovery(true)
              .then((e: any)=>{
                console.log(e);
                setQueryCameraList(e[0].data)
              })
            // let api=await getAPI(CORE_ID) as BPG_WS;
            // let cameraListInfos=await api.cameraDiscovery() as any[];
            // let CM=cameraListInfos.find(info=>info.type=="CM")
            // if(CM===undefined)throw "CM not found"
            // console.log(CM.data);
            // return CM.data as {name:string,id:string,driver_name:string}[];
            
          }}>{rule.camera_id}</Button>
        </Dropdown>



        <Input maxLength={100} value={rule.trigger_tag} 
          style={{width:"100px"}}
          onChange={(e)=>{
            let newRule={...rule};
            newRule.trigger_tag=e.target.value;
            onRuleChange(newRule,false)
        }}/>

        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter+1}次`}
            onConfirm={()=>{}}
            onCancel={()=>{}}
            okButtonProps={{danger:true,onClick:()=>{
              if(delConfirmCounter!=0)
              {
                setDelConfirmCounter(delConfirmCounter-1);
              }
              else
              {
                onRuleChange(undefined,false)
              }
            }}}
            okText={"Yes:"+delConfirmCounter}
            cancelText="No"
          >
          <Button danger type="primary" onClick={()=>{
            setDelConfirmCounter(5);
          }}>DEL</Button>
        </Popconfirm> 
        <br/>
        <Button onClick={()=>{

          onRuleChange(rule,true);
        }}>SHOT</Button>



        {EditUI}


      </>


      break;

    case editState.Region_Edit:


      if(rule.regionInfo.length<=stateInfo_tail.info.idx)
      {
        break;
      }
      
      let regionInfo=rule.regionInfo[stateInfo_tail.info.idx];

      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>
        <ColorRegionLocating_SingleRegion 
          srule={regionInfo} 
          onRuleChange={(newRule_sregion)=>{
            // console.log(newRule);

            
            let newRule={...rule};
            if(newRule_sregion!==undefined)
            {
              newRule.regionInfo[stateInfo_tail.info.idx]=newRule_sregion;
            }
            else
            {
              
              newRule.regionInfo.splice(stateInfo_tail.info.idx, 1);
              
              let new_stateInfo=[...stateInfo]
              new_stateInfo.pop();

              setStateInfo(new_stateInfo)

            }

            onRuleChange(newRule,true)
            // _this.sel_region=undefined

          }}
          canvas_obj={_this.canvasComp}/>
      </>

      break;
    

    case editState.CalibDataCollection:
      EDIT_UI=<>
        <Button key={"_"+-1} onClick={()=>{
          
          let new_stateInfo=[...stateInfo]
          new_stateInfo.pop();

          setStateInfo(new_stateInfo)
        }}>{"<"}</Button>









        </>
      break;
  }
  

  
  return <div style={{...style,width,height}}  className={"overlayCon"}>

    <div className={"overlay"} >

      {EDIT_UI}

    </div>

    
    <HookCanvasComponent dhook={(ctrl_or_draw:boolean,g:type_DrawHook_g,canvas_obj:DrawHook_CanvasComponent)=>{
      _this.canvasComp=canvas_obj;
      // console.log(ctrl_or_draw);
      if(ctrl_or_draw==true)//ctrl
      {
        // if(canvas_obj.regionSelect===undefined)
        // canvas_obj.UserRegionSelect((onSelect,draggingState)=>{
        //   if(draggingState==1)
        //   {

        //   }
        //   else if(draggingState==2)
        //   {
        //     console.log(onSelect);
        //     canvas_obj.UserRegionSelect(undefined)
        //   }
        // });
        
        // ctrlHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
        if(canvas_obj.regionSelect!==undefined)
        {
          if(canvas_obj.regionSelect.pt1===undefined || canvas_obj.regionSelect.pt2===undefined)
          {
            return;
          }
      
          let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
          let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);
           
          
          // console.log(canvas_obj.regionSelect);
          let x,y,w,h;
      
          x=pt1.x;
          w=pt2.x-pt1.x;
      
          y=pt1.y;
          h=pt2.y-pt1.y;
      
      
          if(w<0){
            x+=w;
            w=-w;
          }
          
          if(h<0){
            y+=h;
            h=-h;
          }
          _this.sel_region={
            x,y,w,h
          }
        }
      }
      else//draw
      {
        if(Local_IMCM!==undefined)
        {
          g.ctx.save();
          let scale=Local_IMCM.image_info.scale;
          g.ctx.scale(scale,scale);
          g.ctx.translate(-0.5, -0.5);
          g.ctx.drawImage(_this.imgCanvas, 0, 0);
          g.ctx.restore();
        }
        // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       

        let ctx = g.ctx;
        
        {
          rule.regionInfo.forEach((region:any,idx:number)=>{

            let region_ROI=
            {
              x:region.region[0],
              y:region.region[1],
              w:region.region[2],
              h:region.region[3]
            }
            
            ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
            drawRegion(g,canvas_obj,region_ROI,canvas_obj.rUtil.getIndicationLineSize());
                    
            ctx.font = "40px Arial";
            ctx.fillStyle = "rgba(0, 179, 0,0.5)";
            ctx.fillText("idx:"+idx,region_ROI.x,region_ROI.y)

            
            let region_components=GetObjElement(c_report,["regionInfo",idx,"components"]);
            // console.log(report,region_components);
            if(region_components!==undefined)
            {
              region_components.forEach((regComp:any)=>{

                canvas_obj.rUtil.drawCross(ctx, {x:regComp.x,y:regComp.y}, 5);

                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(regComp.area,regComp.x,regComp.y)
                
                ctx.font = "4px Arial";
                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                ctx.fillText(`${regComp.x},${regComp.y}`,regComp.x,regComp.y+5)
              })

            }



          })
        }


        if(canvas_obj.regionSelect!==undefined && _this.sel_region!==undefined)
        {
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          
          drawRegion(g,canvas_obj,_this.sel_region,canvas_obj.rUtil.getIndicationLineSize());
      
        }
      }

      
      if(renderHook)
      {
        renderHook(ctrl_or_draw,g,canvas_obj,rule);
      }
    }
    }/>

  </div>;

}

let defInfo_Default={
  name:"TEST_DEF",
  type:"m",

  act_cmds:[
    {
      id:"Zero",
      cmds:[
        "console.log('ssssss')"

      ]
    },
    {
      id:"Mylar_Take_1",
      cmds:[
        "reportWait_register('KLKxxx','Locating1')",
        "api.send_P('GS',0,{items:['data_path']})",
        "api.send_P('GS',0,{items:['binary_path','data_path']})",
        "v.fff=NaN;",

        `reportWait_promise('KLKxxx')
          .then(rep=>v.rep=report_process_Mylar_P1(rep))`,
        "v.ddd=0;",
      ]
    },
    // Mylar_Take_2
    
    // Mylar_Insp_1
    // Mylar_Insp_2
    // Mylar_Insp_Wait
    // Pillar_Take
    // Pillar_Insp_1
    // Pillar_Insp_2
    // Pillar_OK_1

    // Pillar_NG_1
    // Pillar_OK_2
    // Pillar_NG_2
  ],

  act_cmd_set_run:[
    {
      id:"Zero",

    }
  ],


  rules:[
    {
      id:"rule1",
      type:"ColorRegionLocating",
      sampleImageFolder:"data/TEST_DEF/rule1_Locating1",
      camera_id:tar_camera_id,//"BMP_carousel_0",
      trigger_tag:"Locating1",
      regionInfo:[
        {
          colorThres:30,
          region:[569,310,100,100],


        },



      ],
    }
  ]
}


function startsWith (str:string, needle:string) {
  var len = needle.length
  var i = -1
  while (++i < len) {
    if (str.charAt(i) !== needle.charAt(i)) {
      return false
    }
  }
  return true
}


async function listCMDPromise(api:BPG_WS,CNC_api:CNC_Perif,v:any,cmdList:string[],onRunCmdIdexChanged:(index:number,info:string)=>void,abortSig?:AbortSignal,onUserInputRequest?:(setting:any)=>Promise<any>)
{
  v.inCMD_Promise=true;
  // v.DisplayIdList=["ruleId1","ruleId2"]

  function SET_DISPLAY_ID_LIST(idList:string[])
  {
    v.DisplayIdList=idList;
  }

  let P=Promise;
  // function PALL(ps:Promise<any>[])
  // {
  //   return Promise.all()
  // }
  function G(code:string)
  {
    return CNC_api.send_P({"type":"GCODE","code":code})
  }
  let Message=message
  async function delay(ms=1000)
  {
    return new Promise((resolve,reject)=>setTimeout(resolve,ms))
  }
  async function LS_Save(key:string,msg:any)
  {
    localStorage.setItem("_listCMDPromise_"+key, JSON.stringify(msg))
  }
  async function LS_Load(key:string)
  {
    let read = localStorage.getItem("_listCMDPromise_"+key)
    if(read===null)return read;
    return JSON.parse(read);
  }

  function S(
    tl:string,
    prop:number,
    data?:{[key:string]:any},
    uintArr?:Uint8Array)
  {
    
    return api.send_P(tl,prop,data,uintArr)
  }
  async function READFILE(fileName:string)
  {
    let pkts = await api.send_P("LD",0,{filename:fileName},undefined) as any[];

    let RP=pkts.find((pkt)=>pkt.type=="FL")
    
    if(RP===undefined)throw "READFILE cannot find "+fileName


    return RP.data;
  }
  function reportWait_reg(key:string,trigger_tag:string,camera_id?:string,trigger_id?:number)
  {
    // console.log(key,trigger_tag,camera_id,trigger_id)
    if(v.reportListener[key])
    {
      if(v.reportListener[key].reject)
        v.reportListener[key].reject();
      
      delete v.reportListener[key];
      v.reportListener[key]=undefined;
    }
    v.reportListener[key]={
      trigger_tag,trigger_id,camera_id
    }
  }

  function reportWait(key:string)
  {
    return new Promise((resolve,reject)=>{

      if(v.reportListener[key])
      {
        if(v.reportListener[key].report)
        {
          resolve(v.reportListener[key].report);
        }
        else
        {
          v.reportListener[key].resolve=resolve;
          v.reportListener[key].reject=reject;
        }
      }
      else
      {
        reject("No registered repWait info");
      }
    }).then((rep:any)=>{
      delete v.reportListener[key];
      return rep;
    })
  }

  async function waitUserSelect(title:string,info="",arr:any[],default_value:any)
  {
    if(onUserInputRequest===undefined)return default_value;
    let value = await onUserInputRequest({
      title:title,
      type:"SELECT",
      data:arr,
      info,
      default_value:default_value});
    if(value)return value;
    return default_value;
  }
  async function waitUserSelects(title:string,info_arr:{text:string,opts:any[],default?:any}[])
  {



    // waitUserSelects("setup",[
    //   {text:"",opts:[1,2,3]},
    //   {text:">>",opts:["A","B","C"]},
    // ],[0,0])


    if(onUserInputRequest===undefined)return info_arr.map((info,idx)=>info.default);
    let value = await onUserInputRequest({
      title:title,
      type:"SELECTS",
      data:info_arr});
    if(value)return value;
    // return default_value;
    return info_arr.map((info,idx)=>info.default);
  }

  
  async function waitUserCheck(title:string,info="")
  {
    if(onUserInputRequest===undefined)return;
    await onUserInputRequest({
      title:title,
      type:"CHECK",
      info});
  }

  async function waitUserInput(obj:any,default_value:any)
  {
    if(onUserInputRequest===undefined)return default_value;
    let value = await onUserInputRequest({...obj,default_value:default_value});
    if(value)return value;
    return default_value;
  }

  let run_cmd_idx=0;
  function progressUpdate(info="")
  {
    onRunCmdIdexChanged(run_cmd_idx,info);
  }

  function NOT_ABORT()
  {
    if(abortSig&&abortSig.aborted)
    {
      throw("ACMD ABORT")
    }
  }

  async function INCLUDE(fileName:string)
  {
    return eval( await READFILE(fileName))
  }
  
  let $async=0;//just for a mark
  for(run_cmd_idx=0; run_cmd_idx<cmdList.length; run_cmd_idx++)
  {
    let cmd=cmdList[run_cmd_idx];
    v.inCMD_index=run_cmd_idx;
    progressUpdate();
    try{
      let ret;
      if(startsWith(cmd,'$async'))
        ret = await eval("(async () => {"+cmd+"})()");
      else
        ret = await eval(cmd);
    }
    catch(e){
      run_cmd_idx=-100;
      progressUpdate();
      throw({type:"ACMD exception",cmd,index:run_cmd_idx,e})
    }
    // console.log("$>",ret)
  }

  run_cmd_idx=-2;
  progressUpdate();
  
  v.inCMD_Promise=false;
  return true;
}

function TargetVIEWUI({defInfo,defReport,IMCM_group,onDefChange}:{
  defInfo:typeof defInfo_Default,
  defReport:any,
  IMCM_group:{[trigID:string]:IMCM_type},
  onDefChange:(...param:any)=>void
}){
  const _this = useRef<any>({
    listCMD_Vairable:{
      inCMD_Promise:false,
      cur_defInfo:{},
      reportListener:{
        _key_:{//example
          time:0,
          trigger_tag:"sss",
          trigger_id:100,
          camera_id:"Cam1",
          
          report:undefined,
          resolve:(...v:any)=>null,
          reject:(...e:any)=>null,
          // reject:undefined,
        }
      },
  
    }

  }).current;

  _this.listCMD_Vairable.cur_defInfo=defInfo;
  enum editState {
    Normal_Show = 0,
    Calib_Param_Edit = 1,
  }

  const [stateInfo,setStateInfo]=useState<{st:editState,info:any}>({
    st:editState.Normal_Show,
    info:undefined
  });

  const [delConfirmCounter,setDelConfirmCounter]=useState(0);

  
  
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [CNC_API,setCNC_API]=useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);


  const [crunIdx,setCRunIdx]=useState(-1);
  const [crunInfo,setCRunInfo]=useState("");
  const [crunAbortCtrl,setCRunAbortCtrl]=useState<AbortController|undefined>(undefined);
  
  const [mouseOnIdx,setMouseOnIdx]=useState(-1);
  
  let DAT_ANY:any=undefined;
  const [modalInfo,setModalInfo]=useState({
    timeTag:0,
    visible:false,
    type:"",
    onOK:(_:any)=>_,
    onCancel:(_:any)=>_,
    title:"",
    DATA:DAT_ANY

  });


  useEffect(() => {
    let cbKey="TargetVIEWUI_CB";
    BPG_API.send_cbs_add(INSP1_REP_PGID_,cbKey,{
      resolve:(pkts)=>{
        
        let RP=pkts.find((info:any)=>info.type=="RP")
        if(RP===undefined)return;
        RP=RP.data;

        let filteredKey=Object.keys(_this.listCMD_Vairable.reportListener)
          .filter(key=>{
            let repListener=_this.listCMD_Vairable.reportListener[key];
            if(repListener.camera_id && repListener.camera_id!==RP.camera_id)return false;
            if(repListener.trigger_tag && repListener.trigger_tag!==RP.trigger_tag)return false;
            if(repListener.trigger_id && repListener.trigger_id!==RP.trigger_id)return false;
            if(repListener.report!==undefined)return false;
            return true;

          })
        
        filteredKey.forEach(key=>{
          if(_this.listCMD_Vairable.reportListener[key].resolve!==undefined)
          {
            _this.listCMD_Vairable.reportListener[key].resolve(RP);
          }
          else
          {
            _this.listCMD_Vairable.reportListener[key].report=RP;
          }
        })
        // console.log(RP.data);
      },
      reject:(pkts)=>{

      }
    });
    
    return (() => {
      BPG_API.send_cbs_remove(INSP1_REP_PGID_,cbKey)
      });
  }, []); 


  function listCMDPromiseRun(cmds:string[])
  {
    const abortController = new AbortController();

    setCRunAbortCtrl(abortController);
    listCMDPromise(BPG_API,CNC_API,_this.listCMD_Vairable,cmds,(index,info)=>{
      // console.log(info)
      if(_this.crunIdx!=index)
      {
        setCRunIdx(index);
        _this.crunIdx=index
        setCRunInfo(info)
      }

      
      _this.throttle_Info_UPDATE=
      ID_throttle(_this.throttle_Info_UPDATE,()=>{
        setCRunInfo(info)
      },()=>_this.throttle_Info_UPDATE=undefined,100);
      // else
      // {
      //   setCRunInfo(info)
      // }
    },abortController.signal,
    async (setting)=>{
      let _setting={...setting}
      _this.listCMD_Vairable.USER_INPUT=undefined;
      if(setting.type==="SELECTS")
      {//preset
        _this.listCMD_Vairable.USER_INPUT=setting.data.map((info:any)=>info.default);
      }
      await new Promise((resolve,reject)=>{
        setModalInfo({
          timeTag:Date.now(),
          visible:true,
          type:setting.type,
          onOK:(_:any)=>{
            resolve(true)
            setModalInfo({...modalInfo,visible:false})
          },
          onCancel:(_:any)=>{
            reject(false)
            setModalInfo({...modalInfo,visible:false})
          },
          title:setting.title,
          DATA:_setting
        })
      })
      
      return _this.listCMD_Vairable.USER_INPUT;
    })
    .then(_=>{
      abortController.abort();
      console.log("DONE")
      setCRunAbortCtrl(undefined);
    })
    .catch(e=>{
      console.log(e);
      setCRunAbortCtrl(undefined);
      delete e.cmd
      if(e.e!==undefined)
        e.e=e.e.toString();
      setModalInfo({
        timeTag:Date.now(),
        visible:true,
        type:"CHECK",
        onOK:(_:any)=>{
          setModalInfo({...modalInfo,visible:false})
        },
        onCancel:(_:any)=>{
          setModalInfo({...modalInfo,visible:false})
        },
        title:"!!!!錯誤 例外!!!!",
        DATA:{info:`${JSON.stringify(e,null,2)}`}
      })
    });

    
    // new Promise((resolve,reject)=>{

    //   const abortCB=(e:Event)=>
    //   {
    //     reject(e)
    //   }
    //   abortController.signal.addEventListener("abort",abortCB);
    // }).catch(e=>{
    //   console.log(e)
    // })
    // setTimeout(()=>{
    //   abortController.abort();
    // },3000)
    

  }

  let displayRuleId=_this.listCMD_Vairable.DisplayIdList as string[];//=defInfo.rules.map((rule,idx)=>rule.id+" ");

  if(_this.listCMD_Vairable.DisplayIdList===undefined)
  {
    displayRuleId=defInfo.rules.map((rule,idx)=>rule.id);
  }

  let displayRuleIdx:number[];
  displayRuleIdx=displayRuleId
    .map(ruleId=>defInfo.rules.findIndex(rule=>rule.id==ruleId))
    .filter(idx=>idx>=0)
  
  let displayRuleIdx_hide:number[]
    =defInfo.rules.map((rule,idx)=>idx).filter(idx=>{
      if(displayRuleIdx.find(idx_to_show=>idx_to_show==idx)===undefined)
        return true;
      return false;
    });


  let defRulesCount=displayRuleIdx.length;
  let WHArr:{w:number,h:number}[]=[];
  if(defRulesCount==1)
  {
    WHArr=[{w:100,h:100}];
  }
  else if(defRulesCount==2)
  {
    WHArr=[{w:50,h:100},{w:50,h:100}];
  }
  else if(defRulesCount==3)
  {
    // WHArr=[{w:50,h:50},{w:50,h:50},{w:100,h:50}];
    WHArr=[{w:33.333,h:100},{w:33.333,h:100},{w:33.333,h:100}];
  }
  else if(defRulesCount==4)
  {
    WHArr=[{w:50,h:50},{w:50,h:50},{w:50,h:50},{w:50,h:50}];
  }

  let siderUI=null;
  switch(stateInfo.st)
  {
    case editState.Normal_Show:

      break;
    case editState.Calib_Param_Edit:
      
      let subMenu=null;

      
      switch(GetObjElement(stateInfo,["info","rootSel"]))
      {
          case "ACT_CMD_Edit":
            //
          // console.log(defInfo.act_cmds[stateInfo.info.act_cmd_idx])
          let act_cmdInfo=defInfo.act_cmds[stateInfo.info.act_cmd_idx];
          let cmds=act_cmdInfo.cmds;

          if(stateInfo.info.showDetail!=true)
          {
            break;
          }


          subMenu= <div style={{
            overflow: "scroll",
            boxShadow: "-5px 5px 15px rgb(0 0 0 / 50%)",
            float:"left",
            width:"50%",
            height: "100%",
            
            
            padding: "5px",
            background: "white",
            color: "black"}}>
            
            
            <Divider style={{margin: "5px"}}> NAME </Divider>

            <Input maxLength={100} value={act_cmdInfo.id} 
                style={{margin:"1px"}}
                onChange={(e)=>{
                  let value=e.target.value;
                  let new_defInfo=ObjShellingAssign(defInfo,["act_cmds",stateInfo.info.act_cmd_idx,"id"],value);
                  console.log(defInfo,new_defInfo);
                  onDefChange(new_defInfo,undefined);

                }}/>

            <Button disabled={crunAbortCtrl!==undefined}   onClick={()=>{
              listCMDPromiseRun(cmds);
            }}>Run</Button>

            <Button disabled={crunAbortCtrl===undefined || (crunAbortCtrl&&crunAbortCtrl.signal.aborted)}   onClick={()=>{
              if(crunAbortCtrl===undefined)return;
              crunAbortCtrl.abort();
              setDelConfirmCounter(delConfirmCounter+1);//HACK this is just to force the update,delConfirmCounter would not be used at this stage
            }}>STOP</Button> 
            
            <Popconfirm
                title="Are you sure to delete this task?"
                onConfirm={()=>{
                  
                }}
                okButtonProps={{danger:true,onClick:()=>{
                  if(delConfirmCounter==0)
                  {
                    defInfo.act_cmds[stateInfo.info.act_cmd_idx]

                    let new_defInfo={...defInfo};
                    new_defInfo.act_cmds.splice(stateInfo.info.act_cmd_idx, 1);
                    onDefChange(new_defInfo,undefined);
                    setStateInfo({...stateInfo,
                      info:{rootSel:"",}
                    })
                  }
                  else
                  {
                    setDelConfirmCounter(delConfirmCounter-1);
                  }
                }}}
                onCancel={()=>{}}
                okText={"Yes:"+delConfirmCounter}
                cancelText="No"
              >
              <Button danger type="primary" onClick={()=>{
                setDelConfirmCounter(5);
              }}>DEL</Button>
            </Popconfirm> 
           
            
            <Divider style={{margin: "5px"}}> ACMD </Divider>
            {
              cmds.map((cmd:string,idx:number)=><>


                <Dropdown
                  overlay={<>
                    <Button size="small" onClick={()=>{
                      let new_defInfo={...defInfo};
                      new_defInfo.act_cmds[stateInfo.info.act_cmd_idx].cmds.splice(idx, 0, "");
                      onDefChange(new_defInfo,undefined);
                    }}>+</Button>
                    <Button size="small" onClick={()=>{
                  
                      let new_defInfo={...defInfo};
                      new_defInfo.act_cmds[stateInfo.info.act_cmd_idx].cmds.splice(idx, 1);
                      onDefChange(new_defInfo,undefined);
                    }}>-</Button>
                  </>}
                >
                  <a style={{color:"#000"}} className="ant-dropdown-link"  onClick={e => e.preventDefault()}>
                    <DownOutlined />
                  </a>
                </Dropdown>

                {crunIdx==idx?(crunInfo.length?crunInfo:"<--------"):null}


                <Input.TextArea value={cmd} 
                // rows={1}
                autoSize
                tabIndex={-1}
                onKeyDown={(e)=>{
                  if (e.key == 'Tab') {
                    // e.preventDefault();

                  }
                }}
                style={{margin:"1px"}}
                onChange={(e)=>{
                  let value=e.target.value;
                  console.log(value)
                  
                  let new_defInfo=ObjShellingAssign(defInfo,["act_cmds",stateInfo.info.act_cmd_idx,"cmds",idx],value);
                  console.log(defInfo,new_defInfo);
                  onDefChange(new_defInfo,undefined);

                }}/>
              </>)
            }

            <Button size="small" onClick={()=>{
              
              let new_defInfo={...defInfo};
              new_defInfo.act_cmds[stateInfo.info.act_cmd_idx].cmds.push("");
              onDefChange(new_defInfo,undefined);

            }}>+</Button>


          </div>
          break;
      }


      // console.log(defInfo);
      let ButtonStyle={
        
        minWidth:"15px",
        color:"#FFF",
        background:"#444",
      }


    

      siderUI=
      <Sider width={subMenu==null?200:400}>

        <div style={{float:"left",height:"100%",width:subMenu==null?"100%":"50%"}}>

          
          <Menu mode="inline" theme="dark" selectable={false}>

            
            <SubMenu key="INSP" title="INSP" 
            onTitleClick={()=>{
              setStateInfo({...stateInfo,
                info:{rootSel:"",}
              })
            }}>
            {
              
              displayRuleIdx.map((ruleIdx,idx)=><Menu.Item onClick={()=>{
                // let id=defInfo.rules[ruleIdx].id;
                displayRuleId.splice(idx, 1);
                _this.listCMD_Vairable.DisplayIdList=displayRuleId;
                  
                console.log(displayRuleIdx,_this.listCMD_Vairable.DisplayIdList)
                setDelConfirmCounter(delConfirmCounter+1);
              }}
              key={defInfo.rules[ruleIdx].id}>{defInfo.rules[ruleIdx].id+"<<"}</Menu.Item>)
            }

              <Menu.Item key={"___"}>-----------</Menu.Item>
            {
              displayRuleIdx_hide.map((ruleIdx,idx)=><Menu.Item
              onClick={()=>{
                _this.listCMD_Vairable.DisplayIdList.push(defInfo.rules[ruleIdx].id)
                setDelConfirmCounter(delConfirmCounter+1);
              }} key={defInfo.rules[ruleIdx].id}>{defInfo.rules[ruleIdx].id+"<<"}</Menu.Item>)
            }

              <Menu.Item key={defInfo.rules.length} onClick={()=>{
                console.log(defInfo.rules);
                let new_defInfo={...defInfo};

                let newId="Untitled";
                for(let idx=-1;;idx++)
                {
                  let paddedIdx=newId;
                  if(idx!==-1)
                  {
                    paddedIdx+=`(${idx})`;
                  }
                  if(defInfo.rules.find(rule=>rule.id==paddedIdx)!==undefined)
                  {
                    continue;
                  }
                  newId=paddedIdx;
                  break;
                }

                new_defInfo.rules=[...new_defInfo.rules,{
                  
                  id: newId,
                  type: "ColorRegionLocating",
                  sampleImageFolder: "data/TEST_DEF/rule1_Locating1",
                  camera_id: "BMP_carousel_0",
                  trigger_tag: "TTag_"+defInfo.rules.length,
                  regionInfo:[
                    {
                      colorThres:30,
                      region:[0,0,10,10],
                    },
                  ],
                }]
                onDefChange(new_defInfo,undefined);
              }}>+</Menu.Item>
            </SubMenu>



            <SubMenu key="ACTCMD" title="ACTCMD" 
            
            onTitleClick={()=>{
              setStateInfo({...stateInfo,
                info:{rootSel:"",}
              })
            }}>
            {
              defInfo.act_cmds.map((cmdInfo,index)=>{
                let isThisIdx="ACT_CMD_Edit"==GetObjElement(stateInfo,["info","rootSel"])
                              &&index===GetObjElement(stateInfo,["info","act_cmd_idx"]);
                let isMouseOnThis=mouseOnIdx==index;
                // PlayCircleOutlined
                return <Menu.Item
                onClick={_=>_}
                className="antd_menu_item_8px_padding"
                onMouseEnter={()=>{
                  setMouseOnIdx(index);
                }}
                onMouseLeave={()=>{
                  setMouseOnIdx(-1);
                }}
                key={index}>
                  
                  {/* <Tag icon={<DownOutlined />}/> */}
                  {(isMouseOnThis==false && isThisIdx==false)?null:
                  <Button.Group>

                  
                  <Button style={ButtonStyle} shape="circle" icon={<PlayCircleOutlined />} onClick={()=>{
                    
                    listCMDPromiseRun(cmdInfo.cmds);
                  }} size={"small"}></Button>
                  <Button shape="circle"  style={{...ButtonStyle,width:"15px"}}
                  
                  icon={<MoreOutlined />} size={"small"} onClick={(e)=>{
                    if(isThisIdx)
                    {
                      setStateInfo({...stateInfo,
                        info:{rootSel:"",}
                      })
                    }
                    else
                    {
                      setStateInfo({...stateInfo,
                        info:{
                          rootSel:"ACT_CMD_Edit",
                          act_cmd_idx:index,
                          showDetail:true,
                        }
                      })
                    }
                  }}/>
                  </Button.Group>}
                  {"    "}
                  {cmdInfo.id}
                  {isThisIdx?"  >":null}
                  {/* <Tag icon={<DownOutlined />} color="#cd201f">
                    Youtube
                  </Tag> */}
                  {/* <Button shape="circle" size="small" icon={<DownOutlined />} /> */}
                </Menu.Item>})
            }




            <Menu.Item key={defInfo.act_cmds.length} onClick={()=>{
              let new_defInfo={...defInfo};

              new_defInfo.act_cmds=[...new_defInfo.act_cmds,{
                id:"NEW_ACT_CMD",
                cmds:[]
              }]
              setStateInfo({...stateInfo,
                info:{
                  rootSel:"ACT_CMD_Edit",
                  act_cmd_idx:defInfo.act_cmds.length,
                }
              })
              onDefChange(new_defInfo,undefined);
              
            }}>+</Menu.Item>
            </SubMenu>
          </Menu>


        </div>
       {subMenu}


      </Sider>


      break;
  }
  // console.log(_this.listCMD_Vairable);
  
  function TargetViewUIShow(index:number,displaySetting={w:100,h:100,hide:false})
  {
    let rule=defInfo.rules[index]
    let rep_rules=GetObjElement(defReport,["rules"]);
    let subRuleRep=undefined;
    if(rep_rules!==undefined)
      subRuleRep=rep_rules.find((rep_rule:any)=>rep_rule.id==rule.id);
    // console.log(rule,subRuleRep);
    // subRuleRep
    // let whsetting={w:50,h:50};
    // if(show)
    //   whsetting=WHArr[index];
    return <SingleTargetVIEWUI_ColorRegionLocating 
    readonly={false} 
    width={displaySetting.w+"%"} 
    height={displaySetting.h+"%"} 
    style={{float:"left",display:displaySetting.hide?"none":undefined}} 
    key={rule.id} 
    IMCM_group={IMCM_group} 
    rule={rule} 
    report={subRuleRep} 
    renderHook={_this.listCMD_Vairable.renderHook} 
    onRuleChange={(new_rule,doInspUpdate=true)=>{
    
      if(new_rule!==undefined)//update
      {
        let new_defInfo={...defInfo};
        new_defInfo.rules[index]=new_rule;
        console.log(new_defInfo);
        onDefChange(new_defInfo,doInspUpdate?new_rule:undefined);
      }
      else//deltetion
      {
        
        let new_defInfo={...defInfo,rules:[...defInfo.rules]};
        
        new_defInfo.rules.splice(index, 1);

        HACK_do_Camera_Check=true;
        onDefChange(new_defInfo,undefined);


        
      }
    }}/>
    
  }
  // console.log(WHArr,displayRuleIdx,displayRuleIdx_hide);
  return <>

    <Layout style={{ height: '100%' }}>
    <Header style={{ width: '100%' }}>

    <Menu theme="dark" mode="horizontal" selectable={false}>
        <Menu.Item key="1" onClick={()=>{
          
          setStateInfo({
            st:stateInfo.st==editState.Normal_Show?editState.Calib_Param_Edit:editState.Normal_Show,
            info:undefined
          })
        }}>Calib_Param_Edit</Menu.Item>
    </Menu>




    </Header>

    <Layout>
    {siderUI}
    
    <Content className="site-layout" style={{ padding: '0 0px'}}>
    <Modal
        title={modalInfo.title}
        visible={modalInfo.visible}
        onOk={modalInfo.onOK}
        // confirmLoading={confirmLoading}
        onCancel={modalInfo.onCancel}
      >
        {/* <ReactJsoneditor ref={ReactJsoneditorRef} values={modalInfo.DATA} /> */}
        {modalInfo.type=="SELECT"?<>
          {modalInfo.DATA.info}<br/>
          {modalInfo.DATA.data.map((sdat:any)=><Button onClick={()=>{ _this.listCMD_Vairable.USER_INPUT=sdat;modalInfo.onOK(0)  }}>{sdat}</Button>)}
          <br/>
          {_this.listCMD_Vairable.USER_INPUTdefault_value===undefined?
            null:<>預設:{_this.listCMD_Vairable.USER_INPUTdefault_value}</>}
          
        </>:modalInfo.type=="SELECTS"?<>
          {modalInfo.DATA.info}
          {modalInfo.DATA.data.map((opts_info:{text:string,opts:any[],default?:any},index:number)=><>
            <Divider style={{margin: "5px"}}> {opts_info.text} </Divider>
            <br/>
            <Radio.Group key={modalInfo.timeTag+":"+index} defaultValue={opts_info.default} onChange={()=>{}}>
            {opts_info.opts.map((opt)=><Radio.Button value={opt} onClick={()=>{
              _this.listCMD_Vairable.USER_INPUT[index]=opt;
              
            }}>{opt}</Radio.Button>)}
            </Radio.Group>
            {/* <br/>
            {opts_info.default===undefined?
              null:<>預設:{opts_info.default}</>} */}
          </>
          )}
         



        </>:modalInfo.type=="CHECK"?<pre>
          {modalInfo.DATA.info}
        </pre>:null
          
        }
    </Modal>
    {[
      displayRuleIdx.map((rule_index,idxidx)=>TargetViewUIShow(rule_index,{...WHArr[idxidx],hide:false})),
      displayRuleIdx_hide.map((index)=>TargetViewUIShow(index,{w:50,h:50,hide:true}))]
      .flat()}
    </Content>
  
    </Layout>
  
    </Layout>
  </>
}


const _DEF_FILE_PATH_="data/xxx.json";
function VIEWUI(){
  const _this = useRef<any>({}).current;
  
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));



  const ACT_FILE_Save= async(filename:string,content:Uint8Array) => {
    
    let api=await getAPI(CORE_ID) as BPG_WS;

    return await api.send_P("SV",0,{filename},content)
  }

  const ACT_FILE_Load= async (filename:string) => {
    
    let api=await getAPI(CORE_ID) as BPG_WS;
    let data = await api.send_P("LD",0,{filename}) as any;

    let FL=data.find((info:any)=>info.type=="FL")
    if(FL===undefined)
    {
      throw "Read Failed  FL is undefined";
    }
    
    
    return FL.data;
  }
  const [defInfoCritUpdate,_setDefInfoCritUpdate]=useState(0)

  const [defInfo,setDefInfo]=useState(defInfo_Default)

  
  let setDefInfoCritUpdate=(_defInfo:typeof defInfo)=>{
    setDefInfo(_defInfo);
    _setDefInfoCritUpdate(defInfoCritUpdate+1);
  }
  const [defReport,setDefReport]=useState<any>(undefined);
  

  async function getAPI(API_ID:string)
  {
    let api=await new Promise((resolve,reject)=>{
      ACT_EXT_API_ACCESS(API_ID,(api)=>{
        if(api===undefined)reject();
        resolve(api) 
      })
    });

    return api;
  }

  async function queryConnCamList()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let cameraListInfos=await api.send_P("CM",0,{type:"connected_camera_list"}) as any[];
    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"

    let camList = CM.data as {name:string,id:string,driver_name:string}[];


    return camList;
  }

  async function queryDiscoverList()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let cameraListInfos=await api.cameraDiscovery() as any[];
    let CM=cameraListInfos.find(info=>info.type=="CM")
    if(CM===undefined)throw "CM not found"
    console.log(CM.data);
    return CM.data as {name:string,id:string,driver_name:string}[];
  }


  async function isConnectedCameraFullfillList(camera_id_List:string[])
  {
    let connCamList=await queryConnCamList();
    for(let i=0;i<camera_id_List.length;i++)
    {
      let cam_id=camera_id_List[i];
      let cam = connCamList.find(caminfo=>caminfo.id==cam_id);
      if(cam===undefined) return false;
    }
    return true;
  }


  async function DisconnectAllCamera()
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let connCamList=await queryConnCamList();
    
    let disconnInfo = await Promise.all(
      connCamList.map(cam=>api.send_P("CM",0,{type:"disconnect",id:cam.id}))
    )

    return disconnInfo;
  }


  async function CameraCheckAndConnect(camera_id_List:string[],froceDisconnectAll=false)
  {
    if(froceDisconnectAll)
      await DisconnectAllCamera();
    let api=await getAPI(CORE_ID) as BPG_WS;
    let isFullfill= await isConnectedCameraFullfillList(camera_id_List)

    if(isFullfill==false)
    {
      await DisconnectAllCamera();
      let discoveredCam = await queryDiscoverList();
      let cameraList:{
        name: string;
        id: string;
        driver_name: string;
      }[]=[];
      

      console.log(camera_id_List,discoveredCam);
      for(let i=0;i<camera_id_List.length;i++)
      {
        let camid=camera_id_List[i];
        let caminfoFound = discoveredCam.find(cam=>cam.id==camid);
        if(caminfoFound===undefined)throw "2";
        cameraList.push(caminfoFound);
      }


      // let cameraList=camera_id_List.map((camid)=>discoveredCam.find(cam=>cam.id==camid))
      console.log(cameraList);



      let connctResult=await Promise.all(cameraList.map(cam=>
        api.send_P("CM",0,{
          type:"connect",
          misc:"data/BMP_carousel_test1",
          id:cam.id}).then((camInfoPkts:any)=>camInfoPkts[0].data)
        
        ));//
      console.log(connctResult);


      
      await Promise.all(cameraList.map(cam=>
        api.send_P("CM",0,{
          type:"setting",id:cam.id,
          trigger_mode:2,
          analog_gain:17.0166,

          exposure:400,//us


          // WB_ONCE:true,
          RGain: 1655,
          GGain: 1023,
          BGain: 1639,
          gamma:0.5,
          frame_rate:9999999,


        }).then((camInfoPkts:any)=>camInfoPkts[0].data)
        
        ));//
    }

    return await queryConnCamList();
  }

  async function InspTargetReload(defInfo:any,_PGID_:number)
  {
    let api=await getAPI(CORE_ID) as BPG_WS;
    let ret = await api.send_P("IT",0,{type:"delete_all"})
    await api.send_P("IT",0,{type:"create",id:"XXX",defInfo,_PGID_:_PGID_,_PGINFO_:{keep:true}})
      

    return await api.send_P("IT",0,{type:"list"});
  }

  function CameraSetChannelID(camera_id_List:string[],channel_id:number,
    cbs:{ reject(...arg: any[]): any; resolve(...arg: any[]): any; })
  {
    ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
      let api=_api as BPG_WS
      let connctResult=camera_id_List.map((cam_id,idx)=>
        api.send("CM",0,{
          type:"set_camera_channel_id",
          id:cam_id,
          _PGID_:channel_id,_PGINFO_:{keep:true}
          
          
        },undefined,cbs)
      )
      
    })
  }



  function updateDefInfo_(_defInfo:any,camTrigInfo:{id:string,trigger_tag:string,trigger_id:number,img_path:string|undefined}|undefined=undefined)
  {

    return InspTargetReload(_defInfo,INSP1_REP_PGID_).then(e=>{
      console.log(e)
      ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
        let api=_api as BPG_WS
        api.send(undefined,0,{_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
          resolve:(e)=>{
            let RP = e.find((pkt:any)=>pkt.type=="RP");
            if(RP===undefined)
            {
              setDefReport(undefined)
              return;
            }
  
            console.log(RP.data)
            setDefReport(RP.data)
            
            // console.log(e)
          },
          reject:(e)=>{
            console.log(e)
          },
        })

        if(camTrigInfo!==undefined)
          api.send_P(
            "CM",0,{
              type:"trigger",
              soft_trigger:true,
              id:camTrigInfo.id,
              trigger_tag:camTrigInfo.trigger_tag,
              // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
              trigger_id:camTrigInfo.trigger_id,
              img_path:camTrigInfo.img_path,
              channel_id:50201
            })

      })
    })
  }

  

  async function updateDefInfo(_defInfo:any,camTrigInfo:{id:string,trigger_tag:string,trigger_id:number,img_path:string|undefined}|undefined=undefined)
  {

    console.log("+===========",_defInfo);
    let reloadRes = await InspTargetReload(_defInfo,INSP1_REP_PGID_);
    console.log("+===========",reloadRes);
    let api = await getAPI(CORE_ID)as BPG_WS

    api.send(undefined,0,{_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
      resolve:(e)=>{
        let RP = e.find((pkt:any)=>pkt.type=="RP");
        if(RP===undefined)
        {
          setDefReport(undefined)
          return;
        }

        console.log(RP.data)
        setDefReport(RP.data)
        
        // console.log(e)
      },
      reject:(e)=>{
        console.log(e)
      },
    })



    if(camTrigInfo!==undefined)
      api.send_P(
        "CM",0,{
          type:"trigger",
          soft_trigger:true,
          id:camTrigInfo.id,
          trigger_tag:camTrigInfo.trigger_tag,
          // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
          trigger_id:camTrigInfo.trigger_id,
          img_path:camTrigInfo.img_path,
          channel_id:50201
        })

    return reloadRes;

  }


  const [IMCM_group,setIMCM_group]=useState<{[trigID:string]:IMCM_type}>({});
  useEffect(()=>{

    if(defInfoCritUpdate==0)return;
    console.log("++++++++++++++",defInfo);
    let cameraList=Object.keys(defInfo.rules.reduce((obj,rule)=>{ 
      obj[rule.camera_id]=1;
      return obj;
       },{}))

    // console.log(cameraList);
    let froceDisconnectAll=true;
    CameraCheckAndConnect(cameraList,froceDisconnectAll).then(e=>{
      // console.log(e)
        
      CameraSetChannelID(cameraList,50201,
      { reject:(e)=>console.log(e),
        resolve:(e)=>{
          let IM=e.find((p:any)=>p.type=="IM");
          if(IM===undefined)return;
          let CM=e.find((p:any)=>p.type=="CM");
          if(CM===undefined)return;
          // console.log("++++++++\n",IM,CM);
          let IMCM={
            image_info:IM.image_info,
            camera_id:CM.data.camera_id,
            trigger_id:CM.data.trigger_id,
            trigger_tag:CM.data.trigger_tag,
          }
          setIMCM_group({
            ...IMCM_group,
            [IMCM.trigger_tag+IMCM.camera_id]:IMCM
          })


        }, 
      })

      
      updateDefInfo(defInfo).then(inspTarList=>{
        console.log(">>>>>>>>>>>>>>>>>",defInfo,inspTarList);
        ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
          let api=_api as BPG_WS
          
          defInfo.rules.forEach(rule=>{
            
            api.send_P(
              "CM",0,{
                type:"trigger",
                soft_trigger:true,
                id:rule.camera_id,
                trigger_tag:rule.trigger_tag,
                // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
                trigger_id:100,
                // img_path:camTrigInfo.img_path,
                channel_id:50201
              })
          })
        })

      });




    })
    .catch(e=>{
      console.log("XXXXXX");
    })


  },[defInfoCritUpdate])

  useEffect(()=>{//load default

    ACT_FILE_Load(_DEF_FILE_PATH_)
    .then((e)=>{
      console.log(e);
      setDefInfoCritUpdate(e);
    })
  },[])
  // console.log(defInfo);
  return <>

        {/* <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send("IT",0,{type:"create",id:"INSP1",_PGID_:INSP1_REP_PGID_,_PGINFO_:{keep:true}},undefined,{
                reject:(e)=>{},
                resolve:(e)=>{


                  console.log(e)
                },
              })

          })




        }}>InspTarget init</Button>


        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            
            Promise.all([
              api.send_P("IT",0,{type:"list"}),
            ])
            
            .then((d)=>{
              console.log(d);
            })
          })
        }}>InspTarget list</Button>



        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send_P(
              "CM",0,{
                type:"start_stream",
                id:tar_camera_id
              })
            // api.send_P(
            //   "CM",0,{
            //     type:"start_stream",
            //     id:"BMP_carousel_0",
            //   })

            //   api.send_P(
            //     "CM",0,{
            //       type:"start_stream",
            //       id:"BMP_carousel_2"
            //     })
            //   api.send_P(
            //     "CM",0,{
            //       type:"start_stream",
            //       id:"BMP_carousel_3",
            //     })


          })




        }}>Start Stream</Button>

 */}

        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            
            Promise.all([
              api.send_P(
                "CM",0,{
                  type:"stop_stream",
                  id:tar_camera_id
                }),
            ])
            
            .then((d)=>{

              api.send_P(
                "CM",0,{
                  type:"clean_trigger_info_matching_buffer",
                })


            })

            




          })




        }}>Stop Stream</Button>
        <Button onClick={()=>{

          ACT_FILE_Load(_DEF_FILE_PATH_)
            .then((e)=>{
              console.log(e);
              setDefInfoCritUpdate(e);
            })
        }}>LOAD</Button>


        <Button onClick={()=>{
          // defInfo
          var enc = new TextEncoder();
          ACT_FILE_Save(_DEF_FILE_PATH_,enc.encode(JSON.stringify(defInfo, null, 2)));
        }}>SAVE</Button>


        <TargetVIEWUI 
          defInfo={defInfo}
          defReport={defReport}
          IMCM_group={IMCM_group}
          onDefChange={(new_def,tarRule)=>{
            

            if(HACK_do_Camera_Check)
            {
              setDefInfoCritUpdate(new_def);
              HACK_do_Camera_Check=false;
            }
            else
            {
              setDefInfo(new_def);
              
              if(tarRule!==undefined)
              {

                console.log(tarRule);
                updateDefInfo(new_def,{
                  id:tarRule.camera_id,
                  trigger_tag:tarRule.trigger_tag,
                  trigger_id:0,
                  img_path:undefined,//"data/TEST_DEF/rule1_Locating1/KKK2.png",
                });
              }
            }


            
            // id:tar_camera_id,//"BMP_carousel_0",
            // trigger_tag:"Locating1",
            // // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
            // trigger_id:0,
          }}
        />

  </> ;

}




function App() {
  
  const _ = useRef<any>({
  });
  let _this=_.current;
  const dispatch = useDispatch();
  const CORE_API_INFO = useSelector((state:StoreTypes) => state.EXT_API[CORE_ID]);


  const ACT_EXT_API_REGISTER= (...p:Parameters<typeof EXT_API_REGISTER>) => dispatch(EXT_API_REGISTER(...p));
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const ACT_EXT_API_UPDATE= (...p:Parameters<typeof EXT_API_UPDATE>) => dispatch(EXT_API_UPDATE(...p));
  const ACT_EXT_API_CONNECTED= (...p:Parameters<typeof EXT_API_CONNECTED>) => dispatch(EXT_API_CONNECTED(...p));
  const ACT_EXT_API_DISCONNECTED= (...p:Parameters<typeof EXT_API_DISCONNECTED>) => dispatch(EXT_API_DISCONNECTED(...p));

  // const [camList,setCamList]=useState<{[key:string]:{[key:string]:any,list:any[]}}>({});
  useEffect(() => {
    
    let core_api=new BPG_WS(CORE_ID);
    core_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CORE_ID);


    ACT_EXT_API_REGISTER(core_api.id,core_api);
    
    core_api.connect({
      url:"ws://localhost:4090"
    });




    let CNC_api=new CNC_Perif(CNC_PERIPHERAL_ID,20666);

    {
      CNC_api.onConnected=()=>ACT_EXT_API_CONNECTED(CNC_PERIPHERAL_ID);
  
      CNC_api.onInfoUpdate=(info:[key: string])=>ACT_EXT_API_UPDATE(CNC_api.id,info);
  
      CNC_api.onDisconnected=()=>ACT_EXT_API_DISCONNECTED(CNC_PERIPHERAL_ID);
      
      CNC_api.BPG_Send=core_api.send.bind(core_api);
  
      ACT_EXT_API_REGISTER(CNC_api.id,CNC_api);
    }
    






    core_api.onConnected=()=>{
      ACT_EXT_API_CONNECTED(CORE_ID);

      CNC_api.connect({
        uart_name:"COM5",
        baudrate:115200
      });
    }

    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, []); 

  // let camUIInfo = Object.keys(camList).map((driverKey:string,index)=>{
   
  //   let camInfos=camList[driverKey];

  //   return camInfos.list.map((cam,idx)=>({
  //     driver_idx:index,
  //     id:cam.id,
  //     misc:(driverKey=="bmpcarousel")?"data/BMP_carousel_test":"",
  //     cam_idx:idx,
  //     channel_id:index*1000+idx+50000
  //   }));
  // }).flat();
  // console.log(camUIInfo);

 
  if(GetObjElement(CORE_API_INFO,["state"])!=1)
  {
    return <div>Wait....</div>;
  }
  return <VIEWUI/>;

}

export default App;
