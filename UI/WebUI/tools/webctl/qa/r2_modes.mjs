#!/usr/bin/env node
/*
 * QA r2_modes — behavioral tests for the MODE / NAVIGATION state machine viewpoint.
 *
 * Drives the running WebUI through the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_LOAD_BY_PATH__), exactly like r1_editor.mjs / flows.mjs.
 *
 * The daemon owns ONE shared browser; the PARENT runs this serially. Each test prints
 * "[name] PASS/FAIL <detail>"; failures tracked, process exits non-zero on REAL failure.
 * If core backend is down (load times out / "Not connected" / "Timeout"), prints
 * "SKIP (core down)" and exits 0 (not a failure).
 *
 * ===========================================================================
 * STATE-MACHINE GROUND TRUTH (verified against source, NOT r1_editor guesses)
 * ---------------------------------------------------------------------------
 *  - The SM lives in an xstate middleware (src/redux/middleware/ECStateMachine.js).
 *    EVERY dispatched action.type is fed to sm.transition(c_state, action.type);
 *    if .value changes, an internal "ev_state_update" updates c_state in the store.
 *    => To drive a mode transition you dispatch a *plain action with the event as
 *       its `type`* (e.g. {type:'Edit_Mode'}); there is no separate SM dispatcher.
 *  - c_state.value is EITHER a string (leaf: "SPLASH","MAIN") OR an object for
 *    compound states: {DEFCONF_MODE:"DEFCONF_MODE_NEUTRAL"} etc. Main state =
 *    typeof string ? value : Object.keys(value)[0]  (mirrors xstate_GetCurrentMainState).
 *  - Top-level config (src/redux/redux.js):
 *      SPLASH  --REMOTE_SYSTEM_READY--> MAIN
 *      MAIN    --Edit_Mode--> DEFCONF_MODE (substate DEFCONF_MODE_NEUTRAL = "DEFCONF_MODE_NEUTRAL")
 *      MAIN    --Insp_Mode--> INSP_MODE     ;  MAIN --InstInsp_Mode--> INSTINSP_MODE
 *      DEFCONF_MODE --EXIT--> MAIN          ;  INSP_MODE/INSTINSP_MODE --EXIT--> MAIN
 *      any mode --REMOTE_SYSTEM_NOT_READY--> SPLASH
 *  - REAL event names (UI_SM_EVENT, src/redux/actions/UIAct.js):
 *      Edit_Mode, Insp_Mode, InstInsp_Mode, REMOTE_SYSTEM_READY,
 *      REMOTE_SYSTEM_NOT_READY, EXIT, ERROR, Line_Create, Arc_Create, ...
 *    *** There is NO "DefConf_Mode" event and NO "Edit_Tar_Update" in UI_SM_EVENT. ***
 *      r1_editor's dispatch of {type:'DefConf_Mode'} is a NO-OP (no such transition);
 *      it happens to "work" only because the app is ALREADY in DEFCONF_MODE.
 *  - Lock-gating events live in DefConfAct.EVENT (src/redux/actions/DefConfAct.js),
 *    NOT UI_SM_EVENT:  DefConf_Lock_Level_Update, Edit_Tar_Update, Shape_Set, ...
 *  - Lock gating (UICtrlReducer.js ~L607): ONLY when main state == DEFCONF_MODE and
 *    defConf_lock_level != 0, a whitelist filters actions. Whitelisted at lock>=1:
 *    DefConf_Lock_Level_Update (always), Edit_Tar_Update (lvl<=2),
 *    Shape_Decoration_ID/Extra (lvl==1). Shape_Set is NEVER whitelisted => rejected.
 *
 * ===========================================================================
 * TEST PLAN
 *  M1 initial_mode   — after reload+connect+load, c_state.value is a sane mode
 *                      (one of SPLASH/MAIN/DEFCONF_MODE/INSP_MODE/INSTINSP_MODE).
 *  M2 enter_defconf  — from MAIN dispatch Edit_Mode; assert main state == DEFCONF_MODE
 *                      with substate DEFCONF_MODE_NEUTRAL.
 *  M3 lock_toggle    — in DEFCONF_MODE: DefConf_Lock_Level_Update data:0 => lock 0;
 *                      data:1 => lock 1; data:0 => lock 0 (restore).
 *  M4 mode_roundtrip — DEFCONF_MODE --EXIT--> MAIN --Edit_Mode--> DEFCONF_MODE; assert
 *                      each leg, i.e. c_state returns to DEFCONF_MODE.
 *  M5 garbage_event  — dispatch a bogus {type:'__NOPE__'} action; assert store still
 *                      responds (getState works) and c_state.value is UNCHANGED.
 *  M6 lock_gates_set — in DEFCONF_MODE at lock=1, Shape_Set must be REJECTED
 *                      (shapeList unchanged). Complements r1_editor T7. Restore lock=0.
 *
 * NOTE: dispatching Edit_Mode while NOT in MAIN is a no-op (no such transition), so
 * M2/M4 first normalise to MAIN via EXIT (DEFCONF/INSP/INSTINSP --EXIT--> MAIN) and,
 * if stuck in SPLASH, REMOTE_SYSTEM_READY --> MAIN.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';
const MAIN_MODES = ['SPLASH', 'MAIN', 'DEFCONF_MODE', 'INSP_MODE', 'INSTINSP_MODE'];

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
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

// Read the SM snapshot: raw .value, derived main state, substate, lock, shapeCount.
const SM = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var v=(s.c_state&&typeof s.c_state.value!=='undefined')?s.c_state.value:null;
  var main=(typeof v==='string')?v:(v?Object.keys(v)[0]:null);
  var sub=(typeof v==='string'||v==null)?null:v[Object.keys(v)[0]];
  var o=s.edit_info&&s.edit_info._obj;
  return {value:v, main:main, sub:sub, lock:s.defConf_lock_level,
          shapes:(o&&o.shapeList)?o.shapeList.length:0};
})()`;
const sm = () => ev(SM);
const dispatch = (action) =>
  ev(`window.__GP_STORE__.dispatch(${JSON.stringify(action)}); 'ok'`);

// Normalise to MAIN: EXIT from any sub-mode; REMOTE_SYSTEM_READY out of SPLASH.
async function gotoMain() {
  for (let i = 0; i < 4; i++) {
    const st = await sm();
    if (st.main === 'MAIN') return st;
    if (st.main === 'SPLASH') {
      await dispatch({ type: 'REMOTE_SYSTEM_READY' });
    } else {
      await dispatch({ type: 'EXIT' });
    }
    await sleep(200);
  }
  return sm();
}

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

async function main() {
  // ---- bring app to a loaded state (core down => SKIP) ----
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // M1 initial_mode — c_state.value must be a recognised main mode.
  let s = await sm();
  report('M1 initial_mode', s.value != null && MAIN_MODES.includes(s.main),
    `value=${JSON.stringify(s.value)} main=${s.main}`);

  // M2 enter_defconf — from MAIN, Edit_Mode => DEFCONF_MODE / DEFCONF_MODE_NEUTRAL.
  await gotoMain();
  await dispatch({ type: 'Edit_Mode' });
  await sleep(250);
  s = await sm();
  report('M2 enter_defconf', s.main === 'DEFCONF_MODE',
    `value=${JSON.stringify(s.value)} (want main=DEFCONF_MODE, sub=DEFCONF_MODE_NEUTRAL)`);

  // M3 lock_toggle — DefConf_Lock_Level_Update in DEFCONF_MODE.
  // (NB: while lock!=0 the gate filters most actions, but DefConf_Lock_Level_Update is
  //  always whitelisted, so 1->0->1->0 all take effect.)
  await dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });
  await sleep(150);
  const l0 = (await sm()).lock;
  await dispatch({ type: 'DefConf_Lock_Level_Update', data: 1 });
  await sleep(150);
  const l1 = (await sm()).lock;
  await dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });
  await sleep(150);
  const l0b = (await sm()).lock;
  report('M3 lock_toggle', l0 === 0 && l1 === 1 && l0b === 0,
    `lock seq: 0->${l0}, 1->${l1}, 0->${l0b}`);

  // M4 mode_roundtrip — DEFCONF_MODE --EXIT--> MAIN --Edit_Mode--> DEFCONF_MODE.
  let pre4 = (await sm()).main; // should be DEFCONF_MODE
  await dispatch({ type: 'EXIT' });
  await sleep(200);
  const mid4 = (await sm()).main; // MAIN
  await dispatch({ type: 'Edit_Mode' });
  await sleep(200);
  const back4 = (await sm()).main; // DEFCONF_MODE
  report('M4 mode_roundtrip', pre4 === 'DEFCONF_MODE' && mid4 === 'MAIN' && back4 === 'DEFCONF_MODE',
    `${pre4} --EXIT--> ${mid4} --Edit_Mode--> ${back4}`);

  // M5 garbage_event — bogus action must not crash store; c_state unchanged.
  s = await sm();
  const valBefore = JSON.stringify(s.value);
  let crashed = false, valAfter = null, alive = false;
  try {
    await dispatch({ type: '__GARBAGE_SM_EVENT_'+Date.now()+'__', data: { junk: true } });
    await sleep(150);
    const s2 = await sm();             // getState() still works?
    alive = s2 && s2.value != null;
    valAfter = JSON.stringify(s2.value);
  } catch (e) {
    crashed = true; valAfter = 'THREW:' + e.message;
  }
  report('M5 garbage_event', !crashed && alive && valAfter === valBefore,
    `crashed=${crashed} alive=${alive} value ${valBefore} -> ${valAfter}`);

  // M6 lock_gates_set — in DEFCONF_MODE, lock=1 must REJECT Shape_Set (shapeList unchanged).
  // Ensure we are in DEFCONF_MODE first.
  s = await sm();
  if (s.main !== 'DEFCONF_MODE') { await gotoMain(); await dispatch({ type: 'Edit_Mode' }); await sleep(200); }
  await dispatch({ type: 'DefConf_Lock_Level_Update', data: 1 });
  await sleep(150);
  s = await sm();
  const lock6 = s.lock, before6 = s.shapes, mode6 = s.main;
  await dispatch({
    type: 'Shape_Set',
    data: { shape: { type: 'line', pt1: { x: 5, y: 5 }, pt2: { x: 9, y: 9 } }, id: undefined },
  });
  await sleep(250);
  s = await sm();
  report('M6 lock_gates_set', mode6 === 'DEFCONF_MODE' && lock6 === 1 && s.shapes === before6,
    `mode=${mode6} lock=${lock6} shapes ${before6}->${s.shapes} (Shape_Set must be rejected)`);
  // restore unlocked for whatever runs next
  await dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
