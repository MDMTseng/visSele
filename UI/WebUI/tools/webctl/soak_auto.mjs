// An 8-hour soak of the two auto-limiting layers, sampled once a minute.
//
//   node soak_auto.mjs setup      -- bring the machine up, once
//   node soak_auto.mjs run <min>  -- sample for <min> minutes, then exit
//
// Split so a long soak survives being watched: each `run` appends to the same
// JSONL and exits on its own, which is the signal to regenerate the report and
// start the next stretch. The alternative -- one process for eight hours --
// makes every check-in a question about whether it is still alive.
//
// THIS RUNS THE MACHINE. The plate turns and READY turns the feeder on.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage, sleep } from './lib_enter.mjs';
import net from 'node:net';
import fs from 'node:fs';

const ctl = makeCtl('http://127.0.0.1:8765');
const MODE = process.argv[2] || 'run';
const MINUTES = Number(process.argv[3] || 60);
const OUT = 'soak_auto.jsonl';
const PLATE_FREQ = 10000;
const MODEL = 'data/testNew2';

function perif(cmd, want, ms = 6000) {
  return new Promise((r) => {
    const s = net.connect(4099, '127.0.0.1');
    let b = '';
    const d = (v) => { try { s.end(); } catch (e) {} r(v); };
    s.on('connect', () => s.write(JSON.stringify(cmd) + String.fromCharCode(10)));
    s.on('data', (x) => {
      b += x.toString();
      for (const l of b.split(String.fromCharCode(10))) {
        const t = l.trim();
        if (!t.startsWith('{') || (want && t.indexOf(want) < 0)) continue;
        try { return d(JSON.parse(t)); } catch (e) { /* partial */ }
      }
    });
    s.on('error', () => d(null));
    setTimeout(() => d(null), ms);
  });
}
const stat = () => perif({ type: 'get_running_stat' }, '"gate"');

if (MODE === 'setup') {
  // START FROM WHATEVER THE LAST RUN LEFT. A soak that only works from a clean
  // IDLE is a soak that cannot be restarted after the thing it is meant to
  // survive: the previous run ended with the core gone, so the board is sitting
  // in 112 with HOST_LINK_TIMEOUT latched and enter_insp_mode would be refused.
  await perif({ type: 'clear_error' }, '"ack"', 3000);
  await perif({ type: 'clear_error_history' }, '"ack"', 3000);
  for (let i = 0; i < 4; i++) {
    await perif({ type: 'exit_insp_mode' }, '"ack"', 3000);
    await sleep(800);
    const s0 = await stat();
    if (s0 && s0.state === 100) break;
  }
  // Speed BEFORE entering: SPINUP compares current against setpoint, and at
  // setpoint 0 there is nothing to spin up to.
  await perif({ type: 'set_setup', plate: { freq: PLATE_FREQ } }, '"ack"');
  await freshPage(ctl, 'http://127.0.0.1:8081/');
  await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  await enterInspection(ctl, { mode: '全檢', log: (m) => console.log('   ' + m) });
  await perif({ type: 'enter_insp_mode' }, '"type"', 4000);
  let ready = false;
  for (let i = 0; i < 120; i++) {
    const s = await stat();
    if (s && s.state === 101) { ready = true; break; }
    if (s && (s.state === 112 || s.error)) {
      console.log(`FAULTED before READY: state=${s.state} err=${JSON.stringify(s.error)}`);
      break;
    }
    if (i % 15 === 0) console.log(`   waiting for READY (state=${s && s.state})`);
    await sleep(1000);
  }
  console.log('READY:', ready);
  if (!ready) process.exit(1);
  // Both layers on auto -- that is the thing being soaked. proc_sep_us to 0 so
  // the host layer stands entirely on what it measures.
  const r = await perif({ type: 'set_setup',
    gate: { cam_mode: 'auto', proc_mode: 'auto', proc_sep_us: 0 } }, '"ack"');
  console.log('auto/auto:', JSON.stringify(r).slice(0, 60));
  const g = (await stat()).gate;
  console.log('cam_mode', g.cam_mode, '| proc_mode', g.proc_mode,
              '| fps', g.cam_fps_limit, '| eff', g.min_sep_eff_us);
  process.exit(0);
}

// ---- sampling ---------------------------------------------------------------
const t0 = Date.now();
let n = 0;
while (Date.now() - t0 < MINUTES * 60000) {
  const s = await stat();
  if (s) {
    const g = s.gate || {}, c = s.count || {}, h = s.health || {},
          p = s.pipe || {}, y = s.cam_sync || {}, l = s.report_latency || {};
    const rec = {
      ts: Date.now(), state: s.state, err: s.error || null,
      // layer 1
      cam_mode: g.cam_mode, cam_fps: g.cam_fps_limit, cam_age: g.cam_fps_age_s,
      cam_stale: !!g.cam_fps_stale,
      min_sep: g.min_sep_us, min_sep_eff: g.min_sep_eff_us,
      // layer 2
      proc_mode: g.proc_mode, proc_rate: g.proc_rate_hz, proc_eff: g.proc_eff_us,
      proc_add: g.proc_auto_add_us, proc_rho: g.proc_rho_pct,
      proc_svc: g.proc_svc_us, proc_svc_id: g.proc_svc_id_us,
      proc_cap_n: g.proc_auto_cap_n, rej_load: g.rej_load,
      // gate
      accept: g.accept, rej_rate: g.rej_rate, rej_dist: g.rej_dist,
      rej_busy: g.rej_busy, edges: g.edges,
      // verdicts
      SEL1: c.SEL1, SEL3: c.SEL3, NA: c.NA, SKIP: c.SKIP, UNANS: c.UNANSWERED,
      nomatch_orphan: c.NOMATCH_ORPHAN, nomatch_window: c.NOMATCH_WINDOW,
      nomatch_consec: c.NOMATCH_CONSEC, consec_unans: h.consec_unanswered,
      waiting: p.waiting, registered: p.registered,
      // clock
      resid: y.resid_us, resid_max: y.resid_max_us, drift: y.drift_us_per_s,
      slope_ppb: y.slope_ppb, valid: y.valid ? 1 : 0, rejected: y.rejected,
      rebuilds: y.rebuilds, recals: y.recals,
      recal_stealth: h.recal_stealth, recal_stealth_ok: h.recal_stealth_ok,
      recal_fallback: h.recal_fallback,
      // latency
      lat_avg: l.cam_avg_us, lat_max: l.cam_max_us,
      heap: h.free_heap, heap_min: h.min_heap,
    };
    fs.appendFileSync(OUT, JSON.stringify(rec) + String.fromCharCode(10));
    n++;
    if (s.error) console.log(`[${new Date().toTimeString().slice(0, 8)}] ERROR state=${s.state} err=${JSON.stringify(s.error)}`);
  } else {
    fs.appendFileSync(OUT, JSON.stringify({ ts: Date.now(), noreply: true }) + String.fromCharCode(10));
  }
  await sleep(60000);
}
console.log(`stretch done: ${n} samples over ${MINUTES} min`);
