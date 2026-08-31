// How closely does the board follow a host whose speed keeps changing?
//
//   node regress_proc_track.mjs [steps]
//
// The step sweep showed the loop finds the right floor and holds. This asks the
// harder question: the host changes speed every ten seconds, unpredictably,
// between 35ms and 100ms of injected delay -- does the board's own estimate of
// the service time follow it, and does the admission floor follow that?
//
// Everything measured here is the BOARD's, computed from the board's clock. The
// core contributes only the act of answering. Written out as JSON so the trace
// can be plotted rather than summarised.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { sleep } from './_rf_lib.mjs';
import net from 'node:net';
import fs from 'node:fs';

const ctl = makeCtl('http://127.0.0.1:8765');
const MODEL = process.argv[3] || 'data/testNew2';
const STEPS = Number(process.argv[2] || 12);
const PLATE_FREQ = 10000;
const PERIOD_TICKS = 1000;              // 20 objects/s
const OUT = 'proc_track.json';

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

async function coreDelay(ms) {
  const { ev } = ctl;
  await ev(`(function(){ window.__DLYR__='pending';
    var st=window.__GP_STORE__; var C=st.getState().ConnInfo.CORE_ID;
    st.dispatch({type:'MW_API_CALL',id:C,method:'send',param:{tl:'SC',prop:0,
      data:{type:'insp_debug_delay',ms:${ms},jitter_ms:0},
      promiseCBs:{
        resolve:function(p){var q=(p||[]).find(function(x){return x&&x.data&&x.data.type==='insp_debug_delay'});
          window.__DLYR__=q?JSON.stringify(q.data):'nopkt';},
        reject:function(e){window.__DLYR__='REJ '+e;}}}});})()`);
  for (let i = 0; i < 40; i++) {
    const r = await ev(`window.__DLYR__`);
    if (r !== 'pending') return r;
    await sleep(400);
  }
  return null;
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
  console.log(`   restored: state=${s && s.state}`);
}
process.on('SIGINT', async () => { await restore(); process.exit(1); });

const trace = [];
try {
  const s0 = await perif({ type: 'get_setup' }, '"gate"');
  if (!s0) throw new Error('no board');
  if ((s0.gate || {}).proc_svc_id_us === undefined
      && (await stat() || {}).gate?.proc_svc_id_us === undefined)
    console.log('   NOTE: this firmware may predate the inter-departure estimator');

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
      throw new Error(`faulted before READY (state=${st.state})`);
    await sleep(1000);
  }
  if (!ready) throw new Error('never reached READY');
  console.log('   READY');

  await perif({ type: 'set_gate_disable', on: true }, '"type"');
  await perif({ type: 'set_setup', gate: { proc_sep_us: 0, proc_auto: true } }, '"ack"');
  await perif({ type: 'virt_pulse', period_ticks: PERIOD_TICKS }, '"type"');
  await sleep(4000);

  // 35..100ms, changing every 10s, in an order nobody can anticipate -- a
  // monotonic ramp would let a slow filter look good by luck.
  const rnd = () => 35 + Math.floor(Math.random() * 66);
  console.log(`   ${STEPS} steps x 10s, delay 35..100ms, sampling 1 Hz`);
  console.log('    t   delay  svc_lat  svc_id   svc  floor  admit  wait  rho  unans');

  let t = 0, prevAcc = null;
  for (let k = 0; k < STEPS; k++) {
    const d = rnd();
    await coreDelay(d);
    for (let i = 0; i < 10; i++) {
      await sleep(1000);
      const s = await stat();
      const g = (s && s.gate) || {}, c = (s && s.count) || {}, pp = (s && s.pipe) || {};
      const acc = g.accept | 0;
      const admit = prevAcc === null ? 0 : (acc - prevAcc);
      prevAcc = acc;
      t += 1;
      const rec = {
        t, delay_ms: d,
        svc_lat_ms: null,
        svc_id_ms: g.proc_svc_id_us ? g.proc_svc_id_us / 1000 : null,
        svc_ms: g.proc_svc_us ? g.proc_svc_us / 1000 : null,
        floor_ms: g.proc_eff_us ? g.proc_eff_us / 1000 : 0,
        admit_hz: admit,
        waiting: pp.waiting | 0,
        rho_pct: g.proc_rho_pct | 0,
        unans: c.UNANSWERED | 0,
        cap_n: g.proc_auto_cap_n | 0,
        state: s && s.state,
      };
      trace.push(rec);
      const f = (v, w, dg = 0) => String(v === null || v === undefined ? '-'
                                 : (typeof v === 'number' ? v.toFixed(dg) : v)).padStart(w);
      console.log(`   ${f(t, 3)}  ${f(d, 5)}  ${f(rec.svc_id_ms, 7, 1)}  ${f(rec.svc_ms, 6, 1)}  `
                + `${f(rec.floor_ms, 6, 1)}  ${f(admit, 5)}  ${f(rec.waiting, 4)}  `
                + `${f(rec.rho_pct, 4)}  ${f(rec.unans, 6)}`);
    }
  }
} catch (e) {
  console.log('\n   ERROR: ' + e.message);
} finally {
  fs.writeFileSync(OUT, JSON.stringify(trace, null, 1));
  console.log(`   wrote ${trace.length} samples to ${OUT}`);
  await restore();
}
