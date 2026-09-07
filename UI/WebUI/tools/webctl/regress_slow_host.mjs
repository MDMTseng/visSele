// What does the machine do when the INSPECTION is too slow for the deadline?
//
//   node regress_slow_host.mjs [recipe]
//
// The target computer is weaker than this bench, so "it keeps up here" says
// nothing about it. Every other knob can be chosen by reading a number; this
// one cannot be tested by choosing anything, because it is about what the
// machine does once the number is already bad -- and that is the whole of the
// project's stated priority order:
//
//     不可檢錯  >  best effort  >  盡量不停機
//     有疑問、不確定就 NA,讓料回流。
//
// So the delay is injected on purpose (insp_debug_delay, core-side, after the
// match and before the verdict -- the shape a slow host actually has) and the
// machine is asked, at each point, which of those three it is doing.
//
// THIS RUNS THE MACHINE FOR REAL: the plate turns and READY turns the feeder
// on. An earlier version held the plate with set_dry_run and fed virt_pulse
// objects instead, which is safer but measured nothing:
//
//   全檢 + enter_insp_mode   -> 243 objects in, 244 NA verdicts, 0 unanswered
//   測試 + enter_insp_test_mode -> 183 objects in, 0 verdicts, 183 unanswered
//
// The whole sweep had been run in 測試, which does not drive the inspection for
// board-triggered objects at all -- so every row read "the host answered
// nothing" for a machine that was never asked. And entering READY turns the
// feeder on regardless, so holding the plate only means feeding a stationary
// one. Real parts, real plate, and the delay is the only thing being varied.
//
// THE EDGE IS THROUGHPUT, NOT THE DEADLINE. Parts arrive around 21/s, i.e.
// every 48ms; the deadline is ~594ms at plate 10000. A serialized 99ms delay
// is comfortably inside the deadline and still breaks the machine, because it
// halves the service rate against an unchanged arrival rate and the queue grows
// without bound. Measured on the first working run: delay 0 -> 185 NA and 0
// unanswered; delay 99 -> 5 NA and 179 unanswered. So the points below bracket
// the ARRIVAL INTERVAL, not the deadline.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { makeTally, sleep } from './_rf_lib.mjs';
import net from 'node:net';

const ctl = makeCtl('http://127.0.0.1:8765');
const T = makeTally(); const { ok, section } = T;
const MODEL = process.argv[2] || 'data/testNew2';
// Their production speed. A faster plate would shorten the deadline and make
// the sweep quicker, but the plate speed is a hardware decision and not this
// test's to take.
const PLATE_FREQ = 10000;

// ---- the peripheral console: one connection per request, as the rig does ----
function perif(cmd, want, waitMs = 3000) {
  return new Promise((res) => {
    const sock = net.connect(4099, '127.0.0.1');
    let buf = '';
    const done = (v) => { try { sock.end(); } catch (e) {} res(v); };
    sock.on('connect', () => sock.write(JSON.stringify(cmd) + String.fromCharCode(10)));
    sock.on('data', (d) => {
      buf += d.toString();
      for (const line of buf.split(String.fromCharCode(10))) {
        const t = line.trim();
        if (!t.startsWith('{') || (want && t.indexOf(want) < 0)) continue;
        try { return done(JSON.parse(t)); } catch (e) { /* still partial */ }
      }
    });
    sock.on('error', () => done(null));
    setTimeout(() => done(null), waitMs);
  });
}
const stat  = () => perif({ type: 'get_running_stat' }, '"gate"');
const setup = () => perif({ type: 'get_setup' }, '"gate"');
// THE DELAY GOES TO THE CORE, NOT TO THE BOARD.
//
// The first version sent it to 4099 -- which is the PERIPHERAL console: it
// forwards to the board, and the board answered `unknown_type`. So the whole
// sweep injected nothing and every row of the table was identical for a reason
// that had nothing to do with the machine. insp_debug_delay is a core command
// and the core is reached over the WS the page already holds.
async function core(cmd) {
  const { ev } = ctl;
  const js = JSON.stringify(cmd);
  await ev(`(function(){
    window.__DLYR__ = 'pending';
    var st = window.__GP_STORE__;
    var CORE_ID = st.getState().ConnInfo.CORE_ID;
    st.dispatch({ type:'MW_API_CALL', id: CORE_ID, method:'send',
      param: { tl:'SC', prop:0, data: ${js},
        promiseCBs: {
          resolve: function(pkts){
            var p = (pkts||[]).find(function(q){ return q && q.data
                    && q.data.type === ${JSON.stringify(cmd.type)}; });
            window.__DLYR__ = p ? JSON.stringify(p.data) : 'no reply packet';
          },
          reject: function(e){ window.__DLYR__ = 'REJECT ' + String(e); } } } });
    return 'sent';})()`);
  // Generous, because the reply shares a core with an inspection thread that
  // this test is deliberately making slow: under a saturated frame queue an
  // ack took longer than 6s once, and treating that as a broken command path
  // aborted a run whose delay had in fact been applied.
  for (let i = 0; i < 50; i++) {
    const r = await ev(`window.__DLYR__`);
    if (r !== 'pending') {
      // A core too old to know the command must not be mistaken for a machine
      // that shrugged off the delay: that reading is the whole result.
      if (String(r).indexOf('no reply packet') >= 0)
        throw new Error('this core has no insp_debug_delay -- nothing would be injected');
      return r;
    }
    await sleep(300);
  }
  return null;   // unknown, not fatal -- the caller decides
}

