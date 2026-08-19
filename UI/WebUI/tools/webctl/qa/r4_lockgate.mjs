#!/usr/bin/env node
/*
 * QA r4_lockgate — characterize the DEFCONF lock-gate in UICtrlReducer.js:607.
 *
 * Gate (verbatim semantic, line 607):
 *   if (state == DEFCONF_MODE && defConf_lock_level != 0 && action.IGNORE_DEFCONF_LOCK != true)
 *      drop action unless its type is in the level-N whitelist.
 *
 * Whitelist (level1Filter — what gets through at lock_level=1):
 *   - DefConf_Lock_Level_Update            (always; otherwise system is permastuck)
 *   - Edit_Tar_Update                      (selection allowed under lock)
 *   - Shape_Decoration_ID_Order_Update     (decoration mutates allowed)
 *   - Shape_Decoration_Extra_Info_Update
 * Everything else in DefConfAct.EVENT is GATED (dropped) when locked in DEFCONF.
 * Non-DefConf actions (e.g. SM Edit_Mode) bypass the gate entirely — the gate only
 * runs inside the DefConf reducer's action dispatch path.
 *
 * TEST PLAN:
 *  T1 baseline_lock         — after load, in DEFCONF, defConf_lock_level == 1.
 *  T2 gated_when_locked     — with lock=1, dispatch Shape_Set / Edit_Tar_Ele_Trace_Update /
 *                             Matching_Angle_Margin_Deg_Update; assert each is DROPPED
 *                             (per-action sentinel proves no state change).
 *  T3 bypass_with_ignore    — same actions w/ IGNORE_DEFCONF_LOCK:true; assert APPLIED.
 *  T4 bypass_when_unlocked  — unlock (lock=0), dispatch same actions w/o flag; APPLIED.
 *                             Re-lock, re-dispatch; DROPPED again (gate restored).
 *  T5 lock_update_not_gated — DefConf_Lock_Level_Update itself must pass while locked
 *                             (else system would be permastuck). Lock=1 -> 0 -> 1.
 *  T6 mode_bypass           — leave DEFCONF (enter Edit_Mode/MAIN), keep lock=1,
 *                             dispatch a gated action; per gate "state==DEFCONF_MODE",
 *                             outside DEFCONF gate is OFF and action APPLIES.
 *  T7 non_defconf_action    — non-DefConf action (Edit_Mode SM event) works regardless
 *                             of lock state in DEFCONF.
 *
 * Sentinels:
 *  - Shape_Set add: shapeList length grows by 1.
 *  - Edit_Tar_Ele_Trace_Update: edit_info.edit_tar_ele_trace deep-equals payload.
 *  - Matching_Angle_Margin_Deg_Update: edit_info._obj.matching_angle_margin_deg matches.
 *
 * Core down (load fails) => SKIP exit 0. Reload at end. ONE shared browser.
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

// Snapshot fields used as sentinels for gated DefConf actions.
const PROBE = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj;
  return {
    // c_state.value is "MAIN" string (leaf) or {DEFCONF_MODE:"..."} object (compound).
    // Reduce to the main state name for assertions.
    smState: (typeof s.c_state.value === 'string' ? s.c_state.value : Object.keys(s.c_state.value)[0]),
    lock: s.defConf_lock_level,
    shapeN: (o && o.shapeList ? o.shapeList.length : 0),
    eleTrace: JSON.stringify(s.edit_info.edit_tar_ele_trace || null),
    angDeg: s.edit_info.matching_angle_margin_deg
  };
})()`;
const probe = () => ev(PROBE);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// Dispatch helpers — return new probe after a short settle.
async function dispatch(actionExpr) {
  await ev(`window.__GP_STORE__.dispatch(${actionExpr}); 'd'`);
  await sleep(220);
  return probe();
}

// Sentinel actions (unique payloads each call to detect "applied").
let _sentSeq = 100;
function nextLine() {
  const n = _sentSeq++;
  return `{type:'Shape_Set',data:{shape:{type:'line',pt1:{x:${n},y:${n}},pt2:{x:${n + 5},y:${n + 5}}},id:undefined}}`;
}
function nextTrace() {
  const tag = 'r4_' + (_sentSeq++);
  return { expr: `{type:'Edit_Tar_Ele_Trace_Update',data:[${JSON.stringify(tag)}]}`, want: JSON.stringify([tag]) };
}
function nextAng() {
  const v = (_sentSeq++) + 0.5;
  return { expr: `{type:'Matching_Angle_Margin_Deg_Update',data:${v}}`, want: v };
}

function withIgnore(expr) {
  // inject IGNORE_DEFCONF_LOCK:true into the action literal
  return expr.replace(/^\{/, '{IGNORE_DEFCONF_LOCK:true,');
}

// SM only has Edit_Mode (MAIN->DEFCONF_MODE) and EXIT (DEFCONF_MODE->MAIN).
// There is NO "DefConf_Mode" event — per r2_modes finding. Edit_Mode also
// causes the reducer to set defConf_lock_level=1 on entry to DEFCONF.
async function enterDefConf() {
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'em'`);
  await sleep(300);
}
async function exitToMain() {
  await ev(`window.__GP_STORE__.dispatch({type:'EXIT'}); 'exit'`);
  await sleep(300);
}
async function setLock(level) {
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:${level}}); 'L'`);
  await sleep(150);
}

async function main() {
  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // Ensure DEFCONF mode for baseline (post-load default per r1_editor).
  await enterDefConf();

  // ---------- T1 baseline_lock ----------
  let p = await probe();
  report('T1 baseline_lock', p.lock === 1,
    `smState=${p.smState} lock=${p.lock} (want 1 after load in DEFCONF)`);

  // ---------- T2 gated_when_locked ----------
  // Make sure we are locked.
  if (p.lock !== 1) await setLock(1);
  let before = await probe();

  const shapeAct = nextLine();
  const traceAct = nextTrace();
  const angAct = nextAng();

  let p_shape = await dispatch(shapeAct);
  let p_trace = await dispatch(traceAct.expr);
  let p_ang = await dispatch(angAct.expr);

  const gShape = p_shape.shapeN === before.shapeN;
  const gTrace = p_trace.eleTrace === before.eleTrace;
  const gAng = p_ang.angDeg === before.angDeg;
  report('T2 gated_when_locked', gShape && gTrace && gAng,
    `Shape_Set dropped=${gShape} (n ${before.shapeN}->${p_shape.shapeN}); ` +
    `Edit_Tar_Ele_Trace_Update dropped=${gTrace}; ` +
    `Matching_Angle_Margin_Deg_Update dropped=${gAng} (deg ${before.angDeg}->${p_ang.angDeg})`);

  // ---------- T3 bypass_with_ignore ----------
  before = await probe();
  const shapeAct2 = nextLine();
  const traceAct2 = nextTrace();
  const angAct2 = nextAng();

  p_shape = await dispatch(withIgnore(shapeAct2));
  p_trace = await dispatch(withIgnore(traceAct2.expr));
  p_ang = await dispatch(withIgnore(angAct2.expr));

  const aShape = p_shape.shapeN === before.shapeN + 1;
  const aTrace = p_trace.eleTrace === traceAct2.want;
  const aAng = Number(p_ang.angDeg) === angAct2.want;
  report('T3 bypass_with_ignore', aShape && aTrace && aAng,
    `Shape_Set applied=${aShape} (n ${before.shapeN}->${p_shape.shapeN}); ` +
    `Trace applied=${aTrace} (want ${traceAct2.want}, got ${p_trace.eleTrace}); ` +
    `Ang applied=${Number(p_ang.angDeg) === angAct2.want} (want ${angAct2.want}, got ${p_ang.angDeg})`);

  // ---------- T4 bypass_when_unlocked + re-lock restores gate ----------
  await setLock(0);
  before = await probe();
  const shapeAct3 = nextLine();
  const traceAct3 = nextTrace();
  const angAct3 = nextAng();

  p_shape = await dispatch(shapeAct3);
  p_trace = await dispatch(traceAct3.expr);
  p_ang = await dispatch(angAct3.expr);

  const uShape = p_shape.shapeN === before.shapeN + 1;
  const uTrace = p_trace.eleTrace === traceAct3.want;
  const uAng = Number(p_ang.angDeg) === angAct3.want;

  // re-lock and confirm gate restored
  await setLock(1);
  const beforeR = await probe();
  const shapeAct4 = nextLine();
  const p_re = await dispatch(shapeAct4);
  const restored = p_re.shapeN === beforeR.shapeN;

  report('T4 bypass_when_unlocked', uShape && uTrace && uAng && restored,
    `unlocked applied: shape=${uShape} trace=${uTrace} ang=${uAng}; ` +
    `re-lock restored gate=${restored} (n ${beforeR.shapeN}->${p_re.shapeN})`);

  // ---------- T5 lock_update_not_gated ----------
  // Currently locked. Should be able to unlock and re-lock.
  let pBefore = await probe();
  await setLock(0);
  let pMid = await probe();
  await setLock(1);
  let pAfter = await probe();
  report('T5 lock_update_not_gated',
    pBefore.lock === 1 && pMid.lock === 0 && pAfter.lock === 1,
    `lock seq ${pBefore.lock}->${pMid.lock}->${pAfter.lock} (want 1->0->1)`);

  // ---------- T6 mode_bypass: outside DEFCONF the gate is OFF even if lock!=0 ----------
  // Currently in DEFCONF lock=1. EXIT to MAIN, then try a Shape_Set (a DefConf action):
  // the gate fires only when state==DEFCONF, so in MAIN it should apply.
  await exitToMain();
  let pE = await probe();
  const inMain = pE.smState; // should be MAIN
  before = await probe();
  const shapeAct5 = nextLine();
  p_shape = await dispatch(shapeAct5);
  const editAppliedOutsideDefconf = p_shape.shapeN === before.shapeN + 1;
  report('T6 mode_bypass', inMain === 'MAIN' && editAppliedOutsideDefconf,
    `smState=${inMain} lock=${before.lock}; Shape_Set applied=${editAppliedOutsideDefconf} ` +
    `(n ${before.shapeN}->${p_shape.shapeN}) — gate only fires in DEFCONF_MODE`);

  // ---------- T7 SM event itself is NOT subject to the reducer gate ----------
  // From MAIN dispatch the Edit_Mode SM event -> SM should transition to DEFCONF_MODE.
  // (SM events go through the xstate middleware; the reducer gate doesn't apply.)
  let p7Before = await probe();
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'sm'`);
  await sleep(300);
  let p7After = await probe();
  const smChanged = p7Before.smState !== p7After.smState && p7After.smState === 'DEFCONF_MODE';
  report('T7 non_defconf_action', smChanged,
    `SM state ${p7Before.smState}->${p7After.smState} (Edit_Mode SM event must not be gated)`);

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
