#!/usr/bin/env node
/*
 * QA r3_serialize — DEF SERIALIZATION round-trip + numeric-input guard viewpoint.
 *
 * Drives the running WebUI via the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__), exactly like r1_editor.mjs /
 * golden.mjs / flows.mjs. The daemon owns ONE shared browser and the PARENT runs
 * QA scripts SERIALLY — this file never spawns its own daemon.
 *
 * __GP_DEF__() === edit_info._obj.GenerateFeature_sig360_circle_line() — the exact
 * deterministic def the UI ships to the core. It is the correctness signal.
 *
 * Each test prints "[name] PASS/FAIL <detail>"; REAL failures => exit non-zero.
 * Core down (load times out / "Not connected") => "SKIP (core down)" + exit 0.
 * This script MUTATES the def, so it reloads the browser at the end (clean leave).
 *
 * TEST PLAN (all core-DEPENDENT):
 *  S1 serialize_stable  — load def; call __GP_DEF__() twice; assert byte-identical
 *                         JSON (serialization is deterministic, no incidental order/float drift).
 *  S2 structure         — assert def.features is an array of count 26, and every feature
 *                         has type + the required fields for its type (line/arc/measure/
 *                         search_point all carry id+type; measure also USL/LSL/value;
 *                         line pt1/pt2; arc pt1; search_point pt1). Type histogram printed.
 *  S3 edit_roundtrip    — unlock (DefConf_Lock_Level_Update 0); select a measure; via
 *                         Shape_Set set USL to a known sentinel; assert __GP_DEF__()
 *                         reflects it; set it back to the original; assert restored.
 *  S4 parsefloat_guard  — THE Wave-1 fix test. Drive the property-sheet <input> for a
 *                         numeric field (the same native-value-setter + 'input' event the
 *                         r1/flows editUSLviaInput uses) with a NON-numeric value "abc".
 *                         The DefConfUI jsonChange input-number branch is guarded with
 *                         `Number.isFinite(parseFloat) ? n : prior`, so the serialized def
 *                         field must STAY its prior finite value (never NaN/null). Then
 *                         drive a VALID number through the same path and assert it takes.
 *  S5 add_serialize     — Shape_Set {shape:line, id:undefined}; assert serialized
 *                         features count grows by 1 and the new line is present with
 *                         finite pt1/pt2 coords.
 *
 * NaN-guard note: S4 truly exercises the jsonChange input path. It does NOT call
 * Shape_Set directly; it sets the real DOM <input>.value via the prototype native
 * setter and dispatches a bubbling 'input' event, so React's synthetic onChange ->
 * JsonEditBlock -> jsonChange("input-number") runs in-app, hitting the Number.isFinite
 * guard. The field under test is auto-discovered: we read input[IDX] in edit mode and
 * find which numeric def field currently equals its value, so we don't hard-code USL.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { MODEL_PATH, diagnoseLoadFailure } from './lib_model.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const EXPECTED_COUNT = 26;
// numeric property-sheet input index the r1/flows editUSLviaInput uses for the measure.
const NUM_INPUT_IDX = 2;

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

// Faithful copy of r1_editor/flows reset(): reload, wait for store, load def w/ retry.
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
  if (!loaded) { const __d = await diagnoseLoadFailure(ev, lastErr); throw new Error(__d.msg); }
}

// raw JSON string of the serialized def (byte-stable comparison signal, like golden.mjs)
const serialDefStr = () => ev('JSON.stringify(window.__GP_DEF__())');
const serialDef = async () => JSON.parse(await serialDefStr());
const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);
const shapeCount = () =>
  ev(`window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.length`);

// select first shape of a type (real selection dispatch, like r1/flows)
const selectType = (type) =>
  ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
    `var m=o.shapeList.find(function(s){return s.type===${JSON.stringify(type)};});` +
    `if(!m)return null;` +
    `window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});` +
    `window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
  );

// drive a property-sheet <input> through the REAL jsonChange path: native value
// setter + bubbling 'input' event (identical technique to flows.editUSLviaInput).
const driveInput = (idx, value) =>
  ev(
    `(function(){var ins=[].slice.call(document.querySelectorAll('input'));var el=ins[${idx}];` +
    `if(!el)return {ok:false,n:ins.length};` +
    `var set=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;` +
    `set.call(el,${JSON.stringify(String(value))});` +
    `el.dispatchEvent(new Event('input',{bubbles:true}));` +
    `return {ok:true,n:ins.length,val:el.value};})()`
  );

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

const REQUIRED = {
  line: ['id', 'type', 'pt1', 'pt2'],
  arc: ['id', 'type', 'pt1'],
  measure: ['id', 'type', 'USL', 'LSL', 'value'],
  search_point: ['id', 'type', 'pt1'],
};

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }
  // edit mode so the property sheet renders <input>s for S4
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(300);

  // ---- S1 serialize_stable ----
  const a = await serialDefStr();
  await sleep(50);
  const b = await serialDefStr();
  report('S1 serialize_stable', a === b && a && a.length > 2,
    `len=${a ? a.length : 0}, identical=${a === b}`);

  // ---- S2 structure ----
  const def = JSON.parse(a);
  const feats = Array.isArray(def.features) ? def.features : [];
  const hist = feats.reduce((m, f) => ((m[f.type] = (m[f.type] || 0) + 1), m), {});
  const bad = [];
  for (const f of feats) {
    if (!f || typeof f.type !== 'string') { bad.push('untyped'); continue; }
    const req = REQUIRED[f.type];
    if (req) for (const k of req) if (!(k in f)) bad.push(`${f.type}#${f.id}:missing ${k}`);
  }
  report('S2 structure', feats.length === EXPECTED_COUNT && bad.length === 0,
    `count=${feats.length}/${EXPECTED_COUNT} hist=${JSON.stringify(hist)}` +
    (bad.length ? ` BAD=[${bad.slice(0, 5).join('; ')}]` : ''));

  // ---- S3 edit_roundtrip ----
  await unlock();
  const measureId = await selectType('measure');
  await sleep(250);
  if (measureId == null) {
    report('S3 edit_roundtrip', false, 'no measure shape present');
  } else {
    const orig = feats.find((f) => f.id === measureId && f.type === 'measure');
    const origUSL = orig ? orig.USL : null;
    const sentinel = 12.3456;
    const setUSL = (v) =>
      ev(
        `(function(){window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0});` +
        `var e=window.__GP_STORE__.getState().UIData.edit_info;` +
        `var m=Object.assign({},e.edit_tar_info,{USL:${v}});` +
        `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:m,id:m.id}});return m.id;})()`
      );
    await setUSL(sentinel);
    await sleep(350);
    let d = await serialDef();
    let mf = d.features.find((f) => f.id === measureId && f.type === 'measure');
    const setOk = mf && Math.abs(Number(mf.USL) - sentinel) < 1e-6;
    // restore
    await setUSL(origUSL);
    await sleep(350);
    d = await serialDef();
    mf = d.features.find((f) => f.id === measureId && f.type === 'measure');
    const restored = mf && Math.abs(Number(mf.USL) - Number(origUSL)) < 1e-6;
    report('S3 edit_roundtrip', !!(setOk && restored),
      `id=${measureId} orig=${origUSL} set->${setOk} restore->${restored}`);
  }

  // ---- S4 parsefloat_guard (Wave-1 fix) ----
  // measure still selected; the property sheet shows its numeric inputs.
  await unlock();
  await sleep(150);
  // discover which numeric def field input[NUM_INPUT_IDX] currently maps to, by
  // reading the input's value and matching it to a finite measure field in the def.
  const d0 = await serialDef();
  const m0 = d0.features.find((f) => f.id === measureId && f.type === 'measure');
  const inVal = await ev(
    `(function(){var ins=document.querySelectorAll('input');var el=ins[${NUM_INPUT_IDX}];return el?el.value:null;})()`
  );
  const numKeys = m0 ? Object.keys(m0).filter((k) => Number.isFinite(Number(m0[k]))) : [];
  const fieldKey = numKeys.find((k) => Math.abs(Number(m0[k]) - parseFloat(inVal)) < 1e-9);

  if (m0 == null || fieldKey == null || inVal == null || isNaN(parseFloat(inVal))) {
    report('S4 parsefloat_guard', false,
      `cannot map input[${NUM_INPUT_IDX}] (val=${inVal}) to a numeric measure field`);
  } else {
    const prior = Number(m0[fieldKey]);
    // (a) drive a NON-numeric value through the real jsonChange path
    const r1 = await driveInput(NUM_INPUT_IDX, 'abc');
    await sleep(400);
    const dA = await serialDef();
    const mA = dA.features.find((f) => f.id === measureId && f.type === 'measure');
    const valA = mA ? mA[fieldKey] : undefined;
    const guarded =
      mA && valA != null && Number.isFinite(Number(valA)) && Math.abs(Number(valA) - prior) < 1e-6;

    // (b) drive a VALID number through the same path -> must take
    const good = prior + 1.5;
    const r2 = await driveInput(NUM_INPUT_IDX, String(good));
    await sleep(400);
    const dB = await serialDef();
    const mB = dB.features.find((f) => f.id === measureId && f.type === 'measure');
    const valB = mB ? Number(mB[fieldKey]) : NaN;
    const accepted = Number.isFinite(valB) && Math.abs(valB - good) < 1e-6;

    report('S4 parsefloat_guard', !!(r1.ok && r2.ok && guarded && accepted),
      `field=${fieldKey} prior=${prior} afterAbc=${valA}(guarded=${guarded}) ` +
      `afterNum=${valB}(want ${good},accepted=${accepted}) inputs=${r1.n}`);
  }

  // ---- S5 add_serialize ----
  await unlock();
  await sleep(150);
  const beforeFeats = (await serialDef()).features.length;
  const beforeShapes = await shapeCount();
  await ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',data:{shape:{type:'line',pt1:{x:111,y:222},pt2:{x:333,y:444}},id:undefined}}); 'add'`
  );
  await sleep(450);
  const dAdd = await serialDef();
  const afterShapes = await shapeCount();
  const newLine = dAdd.features.find(
    (f) => f.type === 'line' && f.pt1 && Math.abs(Number(f.pt1.x) - 111) < 1e-6 && Math.abs(Number(f.pt1.y) - 222) < 1e-6
  );
  const coordsOk =
    newLine &&
    [newLine.pt1.x, newLine.pt1.y, newLine.pt2.x, newLine.pt2.y].every((v) => Number.isFinite(Number(v)));
  report('S5 add_serialize',
    dAdd.features.length === beforeFeats + 1 && afterShapes === beforeShapes + 1 && !!coordsOk,
    `features ${beforeFeats}->${dAdd.features.length}, shapes ${beforeShapes}->${afterShapes}, ` +
    `newLine=${newLine ? 'found pt2=(' + newLine.pt2.x + ',' + newLine.pt2.y + ')' : 'MISSING'}`);

  // leave the browser clean: reload (we mutated the def)
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
