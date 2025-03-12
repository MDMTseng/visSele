
import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext,createContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select,Popover } from 'antd';
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleFilled,PauseCircleOutlined,PauseCircleFilled,SettingOutlined,
    DisconnectOutlined, LinkOutlined,CameraOutlined,SyncOutlined,DeleteOutlined,ExclamationCircleOutlined,LoadingOutlined,StopOutlined,CloseOutlined,TagsOutlined,ToTopOutlined
} from '@ant-design/icons';

import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { Button,Input } from 'antd';
import { type_CameraInfo } from './AppTypes';
import { VEC2D } from './UTIL/MathTools';
export type CompParam_InspTar = {
    display: boolean,
    style?: any,
    fsPath: string,
    EditPermitFlag: number,
    renderHook: ((ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, rule: any) => void) | undefined,
    // IMCM_group:IMCM_group,
    systemInspTarList: any[],
    cameraList: type_CameraInfo[],
    def: any,
    report: any,
    onDefChange: (updatedDef: any, ddd: boolean) => void,
    defDoReload: () => void,



}

export type CompParam_UIOption = {

    UIOption:any|undefined,
    showUIOptionConfigUI:boolean,
    onUIOptionUpdate:((new_UIOption: any) => void),
    APIExport: ((api_set: any) => void) | undefined,

}

export type CompParam_InspTarUI =CompParam_InspTar & CompParam_UIOption;



export type CompParam_UtilUI = {
    systemInspTarList: any[],
    cameraList: type_CameraInfo[],
    defConfig:any,
    globalVariable:any,
    IT_defReload:(id:string)=>void,
    UI_API_Table:any,


    WidgetLayout:any,
    WidgetInfo:any,
    updateWidgetLayout:(newWidgetInfo :any,new_WidgetLayout:any)=>void,


}




