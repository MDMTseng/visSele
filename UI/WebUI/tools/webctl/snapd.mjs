// Persistent snapshot daemon: connect ONCE, keep the latest inspected frame,
// write it on demand.
//
// Why it exists: every screenshot used to spin up a fresh BPG client, start a
// CI session, wait for frames and disconnect -- about ten seconds each, and
// worse than slow. A peer joining or leaving makes the core rebuild the
// peripheral channel, and CONNECT sends RESET to the board, which drops any
// manual light hold (ALL_OUTPUTS_SAFE). So the act of photographing turned the
// backlight off, and the photo came out black.
//
// This is what the WebUI does and why its snapshot is instant: stay connected.
//
// The obvious shortcut does NOT work. `!sv {"type":"__CACHE_IMG__"}` through the
// perif console looks like the WebUI's save, but the console builds the packet
// with GenStrBPGData and the SV handler decides between "dump the cache image"
// and "write these raw bytes" on `dat->size - strlen(json) - 1 == 0`. Injected
// packets fail that test, so the core silently writes a 16MB file of whatever
// was in the buffer -- named .png, containing zeros, with an ACK.
//
// FI, not CI. FI is TriggerMode(2) -- hardware triggered, one frame per pulse --
// so "the latest frame" is unambiguously the one just asked for, and no request
// file is needed: every frame that arrives is written straight out.
//
// Taking a photo is then the board's own snap sequence (script.jsx
// camSnapWithLight): light on, settle, trig_cam_pulse, settle, light off.
// ~250ms, hardware-accurate, and the strobe is part of it -- with the plate
// stopped nothing else strobes the backlight, and a bare trigger returns a
// black frame that reads as "the inspection found nothing".
//
// Usage:
//   node snapd.mjs <def-abs-path> <workdir> [ws]
// Then trigger from anywhere and read <workdir>/snap.jpg + snap.json.
import WebSocket from 'ws';
import fs from 'node:fs';
import path from 'node:path';

const DEF = process.argv[2];
const DIR = process.argv[3];
const URL = process.argv[4] || 'ws://127.0.0.1:4090';
if (!DEF || !DIR) { console.error('usage: snapd.mjs <def> <workdir> [ws]'); process.exit(2); }

const HDR = 9, enc = new TextEncoder(), dec = new TextDecoder();
function frame(t, p, pg, o) {
  const b0 = enc.encode(o == null ? '' : JSON.stringify(o));
  const b = new Uint8Array(HDR + b0.length + 1);
  b[0] = t.charCodeAt(0); b[1] = t.charCodeAt(1); b[2] = p;
  b[3] = pg >> 8; b[4] = pg & 255;
  const L = b.length - HDR;
  b[5] = L >>> 24; b[6] = (L >> 16) & 255; b[7] = (L >> 8) & 255; b[8] = L & 255;
  b.set(b0, HDR); b[HDR + b0.length] = 0;
  return b;
}

let pg = 1, lastJpeg = null, lastReport = null, frames = 0;
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';

ws.on('open', () => setTimeout(() => {
  ws.send(frame('ST', 0, pg++, { IMG_STREAMING_JPEG_QUALITY: 92 }));
  ws.send(frame('FI', 0, pg++, { deffile: DEF, frame_count: -1 }));
  ws.send(frame('SB', 0, pg++, { stream: true }));
  console.log('[snapd] connected, FI started (hardware trigger)');
}, 300));

ws.on('message', (d) => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d
    : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (t === 'IM') {
    const j = b.subarray(HDR + 15);
    if (j[0] === 0xFF && j[1] === 0xD8) {
      lastJpeg = Buffer.from(j); frames++;
      fs.writeFileSync(path.join(DIR, 'snap.jpg'), lastJpeg);
      console.log('[snapd] frame %d -> snap.jpg (%d bytes)', frames, lastJpeg.length);
    }
    return;
  }
  if (t === 'RP') {
    lastReport = dec.decode(b.slice(HDR)).replace(/\0+$/, '');
    fs.writeFileSync(path.join(DIR, 'snap.json'), lastReport);
  }
});

ws.on('close', () => { console.log('[snapd] ws closed'); process.exit(1); });
ws.on('error', (e) => { console.error('[snapd] ws error', e.message); process.exit(1); });

// No poller and no request file: in FI nothing arrives unless something
// triggered it, so whatever is on disk IS the last shot.
setInterval(() => {}, 1 << 30);
