import { connect } from 'react-redux'
import React , { useState,useEffect } from 'react';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import  Icon  from 'antd/lib/icon';
import  Menu  from 'antd/lib/menu';
import  Button  from 'antd/lib/button';
import  Input  from 'antd/lib/input';
import  Tag  from 'antd/lib/tag';
import Card from 'antd/lib/card';
import  Dropdown  from 'antd/lib/dropdown';
import {CusDisp_DB} from 'UTIL/DB_Query';
import  Tabs  from 'antd/lib/tabs';
import { useSelector,useDispatch } from 'react-redux';

import { DEF_EXTENSION } from 'UTIL/BPG_Protocol';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import dclone from 'clone';
import Layout from 'antd/lib/layout';
const { Header, Content, Footer, Sider } = Layout;

import { websocket_autoReconnect, websocket_reqTrack, copyToClipboard, ConsumeQueue ,defFileGeneration,GetObjElement} from 'UTIL/MISC_Util';
import Typography from 'antd/lib/typography';
const { Paragraph, Title } = Typography;
import { WarningOutlined,CheckOutlined,BulbOutlined,SaveOutlined,ReloadOutlined } from '@ant-design/icons';
const { CheckableTag } = Tag;
import Divider from 'antd/lib/divider';
import Slider from 'antd/lib/Slider';
import InputNumber from 'antd/lib/input-number';

import Switch from 'antd/lib/switch';
import * as BASE_COM from 'JSSRCROOT/component/baseComponent.jsx';
let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;

const { TabPane } = Tabs;

function Array_NtoM(N,M)
{
  let Len=M-N+1;
  return [ ...Array(Len).keys() ].map( i => i+N);
}


