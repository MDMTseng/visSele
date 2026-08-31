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
// NOTHING MOVES. set_dry_run holds the plate with the driver energised while
// the stage clock keeps advancing, and virt_pulse supplies the objects.
// Verified in the firmware rather than assumed: StepGo()'s first line is
// `if(DRY_RUN) return;`, so the driver receives no pulses at any plate_freq.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { makeTally, sleep } from './_rf_lib.mjs';
import net from 'node:net';

const ctl = makeCtl('http://127.0.0.1:8765');
const T = makeTally(); const { ok, section } = T;
const MODEL = process.argv[2] || 'data/testNew2';
const PLATE_FREQ = 30000;      // tick rate 60k/s -> a deadline short enough to sweep

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
const core  = (o) => perif(o, '"type"', 2000);

// The plate ramps -- at accel 2000, 0<->30000 is about fifteen seconds. Both
// ends of this test have to wait for it: objects do not flow until the stage
// clock is at speed, and set_dry_run is refused outright while
// PLATE_FREQ_CURRENT is non-zero ("plate must be at rest"). The first version
// slept 800ms and left dry_run ON, which is the one state this test must never
// walk away from.
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
  await core({ type: 'insp_debug_delay', ms: 0 });
  await perif({ type: 'virt_pulse', period_ticks: 0 }, '"type"', 2000);
  await perif({ type: 'exit_insp_mode' }, '"ack"', 2000);
  await perif({ type: 'set_setup', plate: { freq: 0 } }, '"ack"', 2000);
  await waitFreq(0, 25000);
  const off = await perif({ type: 'set_dry_run', on: false }, '"type"', 2000);
  if (off && off.dry_run_err)
    console.log('   WARNING: dry_run is STILL ON -- ' + off.dry_run_err);
  if (ORIG) {
    await perif({ type: 'set_setup', skip_policy: ORIG.skip_policy,
                  gate: { proc_sep_us: ORIG.proc_sep_us } }, '"ack"', 2000);
  }
  console.log('   restored: delay off, train off, dry_run off, policy + proc_sep_us back');
}
process.on('SIGINT', async () => { await restore(); process.exit(1); });

// ---- one delay point -------------------------------------------------------
async function point(label, delayMs, holdMs = 9000) {
  await core({ type: 'insp_debug_delay', ms: delayMs, jitter_ms: Math.round(delayMs * 0.15) });
  const a = snap(await stat());
  await sleep(holdMs);
  const b = snap(await stat());
  const d = diff(a, b);
  const judged = d.SEL1 + d.SEL3;
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

  const dr = await perif({ type: 'set_dry_run', on: true }, '"type"');
  ok('dry_run on — the plate is held, the stage clock runs',
     !!dr && dr.dry_run === true, JSON.stringify(dr));
  // Refusing to continue is the point: every step below assumes the driver is
  // muted, and running them without that turns a bench test into a moving plate.
  if (!dr || dr.dry_run !== true) throw new Error('dry_run refused — nothing else here is safe to run');

  await freshPage(ctl, 'http://127.0.0.1:8081/');
  await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  const entered = await enterInspection(ctl, { mode: '測試', log: (m) => console.log('   ' + m) });
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
  const em = await perif({ type: 'enter_insp_test_mode' }, '"type"', 3000);
  ok('the BOARD is in inspection test mode', !!em && em.err === undefined,
     JSON.stringify(em));
  await sleep(1000);
  await perif({ type: 'set_setup', plate: { freq: PLATE_FREQ } }, '"ack"');
  // Long enough for the ramp: at accel 2000 the setpoint takes ~15s to reach
  // 30000, and objects only flow once the stage clock is at speed.
  await waitFreq(PLATE_FREQ, 25000);
  // 3000 ticks at 60k ticks/s = 20 objects/s: under the camera cap, and under
  // every proc_sep_us this test sets, so the feed is never the limiting factor
  // except where it is made to be.
  await perif({ type: 'virt_pulse', period_ticks: 3000 }, '"type"');
  await sleep(3000);
  const warm = snap(await stat());
  ok('objects flow with the plate held still', warm.accept > 0,
     `accept=${warm.accept} state=${warm.state}`);

  section('a healthy host: the deadline is met');
  const p0 = await point('baseline', 0);
  ok('parts are judged', p0.judged > 0, `judged=${p0.judged}`);
  ok('nothing goes unjudged', p0.d.UNANS === 0 && p0.d.SKIP === 0,
     `unans=${p0.d.UNANS} skip=${p0.d.SKIP}`);

  section('the host slows down — where is the edge?');
  const inside = Math.round(budgetMs * 0.5);
  const over   = Math.round(budgetMs * 1.6);
  const pIn = await point('well inside', inside);
  await point('at the edge', Math.round(budgetMs * 0.9));
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
  await perif({ type: 'enter_insp_test_mode' }, '"type"', 2000);
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
  ok('the plate is stopped and dry_run is off',
     !!s && !(s.gate || {}).dry_run, `state=${s && s.state}`);
}
