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




// Stamp the reference-image FULL path onto a def-INFO object before sending it to the
// core for inspection (live/WS path). The saved .hydef stays path-free; the core reads
// "_ref_image_path" (highest priority) to train the shape locator without guessing the
// sidecar name. Path = <defModelPath>.png (the sidecar written next to the def on save).
// No-op for unsaved defs (no defModelPath) and harmless for sig360 defs (ignored).
export function stampRefImagePath(deffile, edit_info) {
  if (deffile && deffile.featureSet && deffile.featureSet[0] && edit_info && edit_info.defModelPath) {
    deffile.featureSet[0]._ref_image_path = String(edit_info.defModelPath).replace(/\.[^.]+$/, '') + '.png';
  }
  return deffile;
}

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
  // line2Dup edge-strength thresholds for feature generation (core defaults 50/80).
  if (typeof edit_info.shape_weak_thres === 'number')
    report.featureSet[0].shape_weak_thres = edit_info.shape_weak_thres;
  if (typeof edit_info.shape_strong_thres === 'number')
    report.featureSet[0].shape_strong_thres = edit_info.shape_strong_thres;
  // Localizer: shape_based opts into the line2Dup + ROI-refine locator. The reference
  // image is NOT a def-file field (kept path-free / portable): the core gets its path
  // at runtime via "_ref_image_path" (stampRefImagePath, stamped into the def-INFO the
  // WebUI sends), or derives <def>.png from the def path on the --insp path.
  if (edit_info.locating_engine === 'shape_based') {
    report.featureSet[0].locating_engine = 'shape_based';
    // Caliper-only: the raw-gray shape path has no contour, so force every line/arc
    // primitive to caliper locating (seed caliper/edge defaults if the user never
    // opened it). Covers primitives the property sheet's force-on-open didn't touch.
    if (Array.isArray(report.featureSet[0].features)) {
      report.featureSet[0].features = report.featureSet[0].features.map((s) => {
        if (!s || (s.type !== 'line' && s.type !== 'arc') || s.locating === 'caliper') return s;
        const c = { ...s, locating: 'caliper' };
        if (!c.caliper) {
          const len = (s.pt1 && s.pt2) ? Math.hypot(s.pt2.x - s.pt1.x, s.pt2.y - s.pt1.y) : 0;
          c.caliper = { count: 10, width: len > 0 ? len / 10 : 0.1, min_inliers: 5, max_error: 0.1 };
        }
        if (!c.edge) c.edge = { method: 'strongest', polarity: 'falling', nth: 0, min_strength: 60 };
        return c;
      });
    }
    // Pure-SBM feature-extraction region (object-frame mm polygons). The user authors
    // them as loc_include / loc_exclude SHAPES on the canvas (same object-frame mm as
    // every other shape, so the points need no transform). Pull them out of the
    // measurement-feature list and emit them as the def's localization_include /
    // localization_exclude. Falls back to baking the sig360 silhouette into include
    // (migration default) when the user authored no include region. The signature is
    // already mm, so a bin (R mm, theta rad) -> {x:R*cos, y:R*sin}: the pixel->unit
    // step that makes the locator magnification-portable and sig360-independent.
    const feats = Array.isArray(report.featureSet[0].features) ? report.featureSet[0].features : [];
    const inclShapes = feats.filter((s) => s && s.type === 'loc_include');
    const exclShapes = feats.filter((s) => s && s.type === 'loc_exclude');
    if (inclShapes.length || exclShapes.length) {
      // Localization config is NOT a measurement feature — strip from features[].
      report.featureSet[0].features = feats.filter(
        (s) => s && s.type !== 'loc_include' && s.type !== 'loc_exclude');
    }
    const toPolys = (shapes) => shapes
      .map((s) => (Array.isArray(s.points) ? s.points.map((p) => ({ x: p.x, y: p.y })) : []))
      .filter((p) => p.length >= 3);
    const inclPolys = toPolys(inclShapes);
    const exclPolys = toPolys(exclShapes);

    if (inclPolys.length) {
      report.featureSet[0].localization_include = inclPolys;
    } else {
      const inh = report.featureSet[0].inherentfeatures;
      const sig = inh && inh[0] && inh[0].signature;
      if (sig && Array.isArray(sig.magnitude) && Array.isArray(sig.angle)) {
        const r5 = (v) => Math.round(v * 100000) / 100000;   // match signature precision
        const poly = [];
        const n = Math.min(sig.magnitude.length, sig.angle.length);
        for (let i = 0; i < n; i++) {
          const R = sig.magnitude[i], t = sig.angle[i];
          if (R > 1e-4) poly.push({ x: r5(R * Math.cos(t)), y: r5(R * Math.sin(t)) });
        }
        if (poly.length >= 3) report.featureSet[0].localization_include = [poly];
      }
    }
    if (exclPolys.length) report.featureSet[0].localization_exclude = exclPolys;
    // ROI-refine sample points (object-frame mm). ALWAYS emitted for shape_based (even
    // []) so the core treats the def as explicit: it uses exactly these, and an empty
    // list means NO ROI refine (coarse only). "自動產生 ROI 點" fills it from the core.
    report.featureSet[0].roi_refine_points =
      Array.isArray(edit_info.roi_refine_points) ? edit_info.roi_refine_points : [];
  }
  // Preserve def_image_reg (the shape locator's registered pose) across RE-saves of an
  // existing def -- the save flow only writes it fresh on a NEW save (!existed), so
  // without this a migrate+resave of an existing def would drop it. New-save still
  // overwrites with the freshly-detected pose.
  if (edit_info.def_image_reg) report.def_image_reg = edit_info.def_image_reg;

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