// The plate ramps -- at accel 2000, 0<->30000 is about fifteen seconds. Both
// ends of this test have to wait for it: objects do not flow until the stage
// clock is at speed, and the machine will not leave inspection cleanly while
// it is still ramping. The first version slept 800ms for a ramp that takes
// ~15s at accel 2000.
async function waitFreq(target, budgetMs) {
  const t0 = Date.now();
  while (Date.now() - t0 < budgetMs) {
    const s = await stat();
    const f = s && (s.plate_freq_current !== undefined ? s.plate_freq_current
                                                       : (s.plate || {}).freq_current);
    if (f !== undefined && Math.abs(f - target) <= Math.max(50, target * 0.02)) return f;
    if (target === 0 && (f === 0 || f === undefined)) return 0;
    await sleep(700);
  }
  return null;
}

// Everything this test is allowed to judge by, in one shape.
function snap(s) {
  const g = (s && s.gate) || {}, c = (s && s.count) || {},
        h = (s && s.health) || {}, l = (s && s.report_latency) || {};
  return {
    state: s && s.state, err: s && s.error,
    accept: g.accept | 0, rej_rate: g.rej_rate | 0, rej_load: g.rej_load | 0,
    proc_avg_us: g.proc_avg_us | 0,
    NA: c.NA | 0, SKIP: c.SKIP | 0, UNANS: c.UNANSWERED | 0,
    SEL1: c.SEL1 | 0, SEL3: c.SEL3 | 0,
    // JUDGED, UNDER A HELD PLATE, IS NOT SEL*_Count.
    //
    // SEL1_Count is incremented by the ACT_SEL handler, and that handler is
    // gated on sel_ok = PLATE_RUNNING && !SYS_STEPPER_DISABLED && !DRY_RUN. So
    // whenever it is false every judged part takes the other arm --
    // `if(!sel_ok) SEL_SUPPRESSED_N++` -- and the bin counts stay at zero no
    // matter how well the machine is judging. Counted here so a run that
    // cannot actuate is still readable; on a turning plate it stays 0.
    SUPPRESSED: c.SEL_SUPPRESSED | 0,
    consec: h.consec_unanswered | 0, nomatch: c.NOMATCH_CONSEC | 0,
    cam_avg_ms: l.cam_avg_us ? Math.round(l.cam_avg_us / 1000) : 0,
    cam_max_ms: l.cam_max_us ? Math.round(l.cam_max_us / 1000) : 0,
  };
}
const diff = (a, b) => {
  const o = {};
  for (const k of Object.keys(b)) if (typeof b[k] === 'number') o[k] = b[k] - (a[k] | 0);
  return o;
};

