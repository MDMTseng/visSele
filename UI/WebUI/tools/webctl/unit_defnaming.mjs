// Unit test for def path/name arithmetic. No core, no browser, no daemon.
//
//   node unit_defnaming.mjs
//
// Both functions here shipped broken, and neither break was visible:
//
//   refPngPathOf  cut the path at the wrong dot, so a machine installed under
//                 a folder called `X2.0` looked for its template at
//                 <root>/X2.png. The core could not read it, SBM training
//                 failed, and the def ran on sig360 while the studio said "no
//                 features extracted" -- two unrelated-looking symptoms, one
//                 string. Nothing about the def, the image or the settings was
//                 wrong; only the path was, and only on machines whose install
//                 folder had a dot in it.
//
//   nextFreeName  is what stops a newly created object being offered the file
//                 name of the recipe it was created from. A collision here is
//                 one careless save away from overwriting a def that is
//                 running on a line.
//
// So this does not spot check the happy path. It walks the shapes a real path
// can take -- both separators, dots in directories, no directory at all,
// dotfiles, no extension -- because that is the space the bug lived in.
import { refPngPathOf, nextFreeName, takenNamesFrom } from '../../src/UTIL/defNaming.mjs';

const B = String.fromCharCode(92);   // backslash, kept out of the literals below
let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };
const eq = (got, want, what) => check(got === want, `${what}: got "${got}", want "${want}"`);

// ── refPngPathOf ────────────────────────────────────────────────────────────
console.log('refPngPathOf:');
const cases = [
  // THE REGRESSION. defModelPath carries no extension, so there is nothing to
  // strip; the old regex ate backwards into the directory called X2.0.
  ['C:' + B + 'Users' + B + 'SLID005' + B + 'Desktop' + B + 'X2.0' + B + 'data' + B + 'testNew2',
   'C:' + B + 'Users' + B + 'SLID005' + B + 'Desktop' + B + 'X2.0' + B + 'data' + B + 'testNew2.png'],
  // same shape, forward slashes
  ['/opt/X2.0/data/testNew2', '/opt/X2.0/data/testNew2.png'],
  // WITH an extension, dotted directory: the extension goes, the directory stays
  ['C:' + B + 'X2.0' + B + 'data' + B + 'testNew2.hydef', 'C:' + B + 'X2.0' + B + 'data' + B + 'testNew2.png'],
  ['/opt/X2.0/data/testNew2.hydef', '/opt/X2.0/data/testNew2.png'],
  // ordinary cases
  ['data/testNew2', 'data/testNew2.png'],
  ['data/testNew2.hydef', 'data/testNew2.png'],
  ['testNew2', 'testNew2.png'],
  ['testNew2.hydef', 'testNew2.png'],
  // no directory, dotted name
  ['v1.2.3', 'v1.2.png'],
  // a dotfile keeps its name (dot at index 0 is not an extension)
  ['data/.hidden', 'data/.hidden.png'],
  ['/a/.hidden', '/a/.hidden.png'],
  // mixed separators, which the LD payloads really do contain
  ['C:' + B + 'X2.0' + B + 'data' + B + '/testNew2', 'C:' + B + 'X2.0' + B + 'data' + B + '/testNew2.png'],
  // trailing separator: no name at all, but it must not reach into the parent
  ['data/', 'data/.png'],
];
for (const [inp, want] of cases) eq(refPngPathOf(inp), want, 'refPngPathOf(' + inp + ')');
console.log(`  ${cases.length} path shapes`);

// The property the whole function exists for: the DIRECTORY is never touched.
console.log('directory is never modified:');
let dirOk = true, dirAt = null;
for (const sep of ['/', B]) {
  for (const dir of ['a', 'a.b', 'a.b.c', 'X2.0', 'v1.0' + sep + 'sub.dir', '.cfg']) {
    for (const base of ['name', 'name.hydef', 'name.a.b', '.hidden']) {
      const p = dir + sep + base;
      const got = refPngPathOf(p);
      if (!got.startsWith(dir + sep)) { dirOk = false; dirAt = p; break; }
    }
  }
}
check(dirOk, `the directory was rewritten for "${dirAt}"`);
console.log(`  ${dirOk ? 'held over 48 dir/base/separator combinations' : 'BROKEN'}`);

