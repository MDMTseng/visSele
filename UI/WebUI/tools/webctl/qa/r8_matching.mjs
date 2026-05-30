#!/usr/bin/env node
/*
 * QA r8_matching — matching-engine config actions on edit_info.
 *
 * Actions covered (from DefConfAct.js, verified by code search):
 *   - Matching_Angle_Margin_Deg_Update  -> edit_info.matching_angle_margin_deg
 *       reducer: unconditional assign of action.data (no clamp, no type guard).
 *   - Matching_Face_Update              -> edit_info.matching_face
 *       reducer: unconditional assign of action.data.
 *   - IntrusionSizeLimitRatio_Update    -> edit_info.intrusionSizeLimitRatio
 *       reducer: ONLY assigned when typeof action.data === 'number' (guard at
 *       UICtrlReducer.js:773). Non-numbers are silently dropped.
 *   - Matching_Conditions_Update        -> DOES NOT EXIST in DefConfAct.EVENT.
 *       (searched src/redux, only the three above are present.) We still attempt
 *       a dispatch and assert no crash + no state mutation.
 *
 * Defaults (InspectionEditorLogic.js:1472-1475 — applied at def load):
 *   matching_angle_margin_deg: 180
 *   matching_face: 0
 *   intrusionSizeLimitRatio: 0.1
 *
 * LOCK GATE: All four actions are DefConf actions and DROPPED when state is
 * DEFCONF_MODE && defConf_lock_level != 0 unless IGNORE_DEFCONF_LOCK:true is
 * set on the dispatched action (UICtrlReducer.js:607 — see r4_lockgate.mjs).
 * Strategy: unlock at start (setLock 0) AND inject IGNORE_DEFCONF_LOCK on each
 * dispatch (belt + suspenders).
 *
 * TEST PLAN (core-DEPENDENT):
 *  T1 baseline_defaults        — load, snapshot edit_info matching fields,
 *                                assert match doc defaults from line 1472.
 *  T2 angle_margin_basic       — dispatch 45, 90, 270, 360, 0; each lands
 *                                exactly (no clamp).
 *  T3 angle_margin_negative    — dispatch -30; lands as -30 (no clamp -
 *                                record actual behavior).
 *  T4 angle_margin_garbage     — dispatch a non-number (string, object); no
 *                                crash; record whether reducer stored it.
 *  T5 matching_face            — dispatch -1, 0, 1 (back / both / front per
 *                                BPG_WS HR resolve comment); each lands.
 *  T6 intrusion_ratio_number   — dispatch 0.25, 0.5, 1.0, 0; each lands.
 *  T7 intrusion_ratio_garbage  — dispatch "0.3" (string) and an object;
 *                                reducer guard (typeof=='number') => state
 *                                must NOT change.
 *  T8 matching_conditions      — dispatch Matching_Conditions_Update (non-
 *                                existent type); assert no crash + matching
 *                                state untouched.
 *  T9 serialization            — after all edits, edit_info reflects last
 *                                applied values for each field.
 *  T10 no_error_lines          — confirm no NEW [error] lines surfaced beyond
 *                                the legacy + r6 CanvasComponent baseline.
 *
 * Core down (load fails) => SKIP exit 0. Reload at end. ONE shared browser.
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
        `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList?o.shapeList.length:0)};})()`
      );
      if (st.err) { lastErr = st.err; break; }
      if (st.rdy && st.n > 0) { loaded = true; break; }
    }
    if (!loaded) { lastErr = lastErr || 'timeout'; await sleep(3000); }
  }
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

// Snapshot just the matching fields we care about.
const PROBE = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var ei=s.edit_info||{};
  return {
    smState: (typeof s.c_state.value === 'string' ? s.c_state.value : Object.keys(s.c_state.value)[0]),
    lock: s.defConf_lock_level,
    angDeg: ei.matching_angle_margin_deg,
    angDegT: typeof ei.matching_angle_margin_deg,
    face: ei.matching_face,
    faceT: typeof ei.matching_face,
    intr: ei.intrusionSizeLimitRatio,
    intrT: typeof ei.intrusionSizeLimitRatio
  };
})()`;
const probe = () => ev(PROBE);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// Dispatch with IGNORE_DEFCONF_LOCK to bypass gate regardless of mode.
async function dispatchIgnore(actionExpr) {
  // actionExpr must be a JS object literal starting with '{'
  const withIgnore = actionExpr.replace(/^\{/, '{IGNORE_DEFCONF_LOCK:true,');
  await ev(`try{window.__GP_STORE__.dispatch(${withIgnore});}catch(e){window.__lastDispatchErr=String(e);} 'd'`);
  await sleep(180);
  return probe();
}

async function setLock(level) {
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:${level}}); 'L'`);
  await sleep(120);
}

// JS-literal escape for a JS string value.
function jsStr(s) { return JSON.stringify(s); }

async function getErrLines() {
  // Return console error line count (filtered against legacy + r6 CanvasComponent noise).
  return ev(`(function(){
    var L=(window.__GP_CONSOLE__||window.__console_log__||[]);
    if(!Array.isArray(L)) return 0;
    var keep=L.filter(function(l){
      var s=String(l && (l.text||l.msg||l)||'');
      if(!/\\[error\\]/i.test(s)) return false;
      if(/CanvasComponent/.test(s)) return false; // r6 known
      if(/legacy/i.test(s)) return false;
      return true;
    });
    return keep.length;
  })()`).catch(() => 0);
}

async function main() {
  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // Unlock so dispatches apply even without IGNORE flag, but we send IGNORE anyway.
  await setLock(0);

  const errBefore = await getErrLines();

  // ---------- T1 baseline_defaults ----------
  const base = await probe();
  const okBase =
    base.angDeg === 180 &&
    base.face === 0 &&
    Math.abs(base.intr - 0.1) < 1e-9;
  report('T1 baseline_defaults', okBase,
    `angDeg=${base.angDeg} (want 180) face=${base.face} (want 0) intr=${base.intr} (want 0.1)`);

  // ---------- T2 angle_margin_basic ----------
  const values = [45, 90, 270, 360, 0];
  let allOk = true; let detail = [];
  for (const v of values) {
    const p = await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:${v}}`);
    const ok = Number(p.angDeg) === v;
    if (!ok) allOk = false;
    detail.push(`${v}->${p.angDeg}`);
  }
  report('T2 angle_margin_basic', allOk, detail.join(' '));

  // ---------- T3 angle_margin_negative ----------
  const pNeg = await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:-30}`);
  // No clamp in reducer; expect -30 stored verbatim.
  const okNeg = Number(pNeg.angDeg) === -30;
  report('T3 angle_margin_negative', okNeg,
    `dispatched -30, got ${pNeg.angDeg} (reducer has no clamp; verbatim expected)`);

  // ---------- T4 angle_margin_garbage ----------
  // Reducer assigns unconditionally — these WILL land but app must not crash.
  const pStr = await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:${jsStr("not-a-number")}}`);
  const pObj = await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:{bad:1}}`);
  // Restore a sane value so later UI code doesn't choke during serialization read.
  const pRestore = await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:135}`);
  const okGarbage = pRestore.angDeg === 135 && pStr.angDeg === 'not-a-number';
  report('T4 angle_margin_garbage', okGarbage,
    `string->${JSON.stringify(pStr.angDeg)} (type ${pStr.angDegT}); ` +
    `object->${JSON.stringify(pObj.angDeg)} (type ${pObj.angDegT}); ` +
    `restore->${pRestore.angDeg} (reducer is unconditional assign — no validation)`);

  // ---------- T5 matching_face ----------
  const faces = [-1, 0, 1];
  let faceOk = true; let faceDetail = [];
  for (const v of faces) {
    const p = await dispatchIgnore(`{type:'Matching_Face_Update',data:${v}}`);
    if (Number(p.face) !== v) faceOk = false;
    faceDetail.push(`${v}->${p.face}`);
  }
  report('T5 matching_face', faceOk, faceDetail.join(' '));

  // ---------- T6 intrusion_ratio_number ----------
  const ratios = [0.25, 0.5, 1.0, 0];
  let rOk = true; let rDetail = [];
  for (const v of ratios) {
    const p = await dispatchIgnore(`{type:'IntrusionSizeLimitRatio_Update',data:${v}}`);
    if (Math.abs(Number(p.intr) - v) > 1e-9) rOk = false;
    rDetail.push(`${v}->${p.intr}`);
  }
  report('T6 intrusion_ratio_number', rOk, rDetail.join(' '));

  // ---------- T7 intrusion_ratio_garbage (typeof guard) ----------
  // Set a known anchor first.
  await dispatchIgnore(`{type:'IntrusionSizeLimitRatio_Update',data:0.42}`);
  const beforeG = await probe();
  const pStrR = await dispatchIgnore(`{type:'IntrusionSizeLimitRatio_Update',data:${jsStr("0.7")}}`);
  const pObjR = await dispatchIgnore(`{type:'IntrusionSizeLimitRatio_Update',data:{bad:1}}`);
  const dropped = Math.abs(pStrR.intr - 0.42) < 1e-9 && Math.abs(pObjR.intr - 0.42) < 1e-9;
  report('T7 intrusion_ratio_garbage', dropped,
    `anchor=0.42; after "0.7"->${pStrR.intr}; after {}->${pObjR.intr} (typeof guard must drop non-numbers)`);

  // ---------- T8 matching_conditions (non-existent action) ----------
  const before8 = await probe();
  await dispatchIgnore(`{type:'Matching_Conditions_Update',data:{some:'cond'}}`);
  const after8 = await probe();
  const stable =
    after8.angDeg === before8.angDeg &&
    after8.face === before8.face &&
    after8.intr === before8.intr;
  // also check store still alive
  const alive = (await ev('typeof window.__GP_STORE__')) === 'object';
  report('T8 matching_conditions', stable && alive,
    `Matching_Conditions_Update is NOT a registered EVENT — must no-op. ` +
    `state stable=${stable} store alive=${alive}`);

  // ---------- T9 serialization ----------
  // Final known-state edits then re-snapshot.
  await dispatchIgnore(`{type:'Matching_Angle_Margin_Deg_Update',data:77}`);
  await dispatchIgnore(`{type:'Matching_Face_Update',data:1}`);
  await dispatchIgnore(`{type:'IntrusionSizeLimitRatio_Update',data:0.33}`);
  const finalP = await probe();
  const okSer =
    Number(finalP.angDeg) === 77 &&
    Number(finalP.face) === 1 &&
    Math.abs(Number(finalP.intr) - 0.33) < 1e-9;
  report('T9 serialization', okSer,
    `final edit_info: angDeg=${finalP.angDeg} face=${finalP.face} intr=${finalP.intr} (want 77,1,0.33)`);

  // ---------- T10 no_error_lines ----------
  const errAfter = await getErrLines();
  report('T10 no_error_lines', errAfter <= errBefore,
    `error lines before=${errBefore} after=${errAfter}`);

  // ---------- cleanup ----------
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
