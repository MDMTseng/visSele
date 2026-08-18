// The statistics-history "目前" row must read the machine's ACTUAL outlet
// wiring, not a hardcoded selector.
//
//   node hist_wiring.mjs [--url http://localhost:8081/]
//
// Which selector is the reject bin comes from conn_info (cat_ng / cat_ok),
// because guessing is how the panel once reported NG=SEL2 on a machine wired
// cat_ng 1 -- SEL2 is always zero there, so 106 real rejects showed as none.
// That was fixed in the strip's counter row and the fix did not reach the
// history modal, where the same two selectors stayed hardcoded for months. The
// row it broke is the PREVIEW of what 歸零統計 is about to archive, so the
// wrong number is precisely the one an operator checks before destroying the
// counts.
//
// The assertion is a cross-check inside the page: the strip's NG/OK tags and
// the modal's 目前 row are two renderings of one fact, so they must agree on
// both the value AND which selector it came from. Re-hardcode either and they
// diverge. True on any machine and any wiring, and needs nothing from the core.
//
// Both sides publish data-bin / data-sel / data-value, so nothing here scans
// for label text or walks the DOM by position. That matters: the first version
// of this probe looked for a div whose innerText began with 目前 and matched
// the modal container -- coupled to layout, wording and position, and wrong.
//
// Requires: core on :4090, vite, webctld. Non-destructive: it opens a modal
// and reads it, and never presses the reset button.
import { execFileSync } from 'node:child_process';
import { makeCtl, sleep } from './lib_enter.mjs';

const URL_ARG = (process.argv.find((a) => a.startsWith('--url=')) || '').split('=')[1]
             || 'http://localhost:8081/';
const { api, ev } = makeCtl();

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };

// 1. Into the Inspection UI, via the shared sequence.
console.log('[1] entering the Inspection UI');
try {
  execFileSync(process.execPath, ['enter_inspection.mjs', `--url=${URL_ARG}`],
               { cwd: import.meta.dirname, stdio: 'pipe', timeout: 240000 });
} catch (e) {
  console.log('  could not get there:', String(e.stdout || e.message).slice(-300));
  process.exit(2);
}
await sleep(2000);

// 2. The strip's counter row -- the reference rendering.
const readTags = () => ev(`[...document.querySelectorAll('[data-testid="uinsp-count"]')]
  .map(function(e){return {bin:e.dataset.bin, sel:e.dataset.sel, value:e.dataset.value};})`);
const strip = await readTags();
console.log('[2] strip counters:', JSON.stringify(strip));

const sNG = strip.find((t) => t.bin === 'NG');
const sOK = strip.find((t) => t.bin === 'OK');
if (!sNG || !sOK) {
  // No NG/OK claim at all means the wiring never arrived and the strip is
  // showing raw SEL1/2/3. Nothing to cross-check. Skip loudly rather than
  // pass: a green light for a test that could not run is worse than no test.
  console.log('\nSKIP: wiring not declared -- the strip shows raw selectors '
            + `(${strip.map((t) => t.bin).join(', ') || 'no counters'}).`);
  console.log('      Set cat_ng / cat_ok in machine_setting.json conn_info and re-run.');
  process.exit(0);
}
console.log(`    wiring: NG=${sNG.sel}  OK=${sOK.sel}`);

// 3. The history modal.
console.log('[3] opening 統計歷史');
await api('/click', { selector: '[title="統計歷史 / 歸零"]' });
await api('/wait', { selector: '[data-testid="uinsp-hist-current"]' });
await sleep(600);
const cells = await ev(`[...document.querySelectorAll('[data-testid="uinsp-hist-cell"]')]
  .map(function(e){return {bin:e.dataset.bin, sel:e.dataset.sel, value:e.dataset.value, text:e.textContent.trim()};})`);
console.log('[4] 目前 row:', JSON.stringify(cells));

const hNG = cells.find((c) => c.bin === 'NG');
const hOK = cells.find((c) => c.bin === 'OK');
if (!check(hNG && hOK, 'the 目前 row has no NG/OK cells')) process.exit(1);

// 5. Same selector, same number, from both renderings.
console.log('[5] cross-check');
check(hNG.sel === sNG.sel,
      `NG selector: strip says ${sNG.sel}, history says ${hNG.sel || '(none)'} -- history is reading a different outlet`);
check(hOK.sel === sOK.sel,
      `OK selector: strip says ${sOK.sel}, history says ${hOK.sel || '(none)'}`);
check(hNG.value === sNG.value, `NG value: strip ${sNG.value}, history ${hNG.value}`);
check(hOK.value === sOK.value, `OK value: strip ${sOK.value}, history ${hOK.value}`);

await api('/shot', { path: fails ? 'hist_wiring_fail.png' : 'hist_wiring.png' });
console.log(fails ? `\nFAIL: ${fails} assertion(s)`
                  : `\nPASS: 統計歷史 and the strip agree -- NG=${sNG.sel}, OK=${sOK.sel}`);
process.exit(fails ? 1 : 0);
