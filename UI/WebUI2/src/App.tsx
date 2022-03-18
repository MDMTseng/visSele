import React from 'react';
import { useState, useEffect } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Button,Tabs } from 'antd';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UPDATE} from './redux/actions/EXT_API_ACT';


import { GetObjElement} from './UTIL/MISC_Util';

import GenMatching_rdx from './CanvasComponent';
import {CORE_ID,CORE_API_TYPE,CNC_PERIPHERAL_ID,CNC_PERIPHERAL_TYPE,BPG_WS,CNC_Perif} from './EXT_API';




function InspTargetUI()
{

  return (
    <Tabs defaultActiveKey="1" >
    {/* <TabPane tab="Tab 1" key="1">
      Content of Tab Pane 1
    </TabPane>
    <TabPane tab="Tab 2" key="2">
      Content of Tab Pane 2
    </TabPane>
    <TabPane tab="Tab 3" key="3">
      Content of Tab Pane 3
    </TabPane> */}
  </Tabs>)
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

      CNC_api.connect({
        uart_name:"/dev/cu.SLAB_USBtoUART",
        baudrate:230400
      });
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

 
  let test_ch_id=51000;
  return (
    <div className="App">
      <Button onClick={()=>{
        ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
          let api=_api as CORE_API_TYPE;//cast
          api.cameraDiscovery().then((ret:any)=>{
            console.log(ret);
            
            setCamList(ret[0].data)
          })
        })
      }}>{JSON.stringify(CORE_API_INFO)}</Button>
      



      {camUIInfo.map(cam=><Button key={"id_"+cam.channel_id} onClick={()=>{
        ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
          let api=_api as CORE_API_TYPE;//cast
          api.send(
            "CM",0,{
              type:"connect",
              driver_idx:cam.driver_idx,
              cam_idx:cam.cam_idx,
              misc:cam.misc,
              _PGID_:cam.channel_id,
              _PGINFO_:{keep:true}
            },undefined,
            {
              reject:(arg:any[])=>console.error(arg),
              resolve:(arg:any[])=>{

                console.log(arg);

                
              }
            })
        })




      }}>{cam.id}</Button>)}




      <Button onClick={()=>{
        ACT_EXT_API_ACCESS(CORE_ID,(_api)=>{
          let api=_api as CORE_API_TYPE;//cast
          

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
          let api=_api as CORE_API_TYPE;//cast
          

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


      <Button onClick={()=>{
        ACT_EXT_API_ACCESS(CNC_PERIPHERAL_ID,(_api)=>{
          let api=_api as CNC_PERIPHERAL_TYPE;//cast
          api.pushGCode(["G01 Y1000 Z1_600 R11_300 R12_430 F350","G01 Y0 Z1_0 R11_0 R12_0 F350"]);
        })
      }}>CNC cmd</Button>
      
      {/* <GenMatching_rdx image=/> */}
    </div>
  );
}

export default App;
