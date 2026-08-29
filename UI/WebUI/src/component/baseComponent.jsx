
import React, { useState,useEffect,useRef }from 'react';
import React_createClass from 'create-react-class';
import {GetObjElement} from 'UTIL/MISC_Util';

import dateFormat from "dateformat";
import dclone from 'clone';
import { mkLog } from 'UTIL/logger';

import  {default as AntButton}  from 'antd/lib/button';
import  Switch  from 'antd/lib/switch';
import  Modal  from 'antd/lib/modal';
import  Divider  from 'antd/lib/divider';

import  Table  from 'antd/lib/table';
import { 
  FolderOpenOutlined,
  InfoCircleOutlined,
  EditOutlined,
  CloudDownloadOutlined,
  LeftOutlined,
  RightOutlined,
  LinkOutlined,
  DisconnectOutlined,
  FileOutlined,
  FolderOutlined } from '@ant-design/icons';

import Menu from 'antd/lib/menu';
import Input from 'antd/lib/input'
import Space from 'antd/lib/space'


import { parse } from 'semver';
const AntButtonGroup = AntButton.Group;


// import { button as AntButton } from 'antd/lib/button';
// import {Button as AntButton} from 'antd/lib/button';

const log = mkLog('ui.base');

export function InputNumber({key,className,step=0.1,defaultValue,value,onChange})
{
  const [isNumber,setIsNumber]=useState(true);
  const [text,setText]=useState(defaultValue);

  
  useEffect(()=>{
    setText(value);
    setIsNumber(parseFloat(value)==value);
  },[value]);



  return <input key={key} className={className+((isNumber)?"":" error ")} 
    step={step} 
    type="number" 
    pattern="^[-+]?[0-9]*\.?[0-9]*$" 
    defaultValue={defaultValue}
    value={value}

    onChange={(evt)=>{
      //setIsNumber(parseFloat(val)==val);
      onChange(evt)
      // let val = evt.target.value;
      // //console.log(val);
      // if(val=="-")
      // {
      //   setText(val);
      // }
      // else
      // {
      //   setText(text);
      //   onChange(evt)
      // }
      // 
      // setText(val);
      // if(parseFloat(val)==val)
      // {
      //   onChange(evt);
      //   setIsNumber(true);
        
      // }
      // else
      // {
      //   setText(val);
      //   setIsNumber(false);
      // }
    }}/>

}


// Compact property-sheet — rewrite of the touch-era JsonElement leaf
// renderer. Same call surface (`type`, `target`, `onChange`, `children`,
// `dict`/`dictTheme`, `renderLib`) so JsonEditBlock can dispatch unchanged.
// Mouse/keyboard styling: 22px control height, plain HTML inputs instead
// of the NumPad popup, antd Switch size="small".
//
// Numbers: display rounded to 4 decimals via `_toFixed4`; commit-on-blur
// or Enter parses + rounds before propagation. Mid-typing keystrokes are
// held in local state so external re-renders from the same edit don't
// stomp the user's input.

function _toFixed4(v) {
  if (v === undefined || v === null || v === '') return '';
  const n = (typeof v === 'string') ? parseFloat(v) : v;
  if (!Number.isFinite(n)) return '';
  return '' + parseFloat(n.toFixed(4));
}

const _PS_INPUT = {
  width: '100%', height: 22, fontSize: 12, padding: '0 4px',
  border: '1px solid #ccc', borderRadius: 3, background: 'white',
  color: '#222', // override inherited white from ancestor container
  boxSizing: 'border-box',
};

function _PSNumberInput({ value, onCommit }) {
  const [local, setLocal] = useState(() => _toFixed4(value));
  const editingRef = useRef(false);
  useEffect(() => { if (!editingRef.current) setLocal(_toFixed4(value)); }, [value]);

  const commit = () => {
    editingRef.current = false;
    const n = parseFloat(local);
    if (Number.isFinite(n)) {
      const rounded = parseFloat(n.toFixed(4));
      onCommit(rounded);
      setLocal('' + rounded);
    } else {
      setLocal(_toFixed4(value));
    }
  };

  return (
    <input
      type="number" step="0.0001"
      style={_PS_INPUT} value={local}
      onChange={(e) => { editingRef.current = true; setLocal(e.target.value); }}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        else if (e.key === 'Escape') {
          editingRef.current = false;
          setLocal(_toFixed4(value));
          e.currentTarget.blur();
        }
      }}
    />
  );
}

