import JSum from 'jsum';



export function Num2Str_padding(pad,num)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
}

export function round(num,round_nor=1)
{
  let tmp = (1/round_nor);
  let round_nor_inv = Math.round(tmp);
  if(round_nor_inv==0)
  {
    round_nor_inv=tmp;
  }

  return Math.round(num*round_nor_inv)/round_nor_inv;
}


export function xstate_GetCurrentMainState(state)
{
  if(typeof state.value === "string")
  {
    return {state:state.value, substate:null};
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return {state:state_Str, substate:complexState[state_Str]};

}

export function xstate_GetMainState(state)
{
  if(typeof state.value === "string")
  {
    return state.value;
  }
  let complexState = state.value;
  return Object.keys(complexState)[0];
}

export function xstate_GetSubState(state)
{
  if(typeof state.value === "string")
  {
    return undefined;
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return complexState[state_Str];
}


export function GetObjElement(rootObj,keyTrace,traceIdxTo=keyTrace.length-1)
{
  let obj = rootObj;
  let traceIdxTLen = traceIdxTo+1;
  if( rootObj === undefined)return;
  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    //console.log(obj,key,obj[key]);
    obj = obj[key];

    if( obj === undefined)return;
  }
  return obj;
}

export function isString (value) {
  return typeof value === 'string' || value instanceof String;
}


export function DictConv(key,dict,dictTheme)
{
  let translation = (dictTheme===undefined)?undefined:GetObjElement(dict,[dictTheme, key]);

  if(translation===undefined)
  {
    translation = GetObjElement(dict,["fallback", key]);
  }

  return translation;
}



export { websocket_autoReconnect, websocket_reqTrack, websocket_aliveTracking } from './websocket';


  
export function dictLookUp(key,dict,theme) {
  return  Array.isArray(key)?
  GetObjElement(dict,key)||key[key.length-1]:
  GetObjElement(dict,[theme||"_", key])||key;
}


export const copyToClipboard = str => {
  const el = document.createElement('textarea');
  el.value = str;
  document.body.appendChild(el);
  el.select();
  document.execCommand('copy');
  document.body.removeChild(el);
};





export function twoEQXY(a,b,c,d,e,f) {
  /* we solve the linear system
  * ax+by=e
  * cx+dy=f
  */
 let determinant = a*d - b*c;
 if(determinant != 0) {
   let x = (e*d - b*f)/determinant;
   let y = (a*f - e*c)/determinant;
   return [x,y];
 }
 return [0/0,0/0];
}

export function Calibration_MMPP_offset(old1,new1,old2,new2,cur_mmpp,cur_offset=0) {
 //pix1=old1/cur_mmpp-cur_offset
 //mmpp*(pix1+offset)=new1
 //mmpp*pix1 + mmpp*offset = new1
 //X   *a    +           Y =    e
 //b==1

 //a = old_pix1  b=1  e=new1
 //c = old_pix2  d=1  f=new2
 let old_pix1=old1/cur_mmpp-cur_offset;
 let old_pix2=old2/cur_mmpp-cur_offset;
 let retSolver=twoEQXY(
  old_pix1,1,old_pix2,1,new1,new2);
 let [mmpp,mmpp_x_offset] = retSolver;
 return {
   mmpp,offset:(mmpp_x_offset/mmpp)
 };
}







// expression engine + data structures live in focused files; re-export for callers
export { Exp2PostfixExp, PostfixExpCalc } from './expr';
export { CircularCounter, ConsumeQueue } from './structures';



// let exp_str = "Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)";
// //"3+tan(5-1/4*3)/3"
// //"1+2*3/(4+5)/6"

// console.log(//eval(exp_str),
//   ExpCalc(exp_str, {
//     "$>$?$:$":(vals)=>{
//       return vals[0]>vals[1]?vals[2]:vals[3];
//     },
//     "Math.sin$":(vals)=>{
//       return Math.sin(vals[0]);
//     },
//     "Math.tan$":(vals)=>{
//       return Math.tan(vals[0]);
//     },
//     "Math.min$":(vals)=> Math.min(...vals),
//     "Math.max$":(vals)=> Math.max(...vals),
//     default: (key, vals) =>{
//       if(vals===undefined)//it's a single value parsing
//       {
//         let pv=parseFloat(key);
//         if(pv!=pv)
//           {
//             throw "ERROR: key:"+key+" is not parsible!";
//           }
//         return pv;
//       }
      
//       if(key.match(/^\$[\,\$]+$/gm)!==null)
//         return vals;
//       throw "ERROR: "+key+" is not defined!";
//       return vals;
//     }
//   })
// );




export function defFileGeneration(edit_info)
{

  let feature_sig360_circle_line = edit_info._obj.GenerateFeature_sig360_circle_line();
  let preloadedDefFile = edit_info.loadedDefFile;
  if (preloadedDefFile === undefined) preloadedDefFile = {};
  let report = {
    ...preloadedDefFile,
    type: "binary_processing_group",
    intrusionSizeLimitRatio: edit_info.intrusionSizeLimitRatio,
    featureSet: [feature_sig360_circle_line]
  };
  delete report["featureSet_sha1"];

  report.name = edit_info.DefFileName;
  report.tag = edit_info.DefFileTag;

  report.featureSet[0].matching_angle_margin_deg = edit_info.matching_angle_margin_deg;
  report.featureSet[0].matching_angle_offset_deg = edit_info.matching_angle_offset_deg;
  report.featureSet[0].matching_face = edit_info.matching_face;

  let sha1_info_in_json = JSum.digest(report.featureSet, 'sha1', 'hex');
  report.featureSet[0]["__decorator"] = edit_info.__decorator;



  report.featureSet_sha1 = sha1_info_in_json;
  //this.props.ACT_DefFileHash_Update(sha1_info_in_json);
  //console.log(edit_info);
  if(edit_info.DefFileHash===sha1_info_in_json)
  {//means the main part of deffile is still the same, use same DefFileHash_pre
    report.featureSet_sha1_pre = edit_info.DefFileHash_pre;
  }
  else
  {
  report.featureSet_sha1_pre = edit_info.DefFileHash;
  }

  if (edit_info.DefFileHash_root === undefined) {
    if (edit_info.featureSet_sha1_pre === undefined)
      report.featureSet_sha1_root = sha1_info_in_json;
    else
      report.featureSet_sha1_root = edit_info.featureSet_sha1_pre;
  }
  else
    report.featureSet_sha1_root = edit_info.DefFileHash_root;

  return report;
}




export const LocalStorageTools={
  getlist:(lsKey)=>
  {
  
    if(localStorage===undefined)return [];
    let LocalS_list = localStorage.getItem(lsKey);

    try {
      LocalS_list = JSON.parse(LocalS_list);
      if (!(LocalS_list instanceof Array)) {
        LocalS_list = [];
      }
  
        
    } catch (e) {
      LocalS_list = [];
    }
    return LocalS_list;
  },
  setlist:(lsKey,list)=>localStorage.setItem(lsKey, JSON.stringify(list)),
  appendlist:(lsKey,data,pre_removeFilter)=>
  {
    let LocalS_list = LocalStorageTools.getlist(lsKey);

    if(pre_removeFilter!==undefined)
    {
      LocalS_list = LocalS_list.filter(pre_removeFilter);
    }
    LocalS_list.unshift(data);
    LocalStorageTools.setlist(lsKey, LocalS_list)
    return true;
  },
  getobj:(lsKey)=>
  {
  
    if(localStorage===undefined)return undefined;
    let LocalS_list = localStorage.getItem(lsKey);

    try {
      LocalS_list = JSON.parse(LocalS_list);
     
    } catch (e) {
      LocalS_list = undefined;
    }
    return LocalS_list;
  },
  setobj:(lsKey,obj)=>localStorage.setItem(lsKey, JSON.stringify(obj)),



}
