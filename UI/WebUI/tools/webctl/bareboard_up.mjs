// Bring a bare board (no camera, no plate) from cold to INSPECTION_MODE_READY
// over the core's perif console. Headless -- no browser, no webctld.
//
//   node bareboard_up.mjs [--uart COM3] [--freq 15000] [--port 4099]
//
// Requires the core running with INSP_PERIF_CONSOLE=<port> and
// INSP_CAM_TS_SYNTH=1: without the synth the CAMSYNC calibration has no camera
// to learn from, the board sits in 102 until CAL_DEADLINE and lands in 112
// with error 14 (CAM_CLOCK_CAL_FAILED). That is not a configuration problem;
// it is what "no camera" means to this state machine.
//
// ONE CONNECT, EVER. Each PD CONNECT re-opens the serial port, which toggles
// DTR, which reboots the ESP32. Polling for readiness by reconnecting -- the
// obvious thing to write -- resets the board once per poll and it never
// finishes booting. Connect once, then wait.
import net from 'node:net';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const UART = arg('uart', 'COM3');
const FREQ = Number(arg('freq', 15000));     // 15000 = production; 2*freq is the tick rate
const PORT = Number(arg('port', 4099));

const s = net.connect(PORT, '127.0.0.1');
let buf = '', last = null;
const send = (o) => s.write(JSON.stringify(o) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
s.on('data', (d) => {
  buf += d.toString('utf8');
  let nl;
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

await new Promise((r) => s.on('connect', r));

console.log(`1. PD CONNECT ${UART} (this reboots the board -- once)`);
s.write(`!pd ${JSON.stringify({ type: 'CONNECT', uart_name: UART, baudrate: 230400,
  machine_type: 'uInspESP32', cam_idx: 1, pairing: 'timestamp', cat_ng: 1, cat_ok: 3 })}\n`);

console.log('2. waiting for the board to answer (boot + RESYNC takes a few seconds)');
let st = null;
for (let i = 0; i < 30 && !st; i++) { await sleep(1000); st = await stat(900); }
if (!st) { console.error('   board never answered get_running_stat'); process.exit(1); }
console.log(`   up: state=${st.state}`);

console.log('3. dry run on (SEL valves stay put, stepper stays energised)');
send({ type: 'set_dry_run', on: true });
await sleep(300);

// Nested under "plate", not top-level. {"type":"plate","freq":N} is silently
// ignored -- it parses, it just does not match, and the plate keeps whatever
// it had. A run at freq 12 instead of 15000 is one revolution per 50 minutes
// and every timing conclusion drawn from it is wrong.
console.log(`4. plate.freq = ${FREQ}`);
send({ type: 'set_setup', plate: { freq: FREQ } });
await sleep(300);

console.log('5. enter_insp_mode -> 102 (CAL)');
send({ type: 'enter_insp_mode' });

for (let i = 0; i < 60; i++) {
  await sleep(1000);
  const q = await stat(900);
  if (!q) continue;
  const c = q.cam_sync || {};
  if (q.state === 101) {
    console.log(`   READY at t+${i + 1}s: valid=${c.valid} offset_us=${c.offset_us} learned=${c.learned}`);
    s.end(); process.exit(0);
  }
  if (q.state === 112) {
    console.error(`   ERROR state 112 at t+${i + 1}s, error_hist=[${(q.error_hist || []).join(',')}]`);
    console.error('   14 = CAM_CLOCK_CAL_FAILED: calibration never converged. Is INSP_CAM_TS_SYNTH=1 set?');
    s.end(); process.exit(1);
  }
  if (i % 5 === 4) console.log(`   t+${i + 1}s state=${q.state} valid=${c.valid} learned=${c.learned}`);
}
console.error('   never reached 101');
s.end(); process.exit(1);
