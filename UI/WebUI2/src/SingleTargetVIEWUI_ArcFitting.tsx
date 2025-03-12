
import {ObjTree,IMCM_type,EDIT_PERMIT_FLAG,CompParam_InspTarUI,InspTarView_basicInfo,TestInputSelectUI} from './SingleTargetVIEWUI_UTIL';
import {useRef,useState,useEffect} from "react";
import {useDispatch} from "react-redux";
import { EXT_API_ACCESS} from './redux/actions/EXT_API_ACT';

import { CORE_ID, BPG_WS } from './EXT_API';

import { type_IMCM } from './AppTypes';

import { Input,Button } from 'antd';
import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';

import clone from 'clone';
import { threePointToArc} from './UTIL/MathTools';

import { InputNumber } from 'antd';








export function ArcFitting_drawreport(def:any, report:any,ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent)
{
   
    if(report!==undefined)
    {//draw report
        // console.log("defReport",defReport);
        //draw line defReport.pt1,defReport.pt2
        let camMag = canvas_obj.camera.GetCameraScale();
        g.ctx.lineWidth=2/camMag;
        g.ctx.strokeStyle="red";
        g.ctx.beginPath();
        let arcInfo=report.report;
				// console.log(report);

        g.ctx.arc(arcInfo.center.x,arcInfo.center.y,arcInfo.radius
            ,0,2*Math.PI);
        g.ctx.stroke();

        g.ctx.closePath();
        

    }

}

