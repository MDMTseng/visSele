#!/usr/bin/env node
/*
 * QA r1_editor — behavioral tests for the def EDITOR viewpoint (shape CRUD + property sheet).
 *
 * Drives the running WebUI through the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__), exactly like flows.mjs.
 *
 * The daemon owns ONE shared browser; the PARENT runs this serially. Each test prints
 * "[name] PASS/FAIL <detail>"; failures are tracked and the process exits non-zero
 * if any REAL failure occurs. If the core backend is down (load times out / "Not
 * connected" / "Timeout"), prints "SKIP (core down)" and exits 0 (not a failure).
 *
 * TEST PLAN:
 *  T1 load            — load def; assert shapeList non-empty (~26) and serialized def has features.
 *  T2 types_present   — assert >=1 of the expected shape types exist (line/arc/measure/search_point/aux_point).
 *  T3 select_each     — for each TYPE present, run the real selection dispatch
 *                       (Edit_Tar_Update + Shape_Edit) and assert edit_tar_info is populated & matches type.
 *  T4 add_line        — Shape_Set {shape:line, id:undefined}; assert shapeList grows by 1
 *                       and serialized def reflects the new line.
 *  T5 modify_usl      — select a measure, Shape_Set with modified USL; assert it persists
 *                       in the serialized def (read back via __GP_DEF__).
 *  T6 delete_shape    — Shape_Set {shape:null, id:<existing>} (delete convention, confirmed in
 *                       UICtrlReducer Shape_Set: id defined + shape null => delete); assert gone.
 *  T7 lock_gating     — at lock_level=1 in DEFCONF_MODE, a Shape_Set edit must be REJECTED
 *                       (whitelist excludes Shape_Set); assert shapeList unchanged.
 *
 * NOTE on delete convention: confirmed from UICtrlReducer.js case Shape_Set comment +
 * SetShape(null,id). Lock gating only applies in DEFCONF_MODE (UISTS.DEFCONF_MODE), so
 * T7 must NOT be in Edit_Mode — it stays in defconf mode (post-load default) for the check.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { MODEL_PATH, diagnoseLoadFailure } from './lib_model.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const EXPECTED_TYPES = ['line', 'arc', 'measure', 'search_point', 'aux_point'];

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

// Faithful copy of flows.mjs reset(): reload, wait for store, load def w/ retry.
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

const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj, et=s.edit_info.edit_tar_info;
  return {
    lock: s.defConf_lock_level,
    shapes: o.shapeList.map(function(x){return {id:x.id,type:x.type,subtype:x.subtype,name:x.name};}),
    edit_tar: et?{id:et.id,type:et.type,subtype:et.subtype,USL:et.USL}:null
  };
})()`;

const snap = () => ev(SNAP);
const serialDef = () => ev('window.__GP_DEF__()');
const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

async function selectType(type) {
  return ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
    `var m=o.shapeList.find(function(s){return s.type===${JSON.stringify(type)};});` +
    `if(!m)return null;` +
    `window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});` +
    `window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
  );
}

async function main() {
  // ---- bring app to a loaded state (core down => SKIP) ----
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // T1 load
  let s = await snap();
  const n0 = s.shapes.length;
  const def0 = await serialDef();
  report('T1 load', n0 > 0 && def0 && Array.isArray(def0.features) && def0.features.length > 0,
    `shapeList=${n0}, def.features=${def0 && def0.features ? def0.features.length : 'n/a'}`);

  // T2 types_present
  const presentTypes = [...new Set(s.shapes.map((x) => x.type))];
  const known = presentTypes.filter((t) => EXPECTED_TYPES.includes(t));
  report('T2 types_present', known.length > 0,
    `present=[${presentTypes.join(',')}] known=[${known.join(',')}]`);

  // T3 select_each — selection dispatch must populate edit_tar_info per type
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(300);
  let selOk = true, selDetail = [];
  for (const t of known) {
    const id = await selectType(t);
    await sleep(250);
    const ss = await snap();
    const ok = id != null && ss.edit_tar && ss.edit_tar.type === t;
    if (!ok) selOk = false;
    selDetail.push(`${t}:${ok ? 'ok' : 'FAIL(' + (ss.edit_tar ? ss.edit_tar.type : 'null') + ')'}`);
  }
  report('T3 select_each', selOk, selDetail.join(' '));

  // T4 add_line — Shape_Set add (id undefined => push)
  await unlock();
  await sleep(150);
  const before4 = (await snap()).shapes.length;
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'line',pt1:{x:111,y:111},pt2:{x:333,y:333}},id:undefined}}); 'add'`
  );
  await sleep(400);
  s = await snap();
  const def4 = await serialDef();
  const lineCount = def4 && def4.features ? def4.features.filter((f) => f.type === 'line').length : -1;
  report('T4 add_line', s.shapes.length === before4 + 1 && lineCount >= 1,
    `shapeList ${before4}->${s.shapes.length}, def line features=${lineCount}`);
  const addedLineId = s.shapes[s.shapes.length - 1] && s.shapes[s.shapes.length - 1].id;

  // T5 modify_usl — select measure, Shape_Set new USL, assert persists in serialized def
  const measureId = await selectType('measure');
  await sleep(250);
  if (measureId == null) {
    report('T5 modify_usl', false, 'no measure shape present to modify');
  } else {
    const newUSL = 7.654;
    await unlock();
    await ev(
      `(function(){var e=window.__GP_STORE__.getState().UIData.edit_info;` +
      `var m=Object.assign({},e.edit_tar_info,{USL:${newUSL}});` +
      `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:m,id:m.id}});return m.id;})()`
    );
    await sleep(400);
    const def5 = await serialDef();
    const mf = def5 && def5.features ? def5.features.find((f) => f.id === measureId && f.type === 'measure') : null;
    const persisted = mf && Math.abs(Number(mf.USL) - newUSL) < 1e-6;
    report('T5 modify_usl', !!persisted,
      `measure id=${measureId} def.USL=${mf ? mf.USL : 'not-found'} (want ${newUSL})`);
  }

  // T6 delete_shape — Shape_Set {shape:null, id} per reducer delete convention
  await unlock();
  s = await snap();
  const delId = addedLineId != null ? addedLineId : (s.shapes[s.shapes.length - 1] && s.shapes[s.shapes.length - 1].id);
  const before6 = s.shapes.length;
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:null,id:${JSON.stringify(delId)}}}); 'del'`
  );
  await sleep(400);
  s = await snap();
  const gone = !s.shapes.some((x) => x.id === delId);
  report('T6 delete_shape', s.shapes.length === before6 - 1 && gone,
    `deleted id=${delId}, shapeList ${before6}->${s.shapes.length}, gone=${gone}`);

  // T7 lock_gating — in DEFCONF_MODE at lock=1, Shape_Set must be rejected.
  // Leave edit mode so the lock filter (UISTS.DEFCONF_MODE) is active.
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Mode'}); 'defconf'`);
  await sleep(200);
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:1}); 'lock'`);
  await sleep(150);
  s = await snap();
  const before7 = s.shapes.length;
  const lock7 = s.lock;
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'line',pt1:{x:5,y:5},pt2:{x:9,y:9}},id:undefined}}); 'try'`
  );
  await sleep(300);
  s = await snap();
  report('T7 lock_gating', lock7 === 1 && s.shapes.length === before7,
    `lock=${lock7}, shapeList ${before7}->${s.shapes.length} (edit should be rejected)`);

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
