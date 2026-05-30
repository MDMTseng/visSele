// WebSocket + BPG transport for the core connection. Relocated verbatim from
// script.jsx (Path A): same behavior, deps (comp, StoreX) injected via constructor.
import BPG_Protocol from 'UTIL/BPG_Protocol.js';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { GetObjElement } from 'UTIL/MISC_Util';
import * as log from 'loglevel';

function urlConcat(base, add) {
  if (base === undefined) return undefined;
  let xbase = base;
  while (xbase.charAt(xbase.length - 1) == "/") xbase = xbase.slice(0, xbase.length - 1);
  let xadd = add;
  while (xadd.charAt(0) == "/") xadd = xadd.slice(1, xadd.length);
  return xbase + "/" + xadd;
}

    class BPG_WS
    {
      constructor(comp, store)
      {
        this.comp = comp;
        this.store = store;
        this.reqWindow= {};
        this.pgIDCounter= 0;
        this.websocket=undefined;
        this.isConnected=false;
        console.log(">>>>");
        this.systemStatusPull();
      }


      systemStatusPull()
      {
        setInterval(()=>{
          if(this.isConnected==false)return;
          
          this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "GS", 0,  { items: ["precess_queue_status","snap_queue_skip_count","save_snap_folder_full_delete_count","binary_path","data_path"] },
          undefined, 
          {
            resolve: (stacked_pkts,P) => {
              
              let GS=stacked_pkts.find(pkt=>pkt.type=="GS");
              if(GS===undefined)return;
              // console.log(GS.data)


              
              this.store.dispatch({type:"WS_UPDATE",id:this.comp.props.CORE_ID,
                info:GS.data});
            },
            reject:(e)=>{
            }
          });
        },1000);
      }

      connect(info)
      {
        let url = info.url;
        const peerId = this.comp.props.CORE_ID;
        log.info("[ws] connect", { peer: peerId, url });
        if (this._reconnectCount === undefined) this._reconnectCount = 0;
        this.websocket=new WebSocket(url);

        this.websocket.binaryType ="arraybuffer";

        this.websocket.onopen=(ev)=>{
          log.info("[ws] open", { peer: peerId, url, reconnects: this._reconnectCount });
        }
        this.websocket.onclose=(ev)=>{
          this.isConnected=false;
          // 1006 = abnormal closure (no close frame from peer — typically network
          // drop or process kill); call out separately since it's the dominant
          // factory failure mode and the only one with no `reason` payload.
          const code = ev && ev.code;
          const meta = { peer: peerId, url, code, reason: ev && ev.reason, reconnects: this._reconnectCount };
          if (code === 1006) log.warn("[ws] close 1006 (abnormal)", meta);
          else log.info("[ws] close", meta);
          this.store.dispatch(UIAct.EV_WS_REMOTE_SYSTEM_NOT_READY(ev));

          this.store.dispatch({type:"WS_DISCONNECTED",id:this.comp.props.CORE_ID,data:undefined});
          setTimeout(() => {
            this._reconnectCount = (this._reconnectCount || 0) + 1;
            log.info("[ws] reconnect attempt", { peer: peerId, url, attempt: this._reconnectCount });
            this.comp.props.ACT_WS_CONNECT(this.comp.props.CORE_ID, url);
          }, 10*1000);
        }

        this.websocket.onerror=(er)=>{
          // er is a generic Event for security (browsers don't expose details);
          // just note that it fired — onclose will follow with the code.
          log.warn("[ws] error event", { peer: peerId, url });
        }
        this.websocket.onmessage=(ev)=>{
          this.onmessage(ev);
        }
      }

      onmessage(evt){
        //log.info("onMessage:::");
        //log.info(evt);
        if (!(evt.data instanceof ArrayBuffer)) return;

        try {

        let header = BPG_Protocol.raw2header(evt);
        // log.info("onMessage:["+header.type+"]");
        let pgID = header.pgID;

        let parsed_pkt = undefined;
        let SS_start = false;

        switch (header.type) {
          case "HR":
            {
              {
                let HR = BPG_Protocol.raw2obj(evt);
                console.log("HR:",HR);
                this.store.dispatch(UIAct.EV_WS_REMOTE_SYSTEM_READY(HR));
                
                let version = GetObjElement(HR,["data","version"])||"_";
                this.store.dispatch({type:"WS_CONNECTED",id:this.comp.props.CORE_ID,data:HR,brief_info:version});
                
                this.isConnected=true;
              }

              this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "HR", 0, { a: ["d"] });
              
              {

                this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "LD", 0, { filename: "data/default_camera_param.json" },
                undefined, 
                {resolve: (data,action_channal) => {
                  console.log(data);
                  action_channal(data);
                  
                }});

                let machineSettingPath="data/machine_setting.json";
                this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "LD", 0,{ filename: machineSettingPath },
                undefined, 
                {resolve: (data) => {
                  console.log(data);
                  if (data[0].type == "FL") {
                    let info = data[0].data;//complete the necessary info
                    if(info.InspectionMode!="FI_C" &&info.InspectionMode!="FI" && info.InspectionMode!="CI" )
                    {
                      info.InspectionMode="CI";
                    }

                    if(info.setting_directory==undefined)
                    {
                      info.setting_directory="data/";
                    }


                    info.__priv={
                      path:machineSettingPath
                    }

                    if(info.inspection_db_ws_url!==undefined)
                    {
                      this.comp.props.ACT_WS_CONNECT(this.comp.props.Insp_DB_W_ID, urlConcat(info.inspection_db_ws_url,"/insert/insp"));
                      this.comp.props.ACT_WS_CONNECT(this.comp.props.DefFile_DB_W_ID, urlConcat(info.inspection_db_ws_url,"/insert/def"));
                    }


                    if(info.uInsp_peripheral_conn_info!==undefined)
                    {

                      this.comp.props.ACT_WS_GET_OBJ(this.comp.props.uInsp_API_ID, (obj)=>{
                        obj.connect( info.uInsp_peripheral_conn_info);
                      })
                    }
                    else
                    {
                      console.log("No uInsp_peripheral_conn_info:{url:xxxx}");
                    }


                    if(info.SLID_peripheral_conn_info!==undefined)
                    {

                      this.comp.props.ACT_WS_GET_OBJ(this.comp.props.SLID_API_ID, (obj)=>{
                        obj.connect( info.SLID_peripheral_conn_info);
                      })
                    }
                    else
                    {
                      console.log("No SLID_peripheral_conn_info:{url:xxxx}");
                    }


                    if(info.CNC_peripheral_conn_info!==undefined)
                    {

                      this.comp.props.ACT_WS_GET_OBJ(this.comp.props.CNC_API_ID, (obj)=>{
                        obj.connect( info.CNC_peripheral_conn_info);
                      })
                    }
                    else
                    {
                      console.log("No CNC_peripheral_conn_info:{url:xxxx}");
                    }



                    if(info.platform_api_conn_info!==undefined)
                    {

                      this.comp.props.ACT_WS_GET_OBJ(this.comp.props.Platform_API_ID, (obj)=>{
                        //console.log(obj);
                        obj.connect( info.platform_api_conn_info);
                      })
                    }
                    else
                    {
                      console.log("No platform_api_conn_info:{url:xxxx}");
                    }



                    console.log(info.SystemSetting);
                    if(info.SystemSetting!==undefined)
                    {
                      let newSetting={...this.comp.props.System_Setting,...info.SystemSetting}
                      console.log(newSetting);
                      this.comp.props.ACT_System_Setting_Update(newSetting);

                    }
                    else
                    {
                      console.log("No SystemSetting:{...}");
                    }



                    // "inspection_db_ws_url": "ws://localhost:8085/",
                    // "cusdisp_db_fetch_url": "http://localhost:8085/",
                    // "inspection_monitor_url": "http://localhost:8085/inspMon/?db_server=localhost:8085&",
                    // "platform_api_conn_info": {
                    //   "url": "ws://localhost:8085/platform_api"
                    // },

                    this.comp.props.ACT_Machine_Custom_Setting_Update(info);
                  }
                }});
            
                this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "LD", 0, { filename: "data/machine_info" },
                  undefined, 
                  {resolve: (data) => {
                    console.log(data);
                    if (data[0].type == "FL") {
                      let info = data[0].data;
                      if (info.length < 16) {
                        this.comp.props.ACT_MachTag_Update(info);
                      }
                    }
                }});


                let filePath="data/default_camera_setting.json";
                this.comp.props.ACT_WS_SEND_BPG(this.comp.props.CORE_ID, "LD", 0, { filename:filePath },
                  undefined, 
                  {resolve: (pkts) => {
                    if (pkts[0].type != "FL") return;

                    let cam_setting = pkts[0].data;
                    console.log(cam_setting);
                    this.comp.props.DISPATCH({type:"FILE_default_camera_setting",data:pkts[0].data})
                }});


              }


              break;
            }

          case "SS":
            {
              let SS = BPG_Protocol.raw2obj(evt);

              if (SS.data.start==true) {
                SS_start = true;
              }
              else {
              }
              parsed_pkt = SS;
              break;
            }
          case "IM":
            {
              let pkg = BPG_Protocol.raw2Obj_IM(evt);
              parsed_pkt = pkg;
              break;
            }
          case "IR":
          case "RP":
          case "DF":
          case "FL":
          case "SG":
          case "PD":
          default:
            {
              let report = BPG_Protocol.raw2obj(evt);
              parsed_pkt = report;

              break;
            }

        }

        {
          let req_pkt = this.reqWindow[pgID];

          if (req_pkt !== undefined)//Find the tracking req
          {
            if (parsed_pkt !== undefined)//There is an act, push into the req acts
              req_pkt.pkts.push(parsed_pkt);

            if (!SS_start && header.type == "SS")//Get the termination session[SS] pkt
            {//remove tracking(reqWindow) info and Dispatch the pkt
              let stacked_pkts = this.reqWindow[pgID].pkts;
              if (req_pkt._PGINFO_ === undefined || req_pkt._PGINFO_.keep !== true) {
                delete this.reqWindow[pgID];
              }
              else {
                this.reqWindow[pgID].pkts = [];
              }
              if (req_pkt.promiseCBs !== undefined) {
                req_pkt.promiseCBs.resolve(stacked_pkts, this.comp.WSDataDispatch);
              }
              else {
                // let acts={
                //   type:"ATBundle",
                //   ActionThrottle_type:"express",
                //   data:stacked_pkts.map(pkt=>BPG_Protocol.map_BPG_Packet2Act(pkt)).filter(act=>act!==undefined),
                //   rawData:req_pkt
                // };
                // this.props.DISPATCH(acts)
                this.comp.WSDataDispatch(stacked_pkts);
                // req_pkt.pkts.forEach((pkt)=>
                // {
                //   let act=map_BPG_Packet2Act(pkt);
                //   if(act!==undefined)
                //     this.props.DISPATCH(act)
                // });

              }
              //////
            }

          }
          else//No tracking req info in the window
          {
            if (SS_start)//And it's SS start, put the new tracking info
            {
              this.reqWindow[pgID] = {
                time: new Date().getTime(),
                pkts: [parsed_pkt]
              };
            }
            else {
              // let act = BPG_Protocol.map_BPG_Packet2Act(parsed_pkt);
              // if (act !== undefined)
              // this.comp.props.DISPATCH(act);
              console.log("LOSS TRACK pkts", parsed_pkt,this.reqWindow);
            }
          }

        }

        } catch (e) {
          // A malformed/corrupt frame must not kill the socket message pump.
          console.error("BPG_WS.onmessage: decode/dispatch error; dropping frame", e);
        }
      }

      send(info)
      {
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
          while(this.reqWindow[PGID]!==undefined)
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
          this.reqWindow[PGID] = {
            time: new Date().getTime(),
            pkts: [],
            promiseCBs:info.promiseCBs,
            _PGINFO_: PGINFO
          };

          this.websocket.send(BPG_Protocol.objbarr2raw(info.tl, info.prop, PGID, info.data, info.uintArr));
        }


        return true;

      }

      close()
      {
        return this.websocket.close();
      }
    }

export default BPG_WS;
