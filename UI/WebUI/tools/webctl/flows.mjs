#!/usr/bin/env node
// Behavioral regression suite for the WebUI editor. Drives key user flows through
// the running app (via the webctld daemon + dev hooks) and captures a deterministic
// snapshot per flow: serialized def + store state + property-sheet DOM values.
//
//   flows.mjs capture [flow]   record baseline/flows/<flow>.{json,png} (all flows if omitted)
//   flows.mjs verify  [flow]   re-run + diff the JSON snapshot vs baseline
//
// Unlike golden.mjs (def-serialization only), this exercises selection/edit and the
// React re-render path (property-sheet <input> values), so it can catch regressions
// in state-ownership / re-render refactors. Screenshots are kept as review artifacts
// (not diffed — the canvas pan/zoom is non-deterministic).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const DIR = path.join(__dirname, 'baseline', 'flows');
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';

const mode = process.argv[2];
const only = process.argv[3];
if (!['capture', 'verify'].includes(mode)) {
  console.error('usage: flows.mjs <capture|verify> [flow]');
  process.exit(2);
}

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

async function reset(modelPath = MODEL_PATH) {
  await api('/reload', {});
  // wait for the dev store handle
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  // Wait out the app's OWN startup auto-load before loading the model. With
  // machine_setting InspectionMode:FI the app loads the machine def ~1.5s
  // after connect, and it races our load -- whoever lands second wins, so the
  // suite would snapshot a random machine def instead of the model (found
  // 2026-08-16: every flow diffing against a '10155' def nobody asked for).
  // Settle = loadedDefFile.name unchanged across two 1s polls.
  {
    let prev = null;
    for (let i = 0; i < 12; i++) {
      await sleep(1000);
      const nm = await ev(
        `(((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile)||{}).name||null`
      );
      if (nm !== null && nm === prev) break;
      prev = nm;
    }
  }
  // Enter the editor BEFORE loading the model: in the auto-entered FI mode the
  // LD result acts are dropped by the mode guards, so the load "resolves" and
  // changes nothing.
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(800);
  // load def + paired image via the real core LD flow. The core WS can transiently
  // drop (auto-reconnects), so retry the load a few times rather than fail spuriously.
  let loaded = false;
  for (let attempt = 0; attempt < 4 && !loaded; attempt++) {
    await ev(
      `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(modelPath)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
    );
    let err = null;
    for (let i = 0; i < 80; i++) {
      await sleep(100);
      const st = await ev(
        `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList.length)||0};})()`
      );
      if (st.err) { err = st.err; break; }
      if (st.rdy && st.n > 0) { loaded = true; break; }
    }
    if (!loaded) {
      console.warn(`  (load attempt ${attempt + 1} failed${err ? ': ' + err : ' (timeout)'}; core reconnecting, retrying)`);
      await sleep(3000); // give the UI's reconnect loop time
    }
  }
  if (!loaded) throw new Error('def did not load after retries (core on :4090 reachable?)');
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(500);
}

// pick the first measure shape, dispatch the real selection (sets edit target + SM->SHAPE_EDIT)
async function selectFirstMeasure() {
  await ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;var m=o.shapeList.find(s=>s.type==='measure');window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
  );
  await sleep(500);
}

// modify the selected measure's USL via the real Shape_Set action. A loaded def is
// lock_level=1 (read-only); unlock first (the whitelisted action the UI uses), as a
// user would before editing.
async function editSelectedUSL(value) {
  await ev(
    `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});var e=window.__GP_STORE__.getState().UIData.edit_info;var m=Object.assign({},e.edit_tar_info,{USL:${value}});window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:m,id:m.id}});return m.id;})()`
  );
  await sleep(500);
}

// edit the selected measure's USL by driving the real property-sheet <input> (idx 2 for
// the caliper_verify measure). This goes through the JsonEditBlock -> jsonChange path,
// exercising the value<->control-limit COUPLING (UCL recompute) that Shape_Set bypasses.
async function editUSLviaInput(value) {
  await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'unlock'`);
  await sleep(200);
  // Find the USL input by its CURRENT VALUE from the store, not by a fixed
  // index -- the property sheet gains/loses fields as the app evolves and a
  // hardcoded ins[2] silently starts editing a different field (it did).
  const idx = await ev(
    `(function(){var e=window.__GP_STORE__.getState().UIData.edit_info.edit_tar_info;var usl=String(e.USL);var ins=[...document.querySelectorAll('input')];return ins.findIndex(i=>i.value===usl);})()`
  );
  if (idx < 0) throw new Error('USL input not found');
  // REAL Playwright typing, not a synthetic Event: the measure sheet is a
  // per-shape PropertySheet now (antd controls), and a dispatched 'input'
  // event no longer reaches its onChange -- the same lesson as clicks on
  // antd divs (see project notes): synthetic events silently no-op.
  await api('/fill', { selector: `input >> nth=${idx}`, value: String(value) });
  await api('/press', { selector: `input >> nth=${idx}`, key: 'Enter' });
  await sleep(500);
}

