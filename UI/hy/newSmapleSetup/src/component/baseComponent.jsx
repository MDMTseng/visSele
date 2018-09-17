import React from 'react';
import React_createClass from 'create-react-class';

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



export let Button = React_createClass({

  handleClick: function(event) {
    this.props.onClick(event,this);
  },
  render: function() {
    var className=("button s lgray vbox "+ this.props.addClass);
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
