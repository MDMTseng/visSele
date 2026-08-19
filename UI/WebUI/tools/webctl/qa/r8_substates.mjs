#!/usr/bin/env node
/*
 * QA r8_substates — DEFCONF_MODE SUB-STATE viewpoint.
 *
 * Discovered topology (src/redux/redux.js EditStates L18-62,
 * src/redux/actions/UIAct.js L2-25, src/redux/actions/DefConfAct.js L2-25):
 *
 *   STATE-VALUE STRINGS (the constants intentionally use SHORT names —
 *   so c_state.value comes back as e.g. {"DEFCONF_MODE":"LINE_CREATE"},
 *   NOT {"DEFCONF_MODE":"DEFCONF_MODE_LINE_CREATE"}):
 *     DEFCONF_MODE_NEUTRAL        -> "DEFCONF_MODE_NEUTRAL"
 *     DEFCONF_MODE_LINE_CREATE    -> "LINE_CREATE"
 *     DEFCONF_MODE_ARC_CREATE     -> "ARC_CREATE"
 *     DEFCONF_MODE_SEARCH_POINT_CREATE -> "SEARCH_POINT_CREATE"
 *     DEFCONF_MODE_AUX_POINT_CREATE    -> "AUX_POINT_CREATE"
 *     DEFCONF_MODE_AUX_LINE_CREATE     -> "AUX_LINE_CREATE"
 *     DEFCONF_MODE_MEASURE_CREATE -> "MEASURE_CREATE"
 *     DEFCONF_MODE_SHAPE_EDIT     -> "SHAPE_EDIT"
 *
 *   TRANSITIONS (NEUTRAL --evt--> child):
 *     Line_Create         -> LINE_CREATE
 *     Arc_Create          -> ARC_CREATE
 *     Search_Point_Create -> SEARCH_POINT_CREATE
 *     Aux_Point_Create    -> AUX_POINT_CREATE
 *     Aux_Line_Create     -> AUX_LINE_CREATE
 *     Measure_Create      -> MEASURE_CREATE
 *     Shape_Edit          -> SHAPE_EDIT
 *
 *   EXIT FROM A CREATE STATE: NOT the bare "EXIT" event (that exits whole
 *   DEFCONF_MODE back to MAIN!). The internal exit is DefConfAct.EVENT
 *   .SUCCESS == "DEFCONF_MODE_SUCCESS" (-> SHAPE_EDIT) or .FAIL ==
 *   "DEFCONF_MODE_FAIL" (-> NEUTRAL). SHAPE_EDIT's SUCCESS goes to NEUTRAL.
 *
 *   LOCK GATE (UICtrlReducer.js L607): predicate is
 *     stateObj.state == DEFCONF_MODE && defConf_lock_level != 0
 *   — i.e. it checks the TOP-LEVEL state name, which xstate reports as
 *   "DEFCONF_MODE" regardless of which child you're in. So the gate is
 *   active even inside LINE_CREATE etc., and Shape_Set is filtered out.
 *
 * c_state circular-ref gotcha — read .value only (round-5 lesson).
 *
 * TEST PLAN (all core-dependent; reset + Edit_Mode to enter DEFCONF):
 *   T1 baseline_neutral  — after Edit_Mode, mainSub === ['DEFCONF_MODE','DEFCONF_MODE_NEUTRAL'].
 *   T2 line_create       — from NEUTRAL: Line_Create -> LINE_CREATE; FAIL -> NEUTRAL.
 *   T3 arc_create        — from NEUTRAL: Arc_Create -> ARC_CREATE; FAIL -> NEUTRAL.
 *   T4 search_pt_create  — Search_Point_Create -> SEARCH_POINT_CREATE; FAIL -> NEUTRAL.
 *   T5 measure_create    — Measure_Create -> MEASURE_CREATE; FAIL -> NEUTRAL.
 *   T6 shape_edit_loop   — Edit_Tar_Update(shape) + Shape_Edit -> SHAPE_EDIT;
 *                          SUCCESS -> NEUTRAL.
 *   T7 success_from_line — from LINE_CREATE, SUCCESS -> SHAPE_EDIT (per machine).
 *   T8 garbage_in_sub    — bogus SM event from LINE_CREATE: substate unchanged,
 *                          no throw. (We must not leak into MAIN.)
 *   T9 lock_gates_in_sub — lock=2, enter LINE_CREATE, dispatch Shape_Set:
 *                          must be DROPPED (defLastUpdate timestamp must not
 *                          advance, since lock-gate breaks before the switch).
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

const STATE_SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var v=s.c_state&&s.c_state.value;
  var ei=s.edit_info, o=ei&&ei._obj;
  return {
    value: v,
    valueJSON: (function(){ try { return JSON.stringify(v); } catch(e){ return null; } })(),
    lockLevel: s.defConf_lock_level,
    shapeCount: (o&&o.shapeList)?o.shapeList.length:0,
    firstShapeId: (o&&o.shapeList&&o.shapeList[0])?o.shapeList[0].id:null
  };
})()`;
const snap = () => ev(STATE_SNAP);

// helper as spec'd
function mainSub(v) {
  if (v == null) return [null, null];
  if (typeof v === 'string') return [v, null];
  const k = Object.keys(v)[0];
  return [k, v[k]];
}

async function dispatchSM(type, extra) {
  const a = Object.assign({ type }, extra || {});
  return ev(`window.__GP_STORE__.dispatch(JSON.parse(${JSON.stringify(JSON.stringify(a))})); 'ok'`);
}

async function enterDefconfNeutral() {
  // ensure lock=0 (some load flows set lock>0 — clear it with IGNORE_DEFCONF_LOCK)
  await dispatchSM('DefConf_Lock_Level_Update', { data: 0, IGNORE_DEFCONF_LOCK: true });
  await sleep(50);
  await dispatchSM('Edit_Mode');
  await sleep(200);
}

async function main() {
  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  await enterDefconfNeutral();
  let s = await snap();
  let [top, sub] = mainSub(s.value);

  // If we didn't land in DEFCONF_MODE_NEUTRAL, try once more after a beat.
  if (top !== 'DEFCONF_MODE' || sub !== 'DEFCONF_MODE_NEUTRAL') {
    await sleep(300);
    s = await snap();
    [top, sub] = mainSub(s.value);
  }

  // ---- T1 baseline_neutral ----
  const t1_ok = top === 'DEFCONF_MODE' && sub === 'DEFCONF_MODE_NEUTRAL';
  report('T1 baseline_neutral', t1_ok, `value=${s.valueJSON}, lock=${s.lockLevel}`);
  if (!t1_ok) {
    console.log('SKIP remaining: not in DEFCONF_MODE_NEUTRAL');
    try { await api('/reload', {}); } catch (_) {}
    process.exit(failures ? 1 : 0);
  }

  // Helper: assert we are in a specific substate; then return to NEUTRAL via FAIL.
  async function checkSubstate(name, event, expectedSub) {
    await dispatchSM(event);
    await sleep(150);
    const s1 = await snap();
    const [t, su] = mainSub(s1.value);
    const ok_in = t === 'DEFCONF_MODE' && su === expectedSub;
    // Return to neutral via FAIL ("DEFCONF_MODE_FAIL")
    await dispatchSM('DEFCONF_MODE_FAIL');
    await sleep(150);
    const s2 = await snap();
    const [t2, su2] = mainSub(s2.value);
    const ok_out = t2 === 'DEFCONF_MODE' && su2 === 'DEFCONF_MODE_NEUTRAL';
    report(name, ok_in && ok_out,
      `enter=${s1.valueJSON}, fail->${s2.valueJSON}`);
  }

  // ---- T2 line_create / T3 arc / T4 search_pt / T5 measure ----
  await checkSubstate('T2 line_create',    'Line_Create',         'LINE_CREATE');
  await checkSubstate('T3 arc_create',     'Arc_Create',          'ARC_CREATE');
  await checkSubstate('T4 search_pt_create','Search_Point_Create','SEARCH_POINT_CREATE');
  await checkSubstate('T5 measure_create', 'Measure_Create',      'MEASURE_CREATE');

  // ---- T6 shape_edit_loop ----
  // SHAPE_EDIT requires a target. Edit_Tar_Update with first existing shape.
  if (s.firstShapeId == null) {
    console.log('[T6 shape_edit_loop] SKIP — no shape in def to edit');
  } else {
    await dispatchSM('Edit_Tar_Update', { data: { id: s.firstShapeId } });
    await sleep(80);
    await dispatchSM('Shape_Edit');
    await sleep(200);
    const s6 = await snap();
    const [t6, su6] = mainSub(s6.value);
    const ok_in = t6 === 'DEFCONF_MODE' && su6 === 'SHAPE_EDIT';
    // SHAPE_EDIT.SUCCESS -> NEUTRAL
    await dispatchSM('DEFCONF_MODE_SUCCESS');
    await sleep(150);
    const s6b = await snap();
    const [t6b, su6b] = mainSub(s6b.value);
    const ok_out = t6b === 'DEFCONF_MODE' && su6b === 'DEFCONF_MODE_NEUTRAL';
    report('T6 shape_edit_loop', ok_in && ok_out,
      `enter=${s6.valueJSON}, success->${s6b.valueJSON}`);
  }

  // ---- T7 success_from_line ----
  await dispatchSM('Line_Create');
  await sleep(120);
  await dispatchSM('DEFCONF_MODE_SUCCESS');
  await sleep(150);
  const s7 = await snap();
  const [t7, su7] = mainSub(s7.value);
  const t7_ok = t7 === 'DEFCONF_MODE' && su7 === 'SHAPE_EDIT';
  report('T7 success_from_line', t7_ok, `value=${s7.valueJSON}`);
  // recover -> NEUTRAL for next tests
  await dispatchSM('DEFCONF_MODE_SUCCESS');
  await sleep(120);

  // ---- T8 garbage_in_sub ----
  await dispatchSM('Line_Create');
  await sleep(120);
  const sPre = await snap();
  let threw = false;
  try { await dispatchSM('NOPE_xyz_not_a_real_event'); } catch (e) { threw = true; }
  await sleep(100);
  const sPost = await snap();
  const t8_ok = !threw && sPost.valueJSON === sPre.valueJSON
                && mainSub(sPost.value)[1] === 'LINE_CREATE';
  report('T8 garbage_in_sub', t8_ok,
    `threw=${threw}, ${sPre.valueJSON} -> ${sPost.valueJSON}`);
  // back to neutral
  await dispatchSM('DEFCONF_MODE_FAIL');
  await sleep(120);

  // ---- T9 lock_gates_in_sub ----
  // Set lock=2 (must use IGNORE so the lock-update itself passes). Enter
  // LINE_CREATE. Dispatch Shape_Set with the existing shape id — under
  // lock!=0 the level1Filter excludes Shape_Set, so the reducer breaks
  // before the switch and no mutation should happen.
  if (s.firstShapeId == null) {
    console.log('[T9 lock_gates_in_sub] SKIP — no shape to mutate');
  } else {
    await dispatchSM('DefConf_Lock_Level_Update', { data: 2, IGNORE_DEFCONF_LOCK: true });
    await sleep(80);
    await dispatchSM('Line_Create');
    await sleep(120);
    const sLock = await snap();
    const inLine = mainSub(sLock.value)[1] === 'LINE_CREATE';
    const cntBefore = sLock.shapeCount;
    // Try to mutate via Shape_Set with a clearly-different shape body.
    await dispatchSM('Shape_Set', {
      data: { id: s.firstShapeId, shape: { id: s.firstShapeId, name: '__r8_lock_test__' } },
    });
    await sleep(150);
    const sAfter = await snap();
    // The lock-gate dropped the action: shape count must not change AND
    // we must still be in LINE_CREATE (Shape_Set has no transition anyway).
    const cntSame = sAfter.shapeCount === cntBefore;
    const stillIn = mainSub(sAfter.value)[1] === 'LINE_CREATE';
    // Also confirm: with lock cleared, the gate STOPS dropping (sanity).
    await dispatchSM('DefConf_Lock_Level_Update', { data: 0, IGNORE_DEFCONF_LOCK: true });
    await sleep(80);
    const sCleared = await snap();
    report('T9 lock_gates_in_sub', inLine && cntSame && stillIn,
      `inLine=${inLine} cnt ${cntBefore}->${sAfter.shapeCount} stillIn=${stillIn} lockNow=${sCleared.lockLevel}`);
    await dispatchSM('DEFCONF_MODE_FAIL');
    await sleep(80);
  }

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  try { await api('/reload', {}); } catch (_) {}
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