function _PSTextInput({ value, onCommit }) {
  const [local, setLocal] = useState(() => value ?? '');
  const editingRef = useRef(false);
  useEffect(() => { if (!editingRef.current) setLocal(value ?? ''); }, [value]);
  return (
    <input
      type="text" style={_PS_INPUT} value={local}
      onChange={(e) => { editingRef.current = true; setLocal(e.target.value); }}
      onBlur={() => { editingRef.current = false; onCommit(local); }}
      onKeyDown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        else if (e.key === 'Escape') {
          editingRef.current = false; setLocal(value ?? ''); e.currentTarget.blur();
        }
      }}
    />
  );
}

export function JsonElement(props) {
  const { type, target, onChange, children, dict, dictTheme, renderLib } = props;

  // i18n the raw value (only meaningful for `div` text labels).
  let translated = children;
  if (type === 'div') {
    const a = GetObjElement(dict, [dictTheme, children]);
    const b = (a === undefined) ? GetObjElement(dict, ['_', children]) : a;
    if (b !== undefined) translated = b;
  }

  switch (type) {
    case 'input-number':
      return <_PSNumberInput
        value={children}
        onCommit={(v) => onChange(target, 'input-number', { target: { value: v } })}
      />;

    case 'input':
      return <_PSTextInput
        value={children ?? ''}
        onCommit={(v) => onChange(target, 'input', { target: { value: v } })}
      />;

    case 'checkbox':
      return <input type="checkbox" checked={!!children}
        onChange={(evt) => onChange(target, 'checkbox', evt)} />;

    case 'btn':
      return <AntButton size="small" style={{ fontSize: 12, height: 22, padding: '0 8px' }}
        onClick={(evt) => onChange(target, 'btn', evt)}>
        {translated}
      </AntButton>;

    case 'switch': {
      const checked = (typeof children === 'boolean') ? children : (children < 0);
      return <Switch size="small" checked={checked}
        onChange={(c) => onChange(target, 'switch', { target: { checked: c } })} />;
    }

    case 'div':
    default:
      if (renderLib && typeof renderLib[type] === 'function') return renderLib[type](props);
      return <span style={{ fontSize: 12, color: '#444' }}>{translated}</span>;
  }
}

// Compact property sheet. Same prop API as the legacy class:
//   object, dict, dictTheme, whiteListKey, renderLib, additionalData,
//   jsonChange(rootObj, target, type, evt).
// Internals rewritten as a function component:
//   - Mouse/keyboard layout (24px row, 12px font, label column ~96px).
//   - Number leaves round to 4 decimals on commit (NumPad dropped).
//   - Editor maintains a deep clone of `object` keyed by input identity so
//     jsonChange handlers can mutate sub-paths and dispatch the mutated
//     root via the rootObj arg (parent's SetShape stores the clone).
//     useRef-based memo: unrelated re-renders don't re-clone.
//   - renderLib widgets keep the legacy contract (render bare without an
//     outer label row — they show their own labels inside the popover/
//     dropdown trigger). Scalar fields get a label + control row. Nested
//     objects get an indented sub-block with a header.

const _PS_ROW = {
  display: 'flex', alignItems: 'center', minHeight: 24, padding: '1px 4px',
  gap: 6, borderBottom: '1px solid rgba(0,0,0,0.05)', fontSize: 12,
};
const _PS_LABEL = {
  flex: '0 0 96px', color: '#333', overflow: 'hidden',
  textOverflow: 'ellipsis', whiteSpace: 'nowrap',
};
const _PS_VALUE = { flex: 1, minWidth: 0, display: 'flex', alignItems: 'center', gap: 4 };
const _PS_NESTED_HEADER = {
  fontSize: 11, color: '#666', letterSpacing: '0.5px', fontWeight: 500,
  padding: '4px 4px 2px', borderBottom: '1px solid rgba(0,0,0,0.08)',
};
const _PS_NESTED_BODY = {
  paddingLeft: 8, borderLeft: '2px solid rgba(0,0,0,0.05)',
  margin: '0 0 4px 4px',
};
const _PS_CUSTOM_ROW = {
  padding: '2px 4px', borderBottom: '1px solid rgba(0,0,0,0.05)',
};

