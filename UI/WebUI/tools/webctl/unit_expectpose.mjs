// Unit test for the expected-position arithmetic. No core, no browser.
//
//   node unit_expectpose.mjs
//
// This is what replaces "take the highest-scoring candidate" in the studio's
// test and sweep. If it is wrong the studio does not fail loudly -- it rejects
// the real part and reports "no candidate near the expected position", which
// reads exactly like a locator that stopped working. So the arithmetic gets a
// test rather than a careful read.
import { imageCentreInObjectFrame, expectedPosition, pickByPosition, nearestDistance }
  from '../../src/sbmExpectPose.mjs';

let fails = 0;
const check = (c, w) => { if (!c) { console.log('  FAIL ' + w); fails++; } return c; };
const near = (a, b, tol, w) => check(Math.abs(a - b) <= tol, `${w}: ${a} vs ${b}`);

const MMPP = 0.0125, W = 2448, H = 2048;

console.log('image centre in the object frame:');
{
  // Origin at the image centre, no rotation -> the pivot is the origin.
  const p = imageCentreInObjectFrame(W, H, MMPP, { cx: W / 2 * MMPP, cy: H / 2 * MMPP, angle: 0 });
  near(p.x, 0, 1e-9, 'centred origin gives pivot x 0');
  near(p.y, 0, 1e-9, 'centred origin gives pivot y 0');
  // Origin at the image corner -> the pivot is half the frame away.
  const q = imageCentreInObjectFrame(W, H, MMPP, { cx: 0, cy: 0, angle: 0 });
  near(q.x, W / 2 * MMPP, 1e-9, 'corner origin, pivot x');
  near(q.y, H / 2 * MMPP, 1e-9, 'corner origin, pivot y');
  // A rotated object frame rotates the pivot INTO it. 90 degrees swaps the axes.
  const r = imageCentreInObjectFrame(W, H, MMPP, { cx: 0, cy: 0, angle: Math.PI / 2 });
  near(r.x, H / 2 * MMPP, 1e-9, 'rotated frame, pivot x');
  near(r.y, -(W / 2 * MMPP), 1e-9, 'rotated frame, pivot y');
  console.log('  3 registrations');
}

console.log('rotation moves the part around the pivot:');
{
  const pivot = { x: 0, y: 0 };
  const from = { cx: 10, cy: 0 };
  const a = expectedPosition(from, pivot, { rot_deg: 90 });
  near(a.x, 0, 1e-9, '90deg x'); near(a.y, 10, 1e-9, '90deg y');
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
