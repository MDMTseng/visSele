import React from 'react';
import { useState, useEffect,useRef } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout,Button,Tabs } from 'antd';

import clone from 'clone';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER,EXT_API_UNREGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement} from './UTIL/MISC_Util';

import GenMatching_rdx from './CanvasComponent';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';

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
        // ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
        //   let api=_api as BPG_WS;//cast
        //   api.send(
        //     "CM",0,{
        //       type:"connect",
        //       driver_idx:cam.driver_idx,
        //       cam_idx:cam.cam_idx,
        //       misc:cam.misc,
        //       _PGID_:cam.channel_id,
        //       _PGINFO_:{keep:true}
        //     },undefined,
        //     {
        //       reject:(arg:any[])=>console.error(arg),
        //       resolve:(arg:any[])=>{

        //         console.log(arg);

                
        //       }
        //     })
        // })




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

function App() {
  
  const dispatch = useDispatch();
  const CORE_API_INFO = useSelector((state:StoreTypes) => state.EXT_API[CORE_ID]);
  const ACT_EXT_API_REGISTER= (...p:Parameters<typeof EXT_API_REGISTER>) => dispatch(EXT_API_REGISTER(...p));
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const ACT_EXT_API_UPDATE= (...p:Parameters<typeof EXT_API_UPDATE>) => dispatch(EXT_API_UPDATE(...p));
  const ACT_EXT_API_CONNECTED= (...p:Parameters<typeof EXT_API_CONNECTED>) => dispatch(EXT_API_CONNECTED(...p));
  const ACT_EXT_API_DISCONNECTED= (...p:Parameters<typeof EXT_API_DISCONNECTED>) => dispatch(EXT_API_DISCONNECTED(...p));


  const [camList,setCamList]=useState<{[key:string]:{[key:string]:any,list:any[]}}>({});

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

  let camUIInfo = Object.keys(camList).map((driverKey:string,index)=>{
   
    let camInfos=camList[driverKey];

    return camInfos.list.map((cam,idx)=>({
      driver_idx:index,
      id:cam.id,
      misc:(driverKey=="bmpcarousel")?"data/BMP_carousel_test":"",
      cam_idx:idx,
      channel_id:index*1000+idx+50000
    }));
  }).flat();
  console.log(camUIInfo);

 
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

        <CameraDiscover onCameraSelected={(camInfo)=>{
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


                    console.log(e)
                  },
                })
              // api.send_P("CM",0,{type:"connected_camera_list"})


          })
        }}/>
        


        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send("IT",0,{type:"create",id:"INSP1",_PGID_:51000,_PGINFO_:{keep:true}},undefined,{
                reject:(e)=>{},
                resolve:(e)=>{


                  console.log(e)
                },
              })


            api.send("IT",0,{type:"create",id:"INSP2",_PGID_:52000,_PGINFO_:{keep:true}},undefined,{
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
            api.send_P(
              "CM",0,{
                type:"trigger",
                id:"BMP_carousel_0",
                trigger_tag:"KLKS0",
                trigger_id:1
              })

            // api.send_P(
            //   "CM",0,{
            //     type:"trigger",
            //     id:"BMP_carousel_1",
            //     trigger_tag:"KLKS1"
            //   })

          })




        }}>Trig</Button>


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




      </Content>
      {/* <Footer style={{ textAlign: 'center' }}>

        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send(
              "CM",0,{
                type:"target_exchange",
                insp_type:"start_stream",
                channel_id:test_ch_id
              },undefined,
              {
                reject:(arg:any[])=>console.error(arg),
                resolve:(arg:any[])=>{
                  console.log(arg);
                }
              })



          })




        }}>Start Stream</Button>


        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
            let api=_api as BPG_WS;//cast
            

            api.send(
              "CM",0,{
                type:"target_exchange",
                insp_type:"stop_stream",
                channel_id:test_ch_id
              },undefined,
              {
                reject:(arg:any[])=>console.error(arg),
                resolve:(arg:any[])=>{
                  console.log(arg);
                }
              })



          })




        }}>Stop Stream</Button>

        <br/>
        <Button onClick={()=>{
          ACT_EXT_API_ACCESS(CNC_PERIPHERAL_ID,(_api)=>{
            let api=_api as CNC_Perif;//cast
            api.pushGCode(["G01 Y1000 Z1_600 R11_300 R12_430 F350","G01 Y0 Z1_0 R11_0 R12_0 F350"]);
          })
        }}>CNC cmd</Button>
        


      </Footer> */}
    </Layout>

  );
}

export default App;