function _psTranslate(dict, dictTheme, key) {
  const a = GetObjElement(dict, [dictTheme, key]);
  if (a !== undefined) return a;
  const b = GetObjElement(dict, ['_', key]);
  return (b !== undefined) ? b : key;
}

function _resolveRenderComp(renderContext, renderLib) {
  if (renderContext && typeof renderContext.__OBJ__ === 'function') return renderContext.__OBJ__;
  if (!renderLib) return undefined;
  if (typeof renderLib[renderContext] === 'function') return renderLib[renderContext];
  if (renderContext && typeof renderLib[renderContext.__OBJ__] === 'function') return renderLib[renderContext.__OBJ__];
  return undefined;
}

function _composeRows({ obj, whiteListKey, idHeader, keyTrace, onChange, props }) {
  const rows = [];
  const keyList = (whiteListKey == null) ? obj : whiteListKey;
  for (const key in keyList) {
    const ele = obj[key];
    const renderContext = (whiteListKey == null) ? null : whiteListKey[key];
    if (ele === undefined || renderContext === undefined) continue;

    const newKeyTrace = keyTrace.concat(key);
    const label = _psTranslate(props.dict, props.dictTheme, key);
    const RenderComp = _resolveRenderComp(renderContext, props.renderLib);

    if (RenderComp) {
      // Custom widget — owns its own label/trigger layout.
      rows.push(
        <div key={idHeader + '_' + key + '_rc'} style={_PS_CUSTOM_ROW}>
          <RenderComp
            onChange={onChange}
            target={{ obj, keyTrace: newKeyTrace }}
            obj={obj}
            keyTrace={newKeyTrace}
            renderContext={renderContext}
            props={props}
          />
        </div>
      );
      continue;
    }

    if (typeof ele === 'object' && ele !== null) {
      // Header dispatch: renderContext.__OBJ__ controls how the nested
      // header looks. 'div' (or missing) → plain label. 'btn' → clickable
      // AntButton (used by ref/ref_baseLine slots — DefConfUI's jsonChange
      // routes btn-click on a keyTrace starting with 'ref' to the ref-pick
      // action ACT_EDIT_TAR_ELE_TRACE_UPDATE).
      const headerType = (renderContext && typeof renderContext.__OBJ__ === 'string')
        ? renderContext.__OBJ__ : 'div';
      const headerStyle = (headerType === 'div') ? _PS_NESTED_HEADER : { padding: '2px 4px' };
      rows.push(
        <div key={idHeader + '_' + key + '_nested'}>
          <div style={headerStyle}>
            <JsonElement
              type={headerType}
              target={{ obj, keyTrace: newKeyTrace }}
              dict={props.dict}
              dictTheme={props.dictTheme}
              renderLib={props.renderLib}
              onChange={onChange}
            >{label}</JsonElement>
          </div>
          <div style={_PS_NESTED_BODY}>
            {_composeRows({
              obj: ele, whiteListKey: renderContext,
              idHeader: idHeader + '_' + key, keyTrace: newKeyTrace,
              onChange, props,
            })}
          </div>
        </div>
      );
      continue;
    }

    const leafType = (renderContext == null) ? 'div' : renderContext;
    rows.push(
      <div key={idHeader + '_' + key + '_row'} style={_PS_ROW}>
        <div style={_PS_LABEL} title={label}>{label}</div>
        <div style={_PS_VALUE}>
          <JsonElement
            type={leafType}
            target={{ obj, keyTrace: newKeyTrace }}
            dict={props.dict}
            dictTheme={props.dictTheme}
            renderLib={props.renderLib}
            onChange={onChange}
          >
            {ele}
          </JsonElement>
        </div>
      </div>
    );
  }
  return rows;
}

