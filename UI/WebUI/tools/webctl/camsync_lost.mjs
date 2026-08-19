// Does the machine actually STOP when it can no longer place its frames?
//
//   node camsync_lost.mjs [--window 50] [--rate 1] [--phase 60] [--port 4099]
//
// The stop path -- gate() counting LOST_N consecutive out-of-window frames,
// raising fault_pending, and SYS_STATE_Transfer(INSPECTION_ERROR,
// CAM_CLOCK_LOST=13) -- is the one safety property in CAM_SYNC that matters
// most and the one hardest to provoke, because everything upstream of it works
// too well. With INSP_CAM_TS_MULT synthesising a slope:
//
//   * offset_us is re-measured outright on every accepted report (gate():772),
//     so a slope never accumulates into the offset;
//   * DRIFT_COMP projects the offset forward across gaps, and measured on this
//     bench it turns an 83us/s error into ~1us of residual delta;
//   * a slope big enough to defeat both is also big enough to stop the CAL
//     bootstrap converging, which fails as error 14 during calibration and
//     never reaches the running-machine path at all.
//
// So provoke it from the other side: leave the drift where a real machine has
// it and shrink the window under the error instead. Both knobs are runtime
// set_setup fields, so this needs no reflash and no rebuild.
//
// Two phases on one board, which is what makes it an experiment rather than an
// anecdote -- identical traffic, identical clock, one variable:
//   A  window tightened, drift_comp ON   -> expect SURVIVAL (delta ~1us)
//   B  same window,      drift_comp OFF  -> expect STOP     (delta ~83us)
// Phase A is the control. Without it, a stop in phase B only proves the window
// was small, not that compensation was the thing holding the machine up.
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const WINDOW = num('window', 50);
const RATE = num('rate', 1);
const PHASE = num('phase', 60);
const PORT = num('port', 4099);

const s = net.connect(PORT, '127.0.0.1');
let buf = '', last = null;
const send = (o) => s.write(JSON.stringify(o) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
s.on('data', (d) => {
  buf += d.toString('utf8'); let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).trim(); buf = buf.slice(nl + 1);
    if (line.startsWith('{')) { try { const j = JSON.parse(line); if (j.cam_sync) last = j; } catch {} }
  }
});
s.on('error', (e) => { console.error(`console ${PORT}:`, e.message); process.exit(1); });
async function stat(ms = 3000) {
  last = null; send({ type: 'get_running_stat' });
  const t = Date.now(); while (!last && Date.now() - t < ms) await sleep(50);
  return last;
}
const line = (tag, t, q) => {
  const c = q.cam_sync;
  console.log(`${tag} t+${String(t).padStart(3)}s state=${q.state} valid=${c.valid ? 1 : 0}` +
    ` dlast=${c.delta_last_us} dmax=${c.delta_max_us} resid=${c.resid_us}` +
    ` rej=${c.rejected} reb=${c.rebuilds} mdmax=${c.miss_delta_max_us}` +
    ` errs=[${(q.error_hist || []).join(',')}]`);
};

await new Promise((r) => s.on('connect', r));
const st0 = await stat();
if (!st0) { console.error('no reply from the core console'); process.exit(1); }
// 104/RECAL is not a fault and not something to bail on: the board drops into
// it after recal_idle_ms of no traffic, which is exactly what the gap between
// two tests looks like. Wait it out; only a state that is neither 101 nor 104
// means something is actually wrong.
let st = st0;
for (let i = 0; i < 60 && st.state === 104; i++) {
  if (i === 0) console.log('board is in 104/RECAL (idle top-up) -- waiting for READY');
  send({ type: 'trig_phantom_pulse' });          // give the recal something to finish on
  await sleep(1000);
  st = (await stat(900)) || st;
}
if (st.state !== 101) { console.error(`board is in state ${st.state}, not 101 -- bring it up first`); process.exit(1); }
console.log(`start: window ${st.cam_sync.window_us}us drift_comp=${st.cam_sync.drift_comp}` +
            ` slope=${st.cam_sync.slope_ppb}/${st.cam_sync.slope_n}`);

