import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext,createContext } from 'react';
import {CompParam_UIOption} from "./SingleTargetVIEWUI_UTIL"

import * as antd from 'antd';

import * as antd_icons from '@ant-design/icons';


import {InspTargetUI_MUX ,InspTargetTypes} from './InspTarView';
import { Select, Button, Popconfirm, InputNumber,Input,Drawer,Switch,Divider } from 'antd';
import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';
import {useDispatch} from 'react-redux';
import { EXT_API_ACCESS } from './redux/actions/EXT_API_ACT';
import {intersectPoint,vecXY_add} from './UTIL/MathTools';
import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import {ArcFitting_drawreport} from './SingleTargetVIEWUI_ArcFitting';
import {DirectionalCaliper_drawreport,LineFitting_drawreport} from './InspTarView';
  
import { type_CameraInfo } from './AppTypes';
import * as Babel from '@babel/standalone';
import ReactDOM from 'react-dom';

import { DraggableModal,DraggableModalProvider   } from 'ant-design-draggable-modal'
import { unescape } from 'querystring';
import { CompParam_UtilUI } from './SingleTargetVIEWUI_UTIL';


function TestInputSelectUI({folderPath, testTags = [], onFileSelect}: {folderPath: string, testTags: string[], onFileSelect: (fileInfo: any) => void }) {
  const _this = useRef<any>({}).current;
  const dispatch = useDispatch();
  const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [imageFolderInfo, setImageFolderInfo] = useState<any>(undefined);
  const [finalReports, setFinalReports] = useState<any>({});
  const [latestSelect, setLatestSelect] = useState<any>(undefined);

  const [fetchIdxList, setFetchIdxList] = useState<number[]>([]);
  _this.finalReports = finalReports;




  function FileListRefresh() {
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



      FileListRefresh();










      return (() => {

      });
  }, []);

  function ImgTest(folder_path: string, fileInfo: { name: string }, tags: string[] = []) {


      
      let final_tags = [...tags];
      let tid=Date.now();
      try {
          let name =fileInfo.name+"";
          name=name.replace(/\.[^/.]+$/, "")//remove extension
          console.log(name);
          let nameJson = JSON.parse(name);
          final_tags=[...final_tags,...nameJson.tags]
          tid=nameJson.tid;
      } catch (e) {
          // return console.error(e); // error in the above string (in this case, yes)!
      }
      tid%=100000;
      // let sIDTag = injectID_Prefix + fileInfo.name;
      // let final_tags=[sIDTag,...tags];

      console.log(final_tags);
      BPG_API.InjectImage(folder_path + "/" + fileInfo.name, final_tags, tid);

      setLatestSelect({
          ...imageFolderInfo,
          file: fileInfo,
          tags: final_tags,
          tid:tid
      });

  }

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
                  onFileSelect(file);
              }}>
              {file.name.replace(".png", "")}
          </Button>
      })


  // async function fileRename(folder_path: string, cName: string, nName: string) {
  //     let renameResult = await BPG_API.FileRename(folder_path + "/" + cName, folder_path + "/" + nName);
  //     console.log(renameResult);
  //     FileListRefresh();

  // }

  return <>

      <Button danger type="primary" onClick={() => {
          FileListRefresh();

      }}>檔案重整</Button>


      <br />
      <div style={{ width: "100%", height: "400px", background: "rgba(0,0,0,0.8)", overflow: "scroll" }}>
          {bottonS}

      </div>

      
  </>
}

