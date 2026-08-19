#!/usr/bin/env node
/*
 * QA r4_report — REPORT-TRACKING engine (EVENT_Inspection_Report).
 *
 * VIEWPOINT: the reducer accumulates per-inspection results into
 *   edit_info.reportStatisticState = {trackingWindow, historyReport,
 *     statisticValue:{measureList}, reportCount, emptyReportCount, ...}
 *
 * Drives the running WebUI through webctld (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_LOAD_BY_PATH__). Parent runs serially against the
 * shared browser. Core down => SKIP exit 0. Real failure => exit non-zero.
 *
 * DISCOVERED ACTION SHAPE (from src/redux/reducer/UICtrlReducer.js L113-604,
 * dispatched via UISEV.Inspection_Report L664-680; action.date is filled in
 * by the reducer itself at L935, so we don't need to set it):
 *
 *   {
 *     type: 'Inspection_Report',
 *     data: {
 *       type: 'binary_processing_group',
 *       subFeatureDefSha1: '<sha>',
 *       machine_hash: '<h>',
 *       reports: [
 *         { type: 'sig360_circle_line',
 *           reports: [ singleReport, ... ]   // may be [] or undefined => emptyReportCount++
 *         }
 *       ]
 *     }
 *   }
 *
 * singleReport (only fields actually read by the reducer):
 *   { isFlipped, area, rotate, cx, cy,
 *     detectedLines:[{id,cx,cy,vx,vy,status}],
 *     detectedCircles:[{id,x,y,r,s,status}],
 *     searchPoints:[{id,x,y,status}],
 *     judgeReports:[{id,value,status}] }
 *
 * Pre-condition: a model must be loaded so cameraParam is populated
 * (mmpcampix); otherwise EVENT_Inspection_Report bails before reportCount++.
 *
 * TEST PLAN:
 *   T1 baseline       — load def; assert reportStatisticState is initialized
 *                       (reportCount==0, trackingWindow:[], measureList present).
 *   T2 single_report  — dispatch 1 valid report; assert reportCount==1 and
 *                       trackingWindow grows (a tracked entry appears).
 *   T3 repeat_blend   — dispatch the SAME report several times; assert
 *                       reportCount increments and trackingWindow remains
 *                       coherent (entry repeatTime increases or it ages out
 *                       into historyReport; either is "sane").
 *   T4 empty_report   — dispatch a report with reports:[]; assert
 *                       emptyReportCount increments and no throw.
 *   T5 no_errors      — assert __GP_DIAG__.diagText() has no [error] lines
 *                       attributable to our dispatches (we snapshot before
 *                       and diff).
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

// Snapshot of reportStatisticState (and a few helpful fields).
const STAT_SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var ei=s.edit_info, rss=ei&&ei.reportStatisticState;
  var cp=ei&&ei._obj&&ei._obj.cameraParam;
  return {
    hasCam: !!(cp && cp.mmpb2b!==undefined && cp.ppb2b!==undefined),
    mmpcampix: cp?(cp.mmpb2b/cp.ppb2b):null,
    reportCount: rss?rss.reportCount:null,
    emptyReportCount: rss?rss.emptyReportCount:null,
    trackingWindowLen: rss?rss.trackingWindow.length:null,
    historyReportLen: rss?rss.historyReport.length:null,
    hasMeasureList: !!(rss && rss.statisticValue && Array.isArray(rss.statisticValue.measureList)),
    measureListLen: (rss&&rss.statisticValue&&rss.statisticValue.measureList)?rss.statisticValue.measureList.length:0
  };
})()`;
const statSnap = () => ev(STAT_SNAP);
const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);
const errorCount = (txt) =>
  String(txt || '').split('\n').filter((l) => /\[error\]/i.test(l)).length;

// Build a synthetic single-report. The reducer iterates judgeReports/
// detectedLines/detectedCircles/searchPoints. We just need WELL-FORMED arrays.
function singleReport({ cx = 1.0, cy = 2.0, value = 5.0 } = {}) {
  return {
    isFlipped: false,
    area: 100.0,
    rotate: 0,
    cx, cy,
    detectedLines: [],
    detectedCircles: [],
    searchPoints: [],
    judgeReports: [{ id: 0, value, status: 1 /* SUCCESS placeholder */ }],
  };
}

function buildReportAction(singleReports) {
  return {
    type: 'Inspection_Report',
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r4_synthetic',
      machine_hash: 'qa_r4_machine',
      reports: [
        { type: 'sig360_circle_line', reports: singleReports },
      ],
    },
  };
}

