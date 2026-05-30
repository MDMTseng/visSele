#!/usr/bin/env node
/* =============================================================================
 * r8_hrconfig.mjs — HR-TRIGGERED CONFIG LOAD QA.
 *
 * VIEWPOINT: src/comm/BPG_WS.js around L116-L260 — on the HR handshake, the
 * connect handler fires FOUR LD requests with `resolve` callbacks. Each lands
 * in a different store slot:
 *
 *   1) data/default_camera_param.json  -> resolve(data,action_channal) calls
 *      action_channal(data) === comp.WSDataDispatch(pkts). WSDataDispatch
 *      (script.jsx L274) maps each pkt via BPG_Protocol.map_BPG_Packet2Act
 *      and DISPATCHes "ATBundle". For a plain camera-param "FL" packet,
 *      map_BPG_Packet2Act ONLY emits an action when data.type ==
 *      "binary_processing_group" — so the camera-param payload reaches the
 *      Redux pipe but does NOT land in any reducer slot directly (it is also
 *      consumed by edit_info._obj.cameraParam at use time from FL re-replays).
 *      => OBSERVABLE PATH: dispatch happened (ATBundle), but no dedicated
 *      store field. We assert the FL packet was received via __GP_BPG__ /
 *      diag trail and/or that the action_channal codepath ran without error.
 *
 *   2) data/machine_setting.json       -> ACT_Machine_Custom_Setting_Update(info)
 *      => state.UIData.machine_custom_setting  (UICtrlReducer L37 / L735)
 *      Also: ACT_System_Setting_Update(merged)
 *      => state.UIData.System_Setting
 *      Side fields on info: InspectionMode (CI/FI/FI_C), setting_directory,
 *      __priv.path == "data/machine_setting.json".
 *
 *   3) data/machine_info               -> ACT_MachTag_Update(info)  (only if
 *      info.length < 16 — see L242)
 *      => state.UIData.MachTag  (UICtrlReducer L754)
 *
 *   4) data/default_camera_setting.json -> DISPATCH({type:"FILE_default_camera_setting",data:pkts[0].data})
 *      => state.UIData.FILE_default_camera_setting  (UICtrlReducer L47/L93)
 *
 * PLAN (each item -> one named test, prints "[name] PASS/FAIL/SKIP"):
 *   T1 baseline-connect       : /reload, waitStore, wait for
 *                               CORE_ID_CONN_INFO.type=="WS_CONNECTED", grace
 *                               2s for the four LDs to settle. If core down
 *                               => SKIP all and exit 0.
 *   T2 machine_setting-land   : state.UIData.machine_custom_setting is a
 *                               non-empty object AND has InspectionMode in
 *                               {"CI","FI","FI_C"} AND __priv.path ==
 *                               "data/machine_setting.json".
 *   T3 system_setting-merged  : state.UIData.System_Setting is an object
 *                               (the merge ran). Soft: if machine_setting
 *                               carried SystemSetting, it should be a superset.
 *   T4 machine_info-tag       : state.UIData.MachTag is either a string of
 *                               length 0..15 (loaded ok / empty file) or
 *                               undefined (file absent — record actual). No
 *                               crash. We record the actual value.
 *   T5 camera_setting-land    : state.UIData.FILE_default_camera_setting is
 *                               a non-empty object (default_camera_setting.json
 *                               FL packet arrived & FILE_default_camera_setting
 *                               action dispatched).
 *   T6 camera_param-pipe      : the camera_param FL packet went through the
 *                               action_channal path. Since map_BPG_Packet2Act
 *                               does NOT bind plain camera-param FL to any
 *                               reducer slot, we soft-check via __GP_DIAG__
 *                               diagText for a "default_camera_param" trace
 *                               OR confirm edit_info._obj exposes a
 *                               cameraParam after the LDs land.
 *   T7 no-errors              : __GP_DIAG__.diagText() has no NEW "[error]"
 *                               lines beyond known-legacy filters (CanvasComponent
 *                               r6 bug + redux-throttle noise).
 *
 * GAPS:
 *   - We cannot directly intercept the WSDataDispatch invocation count from
 *     outside; T6 is therefore best-effort and not a hard fail.
 *   - If data/machine_info is absent the resolve callback simply doesn't run
 *     (LD returns no FL of expected shape) — MachTag stays undefined; T4
 *     records actual rather than asserting a specific value.
 *
 * CORE-INTERMITTENCY: copies r5_wslife retry/skip-on-core-down. Core down ⇒
 * SKIP all, exit 0.
 *
 * RUN: node tools/webctl/qa/r8_hrconfig.mjs    (needs webctld :8765, app :8081;
 * parent runs serially — do NOT race against other agents on the shared
 * browser).
 * ===========================================================================*/

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const RECONNECT_TIMEOUT_MS = 30000;
const HR_LD_GRACE_MS = 2500;

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