function CameraTriggerButton({camera_id,trigger_tag1,trigger_tagEnd,trigger_id,repeat=1,interval=100,onClick}:{camera_id:string,trigger_tag1:string[],trigger_tagEnd:string[],trigger_id:number,repeat:number,interval:number,onClick:()=>void}){


  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
  const [countdown, setCountdown] = useState(0);
  const [isTriggering, setIsTriggering] = useState(false);

  const triggerCamera = async () => {
    onClick();
    setIsTriggering(true);
    for (let i = repeat; i > 0; i--) {
      setCountdown(i);
      await new Promise(resolve => setTimeout(resolve, interval));
      BPG_API.CameraSWTrigger(camera_id, trigger_tag1, trigger_id+Date.now()%10000, true);
    }


    await new Promise(resolve => setTimeout(resolve, interval));
    BPG_API.CameraSWTrigger(camera_id, trigger_tagEnd, trigger_id+Date.now()%10000, true);
    setCountdown(0);
    setIsTriggering(false);
  };

  return <Button onClick={triggerCamera} disabled={isTriggering}>{'拍照'+(isTriggering ? `(${repeat-countdown}/${repeat})` :"") }</Button>
}



function point_xy_avg(pt:any,p_avg:any,count_avg:number)
{


  if(pt.x===undefined || pt.y===undefined)
    return p_avg;

  return {
    x:(pt.x+p_avg.x*count_avg)/(count_avg+1),
    y:(pt.y+p_avg.y*count_avg)/(count_avg+1)
  }
}

function value_avg(value:number,avg_value:number,count_avg:number)
{
  if(value===undefined || value===null)
    return avg_value;
  return (value+avg_value*count_avg)/(count_avg+1);
}


function FileBrowsingComponent(
  {BPG_API, init_folderPath, fileFilter, selectType, onFileSelect, quickPaths,folderDepth=1}: 
  {BPG_API: BPG_WS, init_folderPath: string, fileFilter: (fileInfo: any) => boolean, selectType:"DIR"|"FILE", onFileSelect: (fileInfo: any,folderInfo:any) => void, quickPaths:string[],folderDepth:number}) {
  const [folderPath, setFolderPath] = useState(init_folderPath);
  const [folderContent, setFolderContent] = useState<any>(null);
  const [loading, setLoading] = useState(false);
  const [sideCollapsed, setSideCollapsed] = useState(false);
  const [selectedItem, setSelectedItem] = useState<any>(null);

  // Load folder contents when path changes
  useEffect(() => {
    loadFolderContents();
  }, [folderPath]);

  const loadFolderContents = async () => {
    setLoading(true);
    try {
      const content = await BPG_API.Folder_Struct(folderPath,folderDepth);
      setFolderContent(content);
    } catch (error) {
      console.error('Failed to load folder:', error);
    }
    setLoading(false);
  };

  const navigateToFolder = (newPath: string) => {
    setFolderPath(newPath);
  };

  const goToParent = () => {
    const parentPath = folderPath.split('/').slice(0, -1).join('/');
    if (parentPath) {
      navigateToFolder(parentPath);
    }
  };

  return (
    <div style={{ width: '100%', padding: '16px', display: 'flex' }}>
      {/* Quick Paths Side Panel */}
      {quickPaths.length > 0 && (
        <div style={{ 
          width: sideCollapsed ? '20px' : '200px',
          marginRight: '16px',
          borderRight: '1px solid #d9d9d9',
          transition: 'width 0.3s',
          overflow: 'hidden'
        }}>
          <Button 
            icon={sideCollapsed ? <antd_icons.RightOutlined /> : <antd_icons.LeftOutlined />}
            onClick={() => setSideCollapsed(!sideCollapsed)}
            style={{ marginBottom: '8px', width: '100%' }}
          />
          {!sideCollapsed && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {quickPaths.map((path, index) => (
                <Button
                  key={index}
                  onClick={() => navigateToFolder(path)}
                  style={{ textAlign: 'left' }}
                  icon={<antd_icons.FolderOutlined />}
                >
                  {path.split('/').pop() || path}
                </Button>
              ))}
            </div>
          )}
        </div>
      )}

      {/* Main Content */}
      <div style={{ flex: 1 }}>
        {/* Path navigation */}
        <div style={{ marginBottom: '16px' }}>
          <Input.Group compact>
            <Input 
              style={{ width: 'calc(100% - 32px)' }} 
              value={folderPath} 
              onChange={(e) => setFolderPath(e.target.value)}
              onPressEnter={loadFolderContents}
            />
            <Button 
              icon={<antd_icons.ReloadOutlined />} 
              onClick={loadFolderContents}
            />
          </Input.Group>
        </div>

        {/* Folder contents */}
        <div style={{ 
          border: '1px solid #d9d9d9', 
          borderRadius: '2px',
          height: '400px',
          overflow: 'auto',
          padding: '8px'
        }}>
          {loading ? (
            <div style={{ textAlign: 'center', padding: '20px' }}>
              <antd.Spin />
            </div>
          ) : folderContent?.files ? (
            <div>
              {/* Parent directory button */}
              <Button 
                icon={<antd_icons.ArrowUpOutlined />}
                onClick={goToParent}
                style={{ marginBottom: '8px' }}
              >
                ..
              </Button>

              {/* File/folder list */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {folderContent.files
                  .filter((item: any) => item.name !== '.' && item.name !== '..')
                  .filter(fileFilter)
                  .map((item: any) => (
                    <Button 
                      key={item.name}
                      icon={item.type === 'DIR' ? 
                        <antd_icons.FolderOutlined /> : 
                        <antd_icons.FileOutlined />
                      }
                      onClick={() => {
                        if (item.type === 'DIR') {
                          if (selectType === 'DIR' && selectedItem?.name === item.name) {
                            onFileSelect(item,folderContent);
                            // setSelectedItem(null);
                          } else {
                            if (selectType === 'DIR') {
                              setSelectedItem(item);
                            }
                            navigateToFolder(`${folderPath}/${item.name}`);
                          }
                        } else {
                          if (selectType === 'FILE' && selectedItem?.name === item.name) {
                            onFileSelect(item,folderContent);
                            // setSelectedItem(null);
                          } else {
                            setSelectedItem(item);
                          }
                        }
                      }}
                      style={{ 
                        textAlign: 'left',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        background: selectedItem?.name === item.name ? '#e6f7ff' : undefined,
                        borderColor: selectedItem?.name === item.name ? '#1890ff' : undefined
                      }}
                    >
                      <span>{item.name}</span>
                      <span style={{ fontSize: '0.8em', color: '#888' }}>
                        {new Date(item.mtime_ms).toLocaleString()}
                      </span>
                    </Button>
                  ))}
              </div>
            </div>
          ) : (
            <div>No contents found</div>
          )}
        </div>
      </div>
    </div>
  );
}




