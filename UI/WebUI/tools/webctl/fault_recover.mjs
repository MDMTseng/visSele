// A4 / worklist 3.2 -- deliberately break the firmware, then check what survived.
//
//   node fault_recover.mjs [--port 4099] [--only wdt|crash]
//
// Three things have to hold after each induced fault, and each one failing
// looks completely different on a bench:
//
//   1. THE BOARD COMES BACK.        uptime resets, then climbs again.
//   2. NVS SURVIVES.                machine_id / io_on_level / cfg_from_nvs
//                                   identical to before. This board is
//                                   active-low; losing io_on_level silently
//                                   inverts every actuator. (cfg_crc is
//                                   reported but NOT asserted -- see below.)
//   3. THE LINK COMES BACK BY ITSELF.  A reboot puts boot-ROM bytes (115200)
//                                   in front of a core reading at 230400, so
//                                   the core's parser latches. Before
//                                   request_rx_resync() (2026-08-21) it stayed
//                                   latched FOREVER while the board sat there
//                                   healthy -- so this is also the end-to-end
//                                   test for that fix.
//
// This is the destructive one. Run it LAST in a Track A session.
//
// It talks to the dev console (INSP_PERIF_CONSOLE=4099), so it exercises the
// real core->UART path rather than opening COM3 directly. A direct-serial
// version would prove the board recovers but say nothing about item 3.
import net from 'node:net';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const PORT = Number(arg('port', 4099));
const ONLY = arg('only', null);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
// Once we are finished the core closes its end, and an ECONNRESET printed
// under a PASS reads like the test failed. Only shout while we still care.
let done = false;
s.on('error', (e) => {
  if (done) return;
  console.error(`console ${PORT}: ${e.message}`);
  process.exit(1);
});
await new Promise((r) => s.once('connect', r));

let id = 70000;
async function ask(obj, ms = 2000) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  await sleep(ms);
  const hit = lines.find((l) => l.includes(`"id":${myId}`));
  if (!hit) return null;
  try { return JSON.parse(hit.slice(hit.indexOf('{'))); } catch { return null; }
}

// The board's own clock, straight from its SYSTIME chatter. A reboot is the
// only thing that makes this go BACKWARDS, which is a far more trustworthy
// signal than "did it stop answering for a while".
async function uptimeMs(windowMs = 2500) {
  lines = [];
  await sleep(windowMs);
  for (let i = lines.length - 1; i >= 0; i--) {
    const m = /SYSTIME:\s*(\d+)\s*ms/.exec(lines[i]);
    if (m) return Number(m[1]);
  }
  return null;
}

// Identity only. cfg_crc is NOT in here on purpose: it covers the config that
// is CURRENTLY IN EFFECT, not the stored one, so any runtime setting that was
// never saved (bareboard_up sets plate.freq=15000 and never calls save_setup)
// legitimately changes it across a reboot. Including it reported a firmware
// defect on the first run of this test; checking plate.freq afterwards found
// it back at 0, which is the whole explanation. cfg_crc is still printed --
// as data, with that caveat attached.
const NVS_KEYS = ['machine_id', 'io_on_level', 'cfg_from_nvs'];
async function nvsSnapshot() {
  const r = await ask({ type: 'get_setup' }, 2500);
  if (!r) return null;
  const out = {};
  for (const k of NVS_KEYS) out[k] = JSON.stringify(r[k]);
  return out;
}

// Give the link as long as a boot plus a resync cycle can take. The WebUI's
// RESYNC comes every ~9s, so anything under ~30s would report a failure that
// is really just impatience -- the same mistake proto_fuzz.mjs made.
const LINK_WAIT_MS = 45000;
async function waitForBoard() {
  const t0 = Date.now();
  while (Date.now() - t0 < LINK_WAIT_MS) {
    const r = await ask({ type: 'ping' }, 1500);
    if (r && r.type === 'pong') return Date.now() - t0;
    await sleep(1000);
  }
  return -1;
}

const FAULTS = [
  ['wdt_test',   { type: 'wdt_test',   confirm: true }, 'starve the task watchdog -> panic'],
  ['crash_test', { type: 'crash_test', confirm: true }, 'null deref -> panic'],
];

let failures = 0;
for (const [name, cmd, what] of FAULTS) {
  if (ONLY && !name.startsWith(ONLY)) continue;

  console.log(`\n=== ${name} -- ${what} ===`);

  const before = await nvsSnapshot();
  const crcBefore = (await ask({ type: 'get_setup' }, 2000) || {}).cfg_crc;
  const upBefore = await uptimeMs();
  if (!before) { console.log('  SKIP: board not answering get_setup before the test'); failures++; continue; }
  console.log(`  before: uptime=${upBefore}ms  cfg_crc=${crcBefore}`);

  const ack = await ask(cmd, 1500);
  console.log(`  fired : ${ack ? JSON.stringify(ack) : '(no ack -- expected if it died instantly)'}`);

  const backIn = await waitForBoard();
  if (backIn < 0) {
    console.log(`  FAIL  : no answer within ${LINK_WAIT_MS / 1000}s -- board or link did not recover`);
    failures++;
    continue;
  }
  console.log(`  link  : back after ${(backIn / 1000).toFixed(1)}s`);

  const upAfter = await uptimeMs();
  const rebooted = upAfter !== null && upBefore !== null && upAfter < upBefore;
  console.log(`  reboot: uptime ${upBefore} -> ${upAfter}  ${rebooted ? 'OK (went backwards)' : 'NOT SEEN'}`);
  if (!rebooted) failures++;

  const after = await nvsSnapshot();
  if (!after) { console.log('  FAIL  : get_setup unanswered after recovery'); failures++; continue; }
  const crcAfter = (await ask({ type: 'get_setup' }, 2000) || {}).cfg_crc;
  if (crcAfter !== crcBefore) {
    console.log(`  cfg_crc: ${crcBefore} -> ${crcAfter}  (expected if an unsaved`);
    console.log('           runtime setting was in effect -- check plate.freq)');
  } else {
    console.log(`  cfg_crc: ${crcAfter} unchanged`);
  }
  const drifted = NVS_KEYS.filter((k) => before[k] !== after[k]);
  if (drifted.length === 0) {
    console.log(`  nvs   : intact (${NVS_KEYS.join(', ')})`);
  } else {
    failures++;
    console.log('  FAIL  : NVS changed across the fault --');
    for (const k of drifted) console.log(`            ${k}: ${before[k]}  ->  ${after[k]}`);
  }
}

console.log(`\n${failures === 0 ? 'PASS' : `FAIL (${failures})`}`);
// end(), not destroy() + exit(): tearing the socket down and exiting in the
// same tick makes libuv assert on Windows ("!(handle->flags & UV_HANDLE_CLOSING)")
// and prints a crash line underneath a PASS, which reads like a failure.
done = true;
process.exitCode = failures === 0 ? 0 : 1;
s.end();
