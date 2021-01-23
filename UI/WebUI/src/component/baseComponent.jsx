
import React, { useState,useEffect,useRef }from 'react';
import React_createClass from 'create-react-class';
import {GetObjElement} from 'UTIL/MISC_Util';

import dclone from 'clone';
import * as logX from 'loglevel';

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

import NumPad from 'react-numpad';

import { parse } from 'semver';
const AntButtonGroup = AntButton.Group;


// import { button as AntButton } from 'antd/lib/button';
// import {Button as AntButton} from 'antd/lib/Button';

let log = logX.getLogger("baseComponent");

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


export class JsonElement extends React.Component{

  render()
  {
 
    if(this.props.type.type !== undefined)
    {
      return this.renderComplex();
    }
    else
    {
      //if(this.props.type instanceof String)
      
      return this.renderSimple();
      
    }
  }


  renderComplex()
  {
    switch(this.props.type.type)
    {
      case "droplist":
      return <div key={this.props.id} className={this.props.className} >
        {this.props.children}
      </div>
    }
  }
  renderSimple()
  {
    let text=this.props.children;
    let translateValue = undefined;
    if(this.props.type=="div"){
      translateValue = GetObjElement(this.props.dict,[this.props.dictTheme, text]);

      if(translateValue===undefined)
      {
        translateValue = GetObjElement(this.props.dict,["_", text]);
      }
    }


    if(translateValue===undefined)
    {
      translateValue = text;
    }
    
    switch(this.props.type)
    {
      case "input-number":
        translateValue = translateValue+"";

        return  <NumPad.Number
          key={this.props.id}
          onChange={(value)=>this.props.onChange(this.props.target,this.props.type,{target:{value}})}
          value={translateValue}>
            < InputNumber 
              className={this.props.className} 
              value={translateValue}/>
          </NumPad.Number>;
        // return  <NumPad.Number
        //   key={this.props.id}
        //   onChange={(value)=>this.props.onChange(this.props.target,this.props.type,{target:{value}})}
        //   value={translateValue}/>;
        // return < InputNumber 
        //   key={this.props.id} 
        //   className={this.props.className} 
        //   //defaultValue={translateValue}
        //   value={translateValue}
        //   onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
        
      case "input":
        return <input key={this.props.id} className={this.props.className} value={translateValue}
                      onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
          
      case "checkbox":
        return <input key={this.props.id} className={this.props.className} type="checkbox" checked={translateValue}
                      onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
      case "btn":
        return <button
          key={this.props.id}
          className={this.props.className}
          onClick={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}>
          {translateValue}</button>

      
      case "switch":
      {
        let checked=(typeof translateValue === "boolean")?translateValue:(translateValue<0)
        return <div className={this.props.className+" white"}>
          <Switch checked={checked}
            onChange={(checked)=>this.props.onChange(this.props.target,this.props.type,{target:{checked}})} />
        </div>
      }
      
      case "div":
      default:
        if(this.props.renderLib!==undefined && this.props.renderLib[this.props.type]!==undefined)
        {
          let renderFunc = this.props.renderLib[this.props.type];
          return renderFunc(this.props);
        }
        return <div key={this.props.id} className={this.props.className} style={{backgroundColor:"#DDD"}}>{translateValue} </div>
      
    }
  }
}

export class JsonEditBlock extends React.Component{
  

  constructor(props) {
      super(props);
      this.tmp={
        object:{}
      };
  }

  onChangeX(target,type,evt) {
    this.props.jsonChange(this.tmp.object,target,type,evt);
    return true;
  }
  shouldComponentUpdate(nextProps, nextState) {
    return true;
  }