// add a brand-new line via the real Shape_Set path (id undefined -> push). Exercises
// the immutable array-add + shape-list re-render.
async function addLine() {
  await ev(
    `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'line',pt1:{x:100,y:100},pt2:{x:300,y:300}},id:undefined}});return 'added';})()`
  );
  await sleep(500);
}

const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj, et=s.edit_info.edit_tar_info;
  return {
    c_state: s.c_state.value,
    lock: s.defConf_lock_level,
    shapes: o.shapeList.map(x=>({id:x.id,type:x.type,name:x.name})),
    inherent: o.inherentShapeList.length,
    edit_tar: et?{id:et.id,type:et.type,subtype:et.subtype,USL:et.USL,LSL:et.LSL,UCL:et.UCL,LCL:et.LCL}:null,
    inputs: Array.from(document.querySelectorAll('input')).map(i=>i.value),
    def_measure_usl: (function(){var d=window.__GP_DEF__();var m=d.features.find(f=>f.type==='measure');return m?m.USL:null;})()
  };
})()`;

// delete the just-added shape. The reducer uses Shape_Set with shape=null +
// existing id to mean "remove" (see UICtrlReducer:847+ comment "ID is defined
// and shape is null - delete"). Exercises the immutable array-delete path.
async function deleteLastShape() {
  await ev(
    `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});var o=window.__GP_STORE__.getState().UIData.edit_info._obj;var last=o.shapeList[o.shapeList.length-1];window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:null,id:last.id}});return last.id;})()`
  );
  await sleep(500);
}

// add an arc shape (3-point primitive) — verifies the arc add path independent
// of line. Different applyDefaults branch + different draw module.
async function addArc() {
  await ev(
    `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'arc',pt1:{x:100,y:100},pt2:{x:200,y:50},pt3:{x:300,y:100}},id:undefined}});return 'added';})()`
  );
  await sleep(500);
}

// add a measure shape (subtype dispatcher) — verifies measure init goes through
// the per-subtype registry path. Distance is the default subtype.
async function addMeasure() {
  await ev(
    `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'measure',subtype:'distance',pt1:{x:200,y:200},value:5,USL:5.5,LSL:4.5,UCL:5.3,LCL:4.7,ref:[]},id:undefined}});return 'added';})()`
  );
  await sleep(500);
}

// ---- inspection-mode enter/exit cycle -------------------------------------
// Covers the operator's main road, which had NO automated test: load a tagged
// def, enter the Inspection UI through the REAL menu (mode tag + play button --
// dispatching Insp_Mode from the editor is a no-op, the SM only accepts it
// from MAIN), assert the tag's control-margin limits were applied to the
// shape_list, exit, assert they were restored and the editor is not dirty.
// The fixture def carries __decorator.control_margin_info.TAGX overriding one
// measure's USL by +7.77.
const TAGGED_MODEL = path.join(__dirname, 'fixtures', 'caliper_verify_tagged');
// Drive the SM to MAIN from wherever it is. SPLASH is transient (the core WS
// drops and reconnects -- the known 1006 blips -- and the app falls back to
// SPLASH until REMOTE_SYSTEM_READY); INSP_MODE exits with the same EVENT the
// back control fires. Waits out both.
async function toMain(maxMs = 40000) {
  const t0 = Date.now();
  while (Date.now() - t0 < maxMs) {
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (st === '"MAIN"') return;
    if (st.includes('INSP_MODE') || st.includes('DEFCONF_MODE') || st.includes('INSTINSP_MODE'))
      await ev(`window.__GP_STORE__.dispatch({type:'EXIT'}); 'x'`);
    await sleep(1000);
  }
  throw new Error('could not reach MAIN');
}
async function inspCycle() {
  // Cold app, settle the auto-load, but do NOT enter the editor: the play
  // button inspects the RECIPE selected at MAIN (edit_info + defModelPath),
  // not whatever an editor session holds -- the first version of this flow
  // loaded the fixture in the editor and then watched play inspect the
  // machine def instead. Set the recipe exactly the way the recipe picker
  // does: load + Def_Model_Path_Update + InspOptionalTag_Update.
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  { // settle the startup auto-load (see reset())
    let prev = null;
    for (let i = 0; i < 12; i++) {
      await sleep(1000);
      const nm = await ev(`(((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile)||{}).name||null`);
      if (nm !== null && nm === prev) break;
      prev = nm;
    }
  }
  // the app auto-enters INSP_MODE (machine InspectionMode:FI); back to MAIN
  await toMain();
  await sleep(1000);
  // install the tagged fixture as the recipe, the picker's way
  await ev(
    `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(TAGGED_MODEL)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
  );
  for (let i = 0; i < 80; i++) {
    await sleep(100);
    if (await ev('!!window.__rdy || !!window.__rdyErr')) break;
  }
  const loadErr = await ev('window.__rdyErr||null');
  if (loadErr) throw new Error('fixture load failed: ' + loadErr);
  await ev(`window.__GP_STORE__.dispatch({type:'Def_Model_Path_Update',data:${JSON.stringify(TAGGED_MODEL)}});window.__GP_STORE__.dispatch({type:'InspOptionalTag_Update',data:['TAGX']}); 'recipe'`);
  await sleep(800);
  const before = await ev(
    `(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {def:(s.edit_info.loadedDefFile||{}).name||null, usl:m?m.USL:null, margin:JSON.stringify(((s.edit_info.__decorator||{}).control_margin_info||{}).TAGX||null)};})()`
  );
  if (before.def !== 'caliper_verify_tagged')
    throw new Error('recipe did not take at MAIN (def=' + before.def + ')');
  await toMain();   // a WS blip between the load and here bounces via SPLASH
  // The camera-reconnect modal (相機重連中... / 跳過相機連線) sits over MAIN
  // after a WS bounce and intercepts every click. It normally closes itself
  // when the (fake) camera answers; give it time, then skip it explicitly.
  // Visibility = rect height, not existence: antd keeps closed modals in the
  // DOM (project note).
  for (let i = 0; i < 20; i++) {
    const open = await ev(
      `(function(){var ws=[...document.querySelectorAll('.ant-modal-wrap')];return ws.some(function(w){var r=w.getBoundingClientRect();return r.height>50&&getComputedStyle(w).display!=='none';});})()`
    );
    if (!open) break;
    if (i === 12) await api('/click', { selector: `text=跳過相機連線` }).catch(() => {});
    await sleep(1000);
  }
  // The three traps of entering from MAIN, in order (see enter_inspection.mjs):
  // 1. the diagnostics drawer may be open over the page -- close it first
  const hasDrawer = await ev(
    `(function(){var e=document.querySelector('.ant-drawer-close');if(!e)return 'no';var r=e.getBoundingClientRect();return r.height>0?'yes':'no';})()`
  );
  if (hasDrawer === 'yes') { await api('/click', { selector: '.ant-drawer-close' }); await sleep(2000); }
  // 2. 檢測方式 must be chosen before play does anything; the mode row's 測試
  //    is the LAST tag reading 測試 on the page. Coming back from inspection
  //    (unlike a cold load) MAIN's side menu can be collapsed and the tags
  //    don't exist in the DOM -- open it via the 主選單 edge tab and retry.
  let modeIdx = -1;
  for (let tryN = 0; tryN < 4 && modeIdx < 0; tryN++) {
    modeIdx = await ev(
      `(function(){var t=[...document.querySelectorAll('span.ant-tag-has-color')].filter(function(e){return e.textContent.trim()==='測試'});return t.length?t.length-1:-1;})()`
    );
    if (modeIdx < 0) {
      await toMain();   // SPLASH bounce leaves an empty page with no tags
      await api('/click', { selector: `text=主選單` }).catch(() => {});
      await sleep(2000);
    }
  }
  if (modeIdx < 0) throw new Error('no 測試 mode tag found on MAIN');
  await api('/click', { selector: `span.ant-tag-has-color:text-is('測試') >> nth=${modeIdx}` });
  await sleep(3000);
  // 3. the REAL entry: the widest button in the bottom-right bar (the class is
  //    shared with its 50x50 neighbours; resolved at runtime)
  const playIdx = await ev(
    `(function(){var all=[...document.querySelectorAll('button.ant-btn')];var best=-1,bw=0;all.forEach(function(e,i){var r=e.getBoundingClientRect();if(r.top>innerHeight*0.8&&r.left>innerWidth*0.8&&r.width>bw){bw=r.width;best=i}});return best;})()`
  );
  if (playIdx < 0) throw new Error('play button not found on MAIN');
  await api('/click', { selector: `button.ant-btn >> nth=${playIdx}` });
  // wait until the SM actually lands in INSP_MODE
  let inInsp = false;
  for (let i = 0; i < 30; i++) {
    await sleep(1000);
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (st.includes('INSP_MODE')) { inInsp = true; break; }
  }
  await sleep(2000);   // let APP_INSP_MODE's mount finish (tag apply + FI/CI send)
  const during = inInsp ? await ev(
    `(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {state:JSON.stringify(s.c_state.value), def:(s.edit_info.loadedDefFile||{}).name||null, usl:(m&&m.USL!==undefined)?m.USL:null};})()`
  ) : { state: 'NEVER-ENTERED', def: null, usl: null };
  // exit -- the same EXIT event the back control fires; unmount must restore.
  // Poll FAST: ~1.5s after landing on MAIN the app can reload the machine
  // def and replace edit_info wholesale, which would make a slow read see a
  // different def rather than the restored one.
  await ev(`window.__GP_STORE__.dispatch({type:'EXIT'}); 'out'`);
  let after = { state: null, usl: null };
  for (let i = 0; i < 20; i++) {
    await sleep(100);
    after = await ev(
      `(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {state:JSON.stringify(s.c_state.value), usl:(m&&m.USL!==undefined)?m.USL:null};})()`
    );
    if (after.usl === before.usl) break;   // restored -- stop before any reload
  }
  // The USL going back to 8.7 alone cannot tell the unmount RESTORE from the
  // MAIN machine-def/fixture reload landing first -- the flow was green once
  // while the restore silently never ran (identity-gated; the console trace
  // exposed it). Demand the component's own log line as evidence.
  const logs = await api('/logs').catch(() => null);
  const logArr = (logs && (logs.logs || logs)) || [];
  const restoreLogged = logArr.slice(-300).some(l =>
    String((l && l.text) || '').includes('[insp-exit] tag-limit overrides restored'));
  return {
    c_state: { entered: inInsp },
    shapes: [],   // keep the runner's "captured (N shapes)" line happy
    def_at_main: before.def,
    def_in_insp: during.def,
    usl_before: before.usl,
    margin_present: before.margin,
    usl_in_insp: during.usl,
    state_in_insp: during.state,
    usl_after_exit: after.usl,
    override_applied: during.usl !== null && during.usl !== before.usl,
    restored: after.usl === before.usl,
    restore_logged: restoreLogged,
  };
}

