// Hold a live CI session open and report what the core does over time.
//
// The II path (INST_CHECK) and the continuous-inspection path are different
// code: II builds a def and inspects one image, CI streams frames through
// inspQueue -> the pool -> the report thread. A leak or a stall in one says
// nothing about the other, so this drives the second one and watches the
// numbers that would show either.
//
//   node soak.mjs <def.hydef> <seconds>
import WebSocket from 'ws';
import fs from 'node:fs';

const DEF = process.argv[2];
const SECS = parseInt(process.argv[3] || '120', 10);
const URL = 'ws://127.0.0.1:4090';
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

const def = JSON.parse(fs.readFileSync(DEF, 'utf8'));
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
let pg = 1;
let reports = 0, dropped = 0, objects = 0, na = 0;
let lastReports = 0;
const t0 = Date.now();

ws.on('open', () => setTimeout(() => {
  ws.send(frame('CI', 0, pg++, { definfo: def, frame_count: -1, trigger_mode: 0 }));
  ws.send(frame('SB', 0, pg++, { stream: true }));
  console.log('CI + SB sent, soaking for %ds', SECS);
}, 400));

ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const b = new Uint8Array(data);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (type !== 'RP') return;
  let j; try { j = JSON.parse(new TextDecoder().decode(b.subarray(BPG_HDR))); } catch { return; }
  reports++;
  if (j.station && typeof j.station.region_dropped === 'number') dropped += j.station.region_dropped;
  const r = j.reports?.[0]?.reports || [];
  objects += r.length;
  if (r.length === 0) na++;
});

const tick = setInterval(() => {
  const el = Math.round((Date.now() - t0) / 1000);
  const rate = reports - lastReports;
  lastReports = reports;
  console.log(`t=${el}s reports=${reports} (+${rate}/5s) objects=${objects} empty=${na} region_dropped=${dropped}`);
}, 5000);

setTimeout(() => {
  clearInterval(tick);
  const el = (Date.now() - t0) / 1000;
  console.log(`\n總計 ${reports} 份報告 / ${el.toFixed(0)}s = ${(reports / el).toFixed(1)}/s`);
  ws.close();
  process.exit(0);
}, SECS * 1000);

ws.on('error', (e) => { console.error('ws error', e.message); process.exit(1); });
