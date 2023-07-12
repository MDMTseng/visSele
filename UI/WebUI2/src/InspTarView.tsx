import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Button, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select } from 'antd';


import type { MenuProps, MenuTheme } from 'antd/es/menu';
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleFilled,PauseCircleOutlined,PauseCircleFilled,
    DisconnectOutlined, LinkOutlined,CameraOutlined,SyncOutlined,DeleteOutlined,ExclamationCircleOutlined,LoadingOutlined,StopOutlined
} from '@ant-design/icons';

import clone from 'clone';

import { StoreTypes } from './redux/store';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { listCMDPromise } from './XCMD';


import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d } from './UTIL/MathTools';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { Row, Col, Input, Tag, Modal, message, Space,Statistic,Avatar } from 'antd';


import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';

let DAT_ANY_UNDEF: any = undefined;

const { SubMenu } = Menu;
const { Option } = Select;

enum EDIT_PERMIT_FLAG {
    XXFLAGXX = 1 << 0
}



type IMCM_type =
    {
        camera_id: string,
        trigger_id: number,
        trigger_tag: string,
        image_info: {
            full_height: number
            full_width: number
            height: number
            image: ImageData|HTMLImageElement
            offsetX: number
            offsetY: number
            scale: number
            width: number
        }
    }


function PtsToXYWH(pt1: VEC2D, pt2: VEC2D) {
    let x, y, w, h;

    x = pt1.x;
    w = pt2.x - pt1.x;

    y = pt1.y;
    h = pt2.y - pt1.y;


    if (w < 0) {
        x += w;
        w = -w;
    }

    if (h < 0) {
        y += h;
        h = -h;
    }
    return {
        x, y, w, h
    }
}

function drawRegion(g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, region: { x: number, y: number, w: number, h: number }, lineWidth: number, drawCenterPoint: boolean = true, lineDeshInfo = [lineWidth * 10, lineWidth * 3, lineWidth * 3, lineWidth * 3]) {
    let ctx = g.ctx;
    // ctx.lineWidth = 5;

    let x = region.x;
    let y = region.y;
    let w = region.w;
    let h = region.h;
    ctx.beginPath();
    ctx.setLineDash(lineDeshInfo);
    // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
    ctx.lineWidth = lineWidth;
    ctx.rect(x, y, w, h);
    ctx.stroke();
    ctx.closePath();

    if (drawCenterPoint) {
        // ctx.strokeStyle = "rgba(179, 0, 0,0.5)";
        ctx.lineWidth = lineWidth * 2 / 3;
        canvas_obj.rUtil.drawCross(ctx, { x: x + w / 2, y: y + h / 2 }, lineWidth * 2 / 3);
    }



}


type IMCM_group = { [trigID: string]: IMCM_type }


export type CompParam_InspTar = {
    display: boolean,
    style?: any,
    fsPath: string,
    EditPermitFlag: number,
    renderHook: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, rule: any) => void) | undefined,
    // IMCM_group:IMCM_group,
    systemInspTarList: any[],
    def: any,
    report: any,
    onDefChange: (updatedDef: any, ddd: boolean) => void,
    APIExport: ((api_set: any) => void) | undefined,


    // UIOption:any,
    
    // onUIOptionUpdate:((new_UIOption: any) => void) | undefined,


}

export type CompParam_UIOption = {

    UIOption:any|undefined,
    showUIOptionConfigUI:boolean,
    onUIOptionUpdate:((new_UIOption: any) => void),


}

export type CompParam_InspTarUI =CompParam_InspTar & CompParam_UIOption;


type MenuItem = Required<MenuProps>['items'][number];

var enc = new TextEncoder();

const _DEF_FOLDER_PATH_ = "data/Test1.xprj";
// import ReactJsoneditor from 'jsoneditor-for-react';

// declare module 'jsoneditor-react'jsoneditor-for-react"

// import 'jsoneditor-react/es/editor.min.css';

let INSPTAR_BASE_STREAM_ID = 51000

const { TabPane } = Tabs;
const { Header, Content, Footer, Sider } = Layout;



function ColorRegionDetection_SingleRegion({ srule, onDefChange, canvas_obj }:
    {
        srule: any,
        onDefChange: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent
    }) {

    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    let srule_Filled = {

        region: [0, 0, 0, 0],
        colorThres: 10,
        hough_circle: {
            dp: 1,
            minDist: 5,
            param1: 100,
            param2: 30,
            minRadius: 5,
            maxRadius: 15
        },
        hsv: {
            rangeh: {
                h: 180, s: 255, v: 255
            },
            rangel: {
                h: 0, s: 0, v: 0
            },
        },

        ...srule
    }
    const _this = useRef<any>({}).current;
    return <>
        <Button key={">>>"} onClick={() => {
            canvas_obj.UserRegionSelect((info, state) => {
                if (state == 2) {
                    console.log(info);

                    let x, y, w, h;

                    x = info.pt1.x;
                    w = info.pt2.x - info.pt1.x;

                    y = info.pt1.y;
                    h = info.pt2.y - info.pt1.y;


                    if (w < 0) {
                        x += w;
                        w = -w;
                    }

                    if (h < 0) {
                        y += h;
                        h = -h;
                    }

                    let newRule = { ...srule_Filled, region: [Math.round(x), Math.round(y), Math.round(w), Math.round(h)] };
                    onDefChange(newRule)

                    canvas_obj.UserRegionSelect(undefined)
                }
            })
        }}>設定範圍</Button>

        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
            onConfirm={() => { }}
            onCancel={() => { }}
            okButtonProps={{
                danger: true, onClick: () => {
                    if (delConfirmCounter != 0) {
                        setDelConfirmCounter(delConfirmCounter - 1);
                    }
                    else {
                        onDefChange(undefined);
                        // onDefChange(undefined,false)
                    }
                }
            }}
            okText={"Yes:" + delConfirmCounter}
            cancelText="No"
        >
            <Button danger type="primary" onClick={() => {
                setDelConfirmCounter(5);
            }}>DEL</Button>
        </Popconfirm>
        {/* 
      <br/>hough_circle
      <Slider defaultValue={srule_Filled.hough_circle.minRadius} max={100} onChange={(v)=>{
  
      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,minRadius:v}};
        onDefChange(newRule)
      },()=>_this.trigTO=undefined,500);
  
      }}/>
      
      <Slider defaultValue={srule_Filled.hough_circle.maxRadius} max={100} onChange={(v)=>{
  
        _this.trigTO=
        ID_debounce(_this.trigTO,()=>{
          let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,maxRadius:v}};
          onDefChange(newRule)
        },()=>_this.trigTO=undefined,500);
  
      }}/>  */}




        <>


            <br />結果顯示
            <Slider defaultValue={srule_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["resultOverlayAlpha"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />

            <br />HSV
            <Slider defaultValue={srule_Filled.hsv.rangeh.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />



            <Slider defaultValue={srule_Filled.hsv.rangeh.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />




            <Slider defaultValue={srule_Filled.hsv.rangeh.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_throttle(_this.trigTO, () => {

                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />





        </>

        <br />細節量
        <Slider defaultValue={srule_Filled.colorThres} max={255} onChange={(v) => {

            _this.trigTO =
                ID_throttle(_this.trigTO, () => {
                    onDefChange(ObjShellingAssign(srule_Filled, ["colorThres"], v));
                }, () => _this.trigTO = undefined, 200);

        }} />




    </>

}





export function SingleTargetVIEWUI_ColorRegionDetection({ display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }: CompParam_InspTarUI) {
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        drawHooks: [],
        ctrlHooks: []


    });
    const [cacheDef, setCacheDef] = useState<any>(def);
    const [cameraQueryList, setCameraQueryList] = useState<any[] | undefined>([]);


    const [defReport, setDefReport] = useState<any>(undefined);
    const [forceUpdateCounter, setForceUpdateCounter] = useState(0);
    let _this = _.current;
    let c_report: any = undefined;
    if (_this.cache_report !== report) {
        if (report !== undefined) {
            _this.cache_report = report;
        }
    }
    c_report = _this.cache_report;


    useEffect(() => {
        console.log("fsPath:" + fsPath)
        _this.cache_report = undefined;
        setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);
    // console.log(IMCM_group,report);
    // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
    // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);


    enum editState {
        Normal_Show = 0,
        Region_Edit = 1,
    }

    const [stateInfo, setStateInfo] = useState<{ st: editState, info: any }[]>([{
        st: editState.Normal_Show,
        info: undefined
    }]);


    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [CNC_API, setCNC_API] = useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);


    const [queryCameraList, setQueryCameraList] = useState<any[] | undefined>(undefined);
    const [delConfirmCounter, setDelConfirmCounter] = useState(0);


    let stateInfo_tail = stateInfo[stateInfo.length - 1];



    function onCacheDefChange(updatedDef: any, ddd: boolean) {
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)

            // await BPG_API.CameraSWTrigger("Hikrobot-00F71598890", "TTT", 4433)

            // await BPG_API.CameraSWTrigger("BMP_carousel_0","TTT",4433)

        })()

    }


    useEffect(() => {//////////////////////
        let cbsKey="_"+Math.random();
        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id,cbsKey, {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    console.log("++++++++\n", IM, CM, RP);


                    setDefReport(RP.data)
                    let IMCM = {
                        image_info: IM.image_info,
                        camera_id: CM.data.camera_id,
                        trigger_id: CM.data.trigger_id,
                        trigger_tag: CM.data.trigger_tag,
                    } as type_IMCM

                    _this.imgCanvas.width = IMCM.image_info.width;
                    _this.imgCanvas.height = IMCM.image_info.height;

                    let ctx2nd = _this.imgCanvas.getContext('2d');

                    if(IMCM.image_info.image instanceof ImageData)
                        ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
                    else if(IMCM.image_info.image instanceof HTMLImageElement)
                        ctx2nd.drawImage(IMCM.image_info.image, 0, 0);

                    setLocal_IMCM(IMCM)
                    // console.log(IMCM)

                },
                reject: (pkts) => {

                }
            }

            )
            // await BPG_API.InspTargetSetStreamChannelID(
            //   cacheDef.id,stream_id,
            //   {
            //     resolve:(pkts)=>{
            //       // console.log(pkts);
            //       let IM=pkts.find((p:any)=>p.type=="IM");
            //       if(IM===undefined)return;
            //       let CM=pkts.find((p:any)=>p.type=="CM");
            //       if(CM===undefined)return;
            //       let RP=pkts.find((p:any)=>p.type=="RP");
            //       if(RP===undefined)return;
            //       console.log("++++++++\n",IM,CM,RP);


            //       setDefReport(RP.data)
            //       let IMCM={
            //         image_info:IM.image_info,
            //         camera_id:CM.data.camera_id,
            //         trigger_id:CM.data.trigger_id,
            //         trigger_tag:CM.data.trigger_tag,
            //       } as type_IMCM

            //       _this.imgCanvas.width = IMCM.image_info.width;
            //       _this.imgCanvas.height = IMCM.image_info.height;

            //       let ctx2nd = _this.imgCanvas.getContext('2d');
            //       ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


            //       setLocal_IMCM(IMCM)
            //       // console.log(IMCM)

            //     },
            //     reject:(pkts)=>{

            //     }
            //   }
            // )

        })()
        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);

                // await BPG_API.InspTargetSetStreamChannelID(
                //   cacheDef.id,0,
                //   {
                //     resolve:(pkts)=>{
                //     },
                //     reject:(pkts)=>{

                //     }
                //   }
                // )
            })()

        })
    }, []);
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


    if (display == false) return null;



    let EDIT_UI = null;

    switch (stateInfo_tail.st) {

        case editState.Normal_Show:


            let EditUI = null;
            if (true)//allow edit
            {
                EditUI = <>
                    <Button key={"_" + 10000} onClick={() => {

                        let newDef = { ...cacheDef };
                        newDef.regionInfo.push({ region: [0, 0, 0, 0], colorThres: 10 });
                        onCacheDefChange(newDef, false)


                        setStateInfo([...stateInfo, {
                            st: editState.Region_Edit,
                            info: {
                                idx: newDef.regionInfo.length - 1
                            }
                        }])

                    }}>+</Button>



                    {cacheDef.regionInfo.map((region: any, idx: number) => {
                        return <Button key={"_" + idx} onClick={() => {
                            if (_this.canvasComp === undefined) return;


                            setStateInfo([...stateInfo, {
                                st: editState.Region_Edit,
                                info: {
                                    idx: idx
                                }
                            }])




                        }}>{"idx:" + idx}</Button>
                    })}
                </>
            }

            EDIT_UI = <>

                <Input maxLength={100} value={cacheDef.id}
                    style={{ width: "100px" }}
                    onChange={(e) => {
                        console.log(e.target.value);

                        let newDef = { ...cacheDef };
                        newDef.id = e.target.value;
                        onCacheDefChange(newDef, false)


                    }} />

                <Input maxLength={100} value={cacheDef.type} disabled
                    style={{ width: "100px" }}
                    onChange={(e) => {

                    }} />

                <Input maxLength={100} value={cacheDef.sampleImageFolder} disabled
                    style={{ width: "100px" }}
                    onChange={(e) => {
                    }} />


                <Dropdown
                    overlay={<>
                        <Menu>
                            {
                                queryCameraList === undefined ?
                                    <Menu.Item disabled danger>
                                        <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                                            Press to update
                                        </a>
                                    </Menu.Item>
                                    :
                                    queryCameraList.map(cam => <Menu.Item key={cam.id}
                                        onClick={() => {
                                            let newDef = { ...cacheDef };
                                            newDef.camera_id = cam.id;
                                            // HACK_do_Camera_Check=true;
                                            onCacheDefChange(newDef, true)
                                        }}>
                                        {cam.id}
                                    </Menu.Item>)

                            }
                        </Menu>
                    </>}
                >
                    <Button onClick={() => {
                        // queryCameraList
                        setQueryCameraList(undefined);
                        BPG_API.queryDiscoverList()
                            .then((e: any) => {
                                console.log(e);
                                setQueryCameraList(e[0].data)
                            })
                        // let api=await getAPI(CORE_ID) as BPG_WS;
                        // let cameraListInfos=await api.cameraDiscovery() as any[];
                        // let CM=cameraListInfos.find(info=>info.type=="CM")
                        // if(CM===undefined)throw "CM not found"
                        // console.log(CM.data);
                        // return CM.data as {name:string,id:string,driver_name:string}[];

                    }}>{cacheDef.camera_id}</Button>
                </Dropdown>



                <Input maxLength={100} value={cacheDef.trigger_tag}
                    style={{ width: "100px" }}
                    onChange={(e) => {
                        let newDef = { ...cacheDef };
                        newDef.trigger_tag = e.target.value;
                        onCacheDefChange(newDef, false)
                    }} />

                <Popconfirm
                    title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                    onConfirm={() => { }}
                    onCancel={() => { }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            if (delConfirmCounter != 0) {
                                setDelConfirmCounter(delConfirmCounter - 1);
                            }
                            else {
                                onCacheDefChange(undefined, false)
                            }
                        }
                    }}
                    okText={"Yes:" + delConfirmCounter}
                    cancelText="No"
                >
                    <Button danger type="primary" onClick={() => {
                        setDelConfirmCounter(5);
                    }}>DEL</Button>
                </Popconfirm>
                <br />
                <Button onClick={() => {
                    onCacheDefChange(cacheDef, true);
                }}>SHOT</Button>
                {/* <Button onClick={() => {
                    onDefChange(cacheDef, true)
                }}>SAVE</Button> */}

                {EditUI}


            </>


            break;

        case editState.Region_Edit:


            if (cacheDef.regionInfo.length <= stateInfo_tail.info.idx) {
                break;
            }

            let regionInfo = cacheDef.regionInfo[stateInfo_tail.info.idx];

            EDIT_UI = <>
                <Button key={"_" + -1} onClick={() => {

                    let new_stateInfo = [...stateInfo]
                    new_stateInfo.pop();

                    setStateInfo(new_stateInfo)
                }}>{"<"}</Button>
                <ColorRegionDetection_SingleRegion
                    srule={regionInfo}
                    onDefChange={(newDef_sregion) => {
                        // console.log(newDef);


                        let newDef = { ...cacheDef };
                        if (newDef_sregion !== undefined) {
                            newDef.regionInfo[stateInfo_tail.info.idx] = newDef_sregion;
                        }
                        else {

                            newDef.regionInfo.splice(stateInfo_tail.info.idx, 1);

                            let new_stateInfo = [...stateInfo]
                            new_stateInfo.pop();

                            setStateInfo(new_stateInfo)

                        }

                        onCacheDefChange(newDef, true)
                        // _this.sel_region=undefined

                    }}
                    canvas_obj={_this.canvasComp} />
            </>

            break;
    }



    return <div style={{ ...style}} className={"overlayCon"}>

        <div className={"overlay"} >

            {EDIT_UI}

        </div>


        <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            _this.canvasComp = canvas_obj;
            // console.log(ctrl_or_draw);
            if (ctrl_or_draw == true)//ctrl
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
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }
                    _this.sel_region = PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2);

                }
            }
            else//draw
            {
                if (Local_IMCM !== undefined) {
                    g.ctx.save();
                    let scale = Local_IMCM.image_info.scale;
                    g.ctx.scale(scale, scale);
                    g.ctx.translate(-0.5, -0.5);
                    g.ctx.drawImage(_this.imgCanvas, 0, 0);
                    g.ctx.restore();
                }
                // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))


                let ctx = g.ctx;

                {
                    cacheDef.regionInfo.forEach((region: any, idx: number) => {

                        let region_ROI =
                        {
                            x: region.region[0],
                            y: region.region[1],
                            w: region.region[2],
                            h: region.region[3]
                        }

                        ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                        drawRegion(g, canvas_obj, region_ROI, canvas_obj.rUtil.getIndicationLineSize());

                        ctx.font = "40px Arial";
                        ctx.fillStyle = "rgba(0, 179, 0,0.5)";
                        ctx.fillText("idx:" + idx, region_ROI.x, region_ROI.y)


                        let region_components = GetObjElement(c_report, ["regionInfo", idx, "components"]);
                        // console.log(report,region_components);
                        if (region_components !== undefined) {
                            region_components.forEach((regComp: any) => {

                                canvas_obj.rUtil.drawCross(ctx, { x: regComp.x, y: regComp.y }, 5);

                                ctx.font = "4px Arial";
                                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                                ctx.fillText(regComp.area, regComp.x, regComp.y)

                                ctx.font = "4px Arial";
                                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                                ctx.fillText(`${regComp.x},${regComp.y}`, regComp.x, regComp.y + 5)
                            })

                        }



                    })
                }


                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());

                }
            }


            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }
        }
        } />

    </div>;

}



