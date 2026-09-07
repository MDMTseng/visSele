// How many image frames actually reach each screen, measured rather than felt.
//
//   node rate_compare.mjs [modelPath]
//   needs: a core on 4090 with a camera, `npm run preview` on 8082, webctld on 8765
//
// "快速驗證 shows images faster than InspectionUI" is a claim about the browser,
// not about the core: both open a CI subscription with the same packet, and the
// core's own ceiling (OK/NG/NA_MAX_FPS, reset to 6 at every session start)
// applies to both. So the only way to answer it is to count what each screen
// receives, in the screen.
//
// window.__DIAG_WS_TICK__ already counts IMAGE frames specifically -- it was
// installed for the renderer-memory work, where the open question was whether
// frames arrive faster than paints retire them -- and __DIAG__() returns msgHz
// and imgKBps over the interval since the last call. Two calls a fixed time
// apart is therefore a rate.
//
// It reports imgW/imgH/imgScale alongside, and that pair is the point: "more
// frames" and "bigger frames" look identical to the eye and are different
// problems. A screen can be slower while receiving MORE data.
import { makeCtl, sleep, toMain, dismissCamModal, loadRecipe, enterInspection }
  from './lib_enter.mjs';

const MODEL = process.argv[2]
  || process.env.WEBCTL_MODEL
  || 'data/test1';
const SETTLE_MS = 4000;      // let the stream reach steady state before counting
const COUNT_MS = 15000;      // long enough that a 6 fps cap resolves cleanly
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const APP = process.env.WEBCTL_APP || 'http://127.0.0.1:8082/';

const ctl = makeCtl(BASE);
const { api, ev } = ctl;
const log = (...a) => console.log(...a);

// __DIAG__() reports over the window since its LAST call, so it is called once
// to open the window and once to close it. Reading it only at the end would
// average in the page load, the def parse and the first frames, which is how a
// steady-state rate gets reported as half what it is.
async function measure(label) {
  await sleep(SETTLE_MS);
  await ev(`(function(){ try { window.__DIAG__(); return 1; } catch(e) { return 0; } })()`);
  await sleep(COUNT_MS);
  const raw = await ev(`(function(){ try { return JSON.stringify(window.__DIAG__()); }
                                     catch(e){ return JSON.stringify({err:String(e)}); } })()`);
  let d = {};
  try { d = JSON.parse(raw); } catch { d = { err: raw }; }
  if (d.err) { log(`  ${label}: __DIAG__ unavailable -- ${d.err}`); return null; }
  log(`  ${label.padEnd(14)} msgHz=${String(d.msgHz).padStart(6)}   `
    + `imgKBps=${String(d.imgKBps).padStart(7)}   `
    + `img=${d.imgW}x${d.imgH} scale=${d.imgScale}`);
  return d;
}

(async () => {
  log(`app ${APP}   model ${MODEL}`);
  await api('/goto', { url: APP });
  await sleep(3000);
  await toMain(ctl);
  await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  log('loaded, entering InspectionUI (CI)');

  await enterInspection(ctl, { log: (m) => log('  ' + m) });
  await sleep(1500);
  const insp = await measure('InspectionUI');

  // Back out, then into the def editor's 快速驗證 with the SAME def and the same
  // CI mode. Same core, same camera, same recipe -- so any difference is the
  // screen and nothing else.
  log('leaving inspection');
  await ev(`window.__GP_STORE__.dispatch({ type: 'EXIT' })`);
  await sleep(2500);
  await toMain(ctl);

  // Into the def editor by the state-machine event rather than by clicking:
  // MAIN + Edit_Mode -> DEFCONF_MODE (redux.js). The menu entry has no hook and
  // adding one whose only user is this line would be a hook that rots.
  log('entering 量測設定 -> 快速驗證 -> 檢驗(CI)');
  // EV_UI_ACT(ACT) returns { type: ACT }, so the event name IS the action type.
  // Dispatching {type:'EV_UI_ACT', data:'Edit_Mode'} is a no-op the store
  // ignores in silence -- which is what left the machine on MAIN.
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  await sleep(4000);
  log('  state=' + await ev(
    `JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`));

  const q = await ev(`(function(){
    function hit(tid){ var e=document.querySelector('[data-testid="'+tid+'"]');
      if(!e) return false; e.click(); return true; }
    return hit('quick-verify') ? 'quick' : 'no-quick-testid';
  })()`);
  log('  ' + q);
  await sleep(1200);
  const c = await ev(`(function(){
    function hit(tid){ var e=document.querySelector('[data-testid="'+tid+'"]');
      if(!e) return false; e.click(); return true; }
    return hit('quick-verify-ci') ? 'ci' : 'no-ci-testid';
  })()`);
  log('  ' + c);

  const quick = await measure('快速驗證');

  if (insp && quick) {
    const r = quick.msgHz / (insp.msgHz || 1);
    log('');
    log(`快速驗證 / InspectionUI  =  ${r.toFixed(2)}x by frame rate, `
      + `${(quick.imgKBps / (insp.imgKBps || 1)).toFixed(2)}x by bytes`);
    log(Math.abs(r - 1) < 0.25
      ? 'Same rate. A visible difference is not the frame count.'
      : 'Different rate -- and now it has a number to chase.');
  }
  process.exit(0);
})().catch((e) => { console.error('FAILED: ' + (e && e.message ? e.message : e)); process.exit(1); });