// A converged slope is the precondition, not a detail.
//
// expectedCamUs() compensates only `if(DRIFT_COMP && slope_n && est_cam_us)`.
// With slope_n at 0 the flag is on and the compensation is nothing, so phase A
// is silently identical to phase B and the experiment has no control. That is
// not hypothetical: after a clear_error the estimate is gone, and a run
// started straight afterwards halts in the CONTROL phase and looks like
// evidence that compensation does not work.
//
// It is also worth knowing in its own right: for the first minutes after an
// error recovery the machine is running with no drift compensation at all.
if (!st.cam_sync.slope_n) {
  console.error('');
  console.error('REFUSING TO RUN: slope_n=0, so DRIFT_COMP is inert and phase A is not a control.');
  console.error('Feed the board at ~1/s for a couple of minutes (camsync_drift.mjs) and retry.');
  process.exit(1);
}

let stopped = null;
async function phase(tag, setup, secs) {
  console.log(`\n--- ${tag} ---`);
  send({ type: 'set_setup', cam: setup });
  await sleep(400);
  // Fire immediately, then on the interval. setInterval alone waits a full
  // period before the first pulse, and at the low rates this test needs that
  // silence -- plus whatever came before it -- exceeds recal_idle_ms and the
  // board drops into a top-up recal before the phase has taken a sample.
  const pulse = () => send({ type: 'trig_phantom_pulse' });
  pulse();
  const feed = setInterval(pulse, 1000 / RATE);
  for (let t = 0; t < secs; t += 5) {
    await sleep(5000);
    const q = await stat(900);
    if (!q) { console.log(`${tag} t+${t + 5}s <no reply>`); continue; }
    line(tag, t + 5, q);
    // 104 is the idle top-up recal, not a stop. Only an ERROR state ends the
    // phase -- calling every non-101 a stop reported a routine recal as a
    // safety halt, which is the opposite of what this test exists to detect.
    if (q.state !== 101 && q.state !== 104 && q.state !== 103) {
      clearInterval(feed); stopped = { tag, t: t + 5, q }; return;
    }
  }
  clearInterval(feed);
}

await phase('A comp=ON ', { match_window_us: WINDOW, drift_comp: true }, PHASE);
if (!stopped) await phase('B comp=OFF', { match_window_us: WINDOW, drift_comp: false }, PHASE);

console.log('\n--- verdict ---');
if (!stopped) {
  console.log(`NO STOP in either phase at window=${WINDOW}us.`);
  console.log('The compensated AND uncompensated deltas both fit. Tighten --window and repeat.');
} else {
  const errs = stopped.q.error_hist || [];
  console.log(`stopped in phase ${stopped.tag.trim()} at t+${stopped.t}s: state=${stopped.q.state} error_hist=[${errs.join(',')}]`);
  // Error 1, not 13, is the expected halt here -- and finding that out is what
  // this test is for.
  //
  // gate() rejects on `nearest_delta > TOL_US` and byTs is set on
  // `nearestDelta <= TOL_US`: one variable, one threshold, complementary. So a
  // frame gate() rejects always has byTs == NULL. In READY, bySync is NULL too
  // (sync pulses only fire during CAL/RECAL), so tarP is NULL and the same
  // pass raises INSP_RESULT_MATCHES_NO_OBJECT and halts -- at :6777, after
  // gate() at :6595 but before consec_reject could ever reach LOST_N.
  //
  // The machine still stops, which is the property that matters. What does not
  // survive is the diagnosis and the hysteresis: the operator is told "a
  // verdict arrived for no known object" instead of "camera clock lost", and
  // LOST_N=2 -- written so that "one is a lost frame or a stray, two in a row
  // is the clock" -- never gets its second frame.
  if (errs.includes(13))
    console.log('error 13 = CAM_CLOCK_LOST -- gate() got its second consecutive miss.');
  else if (errs.includes(1)) {
    console.log('error 1 = INSP_RESULT_MATCHES_NO_OBJECT -- halted, but on the frame-level');
    console.log('        check, which pre-empts CAM_CLOCK_LOST. See the note above: 13 is');
    console.log('        unreachable from the running machine, not merely absent this run.');
  }
  else
    console.log('neither 1 nor 13 -- it stopped for some other reason; read the core log.');
  if (stopped.tag.startsWith('A'))
    console.log('WARNING: it stopped in the CONTROL phase. Compensation was not what held it up; the window is simply too tight.');
}
// Leave the board as it was found. A tightened window left behind would make
// every later test on this bench fail in a way that looks like a real defect.
send({ type: 'set_setup', cam: { match_window_us: 5000, drift_comp: true } });
await sleep(500);
console.log('restored: match_window_us=5000 drift_comp=true');
s.end(); process.exit(0);