export function JsonEditBlock(props) {
  // Maintain a fresh deep clone of `object` keyed by input identity. The
  // legacy contract: jsonChange handlers mutate sub-paths of this clone
  // and the parent receives the mutated root for SetShape persistence.
  const cloneRef = useRef(null);
  const lastInput = useRef(null);
  if (props.object !== lastInput.current) {
    cloneRef.current = dclone(props.object);
    lastInput.current = props.object;
  }
  const root = cloneRef.current;

  const handleChange = (target, type, evt) => {
    props.jsonChange(root, target, type, evt);
  };

  const rows = _composeRows({
    obj: root,
    whiteListKey: props.whiteListKey,
    idHeader: '',
    keyTrace: [],
    onChange: handleChange,
    props,
  });

  return <div style={{ background: 'rgba(255,255,255,0.92)' }}>{rows}</div>;
}



export let CardFrameWarp = React_createClass({


  getDefaultProps: function() {
    return {
      boxShadow:"1px 2px 10px #000",
      addClass: "",
    };

  },
  render: function() {

    let HX_Type=(this.props.fixedFrame)?"HXF":"HXA";
    let topHX_Type = this.props.addClass + ((this.props.fixedFrame)?"":" HXA");
    return(
      <div className={"padding "+ topHX_Type}>
        <div
          className={HX_Type+" white padding "}
          style={{boxShadow:this.props.boxShadow}} >
            {this.props.children}
        </div>
      </div>
    );
  }
});

export let DropDownWarp = React_createClass({

  render: function() {
    var dropDownClassName="HXA dropDownContent "+(this.props.ifShowDropDown?"":"hide ")+this.props.dropdownClass;
    return(
      <div className={"dropDown "+ this.props.containerClass} >
        {this.props.children[0]}
        <div
          className={dropDownClassName}
          style={this.props.dropdownStyle}>
          {this.props.children.slice(1,this.props.children.length)}

        </div>
      </div>
    );
  }
});




export function BPG_FileBrowser_varify_info(fileInfo)
{

  if (!(typeof fileInfo.name === 'string' || fileInfo.name instanceof String))
  {
    return false;
  }
  if (!(typeof fileInfo.path === 'string' || fileInfo.path instanceof String))
  {
    return false;
  }
  if (!(typeof fileInfo.type === 'string' || fileInfo.type instanceof String))
  {
    return false;
  }

  if(typeof fileInfo.ctime_ms !== 'number'){
    return false;
  }
  if(typeof fileInfo.mtime_ms !== 'number'){
    return false;
  }
  if(typeof fileInfo.size_bytes !== 'number'){
    return false;
  }
  return true;
}



export class BPG_FileBrowser extends React.Component{
  render()
  {
    return <BPG_FileBrowser_proto {...this.props} footer={null}/>
  }
}

export class BPG_FileSavingBrowser extends React.Component{
  
  constructor(props) {
    super(props);
    
    this.state={
      fileName:(props.defaultName===undefined)?"":props.defaultName,
      folderInfo:undefined
    };
  }
  isASCII(str, extended=false) {
    return (extended ? /^[\x00-\xFF]*$/ : /^[\x00-\x7F]*$/).test(str);
  }
  render()
  {
    let isTarFileExist = this.state.folderInfo!=undefined && this.state.folderInfo.files!==undefined&&
     ((this.state.folderInfo.files.find((file)=>file.name==this.state.fileName))!==undefined);
    
    return <BPG_FileBrowser_proto {...this.props}
        onFileSelected={(file)=>{
          let fileName= file.substr(file.lastIndexOf('/') + 1);
          this.setState({...this.state,fileName});
        }}

    
    footer={
      <div>
        <Input className="width9" placeholder="File Name" 
        value={this.state.fileName}  style={{float:"left"}}
        onChange={(ev)=>{
          let fileName = ev.target.value;
          if(this.isASCII(fileName))
          {
            this.setState({...this.state,fileName})
          }
          }}/>
        
        <AntButtonGroup className="width2"  style={{float:"left"}}>
          <AntButton onClick={this.props.onCancel}>Cancel</AntButton>
          <AntButton onClick={()=>this.props.onOk(this.state.folderInfo,this.state.fileName,isTarFileExist)} 
            type={isTarFileExist?"danger":"primary"}
            disabled={(this.state.fileName.length==0 || this.state.folderInfo===undefined)}>OK</AntButton>
        </AntButtonGroup>
      </div>
    }
    onFolderLoaded={(folderStruct)=>{
        
        this.setState({...this.state,folderInfo:folderStruct})
      }
    }/>
  }
}