// ---- put the machine back, whatever happened -------------------------------
let ORIG = null;
async function restore() {
  // A DELAY LEFT ON IS THE ONE THING THIS MUST NOT WALK AWAY FROM: the machine
  // would look broken later with nothing on it to say why. So this retries and
  // insists on seeing ms:0 come back, rather than firing once and hoping.
  let cleared = false;
  for (let i = 0; i < 5 && !cleared; i++) {
    const r = await core({ type: 'insp_debug_delay', ms: 0 }).catch(() => null);
    cleared = !!r && String(r).indexOf('"ms":0') >= 0;
  }
  console.log(cleared ? '   delay cleared (confirmed ms:0)'
                      : '   *** COULD NOT CONFIRM THE DELAY IS OFF -- check the core ***');
  await perif({ type: 'set_gate_disable', on: false }, '"type"', 2000);
  // Retried and checked: a single exit_insp_mode has been observed not to take,
  // and setting the plate to 0 is refused while the machine is still in
  // inspection -- which is how a run ends with the plate still turning.
  for (let i = 0; i < 5; i++) {
    await perif({ type: 'exit_insp_mode' }, '"ack"', 2000);
    await sleep(1000);
    const s = await stat();
    if (s && s.state === 100) break;
    if (i === 4) console.log('   *** still not in IDLE after 5 exit attempts ***');
  }
  await perif({ type: 'set_setup', plate: { freq: 0 } }, '"ack"', 2000);
  await waitFreq(0, 25000);
  if (ORIG) {
    await perif({ type: 'set_setup', skip_policy: ORIG.skip_policy,
                  gate: { proc_sep_us: ORIG.proc_sep_us } }, '"ack"', 2000);
  }
  console.log('   restored: delay off, gate open, plate stopped, policy + proc_sep_us back');
}
process.on('SIGINT', async () => { await restore(); process.exit(1); });

// EACH POINT STARTS FROM A DRAINED MACHINE.
//
// Without this the points are not independent and none of them means anything.
// A delay past the service rate makes the queue grow without bound, so the
// backlog from one row is still being worked off during the next -- measured:
// a 178ms row left the core answering nothing at all, and the row after it
// reported "delay=0, 183 unanswered" for a machine whose only problem was the
// previous row. CONSEC_UNANSWERED carried across in the same way, starting a
// run at 1621.
//
// So: stop the feed, wait for the machine to go quiet, change the delay, start
// the feed again. Quiet is defined by the counters standing still rather than
// by a fixed sleep, because how long it takes is exactly what varies.
async function drain(label) {
  // Shut the gate rather than stop a train: the traffic here is real parts from
  // the feeder. A part refused at the gate is not lost, it rides round again --
  // the same property every rate/distance rejection relies on.
  await perif({ type: 'set_gate_disable', on: true }, '"type"', 2000);
  let prev = null, still = 0;
  for (let i = 0; i < 40; i++) {
    await sleep(500);
    const b = snap(await stat());
    const key = `${b.accept}/${b.NA}/${b.UNANS}/${b.SKIP}/${b.SUPPRESSED}`;
    if (key === prev) { if (++still >= 4) return true; } else { still = 0; prev = key; }
  }
  console.log(`   NOTE: ${label} did not go quiet in 20s -- this row starts dirty`);
  return false;
}

// ---- one delay point -------------------------------------------------------
// A POINT THAT MEASURES A STOPPED MACHINE IS NOT A MEASUREMENT.
//
// Any point past the service rate can halt the line -- that is what the sweep
// is for -- and the NEXT point then starts on a machine at state 112, admits
// nothing, and reports zeroes that look like "the delay had no effect".
// Measured: the 45ms point halted the line, and the mode=none section after it
// recorded 進料 0 for a delay that was never given anything to slow down.
//
// So every point puts the machine back in READY first, and says when it had to.
async function ensureReady(label) {
  let st = await stat();
  if (st && st.state === 101) return true;
  console.log(`   ${label}: machine is at state ${st && st.state} -- clearing and re-entering`);
  await perif({ type: 'clear_error' }, '"ack"', 2000);
  await perif({ type: 'exit_insp_mode' }, '"ack"', 2000);
  await sleep(1000);
  await perif({ type: 'set_setup', plate: { freq: PLATE_FREQ } }, '"ack"', 2000);
  await perif({ type: 'enter_insp_mode' }, '"type"', 3000);
  for (let i = 0; i < 60; i++) {
    st = await stat();
    if (st && st.state === 101) return true;
    await sleep(1000);
  }
  console.log(`   ${label}: could not get back to READY (state=${st && st.state})`);
  return false;
}

