import React from 'react';
import { useState, useEffect } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Button } from 'antd';

import {StoreTypes} from './redux/store';
import {EXT_API_ACCESS, EXT_API_CONNECTED,EXT_API_DISCONNECTED, EXT_API_REGISTER} from './redux/actions/EXT_API_ACT';


import { GetObjElement} from './UTIL/MISC_Util';

import BPG_Protocol from './UTIL/BPG_Protocol';


interface MessageProps {
  text: string;
  important?: boolean;
}
 
let CORE_ID= "CORE_ID";

type TYPE_OBJECT ={[key:string]:any}


function App() {
  
  const dispatch = useDispatch();
  const CORE_API_INFO = useSelector((state:StoreTypes) => state.EXT_API[CORE_ID]);
  const ACT_EXT_API_REGISTER= (...p:Parameters<typeof EXT_API_REGISTER>) => dispatch(EXT_API_REGISTER(...p));
  const ACT_EXT_API_ACCESS= (...p:Parameters<typeof EXT_API_ACCESS>) => dispatch(EXT_API_ACCESS(...p));
  const ACT_EXT_API_CONNECTED= (...p:Parameters<typeof EXT_API_CONNECTED>) => dispatch(EXT_API_CONNECTED(...p));
  const ACT_EXT_API_DISCONNECTED= (...p:Parameters<typeof EXT_API_DISCONNECTED>) => dispatch(EXT_API_DISCONNECTED(...p));

  

  useEffect(() => {
    
    class BPG_WS
    {
      id:string
      reqWindow: Map<string|number,  
        {
          time: number,
          pkts: any[],
          promiseCBs:{
            reject(...arg:any[]):any,
            resolve(...arg:any[]):any,
          }|undefined;
          _PGINFO_:any
        }
      >
      pgIDCounter:number
      websocket:WebSocket|undefined//just for websocket
      isConnected:boolean


      constructor(id:string)
      {
        this.id=id;
        this.reqWindow= new Map();
        
        // {"aa":{time:0,pkts:[],promiseCBs:{resolve:()=>{},reject:()=>{}}}};
        this.pgIDCounter= 0;
        this.websocket=undefined;
        this.isConnected=false;
        this.systemStatusPull();
      }


      systemStatusPull()
      {
      }

      connect(info:{url:string})
      {
        let url = info.url;
        console.log(">>>>",info);
        this.websocket=new WebSocket(url);

        this.websocket.binaryType ="arraybuffer"; 

        this.websocket.onopen=(e:any)=>{
          // ACT_EXT_API_CONNECTED(this.id);//wait until HR arrive
        }
        this.websocket.onclose=(ev:any)=>{
          // StoreX.dispatch({type:"EXT_API_CONNECTED",id:comp.props.CORE_ID,data:undefined});
          // setTimeout(() => {
          //   comp.props.ACT_WS_CONNECT(comp.props.CORE_ID, url);
          // }, 10*1000);
          ACT_EXT_API_DISCONNECTED(this.id);
        }
        
        this.websocket.onerror=(er:any)=>{
          console.log("ERROR::",er);
        }
        this.websocket.onmessage=(ev:any)=>{
          this.onmessage(ev);
        }
      }

      onmessage(evt:any){
        
        //log.info("onMessage:::");
        //log.info(evt);
        if (!(evt.data instanceof ArrayBuffer)) return;
        
        let header = BPG_Protocol.raw2header(evt);
        if(header===undefined)return;
        // log.info("onMessage:["+header.type+"]");
        let pgID = header.pgID;

        let parsed_pkt:any = undefined;
        let SS_start = false;

        switch (header.type) {
          case "HR":
            {
              let HR = BPG_Protocol.raw2obj(evt);
              console.log("HR:",HR);
              let version = GetObjElement(HR,["data","version"])||"_";
              console.log(version)
              ACT_EXT_API_CONNECTED(this.id);

              this.send(
                "HR",0,{ a: ["d"] },undefined,
                {
                  reject:(...arg:any[])=>console.error(arg),
                  resolve:(...arg:any[])=>console.log(arg),
                })

              this.send( "LD", 0, { filename: "data/default_camera_param.json"},
                undefined, 
                {reject:(...arg:any[])=>console.error(arg),
                  resolve: (data) => {
                  console.log(data);
              }});
              // comp.props.ACT_WS_SEND_BPG(comp.props.CORE_ID, "HR", 0, { a: ["d"] });
              break;
            }
          case "IM":
            {

              let pkg = BPG_Protocol.raw2obj_IM(evt);
              parsed_pkt = pkg;
              break;
            }
          default:
            {
              
              let report = BPG_Protocol.raw2obj(evt);
              parsed_pkt = report;
              break;
            }
        }

        
        {
          let req_pkt = this.reqWindow.get(pgID);

          if (req_pkt !== undefined)//Find the tracking req
          {
            if (parsed_pkt !== undefined)//There is an act, push into the req acts
              req_pkt.pkts.push(parsed_pkt);

            if (!SS_start && header.type == "SS")//Get the termination session[SS] pkt
            {//remove tracking(reqWindow) info and Dispatch the pkt
              
              let stacked_pkts = req_pkt.pkts;
              if (req_pkt._PGINFO_ === undefined || req_pkt._PGINFO_.keep !== true) {
                this.reqWindow.delete(pgID)
              }
              else {
                req_pkt.pkts = [];
              }
              if (req_pkt.promiseCBs !== undefined) {
                req_pkt.promiseCBs.resolve(stacked_pkts);
              }
              else
              {
                console.error(">>>>promiseCBs is undefined");
              }
              //////
            }

          }
          else//No tracking req info in the window
          {
            if (SS_start)//And it's SS start, put the new tracking info
            {
              this.reqWindow.set(pgID,
                {
                  time: new Date().getTime(),
                  pkts: [parsed_pkt],
                  promiseCBs:undefined,
                  _PGINFO_:undefined
                })
            }
            else {
              // let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
              // if (act !== undefined)
              // comp.props.DISPATCH(act);
              console.log("LOSS TRACK pkts", parsed_pkt,this.reqWindow);
            }
          }

        }
      }

      

      send(
        tl:string,
        prop:number,
        data:TYPE_OBJECT|undefined,
        uintArr:Uint8Array|undefined,
        promiseCBs:{
          reject(...arg:any[]):any,
          resolve(...arg:any[]):any,
        }|undefined)
      {
        return this._send({
          tl,
          prop,
          data,//json object
          uintArr,
          promiseCBs
        })
      }
      _send(info:any)
      {
        if(this.websocket===undefined)return false;
        if(this.websocket.readyState!==WebSocket.OPEN)
        {
          if(info.promiseCBs!==undefined)
          {
            info.promiseCBs.reject("Not connected");
          }
          return false;
        }
        let PGID = undefined;
        let PGINFO = undefined;
        if (info.data instanceof Object) {
          PGID = info.data._PGID_;
          PGINFO = info.data._PGINFO_;
          delete info.data["_PGID_"];
          delete info.data["_PGINFO_"];
        }
        if (PGID === undefined) {
          let maxNum=500;
          PGID = this.pgIDCounter++;
          
          if(this.pgIDCounter>maxNum)
          {
            this.pgIDCounter=0;
          }
          while(this.reqWindow.get(PGID)!==undefined)
          {
            PGID = this.pgIDCounter++;
            
            if(this.pgIDCounter>maxNum)
            {
              this.pgIDCounter=0;
            }
          }

        }


        if (info.data instanceof Uint8Array) {
          // ws_obj.websocket.send(BPG_Protocol.objbarr2raw(data.tl, data.prop, PGID, null, data.data));
          console.log("");
          throw new Error('Here is not allowed anymore');
        }
        else {
          this.reqWindow.set(PGID,{
            time: new Date().getTime(),
            pkts: [],
            promiseCBs:info.promiseCBs,
            _PGINFO_: PGINFO
          });

          this.websocket.send(BPG_Protocol.objbarr2raw(info.tl, info.prop, PGID, info.data, info.uintArr));
        }


        return true;

      }


      close()
      {
        if(this.websocket===undefined)return;
        return this.websocket.close();
      }
    }


    let api=new BPG_WS(CORE_ID);
    ACT_EXT_API_REGISTER(api.id,api);
    
    api.connect({
      url:"ws://localhost:4090"
    });
    // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
    // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
    return (() => {
      });
      
  }, []); 

  return (
    <div className="App">
      <header className="App-header">
        <Button>{JSON.stringify(CORE_API_INFO)}</Button>
      </header>
    </div>
  );
}

export default App;