const emptyModalInfo={
  timeTag:0,
  visible:false,
  type:"",
  onOK:()=>{},
  onCancel:()=>{},
  title:"" as string|undefined,
  DATA:{},
  content:<></>,
  footer:null as any|undefined,

}

// Add a type for the dynamic component
type DynamicComponent = React.ComponentType<any>;

function App(param:{
    globalVariable:any,
    scriptPath:string,
    IT_defReload:(id:string)=>void,
    UI_API_Table:any,
    UIOption:any,
    WidgetLayout:any,
    WidgetInfo:any,
    updateWidgetLayout:(newWidgetInfo :any,new_WidgetLayout:any)=>void,
    onScriptPathChange:(newPath:string)=>void
}) {


  const { globalVariable,
    scriptPath,
    IT_defReload,
    UI_API_Table,
    UIOption,
    WidgetLayout,
    WidgetInfo,
    updateWidgetLayout,
    onScriptPathChange
  }=param;

  const _this=useRef<any>({}).current;
  const dispatch = useDispatch();
  const [BPG_API,setBPG_API]=useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);



  const [tmpScriptPath,setTmpScriptPath]=useState<string>(scriptPath);
  const [error, setError] = useState<{ message: string; stack?: string } | null>(null);
  const [logs, setLogs] = useState<string[]>([]);
  const [drawerVisible, setDrawerVisible] = useState(false);
  // const [enableSourceMap,setEnableSourceMap]=useState<boolean>(false);
  
  const [modalInfo,setModalInfo]=useState(emptyModalInfo);

  // Add a ref to track the current script execution
  const currentScriptRef = useRef<{
    cleanup?: () => void;
    isRunning: boolean;
  }>({ isRunning: false });

  // Add state for the dynamic component
  const [DynamicComponent, setDynamicComponent] = useState<DynamicComponent | null>(null);

  console.log(">>>>UIOption",UIOption);
  _this.paramTable = {
    ENV_PATH:scriptPath,
    globalVariable,
    UIOption,
    APIs:{
      UI_API_Table,
      IT_defReload,
      FileBrowsingDialog:(init_folderPath:string,quickPaths:string[],fileFilter:(fileInfo:any)=>boolean,selectType:"DIR"|"FILE",folderDepth:number=1)=>{
        // FileBrowsingDialog(BPG_API,init_folderPath,onFileSelect);
        let promise=new Promise((resolve,reject)=>{
        setModalInfo({
          ...emptyModalInfo,
          visible:true,
          content:<FileBrowsingComponent BPG_API={BPG_API} 
            init_folderPath={init_folderPath} 
            fileFilter={fileFilter} 
            selectType={selectType}
            folderDepth={folderDepth}
            onFileSelect={(fileInfo,folderInfo)=>{
              resolve({file:fileInfo,folder:folderInfo});
              // onFileSelect(fileInfo,folderInfo);
              setModalInfo(emptyModalInfo);
            }} 
            quickPaths={quickPaths} 
          />,
          onOK:()=>{
            setModalInfo(emptyModalInfo);
          },
          onCancel:()=>{
            setModalInfo(emptyModalInfo);
            resolve(null);
          }
        })});

        return promise;
      
      
      },
    },
    UI_Layout_Info:{
      WidgetLayout,
      WidgetInfo,
      updateWidgetLayout,
    },
    Components:{
      InspTargetUI_MUX,
    },
    // console: customConsole,
    HookCanvasComponent,
    BPG_API
  }


  const runJsx = (scriptPath:string,fileContent: string,enableSourceMap:boolean=false) => {
    // Cleanup previous script if running
    terminateScript();
    
    setError(null);
    setLogs([]);
    setDynamicComponent(null);
    try {
      const transpiledCode = Babel.transform(""+fileContent+"", { 
        presets: ['react', 'typescript'],
        sourceMaps: enableSourceMap ? "inline" : undefined,
        filename: scriptPath,
        parserOpts: {
          plugins: ['jsx', 'typescript']
        }
      }).code;
      if (!transpiledCode) return;

      try {
        // Mark script as running
        currentScriptRef.current.isRunning = true;

        const customConsole = {
          log: (...args: any[]) => {
            const logMessage = args.map(arg => 
              typeof arg === 'object' ? JSON.stringify(arg) : String(arg)
            ).join(' ');
            // setLogs(prev => [...prev, logMessage]);
            console.log(...args);
          },
          error: (...args: any[]) => {
            const logMessage = args.map(arg => 
              typeof arg === 'object' ? JSON.stringify(arg) : String(arg)
            ).join(' ');
            // setLogs(prev => [...prev, `ERROR: ${logMessage}`]);
            console.error(...args);
          },
          warn: (...args: any[]) => {
            const logMessage = args.map(arg => 
              typeof arg === 'object' ? JSON.stringify(arg) : String(arg)
            ).join(' ');
            // setLogs(prev => [...prev, `WARN: ${logMessage}`]);
            console.warn(...args);
          }
        };

        let paramTable = {
          _SCRIPT_PATH_:scriptPath,
          _SCRIPT_FOLDER_:scriptPath.split("/").slice(0, -1).join("/"),
          React,
          ReactDOM,
          document,
          antd,
          antd_icons,
          DraggableModal,DraggableModalProvider,
          _SYS_: _this.paramTable,
          register_cleanup: (cleanup: () => void) => {
            currentScriptRef.current.cleanup = cleanup;
          },
          // Add component registration
          register_component: (component: DynamicComponent) => {
            setDynamicComponent(() => component);
          }
        };
        console.log("paramTable",paramTable);
        // Wrap the code in a module-like scope
        // const wrappedCode = `
        //   (function() {
        //     ${transpiledCode}
        //   })();
        // `;

        const renderFn = new Function(
          ...Object.keys(paramTable),
          transpiledCode
        );
        
        renderFn(...Object.values(paramTable));
      } catch (runtimeError: unknown) {
        if (runtimeError instanceof Error) {
          console.log(runtimeError);
          setError({ message: runtimeError.message, stack: runtimeError.stack });
        } else {
          setError({ message: String(runtimeError) });
        }
      }
    } catch (transpileError: unknown) {
      if (transpileError instanceof Error) {
        setError({ message: transpileError.message, stack: transpileError.stack });
      } else {
        setError({ message: String(transpileError) });
      }
    }
  };

  const terminateScript = () => {
    if (currentScriptRef.current.isRunning) {
      try {
        if (currentScriptRef.current.cleanup) {
          currentScriptRef.current.cleanup();
        }
        setDynamicComponent(null);
        currentScriptRef.current = { isRunning: false };
      } catch (error) {
        console.error('Error during script termination:', error);
      }
    }
  };



  const runScript = (enableSourceMap:boolean=false) => {

    console.log(tmpScriptPath);
    (async ()=>{
      if(tmpScriptPath)
      {
        let fileContent = await BPG_API.FILE_Load(tmpScriptPath);
        // setJsxCode(fileContent);
        runJsx(tmpScriptPath,fileContent,enableSourceMap);
        // console.log(fileContent);
      }
    })();

    if(tmpScriptPath!==scriptPath)
    {
      onScriptPathChange(tmpScriptPath);
    }

  }

  useEffect(()=>{
    runScript(false);
    return () => {
      terminateScript();
    };
  },[]);

  let Modal_UI=<DraggableModalProvider>
  <DraggableModal
      title={modalInfo.title}
      visible={modalInfo.visible}
      onOk={modalInfo.onOK}
      // confirmLoading={confirmLoading}
      onCancel={modalInfo.onCancel}
      footer={modalInfo.footer}
    >
      {modalInfo.content }
  </DraggableModal>
  </DraggableModalProvider>

  return (
    <>

      {Modal_UI}
      <Button 
        type="primary"
        icon={drawerVisible ? <antd_icons.MenuFoldOutlined /> : <antd_icons.MenuUnfoldOutlined />}
        onClick={() => setDrawerVisible(!drawerVisible)}
        style={{
          position: 'fixed',
          right: 0,
          bottom: 0,
          zIndex: 1000,
        }}
      />

      {/* Control Panel Drawer */}
      <Drawer
        title="JSX Runner Controls"
        placement="right"
        onClose={() => setDrawerVisible(false)}
        visible={drawerVisible}
        open={drawerVisible}
        width={"100%"}
        getContainer={false}
        style={{ position: 'absolute' }}
      >

        {/* <Switch checked={enableSourceMap} checkedChildren="Source Map" unCheckedChildren="No Source Map" onChange={(checked)=>setEnableSourceMap(checked)} /> */}
        <Input value={tmpScriptPath} onChange={(e)=>{
          console.log(e.target.value);
          setTmpScriptPath(e.target.value);
        }} />
        <Button 
          onClick={terminateScript} 
          icon={<antd_icons.StopOutlined />} 
          style={{width:"200px"}} 
          type="primary" 
          danger 
          block
        >
          Terminate
        </Button>
        <Button 
          onClick={()=>runScript(false)} 
          icon={<antd_icons.UndoOutlined />} 
          style={{width:"100px", marginTop: '10px'}} 
          type="primary" 
          block
        >
          Run
        </Button>
        <br/>
        <Button 
          onClick={()=>runScript(true)} 
          icon={<antd_icons.UndoOutlined />} 
          size="small"
          style={{width:"30px", marginTop: '10px'}} 
          type="dashed" 
          block
        >
          Run with Source Map
        </Button>
        <div style={{ 
          marginTop: '10px', 
          padding: '10px',
          background: '#f5f5f5',
          border: '1px solid #ddd',
          borderRadius: '4px',
          fontFamily: 'monospace',
          maxHeight: '200px',
          overflow: 'auto'
        }}>
          {logs.map((log, index) => (
            <div key={index}>{log}</div>
          ))}
        </div>

        {error && (
          <div style={{ color: 'red', marginTop: '10px' }}>
            <strong>Error:</strong> {error.message}
            {error.stack && (
              <pre style={{ whiteSpace: 'pre-wrap', background: '#f8d7da', padding: '10px' }}>
                {error.stack}
              </pre>
            )}
          </div>
        )}
      </Drawer>

      {/* Full-screen output */}
      <div 
        id="output" 
        style={{ 
          width: '100%', 
          height: '100%',
          position: 'relative',
          top: 0,
          left: 0,
          border: '1px solid #ccc'
        }} 
      >
        {DynamicComponent ? <DynamicComponent {...param} /> : (
          <div style={{ 
            display: 'flex', 
            justifyContent: 'center', 
            alignItems: 'center', 
            height: '100%',
            color: '#999',
            fontSize: '16px'
          }}>
            No component to display
          </div>
        )}
      </div>
    </>
  );
}

