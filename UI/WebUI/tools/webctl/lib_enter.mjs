// The journey from a cold page to a running Inspection UI, in one place.
//
// It existed twice. flows.mjs grew a copy that works, because the flow kept
// failing in new ways and each fix landed there; enter_inspection.mjs kept the
// original four steps and rotted, until it stopped reaching the UI at all
// ("station region: NOT-IN-INSPECTION-UI"). Two implementations of one
// sequence, one of them silently broken, is what this file is here to end --
// the next probe that needs to get into the Inspection UI imports it rather
// than copying whichever version it happened to read.
//
// Every step below is a trap that cost a debugging session:
//
//  1. The state machine may be anywhere. SPLASH is transient (the core WS
//     blips and the app falls back to it until REMOTE_SYSTEM_READY) and is a
//     DEAD END while the socket is still up -- only an HR from a reconnect
//     leaves it -- so it has to be kicked, not waited out.
//  2. Reading the state and then dispatching EXIT in a second round trip
//     races the app's own exit: EXIT lands on MAIN, and MAIN+EXIT -> SPLASH.
//     Check and dispatch in ONE in-page eval.
//  3. The camera-reconnect modal (相機重連中… / 跳過相機連線) sits over MAIN
//     after a WS bounce and intercepts every click. antd keeps CLOSED modals
//     in the DOM, so "is one open" must be measured by rect height, never by
//     existence.
//  4. The diagnostics drawer may be open over the page.
//  5. 檢測方式 must be chosen before play does anything, and there are three
//     elements reading 測試 -- a title tag, one in 製程, one in 檢測方式. Only
//     the last is the mode. Coming back from inspection the side menu can be
//     collapsed and the tags are not in the DOM at all; reopen it and retry.
//  6. The play button shares its class with its 50x50 neighbours, so it is
//     found as the widest button in the bottom-right corner, at runtime.
//  7. Landing is confirmed by the state machine reaching INSP_MODE, not by a
//     fixed sleep and a DOM probe -- the old version slept 12s and guessed.

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Build the two primitives every caller needs against a webctld base URL.
export function makeCtl(base = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`) {
  const api = async (p, body) => {
    const r = await fetch(base + p, {
      method: body ? 'POST' : 'GET',
      headers: body ? { 'Content-Type': 'application/json' } : undefined,
      body: body ? JSON.stringify(body) : undefined,
    });
    const j = await r.json();
    if (!r.ok) throw new Error(JSON.stringify(j));
    return j;
  };
  const ev = (expr) => api('/eval', { expr }).then((r) => r.result);
  return { api, ev };
}

// Drive the SM to MAIN from wherever it is. Traps 1 and 2.
// POLLED AT 150 ms, NOT 1 s.
//
// Every wait in this file used to be a 1-second tick, which put a floor of
// several seconds on every suite before a single assertion ran -- entering the
// editor is three or four transitions and each one cost a full second of
// waiting for a change that had already happened. A poll is one HTTP round trip
// to the local driver (single-digit ms), so the tick is nearly free; what it
// costs is a longer tail on failure, which the deadlines below still bound.
export async function toMain({ api, ev }, maxMs = 40000) {
  const t0 = Date.now();
  let splashSince = 0;
  while (Date.now() - t0 < maxMs) {
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (st === '"MAIN"') return;
    const v = await ev(`(function(){var v=JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value);
      if(v.indexOf('INSP_MODE')>=0||v.indexOf('DEFCONF_MODE')>=0||v.indexOf('INSTINSP_MODE')>=0)
        window.__GP_STORE__.dispatch({type:'EXIT'});
      return v;})()`);
    if (v === '"SPLASH"') {
      // SPLASH is a dead end while the socket is up, so it is kicked -- but only
      // after it has been stuck for a while. Timed, not counted: the count was
      // 12 polls, which meant 12 seconds at the old tick and would now mean 1.8.
      if (!splashSince) splashSince = Date.now();
      const rs = await ev(`(window.__GP_WS__.inst.websocket||{}).readyState`);
      if (Date.now() - splashSince > 12000 && rs !== 0) {
        splashSince = 0;
        await ev(`try{window.__GP_WS__.inst.websocket.close()}catch(e){}; 'kick'`);
      }
    } else splashSince = 0;
    await sleep(150);
  }
  throw new Error('could not reach MAIN');
}

// Get to a usable page, reloading only when there is something to gain.
//
// A fresh load costs about seven seconds on this bench and almost none of it is
// the page: it is the camera-reconnect modal, which sits over MAIN until the
// WebUI hears the camera is connected (~5 s), and which no button can dismiss
// -- 跳過相機連線 only sets ALLOW_SOFT_CAM; the modal closes when the connection
// info arrives. Every suite paid that toll on every run.
//
// So: if the app is already mounted, its socket is open and nothing is covering
// the screen, keep the page. Isolation is not lost -- every suite starts with
// toMain + loadRecipe, which puts the state machine and the def back to a known
// place, and that is what isolates a run, not a reload. Anything unhealthy
// (no store, socket not open, a modal up) reloads exactly as before.
//
// WEBCTL_COLD=1 forces the reload, for when a suite really wants a virgin page.
export async function freshPage(ctl, url, { log = () => {} } = {}) {
  const { api, ev } = ctl;
  if (process.env.WEBCTL_COLD !== '1') {
    const healthy = await ev(`(function(){
      try {
        if (typeof window.__GP_STORE__ !== 'object') return false;
        var ws = (window.__GP_WS__ && window.__GP_WS__.inst && window.__GP_WS__.inst.websocket) || null;
        if (!ws || ws.readyState !== 1) return false;
        var up = Array.from(document.querySelectorAll('.ant-modal-wrap')).some(function(w){
          var r = w.getBoundingClientRect();
          return r.height > 50 && getComputedStyle(w).display !== 'none'; });
        return !up;
      } catch (e) { return false; }
    })()`).catch(() => false);
    if (healthy === true) { log('reusing the open page'); return 'warm'; }
    // A modal left open by the PREVIOUS suite is not a reason to pay for a
    // reload -- the state machine can close it. Try that once, then look again.
    // (The one modal this cannot clear is the camera-reconnect one, which no
    // button dismisses; that reloads, which is what it did before anyway.)
    const store = await ev(`typeof window.__GP_STORE__`).catch(() => 'x');
    if (store === 'object') {
      await toMain(ctl, 8000).catch(() => {});
      const clear = await ev(`(function(){
        return !Array.from(document.querySelectorAll('.ant-modal-wrap')).some(function(w){
          var r = w.getBoundingClientRect();
          return r.height > 50 && getComputedStyle(w).display !== 'none'; }); })()`).catch(() => false);
      if (clear === true) { log('reusing the open page (after closing a modal)'); return 'warm'; }
    }
  }
  await api('/goto', { url });
  const t0 = Date.now();
  while (Date.now() - t0 < 40000) {
    if ((await ev(`typeof window.__GP_STORE__`)) === 'object') break;
    await sleep(80);
  }
  return 'cold';
}

// Clear the camera-reconnect modal if one is up. Trap 3.
//
// The skip button carries data-testid="cam-reconnect-skip"; the Chinese-label
// selector stays as a fallback so this still works against a build from before
// the hooks were added. Same pattern throughout this file: prefer the hook,
// keep the heuristic as a floor, never depend on the heuristic alone.
export async function dismissCamModal({ api, ev }, budgetMs = 20000) {
  const t0 = Date.now();
  let clicked = 0;
  for (;;) {
    const open = await ev(
      `(function(){var ws=[...document.querySelectorAll('.ant-modal-wrap')];return ws.some(function(w){var r=w.getBoundingClientRect();return r.height>50&&getComputedStyle(w).display!=='none';});})()`
    );
    if (!open) return true;
    // PRESS SKIP AS SOON AS IT IS THERE, and again every second until it goes.
    //
    // The old version waited 60% of its budget before touching the button -- 12
    // seconds of standing in front of a modal whose whole purpose is to be
    // dismissed. On a bench where the modal really is up, that was the single
    // largest fixed cost in every suite: 5.7 s of a 17 s run, measured.
    //
    // Retried rather than clicked once, because it can appear a beat after the
    // first look, and clicking a button that is not there costs nothing.
    if (Date.now() - clicked > 1000) {
      clicked = Date.now();
      const byHook = await api('/click', { selector: '[data-testid="cam-reconnect-skip"]' }).then(() => true).catch(() => false);
      if (!byHook) await api('/click', { selector: `text=跳過相機連線` }).catch(() => {});
    }
    if (Date.now() - t0 > budgetMs) return false;
    await sleep(150);
  }
}

// Load a recipe by path, and wait for it to actually land.
//
// Play does nothing without one. That is the step enter_inspection.mjs never
// had: it relied on the app's startup auto-load, which only happens in FI mode
// and against whatever def the machine is configured for -- so on a machine
// with an empty recipe DB it pressed play into an empty MAIN and reported
// "did not reach the Inspection UI" without saying why.
//
// modelPath is the base path WITHOUT the extension, matching
// __GP_LOAD_BY_PATH__ (it resolves .hydef + the image sidecar itself).
export async function loadRecipe({ ev }, modelPath, { timeoutMs = 30000 } = {}) {
  await ev(`window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(modelPath)}).then(function(){window.__rdy=1}).catch(function(e){window.__rdyErr=String(e)}); 'sent'`);
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutMs) {
    await sleep(80);
    if (await ev('!!window.__rdy || !!window.__rdyErr')) break;
  }
  const err = await ev('window.__rdyErr || null');
  if (err) throw new Error('recipe load failed: ' + err);
  if (!(await ev('!!window.__rdy'))) throw new Error('recipe load timed out: ' + modelPath);
  return ev(`(((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile)||{}).name||null`);
}

// MAIN -> Inspection UI. Traps 4, 5, 6, 7. Assumes the SM is already at MAIN
// (call toMain first) -- kept separate so a caller that just loaded a recipe
// does not pay for another round trip.
export async function enterInspection(ctl, { mode = '測試', log = () => {} } = {}) {
  const { api, ev } = ctl;

  const hasDrawer = await ev(
    `(function(){var e=document.querySelector('.ant-drawer-close');if(!e)return 'no';var r=e.getBoundingClientRect();return r.height>0?'yes':'no';})()`
  );
  if (hasDrawer === 'yes') { log('closing diagnostics drawer'); await api('/click', { selector: '.ant-drawer-close' }); await sleep(2000); }

  // The mode tag, by GROUP rather than by position. 測試 appears in 製程, in
  // 檢測方式, and as a title chip, so "the last element reading 測試" was
  // position standing in for meaning -- and it moves the moment a group gains
  // a tag. data-group says which row it is.
  const modeSel = `[data-testid="tag-option"][data-group="檢測方式"][data-tag=${JSON.stringify(mode)}]`;
  const legacySel = `span.ant-tag-has-color:text-is(${JSON.stringify(mode)})`;
  let useHook = false, modeIdx = -1;
  for (let tryN = 0; tryN < 4; tryN++) {
    if (await ev(`document.querySelectorAll(${JSON.stringify(modeSel)}).length`)) { useHook = true; break; }
    modeIdx = await ev(
      `(function(){var t=[...document.querySelectorAll('span.ant-tag-has-color')].filter(function(e){return e.textContent.trim()===${JSON.stringify(mode)}});return t.length?t.length-1:-1;})()`
    );
    if (modeIdx >= 0) break;
    log('no mode tag visible -- reopening the side menu');
    await toMain(ctl);
    await api('/click', { selector: `text=主選單` }).catch(() => {});
    await sleep(2000);
  }
  if (!useHook && modeIdx < 0) throw new Error(`no ${mode} mode tag found on MAIN`);
  log(`selecting 檢測方式 = ${mode}` + (useHook ? '' : ' (legacy positional selector)'));
  await api('/click', { selector: useHook ? modeSel : `${legacySel} >> nth=${modeIdx}` });
  // Wait for what the click was FOR -- play becoming pressable -- instead of
  // three seconds of hoping. Falls through after 3 s either way, so a build
  // without the hook behaves exactly as before.
  for (let i = 0; i < 30; i++) {
    const rdy = await ev(`(document.querySelector('[data-testid="main-play"]')||{}).dataset?.ready`);
    if (rdy === '1') break;
    await sleep(100);
  }

  // Play, by identity rather than by geometry. The old rule -- widest button
  // in the bottom-right corner -- currently resolves to the FILE BROWSER when
  // the page is in a different state, and every candidate is an icon-only text
  // button, so a wrong pick clicks silently instead of failing.
  const hasPlay = await ev(`document.querySelectorAll('[data-testid="main-play"]').length`);
  if (hasPlay) {
    const ready = await ev(`(document.querySelector('[data-testid="main-play"]')||{}).dataset?.ready`);
    if (ready !== '1') log(`play is not ready (data-ready=${ready}) -- pressing anyway`);
    log('pressing play');
    await api('/click', { selector: '[data-testid="main-play"]' });
  } else {
    const playIdx = await ev(
      `(function(){var all=[...document.querySelectorAll('button.ant-btn')];var best=-1,bw=0;all.forEach(function(e,i){var r=e.getBoundingClientRect();if(r.top>innerHeight*0.8&&r.left>innerWidth*0.8&&r.width>bw){bw=r.width;best=i}});return best;})()`
    );
    if (playIdx < 0) throw new Error('play button not found on MAIN');
    log('pressing play (legacy geometric selector)');
    await api('/click', { selector: `button.ant-btn >> nth=${playIdx}` });
  }

  const tPlay = Date.now();
  while (Date.now() - tPlay < 30000) {
    await sleep(150);
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (String(st).indexOf('INSP_MODE') >= 0) {
      log(`in INSP_MODE after ${((Date.now() - tPlay) / 1000).toFixed(1)}s`); return st;
    }
  }
  throw new Error('play pressed but the SM never reached INSP_MODE');
}

// Start the PLATE -- i.e. actually make the machine run.
//
// Entering the Inspection UI is not the same thing, and the difference cost a
// long detour: the UI sat in INSP_MODE_NEUTRAL with the recipe loaded and the
// station applied, looking entirely correct, while `進料/檢測/OK` were all
// 0.0/s and the wire carried RP=0 IM=0. The panel said `STOP · 盤停止`. Nothing
// is triggered until the plate turns, so nothing is captured and nothing is
// reported. Sending trig_phantom_pulse does NOT substitute: it simulates a part
// signal, not the plate.
//
// The button is identified by what it CONTAINS rather than by position: it is
// the primary button carrying the caret (play) icon. While running it becomes
// `danger` and swaps the caret for a white square, so this same selector also
// tells you the machine is already going -- if it matches nothing, either the
// panel is closed or the plate is already turning.
//
// The firmware refuses to enter inspection mode when the device's plate_freq is
// 0 (the UI shows a speed, but "啟動時套用" means it is applied on start), in
// which case the click is accepted and the machine still does not move. That is
// why the caller should verify with reports on the wire, not with the click's
// return value.
export async function startMachine({ api, ev }, { log = () => {} } = {}) {
  const SEL = '.ant-btn-primary:has(.anticon-caret-right)';
  const n = await ev(`document.querySelectorAll(${JSON.stringify(SEL)}).length`);
  if (!n) { log('plate: no run button visible (already running, or panel closed)'); return false; }
  log('plate: pressing run');
  await api('/click', { selector: SEL });
  await sleep(4000);
  return true;
}
