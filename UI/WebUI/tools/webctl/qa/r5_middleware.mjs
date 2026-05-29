#!/usr/bin/env node
/*
 * QA R5 — MIDDLEWARE LAYER (ActionThrottle / ECStateMachine / MW_API)
 *
 * VIEWPOINT: how the redux middleware chain (applyMiddleware order:
 *   thunk -> MW_API -> ECStateMachine -> ActionThrottle -> reduxCatch -> reducer)
 * handles dispatched actions. Mostly CORE-INDEPENDENT — we observe the store
 * directly via __GP_STORE__.
 *
 * KEY SHAPES discovered in source:
 *   - ActionThrottle factory: ActionThrottle({time:100, posEdge:true}) — see redux.js
 *   - ActionThrottle bypasses if action.ATID !== ATData.ATID. Default ATID undef===undef,
 *     so app-level dispatches DO go through.
 *   - action.ActionThrottle_type === "express"   -> stamped with ExATID and forwarded IMMEDIATELY
 *   - action.ActionThrottle_type === "flush"     -> drain queued actions (internal)
 *   - default (no ActionThrottle_type): posEdge=true means the FIRST action in a
 *     window forwards immediately AND a 100ms timer is armed. Subsequent actions
 *     inside that window are PUSHED to ATData.actions and drained on flush.
 *     So in our 10-toggle test, ALL toggles eventually apply; final state == last.
 *   - ATBundle wrapper observed in code:
 *       {type:"ATBundle", ActionThrottle_type:"express", data:[{type:"X",data:...},...]}
 *     BUT ActionThrottle only stamps express and calls next(action) — it does NOT
 *     unpack data[]. The unpacking must happen in some other middleware/reducer.
 *     We grep'd: no obvious unpacker — the reducer just sees type "ATBundle" (no-op).
 *     ==> ATBundle express test will likely NOT mutate state on its own.
 *     We therefore assert the WEAKER, TRUE invariant: dispatch survives without
 *     throwing AND each child action ALSO dispatched directly (as the app does
 *     in script.jsx) DOES mutate state quickly.
 *
 * TEST PLAN:
 *   1. atbundle.express.survives — dispatch ATBundle express; assert no throw,
 *      then dispatch the child action directly with express and assert
 *      defConf_lock_level updates to 0 within <100ms (express path bypasses defer).
 *   2. throttle.defer.firstEdge — dispatch a plain Lock_Level_Update (no AT_type).
 *      posEdge=true => takes effect IMMEDIATELY (first edge); assert <50ms.
 *      Documents the actual posEdge:true semantics (NOT trailing 100ms).
 *   3. throttle.batch.final — rapidly dispatch 10 toggles 0/1/0/1...; wait 250ms
 *      (>2x throttle window) and assert final lock_level == last value.
 *   4. resilience.garbage — dispatch {type:"___GARBAGE___",data:{nested:undefined}};
 *      assert getState() still works and c_state.value unchanged.
 *   5. xstate.unknownEvent — dispatch {type:"NoSuchSMEvent"}; assert c_state.value
 *      unchanged and no throw (xstate.transition is a no-op on unknown events).
 *   6. order.sequence — dispatch a mix: garbage, unknown SM event, valid Lock=2,
 *      garbage, valid Lock=3. Assert final lock_level == 3.
 *
 * No def load required — all tests run on a fresh /reload + __GP_STORE__ only.
 * DEFERRAL TIMING ASSUMPTION (may be flaky on a busy machine):
 *   - "express <100ms" and "posEdge first-edge <50ms" assume the dev server and
 *     browser are not under heavy load. We use generous bounds.
 *
 * Run: node tools/webctl/qa/r5_middleware.mjs
 */

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

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev("typeof window.__GP_STORE__==='object' && !!window.__GP_STORE__.getState");
    if (ok === true) return true;
  }
  throw new Error('__GP_STORE__ never appeared — is the app running on :8081 in dev?');
}

