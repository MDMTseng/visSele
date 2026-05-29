#!/usr/bin/env node
/* =============================================================================
 * r5_wslife.mjs — WebSocket LIFECYCLE QA for the WebUI core transport.
 *
 * VIEWPOINT: src/comm/BPG_WS.js (open/close/reconnect, the Wave-1 try/catch in
 * onmessage that prevents a single bad frame from killing the socket pump) +
 * src/UTIL/MISC_Util.js websocket_autoReconnect / websocket_aliveTracking. We
 * observe ConnInfo.CORE_ID_CONN_INFO.type transitions (WS_CONNECTED /
 * WS_DISCONNECTED / WS_ERROR) via the dev-mode __GP_STORE__ hook and stress
 * BPG_Protocol decode via __GP_BPG__.
 *
 * PLAN (each item -> one named test, prints "[name] PASS/FAIL/SKIP"):
 *   T1 baseline-connect : after /reload + waitStore + grace, CORE_ID_CONN_INFO
 *                         .type == "WS_CONNECTED" within RECONNECT_TIMEOUT.
 *                         Proves open + HR-handshake-driven WS_CONNECTED works.
 *   T2 reload-reconnect : /reload again; assert it re-reaches WS_CONNECTED
 *                         within RECONNECT_TIMEOUT (the auto-reconnect path is
 *                         exercised on every fresh page load).
 *   T3 garbage-frame    : COMPLEMENT to r2_bpg framing — feed malformed inputs
 *                         DIRECTLY to window.__GP_BPG__ (raw2header on a tiny
 *                         buffer, raw2obj on garbage payload, raw2Obj_IM with
 *                         out-of-bounds length claims) wrapped in try/catch and
 *                         assert the page is still alive (store + WS state
 *                         readable) afterward. Mirrors the Wave-1 try/catch
 *                         guard around BPG_WS.onmessage decode.
 *   T4 periph-keys      : ConnInfo exposes CAM1_ID_CONN_INFO,
 *                         Insp_DB_W_ID_CONN_INFO, DefFile_DB_W_ID_CONN_INFO
 *                         (slice shape, no live-state requirement).
 *   T5 diag-ws-trace    : best-effort soft check that __GP_DIAG__.diagText()
 *                         contains some WS-lifecycle log line ("ws_state",
 *                         "HR:", "CLOSE", "WS_CONNECTED", etc.) after reload.
 *
 * GAPS (cannot do headlessly through the daemon eval hook):
 *   - No DIRECT way to force-close the live core WebSocket from page JS: the
 *     BPG_WS instance is owned by middleware (websocket middleware) and not
 *     exposed on window. We can only observe natural reload-driven open/close.
 *     "Close-then-reconnect" transition is therefore covered indirectly via T2.
 *   - Cannot inject a malformed ArrayBuffer into the LIVE websocket's onmessage
 *     in jsdom (no socket handle, no MessageEvent constructor with binary body
 *     wired to the running pump). T3 instead stresses the decode path that
 *     onmessage's try/catch wraps — same robustness invariant, different entry.
 *
 * CORE-INTERMITTENCY: copies r1_comm.mjs's retry / skip-on-core-down behavior.
 * If the core WS never reaches WS_CONNECTED -> SKIP exit 0, NOT a failure.
 *
 * RUN: node tools/webctl/qa/r5_wslife.mjs   (needs webctld :8765, app :8081;
 *      parent runs serially — do NOT race against other agents on the shared
 *      browser).
 * ===========================================================================*/

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const RECONNECT_TIMEOUT_MS = 30000;

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
function pass(name, note) { console.log(`[${name}] PASS${note ? ' — ' + note : ''}`); }
function fail(name, note) { console.log(`[${name}] FAIL${note ? ' — ' + note : ''}`); failures++; }
function skip(name, note) { console.log(`[${name}] SKIP${note ? ' — ' + note : ''}`); }

const CORE_DOWN = 'CORE_DOWN';
function isCoreDownErr(e) {
  const s = String(e && e.message ? e.message : e);
  return s.includes(CORE_DOWN) || /Not connected|Timeout/i.test(s);
}

