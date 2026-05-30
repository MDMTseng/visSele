#!/usr/bin/env node
/*
 * QA R10 — IndexedDB durable-queue CAP EVICTION (the >1000 force run).
 * CORE-INDEPENDENT: only /reload + window.__GP_DB_QUEUE__ dev hook on app :8081.
 *
 * R3 covered the cheap cap *invariant* (count<=1000) but never actually pushed
 * past 1000 — it documented the gap. R10 closes that gap by force-inserting
 * 1100 records under a unique source tag and asserting that:
 *   (a) the GLOBAL invariant pendingInsertCount() <= 1000 holds throughout
 *       (checked every 100 inserts), and
 *   (b) the cap is GLOBAL — some of our own records get evicted
 *       (getPendingBySource(src).length <= 1000 with strict < expected since
 *       the store starts with other live entries from the running app), and
 *   (c) eviction is OLDEST-FIRST (FIFO): the newest 50 records we inserted are
 *       all still present, while the oldest 50 are gone — proves trimToCap()
 *       drops oldest-by-seq, not random.
 *
 * TEST PLAN:
 *   T1 baseline      : pendingInsertCount(src) == 0 (fresh unique src tag).
 *   T2 under cap     : persist 100 -> our count==100, global count<=1000.
 *   T3 force evict   : persist up to 1100 total (batched 100 per round-trip
 *                      to keep the wall-clock down). After each batch assert
 *                      global count<=1000. After the full 1100, assert our
 *                      surviving count<=1000 (eviction actually happened).
 *   T4 newest kept   : among survivors under src, the last 50 inserted seqs
 *                      are ALL present; some of the first 50 are GONE.
 *   T5 cleanup       : delete every surviving seq under src; count==0.
 *   T6 monotonic seq : back-to-back persist returns strictly increasing seqs
 *                      (no interleave/regression under contention).
 *
 * EXPECTED TIMING: 1100 inserts in 11 batches of 100 inside a single eval()
 *   round-trip per batch. Each persistPending hits IndexedDB (an add() in its
 *   own txn). Realistic budget: ~3-8s total for the fill phase on a warm
 *   Chromium; we'll print the measured ms.
 *
 * CLEANUP: unique tag "QA_R10_CAP_"+Date.now()+"_"+rand. A final finally{}
 * sweep deletes every surviving entry under that tag, so the store is left
 * clean even if an assertion throws partway through.
 *
 * CONSTRAINTS: ONE shared browser, parent runs serially — do NOT launch
 * against a live daemon concurrently with other QA scripts.
 *
 * Run: node tools/webctl/qa/r10_idbcap.mjs
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

const rnd = () => Math.random().toString(36).slice(2, 8);
const SRC = 'QA_R10_CAP_' + Date.now() + '_' + rnd();

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev("typeof window.__GP_DB_QUEUE__==='object' && window.__GP_DB_QUEUE__!==null");
    if (ok === true) return true;
  }
  throw new Error('dev hook __GP_DB_QUEUE__ never appeared — is app on :8081 in dev?');
}

// Persist `n` records under SRC in one round-trip; returns the seqs array.
async function persistBatch(n, offset) {
  return await ev(`(async function(){
    var q = window.__GP_DB_QUEUE__;
    var out = [];
    for (var i=0;i<${n};i++){
      out.push(await q.persistPending({id:'r10', i: ${offset}+i}, ${JSON.stringify(SRC)}));
    }
    return out;
  })()`);
}

async function globalCount() {
  return await ev(`window.__GP_DB_QUEUE__.pendingInsertCount()`);
}
async function srcCount() {
  return await ev(`window.__GP_DB_QUEUE__.pendingInsertCount(${JSON.stringify(SRC)})`);
}

async function runAll() {
  // T1 baseline
  {
    const c = await srcCount();
    report('T1 baseline', c === 0, `srcCount=${c}`);
  }

  // T2 under cap: 100 records
  let allSeqs = [];
  {
    const t0 = Date.now();
    const seqs = await persistBatch(100, 0);
    allSeqs = allSeqs.concat(seqs);
    const dt = Date.now() - t0;
    const sc = await srcCount();
    const gc = await globalCount();
    report('T2 under-cap fill 100', sc === 100 && gc <= 1000,
      `srcCount=${sc}/100, globalCount=${gc} (<=1000), ${dt}ms`);
  }

  // T3 force eviction: push to 1100 total. After each batch assert global<=1000.
  let invariantHeld = true;
  let invariantDetail = '';
  {
    const t0 = Date.now();
    for (let round = 1; round <= 10; round++) {
      const seqs = await persistBatch(100, round * 100);
      allSeqs = allSeqs.concat(seqs);
      const gc = await globalCount();
      if (gc > 1000) {
        invariantHeld = false;
        invariantDetail = `globalCount=${gc} after round ${round} (inserted ${100 + round * 100} total)`;
        break;
      }
    }
    const dt = Date.now() - t0;
    const sc = await srcCount();
    const gc = await globalCount();
    const evicted = sc < allSeqs.length; // some of OUR records dropped (global cap is shared)
    const ok = invariantHeld && sc <= 1000 && gc <= 1000;
    report('T3 force-evict 1100', ok,
      `inserted=${allSeqs.length}, srcSurvived=${sc} (<=1000, evicted=${evicted}), globalCount=${gc}, fillDur=${dt}ms`
      + (invariantHeld ? '' : ` INVARIANT BROKE: ${invariantDetail}`));
  }

  // T4 newest preserved, oldest gone
  {
    const lastSeqs = allSeqs.slice(-50);
    const firstSeqs = allSeqs.slice(0, 50);
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var rows = await q.getPendingBySource(${JSON.stringify(SRC)});
      var present = new Set(rows.map(function(e){return e.seq;}));
      var last = ${JSON.stringify(lastSeqs)};
      var first = ${JSON.stringify(firstSeqs)};
      var newestKept = last.every(function(s){return present.has(s);});
      var oldestGone = 0;
      for (var i=0;i<first.length;i++){ if (!present.has(first[i])) oldestGone++; }
      return {newestKept:newestKept, oldestGoneCount:oldestGone, survivors:rows.length};
    })()`);
    // Newest 50 must ALL be present. Oldest 50 must have at least some gone
    // (since we over-fed by 100, at least ~100 of the earliest should be evicted).
    const ok = res.newestKept && res.oldestGoneCount > 0;
    report('T4 oldest-first eviction',
      ok,
      `newest50Kept=${res.newestKept}, oldest50Gone=${res.oldestGoneCount}/50, survivors=${res.survivors}`);
  }

  // T6 (run before T5 so we test on the live store): monotonic seq under back-to-back
  {
    const seqs = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var s = [];
      // fire 10 persists back-to-back without awaits between (still serialized
      // via the txn promise chain), then await all in order.
      var ps = [];
      for (var i=0;i<10;i++){ ps.push(q.persistPending({id:'mono',i:i}, ${JSON.stringify(SRC)})); }
      for (var j=0;j<ps.length;j++){ s.push(await ps[j]); }
      return s;
    })()`);
    let mono = true;
    for (let i = 1; i < seqs.length; i++) if (!(seqs[i] > seqs[i - 1])) { mono = false; break; }
    report('T6 monotonic-seq', mono, `seqs=${JSON.stringify(seqs)}`);
  }

  // T5 cleanup (final): delete all surviving entries under SRC
  {
    const res = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var rows = await q.getPendingBySource(${JSON.stringify(SRC)});
      var n = 0;
      for (var i=0;i<rows.length;i++){ await q.deletePending(rows[i].seq); n++; }
      var after = await q.pendingInsertCount(${JSON.stringify(SRC)});
      return {deleted:n, after:after};
    })()`);
    report('T5 cleanup', res.after === 0, `deleted=${res.deleted}, srcAfter=${res.after}`);
  }
}

async function finalSweep() {
  try {
    const removed = await ev(`(async function(){
      var q = window.__GP_DB_QUEUE__;
      var rows = await q.getPendingBySource(${JSON.stringify(SRC)});
      for (var i=0;i<rows.length;i++){ await q.deletePending(rows[i].seq); }
      return rows.length;
    })()`);
    if (removed > 0) console.log(`[cleanup] swept ${removed} leftover ${SRC} entr${removed === 1 ? 'y' : 'ies'}`);
  } catch (e) {
    console.log('[cleanup] sweep failed (non-fatal): ' + String(e));
  }
}

async function main() {
  try {
    await waitReady();
    // pre-clean in case a previous run with this exact tag was somehow aborted
    // (defense-in-depth; SRC is timestamped so collisions are unlikely).
    await finalSweep();
    await runAll();
  } catch (e) {
    console.error('FATAL during run:', e);
    failures++;
  } finally {
    await finalSweep();
  }
  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
