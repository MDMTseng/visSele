import React from 'react';
import { useState, useEffect,useRef } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs } from 'antd';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement} from './UTIL/MISC_Util';

import {HookCanvasComponent,DrawHook_CanvasComponent,type_DrawHook_g,type_DrawHook} from './CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

import { Row, Col } from 'antd';


import './basic.css';

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

function InspectionTargets({inspDef,onDefSetupUpdate}:{inspDef:type_InspDef,onDefSetupUpdate:(inspdef:type_InspDef)=>boolean})
{
  const [_inspDef,set_inspDef]=useState<type_InspDef>();
  useEffect(()=>{
   
    set_inspDef(clone(inspDef))
  },[inspDef])

  if(_inspDef===undefined)return null;

  return <>
    {/* InspectionTargets */}

    <Button onClick={()=>{
      
    }}>Add Cam</Button>
    <Button onClick={()=>{
      let newList = [..._inspDef.rules];
      newList.push({id:"ID:"+newList.length,name:"N_"+newList.length});
      
      set_inspDef({..._inspDef,rules:newList})
    }}>Add Insp</Button>
    <Tabs defaultActiveKey="1">
      {_inspDef.rules.map(rule=>(
        <TabPane tab={rule.name} key={rule.id}  >
          <div style={{width:"100%",height:"100%"}}>

          {rule.name}

          </div>
        </TabPane>
      ))}
    </Tabs>
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


function SingleTargetVIEWUI({IMCM_group,rule,onRuleChange}:{
  IMCM_group:{[trigID:string]:IMCM_type},
  rule:any,
  onRuleChange:(...param:any)=>void}){
  const _ = useRef<any>({

    imgCanvas:document.createElement('canvas'),
    canvasComp:undefined
  });
  let _this=_.current;
  console.log(IMCM_group);
  const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
  const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
  const [isInRegionSelect,set_isInRegionSelect]=useState(false);
  const [Local_IMCM,setLocal_IMCM]=
    useState<IMCM_type|undefined>(undefined);

  useEffect(() => {
    setDrawHooks([(CoD,g,canvObj)=>{
      // console.log(_this.imgCanvas);
      g.ctx.drawImage(_this.imgCanvas, 0, 0);
    }])
    
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, []); 

  useEffect(() => {
    let newIMCM=IMCM_group[rule.trigger_tag];
    console.log(newIMCM,rule.trigger_tag);
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


  
  return <div style={{width:"500px",height:"500px"}}  className={"overlayCon"}>

    <div className={"overlay"} >

      <Button  onClick={()=>{
        if(_this.canvasComp===undefined)return;
        set_isInRegionSelect(true);
        let canvas_obj:DrawHook_CanvasComponent=_this.canvasComp;
        canvas_obj.UserRegionSelect((info,state)=>{
          if(state==2)
          {
            console.log(info);
            canvas_obj.UserRegionSelect(undefined)
            set_isInRegionSelect(false);
          }
          else
          {

          }
        })
      }}>RegionSelect</Button>


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
        
        ctrlHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
        if(isInRegionSelect && canvas_obj.regionSelect!==undefined)
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
        drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
       
        if(isInRegionSelect)
        {
          if(_this.sel_region===undefined)return;
          let ctx = g.ctx;
      
          ctx.strokeStyle = 'red';
          ctx.lineWidth = 5;
      
          let x = _this.sel_region.x;
          let y = _this.sel_region.y;
          let w = _this.sel_region.w;
          let h = _this.sel_region.h;
          ctx.beginPath();
          let LineSize = canvas_obj.rUtil.getIndicationLineSize();
          ctx.setLineDash([LineSize*10,LineSize*3,LineSize*3,LineSize*3]);
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          ctx.lineWidth = LineSize;
          ctx.rect(x,y,w,h);
          ctx.stroke();
          ctx.closePath();
      
          ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
          let psize = 2*canvas_obj.rUtil.getPointSize();
          ctx.lineWidth = psize/3;
          canvas_obj.rUtil.drawCross(ctx, {x:x+w/2,y:y+h/2},psize);
      
        }
      }
    }
    }/>

  </div>;

}


function VIEWUI(){
  const _this = useRef<any>({}).current;
  
  const dispatch = useDispatch();
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const [defInfo,setDefInfo]=useState({
    type:"m",
    rules:[
      {
        name:"rule1",
        type:"binarize",
        camera_id:"BMP_carousel_0",
        trigger_tag:"KLKS0",
        region:[50,50,100,100],
        thres:40,
      },
      {
        name:"rule2",
        type:"binarize",
        camera_id:"BMP_carousel_0",
        trigger_tag:"KLKS1",
        region:[50,50,100,100],
        thres:40,
      }
    ]
  })
  

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
    console.log(isFullfill);
    if(isFullfill==false)
    {
      await DisconnectAllCamera();
      let discoveredCam = await queryDiscoverList();
      let cameraList:{
        name: string;
        id: string;
        driver_name: string;
      }[]=[];
      
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
          ...(idx==0?
            {_PGID_:channel_id,_PGINFO_:{keep:true}}:
            {}
          )
        },undefined,cbs)
      )
      
    })
  }




  const [IMCM_group,setIMCM_group]=useState<{[trigID:string]:IMCM_type}>({});
  useEffect(()=>{

    let cameraList=Object.keys(defInfo.rules.reduce((obj,rule)=>{ 
      obj[rule.camera_id]=1;
      return obj;
       },{}))

    console.log(cameraList);
    CameraCheckAndConnect(cameraList,true).then(e=>{
      console.log(e)
        
      CameraSetChannelID(cameraList,50201,
      { reject:(e)=>console.log(e),
        resolve:(e)=>{
          let IM=e.find((p:any)=>p.type=="IM");
          if(IM===undefined)return;
          let CM=e.find((p:any)=>p.type=="CM");
          if(CM===undefined)return;
          let IMCM={
            image_info:IM.image_info,
            camera_id:CM.data.camera_id,
            trigger_id:CM.data.trigger_id,
            trigger_tag:CM.data.trigger_tag,
          }
          setIMCM_group({
            ...IMCM_group,
            [IMCM.trigger_tag]:IMCM
          })
        }, 
      })



    })
    .catch(e=>{

    })

    InspTargetReload(defInfo,51000).then(e=>{
      console.log(e)
      ACT_EXT_API_ACCESS(CORE_ID,(_api )=>{
        let api=_api as BPG_WS
        api.send(undefined,0,{_PGID_:51000,_PGINFO_:{keep:true}},undefined,{
          resolve:(e)=>{
            console.log(e)
          },
          reject:(e)=>{
            console.log(e)
          },
        })
      })
    })



  },[])

  return <>

    {/* <CameraDiscover onCameraSelected={(camInfo)=>{
      console.log(camInfo);

      ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
        let api=_api as BPG_WS;//cast
        
          api.send("CM",0,{
            type:"connect",
            id:camInfo.id,
            _PGID_:50000,
            _PGINFO_:{keep:true},
            misc:"data/BMP_carousel_test1"},undefined,{
              reject:(e)=>{},
              resolve:(e)=>{
                let IM=e.find((p:any)=>p.type=="IM");
                if(IM===undefined)return;
                let CM=e.find((p:any)=>p.type=="CM");
                if(CM===undefined)return;
                let IMCM={
                  image_info:IM.image_info,
                  camera_id:CM.data.camera_id,
                  trigger_id:CM.data.trigger_id,
                  trigger_tag:CM.data.trigger_tag,
                }
                setIMCM_group({
                  ...IMCM_group,
                  [IMCM.trigger_tag]:IMCM
                })
              },
            })
          // api.send_P("CM",0,{type:"connected_camera_list"})


      })
    }}/> */}
    <br/>
    


        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send("IT",0,{type:"create",id:"INSP1",_PGID_:51000,_PGINFO_:{keep:true}},undefined,{
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
                id:"BMP_carousel_1"
              })
            api.send_P(
              "CM",0,{
                type:"start_stream",
                id:"BMP_carousel_0",
              })

              api.send_P(
                "CM",0,{
                  type:"start_stream",
                  id:"BMP_carousel_2"
                })
              api.send_P(
                "CM",0,{
                  type:"start_stream",
                  id:"BMP_carousel_3",
                })


          })




        }}>Start Stream</Button>


<Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            
            api.send_P(
              "CM",0,{
                type:"trigger",
                id:"BMP_carousel_0",
                trigger_tag:"KLKS0",
                trigger_id:0
              })
          })
        }}>Trig0</Button>
        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            
            api.send_P(
              "CM",0,{
                type:"trigger",
                id:"BMP_carousel_0",
                trigger_tag:"KLKS1",
                trigger_id:0
              })
          })
        }}>Trig1</Button>


        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            
            Promise.all([
              api.send_P(
                "CM",0,{
                  type:"stop_stream",
                  id:"BMP_carousel_1"
                }),
              api.send_P(
                "CM",0,{
                  type:"stop_stream",
                  id:"BMP_carousel_0"
                }),
  
              api.send_P(
                "CM",0,{
                  type:"stop_stream",
                  id:"BMP_carousel_2"
                }),
              api.send_P(
                "CM",0,{
                  type:"stop_stream",
                  id:"BMP_carousel_3"
                })
            ])
            
            .then((d)=>{

              api.send_P(
                "CM",0,{
                  type:"clean_trigger_info_matching_buffer",
                })


            })

            




          })




        }}>Stop Stream</Button>

        {
          defInfo.rules.map(rule=><SingleTargetVIEWUI key={rule.name} IMCM_group={IMCM_group} rule={rule} onRuleChange={rule=>console.log(rule)}/>)

        
        
        
        }
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

      // CNC_api.connect({
      //   uart_name:"/dev/cu.SLAB_USBtoUART",
      //   baudrate:230400
      // });
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
  let test_ch_id=51000;
  return (
    <Layout className="layout" style={{width:"100%",height:"100%"}}>

      {/* <Header>00000</Header> */}
      <Sider collapsible collapsed={true}>

      </Sider>
      <Content style={{ padding: '0 50px' }}>

        {/* <InspectionTargets inspDef={{
          name:"aaa",
          cameraList:[{id:"ssss"}],
          rules:[]
        }} onDefSetupUpdate={(_)=>false}/>
        <br/> */}


        <VIEWUI/>
      </Content>
      {/* <Footer style={{ textAlign: 'center' }}>


      </Footer> */}
    </Layout>

  );
}

export default App;
