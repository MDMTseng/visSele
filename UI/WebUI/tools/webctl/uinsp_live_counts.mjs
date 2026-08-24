// C3 / worklist 3.4 -- does the number on the panel equal the number in the machine?
//
//   node uinsp_live_counts.mjs [--n 20] [--url http://localhost:8081]
//
// The uInsp panel polls the board for get_running_stat and renders `count`.
// Nothing checked that the rendered figure is the board's figure. That gap is
// worth closing precisely because it fails QUIETLY: a panel stuck on a stale
// poll, or reading the wrong bin, looks exactly like a machine that is not
// sorting -- and the operator believes the panel.
//
// So drive the machine from the CONSOLE (the board's own truth) and read the
// answer from the DOM (what the operator sees), then compare:
//
//     board get_running_stat.count      vs     [data-testid="uinsp-count"]
//
// data-value carries the UNROUNDED count on purpose (compactN would render
// 1200 as "1.2k"), so this compares integers, not display strings.
//
// Feeding is trig_phantom_train, so no plate and no camera are needed. With
// the core in --bench (INSP_CAM_TS_SYNTH) every part comes back NA, which is
// the correct behaviour for a core that is not really inspecting -- the bin
// does not matter here, the AGREEMENT does.
import { execFileSync } from 'node:child_process';
import net from 'node:net';
import { makeCtl, sleep } from './lib_enter.mjs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const N = Number(arg('n', 20));
const URL_ARG = arg('url', process.env.WEBCTL_URL || 'http://localhost:8081');
const PORT = Number(arg('port', 4099));

const { api, ev } = makeCtl();

// ---- the board side, over the dev console -------------------------------
const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
// Once we are finished the core closes its end, and an ECONNRESET printed
// under a PASS reads like the test failed. Only shout while we still care.
let done = false;
s.on('error', (e) => {
  if (done) return;
  console.error(`console ${PORT}: ${e.message}`);
  process.exit(1);
});
await new Promise((r) => s.once('connect', r));

let id = 88000;
async function ask(obj, ms = 2200) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  await sleep(ms);
  const hit = lines.find((l) => l.includes(`"id":${myId}`));
  if (!hit) return null;
  try { return JSON.parse(hit.slice(hit.indexOf('{'))); } catch { return null; }
}
const boardCounts = async () => {
  const r = await ask({ type: 'get_running_stat' });
  return r && r.count ? r.count : null;
};
const total = (c) => Object.values(c || {}).reduce((a, b) => a + (Number(b) || 0), 0);

// ---- the operator's side, out of the DOM --------------------------------
async function panelCounts() {
  const r = await ev(
    'Array.from(document.querySelectorAll(\'[data-testid="uinsp-count"]\'))' +
    '.map(function(e){return [e.getAttribute("data-bin"), e.getAttribute("data-value")];})'
  );
  if (!Array.isArray(r)) return null;
  const out = {};
  for (const [bin, v] of r) if (v !== '' && v !== null) out[bin] = Number(v);
  // SEL_SUPPRESSED / SEL1_NO_QUOTA are not tags in the strip -- they are a
  // whole line that appears only when they are non-zero (defect 2.9). Read
  // them from that line's own attributes, so "the panel renders it" is what is
  // actually asserted rather than "a tag with this name exists".
  const uns = await ev(
    '(function(){var e=document.querySelector(\'[data-testid="uinsp-unsorted"]\');' +
    'return e?[e.getAttribute("data-suppressed"), e.getAttribute("data-noquota")]:null;})()'
  );
  if (Array.isArray(uns)) {
    if (uns[0] !== null) out.SEL_SUPPRESSED = Number(uns[0]);
    if (uns[1] !== null) out.SEL1_NO_QUOTA = Number(uns[1]);
  }
  return Object.keys(out).length ? out : null;
}

let fail = 0;
const say = (m) => console.log(m);

say('[1] entering the Inspection UI');
try {
  execFileSync(process.execPath, ['enter_inspection.mjs', `--url=${URL_ARG}`],
               { cwd: import.meta.dirname, stdio: 'pipe', timeout: 240000 });
} catch (e) {
  say(`  FAIL: enter_inspection.mjs did not complete (${e.message.split('\n')[0]})`);
  process.exit(1);
}

say('[2] zeroing the board counters');
await ask({ type: 'clear_error' }, 1200);
await ask({ type: 'reset_running_stat' }, 1500);
const zero = await boardCounts();
if (!zero) { say('  FAIL: board did not answer get_running_stat'); process.exit(1); }
say(`  board: ${JSON.stringify(zero)}`);

