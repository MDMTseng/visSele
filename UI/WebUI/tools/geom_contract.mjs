#!/usr/bin/env node
// Does the WebUI's geometry still agree with the core's?
//
//   node tools/geom_contract.mjs
//
// The expectations in InspectionCore/test_suite/geom_vectors.json are GENERATED
// by calling the core's own functions (test_suite/geom_vectors_emit.cpp). This
// file asserts the WebUI's implementation produces the same answers.
//
// WHY A TEST AND NOT JUST ONE SHARED FILE PER SIDE
// ------------------------------------------------
// Some geometry has to exist twice. At edit time the core has not run, so the
// editor must predict what the machine will do. Putting each side in one tidy
// file makes drift reviewable; it does not make it impossible. The proof is
// what this harness was written after: on 2026-08-26 the CORRECT port of
// convert3Pts2ArcData was sitting a few dozen lines away from three wrong
// copies, in the same file, and the wrong ones seeded arc caliper width from
// the complement of the arc -- 11.00x too wide.
//
// Same file did not help. A failing test does.
//
// WHEN THIS FAILS
// ---------------
// Do not "fix" it by regenerating the vectors. The vectors are what the machine
// does. A failure means the screen and the machine have parted company, and the
// question is which one is wrong -- regenerating simply writes the disagreement
// down as the new truth.
//
// Regenerate only when the CORE's behaviour changed on purpose, and say so in
// the commit.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const VECTORS = path.resolve(here, '..', '..', '..', 'InspectionCore', 'test_suite', 'geom_vectors.json');
const MATHTOOLS = path.resolve(here, '..', 'src', 'UTIL', 'MathTools.js');

if (!fs.existsSync(VECTORS)) {
  console.error(`no vectors at ${VECTORS}\n` +
    'Generate them from the core:\n' +
    '  cd InspectionCore && ./build/geom_emit.exe > test_suite/geom_vectors.json');
  process.exit(2);
}

const { arcSweep } = await import('file:///' + MATHTOOLS.replace(/\\/g, '/'));
const spec = JSON.parse(fs.readFileSync(VECTORS, 'utf8'));
const TOL = spec.tolerance.abs;

let fails = 0, limits = 0;
const P = (a) => ({ x: a[0], y: a[1] });

// Angles are only comparable modulo a turn, and either side may land on the
// other end of the branch cut. Compare the shortest angular difference.
const angDiff = (a, b) => {
  let d = a - b;
  while (d > Math.PI) d -= 2 * Math.PI;
  while (d < -Math.PI) d += 2 * Math.PI;
  return Math.abs(d);
};

console.log(`arcSweep: ${spec.arcSweep.length} vectors from the core, tolerance ${TOL}`);
console.log(`(${spec.tolerance.why})\n`);

for (const v of spec.arcSweep) {
  const got = arcSweep(P(v.pt1), P(v.pt2), P(v.pt3));

  // Where the calipers land -- the contract that matters. Same formula as
  // caliper_locate_circle: a = angStart + span*i/(count-1).
  //
  // NOT cx/cy/r. Those are ill-conditioned: the circumcentre of three nearly
  // collinear points is something float and double genuinely disagree about,
  // measured here at 8.3 units out of 50000. Two circles that far apart at the
  // centre still pass through nearly the same points over the span in use, and
  // the points are where the boxes are drawn and the measurement is taken.
  // Testing the parameters would have reported a failure that no amount of care
  // in the JS could fix, on inputs where both sides are in fact drawing the
  // same arc.
  const N = v.anchors.length;
  let worstAnchor = 0, worstIdx = -1;
  for (let k = 0; k < N; k++) {
    const t = k / (N - 1);
    const ang = got.a0 + got.span * t;
    const x = got.x + got.r * Math.cos(ang);
    const y = got.y + got.r * Math.sin(ang);
    const d = Math.hypot(x - v.anchors[k][0], y - v.anchors[k][1]);
    if (d > worstAnchor) { worstAnchor = d; worstIdx = k; }
  }

  // A vector may carry its own tolerance, and must say why. That is how a
  // known numerical limit stays VISIBLE and BOUNDED: a permanently red test is
  // one people learn to ignore, and widening the global tolerance to cover one
  // ill-conditioned input would hide the drift this file exists to catch.
  const tol = (typeof v.tol === 'number' && v.tol > 0) ? v.tol : TOL;
  const checks = [
    ['span',   angDiff(got.span, v.span)],
    ['anchor', worstAnchor],
  ];
  const bad = checks.filter(([, d]) => !(d <= tol));
  if (bad.length) {
    fails++;
    console.log(`FAIL  ${v.name}`);
    for (const [field, d] of bad) {
      if (field === 'anchor')
        console.log(`        caliper anchor ${worstIdx} is ${d.toExponential(3)} away from where the core puts it`);
      else
        console.log(`        ${field}: core ${v[field]}  webui ${got[field]}  (off by ${d.toExponential(3)})`);
    }
  } else if (tol !== TOL) {
    limits++;
    console.log(`LIMIT ${v.name.padEnd(20)} worst anchor ${worstAnchor.toExponential(2)} (allowed ${tol})`);
    console.log(`        ${v.note}`);
  } else {
    console.log(`PASS  ${v.name.padEnd(20)} span ${got.span.toFixed(6)}  worst anchor ${worstAnchor.toExponential(2)}`);
  }
}

console.log(fails
  ? `\n${fails} of ${spec.arcSweep.length} vectors DISAGREE with the core.\n` +
    'Read the note at the top of this file before regenerating anything.'
  : `\n--- all ${spec.arcSweep.length} agree with the core ---`);
process.exit(fails ? 1 : 0);
