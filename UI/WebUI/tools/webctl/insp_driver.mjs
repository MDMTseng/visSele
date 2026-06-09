// Headless WebUI-mimic continuous-inspection driver.
// Connects to the core over the BPG/WebSocket, starts continuous inspection on a
// def, subscribes to the live stream, and receives N inspected frames -- measuring
// client-observed fps + per-frame latency and validating the JPEG decode (incl.
// the gray 1-component / format=2 path). This exercises the FULL live path the
// WebUI uses (camera -> inspection -> data-view transfer -> BPG/WS), unlike the
// C++ --insp-cont harness which stops at the inspection.
//
// Usage:
//   node insp_driver.mjs <def-abs-path> [N=30] [ws=ws://127.0.0.1:4090]
//
// Run the core first with a deterministic frame source, e.g.:
//   FORCE_BMP_CAROUSEL=<abs bmp folder> ./visSele.exe port=4090
import WebSocket from 'ws';
import fs from 'node:fs';

const DEF   = process.argv[2];
const N     = process.argv[3] ? parseInt(process.argv[3], 10) : 30;
const URL   = process.argv[4] || 'ws://127.0.0.1:4090';
if (!DEF) { console.error('usage: node insp_driver.mjs <def-abs-path> [N] [ws-url]'); process.exit(2); }

const BPG_HDR = 9;
const enc = new TextEncoder();

// --- BPG framing (verbatim from BPG_Protocol.objbarr2raw / raw2header) ----
function frame(type, prop, pgID, obj) {
  const str = obj == null ? '' : JSON.stringify(obj);
  const body = enc.encode(str);
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
  return {
    type: String.fromCharCode(b[0], b[1]),
    prop: b[2],
    pgID: (b[3] << 8) | b[4],
    length: b[5] * 0x01000000 + (b[6] << 16) + (b[7] << 8) + b[8],
  };
}
// IM extra-header (15 bytes) per raw2Obj_IM
function parseIM(buf) {
  const h = buf.subarray(BPG_HDR, BPG_HDR + 15);
  return {
    format: h[0], quality: h[1],
    offX: (h[2] << 8) | h[3], offY: (h[4] << 8) | h[5],
    width: (h[6] << 8) | h[7], height: (h[8] << 8) | h[9],
    scale: h[10], fullW: (h[11] << 8) | h[12], fullH: (h[13] << 8) | h[14],
    jpeg: buf.subarray(BPG_HDR + 15),
  };
}

let pg = 1;
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';

// stats
let frames = 0, decoded = 0, badJpeg = 0;
const fmtCount = {}; let lastT = 0; const gaps = [];
let started = false, firstFrameT = 0, savedSample = false;
const t0 = Date.now();
const elapsed = () => Date.now() - t0;

function start() {
  console.log(`[driver] connected; loading def + starting continuous inspection`);
  // 1) JPEG streaming q85 (what the WebUI's DefConf sends) -> format 1/2 instead
  //    of the 20MB raw-RGBA default. Quality is overridable via $JPEGQ.
  const q = process.env.JPEGQ ? parseInt(process.env.JPEGQ, 10) : 85;
  ws.send(frame('ST', 0, pg++, { IMG_STREAMING_JPEG_QUALITY: q }));
  // 2) load def + start continuous inspection (frame_count -1 = free run)
  ws.send(frame('CI', 0, pg++, { deffile: DEF, frame_count: -1 }));
  // 3) subscribe to the live stream
  ws.send(frame('SB', 0, pg++, { stream: true }));
}

ws.on('open', () => { /* wait for core HR, then start */ setTimeout(start, 200); });
ws.on('error', (e) => { console.error('[driver] ws error:', e.message); process.exit(1); });
ws.on('close', () => console.log('[driver] ws closed'));

ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer)) { data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength); }
  const buf = new Uint8Array(data);
  const h = header(data);
  if (!h) return;

  if (h.type === 'HR') {
    // respond to heartbeat like the WebUI does
    ws.send(frame('HR', 0, pg++, { a: ['d'] }));
    return;
  }
  if (h.type !== 'IM') return;   // we only meter image frames here

  const im = parseIM(buf);
  const now = elapsed();
  if (!started) { started = true; firstFrameT = now; lastT = now; }
  else { gaps.push(now - lastT); lastT = now; }

  fmtCount[im.format] = (fmtCount[im.format] || 0) + 1;
  // JPEG validity (format 1/2 are JPEG; format 0 = raw RGBA)
  let ok = true, note = '';
  if (im.format === 1 || im.format === 2) {
    const j = im.jpeg;
    const startOk = j.length > 4 && j[0] === 0xFF && j[1] === 0xD8;
    const endOk   = j.length > 4 && j[j.length - 2] === 0xFF && j[j.length - 1] === 0xD9;
    ok = startOk && endOk;
    note = `jpeg ${j.length}B start=${startOk?'ok':'BAD'} end=${endOk?'ok':'FRAGMENTED'}`;
    if (ok) decoded++; else badJpeg++;
    if (ok && !savedSample) { fs.writeFileSync('insp_sample.jpg', Buffer.from(j)); savedSample = true; note += ' -> insp_sample.jpg'; }
  } else { note = `format=${im.format} (raw)`; }

  if (frames < 12 || !ok)
    console.log(`[frame ${frames}] fmt=${im.format} ${im.width}x${im.height} scale=${im.scale} full=${im.fullW}x${im.fullH}  ${note}`);
  frames++;

  if (frames >= N) finish();
});

function finish() {
  ws.close();
  const dur = (lastT - firstFrameT) / 1000;
  const fps = dur > 0 ? (frames - 1) / dur : 0;
  const avg = gaps.length ? gaps.reduce((a, b) => a + b, 0) / gaps.length : 0;
  const min = gaps.length ? Math.min(...gaps) : 0;
  const max = gaps.length ? Math.max(...gaps) : 0;
  console.log('\n===== insp_driver summary =====');
  console.log(`frames received : ${frames}`);
  console.log(`formats         : ${JSON.stringify(fmtCount)}  (1=BGR-jpeg 2=gray-jpeg 0=raw)`);
  console.log(`jpeg decode     : ${decoded} ok, ${badJpeg} bad/fragmented`);
  console.log(`client fps      : ${fps.toFixed(1)}`);
  console.log(`inter-frame ms  : avg=${avg.toFixed(1)} min=${min} max=${max}`);
  process.exit(badJpeg > 0 ? 1 : 0);
}

// overall timeout
setTimeout(() => { console.error(`[driver] timeout: only ${frames}/${N} frames in ${elapsed()}ms`); finish(); }, 60000);
