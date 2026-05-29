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

async function reset() {
  await api('/reload', {});
  // wait for the dev store handle
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  // load def + paired image via the real core LD flow
  await ev(
    `window.__GP_LOAD_BY_PATH__(${JSON.stringify(MODEL_PATH)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
  );
  for (let i = 0; i < 80; i++) {
    await sleep(100);
    const st = await ev(
      `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList.length)||0};})()`
    );
    if (st.err) throw new Error('core LD failed: ' + st.err);
    if (st.rdy && st.n > 0) break;
  }
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

const FLOWS = {
  async load() { await reset(); return ev(SNAP); },
  async select() { await reset(); await selectFirstMeasure(); return ev(SNAP); },
  async edit() { await reset(); await selectFirstMeasure(); await editSelectedUSL(9.123); return ev(SNAP); },
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
