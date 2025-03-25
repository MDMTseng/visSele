import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext,createContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Button, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select } from 'antd';


import type { MenuProps, MenuTheme } from 'antd/es/menu';
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleFilled,PauseCircleOutlined,PauseCircleFilled,SettingOutlined,
    DisconnectOutlined, LinkOutlined,CameraOutlined,SyncOutlined,DeleteOutlined,ExclamationCircleOutlined,LoadingOutlined,StopOutlined,CloseOutlined,TagsOutlined,ToTopOutlined
} from '@ant-design/icons';

import clone from 'clone';

import { StoreTypes } from './redux/store';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { listCMDPromise } from './XCMD';
import { CameraSetupEditUI } from './CameraSetupEditUI';

import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d,threePointToArc} from './UTIL/MathTools';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { Row, Col, Input, Tag, Modal, message, Space,Statistic,Avatar } from 'antd';

import { ITGlobalVariableContext } from './contexts/GlobalContext';
import { CompParam_GlobalVariable } from './types/contextTypes';

import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';

import { SingleTargetVIEWUI_DimMeasure } from './SingleTargetVIEWUI_DimMeasure';


import { SingleTargetVIEWUI_ArcFitting } from './SingleTargetVIEWUI_ArcFitting';

import { SingleTargetVIEWUI_CameraCalib } from './SingleTargetVIEWUI_CameraCalib';

import { SingleTargetVIEWUI_Orientation_ShapeBasedMatching } from './SingleTargetVIEWUI_Orientation_ShapeBasedMatching';

import { 
    ObjTree,
    PtsToXYWH,drawRegion ,
    CompParam_InspTarUI,
    IMCM_type,
    EDIT_PERMIT_FLAG,
    TagsEdit_DropDown,
    InspTarView_basicInfo,
    TestInputSelectUI
} from './SingleTargetVIEWUI_UTIL';


