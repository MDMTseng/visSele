// Flip the camera's trigger mode from a second client, while the WebUI keeps
// its own session on the same core.
//
// The Inspection UI puts the camera into hardware-trigger mode, and on this
// bench the trigger rides the uInsp backlight line -- no board, no frames, so
// the view freezes on the last one. Setting trigger_mode 0 hands the BMP
// carousel back its free-run thread, which is the only way to watch the
// inspection path actually cycle without hardware.
//
//   node trigmode.mjs 0    -> free run (carousel feeds frames)
//   node trigmode.mjs 1    -> hardware trigger (what the UI sets)
import WebSocket from 'ws';
const MODE = parseInt(process.argv[2] ?? '0', 10);
const URL = process.argv[3] || 'ws://127.0.0.1:4090';
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
  return buf;
}
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
let pg = 1;
ws.on('open', () => setTimeout(() => {
  ws.send(frame('ST', 0, pg++, { CameraSetting: { trigger_mode: MODE, framerate: 10 } }));
  console.log(`sent trigger_mode:${MODE}`);
  setTimeout(() => { ws.close(); process.exit(0); }, 1500);
}, 300));
ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const b = new Uint8Array(data);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
});
ws.on('error', (e) => { console.error('ERR', e.message); process.exit(1); });
