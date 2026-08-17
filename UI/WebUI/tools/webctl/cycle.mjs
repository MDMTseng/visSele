#!/usr/bin/env node
// Mode round-trip endurance for the WebUI: N laps of the operator's whole day.
//
//   MAIN -> editor (DEFCONF_MODE): load the fixture def, run a REAL
//           inst-check (EX snap exam -> inspReport lands) -> EXIT
//        -> MAIN: install the recipe with a DIFFERENT NG-range setup per lap
//           (odd laps tag ['TAGX'] -- the margin overrides measure id 8's
//           USL; even laps no tag -- base limits)
//        -> inspection (INSP_MODE) via the REAL menu road (mode tag + play,
//           with the camera-modal / drawer / SPLASH-bounce traps handled)
//        -> assert the NG range actually in force matches this lap's tag
//        -> EXIT -> assert the editor def is RESTORED (usl + the component's
//           own "[insp-exit] tag-limit overrides restored" log when a tag was
//           applied) and the def hash never drifts across laps.
//
// What this catches that single-pass flows cannot: state leaks that need
// repetition -- a restore that works once but double-applies on lap 2, a mode
// guard that wedges after editor->insp->editor, modal/drawer zombies, hash
// drift from repeated load/apply/restore, SM dead ends.
//
//   node cycle.mjs [laps=3]
//
// Needs webctld (8765), vite (8081), core (4090). Non-destructive: the
// fixture recipe is installed in the STORE only (no SV), machine files
// untouched.
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const LAPS = parseInt(process.argv[2] || '3', 10);
const TAGGED_MODEL = path.join(__dirname, 'fixtures', 'caliper_verify_tagged');

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
const state = () => ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);

async function toMain(maxMs = 40000) {
  const t0 = Date.now();
  let splashN = 0;
  while (Date.now() - t0 < maxMs) {
    const st = await state();
    if (st === '"MAIN"') return;
    // Dispatch EXIT atomically WITH the state check, in-page: reading the
    // state here and dispatching in a second round-trip raced the app's own
    // exit -- our EXIT then landed on MAIN, and MAIN+EXIT -> SPLASH, which
    // only leaves on REMOTE_SYSTEM_READY (an HR, i.e. a reconnect): a dead
    // end with the WS still up. Cost one lap of cycle.mjs to find.
      const v = await ev(`(function(){var v=JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value);
        if(v.indexOf('INSP_MODE')>=0||v.indexOf('DEFCONF_MODE')>=0||v.indexOf('INSTINSP_MODE')>=0)
          window.__GP_STORE__.dispatch({type:'EXIT'});
        return v;})()`);
    // SPLASH with the WS still connected is a dead end (only REMOTE_SYSTEM_READY
    // -- an HR from a reconnect -- leaves it). Force a reconnect. Use the FRESH
    // `v`, not the stale pre-dispatch `st`; the reconnect is scheduled at +10s
    // (BPG_WS.js), so kick at most once per >10s and never while the socket is
    // already CONNECTING (a kick there aborts the dial and reschedules +10s).
    if (v === '"SPLASH"') {
      splashN++;
      const rs = await ev(`(window.__GP_WS__.inst.websocket||{}).readyState`);
      if (splashN >= 12 && rs !== 0) {   // 0 = CONNECTING
        splashN = 0;
        await ev(`try{window.__GP_WS__.inst.websocket.close()}catch(e){}; 'kick'`);
      }
    } else splashN = 0;
    await sleep(1000);
  }
  throw new Error('could not reach MAIN (state=' + await state() + ')');
}

