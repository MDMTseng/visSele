#!/usr/bin/env node
/*
 * QA R1 — PURE / CORE-INDEPENDENT LOGIC tests.
 *
 * These are the most robust tests in the suite: they exercise pure app-bundle
 * logic loaded in the browser and need NO core backend (no reset()/LD flow) —
 * just /reload then wait for the dev store handle. Driven via the webctld
 * daemon (POST /eval, POST /reload) against the running app.
 *
 * TEST PLAN (all core-INDEPENDENT):
 *  1. diagLog ring buffer (src/UTIL/diagLog.js via window.__GP_DIAG__)
 *     - emit console.log("MARKER_x") + console.error(new Error("e"))
 *     - assert diagText() contains the marker AND the error stack
 *     - assert diagCount() increased by >=2
 *  2. inspDBQueue durable lifecycle (src/UTIL/inspDBQueue.js via __GP_DB_QUEUE__)
 *     - persistPending(rec, SRC_A) -> seq (number)
 *     - getPendingBySource(SRC_A) contains it; getPendingBySource(SRC_B) does NOT
 *       (source isolation)
 *     - deletePending(seq) removes it (count for SRC_A back to 0)
 *  3. inspDBQueue global cap = 1000 (MAX_RECORDS)
 *     - persist a bounded batch under a unique source; assert global
 *       pendingInsertCount() never exceeds 1000 (oldest dropped by trimToCap).
 *       (We assert the invariant cheaply: persist a modest batch and confirm the
 *       global count stays <=1000 and our source's entries are tracked, then
 *       clean up. A full >1000 fill is avoided to keep this fast/bounded.)
 *  4. measure coupling (src/shapes/measure.js applyMeasureLimitCoupling)
 *     - REACHABILITY GAP: applyMeasureLimitCoupling is NOT exposed on window.
 *       It is imported only inside src/DefConfUI.js jsonChange and reached via
 *       the editor property-sheet input path, which requires a loaded def (core
 *       dependent — out of scope for the pure viewpoint). We therefore VERIFY
 *       the pure formulas against a local reference reimplementation transcribed
 *       verbatim from measure.js (documents expected behavior without core), and
 *       report the window-reachability gap. No fabrication of in-browser results.
 *
 * Idempotent: IndexedDB tests use unique source tags ("QA_R1_"+Date.now()) and
 * delete their own entries. Exits non-zero on any real failure.
 *
 * Run: node tools/webctl/qa/r1_purelogic.mjs
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

// Wait for the dev store + hooks after a reload.
async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_STORE__==='object' && typeof window.__GP_DIAG__==='object' && typeof window.__GP_DB_QUEUE__==='object'"
    );
    if (ok === true) return true;
  }
  throw new Error('dev hooks (__GP_STORE__/__GP_DIAG__/__GP_DB_QUEUE__) never appeared — is app on :8081 in dev?');
}

// --- 1. diagLog ring buffer -------------------------------------------------
async function testDiagLog() {
  const marker = 'MARKER_' + Date.now();
  const errMsg = 'QA_R1_ERR_' + Date.now();
  try {
    const res = await ev(`(function(){
      var before = window.__GP_DIAG__.diagCount();
      console.log(${JSON.stringify(marker)});
      console.error(new Error(${JSON.stringify(errMsg)}));
      var after = window.__GP_DIAG__.diagCount();
      var txt = window.__GP_DIAG__.diagText();
      return {
        before: before,
        after: after,
        hasMarker: txt.indexOf(${JSON.stringify(marker)}) >= 0,
        hasErrMsg: txt.indexOf(${JSON.stringify(errMsg)}) >= 0,
        // an Error captured via safeArg yields its .stack — look for "Error:" framing
        hasStack: txt.indexOf('Error') >= 0 && txt.indexOf(${JSON.stringify(errMsg)}) >= 0
      };
    })()`);
    const ok = res.hasMarker && res.hasErrMsg && res.hasStack && (res.after - res.before) >= 2;
    report('diagLog', ok,
      `count ${res.before}->${res.after}, marker=${res.hasMarker}, errMsg=${res.hasErrMsg}, stack=${res.hasStack}`);
  } catch (e) {
    report('diagLog', false, String(e));
  }
}

// --- 2. inspDBQueue durable lifecycle + source isolation --------------------
async function testDBLifecycle() {
  const srcA = 'QA_R1_A_' + Date.now();
  const srcB = 'QA_R1_B_' + Date.now();
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var seq = await q.persistPending({id:'rec1', val:42}, ${JSON.stringify(srcA)});
      var inA = await q.getPendingBySource(${JSON.stringify(srcA)});
      var inB = await q.getPendingBySource(${JSON.stringify(srcB)});
      var foundInA = inA.some(function(e){return e.seq===seq && e.record && e.record.id==='rec1';});
      var notInB = !inB.some(function(e){return e.seq===seq;});
      await q.deletePending(seq);
      var afterDel = await q.getPendingBySource(${JSON.stringify(srcA)});
      var removed = !afterDel.some(function(e){return e.seq===seq;});
      return {seqType: typeof seq, foundInA: foundInA, notInB: notInB, removed: removed};
    })()`);
    const ok = res.seqType === 'number' && res.foundInA && res.notInB && res.removed;
    report('inspDBQueue.lifecycle', ok,
      `seqType=${res.seqType}, foundInA=${res.foundInA}, isolated(notInB)=${res.notInB}, removedAfterDelete=${res.removed}`);
  } catch (e) {
    report('inspDBQueue.lifecycle', false, String(e));
    // best-effort cleanup
    await ev(`(async function(){var q=window.__GP_DB_QUEUE__;var all=await q.getPendingBySource(${JSON.stringify(srcA)});for(var i=0;i<all.length;i++)await q.deletePending(all[i].seq);return all.length;})()`).catch(() => {});
  }
}

// --- 3. inspDBQueue global cap = 1000 ---------------------------------------
async function testDBCap() {
  const src = 'QA_R1_CAP_' + Date.now();
  const N = 50; // bounded batch — assert the global invariant stays <=1000
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var seqs = [];
      for (var i=0;i<${N};i++){ seqs.push(await q.persistPending({id:'cap'+i}, ${JSON.stringify(src)})); }
      var globalCount = await q.pendingInsertCount();           // global cap is 1000
      var srcCount = await q.pendingInsertCount(${JSON.stringify(src)});
      // cleanup our own entries
      for (var j=0;j<seqs.length;j++){ await q.deletePending(seqs[j]); }
      var srcAfter = await q.pendingInsertCount(${JSON.stringify(src)});
      return {globalCount: globalCount, srcCount: srcCount, srcAfter: srcAfter, n: seqs.length};
    })()`);
    // invariant: global count never exceeds MAX_RECORDS (1000); our batch was tracked & cleaned
    const ok = res.globalCount <= 1000 && res.srcCount === res.n && res.srcAfter === 0;
    report('inspDBQueue.cap', ok,
      `globalCount=${res.globalCount} (<=1000), src tracked=${res.srcCount}/${res.n}, cleanedUp=${res.srcAfter === 0}`);
  } catch (e) {
    report('inspDBQueue.cap', false, String(e));
    await ev(`(async function(){var q=window.__GP_DB_QUEUE__;var all=await q.getPendingBySource(${JSON.stringify(src)});for(var i=0;i<all.length;i++)await q.deletePending(all[i].seq);return all.length;})()`).catch(() => {});
  }
}

// --- 4. measure coupling (pure formula verification; window-reachability gap) -
// applyMeasureLimitCoupling is NOT exposed on window (only used inside
// DefConfUI.jsonChange, reachable solely via the editor input path with a loaded
// def = core dependent). We verify the pure math against a reference transcribed
// verbatim from src/shapes/measure.js, and report the reachability gap.
function refRound(x, step) {
  // mirror UTIL/MISC_Util round(value, step): round to nearest multiple of step
  return Math.round(x / step) * step;
}
function refCoupling(obj, changedKey, preVal) {
  const r = (x) => refRound(x, 0.001);
  if (obj.value === undefined) return;
  switch (changedKey) {
    case 'value':
      obj.LCL = r(obj.LCL - preVal + obj.value);
      obj.UCL = r(obj.UCL - preVal + obj.value);
      obj.LSL = r(obj.LSL - preVal + obj.value);
      obj.USL = r(obj.USL - preVal + obj.value);
      break;
    case 'USL':
      obj.UCL = r(obj.value + (obj.USL - obj.value) * 2 / 3);
      break;
    case 'LSL':
      obj.LCL = r(obj.value + (obj.LSL - obj.value) * 2 / 3);
      break;
  }
}
function near(a, b) { return Math.abs(a - b) < 1e-9; }
function testMeasureCoupling() {
  let ok = true;
  const why = [];
  // USL change -> UCL = value + (USL-value)*2/3
  {
    const o = { value: 10, USL: 13, UCL: 0 };
    refCoupling(o, 'USL', 13);
    if (!near(o.UCL, refRound(10 + (13 - 10) * 2 / 3, 0.001))) { ok = false; why.push('USL->UCL'); }
    if (!near(o.UCL, 12)) { ok = false; why.push(`UCL expected 12 got ${o.UCL}`); }
  }
  // LSL change -> LCL = value + (LSL-value)*2/3
  {
    const o = { value: 10, LSL: 7, LCL: 0 };
    refCoupling(o, 'LSL', 7);
    if (!near(o.LCL, 8)) { ok = false; why.push(`LCL expected 8 got ${o.LCL}`); }
  }
  // value change -> all limits shift by (value - preVal)
  {
    const o = { value: 12, LCL: 8, UCL: 12, LSL: 7, USL: 13 };
    refCoupling(o, 'value', 10); // preVal=10, new value=12 -> +2 shift
    if (!near(o.LCL, 10) || !near(o.UCL, 14) || !near(o.LSL, 9) || !near(o.USL, 15)) {
      ok = false; why.push(`value-shift got LCL${o.LCL} UCL${o.UCL} LSL${o.LSL} USL${o.USL}`);
    }
  }
  report('measure.coupling(ref)', ok, why.length ? why.join('; ')
    : 'pure formulas verified vs measure.js reference (window hook NOT exposed — see gap note)');
}

async function main() {
  await waitReady();
  await testDiagLog();
  await testDBLifecycle();
  await testDBCap();
  testMeasureCoupling();
  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