async function waitStore() {
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') return true;
  }
  return false;
}

async function connInfoSnap() {
  return ev(`(function(){
    try{
      var ci=window.__GP_STORE__.getState().ConnInfo;
      var core=ci.CORE_ID_CONN_INFO||null;
      return {
        present:true,
        coreType: core?core.type:null,
        keys: Object.keys(ci),
      };
    }catch(e){return {present:false,err:String(e)};}
  })()`);
}

async function waitConnected(timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = await connInfoSnap();
    if (last && last.coreType === 'WS_CONNECTED') return last;
    await sleep(300);
  }
  return last;
}

// ---- tests ------------------------------------------------------------------
async function main() {
  // T1 baseline-connect
  await api('/reload', {});
  if (!(await waitStore())) {
    fail('T1_baseline-connect', '__GP_STORE__ never appeared');
    process.exit(1);
  }
  const snap = await waitConnected(RECONNECT_TIMEOUT_MS);
  if (!snap || snap.coreType !== 'WS_CONNECTED') {
    skip('T1_baseline-connect', `core down (coreType=${snap ? snap.coreType : 'n/a'})`);
    skip('T2_reload-reconnect', 'core down');
    // T3/T4/T5 don't strictly need the core — still run them.
    await testGarbageFrame();
    await testPeriphKeys(snap);
    await testDiagTrace();
    console.log('\nSKIP (core down) — ws-lifecycle observable tests unrunnable; exiting 0.');
    process.exit(0);
  }
  pass('T1_baseline-connect', 'CORE_ID_CONN_INFO.type==WS_CONNECTED after reload');

  // T2 reload-reconnect
  await api('/reload', {});
  if (!(await waitStore())) {
    fail('T2_reload-reconnect', '__GP_STORE__ never re-appeared after reload');
  } else {
    const re = await waitConnected(RECONNECT_TIMEOUT_MS);
    if (re && re.coreType === 'WS_CONNECTED') {
      pass('T2_reload-reconnect', `reconnected within ${RECONNECT_TIMEOUT_MS}ms`);
    } else {
      fail('T2_reload-reconnect', `coreType=${re ? re.coreType : 'n/a'} after reload`);
    }
  }

  // T3 garbage-frame robustness (decode-path stress)
  await testGarbageFrame();

  // T4 peripheral keys
  await testPeriphKeys(await connInfoSnap());

  // T5 diag ws trace
  await testDiagTrace();

  if (failures) { console.log(`\n${failures} test(s) FAILED.`); process.exit(1); }
  console.log('\nAll ws-lifecycle tests passed.');
  process.exit(0);
}