export class BPG_FolderBrowser extends React.Component{
  render()
  {
    return <BPG_FileBrowser_proto {...this.props} footer={null} />
  }
}

export class BPG_FileBrowser_proto extends React.Component{
  
  constructor(props) {
    super(props);
    this.state={
      folderStruct:{},
      history:["./"],
      searchText:undefined,
      searchFolderStruct:undefined,
      selectedFileGroupInfo:undefined
    }
  }


  
  fetchDirFiles(path,depth=1)
  {
    return  new Promise((resolve, reject) => {
      this.props.BPG_Channel("FB",0,{//"FB" is for file browsing
        path:path,
        depth:depth,
      },undefined,{resolve,reject});
      setTimeout(()=>reject("Timeout"),5000)
    })
  }



  goDir(path)
  {

    if(path===undefined)
    {
      if(this.state.history.length<=1)
      {
        return;
      }
      this.state.history.pop();
      
      path = this.state.history[this.state.history.length-1];
    }
    else
    {
      if(path == this.state.history[this.state.history.length-1])
      {
        log.debug("[browser]");
        return;
      }
      this.state.history.push(path);
    }

    this.fetchDirFiles(path)
    .then((data) => {
      log.debug("[fetchDirFiles] ok", data)
      let folderStruct={}
      if(data[1].data.ACK)
      {
        folderStruct = data[0].data;
      }
      this.setState({...this.state,folderStruct:folderStruct,searchText:undefined,searchFolderStruct:undefined});
      if(this.props.onFolderLoaded!==undefined)
      {
        this.props.onFolderLoaded(folderStruct);
      }
    })
    .catch((err) => {
      log.warn("[fetchDirFiles] exception", err)
      this.setState({...this.state,folderStruct:[]});
      if(this.props.onFolderLoaded!==undefined)
      {
        this.props.onFolderLoaded({});
      }
    })
    
    // this.fetchDirFiles(path,1000)
    // .then((data) => {
      
    //   console.log(data);
    // })
    // .catch((err) => {
      
    //   console.log(err);
    // })
  }


  componentWillMount() {
    this.goDir(this.props.path);
  }


  componentDidUpdate(prevProps) {
    
    if(this.props.visible==true && prevProps.visible==false)
    {
      this.goDir(this.props.path);
    }
  }