let INPUT_LINK = {
    InputNumber:(props:any)=>{
        const {
            global_variable,

            set_global_variable,
        } = useContext(ITGlobalVariableContext);

        const [linkStrExpand, setLinkStrExpand] = useState<boolean>(false);

        console.log(props.value);



        let isLinkMode=(typeof props.value!="number")&&(props.value!==undefined);
        let isEmptyInput=(isLinkMode==false)?props.value==0:props.value=="";

        let inputShow=null;
        if(isLinkMode==false)
        {
            inputShow= <InputNumber {...props} 
                onChange={(num) => {
                    console.log(num);
                    props.onChange(num)
                }}
                style={{width: isEmptyInput?'calc(100% - 10px)':"100%"}}/>
        }
        else 
        {
           

            let path_str=props.value as string |null;
            let path=path_str===null?[]:path_str.split(".");
            let value=GetObjElement(global_variable,path);

            console.log(global_variable,path,value);






            inputShow=<> 

                <Dropdown 
                    trigger={['click']}
                    overlay={
                        <div style={{background:"#FFF",border:"5px" }}>
                            <ObjTree obj={global_variable} padding={0} onLeafSelect={(value,name,path)=>{
                                let newPath=[...path,name].join(".");
                                console.log(value,name,path);
                                props.onChange(newPath)
                            }}/>
                        </div>
                    } >
                    
                    <Button 
                    
                    onFocus={()=>setLinkStrExpand(true)}
                    onBlur={()=>setLinkStrExpand(false)}
                    style={{padding:0,margin:0,width: linkStrExpand?'calc(100% - 10px)':"10px"}} onClick={()=>{
                                    }}>{props.value+" "}</Button>

                </Dropdown>
                    
                <InputNumber {...props} value={value}
                onChange={(number) => {
                    // let new_global_variable=ObjShellingAssign(global_variable,path,number);
                    if(set_global_variable!==undefined)
                        set_global_variable(path,number);
                    
                    props.onChange(props.value)
                }}
                style={{width: 'calc(100% - 20px)'}}/>
            </>
        }




        return <div style={{width:"100px",...props.style,display: "inline-block",whiteSpace:"nowrap",overflow:"hidden",verticalAlign: "middle"}}>


            {inputShow}
            <Button style={{padding:0,margin:0,width:10}} onClick={()=>{
                    if(isLinkMode==false)
                    {
                        props.onChange("")
                    }
                    else
                    {
                        props.onChange(undefined)
                    }
    
                }}>_</Button>



        </div>
    },
    RangeSlider:(props:any)=>{
        const {
            global_variable,
            set_global_variable,
        } = useContext(ITGlobalVariableContext);

        const [linkStrExpand, setLinkStrExpand] = useState<boolean>(false);



        let isLinkMode=(typeof props.defaultValue[0]=="string")&&(typeof props.defaultValue[1]=="string");

        let inputShow=null;
        if(isLinkMode==false)
        {
            inputShow= <Slider range {...props} 
                onChange={(nums) => {
                    props.onChange(nums)
                }}
                style={{width: 'calc(100% - 10px)'}}/>
        }
        else    if(isLinkMode==true)
        {
           


            let path_strs=props.defaultValue as string[];
            let paths=path_strs.map(str=>str.split("."));
            let values=paths.map(path=>GetObjElement(global_variable,path));

            console.log(global_variable,paths,values);




            inputShow=<> 

                <Dropdown 
                    trigger={['click']}
                    overlay={
                        <div style={{background:"#FFF",border:"5px" }}>
                            <ObjTree obj={global_variable} padding={0} onLeafSelect={(value,name,path)=>{
                                let newPath=[...path,name].join(".");
                                console.log(value,name,path);
                                props.onChange(newPath)
                                props.onChange([newPath,props.defaultValue[1]])
                            }}/>
                        </div>
                    } >
                    
                    <Button 
                    
                    onFocus={()=>setLinkStrExpand(true)}
                    onBlur={()=>setLinkStrExpand(false)}
                    style={{padding:0,margin:0,width: linkStrExpand?'calc(50% - 5px)':"0px",overflow:"hidden"}} onClick={()=>{
                                    }}>{props.defaultValue[0]+" "}</Button>

                </Dropdown>

                <Dropdown 
                    trigger={['click']}
                    overlay={
                        <div style={{background:"#FFF",border:"5px" }}>
                            <ObjTree obj={global_variable} padding={0} onLeafSelect={(value,name,path)=>{
                                let newPath=[...path,name].join(".");
                                console.log(value,name,path);
                                props.onChange(newPath)
                                props.onChange([props.defaultValue[0],newPath])
                            }}/>
                        </div>
                    } >
                    
                    <Button 
                    
                    onFocus={()=>setLinkStrExpand(true)}
                    onBlur={()=>setLinkStrExpand(false)}
                    style={{padding:0,margin:0,width: linkStrExpand?'calc(50% - 5px)':"10px",overflow:"hidden"}} onClick={()=>{
                                    }}>{props.defaultValue[1]+" "}</Button>

                </Dropdown>



            <Slider range  {...props} value={values}
                onChange={(nums) => {
                    props.onChange(nums)
                    // let new_global_variable=global_variable;


                    // new_global_variable=ObjShellingAssign(new_global_variable,paths[0],nums[0]);
                    // new_global_variable=ObjShellingAssign(new_global_variable,paths[1],nums[1]);


                    if(set_global_variable!==undefined)
                    {
                        if(values[0]!=nums[0])
                            set_global_variable(paths[0],nums[0]);
                        else if(values[1]!=nums[1])
                            set_global_variable(paths[1],nums[1]);
                    }
                    
                    props.onChange(props.defaultValue)

                }}
                style={{width: 'calc(100% - 10px)'}}/>
            </>
        }




        return <div style={{width:"100%",...props.style,display: "flex",whiteSpace:"nowrap",overflow:"hidden",verticalAlign: "middle"}}>


            {/* <InputNumber {...props} 
                    onChange={(num) => {
                        console.log(num);
                        props.onChange(num)
                    }}
                    style={{width:"50%"}}/> */}


            {inputShow}
            <Button style={{padding:0,margin:0,width:10}} onClick={()=>{
                    if(isLinkMode==false)
                    {
                        props.onChange(["",""])
                    }
                    else
                    {
                        props.onChange([0,0])
                    }
    
                }}>_</Button>

            {/* <Select style={{padding:"0px",margin:"0px"}} defaultValue="a" className="select-after" 
                onChange={ (value: string) => {
                    console.log(`selected ${value}`);
                    if(value=="a")
                    {
                        setData(0);
                    }
                    else if(value=="b")
                    {
                        setData("b");
                    }
                }}>
                <Option value="a">a</Option>
                <Option value="b">b</Option>
            </Select> */}


        </div>
    },
    Slider:(props:any)=>{
        const {
            global_variable,
            set_global_variable,
        } = useContext(ITGlobalVariableContext);

        const [linkStrExpand, setLinkStrExpand] = useState<boolean>(false);



        let isLinkMode=(typeof props.value=="string");

        let inputShow=null;
        if(isLinkMode==false)
        {
            inputShow= <Slider {...props} 
                onChange={(nums) => {
                    props.onChange(nums)
                }}
                style={{width: 'calc(100% - 10px)'}}/>
        }
        else    if(isLinkMode==true)
        {
           


            let path_str=props.value as string;
            let path=path_str.split(".");
            let value=GetObjElement(global_variable,path);

            console.log(global_variable,path,value);






            inputShow=<> 


            <Dropdown 
                trigger={['click']}
                overlay={
                    <div style={{background:"#FFF",border:"5px" }}>
                        <ObjTree obj={global_variable} padding={0} onLeafSelect={(value,name,path)=>{
                            let newPath=[...path,name].join(".");
                            console.log(value,name,path);
                            props.onChange(newPath)
                        }}/>
                    </div>
                } >
                
                <Button 
                
                onFocus={()=>setLinkStrExpand(true)}
                onBlur={()=>setLinkStrExpand(false)}
                style={{padding:0,margin:0,width: linkStrExpand?'calc(100% - 10px)':"10px"}} onClick={()=>{
                                }}>{props.value+" "}</Button>

            </Dropdown>




            <Slider  {...props} value={value}
                onChange={(num) => {
                    props.onChange(num)
                    let new_global_variable=global_variable;
                    new_global_variable=ObjShellingAssign(new_global_variable,path,num);


                    if(set_global_variable!==undefined)
                        set_global_variable(path,num);
                    
                    props.onChange(props.defaultValue)

                }}
                style={{width: 'calc(100% - 10px)'}}/>
            </>
        }




        return <div style={{width:"100%",...props.style,display: "flex",whiteSpace:"nowrap",overflow:"hidden",verticalAlign: "middle"}}>


            {inputShow}
            <Button style={{padding:0,margin:0,width:10}} onClick={()=>{
                    if(isLinkMode==false)
                    {
                        props.onChange("")
                    }
                    else
                    {
                        props.onChange(NaN)
                    }
    
                }}>_</Button>



        </div>
    }
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


function SurfaceCheckSimple_SubRegion_EDIT_UI({ BPG_API, fsPath, id,rootDef, def, onDefChange, onCopy, onFinish, canvas_obj, canvas_hook_update }:
    {
        BPG_API: BPG_WS,
        fsPath: string,
        id: string,
        rootDef: any,
        def: any,
        onDefChange: (...param: any) => void,
        onFinish: (...param: any) => void,
        onCopy: (...param: any) => void,
        canvas_obj: DrawHook_CanvasComponent,
        canvas_hook_update: (cb: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, is_post_render: boolean) => any) | undefined) => any
    }) {

    const _this = useRef<any>({}).current;

    const CALC_SCRIPT_INPUT_Ref = useRef<any>(null);

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

    定位方式
    <Switch checkedChildren="無至有" unCheckedChildren="自動閾值" checked={def.locating_type_z2o_or_auto == true} onChange={(check) => {
        onDefChange({ ...def, locating_type_z2o_or_auto: check }, true);
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


            <INPUT_LINK.RangeSlider
                range
                step={1} max={180}
                defaultValue={[def.rangel?.h, def.rangeh?.h]}
                onChange={([vl, vh] : [any,any]) => {

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


            <INPUT_LINK.RangeSlider
                range
                step={1} max={255}
                defaultValue={[def.rangel?.s, def.rangeh?.s]}
                onChange={([vl, vh]:any) => {

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

            <INPUT_LINK.RangeSlider
                range
                step={1} max={255}
                defaultValue={[def.rangel?.v, def.rangeh?.v]}
                onChange={([vl, vh]:any) => {

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
    <INPUT_LINK.Slider
        step={1} max={255}
        value={def.detect_detail}
        onChange={(val:number|string) => {
            
            ID_debounce(_this.trigTO, () => {
                let nedf = def;
                nedf = ObjShellingAssign(nedf, ["detect_detail"], val)
                onDefChange(nedf);
            }, () => _this.trigTO = undefined, 500);
        }}

    />

</>;


    console.log("_________rootDef",rootDef);

    switch(def_Filled.type)
    {
        case "HSVSeg":
        case undefined:

        ConfigUI=<>
    
    
    單物件偵測閾值:
            <INPUT_LINK.InputNumber value={def.point_area_thres} step={0.1}
                onChange={(num:number|string) => {
                    let newDef = { ...def, point_area_thres: num }
                    onDefChange(newDef, true);
                }} />
    
            
    物件面積閾值:
            <INPUT_LINK.InputNumber value={def.element_area_thres} step={0.1}
                onChange={(num:number|string) => {
                    let newDef = { ...def, element_area_thres: num }
                    onDefChange(newDef, true);
                }} />
    物件數量閾值
            <INPUT_LINK.InputNumber value={def.element_count_thres}
                onChange={(num:number|string) => {
                    let newDef = { ...def, element_count_thres: num }
                    onDefChange(newDef, true);
                }} />
    
            <br />
    總面積閾值:

            <INPUT_LINK.InputNumber style={{width:"50px"}} step={0.5}
                value={def.area_min_thres} 
                onChange={(num:number|string) => {
                    let newDef = { ...def, area_min_thres: num}
                    onDefChange(newDef, true);
                }} 
                
                />
            ~
            <INPUT_LINK.InputNumber style={{width:"100px"}} step={0.5}
                value={def.area_thres} 
                onChange={(num:number|string) => {
                    let newDef = { ...def, area_thres: num}
                    onDefChange(newDef, true);
                }} 
                
                />
    
    
    單線長閾值:
            <INPUT_LINK.InputNumber value={def.line_length_thres} min={0.001} step={0.1}
                onChange={(num:number|string) => {
                    let newDef = { ...def, line_length_thres: num  }
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
            <INPUT_LINK.InputNumber value={def_Filled.colorSigma} step={0.1}
                onChange={(num:number|string) => {
                    let newDef = { ...def_Filled, colorSigma:num  }
                    onDefChange(newDef, true);
                }} />


            依亮度校正:
            <Switch checkedChildren="開啟" unCheckedChildren="關閉" checked={def_Filled.brightnessCompensation == true} onChange={(check) => {
                onDefChange(ObjShellingAssign(def_Filled, ["brightnessCompensation"], check));
            }} />


            </>
            break;

        case "DirectionalDiff":
            ConfigUI= <>DirectionalDiff 向差偵測
            

            {["x","y"].map((str)=>
                {
                    let dirAngle = def_Filled?.dirAngle||0;
                    let scanDirStr="x";


                    switch(dirAngle)
                    {
                        case 0:
                            scanDirStr="x";
                            break;
                        case 90:
                            scanDirStr="y";
                            break;

                    }
                    return <Button type={scanDirStr==str?"primary":undefined} onClick={()=>{
                        let dirAngle=0;
                        switch(str)
                        {
                            case "x":
                                dirAngle=0;
                                break;
                            case "y":
                                dirAngle=90;
                                break;
                        }
                        let newDef=def_Filled;
                        newDef=ObjShellingAssign(newDef, ["dirAngle"], dirAngle);
                        onDefChange(newDef);
                        
                    }}>{str}</Button>
                })}


            閾值:
            <InputNumber value={def_Filled.thres} step={0.05}
                onChange={(num) => {
                    let newDef = { ...def_Filled, thres:num  }
                    onDefChange(newDef, true);
                }} />


            低差抑制:
            <InputNumber value={def_Filled.diffSupressThres} step={0.05}
                onChange={(num) => {
                    let newDef = { ...def_Filled, diffSupressThres:num  }
                    onDefChange(newDef, true);
                }} />

{/* 
            依亮度校正:
            <Switch checkedChildren="開啟" unCheckedChildren="關閉" checked={def_Filled.brightnessCompensation == true} onChange={(check) => {
                onDefChange(ObjShellingAssign(def_Filled, ["brightnessCompensation"], check));
            }} /> */}


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

                <Switch checkedChildren="無->有" unCheckedChildren="自動閾值" checked={def_Filled.sense0to1 == true} onChange={(check) => {
                    onDefChange(ObjShellingAssign(def_Filled, ["sense0to1"], check));
                }} />

                <Switch checkedChildren="次像素定位" unCheckedChildren="像素定位" checked={def_Filled.locatingRefinement == true} onChange={(check) => {
                    onDefChange(ObjShellingAssign(def_Filled, ["locatingRefinement"], check));
                }} />
            
            <br />
            <Button onClick={() => { setShowDisplayAdjUI(!showDisplayAdjUI) }}> {showDisplayAdjUI == false ? "+展開顯示調整選項" : "-收起顯示調整選項"}</Button>
            {showDisplayAdjUI == false ? null :ProcessDisplayUI}
            <br/>
                
            <Button onClick={() => { setShowDetectAdjUI(!showDetectAdjUI) }}> {showDetectAdjUI == false ? "+展開偵測調整選項" : "-收起偵測調整選項"}</Button>
    
            {showDetectAdjUI == false ? null : HSVEditUI}

            </>
            break;

        case "PassThru":
            ConfigUI= <>
                {["x","y"].map((str)=>
                {
                    let scanDir = def_Filled?.scanDir;
                    return <Button type={scanDir==str?"primary":undefined} onClick={()=>{
                        let newDef=def_Filled;
                        newDef=ObjShellingAssign(newDef, ["scanDir"], str);
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
        
        case "CALC":
            ConfigUI= <>


                        
                <Dropdown overlay={
                    <Menu> 
                        <Menu.Item onClick={()=>{
                            let newV="INDEX";
                            if(def_Filled.variables.find((vs:string)=>vs==newV)!==undefined)return;
                            let newDef = { ...def_Filled, variables:[...def_Filled.variables,newV] }
                            onDefChange(newDef, true);
                        }}>{"INDEX"}</Menu.Item>



                        {rootDef.sub_regions.map((reg:any)=><Menu.Item onClick={()=>{
                            let newV="N_"+reg.name;
                            if(def_Filled.variables.find((vs:string)=>vs==newV)!==undefined)return;
                            let newDef = { ...def_Filled, variables:[...def_Filled.variables,newV] }
                            onDefChange(newDef, true);
                        }}>{"N_"+reg.name}</Menu.Item>)}
                        
                        {rootDef.sub_regions.map((reg:any)=><Menu.Item onClick={()=>{

                            let newV="N_"+reg.name+".cat";
                            if(def_Filled.variables.find((vs:string)=>vs==newV)!==undefined)return;
                            let newDef = { ...def_Filled, variables:[...def_Filled.variables,newV] }
                            onDefChange(newDef, true);
                        }}>{"N_"+reg.name+".cat"}</Menu.Item>)}
                    </Menu>}>
                
                    <Button>+</Button>
                </Dropdown>

                {def_Filled.variables.map((str:string,index:number)=>{ 


                    let idx=def_Filled.script.indexOf(str);



                    return <>
                    
                    <Button type={idx==-1?"dashed":undefined} onClick={()=>{
                        if(CALC_SCRIPT_INPUT_Ref.current===null)return;
                        let textArea= CALC_SCRIPT_INPUT_Ref.current.resizableTextArea.textArea;
                        let selectionStart=textArea.selectionStart;
                        let selectionEnd=textArea.selectionEnd;

                        let scriptStr=def_Filled.script;
                        //insert str at selectionStart
                        scriptStr=scriptStr.slice(0,selectionStart)+" "+str+" "+scriptStr.slice(selectionEnd);


                        onDefChange({ ...def, script:scriptStr })

                    }}>{str}</Button>


                    <Button disabled={idx!=-1} style={{width:"10px",padding:0,marginRight:"5px"}} danger onClick={()=>{
                        //remove idx from variables
                        let newDef = { ...def_Filled, variables:def_Filled.variables.filter((vs:string)=>vs!=str) }
                        onDefChange(newDef, true);
                    }}>{" "}</Button>

                    </>
                })}





                <Input.TextArea ref={CALC_SCRIPT_INPUT_Ref}
                value={def_Filled.script} 
                autoSize
                tabIndex={-1}
                onKeyDown={(e)=>{
                  if (e.key == 'Tab') {
                    // e.preventDefault();

                  }
                }}
                style={{margin:"1px"}}
                onChange={(e)=>{
                  
                    console.log(e.target.value);
                    onDefChange({ ...def, script: e.target.value })

                }}/>



                範圍:<InputNumber value={def_Filled.rangeFrom}
                    step={0.1}
                        onChange={(num) => {
                            onDefChange({ ...def,  rangeFrom:num})
                        }} />

                ~<InputNumber value={def_Filled.rangeTo}
                    step={0.1}
                        onChange={(num) => {
                            onDefChange({ ...def,  rangeTo:num})
                        }} />
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
                NG變換:{def_Filled.NG_Map_To}
                <DownOutlined />
                </Space>
            </Button>
        </Dropdown>

        <Dropdown overlay={
            <Menu>
                {["HSVSeg","SigmaThres","DirectionalDiff","BrightnessBalance","ScanPoint","PassThru","CALC"].map(str=><Menu.Item onClick={()=>{
                    let newDef = { ...def_Filled, type:str }

                    if(str=="CALC")
                    {
                        newDef={
                            ...newDef,
                            variables:[],
                            script:"0"
                            // post_exp:["N_SC2.v","2","$,$","max$"]
                        }
                    }
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

                    let pt1={...info.pt1};
                    let pt2={...info.pt2};
                    pt1.x%=rootDef.w;
                    pt2.x%=rootDef.w;
                    
                    pt1.y%=rootDef.h;
                    pt2.y%=rootDef.h;
                    console.log(info,rootDef,pt1,pt2);

                    canvas_obj.UserRegionSelect(undefined)


                    onDefChange(ObjShellingAssign(def, ["region"], PtsToXYWH(pt1, pt2)));
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
        <Input maxLength={500} value={def.note}
            addonBefore='備註'
            onChange={(e) => {
                onDefChange({ ...def, note: e.target.value })
            }} />

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



                    
                    <br />

                    <Input.TextArea
                    value={def_Filled.script} 
                    autoSize
                    tabIndex={-1}
                    onKeyDown={(e)=>{
                    if (e.key == 'Tab') {
                        // e.preventDefault();

                    }
                    }}
                    style={{margin:"1px"}}
                    onChange={(e)=>{
                    
                        console.log(e.target.value);
                        onDefChange({ ...def_Filled, script: e.target.value })

                    }}/>


                    <br />

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
                        rootDef={def_Filled}
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


const CAT_ID_Color = {
    "": "gray",
    "0": "gray",
    "1": "green",
    "-1": "red",
    "-2": "orange",
    "-3": "orange",
    "-40000": "gray",

    "-700": "red",
    "-701": "red",
    "-750": "red",
}

const CAT_ID_NAME: { [key: string]: string } = {
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






export function SingleTargetVIEWUI_SurfaceCheckSimple(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, defDoReload,UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;
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




    if (APIExport !== undefined)//keeps update for every state change
    {
        APIExport({
            setDrawHook: (hook:any) => {
                _this.extDrawHook=hook;
            },
            getLatestReport: () => {
                return defReport;
            },
            getCameraState: () => {
                if(_this.canvasComp===undefined)return undefined;
                let ccomp=_this.canvasComp as DrawHook_CanvasComponent;
                return ccomp.camera.toSimpleObj();
            },
            setCameraState: (cameraInfo:any) => {
                if(_this.canvasComp===undefined)return false;
                let ccomp=_this.canvasComp as DrawHook_CanvasComponent;
                ccomp.camera.fromSimpleObj(cameraInfo);
                ccomp.ctrlLogic();
                ccomp.draw(true);
            },

            defInfo: def,
            latest_RP: defReport,
            
        })
    }

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
        if(updatedDef===undefined)
        {
            onDefChange(undefined,ddd);
            return;   
        }
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
                            let NARep=sub_reports.find((rep:any)=>rep.category<-10000);
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
                    }} 
                    
                    defDoReload={()=>defDoReload()}
                    
                    />

                    {/* <br /> */}
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
        // return <div style={{ ...style}}>
        //     <>AAA</>
        // </div>
        console.log(UIOption);
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
                
                let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
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

                    g.ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`, g.mouseStatus.x, g.mouseStatus.y-23)
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
                        
                        let bX=cacheDef.w*blockX;
                        let bY=cacheDef.h*blockY;
                        g.ctx.translate(bX,bY);
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
                                    if (regionInfo.x_locating_mark == true) prefix += "X$"
                                    if (regionInfo.y_locating_mark == true) prefix += "Y$"

                                    let idText = prefix + (regionInfo.name === undefined || regionInfo.name == "" ? "$" + subreg_index : regionInfo.name);// +"["+id_name+"]";


                                    let x=regionInfo.region.x+bX;
                                    let y=regionInfo.region.y+bY;

                                    let dist_Mouse2Frame=Math.max(Math.abs(x-mouseOnCanvas.x),Math.abs(y-mouseOnCanvas.y));

                                    let loc=regionInfo.name_loc_offset!==undefined?regionInfo.name_loc_offset:regionInfo.region;

                                    let dist_Mouse2Name=Math.max(Math.abs(loc.x+bX-mouseOnCanvas.x),Math.abs(loc.y+bY-mouseOnCanvas.y));

                                    if(dist_Mouse2Frame<5 || dist_Mouse2Name<5)
                                    {
                                        ctx.strokeStyle = ctx.fillStyle = "rgba(100, 00, 255,0.8)";

                                        ctx.lineWidth = canvas_obj.rUtil.getIndicationLineSize()/2;
                                        
                                        ctx.beginPath();
                                        ctx.setLineDash([]);
                                        ctx.moveTo(regionInfo.region.x, regionInfo.region.y);
                                        ctx.lineTo(loc.x, loc.y);
                                        ctx.stroke();
                                        ctx.closePath();
                                    }
                                    ctx.lineWidth = lsz;


                                    if(regionInfo.type=="ScanPoint" || regionInfo.type=="BrightnessBalance" || regionInfo.type=="PassThru")//thinner line
                                    {

                                        drawRegion(g, canvas_obj, regionInfo.region, lsz/5);
                                    }
                                    else
                                    {

                                        drawRegion(g, canvas_obj, regionInfo.region, lsz);
                                    }

                                    g.ctx.save();
                                    g.ctx.translate(loc.x, loc.y);
                                    g.ctx.scale(subRegionNameSize,subRegionNameSize);

                                    let yoffset = 0;

                                    if(regionInfo.type=="CALC")
                                    {
                                        ctx.fillText(idText, 0, 0);
                                        if(subreg.compile_error!==undefined)
                                        {
                                            ctx.strokeStyle = ctx.fillStyle = "rgba(179, 0, 0,0.8)";
                                            for(let i=0;i<subreg.compile_error.length;i++)
                                            {
                                                ctx.fillText(subreg.compile_error[i] , 0, yoffset); yoffset += 19;
                                            }
                                        }
                                        else
                                        {
                                            ctx.strokeStyle = ctx.fillStyle = `rgba(100,100,100,0.6)`;
                                            // regionInfo.note
                                            ctx.fillText((subreg.score===null)?"N/A":subreg.score.toFixed(2) , 0, yoffset+subRegionNameSize*15*2); yoffset += 19;
                                        }
                                    }
                                    else
                                    {
                                        ctx.fillText(idText, 0, 0);
                                        ctx.strokeStyle = ctx.fillStyle = `rgba(200,0,200,0.6)`;
                                        let fLoc = ctx.measureText(idText).width;
                                        
                                        ctx.fillText((subreg.score===null)?"N/A":subreg.score.toFixed(2) , fLoc, yoffset); yoffset += 19;
                                    }


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

                                if(regionInfo.type=="PassThru")
                                {

                                }



                                // console.log(subreg);
                                if(subreg.elements!==undefined)
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


                    {
                        let closestInfo={
                            text:"",
                            x:NaN,y:NaN,
                            distance:Infinity,
                            regionInfo:{}
                        }
                        //draw note line when cursor is close to it
                        g_cat.forEach((catInfo: any, _index: number) => {
                            
                            g.ctx.save();
                            let multi_target_column_count=cacheDef.multi_target_column_count||9999;

                            let blockX=_index%multi_target_column_count;
                            let blockY=Math.floor(_index/multi_target_column_count);


                            catInfo.sub_regions.forEach((subreg: any, subreg_index: number) => {
                                let regionInfo = cacheDef.sub_regions[subreg_index]
                                if(regionInfo===undefined)return;

                                let loc=regionInfo.name_loc_offset!==undefined?regionInfo.name_loc_offset:regionInfo.region;
                                let x=cacheDef.w*blockX+loc.x;
                                let y=cacheDef.h*blockY+loc.y;
                                //calc loc to mouseOnCanvas distance
                                let dist=Math.max(Math.abs(x-mouseOnCanvas.x),Math.abs(y-mouseOnCanvas.y));
                                
                                if(regionInfo.note!==undefined &&regionInfo.note.length>0 && dist<closestInfo.distance)
                                {
                                    closestInfo.text=regionInfo.note;
                                    closestInfo.x=x;
                                    closestInfo.y=y;
                                    closestInfo.distance=dist;
                                    closestInfo.regionInfo=regionInfo;
                                }

                            })
                        




                            ctx.restore();
                        })
                        
                        if(closestInfo.distance<5)
                        {
                            // console.log(closestInfo);
                            ctx.save();

                            g.ctx.translate(closestInfo.x, closestInfo.y+subRegionNameSize*15);
                            g.ctx.scale(subRegionNameSize,subRegionNameSize);

                            ctx.fillText(closestInfo.text, 0, 0);



                            ctx.restore();
                        }



                    }

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



function SimpNumpad( props:{value:number,onChange:(v:number)=>void} )
{
    const [v, setV] = useState(props.value||0);


    let width=50;
    return <div style={{boxShadow:"rgba(0,0,0,0.5) 5px 5px 5px" , display:"inline-block"  }}>
        <InputNumber value={v} style={{width:width*3}} onChange={(v)=>{
            setV(v as number);
            // props.onChange(v as number);
        }}/>
        <br/>
        {
        
        [7,8,9,"\n",4,5,6,"\n",1,2,3,"\n","C",0,"<","."].map((ele,index)=>{

            if(typeof ele=="string")
            {
                if(ele=="\n")
                    return <br/>
                if(ele=="C")
                    return <Button key={ele} style={{width}} onClick={()=>{
                        setV(0);
                    }}>{ele}</Button>
                if(ele=="<")
                    return <Button key={ele} style={{width}} onClick={()=>{
                        setV(Math.floor(v/10));
                    }}>{ele}</Button>
            }
            else
            {
                return <Button key={ele} style={{width}} onClick={()=>{
                    setV(v*10+ele);
                    // props.onChange(v*10+ele);
                }}>{ele}</Button>
            }
            return <></>;
        })}
      
        <br/>
        <Button style={{width:width*3}} onClick={()=>{
            props.onChange(v);
        }}>OK</Button>

    </div>
}


let btn_boxshadow="-3px -3px 5px rgba(255,255,255,0.5),3px 3px 5px rgba(70,70,70,0.3), inset -3px -3px 5px rgba(70, 70, 70, 0.3), inset 3px 3px 5px rgba(255, 255, 255, 0.4)"

export function SingleTargetVIEWUI_JSON_Peripheral(props: CompParam_InspTarUI) 
{
    return <>Not implemented</>;
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
                        else
                        {
                            console.error("no reg msg:",msg);
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
                            console.log(i,surface_check_reports[i].source_id)
                            let tarRepIdx=i;

                            let tarRepId = surface_check_reports[tarRepIdx].source_id;

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




        <Button onClick={() => {

        (async () => {
            BPG_API.InspTargetExchange(cacheDef.id, { type: "reloadScript" })
        })()

        }}>reloadScript</Button>




        <Button onClick={() => {

        (async () => {
            BPG_API.send_P("FO",0,{type:"open",path:fsPath+"/script.py"})
        })()

        }}>openScriptInEditor</Button>



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



export function SingleTargetVIEWUI_StageInfoImageSave(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll HXF"} >

    <Button onClick={() => {
        (async () => {

            let pkts = await BPG_API.InspTargetExchange(def.id, {
                type: "SAVE_CACHE",
                // path:"/Users/mdm/workspace",
                // name:"test",
                trigger_id: -4564,
                tags:["s_SIDE_FLAT"],
                addon_tags:["$CAT_OK"]
            }) as any[];
            console.log(pkts);

        })()
    }}>SAVE</Button>
    
    </div>


    </div>;
}



export function SingleTargetVIEWUI_ImgSrc(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;

    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [CameraList, setCameraList] = useState<any[]>([]);

    return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll HXF"} >


        <Dropdown 
            trigger={['click']}
            overlay={
                <div style={{background:"#FFF",border:"5px" }}>
                   

                    {CameraList.map((cam:any,index:number)=>{
                        return cam.formats.map((format:any,findex:number)=>{
                            return <><Button key={index+"-"+findex} onClick={() => {

                                let newDef={...def,CAM_UID:cam.UID,format};
                                console.log(newDef);
                                onDefChange(newDef,true);

                                BPG_API.InspTargetUpdate(newDef)
    
                            }}>{cam.name+" "+format.w+"x"+format.h+" fourcc:"+format.fourcc}</Button>
                            <br/></>
                        })
                       
                    }).flat()
                    }
                </div>
            } 
        >

            <Button onClick={() => {
                (async () => {

                    let pkts = await BPG_API.InspTargetExchange(def.id, {
                        type: "GetCameraList",
                    }) as any[];
                    console.log(pkts);

                    setCameraList(pkts[0].data);

                })()
            }}>{def.CAM_UID||"Empty"}</Button>
        </Dropdown>
                    





    </div>


    </div>;
}




export function SingleTargetVIEWUI_DataTransfer(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange,defDoReload, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;


    const dispatch = useDispatch();
    const [cacheDef, setCacheDef] = useState<any>(def);
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);



    useEffect(() => {
        setCacheDef(def);
        // this.props.ACT_WS_REGISTER(CORE_ID,new BPG_WS());
        // this.props.ACT_WS_CONNECT(CORE_ID, this.coreUrl)
        return (() => {
        });

    }, [def]);


    function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

        if(updatedDef===undefined)
        {
            onDefChange(undefined,false);
            return;   
        }
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)
        })()
        onDefChange(updatedDef, doTakeNewImage);
    }

    console.log("SingleTargetVIEWUI_DataTransfer", cacheDef,def);
    return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll HXF"} >

        <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />

        {((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0)?null:
            <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                onCacheDefChange(newDef, ddd);
            }} 
            
            defDoReload={()=>defDoReload()}
            
            />
        }


    
    </div>


    </div>;
}




export function SingleTargetVIEWUI_TEMPLATE___(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange,defDoReload, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;
    const _this = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        canvasHook: undefined,

        drawHooks: [],
        ctrlHooks: [],

    }).current;
    const dispatch = useDispatch();
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(report);
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    console.log("Local_IMCM",Local_IMCM,"defReport",defReport);

    enum EditState {
        Normal_Show = 0,
        Feature_Edit = 1,
        Test_Saved_Files = 3,


        MISC_Settings = 999,
        NA = -99999
    }

    const [editState, _setEditState] = useState<EditState>(EditState.Normal_Show);

    function setEditState(newEditState: EditState) {
        // _this.sel_region = 
        // _this.sel_region_type = undefined;
        // if (_this.canvasComp == undefined) return;
        //     _this.canvasComp.UserRegionSelect(undefined)


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


    useEffect(() => {
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
                    console.log("++++++++\n",IM,CM,RP);


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

    function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

        if(updatedDef===undefined)
        {
            onDefChange(undefined,false);
            return;   
        }
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)
        })()
        onDefChange(updatedDef, doTakeNewImage);
    }

    console.log("SingleTargetVIEWUI_DataTransfer", cacheDef,def);




    let EDIT_UI = null;
    console.log("editState",editState);
    switch (editState) {

        case EditState.Normal_Show:



        EDIT_UI = <>
                <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
        
                {((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0)?null:
                    <>
                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        onCacheDefChange(newDef, ddd);
                    }} 
                    
                    defDoReload={()=>defDoReload()}
                    
                    />
        
                    
                    </>
                }   

                <Button onClick={() => {
                    console.log(">>>")
                    setEditState(EditState.Test_Saved_Files);
                    }}>測試儲存圖檔</Button>
            </>
            break;
        case EditState.Test_Saved_Files:

            let folderPath = cacheDef.testInputFolder || fsPath;
            let result_InspTar_stream_id = 51001;//HACK hard coded
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>
                <TestInputSelectUI def={cacheDef} testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
            break;

    }



    return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll HXF"} >
        {EDIT_UI}


    
    </div>

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



            }

        }
        } />


    </div>;
}






export function LineFitting_drawreport(def:any, report:any,ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)
{   

    let pt1=report?.report?.pt1;
    let pt2=report?.report?.pt2;
    if(report!==undefined && pt1!==undefined && pt2!==undefined)
    {

        // console.log("report",report);
        //draw line defReport.pt1,defReport.pt2
        g.ctx.strokeStyle="red";
        g.ctx.beginPath();
        g.ctx.moveTo(pt1.x,pt1.y);
        g.ctx.lineTo(pt2.x,pt2.y);
        g.ctx.stroke();
        g.ctx.closePath();
    }
    
}



export function SingleTargetVIEWUI_LineFitting(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange,defDoReload, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;
    const _this = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        canvasHook: undefined,

        drawHooks: [],
        ctrlHooks: [],

    }).current;
    const dispatch = useDispatch();
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(report);
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    console.log("Local_IMCM",Local_IMCM,"defReport",defReport);

    enum EditState {
        Normal_Show = 0,
        Feature_Edit = 1,
        Test_Saved_Files = 3,


        MISC_Settings = 999,
        NA = -99999
    }

    const [editState, _setEditState] = useState<EditState>(EditState.Normal_Show);

    function setEditState(newEditState: EditState) {
        // _this.sel_region = 
        // _this.sel_region_type = undefined;
        // if (_this.canvasComp == undefined) return;
        //     _this.canvasComp.UserRegionSelect(undefined)


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


    useEffect(() => {
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
                    console.log("++++++++\n",IM,CM,RP);


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

    function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

        if(updatedDef===undefined)
        {
            onDefChange(undefined,false);
            return;   
        }
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)
        })()
        onDefChange(updatedDef, doTakeNewImage);
    }

    console.log("SingleTargetVIEWUI_DataTransfer", cacheDef,def);




    let EDIT_UI = null;
    console.log("editState",editState);
    switch (editState) {

        case EditState.Normal_Show:



        EDIT_UI = <>
                <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
        
                {((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0)?null:
                    <>
                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        onCacheDefChange(newDef, ddd);
                    }} 
                    
                    defDoReload={()=>defDoReload()}
                    
                    />
        
                    
                    </>
                }   

                <Button onClick={() => {
                    console.log(">>>")
                    setEditState(EditState.Test_Saved_Files);
                    }}>測試儲存圖檔</Button>
            </>
            break;
        case EditState.Test_Saved_Files:

            let folderPath = cacheDef.testInputFolder || fsPath;
            let result_InspTar_stream_id = 51001;//HACK hard coded
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>
                <TestInputSelectUI def={cacheDef} testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
            break;

    }



    return <div style={{ ...style }} className={"overlayCon"}>
    <div className={"overlay scroll"} >
        {EDIT_UI}


    
    </div>

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




            let cDef={...cacheDef,..._this.UI_def};
            let boxAngle=cDef.region.angle;
            let rotation_control_point={x:0,y:-cDef.region.h/2*1.1};
            let center_control_point={x:0,y:0};
            let sizing_control_point={x:cDef.region.w/2,y:-    cDef.region.h/2};
            let mouseOnCanvas_BOX={x:0,y:0};
            {
                //offset by cDef.region
                let _mouseOnCanvas_BOX = {...mouseOnCanvas};
                _mouseOnCanvas_BOX.x -= cDef.region.x;
                _mouseOnCanvas_BOX.y -= cDef.region.y;
                //rotate by cDef.region.angle
                let angle = boxAngle*Math.PI/180;
                let cos = Math.cos(angle);
                let sin = Math.sin(angle);

                mouseOnCanvas_BOX.x = _mouseOnCanvas_BOX.x*cos - _mouseOnCanvas_BOX.y*sin;
                mouseOnCanvas_BOX.y = _mouseOnCanvas_BOX.x*sin + _mouseOnCanvas_BOX.y*cos;
                // console.log("mouseOnCanvas_BOX",mouseOnCanvas_BOX);
            }

            let nearRange = 10/camMag;
            let isMouseCloseToRotationControlPoint = (mouseOnCanvas_BOX.x-rotation_control_point.x)**2 + (mouseOnCanvas_BOX.y-rotation_control_point.y)**2 < nearRange**2;
            let isMouseCloseToSizingControlPoint = (mouseOnCanvas_BOX.x-sizing_control_point.x)**2 + (mouseOnCanvas_BOX.y-sizing_control_point.y)**2 < nearRange**2;
            let isMouseCloseToCenterControlPoint = (mouseOnCanvas_BOX.x-center_control_point.x)**2 + (mouseOnCanvas_BOX.y-center_control_point.y)**2 < nearRange**2;
            if (ctrl_or_draw == true)//ctrl
            {
                let isMousePressed=g.mouseStatus.status==1;
                let ispreMousePressed=g.mouseStatus.pstatus==1;


                _this.mouseOnCanvas=mouseOnCanvas;

                _this.mouseOnCanvas_BOX=mouseOnCanvas_BOX;
                if(isMousePressed && !ispreMousePressed)//mouse down
                {
                    if(isMouseCloseToCenterControlPoint)
                    {
                        console.log("center");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.region.x=_this.mouseOnCanvas.x;
                            _this.UI_def.region.y=_this.mouseOnCanvas.y;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToRotationControlPoint)
                    {
                        console.log("rotation");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.region.angle=-Math.atan2(
                                _this.mouseOnCanvas.y-_this.UI_def.region.y,
                                _this.mouseOnCanvas.x-_this.UI_def.region.x)*180/Math.PI-90;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToSizingControlPoint)
                    {
                        console.log("sizing");

                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            // _this.mouseOnCanvas_BOX.x;
                            // _this.mouseOnCanvas_BOX.y;
                            
                            _this.UI_def.region.w=Math.abs(_this.mouseOnCanvas_BOX.x*2);
                            _this.UI_def.region.h=Math.abs(_this.mouseOnCanvas_BOX.y*2);

                        
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }

                }
                if(!isMousePressed && ispreMousePressed)//mouse up
                {   
                    if(_this.UI_def!==undefined)
                        onCacheDefChange(_this.UI_def);
                    _this.mouseControlCB=undefined;
                    _this.UI_def=undefined;
                    canvas_obj.UserRegionSelect(undefined);
                }


                if(isMousePressed)//dragging
                {
                    console.log("UI_def",_this.UI_def);
                    if(_this.mouseControlCB!==undefined)
                    {
                        _this.mouseControlCB();
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

                LineFitting_drawreport(cDef,defReport,ctrl_or_draw,g,canvas_obj);
              


                {
                    //draw cDef
                    // console.log("cDef",cDef);
                    //draw box 10x10
                    g.ctx.save();
                    g.ctx.strokeStyle = "red";
                    g.ctx.translate(cDef.region.x, cDef.region.y);
                    g.ctx.rotate(-boxAngle*Math.PI/180);
                    //in box orientation


                    g.ctx.strokeRect(-cDef.region.w/2, -cDef.region.h/2, cDef.region.w, cDef.region.h);



                    {//draw sizing control point(size control point)
                        g.ctx.fillStyle = isMouseCloseToSizingControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(sizing_control_point.x, sizing_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }
                    {
                        //draw center control point(translation control point)
                        g.ctx.fillStyle = isMouseCloseToCenterControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(center_control_point.x, center_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }
                    {

                        //draw rotation control point on top of the box
                        g.ctx.fillStyle = isMouseCloseToRotationControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(rotation_control_point.x, rotation_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }



                    {
                        g.ctx.strokeStyle = "red";

                        // g.ctx.strokeRect(mouseOnCanvas_BOX.x, mouseOnCanvas_BOX.y, 10, 10);
                        //arc
                        g.ctx.beginPath();
                        g.ctx.arc(mouseOnCanvas_BOX.x, mouseOnCanvas_BOX.y, 5, 0, Math.PI*2);
                        g.ctx.stroke();
                        g.ctx.closePath();
    
                    }

                    g.ctx.restore();

                }


            }

        }
        } />


    </div>;
}




export function DirectionalCaliper_drawreport(def:any, report:any,ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)
{
    if(report!==undefined && report.report.location!==undefined)
    {

        let boxAngle=def.region.angle;
        g.ctx.strokeStyle="red";
        g.ctx.beginPath();
        g.ctx.moveTo(report.report.location.x - 5, report.report.location.y );
        g.ctx.lineTo(report.report.location.x + 5, report.report.location.y);
        g.ctx.moveTo(report.report.location.x    , report.report.location.y + 5);
        g.ctx.lineTo(report.report.location.x    , report.report.location.y - 5);
        g.ctx.stroke();
        g.ctx.closePath();


        {//draw refence line with angle
            
            g.ctx.save();
            g.ctx.strokeStyle = "blue";
            g.ctx.translate(report.report.location.x,report.report.location.y);
            g.ctx.rotate(-boxAngle*Math.PI/180);
            g.ctx.moveTo(0,-def.region.h/2);
            g.ctx.lineTo(0,def.region.h/2);
            g.ctx.stroke();
            g.ctx.closePath();
            g.ctx.restore();

        }
    }
    
}

export function SingleTargetVIEWUI_DirectionalCaliper(props: CompParam_InspTarUI) {
    let { display, fsPath,EditPermitFlag, style = undefined, renderHook, def, report, onDefChange,defDoReload, UIOption,onUIOptionUpdate,showUIOptionConfigUI=false ,APIExport} = props;
    const _this = useRef<any>({

        imgCanvas: document.createElement('canvas'),
        canvasComp: undefined,
        canvasHook: undefined,

        drawHooks: [],
        ctrlHooks: [],

    }).current;
    const dispatch = useDispatch();
    const [cacheDef, setCacheDef] = useState<any>(def);

    const [defReport, setDefReport] = useState<any>(report);
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [Local_IMCM, setLocal_IMCM] =
        useState<IMCM_type | undefined>(undefined);

    console.log("Local_IMCM",Local_IMCM,"defReport",defReport);

    enum EditState {
        Normal_Show = 0,
        Feature_Edit = 1,
        Test_Saved_Files = 3,


        MISC_Settings = 999,
        NA = -99999
    }

    const [editState, _setEditState] = useState<EditState>(EditState.Normal_Show);

    function setEditState(newEditState: EditState) {
        // _this.sel_region = 
        // _this.sel_region_type = undefined;
        // if (_this.canvasComp == undefined) return;
        //     _this.canvasComp.UserRegionSelect(undefined)


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


    useEffect(() => {
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
                    console.log("++++++++\n",IM,CM,RP);


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

    function onCacheDefChange(updatedDef: any, doTakeNewImage: boolean = true) {

        if(updatedDef===undefined)
        {
            onDefChange(undefined,false);
            return;   
        }
        console.log(updatedDef);
        setCacheDef(updatedDef);



        (async () => {
            await BPG_API.InspTargetUpdate(updatedDef)
            await BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
        })()
        onDefChange(updatedDef, doTakeNewImage);
    }

    console.log("SingleTargetVIEWUI_DataTransfer", cacheDef,def);




    let EDIT_UI = null;
    console.log("editState",editState);
    switch (editState) {

        case EditState.Normal_Show:



        EDIT_UI = <>
                <Input maxLength={100} value={cacheDef.id} disabled
                    style={{ width: "200px" }}
                    onChange={(e) => {
                    }} />
        
                {((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0)?null:
                    <>
                    <InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
                        onCacheDefChange(newDef, ddd);
                    }} 
                    
                    defDoReload={()=>defDoReload()}
                    
                    />
        
                    
                    </>
                }   

                <Button onClick={() => {
                    console.log(">>>")
                    setEditState(EditState.Test_Saved_Files);
                    }}>測試儲存圖檔</Button>

                
                edge_type:
                <InputNumber min={-1} max={1} precision={1} value={cacheDef.edge_type} onChange={(e) => {
                    console.log("edge_type", e);
                    onCacheDefChange({ ...cacheDef, edge_type: e });
                }} />

                sobelLowSurpress:
                <InputNumber min={0} max={1000} precision={1} value={cacheDef.sobelLowSurpress} onChange={(e) => {
                    console.log("sobelLowSurpress", e);
                    onCacheDefChange({ ...cacheDef, sobelLowSurpress: e });
                }} />


                angle:
                <InputNumber min={-360} max={360} precision={0.1} value={cacheDef.region.angle} onChange={(e) => {
                    console.log("angle", e);
                    onCacheDefChange({ ...cacheDef, region: { ...cacheDef.region, angle: e } });
                }} />
            </>
            break;
        case EditState.Test_Saved_Files:

            let folderPath = cacheDef.testInputFolder || fsPath;
            let result_InspTar_stream_id = 51001;//HACK hard coded
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>
                <TestInputSelectUI def={cacheDef} testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
            break;

    }



    return <div style={{ ...style }} className={"overlayCon"}>
        
    <div className={"overlay scroll"} >
        {EDIT_UI}


    
    </div>

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




            let cDef={...cacheDef,..._this.UI_def};
            let boxAngle=cDef.region.angle;
            let rotation_control_point={x:0,y:-cDef.region.h/2*1.1};
            let center_control_point={x:0,y:0};
            let sizing_control_point={x:cDef.region.w/2,y:-    cDef.region.h/2};
            let mouseOnCanvas_BOX={x:0,y:0};
            {
                //offset by cDef.region
                let _mouseOnCanvas_BOX = {...mouseOnCanvas};
                _mouseOnCanvas_BOX.x -= cDef.region.x;
                _mouseOnCanvas_BOX.y -= cDef.region.y;
                //rotate by cDef.region.angle
                let angle = boxAngle*Math.PI/180;
                let cos = Math.cos(angle);
                let sin = Math.sin(angle);

                mouseOnCanvas_BOX.x = _mouseOnCanvas_BOX.x*cos - _mouseOnCanvas_BOX.y*sin;
                mouseOnCanvas_BOX.y = _mouseOnCanvas_BOX.x*sin + _mouseOnCanvas_BOX.y*cos;
                // console.log("mouseOnCanvas_BOX",mouseOnCanvas_BOX);
            }

            let nearRange = 10/camMag;
            let isMouseCloseToRotationControlPoint = (mouseOnCanvas_BOX.x-rotation_control_point.x)**2 + (mouseOnCanvas_BOX.y-rotation_control_point.y)**2 < nearRange**2;
            let isMouseCloseToSizingControlPoint = (mouseOnCanvas_BOX.x-sizing_control_point.x)**2 + (mouseOnCanvas_BOX.y-sizing_control_point.y)**2 < nearRange**2;
            let isMouseCloseToCenterControlPoint = (mouseOnCanvas_BOX.x-center_control_point.x)**2 + (mouseOnCanvas_BOX.y-center_control_point.y)**2 < nearRange**2;
            if (ctrl_or_draw == true)//ctrl
            {
                let isMousePressed=g.mouseStatus.status==1;
                let ispreMousePressed=g.mouseStatus.pstatus==1;


                _this.mouseOnCanvas=mouseOnCanvas;

                _this.mouseOnCanvas_BOX=mouseOnCanvas_BOX;
                if(isMousePressed && !ispreMousePressed)//mouse down
                {
                    if(isMouseCloseToCenterControlPoint)
                    {
                        console.log("center");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.region.x=_this.mouseOnCanvas.x;
                            _this.UI_def.region.y=_this.mouseOnCanvas.y;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToRotationControlPoint)
                    {
                        console.log("rotation");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.region.angle=-Math.atan2(
                                _this.mouseOnCanvas.y-_this.UI_def.region.y,
                                _this.mouseOnCanvas.x-_this.UI_def.region.x)*180/Math.PI-90;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToSizingControlPoint)
                    {
                        console.log("sizing");

                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            // _this.mouseOnCanvas_BOX.x;
                            // _this.mouseOnCanvas_BOX.y;
                            
                            _this.UI_def.region.w=Math.abs(_this.mouseOnCanvas_BOX.x*2);
                            _this.UI_def.region.h=Math.abs(_this.mouseOnCanvas_BOX.y*2);

                        
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }

                }
                if(!isMousePressed && ispreMousePressed)//mouse up
                {   
                    if(_this.UI_def!==undefined)
                        onCacheDefChange(_this.UI_def);
                    _this.mouseControlCB=undefined;
                    _this.UI_def=undefined;
                    canvas_obj.UserRegionSelect(undefined);
                }


                if(isMousePressed)//dragging
                {
                    console.log("UI_def",_this.UI_def);
                    if(_this.mouseControlCB!==undefined)
                    {
                        _this.mouseControlCB();
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


                DirectionalCaliper_drawreport(cDef,defReport,ctrl_or_draw, g, canvas_obj);


                {
                    //draw cDef
                    // console.log("cDef",cDef);
                    //draw box 10x10
                    g.ctx.save();
                    g.ctx.strokeStyle = "red";
                    g.ctx.translate(cDef.region.x, cDef.region.y);
                    g.ctx.rotate(-boxAngle*Math.PI/180);
                    //in box orientation


                    g.ctx.strokeRect(-cDef.region.w/2, -cDef.region.h/2, cDef.region.w, cDef.region.h);



                    {//draw sizing control point(size control point)
                        g.ctx.fillStyle = isMouseCloseToSizingControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(sizing_control_point.x, sizing_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }
                    {
                        //draw center control point(translation control point)
                        g.ctx.fillStyle = isMouseCloseToCenterControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(center_control_point.x, center_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }
                    {

                        //draw rotation control point on top of the box
                        g.ctx.fillStyle = isMouseCloseToRotationControlPoint?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(rotation_control_point.x, rotation_control_point.y, 5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }



                    {
                        g.ctx.strokeStyle = "red";

                        // g.ctx.strokeRect(mouseOnCanvas_BOX.x, mouseOnCanvas_BOX.y, 10, 10);
                        //arc
                        g.ctx.beginPath();
                        g.ctx.arc(mouseOnCanvas_BOX.x, mouseOnCanvas_BOX.y, 5, 0, Math.PI*2);
                        g.ctx.stroke();
                        g.ctx.closePath();
    
                    }

                    g.ctx.restore();

                }


            }

        }
        } />


    </div>;
}



// 1. First define the map of components
const COMPONENT_MAP = {
    Orientation_ShapeBasedMatching: SingleTargetVIEWUI_Orientation_ShapeBasedMatching,
    SurfaceCheckSimple: SingleTargetVIEWUI_SurfaceCheckSimple,
    JSON_Peripheral: SingleTargetVIEWUI_JSON_Peripheral,
    JSON_CNC_Peripheral: SingleTargetVIEWUI_JSON_CNC_Peripheral,
    StageInfoImageSave: SingleTargetVIEWUI_StageInfoImageSave,
    ImgSrc: SingleTargetVIEWUI_ImgSrc,
    DataTransfer: SingleTargetVIEWUI_DataTransfer,
    LineFitting: SingleTargetVIEWUI_LineFitting,
    DirectionalCaliper: SingleTargetVIEWUI_DirectionalCaliper,
    ArcFitting: SingleTargetVIEWUI_ArcFitting,
    CameraCalib: SingleTargetVIEWUI_CameraCalib,
    DimMeasure: SingleTargetVIEWUI_DimMeasure
} as const;

// 2. Generate types from the map
type InspTargetType = keyof typeof COMPONENT_MAP;

// 3. Type guard remains similar
function isValidInspTargetType(type: string): type is InspTargetType {
    return type in COMPONENT_MAP;
}


// 5. Create the MUX component
export function InspTargetUI_MUX(param: CompParam_InspTarUI) {
    const Component = useMemo<React.ComponentType<CompParam_InspTarUI> | null>(() => {
        const type = param?.def?.type;
        
        if (!type) {
            console.warn('No component type provided');
            return null;
        }

        if (!isValidInspTargetType(type)) {
            console.warn(`Invalid component type: ${type}`);
            return null;
        }

        return COMPONENT_MAP[type];
    }, [param?.def?.type]);

    if (!Component) {
        return <div className="error-message">unknown type: {param?.def?.type}</div>;
    }

    return <Component {...param} />;
}

// 6. Export available types for external use
export const InspTargetTypes = Object.keys(COMPONENT_MAP) as InspTargetType[];
