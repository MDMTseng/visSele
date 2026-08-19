// Everything that can be tested with NO camera and NO board.
//
//   node suite_nohw.mjs            run it
//   node suite_nohw.mjs --list     just print the plan
//
// Needs: a core on :4090, vite on :8081, webctld on :8765. The core starts
// fine with nothing attached -- CameraLayerManager falls back to BMP_carousel
// and the WS comes up -- which is what makes most of this reachable.
//
// Three outcomes, kept distinct on purpose:
//
//   PASS   the probe ran and asserted
//   SKIP   the probe declined for a stated reason (no wiring declared, a
//          fixture that cannot exercise the case). Exit 0, and it is NOT a
//          pass -- the reason is printed every time.
//   NEEDS  the probe cannot run in this configuration at all. Listed, never
//          executed, so a hardware-only gap stays visible instead of being
//          quietly dropped from the count.
//
// The distinction is the whole point. A suite that reports "all green" while
// a third of it never ran is worse than one that reports two thirds, because
// only the second tells you what you still do not know.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dir = path.dirname(fileURLToPath(import.meta.url));
const LIST_ONLY = process.argv.includes('--list');

// Preconditions, checked BEFORE anything runs.
//
// The first version of this file did not, and reported five FAILs that were
// all "ECONNREFUSED 8765" -- webctld simply was not running. Five red lines
// that say nothing about the code under test are worse than one that says the
// harness is not up: they cost a debugging session before anyone reads the
// stack trace.
const PORTS = [
  [4090, 'core (visSele)'],
  [8081, 'vite dev server'],
  [8765, 'webctld'],
];
const portOpen = (p) => new Promise((res) => {
  const s = net.connect({ host: '127.0.0.1', port: p });
  const done = (v) => { s.destroy(); res(v); };
  s.on('connect', () => done(true));
  s.on('error', () => done(false));
  setTimeout(() => done(false), 1500);
});

// The BMP carousel is what stands in for a camera when nothing is attached,
// and it reads a folder that is inside gitignored data/. Materialise it from
// the checked-in fixture rather than assuming somebody left one behind -- the
// probes that need a live stream all failed with "observer got no stream"
// after that folder was cleaned up, and nothing said why.
const CAROUSEL_SRC = path.join(dir, 'fixtures', 'carousel');
const CAROUSEL_DST = path.resolve(dir, '..', '..', '..', '..',
                                  'InspectionCore', 'Core0_1', 'data', 'BMP_carousel_test');
function ensureCarousel() {
  if (!fs.existsSync(CAROUSEL_SRC)) return 'no fixture folder';
  const want = fs.readdirSync(CAROUSEL_SRC).filter((f) => /\.(png|bmp)$/i.test(f));
  if (!want.length) return 'fixture folder is empty';
  fs.mkdirSync(CAROUSEL_DST, { recursive: true });
  let copied = 0;
  for (const f of want) {
    const dst = path.join(CAROUSEL_DST, f);
    if (!fs.existsSync(dst)) { fs.copyFileSync(path.join(CAROUSEL_SRC, f), dst); copied++; }
  }
  return `${want.length} frame(s)${copied ? `, ${copied} copied` : ''}`;
}

// Verified runnable with nothing attached, 2026-08-18.
const RUNNABLE = [
  ['unit_fmt.mjs',              [],                    60,  'compactN width bound, swept 0..1e6'],
  ['unit_no_hardcoded_sel.mjs', [],                    60,  'no NG/OK claim names a selector'],
  ['bpg_sweep.mjs',             ['--include-crashers'],300, '35 protocol cases: valid, malformed, framing abuse, crashers'],
  ['doorbell.mjs',              [],                    120, 'state doorbells: suppression, RC triplet, perif transitions'],
  ['fd_leak.mjs',               [],                    120, 'failed TCP CONNECTs leak no fds'],
  ['slow_client.mjs',           [],                    180, 'a paused subscriber must not wedge the WS for others'],
  ['enter_inspection.mjs',      [],                    300, 'cold page -> recipe -> Inspection UI'],
  ['hist_wiring.mjs',           [],                    400, 'history 目前 row reads the wiring, not a selector'],
  ['play_readiness.mjs',        [],                    300, 'play readiness == AND of every rendered tag group'],
  ['station_probe.mjs',         [],                    120, 'station block: region, clean_regions, bypass, ignore_calib'],
  ['churn.mjs',                 [],                    240, 'WS teardown under fire: 90 clients destroyed mid-stream'],
  ['flows.mjs',                 ['verify'],            900, '9 editor + inspection flows vs baseline'],
  ['cycle.mjs',                 ['1'],                 300, 'one lap of the operator day; def hash stable'],
];

