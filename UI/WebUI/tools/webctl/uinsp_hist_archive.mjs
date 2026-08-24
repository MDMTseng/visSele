// C3 / worklist 3.4b -- the snapshot has to survive what the reset destroys.
//
//   node uinsp_hist_archive.mjs [--n 12] [--url http://localhost:8081]
//
// "歸零統計" wipes the device's counters, and the device keeps no history of
// its own. The panel's answer is to file a snapshot first, and the comments
// around it are unusually emphatic about why:
//
//     "Zeroing DESTROYS the counts on the device, so this row is the only
//      surviving record of the batch"
//
// So the thing to test is not that a button works. It is that the row which
// replaces the destroyed counts actually carries them -- including the two
// fields that were once missing and cannot be reconstructed afterwards: the
// WIRING in force at the time, and the RAW per-selector counts underneath the
// NG/OK mapping.
//
// Three steps, in the order an operator meets them:
//   file + zero   -> device is zero AND exactly one new row holds the old counts
//   export        -> the file carries the rows, the wiring, and the raw counts
//   clear         -> the notepad empties
//
// NON-DESTRUCTIVE. The history is the operator's own record and lives only in
// this browser's localStorage; the run saves it up front and puts it back at
// the end, whatever happens in between.
import { execFileSync } from 'node:child_process';
import net from 'node:net';
import { makeCtl, sleep } from './lib_enter.mjs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const N = Number(arg('n', 12));
const URL_ARG = arg('url', process.env.WEBCTL_URL || 'http://localhost:8081');
const PORT = Number(arg('port', 4099));
const HIST_KEY = 'visSele.uinsp.statHist.v1';

const { api, ev } = makeCtl();

const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
let done = false;
s.on('error', (e) => { if (done) return; console.error(`console ${PORT}: ${e.message}`); process.exit(1); });
await new Promise((r) => s.once('connect', r));

let id = 77000;
async function ask(obj, ms = 2200) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  await sleep(ms);
  const hit = lines.find((l) => l.includes(`"id":${myId}`));
  if (!hit) return null;
  try { return JSON.parse(hit.slice(hit.indexOf('{'))); } catch { return null; }
}
const boardCounts = async () => ((await ask({ type: 'get_running_stat' })) || {}).count || null;

// The machine does not stay in READY between runs. The bench core answers every
// cam_trig synchronously on the RX path, so at short intervals a synth verdict
// can arrive while the previous one is still outstanding and the board stops
// with INSP_RESULT_MATCHES_NO_OBJECT (error 1) -- a known bench artefact of
// INSP_CAM_TS_SYNTH, not something this test is about. Clear it and climb back
// to READY rather than reporting "no parts were counted", which says nothing
// about the archive.
const READY = 101;
async function bringUp() {
  let st = await ask({ type: 'get_running_stat' });
  if (st && st.state === READY) return true;
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'set_dry_run', on: true }, 1200);
  await ask({ type: 'set_setup', plate: { freq: 15000 } }, 1500);   // nested, not top-level
  await ask({ type: 'enter_insp_mode' }, 1500);
  for (let i = 0; i < 30; i++) {
    st = await ask({ type: 'get_running_stat' });
    if (st && st.state === READY) { say(`  bring-up: READY after ${i + 1}s`); return true; }
    await sleep(1000);
  }
  say(`  bring-up FAILED, stuck at state ${st && st.state}`);
  return false;
}
const total = (c) => Object.values(c || {}).reduce((a, b) => a + (Number(b) || 0), 0);

const histJson = () => ev(`window.localStorage.getItem(${JSON.stringify(HIST_KEY)})`);
// Antd buttons carry no testid here, so match on the label the operator reads.
// Prefix, not equality: every one of these buttons rewrites its own text as it
// counts presses ("再按 2 次").
const clickByText = (t) => ev(
  `(function(){var b=Array.from(document.querySelectorAll("button"))` +
  `.find(function(x){return (x.innerText||"").trim().indexOf(${JSON.stringify(t)})===0;});` +
  `if(!b) return false; b.click(); return true;})()`
);

let fail = 0;
const say = (m) => console.log(m);
const bad = (m) => { fail++; say(`  FAIL: ${m}`); };

say('[1] entering the Inspection UI');
try {
  execFileSync(process.execPath, ['enter_inspection.mjs', `--url=${URL_ARG}`],
               { cwd: import.meta.dirname, stdio: 'pipe', timeout: 240000 });
} catch (e) {
  say(`  FAIL: enter_inspection.mjs did not complete (${e.message.split('\n')[0]})`);
  process.exit(1);
}

// Saved before anything is touched, restored in the finally below.
const savedHist = await histJson();
say(`[2] saved the operator's history (${savedHist ? JSON.parse(savedHist).length : 0} rows) for restore`);