  bytesToSize(bytes) {
    var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    if (bytes == 0) return '0 Byte';
    var i = parseInt(Math.floor(Math.log(bytes) / Math.log(1024)));
    return Math.round(bytes / Math.pow(1024, i), 2) + ' ' + sizes[i];
  }
  fList(folderStruct,filter)
  {
    //console.log(folderStruct);
    if(folderStruct===undefined || folderStruct.files===undefined)return [];
    let cfileList = folderStruct.files;
    if(filter!==undefined)
    {
      cfileList = cfileList.filter(filter);
    }
    cfileList.forEach((file)=>{
      file.key =
      file.path = folderStruct.path+"/"+file.name;
      file.size = this.bytesToSize(file.size_bytes);
    });

    cfileList=folderStruct.files.reduce((list,file)=>{
      if(file.type!="DIR")return list;
      return list.concat(this.fList(file.struct,filter));
    },cfileList);
    return cfileList; 
  }
  render()
  {
    let titleRender=[]
    let fv_UI=[]
    let pathSplitBtns;
    let columns =[]
    let fileList;
    let tableWidthClass = "width10"

    let Col_file_Type={
      title: "type",
      dataIndex: "type",
      key:"type",
      render:(text, record) => 
        (text=="DIR")?<FolderOutlined />:<FileOutlined />,
      width:64
    }

    let Col_file_Name={
      title: 'name',
      dataIndex: 'name',
      key:'name',
      sorter: (a, b) => (a.name).localeCompare(b.name),
    }


    let Col_file_Path={
      title: 'path',
      dataIndex: 'path',
      key:'path',
    }


    let Col_file_modified_time_ms={
      title: 'mtime_ms',
      dataIndex: 'mtime_ms',
      key:'mtime_ms',
      render:(millisecond, record) => 
        dateFormat(new Date(millisecond), "yyyy/mm/dd hh:mm:ss"),
      sorter:(a, b) => a.mtime_ms>b.mtime_ms
    }


    let Col_file_Size={
      title: 'size',
      dataIndex: 'size',
      key:'size',
      sorter:(a, b) => a.size_bytes>b.size_bytes
    }

    



    if(this.state.searchText!==undefined && this.state.searchText.length>0)
    {
      columns = [
        Col_file_Type,
        Col_file_Name,
        Col_file_modified_time_ms,
        Col_file_Size,
        Col_file_Path];


      if(this.state.selectedFileGroupInfo==undefined)
      {
        fileList=this.fList(this.state.searchFolderStruct,
          (file)=>(file.name!='.' && file.name!='..' )&&
          file.name.includes(this.state.searchText)&&
          (this.props.fileFilter===undefined?true:this.props.fileFilter(file)));
      }
      else
      {
        fileList=this.state.selectedFileGroupInfo.filter(
          (file)=>(file.name!='.' && file.name!='..' )&&
          file.name.includes(this.state.searchText)&&
          (this.props.fileFilter===undefined?true:this.props.fileFilter(file)));
      }
      log.debug("[file-list]", { sel: this.state.selectedFileGroupInfo, fileList });

      tableWidthClass="width12"
    }
    else if(this.state.selectedFileGroupInfo!==undefined)
    {
      columns = [
        Col_file_Type,
        Col_file_Name,
        Col_file_modified_time_ms,
        Col_file_Size,
        Col_file_Path,];
        
      fileList=this.state.selectedFileGroupInfo;
      tableWidthClass="width10"
    }
    else
    {
      
      columns = [
        Col_file_Type,
        Col_file_Name,
        Col_file_modified_time_ms,
        Col_file_Size,];


      fileList =
      this.fList(this.state.folderStruct,(file)=>
        (file.name!='.' && file.name!='..' )&&
        (this.props.fileFilter===undefined?true:this.props.fileFilter(file)
      ))
      
  
  
      let curPathArr = (typeof this.state.folderStruct.path ==='string')?
        this.state.folderStruct.path.replace(/\\+/g, "/").split("/"):[];
  
      curPathArr=curPathArr.map((name)=>({name}));
  
      curPathArr.reduce((pathInt,pathObj)=>{
          pathInt=pathInt+pathObj.name+"/";
          pathObj.path=pathInt;
          return pathInt;
        },"");
        
  

      pathSplitBtns=<>
        <AntButton type="primary" icon={<LeftOutlined />}  style={{float:"left"}} onClick={()=>this.goDir()}/>
        &nbsp;&nbsp;&nbsp;
        
        {curPathArr.map((folder,idx)=>
          <>
            <AntButton key={folder.name+"_"+idx}  type="link" onClick={()=>this.goDir(folder.path)}>{folder.name}</AntButton>
            /
          </>
        )}
      </>
    }

    if(tableWidthClass!=="width12")
    {
      
      let customfileStruct=(this.props.fileGroups===undefined)?[]:[...this.props.fileGroups];
      
      customfileStruct.push({name:"data",path:"./data/"});
    
    
      fv_UI.push(
      <div className="s height12 width2 scroll" key="sideMenu">
        <Menu
          onClick={(evt)=>{
            if(evt.item.props.list!==undefined)
            {
              let list = evt.item.props.list;
              if(this.state.selectedFileGroupInfo===undefined)
                this.setState({selectedFileGroupInfo:list});
              else
                this.setState({selectedFileGroupInfo:undefined});
              return;
            }
            
            this.setState({selectedFileGroupInfo:undefined});
            if(evt.item.props.path!==undefined)
              this.goDir(evt.item.props.path);
          }}
          mode="inline"
        >
          {
            customfileStruct.map((group,idx)=>
              <Menu.Item key={group.name+"_"+idx} path={group.path} list={group.list}>{group.name}</Menu.Item>)
          }
        </Menu>
      </div>);


    }

    titleRender=<>
    
      {pathSplitBtns}
      <br/>
      {
        (this.props.searchDepth>=0 || this.props.searchDepth===undefined)?
          <Input.Search key={"Search"} className="width3"  allowClear  style={{float:"left"}}
          size="middle" value={this.state.searchText} placeholder="Search" 
          onChange={(evt)=>{
            if(this.state.searchFolderStruct===undefined)
            {
              this.state.searchFolderStruct={};
              let path = this.state.folderStruct.path;
              this.fetchDirFiles(path,this.props.searchDepth)
              .then((data) => {
                let folderStruct={}
                if(data[1].data.ACK)
                {
                  folderStruct = data[0].data;
                }
                this.setState({searchFolderStruct:folderStruct});
                //console.log(this.state.searchFolderStruct);
              })
              .catch((err) => {
                
                this.setState({searchFolderStruct:undefined});
                //console.log(err);
              })
            }
            this.setState({searchText:evt.target.value});
          }}/>:null
      }
      
      <Divider type="vertical"  style={{float:"left"}}/>
      <AntButtonGroup  style={{float:"left"}}>
      {
        this.props.additionalFuncs===undefined?null:
        this.props.additionalFuncs.map((func,idx)=>
          <AntButton key={func.key} icon={func.icon}  onClick={()=>{
            func.action(this.state,this.props)
          }}>
            {func.name}
          </AntButton>)
      }
      </AntButtonGroup>
      
    </>



    fv_UI.push(
      <div className={"height12 scroll "+tableWidthClass} key="folderView">
        <Table key="fileList"
          onRow={(file) => ({
            onClick: (evt) => { 
              if(file.type!="DIR")
                this.props.onFileSelected(file.path,file,this.state.folderStruct);
              else
                this.goDir(file.path);
              
              this.setState({selectedFileGroupInfo:undefined});
            }})} 
          pagination={false}
          columns={columns} dataSource={fileList} />
      </div>);

    
     // title={}
    return (
        <Modal
          title={titleRender}
          visible={this.props.visible}
          //width={this.props.width===undefined?900:this.props.width}
          style={this.props.style}
          className={"modal-flex-justify-end "+this.props.className}
          onCancel={this.props.onCancel}
          onOk={this.props.onOk}
          footer={this.props.footer}
        >
          <div style={{height:"100%"}}>
            {fv_UI}
          </div>
        </Modal>
    );
  }
}






