// Does the sweep measure the SAME object at every step?
//
//   node sweep_identity_probe.mjs <def.hydef> <image.png> [axis]
//
// The robustness sweep reads sbmInspectResult's `pose`, which is `poses[0]` --
// the highest-scoring candidate, not a particular part. On a frame with one
// object that is the same thing. On a frame with several, "highest scoring" is
// a ranking that perturbation can reorder, and then
//
//     moved = thisStep.rotate - baseline.rotate
//
// is the angle between TWO DIFFERENT OBJECTS. The residual it produces is not a
// localization error and is not small, and nothing in the sweep can tell the
// difference: every step located something, every step scored well, and the
// curve looks like a real measurement.
//
// So this does not measure accuracy. It runs the sweep's own perturbations and
// asks one question per step: is the winner still the object that won at
// baseline? Identity is tracked by POSITION -- where the baseline winner must
// have moved to under a known rotation about the image centre -- because that
// is the only thing the two frames genuinely share.
import fs from 'node:fs';
import path from 'node:path';
import WebSocket from 'ws';

const DEF = process.argv[2];
const IMG = (process.argv[3] || '').replace(/\\/g, '/');
if (!DEF || !IMG) { console.error('usage: sweep_identity_probe.mjs <def> <image>'); process.exit(2); }

const FROM = -10, TO = 10, STEPS = 11;      // the sweep's own rotation defaults
const H = 9, enc = new TextEncoder();
const frame = (t, p, g, o) => {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(H + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - H; u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, H); return u;
};

const def = JSON.parse(fs.readFileSync(DEF, 'utf8'));
const fs0 = def.featureSet[0];
fs0._ref_image_path = IMG;
const MMPP = fs0.mmpp;

const ws = new WebSocket('ws://127.0.0.1:4090');
ws.binaryType = 'arraybuffer';
let pg = 1;
const pending = new Map();

ws.on('error', (e) => { console.error('ws ' + e.message); process.exit(1); });
ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  const t = String.fromCharCode(b[0], b[1]);
  const g = (b[3] << 8) | b[4];
  if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  const s = new TextDecoder().decode(b.subarray(H)).replace(/\0+$/, '');
  if (t === 'RP' && pending.has(g)) { try { pending.get(g).rp = JSON.parse(s); } catch {} }
  if (t === 'SS' && pending.has(g)) {
    let j = {}; try { j = JSON.parse(s); } catch {}
    if (j.cmd === 'HR') return;
    const e = pending.get(g); pending.delete(g); e.done(e.rp);
  }
});

const runOnce = (perturb) => new Promise((done) => {
  const g = pg++;
  pending.set(g, { done, rp: null });
  const prop = { calibInfo: { type: 'disable', mmpp: MMPP } };
  if (perturb) prop.perturb = perturb;
  ws.send(frame('II', 0, g, { definfo: def, imgsrc: IMG, img_property: prop }));
  setTimeout(() => { if (pending.has(g)) { pending.delete(g); done(null); } }, 20000);
});

const objectsOf = (rp) => {
  const r = rp && rp.reports && rp.reports[0] && rp.reports[0].reports;
  return Array.isArray(r) ? r.map((o) => ({
    cx: o.cx, cy: o.cy, rot: o.rotate, sim: o.similarity,
  })).filter((o) => Number.isFinite(o.cx)) : [];
};

await new Promise((r) => ws.on('open', r));
await new Promise((r) => setTimeout(r, 700));

// Regenerate first, so the cache matches THIS image and the run measures the
// locator rather than a fingerprint mismatch.
console.log('regenerating features for this image...');
{
  const g = pg++;
  const got = new Promise((done) => pending.set(g, { done, rp: null }));
  ws.send(frame('SF', 0, g, { definfo: def, regenerate: true }));
  await new Promise((r) => setTimeout(r, 4000));
  pending.delete(g);
}
// SF replies on its own type; simplest is to ask again and read the SF frame.
const cache = await new Promise((done) => {
  const g = pg++;
  const on = (d) => {
    if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
    const b = new Uint8Array(d);
    if (String.fromCharCode(b[0], b[1]) !== 'SF') return;
    const s = new TextDecoder().decode(b.subarray(H)).replace(/\0+$/, '');
    ws.off('message', on);
    try { done(JSON.parse(s).shape_cache); } catch { done(null); }
  };
  ws.on('message', on);
  ws.send(frame('SF', 0, g, { definfo: def, regenerate: true }));
  setTimeout(() => { ws.off('message', on); done(null); }, 20000);
});
if (!cache) { console.error('could not regenerate'); process.exit(1); }
fs0.inherentfeatures.find((e) => e.name === '@__SBM_INFO__').shape_cache = cache;
console.log('  ok\n');

