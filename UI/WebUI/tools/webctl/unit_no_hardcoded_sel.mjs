// Guard: no NG/OK claim may name a selector directly.
//
//   node unit_no_hardcoded_sel.mjs
//
// Which physical outlet is the reject bin is WIRING. It comes from the core's
// conn_info as cat_ng / cat_ok and is resolved in the panel as selNG / selOK.
// Writing cnt.SEL2 where NG is meant bakes one machine's wiring into the code,
// and the failure is silent: on a machine wired cat_ng 1 the NG column reads
// SEL2, which is always zero there, so every reject displays as none. That
// shipped, hid 106 real rejects behind a "not wired on this machine" warning,
// and was fixed -- in the counter row. The identical hardcoding in the history
// modal's "目前" row survived that fix and stayed for months, in the row that
// previews what 歸零統計 is about to archive.
//
// A source check rather than a UI test, deliberately. The real integration
// probe needs the Inspection UI entry sequence, which currently exists twice
// (flows.mjs has a working copy, enter_inspection.mjs a broken one) and should
// be shared before a third caller is added. This costs milliseconds, needs no
// core and no browser, and catches the exact mistake that was made twice.
//
// It does NOT prove the panel is correct -- only that nothing claims NG or OK
// while naming a selector. Raw SEL1/SEL2/SEL3 readouts are legitimate and are
// allowed by name below.
import fs from 'node:fs';
import path from 'node:path';

const FILE = path.join(import.meta.dirname, '..', '..', 'src', 'component', 'uInspESP32_UI.jsx');
const src = fs.readFileSync(FILE, 'utf8');

// Blank out comments before scanning, keeping line numbers intact.
//
// Line-by-line comment detection is not enough here: this file explains the
// bug in prose, so the string "cnt.SEL2" appears inside a {/* ... */} block
// describing what the code used to do. Matching that reports the explanation
// as the offence -- which this check did on its first run. Strip block
// comments as regions (newlines preserved so reported line numbers stay
// right), then line comments.
const stripped = src
  .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, ' '))
  .replace(/\/\/[^\n]*/g, (m) => ' '.repeat(m.length));
const lines = stripped.split(/\r?\n/);
const rawLines = src.split(/\r?\n/);

// Lines that legitimately name a selector: the raw all-three readouts, where
// no NG/OK meaning is asserted and the name is the only thing distinguishing
// the columns.
const ALLOW = [
  /<Cell\s+label="SEL[123]"/,              // setup modal: raw per-selector counts
  /tag\('SEL[123]',\s*cnt\.SEL[123],\s*undefined,\s*true\)/,  // unwired fallback row
  /\['SEL1',\s*'SEL2',\s*'SEL3'\]/,        // iteration over all of them
  /sel:\s*\{/,                             // the reset snapshot's raw block
  /SEL1:\s*n0\(cnt\.SEL1\),/,              // ditto, same statement
];

const offenders = [];
lines.forEach((code, i) => {
  if (!/cnt\.SEL[123]|cnt\['SEL[123]'\]/.test(code)) return;
  if (ALLOW.some((re) => re.test(code))) return;
  offenders.push({ n: i + 1, text: (rawLines[i] || '').trim().slice(0, 110) });
});

console.log(`scanned ${lines.length} lines of ${path.basename(FILE)}`);
if (offenders.length) {
  console.log('\nselector named outside an allowed raw readout:');
  offenders.forEach((o) => console.log(`  ${FILE}:${o.n}\n    ${o.text}`));
  console.log('\nFAIL: use selNG / selOK (from conn_info cat_ng / cat_ok).');
  console.log('      If this really is a raw per-selector readout, add it to ALLOW here.');
  process.exit(1);
}

// And the positive half: the two places that DO claim NG/OK must read the
// resolved wiring. Absence means someone deleted the mapping rather than the
// hardcoding -- the check above would then pass while saying nothing.
const usesResolved = (src.match(/cnt\[sel(NG|OK)\]/g) || []).length;
console.log(`cnt[selNG]/cnt[selOK] uses: ${usesResolved}`);
if (usesResolved < 2) {
  console.log('\nFAIL: expected the strip counter row AND the history 目前 row to read cnt[selNG]/cnt[selOK].');
  process.exit(1);
}

console.log('\nPASS: every NG/OK claim reads the wiring, not a selector name');
