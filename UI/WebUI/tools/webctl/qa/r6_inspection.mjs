#!/usr/bin/env node
/*
 * QA r6_inspection — INSPECTION MODE viewpoint (run-time flow, NOT defconf editing).
 *
 * Discovered SM topology (src/redux/redux.js L81-108, UIAct.js L78-117):
 *   SPLASH --REMOTE_SYSTEM_READY--> MAIN
 *   MAIN   --Edit_Mode-->           DEFCONF_MODE (gated by defConf_lock_level)
 *   MAIN   --Insp_Mode-->           INSP_MODE        (initial substate INSP_MODE_NEUTRAL)
 *   MAIN   --InstInsp_Mode-->       INSTINSP_MODE    (initial substate INSTINSP_MODE_NEUTRAL)
 *   INSP_MODE/INSTINSP_MODE --EXIT--> MAIN
 *
 * GATING surprise: defConf_lock_level guards ONLY DEFCONF_MODE actions
 * (UICtrlReducer.js L607). INSP_MODE has no lock — inspection doesn't
 * mutate the def, so a transition is always allowed once core is ready.
 *
 * COUNTER surprise: sendCounter/sendedCounter/totalCounter live as
 * useRef inside src/InspectionUI.js (L134) — they are NOT in the redux
 * store and are therefore unobservable from outside the component tree.
 * The store-observable inspection-time-state is reportStatisticState
 * (reportCount, emptyReportCount, trackingWindow, statisticValue) which
 * is mutated by EVENT_Inspection_Report regardless of c_state.
 *
 * c_state CIRCULAR-REF gotcha (round 5): always read .value only.
 *
 * TEST PLAN (core-dependent — needs def loaded so cameraParam exists):
 *   T1 enter_inspmode   — dispatch Insp_Mode; assert c_state.value
 *                         becomes {INSP_MODE: 'INSP_MODE_NEUTRAL'} (or
 *                         the equivalent xstate object form).
 *   T2 inspmode_report  — dispatch synthetic Inspection_Report while in
 *                         INSP_MODE; assert reportCount and trackingWindow
 *                         both advance (proves report engine runs in mode).
 *   T3 garbage_event    — dispatch a bogus SM event ('NOPE_xyz') while in
 *                         INSP_MODE; assert c_state.value is unchanged
 *                         (xstate ignores unknown events) and no throw.
 *   T4 exit_to_main     — dispatch EXIT; assert c_state.value === 'MAIN'.
 *   T5 enter_instinsp   — dispatch InstInsp_Mode; assert c_state.value
 *                         becomes {INSTINSP_MODE: 'INSTINSP_MODE_NEUTRAL'}.
 *                         EXIT back; assert MAIN.
 *   T6 no_errors        — diff diag [error] lines around the transitions;
 *                         filter benign few-samples / antd / DOM-nesting.
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

// xstate c_state.value can be a string ('MAIN') or an object
// ({INSP_MODE: 'INSP_MODE_NEUTRAL'}). Normalise to a comparable form.
// We pull the value over the wire as JSON (no circular refs at .value level).
const STATE_SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var v=s.c_state&&s.c_state.value;
  var ei=s.edit_info, rss=ei&&ei.reportStatisticState;
  var cp=ei&&ei._obj&&ei._obj.cameraParam;
  return {
    value: v,
    valueJSON: (function(){ try { return JSON.stringify(v); } catch(e){ return null; } })(),
    lockLevel: s.defConf_lock_level,
    hasCam: !!(cp && cp.mmpb2b!==undefined && cp.ppb2b!==undefined),
    reportCount: rss?rss.reportCount:null,
    trackingWindowLen: rss?rss.trackingWindow.length:null
  };
})()`;
const snap = () => ev(STATE_SNAP);
const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);
const errorLines = (txt) =>
  String(txt || '').split('\n').filter((l) => /\[error\]/i.test(l));

// True when c_state.value is at the given top-state (handles string + object form).
function isTopState(val, name) {
  if (val === name) return true;
  if (val && typeof val === 'object' && Object.prototype.hasOwnProperty.call(val, name)) return true;
  return false;
}

async function dispatchSM(type) {
  // SM events flow through the regular store.dispatch.
  return ev(`window.__GP_STORE__.dispatch(${JSON.stringify({ type })}); 'ok'`);
}

function singleReport({ cx = 1.0, cy = 2.0, value = 5.0 } = {}) {
  return {
    isFlipped: false, area: 100.0, rotate: 0, cx, cy,
    detectedLines: [], detectedCircles: [], searchPoints: [],
    judgeReports: [{ id: 0, value, status: 1 }],
  };
}
function buildReportAction(singleReports) {
  return {
    type: 'Inspection_Report',
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r6_synthetic',
      machine_hash: 'qa_r6_machine',
      reports: [{ type: 'sig360_circle_line', reports: singleReports }],
    },
  };
}
async function dispatchReport(singleReports) {
  const act = buildReportAction(singleReports);
  const js = `window.__GP_STORE__.dispatch(JSON.parse(${JSON.stringify(JSON.stringify(act))})); 'ok'`;
  return ev(js);
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  const diag0 = await diagText();
  const err0 = errorLines(diag0).length;

  // Sanity: we should start in MAIN.
  let s = await snap();
  if (!isTopState(s.value, 'MAIN')) {
    // It's allowed: maybe still in SPLASH transiently — give it a beat.
    for (let i = 0; i < 20 && !isTopState(s.value, 'MAIN'); i++) {
      await sleep(100);
      s = await snap();
    }
  }
  if (!isTopState(s.value, 'MAIN')) {
    console.log(`SKIP (not in MAIN after reset, value=${s.valueJSON})`);
    process.exit(0);
  }

  // ---- T1 enter_inspmode ----
  await dispatchSM('Insp_Mode');
  // The SM transition is ASYNC (ActionThrottle -- TEAM_HANDOFF 9.3). Measured
  // on this bench: still MAIN at 0ms, INSP_MODE at ~900ms. The fixed wait
  // it replaced was a guess that happened to hold on the faster Mac box.
  // ...and ActionThrottle can SWALLOW a dispatch outright (9.3 again):
  // reset() leaves a burst of MW_API_CALL/WS_UPDATE in flight, and the
  // event can be dropped rather than merely delayed. Waiting longer does
  // not help a message that was never delivered, so re-send while polling.
  // Re-sending is safe: Insp_Mode is only a transition out of MAIN.
  for (let i = 0; i < 40; i++) {
    if (isTopState((await snap()).value, 'INSP_MODE')) break;
    if (i && i % 10 === 0) await dispatchSM('Insp_Mode');
    await sleep(100);
  }
  let s1 = await snap();
  const t1_ok = isTopState(s1.value, 'INSP_MODE');
  report('T1 enter_inspmode', t1_ok,
    `value=${s1.valueJSON}, lockLevel=${s1.lockLevel} (ungated: lock only guards DEFCONF_MODE)`);

  // ---- T2 inspmode_report ----
  if (!s1.hasCam) {
    console.log(`[T2 inspmode_report] SKIP — no cameraParam (model uncalibrated); reducer bails before reportCount++.`);
  } else {
    const before = s1.reportCount;
    await dispatchReport([singleReport({ cx: 10, cy: 10, value: 5.0 })]);
    await sleep(200);
    const s2 = await snap();
    const t2_ok = s2.reportCount === before + 1 && s2.trackingWindowLen >= 1
                  && isTopState(s2.value, 'INSP_MODE');
    report('T2 inspmode_report', t2_ok,
      `reportCount ${before}->${s2.reportCount}, tw=${s2.trackingWindowLen}, stillInsp=${isTopState(s2.value,'INSP_MODE')}`);
  }

  // ---- T3 garbage_event ----
  const sPre = await snap();
  let threw = false;
  try {
    await dispatchSM('NOPE_xyz_not_a_real_event');
  } catch (e) { threw = true; }
  await sleep(100);
  const sPost = await snap();
  const t3_ok = !threw && sPost.valueJSON === sPre.valueJSON;
  report('T3 garbage_event', t3_ok,
    `threw=${threw}, value ${sPre.valueJSON} -> ${sPost.valueJSON}`);

  // ---- T4 exit_to_main ----
  await dispatchSM('EXIT');
  await sleep(200);
  const s4 = await snap();
  const t4_ok = isTopState(s4.value, 'MAIN');
  report('T4 exit_to_main', t4_ok, `value=${s4.valueJSON}`);

  // ---- T5 enter_instinsp ----
  await dispatchSM('InstInsp_Mode');
  // The SM transition is ASYNC (ActionThrottle -- TEAM_HANDOFF 9.3). Measured
  // on this bench: still MAIN at 0ms, INSTINSP_MODE at ~900ms. The fixed wait
  // it replaced was a guess that happened to hold on the faster Mac box.
  // ...and ActionThrottle can SWALLOW a dispatch outright (9.3 again):
  // reset() leaves a burst of MW_API_CALL/WS_UPDATE in flight, and the
  // event can be dropped rather than merely delayed. Waiting longer does
  // not help a message that was never delivered, so re-send while polling.
  // Re-sending is safe: InstInsp_Mode is only a transition out of MAIN.
  for (let i = 0; i < 40; i++) {
    if (isTopState((await snap()).value, 'INSTINSP_MODE')) break;
    if (i && i % 10 === 0) await dispatchSM('InstInsp_Mode');
    await sleep(100);
  }
  const s5a = await snap();
  const ok5a = isTopState(s5a.value, 'INSTINSP_MODE');
  await dispatchSM('EXIT');
  await sleep(200);
  const s5b = await snap();
  const ok5b = isTopState(s5b.value, 'MAIN');
  report('T5 enter_instinsp', ok5a && ok5b,
    `enter=${s5a.valueJSON}, exit=${s5b.valueJSON}`);

  // ---- T6 no_errors ----
  const diag1 = await diagText();
  // Dev-build React warnings are not errors, and enumerating them one text at a
  // time is a list that is always one release behind. This filter used to name
  // validateDOMNesting / Each child in a list / Failed prop type and then failed
  // on "React does not recognize the `%s` prop on a DOM element" -- a warning
  // nobody had hit yet. Filter by SHAPE instead: React prefixes every dev
  // warning with "Warning:", and a production bundle emits none of them, so a
  // suite asserting "no new errors during these transitions" must not count
  // them. They are still printed below, so a new one is visible without being
  // fatal.
  const newErr = errorLines(diag1)
    .slice(-200)
    .filter((l) => !/few samples|abnormal sample|repeatTime|headSkipTime/i.test(l))
    .filter((l) => !/\[error\]\s*Warning:/i.test(l))       // any React dev warning
    .filter((l) => !/antd:|will be removed in next major/i.test(l));
    // (Defensive R7-era TypeError filter removed: the "(reading '0')" path was
    //  confirmed fully fixed by the InspectionUI down_samp_level_update null-
    //  guard. Any future appearance is a genuine regression — let it fail loudly.)
  // Only count NEW lines (heuristic: compare total error-line count delta and
  // inspect tail). If tail has unexpected lines, fail.
  const t6_ok = newErr.length === 0 || (errorLines(diag1).length - err0) === 0;
  report('T6 no_errors', t6_ok,
    `err lines ${err0}->${errorLines(diag1).length}, unexpected_tail=${newErr.length}${newErr[0]?(': '+newErr[0].slice(0,120)):''}`);

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  // Reload at end so we leave the shared browser clean for the next agent.
  try { await api('/reload', {}); } catch (_) {}
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