function SingleDisplayEditUI({ displayInfo, onUpdate, onCancel, BPG_Channel,onDelete }) {
  const [displayEditInfo, setDisplayEditInfo] = useState(undefined);

  let fileSelectFilter = (fileInfo) => fileInfo.type == "DIR" || fileInfo.name.includes("." + DEF_EXTENSION);
  const [fileBrowserInfo, setFileBrowserInfo] = useState(undefined);

  useEffect(() => {
    setDisplayEditInfo(dclone(displayInfo));
  }, [displayInfo]);
  if (displayEditInfo === undefined) return null;
  return <div>
    <Input addonBefore="Name:" className="s width4" value={displayEditInfo.name}
      onChange={e => {
        let dcEI = dclone(displayEditInfo)
        dcEI.name = e.target.value;
        setDisplayEditInfo(dcEI)
      }} />
    <div className="s width2" />
    <Input addonBefore="Cat:" className="s width6" value={displayEditInfo.cat}
      onChange={e => {
        let dcEI = dclone(displayEditInfo)
        dcEI.cat = e.target.value;
        setDisplayEditInfo(dcEI)
      }} />
    {displayEditInfo.targetDeffiles.map((dfs, id) => {

      return [
        <Input addonBefore="Tag:" value={dfs.tags} key={id + "_input"}
          onChange={e => {

            let dcEI = dclone(displayEditInfo);
            let tarDef = dcEI.targetDeffiles[id];
            tarDef.tags = e.target.value

            setDisplayEditInfo(dcEI)
          }} />,
        "Def[" + id + "]:" + dfs.name + ":" + dfs.featureSet_sha1 + "  " + dfs.path,
        <Button key={id + "_Button"}
          onClick={() => {
            let fileS =
            {
              path: "data/",
              selected: (path, info) => {
                console.log(path, info);

                let PromArr = [
                  new Promise((resolve, reject) => {
                    BPG_Channel("LD", 0,
                      { filename: path },
                      undefined, { resolve, reject }
                    );
                    setTimeout(() => reject("Timeout"), 5000)
                  }),
                  new Promise((resolve, reject) => {
                    BPG_Channel("FB", 0, {//"FB" is for file browsing
                      path: "./",
                      depth: 0,
                    }, undefined, { resolve, reject });
                    setTimeout(() => reject("Timeout"), 5000)
                  })
                ];
                Promise.all(PromArr).
                  then((pkts) => {
                    console.log(pkts);
                    if (pkts[0][0].type != "FL") return;
                    if (pkts[1][0].type != "FS") return;
                    let dataFolderPath = pkts[1][0].data.path;
                    let machInfo = pkts[0][0].data;
                    //console.log(pkts[0].data);
                    let dcEI = dclone(displayEditInfo);
                    let tarDef = dcEI.targetDeffiles[id];
                    tarDef.hash = machInfo.featureSet_sha1;
                    tarDef.featureSet_sha1 = machInfo.featureSet_sha1;
                    tarDef.featureSet_sha1_pre = machInfo.featureSet_sha1_pre;
                    tarDef.featureSet_sha1_root = machInfo.featureSet_sha1_root;


                    tarDef.path = path.replace(dataFolderPath, "").replace(/^\//, "");
                    tarDef.name = machInfo.name
                    //console.log(dcEI);
                    setDisplayEditInfo(dcEI)

                  })
                  .catch((err) => {
                    console.log(err);
                  })

              },
              filter: fileSelectFilter
            }
            setFileBrowserInfo(fileS);
          }}
          style={{ width: '30%' }}
        >
          loadDefFile
        </Button>]
    }
    )}
    {fileBrowserInfo === undefined ? null :
      <BPG_FileBrowser key="BPG_FileBrowser"
        className="width8 modal-sizing"
        searchDepth={4}
        path={fileBrowserInfo.path}
        visible={true}
        BPG_Channel={BPG_Channel}
        onFileSelected={(filePath, fileInfo) => {
          fileBrowserInfo.selected(filePath, fileInfo);
          setFileBrowserInfo(undefined);
        }}
        onCancel={() => {
          setFileBrowserInfo(undefined);
        }}
        fileFilter={fileBrowserInfo.filter} />
    }



    <Button key="onUpdate_btn"
      onClick={() => onUpdate(displayEditInfo)}
      style={{ width: '30%' }}
    >
      OK
    </Button>


    <Button key="onCancel_btn"
      onClick={onCancel}
      style={{ width: '30%' }}
    >
      Cancel
    </Button>

    
    <Divider/>
       
    <Button key="onDel_btn" danger
      onClick={() => onDelete(displayInfo)}
      style={{ width: '30%' }}
    >
      DEL
    </Button>
  </div>
}

function SingleDisplayUI({ displayInfo }) {
  const canvasRef = React.useRef(null)


  return <div>
    <Title level={2}>{displayInfo.name}</Title>
    Name:{displayInfo.name}
    <br />
    Cat:{displayInfo.cat}
    <br />
   
    {/* {displayInfo.targetDeffiles.map((dfs, id) =>
      <pre>
      {"Def[" + id + "]:" + JSON.stringify(displayInfo.targetDeffiles[id], null, 2)}
      </pre>
    )} */}
      {displayInfo.targetDeffiles.map((dfs, id) =>
        <pre>
        {"Path[" + id + "]:" + dfs.path}
        <br />
        {"tags[" + id + "]:" + dfs.tags}

        </pre>
      )}
    
  </div>
}

function CustomDisplayUI({ BPG_Channel, defaultFolderPath, onChange=_=>_ }) {
  const [safeCheckUI, setSafeCheckUI] = useState({
    content:<>
      確認進入設定介面？
      <br/>
      <Button onClick={() => {safeCheckUI.onOK();}} style={{ width: '30%' }} >
            OK
      </Button>
    </>,
    onOK:()=>{
      setSafeCheckUI(undefined);
      refreshData();
    }
  });



  const [displayInfo, setDisplayInfo] = useState(undefined);
  const [displayEle, setDisplayEle] = useState(undefined);
  const _mus = useSelector(state => state.UIData.machine_custom_setting);

  function refreshData()
  {
    setDisplayInfo(undefined);
    CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
      setDisplayInfo(data.prod);
      onChange(data);
    }).catch(e => {
      console.log(e);
    });
  }

  useEffect(() => {
    return () => {
      console.log("1,didUpdate ret::");
    };
  }, []);

  // useEffect(() => {
  //   console.log("2,count->useEffect>>");
  // },[displayInfo]);

  let UI = [];
  if (displayInfo !== undefined && safeCheckUI==undefined) {
    console.log(displayInfo);
    UI = [];


    if (displayEle === undefined) {

      UI.push(
        <Button type="dashed"
          onClick={() => {
            CusDisp_DB.create(_mus.cusdisp_db_fetch_url,{ name: "新設定", targetDeffiles: [{}] }, undefined).then(() => {
              CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
                console.log(displayInfo);
                setDisplayInfo(data.prod);

                setDisplayEle(data.prod[data.prod.length - 1]);
              }).catch(e => {
                console.log(e);
              });
            });
          }}
          style={{ width: '60%' }}
        >
          Add field
        </Button>)
      UI.push(displayInfo.map(info =>
        <div>
          <SingleDisplayUI displayInfo={info} />
          <Button
            onClick={() => {

              setDisplayEle(info);
            }}
            style={{ width: '60%' }}
          >
            EDIT
          </Button>
        </div>
      ))


    }
    else {


      UI.push(
        <div>
          <SingleDisplayEditUI displayInfo={displayEle} BPG_Channel={BPG_Channel} key="SingleDisplayEditUI"
            onUpdate={(updatedEle) => {
              console.log(updatedEle);
              CusDisp_DB.update(_mus.cusdisp_db_fetch_url,updatedEle, displayEle._id).then(() => {
                CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
                  console.log(displayInfo);
                  setDisplayInfo(data.prod);
                  setDisplayEle();
                  
                  refreshData();
                }).catch(e => {
                  console.log(e);
                });
              });
            }
            }

            onDelete={(info)=>{
                
              let checkUI={
                content:<>
                  確認刪除 {info.name}？
                  <br/>
                  <Button danger onClick={() => {checkUI.onOK();}} style={{ width: '5%' }} >
                        OK
                  </Button>
                  <Button onClick={() => {checkUI.onCancel();}} style={{ width: '50%' }} >
                        Cancel
                  </Button>
                </>,
                onOK:()=>{
                  setSafeCheckUI(undefined);
                  setDisplayInfo(undefined);
                  setDisplayEle(undefined);
                  CusDisp_DB.delete(_mus.cusdisp_db_fetch_url,info._id).then(() => {
    
                    CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
                      console.log(displayInfo);
                      setDisplayInfo(data.prod);
                    }).catch(e => {
                      console.log(e);
                    });
                  });
    
                },
                onCancel:()=>{
                  setSafeCheckUI(undefined);
                }


              }
              

              setSafeCheckUI(checkUI)

            }}

            onCancel={() => {
              setDisplayEle();
            }} />

        </div>
      )

      // UI.push(
      // <Button  
      //   onClick={() => {
      //     CusDisp_DB.update({name:"OKOK",targetDeffiles:[{hash:""}]},displayEle._id).then(()=>{

      //       CusDisp_DB.read(".").then(data=>{
      //         console.log(displayInfo);
      //         setDisplayInfo(data.prod);
      //         setDisplayEle(undefined);
      //       }).catch(e=>{
      //         console.log(e);
      //       });
      //     });
      //   }}
      //   style={{ width: '30%' }}
      // >
      //   mod
      // </Button>);


    }

  }
  else if(safeCheckUI!==undefined)
  {
    UI=<>
      {safeCheckUI.content}


    </>
  }



  return (

    <Layout style={{ height: "100%" }}>
      {/* <Layout.Header style={{ color: '#FFFFFF' }} >Header</Layout.Header> */}
      {/* <Layout.Sider  style={{ color: '#FFFFFF' }} collapsible collapsed={collapsed} 
        onCollapse={()=>setCollapsed(!collapsed)}
        onMouseOut={()=>(collapsed)?null:setCollapsed(true)}
        onMouseOver={()=>(collapsed)?setCollapsed(false):null}
        >
          Sider
        </Layout.Sider> */}
      <Layout>
        <Layout.Content style={{ padding: '50px 50px', overflow: "scroll" }}>{UI}</Layout.Content>
        <Layout.Footer>Custom UI v0.0.0</Layout.Footer>
      </Layout>

    </Layout>
  );
}