async function point(label, delayMs, holdMs = 9000) {
  await ensureReady(label);
  await drain(label);
  const ack = await core({ type: 'insp_debug_delay', ms: delayMs,
                          jitter_ms: Math.round(delayMs * 0.15) });
  // Echoed back, not assumed: the core clamps (0..10000) and the value it is
  // actually using is the one this row is about. An unconfirmed setting is
  // SAID, not swallowed -- a row measured against an unknown delay is a row
  // nobody can use.
  if (ack === null) console.log(`   NOTE: no ack for ${delayMs}ms -- this row's delay is unconfirmed`);
  else if (delayMs > 0 && String(ack).indexOf(`"ms":${delayMs}`) < 0)
    console.log(`   NOTE: core reports ${ack} for a requested ${delayMs}ms`);
  await perif({ type: 'set_gate_disable', on: false }, '"type"', 2000);
  await sleep(1500);                       // let the first objects reach CAM1
  const a = snap(await stat());
  await sleep(holdMs);
  const b = snap(await stat());
  const d = diff(a, b);
  // NA IS A VERDICT. It is the machine saying "I looked and I cannot tell",
  // which is judged -- it reaches case 0xFFFF, resets CONSEC_UNANSWERED and
  // increments NA_Count. Unjudged is UNSET/SKIP: nobody answered at all. On a
  // held plate with injected objects every verdict is NA (there is no part in
  // front of the camera), so leaving it out of `judged` made a machine that was
  // answering every single object look like one answering none.
  const judged = d.SEL1 + d.SEL3 + d.SUPPRESSED + d.NA;
  const p = (n, w) => String(n).padStart(w);
  console.log(`   ${label.padEnd(20)} delay=${p(delayMs, 4)}ms  `
    + `進料 ${p(d.accept, 3)}  判定 ${p(judged, 3)}  NA ${p(d.NA, 3)}  `
    + `SKIP ${p(d.SKIP, 3)}  無判決 ${p(d.UNANS, 3)}  擋下 ${p(d.rej_load, 3)}  `
    + `連續 ${b.consec}  回報 ${b.cam_avg_ms}/${b.cam_max_ms}ms  state=${b.state}`);
  return { a, b, d, judged };
}