// The image centre in object-frame mm: the perturbation rotates about it, so it
// is the pivot every expected position is computed around.
const W = 2448, Hh = 2048;                     // the frame this def was authored on
const reg = fs0.def_image_reg || { cx: 0, cy: 0, angle: 0 };
const pivot = { x: W / 2 * MMPP - reg.cx, y: Hh / 2 * MMPP - reg.cy };
// Object-frame axes are rotated by reg.angle relative to the image, so the
// pivot has to come back through that rotation to be comparable.
const ca = Math.cos(-reg.angle), sa = Math.sin(-reg.angle);
const P = { x: pivot.x * ca - pivot.y * sa, y: pivot.x * sa + pivot.y * ca };

const rotAbout = (p, deg) => {
  const t = deg * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
  const dx = p.cx - P.x, dy = p.cy - P.y;
  return { x: P.x + dx * c - dy * s, y: P.y + dx * s + dy * c };
};
const dist = (a, b) => Math.hypot(a.x - b.cx, a.y - b.cy);

console.log('baseline:');
const base = objectsOf(await runOnce(null));
if (!base.length) { console.error('baseline located nothing'); process.exit(1); }
base.forEach((o, i) => console.log(`  [${i}] sim=${o.sim.toFixed(4)} cx=${o.cx.toFixed(3)} cy=${o.cy.toFixed(3)} rot=${(o.rot * 180 / Math.PI).toFixed(2)}`));
const B = base[0];
console.log('');

const vals = [];
for (let i = 0; i < STEPS; i++) vals.push(FROM + (TO - FROM) * (i / (STEPS - 1)));

let swaps = 0, worstResid = 0, worstAt = null;
console.log('  step   n  win  sim     residual   winner-is-baseline?');
for (const v of vals) {
  if (Math.abs(v) < 1e-9) continue;
  const objs = objectsOf(await runOnce({ rot_deg: v, seed: 7 }));
  if (!objs.length) { console.log(`  ${v.toFixed(1).padStart(6)}   0    -       -          LOST`); continue; }
  // Where the baseline winner must be now.
  const want = rotAbout(B, v);
  // Which reported object is actually it (nearest in position).
  let best = 0, bd = Infinity;
  objs.forEach((o, i) => { const d = dist(want, o); if (d < bd) { bd = d; best = i; } });
  const win = objs[0];
  const moved = (win.rot - B.rot) * 180 / Math.PI;
  const resid = ((moved - v + 540) % 360) - 180;
  if (Math.abs(resid) > Math.abs(worstResid)) { worstResid = resid; worstAt = v; }
  const sameObj = (best === 0);
  if (!sameObj) swaps++;
  console.log(`  ${v.toFixed(1).padStart(6)}  ${String(objs.length).padStart(2)}   ${best}   ${win.sim.toFixed(4)}  ${resid.toFixed(3).padStart(8)}°  ${sameObj ? 'yes' : 'NO -- rank ' + best + ' is the baseline part'}`);
}

console.log('');
console.log(`candidates at baseline : ${base.length}`);
console.log(`steps where poses[0] was NOT the baseline object : ${swaps}`);
console.log(`worst residual : ${worstResid.toFixed(3)}° at ${worstAt}°`);
console.log(swaps
  ? '\nThe sweep would report those steps as localization error. They are not:\n'
    + 'they are the angle between two different objects, and every one of them\n'
    + 'located successfully with a good score.'
  : '\nposes[0] stayed the same object at every step, so the residuals on this\n'
    + 'image are real localization error.');
ws.close();
process.exit(0);
