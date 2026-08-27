import JSum from 'jsum';
import { seedCaliper, seedEdge, arcSagittaPx, ARC_MIN_SAGITTA_PX,
         SEARCH_POINT_EDGE_SEED } from '../shapes/_caliperSeed';



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
import { BACK_SIDE_LIMITS_ENABLED, stripBackSideLimits } from 'UTIL/backSideLimits';
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

// Everything on a shape that is inspection OUTPUT rather than def
// configuration. One list, because it answers two questions that must never be
// answered differently:
//
//   * what gets stripped before the def is hashed and saved -- persisting a
//     per-frame result bloats the file and churns the def hash every inspection
//   * what gets IGNORED when deciding whether the user has edited the def since
//     the last run (the cal_hits staleness check in UICtrlReducer)
//
// The staleness check used to be a WHITELIST of seven keys, and had already
// drifted: width, angleDeg, search_far, ref and the arc's direction/fit_mode
// all change the search band and none of them were listed. Rotating a search
// point 90 degrees after a run left the old hits on screen, pinned to the new
// box, reading as fresh confirmation.
//
// A whitelist has to be updated every time a field is added and is silently
// wrong until somebody notices. A blacklist of RESULTS is bounded and obvious:
// a new def field is covered the day it is added, and a new result field fails
// loudly by making everything look stale.
export const SHAPE_RESULT_KEYS = [
  'cal_hits', '_pt1', '_pt2', 'adj_pt1', 'inspection_status', 'inspection_value',
  // added 2026-08-26 with the aux-point work -- the core's reported position
  // and its NA reason are results, not configuration
  'reported_pt', 'na_reason',
];

// A shape with the per-frame results removed: what the def carries, and what
// "has the def changed" must be asked about.
export function shapeDefProjection(s) {
  const c = { ...s };
  for (const k of SHAPE_RESULT_KEYS) delete c[k];
  return c;
}

// Key-ORDER-independent, because JSON.stringify is not: two shapes with the
// same content built in a different order would stringify differently and read
// as an edit that never happened, throwing away hits for no reason.
export function shapeDefFingerprint(s) {
  const stable = (v) => {
    if (Array.isArray(v)) return v.map(stable);
    if (v && typeof v === 'object') {
      const out = {};
      for (const k of Object.keys(v).sort()) out[k] = stable(v[k]);
      return out;
    }
    return v;
  };
  return JSON.stringify(stable(shapeDefProjection(s)));
}

// The inherentfeatures entry carrying the trained line2Dup feature set. Named
// and numbered like @__SIGNATURE__ (100000..100006), clear of it.
export const SBM_INFO_NAME = '@__SBM_INFO__';
export const SBM_INFO_ID = 100100;

