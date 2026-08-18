// Unit test for the pure formatting helpers. No core, no browser, no daemon.
//
//   node unit_fmt.mjs
//
// compactN exists to guarantee one property -- a counter never renders wider
// than its box -- and it shipped violating that property at exactly one input.
// The bug was invisible to spot checks because it lives on a rounding
// boundary: 99999 is under 100000, so it took the one-decimal branch and
// .toFixed(1) carried it to "100.0k", six characters. So this does not spot
// check. It walks every integer in the operating range and asserts the width
// bound on all of them, which is the only way that class of bug stays fixed.
import { compactN } from '../../src/perif/fmt.mjs';

const MAX_CHARS = 5;          // the width the sidebar tag is sized for
let fails = 0;
const check = (cond, what) => {
  if (!cond) { console.log('  FAIL ' + what); fails++; }
  return cond;
};

// 1. Exact values at every boundary the branches turn on.
console.log('boundaries:');
const cases = [
  [0, '0'], [7, '7'], [999, '999'], [1482, '1482'], [9999, '9999'],
  [10000, '10.0k'], [42700, '42.7k'], [99949, '99.9k'],
  // the regression: raw value < 100000 but rounds to 100k, must NOT be "100.0k"
  [99950, '100k'], [99999, '100k'],
  [100000, '100k'], [148200, '148k'], [900000, '900k'], [999499, '999k'],
  [999500, '1.0M'], [999999, '1.0M'], [1000000, '1.0M'], [1200000, '1.2M'],
];
for (const [n, want] of cases) {
  const got = compactN(n);
  check(got === want, `compactN(${n}) = "${got}", want "${want}"`);
}
console.log(`  ${cases.length} boundary cases`);

// 2. The width bound, over the whole operating range. This is the property the
//    function exists for; the boundary list above only documents where it bends.
console.log('width sweep 0..1,000,000:');
let widest = 0, widestAt = 0;
for (let i = 0; i <= 1000000; i++) {
  const L = compactN(i).length;
  if (L > widest) { widest = L; widestAt = i; }
}
check(widest <= MAX_CHARS,
      `widest rendering is ${widest} chars at ${widestAt} ("${compactN(widestAt)}"), limit ${MAX_CHARS}`);
console.log(`  widest = ${widest} chars at ${widestAt} ("${compactN(widestAt)}")`);

// 3. Monotonic: a bigger count must never render as a smaller number. A
//    formatter that folds 148200 to "1482" is the failure being guarded
//    against, and it would pass a width check.
console.log('monotonicity (sampled):');
const val = (s) => {
  const m = /^(-?[\d.]+)([kM])?$/.exec(s);
  if (!m) return NaN;
  return parseFloat(m[1]) * (m[2] === 'M' ? 1e6 : m[2] === 'k' ? 1e3 : 1);
};
let prev = -1, mono = true, monoAt = null;
for (let i = 0; i <= 1000000; i += 97) {     // prime step: crosses every branch
  const v = val(compactN(i));
  if (!(v >= prev)) { mono = false; monoAt = i; break; }
  prev = v;
}
check(mono, `rendering went backwards at ${monoAt} ("${compactN(monoAt)}")`);
console.log(`  ${mono ? 'non-decreasing across 10310 samples' : 'BROKEN'}`);

// 4. Non-numbers must not render as a number.
console.log('non-numeric:');
for (const bad of [undefined, null, NaN, Infinity, -Infinity, '5', {}]) {
  const got = compactN(bad);
  check(got === '—', `compactN(${String(bad)}) = "${got}", want "—"`);
}
console.log('  undefined/null/NaN/Infinity/string/object -> "—"');

console.log(fails ? `\nFAIL: ${fails} assertion(s)` : '\nPASS: compactN holds the 5-char bound over 0..1e6');
process.exit(fails ? 1 : 0);
