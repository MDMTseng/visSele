// Teardown-under-fire probe for the WS layer lock rework (backlog 2.1/2.6).
//
// While an observer client keeps a live inspection stream (CI free-run + SB),
// this hammers the exact paths the rework changed:
//   - churn: waves of clients that subscribe to the stream and then DESTROY
//     the socket with no close frame -- teardown happens while the stream
//     thread may be mid-send to them (deferred-close path).
//   - stall: one client that subscribes and then never reads -- forces the
//     5s SO_SNDTIMEO + shutdown-in-safeSend + doClosing while a sender is
//     provably inside send() on that fd.
//
// Pass = the observer's RP counter advances throughout, and the core still
// answers a GS at the end. A hang, a crash, or a frozen observer stream is
// the failure signature this exists to catch.
//
//   node churn.mjs [rounds=30]
import WebSocket from 'ws';
const H = 9, enc = new TextEncoder();
function frame(t, p, g, o) {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(H + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - H;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, H); return u;
}
const ROUNDS = parseInt(process.argv[2] || '30', 10);
const URL = 'ws://127.0.0.1:4090';

// ---- observer: keep a stream and count it -------------------------------
const A = new WebSocket(URL); A.binaryType = 'arraybuffer';
let pgA = 1, rp = 0, im = 0;
A.on('message', d => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { A.send(frame('HR', 0, pgA++, { a: ['d'] })); return; }
  if (t === 'RP') rp++;
  if (t === 'IM') im++;
});
A.on('error', e => { console.error('observer error:', e.message); process.exit(2); });
await new Promise(r => A.on('open', () => setTimeout(r, 400)));
A.send(frame('ST', 0, pgA++, { IMG_STREAMING_JPEG_QUALITY: 85 }));
A.send(frame('CI', 0, pgA++, { deffile: 'data/test1.hydef', frame_count: -1, trigger_mode: 0 }));
A.send(frame('SB', 0, pgA++, { stream: true }));
await new Promise(r => setTimeout(r, 2500));
const rp0 = rp;
console.log(`observer streaming: RP=${rp} IM=${im}`);
if (rp === 0) { console.error('observer got no stream -- cannot test'); process.exit(2); }

// ---- stall client: subscribe, then never read ---------------------------
const stall = new WebSocket(URL); stall.binaryType = 'arraybuffer';
let pgS = 1;
stall.on('error', () => {});
await new Promise(r => stall.on('open', () => setTimeout(r, 200)));
stall.send(frame('SB', 0, pgS++, { stream: true }));
stall._socket.pause();           // stop reading; the core's sends back up
console.log('stall client armed (subscribed, socket paused)');

// ---- churn waves --------------------------------------------------------
let churned = 0;
for (let round = 0; round < ROUNDS; round++) {
  const wave = [];
  for (let i = 0; i < 3; i++) {
    const w = new WebSocket(URL); w.binaryType = 'arraybuffer';
    w.on('error', () => {});
    let pg = 1;
    w.on('message', d => {
      const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
      if (String.fromCharCode(b[0], b[1]) === 'HR') w.send(frame('HR', 0, pg++, { a: ['d'] }));
    });
    w.on('open', () => w.send(frame('SB', 0, pg++, { stream: true })));
    wave.push(w);
  }
  await new Promise(r => setTimeout(r, 220));
  for (const w of wave) { try { w._socket.destroy(); } catch {} churned++; }
  if ((round + 1) % 10 === 0) console.log(`round ${round + 1}/${ROUNDS}: churned=${churned} observer RP=${rp}`);
}

// ---- let the stall client's SO_SNDTIMEO fire and teardown settle --------
console.log('waiting 8s for the stall client teardown (SO_SNDTIMEO=5s)...');
await new Promise(r => setTimeout(r, 8000));
try { stall._socket.destroy(); } catch {}

// ---- verdict ------------------------------------------------------------
const rpMid = rp;
await new Promise(r => setTimeout(r, 3000));
const gsOK = await new Promise(res => {
  const C = new WebSocket(URL); C.binaryType = 'arraybuffer';
  let pg = 1;
  const to = setTimeout(() => { res(false); }, 5000);
  C.on('error', () => { clearTimeout(to); res(false); });
  C.on('message', d => {
    const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
    const t = String.fromCharCode(b[0], b[1]);
    if (t === 'HR') { C.send(frame('GS', 0, pg++, { items: ['camera_info'] })); return; }
    if (t === 'GS') { clearTimeout(to); C.close(); res(true); }
  });
});
const streamAlive = rp > rpMid;
console.log(`RESULT: churned=${churned} observer RP ${rp0} -> ${rp} (advancing at end: ${streamAlive}) core GS answer: ${gsOK}`);
A.send(frame('CI', 0, pgA++, { frame_count: 0 }));
setTimeout(() => process.exit(streamAlive && gsOK ? 0 : 1), 500);