export function CustomDisplaySelectUI({onSelect}) {
  const [displayInfo, setDisplayInfo] = useState(undefined);
  const [catSet, setCatSet] = useState(undefined);
  const [displayEle, setDisplayEle] = useState(undefined);
  
  const dispatch = useDispatch();
  const ACT_WS_SEND_BPG= (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND_BPG(id, tl, prop, data, uintArr, promiseCBs));
  const CORE_ID = useSelector(state => state.ConnInfo.CORE_ID);
  
  const _mus = useSelector(state => state.UIData.machine_custom_setting);


  function catSetUpdate()
  {    
    CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data=>{
    let dispInfo = data.prod;
    setDisplayInfo(dispInfo);
    let catArr=dispInfo.map(dispI=>dispI.cat).filter(cat=>cat!==undefined);
    let uniCat = [...new Set(catArr)].sort(); 
    let icat={};
    uniCat.forEach(cat=>{
      let catInfo={
        set:dispInfo.filter(dispI=>cat.includes(dispI.cat))
      }
      icat[cat]=catInfo
    })


    let undefSet = dispInfo.filter(dispI=>dispI.cat===undefined);

    if(undefSet.length>0)
      icat["undefined"]=undefSet;
    setCatSet(icat);
  }).catch(e=>{
    console.log(e);
  });

  }


  useEffect(() => {
    setDisplayInfo(undefined);
    setCatSet(undefined);
    catSetUpdate();
    return () => {
      console.log("1,didUpdate ret::");
    };
  },[]);

  
  let UI=[];
  // if(displayInfo!==undefined)
  // {
  //   console.log(displayInfo);
  //   UI=[];
  //     UI.push(displayInfo.map(info=>
  //       <Button onClick={()=>onSelect(info)}>
  //         {info.name}
  //       </Button>
  //     ))
  // }
  
  if(catSet!==undefined)
  {
    console.log(catSet);
    UI=
    <Tabs defaultActiveKey="0">
      {Object.keys(catSet).map((cat,cat_idx)=>
        <TabPane tab={cat} key={""+cat_idx}>
          {catSet[cat].set
          .sort((A,B)=>(A.name).localeCompare(B.name))
          .map(info=>
            <Button key={info.name} onClick={()=>onSelect(info)} size="large">
              {info.name}
            </Button>
          )}
        </TabPane>
      )}
      <TabPane tab={"__SET__"} key={"SETUP"}>
      <CustomDisplayUI
         BPG_Channel={(...args) =>ACT_WS_SEND_BPG(CORE_ID, ...args)}
         onChange={()=>{
          // catSetUpdate();
         }} />

      
      </TabPane>

    </Tabs>
    
  }
  return (
    UI
  );
}


