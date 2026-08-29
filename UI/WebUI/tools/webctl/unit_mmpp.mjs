// Unit test for the mm/px selection rule. No core, no browser, no daemon.
//
//   node unit_mmpp.mjs
//
// Every dimension in a recipe is px * mmpp. A wrong value here does not produce
// a visible fault -- it produces a whole def measured to a consistent,
// plausible, WRONG scale, on a screen where nothing looks different. That is
// the entire reason the ordering is a named function with a test instead of a
// chain of || inside a class method.
//
// The bug this pins: TAKE captured a new part, Def_Retake did not clear
// _obj.sig360info, and the ordering read that first -- so the new object kept
// the PREVIOUS def's scale. The single-shot path was rescued by accident (its
// reply carries a sig360 report that overwrote the stale value); the streaming
// path runs stage_light_report with IMG_ignore_calib precisely so it does no
// measurement, produces no report, and was silently wrong.
import { pickMmpp, mmppFromLensCalib } from '../../src/UTIL/mmppRule.mjs';

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };
const eq = (got, want, what) =>
  check(got === want || (Math.abs(got - want) < 1e-12), `${what}: got ${got}, want ${want}`);

const CP = { mmpb2b: 4, ppb2b: 100 };      // -> 0.04
const SIG = 0.0123, INST = 0.0456;

// ── the ordering ────────────────────────────────────────────────────────────
console.log('ordering (sig360 > instrument > cam_param > 1):');
eq(pickMmpp({ sigMmpp: SIG, instrumentMmpp: INST, camParam: CP }), SIG,
   'a real sig360 measurement wins over everything');
eq(pickMmpp({ sigMmpp: 1, instrumentMmpp: INST, camParam: CP }), INST,
   'sig360 == 1 means ABSENT, so the instrument wins');
eq(pickMmpp({ instrumentMmpp: INST, camParam: CP }), INST,
   'no sig360 at all -> the instrument');
eq(pickMmpp({ sigMmpp: 1, camParam: CP }), 0.04,
   'no instrument -> cam_param');
eq(pickMmpp({ sigMmpp: 1 }), 1, 'nothing at all -> 1');
eq(pickMmpp({}), 1, 'empty input -> 1');
eq(pickMmpp(), 1, 'no input at all -> 1');
console.log('  7 orderings');

// THE REGRESSION, stated as the scenario rather than as values: a camera frame
// must NOT be measured with the scale of the def that happened to be open.
console.log('a camera frame does not inherit the previous def\'s scale:');
{
  const prevDefScale = 0.0123;          // whatever def was loaded before TAKE
  const machineScale = 0.0456;          // this machine's lens calibration
  // After TAKE from the camera: Instrument_Mmpp_Set nulls sig360info AND sets
  // the instrument value. Both halves matter -- the null is what stops the old
  // number winning, and without it the value just written is never read.
  eq(pickMmpp({ sigMmpp: 1, instrumentMmpp: machineScale }), machineScale,
     'with sig360 cleared, the machine scale is used');
  // The half-done version: instrument set, stale signature left in place.
  check(pickMmpp({ sigMmpp: prevDefScale, instrumentMmpp: machineScale }) === prevDefScale,
     'documents WHY sig360info must be nulled: leaving it makes the write invisible');
  console.log('  both halves of Instrument_Mmpp_Set are load-bearing');
}

// Reusing the def's own image keeps the def's scale -- the picture really does
// belong to it.
console.log('reusing the def image keeps the def scale:');
eq(pickMmpp({ sigMmpp: SIG }), SIG, 'no instrument value is set on that path');
console.log('  ok');

// ── garbage in ──────────────────────────────────────────────────────────────
console.log('rejects values that are not a scale:');
for (const bad of [0, -1, NaN, Infinity, -Infinity, undefined, null, '0.05', {}]) {
  eq(pickMmpp({ sigMmpp: bad, instrumentMmpp: bad, camParam: CP }), 0.04,
     `sig/instrument = ${String(bad)} falls through to cam_param`);
}
// A cam_param that cannot divide must not produce Infinity or NaN.
for (const cp of [{ mmpb2b: 4, ppb2b: 0 }, { mmpb2b: 4 }, { ppb2b: 100 },
                  { mmpb2b: NaN, ppb2b: 100 }, {}, null, 'nope']) {
  const got = pickMmpp({ camParam: cp });
  check(Number.isFinite(got) && got > 0, `cam_param ${JSON.stringify(cp)} -> ${got}`);
}
console.log('  9 bad scales + 7 bad cam_params');

// THE PROPERTY: the result is always a usable positive finite number. A NaN
// here would make every measurement NaN, which at least fails loudly -- an
// Infinity or a 0 would not.
console.log('always finite and positive:');
let bad = null;
const vals = [undefined, null, NaN, 0, -1, 1, 0.02, Infinity, '3'];
for (const s of vals) for (const i of vals) for (const c of [CP, {}, null]) {
  const got = pickMmpp({ sigMmpp: s, instrumentMmpp: i, camParam: c });
  if (!(Number.isFinite(got) && got > 0)) { bad = { s, i, c, got }; break; }
}
check(!bad, `produced ${bad && bad.got} for ${JSON.stringify(bad)}`);
console.log(`  ${bad ? 'BROKEN' : 'held over 243 combinations'}`);

// ── lens_calib.json parsing ─────────────────────────────────────────────────
console.log('mmppFromLensCalib:');
eq(mmppFromLensCalib({ um_per_px: 12.5 }), 0.0125, 'um_per_px is micrometres');
eq(mmppFromLensCalib({ m: 80 }), 0.0125, 'm is px/mm, so inverted');
eq(mmppFromLensCalib({ um_per_px: 12.5, m: 999 }), 0.0125, 'um_per_px wins over m');
eq(mmppFromLensCalib({ um_per_px: '12.5' }), 0.0125, 'strings are coerced (JSON from disk)');
// Anything it cannot read must be undefined, NOT a plausible default. The
// caller warns the operator instead; a def with no honest scale is worse than
// one that cannot measure yet.
for (const bad2 of [null, undefined, {}, { um_per_px: 0 }, { um_per_px: -3 },
                    { m: 0 }, { um_per_px: 'x' }, { um_per_px: NaN }]) {
  check(mmppFromLensCalib(bad2) === undefined,
        `mmppFromLensCalib(${JSON.stringify(bad2)}) should be undefined`);
}
console.log('  2 formats + 8 unreadable cases -> undefined, never a guess');

console.log(fails ? `\nFAIL: ${fails} assertion(s)` : '\nPASS: scale follows the picture, and is never silently substituted');
process.exit(fails ? 1 : 0);