// Let the panel's own poll catch up with the reset before reading it, or the
// first comparison races the poll interval rather than testing anything.
await sleep(4000);
const p0 = await panelCounts();
if (!p0) {
  say('  FAIL: no [data-testid="uinsp-count"] nodes -- the uInsp panel is not on screen');
  process.exit(1);
}
say(`  panel: ${JSON.stringify(p0)}`);
if (total(p0) !== 0) { say(`  FAIL: panel did not follow the reset (total=${total(p0)})`); fail++; }

say(`[3] feeding ${N} phantom parts`);
// 300ms, not 150ms. At 150 the gate blocks most of the train (measured:
// in=20 out=3, loss="blocked" -- pipeline depth, working as designed), so the
// test compared 3 parts instead of 20. The accounting stayed consistent, but a
// test that silently shrinks its own sample is a weak test.
await ask({ type: 'trig_phantom_train', count: N, period_us: 300000 }, 1200);

// Wait for the BOARD to finish, then give the panel a few poll intervals.
const deadline = Date.now() + N * 300 + 30000;
let bc = null;
while (Date.now() < deadline) {
  bc = await boardCounts();
  if (bc && total(bc) >= N) break;
  await sleep(800);
}
say(`  board: ${JSON.stringify(bc)}  total=${total(bc)}`);
if (total(bc) < N) {
  // Not a failure: the gate refuses parts while the pipeline is full, and that
  // is the design. Say WHY, with the board's own numbers, so the short sample
  // is an explained fact rather than a mystery.
  const g = (await ask({ type: 'get_running_stat' }) || {}).yield;
  say(`  NOTE: only ${total(bc)}/${N} were admitted -- gate ${JSON.stringify(g && g.gate)}`);
  say('        loss "blocked" is pipeline depth, not lost parts. The comparison');
  say('        below is still valid: it compares what actually happened.');
}

say('[4] waiting for the panel to agree');
let pc = null, agreed = false;
for (let i = 0; i < 20; i++) {
  await sleep(1000);
  pc = await panelCounts();
  if (pc && total(pc) === total(bc)) { agreed = true; say(`  agreed after ${i + 1}s`); break; }
}
say(`  panel: ${JSON.stringify(pc)}  total=${total(pc)}`);

// Two very different failures wear the same "totals do not match" shape, and
// only one of them is a stale panel:
//
//   * a bin the panel RENDERS disagrees        -> the panel is behind or wrong
//   * a bin the panel renders NOWHERE has count -> the operator cannot see
//                                                  those parts at all
//
// The second is the one worth naming. SEL_SUPPRESSED is "a verdict whose
// actuation was scheduled and not delivered", and on a real machine the only
// way to reach it is de-energising the driver while the plate turns -- which,
// in the firmware's own words, "lets a loaded plate coast and throw parts".
const panelBins = new Set(Object.keys(pc || {}));
const unrendered = Object.entries(bc || {})
  .filter(([b, v]) => Number(v) > 0 && !panelBins.has(b))
  .map(([b, v]) => `${b}=${v}`);

if (unrendered.length) {
  fail++;
  say(`  FAIL: the board counted parts in bins the panel does not render at all:`);
  say(`          ${unrendered.join(', ')}`);
  say('        The panel shows ' + JSON.stringify(pc) + ' -- an operator watching');
  say('        it sees nothing happen while those parts go through.');
} else if (!agreed) {
  fail++;
  say(`  FAIL: panel total ${total(pc)} never reached the board's ${total(bc)}`);
} else {
  // Totals matching is not enough -- a panel that puts every part in the wrong
  // bin also totals correctly, and mis-binned counts are exactly what an
  // operator acts on.
  const bins = new Set([...Object.keys(bc || {}), ...Object.keys(pc || {})]);
  const off = [];
  for (const b of bins) {
    const a = Number((bc || {})[b] || 0), c = Number((pc || {})[b] || 0);
    if (a !== c) off.push(`${b}: board ${a} vs panel ${c}`);
  }
  if (off.length) { fail++; say(`  FAIL: totals agree but bins do not -- ${off.join(', ')}`); }
  else say('  PASS: every bin agrees with the board');
}

say(`\n${fail === 0 ? 'PASS' : `FAIL (${fail})`}`);
// end(), not destroy() + exit(): tearing the socket down and exiting in the
// same tick makes libuv assert on Windows ("!(handle->flags & UV_HANDLE_CLOSING)")
// and prints a crash line underneath a PASS, which reads like a failure.
done = true;
process.exitCode = fail === 0 ? 0 : 1;
s.end();