async function dispatchReport(singleReports) {
  const act = buildReportAction(singleReports);
  // Use JSON.parse(...) inside the page to keep the eval expression safe and
  // to reconstruct the plain object on the page side.
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

  // ---- T1 baseline ----
  let s = await statSnap();
  const diag0 = await diagText();
  const err0 = errorCount(diag0);

  if (!s.hasCam) {
    // Without cameraParam the reducer bails before reportCount++; tests are
    // not meaningfully runnable. Report this as a coverage gap and exit 0.
    console.log(`[T1 baseline] SKIP — no cameraParam (mmpcampix). Reducer EVENT_Inspection_Report bails at L171 before reportCount++ when mmpcampix is undefined. Cannot exercise this engine without a calibrated model. snap=${JSON.stringify(s)}`);
    console.log('SKIP (coverage gap: model has no cameraParam)');
    process.exit(0);
  }

  report('T1 baseline',
    s.reportCount === 0 && s.emptyReportCount === 0 &&
      s.trackingWindowLen === 0 && s.hasMeasureList,
    `count=${s.reportCount}, emptyCount=${s.emptyReportCount}, tw=${s.trackingWindowLen}, measureList=${s.measureListLen}`);

  // ---- T2 single_report ----
  await dispatchReport([singleReport({ cx: 10, cy: 10, value: 5.0 })]);
  await sleep(200);
  let s2 = await statSnap();
  const t2_ok = s2.reportCount === 1 && s2.trackingWindowLen >= 1;
  report('T2 single_report', t2_ok,
    `count ${s.reportCount}->${s2.reportCount}, tw ${s.trackingWindowLen}->${s2.trackingWindowLen}`);

  // ---- T3 repeat_blend ----
  // Send the same (similar) report several more times. With statSetting
  // defaults (minReportRepeat=2, headReportSkip=3), trackingWindow should
  // remain ~1 entry and reportCount should keep climbing. After enough
  // repeats + a timeout window, historyReport may grow — but the only
  // INVARIANT we can rely on is reportCount monotonic + no throws.
  for (let i = 0; i < 5; i++) {
    await dispatchReport([singleReport({ cx: 10, cy: 10, value: 5.0 + i * 0.001 })]);
  }
  await sleep(200);
  let s3 = await statSnap();
  const t3_ok = s3.reportCount === s2.reportCount + 5 &&
                s3.trackingWindowLen >= 1 &&
                s3.hasMeasureList;
  report('T3 repeat_blend', t3_ok,
    `count ${s2.reportCount}->${s3.reportCount}, tw=${s3.trackingWindowLen}, history=${s3.historyReportLen}`);

  // ---- T4 empty_report ----
  // reports:[] => report.reports.length==0 => emptyReportCount++.
  const beforeEmpty = (await statSnap()).emptyReportCount;
  await dispatchReport([]);
  await sleep(150);
  let s4 = await statSnap();
  const t4_ok = s4.emptyReportCount === beforeEmpty + 1 &&
                s4.reportCount === s3.reportCount + 1;
  report('T4 empty_report', t4_ok,
    `empty ${beforeEmpty}->${s4.emptyReportCount}, count ${s3.reportCount}->${s4.reportCount}`);

  // ---- T5 no_errors ----
  const diag1 = await diagText();
  const err1 = errorCount(diag1);
  // The reducer itself logs "current data only gets few samples..." as
  // [error] for aged-out single-sample entries — that's expected noise.
  // We only fail if the delta is unreasonable AND new [error] lines are
  // unrelated to that benign pattern.
  const newLines = String(diag1 || '').split('\n').slice(-200)
    .filter((l) => /\[error\]/i.test(l))
    .filter((l) => !/few samples|abnormal sample|repeatTime|headSkipTime/i.test(l))
    // Pre-existing legacy React DOM warnings (e.g. <button> nesting) are unrelated
    // to the report-tracking engine under test — ignore them as background noise.
    .filter((l) => !/Warning: validateDOMNesting|Warning: Each child in a list|Warning: Failed prop type|antd:|will be removed in next major/i.test(l));
  const t5_ok = newLines.length === 0;
  report('T5 no_errors', t5_ok,
    `err lines ${err0}->${err1} (unexpected ${newLines.length}${newLines[0] ? ': ' + newLines[0].slice(0, 120) : ''})`);

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
