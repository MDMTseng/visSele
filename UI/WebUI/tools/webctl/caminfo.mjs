// What does the core actually report for camera_info?
// The UI's reconnect decision is made entirely from this reply.
import WebSocket from 'ws';
const URL = process.argv[2] || 'ws://127.0.0.1:4090';
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
ws.on('open', () => setTimeout(() => ws.send(frame('GS', 0, pg++, { items: ['camera_info'] })), 300));
ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const b = new Uint8Array(data);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  const txt = new TextDecoder().decode(b.subarray(BPG_HDR));
  if (type !== 'GS') return;
  try {
    const j = JSON.parse(txt);
    // The core now pushes unsolicited GS{"camera_state"} doorbells to the
    // default peer (which a sole caminfo client IS) -- a doorbell arriving
    // before our reply would be mistaken for it and print undefined.
    // Only accept the packet that answers what we asked.
    if (j.camera_info === undefined) return;
    const ci = j.camera_info;
    console.log(JSON.stringify(ci, null, 1));
    const c0 = ci && ci[0];
    if (c0) {
      console.log('--- the three fields the UI decides on:');
      console.log('   type       :', JSON.stringify(c0.type));
      console.log('   cam_status :', JSON.stringify(c0.cam_status));
      console.log('   name       :', JSON.stringify(c0.name));
    }
  } catch (e) { console.log('raw:', txt.slice(0, 400)); }
  ws.close(); process.exit(0);
});
ws.on('error', (e) => { console.error('ws error', e.message); process.exit(1); });
setTimeout(() => { console.log('no GS reply in 8s'); process.exit(2); }, 8000);