  composeObject(obj,whiteListKey=null,idHeader="",keyTrace=[])
  {
    var rows = [];
    let keyList = (whiteListKey==null )?obj:whiteListKey;
    for (var key in keyList) {
        let ele = obj[key];
        let renderContext=(whiteListKey==null )?null:whiteListKey[key];

        //console.log(key,keyList[key],obj,obj[key]);
        if((ele === undefined) || renderContext=== undefined)continue;
        //console.log(key,ele,typeof ele);
        
        //this.props.dict
        
        let newkeyTrace = keyTrace.slice();
        newkeyTrace.push(key);

        let translateKey = undefined;


        translateKey = GetObjElement(this.props.dict,[this.props.dictTheme, key]);

        if(translateKey===undefined)
        {
          translateKey = GetObjElement(this.props.dict,["_", key]);
        }

        if(translateKey===undefined)
        {
          translateKey = key;
        }

        let Render_comp=undefined;
        if(typeof renderContext.__OBJ__==="function")
        {
          Render_comp = renderContext.__OBJ__;
        }
        else if(this.props.renderLib!==undefined)
        {
          if(typeof this.props.renderLib[renderContext]==="function")
          {
            Render_comp = this.props.renderLib[renderContext];
          }
          else if(typeof this.props.renderLib[renderContext.__OBJ__]==="function")
          {
            Render_comp = this.props.renderLib[renderContext.__OBJ__];
          }
        }
        if(Render_comp!==undefined)
        {
          // rows.push(
          //   Render_comp({
          //     className:"s WXF vbox black",
          //     onChange:this.onChangeX.bind(this),
          //     target:{obj:obj,keyTrace:newkeyTrace},
          //     renderContext,
          //     props:this.props
          //   }));

            
          //{className,onChange,target,renderContext,props}

          rows.push(
            <Render_comp 
              key={idHeader+"_"+key+"_Render_comp"}
              className="s WXF vbox black"
              onChange={this.onChangeX.bind(this)}
              target={{obj:obj,keyTrace:newkeyTrace}}
              obj={obj}
              keyTrace={newkeyTrace}
              renderContext={renderContext}
              props={this.props}
            />);
          continue;
        }
        else if(typeof ele === "object")
        {
          rows.push(<div key={idHeader+"_"+key+"_HL"} className="s HX0_1 WXF  vbox"></div>);
          let obj_disp_type = (renderContext==null)?"div":renderContext.__OBJ__;
          if(obj_disp_type == undefined)obj_disp_type="div";
          rows.push(<JsonElement key={idHeader+"_"+key+"_ele"}
                                 dict={this.props.dict}
            className="s HX1 WXF vbox black" type={obj_disp_type} 
            onChange={this.onChangeX.bind(this)}
            target={{obj:obj,keyTrace:newkeyTrace}}>{translateKey}</JsonElement>);

          rows.push(<div key={idHeader+"_"+key+"__"} className="s HX1 width1"></div>);
          rows.push(<div key={idHeader+"_"+key+"_C"} className="s HXA width11">{
            this.composeObject(ele,renderContext,idHeader+"_"+key,newkeyTrace)
          }</div>);
          rows.push(<div key={idHeader+"_"+key+"_HL2"} className="s HX0_1 WXF  vbox"></div>);
        }
        else
        {
          if(renderContext==null)renderContext="div";
          rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width4 vbox black">{translateKey}</div>);
          rows.push(<JsonElement key={idHeader+"_"+key+"_ele"} className="s HX1 width8 vbox blackText" type={renderContext}
            target={{obj:obj,keyTrace:newkeyTrace}}
            dict={this.props.dict}
            onChange={this.onChangeX.bind(this)}>{(ele)}</JsonElement>);
        }
    }
    return rows
  }

  render() {
    this.tmp.object = dclone(this.props.object);
   //console.log("this.props.object:",this.props.object,this.tmp.object);
    var rows = this.composeObject(this.tmp.object,this.props.whiteListKey);
    return(
    <div className="WXF HXA">
      {rows}
    </div>
    );
  }
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
    
    console.log(isTarFileExist);
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
        console.log(">>>>>");
        return;
      }
      this.state.history.push(path);
    }

    this.fetchDirFiles(path)
    .then((data) => {
      console.log("fetchDirFiles OK:",data)
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
      console.log("fetchDirFiles exception",err)
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
    if(this.state.searchText!==undefined && this.state.searchText.length>0)
    {
      columns = ['type','name','path'].map((info)=>({
        title: info,
        dataIndex: info,
        key:info,
      }));
      
      columns[0].render=(text, record) => {
        let iconType=(text=="DIR")?<FolderOutlined />:<FileOutlined />
        return iconType
      }
      columns[0].width=64;
      

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
      console.log(this.state.selectedFileGroupInfo,fileList);

      tableWidthClass="width12"
    }
    else if(this.state.selectedFileGroupInfo!==undefined)
    {
      columns = ['type','name','path'].map((info)=>({
        title: info,
        dataIndex: info,
        key:info,
      }));
      
      columns[0].render=(text, record) => {
        let iconType=(text=="DIR")?<FolderOutlined />:<FileOutlined />
        return iconType
      }
      columns[0].width=64;
      
      fileList=this.state.selectedFileGroupInfo;
      tableWidthClass="width10"
    }
    else
    {
      columns = ['type','name','size'].map((info)=>({
        title: info,
        dataIndex: info,
        key:info,
      }));
  
      columns[0].render=(text, record) => {
        let iconType=(text=="DIR")?<FolderOutlined />:<FileOutlined />
        return iconType
      }
      columns[0].width=64;
  

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
      
      customfileStruct.push({name:"origin",path:"./"});
    
    
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
              console.log(evt);
              if(file.type!="DIR")
                this.props.onFileSelected(file.path,file);
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
          className={this.props.className}
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
    //console.log(this.props.iconType)
    return <div
        onClick={this.handleClick}
        style={this.props.style}
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
