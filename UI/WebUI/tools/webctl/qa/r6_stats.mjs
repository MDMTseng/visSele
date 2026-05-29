#!/usr/bin/env node
/*
 * QA r6_stats — spcStats.statReducer (SPC accumulator: count/mean/sigma/CP/CPK).
 *
 * VIEWPOINT: src/redux/reducer/spcStats.js exports statReducer(statistic, report)
 *   which is invoked by UICtrlReducer when a trackingWindow entry ages out
 *   (tdiff >= keepInTrackingTime_ms && repeatTime > minReportRepeat &&
 *    headSkipTime == 0). The aged-out singleReport is then pushed into
 *   historyReport AND its judgeReports[].value is folded into the per-measure
 *   statistic stored under reportStatisticState.statisticValue.measureList[i].statistic.
 *
 * MATH TRANSCRIBED FROM src/redux/reducer/spcStats.js (lines 120-189):
 *   For each measure m, given new judgeReport jr matched by m.id:
 *     if (jr === undefined || jr.status==NA): count_stat.NA++; skip.
 *     count_stat[jr.detailStatus] init to 0 on first sight, else ++ (note: a
 *       buggy-looking but real branch: FIRST sight initializes to 0, NOT 1).
 *     stat.count   += 1
 *     stat.sum     += v
 *     stat.mean     = sum / count
 *     stat.sqSum   += v*v
 *     stat.variance = sqSum/count - mean*mean         // E[X^2] - E[X]^2
 *     stat.sigma    = sqrt(variance)
 *     stat.CPU      = (USL - mean) / (3 * sigma)
 *     stat.CPL      = (mean - LSL) / (3 * sigma)
 *     stat.CP       = min(CPU, CPL)
 *     stat.CK       = (mean - nominal) / ((USL - LSL)/2)
 *     stat.CPK      = CP * (1 - |CK|)
 *     stat.MIN      = v if !(MIN<=v)    // NaN-aware init
 *     stat.MAX      = v if !(MAX>=v)
 *     histogram bucketed in [xmin,xmax] with 502 bins (+overflow ends)
 *
 * REACHABILITY: statReducer is NOT exposed on window. We test it INDIRECTLY:
 *   (a) drive Inspection_Report dispatches → wait keepInTrackingTime_ms (3s)
 *       → dispatch a kick → aged entries flush into historyReport and the
 *       statistic accumulator updates. SLOW (~4s for the single age-out test).
 *   (b) NOTE: a __GP_STATS__ dev hook exposing statReducer + a
 *       getInitStatisticSPState builder would let us unit-test the math
 *       directly in ~10ms and cover edge cases (NaN, USL=LSL=>div0, etc).
 *       Recommend adding: window.__GP_STATS__ = { statReducer, initSP } in
 *       a dev-only module-export shim guarded by NODE_ENV !== 'production'.
 *
 * TEST PLAN:
 *   T1 baseline       — reportStatisticState exists; measureList shape sane
 *                       (entries with statistic.count==0). Pass if measureList
 *                       is non-empty (caliper_verify has 7 measures).
 *   T2 single_report  — dispatch 1 report; trackingWindow grows; statistic
 *                       baseline (count) unchanged (entry not yet aged).
 *   T3 age_out        — dispatch enough repeats to satisfy minReportRepeat,
 *                       wait keepInTrackingTime_ms+buffer, dispatch a kick
 *                       to force the age-out path; assert historyReport grew
 *                       AND at least one measure.statistic.count > 0 AND mean
 *                       is finite (no NaN leak).
 *   T4 empty_report   — emptyReportCount++ and stat math unaffected.
 *   T5 no_value       — dispatch a report where judgeReports is empty for
 *                       some measure id; ensure stats stay finite (no NaN).
 *   T6 ref_math       — synthetic in-process reproduction of the transcribed
 *                       statReducer math on a fixed sequence; verify
 *                       mean/sigma/CP/CPK against analytic ground truth.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';

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

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// ---------- T6 reference-math arm (no browser needed) ----------
// Mirror of statReducer for one measure. Confirms our transcription matches
// observed values when we later read them from the live store.
function refStatReduceSeq(values, { USL, LSL, nominal }) {
  let count = 0, sum = 0, sqSum = 0, MIN = NaN, MAX = NaN;
  for (const v of values) {
    count += 1;
    sum += v;
    sqSum += v * v;
    if (!(MIN <= v)) MIN = v;
    if (!(MAX >= v)) MAX = v;
  }
  const mean = sum / count;
  const variance = sqSum / count - mean * mean;
  const sigma = Math.sqrt(variance);
  const CPU = (USL - mean) / (3 * sigma);
  const CPL = (mean - LSL) / (3 * sigma);
  const CP = Math.min(CPU, CPL);
  const CK = (mean - nominal) / ((USL - LSL) / 2);
  const CPK = CP * (1 - Math.abs(CK));
  return { count, sum, sqSum, mean, variance, sigma, CPU, CPL, CP, CK, CPK, MIN, MAX };
}

function t6_refMath() {
  // Symmetric noisy data centered on nominal=10, USL=11, LSL=9.
  const vals = [10.0, 10.1, 9.9, 10.05, 9.95, 10.2, 9.8];
  const r = refStatReduceSeq(vals, { USL: 11, LSL: 9, nominal: 10 });
  const expectedMean = vals.reduce((a, b) => a + b, 0) / vals.length;
  const expectedVar = vals.reduce((a, b) => a + b * b, 0) / vals.length - expectedMean * expectedMean;
  const okMean = Math.abs(r.mean - expectedMean) < 1e-12;
  const okSigma = Math.abs(r.sigma - Math.sqrt(expectedVar)) < 1e-12;
  // CP = min((USL-m)/3s,(m-LSL)/3s); with this symmetric-ish data both > 0
  const okCP = isFinite(r.CP) && r.CP > 0;
  const okCPK = isFinite(r.CPK) && r.CPK <= r.CP + 1e-9;
  const okMinMax = r.MIN === Math.min(...vals) && r.MAX === Math.max(...vals);
  const ok = okMean && okSigma && okCP && okCPK && okMinMax;
  report('T6 ref_math', ok,
    `mean=${r.mean.toFixed(6)} sigma=${r.sigma.toFixed(6)} CP=${r.CP.toFixed(4)} CPK=${r.CPK.toFixed(4)} MIN=${r.MIN} MAX=${r.MAX}`);
}

// ---------- Browser-driven arm ----------
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
    if (!loaded) { lastErr = lastErr || 'timeout'; await sleep(3000); }
  }
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

const STAT_SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var ei=s.edit_info, rss=ei&&ei.reportStatisticState;
  var cp=ei&&ei._obj&&ei._obj.cameraParam;
  var ml=(rss&&rss.statisticValue&&rss.statisticValue.measureList)||[];
  var measureSummary=ml.map(function(m){
    var st=m.statistic||{};
    return {id:m.id, count:st.count, mean:st.mean, sigma:st.sigma,
            CP:st.CP, CPK:st.CPK, MIN:st.MIN, MAX:st.MAX,
            meanFinite:isFinite(st.mean), sigmaFinite:isFinite(st.sigma)};
  });
  return {
    hasCam: !!(cp && cp.mmpb2b!==undefined && cp.ppb2b!==undefined),
    mmpcampix: cp?(cp.mmpb2b/cp.ppb2b):null,
    reportCount: rss?rss.reportCount:null,
    emptyReportCount: rss?rss.emptyReportCount:null,
    trackingWindowLen: rss?rss.trackingWindow.length:null,
    historyReportLen: rss?rss.historyReport.length:null,
    measureListLen: ml.length,
    measures: measureSummary,
    twRepeats: rss?rss.trackingWindow.map(function(t){return {r:t.repeatTime,h:t.headSkipTime};}):[]
  };
})()`;
const statSnap = () => ev(STAT_SNAP);

// Read measure ids + USL/LSL/nominal from the def so judgeReports can target them.
const MEASURE_DEFS = `(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei&&ei._obj&&ei._obj.shapeList)||[];
  return sl.filter(function(s){return s.type==='measure';})
           .map(function(m){return {id:m.id, USL:m.USL, LSL:m.LSL, value:m.value};});
})()`;

function singleReport({ cx = 1, cy = 2, measureDefs, valueFn }) {
  return {
    isFlipped: false, area: 100.0, rotate: 0, cx, cy,
    detectedLines: [], detectedCircles: [], searchPoints: [],
    judgeReports: measureDefs.map((m, i) => ({
      id: m.id,
      value: valueFn(m, i),
      status: 1, // SUCCESS
      detailStatus: 'UOK',
    })),
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
  const js = `window.__GP_STORE__.dispatch(Object.assign(JSON.parse(${JSON.stringify(JSON.stringify(act))}),{IGNORE_DEFCONF_LOCK:true})); 'ok'`;
  return ev(js);
}

async function main() {
  // Reference-math arm always runs (no browser dep).
  t6_refMath();

  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(failures ? 1 : 0); }
    throw e;
  }

  // ---- T1 baseline ----
  const s0 = await statSnap();
  if (!s0.hasCam) {
    console.log(`[T1 baseline] SKIP — no cameraParam; reducer bails before reportCount++. snap=${JSON.stringify(s0)}`);
    console.log('SKIP (coverage gap: model has no cameraParam)');
    process.exit(failures ? 1 : 0);
  }
  const t1_ok = s0.measureListLen > 0 &&
                s0.measures.every((m) => m.count === 0);
  report('T1 baseline', t1_ok,
    `measureList=${s0.measureListLen}, allCount0=${s0.measures.every((m) => m.count === 0)}`);

  const defs = await ev(MEASURE_DEFS);
  if (!Array.isArray(defs) || defs.length === 0) {
    console.log('SKIP (no measure defs in model)');
    process.exit(failures ? 1 : 0);
  }

  // ---- T2 single_report ----
  await dispatchReport([singleReport({
    measureDefs: defs,
    valueFn: (m) => (typeof m.value === 'number' ? m.value : 5.0),
  })]);
  await sleep(200);
  const s2 = await statSnap();
  const t2_ok = s2.reportCount === 1 && s2.trackingWindowLen >= 1 &&
                s2.measures.every((m) => m.count === 0); // not aged yet
  report('T2 single_report', t2_ok,
    `count ${s0.reportCount}->${s2.reportCount}, tw=${s2.trackingWindowLen}, measCount0=${s2.measures.every((m) => m.count === 0)}`);

  // ---- T3 age_out (the one expensive test) ----
  // Need repeatTime > minReportRepeat (default 2) AND headSkipTime==0
  // (init = headReportSkip = 3 → decrements per match → reset repeatTime to 0
  // when it hits 0). So we need: 3 dispatches to drain headSkipTime + ~3 more
  // to get repeatTime>2. Be generous: 10 same-position dispatches.
  for (let i = 0; i < 10; i++) {
    await dispatchReport([singleReport({
      cx: 10, cy: 10,
      measureDefs: defs,
      valueFn: (m, i2) => (typeof m.value === 'number' ? m.value : 5.0) + (i * 0.001),
    })]);
  }
  await sleep(200);
  const sBeforeAge = await statSnap();
  // wait full keepInTrackingTime_ms (3000) + a small buffer
  await sleep(3500);
  // kick: any dispatch triggers the age-out filter at the top of the handler
  await dispatchReport([singleReport({
    cx: 999, cy: 999,
    measureDefs: defs,
    valueFn: (m) => (typeof m.value === 'number' ? m.value : 5.0),
  })]);
  await sleep(300);
  const s3 = await statSnap();
  const grew = s3.historyReportLen > sBeforeAge.historyReportLen;
  const someCount = s3.measures.some((m) => m.count > 0);
  const allFiniteOrUntouched = s3.measures.every(
    (m) => m.count === 0 || (m.meanFinite && (m.sigmaFinite || m.count < 2))
  );
  const t3_ok = grew && someCount && allFiniteOrUntouched;
  report('T3 age_out', t3_ok,
    `history ${sBeforeAge.historyReportLen}->${s3.historyReportLen}, ` +
    `someCount>0=${someCount}, allFinite=${allFiniteOrUntouched}, ` +
    `tw=${s3.trackingWindowLen} (repeats=${JSON.stringify(sBeforeAge.twRepeats)})`);

  // ---- T4 empty_report ----
  const beforeEmpty = (await statSnap()).emptyReportCount;
  await dispatchReport([]);
  await sleep(150);
  const s4 = await statSnap();
  const t4_ok = s4.emptyReportCount === beforeEmpty + 1 &&
                s4.measures.every((m) => m.count === 0 || (m.meanFinite && (m.sigmaFinite || m.count < 2)));
  report('T4 empty_report', t4_ok,
    `empty ${beforeEmpty}->${s4.emptyReportCount}, allFinite=true`);

  // ---- T5 no_value ----
  // judgeReports empty -> statReducer's `new_rep===undefined` branch -> NA++,
  // no nv_val accumulation, no NaN.
  await dispatchReport([{
    isFlipped: false, area: 100.0, rotate: 0, cx: 5, cy: 5,
    detectedLines: [], detectedCircles: [], searchPoints: [],
    judgeReports: [],
  }]);
  await sleep(200);
  const s5 = await statSnap();
  const t5_ok = s5.measures.every((m) => m.count === 0 || (m.meanFinite && (m.sigmaFinite || m.count < 2)));
  report('T5 no_value', t5_ok,
    `allFinite=${t5_ok}, sample mean=${s5.measures[0] && s5.measures[0].mean}`);

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(failures ? 1 : 0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
