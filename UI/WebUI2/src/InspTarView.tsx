import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Button, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select } from 'antd';

const { Option } = Select;

import type { MenuProps, MenuTheme } from 'antd/es/menu';
const { SubMenu } = Menu;
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleOutlined,
    DisconnectOutlined, LinkOutlined
} from '@ant-design/icons';

import clone from 'clone';

import { StoreTypes } from './redux/store';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { listCMDPromise } from './XCMD';


import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d } from './UTIL/MathTools';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { Row, Col, Input, Tag, Modal, message } from 'antd';


import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';

let DAT_ANY_UNDEF: any = undefined;


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
            image: ImageData
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

function drawRegion(g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, region: { x: number, y: number, w: number, h: number }, lineWidth: number, drawCenterPoint: boolean = true) {
    let ctx = g.ctx;
    // ctx.lineWidth = 5;

    let x = region.x;
    let y = region.y;
    let w = region.w;
    let h = region.h;
    ctx.beginPath();
    ctx.setLineDash([lineWidth * 10, lineWidth * 3, lineWidth * 3, lineWidth * 3]);
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


type CompParam_InspTarUI = {
    display: boolean,
    style?: any,
    stream_id: number,
    fsPath: string,
    EditPermitFlag: number,
    width: number, height: number,
    renderHook: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, rule: any) => void) | undefined,
    // IMCM_group:IMCM_group,
    def: any,
    report: any,
    onDefChange: (updatedDef: any, ddd: boolean) => void
}



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





