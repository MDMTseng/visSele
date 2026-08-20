// Open the peripheral channel and HOLD it, headless.
//
//   node perif_hold.mjs [--uart COM3] [--wsport 4090] &
//   node perif_hold.mjs --status          # is a channel up right now?
//
// The core ties the peripheral channel to the BPG client that CONNECTed it:
// when that websocket closes, delete_PeripheralChannel runs and the console
// starts answering {"err":"no perif channel"}. That is deliberate (it is the
// last-tab-close path), but it means a fire-and-forget CONNECT tool cannot
// work -- the channel dies with the process that opened it.
//
// The console's '!pd' injection would not have this problem, because it comes
// from the core itself rather than from a client. It is also refused
// unconditionally (GenStrBPGData reports a length that excludes the payload's
// NUL, and toUpperLayer's guard requires one inside the length -- see
// CONSOLE_ABUSE_2026-08-19.md F3). Until that is fixed, something has to stay
// attached, and this is it.
//
// Run it in the background for the whole bench session. It answers HR
// heartbeats and does nothing else; everything else still goes over the
// console on :4099.
import WebSocket from 'ws';
import net from 'node:net';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const UART = arg('uart', 'COM3');
const WSPORT = Number(arg('wsport', 4090));
const CONSOLE = Number(arg('port', 4099));
const BPG_HDR = 9, enc = new TextEncoder();

function frame(tl, pg, obj) {
  const b = enc.encode(obj == null ? '' : JSON.stringify(obj));
  // +1 so the payload's terminator falls INSIDE the declared length: the core
  // refuses any BPG payload without a NUL in it.
  const u = new Uint8Array(BPG_HDR + b.length + 1);
  u[0] = tl.charCodeAt(0); u[1] = tl.charCodeAt(1); u[2] = 0;
  u[3] = pg >> 8; u[4] = pg & 255;
  const l = u.length - BPG_HDR;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, BPG_HDR);
  return u;
}

// --status asks the console, not the websocket: what matters is whether the
// channel the DEVICE traffic uses exists, not whether some client is attached.
if (argv.includes('--status')) {
  const s = net.connect(CONSOLE, '127.0.0.1');
  let got = '';
  s.on('data', (d) => { got += d.toString('latin1'); });
  s.on('error', (e) => { console.log(`console ${CONSOLE}: ${e.message}`); process.exit(2); });
  await new Promise((r) => s.once('connect', r));
  s.write(JSON.stringify({ type: 'get_running_stat' }) + '\n');
  await new Promise((r) => setTimeout(r, 2000));
  s.destroy();
  if (/no perif channel/.test(got)) { console.log('perif channel: DOWN'); process.exit(1); }
  if (/"health"/.test(got)) { console.log('perif channel: UP (device answered)'); process.exit(0); }
  console.log('perif channel: channel exists but the device did not answer');
  process.exit(3);
}

let pg = 1;
const ws = new WebSocket(`ws://127.0.0.1:${WSPORT}`);
ws.binaryType = 'arraybuffer';
ws.on('error', (e) => { console.error(`bpg ${WSPORT}: ${e.message}`); process.exit(1); });
ws.on('close', () => { console.error('bpg closed -- the perif channel goes with it'); process.exit(1); });
ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  if (b[0] === 72 && b[1] === 82) ws.send(frame('HR', pg++, { a: ['d'] }));   // 'HR'
});

await new Promise((r) => ws.on('open', () => setTimeout(r, 400)));
console.log(`PD CONNECT ${UART} (reboots the board -- once) and holding`);
ws.send(frame('PD', pg++, { type: 'CONNECT', uart_name: UART, baudrate: 230400,
  machine_type: 'uInspESP32', cam_idx: 1, pairing: 'timestamp', cat_ng: 1, cat_ok: 3 }));

process.on('SIGINT', () => { ws.close(); process.exit(0); });
setInterval(() => {}, 1 << 30);          // stay alive