async function coreType() {
  return ev(`(function(){
    try{
      var ci=window.__GP_STORE__.getState().ConnInfo;
      return ci.CORE_ID_CONN_INFO?ci.CORE_ID_CONN_INFO.type:null;
    }catch(e){return null;}
  })()`);
}

async function waitConnected(timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = await coreType();
    if (last === 'WS_CONNECTED') return last;
    await sleep(300);
  }
  return last;
}

async function uiSnap() {
  return ev(`(function(){
    try{
      var ui=window.__GP_STORE__.getState().UIData||{};
      var mcs=ui.machine_custom_setting||null;
      var fdcs=ui.FILE_default_camera_setting||null;
      var ss=ui.System_Setting||null;
      function summ(o){
        if(!o||typeof o!=='object') return {present:false, type:typeof o};
        var keys=Object.keys(o);
        return {present:true, nKeys:keys.length, keys:keys.slice(0,12)};
      }
      var camParam=null;
      try{
        var ei=ui.edit_info;
        if(ei && ei._obj && ei._obj.cameraParam) camParam=summ(ei._obj.cameraParam);
      }catch(e){}
      return {
        present:true,
        mcs: summ(mcs),
        mcs_inspMode: (mcs&&typeof mcs==='object')?mcs.InspectionMode:undefined,
        mcs_priv_path: (mcs&&mcs.__priv)?mcs.__priv.path:undefined,
        mcs_setdir: (mcs&&typeof mcs==='object')?mcs.setting_directory:undefined,
        ss: summ(ss),
        fdcs: summ(fdcs),
        MachTag: ui.MachTag,
        MachTag_type: typeof ui.MachTag,
        camParam_edit: camParam,
      };
    }catch(e){return {present:false,err:String(e)};}
  })()`);
}

async function diagText() {
  return ev(`(function(){
    try{
      if(!window.__GP_DIAG__||typeof window.__GP_DIAG__.diagText!=='function') return null;
      return window.__GP_DIAG__.diagText()||'';
    }catch(e){return null;}
  })()`);
}