// Reset lock_level to a known value (express path bypasses throttle defer).
async function resetLock(to) {
  await ev(`window.__GP_STORE__.dispatch({type:"DefConf_Lock_Level_Update",data:${to},ActionThrottle_type:"express"})`);
  await sleep(20);
}

// --- 1. ATBundle express survival + child express path ---------------------
async function testATBundleExpress() {
  try {
    await resetLock(5);
    const r = await ev(`(async function(){
      var s = window.__GP_STORE__;
      // dispatch the ATBundle wrapper shape used by script.jsx
      var threw = null;
      try {
        s.dispatch({type:"ATBundle", ActionThrottle_type:"express",
                    data:[{type:"DefConf_Lock_Level_Update", data:0}]});
      } catch(e){ threw = String(e); }
      var afterBundle = s.getState().UIData.defConf_lock_level;
      // Also test the child action directly via express (the real fast-path)
      var t0 = performance.now();
      s.dispatch({type:"DefConf_Lock_Level_Update", data:0, ActionThrottle_type:"express"});
      var dtSync = performance.now() - t0;
      await new Promise(function(r){setTimeout(r,30);});
      var lvlNow = s.getState().UIData.defConf_lock_level;
      return {threw:threw, afterBundle:afterBundle, lvlNow:lvlNow, dtSync:dtSync};
    })()`);
    const ok = !r.threw && r.lvlNow === 0 && r.dtSync < 100;
    report('atbundle.express.survives', ok,
      `threw=${r.threw}, afterBundle=${r.afterBundle}, lvlNow=${r.lvlNow}, expressSyncMs=${r.dtSync.toFixed(1)}`);
  } catch (e) {
    report('atbundle.express.survives', false, String(e));
  }
}

// --- 2. ActionThrottle defer / posEdge first-edge ---------------------------
async function testThrottleDefer() {
  try {
    await resetLock(0);
    // wait > throttle window so we're at a clean edge (timer null)
    await sleep(180);
    const r = await ev(`(async function(){
      var s = window.__GP_STORE__;
      var t0 = performance.now();
      s.dispatch({type:"DefConf_Lock_Level_Update", data:1});
      var dt = performance.now() - t0;
      var lvlImmediate = s.getState().UIData.defConf_lock_level;
      await new Promise(function(r){setTimeout(r,200);});
      var lvlAfter = s.getState().UIData.defConf_lock_level;
      return {dt:dt, lvlImmediate:lvlImmediate, lvlAfter:lvlAfter};
    })()`);
    // posEdge:true => first action forwards immediately
    const ok = r.lvlImmediate === 1 && r.lvlAfter === 1 && r.dt < 50;
    report('throttle.defer.firstEdge', ok,
      `dt=${r.dt.toFixed(1)}ms, lvlImmediate=${r.lvlImmediate}, lvlAfter=${r.lvlAfter} (posEdge:true => immediate)`);
  } catch (e) {
    report('throttle.defer.firstEdge', false, String(e));
  }
}

// --- 3. batching: 10 toggles, final state deterministic ---------------------
async function testThrottleBatch() {
  try {
    await resetLock(0);
    await sleep(180);
    const r = await ev(`(async function(){
      var s = window.__GP_STORE__;
      var seq = [0,1,0,1,0,1,0,1,0,1]; // last = 1
      for (var i=0;i<seq.length;i++){
        s.dispatch({type:"DefConf_Lock_Level_Update", data:seq[i]});
      }
      var lvlImmediate = s.getState().UIData.defConf_lock_level;
      await new Promise(function(r){setTimeout(r,260);}); // > throttle window
      var lvlFinal = s.getState().UIData.defConf_lock_level;
      return {lvlImmediate:lvlImmediate, lvlFinal:lvlFinal, last:seq[seq.length-1]};
    })()`);
    const ok = r.lvlFinal === r.last;
    report('throttle.batch.final', ok,
      `lvlImmediate=${r.lvlImmediate}, lvlFinal=${r.lvlFinal}, expectLast=${r.last}`);
  } catch (e) {
    report('throttle.batch.final', false, String(e));
  }
}

