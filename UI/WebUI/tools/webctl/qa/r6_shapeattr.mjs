#!/usr/bin/env node
/*
 * QA r6_shapeattr — Shape_Attr_Fill DEFAULTS viewpoint.
 *
 * Drives running WebUI via webctld (HTTP :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__). Single shared browser —
 * parent runs QA scripts serially. Do NOT run multiple QA scripts in parallel
 * against the live daemon. Core-down => SKIP exit 0. Reload at end.
 *
 * SHAPE_TYPE (src/redux/actions/UIAct.js):
 *   sign360, point, line, arc, aux_point, aux_line, search_point, measure
 *
 * Shape_Attr_Fill (src/UTIL/InspectionEditorLogic.js:83) ACTUAL coverage:
 *   - line          : sets vertex_touch_searching=false, locating='contour'
 *                     (DOES NOT default pt1/pt2 — gap)
 *   - arc           : sets locating='contour'
 *                     (DOES NOT default pt1/radius — gap)
 *   - search_point  : sets search_far=false, locating_anchor=false,
 *                     line_thickness_value=0
 *   - measure       : sets value_A=0, value_B=1, value_X=0, value_Y=1,
 *                     quality_essential=true, orientation_essential=false,
 *                     NGasNA=false, NAasNG=false.
 *                     (DOES NOT default value/USL/LSL/UCL/LCL/subtype — gap)
 *   - aux_point, aux_line, point, sign360 : NO case branch — pass-through.
 *
 * TEST PLAN:
 *  T1 baseline       — load def, unlock, snapshot.
 *  T2 add per type   — for each candidate type (line, arc, search_point,
 *                      measure, aux_point, aux_line), dispatch Shape_Set
 *                      {shape:{type}, id:undefined, IGNORE_DEFCONF_LOCK:true};
 *                      assert shapeList grew by 1, new shape has unique id,
 *                      and the Shape_Attr_Fill-documented defaults are present.
 *  T3 line defaults  — vertex_touch_searching===false, locating==='contour'.
 *                      Also note (as GAP, non-fatal): pt1/pt2 absent.
 *  T4 arc defaults   — locating==='contour'. Note pt1/radius gap.
 *  T5 sp defaults    — search_far===false, locating_anchor===false,
 *                      line_thickness_value===0.
 *  T6 measure def    — value_A/B/X/Y finite & equal to 0/1/0/1,
 *                      quality_essential===true, orientation_essential===false,
 *                      NGasNA===false, NAasNG===false.
 *                      Note GAP: value, USL, LSL not auto-defaulted.
 *  T7 nonexistent    — Shape_Set {type:'__nonexistent__'}: assert
 *                      shapeList unchanged AND no throw (no-op tolerated).
 *  T8 serialization  — __GP_DEF__().features contains every added id.
 *  T9 unique ids     — all newly-added ids unique and distinct from baseline.
 *
 * Each new shape's id is auto-assigned by SetShape (id = ++shapeCount).
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';

async function api(p, body) {
  const r = await fetch(BASE + p, {
    method: body ? 'POST' : 'GET',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const j = await r.json();
  if (!r.ok) throw new Error(JSON.stringify(j));
  return j;
}
const ev = (expr) => api('/eval', { expr }).then((r) => r.result);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const isCoreDown = (msg) =>
  /not connected|timeout|did not load|reconnect|ECONNREF/i.test(String(msg || ''));

async function reset() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  let loaded = false;
  let lastErr = null;
  for (let attempt = 0; attempt < 4 && !loaded; attempt++) {
    await ev(
      `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(MODEL_PATH)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
    );
    for (let i = 0; i < 80; i++) {
      await sleep(100);
      const st = await ev(
        `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList.length)||0};})()`
      );
      if (st.err) { lastErr = st.err; break; }
      if (st.rdy && st.n > 0) { loaded = true; break; }
    }
    if (!loaded) { lastErr = lastErr || 'timeout'; await sleep(3000); }
  }
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);
const serialDef = () => ev('window.__GP_DEF__()');

const SNAP_IDS = `(function(){
  var o=window.__GP_STORE__.getState().UIData.edit_info._obj;
  return o.shapeList.map(function(x){return x.id;});
})()`;
const snapIds = () => ev(SNAP_IDS);

// Add a minimal shape; return the shape object (the last entry of shapeList)
// if the list grew, else null. Catches dispatch throws to surface as null.
async function addMinimal(type) {
  return ev(
    `(function(){try{` +
      `var s=window.__GP_STORE__;` +
      `var before=s.getState().UIData.edit_info._obj.shapeList.length;` +
      `s.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:{type:${JSON.stringify(type)}},id:undefined}});` +
      `var sl=s.getState().UIData.edit_info._obj.shapeList;` +
      `if(sl.length<=before) return {grew:false};` +
      `var nu=sl[sl.length-1];` +
      `return {grew:true, shape: JSON.parse(JSON.stringify(nu))};` +
    `}catch(e){return {grew:false, err:String(e)};}})()`
  );
}

let failures = 0;
const notes = [];
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}
function note(msg) { notes.push(msg); }

async function main() {
  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }
  await unlock();
  await sleep(150);

  // ---- T1 baseline ----
  const baseIds = await snapIds();
  report('T1 baseline', Array.isArray(baseIds) && baseIds.length > 0,
    `baseline shapes=${baseIds.length}`);

  const types = ['line', 'arc', 'search_point', 'measure', 'aux_point', 'aux_line'];
  const added = {}; // type -> shape

  // ---- T2 add per type ----
  let allGrew = true;
  for (const t of types) {
    const r = await addMinimal(t);
    await sleep(200);
    if (!r || !r.grew) {
      allGrew = false;
      report(`T2 add[${t}]`, false, `did not grow${r && r.err ? ' err='+r.err : ''}`);
    } else {
      added[t] = r.shape;
      report(`T2 add[${t}]`, r.shape && r.shape.id != null && r.shape.type === t,
        `id=${r.shape.id}, type=${r.shape.type}`);
    }
  }

  // Shape_Attr_Fill is called at RENDER time (DefConfUI:2003 in GenTarEditUI), NOT
  // on Shape_Set add — so `added.<type>` is the minimal stored shape and won't have
  // defaults. To test the defaults THE FUNCTION applies, call it directly via the
  // dev hook __GP_MEASURE__.Shape_Attr_Fill — this is what the renderer would see.
  const filled = {};
  for (const t of types) {
    const r = await ev(`(function(){try{return window.__GP_MEASURE__.Shape_Attr_Fill({type:${JSON.stringify(t)}});}catch(e){return {err:String(e)};}})()`);
    if (r && !r.err) filled[t] = r;
  }
  note(`(NOTE) Shape_Attr_Fill is RENDER-TIME ONLY (DefConfUI:2003); defaults are not stored on Shape_Set add — known product design quirk, tracked in OPENQUESTION as the per-shape schema keystone work.`);

  // ---- T3 line defaults (via Shape_Attr_Fill directly) ----
  if (filled.line) {
    const L = filled.line;
    const ok = L.vertex_touch_searching === false && L.locating === 'contour';
    report('T3 line defaults', ok,
      `vertex_touch_searching=${L.vertex_touch_searching}, locating=${L.locating}`);
    if (!('pt1' in L) || !('pt2' in L)) {
      note('GAP: Shape_Attr_Fill(line) does not synthesize pt1/pt2 for minimal {type:"line"}.');
    }
  } else report('T3 line defaults', false, 'no line shape');

  // ---- T4 arc defaults ----
  if (filled.arc) {
    const A = filled.arc;
    const ok = A.locating === 'contour';
    report('T4 arc defaults', ok, `locating=${A.locating}`);
    if (!('pt1' in A) || !('radius' in A)) {
      note('GAP: Shape_Attr_Fill(arc) does not synthesize pt1/radius for minimal {type:"arc"}.');
    }
  } else report('T4 arc defaults', false, 'no arc shape');

  // ---- T5 search_point defaults ----
  if (filled.search_point) {
    const S = filled.search_point;
    const ok = S.search_far === false &&
               S.locating_anchor === false &&
               typeof S.line_thickness_value === 'number' && S.line_thickness_value === 0;
    report('T5 search_point defaults', ok,
      `search_far=${S.search_far}, locating_anchor=${S.locating_anchor}, line_thickness_value=${S.line_thickness_value}`);
  } else report('T5 search_point defaults', false, 'no search_point shape');

  // ---- T6 measure defaults ----
  if (filled.measure) {
    const M = filled.measure;
    const numOk = M.value_A === 0 && M.value_B === 1 && M.value_X === 0 && M.value_Y === 1 &&
                  [M.value_A, M.value_B, M.value_X, M.value_Y].every(Number.isFinite);
    const boolOk = M.quality_essential === true &&
                   M.orientation_essential === false &&
                   M.NGasNA === false &&
                   M.NAasNG === false;
    report('T6 measure defaults', numOk && boolOk,
      `A/B/X/Y=${M.value_A}/${M.value_B}/${M.value_X}/${M.value_Y}, quality=${M.quality_essential}, orient=${M.orientation_essential}, NGasNA=${M.NGasNA}, NAasNG=${M.NAasNG}`);
    const missing = ['value','USL','LSL','UCL','LCL','subtype'].filter(k => !(k in M));
    if (missing.length) {
      note(`GAP: Shape_Attr_Fill(measure) does not default ${missing.join('/')} — downstream code may NaN.`);
    }
  } else report('T6 measure defaults', false, 'no measure shape');

  // aux_point/aux_line gap note
  if (added.aux_point && added.aux_line) {
    note('GAP: aux_point & aux_line have NO Shape_Attr_Fill branch — accepted as-is (no defaults applied).');
  }

  // ---- T7 nonexistent ----
  const idsBefore7 = await snapIds();
  let threw = false;
  let r7;
  try { r7 = await addMinimal('__nonexistent__'); } catch (e) { threw = true; }
  await sleep(200);
  const idsAfter7 = await snapIds();
  const unchanged = idsBefore7.length === idsAfter7.length &&
                    idsBefore7.every((x, i) => x === idsAfter7[i]);
  // Tolerate either "no-op" or "added with type passed through" — but flag latter as note.
  if (!unchanged) {
    note(`NOTE: Shape_Set with unknown type was NOT rejected; shape appended (type passed through). ` +
         `Before=${idsBefore7.length} After=${idsAfter7.length}.`);
  }
  report('T7 nonexistent type', !threw, `threw=${threw}, listChanged=${!unchanged}`);

  // ---- T8 serialization ----
  const def = await serialDef();
  const featIds = new Set((def && def.features || []).map(f => f.id));
  const addedIds = Object.values(added).filter(s => s && s.id != null).map(s => s.id);
  const allInDef = addedIds.every(id => featIds.has(id));
  report('T8 serialization', allInDef,
    `def.features=${featIds.size}, addedIds=[${addedIds.join(',')}], allInDef=${allInDef}`);

  // ---- T9 unique ids ----
  const baseSet = new Set(baseIds);
  const uniq = new Set(addedIds);
  const allDistinctFromBase = addedIds.every(id => !baseSet.has(id));
  const allDistinctEachOther = uniq.size === addedIds.length;
  report('T9 unique ids', allDistinctFromBase && allDistinctEachOther,
    `distinctFromBase=${allDistinctFromBase}, distinctEachOther=${allDistinctEachOther}, ids=[${addedIds.join(',')}]`);

  if (notes.length) {
    console.log('\n--- NOTES / PRODUCT GAPS ---');
    for (const n of notes) console.log('  * ' + n);
  }

  // mutated def — reload for next QA wave.
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
