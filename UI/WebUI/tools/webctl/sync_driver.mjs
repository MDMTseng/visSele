// Headless FI-mode driver, for driving the machine with no browser in the loop.
//
// insp_driver.mjs runs CI (free-running preview). This one runs FI: one frame
// per hardware trigger, which is the only mode that makes the core pair frames
// to the device's triggers and send `report` (carrying cam_ts) back down the
// peripheral channel. That report is what teaches CamClockSync, so it is the
// mode the clock-sync investigation needs.
//
// Deliberately does NOT subscribe to the image stream: nothing here looks at
// pixels, and a 2448x2048 stream per trigger is pure noise for this.
//
// Closes on SIGINT/SIGTERM before exiting. The core has crashed on a client
// vanishing mid-write (EPIPE -> doClosing -> SIGSEGV in ws_conn::runLoop), so
// this never just dies.
//
//   node sync_driver.mjs <def-abs-path> [seconds=120] [ws=ws://127.0.0.1:4090]
import WebSocket from 'ws';

const DEF  = process.argv[2];
const SECS = process.argv[3] ? parseInt(process.argv[3], 10) : 120;
const URL  = process.argv[4] || 'ws://127.0.0.1:4090';
if (!DEF) { console.error('usage: node sync_driver.mjs <def-abs-path> [seconds] [ws-url]'); process.exit(2); }

const BPG_HDR = 9;
const enc = new TextEncoder();

function frame(type, prop, pgID, obj) {
  const body = enc.encode(obj == null ? '' : JSON.stringify(obj));
  const buf = new Uint8Array(BPG_HDR + body.length + 1);
  buf[0] = type.charCodeAt(0); buf[1] = type.charCodeAt(1); buf[2] = prop;
  buf[3] = pgID >> 8; buf[4] = pgID & 255;
  const len = buf.length - BPG_HDR;
  buf[5] = len >>> 24; buf[6] = (len >> 16) & 255; buf[7] = (len >> 8) & 255; buf[8] = len & 255;
  buf.set(body, BPG_HDR);
  buf[BPG_HDR + body.length] = 0;
  return buf;
}
function header(dv) {
  if (dv.byteLength < BPG_HDR) return null;
  const b = new Uint8Array(dv);
  return { type: String.fromCharCode(b[0], b[1]), prop: b[2],
           pgID: (b[3] << 8) | b[4],
           length: b[5] * 0x01000000 + (b[6] << 16) + (b[7] << 8) + b[8] };
}

let pg = 1, closing = false;
const seen = {};
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';

const t0 = Date.now();
const el = () => ((Date.now() - t0) / 1000).toFixed(1);

ws.on('open', () => setTimeout(() => {
  console.log(`[sync] connected, arming FI on ${DEF}`);
  ws.send(frame('FI', 0, pg++, { deffile: DEF }));
}, 200));

ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const h = header(data);
  if (!h) return;
  if (h.type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  seen[h.type] = (seen[h.type] || 0) + 1;
  if (h.type === 'RP') {
    const txt = new TextDecoder().decode(new Uint8Array(data).subarray(BPG_HDR));
    console.log(`[${el()}s] RP ${txt.slice(0, 160)}`);
  }
});

ws.on('error', (e) => { console.error('[sync] ws error:', e.message); finish(1); });
ws.on('close', () => { console.log('[sync] ws closed'); });

function finish(code = 0) {
  if (closing) return;
  closing = true;
  console.log(`\n[sync] packets by type: ${JSON.stringify(seen)}`);
  try { ws.close(); } catch {}
  setTimeout(() => process.exit(code), 500);
}

process.on('SIGINT', () => finish(0));
process.on('SIGTERM', () => finish(0));
setTimeout(() => { console.log(`[sync] ${SECS}s elapsed`); finish(0); }, SECS * 1000);
