#!/usr/bin/env node
/*
 * QA R9 — ConsumeQueue (UTIL/structures.js).
 *
 * VIEWPOINT: ConsumeQueue is the queue underlying DB_WS send flow — a
 * promise-driven FIFO with a "consume_fn that resolves → kick next" loop and
 * an explicit termination/drain hook.
 *
 * SIGNATURES (verbatim from src/UTIL/structures.js):
 *   class ConsumeQueue {
 *     constructor(consumePromiseFunc, QSize=200, onTerminationState=(cq=>{}))
 *     size()
 *     f(idx)                  // peek at logical idx (newest side, via CircularCounter.f)
 *     enQ(data) -> bool       // false if full
 *     deQ()     -> data|undef
 *     head()    -> data|undef // peek tail (next to consume), does NOT remove
 *     termination()           // sets .term, fires onTerminationState ASAP if idle
 *     kick()                  // if !inPromise: calls consumePromiseFunc(this)
 *                             //   .then  -> inPromise=false; auto kick() if size!=0
 *                             //   .catch -> inPromise=false; NO auto re-kick
 *   }
 *
 *  IMPORTANT semantics found while reading source:
 *   - There is NO internal period_ms timer. The prompt's "consume after
 *     period_ms" assumption is WRONG — kick re-fires IMMEDIATELY (microtask)
 *     after resolve when size>0. We test that synchronous-microtask behavior.
 *   - enQ when full returns false; deQ on empty returns undefined.
 *   - head() is FIFO-peek (tail of CC, oldest) — same slot deQ would return.
 *   - termination() only fires onTerminationState when NOT inPromise; if mid-
 *     promise it fires after resolve/reject in _doTermAct. Items left in queue
 *     are NOT auto-rejected; on_termination callback receives the cq for the
 *     caller to drain. We record the actual behavior.
 *   - reject() permanently stops auto-kick until kick() is called manually.
 *
 * DEV HOOK: window.__GP_UTIL__.ConsumeQueue (per r4 follow-up proposal).
 *
 * TEST PLAN (core-INDEPENDENT, in-browser via __GP_UTIL__):
 *  - reach           : probe __GP_UTIL__.ConsumeQueue exists.
 *  - enqDeqBasic     : enQ 5 unique markers, deQ FIFO, size tracks.
 *  - headPeek        : head() returns oldest without removing; size unchanged.
 *  - emptyDeQ        : deQ on empty returns undefined, no throw.
 *  - kickConsume     : enQ + kick → consume_fn observed within ~period_ms.
 *  - resolveChains   : consume_fn resolve() drains the whole queue.
 *  - rejectStops     : consume_fn reject() at item N → no further consumes.
 *  - termDrains      : termination() fires onTerminationState exactly once.
 *  - enqDuringConsume: enQ from inside consume_fn → that item consumed next.
 *  - orderRapid100   : enQ 100 items, drain, assert FIFO order, no loss/dup.
 *
 * Idempotent. Exits non-zero on real failure.
 * Run: node tools/webctl/qa/r9_consumequeue.mjs
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
let skipped = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}
function skip(name, why) {
  console.log(`[${name}] SKIP — ${why}`);
  skipped++;
}

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_STORE__==='object' && typeof window.__GP_DIAG__==='object'"
    );
    if (ok === true) return true;
  }
  throw new Error('dev hooks never appeared — is app running with WEBCTL daemon?');
}

// Period for tests; short to keep total runtime small but big enough that the
// "no kick happened" assertion has signal.
const P = 40;

// All ConsumeQueue tests run as a single eval that returns a structured result.
// We tag a global namespace per test so they don't collide.
function makeNS(name) { return `__r9_${name}_${Date.now()}`; }

async function testEnqDeqBasic() {
  const NS = makeNS('basic');
  const js = `(async function(){
    var U = window.__GP_UTIL__;
    var seen = [];
    var cq = new U.ConsumeQueue(function(q){ return Promise.resolve(); }, 50);
    var items = ['a','b','c','d','e'].map(function(x){ return x+'-${NS}'; });
    items.forEach(function(it){ cq.enQ(it); });
    var size1 = cq.size();
    var out = [];
    while(cq.size()>0){ out.push(cq.deQ()); }
    return { size1: size1, out: out, expect: items };
  })()`;
  const r = await ev(js);
  const ok = r.size1 === 5 && JSON.stringify(r.out) === JSON.stringify(r.expect);
  report('enqDeqBasic', ok, `size=${r.size1} out=${JSON.stringify(r.out)}`);
}

async function testHeadPeek() {
  const NS = makeNS('head');
  const js = `(function(){
    var U = window.__GP_UTIL__;
    var cq = new U.ConsumeQueue(function(){return Promise.resolve();}, 10);
    cq.enQ('x-${NS}'); cq.enQ('y-${NS}');
    var h1 = cq.head();
    var s1 = cq.size();
    var h2 = cq.head();
    var s2 = cq.size();
    return { h1: h1, h2: h2, s1: s1, s2: s2 };
  })()`;
  const r = await ev(js);
  const ok = r.h1 === `x-${NS}` && r.h2 === `x-${NS}` && r.s1 === 2 && r.s2 === 2;
  report('headPeek', ok, JSON.stringify(r));
}

async function testEmptyDeQ() {
  const js = `(function(){
    try {
      var U = window.__GP_UTIL__;
      var cq = new U.ConsumeQueue(function(){return Promise.resolve();}, 5);
      var v = cq.deQ();
      return { v: v, threw: false };
    } catch(e) { return { threw: true, err: String(e) }; }
  })()`;
  const r = await ev(js);
  const ok = !r.threw && r.v === undefined;
  report('emptyDeQ', ok, JSON.stringify(r));
}

async function testKickConsume() {
  const NS = makeNS('kick');
  // Set up a CQ whose consume_fn deQs one item and records a marker; never
  // resolves so we observe only a single consume_fn invocation after kick().
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { calls: 0, items: [], pending: null };
    W.cq = new U.ConsumeQueue(function(q){
      W.calls++;
      W.items.push(q.deQ());
      return new Promise(function(res,rej){ W.pending = {res:res,rej:rej}; });
    }, 50);
    W.cq.enQ('m1-${NS}');
    W.cq.kick();
    return true;
  })()`;
  await ev(setup);
  await sleep(P);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    return { calls: W.calls, items: W.items };
  })()`);
  const ok = probe.calls === 1 && probe.items[0] === `m1-${NS}`;
  report('kickConsume', ok, JSON.stringify(probe));
}

async function testResolveChains() {
  const NS = makeNS('chain');
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { calls: 0, items: [] };
    W.cq = new U.ConsumeQueue(function(q){
      W.calls++;
      W.items.push(q.deQ());
      return Promise.resolve();
    }, 50);
    for(var i=0;i<5;i++) W.cq.enQ('c'+i+'-${NS}');
    W.cq.kick();
    return true;
  })()`;
  await ev(setup);
  await sleep(P * 3);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    return { calls: W.calls, items: W.items, size: W.cq.size() };
  })()`);
  const expected = [0,1,2,3,4].map(i => `c${i}-${NS}`);
  const ok = probe.calls === 5 && probe.size === 0 &&
             JSON.stringify(probe.items) === JSON.stringify(expected);
  report('resolveChains', ok, JSON.stringify(probe));
}

async function testRejectStops() {
  const NS = makeNS('reject');
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { calls: 0, items: [] };
    W.cq = new U.ConsumeQueue(function(q){
      W.calls++;
      W.items.push(q.deQ());
      // reject every consume → must NOT auto-kick again
      return Promise.reject(new Error('stop-${NS}'));
    }, 50);
    for(var i=0;i<5;i++) W.cq.enQ('r'+i+'-${NS}');
    W.cq.kick();
    return true;
  })()`;
  await ev(setup);
  await sleep(P * 3);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    return { calls: W.calls, size: W.cq.size() };
  })()`);
  // After 1st reject, no further consumes. So calls==1, queue still has 4.
  const ok = probe.calls === 1 && probe.size === 4;
  report('rejectStops', ok, JSON.stringify(probe));
}

async function testTermDrains() {
  const NS = makeNS('term');
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { termCalls: 0, drained: [], consumed: 0 };
    W.cq = new U.ConsumeQueue(function(q){
      W.consumed++;
      return Promise.resolve();
    }, 50, function(cq){
      W.termCalls++;
      // record what's left in the queue at termination time
      while(cq.size()>0) W.drained.push(cq.deQ());
    });
    for(var i=0;i<3;i++) W.cq.enQ('t'+i+'-${NS}');
    // call termination BEFORE kick → idle path; should fire immediately
    W.cq.termination();
    return true;
  })()`;
  await ev(setup);
  await sleep(P);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    return { termCalls: W.termCalls, drained: W.drained, consumed: W.consumed };
  })()`);
  // Per source: termination on idle CQ immediately calls onTerminationState
  // exactly once; consume_fn never invoked; queue items still present for
  // on_termination to drain manually.
  const ok = probe.termCalls === 1 && probe.consumed === 0 && probe.drained.length === 3;
  report('termDrains', ok, JSON.stringify(probe));

  // Bonus: termination is idempotent — second call must not re-fire callback.
  await ev(`window['${NS}'].cq.termination();`);
  await sleep(P);
  const p2 = await ev(`window['${NS}'].termCalls`);
  report('termDrains.idempotent', p2 === 1, `termCalls=${p2}`);
}

async function testEnqDuringConsume() {
  const NS = makeNS('eqdc');
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { calls: 0, items: [], injected: false };
    W.cq = new U.ConsumeQueue(function(q){
      W.calls++;
      W.items.push(q.deQ());
      if(!W.injected){
        W.injected = true;
        q.enQ('INJECTED-${NS}');
      }
      return Promise.resolve();
    }, 50);
    W.cq.enQ('first-${NS}');
    W.cq.kick();
    return true;
  })()`;
  await ev(setup);
  await sleep(P * 3);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    return { calls: W.calls, items: W.items, size: W.cq.size() };
  })()`);
  const ok = probe.calls === 2 && probe.size === 0 &&
             probe.items[0] === `first-${NS}` &&
             probe.items[1] === `INJECTED-${NS}`;
  report('enqDuringConsume', ok, JSON.stringify(probe));
}

async function testOrderRapid100() {
  const NS = makeNS('rapid');
  const setup = `(function(){
    var U = window.__GP_UTIL__;
    var W = window['${NS}'] = { items: [] };
    W.cq = new U.ConsumeQueue(function(q){
      W.items.push(q.deQ());
      return Promise.resolve();
    }, 200);
    for(var i=0;i<100;i++) W.cq.enQ(i);
    W.cq.kick();
    return true;
  })()`;
  await ev(setup);
  // 100 microtask-chained consumes — very fast; give it generous slack.
  await sleep(P * 5);
  const probe = await ev(`(function(){
    var W = window['${NS}'];
    var ok = W.items.length === 100;
    for(var i=0;i<W.items.length;i++){ if(W.items[i]!==i){ ok=false; break; } }
    return { len: W.items.length, ok: ok, size: W.cq.size(),
             first: W.items[0], last: W.items[W.items.length-1] };
  })()`);
  report('orderRapid100', probe.ok && probe.size === 0, JSON.stringify(probe));
}

async function probeReachability() {
  try {
    const p = await ev(`(function(){
      return {
        hasUtil: typeof window.__GP_UTIL__==='object' && window.__GP_UTIL__!==null,
        hasCQ: !!(window.__GP_UTIL__ && window.__GP_UTIL__.ConsumeQueue),
        keys: window.__GP_UTIL__ ? Object.keys(window.__GP_UTIL__) : []
      };
    })()`);
    console.log(`[reach] __GP_UTIL__=${p.hasUtil} ConsumeQueue=${p.hasCQ} keys=[${p.keys.join(',')}]`);
    return p;
  } catch (e) {
    console.log(`[reach] probe failed: ${e.message}`);
    return { hasUtil: false, hasCQ: false };
  }
}

async function main() {
  try { await waitReady(); }
  catch (e) {
    console.log(`[fatal] daemon/app unavailable: ${e.message}`);
    process.exit(1);
  }
  const reach = await probeReachability();
  if (!reach.hasCQ) {
    skip('all', '__GP_UTIL__.ConsumeQueue not exposed — parent must add hook in src/script.jsx');
    console.log(`${failures ? failures + ' FAIL' : 'ALL PASS'} (${skipped} skipped)`);
    process.exit(failures ? 1 : 0);
  }

  try { await testEnqDeqBasic(); } catch (e) { report('enqDeqBasic', false, 'threw '+e.message); }
  try { await testHeadPeek(); } catch (e) { report('headPeek', false, 'threw '+e.message); }
  try { await testEmptyDeQ(); } catch (e) { report('emptyDeQ', false, 'threw '+e.message); }
  try { await testKickConsume(); } catch (e) { report('kickConsume', false, 'threw '+e.message); }
  try { await testResolveChains(); } catch (e) { report('resolveChains', false, 'threw '+e.message); }
  try { await testRejectStops(); } catch (e) { report('rejectStops', false, 'threw '+e.message); }
  try { await testTermDrains(); } catch (e) { report('termDrains', false, 'threw '+e.message); }
  try { await testEnqDuringConsume(); } catch (e) { report('enqDuringConsume', false, 'threw '+e.message); }
  try { await testOrderRapid100(); } catch (e) { report('orderRapid100', false, 'threw '+e.message); }

  console.log(failures ? `${failures} test(s) FAILED, ${skipped} skipped` : `ALL PASS (${skipped} skipped)`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
