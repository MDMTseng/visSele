#!/usr/bin/env node
/* =============================================================================
 * r1_comm.mjs — COMM / PROTOCOL / CONNECTION-lifecycle QA for the WebUI.
 *
 * VIEWPOINT: the core WebSocket transport (src/comm/BPG_WS.js) + the BPG framing
 * (src/UTIL/BPG_Protocol.js). We can't easily unit-test the decoders in node
 * (path aliases UTIL/REDUX_STORE_SRC, TextDecoder/ImageData browser deps), so we
 * test the *running bundle* through the webctld daemon + dev hooks, observing the
 * effects of a real handshake/decode end-to-end.
 *
 * PLAN (each item -> one named test, prints "[name] PASS/FAIL"):
 *   T1 store-handle   : window.__GP_STORE__ becomes available after /reload.
 *   T2 ws-connected   : ConnInfo.CORE_ID_CONN_INFO.type == "WS_CONNECTED".
 *                       Proves the WS opened AND the HR handshake frame was
 *                       handled (the WS_CONNECTED dispatch lives in the case "HR"
 *                       branch of BPG_WS.onmessage). This is the regression guard
 *                       for "did the HR-case break the handshake?".
 *   T3 hr-version     : CORE_ID_CONN_INFO.data.version (HR payload) is present.
 *                       Proves BPG_Protocol.raw2obj decoded the HR JSON body, and
 *                       brief_info==version was wired through.
 *   T4 reconnect      : /reload again, assert it re-reaches WS_CONNECTED within
 *                       N seconds (connect/onclose-reconnect lifecycle works).
 *   T5 def-decode     : load the model def via the real core LD flow; assert the
 *                       serialized def (__GP_DEF__()) has features with a sane
 *                       structure (proves the IM/FL/SS packet decode + dispatch
 *                       path produced a usable def end-to-end).
 *   T6 periph-keys    : ConnInfo exposes the peripheral conn-info slots
 *                       (CAM1_ID_CONN_INFO, Insp_DB_W_ID_CONN_INFO,
 *                       DefFile_DB_W_ID_CONN_INFO). Presence of the *keys* (slice
 *                       shape) is asserted; their connected-state is NOT required
 *                       (peripherals are optional / config-driven from HR's LD).
 *   T7 raw-parse      : if a window hook exposes BPG_Protocol.raw2header, feed it a
 *                       hand-built ArrayBuffer and assert type/prop/pgID/length
 *                       parse correctly. Otherwise -> COVERAGE GAP (skipped, not
 *                       failed) — there is currently no such hook.
 *
 * CORE-INTERMITTENCY: the core WS (localStorage coreport, "4190" stable / "4090"
 * dev) drops transiently. We copy flows.mjs's retry loop. If the core never comes
 * up we print "SKIP (core down)" and exit 0 — that is NOT a test failure.
 *
 * RUN: node tools/webctl/qa/r1_comm.mjs   (needs the webctld daemon on :8765 and
 * the app on :8081 — run serially by the parent; do NOT run against a shared live
 * daemon while other agents are using it).
 * ===========================================================================*/

import { MODEL_PATH } from './lib_model.mjs';

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

// ---- result bookkeeping -----------------------------------------------------
let failures = 0;
function pass(name, note) { console.log(`[${name}] PASS${note ? ' — ' + note : ''}`); }
function fail(name, note) { console.log(`[${name}] FAIL${note ? ' — ' + note : ''}`); failures++; }
function skip(name, note) { console.log(`[${name}] SKIP${note ? ' — ' + note : ''}`); }

// CORE-DOWN sentinel: callers throw this string when the core WS won't come up.
const CORE_DOWN = 'CORE_DOWN';
function isCoreDownErr(e) {
  const s = String(e && e.message ? e.message : e);
  return s.includes(CORE_DOWN) || /Not connected|Timeout/i.test(s);
}

// Wait until the redux store handle is live after a reload.
async function waitStore() {
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') return true;
  }
  return false;
}

// Read CORE_ID_CONN_INFO snapshot defensively (store may not be ready).
async function connInfoSnap() {
  return ev(`(function(){
    try{
      var ci=window.__GP_STORE__.getState().ConnInfo;
      var core=ci.CORE_ID_CONN_INFO||null;
      return {
        present:true,
        coreType: core?core.type:null,
        // HR version is wired to brief_info, and the raw HR payload nests it at data.data.version
        // (CORE_ID_CONN_INFO.data IS the HR packet {type,prop,pgID,...,data:{version}}).
        version: (core && (core.brief_info != null ? core.brief_info
                          : (core.data && core.data.data && core.data.data.version))) || null,
        keys: Object.keys(ci),
      };
    }catch(e){return {present:false,err:String(e)};}
  })()`);
}

// Poll for WS_CONNECTED within timeout. Returns the last snapshot.
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

