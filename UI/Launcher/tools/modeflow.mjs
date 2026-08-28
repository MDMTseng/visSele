// Camera-mode transition flow, driven end to end.
//
//   node tools/modeflow.mjs
//     SOAK_APP_ROOT / SOAK_WORKING_DIR   same meaning as soak.mjs
//     MODEFLOW_INSP_MIN=3                minutes per inspection leg (default 3)
//     MODEFLOW_FREERUN_S=30              seconds of free-run preview (default 30)
//
// The question is whether the camera survives being moved between its three
// trigger states in one session. Each leg leaves the camera somewhere different,
// and the next leg has to work from there:
//
//   DefConf -> 快速驗證 free-run preview      camera free-running, no trigger
//   -> Inspection, run 3 min                  hardware trigger on the device's line
//   -> DefConf -> TAKE (soft trigger)         software trigger, one frame
//   -> Inspection, run 3 min                  hardware trigger again
//
// Total ~7 minutes. The pass condition for each inspection leg is the board
// reaching state 101 and the gate edge counter moving -- a session that came up
// with the camera in the wrong state sits in 102/112 with the plate stopped,
// because calibration needs one frame per sync pulse and never gets one.
//
// Every step asserts its OUTCOME, not that a control was clickable: a gesture
// that lands on nothing and a gesture that lands on the wrong thing look
// identical from the click side.
import { _electron as electron } from 'playwright';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { Config } = require('../src/config');

const here = path.dirname(fileURLToPath(import.meta.url));
const APP_DIR = path.resolve(here, '..');
const REPO = path.resolve(here, '..', '..', '..');
const APP_ROOT = process.env.SOAK_APP_ROOT || path.join(REPO, 'export_v2', 'app');
const WORKING_DIR = process.env.SOAK_WORKING_DIR
  || path.join(REPO, 'InspectionCore', 'Core0_1');
const DEF = process.env.MODEFLOW_DEF || 'test1.hydef';
const TAG = process.env.SOAK_TAG || '11沖壓成形';
const INSP_MIN = Number(process.env.MODEFLOW_INSP_MIN || 3);
const FREERUN_S = Number(process.env.MODEFLOW_FREERUN_S || 30);
const FREQ = Number(process.env.SOAK_FREQ || 8000);
const PERIF_CONSOLE = 4099;
const OUT = process.env.MODEFLOW_OUT || path.join(os.tmpdir(), 'modeflow');
fs.mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const results = [];
const note = (s) => { console.log(s); results.push(s); };

const die = async (app, msg) => {
  console.log('FAILED: ' + msg);
  try { await page.screenshot({ path: path.join(OUT, 'modeflow_FAILED.png') }); } catch {}
  try { if (app) await app.close(); } catch {}
  console.log('--- summary ---'); results.forEach((r) => console.log('  ' + r));
  process.exit(1);
};

{
  const control = require('../src/control');
  const r = await control.ping(4098, 800);
  if (r.ok) { console.log('FAILED: a core is already running -- stop it first'); process.exit(1); }
}

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'modeflow-launcher-'));
const cfg = new Config(userData);
cfg.values.appRoot = APP_ROOT;
cfg.values.workingDir = WORKING_DIR;
cfg.save();
console.log('[0] appRoot ' + APP_ROOT);

const app = await electron.launch({
  args: [APP_DIR, `--user-data-dir=${userData}`],
  env: { ...process.env, INSP_PERIF_CONSOLE: String(PERIF_CONSOLE) },
});
const page = await app.firstWindow();

// NO WebSocket wrapper here.
//
// It was installed to give the trigger probe a socket to speak on, and it cost
// the whole run: with it in place the page never opened the peripheral channel,
// so the board console accepted the connect and immediately reset it
// (ECONNRESET) for the entire startup. The probe is a convenience; the board
// console is the only way this test knows whether anything worked.
let onApp = false;
for (let i = 0; i < 90; i++) {
  if (/WebUI/i.test(decodeURIComponent(page.url()))) { onApp = true; break; }
  await sleep(1000);
}
if (!onApp) await die(app, 'never reached the WebUI -- ' + page.url());
await page.waitForLoadState('domcontentloaded');
await sleep(7000);

