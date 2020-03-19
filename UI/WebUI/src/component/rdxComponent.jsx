import { connect } from 'react-redux'
import React from 'react';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import  Icon  from 'antd/lib/icon';
import  Menu  from 'antd/lib/menu';
import  Button  from 'antd/lib/button';
import  Input  from 'antd/lib/input';
import  Tag  from 'antd/lib/tag';
import  Dropdown  from 'antd/lib/dropdown';

function Array_NtoM(N,M)
{
  let Len=M-N+1;
  return [ ...Array(Len).keys() ].map( i => i+N);
}

class TagOptions extends React.Component{

  constructor(props) {
      super(props);
      this.state={
      }
  }
  render()
  {
    console.log(this.props.defFileTag)
    return <div className={this.props.className}>
      {this.props.defFileTag.map(tag=><Tag className="large InspTag fixed">{tag}</Tag>)}
                
      {this.props.inspOptionalTag.map(curTag=>
        <Tag closable className="large InspTag optional" onClose={(e)=>{
          e.preventDefault();
          let tagToDelete=curTag;
          let NewOptionalTag = this.props.inspOptionalTag.filter(tag=>tag!=tagToDelete);
          console.log(e.target,NewOptionalTag);
          this.props.ACT_InspOptionalTag_Update(NewOptionalTag);
          }}>{curTag}</Tag>)}
      <br/>
      <Input placeholder="新標籤"
        onChange={(e)=>{
          let newStr=e.target.value;
          //e.target.setSelectionRange(0, newStr.length)
          this.setState({...this.state,newTagStr:newStr});
        }}
        onPressEnter={(e)=>{
          let newTag=e.target.value.split(",");
          let newTags=[...this.props.inspOptionalTag,...newTag];
          this.props.ACT_InspOptionalTag_Update(newTags);
          this.setState({...this.state,newTagStr:""});
        }}
        className={"width3 "+((this.props.inspOptionalTag.find((str)=>str==this.state.newTagStr))?"error":"")}
        allowClear
        value={this.state.newTagStr}
        prefix={<Icon type="tags"/>}
      />
      {
        ["01首件熱前","02首件熱後","11沖壓成形","21真空熱處理","31滾鍍","32連續鍍"].map((ele,idx,arr)=>
        <Tag className="large InspTag optional fixed"  
          onClick={()=>{
            var array3 = this.props.inspOptionalTag.filter((obj)=>arr.indexOf(obj) == -1);
            this.props.ACT_InspOptionalTag_Update([...array3,ele])
          }}>{ele}</Tag>
        )}
      }
      {
        [
        //   {type:"成形",subtype:["[11]巡檢","[12]首件"]},
        // {type:"熱處理",subtype:["[21]熱處理"]},
        // {type:"表面處理",subtype:["[31]電鍍","[32]電著","[33]攻牙","[34]震動研磨"]},
        {type:"機台",subtype:[...(Array_NtoM(1,40).map(n=>"M"+n)),null,...(Array_NtoM(7,15).map(n=>"P"+n))]},
        ].map(catg=>
          <Dropdown overlay={  <Menu>{
                catg.subtype.map((ele,idx,arr)=>ele===null?<br/>:
                  <Tag className="xlarge InspTag optional fixed"  
                    onClick={()=>{
                      var array3 = this.props.inspOptionalTag.filter((obj)=>arr.indexOf(obj) == -1);
                      this.props.ACT_InspOptionalTag_Update([...array3,ele])
                    }}>{ele}</Tag>
                )}
              </Menu>} placement="bottomLeft">
                <Button>{catg.type}</Button>
          </Dropdown>)
      }

      </div>
  }
}

export const TagOptions_rdx = connect(
  (state) =>({
    inspOptionalTag:state.UIData.edit_info.inspOptionalTag,
    defFileTag:state.UIData.edit_info.DefFileTag,
  }),
  (dispatch, ownProps) => ({ 
    ACT_InspOptionalTag_Update:(newTag)=>{dispatch(DefConfAct.InspOptionalTag_Update(newTag))},
  })
)(TagOptions);