export let DropDown = React_createClass({

  handleClick: function(event,caller) {
    this.props.onClick(event,caller);
  },
  shouldComponentUpdate: function(nextProps, nextState) {
    return nextProps != this.props;
  },
  render: function() {
    var rows = [];

    for( var menu_sec of this.props.dropMenu){


      var group = [];

      for( var ele of menu_sec.ele){
        group.push(
          <Button
            addClass=" textAlignLeft dropDownBtn"
            key={ele.id}
            id={ele.id}
            text={ele.text}
            onClick={menu_sec.callBack}/>
        );
      }


      var classX="black HX"+menu_sec.ele.length+" ";


      rows.push(
        <div  key={menu_sec.id+"_div"}>
          <div
            key={menu_sec.text}
            className={classX+"width1 rotateContent"}>

            <p>
              {menu_sec.text}
            </p>

          </div>
          <div
            key={menu_sec.id+"_block"}
            className={classX+"width11"}>
            {group}
          </div>

        </div>
      );

      rows.push(
        <div
          key={menu_sec.id+"_"}
          className="black HX0_1 ">
        </div>
      );

    }



    var divStyle = {width:'300px'};
    return(
      <DropDownWarp
        containerClass={this.props.className}
        ifShowDropDown={this.props.ifShowDropDown}
        dropdownClass="aniFlipin"
        dropdownStyle={divStyle}>
        <Button
          addClass="HXF lgreen"
          text="..."
          onClick={this.handleClick}/>
        <CardFrameWarp boxShadow="1px 2px 20px #333">
          {rows}
        </CardFrameWarp>
      </DropDownWarp>
    );
  }
});
export let IconButton = React_createClass({

  handleClick: function(event) {
    this.props.onClick(event,this);
  },
  render: function() {

    log.debug(this.props);
    log.debug(this.context);
    var className=("button s "+ this.props.addClass);
    let translation = undefined;

    if(this.props.dict!==undefined)
    {
      translation = GetObjElement(this.props.dict,[this.props.dictTheme, this.props.text]);
  
      if(translation===undefined)
      {
        translation = GetObjElement(this.props.dict,["_", this.props.text]);
      }
    }
    if(translation===undefined)
    {
      translation = this.props.text;
    }
    // Forward data-* through. This component renders a bare <div> with three
    // props, so anything a caller adds is DROPPED SILENTLY -- a data-testid on
    // an IconButton looked applied, produced no attribute, and left a test
    // selecting by label text instead. Label text is the one thing
    // TEAM_HANDOFF §13 forbids, because every button on this bar is icon-only
    // and the wrong pick clicks rather than fails.
    //
    // data-* only, deliberately: a blanket {...this.props} would put onClick,
    // dict and iconType onto a DOM node and React would warn on every render.
    const dataProps = {};
    for (const k in this.props) if (k.indexOf('data-') === 0) dataProps[k] = this.props[k];
    //console.log(this.props.iconType)
    return <div
        onClick={this.handleClick}
        style={this.props.style}
        {...dataProps}
        className={className+" icon_btn"}>

        {
          (this.props.iconType === undefined)?
            null:
            this.props.iconType
        }

        <p className={"layout veleY iconTextPadding"}>
          {translation}
        </p>
    </div>;
  }
});

