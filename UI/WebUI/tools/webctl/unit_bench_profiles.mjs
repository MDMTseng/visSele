// The scale rule, run against several BENCHES instead of just this one.
//
//   node unit_bench_profiles.mjs
//
// WHY THESE EXIST.
//
// Every dimension a recipe records is pixels times mm/px, so the scale is the
// one setting that cannot be checked by looking at the screen: a def measured
// at the wrong mmpp is self-consistent, plausible, and wrong by a constant
// factor. It is also the setting that differs most between machines -- the lens
// on this bench is not the lens on the line -- and the code that picks it has
// four branches, of which this bench only ever exercises one.
//
// So each folder in fixtures/benches is one machine's instrument setup: the
// lens_calib.json it would have, and an expect.json saying what a def opened on
// that machine must measure in. The profiles are DATA, so adding a machine is
// adding a folder, and the expected numbers sit next to the settings that
// produce them rather than inside the test.
//
// The same folders drive the live journey: JOURNEY_BENCH=<name> asserts the
// running studio reports that bench's mmpp (see journey_new_object.mjs), which
// is the half this file cannot cover -- here the rule is exercised, there the
// wiring that feeds it.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { pickMmpp, mmppFromLensCalib } from '../../src/UTIL/mmppRule.mjs';

const DIR = path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'benches');
const near = (a, b) => (a == null || b == null) ? a === b : Math.abs(a - b) < 1e-12;

let fails = 0;
const check = (c, w) => { if (!c) { console.log('  FAIL ' + w); fails++; } return c; };

const benches = fs.readdirSync(DIR).filter((d) =>
  fs.existsSync(path.join(DIR, d, 'expect.json')));
check(benches.length >= 4, `expected several bench profiles, found ${benches.length}`);

for (const name of benches) {
  const p = (f) => JSON.parse(fs.readFileSync(path.join(DIR, name, f), 'utf8'));
  const calib = p('lens_calib.json'), want = p('expect.json');
  console.log(`\n${name}: ${want.why}`);

  // 1. the instrument's own number, read from the file the core writes.
  const inst = mmppFromLensCalib(calib);
  check(near(inst ?? null, want.instrumentMmpp ?? null),
        `instrument mmpp: got ${inst}, expected ${want.instrumentMmpp}`);
  console.log(`  instrument mm/px = ${inst === undefined ? 'none (uncalibrated)' : inst}`);

  // 2. a def with no signature -- every def that came from TAKE. The
  //    instrument decides, and an uncalibrated machine must NOT be handed a
  //    plausible-looking 1: it falls through to the def's own cam_param.
  const noSig = pickMmpp({ sigMmpp: 1, instrumentMmpp: inst });
  if (want.editorMmpp_noSignature == null) {
    check(noSig === 1, `uncalibrated machine invented a scale: ${noSig}`);
    // ...and with a cam_param present, that is what it uses, rather than 1.
    const viaCam = pickMmpp({ sigMmpp: 1, instrumentMmpp: inst,
                              camParam: { mmpb2b: 3, ppb2b: 300 } });
    check(near(viaCam, 0.01), `cam_param fallback: got ${viaCam}`);
    console.log('  no calibration -> falls through to cam_param, never a guess');
  } else {
    check(near(noSig, want.editorMmpp_noSignature),
          `new-object mmpp: got ${noSig}, expected ${want.editorMmpp_noSignature}`);
    console.log(`  a def taken on this machine measures at ${noSig}`);
  }

  // 3. a def that DOES carry a signature: its own measurement of its own
  //    picture beats the instrument, on every bench. This is what stops
  //    opening an old recipe on a different machine from silently rescaling it.
  const withSig = pickMmpp({ sigMmpp: want.editorMmpp_withSignature, instrumentMmpp: inst });
  check(near(withSig, want.editorMmpp_withSignature),
        `signature must win: got ${withSig}`);
  console.log(`  an existing recipe keeps its own ${withSig}, not the machine's`);
}

// The property that ties them together: two benches with different lenses must
// disagree, or none of the above is actually testing anything.
{
  const m = (n) => mmppFromLensCalib(
    JSON.parse(fs.readFileSync(path.join(DIR, n, 'lens_calib.json'), 'utf8')));
  const a = m('this_bench_telecentric'), b = m('fine_5um');
  check(a && b && Math.abs(a - b) > 1e-6, 'two different lenses produced the same scale');
  console.log(`\ndifferent lenses, different scale: ${a} vs ${b}`);
}

console.log(fails ? `\nFAIL: ${fails} assertion(s)`
  : '\nPASS: the scale follows the instrument, the signature still wins, '
    + 'and an uncalibrated machine never invents one');
process.exit(fails ? 1 : 0);