function Orientation_ColorRegionOval_SingleRegion({ srule, onDefChange, canvas_obj }:
    {
        srule: any,
        onDefChange: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent
    }) {

    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    let srule_Filled = {

        region: [0, 0, 0, 0],
        colorThres: 10,
        hsv: {
            rangeh: {
                h: 180, s: 255, v: 255
            },
            rangel: {
                h: 0, s: 0, v: 0
            },
        },

        contour: {
            lengthh: 400000,
            lengthl: 70,

            areah: 639030,
            areal: 400,

        },

        ...srule
    }
    const _this = useRef<any>({}).current;
    return <>
        <Button key={">>>"} onClick={() => {
            canvas_obj.UserRegionSelect((info, state) => {
                if (state == 2) {
                    console.log(info);

                    let x, y, w, h;

                    x = info.pt1.x;
                    w = info.pt2.x - info.pt1.x;

                    y = info.pt1.y;
                    h = info.pt2.y - info.pt1.y;


                    if (w < 0) {
                        x += w;
                        w = -w;
                    }

                    if (h < 0) {
                        y += h;
                        h = -h;
                    }

                    let newRule = { ...srule_Filled, region: [Math.round(x), Math.round(y), Math.round(w), Math.round(h)] };
                    onDefChange(newRule)

                    canvas_obj.UserRegionSelect(undefined)
                }
            })
        }}>設定範圍</Button>

        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
            onConfirm={() => { }}
            onCancel={() => { }}
            okButtonProps={{
                danger: true, onClick: () => {
                    if (delConfirmCounter != 0) {
                        setDelConfirmCounter(delConfirmCounter - 1);
                    }
                    else {
                        onDefChange(undefined);
                        // onDefChange(undefined,false)
                    }
                }
            }}
            okText={"Yes:" + delConfirmCounter}
            cancelText="No"
        >
            <Button danger type="primary" onClick={() => {
                setDelConfirmCounter(5);
            }}>DEL</Button>
        </Popconfirm>
        {/* 
      <br/>hough_circle
      <Slider defaultValue={srule_Filled.hough_circle.minRadius} max={100} onChange={(v)=>{
  
      _this.trigTO=
      ID_debounce(_this.trigTO,()=>{
        let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,minRadius:v}};
        onDefChange(newRule)
      },()=>_this.trigTO=undefined,500);
  
      }}/>
      
      <Slider defaultValue={srule_Filled.hough_circle.maxRadius} max={100} onChange={(v)=>{
  
        _this.trigTO=
        ID_debounce(_this.trigTO,()=>{
          let newRule={...srule_Filled,hough_circle:{...srule_Filled.hough_circle,maxRadius:v}};
          onDefChange(newRule)
        },()=>_this.trigTO=undefined,500);
  
      }}/>  */}




        <>


            <br />結果顯示
            <Slider defaultValue={srule_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["resultOverlayAlpha"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />

            <br />HSV
            <Slider defaultValue={srule_Filled.hsv.rangeh.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />



            <Slider defaultValue={srule_Filled.hsv.rangeh.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />




            <Slider defaultValue={srule_Filled.hsv.rangeh.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangeh", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.hsv.rangel.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_throttle(_this.trigTO, () => {

                        onDefChange(ObjShellingAssign(srule_Filled, ["hsv", "rangel", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />



            <br />Edge thres
            <Slider defaultValue={srule_Filled.contour.lengthh} max={3000} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["contour", "lengthh"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.contour.lengthl} max={3000} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["contour", "lengthl"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />


            <br />Area thres
            <Slider defaultValue={srule_Filled.contour.areah} max={10000} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["contour", "areah"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={srule_Filled.contour.areal} max={10000} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(srule_Filled, ["contour", "areal"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />


        </>

        <br />細節量
        <Slider defaultValue={srule_Filled.colorThres} max={255} onChange={(v) => {

            _this.trigTO =
                ID_throttle(_this.trigTO, () => {
                    onDefChange(ObjShellingAssign(srule_Filled, ["colorThres"], v));
                }, () => _this.trigTO = undefined, 200);

        }} />




    </>

}

function rgb2hsv(r: number, g: number, b: number) {
    let rabs, gabs, babs, rr, gg, bb, h = 0, s, v: number, diff: number, diffc, percentRoundFn;
    rabs = r / 255;
    gabs = g / 255;
    babs = b / 255;
    v = Math.max(rabs, gabs, babs);
    diff = v - Math.min(rabs, gabs, babs);
    diffc = (c: number) => (v - c) / 6 / diff + 1 / 2;
    percentRoundFn = (num: number) => Math.round(num * 100) / 100;
    if (diff == 0) {
        h = s = 0;
    } else {
        s = diff / v;
        rr = diffc(rabs);
        gg = diffc(gabs);
        bb = diffc(babs);

        if (rabs === v) {
            h = bb - gg;
        } else if (gabs === v) {
            h = (1 / 3) + rr - bb;
        } else if (babs === v) {
            h = (2 / 3) + gg - rr;
        }
        if (h < 0) {
            h += 1;
        } else if (h > 1) {
            h -= 1;
        }
    }
    // return {
    //     h: Math.round(h * 360),
    //     s: percentRoundFn(s * 100),
    //     v: percentRoundFn(v * 100)
    // };

    return [h * 180, s * 255, v * 255];
}


function TestInputSelectUI({ folderPath, stream_id, testTags = [] }: { folderPath: string, stream_id: number, testTags: string[] }) {
    const _this = useRef<any>({}).current;
    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [imageFolderInfo, setImageFolderInfo] = useState<any>(undefined);
    const [finalReports, setFinalReports] = useState<any>({});
    const [latestSelect, setLatestSelect] = useState<any>(undefined);

    const injectID_Prefix = "s_InjectID:";
    const cbs_key = "xxxx";
    _this.finalReports = finalReports;




    function FileListReset() {
        (async () => {
            let folderContent = await BPG_API.Folder_Struct(folderPath, 2);
            let regex = /.+\.png/i;

            let imageInfo = folderContent.files
                .filter((finfo: any) => (finfo.name != "FeatureRefImage.png") && regex.test(finfo.name))
                .sort((finfo1: any, finfo2: any) => finfo1.mtime_ms - finfo2.mtime_ms)

            console.log(imageInfo);

            folderContent.files = imageInfo;
            setImageFolderInfo(folderContent);

            setFinalReports({})//clear
            setLatestSelect(undefined);
            console.log(folderContent)


        })()
    }



    useEffect(() => {//////////////////////
        console.log("TestInputSelectUI INIT");




        BPG_API.send_cbs_attach(
            stream_id, cbs_key, {

            resolve: (pkts) => {
                let RP = pkts.find((p: any) => p.type == "RP");
                if (RP === undefined) return;

                let tags = RP.data.tags as string[];
                let injID = tags.find((tag: string) => tag.startsWith(injectID_Prefix));

                if (injID === undefined) return;
                injID = injID.replace(injectID_Prefix, "");


                setFinalReports({ ..._this.finalReports, [injID]: RP.data })


            },
            reject: (pkts) => {

            }
        }

        )


        FileListReset();










        return (() => {

            BPG_API.send_cbs_detach(stream_id, cbs_key);

            console.log("TestInputSelectUI EXIT");
        });
    }, []);

    function ImgTest(folder_path: string, fileInfo: { name: string }, tags: string[] = []) {
        let sIDTag = injectID_Prefix + fileInfo.name;
        // let final_tags=[sIDTag,...tags];
        let final_tags = [...tags];

        console.log(final_tags);
        BPG_API.InjectImage(folder_path + "/" + fileInfo.name, final_tags, Date.now());

        setLatestSelect({
            ...imageFolderInfo,
            sIDTag,
            file: fileInfo
        });

    }

    let bottonRunAll = imageFolderInfo === undefined ? null :
        <Button onClick={() => {

            setFinalReports({})//clear
            setLatestSelect(undefined);
            imageFolderInfo.files.forEach((file: any) => {
                // let resultType=NaN;
                // let report=finalReports[file.name];
                // if(report!==undefined)
                // {
                //   resultType=report.report.category
                // }
                if (file.name.startsWith("IG_")) return;
                //console.log(testTags);
                ImgTest(imageFolderInfo.path, file, testTags);
            })

        }}>

            群組測試
        </Button>

    // console.log(finalReports,latestSelect)
    let bottonS = imageFolderInfo === undefined ? null :
        imageFolderInfo.files.map((file: any) => {
            let hasGenOK = false;
            let hasGenNG = false;
            let report = finalReports[file.name];
            if (report !== undefined) {
                report.report.sub_reports.forEach((subrep: any) => {
                    if (subrep.category == 1) hasGenOK = true;
                    if (subrep.category == -1) hasGenNG = true;
                })
            }
            let pureGenOK = hasGenOK && !hasGenNG;
            let pureGenNG = !hasGenOK && hasGenNG;

            // console.log(hasGenOK,hasGenNG)
            // console.log(file.name,pureGenOK,pureGenNG)

            return <Button key={file.name} type={(pureGenOK || pureGenNG) ? "primary" : "dashed"} danger={hasGenNG} ghost={!hasGenOK && !hasGenNG}
                onClick={() => {
                    ImgTest(imageFolderInfo.path, file, testTags);

                }}>
                {file.name.replace(".png", "")}
            </Button>
        })


    async function fileRename(folder_path: string, cName: string, nName: string) {
        let renameResult = await BPG_API.FileRename(folder_path + "/" + cName, folder_path + "/" + nName);
        console.log(renameResult);
        FileListReset();

    }

    //`確定命名為OK?`
    function Btn_LatestSelectFile_Rename(prefix: string, btnText: string, confirmText: string) {
        return <Popconfirm
            title={confirmText}
            onConfirm={() => { }}
            onCancel={() => { }}
            okButtonProps={{
                danger: true, onClick: () => {

                    let fname = latestSelect.file.name;
                    fname = prefix + fname.replace(/^[a-zA-Z]+_/g, "");
                    fileRename(latestSelect.path, latestSelect.file.name, fname);
                }
            }}
            okText={"好"}
            cancelText="No"
        >
            <Button onClick={() => { }}>
                {btnText}
            </Button>
        </Popconfirm>
    }

    return <>

        <Button danger type="primary" onClick={() => {
            FileListReset();

        }}>檔案重整</Button>

        {bottonRunAll}

        <br />
        {latestSelect === undefined ? null : <>
            {latestSelect.file.name}

            {/* 
        {Btn_LatestSelectFile_Rename("NG_","NG",`確定命名為NG?`)}
  
  
  
        {Btn_LatestSelectFile_Rename("OK_","OK",`確定命名為OK?`)}
   */}


            {latestSelect.file.name.startsWith("IG_") ?
                Btn_LatestSelectFile_Rename("", "加入群組測試", `確定設定至群組測試清單?`) :
                Btn_LatestSelectFile_Rename("IG_", "忽略群組測試", `確定設定至群組測試 忽略清單?`)}


        </>}
        <div style={{ width: "100%", height: "400px", background: "rgba(0,0,0,0.8)", overflow: "scroll" }}>
            {bottonS}

            <br /><br />說明:
            <Button key={"all OK log"} type="primary">
                可檢全OK
            </Button>

            <Button key={"all NG log"} type="primary" danger>
                可檢全NG
            </Button>

            <Button key={"all NG OK Mix"} type="dashed" danger>
                可檢OK NG混合
            </Button>

            <Button key={"no insp"} type="dashed" ghost>
                無可檢
            </Button>
        </div>


    </>
}


export function SingleTargetVIEWUI_Orientation_ShapeBasedMatching(props: CompParam_InspTarUI) {
    let { display, fsPath, style = undefined, renderHook, def, EditPermitFlag, report, onDefChange, APIExport } = props;
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        drawHooks: [],
        ctrlHooks: [],


        extDrawHook: undefined,
        featureImgCanvas: document.createElement('canvas'),
    });
    const SBM_FEAT_REF_IMG_NAME = "FeatureRefImage.png"
    let _this = _.current;
    const [cacheDef, setCacheDef] = useState<any>(def);
    const [featureInfoExt, setFeatureInfoExt] = useState<any>({});

    const [featureInfo, setFeatureInfo] = useState<any>({});

    const [defReport, setDefReport] = useState<any>(undefined);



    const emptyModalInfo = {
        timeTag: 0,
        visible: false,
        type: "",
        onOK: (minfo: any) => { },
        onCancel: (minfo: any) => { },
        title: "",
        DATA: DAT_ANY_UNDEF,
        contentCB: (minfo: any) => <></>

    }
    const [modalInfo, setModalInfo] = useState(emptyModalInfo);


    const [onMouseClick, setOnMouseClick] = useState<any>(undefined);

    let c_report: any = undefined;
    if (_this.cache_report !== report) {
        if (report !== undefined) {
            _this.cache_report = report;
        }
    }
    c_report = _this.cache_report;


    useEffect(() => {
        console.log("fsPath:" + fsPath)
        _this.cache_report = undefined;
        setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);


    useEffect(() => {


        console.log(def);
        BPG_API.InspTargetExchange(def.id, {
            type: "stream_info",
            downsample: display ? 1 : 10,
            stream_id: def.stream_id
        });

        return (() => {
        });

    }, [display]);




    // useEffect(() => {
    //     console.log(APIExport)

    //     if(APIExport!==undefined)
    //     {
    //         APIExport({
    //             api1:()=>"hello world"
    //         })
    //     }



    //     return (() => {
    //     });

    // }, [APIExport]);


    // console.log(">>>>>>>",onMouseClick);

    // console.log(IMCM_group,report);
    // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
    // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);


    enum EditState {
        Normal_Show = 0,
        Feature_Edit = 1,
        Search_Region_Edit = 2,
        Test_Saved_Files = 3,
        NA = -99999
    }

    const [editState, _setEditState] = useState<EditState>(EditState.Normal_Show);

    function setEditState(newEditState: EditState) {
        let state3Ev: EditState[] = [];//3 elements, leave,stay,enter
        if (newEditState != editState) {
            state3Ev = [editState, EditState.NA, newEditState]
        }
        state3Ev.forEach((st, idx) => {

            switch (st)//current state
            {
                case EditState.Normal_Show:
                    if (idx == 2)//enter
                    {

                    }
                    else if (idx == 0)//leave
                    {

                    }
                    break;
                case EditState.Feature_Edit:
                    if (idx == 2)//enter
                    {
                        setFeatureInfo(cacheDef.featureInfo === undefined ? {} : cacheDef.featureInfo);

                        //if(featureInfoExt.IM===undefined)//do a init image fetch
                        (async () => {

                            let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                                type: "extract_feature",
                                image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                                feature_count: -1,
                                image_transfer_downsampling: 1,
                            }) as any[];

                            let newFeatureInfoExt: any = {};


                            let IM = pkts.find((p: any) => p.type == "IM");
                            if (IM !== undefined) {
                                _this.featureImgCanvas.width = IM.image_info.width;
                                _this.featureImgCanvas.height = IM.image_info.height;

                                let ctx2nd = _this.featureImgCanvas.getContext('2d');
                                ctx2nd.putImageData(IM.image_info.image, 0, 0);
                                newFeatureInfoExt.IM = IM;

                            }

                            setFeatureInfoExt({ ...featureInfoExt, ...newFeatureInfoExt })



                        })()
                    }
                    else if (idx == 0)//leave
                    {
                        setFeatureInfo({})
                    }
                    break;

                case EditState.Search_Region_Edit:
                    if (idx == 2)//enter
                    {
                    }
                    else if (idx == 0)//leave
                    {
                    }
                    break;

                case EditState.Test_Saved_Files:
                    if (idx == 2)//enter
                    {
                    }
                    else if (idx == 0)//leave
                    {
                    }
                    break;

            }
        })
        _setEditState(newEditState);
    }

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    const [updateC, setUpdateC] = useState(0);


    function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)
        })()
        onDefChange(updatedDef, doTakeNewImage);
    }


    useEffect(() => {//////////////////////

        let cbsKey="_"+Math.random();
        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});
            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, cbsKey, {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    // console.log("++++++++\n",IM,CM,RP);


                    setDefReport(RP.data)
                    let IMCM = {
                        image_info: IM.image_info,
                        camera_id: CM.data.camera_id,
                        trigger_id: CM.data.trigger_id,
                        trigger_tag: CM.data.trigger_tag,
                    } as type_IMCM

                    _this.imgCanvas.width = IMCM.image_info.width;
                    _this.imgCanvas.height = IMCM.image_info.height;

                    let ctx2nd = _this.imgCanvas.getContext('2d');

                    // console.log(IMCM.image_info);
                    if(IMCM.image_info.image instanceof ImageData)
                        ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
                    else if(IMCM.image_info.image instanceof HTMLImageElement)
                        ctx2nd.drawImage(IMCM.image_info.image, 0, 0);

                    setLocal_IMCM(IMCM)
                    // console.log(IMCM)
                    //console.log(def.id)

                },
                reject: (pkts) => {

                }
            }

            )

        })()
        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);

                // await BPG_API.InspTargetSetStreamChannelID(
                //   cacheDef.id,0,
                //   {
                //     resolve:(pkts)=>{
                //     },
                //     reject:(pkts)=>{

                //     }
                //   }
                // )
            })()

        })
    }, []);
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


    if (display == false) return null;




    let EDIT_UI = null;

    switch (editState) {

        case EditState.Normal_Show:


            let EditUI = null;
            if ((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) != 0)//allow edit
            {
                EditUI = <>

                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        console.log(cacheDef, newDef)
                        onCacheDefChange(newDef, ddd);
                    }} />
                    {/* <Button onClick={()=>{
            BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
          }}>重試</Button> */}








                    {/* <Button onClick={() => {
                        onDefChange(cacheDef, true)
                    }}>Commit</Button> */}
                    <Button onClick={() => {

                        setEditState(EditState.Feature_Edit);

                    }}>編輯特徵</Button>

                    <Button onClick={() => {

                        setEditState(EditState.Search_Region_Edit);

                    }}>編輯搜尋範圍</Button>

                </>
            }

            EDIT_UI = <>

                <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
                {/* 
          <Input maxLength={100} value={cacheDef.type} disabled
            style={{width:"100px"}}
            onChange={(e)=>{
            }}/>
  
          <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
            style={{width:"100px"}}
            onChange={(e)=>{
            }}/> */}

                {/* <Button onClick={()=>{
            onCacheDefChange(cacheDef,true);
          }}>照相</Button> */}



                {EditUI}


                <br />


                <Button onClick={() => {



                    let default_name = Date.now();

                    setModalInfo({
                        timeTag: Date.now(),
                        visible: true,
                        type: ">>",
                        onOK: (minfo: typeof modalInfo) => {
                            // resolve(true)


                            (async () => {
                                let name = minfo.DATA.prefix + minfo.DATA.name + ".png";
                                let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                                    type: "cache_image_save",
                                    folder_path: fsPath + "/",
                                    image_name: name,
                                }) as any[];
                                console.log(pkts);
                                // if(pkts[0].data.ACK)
                                // {
                                // }
                                // else
                                // {
                                //   message.info(`${name} 儲存失敗`);
                                // }

                                message.info(`${name} 儲存...`);
                                setModalInfo({ ...minfo, visible: false })

                            })()
                        },
                        onCancel: (minfo: typeof modalInfo) => {
                            // reject(false)
                            setModalInfo({ ...minfo, visible: false })
                        },
                        title: "儲存當前圖檔",
                        DATA: {
                            prefix: "",
                            name: default_name
                        },
                        contentCB: (minfo: typeof modalInfo) => <>

                            檔案名稱:
                            <Input addonBefore={
                                <Select key={default_name} defaultValue="___" onChange={value => setModalInfo(ObjShellingAssign(minfo, ["DATA", "prefix"], value))}>
                                    <Option value="___">{"___"}</Option>
                                    <Option value="[OK]">OK</Option>
                                    <Option value="[NG]">NG</Option>
                                </Select>} value={minfo.DATA.name}
                                onChange={(ev) => {
                                    setModalInfo(ObjShellingAssign(minfo, ["DATA", "name"], ev.target.value))

                                }} />

                        </>
                    })
                }}>
                    儲存當前圖檔
                </Button>


                <Button onClick={() => {
                    setEditState(EditState.Test_Saved_Files);
                }}>測試儲存圖檔</Button>
            </>


            break;

        case EditState.Feature_Edit:


            EDIT_UI = <>
                <Popconfirm
                    key={"UIBack"}
                    title={`確定要更新？`}
                    onConfirm={() => { }}
                    onCancel={() => {

                        setEditState(EditState.Normal_Show)

                    }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            setCacheDef({ ...cacheDef, featureInfo: featureInfo })
                            setEditState(EditState.Normal_Show)

                        }
                    }}
                    okText={"Yes"}
                    cancelText="No"
                >
                    <Button danger type="primary" onClick={() => {

                    }}>{"<"}</Button>
                </Popconfirm>



                
                <Popconfirm key={"SAVE feat ref image " + updateC}
                        title={`確定要儲存此圖為特徵參考圖？ 再按:${delConfirmCounter + 1}次`}
                        onConfirm={() => { }}
                        onCancel={() => { }}
                        okButtonProps={{
                            danger: true, onClick: () => {
                                if (delConfirmCounter != 0) {
                                    setDelConfirmCounter(delConfirmCounter - 1);
                                }
                                else {
                                    (async () => {

                                        let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                                            type: "cache_image_save",
                                            folder_path: fsPath + "/",
                                            image_name: SBM_FEAT_REF_IMG_NAME,
                                        }) as any[];
                                        console.log(pkts,fsPath,SBM_FEAT_REF_IMG_NAME);

                                    })()
                                    setUpdateC(updateC + 1);
                                }
                            }
                        }}
                        okText={"Yes:" + delConfirmCounter}
                        cancelText="No"
                    >
                        <Button danger type="primary" onClick={() => {
                            setDelConfirmCounter(5);
                        }}>儲存最新圖片為特徵參考圖</Button>
                    </Popconfirm>


                特徵數:
                <InputNumber min={10} value={cacheDef.num_features}
                    onChange={(num) => {
                        setCacheDef({ ...cacheDef, num_features: num })
                    }} />


                圖像邊緣強度:
                <InputNumber value={featureInfo.strong_thresh}
                    onChange={(num) => {
                        setFeatureInfo({ ...featureInfo, strong_thresh: num })
                    }} />
                特徵強度:
                <InputNumber value={featureInfo.weak_thresh}
                    onChange={(num) => {
                        setFeatureInfo({ ...featureInfo, weak_thresh: num })
                    }} />

                <br />




                {
                    featureInfo.mask_regions === undefined ? null :
                        featureInfo.mask_regions.map((regi: any, idx: number) =>



                            <Popconfirm
                                key={"regi_del_" + idx + "..." + updateC}
                                title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                                onConfirm={() => { }}
                                onCancel={() => { }}
                                okButtonProps={{
                                    danger: true, onClick: () => {
                                        if (delConfirmCounter != 0) {
                                            setDelConfirmCounter(delConfirmCounter - 1);
                                        }
                                        else {
                                            let new_mask_regions = [...featureInfo.mask_regions];

                                            new_mask_regions.splice(idx, 1);

                                            setFeatureInfo({ ...featureInfo, mask_regions: new_mask_regions })

                                        }
                                    }
                                }}
                                okText={"Yes:" + delConfirmCounter}
                                cancelText="No"
                            >
                                <Button danger type="primary" onClick={() => {
                                    setDelConfirmCounter(3);
                                }}>{idx}</Button>
                            </Popconfirm>



                        )
                }

                <Button key={"AddNewFeat"} onClick={() => {



                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.sel_region_type = "region"
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {
                            console.log(info);

                            let x, y, w, h;

                            let roi_region = PtsToXYWH(info.pt1, info.pt2);
                            console.log(roi_region)
                            let regInfo = { ...roi_region, isBlackRegion: false };

                            let mask_regions = featureInfo.mask_regions === undefined ? [] : [...featureInfo.mask_regions];

                            mask_regions.push(regInfo);
                            setFeatureInfo({ ...featureInfo, mask_regions })
                            _this.sel_region_type = undefined;
                            // onDefChange(newRule)
                            if (_this.canvasComp == undefined) return;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>+特徵範圍</Button>


                <Button key={"_" + 10000} onClick={() => {


                (async () => {

                    let obj = {
                        type: "extract_feature",
                        image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                        num_features: cacheDef.num_features,
                        weak_thresh: featureInfo.weak_thresh,
                        strong_thresh: featureInfo.strong_thresh,
                        T: [2,2],
                        image_transfer_downsampling: -1,
                        mask_regions: featureInfo.mask_regions
                    }
                    console.log(obj)
                    let pkts = await BPG_API.InspTargetExchange(cacheDef.id, obj) as any[];
                    console.log(pkts);

                    let newFeatureInfo: any = {};
                    let newFeatureInfoExt: any = {};


                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM !== undefined) {
                        _this.featureImgCanvas.width = IM.image_info.width;
                        _this.featureImgCanvas.height = IM.image_info.height;

                        let ctx2nd = _this.featureImgCanvas.getContext('2d');
                        ctx2nd.putImageData(IM.image_info.image, 0, 0);
                        newFeatureInfoExt.IM = IM;

                    }


                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP !== undefined) {
                        newFeatureInfo.templatePyramid = RP.data;
                    }


                    setFeatureInfo({ ...featureInfo, ...newFeatureInfo })
                    setFeatureInfoExt({ ...featureInfoExt, ...newFeatureInfoExt })



                })()

                }}>生成特徵點</Button>



                <br />

                {
                    featureInfo.refine_match_regions === undefined ? null :
                        featureInfo.refine_match_regions.map((regi: any, idx: number) =>



                            <Popconfirm
                                key={"regi_del_" + idx + "..." + updateC}
                                title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                                onConfirm={() => { }}
                                onCancel={() => { }}
                                okButtonProps={{
                                    danger: true, onClick: () => {
                                        if (delConfirmCounter != 0) {
                                            setDelConfirmCounter(delConfirmCounter - 1);
                                        }
                                        else {
                                            let new_refine_match_regions = [...featureInfo.refine_match_regions];

                                            new_refine_match_regions.splice(idx, 1);

                                            setFeatureInfo({ ...featureInfo, refine_match_regions: new_refine_match_regions })

                                        }
                                    }
                                }}
                                okText={"Yes:" + delConfirmCounter}
                                cancelText="No"
                            >
                                <Button danger type="primary" onClick={() => {
                                    setDelConfirmCounter(3);
                                }}>{idx}</Button>
                            </Popconfirm>



                        )
                }

                <Button key={"AddRefineFeat"} onClick={() => {



                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.sel_region_type = "region"
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {
                            console.log(info);

                            let x, y, w, h;

                            let roi_region = PtsToXYWH(info.pt1, info.pt2);
                            console.log(roi_region)
                            let regInfo = { ...roi_region, isBlackRegion: false };

                            let refine_match_regions = featureInfo.refine_match_regions === undefined ? [] : [...featureInfo.refine_match_regions];

                            refine_match_regions.push(regInfo);
                            setFeatureInfo({ ...featureInfo, refine_match_regions })

                            _this.sel_region_type = undefined;
                            // onDefChange(newRule)
                            if (_this.canvasComp == undefined) return;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>+校位範圍</Button>


                <Switch checkedChildren="僅角度" unCheckedChildren="位置與角度" checked={featureInfo.refine_angle_only == true} onChange={(check) => {
                    setFeatureInfo({ ...featureInfo, refine_angle_only: check })
                }} />

                <br />
                <Button key={"AddAnchor"} onClick={() => {



                    _this.sel_region_type = "vector"
                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {
                            _this.sel_region_type = undefined;
                            console.log(info)
                            if (info.pt1.x == info.pt2.x && info.pt1.y == info.pt2.y) {
                                setFeatureInfo({ ...featureInfo, origin_info: undefined });
                            }
                            else {
                                setFeatureInfo({
                                    ...featureInfo, origin_info:
                                    {
                                        pt: info.pt1,
                                        vec: {
                                            x: info.pt2.x - info.pt1.x,
                                            y: info.pt2.y - info.pt1.y
                                        }
                                    }
                                });
                            }
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>設定中心與方向</Button>
            </>

            break;




        case EditState.Search_Region_Edit:


            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>

                <Button type="primary" onClick={() => {

                    onCacheDefChange(cacheDef, false);
                    BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
                }}>驗證</Button>

                計算縮放:
                <InputNumber value={cacheDef.matching_downScale} step={0.05}
                    onChange={(num) => {

                        setCacheDef({ ...cacheDef, matching_downScale: num })
                    }} />


                相似度:
                <InputNumber value={cacheDef.similarity_thres}
                    onChange={(num) => {

                        setCacheDef({ ...cacheDef, similarity_thres: num })
                    }} />


                邊緣強度:
                <InputNumber value={cacheDef.magnitude_thres}
                    onChange={(num) => {
                        setCacheDef({ ...cacheDef, magnitude_thres: num })
                    }} />




                角度:
                <InputNumber min={-360} max={360} step={0.5} value={cacheDef.featureInfo.match_front_face_angle_range[0]}
                    onChange={(num) => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["featureInfo", "match_front_face_angle_range", 0], num));
                    }} />
                ~
                <InputNumber min={-360} max={360} step={0.5} value={cacheDef.featureInfo.match_front_face_angle_range[1]}
                    onChange={(num) => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["featureInfo", "match_front_face_angle_range", 1], num));
                    }} />

                <br />
                校位下限(0~1):
                <InputNumber min={0} step={0.05} max={1} value={cacheDef.refine_score_thres}
                    onChange={(num) => {
                        setCacheDef({ ...cacheDef, refine_score_thres: num })
                    }} />


                <Switch checkedChildren="強制" unCheckedChildren="盡力" checked={cacheDef.must_refine_result == true} onChange={(check) => {
                    setCacheDef({ ...cacheDef, must_refine_result: check })
                }} />

                <Switch checkedChildren="剔除" unCheckedChildren="保留" checked={cacheDef.remove_refine_failed_result == true} onChange={(check) => {
                    setCacheDef({ ...cacheDef, remove_refine_failed_result: check })
                }} />

                <Switch checkedChildren="區域最似" unCheckedChildren="區域全部" checked={cacheDef.regional_most_similar_match == true} onChange={(check) => {
                    setCacheDef({ ...cacheDef, regional_most_similar_match: check })
                }} />
                <br />

                {
                    cacheDef.search_regions === undefined ? null :
                        cacheDef.search_regions.map((regi: any, idx: number) =>



                            <Popconfirm
                                key={"regi_del_" + idx + "..." + updateC}
                                title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                                onConfirm={() => { }}
                                onCancel={() => { }}
                                okButtonProps={{
                                    danger: true, onClick: () => {
                                        if (delConfirmCounter != 0) {
                                            setDelConfirmCounter(delConfirmCounter - 1);
                                        }
                                        else {
                                            let new_search_regions = [...cacheDef.search_regions];

                                            new_search_regions.splice(idx, 1);

                                            setCacheDef({ ...cacheDef, search_regions: new_search_regions })
                                            setUpdateC(updateC + 1);
                                        }
                                    }
                                }}
                                okText={"Yes:" + delConfirmCounter}
                                cancelText="No"
                            >
                                <Button danger type="primary" onClick={() => {
                                    setDelConfirmCounter(3);
                                }}>{idx}</Button>
                            </Popconfirm>



                        )
                }

                <Button key={"AddNewRegion"} onClick={() => {



                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.sel_region_type = "region"
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {
                            console.log(info);

                            let x, y, w, h;

                            let roi_region = PtsToXYWH(info.pt1, info.pt2);
                            console.log(roi_region)
                            let regInfo = { ...roi_region, isBlackRegion: false };

                            let search_regions = cacheDef.search_regions === undefined ? [] : [...cacheDef.search_regions];

                            search_regions.push(regInfo);
                            setCacheDef({ ...cacheDef, search_regions })

                            _this.sel_region_type = undefined;
                            // onDefChange(newRule)
                            if (_this.canvasComp == undefined) return;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>+搜尋範圍</Button>


            </>
            break;




        case EditState.Test_Saved_Files: {

            let folderPath = cacheDef.testInputFolder || fsPath;
            let result_InspTar_stream_id = 51001;//HACK hard coded
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>
                <TestInputSelectUI testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
        } break;




    }

    if (APIExport !== undefined)//keeps update for every state change
    {
        APIExport({
            onMouseClick: (callback: any) => {
                setOnMouseClick({ callback })
            },
            setDrawHook: (hook:any) => {
                _this.extDrawHook=hook;
            },
            getLatestReport: () => {
                return defReport;
            },

            defInfo: def,
            latest_RP: defReport,
            latest_IMCM: Local_IMCM,
            
        })
    }



    return <div style={{ ...style}} className={"overlayCon"}>

        <div className={"overlay"} style={{ width: "100%" }}>

            {EDIT_UI}

        </div>

        <Modal
            title={modalInfo.title}
            visible={modalInfo.visible}
            onOk={() => modalInfo.onOK(modalInfo)}
            // confirmLoading={confirmLoading}
            onCancel={() => modalInfo.onCancel(modalInfo)}
        >
            {modalInfo.visible ? modalInfo.contentCB(modalInfo) : null}
        </Modal>

        <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            _this.canvasComp = canvas_obj;
            // console.log(ctrl_or_draw);
            if(_this.extDrawHook!==undefined && _this.extDrawHook.preDraw!==undefined)
            {
                _this.extDrawHook.preDraw(ctrl_or_draw, g, canvas_obj);
            }
            let ctx = g.ctx;
            let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);

            let camMag = canvas_obj.camera.GetCameraScale();
            if (ctrl_or_draw == true)//ctrl
            {
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }

                    let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
                    let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);

                    _this.sel_region =
                    {
                        ...PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2),
                        pt1, pt2
                    };

                }

                // const imageData = ctx.getImageData(g.mouseStatus.x, g.mouseStatus.y, 1, 1);
                // // 
                // _this.fetchedPixInfo = imageData;
            }
            if (editState == EditState.Normal_Show || editState == EditState.Search_Region_Edit || editState == EditState.Test_Saved_Files) {

                if (ctrl_or_draw == true)//ctrl
                {
                    if (onMouseClick !== undefined && (g.mouseStatus.status == 1 && g.mouseEdge)) {
                        console.log(onMouseClick);
                        // let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
                        let cb = onMouseClick.callback;
                        setOnMouseClick(undefined)
                        cb(mouseOnCanvas);
                    }
                }
                else//draw
                {



                    if (Local_IMCM !== undefined) {
                        g.ctx.save();
                        let scale = Local_IMCM.image_info.scale;
                        g.ctx.scale(scale, scale);
                        g.ctx.translate(-0.5, -0.5);
                        g.ctx.drawImage(_this.imgCanvas, 0, 0);
                        g.ctx.restore();
                    }

                    
                    if (defReport !== undefined) {
                        // console.log(defReport)
                        defReport.report.forEach((match: any, idx: number) => {

                            if (match.confidence <= 0) return;
                            let angle = match.angle;

                            // let distance=Math.abs(match.center.x-mouseOnCanvas.x)+Math.abs(match.center.y-mouseOnCanvas.y)
                            // ctx.font = (100*Math.pow(1000/(distance+1000),5)+10)+"px Arial";
                            ctx.font = "50px Arial";

                            ctx.fillStyle = 'hsl('+ Math.floor(idx/10)*100 +',100%,50%)';
                            ctx.strokeStyle = 'black';
                            let text="[" + idx+"]";
                            ctx.fillText(text, match.center.x, match.center.y - 40)
                            ctx.lineWidth = 2;
                            ctx.strokeText(text, match.center.x, match.center.y - 40)
                            ctx.fillStyle = "rgba(150,100, 100,0.8)";



                            ctx.font = "20px Arial";
                            ctx.fillText("ang:" + (angle * 180 / 3.14159).toFixed(2)+(match.flip?" 反 ":""), match.center.x, match.center.y - 20)

                            if (match.confidence !== undefined)
                                ctx.fillText("sim:" + match.confidence.toFixed(3), match.center.x, match.center.y - 0)


                            ctx.lineWidth = 4 / camMag;
                            ctx.strokeStyle = `HSLA(0, 100%, 50%,1)`;
                            canvas_obj.rUtil.drawCross(ctx, { x: match.center.x, y: match.center.y }, 12 / camMag);



                            ctx.lineWidth = 4 / camMag;
                            let vec = PtRotate2d({ x: 100, y: 0 }, angle, 1);
                            canvas_obj.rUtil.drawLine(ctx, { x1: match.center.x, y1: match.center.y, x2: match.center.x + vec.x, y2: match.center.y + vec.y })

                        })

                        {
                            ctx.save();
                            ctx.resetTransform();
                            ctx.font = "20px Arial";
                            ctx.fillStyle = "rgba(150,100, 100,0.5)";
                            ctx.fillText("ProcessTime:" + (defReport.process_time_us / 1000).toFixed(2) + " ms", 20, 400)
                            ctx.restore();
                        }
                    }
                    // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))
                    try {

                        if (cacheDef.search_regions !== undefined) {
                            cacheDef.search_regions.forEach((regi: any, idx: number) => {
                                ctx.strokeStyle = "rgba(150,50, 50,0.8)";
                                if (defReport && defReport.report[idx] !== undefined) {
                                    if (defReport.report[idx].confidence >= 0)
                                        ctx.strokeStyle = "rgba(50,150, 50,0.8)";

                                }


                                drawRegion(g, canvas_obj, { x: regi.x, y: regi.y, w: regi.w, h: regi.h }, canvas_obj.rUtil.getIndicationLineSize(), false);
                                ctx.font = "20px Arial";
                                ctx.fillStyle = "rgba(50,150, 50,0.8)";
                                ctx.fillText("idx:" + idx, regi.x, regi.y)

                            })
                        }
                    }
                    catch (e) {

                    }


                }

            }

            if (editState == EditState.Feature_Edit) {
                if (ctrl_or_draw == true)//ctrl
                {
                }
                else//draw
                {

                    let camMag = canvas_obj.camera.GetCameraScale();
                    if (featureInfoExt.IM !== undefined) {
                        g.ctx.save();
                        let scale = featureInfoExt.IM.image_info.scale;
                        g.ctx.scale(scale, scale);
                        g.ctx.translate(-0.5, -0.5);
                        g.ctx.drawImage(_this.featureImgCanvas, 0, 0);
                        g.ctx.restore();
                    }


                    if (featureInfo.templatePyramid !== undefined) {
                        if (canvas_obj.regionSelect === undefined)//when in region select, hide the template info
                        {

                            let mult = 1;
                            for (let i = 0; i < featureInfo.templatePyramid.length; i++, mult *= 2) {
                                let template = featureInfo.templatePyramid[i]
                                template.features.forEach((temp_pt: any) => {
                                    // ctx.strokeStyle = "rgba(255, 0, 0,1)";
                                    ctx.lineWidth = 4 / mult/camMag;
                                    ctx.strokeStyle = `HSLA(${300 * i / featureInfo.templatePyramid.length}, 100%, 50%,1)`;
                                    let X=(temp_pt.x + template.tl_x) * mult;
                                    let Y=(temp_pt.y + template.tl_y) * mult;
                                    canvas_obj.rUtil.drawCross(ctx, { x:X, y:Y}, 12 / mult/camMag);
                                    
                                    let ptheta=temp_pt.theta*Math.PI/180;
                                    let dirMag=50/camMag;
                                    if(dirMag>10)dirMag=10;
                                    let vX=Math.cos(ptheta)*dirMag;
                                    let vY=Math.sin(ptheta)*dirMag;

                                    canvas_obj.rUtil.drawLine(ctx, {
                                        x1: X-vX,
                                        y1: Y-vY,
                                        x2: X+vX,
                                        y2: Y+vY
                                    })

                                })
                            }

                        }
                        if (featureInfo.mask_regions !== undefined) {
                            featureInfo.mask_regions.forEach((regi: any, idx: number) => {

                                ctx.strokeStyle =
                                    ctx.fillStyle = "rgba(150,100, 100,0.8)";


                                drawRegion(g, canvas_obj, { x: regi.x, y: regi.y, w: regi.w, h: regi.h }, canvas_obj.rUtil.getIndicationLineSize());
                                let fontSize_eq = 40 / camMag;
                                if (fontSize_eq > 40) fontSize_eq = 40;
                                ctx.font = (fontSize_eq) + "px Arial";
                                ctx.fillText("idx:" + idx, regi.x, regi.y)

                            })
                        }


                    }

                    if (featureInfo.refine_match_regions !== undefined) {
                        if (featureInfo.refine_match_regions !== undefined) {
                            featureInfo.refine_match_regions.forEach((regi: any, idx: number) => {

                                ctx.fillStyle =
                                    ctx.strokeStyle = "rgba(100,100, 200,0.8)";


                                drawRegion(g, canvas_obj, { x: regi.x, y: regi.y, w: regi.w, h: regi.h }, canvas_obj.rUtil.getIndicationLineSize());
                                let fontSize_eq = 40 / camMag;
                                if (fontSize_eq > 40) fontSize_eq = 40;
                                ctx.font = (fontSize_eq) + "px Arial";
                                ctx.fillText("idx:" + idx, regi.x, regi.y)

                            })
                        }
                    }





                    if (featureInfo.origin_info !== undefined) {

                        ctx.setLineDash([0, 0, 0, 0]);
                        let oriInfo = featureInfo.origin_info;
                        canvas_obj.rUtil.drawCross(ctx, { x: oriInfo.pt.x, y: oriInfo.pt.y }, 10/camMag);

                        canvas_obj.rUtil.drawLine(ctx, {
                            x1: oriInfo.pt.x,
                            y1: oriInfo.pt.y,
                            x2: oriInfo.pt.x + oriInfo.vec.x,
                            y2: oriInfo.pt.y + oriInfo.vec.y
                        })
                    }



                }
            }


            if (ctrl_or_draw == false) {
                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    if (_this.sel_region_type == "region") {
                        drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());
                    }
                    if (_this.sel_region_type == "vector") {
                        canvas_obj.rUtil.drawCross(ctx, { x: _this.sel_region.pt1.x, y: _this.sel_region.pt1.y }, 10/camMag);

                        ctx.setLineDash([0, 0, 0, 0]);
                        canvas_obj.rUtil.drawLine(ctx, {
                            x1: _this.sel_region.pt1.x,
                            y1: _this.sel_region.pt1.y,
                            x2: _this.sel_region.pt2.x,
                            y2: _this.sel_region.pt2.y
                        })

                    }
                }

                // if (_this.fetchedPixInfo !== undefined) {
                //     ctx.save();
                //     ctx.resetTransform();
                //     // console.log(_this.fetchedPixInfo)
                //     let pixInfo = _this.fetchedPixInfo.data;
                //     ctx.font = "1.5em Arial";
                //     ctx.fillStyle = "rgba(250,100, 50,1)";

                //     ctx.fillText(rgb2hsv(pixInfo[0], pixInfo[1], pixInfo[2]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)
                //     ctx.restore();
                // }

                {
                    ctx.save();
                    ctx.resetTransform();
                    // console.log(_this.fetchedPixInfo)
                    // let pixInfo = _this.fetchedPixInfo.data;
                    ctx.font = "1.5em Arial";
                    ctx.fillStyle = "rgba(250,100, 50,1)";


                    // console.log(mouseOnCanvas)
                    ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`, g.mouseStatus.x, g.mouseStatus.y)
                    ctx.restore();
                }
            }




            if(_this.extDrawHook!==undefined && _this.extDrawHook.postDraw!==undefined)
            {
                _this.extDrawHook.postDraw(ctrl_or_draw, g, canvas_obj);
            }
            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }
        }
        } />

    </div>;

}


export function SingleTargetVIEWUI_Orientation_ColorRegionOval(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange } = props;
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        drawHooks: [],
        ctrlHooks: []


    });
    const [cacheDef, setCacheDef] = useState<any>(def);
    const [cameraQueryList, setCameraQueryList] = useState<any[] | undefined>([]);


    const [defReport, setDefReport] = useState<any>(undefined);
    const [forceUpdateCounter, setForceUpdateCounter] = useState(0);
    let _this = _.current;
    let c_report: any = undefined;
    if (_this.cache_report !== report) {
        if (report !== undefined) {
            _this.cache_report = report;
        }
    }
    c_report = _this.cache_report;


    useEffect(() => {
        console.log("fsPath:" + fsPath)
        _this.cache_report = undefined;
        setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);
    // console.log(IMCM_group,report);
    // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
    // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);


    enum editState {
        Normal_Show = 0,
        Region_Edit = 1,
    }

    const [stateInfo, setStateInfo] = useState<{ st: editState, info: any }[]>([{
        st: editState.Normal_Show,
        info: undefined
    }]);


    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [CNC_API, setCNC_API] = useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);


    const [queryCameraList, setQueryCameraList] = useState<any[] | undefined>(undefined);
    const [delConfirmCounter, setDelConfirmCounter] = useState(0);


    let stateInfo_tail = stateInfo[stateInfo.length - 1];



    function onCacheDefChange(updatedDef: any, ddd: boolean) {
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            console.log(">>>");
            await BPG_API.InspTargetUpdate(updatedDef)

        })()

        BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });

        onDefChange(updatedDef, ddd);
    }


    useEffect(() => {//////////////////////

        let cbsKey="_"+Math.random();
        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id,cbsKey, {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    console.log("++++++++\n", IM, CM, RP);


                    setDefReport(RP.data)
                    let IMCM = {
                        image_info: IM.image_info,
                        camera_id: CM.data.camera_id,
                        trigger_id: CM.data.trigger_id,
                        trigger_tag: CM.data.trigger_tag,
                    } as type_IMCM

                    _this.imgCanvas.width = IMCM.image_info.width;
                    _this.imgCanvas.height = IMCM.image_info.height;

                    let ctx2nd = _this.imgCanvas.getContext('2d');

                    if(IMCM.image_info.image instanceof ImageData)
                        ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
                    else if(IMCM.image_info.image instanceof HTMLImageElement)
                        ctx2nd.drawImage(IMCM.image_info.image, 0, 0);

                    setLocal_IMCM(IMCM)
                    // console.log(IMCM)

                },
                reject: (pkts) => {

                }
            }

            )

        })()
        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);

                // await BPG_API.InspTargetSetStreamChannelID(
                //   cacheDef.id,0,
                //   {
                //     resolve:(pkts)=>{
                //     },
                //     reject:(pkts)=>{

                //     }
                //   }
                // )
            })()

        })
    }, []);
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


    if (display == false) return null;


    let EDIT_UI = null;

    switch (stateInfo_tail.st) {

        case editState.Normal_Show:


            let EditUI = null;
            if ((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) != 0)//allow edit
            {
                EditUI = <>

                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        onCacheDefChange(newDef, ddd);
                    }} />
                    <Button key={"_" + 10000} onClick={() => {

                        let newDef = { ...cacheDef };
                        newDef.regionInfo.push({ region: [0, 0, 0, 0], colorThres: 10 });
                        onCacheDefChange(newDef, false)


                        setStateInfo([...stateInfo, {
                            st: editState.Region_Edit,
                            info: {
                                idx: newDef.regionInfo.length - 1
                            }
                        }])

                    }}>+</Button>



                    {cacheDef.regionInfo.map((region: any, idx: number) => {
                        return <Button key={"_" + idx} onClick={() => {
                            if (_this.canvasComp === undefined) return;


                            setStateInfo([...stateInfo, {
                                st: editState.Region_Edit,
                                info: {
                                    idx: idx
                                }
                            }])




                        }}>{"idx:" + idx}</Button>
                    })}
                </>
            }

            EDIT_UI = <>

                <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
                {/* <Input maxLength={100} value={cacheDef.type} disabled
                    style={{ width: "100px" }}
                    onChange={(e) => {

                    }} /> */}

                <Input maxLength={100} value={cacheDef.sampleImageFolder} disabled
                    style={{ width: "100px" }}
                    onChange={(e) => {
                    }} />


                <Dropdown
                    overlay={<>
                        <Menu>
                            {
                                queryCameraList === undefined ?
                                    <Menu.Item disabled danger>
                                        <a target="_blank" rel="noopener noreferrer" href="https://www.antgroup.com">
                                            Press to update
                                        </a>
                                    </Menu.Item>
                                    :
                                    queryCameraList.map(cam => <Menu.Item key={cam.id}
                                        onClick={() => {
                                            let newDef = { ...cacheDef };
                                            newDef.camera_id = cam.id;
                                            // HACK_do_Camera_Check=true;
                                            onCacheDefChange(newDef, true)
                                        }}>
                                        {cam.id}
                                    </Menu.Item>)

                            }
                        </Menu>
                    </>}
                >
                    <Button onClick={() => {
                        // queryCameraList
                        setQueryCameraList(undefined);
                        BPG_API.queryDiscoverList()
                            .then((e: any) => {
                                console.log(e);
                                setQueryCameraList(e[0].data)
                            })
                        // let api=await getAPI(CORE_ID) as BPG_WS;
                        // let cameraListInfos=await api.cameraDiscovery() as any[];
                        // let CM=cameraListInfos.find(info=>info.type=="CM")
                        // if(CM===undefined)throw "CM not found"
                        // console.log(CM.data);
                        // return CM.data as {name:string,id:string,driver_name:string}[];

                    }}>{cacheDef.camera_id}</Button>
                </Dropdown>



                <Input maxLength={100} value={cacheDef.trigger_tag}
                    style={{ width: "100px" }}
                    onChange={(e) => {
                        let newDef = { ...cacheDef };
                        newDef.trigger_tag = e.target.value;
                        onCacheDefChange(newDef, false)
                    }} />

                <Popconfirm
                    title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                    onConfirm={() => { }}
                    onCancel={() => { }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            if (delConfirmCounter != 0) {
                                setDelConfirmCounter(delConfirmCounter - 1);
                            }
                            else {
                                onCacheDefChange(undefined, false)
                            }
                        }
                    }}
                    okText={"Yes:" + delConfirmCounter}
                    cancelText="No"
                >
                    <Button danger type="primary" onClick={() => {
                        setDelConfirmCounter(5);
                    }}>DEL</Button>
                </Popconfirm>
                <br />
                <Button onClick={() => {
                    onCacheDefChange(cacheDef, true);
                }}>SHOT</Button>


                {/* <Button onClick={() => {
                    onDefChange(cacheDef, true)
                }}>SAVE</Button> */}

                {EditUI}


            </>


            break;

        case editState.Region_Edit:


            if (cacheDef.regionInfo.length <= stateInfo_tail.info.idx) {
                break;
            }

            let regionInfo = cacheDef.regionInfo[stateInfo_tail.info.idx];

            EDIT_UI = <>
                <Button key={"_" + -1} onClick={() => {

                    let new_stateInfo = [...stateInfo]
                    new_stateInfo.pop();

                    setStateInfo(new_stateInfo)
                }}>{"<"}</Button>
                <Orientation_ColorRegionOval_SingleRegion
                    srule={regionInfo}
                    onDefChange={(newDef_sregion) => {
                        // console.log(newDef);
                        let newDef = { ...cacheDef };
                        if (newDef_sregion !== undefined) {
                            newDef.regionInfo[stateInfo_tail.info.idx] = newDef_sregion;
                        }
                        else {

                            newDef.regionInfo.splice(stateInfo_tail.info.idx, 1);

                            let new_stateInfo = [...stateInfo]
                            new_stateInfo.pop();

                            setStateInfo(new_stateInfo)

                        }

                        onCacheDefChange(newDef, true)
                        // _this.sel_region=undefined

                    }}
                    canvas_obj={_this.canvasComp} />
            </>

            break;
    }



    return <div style={{ ...style}} className={"overlayCon"}>

        <div className={"overlay"} >

            {EDIT_UI}

        </div>


        <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            _this.canvasComp = canvas_obj;
            // console.log(ctrl_or_draw);
            if (ctrl_or_draw == true)//ctrl
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
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }

                    _this.sel_region = PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2)
                }
            }
            else//draw
            {
                if (Local_IMCM !== undefined) {
                    g.ctx.save();
                    let scale = Local_IMCM.image_info.scale;
                    g.ctx.scale(scale, scale);
                    g.ctx.translate(-0.5, -0.5);
                    g.ctx.drawImage(_this.imgCanvas, 0, 0);
                    g.ctx.restore();
                }
                // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))


                let ctx = g.ctx;

                {
                    cacheDef.regionInfo.forEach((region: any, idx: number) => {

                        let region_ROI =
                        {
                            x: region.region[0],
                            y: region.region[1],
                            w: region.region[2],
                            h: region.region[3]
                        }

                        ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                        drawRegion(g, canvas_obj, region_ROI, canvas_obj.rUtil.getIndicationLineSize());

                        ctx.font = "40px Arial";
                        ctx.fillStyle = "rgba(0, 179, 0,0.5)";
                        ctx.fillText("idx:" + idx, region_ROI.x, region_ROI.y)


                        let region_components = GetObjElement(c_report, ["regionInfo", idx, "components"]);
                        // console.log(report,region_components);
                        if (region_components !== undefined) {
                            region_components.forEach((regComp: any) => {

                                canvas_obj.rUtil.drawCross(ctx, { x: regComp.x, y: regComp.y }, 5);

                                ctx.font = "4px Arial";
                                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                                ctx.fillText(regComp.area, regComp.x, regComp.y)

                                ctx.font = "4px Arial";
                                ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                                ctx.fillText(`${regComp.x},${regComp.y}`, regComp.x, regComp.y + 5)
                            })

                        }



                    })
                }


                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());

                }

                if (defReport) {

                    ctx.strokeStyle = "rgba(255,0,100,0.5)";
                    defReport.report.forEach((reg: any) => {
                        if (reg.center === undefined || reg.angle === undefined) return;
                        canvas_obj.rUtil.drawCross(ctx, { x: reg.center.x, y: reg.center.y }, 5);
                        let angle = reg.angle;
                        if (angle > Math.PI / 2) angle -= Math.PI;
                        let vec = PtRotate2d({ x: 30, y: 0 }, angle, 1);
                        canvas_obj.rUtil.drawLine(ctx, { x1: reg.center.x, y1: reg.center.y, x2: reg.center.x + vec.x, y2: reg.center.y + vec.y })
                        // console.log(reg,canvas_obj)
                    })
                }
            }


            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }
        }
        } />

    </div>;

}

const SCS_REF_IMG_NAME = "FeatureRefImage.png"

function SurfaceCheckSimple_RefImg_EDIT_UI({ BPG_API, fsPath, def, onDefChange, onFinish, canvas_obj, canvas_hook_update }:
    {
        BPG_API: BPG_WS,
        fsPath: string,
        def: any,
        onDefChange: (...param: any) => void,
        onFinish: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent,
        canvas_hook_update: (cb: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => any) | undefined) => any
    }) {
    const _this = useRef<any>({
        featureImgCanvas: document.createElement('canvas'),
        featureInfoExt: {}
    }).current;
    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    const [updateC, setUpdateC] = useState(0);
    const [extractedRGB, setExtractedRGB] = useState({ R: NaN, G: NaN, B: NaN });



    async function updateRefInfo(def: any, doUpdateImage: boolean = false) {

        let pkts = await BPG_API.InspTargetExchange(def.id, {
            type: "extract_feature",
            image_transfer_downsampling: doUpdateImage ? 1 : -1,
            image_path: fsPath + "/" + SCS_REF_IMG_NAME,
            colorExtractInfo: _this.def_Filled.majorColorBalancing
        }) as any[];

        let newFeatureInfoExt: any = {};


        let IM = pkts.find((p: any) => p.type == "IM");
        if (IM !== undefined) {
            _this.featureImgCanvas.width = IM.image_info.width;
            _this.featureImgCanvas.height = IM.image_info.height;

            let ctx2nd = _this.featureImgCanvas.getContext('2d');
            ctx2nd.putImageData(IM.image_info.image, 0, 0);
            newFeatureInfoExt.IM = IM;

        }

        newFeatureInfoExt.RP = undefined;
        let RP = pkts.find((p: any) => p.type == "RP");
        if (RP !== undefined) {
            newFeatureInfoExt.RP = RP;

            // onDefChange(_this.def_Filled);
            setExtractedRGB(RP.data.report);

        }
        console.log(newFeatureInfoExt);


        _this.featureInfoExt = { ..._this.featureInfoExt, ...newFeatureInfoExt }


        setUpdateC(updateC + 1)
    }
    // _this.extractedRGB=extractedRGB;
    _this.def_Filled = {

        blackRegions: [],
        ...def
    }

    _this.def_Filled.majorColorBalancing = {
        enable: false,
        refRegions: [],
        refRGB: { R: NaN, G: NaN, B: NaN },
        ...def.majorColorBalancing

    };




    useEffect(() => {


        // updateRefInfo(_this.def_Filled,true);

        canvas_hook_update((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            if (ctrl_or_draw == true) {
                return;
            }

            let ctx = g.ctx;




            if (_this.featureInfoExt.IM !== undefined) {
                g.ctx.save();
                let scale = _this.featureInfoExt.IM.image_info.scale;
                g.ctx.scale(scale, scale);
                g.ctx.translate(-0.5, -0.5);
                g.ctx.drawImage(_this.featureImgCanvas, 0, 0);
                g.ctx.restore();
            }



            if (canvas_obj.regionSelect !== undefined &&
                canvas_obj.regionSelect.pt1 !== undefined &&
                canvas_obj.regionSelect.pt2 !== undefined) {
                ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                let roi_region = PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2);
                drawRegion(g, canvas_obj, roi_region, canvas_obj.rUtil.getIndicationLineSize());

            }
            else {
                _this.def_Filled.majorColorBalancing.refRegions.forEach((region: { x: number, y: number, w: number, h: number }) => {
                    ctx.strokeStyle = "rgba(0, 179, 0,0.5)";
                    drawRegion(g, canvas_obj, region, canvas_obj.rUtil.getIndicationLineSize());
                })


                _this.def_Filled.blackRegions.forEach((region: { x: number, y: number, w: number, h: number }) => {
                    ctx.strokeStyle = "rgba(50, 10, 10,0.8)";
                    drawRegion(g, canvas_obj, region, canvas_obj.rUtil.getIndicationLineSize());
                })
            }


        })


        return (() => {
            canvas_hook_update(undefined)
        });

    }, []);

    useEffect(() => {
        updateRefInfo(_this.def_Filled,
            (_this.featureInfoExt.IM === undefined) ? true : false);
    }, [def]);
    console.log(_this.def_Filled);
    return <>


        <Button danger onClick={() => {
            _this.def_Filled.majorColorBalancing.refRGB = extractedRGB;
            onDefChange(_this.def_Filled);
            onFinish();

        }}>{"<"}</Button>



        <Switch checkedChildren="使用" unCheckedChildren="不使用" checked={_this.def_Filled.majorColorBalancing.enable == true} onChange={(check) => {


            _this.def_Filled.majorColorBalancing.enable = check;
            onDefChange(_this.def_Filled);


        }} />


        區域色彩校正



        <br />

        參考色彩區域:
        {
            _this.def_Filled.majorColorBalancing.refRegions.map((regi: any, idx: number) =>



                <Popconfirm
                    key={"regi_del_" + idx + "..." + updateC}
                    title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                    onConfirm={() => { }}
                    onCancel={() => { }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            if (delConfirmCounter != 0) {
                                setDelConfirmCounter(delConfirmCounter - 1);
                            }
                            else {
                                let new_ref_regions = [..._this.def_Filled.majorColorBalancing.refRegions];

                                new_ref_regions.splice(idx, 1);


                                _this.def_Filled.majorColorBalancing.refRegions = new_ref_regions;

                                // setFeatureInfo({ ...featureInfo, mask_regions })

                                onDefChange(_this.def_Filled);

                            }
                        }
                    }}
                    okText={"Yes:" + delConfirmCounter}
                    cancelText="No"
                >
                    <Button danger type="primary" onClick={() => {
                        setDelConfirmCounter(3);
                    }}>{idx}</Button>
                </Popconfirm>



            )
        }





        <Button danger type="primary" onClick={() => {

            canvas_obj.UserRegionSelect((info, state) => {
                if (state == 2) {
                    console.log(info);

                    let roi_region = PtsToXYWH(info.pt1, info.pt2);
                    console.log(roi_region)

                    _this.def_Filled.majorColorBalancing.refRegions =
                        [..._this.def_Filled.majorColorBalancing.refRegions, roi_region];

                    // setFeatureInfo({ ...featureInfo, mask_regions })

                    onDefChange(_this.def_Filled);
                    canvas_obj.UserRegionSelect(undefined)

                }
            })



        }}>+</Button>
        <br />
        {JSON.stringify(extractedRGB)}

        <br />

        忽略區域:

        {
            _this.def_Filled.blackRegions.map((regi: any, idx: number) =>



                <Popconfirm
                    key={"regi_del_" + idx + "..." + updateC}
                    title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                    onConfirm={() => { }}
                    onCancel={() => { }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            if (delConfirmCounter != 0) {
                                setDelConfirmCounter(delConfirmCounter - 1);
                            }
                            else {
                                let new_ref_regions = [..._this.def_Filled.blackRegions];

                                new_ref_regions.splice(idx, 1);


                                _this.def_Filled.blackRegions = new_ref_regions;

                                // setFeatureInfo({ ...featureInfo, mask_regions })

                                onDefChange(_this.def_Filled);

                            }
                        }
                    }}
                    okText={"Yes:" + delConfirmCounter}
                    cancelText="No"
                >
                    <Button danger type="primary" onClick={() => {
                        setDelConfirmCounter(3);
                    }}>{idx}</Button>
                </Popconfirm>



            )
        }





        <Button danger type="primary" onClick={() => {

            canvas_obj.UserRegionSelect((info, state) => {
                if (state == 2) {
                    console.log(info);

                    let roi_region = PtsToXYWH(info.pt1, info.pt2);
                    console.log(roi_region)

                    _this.def_Filled.blackRegions =
                        [..._this.def_Filled.blackRegions, roi_region];

                    // setFeatureInfo({ ...featureInfo, mask_regions })

                    onDefChange(_this.def_Filled);
                    canvas_obj.UserRegionSelect(undefined)

                }
            })



        }}>+</Button>
    </>





}


function SurfaceCheckSimple_SubRegion_EDIT_UI({ BPG_API, fsPath, id, pxSize, def, onDefChange, onCopy, onFinish, canvas_obj, canvas_hook_update }:
    {
        BPG_API: BPG_WS,
        fsPath: string,
        id: string,
        pxSize: number,
        def: any,
        onDefChange: (...param: any) => void,
        onFinish: (...param: any) => void,
        onCopy: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent,
        canvas_hook_update: (cb: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, is_post_render: boolean) => any) | undefined) => any
    }) {

    const _this = useRef<any>({}).current;


    const [delConfirmCounter, setDelConfirmCounter] = useState(0);


    const [showDetectAdjUI, setShowDetectAdjUI] = useState(false);
    const [showDisplayAdjUI, setShowDisplayAdjUI] = useState(false);


    // const [namePosSettingInfo, setNamePosSettingInfo] = useState<{name:string,location:{x:number,y:number}}|undefined>(undefined);
    // console.log("??????",canvasDrawCB);
    canvas_hook_update((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, is_post_render: boolean) => {
        if (is_post_render == false) return false;

        let region = { x: def.region.x, y: def.region.y, w: def.region.w, h: def.region.h };
        // console.log(def)
        let lsz = canvas_obj.rUtil.getIndicationLineSize();
        let ctx=g.ctx;
        g.ctx.strokeStyle = "rgba(255, 200, 255,1)";
        drawRegion(g, canvas_obj, region, canvas_obj.rUtil.getIndicationLineSize());
        // console.log("<<<<><",_this.canvasDrawCB);



        {

            if (def.ignore_regions !== undefined) {
                def.ignore_regions.forEach((ig_region: any) => {

                    ctx.strokeStyle = "rgba(200,200, 200,0.8)";
                    ctx.fillStyle = "rgba(200,200, 200,0.2)";
                    let igr = { ...ig_region };
                    igr.x += def.region.x;
                    igr.y += def.region.y;
                    drawRegion(g, canvas_obj, igr, lsz / 2, false, []);
                    ctx.fill();

                    ctx.strokeStyle = "rgba(200,200, 200,0.8)";
                    ctx.beginPath();
                    ctx.moveTo(def.region.x, def.region.y);
                    ctx.lineTo(igr.x, igr.y);
                    ctx.stroke();
                })
            }
        }


        
        if(_this.canvasDrawCB!==undefined)
        {
            _this.canvasDrawCB();
        }
        // if(namePosSettingInfo!==undefined)
        // {
        //     let lsz = canvas_obj.rUtil.getIndicationLineSize();


        //     let ctx = g.ctx;
        //     ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,0.8)";

        //     ctx.fillStyle = ctx.strokeStyle;
        //     let lineHeight = 15;
        //     ctx.font = lineHeight + "px Arial";
        //     ctx.fillText(id, def.region.x+namePosSettingInfo.location.x, def.region.y+namePosSettingInfo.location.y);
        // }
    })

    useEffect(() => {


        return (() => {

            if (_this.is_in_overlay_disable == true) {
                BPG_API.InspTargetExchange(id,
                    {
                        type: "show_display_overlay",
                        enable: true
                    });
                onDefChange(def)
            }
            canvas_obj.UserRegionSelect(undefined);
            canvas_hook_update(undefined)

        });

    }, []);



    let def_Filled = {

        ignore_regions: [],
        type:"HSVSeg",
        NG_Map_To:"NG",
        ...def
    }


    let ConfigUI:JSX.Element=<></>;


    let ProcessDisplayUI=<>
    
    <Row>
        <Col span={4}>
            結果顯示:{def.resultOverlayAlpha * 100}%
        </Col>
        <Col span={6}>
            <Slider defaultValue={def.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v) => {

                _this.trigTO = ID_debounce(_this.trigTO, () => {
                    onDefChange(ObjShellingAssign(def, ["resultOverlayAlpha"], v));
                }, () => _this.trigTO = undefined, 500);


            }} />

        </Col>
        <Col span={14}>
            R
            <InputNumber value={def.overlayColor?.r}
                onChange={(num) => {
                    onDefChange(ObjShellingAssign(def, ["overlayColor", "r"], num));
                }} />
            G
            <InputNumber value={def.overlayColor?.g}
                onChange={(num) => {
                    onDefChange(ObjShellingAssign(def, ["overlayColor", "g"], num));
                }} />
            B:
            <InputNumber value={def.overlayColor?.b}
                onChange={(num) => {
                    onDefChange(ObjShellingAssign(def, ["overlayColor", "b"], num));
                }} />


        </Col>

    </Row>

    處理圖片顯示
    <Switch checkedChildren="顯示" unCheckedChildren="原圖" checked={def.show_processed_image == true} onChange={(check) => {
        onDefChange({ ...def, show_processed_image: check }, true);
    }} />

</>
    let HSVEditUI= <>
    
    



    
    
    偵測反轉
    <Switch checkedChildren="反轉" unCheckedChildren="正常" checked={def.invert_detection == true} onChange={(check) => {
        onDefChange({ ...def, invert_detection: check }, true);
    }} />



    背景差分
    <Switch checkedChildren="開" unCheckedChildren="無" checked={def.bg_diff == true} onChange={(check) => {
        onDefChange({ ...def, bg_diff: check }, true);
    }} />
    <br />

    {/* X ng鏡像
    <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def.x_flip_mark == true} onChange={(check) => {
        onDefChange({ ...def, x_flip_mark: check }, true);
    }} />
    Y ng鏡像
    <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def.y_flip_mark == true} onChange={(check) => {
        onDefChange({ ...def, y_flip_mark: check }, true);
    }} /> */}



    X定位
    <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def.x_locating_mark == true} onChange={(check) => {
        onDefChange({ ...def, x_locating_mark: check }, true);
    }} />
    X定位方向
    <Switch checkedChildren="方向1" unCheckedChildren="方向2" checked={def.x_locating_dir == true} onChange={(check) => {
        onDefChange({ ...def, x_locating_dir: check }, true);
    }} />
    Y定位
    <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def.y_locating_mark == true} onChange={(check) => {
        onDefChange({ ...def, y_locating_mark: check }, true);
    }} />
    Y定位方向
    <Switch checkedChildren="方向1" unCheckedChildren="方向2" checked={def.y_locating_dir == true} onChange={(check) => {
        onDefChange({ ...def, y_locating_dir: check }, true);
    }} />


    <br />

    色彩補償:

    <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def.color_compensation_enable == true} onChange={(check) => {
        onDefChange({ ...def, color_compensation_enable: check }, true);
    }} />

    {
        def.color_compensation_enable != true ? null : <>

            補償差異過大
            <Switch checkedChildren="NA" unCheckedChildren="NG" checked={def.color_compensation_diff_NG_as_NA == true} onChange={(check) => {
                onDefChange({ ...def, color_compensation_diff_NG_as_NA: check }, true);
            }} />
            <Button onClick={() => {
                if (_this.is_in_overlay_disable) {

                    BPG_API.InspTargetExchange(id,
                        {
                            type: "show_display_overlay",
                            enable: true
                        });

                    onDefChange(def)
                    _this.is_in_overlay_disable = false;
                    canvas_obj.UserRegionSelect(undefined);
                    return;
                }
                BPG_API.InspTargetExchange(id,
                    {
                        type: "show_display_overlay",
                        enable: false
                    });


                onDefChange(def)

                _this.is_in_overlay_disable = true;

                canvas_obj.UserRegionSelect((info, draggingState) => {
                    if (draggingState == 1) {
                    }
                    else if (draggingState == 2) {
                        console.log(info);



                        (async () => {
                            canvas_obj.UserRegionSelect(undefined);
                            let extColor = (await BPG_API.InspTargetExchange(id,
                                {
                                    type: "extract_color",
                                    region: PtsToXYWH(info.pt1, info.pt2)
                                }) as any)[0].data.report;



                            await BPG_API.InspTargetExchange(id,
                                {
                                    type: "show_display_overlay",
                                    enable: true
                                });

                            extColor.r = Math.round(extColor.r);
                            extColor.g = Math.round(extColor.g);
                            extColor.b = Math.round(extColor.b);
                            onDefChange({ ...def, color_compensation_target: extColor })
                            _this.is_in_overlay_disable = false;

                            console.log(extColor);
                        })();

                    }
                });
            }}>抽取色彩補償標的 </Button>

            {JSON.stringify(def.color_compensation_target)}

            <Row>
                <Col span={8}>
                    色彩補償差異閾值
                </Col>
                <Col span={14}>


                    <Slider defaultValue={def.color_compensation_diff_thres} max={255} onChange={(v) => {

                        _this.trigTO =
                            ID_debounce(_this.trigTO, () => {
                                onDefChange({ ...def, color_compensation_diff_thres: v });
                            }, () => _this.trigTO = undefined, 500);

                    }} />


                </Col>
            </Row>



        </>
    }



    銳化半徑
    <InputNumber value={def.sharpening_blurRad} step={1} min={0} max={40}
        onChange={(num) => {
            let newDef = { ...def, sharpening_blurRad: num }
            onDefChange(newDef, true);
        }} />

    銳化
    <InputNumber value={def.sharpening_alpha} step={1} min={0}
        onChange={(num) => {
            let newDef = { ...def, sharpening_alpha: num }
            onDefChange(newDef, true);
        }} />

    <Row>
        <Col span={2}>
            H[{def.rangel?.h}:{def.rangeh?.h}]
        </Col>
        <Col span={20}>
            {/* <Slider defaultValue={def.rangeh?.h} max={180} onChange={(v) => {

        _this.trigTO =
            ID_debounce(_this.trigTO, () => {
                let newL=ObjShellingAssign(def, ["rangeh", "h"], v)
                console.log(def,newL);
                onDefChange(newL);
            }, () => _this.trigTO = undefined, 500);

        }} />
        <Slider defaultValue={def.rangel?.h} max={180} onChange={(v) => {

        _this.trigTO =
            ID_debounce(_this.trigTO, () => {
                onDefChange(ObjShellingAssign(def, ["rangel", "h"], v));
            }, () => _this.trigTO = undefined, 500);

        }} /> */}


            <Slider
                range
                step={1} max={180}
                defaultValue={[def.rangel?.h, def.rangeh?.h]}
                onChange={([vl, vh]) => {

                    ID_debounce(_this.trigTO, () => {
                        let nedf = def;
                        nedf = ObjShellingAssign(nedf, ["rangeh", "h"], vh)
                        nedf = ObjShellingAssign(nedf, ["rangel", "h"], vl)
                        onDefChange(nedf);
                    }, () => _this.trigTO = undefined, 500);
                }}

            />

        </Col>
    </Row>


    <Row>
        <Col span={2}>
            S[{def.rangel?.s}:{def.rangeh?.s}]
        </Col>
        <Col span={20}>


            <Slider
                range
                step={1} max={255}
                defaultValue={[def.rangel?.s, def.rangeh?.s]}
                onChange={([vl, vh]) => {

                    ID_debounce(_this.trigTO, () => {
                        let nedf = def;
                        nedf = ObjShellingAssign(nedf, ["rangeh", "s"], vh)
                        nedf = ObjShellingAssign(nedf, ["rangel", "s"], vl)
                        onDefChange(nedf);
                    }, () => _this.trigTO = undefined, 500);
                }}

            />



        </Col>
    </Row>


    <Row>
        <Col span={2}>
            V[{def.rangel?.v}:{def.rangeh?.v}]
        </Col>
        <Col span={20}>

            <Slider
                range
                step={1} max={255}
                defaultValue={[def.rangel?.v, def.rangeh?.v]}
                onChange={([vl, vh]) => {

                    ID_debounce(_this.trigTO, () => {
                        let nedf = def;
                        nedf = ObjShellingAssign(nedf, ["rangeh", "v"], vh)
                        nedf = ObjShellingAssign(nedf, ["rangel", "v"], vl)
                        onDefChange(nedf);
                    }, () => _this.trigTO = undefined, 500);
                }}

            />




        </Col>
    </Row>

    細節量
    <Slider
        step={1} max={255}
        value={def.detect_detail}
        onChange={(val) => {

            ID_debounce(_this.trigTO, () => {
                let nedf = def;
                nedf = ObjShellingAssign(nedf, ["detect_detail"], val)
                onDefChange(nedf);
            }, () => _this.trigTO = undefined, 500);
        }}

    />

</>;

    switch(def_Filled.type)
    {
        case "HSVSeg":
        case undefined:

        ConfigUI=<>
    
    
        單物件偵測閾值:
            <InputNumber value={def.point_area_thres * (pxSize * pxSize)} step={0.1}
                onChange={(num) => {
                    let newDef = { ...def, point_area_thres: num / (pxSize * pxSize) }
                    onDefChange(newDef, true);
                }} />
    
            物件面積閾值:
            <InputNumber value={def.element_area_thres * (pxSize * pxSize)} step={0.001}
                onChange={(num) => {
                    let newDef = { ...def, element_area_thres: num / (pxSize * pxSize) }
                    onDefChange(newDef, true);
                }} />
            物件數量閾值:
            <InputNumber value={def.element_count_thres}
                onChange={(num) => {
                    let newDef = { ...def, element_count_thres: num }
                    onDefChange(newDef, true);
                }} />
    
            <br />
    
            總面積閾值:
            <InputNumber value={def.area_thres * (pxSize * pxSize)} step={0.005}
                onChange={(num) => {
                    let newDef = { ...def, area_thres: num / (pxSize * pxSize) }
                    onDefChange(newDef, true);
                }} />
    
            單線長閾值:
            <InputNumber value={def.line_length_thres * pxSize} min={0.001} step={0.1}
                onChange={(num) => {
                    let newDef = { ...def, line_length_thres: num / pxSize }
                    onDefChange(newDef, true);
                }} />
    
            <br />
            <Button onClick={() => { setShowDisplayAdjUI(!showDisplayAdjUI) }}> {showDisplayAdjUI == false ? "+展開顯示調整選項" : "-收起顯示調整選項"}</Button>
            {showDisplayAdjUI == false ? null :ProcessDisplayUI}
    
    
    
            <br />
            <Button onClick={() => { setShowDetectAdjUI(!showDetectAdjUI) }}> {showDetectAdjUI == false ? "+展開偵測調整選項" : "-收起偵測調整選項"}</Button>
    
            {showDetectAdjUI == false ? null : HSVEditUI}
    
    
            {/* <pre>{ JSON.stringify( def, null, 2)}</pre> */}
        </>;
            break;
        case "SigmaThres":
            ConfigUI= <>SigmaThres
            
            
            色差偵測:
            <InputNumber value={def_Filled.colorSigma} step={0.1}
                onChange={(num) => {
                    let newDef = { ...def_Filled, colorSigma:num  }
                    onDefChange(newDef, true);
                }} />


            依亮度校正:
            <Switch checkedChildren="開啟" unCheckedChildren="關閉" checked={def_Filled.brightnessCompensation == true} onChange={(check) => {
                onDefChange(ObjShellingAssign(def_Filled, ["brightnessCompensation"], check));
            }} />


            </>
            break;
        case "BrightnessBalance":
            ConfigUI= <>
            
                <Switch checkedChildren="開啟" unCheckedChildren="關閉" checked={def_Filled.enable == true} onChange={(check) => {
                    onDefChange(ObjShellingAssign(def_Filled, ["enable"], check));
                }} />
                R<InputNumber value={def_Filled?.bTar?.r} min={0} max={255} step={1}
                    onChange={(num) => {
                        onDefChange(ObjShellingAssign(def_Filled, ["bTar","r"], num));
                }} />
                G<InputNumber value={def_Filled?.bTar?.g} min={0} max={255} step={1}
                    onChange={(num) => {
                        onDefChange(ObjShellingAssign(def_Filled, ["bTar","g"], num));
                }} />
                B<InputNumber value={def_Filled?.bTar?.b} min={0} max={255} step={1}
                    onChange={(num) => {
                        onDefChange(ObjShellingAssign(def_Filled, ["bTar","b"], num));
                }} />
            </>
            break;

        case "ScanPoint":
            ConfigUI= <>

                {["x","y","-x","-y","cx","cy"].map((str)=>
                {
                    let scanAngle = def_Filled?.scanAngle||0;
                    let centerOrEdge = (def_Filled?.centerOrEdge)==true;//true or false
                    let scanDirStr="x";


                    switch(scanAngle)
                    {
                        case 0:
                            scanDirStr=(centerOrEdge)?"cx":"x";
                            break;
                        case 90:
                            scanDirStr=(centerOrEdge)?"cy":"y";
                            break;
                        case 270:
                        case -90:
                            scanDirStr=(centerOrEdge)?"cy":"-y";
                            break;
                        case 180:
                            scanDirStr=(centerOrEdge)?"cx":"-x";
                            break;

                    }
                    return <Button type={scanDirStr==str?"primary":undefined} onClick={()=>{
                        let scanAngle=0;
                        let centerOrEdge=false;
                        switch(str)
                        {
                            case "x":
                                scanAngle=0;
                                break;
                            case "y":
                                scanAngle=90;
                                break;
                            case "-y":
                                scanAngle=-90;
                                break;
                            case "-x":
                                scanAngle=180;
                                break;

                            case "cy":
                                scanAngle=90;
                                centerOrEdge=true;
                                break;
                            case "cx":
                                scanAngle=180;
                                centerOrEdge=true;
                                break;
                        }
                        let newDef=def_Filled;
                        newDef=ObjShellingAssign(newDef, ["scanAngle"], scanAngle);
                        newDef=ObjShellingAssign(newDef, ["centerOrEdge"], centerOrEdge);
                        onDefChange(newDef);
                        
                    }}>{str}</Button>
                })}
            
            <br />
            <Button onClick={() => { setShowDisplayAdjUI(!showDisplayAdjUI) }}> {showDisplayAdjUI == false ? "+展開顯示調整選項" : "-收起顯示調整選項"}</Button>
            {showDisplayAdjUI == false ? null :ProcessDisplayUI}
            <br/>
                
            <Button onClick={() => { setShowDetectAdjUI(!showDetectAdjUI) }}> {showDetectAdjUI == false ? "+展開偵測調整選項" : "-收起偵測調整選項"}</Button>
    
            {showDetectAdjUI == false ? null : HSVEditUI}

            </>
            break;
        default:
            ConfigUI=<>UNKNOWN type</>
            break;
    }

    return <>

        <Button danger onClick={() => {
            onFinish();

        }}>{"<"}</Button>

        <Button danger onClick={() => {
            onDefChange(undefined)
        }}>X</Button>
        <Input maxLength={100} style={{ width: "200px" }} value={def.name}
            onChange={(e) => {
                onDefChange({ ...def, name: e.target.value })
            }} />

        <Button onClick={() => {
            // onCopy(def)

    
            canvas_obj.UserRegionSelect((info, draggingState) => {

                _this.canvasDrawCB=(()=>{


                    if(canvas_obj===undefined || canvas_obj.g===undefined)return;


                    let lsz = canvas_obj.rUtil.getIndicationLineSize();


                    let ctx = canvas_obj.g.ctx;
                    ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,0.8)";

                    ctx.fillStyle = ctx.strokeStyle;
                    let lineHeight = 15;
                    ctx.font = lineHeight + "px Arial";
                    ctx.fillText(def.name, info.pt2.x, info.pt2.y);






                    
                })
                if (draggingState == 2) {
                    canvas_obj.UserRegionSelect(undefined)
                    _this.canvasDrawCB=undefined;

                    onDefChange({ ...def_Filled, name_loc_offset:{...info.pt2} }, true);
                    // setCanvasDrawCB(undefined);
                }
                else
                {
                }
                console.log(info,draggingState,_this.canvasDrawCB);
                // if (draggingState == 1) {
                // }
                // else if (draggingState == 2) {
                //     console.log(info);
                //     canvas_obj.UserRegionSelect(undefined)

                //     onDefChange(ObjShellingAssign(def, ["name_region"], PtsToXYWH(info.pt1, info.pt2)));
                // }
            });




        }}>名稱位置</Button>

        <Button danger onClick={() => {
            onCopy(def)
        }}>COPY</Button>

    
    
        <Dropdown overlay={
            <Menu> 
                {["NG","NG2","NA"].map(str=><Menu.Item onClick={()=>{
                    let newDef = { ...def_Filled, NG_Map_To:str }
                    onDefChange(newDef, true);
                }}>{str}</Menu.Item>)}
                
            </Menu>}>
           
            <Button>
                <Space>
                差異過大:{def_Filled.NG_Map_To}
                <DownOutlined />
                </Space>
            </Button>
        </Dropdown>

        <Dropdown overlay={
            <Menu>
                {["HSVSeg","SigmaThres","BrightnessBalance","ScanPoint"].map(str=><Menu.Item onClick={()=>{
                    let newDef = { ...def_Filled, type:str }
                    onDefChange(newDef, true);
                }}>{str}</Menu.Item>
                )}
            </Menu>}>
           
            <Button>
                <Space>
                {def_Filled.type}
                <DownOutlined />
                </Space>
            </Button>
        </Dropdown>


        <Button onClick={() => {
    
    
    
            canvas_obj.UserRegionSelect((info, draggingState) => {
                if (draggingState == 1) {
                }
                else if (draggingState == 2) {
                    console.log(info);
                    canvas_obj.UserRegionSelect(undefined)

                    onDefChange(ObjShellingAssign(def, ["region"], PtsToXYWH(info.pt1, info.pt2)));
                }
            });
        }}>設定範圍</Button>
        

                
        {
            def_Filled.ignore_regions.map((regionInfo: any, index: number) => {





                return <Popconfirm
                    key={"regi_del_" + index + "..."}
                    title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
                    onConfirm={() => { }}
                    onCancel={() => { }}
                    okButtonProps={{
                        danger: true, onClick: () => {
                            if (delConfirmCounter != 0) {
                                setDelConfirmCounter(delConfirmCounter - 1);
                            }
                            else {
                                let new_ig_regions = [...def_Filled.ignore_regions];

                                new_ig_regions.splice(index, 1);


                                def_Filled.ignore_regions = new_ig_regions;

                                // setFeatureInfo({ ...featureInfo, mask_regions })

                                onDefChange(def_Filled);

                            }
                        }
                    }}
                    okText={"Yes:" + delConfirmCounter}
                    cancelText="No"
                >
                    <Button type="primary" onClick={() => {
                        setDelConfirmCounter(5);
                    }}>{index}</Button>
                </Popconfirm>






            })

        }
        <Button danger type="primary" onClick={() => {

            // let newDef = ObjShellingAssign(def_Filled,["ignore_regions",def_Filled.ignore_regions.length],{
            //         x:0,y:0,w:1,h:1

            // });

            // onDefChange(newDef)



            canvas_obj.UserRegionSelect((info, state) => {
                if (state == 2) {
                    console.log(info);

                    let roi_region = PtsToXYWH(info.pt1, info.pt2);
                    console.log(roi_region)
                    roi_region.x -= def_Filled.region.x;
                    roi_region.y -= def_Filled.region.y;



                    let newDef = ObjShellingAssign(def_Filled, ["ignore_regions", def_Filled.ignore_regions.length], roi_region);

                    onDefChange(newDef)
                    canvas_obj.UserRegionSelect(undefined)

                }
            })



        }}>+忽略區域</Button>
        <br />
        {ConfigUI}
    </>//<pre>{ JSON.stringify( def, null, 2)}</pre>
}



function SurfaceCheckSimple_EDIT_UI(param:
    {
        BPG_API: BPG_WS,
        fsPath: string,
        def: any,
        onDefChange: (...param: any) => void,
        onFinish: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent,
        canvas_hook_update: (cb: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, is_post_render: boolean) => any) | undefined) => any
    }) {

    let { BPG_API, fsPath, def, onDefChange, onFinish, canvas_obj, canvas_hook_update } = param;

    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    const [updateC, setUpdateC] = useState(0);
    const [UIStack, setUIStack] = useState<{ type: string, info: any, exitcb: (info: any) => any, updatecb: (info: any) => any }[]>([]);
    let def_Filled = {
        sub_regions: [],
        color_ch_mul: {
            r: 1, g: 1, b: 1
        },
        ...def
    }
    const _this = useRef<any>({}).current;

    let topUI = UIStack[UIStack.length - 1];

    let UI_POP = (retInfo: any) => {
        topUI.exitcb(retInfo);
        let newUIStack = [...UIStack]
        newUIStack.pop();
        setUIStack(newUIStack)
    };


    function cropROIUpdate(newROI: { x_offset: number, y_offset: number, w: number, h: number }) {
        let coffset_x = newROI.x_offset - def_Filled.x_offset + (newROI.w - def_Filled.w) / 2;
        let coffset_y = newROI.y_offset - def_Filled.y_offset + (newROI.h - def_Filled.h) / 2;

        let updatedSubReg = def_Filled.sub_regions.map((sreg: any) => {
            let org = sreg.region;
            return { ...sreg, region: { ...org, x: org.x + coffset_x, y: org.y + coffset_y } }
        })




        onDefChange({ ...def_Filled, ...newROI, sub_regions: updatedSubReg }, true);

    }


    if (UIStack.length > 0) {

        switch (topUI.type) {

            case "擷取參數設定":
                return <>


                    <Button danger onClick={() => {
                        canvas_hook_update(undefined)
                        UI_POP(undefined);
                    }}>{"<"}</Button>

                    XOffset:
                    <InputNumber value={def_Filled.x_offset}
                        onChange={(num) => {

                            cropROIUpdate({ ...def_Filled, x_offset: num });
                        }} />
                    {"  "}YOffset:
                    <InputNumber value={def_Filled.y_offset}
                        onChange={(num) => {
                            cropROIUpdate({ ...def_Filled, y_offset: num });
                        }} />

                    <br />W:
                    <InputNumber min={10} max={2000} value={def_Filled.w}
                        onChange={(num) => {
                            cropROIUpdate({ ...def_Filled, w: num });
                        }} />
                    {"  "}H:
                    <InputNumber min={10} max={2000} value={def_Filled.h}
                        onChange={(num) => {
                            cropROIUpdate({ ...def_Filled, h: num });
                        }} />
                    <br />
                    {"  "}角度調整:
                    <InputNumber value={def_Filled.angle_offset}
                        onChange={(num) => {
                            let newDef = { ...def_Filled, angle_offset: num }
                            onDefChange(newDef, true);
                        }} />

                    {"  "}區域名稱尺寸:
                    <InputNumber min={0.1} max={10} step={0.1} value={def_Filled.subRegionNameSize}
                        onChange={(num) => {
                            let newDef = { ...def_Filled, subRegionNameSize: num }
                            onDefChange(newDef, true);
                        }} />


                    圖序反轉:
                    <Switch checkedChildren="左至右" unCheckedChildren="右至左" checked={def_Filled.img_order_reverse == true} onChange={(check) => {
                        onDefChange(ObjShellingAssign(def_Filled, ["img_order_reverse"], check));
                    }} />

                    多目標列數:
                    <InputNumber value={def_Filled.multi_target_column_count}
                        onChange={(num) => {
                            let newDef = { ...def_Filled, multi_target_column_count: num }
                            onDefChange(newDef, true);
                        }} />


                    {"  "}降採樣倍率:
                    <InputNumber min={1} max={20} step={1} value={def_Filled.down_sample_factor}
                        onChange={(num) => {
                            let newDef = { ...def_Filled, down_sample_factor: num }
                            onDefChange(newDef, true);
                        }} />



                    <Button danger onClick={() => {
                        let BG_IMG_NAME="background_temp.png";
                        (async () => {

                            let pkts = await BPG_API.InspTargetExchange(def_Filled.id, {
                                type: "cache_image_save",
                                folder_path: fsPath + "/",
                                image_name: BG_IMG_NAME,
                            }) as any[];

                            await BPG_API.InspTargetExchange(def_Filled.id, {
                                type: "reload_background_temp"
                            }) as any[];


                            console.log(pkts,fsPath,BG_IMG_NAME);

                        })()

                        
                    }}>{"儲存背景模板"}</Button>

                    
                </>

            case "子區域設定":
                return <>
                    <SurfaceCheckSimple_SubRegion_EDIT_UI
                        {...param}

                        id={def.id!==undefined?def.id:("$"+topUI.info.index)}
                        pxSize={1}
                        def={GetObjElement(def_Filled, topUI.info.opath)}
                        onDefChange={(newDef) => {
                            console.log(newDef);
                            if (newDef !== undefined) {
                                onDefChange(ObjShellingAssign(def_Filled, topUI.info.opath, newDef))
                            }
                            else {
                                def_Filled.sub_regions.splice(topUI.info.index, 1);
                                console.log(def_Filled);
                                onDefChange(def_Filled);
                                UI_POP("AAA RET");
                            }


                        }}
                        onCopy={(def) => {
                            let newDef = clone(def);
                            newDef.region.x += 0;
                            newDef.region.y += 0;
                            newDef.name = newDef.name+"_COPY";
                            def_Filled.sub_regions.push(newDef);
                            console.log(def_Filled);
                            onDefChange(def_Filled);
                        }}
                        onFinish={() => {
                            canvas_hook_update(undefined)
                            UI_POP("AAA RET");
                        }}


                    />
                </>

        }


        return <Button danger onClick={() => {
            UI_POP(undefined);
        }}>BACK</Button>;
    }



    console.log(def_Filled);
    return <>

        <Button key={"_" + -1} onClick={() => {
            onFinish(def_Filled);
        }}>{"<"}</Button>

        <Button danger type="primary" onClick={() => {
            setUIStack([...UIStack, {
                type: "擷取參數設定",
                exitcb: (info) => {
                    console.log(info);
                },
                info: undefined,
                updatecb: (info) => {

                }
            }])
        }}>擷取參數設定</Button>


        <br />
        子區域設定:
        {
            def_Filled.sub_regions.map((regionInfo: any, index: number) => {

                return <Button type="primary" onClick={() => {
                    setUIStack([...UIStack, {
                        type: "子區域設定",
                        exitcb: (info) => {
                            console.log(info);
                        },
                        info: {
                            index: index,
                            def: def_Filled,
                            opath: ["sub_regions", index],
                            regionInfo,
                        },
                        updatecb: (info) => {

                        }
                    }])
                }}>{regionInfo.name === undefined || regionInfo.name == "" ? "$" + index : regionInfo.name}</Button>

            })

        }
        <Button danger type="primary" onClick={() => {
            let newDef = ObjShellingAssign(def_Filled, ["sub_regions", def_Filled.sub_regions.length], {
                region: {
                    x: 0, y: 0, w: 1, h: 1
                }


            });

            onDefChange(newDef)
            setUIStack([...UIStack, {
                type: "子區域設定",
                exitcb: (info) => {
                    console.log(info);
                },
                info: {
                    def: def_Filled,
                    opath: ["sub_regions", def_Filled.sub_regions.length],
                },
                updatecb: (info) => {

                }
            }])
        }}>+</Button>



        <br />
        RX
        <InputNumber min={0.1} step={0.05} value={def_Filled.color_ch_mul.r}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["color_ch_mul", "r"], num));
            }} />
        GX
        <InputNumber min={0.1} step={0.05} value={def_Filled.color_ch_mul.g}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["color_ch_mul", "g"], num));
            }} />
        BX
        <InputNumber min={0.1} step={0.05} value={def_Filled.color_ch_mul.b}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["color_ch_mul", "b"], num));
            }} />
        <br />

        bilateral

        d
        <InputNumber min={1} step={1} value={def_Filled.bilateral?.d}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["bilateral", "d"], num));
            }} />
        sigmaColor
        <InputNumber min={0.0001} step={1} value={def_Filled.bilateral?.sigmaColor}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["bilateral", "sigmaColor"], num));
            }} />
        sigmaSpace
        <InputNumber min={0.0001} step={1} value={def_Filled.bilateral?.sigmaSpace}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["bilateral", "sigmaSpace"], num));
            }} />








        <br />
        直方等化:
        <Switch checkedChildren="使用" unCheckedChildren="停用" checked={def_Filled.equalize_hist == true} onChange={(check) => {
            onDefChange(ObjShellingAssign(def_Filled, ["equalize_hist"], check));
        }} />
        {/* _pixelSize(mm)
        <InputNumber min={0.001} step={0.001} defaultValue={1} value={def_Filled.pxSize}
            onChange={(num) => {
                onDefChange(ObjShellingAssign(def_Filled, ["pxSize"], num));
            }} /> */}

    </>

}



function tagsMatching(tags1: string[], tags2: string[]) {
    for (let i = 0; i < tags1.length; i++) {
        let isMatched = false;
        for (let j = 0; j < tags2.length; j++) {
            if (tags1[i] == tags2[j]) {
                isMatched = true;
                break;
            }
        }
        if (isMatched == false) return false;
    }
    return true;
}

function TagsEdit_DropDown({ tags, onTagsChange, children }: { tags: (string | string[])[], onTagsChange: (tags: (string | string[])[]) => void, children: React.ReactChild }) {
    const [visible, _setVisible] = useState(false);
    const [newTagTxt, setNewTagTxt] = useState("");


    const [tagDelInfo, setTagDelInfo] = useState<{ tarTag: (string | string[]), countdown: number }>({ tarTag: "", countdown: 0 });


    function setVisible(enable: boolean) {
        setTagDelInfo({ ...tagDelInfo, tarTag: "" });
        _setVisible(enable);
    }
    if (tags === undefined)
        tags = []

    let newTags = newTagTxt.split(',');



    let isNewTagTxtDuplicated = tags.find(tag => tagsMatching(Array.isArray(tag) ? tag : [tag], newTags)) != undefined;



    return <Dropdown onVisibleChange={setVisible} visible={visible}
        overlay={<Menu>
            {
                [...tags.map((tag: string | string[], index: number) => (
                    <Menu.Item key={tag + "_" + index}
                        onClick={() => {
                            if (tagDelInfo.tarTag != tag) {
                                setTagDelInfo({
                                    tarTag: tag,
                                    countdown: 3
                                });
                                return;
                            }

                            if (tagDelInfo.countdown > 0) {
                                setTagDelInfo({ ...tagDelInfo, countdown: tagDelInfo.countdown - 1 });
                                return;
                            }

                            let newList = [...tags]
                            newList.splice(index, 1);
                            onTagsChange(newList);
                        }}>
                        {tag + ((tagDelInfo.tarTag != tag) ? "" : ("   cd:" + tagDelInfo.countdown))}
                    </Menu.Item>)),

                <Menu.Item key={"ADD"}
                    onClick={(e) => {
                    }}>

                    <Input maxLength={100} value={newTagTxt} status={isNewTagTxtDuplicated ? "error" : undefined}
                        onChange={(e) => {
                            setNewTagTxt(e.target.value);
                        }}
                        onPressEnter={(e) => {
                            let new_tags = [...tags, newTags];

                            if (isNewTagTxtDuplicated == false) {
                                onTagsChange(new_tags);
                                setNewTagTxt("");
                            }
                        }} />

                </Menu.Item>
                ]
            }
        </Menu>}
    >
        {children}
    </Dropdown>

}

const CAT_ID_Color = {
    "": "gray",
    "0": "gray",
    "1": "green",
    "-1": "red",
    "-2": "red",
    "-3": "red",
    "-40000": "gray",

    "-700": "red",
    "-701": "red",
    "-750": "red",
}

const CAT_ID_NAME = {
    "": "NA",
    "0": "NA",
    "1": "OK",
    "-1": "NG",
    "-2": "NG2",
    "-3": "NG3",
    "-40000": "--",

    "-700": "點過大",
    "-701": "邊過長",
    "-750": "色彩校正差異過大",
}
const _MM_P_STP_ = 4;
const _OBJ_SEP_DIST_ = 4;





export function InspTarView_basicInfo({ display, fsPath, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }: CompParam_InspTarUI) {

    const [cacheDef, _setCacheDef] = useState<any>(def);

    useEffect(() => {
        console.log("fsPath:" + fsPath)
        _setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);



    const dispatch = useDispatch();
    // const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    // const [queryCameraList, setQueryCameraList] = useState<any[] | undefined>(undefined);
    const [delConfirmCounter, setDelConfirmCounter] = useState(0);


    return <>

        <TagsEdit_DropDown tags={cacheDef.match_tags}
            onTagsChange={(newTags) => {

                onDefChange({ ...cacheDef, match_tags: newTags }, false)
            }}>
            <a>TAGS</a>
        </TagsEdit_DropDown>




        <Popconfirm
            title={`確定要刪除？ 再按:${delConfirmCounter + 1}次`}
            onConfirm={() => { }}
            onCancel={() => { }}
            okButtonProps={{
                danger: true, onClick: () => {
                    if (delConfirmCounter != 0) {
                        setDelConfirmCounter(delConfirmCounter - 1);
                    }
                    else {
                        onDefChange(undefined, false)
                    }
                }
            }}
            okText={"Yes:" + delConfirmCounter}
            cancelText="No"
        >
            <Button danger type="primary" onClick={() => {
                setDelConfirmCounter(5);
            }}>DEL</Button>
        </Popconfirm>


        <Button onClick={() => {
            onDefChange(cacheDef, true)
        }}>SAVE</Button>

        {/* <Switch checkedChildren="隱藏" unCheckedChildren="顯示" checked={cacheDef.default_hide == true} onChange={(check) => {

            onDefChange({ ...cacheDef, default_hide: check }, false)
        }} /> */}
    </>

}





export function SingleTargetVIEWUI_SurfaceCheckSimple(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false } = props;
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        canvasHook: undefined,

        drawHooks: [],
        ctrlHooks: [],

        stepQueryTime: 1000,
        periodicTask_HDL: undefined,
        periodicTask: () => { }

    });

    const [perifConnState, setPerifConnState] = useState<boolean>(false);


    const [cacheDef, setCacheDef] = useState<any>(def);
    const [cameraQueryList, setCameraQueryList] = useState<any[] | undefined>([]);


    const [defReport, setDefReport] = useState<any>(undefined);
    const [NGInfoList, setNGInfoList] = useState<{ location_mm: number, category: number }[]>([]);
    const [latestRepStepCount, setLatestRepStepCount] = useState(0);
    const [reelStep, setReelStep] = useState<number>(0);

    const [forceUpdateCounter, setForceUpdateCounter] = useState(0);
    let _this = _.current;
    let c_report: any = undefined;
    if (_this.cache_report !== report) {
        if (report !== undefined) {
            _this.cache_report = report;
        }
    }
    c_report = _this.cache_report;


    useEffect(() => {
        console.log("fsPath:" + fsPath)
        _this.cache_report = undefined;
        setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);


    useEffect(() => {


        console.log(def);
        BPG_API.InspTargetExchange(def.id, {
            type: "stream_info",
            downsample: display ? 1 : 10,
            stream_id: def.stream_id
        });

        return (() => {
        });

    }, [display]);

    // console.log(IMCM_group,report);
    // const [drawHooks,setDrawHooks]=useState<type_DrawHook[]>([]);
    // const [ctrlHooks,setCtrlHooks]=useState<type_DrawHook[]>([]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);


    enum EditState {
        Normal_Show = 0,
        Region_Edit = 1,
    }

    const [editState, setEditState] = useState<EditState>(EditState.Normal_Show);


    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [CNC_API, setCNC_API] = useState<CNC_Perif>(dispatch(EXT_API_ACCESS(CNC_PERIPHERAL_ID)) as any);


    const [queryCameraList, setQueryCameraList] = useState<any[] | undefined>(undefined);
    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    const [showNonNAOnly, setShowNonNAOnly] = useState(false);


    function onCacheDefChange(updatedDef: any, ddd: boolean) {
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            console.log(">>>");
            await BPG_API.InspTargetUpdate(updatedDef)

        })()

        BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
        onDefChange(updatedDef, ddd);
    }

    function periodicCB() {
        // console.log("jkdshfsdhf;iohsd;iofjhsdio;fhj;isdojfhdsil;");
        _this.periodicTask_HDL = undefined;
        return;
        {
            (async () => {
                // console.log(  CNC_API.isConnected)
                if (CNC_API.isConnected != perifConnState)
                    setPerifConnState(CNC_API.isConnected);

                // if(CNC_API.isConnected)
                {

                    // console.log("SEND....")
                    let ret = await CNC_API.send_P({ "type": "GET_CUR_STEP_COUNTER" }) as any
                    // console.log(ret)
                    if (ret.step != reelStep) {
                        setReelStep(ret.step)
                        _this.stepQueryTime = 200;
                    }
                    else {
                        _this.stepQueryTime += 50;
                        if (_this.stepQueryTime > 1000)
                            _this.stepQueryTime = 1000;
                    }
                }

            })()
                .catch((e) => {

                    // console.log(e)
                    if (_this.periodicTask_HDL !== undefined) {
                        window.clearTimeout(_this.periodicTask_HDL);
                    }
                    _this.periodicTask_HDL = window.setTimeout(_this.periodicTask, _this.stepQueryTime);
                })

            if (_this.periodicTask_HDL !== undefined) {
                window.clearTimeout(_this.periodicTask_HDL);
            }
            _this.periodicTask_HDL = window.setTimeout(_this.periodicTask, _this.stepQueryTime);
            // if(_this.periodicTask_HDL!==undefined)
            // {
            //   window.clearTimeout(_this.periodicTask_HDL);
            // }
            // _this.periodicTask_HDL=window.setTimeout(_this.periodicTask,_this.stepQueryTime);

        }
    }
    _this.periodicTask = periodicCB;

    _this.TMP_NGInfoList = NGInfoList;
    _this.perifConnState = perifConnState;
    _this.showNonNAOnly = showNonNAOnly;
    useEffect(() => {//////////////////////

        let cbsKey="_"+Math.random();
        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id,cbsKey, {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    console.log("++++++++\n", IM, CM, RP);

                    if(_this.showNonNAOnly)
                    {
                        let sub_reports=RP?.data?.report?.sub_reports;
                        if(sub_reports!==undefined)
                        {
                            let NARep=sub_reports.find((rep:any)=>rep.category==0);
                            if(NARep!==undefined)return;
                        }

                    }



                    // setDefReport(RP.data)
                    let IMCM = {
                        image_info: IM.image_info,
                        camera_id: CM.data.camera_id,
                        trigger_id: CM.data.trigger_id,
                        trigger_tag: CM.data.trigger_tag,
                    } as type_IMCM

                    _this.imgCanvas.width = IMCM.image_info.width;
                    _this.imgCanvas.height = IMCM.image_info.height;

                    let ctx2nd = _this.imgCanvas.getContext('2d');
                    if(IMCM.image_info.image instanceof ImageData)
                        ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
                    else if(IMCM.image_info.image instanceof HTMLImageElement)
                        ctx2nd.drawImage(IMCM.image_info.image, 0, 0);


                    setLocal_IMCM(IMCM)
                    let rep = RP.data;
                    setDefReport(rep);
                    if (rep.report.category <= 0 && _this.perifConnState) {
                        CNC_API.send_P({ "type": "EM_STOP" })
                            .then((ret) => {
                                // console.log(ret)
                            })
                            .catch(e => {

                            })



                        let repAtStepTag = rep.tags.find((tag: string) => tag.startsWith("s_Step_"));
                        if (repAtStepTag !== undefined) {
                            let repAtStep = parseInt(repAtStepTag.replace('s_Step_', ''));
                            // rep.report
                            console.log(rep, repAtStep)

                            let newNGInfo = rep.report.sub_reports.map((bad_ele: any, index: number) => ({ location_mm: -repAtStep * _MM_P_STP_ + index * _MM_P_STP_, category: bad_ele.category }))
                                .filter((ele: any) => ele.category <= 0)
                            console.log(newNGInfo, "mm")

                            setLatestRepStepCount(repAtStep);
                            setNGInfoList([..._this.TMP_NGInfoList, ...newNGInfo])
                        }

                    }

                },
                reject: (pkts) => {

                }
            }

            )

        })()

        _this.periodicTask();

        return (() => {
            window.clearTimeout(_this.periodicTask_HDL);
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);

                // await BPG_API.InspTargetSetStreamChannelID(
                //   cacheDef.id,0,
                //   {
                //     resolve:(pkts)=>{
                //     },
                //     reject:(pkts)=>{

                //     }
                //   }
                // )
            })()

        })
    }, []);
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


    let showEditUI:boolean=true;
    let showGraphic:boolean=true;
    if(UIOption!=undefined)
    {
        showGraphic=true;
    }
    else
    {
        showEditUI=true;
        showGraphic=true;
    }
    // console.log(UIOption);

    if (display == false) return null;



    let EDIT_UI = null;
    if(showEditUI)
    switch (editState) {

        case EditState.Normal_Show:


            let EditUI = null;

            if ((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) != 0)//allow edit
            {
                EDIT_UI = <>

                    {/* <Input maxLength={100} value={cacheDef.type} disabled
            style={{width:"100px"}}
            onChange={(e)=>{
              
            }}/>
  
          <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
            style={{width:"100px"}}
            onChange={(e)=>{
            }}/> */}
                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        onCacheDefChange(newDef, ddd);
                    }} />


                    <br />
                    {/* <Button onClick={() => {
                        onCacheDefChange(cacheDef, true);
                    }}>SHOT</Button> */}


                    <Button key={"_" + 10000} onClick={() => {
                        setShowNonNAOnly(false);
                        setEditState(EditState.Region_Edit);
                    }}>EDIT</Button>

                </>
            }


            EDIT_UI = <>

                <Input maxLength={300} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
                {EDIT_UI}
                <Switch checkedChildren="僅顯示可驗" unCheckedChildren="全顯示圖像" checked={showNonNAOnly} onChange={(check) => {
                    setShowNonNAOnly(check)
                }} />
                {"  "}
                {
                    (perifConnState) ? <>
                        <Button onClick={() => {
                            CNC_API.send_P({ "type": "Encoder_Reset" })
                        }}>歸零</Button>

                        <Button onClick={() => {

                            CNC_API.send_P({ "type": "TRIG_CAMERA_TAKE" })
                            // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
                        }}>測試觸發</Button>
                        <Button onClick={() => {

                            CNC_API.send_P({ "type": "LIGHT_1_ON" })
                            // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
                        }}>LON</Button>
                        <Button onClick={() => {

                            CNC_API.send_P({ "type": "LIGHT_1_OFF" })
                            // BPG_API.CameraSWTrigger("Hikrobot-2BDF71598890-00F71598890","",0,false)
                        }}>LOFF</Button>
                    </> :
                        null
                }
                <br />
                {
                    (NGInfoList.length > 0) ?
                        <Popconfirm
                            placement="rightBottom"
                            title={`確定要刪除全部NG？ 再按:${delConfirmCounter + 1}次`}
                            onConfirm={() => { }}
                            onCancel={() => { }}
                            okButtonProps={{
                                danger: true, onClick: () => {
                                    if (delConfirmCounter != 0) {
                                        setDelConfirmCounter(delConfirmCounter - 1);
                                    }
                                    else {
                                        setNGInfoList([])
                                    }
                                }
                            }}
                            okText={"Yes:" + delConfirmCounter}
                            cancelText="No"
                        >
                            <Button danger type="primary" onClick={() => {
                                setDelConfirmCounter(5);
                            }}>X</Button>
                        </Popconfirm> : null
                }
                {
                    NGInfoList.map((nginfo, index) =>
                        <Button danger onClick={() => {

                            let newList = [...NGInfoList]
                            newList.splice(index, 1);
                            setNGInfoList(newList);
                        }}>{((nginfo.location_mm + reelStep * _MM_P_STP_) / _OBJ_SEP_DIST_) + "顆 [" + CAT_ID_NAME[nginfo.category + ""] + "]"}</Button>
                    )
                }


            </>


            break;

        case EditState.Region_Edit:

            EDIT_UI = <>
                {/* <Button key={"_" + -1} onClick={() => {

                    setEditState(EditState.Normal_Show);
                }}>{"<"}</Button> */}


                <SurfaceCheckSimple_EDIT_UI
                    BPG_API={BPG_API}
                    fsPath={fsPath}
                    def={cacheDef}
                    onDefChange={(newDef) => {
                        onCacheDefChange(newDef, true);

                    }}

                    onFinish={(newDef) => {

                        _this.canvasHook = undefined;
                        setEditState(EditState.Normal_Show);
                    }}
                    canvas_obj={_this.canvasComp}
                    canvas_hook_update={(new_canvas_hook) => { _this.canvasHook = new_canvas_hook }}

                />

            </>

            break;
    }


    if(showUIOptionConfigUI)
    {
        return <div style={{ ...style}}>
            <>AAA</>
        </div>
    }


    let img_order_reverse = cacheDef.img_order_reverse === true;
    // console.log("img_order_reverse:"+img_order_reverse) 
    return <div style={{ ...style}} className={"overlayCon"}>

        <div className={"overlay"} >

            {EDIT_UI}

        </div>

        {(showGraphic==true)?
        <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            _this.canvasComp = canvas_obj;
            if (_this.canvasHook !== undefined) {
                if (_this.canvasHook(ctrl_or_draw, g, canvas_obj, false) == true)
                    return;
            }
            // console.log(ctrl_or_draw);
            if (ctrl_or_draw == true)//ctrl
            {
                const imageData = g.ctx.getImageData(g.mouseStatus.x, g.mouseStatus.y, 1, 1);
                _this.fetchedPixInfo = imageData;
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
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }
                    _this.sel_region = PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2);
                }

            }
            else//drawvv
            {
                let insp_down_sample_factor=(cacheDef?.down_sample_factor)
                
                let camMag = canvas_obj.camera.GetCameraScale();
                if (Local_IMCM !== undefined) {
                    g.ctx.save();
                    let scale = Local_IMCM.image_info.scale*insp_down_sample_factor;
                    g.ctx.scale(scale, scale);
                    g.ctx.translate(-0.5, -0.5);
                    g.ctx.drawImage(_this.imgCanvas, 0, 0);
                    g.ctx.restore();
                }
                // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))

                if (_this.fetchedPixInfo !== undefined) {
                    g.ctx.save();
                    g.ctx.resetTransform();
                    // console.log(_this.fetchedPixInfo)
                    let pixInfo = _this.fetchedPixInfo.data;
                    g.ctx.font = "1.5em Arial";
                    g.ctx.fillStyle = "rgba(250,100, 50,1)";

                    // g.ctx.fillText(rgb2hsv(pixInfo[0], pixInfo[1], pixInfo[2]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)

                    g.ctx.fillText((pixInfo as number[]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)
                    g.ctx.restore();
                }

                let ctx = g.ctx;


                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());

                }


                if (defReport !== undefined) {

                    let subRegionNameSize=cacheDef.subRegionNameSize||1;
                    let g_cat = defReport.report.sub_reports;

                    {
                        ctx.save();
                        ctx.resetTransform();
                        ctx.font = "20px Arial";
                        ctx.fillStyle = "rgba(150,100, 100,0.5)";
                        let Y = 350 
                        let result_text = CAT_ID_NAME[defReport.report.category + ""] || "NA"
                        ctx.fillText("Result:" + result_text, 20, Y);
                        Y += 30; ctx.fillText("ProcessTime:" + (defReport.process_time_us / 1000).toFixed(2) + " ms", 20, Y)


                        // // console.error(g_cat);
                        // let catSet = g_cat.reduce((catSet:any,catInfo: any) => {
                        //     return {...catSet,
                        //         ...catInfo.elements.reduce((catSet:any,catEInfo: any) => {
                        //             if(catEInfo.category!=1)
                        //             {
                        //                 catSet[catEInfo.category+""]=1;
                        //             }
                        //             return catSet;
                        //         },{})
                        //     };
                        // },{})
                        // Object.keys(catSet).forEach(cat=>{
                        //     Y+=30;
                        //     ctx.fillText(CAT_ID_NAME[cat + ""], 20, Y)

                        // })


                        ctx.restore();
                    }
                    g_cat.forEach((catInfo: any, _index: number) => {
                        
                        g.ctx.save();
                        let multi_target_column_count=cacheDef.multi_target_column_count||9999;

                        let blockX=_index%multi_target_column_count;
                        let blockY=Math.floor(_index/multi_target_column_count);
                        
                        g.ctx.translate(cacheDef.w*blockX, cacheDef.h*blockY);
                        // console.log(catInfo.category);
                        
                        if (catInfo.sub_regions.length == cacheDef.sub_regions.length)
                            catInfo.sub_regions.forEach((subreg: any, subreg_index: number) => {
                                let regionInfo = cacheDef.sub_regions[subreg_index]


                                let lsz = canvas_obj.rUtil.getIndicationLineSize();
                                let id_name = CAT_ID_NAME[subreg.category + ""];
                                if (id_name == "OK")
                                    ctx.strokeStyle = ctx.fillStyle = "rgba(0, 179, 0,0.8)";
                                else if (id_name == "NG") {
                                    ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,0.8)";
                                    lsz *= 1.5;
                                }
                                else if (id_name == "NG2") {
                                    ctx.strokeStyle = ctx.fillStyle = "rgba(200, 100, 0,0.8)";
                                    lsz *= 1.5;
                                }
                                else {
                                    ctx.strokeStyle = ctx.fillStyle = "rgba(150, 150, 150,0.8)";
                                    lsz *= 1.5;
                                }


                                {

                                    ctx.fillStyle = ctx.strokeStyle;
                                    let lineHeight = 15;
                                    ctx.font = lineHeight + "px Arial";
                                    let prefix = "";
                                    if (regionInfo.x_locating_mark == true) prefix += "X"
                                    if (regionInfo.y_locating_mark == true) prefix += "Y"

                                    let idText = prefix + (regionInfo.name === undefined || regionInfo.name == "" ? "$" + subreg_index : regionInfo.name);// +"["+id_name+"]";


                                    if(regionInfo.type=="ScanPoint" || regionInfo.type=="BrightnessBalance")
                                    {

                                        drawRegion(g, canvas_obj, regionInfo.region, lsz/5);
                                    }
                                    else
                                    {

                                        drawRegion(g, canvas_obj, regionInfo.region, lsz);
                                    }

                                    let loc=regionInfo.name_loc_offset!==undefined?regionInfo.name_loc_offset:regionInfo.region;
                                    g.ctx.save();
                                    g.ctx.translate(loc.x, loc.y);
                                    g.ctx.scale(subRegionNameSize,subRegionNameSize);

                                    ctx.fillText(idText, 0, 0);
                                    let fLoc = ctx.measureText(idText).width;
                                    let yoffset = 0;
                                    ctx.strokeStyle = ctx.fillStyle = `rgba(200,200,200,0.6)`;
                                    ctx.fillText((subreg.score===null)?"N/A":subreg.score.toFixed(2) , fLoc, yoffset); yoffset += 19;

                                    let overlayColor = { r: 255, g: 0, b: 0, ...regionInfo.overlayColor }

                                    ctx.strokeStyle = ctx.fillStyle = `rgba(${overlayColor.r}, ${overlayColor.g},${overlayColor.b},1)`;
                                    ctx.arc(0, 0, 3, 0, 2 * Math.PI, false);
                                    ctx.fill();
                                    ctx.closePath();

                                    ctx.restore();

                                }

                                if(regionInfo.type=="ScanPoint")
                                {
                                    // canvas_obj.rUtil.drawCross(ctx, { x: ele.x*insp_down_sample_factor, y: ele.y*insp_down_sample_factor }, 5);
                                    // console.log(subreg,regionInfo);
                                    if(regionInfo.scanAngle==0 || regionInfo.scanAngle==180)
                                        canvas_obj.rUtil.drawCross(ctx, { x:subreg.score, y: regionInfo.region.y+regionInfo.region.h/2 }, 5);
                                    else
                                        canvas_obj.rUtil.drawCross(ctx, { x:regionInfo.region.x+regionInfo.region.w/2, y: subreg.score }, 5);
                                }





                                // console.log(subreg);
                                subreg.elements.forEach((ele: any, _index: number) => {
                                    // console.log(subreg);


                                    let id_name = CAT_ID_NAME[ele.category + ""];
                                    if (id_name == "OK")
                                        ctx.strokeStyle = ctx.fillStyle = "rgba(0, 179, 0,0.8)";
                                    else if (id_name == "NA")
                                        ctx.strokeStyle = ctx.fillStyle = "rgba(150, 150, 150,0.8)";
                                    else
                                        ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,0.8)";


                                    ctx.fillStyle = ctx.strokeStyle;
                                    canvas_obj.rUtil.drawCross(ctx, { x: ele.x*insp_down_sample_factor, y: ele.y*insp_down_sample_factor }, 5);


                                    // let fontSize_eq=10/camMag;
                                    // if(fontSize_eq>10)fontSize_eq=40;
                                    // ctx.font = (fontSize_eq)+"px Arial";
                                    ctx.font = "1px Arial";
                                    ctx.fillText(id_name + ":" + ele.area, ele.x*insp_down_sample_factor, ele.y*insp_down_sample_factor);





                                })


                                // console.log(regionInfo.ignore_regions);
                                if (regionInfo.ignore_regions !== undefined) {
                                    regionInfo.ignore_regions.forEach((ig_region: any) => {

                                        ctx.strokeStyle = "rgba(100,100, 100,0.8)";
                                        ctx.fillStyle = "rgba(100,100, 100,0.2)";
                                        let igr = { ...ig_region };
                                        igr.x += regionInfo.region.x;
                                        igr.y += regionInfo.region.y;
                                        drawRegion(g, canvas_obj, igr, lsz / 2, false, []);
                                        ctx.fill();

                                        ctx.strokeStyle = "rgba(100,100, 100,0.5)";
                                        ctx.beginPath();
                                        ctx.moveTo(regionInfo.region.x, regionInfo.region.y);
                                        ctx.lineTo(igr.x, igr.y);
                                        ctx.stroke();
                                        ctx.closePath();
                                    })
                                }


                            })
                        
                        
                        {
                            let id_name = CAT_ID_NAME[catInfo.category + ""];
                            if (id_name == "OK")
                                ctx.strokeStyle = ctx.fillStyle = "rgba(0, 179, 0,1)";
                            else if (id_name == "NG") {
                                ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,1)";
                            }
                            else if (id_name == "NG2") {
                                ctx.strokeStyle = ctx.fillStyle = "rgba(200, 100, 0,1)";
                            }
                            else {
                                ctx.strokeStyle = ctx.fillStyle = "rgba(150, 150, 150,1)";
                            }
                            let size=2;
                            ctx.lineWidth = size*3/camMag;
                            ctx.strokeStyle="rgba(0,0,0,1)";
                            ctx.setLineDash([]);
                            ctx.beginPath();
                            ctx.lineTo(0, 0);
                            ctx.arc(0, 0, size*18/camMag, 0, Math.PI/2, false);
                            ctx.lineTo(0, 0);
                            ctx.fill();
                            ctx.fillStyle = "rgba(255, 255, 255,1)";
                            let tsize=size*10/camMag
                            ctx.font = tsize+"px Arial";
                            ctx.fillText(_index+"", tsize/5,tsize);



                            ctx.closePath();
                            ctx.stroke();

                        }
                        ctx.restore();
                    })
                }
                else {
                    if (cacheDef.sub_regions !== undefined)
                        cacheDef.sub_regions.forEach((regionInfo: any, index: number) => {

                            ctx.strokeStyle = "rgba(100, 100, 100,0.6)";

                            let lsz = canvas_obj.rUtil.getIndicationLineSize();
                            ctx.fillStyle = ctx.strokeStyle;
                            ctx.font = "20px Arial";
                            ctx.fillText(index + "", regionInfo.region.x, regionInfo.region.y);
                            drawRegion(g, canvas_obj, regionInfo.region, lsz);
                        })
                }
            }

            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }

            if (_this.canvasHook !== undefined) {
                _this.canvasHook(ctrl_or_draw, g, canvas_obj, true);
            }

        }
        } />:<></>
        }

    </div>;

}




export function SingleTargetVIEWUI_BASE(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange } = props;
    const _ = useRef<any>({
        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
    });
    let _this = _.current;
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(undefined);

    useEffect(() => {
        console.log("fsPath:" + fsPath)
        setCacheDef(def);
        return (() => {
        });

    }, [def]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

    function onCacheDefChange(updatedDef: any) {
        setCacheDef(updatedDef);



        (async () => {
            console.log(">>>");
            await BPG_API.InspTargetUpdate(updatedDef)
            await BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
        })()
    }

    useEffect(() => {//////////////////////

        let cbsKey="_"+Math.random();
        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, cbsKey, {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    console.log("++++++++\n", IM, CM, RP);


                    // setDefReport(RP.data)
                    let IMCM = {
                        image_info: IM.image_info,
                        camera_id: CM.data.camera_id,
                        trigger_id: CM.data.trigger_id,
                        trigger_tag: CM.data.trigger_tag,
                    } as type_IMCM

                    _this.imgCanvas.width = IMCM.image_info.width;
                    _this.imgCanvas.height = IMCM.image_info.height;
                    console.log(IMCM.image_info);
                    let ctx2nd = _this.imgCanvas.getContext('2d');

                    if(IMCM.image_info.image instanceof ImageData)
                        ctx2nd.putImageData(IMCM.image_info.image, 0, 0);
                    else if(IMCM.image_info.image instanceof HTMLImageElement)
                        ctx2nd.drawImage(IMCM.image_info.image, 0, 0);

                    setLocal_IMCM(IMCM)
                    let rep = RP.data;
                    setDefReport(rep);

                },
                reject: (pkts) => {

                }
            }

            )

        })()
        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);
            })()

        })
    }, []);


    if (display == false) return null;

    return <div style={{ ...style}} className={"overlayCon"}>

        <div className={"overlay"} >

            {/* {EDIT_UI} */}

        </div>


        <HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
            _this.canvasComp = canvas_obj;
            // console.log(ctrl_or_draw);
            if (ctrl_or_draw == true)//ctrl
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
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }
                    _this.sel_region = PtsToXYWH(canvas_obj.regionSelect.pt1, canvas_obj.regionSelect.pt2);
                }
            }
            else//draw
            {
                let camMag = canvas_obj.camera.GetCameraScale();
                if (Local_IMCM !== undefined) {
                    g.ctx.save();
                    let scale = Local_IMCM.image_info.scale;
                    g.ctx.scale(scale, scale);
                    g.ctx.translate(-0.5, -0.5);
                    g.ctx.drawImage(_this.imgCanvas, 0, 0);
                    g.ctx.restore();
                }
                // drawHooks.forEach(dh=>dh(ctrl_or_draw,g,canvas_obj))


                let ctx = g.ctx;


                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());

                }


            }

            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }
        }
        } />

    </div>;

}


let btn_boxshadow="-3px -3px 5px rgba(255,255,255,0.5),3px 3px 5px rgba(70,70,70,0.3), inset -3px -3px 5px rgba(70, 70, 70, 0.3), inset 3px 3px 5px rgba(255, 255, 255, 0.4)"

export function SingleTargetVIEWUI_JSON_Peripheral(props: CompParam_InspTarUI) {
    let { display, fsPath, EditPermitFlag, style = undefined, renderHook, systemInspTarList, def, report, onDefChange } = props;
    const _ = useRef<any>({
        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        groupTestTIDList: {}
    });
    let TID_OFFSET = -10100;
    let _this = _.current;
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(undefined);

    // const [freq, setFreq] = useState(1800);
    // const [CAM_T, setCAM_T] = useState(3510);
    // const [SEL1_T, setSEL1_T] = useState(10845);
    // const [SEL2_T, setSEL2_T] = useState(13038);



    const [machConfig, setMachConfig] = useState<any>(undefined);


    const [uInspCount, setuInspCount] = useState({SEL1:0,SEL2:0,SEL3:0,NA:0});
    const [processTimeInfo, setProcessTimeInfo] = useState({});
    const [errResetingInfo, setErrResetingInfo] = useState<string|undefined>(undefined);


    const [fileCandList, setFileCandList] = useState({});
    const [fileCandSelectTID, setFileCandSelectTID] = useState("");
    const [fetchSrcTIDList, setFetchSrcTIDList] = useState<number[]>([]);





    const [spanSetupOptionUI, setSpanSetupOptionUI] = useState(false);
    const [imgReviewOptionUI, setImgReviewOptionUI] = useState(false);
    const [inspStatistic, setInspStatistic] = useState<any>({});
    const [latestReport, setLatestReport] = useState<any>(undefined);


    const [spanStatisticUI, setSpanStatisticUI] = useState(false);
    const [isRunning, setIsRunning] = useState(false);
    const [connSendBlock, setConnSendBlock] = useState(false);


    const [periodicPullCMDs, _setPeriodicPullCMDs] = useState<any[]>([]);
    const [runningState, setRunningState] = useState<any>(undefined);
    const [scriptRunningState, setScriptRunningState] = useState(false);
    


    function setPeriodicPullCMDs(CMDs:any[])
    {
        _setPeriodicPullCMDs(CMDs);
        let dataCMDs=CMDs.map(cmd=>{
            let ncmd={...cmd};
            delete ncmd["receive"];
            return ncmd;
        });
        

        console.log(dataCMDs)
        BPG_API.InspTargetExchange(cacheDef.id, { type: "setPeriodicPullCMDs",cmds:dataCMDs });
    }

    useEffect(() => {
        console.log("fsPath:" + fsPath)
        setCacheDef(def);
        return (() => {
        });

    }, [def]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);

    const PeripheralCONNID = cacheDef.stream_id+1000;
    async function delay(ms = 1000) {
        return new Promise((resolve, reject) => setTimeout(resolve, ms))
    }

    async function fetchSetup()
    {

        let setupInfo=await _this.send({ type: "get_setup" })
        // setMachConfig(setupInfo)
        // onDefChange({...def,mach_config:setupInfo},false);
    }

    // console.log(">>>",runningState)
    _this.fileCandList = fileCandList;
    _this.inspStatistic = inspStatistic;
    _this.periodicPullCMDs=periodicPullCMDs;
    _this.runningState=runningState;
    _this.scriptRunningState=scriptRunningState;
    useEffect(() => {//////////////////////

        _this.send_id = 0;
        _this.sendCBDict = {};
        async function pSend(data: any, timeout = 0) {
            if (data.id === undefined) {
                data.id = _this.send_id;
                _this.send_id++;
            }



            return new Promise((resolve, reject) => {
                _this.sendCBDict[data.id] = {
                    resolve,
                    reject
                }

                BPG_API.InspTargetExchange(cacheDef.id, { type: "MESSAGE", msg: data });
                if (timeout > 0)
                    setTimeout(reject, timeout)
            })
        }
        _this.send = pSend;

        BPG_API.send(undefined, 0, { _PGID_: PeripheralCONNID, _PGINFO_: { keep: true } }, undefined,
            {

                resolve: (stacked_pkts) => {
                    let msg = stacked_pkts[0].data.msg;

                    if (_this.sendCBDict[msg.id] !== undefined) {
                        _this.sendCBDict[msg.id].resolve(msg)
                        delete _this.sendCBDict[msg.id];
                    }
                    else
                    {
                        let PD = stacked_pkts.find((p: any) => p.type == "PD");
                        if(PD!==undefined)
                        {
                            // console.log("PD:",PD?.data?.msg);

                            let msg=PD?.data?.msg;
                            if(msg!==undefined && msg.id<0)
                            {

                                // console.log("PD:",msg);

                                let tarCMD=_this.periodicPullCMDs.find((cmd:any)=>cmd.id==msg.id)
                                if(tarCMD)
                                { 
                                    
                                    // console.log("PD:",msg,"tarCMD:",tarCMD);
                                    tarCMD.receive(msg);
                                }
                                // ?.receive(msg);
                            }
                            return;
                        }


                        console.log("event:",stacked_pkts);

                    }



                },
                reject: (stacked_pkts) => {
                    // console.error(">>>>>",stacked_pkts);
                }
            });

        
            
        _this.periodicWatchDog=setInterval(async ()=>{

            let is_Script_Running=false;
            try{
                is_Script_Running=(await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_Script_Running" }) as any)
                [0].data.ACK
            }
            catch(e)
            {

            }

            if(is_Script_Running!=_this.scriptRunningState)
            {
                console.log("is_Script_Running",is_Script_Running,"scriptRunningState",_this.scriptRunningState);
                setScriptRunningState(is_Script_Running);
            }

            
            if(_this.runningState===undefined)return;
            if(Date.now()-_this.runningState.timeStamp>3000)
            {
                setRunningState(undefined);
            }
        },1000);

        
        let cbsKey="_"+Math.random();

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });



            {
                let pCMDs=[];
                let id=-1000;
                pCMDs.push({type:"get_running_stat",id,receive:(msg:any)=>{
                    setRunningState({...msg,timeStamp:Date.now()});
                }});id--;


                setPeriodicPullCMDs(pCMDs);
            }


            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, cbsKey, {

                resolve: (pkts) => {

                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    console.log("++++++++\n", CM, RP);

                    if (RP.data.trigger_id < TID_OFFSET) {
                        let otid = TID_OFFSET - RP.data.trigger_id;
                        if (_this.fileCandList[otid] !== undefined) {
                            let newFileCandList = { ..._this.fileCandList };

                            delete _this.groupTestTIDList[otid]
                            // console.log("otid:", otid);
                            newFileCandList[otid] = { ...newFileCandList[otid], result: RP.data.report.category }
                            setFileCandList(newFileCandList);
                        }

                    }


                    if (1) do {

                        // if (RP.data.report.hole_location_index == -1) break;
                        // if(RP.data.trigger_id<0)break;

                        // let tarRepIdx = RP.data.report.hole_location_index == 0 ? 1 : 0;
                        let ignore_indexes=(RP.data.report.ignore_indexes||[]) as number[];
                        let group = RP.data.report.group;


                        let newStat={..._this.inspStatistic};
                        console.log(RP.data);
                        for(let i=0;i<group.length;i++)
                        {
                            console.log(i,group[i])
                            if(group[i].InspTar_type!="SurfaceCheckSimple")continue;
                            if(ignore_indexes.find(idx=>idx==i)!==undefined)continue;
                            let tarRepIdx=i;

                            let tarRepId = group[tarRepIdx].InspTar_id;

                            let regionsReps = group[tarRepIdx].report.sub_reports[0].sub_regions;
                            let repDef = systemInspTarList.find(def => def.id == tarRepId)
                            // console.log(tarRepIdx,surface_check_reports,regionsReps,repDef)
    
                            let stat = { ..._this.inspStatistic[tarRepId] };
    
                            regionsReps.forEach((rrep: any, index: number) => {
                                let rdef = repDef.sub_regions[index];
                                // repDef
                                let name = rdef.name;
                                // console.log(name, rrep, repDef.sub_regions[index])
                                let cstat = {name, rec: [], OK: 0, NG: 0, NG2: 0, NG3: 0, NA: 0, ...stat[index] };
    
                                if (rrep.category == 1) {
                                    cstat.OK++;
                                }
                                else if (rrep.category == -1) {
                                    cstat.NG++;
                                }
                                else if (rrep.category == -2) {
                                    cstat.NG2++;
                                }
                                else if (rrep.category == -3) {
                                    cstat.NG3++;
                                }
                                else {
                                    cstat.NA++;
                                }
    
    
                                cstat.rec.push(rrep);
                                stat[index] = cstat;
                            })
    
                            newStat[tarRepId]=stat;
                        }

                        // console.log(_this.inspStatistic)

                        setInspStatistic(newStat)
                    } while (false);


                    setLatestReport(RP.data);


                },
                reject: (pkts) => {

                }
            }

            )


            console.log(ret);
            console.log(def);

            let is_CONNECTED = (await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_CONNECTED" }) as any)[0].data.ACK;
            console.error("is_CONNECTED:", is_CONNECTED, " PeripheralCONNID", PeripheralCONNID);

            if (is_CONNECTED == false) {

                await BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID });

                await delay(1000);
            }

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            is_CONNECTED = (await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_CONNECTED" }) as any)[0].data.ACK;
            console.error("is_CONNECTED:", is_CONNECTED);

            
            
            setMachConfig(cacheDef.mach_config)
        })()



        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);
            })()

            clearInterval(_this.periodicWatchDog);

        })
    }, []);


    if (display == false) return null;

    function getCurrentMachInfo(config=machConfig) {
        if(machConfig===undefined)return {};
        let originalInfo={
            plateFreq:config.plateFreq,
            minDetectTimeSep_us:config.minDetectTimeSep_us,
            // CAM1:machConfig.CAM1_on,
            // CAM1_span:machConfig.CAM1_off-machConfig.CAM1_on,

            // L1A:machConfig.L1A_on,
            // L1A_span:machConfig.L1A_off-machConfig.L1A_on,


        };


        ["CAM1","L1A","CAM2","L2A","SEL1","SEL2","SEL3"].forEach(key=>{
            originalInfo[key]=config.stage_pulse_offset[key+"_on"];
            originalInfo[key+"_span"]=config.stage_pulse_offset[key+"_off"]-config.stage_pulse_offset[key+"_on"];
        })

        // console.error(originalInfo);
        return originalInfo as any;
    }

    function calcMachConf(newInfo:{
        plateFreq:number,
        minDetectTimeSep_us:number,
        CAM1:number,
        CAM1_span:number,
        L1A:number,
        L1A_span:number,
        CAM2:number,
        CAM2_span:number,
        L2A:number,
        L2A_span:number,
        SEL1:number,
        SEL1_span:number,
        SEL2:number,
        SEL2_span:number,
        SEL3:number,
        SEL3_span:number}) {
        
        let newConfig={
            plateFreq:newInfo.plateFreq,
            minDetectTimeSep_us:newInfo.minDetectTimeSep_us,
            stage_pulse_offset:{}
        }as any;


        ["CAM1","L1A","CAM2","L2A","SEL1","SEL2","SEL3"].forEach(key=>{
            newConfig.stage_pulse_offset[key+"_on"]=newInfo[key];
            newConfig.stage_pulse_offset[key+"_off"]=newInfo[key]+newInfo[key+"_span"];
        })

        newConfig.stage_pulse_offset["SWITCH"]=newConfig.stage_pulse_offset.SEL1_on-10;

        console.error(newConfig);
        return newConfig;
    }


    async function sendMachConf(info:any) {


        let newMachConf=calcMachConf({...getCurrentMachInfo(),...info});
        console.error(newMachConf);
        setMachConfig(newMachConf)
        let newDef={...def,mach_config:newMachConf}
        onDefChange(newDef,true);
        await BPG_API.InspTargetUpdate(newDef)
        console.log(await _this.send({
            type: "set_setup", stepRun: -1,...newMachConf
        }));
    }

    function trigSimInsp(tid: string) {
        fileCandList[tid].list.forEach((finfo: any) => {
            // console.log(finfo);
            let tags = finfo.info.tags;//.filter((tag:string)=>tag!="SIIS");
            BPG_API.InjectImage(fileCandList[tid].path + "/" + finfo.name, tags, TID_OFFSET - finfo.info.tid)
            setFileCandSelectTID(tid);
        })
    }



    async function LoadFileCandList() {

        let fStruct = await BPG_API.InspTargetEnvFolderStructure("ImDataSave", "", 1);

        let candList = {

        }
        BPG_API.FileStructFilter(fStruct, (fileInfo, folder_path) => {
            // console.log(fileInfo.name,BPG_API.StrInfoParse(fileInfo.name));
            let parseInfo = BPG_API.StrInfoDec(fileInfo.name);
            if (Object.keys(parseInfo).length == 0) return false;
            if (parseInfo === undefined || parseInfo.tid == undefined) return false;
            if (parseInfo.tags.find((tag: string) => tag == "CAM_A" || tag == "CAM_B") === undefined) return false;

            parseInfo.atags = parseInfo.atags || [];
            // if(parseInfo.tags.find(tag=>tag=="CAT_0")===undefined)return false;

            if (candList[parseInfo.tid] === undefined) candList[parseInfo.tid] = { path: folder_path, list: [], additionalTags: [], additionalTagsDict: {}, result: "UN" };
            let curT = candList[parseInfo.tid];
            curT.list.push({ ...fileInfo, info: parseInfo });


            parseInfo.atags.forEach((tag: string) => {
                if (tag.startsWith("$") == false) return;
                curT.additionalTagsDict[tag] = 1;
            })

            return false;
        });

        Object.keys(candList).forEach((candKey: any) => {
            let candInfo = candList[candKey]
            console.log(candInfo);
            candInfo.additionalTags = Object.keys(candInfo.additionalTagsDict);
            delete candInfo["additionalTagsDict"]
        })

        console.log(candList);
        setFileCandList(candList);
        setFileCandSelectTID("");
    }




    async function fileCheck(fileTIDList: string[]) {
        let newFileCandList = { ...fileCandList }

        fileTIDList.forEach((tid: string, index: number) => {
            newFileCandList[tid] = { ...newFileCandList[tid], result: "" }
        })
        setFileCandList(newFileCandList);
        let injectCand: Promise<unknown>[] = [];
        for (let idx = 0; idx < fileTIDList.length; idx++) {
            let tid = fileTIDList[idx];
            let cand = fileCandList[tid];

            let new_injectCand = cand.list.map((finfo: any) => {
                let tags = finfo.info.tags;//.filter((tag:string)=>tag!="SIIS");
                _this.groupTestTIDList[tid] = 1;

                return BPG_API.InjectImage(fileCandList[tid].path + "/" + finfo.name, tags, TID_OFFSET - finfo.info.tid)
            })
            injectCand = injectCand.concat(new_injectCand);
            if (injectCand.length > 20 || idx == fileTIDList.length - 1) {
                await Promise.all(injectCand);
                injectCand = []
            }
        }

    }
    let machInfo  = getCurrentMachInfo();
    // console.log(machConfig,machInfo);



    let curTPS=Math.round(1000000/(machInfo.minDetectTimeSep_us));
    let setupOption = spanSetupOptionUI == false ? null : <>



        錯誤歷史:   
        {runningState?.ERROR_HIST?.map((err:number)=><Tag color="red">{err}</Tag>)}
        



        <Popconfirm
        title="確定重置錯誤列表?"
        onConfirm={()=>{
            (async () => {
                let ret = await _this.send({ type: "clear_error_history" });
                // setuInspCount(ret.count)
            })()
        }}
        onCancel={()=>{
        }}
        okText="OK"
        cancelText="NO"
        >
            
            <Button  size="large" danger>
                <DeleteOutlined/>
            </Button>
        </Popconfirm>



        <Popconfirm
        title="確定重置計數?"
        onConfirm={()=>{
            (async () => {
                let ret = await _this.send({ type: "reset_running_stat" });
                // setuInspCount(ret.count)
            })()
        }}
        onCancel={()=>{
        }}
        okText="OK"
        cancelText="NO"
        >
            
            <Button  size="large" danger>
            重置全檢計數<DeleteOutlined/>
            </Button>
        </Popconfirm>


        <br/>

        <Button onClick={() => {

        (async () => {
        
            await fetchSetup();
        })()

        }}>GetSetup</Button>

        <Button onClick={() => {

        (async () => {
            // let is_CONNECTED = (await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_CONNECTED" }) as any)[0].data.ACK;
            // console.error("is_CONNECTED:", is_CONNECTED, " PeripheralCONNID", PeripheralCONNID);
            // if (is_CONNECTED == false) {
            //     await BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID });
            // }

            await BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID });
            // await fetchSetup();
        })()

        }}>CONNECT</Button>



        <Button onClick={() => {

        (async () => {
            await BPG_API.InspTargetExchange(cacheDef.id, { type: "DISCONNECT", comm_id: PeripheralCONNID });

        })()

        }}>DISCONNECT</Button>


        <Button onClick={() => {

        (async () => {
            sendMachConf({...machInfo,plateFreq:machInfo.plateFreq+100});
        })()

        }}>轉速+ {machInfo.plateFreq}</Button>



        <Button onClick={() => {

        (async () => {
            let plateFreq=machInfo.plateFreq-100;
            if(plateFreq<0)plateFreq=0;
            sendMachConf({...machInfo,plateFreq});
        })()
        }}>-</Button>



        <Button onClick={() => {

        (async () => {
            curTPS+=1;
            sendMachConf({...machInfo,minDetectTimeSep_us:Math.round(1000000/curTPS)});
        })()

        }}>偵速+ {curTPS}</Button>

        <Button onClick={() => {

        (async () => {

            curTPS-=1;
            if(curTPS<1)curTPS=1;
            sendMachConf({...machInfo,minDetectTimeSep_us:Math.round(1000000/curTPS)});
        })()
        }}>-</Button>

        <br/>
        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,CAM1:machInfo.CAM1+10,L1A:machInfo.CAM1+10});
            })()

        }}>CAM1+ {machInfo.CAM1}</Button>



        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,CAM1:machInfo.CAM1-10,L1A:machInfo.CAM1-10});
            })()
        }}>-</Button>



        <Button onClick={() => {


            (async () => {
                sendMachConf({...machInfo,SEL1:machInfo.SEL1+5});
            })()


        }}>SE1+5 {machInfo.SEL1}</Button>

        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL1:machInfo.SEL1+1});
            })()
        }}>+</Button>


        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL1:machInfo.SEL1-1});
            })()
        }}>-</Button>

        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL1:machInfo.SEL1-5});
            })()

        }}>-5</Button>



        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL2:machInfo.SEL2+5});
            })()

        }}>SE2+5 {machInfo.SEL2}</Button>

        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL2:machInfo.SEL2+1});
            })()
        }}>+</Button>
        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL2:machInfo.SEL2-1});
            })()

        }}>-</Button>


        <Button onClick={() => {

            (async () => {
                sendMachConf({...machInfo,SEL2:machInfo.SEL2-5});
            })()

        }}>-5</Button>

        <br />


        <Button onClick={() => {

            (async () => {
                console.log(await _this.send({ type: "PIN_ON", pin: 16 }));
            })()

        }}>LON</Button>

        <Button onClick={() => {

            (async () => {
                console.log(await _this.send({ type: "PIN_OFF", pin: 16 }));
            })()

        }}>LOFF</Button>


        <Button onClick={() => {

            (async () => {
                console.log(await _this.send({ type: "sel_act", idx: 1 }));
            })()

        }}>SEL1</Button>
        <Button onClick={() => {

            (async () => {
                console.log(await _this.send({ type: "sel_act", idx: 2 }));
            })()

        }}>SEL2</Button>
        <Button onClick={() => {

            (async () => {
                console.log(await _this.send({ type: "sel_act", idx: 3 }));
            })()

        }}>SEL3</Button>




        <br />

        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "OK_OK" });
        }}>T_OKOK</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "OK_NA", space: [1, 20] });
        }}>T_OKNA</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "OK_NG", space: [1, 1] });
        }}>T_OKNG</Button>

        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "NG_NG" });
        }}>T_NGNG</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "NG_NA", space: [1, 20] });
        }}>T_NGNA</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "NA_NA" });
        }}>T_NANA</Button>


        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "TEST_MODE", mode: "" });
        }}>T_NORMAL</Button>


        <br />


{/* 
        <Button onClick={() => {


            console.log(_this.send({ type: "sel1_act_countdown", count: 10 }));

        }}>sel1_10</Button>

        <Button onClick={() => {


            console.log(_this.send({ type: "sel1_act_countdown", count: 100 }));

        }}>sel1_100</Button>

        <Button onClick={() => {


            console.log(_this.send({ type: "sel1_act_countdown", count: -1 }));

        }}>sel1_no limit</Button> */}
        <br />



        



        <br />
    </>



    let ImgReviewOption=imgReviewOptionUI == false ? null : <>
    
        存檔
        <Button onClick={() => {


        BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 1000000, count_NG: 1000000, count_NA: 0 });

        }}>ALL OK NG</Button>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 1000000, count_NG: 0, count_NA: 0 });

        }}>OK</Button>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 1, count_NG: 0, count_NA: 0 });

        }}>1OK</Button>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 0, count_NG: 20,count_NG2: 20, count_NA: 0 });

        }}>NG</Button>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 0, count_NG: 1,count_NG2: 1, count_NA: 0 });

        }}>1NG</Button>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 0, count_NG: 0, count_NA: 100 });

        }}>NA</Button>

        <Divider  type="vertical"></Divider>
        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 20, count_NG: 20,count_NG2: 20, count_NA: 5 });

        }}>20OK&NG 5NA</Button>






        <Divider  type="vertical"></Divider>

        <Button onClick={() => {


            BPG_API.InspTargetExchange(cacheDef.id, { type: "SrcImgSaveCountDown", count_OK: 0, count_NG: 0,count_NG2: 0, count_NA: 0 });

        }}>不存</Button>
        {
            (() => {


                if (fileCandSelectTID.length == 0) return null;

                let fileCandInfo = fileCandList[fileCandSelectTID];
                if (fileCandInfo === undefined) return null;



                function chooseNextUNSET(_fileCandList = fileCandList) {
                    let keys = Object.keys(_fileCandList);
                    let tar_tid = "";
                    for (let i = 0; i < keys.length; i++) {
                        let tid = keys[i];
                        let tag = _fileCandList[tid].additionalTags.find((tag: string) => tag.startsWith("$CAT_"));
                        console.log(tag);
                        if (tag === undefined) {
                            tar_tid = tid;
                            trigSimInsp(tar_tid);
                            break;
                        }

                    }

                    setFileCandSelectTID(tar_tid);
                }


                let additionalTags = fileCandInfo.additionalTags as string[];
                if (additionalTags === undefined) additionalTags = [];
                // console.log(fileCandInfo);

                function CATChange(toCAT: string) {
                    let newTags = [...additionalTags.filter(tag => !tag.startsWith("$CAT_"))];
                    if (toCAT != "") {
                        newTags = [...newTags, "$CAT_" + toCAT]
                    }
                    let newFileCandList = ObjShellingAssign(fileCandList, [fileCandSelectTID, "additionalTags"], newTags);
                    setFileCandList(newFileCandList);
                    chooseNextUNSET(newFileCandList);
                }

                return <>
                    {JSON.stringify(fileCandInfo.list[0].info)}
                    {JSON.stringify(fileCandInfo.additionalTags)}
                    <br />
                    <Button type="primary" onClick={() => {
                        CATChange("OK");
                    }}>OK</Button>
                    <Button danger onClick={() => {
                        CATChange("NG_1");
                    }}>NG1</Button>
                    <Button danger onClick={() => {
                        CATChange("NG_2");
                    }}>NG2</Button>
                    <Button type="dashed" onClick={() => {
                        CATChange("NA");
                    }}>NA</Button>
                    <Button onClick={() => {
                        CATChange("");
                    }}>UNSET</Button>


                    <Button onClick={() => {
                        trigSimInsp(fileCandSelectTID);
                    }}>CHECK</Button>
                    <br />
                </>
            })()
        }

        <Divider orientation="left"></Divider>


        <Button onClick={() => {

            LoadFileCandList();
        }}>Update</Button>

        <Button onClick={() => {
            _this.groupTestTIDList = {};
            BPG_API.InspTargetExchange("ImTran", { type: "force_down_scale", scale: -1 });

            fileCheck(Object.keys(fileCandList))

        }}>TEST ALL:{
                Object.keys(_this.groupTestTIDList).length + "/" + Object.keys(fileCandList).length}</Button>

        <Button onClick={() => {
            Object.keys(fileCandList).forEach((tid, index) => {
                let fileInfo = fileCandList[tid];
                let atags = fileInfo.additionalTags;
                fileInfo.list.forEach((file: any, findex: number) => {

                    let info = file.info;
                    info.atags = atags;
                    let fileExt = file.name.split('.').pop();
                    let name = BPG_API.Enc2StrInfo(file.info)
                    let newName = name + "." + fileExt;
                    if (file.name != newName) {
                        console.log(fileInfo);
                        // console.log(file.name,">",newName);

                        let folderPath = fileInfo.path;

                        BPG_API.FileRename(folderPath + "/" + file.name, folderPath + "/" + newName)

                    }
                })


            })


            LoadFileCandList();



        }}>SAVE ATAGS</Button>

        <Row justify="center" align="top">
            <Col>

                <Button onClick={() => {

                    _this.groupTestTIDList = {};
                    Object.keys(fileCandList)


                    fileCheck(Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.every((tag: string) => !tag.startsWith("$CAT"))))

                }}>UNSET</Button>
                <br />



                {
                    Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.every((tag: string) => !tag.startsWith("$CAT_")))
                        .map((tid) => {


                            return <Button size='small' key={tid} onClick={() => {
                                trigSimInsp(tid);
                            }}>{tid + ":" + fileCandList[tid].result}</Button>
                        })
                }
            </Col>
        </Row>
        <Row justify="center" align="top">
            <Col span={7}>
                <Button onClick={() => {

                    _this.groupTestTIDList = {};
                    Object.keys(fileCandList)


                    fileCheck(Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_OK"))))

                }}>OK</Button>
                <br />


                {
                    Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_OK")))
                        .map((tid) => {


                            return <Button size='small' danger={fileCandList[tid].result != 1} key={tid} onClick={() => {
                                trigSimInsp(tid);
                            }}>{tid + ":" + fileCandList[tid].result}</Button>
                        })
                }
            </Col>
            <Col span={7}>
                <Button onClick={() => {

                    _this.groupTestTIDList = {};

                    fileCheck(Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_NG"))))

                }}>NG</Button>
                <br />
                {
                    Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_NG_1")))
                        .map((tid) => {


                            return <Button size='small' danger={fileCandList[tid].result != -1} key={tid} onClick={() => {
                                trigSimInsp(tid);
                            }}>{tid + ":" + fileCandList[tid].result}</Button>
                        })
                }

            </Col>
            <Col span={7}>
                NG2:<br />
                {
                    Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_NG_2")))
                        .map((tid) => {


                            return <Button size='small' key={tid} danger={fileCandList[tid].result != -1} onClick={() => {
                                trigSimInsp(tid);
                            }}>{tid + ":" + fileCandList[tid].result}</Button>
                        })
                }

            </Col>
            <Col span={3}>
                <Button type='primary' onClick={() => {

                    _this.groupTestTIDList = {};

                    fileCheck(Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_NA"))))

                }}>NA</Button>
                <br />

                {
                    Object.keys(fileCandList)
                        .filter(tid => fileCandList[tid].additionalTags.some((tag: string) => tag.startsWith("$CAT_NA")))
                        .map((tid) => {


                            return <Button size='small' key={tid} danger={fileCandList[tid].result != 0} onClick={() => {
                                trigSimInsp(tid);
                            }}>{tid + ":" + fileCandList[tid].result}</Button>
                        })
                }
            </Col>
        </Row>


       <Divider orientation="left">暫存圖訊</Divider>


        
        <Button onClick={() => {
            if(fetchSrcTIDList.length!=0)
            {
                setFetchSrcTIDList([]);
                return;
            }
            (async () => {
                let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "GetFetchSrcTIDList" }) as any;
                console.log(ret[0].data);
                setFetchSrcTIDList(ret[0].data);
            })()
        }}>更新</Button>

        <Button onClick={() => {

            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: -1, count_NG: 0, count_NG2: 0, count_NA: 0 });
        }}>OK only</Button>

        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: 0, count_NG: -1, count_NG2: 0, count_NA: 0 });
        }}>NG only</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: 0, count_NG: 0, count_NG2: -1, count_NA: 0 });
        }}>NG2 only</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: 0, count_NG: 0, count_NG2: 0, count_NA: -1 });
        }}>NA only</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: -1, count_NG: -1, count_NG2: -1, count_NA:0 });
        }}>not NA only</Button>
        <Button onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "FetchCountDown", count_OK: -1, count_NG: -1, count_NG2: 0, count_NA: -1 });
        }}>ALL</Button>

        <br/>
        {
            fetchSrcTIDList.map((tid, index) => {
                return <Button key={"trigTID_" + tid + "_" + index} onClick={() => {
                    BPG_API.InspTargetExchange(cacheDef.id, { type: "TriggerFetchSrc", trigger_id: tid })
                }}>{tid}</Button>
            })
        }   
        {

            (fetchSrcTIDList.length==0)?null:<Button onClick={() => {

                (async () => {
                for(let i=0;i<fetchSrcTIDList.length;i++)
                {
                    await BPG_API.InspTargetExchange(cacheDef.id, { type: "TriggerFetchSrc", trigger_id: fetchSrcTIDList[i] })
                }
                })()


            }}>測試全部暫存圖檔</Button>
        }

        <Button danger onClick={() => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "ClearFetchSrc" });
        }}>清除暫存圖</Button>

    </>;

    function NumToStrWithPadding(num:any,padding:number=4)
    {
        if(num===undefined)return "----";
        let str=(typeof num === 'number')?num.toString():"";
        while(str.length<padding)
        {
            str="0"+str;
        }
        return str;
    }

    let iconSize_B='50px'
    let iconSize_S='30px'
    let RoundBGSize_S='40px'

    let tagSize={ fontSize: '15px', padding: '4px 8px' }


    let evStateRunning=(runningState?.plateFreq!=0);
    let isRealRunning=isRunning||(evStateRunning);

    return <div style={{ ...style }} className={"overlayCon"}>
        <div className={"overlay scroll HXF"} style={{width:"100%",pointerEvents:errResetingInfo===undefined?undefined: 'none'}} >



            <Row align="middle">
                <Divider type="vertical"/>
                {
                runningState===undefined?<Avatar size={100} icon={connSendBlock?<LoadingOutlined/>:<LinkOutlined />}  style={{boxShadow:btn_boxshadow, backgroundColor:connSendBlock?"#AAA":"#5F5" }} 
                    onClick={()=>{
                        if(connSendBlock)return;

                        setConnSendBlock(true);
                        BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID })
                        setTimeout(()=>{
                            setConnSendBlock(false);
                        },2000);
                    }}/>:
                <Avatar size={100} icon={isRealRunning?<><PauseCircleFilled /></>:<><PlayCircleFilled /></>}  style={{ 
                    boxShadow:btn_boxshadow,
                    backgroundColor:
                    (runningState===undefined || evStateRunning!=isRealRunning)?undefined:( isRealRunning?"#F55":'#5F5') }} 
                    onClick={()=>{


                        if(isRealRunning)
                        {
                            setIsRunning(false);
                            (async () => {
                                console.log(await _this.send({ type: "set_setup", plateFreq: 0 }));
                                console.log(await _this.send({ type: "exit_insp_mode" }));
    
                            })();
                            return;
                        }
    
                        setIsRunning(true);
                        (async () => {
                            (async () => {
                                await sendMachConf({...machInfo});
                            })()
                            console.log(await _this.send({ type: "clear_error" }));
                            console.log(await _this.send({ type: "enter_insp_mode" }));
                        })();

                    }}
                    
                    
                    />
                }
                {/* <div style={{width:"20px"}} ></div> */}

                <Divider type="vertical"/>

                <Col>

                <Row align="middle">

                <div>


                <Tag color="green" style={tagSize}>{NumToStrWithPadding(runningState?.count?.SEL1)}</Tag>
                <Tag color="red" style={tagSize}>{NumToStrWithPadding(runningState?.count?.SEL2)}</Tag>
                <Tag color="yellow" style={tagSize}>{NumToStrWithPadding(runningState?.count?.SEL3)}</Tag>
                <Tag color="gray" style={tagSize}>{NumToStrWithPadding(runningState?.count?.NA)}</Tag>


                </div>

                {/* <Button  size="large" danger onClick={() => {

                (async () => {
                    let ret = await _this.send({ type: "reset_running_stat" });
                    setuInspCount(ret.count)
                })()

                }}>
                </Button> */}

                </Row>

                <Row align="middle">

                <Tag color={runningState?.state!==undefined?"green":"red"} style={tagSize}>檢驗機狀態碼{runningState?.state||"離線"}</Tag>
                <Tag color={scriptRunningState?"green":"red"} style={tagSize}>腳本{scriptRunningState?"運行中":"離線"}</Tag>







                </Row>
                </Col>

            </Row>

            <Row justify="center">

            
                <Col>
                <Avatar shape="square" size={100} icon={CAT_ID_NAME[latestReport?.report?.category+""]}  style={{ 
                width:"200px",
                boxShadow:btn_boxshadow,
                backgroundColor:CAT_ID_Color[latestReport?.report?.category+""],}}
                onClick={()=>{


                }}
                
                
                />


                </Col>

            </Row>
            <br/>
            <br/>

            <Row align="middle">
            <Button onClick={() => {

            (async () => {

                console.log(await _this.send({ type: "stepper_enable" }));
            })()

            }}>馬達ON</Button>


            <Button onClick={() => {

            (async () => {

            console.log(await _this.send({ type: "stepper_disable" }));
            })()

            }}>OFF</Button>
            <Divider type="vertical"/>
            <Button  size="large" onClick={() => {
                
                (async () => {
                    for (let i = 0; i < 1; i++) {
                        _this.send({ type: "trig_phamton_pulse" })
                        await delay(1000/25);
                    }
                })()
            }}>
            <CameraOutlined/>
            </Button>


                
            </Row>

            <Popconfirm
                disabled={errResetingInfo!==undefined}
                title="確定重置錯誤?"
                onConfirm={()=>{
                   
                    (async () => {
                        

                        try{

                            setErrResetingInfo("機台:停轉");
                            console.log(await _this.send({ type: "set_setup", plateFreq: 0 },1000));
                            await delay(500);


                        
                            setErrResetingInfo("機台:離開檢驗模式");await delay(500)
                            console.log(await _this.send({ type: "exit_insp_mode" }));
    
                            setErrResetingInfo("機台:等待停止3");
                            await delay(1000)
                            setErrResetingInfo("機台:等待停止2");
                            await delay(1000)
                            setErrResetingInfo("機台:等待停止1");
                            await delay(1000)

                            setErrResetingInfo("機台:清理錯誤Flag");await delay(500)
                            console.log(await _this.send({ type: "clear_error" }));
                        }
                        catch(e)
                        {

                        }
                        setIsRunning(false);

                        setErrResetingInfo("核心:清理暫存圖像");
                        await BPG_API.CameraClearTriggerInfo();
                        setErrResetingInfo("核心:清理檢驗超時偵測");await delay(500)
                        let pt_info=await BPG_API.InspTargetExchange(cacheDef.id, { type: "GetProcessTimeInfo",reset:true }) as any


                        setProcessTimeInfo({})
                        setErrResetingInfo("結束...");await delay(500)
                        setErrResetingInfo(undefined);
                    })()
                }}
                onCancel={()=>{
                }}
                okText="OK"
                cancelText="NO"
                >
                    
                    <Button danger disabled={errResetingInfo!==undefined} onClick={() => {
                    }}>系統重置{errResetingInfo!==undefined?<><LoadingOutlined/>{errResetingInfo}</>:""}</Button>


                </Popconfirm>


            

                <Button onClick={() => {
                    (async () => {
                        let pt_info=await BPG_API.InspTargetExchange(cacheDef.id, { type: "GetProcessTimeInfo" }) as any
                        let pti=pt_info[0].data;
                        console.log(pti);
                        setProcessTimeInfo(pti)
                    })()
                }}>處理時間:{JSON.stringify(processTimeInfo)}</Button>
                


                <Button onClick={() => {
                    BPG_API.InspTargetExchange(cacheDef.id, { type: "reloadscript" })
                }}>reloadscript</Button>
                
            
            {/* <Button onClick={() => {

                (async () => {
                    console.log(await _this.send({ type: "clear_error" }));
                })()

            }}>錯誤重置</Button> */}


            <br />

            




            <Divider orientation="left">
                <Button onClick={() => {
                    setImgReviewOptionUI(!imgReviewOptionUI)
                }}>圖像測試{imgReviewOptionUI ? ' -' : ' +'}</Button>
            </Divider>
            {ImgReviewOption}



            <Divider orientation="left">
                <Button onClick={() => {
                    setSpanSetupOptionUI(!spanSetupOptionUI)
                }}>細部設定{spanSetupOptionUI ? ' -' : ' +'}</Button>
            </Divider>

            {setupOption}




            <Divider orientation="left">
                <Button onClick={() => {
                    setSpanStatisticUI(!spanStatisticUI)
                }}>統計顯示{spanStatisticUI ? ' -' : ' +'}</Button>
            </Divider>

            {
                spanStatisticUI==false?null:<>{
                    
                    Object.keys(inspStatistic).map(key=>{
                
                    let inspTarStat=inspStatistic[key];
                    return <>
                        {key}<br/>
                        {
                            Object.keys(inspTarStat).map(itskey=><>
                                {"O:"+inspTarStat[itskey].OK}
                                {" X:"+inspTarStat[itskey].NG+","+inspTarStat[itskey].NG2+","+inspTarStat[itskey].NG3}
                                {" N:"+inspTarStat[itskey].NA} 
                                ---{inspTarStat[itskey].name}<br/>
                            </>)

                            // inspStatistic[key].map((stat:any,index:number)=>{

                            //     <Statistic title={index+"_"} value={stat} />
                            // })
                        }
                    </>})

                }
                <Button onClick={() => {
                    setInspStatistic({})
                }}>RESET</Button>
                </>
            }



        </div>


    </div>;

}





export function SingleTargetVIEWUI_JSON_CNC_Peripheral(props: CompParam_InspTarUI) {
    let { display, fsPath, EditPermitFlag, style = undefined, renderHook, systemInspTarList, def, report, onDefChange } = props;
    const _ = useRef<any>({
        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        groupTestTIDList: {}
    });
    let TID_OFFSET = -10100;
    let _this = _.current;
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(undefined);

    // const [freq, setFreq] = useState(1800);
    // const [CAM_T, setCAM_T] = useState(3510);
    // const [SEL1_T, setSEL1_T] = useState(10845);
    // const [SEL2_T, setSEL2_T] = useState(13038);

    const [machConfig, setMachConfig] = useState<any>(undefined);

    const [processTimeInfo, setProcessTimeInfo] = useState({});

    const [fileCandList, setFileCandList] = useState({});
    const [fileCandSelectTID, setFileCandSelectTID] = useState("");
    const [fetchSrcTIDList, setFetchSrcTIDList] = useState<number[]>([]);





    const [spanSetupOptionUI, setSpanSetupOptionUI] = useState(false);

    const [inspStatistic, setInspStatistic] = useState<any>({});
    const [spanStatisticUI, setSpanStatisticUI] = useState(false);


    useEffect(() => {
        console.log("fsPath:" + fsPath)
        setCacheDef(def);
        return (() => {
        });

    }, [def]);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);



    const [connSendBlock, setConnSendBlock] = useState(false);
    const [periodicPullCMDs, _setPeriodicPullCMDs] = useState<any[]>([]);
    const [runningState, setRunningState] = useState<any>(undefined);
    function setPeriodicPullCMDs(CMDs:any[])
    {
        _setPeriodicPullCMDs(CMDs);
        let dataCMDs=CMDs.map(cmd=>{
            let ncmd={...cmd};
            delete ncmd["receive"];
            return ncmd;
        });
        

        console.log(dataCMDs)
        BPG_API.InspTargetExchange(cacheDef.id, { type: "setPeriodicPullCMDs",cmds:dataCMDs });
    }




    const PeripheralCONNID = 3456;
    async function delay(ms = 1000) {
        return new Promise((resolve, reject) => setTimeout(resolve, ms))
    }

    async function fetchSetup()
    {

        let setupInfo=await _this.send({ type: "get_setup" })
        console.log(setupInfo);
        // setMachConfig(setupInfo)
        // onDefChange({...def,mach_config:setupInfo},false);
    }

    _this.fileCandList = fileCandList;
    _this.inspStatistic = inspStatistic;
    _this.runningState = runningState;
    _this.periodicPullCMDs=periodicPullCMDs;
    useEffect(() => {//////////////////////

        _this.send_id = 0;
        _this.sendCBDict = {};
        async function pSend(data: any, timeout = 0) {
            if (data.id === undefined) {
                data.id = _this.send_id;
                _this.send_id++;
            }

            let retPkgs=await BPG_API.InspTargetExchange(cacheDef.id, { type: "MESSAGE", msg: data }) as any[];
            let retMsg = retPkgs.find((p: any) => p.type == "PD");
            return retMsg?.data?.msg;
            // return new Promise((resolve, reject) => {
            //     _this.sendCBDict[data.id] = {
            //         resolve,
            //         reject
            //     }

            //     BPG_API.InspTargetExchange(cacheDef.id, { type: "MESSAGE", msg: data });
            //     if (timeout > 0)
            //         setTimeout(reject, timeout)
            // })
        }
        _this.send = pSend;

        _this.periodicWatchDog=setInterval(()=>{
            if(_this.runningState===undefined)return;
            if(Date.now()-_this.runningState.timeStamp>2000)
            {
                setRunningState(undefined);
                console.log("runningState timeout...");
            }
        },1000);



        if(0){
            let pCMDs=[];
            let id=-1000;
            pCMDs.push({type:"get_running_stat",id,receive:(msg:any)=>{
                // console.log(">>");
                setRunningState({...msg,timeStamp:Date.now()});
            }});id--;


            setPeriodicPullCMDs(pCMDs);
        }


        BPG_API.send(undefined, 0, { _PGID_: PeripheralCONNID, _PGINFO_: { keep: true } }, undefined,
            {

                resolve: (stacked_pkts) => {
                    let PD = stacked_pkts[0];
                    
                    let msg=PD?.data?.msg;
                    if(msg!==undefined && msg.id<0)
                    {


                        let tarCMD=_this.periodicPullCMDs.find((cmd:any)=>cmd.id==msg.id)
                        if(tarCMD)
                        { 
                            tarCMD.receive(msg);
                        }
                        // ?.receive(msg);
                    }



                },
                reject: (stacked_pkts) => {
                    // console.error(">>>>>",stacked_pkts);
                }
            });

        
        let cbsKey="_"+Math.random();

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });


            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, cbsKey, {

                resolve: (pkts) => {
                    console.log(pkts);
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    // console.log("++++++++\n", CM, RP);

                    if (RP.data.trigger_id < TID_OFFSET) {
                        let otid = TID_OFFSET - RP.data.trigger_id;
                        if (_this.fileCandList[otid] !== undefined) {
                            let newFileCandList = { ..._this.fileCandList };

                            delete _this.groupTestTIDList[otid]
                            // console.log("otid:", otid);
                            newFileCandList[otid] = { ...newFileCandList[otid], result: RP.data.report.category }
                            setFileCandList(newFileCandList);
                        }

                    }


                    if (1) do {

                        // if (RP.data.report.hole_location_index == -1) break;
                        // if(RP.data.trigger_id<0)break;

                        // let tarRepIdx = RP.data.report.hole_location_index == 0 ? 1 : 0;
                        let surface_check_reports = RP.data.report.surface_check_reports;


                        let newStat={..._this.inspStatistic};
                        console.log();
                        for(let i=0;i<surface_check_reports.length;i++)
                        {
                            console.log(i,surface_check_reports[i].InspTar_id)
                            let tarRepIdx=i;

                            let tarRepId = surface_check_reports[tarRepIdx].InspTar_id;

                            let regionsReps = surface_check_reports[tarRepIdx].report.sub_reports[0].sub_regions;
                            let repDef = systemInspTarList.find(def => def.id == tarRepId)
                            // console.log(tarRepIdx,surface_check_reports,regionsReps,repDef)
    
                            let stat = { ..._this.inspStatistic[tarRepId] };
    
                            regionsReps.forEach((rrep: any, index: number) => {
                                let rdef = repDef.sub_regions[index];
                                // repDef
                                let name = rdef.name;
                                // console.log(name, rrep, repDef.sub_regions[index])
                                let cstat = {name, rec: [], OK: 0, NG: 0, NG2: 0, NG3: 0, NA: 0, ...stat[index] };
    
                                if (rrep.category == 1) {
                                    cstat.OK++;
                                }
                                else if (rrep.category == -1) {
                                    cstat.NG++;
                                }
                                else if (rrep.category == -2) {
                                    cstat.NG2++;
                                }
                                else if (rrep.category == -3) {
                                    cstat.NG3++;
                                }
                                else {
                                    cstat.NA++;
                                }
    
    
                                cstat.rec.push(rrep);
                                stat[index] = cstat;
                            })
    
                            newStat[tarRepId]=stat;
                        }

                        // console.log(_this.inspStatistic)

                        setInspStatistic(newStat)
                    } while (false);




                },
                reject: (pkts) => {

                }
            }

            )


            console.log(ret);
            console.log(def);

            let is_CONNECTED = (await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_CONNECTED" }) as any)[0].data.ACK;
            console.error("is_CONNECTED:", is_CONNECTED, " PeripheralCONNID", PeripheralCONNID);

            if (is_CONNECTED == false) {

                await BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID });

                await delay(1000);
            }

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            is_CONNECTED = (await BPG_API.InspTargetExchange(cacheDef.id, { type: "is_CONNECTED" }) as any)[0].data.ACK;
            console.error("is_CONNECTED:", is_CONNECTED);
            
            setMachConfig(cacheDef.mach_config)
        })()



        return (() => {
            (async () => {
                await BPG_API.send_cbs_detach(
                    cacheDef.stream_id, cbsKey);
            })()

            clearInterval(_this.periodicWatchDog);

        })
    }, []);


    if (display == false) return null;


    return <div style={{ ...style }} className={"overlayCon"}>
        <div className={"overlay scroll HXF"} >


        {runningState!==undefined?<Avatar size={100} icon={<DisconnectOutlined />}  style={{boxShadow:btn_boxshadow, backgroundColor:"#F55" }} 
        onClick={()=>{
            
            BPG_API.InspTargetExchange(cacheDef.id, { type: "DISCONNECT", comm_id: PeripheralCONNID });
            setRunningState(undefined);
        }}/>:
        <Avatar size={100} icon={connSendBlock?<LoadingOutlined/>:<LinkOutlined />}  style={{boxShadow:btn_boxshadow, backgroundColor:connSendBlock?"#AAA":"#5F5" }} 
        onClick={()=>{
            if(connSendBlock)return;

            setConnSendBlock(true);
            BPG_API.InspTargetExchange(cacheDef.id, { type: "CONNECT", comm_id: PeripheralCONNID })
            setTimeout(()=>{
                setConnSendBlock(false);
            },2000);
        }}/>}






        <Button onClick={() => {

        (async () => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "DISCONNECT", comm_id: PeripheralCONNID })
        })()

        }}>Disconnect</Button>





        <Button onClick={() => {

        (async () => {

            await fetchSetup();
        })()

        }}>GetSetup</Button>





        {/* <Button onClick={() => {
            (async () => {
                navigator.mediaDevices.getUserMedia({ audio: true, video: true })
                
                let devices = (await navigator.mediaDevices.enumerateDevices()).filter(d => d.kind == "videoinput");
                console.log(devices);
                // await fetchSetup();
            })()

        }}>DDDDDD</Button> */}


        {/* <Button onClick={() => {
            (async () => {

                await BPG_API.CameraSWTrigger("Hikvision-2BDF73541011-00E73541011","CAM_FB",0,true);
            })()

        }}>CAM_TRIG</Button> */}

        </div>


    </div>;

}

