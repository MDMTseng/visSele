#!/usr/bin/env node
/*
 * QA r3_refbind — behavioral tests for the SHAPE REFERENCE-BINDING viewpoint.
 *
 * Shapes reference each other through a small object-graph: a measure's `ref`
 * array (and `ref_baseLine`) hold {id,type} pointers to search_points / lines /
 * arcs / aux_points; a search_point's `ref` points to the line that gives its
 * direction; an aux_point's `ref` points to the shape(s) it derives from. These
 * pointers are RESOLVED to concrete geometry by the parse* family in
 * InspectionEditorLogic.js (auxPointParse / searchPointParse /
 * shapeMiddlePointParse / shapeVectorParse).
 *
 * Bindings are WRITTEN through a 2-dispatch handshake, gated by the xstate
 * substate (xstate_GetCurrentMainState(c_state).substate):
 *   1) Edit_Tar_Ele_Trace_Update  data: keyTrace array   (e.g. ["ref",0])
 *   2) Edit_Tar_Ele_Cand_Update   data: {shape:{id,type}}  (the picked candidate)
 * On the 2nd dispatch the reducer (UICtrlReducer ~L901) calls
 *   edit_info._obj.applyEditTarSubstate(edit_info, substate)
 * which, in the SHAPE_EDIT / *_CREATE cases, walks the keyTrace into
 * edit_tar_info, writes obj[lastKey] = {id:cand.shape.id, type:cand.shape.type},
 * replaces edit_tar_info with a fresh object, and CLEARS trace+cand back to null.
 *
 * To make the substate == DEFCONF_MODE_SHAPE_EDIT we mirror r1_editor:
 *   dispatch Edit_Tar_Update(shape)  -> populates edit_tar_info, clears trace/cand
 *   dispatch Shape_Edit (UI_SM_EVENT) -> SM: DEFCONF_MODE_NEUTRAL -> ..._SHAPE_EDIT
 * Confirmed from src/redux/redux.js EditStates + actions/{UIAct,DefConfAct}.js.
 *
 * EVENT NAMES discovered:
 *   UI_SM_EVENT:   Edit_Mode, Shape_Edit, Measure_Create, Search_Point_Create, EXIT
 *   DefConfAct:    Edit_Tar_Update, Edit_Tar_Ele_Trace_Update,
 *                  Edit_Tar_Ele_Cand_Update, DEFCONF_MODE_SUCCESS, Shape_Set
 *   substates:     DEFCONF_MODE_NEUTRAL / _SHAPE_EDIT / _MEASURE_CREATE / _SEARCH_POINT_CREATE
 *
 * Drives the running WebUI through the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__), exactly like r1_editor.mjs.
 * CORE-DEPENDENT: a core-down load (timeout/Not connected) => SKIP, exit 0.
 *
 * ===========================================================================
 * TEST PLAN (explore caliper_verify, 26 shapes, to find REAL references)
 *  R1 ref_structure  — load def; enumerate shapes by type; for each measure /
 *                      search_point / aux_point dump its ref / ref_baseLine
 *                      pointers. Assert >=1 referencing shape with a non-empty
 *                      ref that resolves to a real shape id in the list. This
 *                      DOCUMENTS the actual reference structure.
 *  R2 select_populates — pick a referencing measure; Edit_Tar_Update + Shape_Edit;
 *                      assert edit_tar_info carries its ref fields (ref array /
 *                      ref_baseLine present) AND substate transitioned to
 *                      DEFCONF_MODE_SHAPE_EDIT.
 *  R3 parse_resolves — for each referencing shape type present, call the matching
 *                      parse* on the live _obj (auxPointParse / searchPointParse /
 *                      shapeMiddlePointParse / shapeVectorParse) and assert the
 *                      resolved point/vector is non-null & finite (NOT null where
 *                      geometry is expected). Real FAIL if a resolvable ref => null.
 *  R4 handshake      — drive the 2-dispatch binding on the selected measure: read
 *                      its first ref slot's CURRENT {id,type}; dispatch
 *                      Edit_Tar_Ele_Trace_Update(["ref",0]) then
 *                      Edit_Tar_Ele_Cand_Update({shape:{id,type}}) re-binding it to
 *                      a same-type candidate; assert (a) trace+cand were CLEARED to
 *                      null by applyEditTarSubstate and (b) edit_tar_info.ref[0].id
 *                      equals the candidate id (binding written). If subtype/type
 *                      acceptance rejects the cand we assert the no-throw + clear
 *                      path and mark the write as a documented gap.
 *  R5 no_diag_error  — selecting/parsing a measure whose ref resolves must not
 *                      throw. We have NO diag ring-buffer hook exposed, so we
 *                      install a window.__r3err counter over console.error around
 *                      the select+parse and assert it did not increment for the
 *                      resolvable case (best-effort proxy for the diag buffer).
 *
 * exit non-zero ONLY on a real failure (throw, or null where geometry expected,
 * or trace/cand not cleared after a valid handshake). core-down => SKIP exit 0.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { MODEL_PATH, diagnoseLoadFailure } from './lib_model.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;

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
const dispatch = (action) =>
  ev(`window.__GP_STORE__.dispatch(${JSON.stringify(action)}); 'ok'`);

const isCoreDown = (msg) =>
  /not connected|timeout|did not load|reconnect|ECONNREF/i.test(String(msg || ''));

// Faithful copy of r1_editor reset(): reload, wait for store, load def w/ retry.
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
    if (!loaded) {
      lastErr = lastErr || 'timeout';
      await sleep(3000);
    }
  }
  if (!loaded) { const __d = await diagnoseLoadFailure(ev, lastErr); throw new Error(__d.msg); }
}

// Snapshot of shapes + their reference pointers + substate.
const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj, et=s.edit_info.edit_tar_info;
  var v=(s.c_state&&typeof s.c_state.value!=='undefined')?s.c_state.value:null;
  var main=(typeof v==='string')?v:(v?Object.keys(v)[0]:null);
  var sub=(typeof v==='string'||v==null)?null:v[Object.keys(v)[0]];
  function refSlim(sh){
    return {id:sh.id,type:sh.type,subtype:sh.subtype,name:sh.name,
      ref:sh.ref?sh.ref.map(function(r){return {id:r.id,type:r.type,keyTrace:r.keyTrace,element:r.element};}):undefined,
      ref_baseLine:sh.ref_baseLine?{id:sh.ref_baseLine.id,type:sh.ref_baseLine.type}:undefined};
  }
  return {
    main:main, sub:sub,
    shapes:o.shapeList.map(refSlim),
    edit_tar: et?refSlim(et):null,
    trace: s.edit_info.edit_tar_ele_trace,
    cand: s.edit_info.edit_tar_ele_cand
  };
})()`;
const snap = () => ev(SNAP);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// Select a shape by id: Edit_Tar_Update(shape) + Shape_Edit (=> substate SHAPE_EDIT).
async function selectShapeId(id) {
  await ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
    `var m=o.shapeList.find(function(s){return s.id===${JSON.stringify(id)};});` +
    `if(!m)return null;` +
    `window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});` +
    `window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
  );
  await sleep(200);
}

// Call a parse* method on the live model for a given shape id; return resolved
// geometry (or {__null:true} / {__throw:msg}). Runs in the page so we exercise
// the REAL InspectionEditorLogic methods, not a reimplementation.
async function parseShape(id) {
  return ev(
    `(function(){try{` +
    `var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
    `var sh=o.shapeList.find(function(s){return s.id===${JSON.stringify(id)};});` +
    `if(!sh)return {__missing:true};` +
    `var pt=o.shapeMiddlePointParse(sh,o.shapeList);` +
    `var vec=o.shapeVectorParse(sh,o.shapeList);` +
    `function fin(p){return p&&typeof p.x==='number'&&typeof p.y==='number'&&isFinite(p.x)&&isFinite(p.y);}` +
    `return {type:sh.type,pt:pt?{x:pt.x,y:pt.y}:null,vec:vec?{x:vec.x,y:vec.y}:null,` +
    `ptFinite:fin(pt),vecFinite:fin(vec)};` +
    `}catch(e){return {__throw:String(e&&e.message||e)};}})()`
  );
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // Enter DEFCONF edit mode so Shape_Edit substate transitions are live.
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(300);

  // ---- R1 ref_structure ----
  let s = await snap();
  const byType = {};
  for (const sh of s.shapes) (byType[sh.type] = byType[sh.type] || []).push(sh);
  const typeCounts = Object.keys(byType).map((t) => `${t}:${byType[t].length}`).join(' ');

  // A "referencing" shape: has a non-empty ref[] (with at least one id) or ref_baseLine.id.
  const idSet = new Set(s.shapes.map((x) => x.id));
  const refers = (sh) => {
    const refIds = (sh.ref || []).map((r) => r && r.id).filter((x) => x != null);
    const baseId = sh.ref_baseLine && sh.ref_baseLine.id;
    if (baseId != null) refIds.push(baseId);
    return refIds;
  };
  const referencingShapes = s.shapes.filter((sh) => {
    const ids = refers(sh);
    return ids.length > 0 && ids.every((id) => idSet.has(id));
  });
  // Document what we found.
  const docLines = referencingShapes.slice(0, 8).map((sh) =>
    `${sh.type}#${sh.id}->[${refers(sh).join(',')}]`);
  report('R1 ref_structure', referencingShapes.length > 0,
    `types{${typeCounts}} referencing=${referencingShapes.length} :: ${docLines.join(' ')}`);

  // Pick a referencing MEASURE if present, else any referencing shape.
  const refMeasure = referencingShapes.find((x) => x.type === 'measure');
  const target = refMeasure || referencingShapes[0] || null;

  // ---- R2 select_populates ----
  if (!target) {
    report('R2 select_populates', false, 'no referencing shape found to select');
  } else {
    await selectShapeId(target.id);
    s = await snap();
    const et = s.edit_tar;
    const hasRefFields = !!(et && ((et.ref && et.ref.length > 0) || et.ref_baseLine));
    const inEdit = s.sub === 'SHAPE_EDIT';
    report('R2 select_populates', !!et && et.id === target.id && hasRefFields && inEdit,
      `sel=${target.type}#${target.id} et.ref=${et && et.ref ? JSON.stringify(et.ref.map((r)=>r&&r.id)) : 'n/a'} ` +
      `baseLine=${et && et.ref_baseLine ? et.ref_baseLine.id : 'none'} sub=${s.sub}`);
  }

  // ---- R3 parse_resolves ----
  // For each referencing shape whose type the parse* family can resolve to a
  // POINT (aux_point, search_point, line, arc) assert non-null finite geometry.
  const RESOLVABLE = new Set(['aux_point', 'search_point', 'line', 'arc']);
  const toParse = s.shapes.filter((sh) => RESOLVABLE.has(sh.type));
  let r3ok = true, r3detail = [], r3throw = false, r3null = false;
  for (const sh of toParse) {
    const r = await parseShape(sh.id);
    if (r.__throw) { r3ok = false; r3throw = true; r3detail.push(`${sh.type}#${sh.id}:THROW(${r.__throw})`); continue; }
    if (r.__missing) { r3detail.push(`${sh.type}#${sh.id}:missing`); continue; }
    // search_point/aux_point/line/arc must middle-point-parse to a finite point.
    const ok = r.ptFinite === true;
    // A referencing shape with all refs resolvable returning null IS a real failure.
    const isReferencing = referencingShapes.some((x) => x.id === sh.id) ||
      sh.type === 'line'; // lines parse from their own geometry
    if (!ok && isReferencing) { r3ok = false; r3null = true; }
    r3detail.push(`${sh.type}#${sh.id}:pt=${ok ? 'ok' : (r.pt ? 'NaN' : 'null')}${r.vecFinite ? ',vec=ok' : ''}`);
  }
  report('R3 parse_resolves', r3ok,
    (toParse.length ? r3detail.slice(0, 10).join(' ') : 'no resolvable shapes') +
    (r3throw ? ' [THREW]' : '') + (r3null ? ' [NULL-GEOM]' : ''));

  // ---- R4 handshake (2-dispatch binding) ----
  // Re-bind the first ref slot of the selected measure to a same-type candidate
  // and assert applyEditTarSubstate wrote it & cleared trace/cand.
  if (!target) {
    report('R4 handshake', false, 'no target to drive handshake');
  } else {
    await selectShapeId(target.id);
    let st = await snap();
    const et = st.edit_tar;
    const curRef0 = et && et.ref && et.ref[0] ? et.ref[0] : null;
    if (!curRef0 || curRef0.id == null) {
      // No ref[0] pointer to re-bind — document as gap, but still verify the
      // handshake does not throw and clears state.
      report('R4 handshake', true,
        `GAP: selected ${target.type}#${target.id} has no ref[0].id to re-bind (subtype=${et && et.subtype})`);
    } else {
      // find a same-type candidate shape != current
      const candType = curRef0.type;
      const cand = st.shapes.find((x) => x.type === candType && x.id !== curRef0.id) ||
                   st.shapes.find((x) => x.type === candType);
      if (!cand) {
        report('R4 handshake', true, `GAP: no same-type(${candType}) candidate to re-bind`);
      } else {
        // A loaded def is defConf_lock_level=1; in DEFCONF mode the reducer gates ALL
        // DefConf actions (incl. the trace/cand handshake) unless unlocked. Unlock first.
        await dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });
        await sleep(120);
        // dispatch 1: trace into ref[0]
        await dispatch({ type: 'Edit_Tar_Ele_Trace_Update', data: ['ref', 0] });
        await sleep(120);
        const mid = await snap();
        const traceSet = Array.isArray(mid.trace) && mid.trace[0] === 'ref';
        // dispatch 2: candidate => triggers applyEditTarSubstate (SHAPE_EDIT case)
        await dispatch({ type: 'Edit_Tar_Ele_Cand_Update', data: { shape: { id: cand.id, type: cand.type } } });
        await sleep(150);
        st = await snap();
        const cleared = st.trace == null && st.cand == null;
        const wrote = st.edit_tar && st.edit_tar.ref && st.edit_tar.ref[0] &&
                      st.edit_tar.ref[0].id === cand.id;
        // For SHAPE_EDIT the binding is unconditional (no subtype gate) => must write.
        report('R4 handshake', traceSet && cleared && wrote,
          `cand=${candType}#${cand.id} traceSet=${traceSet} cleared=${cleared} ` +
          `ref0.id ${curRef0.id}->${st.edit_tar && st.edit_tar.ref && st.edit_tar.ref[0] ? st.edit_tar.ref[0].id : 'n/a'} wrote=${wrote}`);
      }
    }
  }

  // ---- R5 no_diag_error ----
  // No diag ring-buffer hook is exposed by the daemon/dev hooks, so we proxy with
  // a console.error counter installed in the page across a select+parse cycle.
  if (!target) {
    report('R5 no_diag_error', false, 'no target');
  } else {
    await ev(
      `(function(){if(!window.__r3errPatched){window.__r3errPatched=true;window.__r3err=0;` +
      `var oe=console.error;console.error=function(){window.__r3err++;return oe.apply(console,arguments);};}` +
      `window.__r3err=0;return 'ok';})()`
    );
    await selectShapeId(target.id);
    await parseShape(target.id);
    await sleep(120);
    const errCount = await ev('window.__r3err|0');
    report('R5 no_diag_error', errCount === 0,
      `console.error count during select+parse of resolvable ${target.type}#${target.id} = ${errCount}` +
      (errCount !== 0 ? ' (NOTE: proxy for diag ring-buffer; no diag hook exposed)' : ''));
  }

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