export function TagDisplay_rdx({Tag_ClassName="InspTag fixed",closable, size="large"})
{
  
  const inspOptionalTag = useSelector(state =>state.UIData.edit_info.inspOptionalTag);
  const defFileTag = useSelector(state =>state.UIData.edit_info.DefFileTag);
  
  const MachTag = useSelector(state =>state.UIData.MachTag);

  const dispatch = useDispatch();
  const ACT_InspOptionalTag_Update= (newTag) => dispatch(dispatch(DefConfAct.InspOptionalTag_Update(newTag)));
  // console.log(inspOptionalTag,defFileTag,MachTag);
  let mergeClassName=Tag_ClassName+" "+size;
  return <>
    {MachTag!==undefined?<Tag className={mergeClassName} key="MACH">{MachTag}</Tag>:null}
    {defFileTag!==undefined?defFileTag.map(tag=><Tag className={mergeClassName} key={tag+"_dfTag"}>{tag}</Tag>):null}

    {inspOptionalTag.map(curTag=>
      <Tag closable={closable} className={mergeClassName+" optional"}  key={curTag+"_inspOptTag"} onClose={(e)=>{
        e.preventDefault();
        let tagToDelete=curTag;
        let NewOptionalTag = inspOptionalTag.filter(tag=>tag!=tagToDelete);
        //console.log(e.target,NewOptionalTag);
        ACT_InspOptionalTag_Update(NewOptionalTag);
        }}>{curTag}</Tag>)}

  </>
}



export const tagGroupsPreset=[
  {
    name:"製程",
    //maxCount:1,
    minCount:1,
    tags:["01首件熱前","02首件熱後","11沖壓成形","21真空熱處理","31滾電金","31滾電錫","35清洗封孔","36連續鍍","測試"]
  },
  {
    name:"檢測方式",
    tags:["抽檢","全檢","測試"]
  }

]



