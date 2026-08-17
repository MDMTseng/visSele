// Dump every measurement an II (DefConfUI's INST_CHECK) produces, for A/B.
//
// The point is byte-comparable output across two builds: same def, same images,
// same order, values printed at full precision. Timings are deliberately NOT
// included -- they differ run to run and would drown the signal.
//
//   node ii_dump.mjs <def.hydef> <img1> [img2 ...] > out.txt
import WebSocket from 'ws';
import fs from 'node:fs';

const DEF = process.argv[2];
const IMGS = process.argv.slice(3);
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
let pg = 1, idx = 0;

function fire() {
  if (idx >= IMGS.length) { ws.close(); process.exit(0); }
  ws.send(frame('II', 0, pg++, {
    definfo: def,
    imgsrc: IMGS[idx],
    img_property: { calibInfo: { type: 'disable', mmpp: def.featureSet[0].mmpp } },
  }));
}

// Walk the report tree and emit every numeric leaf under a stable path, so a
// change anywhere in the measurement chain shows up as a diff line.
function walk(node, path, out) {
  if (node === null || node === undefined) return;
  if (typeof node === 'number') {
    out.push(`${path} = ${Number.isFinite(node) ? node.toPrecision(12) : String(node)}`);
    return;
  }
  if (Array.isArray(node)) { node.forEach((v, i) => walk(v, `${path}[${i}]`, out)); return; }
  if (typeof node === 'object') {
    for (const k of Object.keys(node).sort()) {
      if (/_ms$|_us$|time|timestamp|seq/i.test(k)) continue;   // run-to-run noise
      walk(node[k], path ? `${path}.${k}` : k, out);
    }
  }
}

ws.on('open', () => setTimeout(fire, 600));
ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const b = new Uint8Array(data);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (type !== 'RP') return;
  let j; try { j = JSON.parse(new TextDecoder().decode(b.subarray(BPG_HDR))); } catch { return; }
  const out = [];
  walk(j.reports, '', out);
  const name = IMGS[idx].split('/').pop();
  console.log(`### ${name}`);
  for (const l of out) console.log(l);
  idx++;
  setTimeout(fire, 250);
});
ws.on('error', (e) => { console.error('ws error', e.message); process.exit(1); });
setTimeout(() => { console.error('timeout'); process.exit(1); }, 60000);