// And it always ends in exactly one .png.
console.log('always exactly one .png:');
let extOk = true, extAt = null;
for (const [inp] of cases) {
  const got = refPngPathOf(inp);
  if (!got.endsWith('.png') || got.endsWith('.png.png')) { extOk = false; extAt = inp; break; }
}
check(extOk, `bad extension for "${extAt}"`);
console.log(`  ${extOk ? 'ok' : 'BROKEN'}`);

// ── nextFreeName ────────────────────────────────────────────────────────────
console.log('nextFreeName:');
const S = (...n) => new Set(n.map((x) => x.toLowerCase()));
eq(nextFreeName('HY-1234', S()), 'HY-1234', 'free name is used as-is');
eq(nextFreeName('HY-1234', S('hy-1234')), 'HY-1234[1]', 'first collision');
eq(nextFreeName('HY-1234', S('hy-1234', 'hy-1234[1]')), 'HY-1234[2]', 'second collision');
eq(nextFreeName('HY-1234', S('hy-1234', 'hy-1234[2]')), 'HY-1234[1]', 'fills the first gap');
// Case-insensitive, because Windows file names are.
eq(nextFreeName('HY-1234', S('HY-1234')), 'HY-1234[1]', 'collision is case-insensitive');
eq(nextFreeName('hy-1234', S('HY-1234')), 'hy-1234[1]', 'and both ways round');
// Illegal characters are replaced, not dropped -- dropping can MERGE two
// different part numbers into one name.
eq(nextFreeName('a/b', S()), 'a_b', 'separator is replaced');
eq(nextFreeName('a:b*c?', S()), 'a_b_c_', 'every illegal char is replaced');
check(nextFreeName('a/b', S()) !== nextFreeName('a' + B + 'b', S('a_b')),
      'two different raw names must not collapse onto one taken name');
// Empty / whitespace / nullish must still produce something usable.
for (const bad of ['', '   ', null, undefined]) {
  const got = nextFreeName(bad, S());
  check(typeof got === 'string' && got.length > 0, `nextFreeName(${String(bad)}) produced "${got}"`);
}
eq(nextFreeName('', S()), 'Sample', 'empty falls back to Sample');
eq(nextFreeName('  HY-9  ', S()), 'HY-9', 'surrounding space is trimmed');

console.log('  15 naming cases');

// THE PROPERTY: whatever it returns is not in the taken set. Swept, because a
// single returned collision is a def overwritten.
console.log('never returns a taken name:');
let collided = null;
for (let n = 0; n < 60 && !collided; n++) {
  const taken = new Set();
  taken.add('part');
  for (let i = 1; i <= n; i++) taken.add(('part[' + i + ']').toLowerCase());
  const got = nextFreeName('Part', taken);
  if (taken.has(got.toLowerCase())) collided = { n, got };
}
check(!collided, `collided at ${collided && collided.n} -> "${collided && collided.got}"`);
console.log(`  ${collided ? 'BROKEN' : 'held for 0..59 consecutive suffixes'}`);

// Exhausted range: must still not collide.
{
  const taken = new Set(['part']);
  for (let i = 1; i < 999; i++) taken.add(('part[' + i + ']').toLowerCase());
  const got = nextFreeName('Part', taken);
  check(!taken.has(got.toLowerCase()), `exhausted range returned a taken name "${got}"`);
  console.log('  exhausted range still returns something free');
}

// ── takenNamesFrom ──────────────────────────────────────────────────────────
console.log('takenNamesFrom:');
{
  const set = takenNamesFrom([
    'data/one.hydef',
    { name: 'Two.HYDEF' },
    { path: 'C:' + B + 'x' + B + 'three.png' },
    'four',
    null, undefined, {}, 42,
  ]);
  for (const want of ['one', 'two', 'three', 'four'])
    check(set.has(want), `"${want}" missing from the taken set`);
  check(!set.has(''), 'an empty name leaked into the taken set');
  console.log(`  ${set.size} names from 8 mixed entries, junk ignored`);
}
// A listing it cannot read must mean "nothing is taken", never a throw: this
// runs inside a capture, and a listing failure must not block one.
for (const bad of [null, undefined, [], 'not-a-list', 42, {}]) {
  let threw = false;
  try { takenNamesFrom(bad); } catch (e) { threw = true; }
  check(!threw, `takenNamesFrom(${String(bad)}) threw`);
}
console.log('  unreadable listings degrade to empty, never throw');

console.log(fails ? `\nFAIL: ${fails} assertion(s)` : '\nPASS: def paths keep their directory and new names cannot collide');
process.exit(fails ? 1 : 0);