function UtilUI_JsAPP_SetupUI({globalVariable, UIOption,defConfig, showUIOptionConfigUI, onUIOptionUpdate, systemInspTarList }: CompParam_UtilUI & CompParam_UIOption) 
{
  const [scriptPath,setscriptPath]=useState<string>(UIOption?.scriptPath);
  return <>
    <Input value={scriptPath} onChange={(e)=>setscriptPath(e.target.value)} />


    <Button onClick={()=>onUIOptionUpdate({...UIOption,scriptPath})}>確定</Button>
    <Divider />
  </>;
}


export function UtilUI_JsAPP(param: CompParam_UtilUI & CompParam_UIOption) {
  const { UIOption,defConfig, showUIOptionConfigUI, onUIOptionUpdate, systemInspTarList,globalVariable,IT_defReload,UI_API_Table,APIExport ,WidgetLayout,WidgetInfo,updateWidgetLayout,}=param;
//   const [UISetupMode, setUISetupMode] = useState(false);
  console.log(UIOption,defConfig);
  

  let scriptPath = UIOption.scriptPath;
  // {
  //   // Check if scriptPath exists and handle absolute paths for both Windows and Linux
  //   if (scriptPath) {
  //     const isWindowsAbsolute = /^[A-Za-z]:\\/.test(scriptPath);
  //     const isUnixAbsolute = scriptPath.startsWith('/');
      
  //     if (!isWindowsAbsolute && !isUnixAbsolute) {
  //       scriptPath = `${defConfig.path}/${scriptPath}`;
  //     }
  //   }
  // }



  return <div style={{ width: '100%', height: '100%', position: 'relative', overflow: 'hidden' }}>
    {/* ID:{UIOption.id} */}
    {/* <Button onClick={() => setUISetupMode(!UISetupMode)}>{UISetupMode ? "取消" : "設定"}</Button>
    
    {UISetupMode && <>
      <UtilUI_JsAPP_SetupUI {...param} />
    </>} */}
    
    <App scriptPath={scriptPath} UIOption={UIOption} globalVariable={globalVariable} IT_defReload={IT_defReload} UI_API_Table={UI_API_Table} 
    WidgetLayout={WidgetLayout}
    WidgetInfo={WidgetInfo}
    updateWidgetLayout={updateWidgetLayout}
    onScriptPathChange={(newPath:string)=>
    {
      console.log(newPath);
        onUIOptionUpdate({...UIOption,scriptPath:newPath});
    }
    }
    />
    
  </div>;
}
