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
//
// The CONNECT goes over the BPG WEBSOCKET (:4090) and is held open by a
// DETACHED perif_hold.mjs, not over the console's '!pd' injection, because
// '!pd' does not work and never has -- and because the channel dies with the
// client that opened it, so the CONNECT cannot be fire-and-forget either.
//
// toUpperLayer refuses any BPG payload with no NUL inside its declared length
// ("[PD] payload of N bytes is not NUL-terminated -- refusing"), a guard added
// against dat_raw being scanned past its end. The WebUI and every ws client in
// this directory satisfy it by allocating body.length + 1 and counting the
// trailing zero in the length. The console's injection builds its packet with
// GenStrBPGData, which sets `size = strlen(jsonStr)` -- the terminator is at
// dat_raw[size], one byte OUTSIDE the declared length -- so the guard rejects
// every '!TL' packet, unconditionally. The console still prints
// {"core":"PD injected"} first, so it looks like it worked.
//
// This was invisible on the bench all of 2026-08-19 because a leftover
// Playwright headless Chromium was still attached to :4090 and IT was doing
// the CONNECT. Kill the browser and this tool stops working -- which is how
// the bug was finally found. Plain JSON typed at the console still reaches the
// device normally; only the '!TL' core-injection path is dead.
import net from 'node:net';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HOLDER = path.join(path.dirname(fileURLToPath(import.meta.url)), 'perif_hold.mjs');

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const UART = arg('uart', 'COM3');
const FREQ = Number(arg('freq', 15000));     // 15000 = production; 2*freq is the tick rate
const PORT = Number(arg('port', 4099));
const WSPORT = Number(arg('wsport', 4090));   // BPG websocket -- where PD CONNECT has to go

const s = net.connect(PORT, '127.0.0.1');
let buf = '', last = null, raw = '';
const send = (o) => s.write(JSON.stringify(o) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
s.on('data', (d) => {
  buf += d.toString('utf8');
  raw += d.toString('utf8');   // undrained copy: buf is consumed line by line below
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

// The channel belongs to the BPG client that opened it: when that websocket
// closes the core runs delete_PeripheralChannel and the console goes back to
// {"err":"no perif channel"}. So the CONNECT cannot be done and forgotten from
// here -- something has to stay attached for the whole session. perif_hold.mjs
// is that something; spawn it DETACHED and let it outlive this process.
//
// The "is a channel already up?" test is done on the console connection THIS
// process already holds, not by spawning `perif_hold --status`. The console
// serves one client at a time and does not say so: a second connection is
// accepted by TCP, then sits unread in the listen backlog until the first
// leaves. Spawning the status check therefore always reported DOWN, and this
// tool started a second holder and rebooted the board for no reason. (That is
// finding F5 in CONSOLE_ABUSE_2026-08-19.md, reproduced by accident here.)
{
  raw = '';
  send({ type: 'get_running_stat' });
  await sleep(2000);
  const held = !/no perif channel/.test(raw) && /"health"/.test(raw);
  if (held) {
    console.log('1. perif channel already up (a holder is attached) -- no CONNECT, no reboot');
  } else {
    console.log(`1. PD CONNECT ${UART} via perif_hold.mjs on :${WSPORT} (this reboots the board -- once)`);
    const h = spawn(process.execPath, [HOLDER, '--uart', UART, '--wsport', String(WSPORT)],
                    { detached: true, stdio: 'ignore' });
    h.unref();
    await sleep(1500);
  }
}

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
