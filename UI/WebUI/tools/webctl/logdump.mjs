// Ask the running core to dump its log ring, without the WebUI.
//
//   node tools/webctl/logdump.mjs
//
// The ring is in memory and the only way out of it is SC {type:"log_dump"},
// which is wired to a button in the 運算核心 modal. That makes the log
// unobtainable from a terminal -- and the log is exactly what a terminal wants
// when the UI is the thing behaving oddly. The control port (4098) answers only
// ping and shutdown, so this goes over BPG on 4090 like the WebUI does.
//
// Writes crash_<UTC>.dump next to the core (its working directory), and prints
// the path it reports. Does not stop or disturb the core: log_request_dump()
// asks the drainer to write the ring out and returns.
import WebSocket from 'ws';

const BPG_HDR = 9, enc = new TextEncoder();
function frame(t, p, g, o) {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(BPG_HDR + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - BPG_HDR;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, BPG_HDR); return u;
}

const ws = new WebSocket('ws://127.0.0.1:4090');
ws.binaryType = 'arraybuffer';
let pg = 1, answered = false;

ws.on('error', (e) => { console.log('ERR ' + e.message); process.exit(1); });
ws.on('open', () => {
  // A short settle before speaking: the core answers a heartbeat first on some
  // builds, and a request sent into that window is dropped rather than queued.
  setTimeout(() => ws.send(frame('SC', 0, pg++, { type: 'log_dump' })), 300);
});
ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  const t = String.fromCharCode(b[0], b[1]);
  // The heartbeat must be answered or the core drops the peer mid-request.
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  const s = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0+$/, '');
  console.log(t + ' ' + s.slice(0, 200));
  if (t === 'SS' || s.includes('log_dump')) answered = true;
});
setTimeout(() => {
  if (!answered) console.log('no reply -- is a core running on 4090?');
  ws.close(); process.exit(answered ? 0 : 1);
}, 6000);
