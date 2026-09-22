// Does "使用這一幀" after a stream really keep the STREAMED frame?
//
//   node take_cache_probe.mjs            # needs a core on 4090 with a camera
//
// The take dialog picks which of the core's two caches its frame comes from,
// and the two are not the same picture:
//
//   __CACHE_IMG__                 the image loaded on DefConf entry -- the def's
//                                 own .png. A CI/FI stream does NOT update it.
//   __LAST_DATA_VIEW_CACHE_IMG__  the last frame that went through the data
//                                 view, i.e. what the live preview is showing.
//
// Choosing wrong writes the PREVIOUS recipe's picture as the new object's
// template, and nothing on screen looks any different -- the canvas shows the
// live frame either way, and the def saves without complaint. It is only
// visible later, as a locator trained on a part that is not the one in front of
// it. saveAlternateImage's comment records the last time this was got wrong.
//
// So this does not drive the UI. It reproduces the exact sequence the dialog
// performs -- load a known image, stream, stop, save from each cache -- and
// then asks the only question that matters: are the two files different, and is
// the streamed one the live camera rather than the file we loaded?
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import WebSocket from 'ws';

const here = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(here, '..', '..', '..', '..');
const CORE_DIR = process.env.PROBE_CORE_DIR || path.join(REPO, 'InspectionCore', 'Core0_1');
const KNOWN = (process.env.PROBE_IMG
  || path.join(here, 'fixtures', 'sbm_synth.png')).replace(/\\/g, '/');
const STREAM_MS = Number(process.env.PROBE_STREAM_MS || 8000);
const PGID = 11007;                       // the same group the take dialog uses

const OUT_CACHE = 'data/__probe_cache';           // core-relative, as SV expects
const OUT_LIVE = 'data/__probe_lastview';
const fileOf = (base) => path.join(CORE_DIR, base.replace(/\//g, path.sep) + '.png');

const BPG_HDR = 9, enc = new TextEncoder();
const frame = (t, p, g, o) => {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(BPG_HDR + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - BPG_HDR;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, BPG_HDR); return u;
};

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const ws = new WebSocket('ws://127.0.0.1:4090');
ws.binaryType = 'arraybuffer';
let pg = 1, imFrames = 0;
const send = (t, o) => ws.send(frame(t, 0, pg++, o));

ws.on('error', (e) => { console.log('ERR ' + e.message); process.exit(1); });
ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (t === 'IM') imFrames++;
});

for (const f of [fileOf(OUT_CACHE), fileOf(OUT_LIVE)]) {
  try { fs.unlinkSync(f); } catch { /* first run */ }
}

await new Promise((r) => ws.on('open', r));
await sleep(500);

// 1. Load a known image, exactly as entering DefConf does. This is what
//    __CACHE_IMG__ should hold for the rest of the run.
console.log('loading a known image into __CACHE_IMG__:');
send('LD', { imgsrc: KNOWN, down_samp_level: 1 });
await sleep(1500);
console.log('  ' + path.basename(KNOWN));

// 2. Stream, the way the take dialog does it.
console.log(`streaming for ${STREAM_MS / 1000}s (free run + stage_light_report):`);
send('ST', { CameraSetting: { trigger_mode: 0, down_samp_level: 1 } });
send('CI', { _PGID_: PGID, _PGINFO_: { keep: true },
             definfo: { type: 'stage_light_report', grid_size: [10, 10],
                        nonBG_thres: 100, nonBG_spread_thres: 180 },
             IMG_ignore_calib: true });
await sleep(STREAM_MS);
send('CI', { _PGID_: PGID, _PGINFO_: { keep: false } });
await sleep(600);
check(imFrames > 0, 'the stream produced no frames at all -- is the camera connected?');
console.log(`  ${imFrames} IM frames`);

// 3. Save from BOTH caches. The dialog picks one; this takes both so the
//    difference is a fact rather than an inference.
console.log('saving from both caches:');
send('SV', { filename: OUT_CACHE, type: '__CACHE_IMG__' });
await sleep(900);
send('SV', { filename: OUT_LIVE, type: '__LAST_DATA_VIEW_CACHE_IMG__' });
await sleep(1400);

const a = fileOf(OUT_CACHE), b2 = fileOf(OUT_LIVE);
const okA = fs.existsSync(a), okB = fs.existsSync(b2);
check(okA, `__CACHE_IMG__ wrote nothing (${a})`);
check(okB, `__LAST_DATA_VIEW_CACHE_IMG__ wrote nothing (${b2})`);
if (!okA || !okB) { console.log('\nFAIL: cannot compare'); ws.close(); process.exit(1); }

const A = fs.readFileSync(a), B = fs.readFileSync(b2), K = fs.readFileSync(KNOWN);
const same = (x, y) => x.length === y.length && x.equals(y);
console.log(`  __CACHE_IMG__          ${A.length} bytes`);
console.log(`  __LAST_DATA_VIEW__     ${B.length} bytes`);
console.log(`  the loaded image       ${K.length} bytes`);

// THE ASSERTION. If these two are the same file, the take dialog's choice of
// cache cannot matter -- and the reason it matters is the whole point.
console.log('the two caches hold different pictures:');
check(!same(A, B), 'both caches returned identical bytes; streaming did not change the data view');
console.log(`  ${same(A, B) ? 'IDENTICAL -- the distinction is not real on this build' : 'different'}`);

// And which is which: __CACHE_IMG__ must still be the thing we loaded.
//
// Compared by DIMENSIONS, not by size and not byte-for-byte. The core
// re-encodes on save with its own PNG settings, so the same pixels come back a
// different length -- the first version of this check compared byte counts and
// called a correct result a failure, because a flat synthetic image written at
// zlib level 9 nearly doubles when re-encoded. Dimensions are what actually
// distinguishes the two: the loaded file is the full frame, a live frame
// carries the camera's ROI.
const ihdr = (buf) => ({ w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) });
const dA = ihdr(A), dB = ihdr(B), dK = ihdr(K);
console.log(`  __CACHE_IMG__          ${dA.w}x${dA.h}`);
console.log(`  __LAST_DATA_VIEW__     ${dB.w}x${dB.h}`);
console.log(`  the loaded image       ${dK.w}x${dK.h}`);
console.log('__CACHE_IMG__ is still the image that was loaded, not a camera frame:');
check(dA.w === dK.w && dA.h === dK.h,
      `__CACHE_IMG__ is ${dA.w}x${dA.h} against the loaded ${dK.w}x${dK.h} -- a stream overwrote it`);
console.log(`  ${(dA.w === dK.w && dA.h === dK.h) ? 'ok' : 'OVERWRITTEN'}`);

console.log(fails
  ? `\nFAIL: ${fails} assertion(s). Files kept for inspection:\n  ${a}\n  ${b2}`
  : `\nPASS: the streamed frame lives in __LAST_DATA_VIEW_CACHE_IMG__, and __CACHE_IMG__ still holds the loaded image.\n  ${a}\n  ${b2}`);
ws.close();
process.exit(fails ? 1 : 0);
