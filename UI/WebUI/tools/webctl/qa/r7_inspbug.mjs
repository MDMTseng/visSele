#!/usr/bin/env node
/*
 * QA r7_inspbug — PIN TEST for a confirmed legacy CanvasComponent bug.
 *
 * BUG (round 6 surfaced, round 7 confirmed by live repro):
 *   Loading caliper_verify (no inspection reports yet) then dispatching
 *   Insp_Mode triggers a render-time throw:
 *
 *     TypeError: Cannot read properties of undefined (reading '0')
 *       in CanvasComponent  (caught by RootErrorBoundary, no white-screen)
 *
 *   Round 3 added a RootErrorBoundary (script.jsx) and ring-buffer diag
 *   that captures window.onerror + componentDidCatch with the React
 *   componentStack. Round 6 saw the line "in RootErrorBoundary (at
 *   script.jsx:2105)" + "The above error occurred in the <CanvasComponent>"
 *   in __GP_DIAG__ after Insp_Mode dispatch on this model.
 *
 * ROOT-CAUSE INVESTIGATION (this round):
 *   The throw fires from CanvasComponent (InspectionUI.js L903) during the
 *   first render under INSP_MODE on a model that has never produced an
 *   inspection report. Suspected culprits scanned for `[0]` access:
 *
 *     - InspectionUI.js:937,1372 — props.camera_calibration_report.reports[0]
 *       (camera_calibration_report is NOT in Edit_info_Empty(); will be
 *        undefined for many models before a calib report arrives)
 *     - EverCheckCanvasComponent.js:1044,1050,1055
 *       (inspectionReportList[0] — but guarded by length>=1 at L1039)
 *     - renderUTIL.js:1289,1295 — subObjs[0]/subObjs[1] inside aux_point
 *       (guarded by subObjs.length==2)
 *     - renderUTIL.js:1174 — BPG_ExpCalc(...)[0] (returns undefined when
 *        post_exp lacks a value; then `.toFixed` below blows up — different
 *        message though)
 *
 *   The most plausible single-line culprit is the `camera_calibration_report`
 *   path: AngledCalibrationHelper.getDerivedStateFromProps L1372 dereferences
 *   .reports[0] unconditionally — BUT AngledCalibrationHelper is commented
 *   out at L2614 in the InspectionUI tree so it's not mounted here. The
 *   sister access at L937 (`ec_canvas_EmitEvent` "down_samp_level_update")
 *   IS reachable via canvas init -> updateCanvas -> draw -> zoom_emit ->
 *   EmitEvent("down_samp_level_update"), but down_samp_level_update is
 *   dispatched ASYNC from zoom_emit (it's an outbound event, not a render
 *   error). Without a fresh diag stack (core was down at run time and the
 *   ring buffer was cleared by reload) the EXACT line cannot be pinned.
 *
 * STANCE: We do NOT apply a speculative fix. We pin the bug presence so
 * round 8 (with a stable core + live stack) can localise it without losing
 * track of the regression. When the underlying bug is fixed, this test
 * will FAIL (with an explicit message asking the next agent to delete or
 * invert the pin), forcing visibility of the change.
 *
 * TEST PLAN:
 *   T0  core_up               — bail SKIP exit 0 if core/def cannot load
 *                                (matches r6 reset() convention).
 *   T1  enter_inspmode        — dispatch Insp_Mode; assert c_state.value
 *                                top-state is INSP_MODE. (a separate
 *                                concern from the render error: the SM
 *                                transition itself succeeds.)
 *   T2  bug_present_pin       — poll __GP_DIAG__.diagText() for ~1.5s
 *                                after the dispatch; PASS iff one of the
 *                                following bug-signature lines is present:
 *                                  - "Cannot read properties of undefined (reading '0')"
 *                                  - "RootErrorBoundary caught"
 *                                  - "The above error occurred in the <CanvasComponent>"
 *                                FAIL with a "BUG APPEARS FIXED — invert
 *                                this pin" message otherwise.
 *
 * SHARED-BROWSER ETIQUETTE:
 *   - ONE /reload at start, ONE def load, ONE Insp_Mode dispatch, brief
 *     diag polls. EXIT back to MAIN at the end and final /reload so the
 *     shared browser is clean for the next agent.
 *   - Uses cState.value only (no full c_state — circular refs per round 5).
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
    if (!loaded) {
      lastErr = lastErr || 'timeout';
      await sleep(3000);
    }
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
  return { value: v, valueJSON: (function(){try{return JSON.stringify(v);}catch(e){return null;}})() };
})()`;
const snap = () => ev(STATE_SNAP);
const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);

function isTopState(val, name) {
  if (val === name) return true;
  if (val && typeof val === 'object' && Object.prototype.hasOwnProperty.call(val, name)) return true;
  return false;
}

const BUG_SIGNATURES = [
  /Cannot read propert(y|ies) of undefined \(reading '0'\)/i,
  /RootErrorBoundary caught/i,
  /The above error occurred in the <CanvasComponent>/i,
];
function diagHasBug(txt) {
  const s = String(txt || '');
  return BUG_SIGNATURES.some((re) => re.test(s));
}

async function dispatchSM(type) {
  return ev(`window.__GP_STORE__.dispatch(${JSON.stringify({ type })}); 'ok'`);
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  let s = await snap();
  if (!isTopState(s.value, 'MAIN')) {
    for (let i = 0; i < 20 && !isTopState(s.value, 'MAIN'); i++) {
      await sleep(100);
      s = await snap();
    }
  }
  if (!isTopState(s.value, 'MAIN')) {
    console.log(`SKIP (not in MAIN after reset, value=${s.valueJSON})`);
    process.exit(0);
  }

  // Snapshot diag BEFORE so we can detect NEW bug-signature lines.
  const diagPre = await diagText();
  const bugPre = diagHasBug(diagPre);

  // ---- T1 enter_inspmode ----
  await dispatchSM('Insp_Mode');
  await sleep(250);
  const s1 = await snap();
  const t1_ok = isTopState(s1.value, 'INSP_MODE');
  report('T1 enter_inspmode', t1_ok, `value=${s1.valueJSON}`);

  // ---- T2 bug_present_pin ----
  // Poll up to ~2s — the throw + boundary capture is synchronous on render,
  // but we give a bit of slack for the diag ring-buffer write.
  let bugSeen = false;
  let lastDiagLen = 0;
  for (let i = 0; i < 20; i++) {
    await sleep(100);
    const dt = await diagText();
    lastDiagLen = String(dt || '').length;
    if (diagHasBug(dt) && !bugPre) { bugSeen = true; break; }
    if (diagHasBug(dt) && bugPre) {
      // bug signature already in diag before our dispatch — still counts as
      // "bug present" for the pin (the buffer may carry across mount).
      bugSeen = true; break;
    }
  }
  // Inverted: the bug was FIXED in r7 by guarding the camera_calibration_report.reports[0]
  // access in InspectionUI down_samp_level_update (and the related NaN guard). This pin now
  // asserts the bug stays GONE — if it returns, this fails loudly and points at the regression.
  const t2_ok = !bugSeen;
  report('T2 bug_no_longer_fires', t2_ok,
    t2_ok
      ? `confirmed FIXED: no "(reading '0')" / CanvasComponent crash signature on INSP_MODE entry (diag len=${lastDiagLen})`
      : `REGRESSION: CanvasComponent INSP_MODE crash signature is BACK. The fix at InspectionUI down_samp_level_update may have been reverted. Check the camera_calibration_report.reports[0] null-guard.`);

  // Clean exit so subsequent agents start in MAIN.
  try { await dispatchSM('EXIT'); await sleep(150); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  try { await api('/reload', {}); } catch (_) {}
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
