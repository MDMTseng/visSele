import React from 'react';
import React_createClass from 'create-react-class';
import { Icon } from 'antd';
import {GetObjElement} from 'UTIL/MISC_Util';



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
    if(this.props.type!=="input-number"||this.props.type!=="checkbox"){

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
        return <input key={this.props.id} className={this.props.className} type="number" value={translateValue}
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
              onChange={this.onChangeX.bind(this)}>{(typeof ele ==="number" )?(ele).toFixed(4):(ele)}</JsonElement>);
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
    this.tmp.object = JSON.parse(JSON.stringify(this.props.object));
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

    console.log(this.props);
    console.log(this.context);
    var className=("button s "+ this.props.addClass);
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


    return <div
        onClick={this.handleClick}
        className={className}>
        <Icon className="layout iconButtonSize veleY" type={this.props.iconType}/>

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
