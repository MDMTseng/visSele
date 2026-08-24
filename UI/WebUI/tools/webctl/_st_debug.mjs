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
const ws = new WebSocket('ws://127.0.0.1:4090'); ws.binaryType = 'arraybuffer';
let pg = 1;
ws.on('error', e => { console.log('ERR', e.message); process.exit(1); });
ws.on('open', () => { console.log('open'); setTimeout(() => {
  console.log('-> ST {MachineSetting:{...}}');
  ws.send(frame('ST', 0, pg++, { MachineSetting: { inspection_region: {x:1222,y:498,w:366,h:294,fit:'contain'}, clean_regions: [] } }));
}, 400); });
ws.on('message', d => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d); const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); console.log('<- HR (answered)'); return; }
  const s = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0+$/, '');
  console.log('<-', t, s.slice(0, 160));
});
setTimeout(() => { ws.close(); process.exit(0); }, 12000);
