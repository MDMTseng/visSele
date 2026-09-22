// Does the app enter the inspection flow on the REAL machine?
//
//   node check_real_machine.mjs [recipe]
//
// Same path an operator takes: MAIN -> load a recipe -> 檢測方式 -> play, then
// confirm the machine is actually producing -- frames on the wire and reports
// in the store, not just a screen that opened.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { makeProbe, makeTally, sleep } from './_rf_lib.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { api, ev } = ctl;
const P = makeProbe(ev); const T = makeTally(); const { ok, section } = T;
const SM = 'window.__GP_STORE__.getState().UIData.c_state.value';
const MODEL = process.argv[2] || 'data/testNew2';

section('the app reaches MAIN with the real core');
await freshPage(ctl, 'http://127.0.0.1:8081/');
await toMain(ctl);
ok('cleared any camera-reconnect modal', await dismissCamModal(ctl));
ok('the socket is up', (await ev(`(window.__GP_WS__.inst.websocket||{}).readyState`)) === 1);
console.log('   connection info:', String(await ev(
  `JSON.stringify(window.__GP_STORE__.getState().ConnectionInfo||{}).slice(0,240)`)));

section('a recipe loads');
const name = await loadRecipe(ctl, MODEL);
ok('the def loads', !!name, `name=${JSON.stringify(name)}`);
ok('no integrity error', !(await P.store(`window.__GP_STORE__.getState().UIData.edit_info.defIntegrityError`)));

section('the inspection screen opens');
let entered = null;
try { entered = await enterInspection(ctl, { mode: '測試', log: (m) => console.log('   ' + m) }); }
catch (e) { console.log('   ' + e.message); }
ok('play reaches INSP_MODE', !!entered, String(entered));

section('the machine actually produces');
if (entered) {
  // A REAL trigger, without moving anything -- AND WHAT THAT CANNOT PROVE.
  //
  // The plate is what makes parts arrive, and starting it moves hardware -- not
  // something a check should do on its own. trig_phantom_pulse is the part
  // SIGNAL only, so it looked like the half worth testing here.
  //
  // It is not, and the firmware says so at phantomEmitOne():
  //
  //     Ask; do not emit. [...] The pulse lands on the next timer tick.
  //     With the timer alarm off (PLATE_FREQ_CURRENT==0) the request simply
  //     waits. Nothing is lost: a phantom's stage tasks are scheduled at
  //     future step counts, so on a stationary plate it could never have
  //     reached its camera either.
  //
  // So on a stopped plate every request queues and nothing is ever inspected.
  // Measured 2026-08-31: 14 signals sent, 0 reports, and `ph_pend` standing at
  // 33 with every gate counter -- accept, rej_rate, rej_busy, rej_blocked --
  // unmoved, because newPulseEvent was never reached.
  //
  // This check asserted reports anyway. That is a check that can only fail, and
  // it had been passing as SKIP purely because the console it needs was usually
  // closed -- so opening the console did not reveal a machine fault, it
  // revealed the check. A suite whose red light means "the plate is stopped"
  // teaches people to ignore it, which is worse than not having the assertion.
  //
  // So: assert what a stopped plate CAN establish -- that the board accepted
  // the signal and is holding it -- and let 快速驗證 below carry the real
  // question (does the camera produce frames and the engine produce verdicts),
  // which it already answers without moving anything.
  const before = await ev(`(function(){try{return window.__DIAG__().msgHz;}catch(e){return -1;}})()`);
  console.log('   msgHz before any trigger:', before);
  await ev(`window.__RC__ = 0; (function(){ var s = window.__GP_STORE__;
    if (s.__rc) return; s.__rc = 1; var d = s.dispatch.bind(s);
    s.dispatch = function(a){ if (a && /Report|RP|Insp|Statistic/i.test(String(a.type))) window.__RC__++; return d(a); }; })()`);
  const { default: net } = await import('node:net');
  let phantomSent = false;
  await new Promise((res) => {
    const sock = net.connect(4099, '127.0.0.1');
    let n = 0;
    sock.on('connect', () => {
      const LINE = JSON.stringify({ type: 'trig_phantom_pulse' }) + String.fromCharCode(10);
      const iv = setInterval(() => { sock.write(LINE); n++; }, 400);
      setTimeout(() => { clearInterval(iv); phantomSent = n > 0;
        console.log(`   sent ${n} phantom part signals`); sock.end(); res(); }, 6000);
    });
    sock.on('error', (e) => { console.log('   perif console 4099: ' + e.message
      + ' -- no phantom triggers available (that console is not open)'); res(); });
  });
  await sleep(1500);
  // The board's own account of what happened to those requests.
  const pend = await new Promise((res) => {
    const sock = net.connect(4099, '127.0.0.1');
    let buf = '';
    sock.on('connect', () => sock.write(JSON.stringify({ type: 'poll' }) + String.fromCharCode(10)));
    sock.on('data', (d) => { buf += d.toString();
      const m = /"ph_pend"\s*:\s*(\d+)/.exec(buf);
      if (m) { sock.end(); res(Number(m[1])); } });
    sock.on('error', () => res(null));
    setTimeout(() => { try { sock.end(); } catch (e) {} res(null); }, 3000);
  });
  const n = await ev(`window.__RC__||0`);
  const after = await ev(`(function(){try{var d=window.__DIAG__();return d.msgHz+' Hz, '+d.imgKBps+' KB/s';}catch(e){return '?';}})()`);
  console.log('   wire after triggers:', after);
  // SKIPPED, not failed, when there is no console to trigger through: 4099 is
  // the peripheral console, and without it the only part signal is the real
  // plate -- which moves hardware and is not this check's business.
  //
  // Two outcomes are both correct here, and which one applies is decided by the
  // plate, not by the machine's health:
  //   plate turning -> the requests drain and reports arrive.
  //   plate stopped -> they queue, ph_pend rises, and no report is possible.
  // Asserting the first unconditionally is what made this leg lie.
  if (phantomSent && pend !== null && pend > 0 && n === 0) {
    console.log(`   plate is stopped -- ${pend} phantom signals queued (ph_pend),`
              + ' waiting for a plate tick. Reports need the plate turning;'
              + ' 快速驗證 below covers camera+engine without it.');
    ok('the board accepted the part signals and is holding them', pend > 0,
       `ph_pend=${pend}`);
  } else {
    ok('inspection reports arrive when a part is signalled',
       phantomSent ? (n > 0) : null, `report actions=${n}, ph_pend=${pend}`);
  }
  ok('the canvas is drawing', await ev(
    `(function(){var c=document.querySelector('canvas'); if(!c) return false;
       var r=c.getBoundingClientRect(); return r.width>100 && r.height>100;})()`));
  const st = JSON.stringify(await P.store(SM));
  ok('still in the inspection screen', st.indexOf('INSP_MODE') >= 0, st);
}