export function defFileGeneration(edit_info)
{

  let feature_sig360_circle_line = edit_info._obj.GenerateFeature_sig360_circle_line();
  let preloadedDefFile = edit_info.loadedDefFile;
  if (preloadedDefFile === undefined) preloadedDefFile = {};
  let report = {
    ...preloadedDefFile,
    type: "binary_processing_group",
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
  // (top-level) root (FeatureManager_group.cpp)
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
  // loc_include / loc_exclude are localization regions, never measurement
  // features -- strip them from features[] whatever the locator is.
  //
  // This used to happen only in the shape_based branch below, so a def whose
  // locating_engine was anything else shipped them to the core inside
  // features[]. sig360's parser rejects an unknown feature type outright, and
  // it fails the WHOLE def: "feature[7] has unknown type:[loc_include]" then
  // "cJSON parse failed", leaving the engine with no features at all --
  // inspection then returns in microseconds and judges everything NA.
  //
  // It is not a corner case: loading a def re-creates these shapes from
  // localization_include/exclude (InspectionEditorLogic), so any def that has
  // ever used the shape locator carries them in the editor forever, and breaks
  // the moment locating_engine is not shape_based. The saved file looks clean,
  // because saving under shape_based did strip them -- which is why the file on
  // disk and the def pushed over the wire could disagree.
  const _featsAll = Array.isArray(report.featureSet[0].features)
    ? report.featureSet[0].features : [];
  const _locIncl = _featsAll.filter((s) => s && s.type === 'loc_include');
  const _locExcl = _featsAll.filter((s) => s && s.type === 'loc_exclude');
  if (_locIncl.length || _locExcl.length) {
    report.featureSet[0].features = _featsAll.filter(
      (s) => s && s.type !== 'loc_include' && s.type !== 'loc_exclude');
  }

  if (edit_info.locating_engine === 'shape_based') {
    report.featureSet[0].locating_engine = 'shape_based';
    // Caliper-only: the raw-gray shape path has no contour, so force every line/arc
    // primitive to caliper locating (seed caliper/edge defaults if the user never
    // opened it). Covers primitives the property sheet's force-on-open didn't touch.
    if (Array.isArray(report.featureSet[0].features)) {
      report.featureSet[0].features = report.featureSet[0].features.map((s) => {
        // SEARCH POINTS CONVERT TOO, and forgetting them is not a small gap.
        //
        // A contour-mode search point scans an edge_grid, and the shape_based
        // path never builds one -- so it returns NA, with no reason attached,
        // for every part forever. Measured on a real migration (test2 -> SBM):
        // localization perfect (similarity 1.000, origin to 4 decimals), all 4
        // lines SUCCESS with 10 hits each and confidence 86.5, and all 8 search
        // points NA, taking 4 of the 7 judgements down with them. The def trains,
        // loads, locates, and measures nothing that depends on a point.
        if (s && s.type === 'search_point' && s.locating !== 'caliper') {
          const c = { ...s, locating: 'caliper' };
          if (!c.edge) c.edge = { ...SEARCH_POINT_EDGE_SEED };
          // min_strength is required in caliper mode; a def carrying an edge
          // block without it is NA'd by name. Fill only that.
          else if (typeof c.edge.min_strength !== 'number')
            c.edge = { ...c.edge, min_strength: SEARCH_POINT_EDGE_SEED.min_strength };
          return c;
        }
        if (!s || (s.type !== 'line' && s.type !== 'arc') || s.locating === 'caliper') return s;
        // An arc taught nearly collinear does not get converted, it gets LEFT in
        // contour mode -- which the core then refuses under shape_based, by name.
        // That refusal is the point. Converting it produces a number rather than
        // an error: measured on BCG-20X40X53 [13][1], caliper returns r=0.49mm
        // against contour's 0.20mm, and its neighbour returns SUCCESS with a
        // radius half again too big. A wrong radius that passes is worse than a
        // def that will not train, because only one of the two gets noticed.
        // Small sagitta <=> distant circumcentre <=> the search rays run nearly
        // parallel instead of fanning around the bend; see _caliperSeed.
        if (s.type === 'arc') {
          const sag = arcSagittaPx(s, report.featureSet[0].mmpp);
          if (sag !== null && sag < ARC_MIN_SAGITTA_PX) {
            console.warn('[def] arc id=' + s.id + ' (' + (s.name || '') + ') taught sagitta ' +
              sag.toFixed(1) + 'px -- left in contour mode; it needs re-teaching, not converting');
            return s;
          }
        }
        const c = { ...s, locating: 'caliper' };
        // seedCaliper/seedEdge, never a local copy: this path and the offline
        // converter must produce the same def, and this is the path a migration
        // actually takes -- saving under shape_based converts every primitive
        // without the user opening one. The copy that used to sit here measured
        // an ARC's caliper width from the CHORD (arcSweep understates by more
        // the tighter the bend) and searched every shape `falling` (an arc
        // usually measures an inner radius, where falling takes the wrong side
        // of the wire: -0.11mm on every R1.0 in the corpus). Both were fixed in
        // _caliperSeed and measured there; this call site never followed.
        if (!c.caliper) c.caliper = seedCaliper(s);
        if (!c.edge) c.edge = seedEdge(s);
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
    // Already pulled out of features[] above, whatever the locator.
    const inclShapes = _locIncl;
    const exclShapes = _locExcl;
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
  // def_image_reg LIVES IN featureSet[0] as of 2026-08-26. It used to sit at the
  // def top level, and that cost two things.
  //
  // ONE: the sub-feature parser only sees featureSet[i], so a helper existed
  // purely to copy it down (def_stamp_context), called from four places. Missing
  // the call does not fail -- `has_reg` goes false and the object origin falls
  // back to the Otsu interior-blob centroid, silently relocating the frame every
  // measurement in the def is expressed in.
  //
  // TWO, and this is what decided it: featureSet_sha1 hashes `featureSet` only,
  // so a registration change did not change the def hash. The save-conflict
  // check could not see a reg-only edit, and subFeatureDefSha1 -- which rides
  // every inspection report into the database -- was identical either side of a
  // change of coordinate system. A def whose reg moved IS a different recipe and
  // the hash said it was not. Written here, BEFORE the digest, it is one.
  //
  // The top-level key is deleted rather than mirrored: two places holding the
  // same value is how they come to disagree. Old defs still load -- the reader
  // prefers the sub-feature copy and falls back to the root -- and old cores
  // still work, because def_stamp_context only stamps when the sub-feature
  // lacks it.
  if (edit_info.def_image_reg) {
    report.featureSet[0].def_image_reg = edit_info.def_image_reg;
    delete report.def_image_reg;
  }

  // Strip transient per-frame inspection RESULTS from the shapes before they get
  // hashed/saved. cal_hits (per-caliper edge hits) and the derived fit fields are
  // inspection OUTPUT, not def configuration -- persisting them bloats the def
  // file and churns the def hash on every inspection. Clone (shallow is enough;
  // these are all top-level shape keys) so the live editor shapes keep them for
  // on-canvas display.
  {
    const feats = report.featureSet[0].features;
    if (Array.isArray(feats)) {
      report.featureSet[0].features = feats.map(shapeDefProjection);
    }
  }

  // BEFORE the hash, deliberately. featureSet_sha1 identifies the def the core
  // is actually given; stripping after it would make the hash describe a
  // document that was never sent. A def carrying _b keys therefore hashes
  // differently while the feature is off -- correct, because it IS a different
  // def now, and that makes the change visible rather than silent.
  if (!BACK_SIDE_LIMITS_ENABLED) stripBackSideLimits(report.featureSet);
  // THE TRAINED SBM FEATURES, beside the sig360 signature.
  //
  // Both are the trained representation of the part -- one entry per locator --
  // so they live in the same array, in the same shape. It used to be a
  // top-level `__shape_cache` key added AFTER the digest, deliberately, so a
  // def with a cache and one without hashed alike.
  //
  // It is now INSIDE the hash, and that is a choice, not an accident:
  //   * what the machine matches against is part of the recipe, and the
  //     signature is hashed for exactly that reason;
  //   * an entry in inherentfeatures that is NOT hashed while its siblings are
  //     is a subtlety somebody trips over later;
  //   * it is the same argument that moved def_image_reg into featureSet[0].
  // The cost is that regenerating features counts as a def revision. To undo,
  // move this block below the digest -- but then read the second bullet again.
  //
  // COPIED, never appended in place: inherentfeatures is the live
  // inherentShapeList off the editor object, and pushing into it would grow the
  // list by one entry on every save.
  if (edit_info.__shape_cache) {
    const _inh = Array.isArray(report.featureSet[0].inherentfeatures)
      ? report.featureSet[0].inherentfeatures : [];
    report.featureSet[0].inherentfeatures = _inh
      .filter((e) => !(e && e.name === SBM_INFO_NAME))
      .concat([{ id: SBM_INFO_ID, type: 'sbm_info', name: SBM_INFO_NAME,
                 shape_cache: edit_info.__shape_cache }]);
  }
  // The legacy placement is removed rather than mirrored: two copies of one
  // value is how they come to disagree, and the core prefers the new one
  // anyway.
  delete report.featureSet[0]["__shape_cache"];

  let sha1_info_in_json = JSum.digest(report.featureSet, 'sha1', 'hex');
  // AFTER the digest, and it has to stay there. __decorator is per-session UI
  // state, not recipe, so folding it into the hash would make the same def read
  // as a different one every time somebody changed a display preference.
  //
  // The trained SBM features used to sit down here for the same reason; they
  // are now an inherentfeatures entry written ABOVE, and hashed -- see the note
  // there for why that changed.
  report.featureSet[0]["__decorator"] = edit_info.__decorator;

  // A def that ARRIVED with trained features and is leaving without them is
  // losing them.
  //
  // That is not hypothetical: between 2026-08 and this note the write existed
  // and nothing read the key back on load, so the features survived one save
  // and every later open-and-save dropped them silently. The only symptom is
  // that the core falls back to sig360 -- which still locates, so nothing looks
  // wrong until it does.
  if (!edit_info.__shape_cache) {
    const _prev = edit_info.loadedDefFile
      && edit_info.loadedDefFile.featureSet
      && edit_info.loadedDefFile.featureSet[0];
    const _had = _prev && (_prev.__shape_cache
      || (Array.isArray(_prev.inherentfeatures)
          && _prev.inherentfeatures.some((e) => e && e.name === SBM_INFO_NAME)));
    if (_had)
      console.error('defFileGeneration: this def arrived with trained SBM features '
        + 'and is being saved without them -- the loader is not carrying them. '
        + 'The core will fall back to sig360.');
  }

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