// ---- tests ------------------------------------------------------------------
async function main() {
  await api('/reload', {});
  if (!(await waitStore())) {
    fail('T1_baseline-connect', '__GP_STORE__ never appeared');
    process.exit(1);
  }
  const ct = await waitConnected(RECONNECT_TIMEOUT_MS);
  if (ct !== 'WS_CONNECTED') {
    skip('T1_baseline-connect', `core down (coreType=${ct})`);
    skip('T2_machine_setting-land', 'core down');
    skip('T3_system_setting-merged', 'core down');
    skip('T4_machine_info-tag', 'core down');
    skip('T5_camera_setting-land', 'core down');
    skip('T6_camera_param-pipe', 'core down');
    skip('T7_no-errors', 'core down');
    console.log('\nSKIP (core down) — HR-config tests unrunnable; exiting 0.');
    process.exit(0);
  }
  pass('T1_baseline-connect', 'CORE_ID_CONN_INFO.type==WS_CONNECTED after reload');

  // Grace for the four HR-triggered LDs to round-trip & dispatch.
  await sleep(HR_LD_GRACE_MS);

  const snap = await uiSnap();
  if (!snap || !snap.present) {
    fail('T2_machine_setting-land', `UIData snap unreadable: ${JSON.stringify(snap)}`);
    fail('T3_system_setting-merged', 'UIData snap unreadable');
    fail('T4_machine_info-tag', 'UIData snap unreadable');
    fail('T5_camera_setting-land', 'UIData snap unreadable');
    skip('T6_camera_param-pipe', 'UIData snap unreadable');
  } else {
    // T2 machine_setting -> state.UIData.machine_custom_setting
    {
      const ok = snap.mcs && snap.mcs.present && snap.mcs.nKeys > 0;
      const modeOk = ['CI', 'FI', 'FI_C'].includes(snap.mcs_inspMode);
      const pathOk = snap.mcs_priv_path === 'data/machine_setting.json';
      if (ok && modeOk && pathOk) {
        pass('T2_machine_setting-land', `mcs keys=${snap.mcs.nKeys}, InspectionMode=${snap.mcs_inspMode}, __priv.path=${snap.mcs_priv_path}`);
      } else if (ok && !pathOk) {
        // mcs got populated but not by the HR LD path (e.g. earlier load); treat as soft fail
        fail('T2_machine_setting-land', `mcs populated (keys=${snap.mcs.nKeys}) but __priv.path=${snap.mcs_priv_path} (expected data/machine_setting.json); InspectionMode=${snap.mcs_inspMode}`);
      } else {
        fail('T2_machine_setting-land', `mcs=${JSON.stringify(snap.mcs)} InspMode=${snap.mcs_inspMode} __priv.path=${snap.mcs_priv_path}`);
      }
    }
    // T3 System_Setting present (merged)
    {
      const ok = snap.ss && snap.ss.present && snap.ss.nKeys > 0;
      if (ok) pass('T3_system_setting-merged', `System_Setting keys=${snap.ss.nKeys}`);
      else fail('T3_system_setting-merged', `System_Setting=${JSON.stringify(snap.ss)}`);
    }
    // T4 machine_info -> MachTag
    {
      const t = snap.MachTag_type;
      const v = snap.MachTag;
      if (t === 'undefined') {
        pass('T4_machine_info-tag', 'MachTag undefined (data/machine_info absent or did not load) — no crash');
      } else if (t === 'string' && v.length < 16) {
        pass('T4_machine_info-tag', `MachTag set (len=${v.length}, value="${v.slice(0,15)}")`);
      } else {
        fail('T4_machine_info-tag', `unexpected MachTag type=${t} val=${JSON.stringify(v)}`);
      }
    }
    // T5 default_camera_setting -> FILE_default_camera_setting
    {
      const ok = snap.fdcs && snap.fdcs.present && snap.fdcs.nKeys > 0;
      if (ok) pass('T5_camera_setting-land', `FILE_default_camera_setting keys=${snap.fdcs.nKeys}`);
      else fail('T5_camera_setting-land', `FILE_default_camera_setting=${JSON.stringify(snap.fdcs)}`);
    }
    // T6 camera_param pipe — soft check via diag or edit_info._obj.cameraParam
    {
      const txt = await diagText();
      let diagHit = false;
      if (typeof txt === 'string') {
        diagHit = txt.indexOf('default_camera_param') >= 0;
      }
      const camOnEdit = !!(snap.camParam_edit && snap.camParam_edit.present);
      if (diagHit || camOnEdit) {
        pass('T6_camera_param-pipe', `diag hit=${diagHit}, edit_info._obj.cameraParam=${camOnEdit ? 'present(keys=' + (snap.camParam_edit.nKeys) + ')' : 'absent'}`);
      } else {
        // Map_BPG_Packet2Act has no slot for plain camera_param FL — best effort.
        skip('T6_camera_param-pipe', 'no observable landing for default_camera_param.json (map_BPG_Packet2Act drops non-binary-processing-group FL) — known gap');
      }
    }
  }

  // T7 no [error] lines added during HR pipeline.
  {
    const txt = await diagText();
    if (typeof txt !== 'string') {
      skip('T7_no-errors', 'COVERAGE GAP: __GP_DIAG__.diagText() unavailable');
    } else {
      // Filter known-legacy + r6 CanvasComponent bug.
      const KNOWN_FILTERS = [
        /CanvasComponent/i,                   // round-6 known issue
        /ActionThrottle/i,                    // redux-throttle noise
        /No (uInsp|SLID|CNC|platform)_/i,     // expected "No xxx_peripheral_conn_info" path is console.log not error, but be safe
      ];
      const lines = txt.split(/\r?\n/);
      const errLines = lines.filter((ln) => /\[error\]/i.test(ln) && !KNOWN_FILTERS.some((rx) => rx.test(ln)));
      if (errLines.length === 0) pass('T7_no-errors', 'no new [error] lines in diag (filtered legacy)');
      else fail('T7_no-errors', `${errLines.length} new [error] line(s): ${errLines.slice(0,3).map((s) => s.slice(0,140)).join(' | ')}`);
    }
  }

  if (failures) { console.log(`\n${failures} test(s) FAILED.`); process.exit(1); }
  console.log('\nAll HR-config tests passed.');
  process.exit(0);
}

main().catch((e) => {
  if (isCoreDownErr(e)) {
    console.log('\nSKIP (core down) — ' + String(e.message || e));
    process.exit(0);
  }
  console.error('UNEXPECTED ERROR:', e);
  process.exit(1);
});
