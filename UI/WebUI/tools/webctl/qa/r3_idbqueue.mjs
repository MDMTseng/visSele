#!/usr/bin/env node
/*
 * QA R3 — IndexedDB durable insert-queue, DEEP (src/UTIL/inspDBQueue.js via
 * window.__GP_DB_QUEUE__). CORE-INDEPENDENT: needs only /reload + the dev hook,
 * no core backend / reset() / LD flow. Driven via webctld (POST /eval,/reload).
 *
 * The durable mirror persists not-yet-confirmed DB inserts to IndexedDB
 * (db "visSele_local", store "pending_inserts" v2, keyPath seq autoIncrement,
 * global cap MAX_RECORDS=1000, oldest dropped). The WHOLE POINT is survival
 * across a page reload, so the central test actually reloads the page.
 *
 * TEST PLAN:
 *  1. durability-across-reload (the headline test)
 *     - persistPending(rec, SRC_DUR) -> seq (number); capture seq.
 *     - POST /reload; waitReady for the hook to reappear.
 *     - getPendingBySource(SRC_DUR) STILL contains {seq, record} — proves the
 *       IndexedDB mirror survived a full page reload (in-memory state is gone,
 *       so a hit here can only come from disk). Then deletePending(seq) cleanup;
 *       confirm gone.
 *  2. source-isolation
 *     - persist under SRC_A and SRC_B; getPendingBySource(A) contains only A's,
 *       (B) only B's; cross-check pendingInsertCount(A)/(B). Clean up both.
 *  3. delete-confirm
 *     - persist -> seq; deletePending(seq); assert getPendingBySource empty for
 *       that source (entry gone).
 *  4. cap-invariant (bounded; NOT a full eviction run)
 *     - persist a modest batch (20) under a unique source; assert the GLOBAL
 *       invariant pendingInsertCount() <= 1000 (MAX_RECORDS) holds and our batch
 *       is fully tracked (count==20), then clean up (count->0).
 *     - COVERAGE GAP: actually triggering trimToCap eviction needs >1000 live
 *       inserts, which is slow/heavy against the shared browser; we assert the
 *       invariant cheaply instead of forcing eviction. Documented, not run.
 *  5. bad-input-safety (best-effort)
 *     - persistPending(undefined) should not throw / crash the page (record may
 *       be stored as undefined; we only require no exception + page still alive).
 *     - deletePending(null) is an explicit no-op (guarded by `seq == null`).
 *     - Clean up any stray entry the undefined-persist may have created via its
 *       returned seq.
 *
 * CLEANUP: unique source tags ("QA_R3_"+Date.now()+rand); a final finally-block
 * sweeps every QA_R3_ source seen and deletes leftovers so the store is left
 * empty of QA data even on failure.
 *
 * CONSTRAINTS: ONE shared browser, parent runs serially — do NOT launch against
 * the live daemon concurrently. Self-contained.
 *
 * Run: node tools/webctl/qa/r3_idbqueue.mjs
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

// unique tag generator + registry of every source we touch (for final sweep)
const rnd = () => Math.random().toString(36).slice(2, 8);
const touchedSources = new Set();
function tag(label) {
  const s = 'QA_R3_' + label + '_' + Date.now() + '_' + rnd();
  touchedSources.add(s);
  return s;
}

// Wait for the dev DB-queue hook after a reload.
async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev("typeof window.__GP_DB_QUEUE__==='object' && window.__GP_DB_QUEUE__!==null");
    if (ok === true) return true;
  }
  throw new Error('dev hook __GP_DB_QUEUE__ never appeared — is app on :8081 in dev?');
}

// --- 1. durability across reload (headline) --------------------------------
async function testDurability() {
  const src = tag('DUR');
  try {
    // persist BEFORE reload
    const seq = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      return await q.persistPending({id:'durRec', val:7}, ${JSON.stringify(src)});
    })()`);
    const seqIsNum = typeof seq === 'number';

    // full page reload — wipes all in-memory state; only IndexedDB survives
    await waitReady();

    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var rows = await q.getPendingBySource(${JSON.stringify(src)});
      var hit = rows.find(function(e){return e.seq===${JSON.stringify(seq)};});
      return {
        survived: !!hit,
        recOk: !!(hit && hit.record && hit.record.id==='durRec' && hit.record.val===7)
      };
    })()`);

    // cleanup + confirm gone
    const gone = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      await q.deletePending(${JSON.stringify(seq)});
      var rows = await q.getPendingBySource(${JSON.stringify(src)});
      return rows.length===0;
    })()`);

    const ok = seqIsNum && res.survived && res.recOk && gone;
    report('durability-across-reload', ok,
      `seqNum=${seqIsNum}, survivedReload=${res.survived}, recordIntact=${res.recOk}, cleaned=${gone}`);
  } catch (e) {
    report('durability-across-reload', false, String(e));
  }
}

// --- 2. source isolation ----------------------------------------------------
async function testIsolation() {
  const srcA = tag('A');
  const srcB = tag('B');
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var a1 = await q.persistPending({id:'a1'}, ${JSON.stringify(srcA)});
      var a2 = await q.persistPending({id:'a2'}, ${JSON.stringify(srcA)});
      var b1 = await q.persistPending({id:'b1'}, ${JSON.stringify(srcB)});
      var inA = await q.getPendingBySource(${JSON.stringify(srcA)});
      var inB = await q.getPendingBySource(${JSON.stringify(srcB)});
      var cntA = await q.pendingInsertCount(${JSON.stringify(srcA)});
      var cntB = await q.pendingInsertCount(${JSON.stringify(srcB)});
      var aOnlyA = inA.length===2 && inA.every(function(e){return e.record.id==='a1'||e.record.id==='a2';});
      var bOnlyB = inB.length===1 && inB[0].record.id==='b1';
      var noCross = !inA.some(function(e){return e.seq===b1;}) && !inB.some(function(e){return e.seq===a1||e.seq===a2;});
      // cleanup both sources
      for (var i=0;i<inA.length;i++) await q.deletePending(inA[i].seq);
      for (var j=0;j<inB.length;j++) await q.deletePending(inB[j].seq);
      var afterA = await q.pendingInsertCount(${JSON.stringify(srcA)});
      var afterB = await q.pendingInsertCount(${JSON.stringify(srcB)});
      return {aOnlyA:aOnlyA, bOnlyB:bOnlyB, noCross:noCross, cntA:cntA, cntB:cntB, afterA:afterA, afterB:afterB};
    })()`);
    const ok = res.aOnlyA && res.bOnlyB && res.noCross && res.cntA === 2 && res.cntB === 1
      && res.afterA === 0 && res.afterB === 0;
    report('source-isolation', ok,
      `A={got ${res.cntA}/2}, B={got ${res.cntB}/1}, noCross=${res.noCross}, cleaned(A=${res.afterA},B=${res.afterB})`);
  } catch (e) {
    report('source-isolation', false, String(e));
  }
}

// --- 3. delete confirm ------------------------------------------------------
async function testDelete() {
  const src = tag('DEL');
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var seq = await q.persistPending({id:'del'}, ${JSON.stringify(src)});
      var before = (await q.getPendingBySource(${JSON.stringify(src)})).length;
      await q.deletePending(seq);
      var after = (await q.getPendingBySource(${JSON.stringify(src)}));
      return {before:before, after:after.length, gone:!after.some(function(e){return e.seq===seq;})};
    })()`);
    const ok = res.before === 1 && res.after === 0 && res.gone;
    report('delete-confirm', ok, `before=${res.before}, after=${res.after}, gone=${res.gone}`);
  } catch (e) {
    report('delete-confirm', false, String(e));
  }
}

// --- 4. cap invariant (bounded; not a full eviction run) --------------------
async function testCapInvariant() {
  const src = tag('CAP');
  const N = 20;
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var seqs = [];
      for (var i=0;i<${N};i++){ seqs.push(await q.persistPending({id:'cap'+i}, ${JSON.stringify(src)})); }
      var globalCount = await q.pendingInsertCount();
      var srcCount = await q.pendingInsertCount(${JSON.stringify(src)});
      for (var j=0;j<seqs.length;j++){ await q.deletePending(seqs[j]); }
      var srcAfter = await q.pendingInsertCount(${JSON.stringify(src)});
      return {globalCount:globalCount, srcCount:srcCount, srcAfter:srcAfter, n:seqs.length};
    })()`);
    const ok = res.globalCount <= 1000 && res.srcCount === res.n && res.srcAfter === 0;
    report('cap-invariant', ok,
      `globalCount=${res.globalCount} (<=1000), tracked=${res.srcCount}/${res.n}, cleaned=${res.srcAfter === 0}`
      + ' [GAP: >1000 eviction not exercised — heavy/slow]');
  } catch (e) {
    report('cap-invariant', false, String(e));
  }
}

// --- 5. bad-input safety (best-effort) --------------------------------------
async function testBadInput() {
  try {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var out = {persistThrew:false, deleteNullThrew:false, alive:false, strayDeleted:true, persistSeq:null};
      // persistPending(undefined) — must not crash the page
      try {
        out.persistSeq = await q.persistPending(undefined, "QA_R3_BAD_undef");
      } catch(e){ out.persistThrew = true; out.persistErr = String(e); }
      // deletePending(null) — explicit no-op
      try { await q.deletePending(null); } catch(e){ out.deleteNullThrew = true; }
      // clean up the stray undef record if one was created
      if (typeof out.persistSeq === 'number') {
        try { await q.deletePending(out.persistSeq); } catch(e){ out.strayDeleted = false; }
      }
      // page still alive / hook still usable
      out.alive = typeof window.__GP_DB_QUEUE__ === 'object' && typeof q.pendingInsertCount === 'function';
      return out;
    })()`);
    // success = no crash from undefined-persist, null-delete is a no-op, page alive
    const ok = !res.persistThrew && !res.deleteNullThrew && res.alive && res.strayDeleted;
    report('bad-input-safety', ok,
      `persistUndef(noThrow)=${!res.persistThrew}, deleteNull(noop)=${!res.deleteNullThrew}, pageAlive=${res.alive}, strayCleaned=${res.strayDeleted}`);
  } catch (e) {
    report('bad-input-safety', false, String(e));
  }
}

// Final sweep: delete any leftover QA_R3_ entries across every touched source
// (plus the fixed bad-input source) so the store is left empty of QA data.
async function finalSweep() {
  const sources = Array.from(touchedSources);
  sources.push('QA_R3_BAD_undef');
  try {
    const removed = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var srcs = ${JSON.stringify(sources)};
      var n = 0;
      for (var i=0;i<srcs.length;i++){
        var rows = await q.getPendingBySource(srcs[i]);
        for (var k=0;k<rows.length;k++){ await q.deletePending(rows[k].seq); n++; }
      }
      return n;
    })()`);
    if (removed > 0) console.log(`[cleanup] swept ${removed} leftover QA_R3_ entr${removed === 1 ? 'y' : 'ies'}`);
  } catch (e) {
    console.log('[cleanup] sweep failed (non-fatal): ' + String(e));
  }
}

async function main() {
  try {
    await waitReady();
    await testDurability();
    await testIsolation();
    await testDelete();
    await testCapInvariant();
    await testBadInput();
  } finally {
    await finalSweep();
  }
  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
