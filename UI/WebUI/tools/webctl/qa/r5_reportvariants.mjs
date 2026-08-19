#!/usr/bin/env node
/*
 * QA r5_reportvariants — EVENT_Inspection_Report ALTERNATE SUBTYPES.
 *
 * VIEWPOINT: r4_report covered sig360_circle_line. EVENT_Inspection_Report
 * also handles three other branches. Exercise each, plus an unknown subtype
 * for graceful fallback.
 *
 * DISCOVERED CASE NAMES (src/redux/reducer/UICtrlReducer.js):
 *   outer action.data.type='binary_processing_group' iterates action.data.reports[]:
 *     - report.type==='sig360_extractor'      (~L155-161)
 *         → Edit_info_reset(newState);
 *         → newState.edit_info._obj.Setsig360info(action.data);
 *         → newState.edit_info.sig360info = newState.edit_info._obj.sig360info
 *         NOTE: Setsig360info expects sig360info.reports[0].signature.magnitude
 *         + .angle arrays (it rounds them in-place). So we pass action.data
 *         shaped like {reports:[{signature:{magnitude:[...], angle:[...]}}]}.
 *     - report.type==='sig360_circle_line'    (covered by r4_report)
 *     - report.type==='camera_calibration'    (~L580-589)
 *         if (report.error===0) {
 *           _obj.SetCameraParamInfo(report);
 *           edit_info.camera_calibration_report = action.data;
 *         } else {
 *           _obj.SetCameraParamInfo(undefined);
 *           edit_info.camera_calibration_report = undefined;
 *         }
 *   outer action.data.type==='stage_light_report' (~L596-600)
 *         → edit_info.stage_light_report = action.data;
 *
 * Setter signatures (src/UTIL/InspectionEditorLogic.js):
 *   Setsig360info(sig360info)        L205 — reads reports[0].signature.{magnitude,angle}
 *   SetCameraParamInfo(cameraParam)  L790 — just stores: this.cameraParam = cameraParam
 *
 * TEST PLAN:
 *   T1 sig360_extractor  — dispatch binary_processing_group whose inner report
 *                          has type='sig360_extractor'. Assert edit_info.sig360info
 *                          is set with the matching signature data (was null/None
 *                          before).
 *   T2 camera_calibration_ok — dispatch binary_processing_group inner type
 *                          'camera_calibration' with error=0; assert
 *                          edit_info.camera_calibration_report is the outer
 *                          action.data, and _obj.cameraParam updated.
 *   T3 camera_calibration_err — same but report.error=1; assert
 *                          camera_calibration_report===undefined and
 *                          _obj.cameraParam===undefined.
 *   T4 stage_light_report — dispatch outer type='stage_light_report'; assert
 *                          edit_info.stage_light_report === action.data.
 *   T5 unknown_subtype   — dispatch outer type 'totally_made_up'; assert no
 *                          throw and edit_info shape stable (no edit_info.* fields
 *                          we care about clobbered).
 *   T6 no_errors         — diagText() has no NEW [error] lines from us
 *                          (filtered for the same legacy/antd noise as r4).
 *
 * Uses IGNORE_DEFCONF_LOCK:true on each action since DEFCONF mode would
 * otherwise filter Inspection_Report (per round-3/4 findings).
 *
 * CONSTRAINTS: HTTP daemon @ :8765, ONE shared browser, core "4190",
 * dev hooks __GP_STORE__/__GP_LOAD_BY_PATH__. Core down => SKIP exit 0.
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

const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);

async function dispatch(action) {
  // Force IGNORE_DEFCONF_LOCK regardless of c_state.
  const a = { ...action, IGNORE_DEFCONF_LOCK: true };
  const js = `(function(){try{window.__GP_STORE__.dispatch(JSON.parse(${JSON.stringify(JSON.stringify(a))}));return 'ok';}catch(e){return 'THROW:'+String(e);}})()`;
  return ev(js);
}

function buildSig360ExtractorAction() {
  // action.data is what Setsig360info receives — must have reports[0].signature
  // with .magnitude / .angle arrays.
  return {
    type: 'Inspection_Report',
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r5_sig360x',
      machine_hash: 'qa_r5',
      reports: [{ type: 'sig360_extractor' }],
      // sig360info-shaped payload at top level (Setsig360info(action.data)):
      reports_signature_payload_marker: 'qa_r5',
    },
  };
  // NOTE: The reducer calls Setsig360info(action.data) — passing the OUTER
  // binary_processing_group object. That object must itself have
  // reports[0].signature.{magnitude,angle}. We craft it below.
}

function buildSig360ExtractorActionV2() {
  // action.data MUST satisfy both:
  //   - reducer reads action.data.reports.forEach(report => report.type==='sig360_extractor')
  //   - Setsig360info(action.data) reads action.data.reports[0].signature.magnitude / .angle
  // So the inner report object both has type='sig360_extractor' AND a
  // signature with the required arrays.
  return {
    type: 'Inspection_Report',
    IGNORE_DEFCONF_LOCK: true,
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r5_sig360x',
      machine_hash: 'qa_r5',
      reports: [
        {
          type: 'sig360_extractor',
          cx: 0, cy: 0,
          area: 100,
          orientation: 0,
          mmpp: 0.01,
          signature: {
            magnitude: [1.0, 1.1, 1.2, 1.3],
            angle: [0, 1.57, 3.14, 4.71],
          },
        },
      ],
    },
  };
}

function buildCameraCalibrationAction(errCode) {
  return {
    type: 'Inspection_Report',
    IGNORE_DEFCONF_LOCK: true,
    data: {
      type: 'binary_processing_group',
      subFeatureDefSha1: 'qa_r5_camcalib',
      machine_hash: 'qa_r5',
      reports: [
        {
          type: 'camera_calibration',
          error: errCode,
          mmpb2b: 0.5,
          ppb2b: 50,
          // any other fields the report needs are not read by the reducer
          qa_marker: 'r5_camcalib_' + errCode,
        },
      ],
    },
  };
}

function buildStageLightAction() {
  return {
    type: 'Inspection_Report',
    IGNORE_DEFCONF_LOCK: true,
    data: {
      type: 'stage_light_report',
      qa_marker: 'r5_stagelight',
      brightness: 42,
    },
  };
}

function buildUnknownAction() {
  return {
    type: 'Inspection_Report',
    IGNORE_DEFCONF_LOCK: true,
    data: {
      type: 'totally_made_up_xyz',
      qa_marker: 'r5_unknown',
    },
  };
}

const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var ei=s.edit_info||{};
  var obj=ei._obj||{};
  var sig=ei.sig360info;
  var cp=obj.cameraParam;
  var ccr=ei.camera_calibration_report;
  var slr=ei.stage_light_report;
  return {
    sigPresent: !!sig,
    sigMagLen: (sig&&sig.reports&&sig.reports[0]&&sig.reports[0].signature&&sig.reports[0].signature.magnitude)?sig.reports[0].signature.magnitude.length:0,
    camParamPresent: !!cp,
    camParamMmp: cp?cp.mmpb2b:null,
    camParamPp: cp?cp.ppb2b:null,
    ccrMarker: ccr?ccr.subFeatureDefSha1:null,
    ccrPresent: ccr!==undefined,
    slrMarker: slr?slr.qa_marker:null,
    slrPresent: slr!==undefined,
    cState: s.c_state && s.c_state.value, // .value only: full c_state has circular xstate machine refs
    keys: Object.keys(ei).length
  };
})()`;
const snap = () => ev(SNAP);

const errorLines = (txt) =>
  String(txt || '').split('\n')
    .filter((l) => /\[error\]/i.test(l))
    .filter((l) => !/few samples|abnormal sample|repeatTime|headSkipTime/i.test(l))
    .filter((l) => !/Warning: validateDOMNesting|Warning: Each child in a list|Warning: Failed prop type|antd:|will be removed in next major/i.test(l));

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  const diag0 = await diagText();
  const err0 = errorLines(diag0).length;
  const s0 = await snap();

  // ---- T1 sig360_extractor ----
  const act1 = buildSig360ExtractorActionV2();
  const r1 = await dispatch(act1);
  await sleep(150);
  const s1 = await snap();
  const t1_threw = String(r1 || '').startsWith('THROW');
  const t1_ok = !t1_threw && s1.sigPresent && s1.sigMagLen === 4;
  report('T1 sig360_extractor', t1_ok,
    `dispatch=${r1}, sigPresent ${s0.sigPresent}->${s1.sigPresent}, magLen=${s1.sigMagLen}`);

  // ---- T2 camera_calibration_ok ----
  const act2 = buildCameraCalibrationAction(0);
  const r2 = await dispatch(act2);
  await sleep(150);
  const s2 = await snap();
  const t2_threw = String(r2 || '').startsWith('THROW');
  const t2_ok = !t2_threw && s2.camParamPresent && s2.camParamMmp === 0.5 && s2.ccrPresent;
  report('T2 camera_calibration_ok', t2_ok,
    `dispatch=${r2}, camParam=${s2.camParamPresent}(mmp=${s2.camParamMmp},pp=${s2.camParamPp}), ccrPresent=${s2.ccrPresent}`);

  // ---- T3 camera_calibration_err ----
  const act3 = buildCameraCalibrationAction(1);
  const r3 = await dispatch(act3);
  await sleep(150);
  const s3 = await snap();
  const t3_threw = String(r3 || '').startsWith('THROW');
  const t3_ok = !t3_threw && !s3.camParamPresent && !s3.ccrPresent;
  report('T3 camera_calibration_err', t3_ok,
    `dispatch=${r3}, camParamPresent=${s3.camParamPresent}, ccrPresent=${s3.ccrPresent}`);

  // ---- T4 stage_light_report ----
  const act4 = buildStageLightAction();
  const r4 = await dispatch(act4);
  await sleep(150);
  const s4 = await snap();
  const t4_threw = String(r4 || '').startsWith('THROW');
  const t4_ok = !t4_threw && s4.slrPresent && s4.slrMarker === 'r5_stagelight';
  report('T4 stage_light_report', t4_ok,
    `dispatch=${r4}, slrPresent=${s4.slrPresent}, marker=${s4.slrMarker}`);

  // ---- T5 unknown_subtype ----
  const before5 = await snap();
  const act5 = buildUnknownAction();
  const r5 = await dispatch(act5);
  await sleep(150);
  const s5 = await snap();
  const t5_threw = String(r5 || '').startsWith('THROW');
  // Stable means: stage_light_report marker unchanged, camera report unchanged,
  // sig360 still present.
  const t5_ok = !t5_threw &&
    s5.slrMarker === before5.slrMarker &&
    s5.ccrPresent === before5.ccrPresent &&
    s5.sigPresent === before5.sigPresent;
  report('T5 unknown_subtype', t5_ok,
    `dispatch=${r5}, slr unchanged=${s5.slrMarker===before5.slrMarker}, ccr unchanged=${s5.ccrPresent===before5.ccrPresent}, sig unchanged=${s5.sigPresent===before5.sigPresent}`);

  // ---- T6 no_errors ----
  const diag1 = await diagText();
  const newLines = errorLines(String(diag1 || '').split('\n').slice(-300).join('\n'));
  const t6_ok = newLines.length === 0;
  report('T6 no_errors', t6_ok,
    `err lines ${err0}->${errorLines(diag1).length} (unexpected ${newLines.length}${newLines[0] ? ': ' + newLines[0].slice(0, 140) : ''})`);

  // reload at end per harness convention
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
