#!/usr/bin/env node
// Regression oracle for the WebUI editor: load a .hydef into the running app
// (via the dev __GP_STORE__ handle), then capture/verify the serialized def
// (GenerateFeature_sig360_circle_line) + a screenshot.
//
//   golden.mjs capture <name> <hydef-path>   record baseline/<name>.{json,png}
//   golden.mjs verify  <name> <hydef-path>   re-load + diff serialized def vs baseline
//
// The serialized def is the deterministic correctness signal (it's what the UI
// sends to the core wholesale). Refactors that don't change behavior must leave
// it byte-identical.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const DIR = path.join(__dirname, 'baseline');

const [mode, name, hydef] = process.argv.slice(2);
if (!['capture', 'verify'].includes(mode) || !name || !hydef) {
  console.error('usage: golden.mjs <capture|verify> <name> <hydef-path>');
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
const evalExpr = (expr) => api('/eval', { expr }).then((r) => r.result);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function loadByPath(modelPath) {
  // Load def + paired image through the real core LD flow (deffile + imgsrc),
  // exactly as DefConfUI.loadDefFile does — a faithful end-to-end oracle.
  await evalExpr(
    `(function(){window.__GP_LOAD_BY_PATH__(${JSON.stringify(modelPath)}).then(()=>{window.__GP_LD_DONE=true}).catch(e=>{window.__GP_LD_ERR=String(e)});return "sent";})()`
  );
  for (let i = 0; i < 80; i++) {
    await sleep(100);
    const st = await evalExpr(
      `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {done:!!window.__GP_LD_DONE,err:window.__GP_LD_ERR||null,shapes:(o&&o.shapeList.length)||0};})()`
    );
    if (st.err) throw new Error('core LD failed: ' + st.err);
    if (st.done && st.shapes > 0) return;
  }
  throw new Error('def did not load via core LD (is the core running on :4090, and the path readable?)');
}

function serializeDef() {
  return evalExpr('JSON.stringify(window.__GP_DEF__())');
}

// hydef arg may be the .hydef file or the model path; the core LD wants the path w/o extension
const modelPath = hydef.replace(/\.hydef$/, '');
// Edit mode FIRST: with machine_setting InspectionMode:FI the app auto-enters
// inspection on connect, and in that mode the LD result acts are dropped -- the
// load resolves but the def never changes (found 2026-08-16).
await evalExpr(`(function(){window.__GP_STORE__.dispatch({type:"Edit_Mode"});return "edit"})()`);
await sleep(800);
await loadByPath(modelPath);
await sleep(1500);
const serialized = await serializeDef();
const pretty = JSON.stringify(JSON.parse(serialized), null, 2);

fs.mkdirSync(DIR, { recursive: true });
const jsonPath = path.join(DIR, name + '.json');
const pngPath = path.join(DIR, name + '.png');

if (mode === 'capture') {
  fs.writeFileSync(jsonPath, pretty);
  await api('/shot?' + new URLSearchParams({ path: pngPath }).toString());
  console.log(`captured: ${jsonPath}\n          ${pngPath}\n  shapes: ${JSON.parse(serialized).features.length}`);
} else {
  if (!fs.existsSync(jsonPath)) {
    console.error(`no baseline at ${jsonPath} — run capture first`);
    process.exit(2);
  }
  const baseline = fs.readFileSync(jsonPath, 'utf8');
  if (baseline === pretty) {
    console.log(`PASS: serialized def matches baseline (${name})`);
  } else {
    const cur = path.join(DIR, name + '.current.json');
    fs.writeFileSync(cur, pretty);
    console.error(`FAIL: serialized def differs from baseline.\n  baseline: ${jsonPath}\n  current:  ${cur}\n  diff:     diff ${jsonPath} ${cur}`);
    process.exit(1);
  }
}
