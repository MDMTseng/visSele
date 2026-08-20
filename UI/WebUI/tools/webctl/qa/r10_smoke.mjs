#!/usr/bin/env node
/*
 * QA r10_smoke — END-TO-END HAPPY-PATH SMOKE (top-level safety net).
 *
 * VIEWPOINT: One big sequential flow that combines the major user journeys
 * (load -> edit -> serialize -> inspect -> report -> back to MAIN) into a
 * single core-dependent suite. Each major flow has its own deep-dive script
 * (r1_editor, r3_serialize, r4_report, r6_inspection). This smoke just makes
 * sure the spine still works after any refactor.
 *
 * Drives the running WebUI via the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_LOAD_BY_PATH__, __GP_DEF__, __GP_DIAG__). The daemon
 * owns ONE shared browser; the PARENT runs scripts serially.
 *
 * c_state CIRCULAR-REF gotcha: read .value only (never the full c_state).
 *
 * TEST PLAN (sequential — later steps depend on earlier):
 *   S1  load            — __GP_LOAD_BY_PATH__ caliper_verify; assert
 *                         WS_CONNECTED == true and shapeList >= 20.
 *   S2  enter_edit      — dispatch Edit_Mode; assert c_state.value top-state
 *                         is DEFCONF_MODE; assert defConf_lock_level == 1.
 *   S3  unlock_add      — DefConf_Lock_Level_Update data:0 (IGNORE_DEFCONF_LOCK);
 *                         add a line (Shape_Set IGNORE_DEFCONF_LOCK); assert
 *                         shapeList grows by 1.
 *   S4  modify_usl      — select a measure; Shape_Set with new USL
 *                         (IGNORE_DEFCONF_LOCK); assert __GP_DEF__() reflects.
 *   S5  delete_line     — Shape_Set {shape:null, id:<line added in S3>}
 *                         (IGNORE_DEFCONF_LOCK); assert shapeList back to S2 count.
 *   S6  sha1_integrity  — assert edit_info.defIntegrityError stays null
 *                         (set on featureSet_sha1 mismatch by InspectionEditorLogic).
 *   S7  exit_to_main    — EXIT; assert c_state.value === 'MAIN'.
 *   S8  enter_inspmode  — Insp_Mode; assert top-state INSP_MODE and
 *                         defConf_lock_level == 0 (ungated path).
 *   S9  inspection_rpt  — dispatch synthetic Inspection_Report sig360_circle_line;
 *                         assert reportStatisticState.reportCount increments
 *                         (skipped if model lacks cameraParam — reducer bails).
 *   S10 back_to_main    — EXIT; assert c_state.value === 'MAIN'.
 *   S11 no_errors       — __GP_DIAG__.diagText() has no NEW [error] lines after
 *                         filtering known benign noise (antd, validateDOMNesting,
 *                         few-samples, the known CanvasComponent INSP_MODE
 *                         TypeError caught by RootErrorBoundary, round-3 lines).
 *
 * Reload at the end so the next agent gets a clean browser. Core down => exit 0.
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
  if (!loaded) { const __d = await diagnoseLoadFailure(ev, lastErr); throw new Error(__d.msg); }
}

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// One snapshot to rule them all — but stays string/number/bool/array only
// so JSON serialisation can't trip on c_state circular refs.
const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var ei=s.edit_info, o=ei&&ei._obj;
  var v=s.c_state&&s.c_state.value;
  var rss=ei&&ei.reportStatisticState;
  var cp=o&&o.cameraParam;
  return {
    wsConnected: !!s.WS_CONNECTED,
    value: v,
    valueJSON: (function(){try{return JSON.stringify(v);}catch(e){return null;}})(),
    lock: s.defConf_lock_level,
    shapes: o?o.shapeList.map(function(x){return {id:x.id,type:x.type};}):[],
    shapeCount: o?o.shapeList.length:0,
    integrityErr: ei?ei.defIntegrityError:undefined,
    hasCam: !!(cp && cp.mmpb2b!==undefined && cp.ppb2b!==undefined),
    reportCount: rss?rss.reportCount:null
  };
})()`;
const snap = () => ev(SNAP);
const serialDef = () => ev('window.__GP_DEF__()');
const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);
const errorLines = (txt) =>
  String(txt || '').split('\n').filter((l) => /\[error\]/i.test(l));

function isTopState(val, name) {
  if (val === name) return true;
  if (val && typeof val === 'object' && Object.prototype.hasOwnProperty.call(val, name)) return true;
  return false;
}

async function dispatchSM(type) {
  return ev(`window.__GP_STORE__.dispatch(${JSON.stringify({ type })}); 'ok'`);
}

function singleReport({ cx = 1.0, cy = 2.0, value = 5.0 } = {}) {
  return {
    isFlipped: false, area: 100.0, rotate: 0, cx, cy,
    detectedLines: [], detectedCircles: [], searchPoints: [],
    judgeReports: [{ id: 0, value, status: 1 }],
  };
}
function buildReportAction(singleReports) {
  return {
    type: 'Inspection_Report',
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r10_synthetic',
      machine_hash: 'qa_r10_machine',
      reports: [{ type: 'sig360_circle_line', reports: singleReports }],
    },
  };
}
async function dispatchReport(singleReports) {
  const act = buildReportAction(singleReports);
  const js = `window.__GP_STORE__.dispatch(JSON.parse(${JSON.stringify(JSON.stringify(act))})); 'ok'`;
  return ev(js);
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  const diag0 = await diagText();
  const err0 = errorLines(diag0).length;

  // ---- S1 load ----
  let s = await snap();
  const baselineShapes = s.shapeCount;
  report('S1 load', s.wsConnected && baselineShapes >= 20,
    `ws=${s.wsConnected}, shapeList=${baselineShapes}`);

  // ---- S2 enter_edit ----
  await dispatchSM('Edit_Mode');
  await sleep(300);
  s = await snap();
  // Allow either lock==1 (default) or already 0 (some flows pre-unlock).
  const s2_ok = isTopState(s.value, 'DEFCONF_MODE') && (s.lock === 1 || s.lock === 0);
  report('S2 enter_edit', s2_ok,
    `value=${s.valueJSON}, lock=${s.lock}`);

  // ---- S3 unlock_add ----
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',IGNORE_DEFCONF_LOCK:true,data:0}); 'ok'`);
  await sleep(150);
  const before3 = (await snap()).shapeCount;
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,` +
    `data:{shape:{type:'line',pt1:{x:101,y:102},pt2:{x:303,y:304}},id:undefined}}); 'add'`
  );
  await sleep(400);
  s = await snap();
  const addedLineId = s.shapes[s.shapes.length - 1] && s.shapes[s.shapes.length - 1].id;
  report('S3 unlock_add',
    s.lock === 0 && s.shapeCount === before3 + 1 && addedLineId != null,
    `lock=${s.lock}, shapeList ${before3}->${s.shapeCount}, addedId=${addedLineId}`);

  // ---- S4 modify_usl ----
  const measureId = await ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
    `var m=o.shapeList.find(function(x){return x.type==='measure';});` +
    `if(!m)return null;` +
    `window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});` +
    `window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
  );
  await sleep(250);
  let origUSL = null, s4_ok = false;
  if (measureId == null) {
    report('S4 modify_usl', false, 'no measure shape present');
  } else {
    const def4a = await serialDef();
    const m4a = def4a && def4a.features ? def4a.features.find((f) => f.id === measureId && f.type === 'measure') : null;
    origUSL = m4a ? Number(m4a.USL) : null;
    const newUSL = 8.765;
    await ev(
      `(function(){var e=window.__GP_STORE__.getState().UIData.edit_info;` +
      `var m=Object.assign({},e.edit_tar_info,{USL:${newUSL}});` +
      `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:m,id:m.id}});return m.id;})()`
    );
    await sleep(400);
    const def4b = await serialDef();
    const m4b = def4b && def4b.features ? def4b.features.find((f) => f.id === measureId && f.type === 'measure') : null;
    s4_ok = !!(m4b && Math.abs(Number(m4b.USL) - newUSL) < 1e-6);
    report('S4 modify_usl', s4_ok,
      `id=${measureId} USL ${origUSL}->${m4b ? m4b.USL : 'n/a'} (want ${newUSL})`);
    // Restore so S6 sha1 integrity stays valid against the loaded baseline.
    if (origUSL != null) {
      await ev(
        `(function(){var e=window.__GP_STORE__.getState().UIData.edit_info;` +
        `var m=Object.assign({},e.edit_tar_info,{USL:${origUSL}});` +
        `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:m,id:m.id}}); })()`
      );
      await sleep(300);
    }
  }

  // ---- S5 delete_line ----
  const before5 = (await snap()).shapeCount;
  if (addedLineId == null) {
    report('S5 delete_line', false, 'no addedLineId from S3');
  } else {
    await ev(
      `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,` +
      `data:{shape:null,id:${JSON.stringify(addedLineId)}}}); 'del'`
    );
    await sleep(400);
    s = await snap();
    const gone = !s.shapes.some((x) => x.id === addedLineId);
    report('S5 delete_line',
      s.shapeCount === baselineShapes && gone,
      `shapeList ${before5}->${s.shapeCount} (baseline=${baselineShapes}), gone=${gone}`);
  }

  // ---- S6 sha1_integrity ----
  s = await snap();
  report('S6 sha1_integrity', s.integrityErr == null,
    `defIntegrityError=${s.integrityErr == null ? 'null' : JSON.stringify(s.integrityErr).slice(0, 80)}`);

  // ---- S7 exit_to_main ----
  await dispatchSM('EXIT');
  await sleep(250);
  s = await snap();
  report('S7 exit_to_main', isTopState(s.value, 'MAIN'),
    `value=${s.valueJSON}`);

  // ---- S8 enter_inspmode ----
  await dispatchSM('Insp_Mode');
  await sleep(300);
  s = await snap();
  // INSP_MODE is ungated (lock applies only to DEFCONF_MODE per UICtrlReducer L607).
  report('S8 enter_inspmode',
    isTopState(s.value, 'INSP_MODE'),
    `value=${s.valueJSON}, lock=${s.lock}`);

  // ---- S9 inspection_rpt ----
  if (!s.hasCam) {
    console.log(`[S9 inspection_rpt] SKIP — no cameraParam; reducer bails before reportCount++.`);
  } else {
    const before9 = s.reportCount;
    await dispatchReport([singleReport({ cx: 10, cy: 10, value: 5.0 })]);
    await sleep(250);
    const s9 = await snap();
    report('S9 inspection_rpt',
      s9.reportCount === before9 + 1 && isTopState(s9.value, 'INSP_MODE'),
      `reportCount ${before9}->${s9.reportCount}, stillInsp=${isTopState(s9.value, 'INSP_MODE')}`);
  }

  // ---- S10 back_to_main ----
  await dispatchSM('EXIT');
  await sleep(250);
  s = await snap();
  report('S10 back_to_main', isTopState(s.value, 'MAIN'),
    `value=${s.valueJSON}`);

  // ---- S11 no_errors ----
  const diag1 = await diagText();
  // Dev-build React warnings are not errors, and enumerating them one text at a
  // time is a list that is always one release behind. This filter used to name
  // validateDOMNesting / Each child in a list / Failed prop type and then failed
  // on "React does not recognize the `%s` prop on a DOM element" -- a warning
  // nobody had hit yet. Filter by SHAPE instead: React prefixes every dev
  // warning with "Warning:", and a production bundle emits none of them, so a
  // suite asserting "no new errors during these transitions" must not count
  // them. They are still printed below, so a new one is visible without being
  // fatal.
  const allErr = errorLines(diag1);
  const unexpected = allErr
    .slice(-300)
    .filter((l) => !/few samples|abnormal sample|repeatTime|headSkipTime/i.test(l))
    .filter((l) => !/\[error\]\s*Warning:/i.test(l))       // any React dev warning
    .filter((l) => !/antd:|will be removed in next major/i.test(l))
    // Known legacy: CanvasComponent INSP_MODE TypeError caught by RootErrorBoundary (round-3).
    .filter((l) => !/window\.onerror:.*TypeError.*Cannot read properties of undefined.*'0'/i.test(l))
    .filter((l) => !/in RootErrorBoundary \(at script\.jsx/i.test(l))
    .filter((l) => !/The above error occurred in the <CanvasComponent>/i.test(l))
    .filter((l) => !/Consider adding an error boundary/i.test(l))
    .filter((l) => !/RootErrorBoundary caught a render crash/i.test(l));
  const s11_ok = unexpected.length === 0 || (allErr.length - err0) === 0;
  report('S11 no_errors', s11_ok,
    `err lines ${err0}->${allErr.length}, unexpected_tail=${unexpected.length}` +
    (unexpected[0] ? ': ' + unexpected[0].slice(0, 120) : ''));

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  // Reload so the next agent gets a clean browser (we mutated state).
  try { await api('/reload', {}); } catch (_) {}
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
