// Hold an FI (full-inspection) session open on the core, and meter it.
//
// insp_driver.mjs sends CI and exits after N frames -- fine for measuring the
// stream, useless as a load. FI is the production session: TriggerMode(2)
// (hardware trigger off the board's Line0) and the station inspection_region
// ENFORCED, which is what decides whether a located object is judged at all.
//
// Stays connected until killed, so real_parts.py can drive the plate
// underneath it and the inspection runs for real.
//
//   node fi_hold.mjs <def-abs-path> [ws://127.0.0.1:4090]
import WebSocket from 'ws';

const DEF = process.argv[2];
const URL = process.argv[3] || 'ws://127.0.0.1:4090';
if (!DEF) { console.error('usage: node fi_hold.mjs <def-abs-path> [ws-url]'); process.exit(2); }

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
function header(dv) {
  if (dv.byteLength < BPG_HDR) return null;
  const b = new Uint8Array(dv);
  return { type: String.fromCharCode(b[0], b[1]), prop: b[2],
           pgID: (b[3] << 8) | b[4],
           length: b[5] * 0x01000000 + (b[6] << 16) + (b[7] << 8) + b[8] };
}

let pg = 1, frames = 0, lastReport = Date.now(), sinceReport = 0, lastPairKey = '';
const counts = {};
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';

ws.on('open', () => setTimeout(() => {
  console.log(`[fi] loading ${DEF} and opening a full-inspection session`);
  // JPEGQ=0 keeps the legacy raw-RGBA wire format (~1MB a frame), which is what
  // the WebUI still gets by default; 1-100 switches to JPEG. It is an A/B lever
  // for the preview's cost, so it has to be settable from outside.
  const jq = process.env.JPEGQ === undefined ? 85 : Number(process.env.JPEGQ);
  console.log(`[fi] IMG_STREAMING_JPEG_QUALITY=${jq}${jq ? '' : ' (raw RGBA)'}`);
  ws.send(frame('ST', 0, pg++, { IMG_STREAMING_JPEG_QUALITY: jq }));
  // MAXFPS caps the preview the way the WebUI does, per verdict class. Without
  // it the defaults decide how much of the stream is actually encoded, and a
  // throughput run then reports the cost of whatever rate happened to apply --
  // in the NA-heavy virtual-object runs that was 1 frame in 3.
  if (process.env.MAXFPS) {
    const f = Number(process.env.MAXFPS);
    console.log(`[fi] preview capped at ${f} fps (OK/NG/NA)`);
    ws.send(frame('ST', 0, pg++, { ImageTransferSetup: {
      OK_MAX_FPS: f, NG_MAX_FPS: f, NA_MAX_FPS: f } }));
  }
  ws.send(frame('FI', 0, pg++, { deffile: DEF, frame_count: -1 }));
  // Pairing health, which only the WS side can ask for: perif_pairing is a GS
  // item, while the soak harness talks to the serial console on 4099 and gets
  // the DEVICE's stat -- so trig_wait_* is invisible from there. It is the one
  // counter that separates "the link went quiet" from "the plate was empty",
  // since both leave every part unjudged and both read as a clean NA run.
  // Polled here rather than logged in the core because LOGE goes to the ring,
  // which is only readable after the fact, and the 2026-08-10 collapse took
  // the console down with it -- the dump request got no reply at all.
  setInterval(() => ws.send(frame('GS', 0, pg++, { items: ['perif_pairing'] })), 15000);
  if (!process.env.NO_STREAM) ws.send(frame('SB', 0, pg++, { stream: true }));
  else console.log('[fi] NOT subscribing to the image stream');
}, 300));
ws.on('error', (e) => { console.error('[fi] ws error:', e.message); process.exit(1); });
ws.on('close', () => { console.log('[fi] ws closed'); process.exit(0); });

ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer))
    data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const h = header(data);
  if (!h) return;
  counts[h.type] = (counts[h.type] || 0) + 1;
  if (h.type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  // GS replies come back as their own type, not as AK -- the first version of
  // this only parsed AK/ER and silently printed nothing while the frames were
  // arriving and being counted.
  if (h.type === 'AK' || h.type === 'ER' || h.type === 'GS') {
    const txt = new TextDecoder().decode(new Uint8Array(data).subarray(BPG_HDR));
    // Print pairing health only when it CHANGES. A line every 15s would bury
    // the transition that matters in a run measured in hours.
    const p = (() => { try { return JSON.parse(txt).perif_pairing; } catch { return null; } })();
    if (p) {
      const k = `${p.trig_wait_suppressed}/${p.trig_wait_skipped}/${p.stale}/${p.no_candidate}`;
      if (k !== lastPairKey) {
        lastPairKey = k;
        console.log(`[fi] pairing: suppressed=${p.trig_wait_suppressed}`
          + ` skipped=${p.trig_wait_skipped} wait_max=${p.trig_wait_max_ms}`
          + ` rx=${p.rx} matched=${p.matched} stale=${p.stale} no_cand=${p.no_candidate}`);
      }
      return;
    }
    console.log(`[fi] ${h.type}: ${txt.slice(0, 300)}`);
    return;
  }
  if (h.type === 'IM') { frames++; sinceReport++; }
  const now = Date.now();
  if (now - lastReport >= 5000) {
    console.log(`[fi] t=${Math.round((now - lastReport) / 1000)}s  img=${sinceReport}`
      + `  (${(sinceReport / ((now - lastReport) / 1000)).toFixed(1)}/s)`
      + `  total_img=${frames}  types=${JSON.stringify(counts)}`);
    sinceReport = 0; lastReport = now;
  }
});
