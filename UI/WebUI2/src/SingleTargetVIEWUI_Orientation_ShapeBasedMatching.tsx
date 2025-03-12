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

import {ITGlobalVariableContext,CompParam_GlobalVariable } from './App';

import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';

import { SingleTargetVIEWUI_DimMeasure } from './SingleTargetVIEWUI_DimMeasure';


import { SingleTargetVIEWUI_ArcFitting } from './SingleTargetVIEWUI_ArcFitting';

import { SingleTargetVIEWUI_CameraCalib } from './SingleTargetVIEWUI_CameraCalib';

import { ObjTree,
    PtsToXYWH,
    drawRegion ,
    CompParam_InspTarUI,
    IMCM_type,
    EDIT_PERMIT_FLAG,
    InspTarView_basicInfo,
    TagsEdit_DropDown,
    tagsMatching,
    TestInputSelectUI,
    CountDownCheckPopup
} from './SingleTargetVIEWUI_UTIL';


const { SubMenu } = Menu;
const { Option } = Select;

export function SingleTargetVIEWUI_Orientation_ShapeBasedMatching(props: CompParam_InspTarUI) {
    let { display, fsPath, style = undefined, renderHook, def, EditPermitFlag, report, onDefChange,defDoReload, APIExport } = props;
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



    let DAT_ANY_UNDEF: any = undefined;


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


        MISC_Settings = 9,
        NA = -99999
    }

    const [editState, _setEditState] = useState<EditState>(EditState.Normal_Show);

    function setEditState(newEditState: EditState,info:any=undefined) {
        _this.sel_region = 
        _this.sel_region_type = undefined;
        if (_this.canvasComp == undefined) return;
            _this.canvasComp.UserRegionSelect(undefined)


    let state3Ev: EditState[] = [];//3 elements, leave,stay,enter
        if (newEditState != editState) {
            state3Ev = [editState, EditState.NA, newEditState]
        }
        else
        {
            state3Ev = [ EditState.NA, newEditState,  EditState.NA]
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
                    if (idx == 2 || (idx == 1 && info?.type==="reload_feature_image"))//enter
                    {
                        setFeatureInfo(cacheDef.featureInfo === undefined ? {} : cacheDef.featureInfo);

                        //if(featureInfoExt.IM===undefined)//do a init image fetch
                        (async () => {

                            let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                                type: "extract_feature",
                                image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                                num_features: -1,
                                image_scale: 1,
                            }) as any[];

                            let newFeatureInfoExt: any = {};
                            console.log(pkts);

                            let IM = pkts.find((p: any) => p.type == "IM");
                            if (IM !== undefined) {
                                _this.featureImgCanvas.width = IM.image_info.width;
                                _this.featureImgCanvas.height = IM.image_info.height;

                                let ctx2nd = _this.featureImgCanvas.getContext('2d');
                                
                                if(IM.image_info.image instanceof ImageData)
                                    ctx2nd.putImageData(IM.image_info.image, 0, 0);
                                else if(IM.image_info.image instanceof HTMLImageElement)
                                    ctx2nd.drawImage(IM.image_info.image, 0, 0);



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
                    }} 
                    
                    defDoReload={()=>defDoReload()}
                    
                    />
                    {/* <Button onClick={()=>{
            BPG_API.InspTargetExchange(cacheDef.id,{type:"revisit_cache_stage_info"});
          }}>重試</Button> */}








                    {/* <Button onClick={() => {
                        onDefChange(cacheDef, true)
                    }}>Commit</Button> */}

                    <Button onClick={() => {

                    setEditState(EditState.MISC_Settings);

                    }}><SettingOutlined/></Button>

                    <Button onClick={() => {

                        setEditState(EditState.Feature_Edit);

                    }}>編輯樣本</Button>

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


                <Button disabled={defReport===undefined} onClick={() => {

                    let mmpp=defReport?.mmpp??1;
                    // let mmpp=Number((defReport?.mmpp??1).toFixed(8));
                    console.log(">>>",mmpp);


                    // let default_name = `tid=${defReport.trigger_id} tags=${defReport.tags.toString()} t=${Date.now()}`
                    let default_name = JSON.stringify({
                        tid: defReport.trigger_id,
                        tags: [...defReport.tags,def.id+"_Inject"],
                        t: Date.now(),
                        mmpp
                    })
                    
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
                            name: default_name,
                            report: defReport,
                        },
                        contentCB: (minfo: typeof modalInfo) =>{ 
                        console.log(minfo)
                        return<>

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
                        }
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

                <CountDownCheckPopup countdown={5} onConfirm={async()=>{

                        let imgProps = await BPG_API.InspTargetExchange(cacheDef.id, {
                            type: "get_cache_image_prop"
                        }) as any[];
                        console.log(imgProps);
                        let mmpp=imgProps?.[0]?.data?.mmpp;
                        console.log(mmpp);
                        if(mmpp===undefined)return;//failed

                        console.log(featureInfo);
                        setFeatureInfo({ ...featureInfo, mmpp,mask_regions:[],refine_match_regions:[] })
                        let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                            type: "cache_image_save",
                            folder_path: fsPath + "/",
                            image_name: SBM_FEAT_REF_IMG_NAME,
                        }) as any[];
                        console.log(pkts,fsPath,SBM_FEAT_REF_IMG_NAME);

                        console.log("reload_feature_image");
                        // setEditState(EditState.Feature_Edit,{type:"reload_feature_image"});

                        {//load feature image

                            let pkts = await BPG_API.InspTargetExchange(cacheDef.id, {
                                type: "extract_feature",
                                image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                                num_features: -1,
                                image_scale: 1,
                            }) as any[];
    
                            let newFeatureInfoExt: any = {};
                            console.log(pkts);
    
                            let IM = pkts.find((p: any) => p.type == "IM");
                            if (IM !== undefined) {
                                _this.featureImgCanvas.width = IM.image_info.width;
                                _this.featureImgCanvas.height = IM.image_info.height;
    
                                let ctx2nd = _this.featureImgCanvas.getContext('2d');
                                
                                ctx2nd.drawImage(IM.image_info.image, 0, 0);
    
    
    
                                newFeatureInfoExt.IM = IM;
    
                            }
    
                            setFeatureInfoExt({ ...featureInfoExt, ...newFeatureInfoExt })
    
                        }


                }}>
                    <Button danger type="primary" onClick={() => {
                    }}>使用最新圖檔重置</Button>
                </CountDownCheckPopup>


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

                特徵最小間距:
                <InputNumber value={featureInfo.feature_min_space}
                    onChange={(num) => {
                        setFeatureInfo({ ...featureInfo, feature_min_space: num })
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


                    let tarT=[2,4,8];
                    let obj = {
                        type: "extract_feature",
                        image_path: fsPath + "/" + SBM_FEAT_REF_IMG_NAME,
                        num_features: cacheDef.num_features,
                        weak_thresh: featureInfo.weak_thresh,
                        strong_thresh: featureInfo.strong_thresh,
                        feature_min_space: featureInfo.feature_min_space,
                        T: tarT,
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


												if (IM.image_info.image instanceof ImageData)
													ctx2nd?.putImageData(IM.image_info.image, 0, 0);
												else if (IM.image_info.image instanceof HTMLImageElement)
													ctx2nd?.drawImage(IM.image_info.image, 0, 0);

                        newFeatureInfoExt.IM = IM;

                    }


                    let RP = pkts.find((p: any) => p.type == "RP");
                    if (RP !== undefined) {
                        newFeatureInfo.templatePyramid = RP.data;
                    }

                    newFeatureInfo.templatePyramid=newFeatureInfo.templatePyramid.filter((pyr: any) => pyr.features.length > 0)

                    if(tarT.length>newFeatureInfo.templatePyramid.length)
                    {
                        tarT=tarT.slice(0,newFeatureInfo.templatePyramid.length);
                        
                    }

                    console.log(newFeatureInfo.templatePyramid,tarT )


                    setFeatureInfo({ ...featureInfo, ...newFeatureInfo, T:tarT})
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

                    console.log(cacheDef);
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
                {"["}
                <InputNumber min={-360} max={360} step={1} value={cacheDef.featureInfo.match_front_face_angle_segs}
                    onChange={(num) => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["featureInfo", "match_front_face_angle_segs"], num));
                    }} />

                {"]"}


								matching_angle_apart:

                <InputNumber value={cacheDef.matching_angle_apart} step={1}
                    onChange={(num) => {

                        setCacheDef({ ...cacheDef, matching_angle_apart: num })
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
                                        if (_this.canvasComp !== undefined) 
                                            _this.canvasComp.UserRegionSelect(undefined);
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
                                    console.log(">>>");
                                    if (_this.canvasComp !== undefined)
                                    {
                                        _this.sel_region = undefined;
                                        _this.sel_region_type = "region"

                                        let search_regions = cacheDef.search_regions === undefined ? [] : [...cacheDef.search_regions];
                                        if(idx<search_regions.length)
                                        {

                                            _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                                                if (state == 2) {
                                                    console.log(info);
                        
                                                    let x, y, w, h;
                        
                                                    let roi_region = PtsToXYWH(info.pt1, info.pt2);
                                                    console.log(roi_region)
                                                    let regInfo = { ...roi_region, isBlackRegion: false };
                                                    
                                                    search_regions[idx]=regInfo;
                                                    setCacheDef({ ...cacheDef, search_regions })
                        
                                                    _this.sel_region_type = undefined;
                                                    if (_this.canvasComp !== undefined) 
                                                        _this.canvasComp.UserRegionSelect(undefined)
                                                }
                                            })
                                        }
                                    }
        
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

                {["<", ">", "v", "^","↦","↤","↧","↥"].map((dir, idx) => {

                    return <Button key={"AddNewRegion" + dir} onClick={() => {
                        
                        let new_search_regions = [...cacheDef.search_regions];
                        console.log(new_search_regions)

                        let offset={x:0,y:0,w:0,h:0};//x,y,w,h
                        let step=5;
                        switch(dir)
                        {
                            case "<":
                                offset.x=-step;
                                break;
                            case ">":
                                offset.x=step;
                                break;
                            case "v":
                                offset.y=step;
                                break;
                            case "^":
                                offset.y=-step;
                                break;
                            case "↦":
                                offset.w=step;
                                break;
                            case "↤":
                                offset.w=-step;
                                break;

                            case "↧":
                                offset.h=step;
                                break;
                            case "↥":
                                offset.h=-step;
                                break;
                        }
                        new_search_regions=new_search_regions.map((regi:any)=>{
                            return {...regi,x:regi.x+offset.x,y:regi.y+offset.y,w:regi.w+offset.w,h:regi.h+offset.h}
                        })
                        setCacheDef({ ...cacheDef, search_regions: new_search_regions })
                        setUpdateC(updateC + 1);
                    }}>{dir}</Button>

                })}

                
                <Switch checkedChildren="開啟顏色預處理" unCheckedChildren="關閉顏色預處理" checked={cacheDef?.mask?.enable == true} onChange={(check) => {
                    setCacheDef(ObjShellingAssign(cacheDef, ["mask","enable"], check));
                }} />

                <Slider defaultValue={def?.mask?.hh} min={0} max={180} step={1} onChange={(v) => {
                    _this.trigTO = ID_debounce(_this.trigTO, () => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["mask","hh"], v));
                    }, () => _this.trigTO = undefined, 500);
                }} />

                <Slider defaultValue={def?.mask?.lh} min={0} max={180} step={1} onChange={(v) => {
                    _this.trigTO = ID_debounce(_this.trigTO, () => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["mask","lh"], v));
                    }, () => _this.trigTO = undefined, 500);
                }} />

                <Slider defaultValue={def?.mask?.blur1_size} min={0} max={50} step={1} onChange={(v) => {
                    _this.trigTO = ID_debounce(_this.trigTO, () => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["mask","blur1_size"], v));
                    }, () => _this.trigTO = undefined, 500);
                }} />

                <Slider defaultValue={def?.mask?.blur2_size} min={0} max={50} step={1} onChange={(v) => {
                    _this.trigTO = ID_debounce(_this.trigTO, () => {
                        setCacheDef(ObjShellingAssign(cacheDef, ["mask","blur2_size"], v));
                    }, () => _this.trigTO = undefined, 500);
                }} />




            </>
            break;




        case EditState.Test_Saved_Files: {

            let folderPath = cacheDef.testInputFolder || fsPath;
            let result_InspTar_stream_id = 51001;//HACK hard coded
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {

                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>
                <TestInputSelectUI def={cacheDef} testTags={[def.id + "_Inject"]} folderPath={folderPath} stream_id={result_InspTar_stream_id}></TestInputSelectUI>
            </>
        } break;


        case EditState.MISC_Settings: {
            let folderPath = cacheDef.testInputFolder || fsPath;
            EDIT_UI = <>
                <Button danger type="primary" onClick={() => {
                    setEditState(EditState.Normal_Show)
                }}>{"<"}</Button>

                <Button 
                    danger={cacheDef.display_origin!==undefined}
                    onClick={() => {
                    if (_this.canvasComp == undefined) return;
                    if(cacheDef.display_origin!==undefined)
                    {   
                        let redef={ ...cacheDef}
                        delete redef.display_origin;
                        setCacheDef(redef)
                        return;
                    }
                    _this.sel_region = undefined;
                    _this.sel_region_type = "point"
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {

                            // info.pt2.x;
                            // info.pt2.y;




                            setCacheDef({ ...cacheDef, display_origin: info.pt2 })
                            setUpdateC(updateC + 1);

                            console.log(info);
                            _this.sel_region_type = undefined;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    })
                }}>{`${cacheDef.display_origin!==undefined?"刪除":"設定"}"顯示"座標原點`}</Button>

                <Button onClick={() => {
                    if (_this.canvasComp == undefined) return;
                    _this.sel_region = undefined;
                    _this.sel_region_type = "measure"
                    _this.canvasComp.UserRegionSelect((info: any, state: number) => {
                        if (state == 2) {

                            // info.pt2.x;
                            // info.pt2.y;




                            // setCacheDef({ ...cacheDef, display_origin: info.pt2 })
                            // setUpdateC(updateC + 1);

                            console.log(info);
                            _this.sel_region_type = undefined;
                            _this.canvasComp.UserRegionSelect(undefined)
                        }
                    },"click")
                }}>測試距離</Button>




                <br/>
                {/* Y軸:<Switch checkedChildren="正常向" unCheckedChildren="圖向" checked={cacheDef.display_origin_use_normal_Y_direction ==true} onChange={(check) => {
                    setCacheDef({ ...cacheDef, display_origin_use_normal_Y_direction: check })
                }} /> */}

                Y軸轉換 (unit/pix):<InputNumber value={cacheDef.display_y_axis_scale??1} 
                onChange={(num) => {
                    setCacheDef({ ...cacheDef, display_y_axis_scale:num})
                    setUpdateC(updateC + 1);

                }}/>
                <br/>
                X軸轉換 (unit/pix):<InputNumber value={cacheDef.display_x_axis_scale??1} 
                onChange={(num) => {
                    setCacheDef({ ...cacheDef, display_x_axis_scale:num})
                    setUpdateC(updateC + 1);

                }}/>

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
            if (editState == EditState.Normal_Show || editState == EditState.Search_Region_Edit || editState == EditState.Test_Saved_Files|| editState == EditState.MISC_Settings) {

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

                            g.ctx.save();

                            g.ctx.translate(match.center.x, match.center.y );
                            // let distance=Math.abs(match.center.x-mouseOnCanvas.x)+Math.abs(match.center.y-mouseOnCanvas.y)
                            // ctx.font = (100*Math.pow(1000/(distance+1000),5)+10)+"px Arial";
                            ctx.font = "50px Arial";
                            g.ctx.scale(1.5/camMag,1.5/camMag);

                            ctx.fillStyle = 'hsl('+ Math.floor(idx/10)*100 +',100%,50%)';
                            ctx.strokeStyle = 'black';
                            let text="[" + idx+"]";
                            ctx.fillText(text,0,0- 40)
                            ctx.lineWidth = 2;
                            ctx.strokeText(text, 0,0- 40)
                            ctx.fillStyle = "rgba(150,100, 100,0.8)";



                            ctx.font = "20px Arial";
                            ctx.fillText("ang:" + (angle * 180 / 3.14159).toFixed(2)+(match.flip?" 反 ":""), 0,0- 20)

                            if (match.confidence !== undefined)
                                ctx.fillText("sim:" + match.confidence.toFixed(3), 0,0 - 0)


                            ctx.lineWidth = 4;
                            ctx.strokeStyle = `HSLA(0, 100%, 50%,1)`;
                            canvas_obj.rUtil.drawCross(ctx, { x:0, y: 0}, 12);



                            ctx.lineWidth = 4;
                            let vec = PtRotate2d({ x: 100, y: 0 }, angle, 1);
                            canvas_obj.rUtil.drawLine(ctx, { x1: 0, y1: 0, x2: 0 + vec.x, y2:0+ vec.y })
                            g.ctx.restore();

                        })

                        {
                            ctx.save();
                            ctx.resetTransform();
                            ctx.font = "20px Arial";
                            ctx.fillStyle = "rgba(150,100, 100,0.5)";
                            ctx.fillText("ProcessTime:" + (defReport.process_time_us / 1000).toFixed(2) + " ms", 20, 400)
                            

                            let tagsStr="";
                            if(defReport.tags!==undefined)
                            {
                                tagsStr=defReport.tags.join(",");
                            }
                            ctx.fillText("tags:"+tagsStr, 20, 400+20*(1))



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
                                ctx.font = "40px Arial";
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

                {
                    ctx.save();

                    // let lineLengthMult=500/camMag;
                    //offset draw oirigin
                    {
                        let x=0;
                        let y=0;
                        if(cacheDef.display_origin!==undefined)
                        {
                            x=cacheDef.display_origin.x;
                            y=cacheDef.display_origin.y;
                        }
                        ctx.translate(x, y);
                    }

                    ctx.scale(5, 5);

                    ctx.lineWidth = 1/camMag;
                    //reset dash
                    ctx.setLineDash([0, 0, 0, 0]);

                    //font size
                    ctx.font = (1/camMag)+"em Arial";
                    //aline text to XY center
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";

                    let X_mult=1;
                    if(cacheDef.display_x_axis_scale<0)
                    {
                        X_mult=-1;
                    }

                    let Y_mult=1;
                    if(cacheDef.display_y_axis_scale<0)
                    {
                        Y_mult=-1;
                    }
                    //draw origin and axis arrow
                    //draw x axis
                    ctx.strokeStyle = "rgba(255, 0, 0,1)";
                    ctx.fillStyle = "rgba(255, 0, 0,1)";
                    canvas_obj.rUtil.drawLine(ctx, { x1: 0, y1: 0, x2: 100*X_mult, y2: 0 })
                    canvas_obj.rUtil.drawLine(ctx, { x1: 100*X_mult, y1: 0, x2: 90*X_mult, y2: 10 })
                    canvas_obj.rUtil.drawLine(ctx, { x1: 100*X_mult, y1: 0, x2: 90*X_mult, y2: -10 })
                    
                    // ctx.strokeStyle = "rgba(0, 0, 0,1)";
                    // ctx.strokeText("X", 100*X_mult, -10*Y_mult)
                    // ctx.fillText("X", 100*X_mult, -10*Y_mult)

                    //draw y axis
                    ctx.fillStyle =
                    ctx.strokeStyle = "rgba(0, 255, 0,1)";


                    canvas_obj.rUtil.drawLine(ctx, { x1: 0, y1: 0, x2: 0, y2: 100*Y_mult })
                    canvas_obj.rUtil.drawLine(ctx, { x1: 0, y1: 100*Y_mult, x2: 10, y2: 90*Y_mult })
                    canvas_obj.rUtil.drawLine(ctx, { x1: 0, y1: 100*Y_mult, x2: -10, y2: 90*Y_mult })

                    // ctx.strokeStyle = "rgba(0, 0, 0,1)";
                    // ctx.strokeText("Y", -10*X_mult, 100*Y_mult)
                    // ctx.fillText("Y", -10*X_mult, 100*Y_mult)

                    //scale x5


                    ctx.restore();
                    

                }




                if (_this.sel_region_type == "point") {
                    // console.log(mouseOnCanvas)

                    drawRegion(g, canvas_obj,{
                        x: mouseOnCanvas.x-5,
                        y: mouseOnCanvas.y-5,
                        w: 10,
                        h: 10
                    }, canvas_obj.rUtil.getIndicationLineSize());
                }


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

                    if (_this.sel_region_type == "measure") {
                        // canvas_obj.rUtil.drawCross(ctx, { x: _this.sel_region.pt1.x, y: _this.sel_region.pt1.y }, 10/camMag);

                        ctx.setLineDash([0, 0, 0, 0]);
                        canvas_obj.rUtil.drawLine(ctx, {
                            x1: _this.sel_region.pt1.x,
                            y1: _this.sel_region.pt1.y,
                            x2: _this.sel_region.pt2.x,
                            y2: _this.sel_region.pt2.y
                        })
                        

                        let dx=(_this.sel_region.pt2.x-_this.sel_region.pt1.x)*(cacheDef.display_x_axis_scale??1);
                        let dy=(_this.sel_region.pt2.y-_this.sel_region.pt1.y)*(cacheDef.display_y_axis_scale??1);

                        let dist=Math.sqrt(dx*dx+dy*dy);
                        ctx.save();
                        ctx.resetTransform();
                        ctx.font = "1.5em Arial";
                        ctx.fillStyle = "rgba(250,100, 50,1)";
                        ctx.fillText(`距離: ${dist.toFixed(2)}`,  g.mouseStatus.x+10, g.mouseStatus.y+35)

                        ctx.restore();

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
                    
                    ctx.fillStyle = "rgba(250,100, 50,1)";

                    let dispX=mouseOnCanvas.x;
                    let dispY=mouseOnCanvas.y;

                    let altered=false;
                    if(cacheDef.display_origin!==undefined)
                    {
                        dispX-=cacheDef.display_origin.x;
                        dispY-=cacheDef.display_origin.y;  
                        
                        altered=true;
                    }
                    if(cacheDef.display_x_axis_scale!==undefined)
                    {
                        dispX*=cacheDef.display_x_axis_scale;  
                        altered=true;
                    }
                    if(cacheDef.display_y_axis_scale!==undefined)
                    {    
                        dispY*=cacheDef.display_y_axis_scale;  
                        altered=true;

                    }

                    ctx.font = "2em Arial";
                    ctx.fillStyle = "rgba(250,100, 50,1)";
                    ctx.fillText(`${dispX.toFixed(1)},${dispY.toFixed(1)}`, g.mouseStatus.x, g.mouseStatus.y)

                    ctx.font = "1.5em Arial";
                    ctx.fillStyle = "rgba(150,0, 200,1)";
                    ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`, g.mouseStatus.x+25, g.mouseStatus.y+15)
                    // console.log(mouseOnCanvas)
                    // ctx.font = "5em Arial";
                    // ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`, g.mouseStatus.x, g.mouseStatus.y)




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