export function PtsToXYWH(pt1: VEC2D, pt2: VEC2D) {
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



export enum EDIT_PERMIT_FLAG {
    OPONLY=0,
    XXFLAGXX=1<<0
  }

export type IMCM_type =
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
    

export function drawRegion(g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent, region: { x: number, y: number, w: number, h: number }, lineWidth: number, drawCenterPoint: boolean = true, lineDeshInfo = [lineWidth * 10, lineWidth * 3, lineWidth * 3, lineWidth * 3]) {
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



export function ObjTree( {obj,padding=0,onLeafSelect,renderer}:{obj:any,padding:number,onLeafSelect?:(value:any,name:string,path:string[])=>void,renderer?:(value:any,name:string,path:string[])=>any  }):any{

    return Object.entries(obj).map(([key, value]:any)=>{
      let renderResult=renderer===undefined?undefined:renderer(value,key,[]);
      if(typeof value === 'object')
      {

        return <>

          {
            renderResult!==undefined?    renderResult:      
                <div style={{marginLeft:padding ,display:"block"}}  onClick={()=>{
                if(onLeafSelect)onLeafSelect(value,key,[])
                }}>{key+"[-]"}</div>
          }





          <ObjTree obj={value} padding={padding+15} onLeafSelect={(value,name,path)=>{
            if(onLeafSelect)
                return onLeafSelect(value,name,[key,...path])
          }} renderer={renderer===undefined?undefined:(value,name,path)=>{
            return renderer(value,name,[key,...path])
          }}/>
        </>
      }
      else
      {
        if(renderResult!==undefined)return renderResult;
        
        return <>
        <Button size='small' style={{marginLeft:padding,display:"block"}} onClick={()=>{
          if(onLeafSelect)onLeafSelect(value,key,[])
        }}>{key}: {value}</Button>
        </>
      }
    })
  
  } 
  


export function tagsMatching(tags1: string[], tags2: string[]) {
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


export  function TagsEdit_DropDown({ tags, onTagsChange, children }: { tags: (string | string[])[], onTagsChange: (tags: (string | string[])[]) => void, children: React.ReactChild }) {
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



export function InspTarView_basicInfo({ display, fsPath, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange,defDoReload }: CompParam_InspTarUI) {

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
            <Button><TagsOutlined /></Button>
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
            }}><CloseOutlined /></Button>
        </Popconfirm>




        <Popconfirm
            title={`確定要重新載入？ 再按:${delConfirmCounter + 1}次`}
            onConfirm={() => { }}
            onCancel={() => { }}
            okButtonProps={{
                danger: true, onClick: () => {
                    if (delConfirmCounter != 0) {
                        setDelConfirmCounter(delConfirmCounter - 1);
                    }
                    else {
                        defDoReload()
                    }
                }
            }}
            okText={"Yes:" + delConfirmCounter}
            cancelText="No"
        >
            <Button danger type="primary" onClick={() => {
                setDelConfirmCounter(5);
            }}><SyncOutlined /></Button>
        </Popconfirm>

        <Button onClick={() => {
            onDefChange(cacheDef, true)
        }}><ToTopOutlined /></Button>

        {/* <Switch checkedChildren="隱藏" unCheckedChildren="顯示" checked={cacheDef.default_hide == true} onChange={(check) => {

            onDefChange({ ...cacheDef, default_hide: check }, false)
        }} /> */}
    </>

}




export function TestInputSelectUI({def, folderPath, stream_id, testTags = [] }: {def:any, folderPath: string, stream_id: number, testTags: string[] }) {
    const _this = useRef<any>({}).current;
    const dispatch = useDispatch();
    const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
    const [imageFolderInfo, setImageFolderInfo] = useState<any>(undefined);
    const [finalReports, setFinalReports] = useState<any>({});
    const [latestSelect, setLatestSelect] = useState<any>(undefined);

    const [fetchIdxList, setFetchIdxList] = useState<number[]>([]);
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


        
        let final_tags = [...tags];
        let tid=Date.now();
        let nameJson:any={mmpp:1};
        try {
            let name =fileInfo.name+"";
            name=name.replace(/\.[^/.]+$/, "")//remove extension
            console.log(name);
            nameJson ={...nameJson,...JSON.parse(name)};
            final_tags=[...final_tags,...nameJson.tags]
            tid=nameJson.tid;
        } catch (e) {
            // return console.error(e); // error in the above string (in this case, yes)!
        }
        tid%=100000;
        // let sIDTag = injectID_Prefix + fileInfo.name;
        // let final_tags=[sIDTag,...tags];

        console.log(final_tags);
        BPG_API.InjectImage(folder_path + "/" + fileInfo.name, final_tags, tid,nameJson.mmpp);

        setLatestSelect({
            ...imageFolderInfo,
            file: fileInfo,
            tags: final_tags,
            tid:tid
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




            <br/>
            <br/>

            <Button danger type="primary" onClick={async () => {

                let pkts = await BPG_API.InspTargetExchange(def.id, {
                    type: "GetFetchSrcTIDList",
                }) as any[];
                let list=pkts[0].data as number[];
                setFetchIdxList(list);
                console.log(list);

            }}>UpdateFetch</Button>

            {fetchIdxList.map((idx:number)=>
                <Button key={idx} onClick={async () => {
                    await BPG_API.InspTargetExchange(def.id, {
                        type: "TriggerFetchSrc",
                        index:idx,
                    }) 
                }}>
                    {idx}
                </Button>
            )}
        </div>

        
    </>
}





export function CountDownCheckPopup({countdown = 3, onCancel, onConfirm, children}:{countdown?:number, onCancel?:()=>void, onConfirm?:()=>void, children:React.ReactNode} ) {
    const _this = useRef<{userDecision:boolean}>({userDecision:false}).current;
    const [isPopoverVisible, setIsPopoverVisible] = useState(false);
    const [remainingClicks, setRemainingClicks] = useState(countdown);
    
    const handleVisibleChange = (visible: boolean) => {
      setIsPopoverVisible(visible);
      if (!visible) {
        setRemainingClicks(countdown);
        if(_this.userDecision===false && onCancel) onCancel();
        if(_this.userDecision===true && onConfirm) onConfirm();
      }
    };
  
    const handleConfirmClick = () => {
      const newCount = remainingClicks - 1;
      if(newCount < 0)
        return;
      if (newCount == 0) {
        setIsPopoverVisible(false);
        setRemainingClicks(newCount);
        if (onConfirm)
        {
          _this.userDecision=true;
        }
      } else{
        setRemainingClicks(newCount);
      }
    };
  
    const buttonStyle = {
      color: remainingClicks <= 2 ? '#fff' : undefined,
      backgroundColor: remainingClicks <= 2 ? '#000' : undefined,
      animation: remainingClicks <= 2 ? 'flash 0.1s infinite alternate' : undefined,
    };
  
    const textStyle = {
      color: remainingClicks <= 2 ? '#000' : undefined,
      animation: remainingClicks <= 2 ? 'textFlash 0.1s infinite alternate' : undefined,
      fontWeight: remainingClicks <= 2 ? 'bold' : undefined
    };
  
    // Add CSS keyframes for animations
    const keyframes = `
      @keyframes flash {
        from { background-color: #000; }
        to { background-color: #ff4d4f; }
      }
      @keyframes textFlash {
        from { color: #000; }
        to { color: #ff4d4f; }
      }
    `;
  
    return (
      <Popover
        content={
          <>
            <style>{keyframes}</style>
            <div>
              <p style={textStyle}>
                {remainingClicks > 0 ? 
                  `請再確認${remainingClicks}次以繼續操作` : 
                  '最後決定 可按取消'
                }
              </p>
              <div style={{ marginTop: 8, textAlign: 'right' }}>
                <Button 
                  size="small" 
                  style={{ marginRight: 8 }} 
                  onClick={() => {
                    _this.userDecision=false;
                    handleVisibleChange(false)
                  }}
                >
                  取消
                </Button>
                <Button 
                  size="small" 
                  type="primary" 
                  danger={remainingClicks <= 2}
                  style={buttonStyle}
                  onClick={handleConfirmClick}
                >
                  確認 ({remainingClicks}次)
                </Button>
              </div>
            </div>
          </>
        }
        title="確認操作"
        trigger="click"
        onVisibleChange={handleVisibleChange}
      >
        {children}
      </Popover>
    );
  }