// ---- helpers (same shapes soak.mjs uses; an outcome check follows every one)
const closeDrawers = () => page.evaluate(() => {
  let n = 0;
  for (const x of document.querySelectorAll('.ant-drawer-close')) if (x.offsetParent) { x.click(); n++; }
  return n;
}).catch(() => 0);

const skipCamera = () => page.evaluate(() => {
  let n = 0;
  for (const b of document.querySelectorAll('button, .ant-btn, a'))
    if (b.offsetParent && (b.innerText || '').includes('跳過相機連線')) { b.click(); n++; }
  return n;
}).catch(() => 0);

// Whitespace-insensitive on purpose: antd renders a two-character Chinese
// button label as "關 閉", with a space injected between the glyphs. An exact
// match then finds nothing and the step reports the control as missing -- which
// is what "the 快速驗證 preview had no 關閉 button" was, about a button that was
// on screen and clickable.
const clickText = (label) => page.evaluate((label) => {
  const norm = (x) => (x || '').replace(/\s+/g, '');
  const want = norm(label);
  for (const e of document.querySelectorAll('*')) {
    if (e.children.length || !e.offsetParent) continue;
    if (norm(e.innerText) === want) { e.click(); return true; }
  }
  for (const e of document.querySelectorAll('button, .ant-btn, a, li')) {
    if (e.offsetParent && norm(e.innerText) === want) { e.click(); return true; }
  }
  return false;
}, label);

const clickIcon = (icon, nth = 0) => page.evaluate(({ icon, nth }) => {
  const out = [];
  for (const ic of document.querySelectorAll('[class*="' + icon + '"]')) {
    const act = ic.closest('button, [role="button"], .ant-btn, a, li, [class*="btn"]') || ic;
    if (!act.offsetParent) continue;
    const disabled = act.disabled === true || act.getAttribute('aria-disabled') === 'true';
    if (!disabled && !out.includes(act)) out.push(act);
  }
  if (!out[nth]) return false;
  out[nth].click(); return true;
}, { icon, nth });

const seeText = (needle) => page.evaluate((n) => document.body.innerText.includes(n), needle)
  .catch(() => false);

const waitText = async (needle, why, ms = 20000) => {
  for (let i = 0; i < ms / 500; i++) { if (await seeText(needle)) return true; await sleep(500); }
  await die(app, why + ' (waiting for "' + needle + '")');
};

// Leave DefConfUI and PROVE it, instead of clicking a back arrow and hoping.
//
// Two things make the bare click unreliable: there is more than one arrow-left
// glyph on screen, and leaving after anything touched the recipe raises
// 「設定已更動 確定要離開嗎?」 -- a modal whose overlay swallows every later
// click, so the next step reports "no play control" about a control that is
// simply behind a dialog.
const leaveDefConf = async () => {
  // Try every arrow-left on screen, not just the first. There is more than one
  // (the rail carries its own), and clickIcon's first match is not necessarily
  // the page's back button -- which read as "the click did nothing".
  for (let attempt = 0; attempt < 8; attempt++) {
    await closeDrawers();
    const nth = attempt % 4;
    const hit = await clickIcon('anticon-arrow-left', nth);
    if (nth === 1) {
      // The rail's own 主選單 entry. More reliable than the arrow glyph, which
      // sits inside a custom IconButton rather than a real <button> -- so a
      // synthetic click on the SVG does not always reach the handler.
      await clickText('主選單');
    }
    if (!hit && nth === 0) await clickText('←');
    await sleep(1500);
    // Confirm the unsaved-changes dialog if it came up. 確定 is the leave path;
    // this test deliberately does not save.
    if (await seeText('設定已更動') || await seeText('確定要離開')) {
      await clickText('確定') || await clickText('確 定') || await clickText('OK');
      await sleep(1500);
    }
    if (await seeText('量測設定')) return true;   // back on the main menu
  }
  await page.screenshot({ path: path.join(OUT, 'leaveDefConf_FAILED.png') }).catch(() => {});
  return false;
};