export function SingleTargetVIEWUI_ArcFitting(props: CompParam_InspTarUI) {
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

    const [show_detail_setting, setShow_detail_setting] = useState<boolean>(false);
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
        console.log("new----def",def);
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
    console.log("def",cacheDef);

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

                margin:inner:
                <InputNumber min={0} max={1000} precision={0.05} value={cacheDef.innerMargin} onChange={(e) => {
                    console.log("innerMargin", e);
                    onCacheDefChange({ ...cacheDef, innerMargin: e });
                }} />
                ~outer:
                <InputNumber min={0} max={1000} precision={0.05} value={cacheDef.outerMargin} onChange={(e) => {
                    console.log("outerMargin", e);
                    onCacheDefChange({ ...cacheDef, outerMargin: e });
                }} />
                noise_threshold~:
                <InputNumber min={0} max={1000} precision={1} value={cacheDef.noise_threshold} onChange={(e) => {
                    console.log("noise_threshold", e);
                    onCacheDefChange({ ...cacheDef, noise_threshold: e });
                }} />

                edge_type:
                <InputNumber min={-1} max={1} precision={1} value={cacheDef.edge_type} onChange={(e) => {
                    console.log("edge_type", e);
                    onCacheDefChange({ ...cacheDef, edge_type: e });
                }} />

                <br/>
                <Button onClick={()=>{
                    setShow_detail_setting(!show_detail_setting);
                }}>詳細設定:{show_detail_setting?"顯示":"隱藏"}</Button>
                {show_detail_setting==false?null:
                <>
                op_mode:
                <InputNumber min={0} max={1} precision={1} value={cacheDef.op_mode} onChange={(e) => {
                    console.log("op_mode", e);
                    onCacheDefChange({ ...cacheDef, op_mode: e });
                }} />



                noise_level:
                <InputNumber min={0} max={100} precision={0.05} value={cacheDef.noise_level} onChange={(e) => {
                    console.log("noise_level", e);
                    onCacheDefChange({ ...cacheDef, noise_level: e });
                }} />

                blur_sigma:
                <InputNumber min={0} max={30} precision={0.1} value={cacheDef.blur_sigma} onChange={(e) => {
                    console.log("blur_sigma", e);
                    onCacheDefChange({ ...cacheDef, blur_sigma: e });
                }} />

                blur_size:
                <InputNumber min={0} max={50} precision={2} value={cacheDef.blur_size} onChange={(e) => {
                    console.log("blur_size", e);
                    onCacheDefChange({ ...cacheDef, blur_size: e });
                }} />
        

                scale_R:
                <InputNumber min={0.1} max={10} value={cacheDef.scale_R} onChange={(e) => {
                    console.log("scale_R", e);
                    onCacheDefChange({ ...cacheDef, scale_R: e });
                }} />
                scale_ANG:
                <InputNumber min={0.1} max={10} value={cacheDef.scale_ANG} onChange={(e) => {
                    console.log("scale_ANG", e);
                    onCacheDefChange({ ...cacheDef, scale_ANG: e });
                }} />


                
                </>
                }
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
           
            let nearRange = 10/camMag;
            let isMouseCloseToPt1 = (mouseOnCanvas.x-cDef.pt1.x)**2 + (mouseOnCanvas.y-cDef.pt1.y)**2 < nearRange**2;
            let isMouseCloseToPt2 = (mouseOnCanvas.x-cDef.pt2.x)**2 + (mouseOnCanvas.y-cDef.pt2.y)**2 < nearRange**2;
            let isMouseCloseToPt3 = (mouseOnCanvas.x-cDef.pt3.x)**2 + (mouseOnCanvas.y-cDef.pt3.y)**2 < nearRange**2;
            if (ctrl_or_draw == true)//ctrl
            {
                let isMousePressed=g.mouseStatus.status==1;
                let ispreMousePressed=g.mouseStatus.pstatus==1;


                _this.mouseOnCanvas=mouseOnCanvas;

                if(isMousePressed && !ispreMousePressed)//mouse down
                {
                    if(isMouseCloseToPt1)
                    {
                        console.log("center");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.pt1.x=_this.mouseOnCanvas.x;
                            _this.UI_def.pt1.y=_this.mouseOnCanvas.y;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToPt2)
                    {
                        console.log("rotation");
                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            _this.UI_def.pt2.x=_this.mouseOnCanvas.x;
                            _this.UI_def.pt2.y=_this.mouseOnCanvas.y;
                        };
                        canvas_obj.UserRegionSelect(()=>{});
                    }
                    else if(isMouseCloseToPt3)
                    {
                        console.log("sizing");

                        _this.UI_def=clone(cacheDef);

                        _this.mouseControlCB=()=>{
                            
                            _this.UI_def.pt3.x=_this.mouseOnCanvas.x;
                            _this.UI_def.pt3.y=_this.mouseOnCanvas.y;
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


                if(isMousePressed && ispreMousePressed)//dragging
                {
                    // console.log("UI_def",_this.UI_def);
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
										//no smooth
										g.ctx.imageSmoothingEnabled = false;

                    let scale = Local_IMCM.image_info.scale;
                    g.ctx.scale(scale, scale);
                    g.ctx.translate(-0.5, -0.5);
                    g.ctx.drawImage(_this.imgCanvas, 0, 0);
                    g.ctx.restore();
                }

                // console.log("mouseOnCanvas",mouseOnCanvas,defReport);
                {
                    //draw cDef
                    // console.log("cDef",cDef);
                    //draw box 10x10
                    g.ctx.save();
                    g.ctx.strokeStyle = "red";

                    let baseSize=1/camMag;
                    {//draw arc from 3 points
                        g.ctx.strokeStyle="green";
                        g.ctx.lineWidth=baseSize*5;


                        let arc=threePointToArc(cDef.pt1,cDef.pt2,cDef.pt3);


                        g.ctx.beginPath();
                        g.ctx.arc(arc.x,arc.y,arc.r,arc.thetaS,arc.thetaE);
                        g.ctx.stroke();
                        g.ctx.closePath();



                        g.ctx.lineWidth=baseSize*2;
                        g.ctx.beginPath();
                        g.ctx.arc(arc.x,arc.y,arc.r+cDef.outerMargin,arc.thetaS,arc.thetaE);
                        g.ctx.stroke();
                        g.ctx.closePath();

                        if(arc.r-cDef.innerMargin>0)
                        {
                            g.ctx.beginPath();
                            g.ctx.arc(arc.x,arc.y,arc.r-cDef.innerMargin,arc.thetaS,arc.thetaE);
                            g.ctx.stroke();
                            g.ctx.closePath();
                        }
                        else
                        {//draw a circle at center
                            g.ctx.beginPath();
                            g.ctx.arc(arc.x,arc.y,baseSize*2,0,2*Math.PI);
                            g.ctx.stroke();
                            g.ctx.closePath();
                        }


                    }



                    {//draw sizing control point(size control point)
                        g.ctx.fillStyle = isMouseCloseToPt1?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(cDef.pt1.x, cDef.pt1.y, baseSize*5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }
                    {
                        g.ctx.fillStyle = isMouseCloseToPt2?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(cDef.pt2.x, cDef.pt2.y, baseSize*5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }

                    {
                        g.ctx.fillStyle = isMouseCloseToPt3?"blue":"red";
                        g.ctx.beginPath();
                        g.ctx.arc(cDef.pt3.x, cDef.pt3.y, baseSize*5, 0, Math.PI*2);
                        g.ctx.fill();
                        g.ctx.closePath();
                    }


                    g.ctx.restore();

                }

                ArcFitting_drawreport(def,defReport,ctrl_or_draw, g, canvas_obj);
                if(defReport?.report?.center!==undefined && defReport?.report?.radius!==null)
                {
                    let arcInfo=defReport.report;
                    
                    let camMag = canvas_obj.camera.GetCameraScale();
                    g.ctx.lineWidth=2/camMag;
                    g.ctx.strokeStyle="red";
                    g.ctx.beginPath();
                    //draw line from arcInfo.center to cDef.pt2
                    g.ctx.moveTo(arcInfo.center.x,arcInfo.center.y);
                    g.ctx.lineTo(cDef.pt2.x,cDef.pt2.y);
                    g.ctx.stroke();
            
                    g.ctx.closePath();


                    let print_loc={...cDef.pt2};
                    let extend_len=50;
                    let extend_vec={x:cDef.pt2.x-arcInfo.center.x,y:cDef.pt2.y-arcInfo.center.y};
                    //normalize extend_vec
                    extend_vec.x/=arcInfo.radius;
                    extend_vec.y/=arcInfo.radius;
                    print_loc.x+=extend_vec.x*extend_len;
                    print_loc.y+=extend_vec.y*extend_len;

                    //show radius and center value
                    g.ctx.fillStyle="red";
                    g.ctx.font="12px Arial";
                    g.ctx.fillText("R:"+arcInfo.radius.toFixed(4),print_loc.x,print_loc.y);
                    g.ctx.fillText("C:"+arcInfo.center.x.toFixed(4)+","+arcInfo.center.y.toFixed(4),print_loc.x,print_loc.y+15);
                }
                

                if(defReport!==undefined)
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





                    {//draw cursor point and show image value
                        const imageData = g.ctx.getImageData(g.mouseStatus.x, g.mouseStatus.y, 1, 1);

                        ctx.save();
                        g.ctx.font = (1.5/camMag)+"em Arial";
                        g.ctx.fillStyle = "rgba(250,100, 50,1)";
    
                        // g.ctx.fillText(rgb2hsv(pixInfo[0], pixInfo[1], pixInfo[2]).map(num => num.toFixed(1)).toString(), g.mouseStatus.x, g.mouseStatus.y)
    
                        let pixInfo = imageData.data as any;
                        g.ctx.fillText((pixInfo as number[]).map(num => num.toFixed(1)).toString(), mouseOnCanvas.x, mouseOnCanvas.y)
    
                        g.ctx.fillText(`${mouseOnCanvas.x.toFixed(1)},${mouseOnCanvas.y.toFixed(1)}`,  mouseOnCanvas.x, mouseOnCanvas.y-23/camMag)
                        g.ctx.restore();


                    }


                    if(defReport.extracted_edge_points!==undefined && defReport.extracted_edge_points.length>0)
                    {
                        let edgePts=defReport.extracted_edge_points;

                        // g.ctx.fillStyle="blue";
                        //draw blue line
                        g.ctx.strokeStyle="rgba(0,0,255,0.5)";
                        g.ctx.lineWidth=10/camMag;
                        g.ctx.beginPath();
                        g.ctx.moveTo(edgePts[0][0],edgePts[0][1]);
                        let isInExclude=false;
                        let prePoint=[...edgePts[0]];
                        for(let i=1;i<edgePts.length;i++)
                        {
                            let pt=edgePts[i];
                            let w=pt[2];
                            if(w<=0)
                            {//isInExclude points

                                if(isInExclude!=true)
                                {
                                    g.ctx.stroke();
                                    g.ctx.closePath();
                                    g.ctx.beginPath();
                                    g.ctx.strokeStyle="rgba(255,0,0,0.5)";
                                    g.ctx.moveTo(pt[0],pt[1]);
                                }
                                else
                                {

                                    g.ctx.lineTo(pt[0],pt[1]);
                                }

                                isInExclude=true;

                            }
                            else
                            {
                                if(isInExclude!=false)
                                {
                                    g.ctx.stroke();
                                    g.ctx.closePath();
                                    g.ctx.beginPath();
                                    g.ctx.strokeStyle="rgba(0,0,255,0.5)";
                                    g.ctx.moveTo(pt[0],pt[1]);
                                }
                                else
                                {
                                    g.ctx.lineTo(pt[0],pt[1]);

                                }

                                isInExclude=false;
                            }
                            // g.ctx.lineTo(pt[0],pt[1]);

                        }

                        g.ctx.stroke();
                        g.ctx.closePath();
                    }

                }
            }

        }
        } />


    </div>;
}
