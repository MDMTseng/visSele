import { connect } from 'react-redux'
import React , { useState,useEffect } from 'react';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import  Icon  from 'antd/lib/icon';
import  Menu  from 'antd/lib/menu';
import  Button  from 'antd/lib/button';
import  Input  from 'antd/lib/input';
import  Tag  from 'antd/lib/tag';
import  Dropdown  from 'antd/lib/dropdown';
import {CusDisp_DB} from 'UTIL/DB_Query';
import  Tabs  from 'antd/lib/tabs';
import { useSelector,useDispatch } from 'react-redux';


const { TabPane } = Tabs;

function Array_NtoM(N,M)
{
  let Len=M-N+1;
  return [ ...Array(Len).keys() ].map( i => i+N);
}


export function CustomDisplaySelectUI({onSelect}) {
  const [displayInfo, setDisplayInfo] = useState(undefined);
  const [catSet, setCatSet] = useState(undefined);
  const [displayEle, setDisplayEle] = useState(undefined);

  useEffect(() => {
    setDisplayInfo(undefined);
    setCatSet(undefined);
    CusDisp_DB.read(".").then(data=>{
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
            <Button onClick={()=>onSelect(info)}>
              {info.name}
            </Button>
          )}
        </TabPane>
      )}
    </Tabs>
    
  }
  return (
    <div>
    {UI}
    </div>
  );
}

export const essentialTags=["01首件熱前","02首件熱後","11沖壓成形","21真空熱處理","31滾電金","31滾電錫","35清洗封孔","36連續鍍"];






class TagDisplay extends React.Component{

  constructor(props) {
    super(props);
    this.state={
    }
  }

  render()
  {
    return <div className={this.props.className}>
      <Tag className="large InspTag fixed" key="MACH">{this.props.MachTag}</Tag>
      {this.props.defFileTag.map(tag=><Tag className="large InspTag fixed" key={tag+"_dfTag"}>{tag}</Tag>)}

      {this.props.inspOptionalTag.map(curTag=>
        <Tag closable className="large InspTag optional"  key={curTag+"_inspOptTag"} onClose={(e)=>{
          e.preventDefault();
          let tagToDelete=curTag;
          let NewOptionalTag = this.props.inspOptionalTag.filter(tag=>tag!=tagToDelete);
          //console.log(e.target,NewOptionalTag);
          this.props.ACT_InspOptionalTag_Update(NewOptionalTag);
          }}>{curTag}</Tag>)}
      <br/>
    </div>
  }
   
}

export const TagDisplay_rdx = connect(
  (state) =>({
    inspOptionalTag:state.UIData.edit_info.inspOptionalTag,
    defFileTag:state.UIData.edit_info.DefFileTag,
    MachTag:state.UIData.MachTag,
  }),
  (dispatch, ownProps) => ({ 
    ACT_InspOptionalTag_Update:(newTag)=>{dispatch(DefConfAct.InspOptionalTag_Update(newTag))},
  })
)(TagDisplay);


export const tagGroupsPreset=[
  {
    name:"製程",
    maxCount:1,
    minCount:1,
    tags:["01首件熱前","02首件熱後","11沖壓成形","21真空熱處理","31滾電金","31滾電錫","35清洗封孔","36連續鍍"]
  },
  {
    name:"檢測方式",
    tags:["測試","全檢"]
  }

]



function setupGroupSelectInfo(groups)
{
  let info=groups.map(group=>{
    return {
      selectIndexes:[],
      g:group
    }
  })
  return info
}
export const TagOptions_rdx = ({className}) => {
  const inspOptionalTag = useSelector(state => state.UIData.edit_info.inspOptionalTag);
  const defFileTag = useSelector(state => state.UIData.edit_info.DefFileTag);
  const MachTag = useSelector(state => state.UIData.MachTag);
  const dispatch = useDispatch();
  const ACT_InspOptionalTag_Update= (newTag) => dispatch(dispatch(DefConfAct.InspOptionalTag_Update(newTag)));
 
  
  const [newTagStr,setNewTagStr]=useState([]);

  return <div className={className}>
    {
    }
    {



      essentialTags.map((ele,idx,arr)=>
      <Tag className="large InspTag optional fixed" key={ele+"_essTag"} 
        onClick={()=>{
          var array3 = inspOptionalTag.filter((obj)=>arr.indexOf(obj) == -1);
          ACT_InspOptionalTag_Update([...array3,ele])
        }}>{ele}</Tag>
      )}
    }

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
  
}

