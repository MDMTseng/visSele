// Build the shape_based fixture def by asking a running core to extract the
// features, then writing what it hands back.
//
//   1. python make_sbm_fixture_image.py sbm_synth.png
//   2. start a core (its cwd needs a data/ folder)
//   3. node make_sbm_fixture_def.mjs [ws://127.0.0.1:4090] [abs/path/to/sbm_synth.png]
//
// WHY THE CORE HAS TO DO THIS
//
// A def's __SBM_INFO__ carries a line2Dup FeatureSet plus a fingerprint over
// the template and every extraction parameter. Hand-writing one is not a
// shortcut, it is a forgery: the core recomputes that fingerprint on load, a
// value that does not match is rejected as stale, and implicit extraction is
// off -- so the fixture would load, refuse its own features, and fall back to
// sig360, which is precisely the failure the fixture exists to test. The only
// way to get a fingerprint that matches is to let the extractor produce it.
//
// The image path is stamped as _ref_image_path (an ABSOLUTE path here) because
// the core trains from a file on disk and has no path that uses an image in
// memory. That key is stripped from what is written out: a checked-in fixture
// must not carry one developer's directory layout.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import WebSocket from 'ws';

const here = path.dirname(fileURLToPath(import.meta.url));
const URL = process.argv[2] || 'ws://127.0.0.1:4090';
const IMG = path.resolve(process.argv[3] || path.join(here, 'sbm_synth.png'));
const OUT = path.join(here, 'sbm_synth.hydef');

if (!fs.existsSync(IMG)) { console.error('no image at ' + IMG); process.exit(2); }

const BPG_HDR = 9, enc = new TextEncoder();
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

// mmpp is a real-ish number rather than 1: a fixture measured in "pixels"
// would pass tests that a machine in millimetres fails.
const MMPP = 0.0125;
const ORIGIN_PX = { x: 1224, y: 1024 };

// The whole part, with a margin. The extractor only looks inside this, so a
// region that clipped the outline would train on a fragment -- and the def
// would still load and still locate, just worse, which is not a fixture
// anybody could trust.
//
// IN OBJECT-FRAME MILLIMETRES, not pixels. localization_include is documented
// as an object-frame mm polygon (FeatureManager_sig360_circle_line.cpp, "Object-
// frame mm polygon arrays"), and the first version of this file wrote pixels --
// 80x too large, so the mask covered nothing, the core logged
// "masked features=0 too few; retrying without mask" and extracted from the
// whole frame anyway. The fixture still worked, which is the problem: it would
// have shipped carrying a region that does nothing, and the first person to
// trust it would conclude regions do not work.
const mm = (px, py) => ({ x: (px - ORIGIN_PX.x) * MMPP, y: (py - ORIGIN_PX.y) * MMPP });
const INCLUDE = [[mm(380, 250), mm(2070, 250), mm(2070, 1800), mm(380, 1800)]];

const defInfo = {
  type: 'binary_processing_group',
  name: 'sbm_synth',
  tag: 'fixture,synthetic',
  featureSet: [{
    type: 'sig360_circle_line',
    ver: '0.0.1.0',
    unit: 'px',
    mmpp: MMPP,
    locating_engine: 'shape_based',
    shape_match_scale: 0.5,
    shape_weak_thres: 30,
    shape_strong_thres: 30,
    matching_angle_margin_deg: 180,
    matching_face: 1,
    // The origin and the 0-degree axis. Chosen, not defaulted: an absent
    // def_image_reg puts the object frame at the IMAGE CORNER, so the part
    // rotates about a corner -- fine on the reference image where the rotation
    // is zero, wrong on anything turned, and reported by nothing.
    def_image_reg: { cx: 1224 * MMPP, cy: 1024 * MMPP, angle: 0, isFlipped: false },
    features: [],
    inherentfeatures: [{
      id: 100200, type: 'sbm_info', name: '@__SBM_INFO__',
      localization_include: INCLUDE,
    }],
    _ref_image_path: IMG.replace(/\\/g, '/'),
  }],
};

const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
let pg = 1, done = false;

const bail = (why, code = 1) => { console.error(why); try { ws.close(); } catch {} process.exit(code); };
setTimeout(() => { if (!done) bail('timed out waiting for the core'); }, 30000);

ws.on('error', (e) => bail('ws: ' + e.message));
ws.on('open', () => {
  // A short settle: the core answers a heartbeat first on some builds and a
  // request sent into that window is dropped rather than queued.
  setTimeout(() => {
    console.log('extracting from ' + IMG);
    ws.send(frame('SF', 0, pg++, { definfo: defInfo, regenerate: true }));
  }, 400);
});

ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  const text = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0+$/, '');
  if (type !== 'SF') return;

  let data;
  try { data = JSON.parse(text); } catch (e) { bail('SF reply was not JSON: ' + text.slice(0, 200)); }

  const nFeat = (data.features || []).length;
  const cache = data.shape_cache;
  if (!cache || nFeat === 0)
    bail(`extraction produced nothing (features=${nFeat}, cache=${!!cache}). ` +
         'The core could not read the template, or the include region excluded the part.');

  // Write the fixture: the def as sent, plus the cache the core just produced,
  // minus this machine's absolute path.
  const out = JSON.parse(JSON.stringify(defInfo));
  const fs0 = out.featureSet[0];
  delete fs0._ref_image_path;
  const sbm = fs0.inherentfeatures.find((e) => e.name === '@__SBM_INFO__');
  sbm.shape_cache = cache;

  // The returned `roi` is DELIBERATELY NOT written into the fixture.
  //
  // The cache fingerprint covers the ROI points (roi<set>:<count> in the fp
  // string). These were extracted with none, so the fingerprint says roi0:0 --
  // and a def carrying eight of them recomputes roi1:8 on load, does not match,
  // is rejected as stale, and falls back to sig360. The fixture would look
  // fine, load without error, and quietly test the wrong locator.
  //
  // A def that wants explicit ROI points has to be EXTRACTED with them (fill
  // them in the studio, then press 生成特徵點), which is a different fixture.

  fs.writeFileSync(OUT, JSON.stringify(out, null, 2));
  done = true;
  console.log(`features=${nFeat}  roi=${(data.roi || []).length}  fp=${String(cache.fp).slice(0, 60)}`);
  console.log('wrote ' + OUT + ' (' + fs.statSync(OUT).size + ' bytes)');
  ws.close();
  process.exit(0);
});
