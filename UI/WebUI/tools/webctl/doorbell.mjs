// Camera-state doorbell end-to-end test (core CamStateWatchThread + the
// RC "cam_doorbell_ping" test hook).
//
// Why the hook exists: a healthy bench camera never produces a real
// transition -- a successful camera_ez_reconnect swaps the camera atomically
// under camera_lifetime_lock (no observable NULL window), and the BMP
// carousel's isInOperation() is always ACK. So the only way to exercise the
// full core->wire->client path on demand is to ring the bell explicitly.
//
// Phase 1 (suppression): subscribed and idle for 15s, the doorbell pgID
//   (0xCA11) must carry ZERO packets. A flapping watcher would push at up to
//   1/s to every subscriber -- worse than the 2s poll it replaced.
// Phase 2 (ring): RC {"target":"cam_doorbell_ping"} must produce, within 3s,
//   the exact ordered triplet on pgID 0xCA11:
//     SS {"start":true}  ->  GS {"camera_state":{...}}  ->  SS {"ACK":true}
//   with cam_status/present fields present in the GS.
//
// Non-destructive; safe to run against a live bench core.
//   node doorbell.mjs
import WebSocket from 'ws';
const H = 9, enc = new TextEncoder(), dec = new TextDecoder();
const DOORBELL_PGID = 0xCA11;
function frame(t, p, g, o) {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(H + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - H;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, H); return u;
}
const ws = new WebSocket('ws://127.0.0.1:4090'); ws.binaryType = 'arraybuffer';
let pg = 1;
const bell = [];   // every packet on the doorbell pgID, in arrival order
ws.on('message', d => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  const pgid = (b[3] << 8) | b[4];
  if (pgid !== DOORBELL_PGID) return;
  let j = null;
  try { j = JSON.parse(dec.decode(b.subarray(H)).replace(/\0+$/, '')); } catch {}
  bell.push({ t, j });
});
ws.on('error', e => { console.error('ws error:', e.message); process.exit(2); });
await new Promise(r => ws.on('open', () => setTimeout(r, 400)));
ws.send(frame('SB', 0, pg++, { stream: true }));

console.log('phase 1: 15s subscribed idle -- expecting 0 doorbell packets...');
await new Promise(r => setTimeout(r, 15000));
const idleCount = bell.length;
console.log(`  doorbell packets while idle: ${idleCount}`);

console.log('phase 2: RC cam_doorbell_ping -- expecting the SS/GS/SS triplet...');
bell.length = 0;
ws.send(frame('RC', 0, pg++, { target: 'cam_doorbell_ping' }));
await new Promise(r => setTimeout(r, 3000));

const shape = bell.map(p => p.t).join(',');
const gs = bell.find(p => p.t === 'GS');
const cs = gs && gs.j && gs.j.camera_state;
const okTriplet =
  shape === 'SS,GS,SS' &&
  bell[0].j && bell[0].j.start === true &&
  cs && typeof cs.cam_status === 'number' && typeof cs.present === 'boolean' &&
  bell[2].j && bell[2].j.start === false;
console.log(`  got [${shape}] camera_state=${JSON.stringify(cs)}`);

// Phase 3 (perif doorbell, pgID 0xCA12): a PD CONNECT to a throwaway local
// TCP "board" flips perif_pairing.link.connected -> the watcher must push a
// perif_state batch; PD DISCONNECT flips it back -> another one. Real state
// transitions, not a test hook.
console.log('phase 3: PD CONNECT/DISCONNECT -- expecting perif_state doorbells...');
const net = await import('node:net');
const srv = net.createServer(s => { s.on('error', () => {}); s.on('data', () => {}); });
await new Promise(r => srv.listen(5998, r));
const PERIF_PGID = 0xCA12;
const pbell = [];
ws.on('message', d => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  if (((b[3] << 8) | b[4]) !== PERIF_PGID) return;
  const t = String.fromCharCode(b[0], b[1]);
  let j = null; try { j = JSON.parse(dec.decode(b.subarray(H)).replace(/\0+$/, '')); } catch {}
  pbell.push({ t, j });
});
ws.send(frame('PD', 0, pg++, { type: 'CONNECT', ip: '127.0.0.1', port: 5998, machine_type: 'uInspESP32', cat_ok: 3, cat_ng: 1 }));
await new Promise(r => setTimeout(r, 3000));
const gotConn = pbell.some(p => p.t === 'GS' && p.j && p.j.perif_state && p.j.perif_state.connected === true);
ws.send(frame('PD', 0, pg++, { type: 'DISCONNECT' }));
await new Promise(r => setTimeout(r, 3000));
const gotDisc = pbell.some(p => p.t === 'GS' && p.j && p.j.perif_state && p.j.perif_state.connected === false);
srv.close();
console.log(`  perif doorbells: ${pbell.filter(p => p.t === 'GS').length} (connect seen: ${gotConn}, disconnect seen: ${gotDisc})`);

const pass = (idleCount === 0) && okTriplet && gotConn && gotDisc;
console.log(`RESULT: suppression=${idleCount === 0 ? 'PASS' : 'FAIL(' + idleCount + ' pkts)'} triplet=${okTriplet ? 'PASS' : 'FAIL'} perif=${gotConn && gotDisc ? 'PASS' : 'FAIL'}`);
ws.close();
process.exit(pass ? 0 : 1);
