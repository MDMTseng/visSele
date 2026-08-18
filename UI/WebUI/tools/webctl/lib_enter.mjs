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
export async function toMain({ api, ev }, maxMs = 40000) {
  const t0 = Date.now();
  let splashN = 0;
  while (Date.now() - t0 < maxMs) {
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (st === '"MAIN"') return;
    const v = await ev(`(function(){var v=JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value);
      if(v.indexOf('INSP_MODE')>=0||v.indexOf('DEFCONF_MODE')>=0||v.indexOf('INSTINSP_MODE')>=0)
        window.__GP_STORE__.dispatch({type:'EXIT'});
      return v;})()`);
    if (v === '"SPLASH"') {
      splashN++;
      const rs = await ev(`(window.__GP_WS__.inst.websocket||{}).readyState`);
      if (splashN >= 12 && rs !== 0) {
        splashN = 0;
        await ev(`try{window.__GP_WS__.inst.websocket.close()}catch(e){}; 'kick'`);
      }
    } else splashN = 0;
    await sleep(1000);
  }
  throw new Error('could not reach MAIN');
}

// Clear the camera-reconnect modal if one is up. Trap 3.
//
// The skip button carries data-testid="cam-reconnect-skip"; the Chinese-label
// selector stays as a fallback so this still works against a build from before
// the hooks were added. Same pattern throughout this file: prefer the hook,
// keep the heuristic as a floor, never depend on the heuristic alone.
export async function dismissCamModal({ api, ev }, tries = 20) {
  for (let i = 0; i < tries; i++) {
    const open = await ev(
      `(function(){var ws=[...document.querySelectorAll('.ant-modal-wrap')];return ws.some(function(w){var r=w.getBoundingClientRect();return r.height>50&&getComputedStyle(w).display!=='none';});})()`
    );
    if (!open) return true;
    if (i === Math.floor(tries * 0.6)) {
      const byHook = await api('/click', { selector: '[data-testid="cam-reconnect-skip"]' }).then(() => true).catch(() => false);
      if (!byHook) await api('/click', { selector: `text=跳過相機連線` }).catch(() => {});
    }
    await sleep(1000);
  }
  return false;
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
    await sleep(200);
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
  await sleep(3000);

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

  for (let i = 0; i < 30; i++) {
    await sleep(1000);
    const st = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (String(st).indexOf('INSP_MODE') >= 0) { log(`in INSP_MODE after ${i + 1}s`); return st; }
  }
  throw new Error('play pressed but the SM never reached INSP_MODE');
}
