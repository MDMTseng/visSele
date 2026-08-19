// Measure the board's clock-drift estimator against a KNOWN slope.
//
//   node camsync_drift.mjs <seconds> [--rate n] [--mult m] [--port 4099]
//
// Why this test can only be done on the bench, and why the bench is BETTER
// than the real machine for it:
//
// On a real machine cam_ts comes from the camera's crystal and cam_us from the
// board's, so CAM_SYNC's whole job -- offset, slope, drift compensation -- is
// measured against two clocks whose true relationship nobody knows. Every
// number the estimator produces has to be taken on faith. Measured drift on
// this pair is ~83us/s, but that figure came from the estimator being tested.
//
// With INSP_CAM_TS_SYNTH the core derives cam_ts from the board's own t_us:
//
//     cam_ts = t_us * INSP_CAM_TS_MULT + INSP_CAM_TS_OFFSET_US
//
// One clock, so the truth is arithmetic. The multiplier IS the slope: the
// offset is learned at some T0 as T0*(m-1), and thereafter the residual is
// (T-T0)*(m-1) -- a genuine ramp against the learned offset, not a step. So
// slope_ppb should converge to (m-1)*1e9 and this script can say how close it
// got, which no run with a camera attached can.
//
// At m=1 exactly (the default fixture) resid_us is identically zero, last_gap
// teaches nothing, slope_n stays 0, and the `if(DRIFT_COMP && slope_n && ...)`
// branch in expectedCamUs() is never entered. Everything below is code that
// has never executed on this bench until this test.
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (name, dflt) => {
  const i = argv.indexOf(`--${name}`);
  return i >= 0 ? Number(argv[i + 1]) : dflt;
};
const SECS = Number(argv[0] || 180);
const RATE = num('rate', 3);          // parts/s -- slow: the slope learns from LONG gaps
const MULT = num('mult', NaN);        // informational; the core was launched with it
const PORT = num('port', 4099);
const EXPECT_PPB = Number.isFinite(MULT) ? Math.round((MULT - 1) * 1e9) : null;

// The core log is the only place the ACTUAL multiplier is recorded, and it has
// to be checked rather than assumed. An orphaned core from an earlier run holds
// 4099 and the serial port; the one just launched fails to bind and exits
// quietly, so every command here reaches the OLD process with the OLD
// multiplier -- and the run looks completely normal. That happened. Point
// --log at the log of the core that is supposed to be answering.
const LOG = (() => { const i = argv.indexOf('--log'); return i >= 0 ? argv[i + 1] : null; })();

const s = net.connect(PORT, '127.0.0.1');
let buf = '';
const rows = [];
let lastStat = null;
const seen = { states: new Set(), errors: new Set() };

const send = (o) => s.write((typeof o === 'string' ? o : JSON.stringify(o)) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

s.on('data', (d) => {
  buf += d.toString('utf8');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).trim();
    buf = buf.slice(nl + 1);
    if (!line || line[0] !== '{') continue;
    let j; try { j = JSON.parse(line); } catch { continue; }
    if (j.cam_sync) lastStat = j;
    if (typeof j.state === 'number') seen.states.add(j.state);
    if (Array.isArray(j.error_hist)) for (const e of j.error_hist) seen.errors.add(e);
  }
});
s.on('error', (e) => { console.error(`console ${PORT}:`, e.message); process.exit(1); });

async function stat(timeoutMs = 3000) {
  lastStat = null;
  send({ type: 'get_running_stat' });
  const t0 = Date.now();
  while (!lastStat && Date.now() - t0 < timeoutMs) await sleep(50);
  return lastStat;
}

await new Promise((r) => s.on('connect', r));
console.log(`camsync_drift: ${SECS}s at ${RATE}/s` +
            (EXPECT_PPB === null ? '' : `, mult=${MULT} -> expect slope_ppb ~ ${EXPECT_PPB}`));

// The board is assumed already in inspection mode -- driving it there is
// enter_insp_mode's job and doing it here would hide a failure to get there.
const st0 = await stat();
if (!st0) { console.error('no get_running_stat reply -- is the core up with INSP_PERIF_CONSOLE?'); process.exit(1); }
if (LOG && Number.isFinite(MULT)) {
  const fs = await import('node:fs');
  const txt = fs.readFileSync(LOG, 'utf8');
  const m = txt.match(/cam_ts = t_us\*([0-9.]+) \+ (-?\d+)us/);
  if (!m) {
    console.error(`FATAL: ${LOG} never logged the synth line -- that core is not answering cam_trig.`);
    console.error('       Another core is almost certainly holding the console port.');
    process.exit(1);
  }
  if (Math.abs(Number(m[1]) - MULT) > 1e-9) {
    console.error(`FATAL: core log says mult=${m[1]}, this run claims ${MULT}. Wrong core.`);
    process.exit(1);
  }
  console.log(`core log confirms mult=${m[1]} offset=${m[2]}us`);
}
console.log(`start: state=${st0.state} cam_sync=${JSON.stringify(st0.cam_sync)}`);
if (st0.state !== 101) console.log(`  (not 101/READY -- results below may be a calibration transient)`);

const t0 = Date.now();
const feed = setInterval(() => send({ type: 'trig_phantom_pulse' }), 1000 / RATE);
const POLL_MS = 10000;

while (Date.now() - t0 < SECS * 1000) {
  await sleep(POLL_MS);
  const st = await stat();
  if (!st) { console.log(`t+${((Date.now() - t0) / 1000).toFixed(0)}s  <no reply>`); continue; }
  const c = st.cam_sync, t = ((Date.now() - t0) / 1000).toFixed(0);
  rows.push({ t: Number(t), ...c, state: st.state });
  console.log(
    `t+${t.padStart(4)}s state=${st.state} valid=${c.valid ? 1 : 0}` +
    ` off=${c.offset_us} resid=${c.resid_us} rmax=${c.resid_max_us}` +
    ` slope=${c.slope_ppb}/${c.slope_n} dmax=${c.delta_max_us}` +
    ` rej=${c.rejected} reb=${c.rebuilds} est=${c.established}` +
    ` errs=[${(st.error_hist || []).join(',')}]`);
}
clearInterval(feed);
await sleep(1500);
const end = await stat();

console.log('\n--- verdict ---');
console.log(`states seen : ${[...seen.states].sort((a, b) => a - b).join(', ')}`);
console.log(`errors seen : ${seen.errors.size ? [...seen.errors].join(', ') : 'none'}`);
if (end) {
  const c = end.cam_sync;
  console.log(`final       : ${JSON.stringify(c)}`);
  if (EXPECT_PPB !== null) {
    if (!c.slope_n) {
      console.log(`slope       : NEVER LEARNED (slope_n=0) -- expected ~${EXPECT_PPB} ppb`);
    } else {
      const err = c.slope_ppb - EXPECT_PPB;
      const pct = EXPECT_PPB ? (100 * err / EXPECT_PPB).toFixed(1) : 'n/a';
      console.log(`slope       : ${c.slope_ppb} ppb vs truth ${EXPECT_PPB} ppb` +
                  `  (err ${err > 0 ? '+' : ''}${err}, ${pct}%)  from ${c.slope_n} samples`);
    }
  }
}
s.end();
process.exit(0);
