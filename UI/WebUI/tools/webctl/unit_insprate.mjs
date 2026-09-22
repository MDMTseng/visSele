// Guard: one inspection frame rate, not two. No core, no browser.
//
//   node unit_insprate.mjs
//
// InspectionUI asked the camera for 10 fps in CI and 快速驗證 asked for 8. The
// difference had no author -- nobody chose 8; it is what the other screen
// happened to say when it was copied. It surfaced as "快速驗證 shows images
// faster than InspectionUI", which sent us through the core's throttle, the
// snapshot policy and the streaming ceiling before the two literals turned out
// to be the whole of it.
//
// So the numbers live in one module, and this checks two things: the module
// says what it should, and NOTHING ELSE names a frame rate for an inspection.
// The second half is the one that matters -- a module only unifies while it is
// the only writer.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { CI_FRAME_RATE, FI_FRAME_RATE, inspFrameRate, applyInspFrameRate }
  from '../../src/UTIL/inspRatePolicy.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const SRC = path.join(here, '..', '..', 'src');
let fails = 0;
const check = (c, w) => { if (!c) { console.log('  FAIL ' + w); fails++; } return c; };

console.log('the policy:');
check(CI_FRAME_RATE === 10, `CI is ${CI_FRAME_RATE}, expected 10`);
check(FI_FRAME_RATE > 1000, `FI should be an unreachable ceiling, got ${FI_FRAME_RATE}`);
check(inspFrameRate('FI') === FI_FRAME_RATE, 'FI maps to the FI rate');
check(inspFrameRate('CI') === CI_FRAME_RATE, 'CI maps to the CI rate');
// Anything unrecognised must NOT get production speed. A screen that cannot say
// which mode it is in is the last thing that should be asking for it.
for (const m of [undefined, null, '', 'fi', 'ci', 'XX', 0, {}]) {
  check(inspFrameRate(m) === CI_FRAME_RATE, `inspFrameRate(${JSON.stringify(m)}) should fall back to CI`);
}
console.log('  4 + 8 fallbacks');

console.log('applying it:');
{
  let got = null;
  const ok = applyInspFrameRate({ setCameraFrameRate: (v) => { got = v; } }, 'FI');
  check(ok === true && got === FI_FRAME_RATE, `apply FI set ${got}`);
  // A caller without the method must be refused, not crash: these are passed a
  // CameraTransferCtrl that may not exist yet on a screen still mounting.
  for (const bad of [undefined, null, {}, { setCameraFrameRate: 5 }]) {
    let threw = false, r = true;
    try { r = applyInspFrameRate(bad, 'CI'); } catch { threw = true; }
    check(!threw && r === false, `applyInspFrameRate(${JSON.stringify(bad)}) -> threw=${threw} r=${r}`);
  }
  console.log('  1 apply + 4 bad handles');
}

// THE GUARD. Any other place that sets an inspection frame rate is a second
// definition, and second definitions drift silently -- that is the entire
// history of this file.
console.log('nothing else names an inspection frame rate:');
{
  const ALLOW = new Set(['UTIL/inspRatePolicy.mjs', 'UTIL/BPG_Protocol.js']);
  const offenders = [];
  const walk = (dir) => {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      const p = path.join(dir, e.name);
      if (e.isDirectory()) { if (e.name !== 'node_modules') walk(p); continue; }
      if (!/\.(js|jsx|mjs)$/.test(e.name)) continue;
      const rel = path.relative(SRC, p).split(String.fromCharCode(92)).join('/');
      if (ALLOW.has(rel)) continue;
      const src = fs.readFileSync(p, 'utf8');
      for (const [i, line] of src.split('\n').entries()) {
        if (/^\s*(\/\/|\*)/.test(line)) continue;           // comments may discuss it
        if (/setCameraFrameRate\s*\(|setCameraSpeed_(HIGHEST|HIGH|LOW)\s*\(/.test(line))
          offenders.push(`${rel}:${i + 1}  ${line.trim().slice(0, 80)}`);
      }
    }
  };
  walk(SRC);
  check(offenders.length === 0,
    'these set a camera rate outside the policy:\n    ' + offenders.join('\n    '));
  console.log(`  ${offenders.length === 0 ? 'clean' : offenders.length + ' offender(s)'}`);
}

console.log(fails ? `\nFAIL: ${fails} assertion(s)`
  : '\nPASS: one inspection frame rate, and only one place sets it');
process.exit(fails ? 1 : 0);
