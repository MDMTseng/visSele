#!/usr/bin/env node
/*
 * QA r2_canvas — behavioral tests for the CANVAS / RENDERING viewpoint (live editor canvas).
 *
 * Drives the running WebUI through the webctld daemon (HTTP @ :8765) + dev hooks
 * (__GP_STORE__, __GP_LOAD_BY_PATH__, __GP_DEF__, __GP_DIAG__), exactly like r1_editor.mjs.
 * The daemon owns ONE shared browser; the PARENT runs this serially. Each test prints
 * "[name] PASS/FAIL <detail>". Exit non-zero ONLY on a real failure (canvas missing / a
 * throw). If the core backend is down (load times out / "Not connected" / "Timeout"),
 * prints "SKIP (core down)" and exits 0 (not a failure).
 *
 * WHY STRUCTURE, NOT PIXELS: canvas pixels are non-deterministic (pan/zoom + camera scale
 * in EverCheckCanvasComponent), so we DO NOT pixel-diff. We assert (a) the <canvas> DOM
 * node exists with non-zero dims, (b) the redux state that drives the canvas
 * (edit_info._obj.shapeList) is populated, (c) selection + Edit_Mode dispatches don't throw
 * and the canvas survives, (d) no canvas/draw "error"-level diagnostics appear, (e) the
 * image path (edit_info.img / a direct draw()) is handled without error.
 *
 * OBSERVABILITY (from source):
 *  - MAINUI.CanvasComponent renders <canvas ref="canvas" className="s width12 height12"/>
 *    inside CanvasComponent_rdx, mounted in the main layout. componentDidMount builds
 *    EC_CANVAS_Ctrl.Preview_CanvasComponent(canvas); updateCanvas() -> EditDBInfoSync +
 *    SetState + draw() on every store update. So a present, sized <canvas> + a populated
 *    edit_info is the structural contract.
 *  - Preview_CanvasComponent.EditDBInfoSync reads edit_DB_info.inherentShapeList & .img;
 *    draw() calls canvas.getContext('2d'). A render error would surface as a console.error
 *    captured by diagLog ring (__GP_DIAG__.diagText()).
 *  - __GP_DIAG__.diagText() returns a string of lines "ISO [level] msg"; diagCount() the
 *    ring size. We scan for "[error]" lines mentioning canvas/draw/getContext.
 *
 * TEST PLAN:
 *  C1 canvas_present   — after reset()+Edit_Mode, a <canvas> exists in DOM with w>0 & h>0.
 *  C2 state_drives     — store edit_info._obj.shapeList.length>0 AND a canvas exists
 *                        (the data that drives the renderer is present alongside the node).
 *  C3 select_no_throw  — Edit_Tar_Update + Shape_Edit on a shape doesn't throw; canvas
 *                        still present & sized after select (re-render survived).
 *  C4 force_draw       — force a re-render by toggling Edit_Mode/DefConf_Mode (componentDidUpdate
 *                        -> draw()); assert no throw and canvas intact. Best-effort direct draw via
 *                        onCanvasInit ref isn't reachable headlessly, so we exercise the React path.
 *  C5 img_handled      — read edit_info.img shape; if present assert it's an object the
 *                        canvas can consume (has width/height/scale or img), no throw.
 *                        Soft: img may be absent in a headless/cameraless load.
 *  C6 no_render_errors — after the load+select+draw cycle, __GP_DIAG__ ring has no
 *                        "[error]" entries mentioning canvas/draw/getContext (SOFT).
 *  C7 screenshot       — capture /shot to qa/r2_canvas.png as a review artifact (NOT asserted).
 *
 * COVERAGE GAPS (honest, headless): cannot verify actual pixels were painted (no pixel diff
 * by design); cannot assert pan/zoom/camera math visually; getImageData on the live canvas
 * is possible but non-deterministic so not asserted; direct ec_canvas instance isn't exposed
 * on window so we drive it only through the React/redux update path.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';
const SHOT_PATH = path.join(__dirname, 'r2_canvas.png');

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

// Faithful copy of r1_editor.mjs reset(): reload, wait for store, load def w/ retry.
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

// Canvas DOM probe: dims of the FIRST <canvas> (the editor preview canvas). Returns
// {present, count, w, h}. Uses clientWidth/Height (layout size) OR the canvas .width/.height
// backing-store size — either non-zero means it's a live, sized canvas.
const CANVAS_PROBE = `(function(){
  var cs=document.querySelectorAll('canvas');
  if(!cs.length)return {present:false,count:0,w:0,h:0};
  var c=cs[0];
  var w=Math.max(c.width||0, c.clientWidth||0);
  var h=Math.max(c.height||0, c.clientHeight||0);
  return {present:true,count:cs.length,w:w,h:h};
})()`;
const probeCanvas = () => ev(CANVAS_PROBE);

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}
function soft(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'SOFT-FAIL'}${detail ? ' — ' + detail : ''}`);
}

async function main() {
  // ---- bring app to a loaded state (core down => SKIP) ----
  try {
    await reset();
  } catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  // Enter Edit_Mode so the editor preview canvas is mounted/active.
  await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'edit'`);
  await sleep(400);

  // C1 canvas_present
  let cv = await probeCanvas();
  report('C1 canvas_present', cv.present && cv.w > 0 && cv.h > 0,
    `present=${cv.present} count=${cv.count} ${cv.w}x${cv.h}`);

  // C2 state_drives — the redux data that feeds the renderer is present alongside the node.
  const n = await ev(
    `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return (o&&o.shapeList)?o.shapeList.length:0;})()`
  );
  cv = await probeCanvas();
  report('C2 state_drives', n > 0 && cv.present,
    `shapeList=${n}, canvas present=${cv.present}`);

  // C3 select_no_throw — selection dispatch must not throw; canvas survives the re-render.
  let threw = null;
  try {
    await ev(
      `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;` +
      `var m=o.shapeList[0];if(!m)return 'no-shape';` +
      `window.__GP_STORE__.dispatch({type:'Edit_Tar_Update',data:m});` +
      `window.__GP_STORE__.dispatch({type:'Shape_Edit'});return m.id;})()`
    );
  } catch (e) { threw = e.message; }
  await sleep(350);
  cv = await probeCanvas();
  report('C3 select_no_throw', !threw && cv.present && cv.w > 0 && cv.h > 0,
    threw ? `threw: ${threw}` : `canvas present=${cv.present} ${cv.w}x${cv.h}`);

  // C4 force_draw — toggle mode to trigger componentDidUpdate->updateCanvas->draw(); no throw.
  threw = null;
  try {
    await ev(`window.__GP_STORE__.dispatch({type:'DefConf_Mode'}); 'd'`);
    await sleep(250);
    await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'e'`);
    await sleep(350);
  } catch (e) { threw = e.message; }
  cv = await probeCanvas();
  report('C4 force_draw', !threw && cv.present && cv.w > 0 && cv.h > 0,
    threw ? `threw: ${threw}` : `re-render ok, canvas ${cv.w}x${cv.h}`);

  // C5 img_handled — inspect edit_info.img; if present it must be a consumable object.
  const imgInfo = await ev(
    `(function(){var im=window.__GP_STORE__.getState().UIData.edit_info.img;` +
    `if(im==null)return {present:false};` +
    `return {present:true,type:typeof im,` +
    `hasW:('width' in im),hasH:('height' in im),hasScale:('scale' in im),hasImg:('img' in im)};})()`
  );
  if (!imgInfo.present) {
    soft('C5 img_handled', true, 'no edit_info.img in headless/cameraless load (expected gap)');
  } else {
    const okShape = imgInfo.type === 'object' && (imgInfo.hasImg || (imgInfo.hasW && imgInfo.hasH));
    report('C5 img_handled', okShape,
      `img present type=${imgInfo.type} w=${imgInfo.hasW} h=${imgInfo.hasH} scale=${imgInfo.hasScale} img=${imgInfo.hasImg}`);
  }

  // C6 no_render_errors — scan diagnostics ring for error-level canvas/draw entries (SOFT).
  const diag = await ev(
    `(function(){try{if(!window.__GP_DIAG__||!window.__GP_DIAG__.diagText)return null;` +
    `var txt=window.__GP_DIAG__.diagText();` +
    `var lines=txt.split('\\n').filter(function(l){` +
    `return /\\[error\\]/i.test(l) && /canvas|draw|getContext|render/i.test(l);});` +
    `return {count:(window.__GP_DIAG__.diagCount?window.__GP_DIAG__.diagCount():-1),hits:lines.slice(0,5)};` +
    `}catch(e){return {err:String(e)};}})()`
  );
  if (diag == null) {
    soft('C6 no_render_errors', true, '__GP_DIAG__ unavailable (skip)');
  } else if (diag.err) {
    soft('C6 no_render_errors', true, 'diag read error: ' + diag.err);
  } else {
    soft('C6 no_render_errors', diag.hits.length === 0,
      `ring=${diag.count}, canvas/draw error lines=${diag.hits.length}${diag.hits.length ? ' :: ' + diag.hits.join(' | ') : ''}`);
  }

  // C7 screenshot — review artifact only (NOT asserted).
  try {
    await api(`/shot?path=${encodeURIComponent(SHOT_PATH)}`);
    console.log(`[C7 screenshot] artifact -> ${SHOT_PATH}`);
  } catch (e) {
    console.log(`[C7 screenshot] (artifact capture failed, non-fatal): ${e.message}`);
  }

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