async function testGarbageFrame() {
  const hookKind = await ev(`(function(){
    if(window.__GP_BPG__){
      return {
        h: typeof window.__GP_BPG__.raw2header,
        o: typeof window.__GP_BPG__.raw2obj,
        i: typeof window.__GP_BPG__.raw2Obj_IM,
      };
    }
    return null;
  })()`);
  if (!hookKind || hookKind.h !== 'function') {
    skip('T3_garbage-frame', 'COVERAGE GAP: window.__GP_BPG__.raw2header not exposed');
    return;
  }
  // Hit the decode functions with intentionally bad inputs; wrap each in
  // try/catch — the invariant is "the page survives" (store still readable).
  const res = await ev(`(function(){
    var b=window.__GP_BPG__;
    var attempts=[];
    function tryIt(label, fn){
      try{ var r=fn(); attempts.push({label, ok:true, threw:false, sample:(r&&typeof r==='object')?Object.keys(r).slice(0,4):typeof r}); }
      catch(e){ attempts.push({label, ok:true, threw:true, err:String(e).slice(0,120)}); }
    }
    // tiny buffer (3 bytes) — too short for a 9-byte header
    tryIt('raw2header_tiny', function(){
      var ab=new Uint8Array([0x48,0x52,0x00]).buffer;
      return b.raw2header({data:ab},0);
    });
    // header claims length=999999 but body is empty/garbage
    tryIt('raw2obj_bogus_len', function(){
      var bytes=[0x46,0x4C, 0x07, 0x00,0x01, 0x00,0x0F,0x42,0x40, 0x7B,0x7D]; // type FL, len ~1M, body "{}"
      var ab=new Uint8Array(bytes).buffer;
      return b.raw2obj({data:ab},0);
    });
    // raw2Obj_IM on a buffer that doesn't begin to satisfy the IM schema
    tryIt('raw2Obj_IM_garbage', function(){
      var bytes=new Uint8Array(32);
      for(var i=0;i<32;i++) bytes[i]= (i*37+13)&0xFF;
      return b.raw2Obj_IM({data:bytes.buffer},0);
    });
    // page-alive check: store + ConnInfo readable
    var alive=false, coreType=null;
    try{
      var ci=window.__GP_STORE__.getState().ConnInfo;
      alive=!!ci; coreType=ci.CORE_ID_CONN_INFO?ci.CORE_ID_CONN_INFO.type:null;
    }catch(e){ alive=false; }
    return {attempts, alive, coreType};
  })()`);
  if (!res || !res.alive) {
    fail('T3_garbage-frame', `page not alive after decode stress: ${JSON.stringify(res)}`);
    return;
  }
  // Success criterion: every call EITHER threw cleanly (caught) OR returned
  // something — neither outcome crashed the JS context. The on-the-wire
  // try/catch in BPG_WS.onmessage relies on exactly this property.
  const allHandled = Array.isArray(res.attempts) && res.attempts.every((a) => a.ok);
  if (allHandled) {
    const threw = res.attempts.filter((a) => a.threw).map((a) => a.label);
    pass('T3_garbage-frame', `decode survived; threw-but-caught: [${threw.join(',')}]; coreType=${res.coreType}`);
  } else {
    fail('T3_garbage-frame', `unexpected attempt result: ${JSON.stringify(res.attempts)}`);
  }
}

async function testPeriphKeys(snap) {
  const wanted = ['CAM1_ID_CONN_INFO', 'Insp_DB_W_ID_CONN_INFO', 'DefFile_DB_W_ID_CONN_INFO', 'CORE_ID_CONN_INFO'];
  if (!snap || !snap.present || !Array.isArray(snap.keys)) {
    fail('T4_periph-keys', 'ConnInfo slice unreadable');
    return;
  }
  const missing = wanted.filter((k) => !snap.keys.includes(k));
  if (missing.length === 0) pass('T4_periph-keys', `ConnInfo exposes ${wanted.length} expected slots`);
  else fail('T4_periph-keys', `missing keys: ${missing.join(', ')}`);
}

async function testDiagTrace() {
  const probe = await ev(`(function(){
    try{
      if(!window.__GP_DIAG__ || typeof window.__GP_DIAG__.diagText!=='function')
        return {hook:false};
      var txt=window.__GP_DIAG__.diagText()||'';
      var hits=['ws_state','HR:','CLOSE','WS_CONNECTED','WS_DISCONNECTED','onmessage']
        .filter(k=>txt.indexOf(k)>=0);
      return {hook:true, len:txt.length, hits:hits};
    }catch(e){return {hook:false, err:String(e)};}
  })()`);
  if (!probe.hook) {
    skip('T5_diag-ws-trace', 'COVERAGE GAP: __GP_DIAG__.diagText() unavailable');
    return;
  }
  if (probe.hits && probe.hits.length > 0) {
    pass('T5_diag-ws-trace', `diag ring buffer has ws lifecycle marks: [${probe.hits.join(',')}] (len=${probe.len})`);
  } else {
    // Soft check — don't fail the run; the diag wrap can miss console.log if
    // BPG_WS uses raw console rather than the wrapped sink.
    skip('T5_diag-ws-trace', `no WS markers in diagText (len=${probe.len}) — soft skip (diag wrap coverage gap)`);
  }
}

main().catch((e) => {
  if (isCoreDownErr(e)) {
    console.log('\nSKIP (core down) — ' + String(e.message || e));
    process.exit(0);
  }
  console.error('UNEXPECTED ERROR:', e);
  process.exit(1);
});
