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

import Typography from 'antd/lib/typography';
const { Paragraph, Title } = Typography;
import { WarningOutlined,CheckOutlined } from '@ant-design/icons';
const { CheckableTag } = Tag;
import Divider from 'antd/lib/divider';

import * as BASE_COM from 'JSSRCROOT/component/baseComponent.jsx';
let BPG_FileBrowser = BASE_COM.BPG_FileBrowser;

const { TabPane } = Tabs;

function Array_NtoM(N,M)
{
  let Len=M-N+1;
  return [ ...Array(Len).keys() ].map( i => i+N);
}


function SingleDisplayEditUI({ displayInfo, onUpdate, onCancel, BPG_Channel }) {
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

function CustomDisplayUI({ BPG_Channel, defaultFolderPath }) {
  const [displayInfo, setDisplayInfo] = useState(undefined);
  const [displayEle, setDisplayEle] = useState(undefined);
  const _mus = useSelector(state => state.UIData.machine_custom_setting);

  function refreshData()
  {
    setDisplayInfo(undefined);
    CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
      setDisplayInfo(data.prod);
    }).catch(e => {
      console.log(e);
    });
  }

  useEffect(() => {
    refreshData();
    return () => {
      console.log("1,didUpdate ret::");
    };
  }, []);

  // useEffect(() => {
  //   console.log("2,count->useEffect>>");
  // },[displayInfo]);

  let UI = [];
  if (displayInfo !== undefined) {
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
            style={{ width: '30%' }}
          >
            EDIT
          </Button>
          <Button type="dashed"
            onClick={() => {
              setDisplayInfo(undefined);
              CusDisp_DB.delete(_mus.cusdisp_db_fetch_url,info._id).then(() => {

                CusDisp_DB.read(_mus.cusdisp_db_fetch_url,".").then(data => {
                  console.log(displayInfo);
                  setDisplayInfo(data.prod);
                }).catch(e => {
                  console.log(e);
                });
              });
            }}
            style={{ width: '60%' }}
          >
            Ｘ
          </Button>
        </div>
      ))


    }
    else {


      UI.push(
        <div>
          <SingleDisplayEditUI displayInfo={displayEle} BPG_Channel={BPG_Channel}
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
  const ACT_WS_SEND= (id, tl, prop, data, uintArr, promiseCBs) => dispatch(UIAct.EV_WS_SEND(id, tl, prop, data, uintArr, promiseCBs));
  const WS_ID = useSelector(state => state.UIData.WS_ID);
  
  const _mus = useSelector(state => state.UIData.machine_custom_setting);
  useEffect(() => {
    setDisplayInfo(undefined);
    setCatSet(undefined);
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
      icat["undefined"]={
        set:dispInfo.filter(dispI=>dispI.cat===undefined)
      }
      setCatSet(icat);
    }).catch(e=>{
      console.log(e);
    });
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
          {catSet[cat].set.map(info=>
            <Button onClick={()=>onSelect(info)} size="large">
              {info.name}
            </Button>
          )}
        </TabPane>
      )}
      <TabPane tab={"__SET__"} key={"SETUP"}>
      <CustomDisplayUI
         BPG_Channel={(...args) =>
         {
          console.log(">>>");
         ACT_WS_SEND(WS_ID, ...args)
         }} />
      </TabPane>

    </Tabs>
    
  }
  return (
    UI
  );
}


export function TagDisplay_rdx({Tag_ClassName="large InspTag fixed",closable})
{
  
  const inspOptionalTag = useSelector(state =>state.UIData.edit_info.inspOptionalTag);
  const defFileTag = useSelector(state =>state.UIData.edit_info.DefFileTag);
  
  const MachTag = useSelector(state =>state.UIData.MachTag);

  const dispatch = useDispatch();
  const ACT_InspOptionalTag_Update= (newTag) => dispatch(dispatch(DefConfAct.InspOptionalTag_Update(newTag)));
  // console.log(inspOptionalTag,defFileTag,MachTag);
  return <>
    {MachTag!==undefined?<Tag className={Tag_ClassName} key="MACH">{MachTag}</Tag>:null}
    {defFileTag!==undefined?defFileTag.map(tag=><Tag className={Tag_ClassName} key={tag+"_dfTag"}>{tag}</Tag>):null}

    {inspOptionalTag.map(curTag=>
      <Tag closable={closable} className={Tag_ClassName+" optional"}  key={curTag+"_inspOptTag"} onClose={(e)=>{
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
export const TagOptions_rdx = ({className,tagGroups=tagGroupsPreset,onFulfill}) => {
  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);
  const defFileTag = useSelector(state => state.UIData.edit_info.DefFileTag);
  const MachTag = useSelector(state => state.UIData.MachTag);
  const dispatch = useDispatch();
  const ACT_InspOptionalTag_Update= (newTag) => dispatch(dispatch(DefConfAct.InspOptionalTag_Update(newTag)));
 
  
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
            className="large"
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