const FLOWS = {
  async load() { await reset(); return ev(SNAP); },
  async select() { await reset(); await selectFirstMeasure(); return ev(SNAP); },
  async edit() { await reset(); await selectFirstMeasure(); await editSelectedUSL(9.123); return ev(SNAP); },
  async editInput() { await reset(); await selectFirstMeasure(); await editUSLviaInput(9.0); return ev(SNAP); },
  async add() { await reset(); await addLine(); return ev(SNAP); },
  async addArc() { await reset(); await addArc(); return ev(SNAP); },
  async addMeasure() { await reset(); await addMeasure(); return ev(SNAP); },
  async addThenDelete() { await reset(); await addLine(); await deleteLastShape(); return ev(SNAP); },
  inspCycle,
};

const names = only ? [only] : Object.keys(FLOWS);
fs.mkdirSync(DIR, { recursive: true });
let failures = 0;

for (const name of names) {
  if (!FLOWS[name]) { console.error(`unknown flow: ${name}`); process.exit(2); }
  process.stdout.write(`[${name}] running... `);
  const snap = await FLOWS[name]();
  const pretty = JSON.stringify(snap, null, 2);
  const jsonPath = path.join(DIR, name + '.json');
  await api('/shot?' + new URLSearchParams({ path: path.join(DIR, name + '.png') }).toString());

  if (mode === 'capture') {
    fs.writeFileSync(jsonPath, pretty);
    console.log(`captured (${snap.shapes.length} shapes, c_state=${JSON.stringify(snap.c_state)})`);
  } else {
    if (!fs.existsSync(jsonPath)) { console.log('NO BASELINE'); failures++; continue; }
    const baseline = fs.readFileSync(jsonPath, 'utf8');
    if (baseline === pretty) {
      console.log('PASS');
    } else {
      const cur = path.join(DIR, name + '.current.json');
      fs.writeFileSync(cur, pretty);
      console.log(`FAIL — diff ${jsonPath} ${cur}`);
      failures++;
    }
  }
}
if (failures) process.exit(1);