// ---- board console, for the only pass condition that matters
let sock = null, buf = '', lines = [], askId = 91000, lastConsoleErr = '';
const connectConsole = () => new Promise((resolve) => {
  const s2 = net.connect(PERIF_CONSOLE, '127.0.0.1');
  let settled = false;
  const done = (v) => { if (!settled) { settled = true; resolve(v); } };
  s2.on('data', (d) => {
    buf += d.toString('latin1'); let n;
    while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); }
  });
  s2.on('error', (e) => { lastConsoleErr = e && e.code ? e.code : String(e); done(null); });
  s2.on('close', () => { if (sock === s2) sock = null; });
  s2.once('connect', () => done(s2));
});
async function ask(o, ms = 1800) {
  if (!sock) { sock = await connectConsole(); if (!sock) return null; buf = ''; }
  const my = askId++; lines = [];
  try { sock.write(JSON.stringify({ ...o, id: my }) + '\n'); } catch { sock = null; return null; }
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return null; }
}
const stat = () => ask({ type: 'get_running_stat' });
const edges = (s) => (s && s.gate ? s.gate.edges : -1);

// The camera's ACTUAL trigger configuration, so each leg records what it left
// behind rather than what it meant to.
const probe = async () => {
  const r = await page.evaluate(() => new Promise((res) => {
    const ws = window.__MF_WS__;   // absent by design; see the note at startup
    if (!ws || ws.readyState !== 1) return res(null);
    const b = new TextEncoder().encode(JSON.stringify({ type: 'cam_trigger_probe' }));
    const u = new Uint8Array(9 + b.length + 1);
    u[0] = 83; u[1] = 67; u[2] = 0;                      // 'S','C'
    const g = (window.__MFPG__ = (window.__MFPG__ || 60000) + 1);
    u[3] = g >> 8; u[4] = g & 255;
    const l = u.length - 9;
    u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
    u.set(b, 9);
    const on = (ev) => {
      try {
        const a = new Uint8Array(ev.data);
        const s = new TextDecoder().decode(a.subarray(9)).replace(/\0+$/, '');
        if (s.includes('cam_trigger_probe')) { ws.removeEventListener('message', on); res(s); }
      } catch {}
    };
    ws.addEventListener('message', on);
    ws.send(u);
    setTimeout(() => { ws.removeEventListener('message', on); res(null); }, 3000);
  })).catch(() => null);
  return r;
};

