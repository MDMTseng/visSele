// Unit test for the expected-position arithmetic. No core, no browser.
//
//   node unit_expectpose.mjs
//
// This is what replaces "take the highest-scoring candidate" in the studio's
// test and sweep. If it is wrong the studio does not fail loudly -- it rejects
// the real part and reports "no candidate near the expected position", which
// reads exactly like a locator that stopped working. So the arithmetic gets a
// test rather than a careful read.
import { imageCentre, expectedPosition, pickByPosition, nearestDistance }
  from '../../src/sbmExpectPose.mjs';

let fails = 0;
const check = (c, w) => { if (!c) { console.log('  FAIL ' + w); fails++; } return c; };
const near = (a, b, tol, w) => check(Math.abs(a - b) <= tol, `${w}: ${a} vs ${b}`);

const MMPP = 0.0125, W = 2448, H = 2048;

console.log('the pivot is the image centre, in image mm:');
{
  const p = imageCentre(W, H, MMPP);
  near(p.x, W / 2 * MMPP, 1e-9, 'pivot x');
  near(p.y, H / 2 * MMPP, 1e-9, 'pivot y');
  console.log('  no registration in it -- see the note in sbmExpectPose');
}

// THE ONE THAT IS NOT SELF-CONSISTENCY.
//
// Everything else here checks the model against itself, and the model used to
// be internally perfect and wrong: it worked in the object frame while the
// poses it compared against were in image mm, and it rotated the wrong way. The
// suite passed throughout, and the studio's sweep reported "usable range
// 0.00deg ~ 0.00deg" for a locator that was fine.
//
// So: real output from the core, for a real def, at five perturbations. Numbers
// from --insp on data/test1 with {"rot_deg": N} (2448x2048, mmpp 0.0138859432,
// def_image_reg 15.0252/9.3055). If the prediction stops matching these, the
// prediction is wrong -- not the fixture.
console.log('against the core, not against itself:');
{
  const M = 0.0138859432190657, IW = 2448, IH = 2048;
  const pivot = imageCentre(IW, IH, M);
  const base = { cx: 15.0250, cy: 9.3044 };          // rot_deg 0
  const measured = [
    [   4, 14.6871, 9.4536],
    [  -4, 15.3731, 9.1780],
    [  10, 14.2011, 9.7227],
    [ -10, 15.9090, 9.0372],
  ];
  const TOL_PX = 0.5;                                 // the sweep works to 20
  for (const [deg, mx, my] of measured) {
    const e = expectedPosition(base, pivot, { rot_deg: deg });
    const dpx = Math.hypot(e.x - mx, e.y - my) / M;
    check(dpx <= TOL_PX,
          `rot_deg ${deg}: predicted (${e.x.toFixed(4)}, ${e.y.toFixed(4)}), ` +
          `core said (${mx}, ${my}) -- ${dpx.toFixed(2)} px apart`);
  }
  // And rot_deg 0 must be the identity, which is the case that hid the bug.
  const z = expectedPosition(base, pivot, { rot_deg: 0 });
  near(z.x, base.cx, 1e-9, 'rot 0 x'); near(z.y, base.cy, 1e-9, 'rot 0 y');
  console.log('  4 perturbations measured on the bench, within ' + TOL_PX + ' px');
}

console.log('rotation moves the part around the pivot:');
{
  const pivot = { x: 0, y: 0 };
  const from = { cx: 10, cy: 0 };
  // +90 moves it to -y: a positive rot_deg turns the SCENE counter-clockwise in
  // a y-up sense, so a point in y-down image coordinates goes the other way.
  const a = expectedPosition(from, pivot, { rot_deg: 90 });
  near(a.x, 0, 1e-9, '90deg x'); near(a.y, -10, 1e-9, '90deg y');
  const b = expectedPosition(from, pivot, { rot_deg: 180 });
  near(b.x, -10, 1e-9, '180deg x'); near(b.y, 0, 1e-9, '180deg y');
  // A part AT the pivot does not move however far it is rotated.
  for (const d of [1, 17, 90, -33, 180]) {
    const c = expectedPosition({ cx: 0, cy: 0 }, pivot, { rot_deg: d });
    check(Math.hypot(c.x, c.y) < 1e-9, `a part at the pivot moved for rot ${d}`);
  }
  // Full circle returns it.
  const f = expectedPosition(from, pivot, { rot_deg: 360 });
  near(f.x, 10, 1e-9, '360deg x'); near(f.y, 0, 1e-9, '360deg y');
  console.log('  90/180/360 + pivot invariance');
}