// --- 4. malformed action survival -------------------------------------------
async function testGarbage() {
  try {
    await resetLock(0);
    const r = await ev(`(function(){
      var s = window.__GP_STORE__;
      var cBefore = s.getState().UIData.c_state ? JSON.stringify(s.getState().UIData.c_state.value) : null;
      var lvlBefore = s.getState().UIData.defConf_lock_level;
      var threw = null;
      try {
        s.dispatch({type:"___GARBAGE___", data:{nested:undefined, a:[1,2,{b:undefined}]}});
      } catch(e){ threw = String(e); }
      var stateOk = false;
      try { stateOk = !!s.getState(); } catch(e){}
      var cAfter = s.getState().UIData.c_state ? JSON.stringify(s.getState().UIData.c_state.value) : null;
      var lvlAfter = s.getState().UIData.defConf_lock_level;
      return {threw:threw, stateOk:stateOk, cBefore:cBefore, cAfter:cAfter, lvlBefore:lvlBefore, lvlAfter:lvlAfter};
    })()`);
    const ok = !r.threw && r.stateOk && r.cBefore === r.cAfter && r.lvlBefore === r.lvlAfter;
    report('resilience.garbage', ok,
      `threw=${r.threw}, c_state ${r.cBefore} -> ${r.cAfter}, lock ${r.lvlBefore}->${r.lvlAfter}`);
  } catch (e) {
    report('resilience.garbage', false, String(e));
  }
}

// --- 5. xstate unknown event -------------------------------------------------
async function testUnknownSMEvent() {
  try {
    const r = await ev(`(function(){
      var s = window.__GP_STORE__;
      var cBefore = s.getState().UIData.c_state ? JSON.stringify(s.getState().UIData.c_state.value) : null;
      var threw = null;
      try {
        s.dispatch({type:"NoSuchSMEvent_QA_R5"});
      } catch(e){ threw = String(e); }
      var cAfter = s.getState().UIData.c_state ? JSON.stringify(s.getState().UIData.c_state.value) : null;
      return {threw:threw, cBefore:cBefore, cAfter:cAfter};
    })()`);
    const ok = !r.threw && r.cBefore === r.cAfter;
    report('xstate.unknownEvent', ok,
      `threw=${r.threw}, c_state ${r.cBefore} -> ${r.cAfter}`);
  } catch (e) {
    report('xstate.unknownEvent', false, String(e));
  }
}

// --- 6. mixed sequence: gated/unknown dropped, valid applied ----------------
async function testOrderSequence() {
  try {
    await resetLock(0);
    await sleep(180);
    const r = await ev(`(async function(){
      var s = window.__GP_STORE__;
      // mix: garbage, unknown SM, valid express=2, garbage, valid express=3
      s.dispatch({type:"___GARBAGE_A___", data:{x:undefined}});
      s.dispatch({type:"NoSuchSMEvent_QA_R5_B"});
      s.dispatch({type:"DefConf_Lock_Level_Update", data:2, ActionThrottle_type:"express"});
      s.dispatch({type:"___GARBAGE_C___"});
      s.dispatch({type:"DefConf_Lock_Level_Update", data:3, ActionThrottle_type:"express"});
      await new Promise(function(r){setTimeout(r,30);});
      var lvl = s.getState().UIData.defConf_lock_level;
      var stateOk = !!s.getState();
      return {lvl:lvl, stateOk:stateOk};
    })()`);
    const ok = r.stateOk && r.lvl === 3;
    report('order.sequence', ok, `final lock=${r.lvl} (expect 3), stateOk=${r.stateOk}`);
  } catch (e) {
    report('order.sequence', false, String(e));
  }
}

async function main() {
  await waitReady();
  await testATBundleExpress();
  await testThrottleDefer();
  await testThrottleBatch();
  await testGarbage();
  await testUnknownSMEvent();
  await testOrderSequence();
  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