// antd keeps closed modals in the DOM -- visibility is rect height (project note).
async function dismissBlockers() {
  for (let i = 0; i < 20; i++) {
    const open = await ev(
      `(function(){var ws=[...document.querySelectorAll('.ant-modal-wrap')];return ws.some(function(w){var r=w.getBoundingClientRect();return r.height>50&&getComputedStyle(w).display!=='none';});})()`
    );
    if (!open) break;
    if (i === 8) await api('/click', { selector: `text=跳過相機連線` }).catch(() => {});
    await sleep(1000);
  }
  const hasDrawer = await ev(
    `(function(){var e=document.querySelector('.ant-drawer-close');if(!e)return 'no';var r=e.getBoundingClientRect();return r.height>0?'yes':'no';})()`
  );
  if (hasDrawer === 'yes') { await api('/click', { selector: '.ant-drawer-close' }); await sleep(1500); }
}

async function loadFixtureIntoStore() {
  await ev(
    `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(TAGGED_MODEL)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
  );
  for (let i = 0; i < 80; i++) {
    await sleep(100);
    if (await ev('!!window.__rdy || !!window.__rdyErr')) break;
  }
  const err = await ev('window.__rdyErr||null');
  if (err) throw new Error('fixture load failed: ' + err);
}

// The editor's inst-check, by the same action the 檢測 button dispatches (EX
// snap exam). Resolves when edit_info.inspReport carries a report.
async function editorInstCheck() {
  // Clear sig360info FIRST. Loading the def seeds sig360info from its embedded
  // signature, so a post-EX `reports.length>0` check proved nothing -- it
  // passed on the def seed even if EX silently downgraded/failed. Null it, then
  // require EX to repopulate it: now the assertion actually tests EX.
  await ev(`window.__GP_STORE__.dispatch({type:'SIG360_Report_Update',data:{reports:[]}}); 'cleared'`);
  await sleep(200);
  const cleared = await ev(`(function(){var si=window.__GP_STORE__.getState().UIData.edit_info.sig360info;return (si&&si.reports&&si.reports.length)||0;})()`);
  if (cleared !== 0) throw new Error('could not clear sig360info before inst-check (got ' + cleared + ')');
  await ev(`(function(){
    var S=window.__GP_STORE__;var CORE=S.getState().ConnInfo.CORE_ID;
    window.__exDone=false;window.__exErr=null;
    S.dispatch({type:'DefConf_Lock_Level_Update',data:0});
    S.dispatch({type:'MW_API_CALL',id:CORE,method:'send',param:{tl:'EX',prop:0,
      data:{trigger_type:0,timeout:-1,img_property:{down_samp_level:2}},
      promiseCBs:{
        resolve:function(pkts){
          try{
            var SSp=pkts.find(function(p){return p.type=='SS'});
            if(!SSp || SSp.data.ACK!==true){window.__exErr='no-ACK';window.__exDone=true;return;}
            var acts=pkts.map(function(p){var a=window.__GP_BPG__.map_BPG_Packet2Act(p);if(a)a.IGNORE_DEFCONF_LOCK=true;return a;}).filter(Boolean);
            S.dispatch({type:'ATBundle',ActionThrottle_type:'express',data:acts});
            window.__exDone=true;
          }catch(e){window.__exErr=String(e);window.__exDone=true;}
        },
        reject:function(e){window.__exErr='reject';window.__exDone=true;}
      }}});
    return 'ex-sent';
  })()`);
  for (let i = 0; i < 100; i++) {
    await sleep(100);
    if (await ev('!!window.__exDone')) break;
  }
  const err = await ev('window.__exErr||null');
  if (err) throw new Error('inst-check failed: ' + err);
  // the report must actually LAND. EX replies SG (sig360 report) + IM; the
  // SG act (SIG360_Report_Update, with IGNORE_DEFCONF_LOCK like the editor's
  // own path) lands in edit_info.sig360info.
  for (let i = 0; i < 30; i++) {
    const n = await ev(`(function(){var r=window.__GP_STORE__.getState().UIData.edit_info.sig360info;return (r&&r.reports&&r.reports.length)||0;})()`);
    if (n > 0) return n;
    await sleep(200);
  }
  throw new Error('inst-check ACKed but no sig360info report landed');
}

async function clickModeTagAndPlay() {
  let modeIdx = -1;
  for (let tryN = 0; tryN < 4 && modeIdx < 0; tryN++) {
    modeIdx = await ev(
      `(function(){var t=[...document.querySelectorAll('span.ant-tag-has-color')].filter(function(e){return e.textContent.trim()==='測試'});return t.length?t.length-1:-1;})()`
    );
    if (modeIdx < 0) { await toMain(); await api('/click', { selector: `text=主選單` }).catch(() => {}); await sleep(2000); }
  }
  if (modeIdx < 0) throw new Error('no 測試 mode tag on MAIN');
  await api('/click', { selector: `span.ant-tag-has-color:text-is('測試') >> nth=${modeIdx}` });
  await sleep(2500);
  const playIdx = await ev(
    `(function(){var all=[...document.querySelectorAll('button.ant-btn')];var best=-1,bw=0;all.forEach(function(e,i){var r=e.getBoundingClientRect();if(r.top>innerHeight*0.8&&r.left>innerWidth*0.8&&r.width>bw){bw=r.width;best=i}});return best;})()`
  );
  if (playIdx < 0) throw new Error('play button not found');
  await api('/click', { selector: `button.ant-btn >> nth=${playIdx}` });
  for (let i = 0; i < 30; i++) {
    await sleep(1000);
    if ((await state()).includes('INSP_MODE')) return;
  }
  throw new Error('never entered INSP_MODE');
}

// ---------------------------------------------------------------------------
console.log(`mode round-trip x${LAPS} laps`);
await api('/reload', {});
for (let i = 0; i < 60; i++) { await sleep(150); if ((await ev('typeof window.__GP_STORE__')) === 'object') break; }
{ // settle the startup auto-load (see flows.reset)
  let prev = null;
  for (let i = 0; i < 12; i++) {
    await sleep(1000);
    const nm = await ev(`(((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile)||{}).name||null`);
    if (nm !== null && nm === prev) break;
    prev = nm;
  }
}
await toMain();

let hash0 = null;
let fails = 0;
for (let lap = 1; lap <= LAPS; lap++) {
  const tag = (lap % 2 === 1) ? ['TAGX'] : [];
  const t0 = Date.now();
  try {
    // -- editor + inst-check ---------------------------------------------
    await toMain(); await dismissBlockers();
    await ev(`window.__GP_STORE__.dispatch({type:'Edit_Mode'}); 'e'`);
    await sleep(800);
    if (!(await state()).includes('DEFCONF_MODE')) throw new Error('did not enter DEFCONF_MODE');
    await loadFixtureIntoStore();
    await sleep(500);
    const nRep = await editorInstCheck();
    await ev(`window.__GP_STORE__.dispatch({type:'EXIT'}); 'x'`);
    await toMain();

    // -- recipe with this lap's NG-range setup ---------------------------
    // Landing on MAIN can trigger the machine-def auto-reload ~1.5s later
    // (InspectionMode:FI). Loading the recipe before that lands means the
    // reload CLOBBERS it -- settle first, install, verify, retry once.
    let before = null;
    for (let attempt = 0; attempt < 3; attempt++) {
      { let prev = null;
        for (let i = 0; i < 8; i++) {
          await sleep(1000);
          const nm = await ev(`(((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile)||{}).name||null`);
          if (nm !== null && nm === prev) break;
          prev = nm;
        } }
      await loadFixtureIntoStore();
      await ev(`window.__GP_STORE__.dispatch({type:'Def_Model_Path_Update',data:${JSON.stringify(TAGGED_MODEL)}});window.__GP_STORE__.dispatch({type:'InspOptionalTag_Update',data:${JSON.stringify(tag)}}); 'recipe'`);
      await sleep(800);
      before = await ev(
        `(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {def:(s.edit_info.loadedDefFile||{}).name||null, usl:m?m.USL:null, hash:s.edit_info.DefFileHash||null};})()`
      );
      if (before.def === 'caliper_verify_tagged') break;
    }
    if (before.def !== 'caliper_verify_tagged') throw new Error('recipe did not take (def=' + before.def + ')');
    if (hash0 === null) hash0 = before.hash;
    else if (before.hash !== hash0) throw new Error(`def hash drifted: ${hash0} -> ${before.hash}`);

    // Within-lap round-trip integrity: snapshot id-8's FULL limit set (not
    // just USL) right before entering inspection. After restore we require a
    // byte-identical snapshot -- this is the real "no state leaked across the
    // editor->insp->editor round-trip" test the header advertises, and it
    // catches a leak in ANY limit field (USL, LSL, ...), not only USL.
    const shapeSnap = (label) => ev(
      `(function(){var m=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.find(x=>x.id===8);return m?JSON.stringify({USL:m.USL,LSL:m.LSL,UCL:m.UCL,LCL:m.LCL}):null;})()`
    );
    const beforeShape = await shapeSnap();

    // -- inspection ------------------------------------------------------
    await toMain(); await dismissBlockers();
    await clickModeTagAndPlay();
    await sleep(2000);   // APP_INSP_MODE mount (tag apply + FI/CI send)
    const during = await ev(
      `(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {usl:(m&&m.USL!==undefined)?m.USL:null};})()`
    );
    // Assert the EXACT NG range in force, not merely "changed": TAGX overrides
    // id-8 USL to 16.47; no tag keeps the base 8.7. A loose "!== before"
    // passed for a wrong value (stale/LSL/garbage) -- 16.47 pins it.
    const TAG_USL = 16.47, BASE_USL = 8.7;
    const expectUsl = tag.length ? TAG_USL : BASE_USL;
    if (during.usl !== expectUsl)
      throw new Error(`NG-range wrong in insp: tag=${JSON.stringify(tag)} expected USL ${expectUsl}, got ${during.usl} (before ${before.usl})`);

    // -- exit + restore ---------------------------------------------------
    const logMark = Date.now();
    await ev(`window.__GP_STORE__.dispatch({type:'EXIT'}); 'out'`);
    let after = { usl: null }, afterShape = null;
    for (let i = 0; i < 25; i++) {
      await sleep(100);
      after = await ev(`(function(){var s=window.__GP_STORE__.getState().UIData;var m=s.edit_info._obj.shapeList.find(x=>x.id===8);return {usl:(m&&m.USL!==undefined)?m.USL:null};})()`);
      afterShape = await shapeSnap();
      if (afterShape === beforeShape) break;
    }
    if (afterShape !== beforeShape)
      throw new Error(`round-trip leak: id-8 limits ${beforeShape} -> ${afterShape}`);
    if (tag.length) {   // a tag was applied -> the component MUST log its restore
      const logs = await api('/logs').catch(() => null);
      const arr = (logs && (logs.logs || logs)) || [];
      const restored = arr.some(l => l.t >= logMark && String(l.text || '').includes('[insp-exit] tag-limit overrides restored'));
      if (!restored) throw new Error('restore log line missing (restore may not have run)');
    }
    await toMain();
    console.log(`lap ${lap}/${LAPS} PASS  tag=${JSON.stringify(tag)} instRep=${nRep} usl ${before.usl}->${during.usl}->${after.usl}  (${((Date.now() - t0) / 1000).toFixed(0)}s)`);
  } catch (e) {
    fails++;
    console.error(`lap ${lap}/${LAPS} FAIL: ${e.message}`);
    await api('/shot?' + new URLSearchParams({ path: path.join(__dirname, `cycle_fail_lap${lap}.png`) }).toString()).catch(() => {});
    await toMain().catch(() => {});
  }
}
const finalState = await state();
console.log(`RESULT: ${LAPS - fails}/${LAPS} laps clean, final state=${finalState}, def-hash stable=${hash0 !== null}`);
process.exit(fails === 0 && finalState === '"MAIN"' ? 0 : 1);