export let Button = React_createClass({

  handleClick: function(event) {
    this.props.onClick(event,this);
  },
  render: function() {
    var className=("button s vbox "+ this.props.addClass);
    return <div
      onClick={this.handleClick}
      className={className}>
      <p>
        {this.props.text}
      </p>
    </div>;
  }
});





export let ImgSprite = React_createClass({
  shouldComponentUpdate: function(nextProps, nextState) {
    return(
    (nextProps.position != this.props.position) ||
    (nextProps.id != this.props.id)||
    (nextProps.style != this.props.style)||
    (nextProps.offset != this.props.offset)
    );
  },
  render: function() {
    let spriteInfo=this.props.spriteInfo;

    let spriteScale=(this.props.spriteScale!=undefined)?this.props.spriteScale:1;

    let offset=(this.props.offset!=undefined)?this.props.offset:
    {
      x:0,
      y:0,
    };

    var xId=-this.props.id%spriteInfo.xLimit;
    var yId=-Math.floor(this.props.id/spriteInfo.xLimit);


    let style=Object.assign({},this.props.style,
      {
          width: spriteInfo.sWidth*spriteScale+"px",
          height:spriteInfo.sHeight*spriteScale+"px",
          background: 'url(' + spriteInfo.url + ') '+(offset.x+spriteInfo.sWidth*xId)*spriteScale+"px "+(offset.y+spriteInfo.sHeight*yId)*spriteScale+"px",
          backgroundSize: spriteInfo.width*spriteScale+"px "+spriteInfo.height*spriteScale+"px"
      }
    );

    return <div style={style} className={this.props.className}></div>
  }
});

export let SwitchButton = React_createClass({

  handleClick: function(event) {
    this.props.onClick(event,this);
  },
  render: function() {
    var className=("lgray vbox "+ this.props.addClass);
    return <button
      onClick={this.handleClick}
      className={className}>
      <p>
        {this.props.text}
      </p>
    </button>;
  }
});