// ---------------------------------------------------------------------------
// 快速驗證 IS THE PART THAT NEEDS NO PLATE.
//
// FI waits for a part signal, so with the plate stopped an idle inspection
// screen proves only that the screen opened. The quick check runs CI against
// the live camera on demand -- same core path, same def, no hardware motion --
// so it is the strongest end-to-end statement available without starting the
// machine.
section('快速驗證 against the live camera (no plate needed)');
{
  await toMain(ctl);
  await loadRecipe(ctl, MODEL);
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  const inEditor = await P.waitFor('DEFCONF', async () =>
    JSON.stringify(await P.store(SM)).indexOf('DEFCONF_MODE') >= 0, { timeout: 25000 });
  ok('the def editor opens', inEditor);
  if (inEditor) {
    ok('快速驗證 opens', await P.click('quick-verify'));
    const ciThere = await P.waitExists('quick-verify-ci', { timeout: 8000 });
    ok('the mode choice appears', ciThere);
    if (ciThere) {
      await P.click('quick-verify-ci');
      ok('the inspection view opens', await P.waitFor('report view', () => ev(
        `!!document.querySelector('.ant-modal-body canvas')`), { timeout: 20000 }));
      ok('frames arrive from the real camera', await P.waitFor('frames', async () => {
        const hz = await ev(`(function(){try{return window.__DIAG__().msgHz;}catch(e){return 0;}})()`);
        return Number(hz) > 0.5;
      }, { timeout: 15000 }), `msgHz=${await ev(`(function(){try{return window.__DIAG__().msgHz;}catch(e){return -1;}})()`)}`);
      ok('it produces inspection reports', await P.waitFor('a report', () => ev(
        `(function(){var s=window.__GP_STORE__.getState().UIData;
           return !!(s.edit_info && s.edit_info.inspReport); })()`), { timeout: 20000 }));
      const rep = await ev(`(function(){try{
        var r=window.__GP_STORE__.getState().UIData.edit_info.inspReport;
        return JSON.stringify({ status: r.status, objs: (r.reports||[]).length }); }catch(e){ return '?'; }})()`);
      console.log('   last report:', rep);
      if (process.env.CHECK_SHOT) {
        const u = 'http://127.0.0.1:8765/shot?path=' + encodeURIComponent(process.env.CHECK_SHOT);
        console.log('   shot ->', await (await fetch(u)).text());
      }
    }
  }
}

await toMain(ctl).catch(() => {});
process.exit(T.done() ? 1 : 0);
