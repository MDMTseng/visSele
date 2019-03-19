import React from 'react';
import React_createClass from 'create-react-class';
import {GetObjElement} from 'UTIL/MISC_Util';

import dclone from 'clone';
import * as logX from 'loglevel';

import EC_zh_TW from "../languages/zh_TW";

import  {default as AntButton}  from 'antd/lib/button';
import  Modal  from 'antd/lib/modal';
import  Table  from 'antd/lib/table';
import  Icon  from 'antd/lib/icon';
import Menu from 'antd/lib/menu';
import Input from 'antd/lib/input'
const AntButtonGroup = AntButton.Group;


// import { button as AntButton } from 'antd/lib/button';
// import {Button as AntButton} from 'antd/lib/Button';

let log = logX.getLogger("baseComponent");

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
        translateValue = GetObjElement(this.props.dict,["fallback", text]);
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
        return <input key={this.props.id} className={this.props.className} type="number" step="0.01" pattern="^[-+]?[0-9]*(\.[0-9]*)?" 
          value={translateValue}
          onChange={(evt)=>this.props.onChange(this.props.target,this.props.type,evt)}/>
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
      case "div":
      default:
        return <div key={this.props.id} className={this.props.className} >{translateValue} </div>
      
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
        let displayMethod=(whiteListKey==null )?null:whiteListKey[key];

        //console.log(key,keyList[key],obj,obj[key]);
        if((ele === undefined) || displayMethod=== undefined)continue;
        //console.log(key,ele,typeof ele);
        
        //this.props.dict
        
        let newkeyTrace = keyTrace.slice();
        newkeyTrace.push(key);

        let translateKey = undefined;


        translateKey = GetObjElement(this.props.dict,[this.props.dictTheme, key]);

        if(translateKey===undefined)
        {
          translateKey = GetObjElement(this.props.dict,["fallback", key]);
        }

        if(translateKey===undefined)
        {
          translateKey = key;
        }

        switch(typeof ele)
        {
          case "string":
          case "boolean":
          case "number":
            if(displayMethod==null)displayMethod="div";



            rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width4 vbox black">{translateKey}</div>);


            rows.push(<JsonElement key={idHeader+"_"+key+"_ele"} className="s HX1 width8 vbox blackText" type={displayMethod}
              target={{obj:obj,keyTrace:newkeyTrace}}
              dict={this.props.dict}
              onChange={this.onChangeX.bind(this)}>{(ele)}</JsonElement>);
          break;
          case "object":
          {

            rows.push(<div key={idHeader+"_"+key+"_HL"} className="s HX0_1 WXF  vbox"></div>);
            let obj_disp_type = (displayMethod==null)?"div":displayMethod.__OBJ__;
            if(obj_disp_type == undefined)obj_disp_type="div";
            rows.push(<JsonElement key={idHeader+"_"+key+"_ele"}
                                   dict={this.props.dict}
              className="s HX1 WXF vbox black" type={obj_disp_type} 
              onChange={this.onChangeX.bind(this)}
              target={{obj:obj,keyTrace:newkeyTrace}}>{translateKey}</JsonElement>);

            rows.push(<div key={idHeader+"_"+key+"__"} className="s HX1 width1"></div>);
            rows.push(<div key={idHeader+"_"+key+"_C"} className="s HXA width11">{
              this.composeObject(ele,displayMethod,idHeader+"_"+key,newkeyTrace)
            }</div>);
            rows.push(<div key={idHeader+"_"+key+"_HL2"} className="s HX0_1 WXF  vbox"></div>);
          }
          break;
          default:
            rows.push(<div key={idHeader+"_"+key+"_txt"} className="s HX1 width3 vbox black">{key}</div>);
            rows.push(<div key={idHeader+"_"+key+"_XXX"} className="s HX1 width9 vbox lblue">Not supported</div>);
          break;
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
    let isTarFileExist = this.state.folderInfo!=undefined &&
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
        value={this.state.fileName}
        onChange={(ev)=>{
          let fileName = ev.target.value;
          if(this.isASCII(fileName))
          {
            this.setState({...this.state,fileName})
          }
          }}/>
        
        <AntButtonGroup className="width2">
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
    }
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
        return;
      }
      this.state.history.push(path);
    }
    new Promise((resolve, reject) => {
      this.props.BPG_Channel("FB",0,{//"FB" is for file browsing
        path:path,
        depth:0,
      },undefined,{resolve,reject});
      setTimeout(()=>reject("Timeout"),1000)
    })
    .then((data) => {
      if(data[1].data.ACK)
      {
        let folderStruct = data[0].data;
        this.setState(Object.assign(this.state,{folderStruct:folderStruct}));
        if(this.props.onFolderLoaded!==undefined)
        {
          this.props.onFolderLoaded(folderStruct);
        }
      }
      else
      {
        setTimeout(()=>this.goDir(),1000)
        
      }
    })
    .catch((err) => {
      console.log(err);
    })
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
  
  render()
  {

    const columns = ['type','name','size'].map((info)=>({
      title: info,
      dataIndex: info,
      key:info,
    }));

    columns[0].render=(text, record) => {
      
      let iconType=(text=="DIR")?"folder":"file"
      return <Icon type={iconType} />
    }
    columns[0].width=64;

    let fileList = (this.state.folderStruct.files===undefined)?[]:this.state.folderStruct.files;
    fileList = fileList.filter((file)=>
      (file.name!='.' && file.name!='..' )&&
      (this.props.fileFilter===undefined?true:this.props.fileFilter(file)
    ));
    fileList.forEach((file)=>{
      file.size = this.bytesToSize(file.size_bytes);
      file.key = file.name;
    });

    let favoriteDir=[
      {name:"origin",path:"./"},];




    let curPathArr = (typeof this.state.folderStruct.path ==='string')?
      this.state.folderStruct.path.replace(/\\+/g, "/").split("/"):[];

    curPathArr=curPathArr.map((name)=>({name}));

    curPathArr.reduce((pathInt,pathObj)=>{
        pathInt=pathInt+pathObj.name+"/";
        pathObj.path=pathInt;
        return pathInt;
      },"");
      
    let titleRender=<div>
      
      <AntButtonGroup>
        <AntButton type="primary"  onClick={()=>this.goDir()}>
          <Icon type="left" />
        </AntButton>
      </AntButtonGroup>
      &nbsp;&nbsp;&nbsp;
      <AntButtonGroup>
        {curPathArr.map((folder,idx)=>
          <AntButton key={folder.name+"_"+idx}   onClick={()=>this.goDir(folder.path)}>{folder.name}</AntButton>
        )}
      </AntButtonGroup>

    </div>

     // title={}
    return (
        <Modal
          title={titleRender}
          visible={this.props.visible}
          width={this.props.width===undefined?900:this.props.width}
          onCancel={this.props.onCancel}
          onOk={this.props.onOk}
          footer={this.props.footer}
        >
          <div style={{height:this.props.height===undefined?400:this.props.height}}>
          
            <div className="s height12 width2 scroll" key="sideMenu">
              <Menu
                onClick={(evt)=>{this.goDir(evt.item.props.path);}}
                mode="inline"
              >
                {
                  favoriteDir.map(dir=>
                    <Menu.Item key={dir.name} path={dir.path}>{dir.name}</Menu.Item>)
                }
              </Menu>
            </div>
            <div className="height12 width10 scroll" key="folderView">
              <Table 
                onRow={(file) => ({
                  onClick: (evt) => { 
                    if(file.type!="DIR")
                      this.props.onFileSelected(this.state.folderStruct.path+"/"+file.name,file);
                    else
                      this.goDir(this.state.folderStruct.path+"/"+file.name);
                  }})} 
                pagination={false}
                columns={columns} dataSource={fileList} />
              </div>
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
export let AButton = React_createClass({

  handleClick: function(event) {
    this.props.onClick(event,this);
  },
  render: function() {

    let translation = undefined;


    translation = GetObjElement(this.props.dict,[this.props.dictTheme, this.props.text]);

    if(translation===undefined)
    {
      translation = GetObjElement(this.props.dict,["fallback", this.props.text]);
    }

    if(translation===undefined)
    {
      translation = this.props.text;
    }

    return <AntButton block type={this.props.type} shape="round" icon={this.props.icon} size={this.props.size} dict={EC_zh_TW}
            onClick={this.handleClick}>
      {translation}
    </AntButton>;

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
        translation = GetObjElement(this.props.dict,["fallback", this.props.text]);
      }
    }
    if(translation===undefined)
    {
      translation = this.props.text;
    }

    return <div
        onClick={this.handleClick}
        className={className}>

        {
          (this.props.iconType === undefined)?
            null:
            <Icon className="layout iconButtonSize veleY" type={this.props.iconType}/>
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