// ---- board ready
{
  // The peripheral channel is opened by the PAGE's PD CONNECT, not by the core
  // starting, so the console is not there the moment the window paints -- and
  // how long it takes varies with how long the board takes to come up. Keep
  // asking; a fixed short budget turned a slow start into "never answered".
  // soak.mjs's exact shape, because it is the one that works: a FRESH socket on
  // every failure, and a wide gap between tries.
  //
  // The peripheral channel is opened by the PAGE's PD CONNECT, not by the core
  // starting, so an early connect is accepted with no channel behind it -- the
  // core neither answers nor closes. Reusing that socket asks into a void
  // forever, which is what "connected, but no reply" was. Reconnecting only
  // every fifth try was still mostly asking down the dead one.
  await closeDrawers(); await skipCamera();
  let s = null;
  for (let i = 0; i < 20 && !s; i++) {
    s = await ask({ type: 'get_running_stat' }, 2500);
    if (!s) {
      sock = null;
      await closeDrawers(); await skipCamera();
      if (i % 5 === 4) console.log(`    still waiting for the board console (${i + 1}/20)`);
      await sleep(3000);
    }
  }
  if (!s) await die(app, 'board not answering after 20 tries'
    + (lastConsoleErr ? ' (last connect error: ' + lastConsoleErr + ')' : ' (connected, no reply)'));
  if (s.state !== 100 || (s.error_hist || []).length) {
    await ask({ type: 'set_setup', plate: { freq: 0 } }, 1500);
    await sleep(2500);
    await ask({ type: 'exit_insp_mode' }, 2000);
    await ask({ type: 'clear_error' }, 1200);
    await ask({ type: 'clear_error_history' }, 1200);
    await sleep(1500);
    await ask({ type: 'exit_insp_mode' }, 1500);   // clear_error puts it back IN
    s = await ask({ type: 'get_running_stat' }, 1800);
  }
  // Set the plate speed, or RUN stays disabled.
  //
  // Without it the inspection UI shows "STOP · 盤停止 · 尚無轉速,請先在設定面板
  // 設定" and greys the play control -- which the harness reported as "no play
  // control to start the machine", about a control that was on screen and
  // deliberately unusable.
  const fr = await ask({ type: 'set_setup', plate: { freq: FREQ } }, 1800);
  note(`[1] board ready, state ${s && s.state}, plate freq = ${FREQ}`
     + `${fr && fr.ack ? '' : ' (NO ACK)'}`);
}

// ---- open the recipe
await closeDrawers(); await skipCamera();
await clickIcon('anticon-folder-open');
await waitText(DEF, 'the recipe browser never listed ' + DEF);
{
  const item = page.locator(`text="${DEF}"`).first();
  try { await item.click({ timeout: 8000 }); await sleep(400); await item.dblclick({ timeout: 8000 }); }
  catch {}
  await sleep(3000);
}
await closeDrawers();
await clickText(TAG); await sleep(1200);
await closeDrawers();
note(`[2] recipe ${DEF} + tag ${TAG}`);

// ---- leg A: DefConf -> 快速驗證 free-run preview
await waitText('量測設定', 'the main menu never showed 量測設定', 15000);
if (!await clickText('量測設定')) await die(app, 'could not enter 量測設定 (DefConfUI)');
await sleep(4000);
note('[3] in DefConfUI');

if (!await clickText('快速驗證')) await die(app, '快速驗證 not found in DefConfUI');
await sleep(2500);
// The mode dialog offers CI/FI; CI is the free-running continuous one.
await clickText('檢驗');
await sleep(3000);
note(`[4] 快速驗證 free-run for ${FREERUN_S}s`);
await sleep(FREERUN_S * 1000);
await page.screenshot({ path: path.join(OUT, 'a_freerun.png') }).catch(() => {});
note('    camera after free-run: ' + ((await probe()) || 'probe unavailable'));

// Leave the preview by ITS OWN 關閉. 快速驗證 opens a modal over DefConfUI --
// the page's back arrow is behind it and does nothing, which read as "no play
// control" two steps later.
if (!await clickText('關閉')) await die(app, 'the 快速驗證 preview had no 關閉 button');
await sleep(2500);
if (!await leaveDefConf()) await die(app, 'could not get back out of DefConfUI after the preview');