// Cannot run here, and why. Kept in the file so the gap is a line of output
// rather than an absence.
const NEEDS = [
  ['qwatch.mjs',          'a sustained load to watch; the carousel alone is not one'],
  ['dv_bench.mjs',        'an image stream to measure'],
  ['soak.mjs',            'a CI stream'],
  ['link_fault.mjs',      'a real board (tx_fail -> suspect -> reopen)'],
  ['phantom_feed.mjs',    'a real board (trig_phantom_pulse over console 4099)'],
  ['pulse_load.mjs',      'a real board (CAM1 hardware triggers)'],
  ['rc_hammer.mjs',       'a real camera (camera_ez_reconnect lifecycle)'],
  ['calib_sticky.mjs',    'CI sessions against a def+image pair; worth revisiting now the carousel has frames'],
];

if (LIST_ONLY) {
  console.log('RUNNABLE with no hardware:');
  RUNNABLE.forEach(([f, a, , why]) => console.log(`  ${[f, ...a].join(' ').padEnd(34)} ${why}`));
  console.log('\nNEEDS more than this machine has:');
  NEEDS.forEach(([f, why]) => console.log(`  ${f.padEnd(34)} ${why}`));
  process.exit(0);
}

console.log('carousel frames:', ensureCarousel());
let missing = [];
for (const [p, name] of PORTS) if (!(await portOpen(p))) missing.push(`${name} on :${p}`);
if (missing.length) {
  console.error('\nNOT RUN -- these are not up:\n  ' + missing.join('\n  '));
  console.error('\nStart them and re-run:');
  console.error('  cd InspectionCore/Core0_1 && ../dist/win/visSele.exe');
  console.error('  cd UI/WebUI && npm run dev');
  console.error('  cd UI/WebUI/tools/webctl && WEBCTL_URL=http://localhost:8081 WEBCTL_HEADLESS=1 node webctld.mjs');
  process.exit(2);   // 2 = could not run, distinct from 1 = something failed
}
console.log('preconditions OK (core, vite, webctld)\n');

const rows = [];
for (const [file, args, secs, why] of RUNNABLE) {
  process.stdout.write(`${[file, ...args].join(' ').padEnd(34)} `);
  const t0 = Date.now();
  const r = spawnSync(process.execPath, [file, ...args], {
    cwd: dir, encoding: 'utf8', timeout: secs * 1000,
  });
  const ms = Date.now() - t0;
  const out = (r.stdout || '') + (r.stderr || '');
  // A probe that declined is not a probe that passed. Both exit 0, so the
  // difference has to come from what it said.
  const skipped = /^SKIP\b/m.test(out) || /\bSKIP:/.test(out);
  const state = r.status === 0 ? (skipped ? 'SKIP' : 'PASS')
              : r.signal || r.error ? 'ERROR' : 'FAIL';
  rows.push({ file, state, ms, why, out });
  const note = state === 'SKIP'
    ? '  ' + ((out.match(/SKIP[:.]?\s*(.+)/) || [])[1] || '').slice(0, 70)
    : '';
  console.log(`${state}  ${(ms / 1000).toFixed(0)}s${note}`);
}

const n = (s) => rows.filter((r) => r.state === s).length;
console.log(`\n${n('PASS')} pass, ${n('SKIP')} skip, ${n('FAIL') + n('ERROR')} fail, of ${rows.length} runnable`);
console.log(`${NEEDS.length} more need hardware or a live stream and were NOT run:`);
NEEDS.forEach(([f, w]) => console.log(`  ${f.padEnd(24)} ${w}`));

for (const r of rows.filter((x) => x.state === 'FAIL' || x.state === 'ERROR')) {
  console.log(`\n--- ${r.file} (${r.state}) ---`);
  console.log(r.out.split('\n').slice(-12).join('\n'));
}
process.exit(n('FAIL') + n('ERROR') ? 1 : 0);
