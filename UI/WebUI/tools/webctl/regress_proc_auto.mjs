// Can the gate find its own throttle when the host is made slow?
//
//   node regress_proc_auto.mjs
//
// proc_sep_us has to be typed in, it differs per computer, and it moves with
// the recipe. proc_auto is meant to remove that: hold rho = lambda * service at
// a target, so the queue never starts growing. This makes the host slow on
// purpose and asks whether the loop finds the right admission rate by itself.
//
// WHY rho AND NOT THE QUEUE. Queue length is the integral of
// (arrival - service): below saturation it barely moves, above it diverges, so
// it only carries information once it is too late. Measured on this bench, with
// the queue reading zero unjudged the whole time:
//     delay  0ms -> rho 0.71   delay 19ms -> rho 1.11   delay 45ms -> halted
// rho crossed 1 at 19ms and said so; the queue said nothing until 45ms.
//
// TRAFFIC IS VIRTUAL, ON PURPOSE. virt_pulse paces objects by PLATE POSITION,
// so the arrival rate is exactly known and does not depend on how the feeder is
// behaving -- which matters when the whole measurement is a ratio against it.
// The gate is shut so real parts are never registered; they ride round again,
// exactly as they do for any rate or distance rejection. The plate still turns,
// because the stage clock is what carries objects to the camera.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { makeTally, sleep } from './_rf_lib.mjs';
import net from 'node:net';

const ctl = makeCtl('http://127.0.0.1:8765');
const T = makeTally(); const { ok, section } = T;
const MODEL = process.argv[2] || 'data/testNew2';
const PLATE_FREQ = 10000;          // tick rate 20k/s
const PERIOD_TICKS = 1000;         // 20 objects/s
const ARRIVAL_HZ = (2 * PLATE_FREQ) / PERIOD_TICKS;

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
        try { return done(JSON.parse(t)); } catch (e) { /* partial */ }
      }
    });
    sock.on('error', () => done(null));
    setTimeout(() => done(null), waitMs);
  });
}
const stat = () => perif({ type: 'get_running_stat' }, '"gate"');

// insp_debug_delay is a CORE command; 4099 forwards to the BOARD, which answers
// unknown_type. The core is reached over the WS the page already holds.
async function coreDelay(ms) {
  const { ev } = ctl;
  await ev(`(function(){ window.__DLYR__='pending';
    var st=window.__GP_STORE__; var C=st.getState().ConnInfo.CORE_ID;
    st.dispatch({type:'MW_API_CALL',id:C,method:'send',param:{tl:'SC',prop:0,
      data:{type:'insp_debug_delay',ms:${ms},jitter_ms:${Math.round(ms * 0.1)}},
      promiseCBs:{
        resolve:function(p){var q=(p||[]).find(function(x){return x&&x.data&&x.data.type==='insp_debug_delay'});
          window.__DLYR__=q?JSON.stringify(q.data):'nopkt';},
        reject:function(e){window.__DLYR__='REJ '+e;}}}});})()`);
  for (let i = 0; i < 60; i++) {
    const r = await ev(`window.__DLYR__`);
    if (r !== 'pending') return r;
    await sleep(500);
  }
  return null;
}

function row(label, s, dAcc, secs) {
  const g = (s && s.gate) || {}, c = (s && s.count) || {}, p = (s && s.pipe) || {};
  const f = (v, w, d = 0) => String(v === undefined ? '-' : (d ? v.toFixed(d) : v)).padStart(w);
  return `   ${label.padEnd(16)} rho ${f(g.proc_rho_pct, 4)}%  服務 ${f(g.proc_svc_us ? Math.round(g.proc_svc_us / 1000) : 0, 4)}ms  `
    + `自動+ ${f(g.proc_auto_add_us ? Math.round(g.proc_auto_add_us / 1000) : 0, 4)}ms  `
    + `實際下限 ${f(g.proc_eff_us ? Math.round(g.proc_eff_us / 1000) : 0, 4)}ms  `
    + `放行 ${f(dAcc / secs, 5, 1)}/s  擋下 ${f(g.rej_load, 5)}  `
    + `等待 ${f(p.waiting, 3)}  無判決 ${f(c.UNANSWERED, 5)}  觸頂 ${f(g.proc_auto_cap_n, 3)}  st ${f(s && s.state, 3)}`;
}

async function restore() {
  await coreDelay(0).catch(() => {});
  await perif({ type: 'virt_pulse', period_ticks: 0 }, '"type"', 2000);
  await perif({ type: 'set_setup', gate: { proc_auto: false, proc_sep_us: 33333 } }, '"ack"', 2000);
  await perif({ type: 'set_gate_disable', on: false }, '"type"', 2000);
  for (let i = 0; i < 5; i++) {
    await perif({ type: 'exit_insp_mode' }, '"ack"', 2000);
    await sleep(1000);
    const s = await stat();
    if (s && s.state === 100) break;
  }
  await perif({ type: 'set_setup', plate: { freq: 0 } }, '"ack"', 2000);
  for (let i = 0; i < 30; i++) {
    const s = await stat();
    if (s && s.plate_freq === 0 && !s.plate_freq_meas) break;
    await sleep(1000);
  }
  const s = await stat();
  console.log(`   restored: delay off, train off, auto off, gate open, state=${s && s.state}`);
}
process.on('SIGINT', async () => { await restore(); process.exit(1); });