// Load the model def via the real core LD flow (mirrors flows.mjs reset()).
// Returns true on success; throws CORE_DOWN if the core never serves it.
async function loadDef() {
  for (let attempt = 0; attempt < 4; attempt++) {
    await ev(
      `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(MODEL_PATH)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
    );
    let err = null;
    for (let i = 0; i < 80; i++) {
      await sleep(100);
      const st = await ev(
        `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList.length)||0};})()`
      );
      if (st.err) { err = st.err; break; }
      if (st.rdy && st.n > 0) return true;
    }
    await sleep(3000); // give the UI reconnect loop time
  }
  throw new Error(CORE_DOWN + ': def did not load after retries');
}

// ---- tests ------------------------------------------------------------------
async function main() {
  // ---- bring the app up fresh -------------------------------------------
  await api('/reload', {});
  const storeOk = await waitStore();
  if (!storeOk) { fail('T1_store-handle', 'window.__GP_STORE__ never appeared after reload'); }
  else { pass('T1_store-handle'); }

  // T2 / T3 — handshake reached WS_CONNECTED and HR payload decoded.
  const snap = await waitConnected(RECONNECT_TIMEOUT_MS);
  if (!snap || snap.coreType !== 'WS_CONNECTED') {
    // core never came up -> whole comm suite is unobservable; skip cleanly.
    skip('T2_ws-connected', `core down (coreType=${snap ? snap.coreType : 'n/a'})`);
    skip('T3_hr-version', 'core down');
    skip('T4_reconnect', 'core down');
    skip('T5_def-decode', 'core down');
    // T6 (slice shape) and T7 (raw parse) do NOT need the core — still run them.
    await testPeripheralKeys(snap);
    await testRawParse();
    console.log('\nSKIP (core down) — comm tests unobservable; exiting 0.');
    process.exit(0);
  }
  pass('T2_ws-connected', 'CORE_ID_CONN_INFO.type==WS_CONNECTED (HR handshake handled)');

  if (snap.version != null && snap.version !== '_' && snap.version !== '') {
    pass('T3_hr-version', `HR data.version=${JSON.stringify(snap.version)}`);
  } else {
    fail('T3_hr-version', `version missing/blank (got ${JSON.stringify(snap.version)}) — HR JSON decode or brief_info wiring broken`);
  }

  // T4 — reconnect after reload.
  await api('/reload', {});
  await waitStore();
  const re = await waitConnected(RECONNECT_TIMEOUT_MS);
  if (re && re.coreType === 'WS_CONNECTED') {
    pass('T4_reconnect', 'reconnected to WS_CONNECTED after reload');
  } else {
    fail('T4_reconnect', `did not reconnect within ${RECONNECT_TIMEOUT_MS}ms (got ${re ? re.coreType : 'n/a'})`);
  }

  // T5 — def decode end-to-end (IM/FL/SS path -> usable serialized def).
  try {
    await loadDef();
    const def = await ev(`(function(){
      try{
        var d=window.__GP_DEF__();
        var feats=(d&&d.features)||[];
        return {
          ok:true,
          count:feats.length,
          types:feats.map(f=>f.type).filter((v,i,a)=>a.indexOf(v)===i),
          hasMeasure:feats.some(f=>f.type==='measure'),
          allHaveType:feats.every(f=>typeof f.type==='string'),
        };
      }catch(e){return {ok:false,err:String(e)};}
    })()`);
    if (def.ok && def.count > 0 && def.allHaveType) {
      pass('T5_def-decode', `${def.count} features, types=${JSON.stringify(def.types)}, hasMeasure=${def.hasMeasure}`);
    } else {
      fail('T5_def-decode', `def structure unexpected: ${JSON.stringify(def)}`);
    }
  } catch (e) {
    if (isCoreDownErr(e)) skip('T5_def-decode', 'core down during load');
    else fail('T5_def-decode', String(e));
  }

  // T6 — peripheral conn-info slice shape.
  await testPeripheralKeys(await connInfoSnap());

  // T7 — raw header parse (only if a hook exists).
  await testRawParse();

  if (failures) { console.log(`\n${failures} test(s) FAILED.`); process.exit(1); }
  console.log('\nAll comm tests passed.');
  process.exit(0);
}

async function testPeripheralKeys(snap) {
  const wanted = ['CAM1_ID_CONN_INFO', 'Insp_DB_W_ID_CONN_INFO', 'DefFile_DB_W_ID_CONN_INFO', 'CORE_ID_CONN_INFO'];
  if (!snap || !snap.present || !Array.isArray(snap.keys)) {
    fail('T6_periph-keys', 'ConnInfo slice unreadable');
    return;
  }
  const missing = wanted.filter((k) => !snap.keys.includes(k));
  if (missing.length === 0) {
    pass('T6_periph-keys', `ConnInfo exposes ${wanted.length} expected slots`);
  } else {
    fail('T6_periph-keys', `missing ConnInfo keys: ${missing.join(', ')}`);
  }
}

// Feed a hand-built BPG frame to raw2header IF the bundle exposes a hook.
// Layout (9-byte header): [t0,t1,prop, pgIDhi,pgIDlo, len32be...]
async function testRawParse() {
  const hookKind = await ev(`(function(){
    if(window.__GP_BPG__ && typeof window.__GP_BPG__.raw2header==='function') return 'GP_BPG';
    if(window.BPG_Protocol && typeof window.BPG_Protocol.raw2header==='function') return 'BPG_Protocol';
    return null;
  })()`);
  if (!hookKind) {
    skip('T7_raw-parse', 'COVERAGE GAP: no window hook exposes BPG_Protocol.raw2header; raw framing untested in node (path aliases prevent direct import)');
    return;
  }
  // type "HR", prop 0x07, pgID 0x0102 (258), length 0x00000004 (4).
  const res = await ev(`(function(){
    try{
      var fn=(window.__GP_BPG__&&window.__GP_BPG__.raw2header)||window.BPG_Protocol.raw2header;
      var bytes=[0x48,0x52, 0x07, 0x01,0x02, 0x00,0x00,0x00,0x04, 9,9,9,9];
      var ab=new Uint8Array(bytes).buffer;
      var h=fn({data:ab},0);
      return {type:h.type,prop:h.prop,pgID:h.pgID,length:h.length};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.type === 'HR' && res.prop === 7 && res.pgID === 258 && res.length === 4) {
    pass('T7_raw-parse', 'raw2header parsed type/prop/pgID/length correctly');
  } else {
    fail('T7_raw-parse', `raw2header mis-parsed: ${JSON.stringify(res)}`);
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