try {
  say(`[3] producing a batch: zero, then feed ${N}`);
  await ask({ type: 'clear_error' }, 1200);
  if (!(await bringUp())) { bad('machine would not return to READY'); throw new Error('stop'); }
  await ask({ type: 'reset_running_stat' }, 1500);
  await ask({ type: 'trig_phantom_train', count: N, period_us: 400000 }, 1200);
  const deadline = Date.now() + N * 400 + 20000;
  let bc = null;
  while (Date.now() < deadline) {
    bc = await boardCounts();
    if (bc && total(bc) > 0) break;
    await sleep(800);
  }
  if (!bc || total(bc) === 0) { bad('no parts were counted -- nothing to archive'); throw new Error('stop'); }
  say(`  board: ${JSON.stringify(bc)} total=${total(bc)}`);

  say('[4] opening the history modal');
  await api('/click', { selector: '[title="統計歷史 / 歸零"]' });
  await api('/wait', { selector: '[data-testid="uinsp-hist-current"]' });
  // The "目前" row is the preview of exactly what the reset is about to file,
  // so it is the right thing to compare the stored row against -- not the
  // board, which is about to be wiped.
  const preview = await ev(
    'Array.from(document.querySelectorAll(\'[data-testid="uinsp-hist-cell"]\'))' +
    '.map(function(e){return [e.getAttribute("data-bin"), e.getAttribute("data-sel"), e.getAttribute("data-value")];})'
  );
  say(`  preview: ${JSON.stringify(preview)}`);
  const pv = {};
  for (const [bin, , v] of preview || []) if (v !== '' && v !== null) pv[bin] = Number(v);

  const before = JSON.parse((await histJson()) || '[]');

  say('[5] 送入歷史並歸零 (three presses)');
  for (let i = 0; i < 3; i++) { await clickByText('送入歷史並歸零'); await sleep(400); }
  // The first press may already read "再按 2 次", so drive the counter home.
  for (let i = 0; i < 3; i++) { await clickByText('再按'); await sleep(400); }
  await sleep(2500);

  const afterCounts = await boardCounts();
  if (total(afterCounts) !== 0) bad(`device not zeroed after reset: ${JSON.stringify(afterCounts)}`);
  else say('  device zeroed');

  const after = JSON.parse((await histJson()) || '[]');
  if (after.length !== before.length + 1) {
    bad(`expected exactly one new row, got ${before.length} -> ${after.length}`);
  } else {
    const row = after[0];
    say(`  filed row: ${JSON.stringify(row).slice(0, 220)}`);
    // The counts the device no longer has.
    for (const bin of ['NG', 'OK', 'NA']) {
      if (bin in pv && Number(row[bin]) !== pv[bin]) {
        bad(`row ${bin}=${row[bin]} does not match what the panel previewed (${pv[bin]})`);
      }
    }
    // The two fields that make an old row still readable. Their absence is the
    // regression this shape was introduced to prevent.
    if (row.v !== 2) bad(`row is v:${row.v}, not the v:2 shape that carries wiring + raw counts`);
    else {
      const hasWiring = row.wiring && (row.wiring.ng || row.wiring.NG || row.wiring.ok || row.wiring.OK);
      if (!hasWiring) bad(`v:2 row carries no wiring -- "NG 3124" does not say NG meant SEL1`);
      const raw = row.raw || row.sel || row.counts;
      if (!raw) bad('v:2 row carries no raw per-selector counts');
      if (hasWiring && raw) say('  row carries the wiring and the raw selector counts');
    }
  }

  say('[6] 匯出歷史 JSON');
  // Intercept the Blob instead of chasing a download: the click handler builds
  // the payload and hands it straight to createObjectURL, so capturing there
  // tests the real handler and needs nothing from the browser's download path
  // (which is sandboxed here anyway).
  await ev('(function(){window.__EXP__=null;var o=URL.createObjectURL;' +
           'URL.createObjectURL=function(b){try{b.text().then(function(t){window.__EXP__=t;});}catch(e){}' +
           'return o.apply(URL,arguments);};return true;})()');
  const clicked = await clickByText('匯出歷史 JSON');
  if (!clicked) bad('no 匯出歷史 JSON button (it only appears when history is non-empty)');
  await sleep(1500);
  const expText = await ev('window.__EXP__');
  if (!expText) bad('export produced no payload');
  else {
    let doc = null;
    try { doc = JSON.parse(expText); } catch (e) { bad(`export is not valid JSON: ${e.message}`); }
    if (doc) {
      if (!Array.isArray(doc.rows) || doc.rows.length !== after.length)
        bad(`export carries ${doc.rows && doc.rows.length} rows, history has ${after.length}`);
      else if (!doc.wiring || (!doc.wiring.ng && !doc.wiring.ok))
        bad('export carries no wiring block');
      else say(`  export OK: ${doc.rows.length} rows, wiring ${JSON.stringify(doc.wiring)}, machine ${doc.machine}`);
    }
  }

  say('[7] 清除歷史 (three presses)');
  for (let i = 0; i < 4; i++) { await clickByText('清除歷史'); await clickByText('再按'); await sleep(400); }
  await sleep(1200);
  const cleared = JSON.parse((await histJson()) || '[]');
  if (cleared.length !== 0) bad(`清除歷史 left ${cleared.length} rows`);
  else say('  history cleared');
} catch (e) {
  if (e.message !== 'stop') { fail++; say(`  FAIL: ${e.message}`); }
} finally {
  // Always. The history is the operator's record, not ours to spend on a test.
  await ev(`window.localStorage.setItem(${JSON.stringify(HIST_KEY)}, ${JSON.stringify(savedHist || '[]')})`);
  const back = JSON.parse((await histJson()) || '[]');
  say(`[8] restored the operator's history (${back.length} rows)`);
}

say(`\n${fail === 0 ? 'PASS' : `FAIL (${fail})`}`);
done = true;
process.exitCode = fail === 0 ? 0 : 1;
s.end();
