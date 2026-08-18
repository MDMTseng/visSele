// The statistics-history "目前" row must read the machine's ACTUAL outlet
// wiring, not a hardcoded selector.
//
//   node hist_wiring.mjs [--url http://localhost:8081/]
//
// Why this exists. The panel resolves which selector is the reject bin and
// which is the pass bin from conn_info (cat_ng / cat_ok), because guessing is
// how it once reported NG=SEL2 on a machine wired cat_ng 1 -- SEL2 is always
// zero there, so 106 real rejects showed as none. That was fixed in the strip's
// counter row and the fix did not reach the history modal, where the same two
// selectors stayed hardcoded for months. The row it broke is the PREVIEW of
// what "歸零統計" is about to archive, so the wrong number is precisely the one
// an operator checks before destroying the counts.
//
// The assertion is a cross-check inside the page, not a comparison against a
// recorded number: the strip's own NG/OK tags already read the wiring
// correctly, so the modal must agree with them. Re-hardcode either one and the
// two diverge. That holds on any machine and any wiring, and needs nothing
// from the core.
//
// Requires: core on :4090 with cat_ok/cat_ng declared in conn_info, vite,
// webctld. Non-destructive -- it opens a modal and reads it, and never presses
// the reset button.
//
// BLOCKED (2026-08-18): enter_inspection.mjs does not currently reach the
// Inspection UI -- it stops at "station region: NOT-IN-INSPECTION-UI". The
// same journey inside flows.mjs's inspCycle DOES work, because that copy also
// handles the camera-reconnect modal, a collapsed side menu and SPLASH
// bounces. Two implementations of one sequence, and the standalone one has
// rotted; the fix is to share it, not to add a third copy here. Until then
// this exits 2 -- distinct from PASS(0) and FAIL(1) -- so a run that could not
// happen can never be read as a run that passed.
//
// The narrower guard that DOES run today is unit_no_hardcoded_sel.mjs.
import { execFileSync } from 'node:child_process';

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const URL_ARG = (process.argv.find(a => a.startsWith('--url=')) || '').split('=')[1]
             || 'http://localhost:8081/';

const api = async (path, body) => {
  const r = await fetch(BASE + path, {
    method: body === undefined ? 'GET' : 'POST',
    headers: { 'content-type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const j = await r.json();
  if (j.error) throw new Error(`${path}: ${j.error}`);
  return j;
};
const evalJs = (expr) => api('/eval', { expr }).then(r => r.result);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };

// 1. Get into the Inspection UI. Reusing the script rather than repeating it:
//    every one of its four steps has a trap documented in its header, and a
//    second copy of that knowledge is a second copy to rot.
console.log('[1] entering the Inspection UI (enter_inspection.mjs)');
try {
  execFileSync(process.execPath, ['enter_inspection.mjs', `--url=${URL_ARG}`],
               { cwd: import.meta.dirname, stdio: 'pipe', timeout: 180000 });
} catch (e) {
  console.log('  could not reach the Inspection UI:', String(e.stdout || e.message).slice(-300));
  process.exit(2);
}
await sleep(2000);

// 2. The strip's own NG/OK tags. These are the reference: they read
//    cnt[selNG]/cnt[selOK] and are what the operator watches while running.
//    Absent means the wiring is not declared -- the panel then shows raw
//    SEL1/2/3 and there is no NG/OK claim to check. Skip rather than pass:
//    a green light for a test that could not run is worse than no test.
console.log('[2] reading the strip counter row');
const strip = await evalJs(`(() => {
  const tags = [...document.querySelectorAll('.ant-tag')]
    .filter(t => /^\\s*[\\d.,]+[kM]?\\s*$/.test(t.innerText));
  return tags.slice(0, 4).map(t => ({
    text: t.innerText.trim(),
    color: getComputedStyle(t).backgroundColor,
  }));
})()`);
console.log('  tags:', JSON.stringify(strip));

// 3. Open the history modal and read the header + the 目前 row.
console.log('[3] opening 統計歷史');
await api('/click', { selector: '[title="統計歷史 / 歸零"]' });
await api('/wait', { selector: '.ant-modal-title:has-text("統計歷史")' });
await sleep(800);

const modal = await evalJs(`(() => {
  const m = [...document.querySelectorAll('.ant-modal')]
    .find(x => (x.innerText || '').includes('統計歷史'));
  if (!m) return null;
  const rows = [...m.querySelectorAll('div')].filter(d => (d.innerText||'').startsWith('目前'));
  const hdr  = [...m.querySelectorAll('div')].filter(d => (d.innerText||'').startsWith('歸零時間'));
  const cells = (el) => el ? [...el.children].map(c => c.innerText.trim()) : null;
  return { current: cells(rows[0]), header: cells(hdr[0]) };
})()`);
if (!check(modal && modal.current && modal.header, 'could not read the modal rows')) {
  await api('/shot', { path: 'hist_wiring_fail.png' });
  process.exit(1);
}
console.log('  header :', JSON.stringify(modal.header));
console.log('  目前   :', JSON.stringify(modal.current));

// 4. The header must name the outlet. "NG" with no selector means the wiring
//    never arrived, which is the skip case, not a pass.
const ngHdr = (modal.header[1] || '');
const okHdr = (modal.header[2] || '');
const m1 = /NG\s+(SEL\d)/.exec(ngHdr), m2 = /OK\s+(SEL\d)/.exec(okHdr);
if (!m1 || !m2) {
  console.log(`\nSKIP: wiring not declared (header "${ngHdr}" / "${okHdr}").`);
  console.log('      Set cat_ng / cat_ok in machine_setting.json conn_info and re-run.');
  process.exit(0);
}
console.log(`[4] wiring from header: NG=${m1[1]}  OK=${m2[1]}`);

// 5. The cross-check. 目前's NG/OK cells must equal the strip's NG/OK tags.
const curNG = modal.current[1], curOK = modal.current[2];
const tagNG = strip[0] && strip[0].text, tagOK = strip[1] && strip[1].text;
console.log(`[5] strip NG=${tagNG} OK=${tagOK}   vs   modal NG=${curNG} OK=${curOK}`);
check(curNG === tagNG, `NG disagrees: strip "${tagNG}" vs history "${curNG}" -- history is reading a different selector`);
check(curOK === tagOK, `OK disagrees: strip "${tagOK}" vs history "${curOK}"`);

await api('/shot', { path: 'hist_wiring.png' });
console.log(fails ? `\nFAIL: ${fails} assertion(s)`
                  : '\nPASS: 統計歷史 reads the same wiring as the strip');
process.exit(fails ? 1 : 0);