export function SingleTargetVIEWUI_ColorRegionDetection({ display, stream_id, fsPath, width, height, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }: CompParam_InspTarUI) {
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

            await BPG_API.CameraSWTrigger("Hikrobot-00F71598890", "TTT", 4433)

            // await BPG_API.CameraSWTrigger("BMP_carousel_0","TTT",4433)

        })()

    }


    useEffect(() => {//////////////////////

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, "ColorRegionDetection", {

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
                    ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


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
                    stream_id, "ColorRegionDetection");

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



    return <div style={{ ...style, width: width + "%", height: height + "%" }} className={"overlayCon"}>

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

                    let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
                    let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);


                    // console.log(canvas_obj.regionSelect);
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
                    _this.sel_region = {
                        x, y, w, h
                    }
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
    v = Math.max(rabs, gabs, babs),
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


function TestInputSelectUI({ folderPath, stream_id, testTags=[]}: { folderPath: string, stream_id: number,testTags:string[] }) {
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

    function ImgTest(folder_path: string, fileInfo: { name: string },tags:string[]=[]) {
        let sIDTag = injectID_Prefix + fileInfo.name;
        // let final_tags=[sIDTag,...tags];
        let final_tags=[...tags];
        
        console.log(final_tags);
        BPG_API.InjectImage(folder_path + "/" + fileInfo.name,final_tags, Date.now());

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
                ImgTest(imageFolderInfo.path, file,testTags);
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
                    ImgTest(imageFolderInfo.path, file,testTags);

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
    let { display, stream_id, fsPath, width, height, style = undefined, renderHook, def, EditPermitFlag, report, onDefChange }=props;
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        drawHooks: [],
        ctrlHooks: [],



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

            // await BPG_API.CameraSWTrigger("Hikrobot-00F92938639","TTT",4433)
            if (doTakeNewImage)
                await BPG_API.CameraSWTrigger("BMP_carousel_0", "s_Step_25", Date.now())

        })()

    }


    useEffect(() => {//////////////////////

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, "KEY_KEY_Orientation_ShapeBasedMatching", {

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
                    ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


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
                    stream_id, "KEY_KEY_Orientation_ShapeBasedMatching");

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



                    <InspTarView_basicInfo {...props}  def={cacheDef}/>


                    {/* <Button onClick={()=>{
            BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
          }}>重試</Button> */}



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
                                        console.log(pkts);

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
                        }}>存為特徵參考圖</Button>
                    </Popconfirm>




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







                <Button key={"_" + 10000} onClick={() => {


                    (async () => {

                        let obj = {
                            type: "extract_feature",
                            image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                            num_features: cacheDef.num_features,
                            weak_thresh: featureInfo.weak_thresh,
                            strong_thresh: featureInfo.strong_thresh,
                            T: [2, 2],
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

                }}>Templ</Button>

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
                    _this.sel_region_type="region"
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
                            _this.sel_region_type=undefined;
                            // onDefChange(newRule)
                            if (_this.canvasComp == undefined) return;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>+特徵範圍</Button>



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
                    _this.sel_region_type="region"
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

                            _this.sel_region_type=undefined;
                            // onDefChange(newRule)
                            if (_this.canvasComp == undefined) return;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>+校位範圍</Button>

                <Button key={"AddAnchor"} onClick={() => {



                    _this.sel_region_type="vector"
                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {
                            _this.sel_region_type=undefined;
                            console.log(info)
                            if(info.pt1.x==info.pt2.x&&info.pt1.y==info.pt2.y)
                            {
                                setFeatureInfo({ ...featureInfo, origin_info:undefined });
                            }
                            else
                            {
                                setFeatureInfo({ ...featureInfo, origin_info:
                                    {pt:info.pt1,
                                    vec:{
                                        x:info.pt2.x-info.pt1.x,
                                        y:info.pt2.y-info.pt1.y}} });
                            }
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>設定原點與方向</Button>
            </>

            break;




        case EditState.Search_Region_Edit:


            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>

                <Button onClick={() => {

                    onCacheDefChange(cacheDef, false);
                    BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
                }}>ReCheck</Button>

                scaleD:
                <InputNumber value={cacheDef.matching_downScale}
                    onChange={(num) => {

                        setCacheDef({ ...cacheDef, matching_downScale: num })
                    }} />


                Sim:
                <InputNumber value={cacheDef.similarity_thres}
                    onChange={(num) => {

                        setCacheDef({ ...cacheDef, similarity_thres: num })
                    }} />


                MagThres:
                <InputNumber value={cacheDef.magnitude_thres}
                    onChange={(num) => {
                        setCacheDef({ ...cacheDef, magnitude_thres: num })
                    }} />



                
                角度:
                <InputNumber min={-360} max={360} value={cacheDef.featureInfo.match_front_face_angle_range[0]}
                    onChange={(num) => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["featureInfo", "match_front_face_angle_range", 0], num));
                    }} />
                ~
                <InputNumber min={-360} max={360} value={cacheDef.featureInfo.match_front_face_angle_range[1]}
                    onChange={(num) => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["featureInfo", "match_front_face_angle_range", 1], num));
                    }} />

                {" "}
                校位下限(0~1):
                <InputNumber min={0} max={1} value={cacheDef.refine_score_thres}
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
                    _this.sel_region_type="region"
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

                            _this.sel_region_type=undefined;
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
                <TestInputSelectUI testTags={[def.id+"_Inject"]}  folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
        } break;




    }



    return <div style={{ ...style, width: width + "%", height: height + "%" }} className={"overlayCon"}>

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

            let ctx = g.ctx;

            let camMag = canvas_obj.camera.GetCameraScale();
            if (ctrl_or_draw == true)//ctrl
            {
                if (canvas_obj.regionSelect !== undefined) {
                    if (canvas_obj.regionSelect.pt1 === undefined || canvas_obj.regionSelect.pt2 === undefined) {
                        return;
                    }

                    let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
                    let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);


                    // console.log(canvas_obj.regionSelect);
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
                    _this.sel_region = {
                        x, y, w, h,pt1,pt2
                    }


                }

                // const imageData = ctx.getImageData(g.mouseStatus.x, g.mouseStatus.y, 1, 1);
                // // 
                // _this.fetchedPixInfo = imageData;
            }
            if (editState == EditState.Normal_Show || editState == EditState.Search_Region_Edit || editState == EditState.Test_Saved_Files) {
                if (ctrl_or_draw == true)//ctrl
                {
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
                            ctx.lineWidth = 4 / camMag;
                            ctx.strokeStyle = `HSLA(0, 100%, 50%,1)`;
                            canvas_obj.rUtil.drawCross(ctx, { x: match.center.x, y: match.center.y }, 12 / camMag);

                            let angle = match.angle;

                            ctx.font = "40px Arial";
                            ctx.fillStyle = "rgba(150,100, 100,0.8)";
                            ctx.fillText("idx:" + idx, match.center.x, match.center.y - 40)
                            ctx.font = "20px Arial";
                            ctx.fillText("ang:" + (angle * 180 / 3.14159).toFixed(2), match.center.x, match.center.y - 20)
                            ctx.fillText("sim:" + match.confidence.toFixed(3), match.center.x, match.center.y - 0)




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
                                    ctx.lineWidth = 4 / mult;
                                    ctx.strokeStyle = `HSLA(${300 * i / featureInfo.templatePyramid.length}, 100%, 50%,1)`;
                                    canvas_obj.rUtil.drawCross(ctx, { x: (temp_pt.x + template.tl_x) * mult, y: (temp_pt.y + template.tl_y) * mult }, 12 / mult);
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

                        ctx.setLineDash([0,0,0,0]);
                        let oriInfo=featureInfo.origin_info;
                        canvas_obj.rUtil.drawCross(ctx, { x: oriInfo.pt.x, y: oriInfo.pt.y }, 10);
                        
                        canvas_obj.rUtil.drawLine(ctx,{
                            x1:oriInfo.pt.x, 
                            y1:oriInfo.pt.y, 
                            x2:oriInfo.pt.x+oriInfo.vec.x, 
                            y2:oriInfo.pt.y+oriInfo.vec.y })
                    }



                }
            }


            if (ctrl_or_draw == false) {
                if (canvas_obj.regionSelect !== undefined && _this.sel_region !== undefined) {
                    ctx.strokeStyle = "rgba(179, 0, 0,0.5)";

                    if(_this.sel_region_type=="region")
                    {
                        drawRegion(g, canvas_obj, _this.sel_region, canvas_obj.rUtil.getIndicationLineSize());
                    }
                    if(_this.sel_region_type=="vector")
                    {
                        canvas_obj.rUtil.drawCross(ctx, { x: _this.sel_region.pt1.x, y: _this.sel_region.pt1.y }, 10);
                        
                        ctx.setLineDash([0,0,0,0]);
                        canvas_obj.rUtil.drawLine(ctx,{
                            x1: _this.sel_region.pt1.x, 
                            y1: _this.sel_region.pt1.y, 
                            x2: _this.sel_region.pt2.x, 
                            y2: _this.sel_region.pt2.y })

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
                    g.worldTransform
                    

                    let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
                    // console.log(mouseOnCanvas)
                    ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`, g.mouseStatus.x, g.mouseStatus.y)
                    ctx.restore();
                }
            }




            if (renderHook) {
                // renderHook(ctrl_or_draw,g,canvas_obj,newDef);
            }
        }
        } />

    </div>;

}


export function SingleTargetVIEWUI_Orientation_ColorRegionOval(props: CompParam_InspTarUI) {
    let { display, stream_id, fsPath, width, height, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }=props;
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
    }


    useEffect(() => {//////////////////////

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, "ColorRegionDetection", {

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
                    ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


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
                    stream_id, "ColorRegionDetection");

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



                    <InspTarView_basicInfo {...props}  def={cacheDef}/>

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



    return <div style={{ ...style, width: width + "%", height: height + "%" }} className={"overlayCon"}>

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

                    let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
                    let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);


                    // console.log(canvas_obj.regionSelect);
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
                    _this.sel_region = {
                        x, y, w, h
                    }
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



function SurfaceCheckSimple_EDIT_UI({ def, onDefChange, canvas_obj }:
    {
        def: any,
        onDefChange: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent
    }) {

    const [delConfirmCounter, setDelConfirmCounter] = useState(0);
    let def_Filled = {

        W: 500,
        H: 500,

        hsv: {
            rangeh: {
                h: 180, s: 255, v: 255
            },
            rangel: {
                h: 0, s: 0, v: 0
            },
        },
        colorThres: 10,
        resultOverlayAlpha: 0,
        img_order_reverse: false,

        ...def
    }
    const _this = useRef<any>({}).current;
    return <>

        <br />XOffset:
        <InputNumber value={def_Filled.X_offset}
            onChange={(num) => {
                let newDef = { ...def_Filled, X_offset: num }
                onDefChange(newDef, true);
            }} />
        {"  "}YOffset:
        <InputNumber value={def_Filled.Y_offset}
            onChange={(num) => {
                let newDef = { ...def_Filled, Y_offset: num }
                onDefChange(newDef, true);
            }} />

        <br />W:
        <InputNumber min={10} max={2000} value={def_Filled.W}
            onChange={(num) => {
                let newDef = { ...def_Filled, W: num }
                onDefChange(newDef, true);
            }} />
        {"  "}H:
        <InputNumber min={10} max={2000} value={def_Filled.H}
            onChange={(num) => {
                let newDef = { ...def_Filled, H: num }
                onDefChange(newDef, true);
            }} />
        <br />iW:
        <InputNumber min={10} max={2000} value={def_Filled.inner_W}
            onChange={(num) => {
                let newDef = { ...def_Filled, inner_W: num }
                onDefChange(newDef, true);
            }} />
        {"  "}iH:
        <InputNumber min={10} max={2000} value={def_Filled.inner_H}
            onChange={(num) => {
                let newDef = { ...def_Filled, inner_H: num }
                onDefChange(newDef, true);
            }} />
        <br />
        {"  "}角度調整:
        <InputNumber value={def_Filled.angle_offset}
            onChange={(num) => {
                let newDef = { ...def_Filled, angle_offset: num }
                onDefChange(newDef, true);
            }} />
        <br />
        面積閾值:
        <InputNumber value={def_Filled.area_thres}
            onChange={(num) => {
                let newDef = { ...def_Filled, area_thres: num }
                onDefChange(newDef, true);
            }} />

        單線長閾值:
        <InputNumber value={def_Filled.line_length_thres}
            onChange={(num) => {
                let newDef = { ...def_Filled, line_length_thres: num }
                onDefChange(newDef, true);
            }} />
        單面積閾值:
        <InputNumber value={def_Filled.point_area_thres}
            onChange={(num) => {
                let newDef = { ...def_Filled, point_area_thres: num }
                onDefChange(newDef, true);
            }} />




        <>


            <br />結果顯示
            <Slider defaultValue={def_Filled.resultOverlayAlpha} min={0} max={1} step={0.1} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["resultOverlayAlpha"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />


            圖序反轉:
            <Switch checkedChildren="左至右" unCheckedChildren="右至左" checked={def_Filled.img_order_reverse == true} onChange={(check) => {
                onDefChange(ObjShellingAssign(def_Filled, ["img_order_reverse"], check));
            }} />

            <br />HSV
            <Slider defaultValue={def_Filled.hsv.rangeh.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangeh", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={def_Filled.hsv.rangel.h} max={180} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangel", "h"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />



            <Slider defaultValue={def_Filled.hsv.rangeh.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangeh", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={def_Filled.hsv.rangel.s} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangel", "s"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />




            <Slider defaultValue={def_Filled.hsv.rangeh.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_debounce(_this.trigTO, () => {
                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangeh", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />
            <Slider defaultValue={def_Filled.hsv.rangel.v} max={255} onChange={(v) => {

                _this.trigTO =
                    ID_throttle(_this.trigTO, () => {

                        onDefChange(ObjShellingAssign(def_Filled, ["hsv", "rangel", "v"], v));
                    }, () => _this.trigTO = undefined, 500);

            }} />





        </>

        <br />細節量
        <Slider defaultValue={def_Filled.colorThres} max={255} onChange={(v) => {

            _this.trigTO =
                ID_throttle(_this.trigTO, () => {
                    onDefChange(ObjShellingAssign(def_Filled, ["colorThres"], v));
                }, () => _this.trigTO = undefined, 200);

        }} />




    </>

}


function TagsEdit_DropDown({ tags, onTagsChange, children }: { tags: string[], onTagsChange: (tags: string[]) => void, children: React.ReactChild }) {
    const [visible, _setVisible] = useState(false);
    const [newTagTxt, setNewTagTxt] = useState("");


    const [tagDelInfo, setTagDelInfo] = useState({ tarTag: "", countdown: 0 });


    function setVisible(enable: boolean) {
        setTagDelInfo({ ...tagDelInfo, tarTag: "" });
        _setVisible(enable);
    }
    if(tags===undefined)
        tags=[]
    let isNewTagTxtDuplicated = tags.find(tag => tag == newTagTxt) != undefined;
    return <Dropdown onVisibleChange={setVisible} visible={visible}
        overlay={<Menu>
            {
                [...tags.map((tag: string, index: number) => (
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
                            if (isNewTagTxtDuplicated == false) {
                                onTagsChange([...tags, newTagTxt]);
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

const CAT_ID_NAME = {
    "0": "NA",
    "1": "OK",
    "-1": "NG",
    "-40000": "空",

    "-700": "點過大",
    "-701": "邊過長",
}
const _MM_P_STP_ = 4;
const _OBJ_SEP_DIST_ = 4;





export function InspTarView_basicInfo({ display, stream_id, fsPath, width, height, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }:CompParam_InspTarUI) {

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

        <Switch checkedChildren="隱藏" unCheckedChildren="顯示" checked={cacheDef.default_hide == true} onChange={(check) => {
            
            onDefChange({ ...cacheDef, default_hide: check }, false)
        }} />
    </>

}





export function SingleTargetVIEWUI_SurfaceCheckSimple(props: CompParam_InspTarUI) {
    let { display, stream_id, fsPath, width, height, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange }=props;
    const _ = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
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


    function onCacheDefChange(updatedDef: any, ddd: boolean) {
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            console.log(">>>");
            await BPG_API.InspTargetUpdate(updatedDef)

        })()

        BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
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
    useEffect(() => {//////////////////////

        (async () => {

            let ret = await BPG_API.InspTargetExchange(cacheDef.id, { type: "get_io_setting" });
            console.log(ret);

            // await BPG_API.InspTargetExchange(cacheDef.id,{type:"get_io_setting"});

            await BPG_API.send_cbs_attach(
                cacheDef.stream_id, "SurfaceCheckSimple", {

                resolve: (pkts) => {
                    // console.log(pkts);
                    let IM = pkts.find((p: any) => p.type == "IM");
                    if (IM === undefined) return;
                    let CM = pkts.find((p: any) => p.type == "CM");
                    if (CM === undefined) return;
                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP === undefined) return;
                    // console.log("++++++++\n",IM,CM,RP);


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
                    ctx2nd.putImageData(IMCM.image_info.image, 0, 0);


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
                    stream_id, "SurfaceCheckSimple");

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
                EDIT_UI = <>

                    {/* <Input maxLength={100} value={cacheDef.type} disabled
            style={{width:"100px"}}
            onChange={(e)=>{
              
            }}/>
  
          <Input maxLength={100} value={cacheDef.sampleImageFolder}  disabled
            style={{width:"100px"}}
            onChange={(e)=>{
            }}/> */}
                    <InspTarView_basicInfo {...props} def={cacheDef} />


                    <br />
                    <Button onClick={() => {
                        onCacheDefChange(cacheDef, true);
                    }}>SHOT</Button>


                    <Button key={"_" + 10000} onClick={() => {
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
                <Button key={"_" + -1} onClick={() => {

                    setEditState(EditState.Normal_Show);
                }}>{"<"}</Button>


                <SurfaceCheckSimple_EDIT_UI
                    def={cacheDef}
                    onDefChange={(newDef) => {
                        onCacheDefChange(newDef, true);

                    }}
                    canvas_obj={_this.canvasComp} />

            </>

            break;
    }

    let img_order_reverse = cacheDef.img_order_reverse === true;
    // console.log("img_order_reverse:"+img_order_reverse) 
    return <div style={{ ...style, width: width + "%", height: height + "%" }} className={"overlayCon"}>

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

                    let pt1 = canvas_obj.regionSelect.pt1;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst1, g.worldTransform_inv);
                    let pt2 = canvas_obj.regionSelect.pt2;//canvas_obj.VecX2DMat(canvas_obj.regionSelect.pcvst2, g.worldTransform_inv);


                    // console.log(canvas_obj.regionSelect);
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
                    _this.sel_region = {
                        x, y, w, h
                    }
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



                if (defReport !== undefined) {
                    {
                        ctx.save();
                        ctx.resetTransform();
                        ctx.font = "20px Arial";
                        ctx.fillStyle = "rgba(150,100, 100,0.5)";
                        let Y = 350 + 200;
                        ctx.fillText("Result:" + defReport.report.category + " DIST:" + (reelStep * _MM_P_STP_ / _OBJ_SEP_DIST_) + "顆", 20, Y);
                        ctx.fillText("ProcessTime:" + (defReport.process_time_us / 1000).toFixed(2) + " ms", 20, Y + 30)

                        ctx.restore();
                    }
                    let g_cat = defReport.report.sub_reports;
                    g_cat.forEach((catInfo: any, _index: number) => {
                        // console.log(catInfo,cacheDef);

                        // console.log(catInfo);

                        let index = img_order_reverse ? (g_cat.length - 1 - _index) : _index;
                        let TextY = 15;


                        ctx.font = TextY + "px Arial";
                        if (catInfo.category > 0)
                            ctx.strokeStyle =ctx.fillStyle = "rgba(0, 255, 0,1)";
                        else
                            ctx.strokeStyle =ctx.fillStyle = "rgba(255, 0, 0,1)";




                        let curOffset = (reelStep * _MM_P_STP_ - latestRepStepCount * _OBJ_SEP_DIST_ + _index * _MM_P_STP_) / _OBJ_SEP_DIST_;
                        ctx.fillText(CAT_ID_NAME[catInfo.category + ""] + " " + curOffset + "顆", cacheDef.W * index, 0 + TextY + 5)

                        ctx.font = (TextY * 0.7).toFixed(2) + "px Arial";
                        ctx.fillText(catInfo.score, cacheDef.W * index, 0 + TextY + TextY + 5)


                        // ctx.strokeStyle = "rgba(179, 0, 0,1)";
                        let lsz=canvas_obj.rUtil.getIndicationLineSize();
                        drawRegion(g, canvas_obj, {
                            x: cacheDef.W * index+lsz/2,
                            y: 0+lsz/2,
                            w: cacheDef.W-lsz,
                            h: cacheDef.H-lsz
                        },
                        lsz,
                            false);


                        catInfo.elements.forEach((ele: any) => {

                            if (CAT_ID_NAME[ele.category + ""] == "OK")
                                ctx.strokeStyle = "rgba(0, 179, 0,0.6)";
                            else
                                ctx.strokeStyle = "rgba(179, 0, 0,0.6)";

                            ctx.fillStyle = ctx.strokeStyle;
                            canvas_obj.rUtil.drawCross(ctx, { x: ele.x, y: ele.y }, 1);


                            // let fontSize_eq=10/camMag;
                            // if(fontSize_eq>10)fontSize_eq=40;
                            // ctx.font = (fontSize_eq)+"px Arial";
                            ctx.font = "1px Arial";
                            ctx.fillText(CAT_ID_NAME[ele.category + ""], ele.x, ele.y);



                        });



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

