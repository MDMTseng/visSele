#!/usr/bin/env node
/*
 * QA r7_savefload — DEF FILE SAVE/LOAD round-trip integrity viewpoint.
 *
 * Real Save dispatches DefFile_DB_W (IndexedDB-backed DB_WS) + ACT_Report_Save to
 * the core — the test cannot observe the server side. What it CAN observe is the
 * in-app serialize <-> load cycle and the sha1 lineage bookkeeping driven by
 * ACT_DefFileHash_Update (reducer at UICtrlReducer.js:779) and the load-side
 * rootDefInfoLoading (InspectionEditorLogic.js:324). That's the integrity surface
 * this script asserts.
 *
 * Drives the running WebUI via the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__), exactly like r3_serialize.mjs.
 * The daemon owns ONE shared browser and the PARENT runs QA scripts SERIALLY —
 * this file never spawns its own daemon. core-down => SKIP exit 0.
 *
 * TEST PLAN:
 *  T1 idempotent_load   — __GP_LOAD_BY_PATH__(A) twice; shapeList.length identical
 *                         AND serialized def byte-identical (no leakage / cumulative state).
 *  T2 serialize_stable  — __GP_DEF__() twice w/ no edits; byte-identical.
 *                         (Subset of r3 S1, but kept here as save-cycle precondition.)
 *  T3 edit_reflected    — unlock; add line (Shape_Set, IGNORE_DEFCONF_LOCK); features
 *                         count grows by 1; remove same line (Shape_Set shape:null);
 *                         features count + serialized length back to baseline.
 *  T4 sha1_lineage      — capture edit_info.DefFileHash{,_pre,_root} after load.
 *                         Edit; capture again. Document whether mutation alone changes
 *                         the hash. Expected behavior (from DefConfUI.js:1571): hash
 *                         is only rewritten on Save via ACT_DefFileHash_Update — pure
 *                         edits should NOT alter it. Assert deterministic (no flutter).
 *  T5 no_new_errors     — snapshot window.__webctl_errors length before load+edit+load
 *                         cycle; assert nothing new beyond a known-noise allowlist
 *                         (legacy antd findDOMNode, react-redux warnings, "few samples",
 *                         and the round-6-known CanvasComponent INSP_MODE bug if it
 *                         leaks here).
 *
 * COVERAGE GAP: two-def cross-load (A->B leakage) is documented but NOT exercised —
 * only one model path is available in the test environment. Server-side Save (the
 * actual DefFile_DB_W IndexedDB write + core file write) is also out of scope.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';
const MODEL_PATH_B = process.env.WEBCTL_MODEL_B || null; // optional second def for cross-load test

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

// Faithful copy of r3_serialize reset(): reload, wait for store, load def w/ retry.
async function reset() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  await loadPath(MODEL_PATH);
}

async function loadPath(modelPath) {
  let loaded = false;
  let lastErr = null;
  for (let attempt = 0; attempt < 4 && !loaded; attempt++) {
    await ev(
      `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(modelPath)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
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

const serialDefStr = () => ev('JSON.stringify(window.__GP_DEF__())');
const serialDef = async () => JSON.parse(await serialDefStr());
const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);
const shapeCount = () =>
  ev(`window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.length`);
// edit_info.DefFileHash{,_pre,_root} live on edit_info itself (NOT _obj) — see
// UICtrlReducer.js:779 DefFileHash_Update branch.
const hashSnap = () =>
  ev(
    `(function(){var e=window.__GP_STORE__.getState().UIData.edit_info;` +
    `return {h:e.DefFileHash||null,pre:e.DefFileHash_pre||null,root:e.DefFileHash_root||null};})()`
  );
// cState.value only — full c_state has circular xstate refs that can't be JSON'd.
const cStateVal = () =>
  ev(`(function(){var s=window.__GP_STORE__.getState();var c=s.UIData&&s.UIData.c_state;return c&&c.value?c.value:null;})()`);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// known-noise allowlist for T5 (don't trip on pre-existing legacy warnings).
const NOISE_RX = [
  /findDOMNode/i,
  /componentWill(Mount|Receive|Update)/i,
  /react-redux/i,
  /few samples/i,
  /antd/i,
  // round-6-known CanvasComponent INSP_MODE bug (if it leaks through canvas mounts)
  /CanvasComponent.*INSP_MODE/i,
  /INSP_MODE.*CanvasComponent/i,
];
const isNoise = (s) => NOISE_RX.some((rx) => rx.test(String(s || '')));

async function collectErrors() {
  // webctl daemon installs window.__webctl_errors (per other r* scripts' pattern);
  // fall back to [] if not present so this test never falsely fails on a fresh daemon.
  return ev(
    `(function(){var a=window.__webctl_errors;return Array.isArray(a)?a.slice():[];})()`
  );
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // ---- T2 serialize_stable (precondition for the rest) ----
  const sA = await serialDefStr();
  await sleep(50);
  const sB = await serialDefStr();
  report('T2 serialize_stable', sA === sB && sA && sA.length > 2,
    `len=${sA ? sA.length : 0} identical=${sA === sB}`);

  // ---- T4 sha1_lineage (part 1: post-load snapshot) ----
  const hashAfterLoad = await hashSnap();

  // ---- T1 idempotent_load ----
  const n1 = await shapeCount();
  const def1 = await serialDefStr();
  try { await loadPath(MODEL_PATH); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down on 2nd load): ' + e.message); process.exit(0); }
    throw e;
  }
  const n2 = await shapeCount();
  const def2 = await serialDefStr();
  report('T1 idempotent_load',
    n1 === n2 && def1 === def2,
    `shapeList ${n1}->${n2} serialLen ${def1.length}->${def2.length} identical=${def1 === def2}`);

  // ---- T3 edit_reflected (add+remove line) ----
  await unlock();
  await sleep(150);
  const baseFeats = (await serialDef()).features.length;
  const baseShapes = await shapeCount();
  const baseLen = (await serialDefStr()).length;
  // unique coords so we can find + remove deterministically
  const X1 = 707, Y1 = 808, X2 = 909, Y2 = 1010;
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:{type:'line',pt1:{x:${X1},y:${Y1}},pt2:{x:${X2},y:${Y2}}},id:undefined}}); 'add'`
  );
  await sleep(400);
  const dAdd = await serialDef();
  const addedShapes = await shapeCount();
  const newLine = dAdd.features.find(
    (f) => f.type === 'line' && f.pt1 && Math.abs(Number(f.pt1.x) - X1) < 1e-6 && Math.abs(Number(f.pt1.y) - Y1) < 1e-6
  );
  const addOk = dAdd.features.length === baseFeats + 1 && addedShapes === baseShapes + 1 && !!newLine;

  // remove it: Shape_Set with shape:null + id of the new shape (matches r4 delete pattern)
  // The Shape_Set add returned no id; look it up in shapeList by coords.
  const newShapeId = await ev(
    `(function(){var sl=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList;` +
    `var s=sl.find(function(s){return s.type==='line'&&s.pt1&&Math.abs(s.pt1.x-${X1})<1e-6&&Math.abs(s.pt1.y-${Y1})<1e-6;});` +
    `return s?s.id:null;})()`
  );
  if (newShapeId != null) {
    await ev(
      `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:null,id:${JSON.stringify(newShapeId)}}}); 'del'`
    );
    await sleep(400);
  }
  const dDel = await serialDef();
  const delShapes = await shapeCount();
  const delLen = (await serialDefStr()).length;
  const delOk =
    newShapeId != null &&
    dDel.features.length === baseFeats &&
    delShapes === baseShapes &&
    delLen === baseLen;
  report('T3 edit_reflected', addOk && delOk,
    `add: feats ${baseFeats}->${dAdd.features.length} shapes ${baseShapes}->${addedShapes} found=${!!newLine}; ` +
    `del: feats->${dDel.features.length} shapes->${delShapes} len ${baseLen}->${delLen} id=${newShapeId}`);

  // ---- T4 sha1_lineage (part 2: after edits, before re-load) ----
  // Capture twice with a small gap to make sure it's deterministic (no flutter).
  const hashAfterEdit1 = await hashSnap();
  await sleep(100);
  const hashAfterEdit2 = await hashSnap();
  const deterministic = JSON.stringify(hashAfterEdit1) === JSON.stringify(hashAfterEdit2);
  // Document observed behavior. Per DefConfUI.js:1571 ACT_DefFileHash_Update is only
  // fired on Save, so a pure edit cycle SHOULD leave DefFileHash equal to post-load.
  const sameAsLoad = JSON.stringify(hashAfterLoad) === JSON.stringify(hashAfterEdit1);
  const observed = sameAsLoad
    ? 'hash UNCHANGED by edits (expected — only Save dispatches DefFileHash_Update)'
    : 'hash CHANGED by edits (unexpected vs DefConfUI.js:1571 contract)';
  report('T4 sha1_lineage', deterministic,
    `${observed}; load=${JSON.stringify(hashAfterLoad)} afterEdit=${JSON.stringify(hashAfterEdit1)} deterministic=${deterministic}`);

  // ---- T5 no_new_errors ----
  // The relevant window is "during this script's load+edit+load cycle". We can't
  // perfectly retro-snapshot the very-first reset(), but we CAN measure the delta
  // from now (post-T3) through one more clean load.
  const errsBefore = await collectErrors();
  try { await loadPath(MODEL_PATH); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down on T5 reload): ' + e.message); process.exit(0); }
    throw e;
  }
  await sleep(300);
  const errsAfter = await collectErrors();
  const added = errsAfter.slice(errsBefore.length);
  const realErrs = added.filter((e) => !isNoise(typeof e === 'string' ? e : (e && (e.msg || e.message)) || JSON.stringify(e)));
  report('T5 no_new_errors', realErrs.length === 0,
    `added=${added.length} noise-filtered=${added.length - realErrs.length} real=${realErrs.length}` +
    (realErrs.length ? ` first=${JSON.stringify(realErrs[0]).slice(0, 200)}` : ''));

  // (Coverage gap) two-def cross-load — only run if a second model is supplied.
  if (MODEL_PATH_B) {
    try {
      await loadPath(MODEL_PATH_B);
      const nB = await shapeCount();
      const defB = await serialDefStr();
      // re-load A; assert B's contents didn't leak into A
      await loadPath(MODEL_PATH);
      const nA2 = await shapeCount();
      const defA2 = await serialDefStr();
      report('T6 cross_load_no_leak',
        nA2 === n1 && defA2 === def1 && nB !== n1,
        `A:${n1} B:${nB} A':${nA2}`);
    } catch (e) {
      console.log(`[T6 cross_load_no_leak] SKIP — second model unavailable: ${e.message}`);
    }
  } else {
    console.log('[T6 cross_load_no_leak] SKIP — WEBCTL_MODEL_B not set (documented coverage gap)');
  }

  // Mutation safety: we added/removed shapes; leave clean.
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