try {
  section('the bench is set up to be starved, without moving anything');
  const s0 = await setup();
  ok('the board answers get_setup', !!s0);
  if (!s0) throw new Error('no board on the peripheral console');
  ORIG = { skip_policy: s0.skip_policy, proc_sep_us: (s0.gate || {}).proc_sep_us };
  console.log('   original:', JSON.stringify(ORIG));

  const spo = s0.stage_pulse_offset || {};
  const budgetMs = (spo.SWITCH - spo.CAM1_on) / (2 * PLATE_FREQ) * 1000;
  console.log(`   deadline at plate_freq ${PLATE_FREQ}: `
    + `(${spo.SWITCH}-${spo.CAM1_on}) ticks = ${budgetMs.toFixed(0)} ms`);
  ok('the deadline is short enough to sweep', budgetMs > 50 && budgetMs < 2000,
     `${budgetMs.toFixed(0)} ms`);

  await freshPage(ctl, 'http://127.0.0.1:8081/');
  await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  const entered = await enterInspection(ctl, { mode: '全檢', log: (m) => console.log('   ' + m) });
  ok('the app is in the inspection screen', !!entered, String(entered));

  // THE APP'S PLAY BUTTON DOES NOT PUT THE BOARD IN INSPECTION MODE.
  //
  // Measured: the app reports INSP_MODE and the board sits at state 100 (IDLE)
  // the whole time -- accept, NA, SKIP, UNANSWERED, every counter flat, because
  // no object was ever registered. The screen and the machine are two different
  // state machines and only one of them is what this test is about.
  //
  // The first version of this file trusted the screen and reported six clean
  // zeroes as six failures. It was measuring nothing.
  // THE SETPOINT COMES FIRST. enter_insp_mode runs CAL -> SPINUP -> READY, and
  // SPINUP compares PLATE_FREQ_CURRENT against PLATE_FREQ_SETPOINT -- with the
  // setpoint still 0 there is nothing to spin up to and READY is never reached.
  // Measured: the board sat at state 100 for 90s and the run aborted. The order
  // that works is the one run_recal_watch uses: set the speed, then enter.
  await perif({ type: 'set_setup', plate: { freq: PLATE_FREQ } }, '"ack"');

  const em = await perif({ type: 'enter_insp_mode' }, '"type"', 4000);
  ok('the BOARD accepted enter_insp_mode', !!em, JSON.stringify(em));

  // WAIT FOR READY, AND TOUCH NOTHING UNTIL IT ARRIVES.
  //
  // enter_insp_mode does not mean running: the sequence is CAL -> SPINUP ->
  // READY, and CAL owns GATE_DISABLED (it saves CAL_GATE_PREV on entry and
  // restores it on exit). drain() shuts the gate, so a measurement starting
  // before READY reaches in and takes the flag calibration is using -- and
  // calibrationEnd then restores the value the test wrote, not the one the
  // machine had.
  //
  // Measured: the run did exactly that, calibration timed out, error 14
  // (CAM_CLOCK_CAL_FAILED) landed in the history, the machine never reached
  // READY, and the whole sweep reported 進料 0 / delayed_n 0 -- the delay was
  // set correctly every time and never executed once, because nothing was ever
  // inspected. Every row was a measurement of a machine the test had broken.
  let ready = false;
  for (let i = 0; i < 90; i++) {
    const st = await stat();
    const code = st && st.state;
    if (code === 101) { ready = true; break; }
    if (code === 112 || (st && st.error)) {
      throw new Error(`the machine faulted before READY (state=${code}, `
                    + `err=${JSON.stringify(st && st.error)}) -- nothing below would measure it`);
    }
    if (i % 10 === 0) console.log(`   waiting for READY (state=${code})`);
    await sleep(1000);
  }
  ok('the machine reached READY (CAL and SPINUP are done)', ready);
  if (!ready) throw new Error('never reached READY');
  await sleep(1000);

  await sleep(4000);   // let the feeder get parts moving
  const warm0 = snap(await stat());
  await sleep(6000);
  const warm = snap(await stat());
  const warmRateMs = 6000 / Math.max(1, warm.accept - warm0.accept);
  console.log(`   warm-up: accept=${warm.accept} (+${warm.accept - warm0.accept} in 6s) `
            + `NA=${warm.NA} unans=${warm.UNANS}`);
  // accept is CUMULATIVE, so `accept > 0` is true on any machine that has ever
  // run and asserts nothing. What matters is whether parts are arriving NOW.
  ok('parts are arriving now', (warm.accept - warm0.accept) > 0,
     `+${warm.accept - warm0.accept} in 6s, state=${warm.state}`);
  if ((warm.accept - warm0.accept) === 0)
    throw new Error('no parts are arriving -- every delay point below would divide by this');

  section('a healthy host: the deadline is met');
  const p0 = await point('baseline', 0);
  ok('parts are judged', p0.judged > 0, `judged=${p0.judged}`);
  ok('nothing goes unjudged', p0.d.UNANS === 0 && p0.d.SKIP === 0,
     `unans=${p0.d.UNANS} skip=${p0.d.SKIP}`);

  section('the host slows down — where is the edge?');
  // Bracketing the service rate, which is what actually binds. arrivalMs is
  // measured from the warm-up rather than assumed from virt_pulse's period,
  // because with a real feeder nobody chooses it.
  const arrivalMs = warmRateMs;
  console.log(`   arrival interval measured at ${arrivalMs.toFixed(0)} ms `
            + `(${(1000 / arrivalMs).toFixed(1)} /s); deadline ${budgetMs.toFixed(0)} ms`);
  const inside = Math.max(5, Math.round(arrivalMs * 0.4));
  const over   = Math.round(arrivalMs * 2.5);
  const pIn = await point('well inside', inside);
  await point('at the edge', Math.round(arrivalMs * 0.95));
  ok('inside the deadline the machine still judges', pIn.judged > 0, `judged=${pIn.judged}`);
  ok('the reported latency tracks the injected delay',
     pIn.b.cam_avg_ms >= inside * 0.5,
     `cam_avg=${pIn.b.cam_avg_ms}ms vs injected ${inside}ms`);

  section('past the deadline, policy = none (no safety net)');
  await perif({ type: 'set_setup', skip_policy: { mode: 'none' } }, '"ack"');
  const pNone = await point('over, mode=none', over);
  // RULE 1 IS THE ONE THAT MUST HOLD HERE. A part nobody judged must not be
  // sorted; it is left unactuated and rides round again. That is what makes
  // "best effort" safe, and it is the assertion worth keeping.
  ok('parts go unjudged rather than being sorted wrong',
     (pNone.d.UNANS + pNone.d.SKIP) > 0,
     `unans=${pNone.d.UNANS} skip=${pNone.d.SKIP} judged=${pNone.judged}`);
  ok('the machine does not stop', !pNone.b.err,
     `state=${pNone.b.state} err=${JSON.stringify(pNone.b.err)}`);
  console.log('   ^ mode=none 的代價:不停機,但也沒人被判過,而且沒有任何訊號');

  section('past the deadline, policy = stop_only (the safety net)');
  await perif({ type: 'clear_error' }, '"ack"', 2000);
  await perif({ type: 'set_setup', skip_policy: { mode: 'stop_only', stop_after: 5 } }, '"ack"');
  const pStop = await point('over, stop_only', over, 12000);
  ok('the consecutive counter is what reacts',
     pStop.b.consec > 0 || !!pStop.b.err,
     `consec=${pStop.b.consec} state=${pStop.b.state} err=${JSON.stringify(pStop.b.err)}`);

  section('the answer: throttle the feed instead of hitting the threshold');
  await perif({ type: 'clear_error' }, '"ack"', 2000);
  // stop_only may have halted the line above; that is the point of it. Re-enter
  // so this section measures the throttle rather than the aftermath.
  await perif({ type: 'enter_insp_mode' }, '"type"', 2000);
  await sleep(1500);
  // Feed at a rate the SLOW host can actually answer at, with margin -- which
  // is exactly what the panel's 「填平均」 button offers from cam_avg_us.
  const procSep = Math.round(over * 1000 * 1.4);
  await perif({ type: 'set_setup', gate: { proc_sep_us: procSep } }, '"ack"');
  console.log(`   proc_sep_us = ${procSep} (${(1e6 / procSep).toFixed(1)} 顆/秒)`);
  const pGate = await point('over + 均速節流', over, 12000);
  ok('the feed is being throttled', pGate.d.rej_load > 0, `rej_load=${pGate.d.rej_load}`);
  ok('the parts that do get in are judged', pGate.judged > 0,
     `judged=${pGate.judged} unans=${pGate.d.UNANS}`);
  ok('so the machine keeps running instead of stopping', !pGate.b.err,
     `state=${pGate.b.state} err=${JSON.stringify(pGate.b.err)}`);

  console.log(`\n   對照 (同一個 ${over}ms 慢主機):`);
  console.log(`     沒有節流:  判定 ${pStop.judged}  無判決 ${pStop.d.UNANS + pStop.d.SKIP}  連續 ${pStop.b.consec}`);
  console.log(`     有節流:    判定 ${pGate.judged}  無判決 ${pGate.d.UNANS + pGate.d.SKIP}  擋下 ${pGate.d.rej_load}`);
} catch (e) {
  console.log('\n   ERROR: ' + e.message);
  ok('the run completed', false, e.message);
} finally {
  section('restore');
  await restore();
  const s = await stat();
  ok('the plate is stopped and the machine is idle',
     !!s && s.state === 100 && s.plate_freq === 0, `state=${s && s.state}`);
}