async function point(label, delayMs, holdMs = 20000) {
  const ack = await coreDelay(delayMs);
  if (ack === null) console.log(`   NOTE: no ack for ${delayMs}ms -- this row is unconfirmed`);
  const a = await stat();
  const a0 = (a.gate || {}).accept | 0;
  await sleep(holdMs);
  const b = await stat();
  const dAcc = ((b.gate || {}).accept | 0) - a0;
  console.log(row(label, b, dAcc, holdMs / 1000));
  return b;
}

try {
  section('a machine fed at a rate we chose');
  const s0 = await perif({ type: 'get_setup' }, '"gate"');
  ok('the board answers', !!s0);
  if (!s0) throw new Error('no board');
  ok('this firmware has proc_auto', (s0.gate || {}).proc_auto !== undefined,
     JSON.stringify(s0.gate && s0.gate.proc_auto));

  await perif({ type: 'set_setup', plate: { freq: PLATE_FREQ } }, '"ack"');
  await freshPage(ctl, 'http://127.0.0.1:8081/');
  await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  await enterInspection(ctl, { mode: '全檢', log: (m) => console.log('   ' + m) });
  await perif({ type: 'enter_insp_mode' }, '"type"', 4000);

  let ready = false;
  for (let i = 0; i < 90; i++) {
    const st = await stat();
    if (st && st.state === 101) { ready = true; break; }
    if (st && (st.state === 112 || st.error))
      throw new Error(`faulted before READY (state=${st.state} err=${JSON.stringify(st.error)})`);
    if (i % 15 === 0) console.log(`   waiting for READY (state=${st && st.state})`);
    await sleep(1000);
  }
  ok('the machine reached READY', ready);
  if (!ready) throw new Error('never reached READY');

  // Real parts stop being registered; the virtual train is the only traffic, so
  // the arrival rate is a number this test chose rather than one it measured.
  await perif({ type: 'set_gate_disable', on: true }, '"type"');
  // proc_sep_us to 0 so the auto term is standing on its own and the numbers
  // below are entirely its doing.
  await perif({ type: 'set_setup', gate: { proc_sep_us: 0, proc_auto: true } }, '"ack"');
  await perif({ type: 'virt_pulse', period_ticks: PERIOD_TICKS }, '"type"');
  await sleep(4000);
  console.log(`   arrival ${ARRIVAL_HZ.toFixed(1)} /s (virt_pulse ${PERIOD_TICKS} ticks), `
            + `target rho ${(s0.gate || {}).proc_auto_rho_pct}%`);

  section('the host is made slower, step by step');
  const pts = [];
  for (const d of [0, 30, 60, 120, 250]) pts.push({ d, s: await point(`delay ${d}ms`, d) });

  section('and then it is fast again');
  const back = await point('delay 0ms again', 0, 40000);

  section('did the loop do its job');
  const gAt = (p) => (p.s.gate || {});
  const base = gAt(pts[0]), slow = gAt(pts[pts.length - 1]);

  ok('the throttle grew as the host slowed',
     (slow.proc_auto_add_us | 0) > (base.proc_auto_add_us | 0),
     `${Math.round((base.proc_auto_add_us | 0) / 1000)}ms -> ${Math.round((slow.proc_auto_add_us | 0) / 1000)}ms`);

  // THE ONE THAT MATTERS. The admission floor should land near service/target,
  // because that is what holding rho at the target means. Checked against the
  // service time the board measured in the same window, not against the delay
  // that was asked for -- the real service time is the injected delay plus the
  // inspection the machine was doing anyway.
  const want = (slow.proc_svc_us | 0) * 100 / ((s0.gate || {}).proc_auto_rho_pct || 80);
  const got = slow.proc_eff_us | 0;
  ok('the floor it found matches service/target',
     want > 0 && Math.abs(got - want) < want * 0.35,
     `found ${Math.round(got / 1000)}ms, service/target = ${Math.round(want / 1000)}ms`);

  ok('nothing went unjudged at any point',
     pts.every((p) => ((p.s.count || {}).UNANSWERED | 0) === ((pts[0].s.count || {}).UNANSWERED | 0)),
     pts.map((p) => (p.s.count || {}).UNANSWERED).join(' -> '));

  ok('the machine never stopped', pts.every((p) => p.s.state === 101) && back.state === 101,
     pts.map((p) => p.s.state).join(' -> ') + ' -> ' + back.state);

  ok('it did not run away to the bound', (slow.proc_auto_cap_n | 0) === 0,
     `cap_n=${slow.proc_auto_cap_n}`);

  ok('it gave the throughput back when the host recovered',
     (back.gate.proc_auto_add_us | 0) < (slow.proc_auto_add_us | 0),
     `${Math.round((slow.proc_auto_add_us | 0) / 1000)}ms -> ${Math.round((back.gate.proc_auto_add_us | 0) / 1000)}ms`);
} catch (e) {
  console.log('\n   ERROR: ' + e.message);
  ok('the run completed', false, e.message);
} finally {
  section('restore');
  await restore();
}