// ---- leg B: inspection, INSP_MIN minutes
const inspectionLeg = async (tag) => {
  await closeDrawers(); await skipCamera();
  // Re-pick the 製程 tag every time.
  //
  // Coming back from DefConfUI clears it -- the prep screen shows 製程 with a
  // warning and the play control is disabled, which reads as "no play control"
  // when the control is right there and simply not usable yet.
  await clickText(TAG); await sleep(1200);
  await closeDrawers();
  // Re-assert the plate speed here too. A CI session (快速驗證) leaves it at 0 --
  // INSPECTION_MODE_CAL holds the plate stopped on purpose -- and the run
  // control stays disabled with "尚無轉速" until something sets it again.
  const _fr = await ask({ type: 'set_setup', plate: { freq: FREQ } }, 1800);
  const _chk = await ask({ type: 'get_running_stat' }, 1800);
  note(`[${tag}] plate freq -> ${FREQ} (${_fr && _fr.ack ? 'ack' : 'NO ACK'}), `
     + `board reports ${_chk && _chk.plate_freq}`);
  await sleep(2000);
  if (!await clickIcon('anticon-caret-right')) await die(app, tag + ': no play control to enter the Inspection UI');
  await waitText('工位', tag + ': the Inspection UI never opened');
  await sleep(4000);
  await closeDrawers(); await skipCamera();
  if (!await clickIcon('anticon-caret-right')) await die(app, tag + ': no play control to start the machine');
  await sleep(9000);

  const s0 = await stat();
  const e0 = edges(s0);
  await sleep(10000);
  const s1 = await stat();
  const e1 = edges(s1);
  note(`[${tag}] state ${s1 && s1.state}  gate ${e0} -> ${e1}`);
  if (!(s1 && s1.state === 101 && e1 > e0)) {
    await page.screenshot({ path: path.join(OUT, tag + '_FAILED.png') }).catch(() => {});
    note(`[${tag}] FAIL -- state ${s1 && s1.state}, gate ${e0}->${e1} (101 + moving edges is the pass)`);
    return false;
  }
  note('    camera during inspection: ' + ((await probe()) || 'probe unavailable'));
  // Photograph the running screen, not just the counters. A state number says
  // the machine is inspecting; only the picture says what it is judging.
  await sleep(4000);
  await page.screenshot({ path: path.join(OUT, tag + '_running.png') }).catch(() => {});
  let shot = 0;
  const until = Date.now() + INSP_MIN * 60000;
  while (Date.now() < until) {
    await sleep(30000);
    const s = await stat();
    note(`    +30s state ${s && s.state} gate ${edges(s)}`);
    await page.screenshot({ path: path.join(OUT, tag + '_t' + (++shot) + '.png') }).catch(() => {});
    if (!s || s.state !== 101) { note(`[${tag}] LOST state 101 mid-run`); return false; }
  }
  // stop, so the next leg starts from a known place
  await clickIcon('anticon-caret-right');
  await sleep(6000);
  return true;
};

const okB = await inspectionLeg('B-insp-after-freerun');

// ---- leg C: back to DefConf, TAKE with the software trigger
await closeDrawers();
await clickIcon('anticon-arrow-left'); await sleep(3000);
if (!await clickText('量測設定')) await die(app, '[C] could not re-enter 量測設定');
await sleep(4000);
if (!await seeText('TAKE') && !await seeText('重新設定')) {
  note('[C] TAKE control not visible in DefConfUI -- skipping the retake leg');
} else {
  await clickText('重新設定/TAKE') || await clickText('TAKE');
  await sleep(2000);
  // "立即" is the immediate (software) capture; the other button waits for a
  // hardware trigger, which is not what this leg is testing.
  if (!await clickText('立即')) await die(app, '[C] the TAKE dialog had no 立即 button');
  await sleep(6000);
  note('[C] TAKE (software trigger) done');
  note('    camera after TAKE: ' + ((await probe()) || 'probe unavailable'));
  await page.screenshot({ path: path.join(OUT, 'c_take.png') }).catch(() => {});
}

// ---- leg D: inspection again, from the software-trigger state
if (!await leaveDefConf()) await die(app, '[D] could not get back out of DefConfUI after TAKE');
const okD = await inspectionLeg('D-insp-after-take');

console.log('--- summary ---');
results.forEach((r) => console.log('  ' + r));
console.log(okB && okD ? 'PASS -- both inspection legs reached 101 with the gate moving'
                       : 'FAIL -- see the legs above');
try { await app.close(); } catch {}
process.exit(okB && okD ? 0 : 1);