console.log('scale and skew:');
{
  const pivot = { x: 5, y: 5 };
  const s2 = expectedPosition({ cx: 7, cy: 5 }, pivot, { scale: 2 });
  near(s2.x, 9, 1e-9, 'scale 2 doubles the offset');
  near(s2.y, 5, 1e-9, 'scale leaves the other axis');
  const sk = expectedPosition({ cx: 5, cy: 7 }, pivot, { skew: 0.5 });
  near(sk.x, 6, 1e-9, 'skew shifts x by skew*y');
  // THE ONES THAT MUST NOT MOVE IT. gain/bias/noise change what the pixels say,
  // not where the part is; a prediction that moved for them would reject the
  // real part on every step of a brightness sweep.
  for (const p of [{ gain: 2 }, { bias: 40 }, { noise: 30 }, { gain: 0.5, noise: 10 }]) {
    const r = expectedPosition({ cx: 7, cy: 3 }, pivot, p);
    check(Math.abs(r.x - 7) < 1e-9 && Math.abs(r.y - 3) < 1e-9,
          `${JSON.stringify(p)} moved the expected position`);
  }
  // No perturbation at all is the identity.
  for (const p of [null, undefined, {}]) {
    const r = expectedPosition({ cx: 7, cy: 3 }, pivot, p);
    check(Math.abs(r.x - 7) < 1e-9 && Math.abs(r.y - 3) < 1e-9, 'empty perturb moved it');
  }
  console.log('  scale, skew, and the four that must not move it');
}

console.log('picking the candidate:');
{
  // The numbers are the real ones: four candidates from a def on the bench, the
  // runner-up at 0.9826 against 1.0000 and 13.7 mm away.
  const poses = [
    { cx: 11.160, cy: 8.103, similarity: 1.0000 },
    { cx: 24.835, cy: 8.869, similarity: 0.9826 },
    { cx: 17.540, cy: 8.845, similarity: 0.8837 },
    { cx: 1.474, cy: 9.040, similarity: 0.8605 },
  ];
  const tol = 20 * 0.01388594;              // 20 px at the machine's scale
  check(pickByPosition(poses, { x: 11.160, y: 8.103 }, tol) === 0, 'picks the part at its own position');
  check(pickByPosition(poses, { x: 24.835, y: 8.869 }, tol) === 1, 'picks a different one when that is where we look');
  // Out by more than the tolerance -> refuse, do NOT fall back to the top score.
  check(pickByPosition(poses, { x: 11.160 + 1.0, y: 8.103 }, tol) === -1,
        'a part 1 mm away must be refused, not rounded to the top scorer');
  check(pickByPosition(poses, { x: 40, y: 40 }, tol) === -1, 'nothing near -> -1');
  // Order must not matter: identity is position, not rank.
  const shuffled = [poses[2], poses[0], poses[3], poses[1]];
  check(shuffled[pickByPosition(shuffled, { x: 11.160, y: 8.103 }, tol)].similarity === 1.0,
        'ranking order changed the answer');
  // No tolerance given -> nearest wins, which is the old behaviour made explicit.
  check(pickByPosition(poses, { x: 40, y: 40 }, undefined) >= 0, 'no tolerance should not refuse');
  // Junk must not throw or claim a match.
  for (const bad of [null, undefined, [], [{}], [{ cx: NaN, cy: 1 }]]) {
    let threw = false, r = 0;
    try { r = pickByPosition(bad, { x: 0, y: 0 }, tol); } catch { threw = true; }
    check(!threw && r === -1, `pickByPosition(${JSON.stringify(bad)}) -> ${r}`);
  }
  near(nearestDistance(poses, { x: 11.160, y: 8.103 }), 0, 1e-9, 'nearest distance at the part');
  check(Number.isNaN(nearestDistance([], { x: 0, y: 0 })), 'no candidates -> NaN, not 0');
  console.log('  6 picks + 5 junk inputs, on the measured candidate set');
}

console.log(fails ? `\nFAIL: ${fails} assertion(s)`
  : '\nPASS: the part is identified by where it must be, and a miss is refused rather than rounded');
process.exit(fails ? 1 : 0);