export function isTagFulFillRequrement(tags,tagGroupsInfo)
{
  
  return tagGroupsInfo.reduce((isFulFill,group)=>{
    if(!isFulFill)return isFulFill;
    let matchCount=tags.reduce((count,tag)=>(group.tags.indexOf(tag)>-1)?count+1:count,0);
    if(group.maxCount!==undefined && matchCount>group.maxCount)
    {
      return false
    }
    if(group.minCount!==undefined && matchCount<group.minCount)
    {
      return false
    }
    return true
  },true)
}
export const TagOptions_rdx = ({className,tagGroups=tagGroupsPreset,onFulfill,size="large"}) => {
  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);
  const defFileTag = useSelector(state => state.UIData.edit_info.DefFileTag);
  const MachTag = useSelector(state => state.UIData.MachTag);
  const dispatch = useDispatch();
  const ACT_InspOptionalTag_Update= (newTag) => dispatch(DefConfAct.InspOptionalTag_Update(newTag));
 
  
  const [newTagStr,setNewTagStr]=useState([]);

  let warnIcon =<WarningOutlined style={{color:"#ff8b20"}}/>;
  let acceptIcon =<CheckOutlined style={{color:"#5191a5"}}/>;

  let returnUI= <div className={className}>
    {
    tagGroups.map((group,g_idx)=>{
      let matchCount=inspOptionalTag.reduce((count,tag)=>(group.tags.indexOf(tag)>-1)?count+1:count,0);
      let isFullFill=true;
      if(group.maxCount!==undefined && matchCount>group.maxCount)
      {
        isFullFill=false;
      }
      if(group.minCount!==undefined && matchCount<group.minCount)
      {
        isFullFill=false;
      }

      return[
      <Divider orientation="left" key={"divd_"+group.name+" "+g_idx}>
        {isFullFill?acceptIcon:warnIcon}{"  "}{group.name}
      </Divider>
      ,group.tags.map((tag,tag_idx)=>{
        let idxOf= inspOptionalTag.indexOf(tag);
        let is_cur_checked =idxOf > -1;
        return <Tag key={tag+"_essTag_"+tag_idx} 
            className={size}
            checked={is_cur_checked}
            color= {is_cur_checked?"#5191a5":"#AAA"}
            onClick={()=>{
              console.log(tag);
              let checked =!is_cur_checked;
              if(checked)
              {
                if(group.maxCount===undefined || matchCount+1<=group.maxCount)
                {
                  let newTags=[...inspOptionalTag,tag];
                  ACT_InspOptionalTag_Update(newTags);
                }
                else
                {
                  //Over size pass
                }
              }
              else
              {
                let newTags=[...inspOptionalTag];
                console.log(newTags);
                console.log(idxOf,tag,checked);
                newTags.splice(idxOf, 1)
                console.log(newTags); 
                ACT_InspOptionalTag_Update(newTags)
              }

            }}>{tag}</Tag>})
      ]})

    }
    <Divider orientation="left"></Divider>
    <Input placeholder="新標籤"
      onChange={(e)=>{
        let newStr=e.target.value;
        //e.target.setSelectionRange(0, newStr.length)
        setNewTagStr(newStr)
      }}
      onPressEnter={(e)=>{
        let newTag=e.target.value.split(",");
        let newTags=[...inspOptionalTag,...newTag];
        ACT_InspOptionalTag_Update(newTags);
        this.setState({...this.state,newTagStr:""});
      }}
      className={"width3 "+((inspOptionalTag.find((str)=>str==newTagStr))?"error":"")}
      allowClear
      value={newTagStr}
      // prefix={<Icon type="tags"/>}
    />
    </div>
  
  return returnUI;
}



