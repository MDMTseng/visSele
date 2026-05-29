#!/usr/bin/env node
/*
 * QA R3 — DIAGNOSTICS RING BUFFER (deep) tests. CORE-INDEPENDENT.
 *
 * Exercises src/UTIL/diagLog.js entirely in the running app bundle via the
 * window.__GP_DIAG__ dev hook ({downloadDiag, diagCount, diagText}). No core
 * backend needed — just /reload then wait for the dev hook (copied from
 * r1_purelogic.mjs: api/ev/sleep + /reload + waitReady).
 *
 * diagLog.js facts under test:
 *  - initDiag() wraps console.{log,info,warn,error,debug}; every call pushes an
 *    entry {t, level, msg} into a ring capped at MAX_ENTRIES=2000 (oldest
 *    dropped via ring.shift()).
 *  - safeArg(): Error -> .stack (or "name: message"); string passthrough; else
 *    JSON.stringify. Args joined with " ".
 *  - diagText(): header ("=== visSele WebUI diagnostics ===", "generated:",
 *    "userAgent:", "entries: N (cap 2000)") + body lines
 *    "<ISO> [level] msg".
 *  - diagCount(): ring.length.
 *  - downloadDiag(): Blob + <a download> click (cannot verify file headlessly).
 *  - script.jsx installs window 'error'/'unhandledrejection' handlers that
 *    log.error("window.onerror:", ...) / log.error("unhandledrejection:", ...)
 *    -> loglevel -> console.error -> captured by the ring.
 *
 * TEST PLAN (all core-INDEPENDENT):
 *  1. capture-all-levels: emit console.{log,info,warn,error,debug} with unique
 *     markers; assert diagText() contains each marker tagged with the right
 *     [level].
 *  2. error-formatting: console.error(new Error("xyz")) -> diagText contains
 *     "xyz" and "Error" (the .stack framing from safeArg).
 *  3. count-increases: diagCount() rises by exactly the number of emitted lines.
 *  4. cap-eviction (invariant + newest-present): emit a bounded burst of 50
 *     marked lines; assert diagCount() never exceeds MAX_ENTRIES (2000) and the
 *     newest markers are present (oldest-dropped/newest-kept semantics). A full
 *     >2000 fill is heavy — we assert the invariant + newest-present rather than
 *     forcing real eviction. GAP: deep eviction (>2000 fill then confirm oldest
 *     actually dropped) is noted, not exercised.
 *  5. global-handlers -> ring: dispatch a synthetic window 'error' (ErrorEvent)
 *     and 'unhandledrejection'; assert the handlers ran without throwing and a
 *     corresponding [error] line ("window.onerror:" / "unhandledrejection:")
 *     appears in diagText (best-effort, polled — timing may vary).
 *  6. diagText-format: header contains "diagnostics" + "entries:" and a sampled
 *     body line matches /^<ISO> \[level\] /.
 *  7. downloadDiag: call it; assert it does not throw. GAP: cannot verify the
 *     actual browser file download headlessly.
 *
 * Prints "[name] PASS/FAIL"; exits non-zero on real failure.
 * Run: node tools/webctl/qa/r3_diag.mjs
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

// Wait for the dev hook after a reload.
async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_DIAG__==='object' && typeof window.__GP_DIAG__.diagText==='function' && typeof window.__GP_DIAG__.diagCount==='function'"
    );
    if (ok === true) return true;
  }
  throw new Error('dev hook window.__GP_DIAG__ never appeared — is app on :8081 in dev?');
}

// --- 1. capture all five console levels with correct [level] tagging --------
async function testCaptureAllLevels() {
  const id = Date.now();
  const levels = ['log', 'info', 'warn', 'error', 'debug'];
  // console.log/console.debug both push level "log"/"debug"; the ring tag is the
  // console method name itself (see diagLog initDiag forEach).
  const markers = Object.fromEntries(levels.map((l) => [l, `R3_LVL_${l}_${id}`]));
  try {
    const res = await ev(`(function(){
      var m = ${JSON.stringify(markers)};
      console.log(m.log);
      console.info(m.info);
      console.warn(m.warn);
      console.error(m.error);
      console.debug(m.debug);
      var txt = window.__GP_DIAG__.diagText();
      var out = {};
      ['log','info','warn','error','debug'].forEach(function(l){
        // line must contain "[<level>] <marker>" — marker right after its level tag
        out[l] = txt.indexOf('['+l+'] '+m[l]) >= 0;
      });
      return out;
    })()`);
    const bad = Object.entries(res).filter(([, v]) => !v).map(([k]) => k);
    report('capture-all-levels', bad.length === 0,
      bad.length ? `missing/mis-tagged: ${bad.join(',')}` : 'log/info/warn/error/debug each tagged correctly');
  } catch (e) {
    report('capture-all-levels', false, String(e));
  }
}

// --- 2. Error object formatting (safeArg -> .stack) -------------------------
async function testErrorFormatting() {
  const msg = `R3_ERR_${Date.now()}`;
  try {
    const res = await ev(`(function(){
      console.error(new Error(${JSON.stringify(msg)}));
      var txt = window.__GP_DIAG__.diagText();
      return {
        hasMsg: txt.indexOf(${JSON.stringify(msg)}) >= 0,
        // safeArg returns err.stack which begins "Error: <msg>"
        hasErrorFraming: txt.indexOf('Error') >= 0,
        tagged: txt.indexOf('[error] ') >= 0 && txt.indexOf(${JSON.stringify(msg)}) >= 0
      };
    })()`);
    const ok = res.hasMsg && res.hasErrorFraming && res.tagged;
    report('error-formatting', ok,
      `msg=${res.hasMsg}, Error-framing=${res.hasErrorFraming}, [error]-tagged=${res.tagged}`);
  } catch (e) {
    report('error-formatting', false, String(e));
  }
}

// --- 3. diagCount() increases by exactly the number emitted -----------------
async function testCountIncreases() {
  const id = Date.now();
  const N = 7;
  try {
    const res = await ev(`(function(){
      var before = window.__GP_DIAG__.diagCount();
      for (var i=0;i<${N};i++){ console.log('R3_CNT_${id}_'+i); }
      var after = window.__GP_DIAG__.diagCount();
      return {before: before, after: after};
    })()`);
    // exact, unless we were already at the 2000 cap (then deltas clamp). Guard:
    const delta = res.after - res.before;
    const atCap = res.before >= 2000 || res.after >= 2000;
    const ok = atCap ? (res.after <= 2000) : (delta === N);
    report('count-increases', ok,
      `count ${res.before}->${res.after} (delta ${delta}, expected ${N}${atCap ? ', at-cap clamp' : ''})`);
  } catch (e) {
    report('count-increases', false, String(e));
  }
}

// --- 4. cap/eviction invariant + newest-present ------------------------------
async function testCapEviction() {
  const id = Date.now();
  const BURST = 50;
  try {
    const res = await ev(`(function(){
      for (var i=0;i<${BURST};i++){ console.log('R3_BURST_${id}_'+i); }
      var count = window.__GP_DIAG__.diagCount();
      var txt = window.__GP_DIAG__.diagText();
      // newest few markers must survive (newest-kept)
      var newest = [];
      for (var k=${BURST}-3;k<${BURST};k++){ newest.push(txt.indexOf('R3_BURST_${id}_'+k) >= 0); }
      // header advertises the cap
      var capDeclared = txt.indexOf('(cap 2000)') >= 0;
      return {count: count, newestAllPresent: newest.every(Boolean), capDeclared: capDeclared};
    })()`);
    const ok = res.count <= 2000 && res.newestAllPresent && res.capDeclared;
    report('cap-eviction', ok,
      `count=${res.count} (<=2000 invariant), newest-present=${res.newestAllPresent}, header cap=${res.capDeclared}`);
  } catch (e) {
    report('cap-eviction', false, String(e));
  }
}

// --- 5. global error / unhandledrejection handlers -> ring ------------------
async function testGlobalHandlers() {
  const errTag = `R3_GLOBALERR_${Date.now()}`;
  const rejTag = `R3_GLOBALREJ_${Date.now()}`;
  try {
    // Dispatch synthetic events; the script.jsx handlers log.error(...) -> ring.
    const dispatched = await ev(`(function(){
      var threw = null;
      try {
        var ee = new ErrorEvent('error', { error: new Error(${JSON.stringify(errTag)}), message: ${JSON.stringify(errTag)} });
        window.dispatchEvent(ee);
      } catch(e1) { threw = 'errorEvent:'+String(e1); }
      try {
        // PromiseRejectionEvent may not be constructible everywhere; fall back to
        // a plain CustomEvent carrying .reason so the handler's (e && e.reason) reads it.
        var re;
        try {
          re = new PromiseRejectionEvent('unhandledrejection', { promise: Promise.reject(${JSON.stringify(rejTag)}).catch(function(){}), reason: ${JSON.stringify(rejTag)} });
        } catch(_) {
          re = new CustomEvent('unhandledrejection');
          re.reason = ${JSON.stringify(rejTag)};
        }
        window.dispatchEvent(re);
      } catch(e2) { threw = (threw||'')+' rejEvent:'+String(e2); }
      return { threw: threw };
    })()`);
    // handlers/log path is sync, but poll briefly to be safe.
    let res = { hasErr: false, hasRej: false };
    for (let i = 0; i < 10; i++) {
      res = await ev(`(function(){
        var txt = window.__GP_DIAG__.diagText();
        return {
          hasErr: txt.indexOf('window.onerror:') >= 0 && txt.indexOf(${JSON.stringify(errTag)}) >= 0,
          hasRej: txt.indexOf('unhandledrejection:') >= 0 && txt.indexOf(${JSON.stringify(rejTag)}) >= 0
        };
      })()`);
      if (res.hasErr && res.hasRej) break;
      await sleep(120);
    }
    const ranClean = !dispatched.threw;
    const ok = ranClean && res.hasErr && res.hasRej;
    report('global-handlers', ok,
      `dispatchedClean=${ranClean}${dispatched.threw ? '(' + dispatched.threw + ')' : ''}, ` +
      `error->ring=${res.hasErr}, unhandledrejection->ring=${res.hasRej}`);
  } catch (e) {
    report('global-handlers', false, String(e));
  }
}

// --- 6. diagText() format (header + line shape) -----------------------------
async function testDiagTextFormat() {
  const id = Date.now();
  try {
    const res = await ev(`(function(){
      console.warn('R3_FMT_${id}');
      var txt = window.__GP_DIAG__.diagText();
      var head = txt.split('=================================')[0] || '';
      var hasDiag = head.indexOf('diagnostics') >= 0;
      var hasEntries = head.indexOf('entries:') >= 0;
      // find our line and validate "<ISO> [warn] R3_FMT_..."
      var lines = txt.split('\\n');
      var line = '';
      for (var i=0;i<lines.length;i++){ if (lines[i].indexOf('R3_FMT_${id}') >= 0){ line = lines[i]; break; } }
      var lineOk = /^\\d{4}-\\d{2}-\\d{2}T[\\d:.]+Z \\[warn\\] R3_FMT_${id}$/.test(line);
      return { hasDiag: hasDiag, hasEntries: hasEntries, line: line, lineOk: lineOk };
    })()`);
    const ok = res.hasDiag && res.hasEntries && res.lineOk;
    report('diagText-format', ok,
      `header diagnostics=${res.hasDiag}, entries:=${res.hasEntries}, line="${res.line}" matches=${res.lineOk}`);
  } catch (e) {
    report('diagText-format', false, String(e));
  }
}

// --- 7. downloadDiag() does not throw (cannot verify download headlessly) ----
async function testDownloadDiag() {
  try {
    const res = await ev(`(function(){
      var threw = null;
      try { window.__GP_DIAG__.downloadDiag(); } catch(e){ threw = String(e); }
      return { threw: threw, isFn: typeof window.__GP_DIAG__.downloadDiag === 'function' };
    })()`);
    const ok = res.isFn && !res.threw;
    report('downloadDiag', ok,
      res.threw ? `threw: ${res.threw}` : 'invoked without throwing (GAP: actual file download not headlessly verifiable)');
  } catch (e) {
    report('downloadDiag', false, String(e));
  }
}

async function main() {
  await waitReady();
  await testCaptureAllLevels();
  await testErrorFormatting();
  await testCountIncreases();
  await testCapEviction();
  await testGlobalHandlers();
  await testDiagTextFormat();
  await testDownloadDiag();
  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
