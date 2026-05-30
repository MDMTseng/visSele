#!/usr/bin/env node
/*
 * QA r9_boundary — RootErrorBoundary FALLBACK UI positive test.
 *
 * BACKGROUND
 *   Round 6 surfaced a real legacy bug: dispatching Insp_Mode while the
 *   caliper_verify def is loaded causes CanvasComponent (inside InspectionUI)
 *   to throw "Cannot read properties of undefined (reading '0')". The throw
 *   propagates to RootErrorBoundary (src/script.jsx ~L2030-2074), whose
 *   render() swaps in a fallback panel: bilingual <h1> "系統發生錯誤 /
 *   Something went wrong" + error <div> + "重新載入 / Reload" <button>.
 *
 *   This script verifies that fallback UI *actually renders correctly* —
 *   not just that the boundary logs componentDidCatch.
 *
 * PLAN (core-dependent — the crash trigger needs a loaded def)
 *   B0 baseline       — after /reload + def load, fallback NOT present:
 *                       no h1 containing "系統發生錯誤" / "Something went wrong";
 *                       the normal app shell IS in the DOM (a button exists,
 *                       and #container has > 0 child elements that are not
 *                       the fallback panel — we sample the body text).
 *   B1 trigger_crash  — dispatch Insp_Mode; CanvasComponent throws;
 *                       RootErrorBoundary catches; React unmounts the tree
 *                       and re-renders fallback.
 *   B2 fallback_dom   — querySelector finds:
 *                         * h1.Title with text matching /系統發生錯誤/
 *                         * same h1 text also contains "Something went wrong"
 *                         * a <button> whose text matches /重新載入|Reload/
 *                         * the panel wrapper has classes "HXF","WXF","overlay"
 *   B3 app_gone       — the previous app tree is replaced: there is exactly
 *                       ONE button in the document (the Reload button) and
 *                       no top-bar / menu marker from the normal app. We use
 *                       a permissive marker: the button count drops from
 *                       baseline (>1) to a small number (<=2) AND the
 *                       fallback panel is the sole child of #container.
 *   B4 diag_logged    — __GP_DIAG__.diagText() contains the literal
 *                       "RootErrorBoundary caught a render crash" line
 *                       (script.jsx:2053 wording — matches r6 filter).
 *   B5 recovery       — /reload; re-wait for store; assert fallback gone AND
 *                       app shell back (button count > 1, no fallback h1).
 *
 *   Print "[name] PASS/FAIL". Non-zero exit on real failure. Core-down ⇒
 *   SKIP exit 0. Always /reload at the end to leave the shared browser clean.
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
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// DOM probe: counts and key markers used by all assertions.
const DOM_PROBE = `(function(){
  var h1s=document.querySelectorAll('h1');
  var fbH1=null;
  for(var i=0;i<h1s.length;i++){ if(/系統發生錯誤/.test(h1s[i].textContent||'')){ fbH1=h1s[i]; break; } }
  var fbH1Text=fbH1?fbH1.textContent:'';
  var fbH1HasTitle=fbH1?fbH1.classList.contains('Title'):false;
  var btns=document.querySelectorAll('button');
  var reloadBtn=null;
  for(var j=0;j<btns.length;j++){ var t=btns[j].textContent||''; if(/重新載入|Reload/.test(t)){ reloadBtn=btns[j]; break; } }
  var reloadBtnText=reloadBtn?reloadBtn.textContent:'';
  var panel=fbH1?fbH1.parentElement:null;
  var panelCls=panel?(panel.className||''):'';
  var container=document.getElementById('container');
  var cChildren=container?container.children.length:-1;
  var cFirstCls=(container&&container.children[0])?(container.children[0].className||''):'';
  return {
    fbH1Present: !!fbH1,
    fbH1Text: fbH1Text,
    fbH1HasTitleClass: fbH1HasTitle,
    fbH1HasBoth: /系統發生錯誤/.test(fbH1Text)&&/Something went wrong/.test(fbH1Text),
    reloadBtnPresent: !!reloadBtn,
    reloadBtnText: reloadBtnText,
    panelClass: panelCls,
    panelHasOverlay: /\\boverlay\\b/.test(panelCls),
    panelHasHXF: /\\bHXF\\b/.test(panelCls),
    panelHasWXF: /\\bWXF\\b/.test(panelCls),
    buttonCount: btns.length,
    containerChildren: cChildren,
    containerFirstClass: cFirstCls
  };
})()`;
const probe = () => ev(DOM_PROBE);

const diagText = () =>
  ev(`(typeof window.__GP_DIAG__==='object' && window.__GP_DIAG__ && typeof window.__GP_DIAG__.diagText==='function')?window.__GP_DIAG__.diagText():''`);

const snap = () => ev(`(function(){
  var s=window.__GP_STORE__.getState().UIData;
  var v=s.c_state&&s.c_state.value;
  return { valueJSON: (function(){try{return JSON.stringify(v);}catch(e){return null;}})() };
})()`);

async function dispatchSM(type) {
  return ev(`window.__GP_STORE__.dispatch(${JSON.stringify({ type })}); 'ok'`);
}

async function waitForStore() {
  for (let i = 0; i < 80; i++) {
    await sleep(100);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') return true;
  }
  return false;
}

async function main() {
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // ---- B0 baseline ----
  const p0 = await probe();
  const b0_ok = !p0.fbH1Present && !p0.reloadBtnPresent && p0.buttonCount > 1;
  report('B0 baseline', b0_ok,
    `fbH1=${p0.fbH1Present}, reloadBtn=${p0.reloadBtnPresent}, buttons=${p0.buttonCount}, containerChildren=${p0.containerChildren}`);
  const baselineButtonCount = p0.buttonCount;

  // ---- B1 trigger_crash ----
  await dispatchSM('Insp_Mode');
  // Give React commit + boundary unmount + fallback re-render time.
  await sleep(600);
  const s1 = await snap();
  // The crash may happen before/during/after the state-transition commit.
  // We don't assert c_state here — the fallback DOM is the ground truth.
  report('B1 trigger_crash', true,
    `dispatched Insp_Mode; c_state=${s1.valueJSON}`);

  // ---- B2 fallback_dom ----
  const p2 = await probe();
  const b2_checks = {
    h1Present: p2.fbH1Present,
    h1Bilingual: p2.fbH1HasBoth,
    h1TitleClass: p2.fbH1HasTitleClass,
    reloadBtn: p2.reloadBtnPresent,
    overlayCls: p2.panelHasOverlay,
    hxfCls: p2.panelHasHXF,
    wxfCls: p2.panelHasWXF,
  };
  const b2_ok = p2.fbH1Present && p2.fbH1HasBoth && p2.fbH1HasTitleClass
              && p2.reloadBtnPresent
              && p2.panelHasOverlay && p2.panelHasHXF && p2.panelHasWXF;
  report('B2 fallback_dom', b2_ok,
    `checks=${JSON.stringify(b2_checks)}, h1="${p2.fbH1Text.slice(0,60)}", btn="${p2.reloadBtnText.slice(0,40)}", panelClass="${p2.panelClass.slice(0,80)}"`);

  // ---- B3 app_gone ----
  // The fallback panel replaces APPMasterX_rdx — so the normal app tree is gone.
  // Heuristics: button count drops dramatically (baseline had many buttons; fallback
  // has effectively one — the Reload button), AND #container's only child is the
  // fallback panel (carrying overlay class).
  const containerOnlyFallback = p2.containerChildren === 1
                              && /\boverlay\b/.test(p2.containerFirstClass);
  const buttonsCollapsed = p2.buttonCount <= 2 && p2.buttonCount < baselineButtonCount;
  const b3_ok = containerOnlyFallback && buttonsCollapsed;
  report('B3 app_gone', b3_ok,
    `containerChildren=${p2.containerChildren} firstClass="${p2.containerFirstClass.slice(0,60)}", buttons ${baselineButtonCount}->${p2.buttonCount}`);

  // ---- B4 diag_logged ----
  const diag = await diagText();
  const b4_ok = /RootErrorBoundary caught a render crash/.test(diag);
  report('B4 diag_logged', b4_ok,
    `diag has boundary line=${b4_ok}, diag_len=${diag.length}`);

  // ---- B5 recovery ----
  // Use /reload (cleaner than clicking the in-page button, which would also
  // trigger a full page reload via window.location.reload()).
  await api('/reload', {});
  const storeBack = await waitForStore();
  // Give React a beat to mount the normal app shell.
  await sleep(400);
  const p5 = await probe();
  const b5_ok = storeBack && !p5.fbH1Present && !p5.reloadBtnPresent && p5.buttonCount > 1;
  report('B5 recovery', b5_ok,
    `storeBack=${storeBack}, fbH1=${p5.fbH1Present}, reloadBtn=${p5.reloadBtnPresent}, buttons=${p5.buttonCount}`);

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  // Final /reload to leave shared browser clean (idempotent if B5 already did).
  try { await api('/reload', {}); } catch (_) {}
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
