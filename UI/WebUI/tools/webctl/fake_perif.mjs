// A fake peripheral device over TCP, so arbitrary bytes can be pushed INTO the
// core's data layer.
//
//   node fake_perif.mjs --inject stray            # one byte outside a frame
//   node fake_perif.mjs --inject badjson          # a malformed object
//   node fake_perif.mjs --inject huge             # a frame bigger than dataBuff
//   node fake_perif.mjs --inject none --hold 60   # just behave, for a link test
//
// The core's peripheral channel is not serial-only: the PD CONNECT handler
// takes `ip`+`port` as well as `uart_name`+`baudrate` and describes the result
// as "tcp:<ip>:<port>" (wiringPanel.cpp). So the whole device side can be
// impersonated from Node, and the bytes the core's parser sees become an
// input to the test instead of something to wait for.
//
// WHY THAT MATTERS: the core's data layer calls PerifChannel::recv_ERROR() on
// five conditions -- INIT_CHAR_ERROR (any byte outside a frame that is not
// '{' or whitespace), JSON_FORMAT_ERROR, RECV_BUFFER_FULL, RAW_DATA_OVERSIZE
// and RAW_CRC_ERROR -- and that function has an empty body with an `int`
// return type, which gcc -O2 emits as a single `ud2`. Every one of those five
// is therefore a core crash, and the first of them is what a noisy cable, a
// device reboot mid-frame or a half-written frame produces on a real machine.
//
// This script CONNECTs the core to itself, injects, and reports whether the
// core process survived. It does NOT put the link back: run bareboard_up.mjs
// afterwards to return the core to the real board.
import net from 'node:net';
import { execSync } from 'node:child_process';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const INJECT = arg('inject', 'stray');
const DEVPORT = Number(arg('devport', 4599));
const CONSOLE = Number(arg('console', 4099));
const HOLD = Number(arg('hold', 15));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const coreAlive = () => { try { return /visSele\.exe/.test(execSync('tasklist', { encoding: 'utf8' })); } catch { return null; } };

let sock = null;
let rxBytes = 0, rxFrames = 0;

const srv = net.createServer((c) => {
  sock = c;
  console.log('core connected to the fake device');
  c.on('data', (d) => { rxBytes += d.length; rxFrames += (d.toString('latin1').match(/\}/g) || []).length; });
  c.on('error', (e) => console.log(`device socket: ${e.message}`));
  c.on('close', () => { console.log('core closed the link'); sock = null; });
});

await new Promise((r) => srv.listen(DEVPORT, '127.0.0.1', r));
console.log(`fake device listening on 127.0.0.1:${DEVPORT}`);

// Tell the core to attach to us.
const con = net.connect(CONSOLE, '127.0.0.1');
await new Promise((r) => con.once('connect', r));
let conRx = '';
con.on('data', (d) => { conRx += d.toString('latin1'); });
console.log('PD DISCONNECT (releasing the real board)');
con.write(`!pd ${JSON.stringify({ type: 'DISCONNECT', CONN_ID: 714 })}\n`);
await sleep(1500);
console.log(`PD CONNECT tcp:127.0.0.1:${DEVPORT}`);
con.write(`!pd ${JSON.stringify({ type: 'CONNECT', ip: '127.0.0.1', port: DEVPORT,
  machine_type: 'uInspESP32', cam_idx: 1, pairing: 'timestamp', cat_ng: 1, cat_ok: 3 })}\n`);

for (let i = 0; i < 15 && !sock; i++) await sleep(500);
if (!sock) { console.error('the core never opened the TCP link -- is another channel still attached?'); process.exit(1); }

// Look alive for a moment first, so the injection lands on an established
// link rather than on one still being set up.
const beat = setInterval(() => { if (sock) sock.write('{"dbg":"SYSTIME: 1000 ms"}\n'); }, 1000);
await sleep(3000);
console.log(`link up: core has sent us ${rxBytes} bytes / ~${rxFrames} frames`);

const before = coreAlive();
console.log(`core process before injection: ${before ? 'RUNNING' : 'gone'}`);

const PAYLOAD = {
  // A single byte that is not '{', space, tab or newline, sent between frames.
  // This is the INIT_CHAR_ERROR path -- one stray byte on the wire.
  stray: () => 'X',
  // Well-framed braces, invalid JSON inside: JSON_FORMAT_ERROR.
  badjson: () => '{"a":]}\n',
  // Never closes: the assembler runs to sizeof(dataBuff) -> RECV_BUFFER_FULL.
  huge: () => '{"pad":"' + 'A'.repeat(24000) + '\n',
  none: () => '',
};
const gen = PAYLOAD[INJECT];
if (!gen) { console.error(`unknown --inject ${INJECT}`); process.exit(1); }

const bytes = gen();
if (bytes) {
  console.log(`injecting ${INJECT}: ${bytes.length} byte(s)`);
  sock.write(bytes);
}

for (let t = 1; t <= HOLD; t++) {
  await sleep(1000);
  if (!coreAlive()) {
    clearInterval(beat);
    console.log(`\nt+${t}s  CORE PROCESS GONE`);
    try {
      const dmp = execSync('ls -t InspectionCore/Core0_1/insp_crash_*.dmp | head -1',
        { encoding: 'utf8', shell: 'C:/msys64/usr/bin/bash.exe', cwd: 'C:/Users/w2110/Documents/workspace/visSele' }).trim();
      if (dmp) console.log(`minidump: ${dmp}`);
    } catch {}
    process.exit(0);
  }
}
clearInterval(beat);
console.log(`\ncore survived ${HOLD}s after --inject ${INJECT}`);
process.exit(1);
