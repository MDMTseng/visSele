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


  
import { mkLog } from 'UTIL/logger';
const _i18nLog = mkLog('i18n');

export function dictLookUp(key,dict,theme) {
  const path = Array.isArray(key) ? key : [theme||"_", key];
  const hit = GetObjElement(dict, path);
  if (hit) return hit;
  // Surface missing keys ONCE per key (no spam). Flip noise via
  //   __log.verbose('i18n') / __log.quiet('i18n')
  if (!dictLookUp._warned) dictLookUp._warned = new Set();
  const pathKey = path.join('.');
  if (!dictLookUp._warned.has(pathKey)) {
    dictLookUp._warned.add(pathKey);
    _i18nLog.warn('[missing-key]', pathKey);
  }
  return Array.isArray(key) ? key[key.length-1] : key;
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
  // Only emit perf opt-ins if non-default — keeps existing defs hash-stable.
  // v2 matcher: the core reads matching_version as the STRING "v2" on the sig360
  // sub-feature (FeatureManager_sig360_circle_line.cpp: JFetch(...,cJSON_String)).
  // A numeric 2 is silently ignored by the type-strict JFetch, so emit "v2".
  if (typeof edit_info.matching_version === 'number' && edit_info.matching_version === 2)
    report.featureSet[0].matching_version = "v2";
  // downsample: the core reads inspection_downsample as a NUMBER on the GROUP
  // (top-level) root (FeatureManager_group.cpp), alongside intrusionSizeLimitRatio
  // — NOT inside featureSet[0]. Emit it at the top level so the group picks it up.
  if (typeof edit_info.inspection_downsample === 'number' && edit_info.inspection_downsample !== 1)
    report.inspection_downsample = edit_info.inspection_downsample;
  // sig360 match-acceptance threshold: core reads "sig_match_sim_thres" (NUMBER,
  // default 0.9) on the sig360 sub-feature. Only emit when non-default to keep
  // existing defs hash-stable.
  if (typeof edit_info.sig_match_sim_thres === 'number')
    report.featureSet[0].sig_match_sim_thres = edit_info.sig_match_sim_thres;
  // Locating-anchor morph (deformation correction). Core reads "morph_mode" (STRING:
  // "tps" | "wls_similarity" | "legacy") on the sig360 sub-feature; absent => "tps"
  // (the core default). Only emit when non-default to keep existing defs hash-stable.
  // lambda/max_iter only emitted when the user has set them (else core defaults).
  if (typeof edit_info.morph_mode === 'string' && edit_info.morph_mode !== 'tps')
    report.featureSet[0].morph_mode = edit_info.morph_mode;
  if (typeof edit_info.morph_tps_lambda === 'number')
    report.featureSet[0].morph_tps_lambda = edit_info.morph_tps_lambda;
  if (typeof edit_info.morph_max_iter === 'number')
    report.featureSet[0].morph_max_iter = edit_info.morph_max_iter;
  if (typeof edit_info.morph_alpha === 'number')
    report.featureSet[0].morph_alpha = edit_info.morph_alpha;
  if (typeof edit_info.shape_match_scale === 'number')
    report.featureSet[0].shape_match_scale = edit_info.shape_match_scale;

  // Strip transient per-frame inspection RESULTS from the shapes before they get
  // hashed/saved. cal_hits (per-caliper edge hits) and the derived fit fields are
  // inspection OUTPUT, not def configuration -- persisting them bloats the def
  // file and churns the def hash on every inspection. Clone (shallow is enough;
  // these are all top-level shape keys) so the live editor shapes keep them for
  // on-canvas display.
  {
    const STRIP = ['cal_hits', '_pt1', '_pt2', 'adj_pt1', 'inspection_status', 'inspection_value'];
    const feats = report.featureSet[0].features;
    if (Array.isArray(feats)) {
      report.featureSet[0].features = feats.map((s) => {
        const c = { ...s };
        for (const k of STRIP) delete c[k];
        return c;
      });
    }
  }

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