export function UINSP_UI({UI_INSP_Count=false,UI_INSP_Count_Rate=false,UI_INSP_Count_font_size=25,UI_Speed_Slider=false,UI_detail=false})
{
  
  const dispatch = useDispatch();
  
  const DICT = useSelector(state => state.UIData.DICT);
  const uInsp_API_ID = useSelector(state => state.ConnInfo.uInsp_API_ID);
  
  const uInsp_API_ID_CONN_INFO = useSelector(state => state.ConnInfo.uInsp_API_ID_CONN_INFO);

  const ACT_WS_GET_OBJ= (callback)=>dispatch(UIAct.EV_WS_GET_OBJ(uInsp_API_ID,callback));

  
  // useEffect(()=>{
  // },[])  

  if(uInsp_API_ID_CONN_INFO===undefined )
  {
    return null;
  }

  let machineStatus = uInsp_API_ID_CONN_INFO.machineStatus;
  let machineSetup = uInsp_API_ID_CONN_INFO.machineSetup;
  if(uInsp_API_ID_CONN_INFO.type!=="WS_CONNECTED" || machineStatus===undefined || machineSetup===undefined )
  {
    return "!!全檢儀器未連線!!";
  }

  function leftFillNum(num, targetLength) {
    return num.toString().padStart(targetLength, 0);
  }

  let res_count=machineStatus.res_count||{OK:0,NG:0,NA:0};

  let error_codes=machineStatus.error_codes||[];
  let len = error_codes.length;
  if(error_codes.length>10)
  {
    error_codes=error_codes.slice(-10);
    
    error_codes=`(${len}):...${error_codes}`
  }
  else
  {
    error_codes=`(${len}):${error_codes}`
  }
  let pulse_hz=machineSetup.pulse_hz||0;

  // console.log(uInsp_API_ID_CONN_INFO);

  let OKColor="#87d068";
  let NGColor="#f50";
  let NAColor="#aaa";

  let tagStyle={
    'fontSize': UI_INSP_Count_font_size,

  }
  // text-align: center; display:block;
  let insp_count=UI_INSP_Count==false?null:
  <>
    <Tag style={tagStyle}
      color={OKColor}>{leftFillNum(res_count.OK,5)}
      </Tag>
    <Tag style={tagStyle}
      color={NGColor}>{leftFillNum(res_count.NG,5)}
    </Tag>
    <Tag style={tagStyle}
      color={NAColor}>{leftFillNum(res_count.NA,5)}
    </Tag>
    </>

  let result_count_rate_recent=uInsp_API_ID_CONN_INFO.result_count_rate_recent||{OK:0,NG:0,NA:0};
  let insp_count_rate=UI_INSP_Count_Rate==false?null:
  <>
    <Tag style={tagStyle}
      color={OKColor}>{result_count_rate_recent.OK.toFixed(1)}
      </Tag>
    <Tag style={tagStyle}
      color={NGColor}>{result_count_rate_recent.NG.toFixed(1)}
    </Tag>
    <Tag style={tagStyle}
      color={NAColor}>{result_count_rate_recent.NA.toFixed(1)}
    </Tag>
  </>



  let SpeedSlider=UI_Speed_Slider==false?null:
  <>
    <br/>
    <Slider key="speedSlider"
      min={0}
      max={20000}
      onChange={(value) => {
        ACT_WS_GET_OBJ((api)=>{
          api.machineSetupUpdate({pulse_hz:value});
        })
      }}
      value={pulse_hz}
      step={100}
    />
  </>

  let detailSetup=UI_detail==false?null:<>
    <Divider/>
    <div style={{ height: "600px" }}>
      
      <Button.Group key="GGGG">
        <Button
          icon={<BulbOutlined />}
            key="L_ON"
            onClick={() =>
              ACT_WS_GET_OBJ((api)=>{
                api.send({type: "MISC/BACK_LIGHT/ON"},
                (ret)=>{
                },(e)=>console.log(e));
              })
            }>
            ON
        </Button>

        <Button
          key="L_OFF"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{
              api.send({type: "MISC/BACK_LIGHT/OFF"},
              (ret)=>{console.log(ret)},(e)=>console.log(e));
            })
          }>OFF
        </Button>

        <Button
          icon={<SaveOutlined/>}
          key="SaveToFile"
          onClick={() => {


            ACT_WS_GET_OBJ((api)=>{
              api.saveMachineSetupIntoFile();
            })



            
          }}>{DICT._.save_machine_setting}</Button>


      </Button.Group>


      <Button.Group key="MISC_BB">

        <Button
          icon={<ReloadOutlined />}
          key="res_count_clear"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{
              api.send({type: "res_count_clear"},
              (ret)=>{console.log(ret)},(e)=>console.log(e));
            })
          }>{DICT._.RESET_INSPECTION_COUNTER}
      </Button>

      </Button.Group>




      <Divider orientation="left" key="ERROR">{DICT._.ERROR_INFO}</Divider>

      <Button.Group key="ERRORG">
        <Button
          key="error_get"
          onClick={() =>{
          }}>
          {DICT._.ERROR_CODES}:{error_codes}
        </Button>

        <Button
          key="error_clear"
          onClick={() =>
            

            ACT_WS_GET_OBJ((api)=>{
              api.send({type: "error_clear"})
            })
          }>{DICT._.ERROR_CLEAR}
        </Button>


        <Button
          key="speed_set"
          onClick={() => {
            ACT_WS_GET_OBJ((api)=>{
              api.machineSetupUpdate({pulse_hz: pulse_hz});
            })
          }
          }>{DICT._.SPEED_SET}:{pulse_hz}
      </Button>
      </Button.Group>


      <Divider orientation="left">{DICT._.uInsp_ACTION_TRIGGER_TIMING}</Divider>


      預設不噴氣{machineSetup.mode}:
      <Switch checked={(machineSetup.mode==="TEST_NO_BLOW")}
      onChange={(checked)=>
        {
          
          ACT_WS_GET_OBJ((api)=>{
            api.machineSetupUpdate({mode:checked?"TEST_NO_BLOW":"NORMAL"});
          })
        }
      } 
      />
      <br/>
  
      {


        (machineSetup.state_pulseOffset === undefined)?null:
        machineSetup.state_pulseOffset.map((pulseC, idx) =>
          <InputNumber value={pulseC} size="small" key={"poff" + idx} onChange={(value) => {
            let state_pulseOffset = dclone(machineSetup.state_pulseOffset);
            


            //[0,55,56,57,58,59] =(idx:3 set 59)=> [0,55,56,59,58,59]
            //push F => [0,55,56,59,60,61]

            //[0,55,56,57,58,59] =(idx:3 set 50)=> [0,55,56,50,58,59]
            //push B => [0,48,49,50,58,59]

            // state_pulseOffset[idx] = value;
            state_pulseOffset.forEach((v,_idx)=>{
              let tarOffset=value+(_idx-idx)*1;
              if(_idx==idx)
              {
                state_pulseOffset[_idx] = tarOffset;
              }
              else if(_idx<idx)//push back
              {
                if(state_pulseOffset[_idx]>tarOffset)
                  state_pulseOffset[_idx]=tarOffset;
              }
              else//push forward
              {
                if(state_pulseOffset[_idx]<tarOffset)
                  state_pulseOffset[_idx]=tarOffset;
              }
            });

            ACT_WS_GET_OBJ((api)=>{
              api.machineSetupUpdate({state_pulseOffset});
            })

          }} />)
      }


    <Divider orientation="left">{DICT._.TEST_MODE}</Divider>
      <Button.Group key="MODE_G">
        <Button
          key="TEST_INC"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{ api.send({type: "mode_set", mode: "TEST_INC"})})

          }>{DICT._.TEST_MODE_INC}
        </Button> 

        <Button
          key="TEST_NO_BLOW"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{ api.send({type: "mode_set", mode: "TEST_NO_BLOW"})})

          }>{DICT._.TEST_MODE_NO_BLOW}
        </Button> 

        <Button
          key="MODE:TEST"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{ api.send({type: "mode_set", mode: "TEST_ALTER_BLOW"})})

          }>{DICT._.TEST_MODE_ALTER_BLOW}
        </Button>
        <Button
          key="MODE:NORMAL"
          onClick={() =>
            ACT_WS_GET_OBJ((api)=>{ api.send({type: "mode_set", mode: "NORMAL"})})
          }>{DICT._.TEST_MODE_NORMAL}
        </Button>

        {/* <Button type="danger" key="Disconnect uInsp"
          icon={<DisconnectOutlined/>}
          onClick={() => {
            new Promise((resolve, reject) => {
              this.props.ACT_WS_SEND_CORE_BPG( "PD", 0,
                {},
                undefined, { resolve, reject });
              //setTimeout(()=>reject("Timeout"),1000)
            })
              .then((data) => {
                console.log(data);
              })
              .catch((err) => {
                console.log(err);
              })
            }}>{DICT._.TEST_MODE_DISCONNECT}</Button> */}


      </Button.Group> 
    
    </div>
  </>


  return <>
    {insp_count}
    {insp_count_rate}
    {SpeedSlider}
    {detailSetup}
  </>

  // console.log(JSON.stringify(uInsp_API_ID_CONN_INFO.machineSetup,null,2));
  // console.log(JSON.stringify(uInsp_API_ID_CONN_INFO.machineStatus,null,2));

  return <>
    <Button 
      onClick={()=>{
        ACT_WS_GET_OBJ((api)=>{
          api.machineSetupUpdate({pulse_hz:0});
        })

      }}>
        Set 0
    </Button>
    
    <Button 
      onClick={()=>{

        ACT_WS_GET_OBJ((api)=>{
          
          api.machineSetupUpdate({pulse_hz:10000});
        })
      }}>
        Set 10000
    </Button>
  </>
}
