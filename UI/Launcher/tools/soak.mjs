// Long soak through the REAL Electron launcher.
//
//   node tools/soak.mjs [minutes]
//     SOAK_APP_ROOT=<dir>    where the versions live (default ../../export_v2)
//     SOAK_WORKING_DIR=<dir> the folder the core runs in, PARENT of data/
//     SOAK_FREQ=10000        plate frequency (default 8000)
//     SOAK_NOCLEAN=1         drop the station's clean_regions for the run
//     SOAK_SEP_US=14286      gate rate ceiling (70/s)
//
// This replaces driving a Vite preview with headless Chromium. That measured a
// stack nobody ships: a different browser binary, no launcher, and a core
// started by hand. Here the launcher starts the core exactly as it will on the
// machine, the WebUI runs inside the Electron that will ship, and three things
// become measurable that were not before:
//
//   * per-process CPU and memory from app.getAppMetrics(), instead of scraping
//     tasklist for a chrome-headless-shell that will never exist in production
//   * the SUPERVISOR's own view -- missed health pings, whether a stop was ever
//     forced -- which is the launcher's opinion of the core, separate from the
//     core's opinion of itself
//   * whatever the launcher itself leaks, if anything, over hours
//
// Everything else is soak6h.mjs's, including the reasoning in its comments
// about why the browser must never let go of the machine: the core opens its
// peripheral channel on a UI PD CONNECT, so a closing window takes the channel
// with it and the board raises error 12 HOST_LINK_TIMEOUT by design. A flat
// heap on a stopped machine is the most convincing wrong answer this bench can
// produce, so the panel text and the board are sampled every single tick.
import { _electron as electron } from 'playwright';
import { execSync } from 'node:child_process';
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

const MIN = Number(process.argv[2] || 360);
const APP_ROOT = process.env.SOAK_APP_ROOT || path.join(REPO, 'export_v2');
const WORKING_DIR = process.env.SOAK_WORKING_DIR || path.join(REPO, 'InspectionCore', 'Core0_1');
const FREQ_WANT = Number(process.env.SOAK_FREQ || 8000);
// CAPPED, AND THE CAP IS PHYSICAL: above 10000 the plate throws parts off. The
// firmware clamp is 60000, which is no help -- this is the mechanical limit.
const FREQ = Math.min(FREQ_WANT, 10000);
const NOCLEAN = process.env.SOAK_NOCLEAN === '1';
const SEP = Number(process.env.SOAK_SEP_US || 14286);
// --- accelerated reproduction ------------------------------------------------
//
// The memory growth is bytes-per-frame times frames-per-second, so raising
// either shortens the time it takes to see a slope. At the normal operating
// point -- 6 fps of preview, 32 KB a frame -- the renderer climbs 10 MB/min and
// twelve minutes of data are needed before the trend is out of the noise. That
// made every hypothesis a twelve-minute round trip, which is a bad way to test
// four of them.
//
// SOAK_IMG_FPS raises the per-class preview ceiling (the same ST the
// InspectionUI's 圖像檢視側重 buttons send), and SOAK_IMG_Q raises the JPEG
// quality, which raises the bytes per frame. Together they buy roughly a
// factor of ten; with SOAK_TICK_S at 10 instead of 60, a run of three or four
// minutes settles a question.
//
// Nothing here changes the code path -- the same frames go through the same
// decode and the same canvas, just more of them.
// 0 is a MEANINGFUL value here -- it turns the preview off entirely, which is
// the cheapest way to ask whether the memory growth follows the image stream or
// the inspection reports. So presence of the variable decides, not truthiness.
const IMG_FPS = process.env.SOAK_IMG_FPS === undefined ? -1 : Number(process.env.SOAK_IMG_FPS);
const IMG_Q = Number(process.env.SOAK_IMG_Q || 0);
const TICK_S = Number(process.env.SOAK_TICK_S || 60);
// Replays each received image frame N extra times through the full production
// path. See the note at the amplifier in UI/WebUI/src/UTIL/BPG_Protocol.js.
const AMP = Number(process.env.SOAK_AMP || 0);
// Force a renderer collection every N seconds from THIS process's CDP session.
//
// The launcher has its own periodic collection (rendererGcIntervalMs), but it
// cannot run while a soak is attached: Electron allows one debugger per
// webContents, Playwright already holds it, and webContents.debugger.attach()
// throws. Measuring the launcher's feature from inside the soak therefore
// measures nothing -- which it silently did once, reporting "no effect" for a
// collection that never happened.
const GC_S = Number(process.env.SOAK_GC_S || 0);
// Write a V8 heap snapshot after this many seconds, then stop. The retainer
// analysis (tools/heap_retainers.mjs) reads it offline -- the one question the
// live counters cannot answer is WHICH object holds the detached nodes.
const SNAP_S = Number(process.env.SOAK_SNAPSHOT_S || 0);
// Set to run the UI from a Vite dev server instead of the built bundle -- see
// the note on ui in scripts/boot.js. The launcher gets it from the environment;
// the soak needs it only to recognise the page when it appears.
const INSP_UI_DEV_URL = process.env.INSP_UI_DEV_URL || '';
const SHOT_EVERY_TICK = process.env.SOAK_SHOT_EVERY === '1';
const PERIF_CONSOLE = 4099;
const OUT = 'C:/Users/w2110/Downloads/pw';
const MSET = path.join(WORKING_DIR, 'data', 'machine_setting.json');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const die = async (app, msg) => {
  console.log('FAILED: ' + msg);
  try { if (app) await app.close(); } catch {}
  process.exit(1);
};

// A core already running holds the control port exclusively, so the one the
// launcher starts would come up with no control channel and every supervisor
// reading below would be about a process we do not own.
{
  const control = require('../src/control');
  const r = await control.ping(4098, 800);
  if (r.ok) await die(null, `a core is already running (pid ${r.reply && r.reply.pid}) -- stop it first`);
}

// A throwaway launcher config, so the soak never touches the operator's.
const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'soak-launcher-'));
const cfg = new Config(userData);
cfg.values.appRoot = APP_ROOT;
cfg.values.workingDir = WORKING_DIR;
cfg.save();
console.log(`[0] launcher userData ${userData}`);
console.log(`    appRoot     ${APP_ROOT}`);
console.log(`    workingDir  ${WORKING_DIR}`);

const app = await electron.launch({
  args: [APP_DIR, `--user-data-dir=${userData}`],
  // The board console is the only way to ask the device anything while the core
  // owns the serial port. boot.js passes it through if it is set.
  env: { ...process.env, INSP_PERIF_CONSOLE: String(PERIF_CONSOLE) },
});
app.process().stderr.on('data', (d) => {
  const s = String(d).trim();
  if (s && !s.startsWith('[main] ')) console.log('  [main] ' + s.slice(0, 160));
});

const page = await app.firstWindow();

// Installed while the shell is still showing, so it is in place before the
// launcher navigates to the WebUI. ST and GS both go over the PAGE's socket: a
// second WS peer that connects first takes the peripheral-channel slot and the
// channel never comes up.
await page.addInitScript(() => {
  const Native = window.WebSocket;
  window.WebSocket = function (...a) {
    const ws = new Native(...a);
    if (String(a[0]).includes('4090')) window.__APPWS__ = ws;
    return ws;
  };
  window.WebSocket.prototype = Native.prototype;
  Object.assign(window.WebSocket, Native);
  window.__SEND_ST__ = (mset) => {
    const ws = window.__APPWS__;
    if (!ws || ws.readyState !== 1) return 'no app socket';
    const b = new TextEncoder().encode(JSON.stringify({ MachineSetting: mset }));
    const u = new Uint8Array(9 + b.length + 1);
    u[0] = 83; u[1] = 84; u[2] = 0;
    const g = (window.__STPG__ = (window.__STPG__ || 50000) + 1);
    u[3] = g >> 8; u[4] = g & 255;
    const l = u.length - 9;
    u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
    u.set(b, 9);
    ws.send(u);
    return 'sent';
  };
});
page.on('pageerror', (e) => console.log('  [pageerror]', e.message.slice(0, 110)));

console.log('[1] waiting for the launcher to start the core and hand over the window');
let onApp = false;
for (let i = 0; i < 120; i++) {
  // "WebUI" is in the built bundle's file:// path, but the dev server serves
  // the same application from http://localhost:5173/ with no such word in it,
  // so matching the path alone reported "never reached the application UI"
  // for a UI that was on screen. Match the shell instead: anything that is not
  // the launcher's own page is the app.
  const u = decodeURIComponent(page.url());
  if (/WebUI/i.test(u) || (INSP_UI_DEV_URL && u.startsWith(INSP_UI_DEV_URL))) {
    onApp = true; break;
  }
  await sleep(1000);
}
if (!onApp) await die(app, 'the launcher never reached the application UI -- ' + decodeURIComponent(page.url()));
await page.waitForLoadState('domcontentloaded');
await sleep(7000);

const rawClickText = async (label, tag = '*') => page.evaluate(({ label, tag }) => {
  for (const e of document.querySelectorAll(tag)) {
    const t = (e.innerText || '').trim();
    if (t !== label || e.children.length || !e.offsetParent) continue;
    e.click(); return true;
  } return false; }, { label, tag });
// Finds the icon FIRST, then the thing that acts on it -- not the other way
// round.
//
// The original scanned document.querySelectorAll('button'), which is only
// correct while every icon happens to sit inside a real <button>. The play
// control in the def editor does not: it is an icon in a floating toolbar, so
// the scan found nothing, and the pre-check built on the same scan reported
// "the play button stayed disabled" about a button that was enabled and on
// screen the whole time. A screenshot settled it in one look; the message would
// never have.
const iconTargets = (icon) => page.evaluate((icon) => {
  const out = [];
  for (const ic of document.querySelectorAll('[class*="' + icon + '"]')) {
    // Climb to whatever actually takes the click, falling back to the icon.
    const act = ic.closest('button, [role="button"], .ant-btn, a, li, [class*="btn"]') || ic;
    if (!act.offsetParent) continue;
    const disabled = act.disabled === true
      || act.getAttribute('aria-disabled') === 'true'
      || /(^|\s)(ant-btn-disabled|disabled)(\s|$)/.test(act.className || '');
    if (!out.some((o) => o.el === act)) out.push({ el: act, disabled });
  }
  window.__ICONS__ = out.map((o) => o.el);
  return out.map((o, i) => ({ i, disabled: o.disabled, tag: o.el.tagName }));
}, icon);

const rawClickIcon = async (icon, nth = 0) => {
  const found = await iconTargets(icon);
  const usable = found.filter((f) => !f.disabled);
  if (!usable[nth]) return false;
  return page.evaluate((i) => { const e = window.__ICONS__[i]; if (!e) return false;
    e.click(); return true; }, usable[nth].i);
};

// EVERY CLICK IS CHECKED, and waits for its target rather than assuming it is
// already there.
//
// The previous version discarded the boolean these return. A click that landed
// on nothing was therefore silent, and the run failed thirty seconds later with
// "machine is not running" -- which points at the machine, at the board, at the
// plate: everywhere except the click that never happened. One bring-up did fail
// exactly that way and the screenshot showed the UI still on its empty
// placeholder, no recipe loaded.
//
// It also stopped working for an interesting reason. The clear-the-board block
// above only runs when the board comes up dirty, and it happens to take about
// ten seconds -- which is how long the UI needed to finish loading. On a run
// where the board was already clean that block was skipped, the incidental
// delay went with it, and the clicks arrived too early. Waiting for the target
// removes the dependency on an accident.
const waitClick = async (what, fn, tries = 30) => {
  for (let i = 0; i < tries; i++) {
    if (await fn()) return true;
    await sleep(1000);
  }
  await page.screenshot({ path: OUT + '/esoak_FAILED.png' });
  await die(app, `could not click "${what}" after ${tries}s -- see esoak_FAILED.png`);
  return false;
};
const clickText = (label, tag = '*') => waitClick(label, () => rawClickText(label, tag));
const clickIcon = (icon, nth = 0) => waitClick(icon + '#' + nth, () => rawClickIcon(icon, nth));

// DRAWERS ONLY -- never modals.
//
// This closed `.ant-modal-close` too, and the recipe browser IS an ant modal:
// the helper meant to clear obstructions was closing the very dialog the next
// click needed. The def then never opened, and the WebUI threw
// "Cannot read properties of undefined (reading 'callBack')" because a
// selection arrived after its own dialog state had been torn down.
//
// A diagnostics drawer really does swallow clicks aimed behind it, so the
// helper earns its place -- but only for the thing that is in the way, not for
// anything that happens to be dismissible.
const closeDrawers = () => page.evaluate(() => {
  let n = 0;
  for (const x of document.querySelectorAll('.ant-drawer-close')) {
    if (x.offsetParent) { x.click(); n++; }
  }
  return n;
}).catch(() => 0);

// A visible-text probe, for asserting that a step actually took effect rather
// than that a button was pressable.
const seeText = (needle) => page.evaluate((n) => document.body.innerText.includes(n), needle)
  .catch(() => false);
const waitText = async (needle, what, tries = 40) => {
  for (let i = 0; i < tries; i++) {
    if (await seeText(needle)) return true;
    await sleep(1000);
  }
  await page.screenshot({ path: OUT + '/esoak_FAILED.png' });
  await die(app, `${what}: never saw "${needle}" -- see esoak_FAILED.png`);
  return false;
};

// Optional: only present when a camera is not attached.
await rawClickText('跳過相機連線', 'button'); await sleep(1500);

// ---- the board console ----------------------------------------------------
//
// RECONNECTING, and that is the point. The peripheral channel is opened by the
// PAGE's PD CONNECT, not by the core starting, so connecting the moment the
// page paints races it: the core accepts the TCP connect and then RESETS the
// socket because it has no channel behind it yet.
let sock = null, buf = '', lines = [];
let id = 97000;
function connectConsole() {
  return new Promise((resolve) => {
    const s2 = net.connect(PERIF_CONSOLE, '127.0.0.1');
    let settled = false;
    const done = (v) => { if (!settled) { settled = true; resolve(v); } };
    s2.on('data', (d) => { buf += d.toString('latin1'); let n;
      while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); } });
    s2.on('error', () => done(null));
    s2.on('close', () => { if (sock === s2) sock = null; });
    s2.once('connect', () => done(s2));
  });
}
async function ask(o, ms = 1800) {
  if (!sock) { sock = await connectConsole(); if (!sock) return null; buf = ''; }
  const my = id++; lines = [];
  try { sock.write(JSON.stringify({ ...o, id: my }) + '\n'); } catch { sock = null; return null; }
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return null; }
}

console.log('[2] waiting for the board console');
let st = null;
for (let i = 0; i < 20 && !st; i++) {
  st = await ask({ type: 'get_running_stat' }, 2500);
  if (!st) { sock = null; await sleep(3000); }
}
if (!st) await die(app, 'board not answering after 20 tries (is INSP_PERIF_CONSOLE reaching the core?)');
console.log(`    state ${st.state}  err ${JSON.stringify(st.error_hist)}`);

// exit_insp_mode BEFORE clear_error, or 112 never clears: the board is still IN
// inspection mode and clear_error alone does not take it out.
if (st.state !== 100 || (st.error_hist || []).length) {
  console.log('[3] clearing');
  await ask({ type: 'set_setup', plate: { freq: 0 } }, 1500);
  await sleep(2500);
  await ask({ type: 'exit_insp_mode' }, 2000);
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'clear_error_history' }, 1200);
  await sleep(1500);
  await ask({ type: 'exit_insp_mode' }, 1500);   // clear_error puts it back IN
  st = await ask({ type: 'get_running_stat' }, 1800);
  console.log(`    now state ${st && st.state}  err ${JSON.stringify(st && st.error_hist)}`);
}

// Pin the gate ceiling rather than inheriting whatever the board holds:
// min_detect_sep_us lives in RAM unless save_setup was called, so a flash
// silently reverts it and the run stops being comparable to the previous ones.
await ask({ type: 'set_setup', gate: { min_detect_sep_us: SEP } }, 1800);
const sc = await ask({ type: 'get_setup' }, 2000);
const sepNow = sc && sc.gate ? sc.gate.min_detect_sep_us : -1;
console.log(`[4] min_detect_sep_us = ${sepNow} (${sepNow > 0 ? (1e6 / sepNow).toFixed(1) : '?'}/s ceiling)`);
if (sepNow !== SEP) await die(app, `gate ceiling did not take (wanted ${SEP})`);

const fr = await ask({ type: 'set_setup', plate: { freq: FREQ } }, 1800);
console.log(`[5] plate freq = ${FREQ}${FREQ_WANT > FREQ ? ` (capped from ${FREQ_WANT})` : ''}`
          + ` -> ${fr && fr.ack ? 'ack' : 'NOT ACKED'}`);

console.log('[6] recipe + 製程 + 檢測方式');
await clickIcon('anticon-folder-open');
await waitText('test1.hydef', 'the recipe browser never listed the file');
// ASSERT THE OUTCOME -- AND PICK A MARKER THAT ONLY EXISTS AFTERWARDS.
//
// Three attempts failed here, each for a different surface reason, and the
// third one is the instructive one: I checked for the text 檢測方式 as proof
// that the recipe had loaded. 檢測方式 is a section of the tag panel that is
// on screen BEFORE anything is opened, so the check passed every time and the
// run sailed on to die at the play button -- two steps and thirty seconds away
// from the thing that had actually not happened.
//
// A marker has to be something the state change CREATES. 已設定範圍 is empty
// until a def is open and lists its declared scope once one is, so counting
// what is inside it answers the real question.
const defScopeCount = () => page.evaluate(() => {
  const vis = [...document.querySelectorAll('*')].filter((e) => e.offsetParent);
  const txt = (e) => (e.innerText || '').trim();
  const pickHeader = (name) => {
    const h = vis.filter((e) => txt(e) === name);
    if (!h.length) return -1;
    return vis.indexOf(h.reduce((a, b) =>
      a.querySelectorAll('*').length <= b.querySelectorAll('*').length ? a : b));
  };
  const a = pickHeader('已設定範圍');
  const b = pickHeader('製程');
  if (a < 0 || b < 0 || b <= a) return -1;
  return vis.slice(a + 1, b).filter((e) => !e.children.length && txt(e) && txt(e).length < 16).length;
}).catch(() => -1);

{
  const item = page.locator('text="test1.hydef"').first();
  let loaded = 0;
  for (let attempt = 0; attempt < 4 && loaded <= 0; attempt++) {
    try {
      if (attempt === 0) { await item.click({ timeout: 8000 }); await sleep(500); }
      await item.dblclick({ timeout: 8000 });
    } catch { /* the outcome check decides, not the gesture */ }
    for (let i = 0; i < 10 && loaded <= 0; i++) { loaded = await defScopeCount(); if (loaded <= 0) await sleep(1000); }
    if (loaded <= 0 && attempt < 3) {
      console.log(`    def not open yet (已設定範圍 holds ${loaded} items), retrying`);
      await clickIcon('anticon-folder-open');
      await waitText('test1.hydef', 'the recipe browser never listed the file again');
    }
  }
  if (loaded <= 0) {
    await page.screenshot({ path: OUT + '/esoak_FAILED.png' });
    await die(app, 'test1.hydef never opened -- 已設定範圍 stayed empty, so no def is loaded '
                 + '(the tag panel is on screen either way, which is what fooled the earlier check). '
                 + 'See esoak_FAILED.png');
  }
  console.log(`    def open, 已設定範圍 lists ${loaded} item(s)`);
}

// The click TARGETS here are the original sequence's, unchanged, and that is
// deliberate.
//
// They look wrong: "11沖壓成形" appears in both 已設定範圍 and 製程, "全檢" in
// both 製程 and 檢測方式, so a first-match-by-text click is ambiguous. I
// "fixed" that by scoping each click to its section -- and broke a bring-up
// that had been working, because the ambiguity was load-bearing. Scoped
// properly, 已設定範圍 came up EMPTY, the chosen 製程 was then outside the
// recipe's declared scope, and the play button greyed out exactly as the recipe
// intends when you pick something it does not allow.
//
// The original failure was never here. It was upstream: the recipe had not
// loaded at all, and the screenshot showed the UI still on its empty
// placeholder. Everything below the load was collateral.
//
// So: same clicks, plus the checks that do not change what is clicked --
// waiting for the target, closing a drawer that would swallow the click, and
// asserting the outcome afterwards.
await closeDrawers();
await clickText('11沖壓成形'); await sleep(1200);
await closeDrawers();
await clickText('全檢'); await sleep(1500);

// The OUTCOME of those selections is an enabled play control. Checking it here
// reports a bad selection where it happened, instead of as an unfindable button
// three steps later.
{
  let found = [];
  for (let i = 0; i < 20; i++) {
    found = await iconTargets('anticon-caret-right');
    if (found.some((f) => !f.disabled)) break;
    await sleep(1000);
  }
  if (!found.some((f) => !f.disabled)) {
    await page.screenshot({ path: OUT + '/esoak_FAILED.png' });
    await die(app, 'no enabled play control after selecting 製程/檢測方式; found: '
      + (found.length ? found.map((f) => f.tag + (f.disabled ? '(disabled)' : '')).join(', ')
                      : 'none at all')
      + ' -- see esoak_FAILED.png');
  }
}

console.log('[7] into the Inspection UI');
await closeDrawers();
await clickIcon('anticon-caret-right');
await waitText('工位', 'the Inspection UI never opened');
await sleep(4000);

// Clean regions OFF BEFORE the machine starts, so not one part is judged under
// a configuration this run is not measuring.
if (NOCLEAN) {
  const mset = JSON.parse(fs.readFileSync(MSET, 'utf8'));
  const sent = await page.evaluate((m) => window.__SEND_ST__(m),
                                   { inspection_region: mset.inspection_region, clean_regions: [] });
  console.log(`[8] clean_regions -> [] : ${sent} (was ${(mset.clean_regions || []).length}; runtime only)`);
  if (sent !== 'sent') await die(app, 'could not send ST');
  await sleep(4000);
}

console.log('[9] start the machine');
await closeDrawers();
await clickIcon('anticon-caret-right'); await sleep(9000);
// Say WHICH half is wrong. "edges climbing but state 100" means the plate is
// turning and the board is not inspecting, which is a different problem from
// "nothing is moving" -- and the old message named neither.
await page.evaluate(() => { const x = document.querySelector('.ant-drawer-close');
  if (x && x.offsetParent) x.click(); });
await sleep(2000);

const edges = (s) => (s && s.yield && s.yield.gate ? s.yield.gate.in : -1);
const a0 = await ask({ type: 'get_running_stat' }, 1800);
await sleep(10000);
const b0 = await ask({ type: 'get_running_stat' }, 1800);
console.log(`[10] state ${b0 && b0.state}  edges ${edges(a0)} -> ${edges(b0)}`);
if (!b0 || b0.state !== 101 || edges(b0) <= edges(a0)) {
  await page.screenshot({ path: OUT + '/esoak_FAILED.png' });
  const moving = b0 && edges(b0) > edges(a0);
  await die(app, !b0 ? 'the board stopped answering during start-up'
    : moving ? `the plate IS turning (${edges(a0)} -> ${edges(b0)}) but the board is in state `
             + `${b0.state}, not 101 -- it never entered inspection mode`
             : `no gate edges in 10 s (${edges(a0)} -> ${edges(b0)}), state ${b0.state} `
             + '-- the plate is not turning');
}
await page.screenshot({ path: OUT + '/esoak_start.png' });

// Applied AFTER the machine is up, so the run's first samples are already at
// the accelerated rate rather than showing a step in the middle of the data.
if (IMG_FPS >= 0 || IMG_Q > 0) {
  const sent = await page.evaluate(({ fps, q }) => {
    const ws = window.__APPWS__;
    if (!ws || ws.readyState !== 1) return 'no app socket';
    const body = { ImageTransferSetup: {} };
    if (fps >= 0) {
      body.ImageTransferSetup.OK_MAX_FPS = fps;
      body.ImageTransferSetup.NG_MAX_FPS = fps;
      body.ImageTransferSetup.NA_MAX_FPS = fps;
    }
    // TOP LEVEL, not inside ImageTransferSetup. The core reads this one with
    // JFetch_NUMBER(json, ...) at wiringPanel.cpp:6126 -- `json`, not the
    // ImageTransferSetup object it fetched a few hundred lines earlier. Nested,
    // it is silently ignored: the ST is acked, nothing changes, and the frame
    // size in the next sample is the giveaway.
    if (q > 0) body.IMG_STREAMING_JPEG_QUALITY = q;
    const b = new TextEncoder().encode(JSON.stringify(body));
    const u = new Uint8Array(9 + b.length + 1);
    u[0] = 83; u[1] = 84; u[2] = 0;
    const g = (window.__STPG2__ = (window.__STPG2__ || 51000) + 1);
    u[3] = g >> 8; u[4] = g & 255;
    const l = u.length - 9;
    u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
    u.set(b, 9);
    ws.send(u);
    return 'sent';
  }, { fps: IMG_FPS, q: IMG_Q });
  console.log(`[11] image stream: fps=${IMG_FPS < 0 ? 'unchanged' : IMG_FPS} `
            + `quality=${IMG_Q || '-'} -> ${sent}`);
  await sleep(4000);
}
if (AMP > 0) {
  const ok = await page.evaluate((n) => { window.__DIAG_AMPLIFY__ = n; return window.__DIAG_AMPLIFY__; }, AMP);
  console.log(`[11] amplifier x${ok} (each frame replayed ${ok} extra times through the full path)`);
  await sleep(3000);
}
if (TICK_S !== 60) console.log(`[11] sampling every ${TICK_S}s`);

// ---- sampling ---------------------------------------------------------------

// Per-process CPU and memory straight from Electron, which is both more honest
// and more detailed than scraping tasklist: it names each process by ROLE, so
// "the renderer is growing" and "the GPU process is growing" stop being one
// number. percentCPUUsage is a percentage of ONE core.
const metrics = () => app.evaluate(({ app: a }) => {
  const out = { cpu: 0, rssMB: 0, byType: {} };
  for (const m of a.getAppMetrics()) {
    const cpu = (m.cpu && m.cpu.percentCPUUsage) || 0;
    const mb = ((m.memory && m.memory.workingSetSize) || 0) / 1024;
    out.cpu += cpu;
    out.rssMB += mb;
    const k = m.type || 'other';
    out.byType[k] = out.byType[k] || { cpu: 0, mb: 0 };
    out.byType[k].cpu += cpu;
    out.byType[k].mb += mb;
  }
  out.cpu = +out.cpu.toFixed(1);
  out.rssMB = Math.round(out.rssMB);
  return out;
}).catch(() => null);

// The SUPERVISOR's opinion of the core, which is not the same as the core's
// opinion of itself: missed pings say the launcher stopped getting answers,
// and that has been seen with every port still bound.
const supervisor = () => page.evaluate(async () => {
  try {
    if (!window.launcher) return null;
    const s = await window.launcher.status();
    return {
      running: s.core.running,
      pid: s.core.pid,
      missed: s.core.missedPings,
      unresponsive: s.core.unresponsive,
      forced: s.core.lastStopWasForced,
      healthAgeS: s.core.lastHealth ? (Date.now() - s.core.lastHealth.at) / 1000 : null,
      coreUptimeS: s.core.lastHealth && s.core.lastHealth.info
                   ? s.core.lastHealth.info.uptime_s : null,
    };
  } catch { return null; }
}).catch(() => null);

// The core is NOT in app.getAppMetrics(). Electron reports its own processes
// -- main, renderer, GPU, utility -- and the core is a child it spawned and
// knows nothing about. Reporting only elCPU would say the machine uses 5% of a
// core while the process doing the actual inspecting went unmeasured, which on
// a two-core target is the wrong half of the answer.
//
// RSS and cumulative processor time in ONE PowerShell call, because two would
// be two ~300 ms spawns a minute. .CPU is total processor SECONDS since start
// (user + kernel), so a percentage has to come from the delta between ticks --
// the absolute value is meaningless on its own and grows forever.
const coreProc = () => { try {
  // SINGLE quotes inside the PowerShell script, and none of them escaped.
  //
  // The first version formatted with a double-quoted PowerShell string, which
  // meant a bare " landed inside the cmd.exe -Command "..." argument and
  // truncated it. Every sample came back -1 for a core that was running and
  // visible in Task Manager, and the surrounding try/catch swallowed the
  // reason. It survived a hand test only because the shell I tested from
  // escaped the quotes differently than Node does -- so the test and the code
  // were not running the same command, which is the least useful kind of
  // passing test.
  const o = execSync('powershell -NoProfile -NonInteractive -Command '
    + '"$p=Get-Process visSele -ErrorAction SilentlyContinue | Select-Object -First 1;'
    + " if($p){'{0} {1}' -f [int]($p.WorkingSet64/1MB),$p.CPU}\"",
    { encoding: 'utf8', timeout: 20000 });
  const m = o.trim().split(/\s+/);
  if (m.length < 2) return { rss: -1, cpuSec: null };
  const rss = Number(m[0]), cpuSec = Number(m[1]);
  return { rss: Number.isFinite(rss) ? rss : -1,
           cpuSec: Number.isFinite(cpuSec) ? cpuSec : null };
} catch { return { rss: -1, cpuSec: null }; } };

// The WebUI's own observation point (UI/WebUI/src/UTIL/diagProbe.js).
//
// Absent on an older payload, which is not an error -- the run just carries
// empty columns and says so once.
let diagWarned = false;
const diag = async () => {
  const d = await page.evaluate(() => (window.__DIAG__ ? window.__DIAG__() : null)).catch(() => null);
  if (!d && !diagWarned) {
    diagWarned = true;
    console.log('  [diag] window.__DIAG__ is not present in this payload -- '
              + 'the array census columns will be empty');
  }
  return d;
};

// THE DECISIVE TEST: force a collection and see whether the memory comes back.
//
// Renderer RSS climbs ~10 MB/min while the JS heap sits flat at 28 MB and every
// array in the store stays the same length. Nothing is being RETAINED in JS --
// so either something off-heap is genuinely leaked, or it is ordinary garbage
// that V8 has no reason to collect. A 28 MB heap generates no collection
// pressure, and Blob bytes live in Chromium's blob store where the heap cannot
// see them: the wrappers stay alive, uncollected, and their payloads pile up.
//
// Those two possibilities look identical on every graph drawn so far and have
// completely different fixes, and one command tells them apart.
let gcSession = null;
const forceGC = async () => {
  try {
    if (!gcSession) gcSession = await page.context().newCDPSession(page);
    await gcSession.send('HeapProfiler.collectGarbage');
    return true;
  } catch (e) { return false; }
};

// Documents, DOM nodes and JS EVENT LISTENERS, straight from Chromium.
//
// The renderer retains ~33 KB per received frame, off the JS heap, and a forced
// collection does not reclaim it -- so something is HOLDING it. A listener
// registered per frame and never removed has exactly that shape: the event
// target keeps it alive, so it is not garbage at all and no amount of
// collecting will help.
//
// jsEventListeners climbing in step with frames received would settle it.
const domCounters = async () => {
  try {
    if (!gcSession) gcSession = await page.context().newCDPSession(page);
    const r = await gcSession.send('Memory.getDOMCounters');
    return r;
  } catch (e) { return null; }
};

// How many objects of each DOM class are ALIVE, including ones no longer in the
// document.
//
// The element census counted what querySelectorAll can see -- about 250
// elements -- while Chromium reported 6116 nodes and climbing. The difference
// is DETACHED nodes: created, removed from the tree, and still referenced by
// something, so they are neither visible to a walk of the document nor
// collectable. That is exactly the shape of the growth, and neither of the two
// instruments used so far could see it.
//
// Runtime.queryObjects performs a collection before it answers, so what it
// returns is retained, not merely uncollected. Reported per class so the growth
// names an element type rather than a number.
const DOM_CLASSES = ['HTMLDivElement', 'HTMLCanvasElement', 'HTMLSpanElement',
                     'HTMLImageElement', 'HTMLButtonElement', 'HTMLTableRowElement',
                     'Text', 'SVGSVGElement'];
const liveDom = async () => {
  try {
    if (!gcSession) gcSession = await page.context().newCDPSession(page);
    const out = {};
    for (const cls of DOM_CLASSES) {
      const ev = await gcSession.send('Runtime.evaluate',
        { expression: `typeof ${cls} === 'function' ? ${cls}.prototype : null` });
      if (!ev.result || !ev.result.objectId) continue;
      const q = await gcSession.send('Runtime.queryObjects', { prototypeObjectId: ev.result.objectId });
      const n = await gcSession.send('Runtime.callFunctionOn', {
        objectId: q.objects.objectId,
        functionDeclaration: 'function(){ return this.length; }',
        returnByValue: true,
      });
      out[cls] = n.result.value;
      // The count says how many; a sample says WHICH. The last entries are the
      // most recently created, so they are the ones being produced now rather
      // than left over from start-up.
      if (cls === 'HTMLDivElement' && n.result.value > 40) {
        const sam = await gcSession.send('Runtime.callFunctionOn', {
          objectId: q.objects.objectId,
          functionDeclaration: `function(){
            const out = [];
            // DETACHED ONLY. The array holds every live instance, and the
            // tail of it is whatever the document happens to contain -- the
            // first attempt sampled the panel headers and said nothing about
            // the ones piling up.
            const det = this.filter((e) => !e.isConnected);
            for (const e of det.slice(-6)) {
              out.push((e.isConnected ? 'ATTACHED ' : 'detached ')
                + '<' + e.tagName.toLowerCase()
                + (e.className ? ' class="' + String(e.className).slice(0,60) + '"' : '') + '> '
                + (e.textContent || '').replace(/\s+/g,' ').slice(0,60));
            }
            return out.join(' || ');
          }`,
          returnByValue: true,
        }).catch(() => null);
        if (sam && sam.result) out.__sample = sam.result.value;
      }
      await gcSession.send('Runtime.releaseObject', { objectId: q.objects.objectId }).catch(() => {});
      await gcSession.send('Runtime.releaseObject', { objectId: ev.result.objectId }).catch(() => {});
    }
    return out;
  } catch (e) { return null; }
};

const panelText = () => page.evaluate(() => {
  for (const e of document.querySelectorAll('div')) {
    const t = (e.innerText || '');
    if (t.includes('rpm') && t.includes('/s') && t.length < 260) return t.replace(/\s+/g, ' ').trim();
  } return '';
}).catch(() => '');

const vd = (s) => (s && s.yield && s.yield.verdict) || {};
const so = (s) => (s && s.yield && s.yield.sort) || {};
const rl = (s) => (s && s.report_latency) || {};
const ct = (s) => (s && s.count) || {};
const cs = (s) => (s && s.cam_sync) || {};

const coreCounters = () => page.evaluate(async () => {
  try {
    const ws = window.__APPWS__;
    if (!ws || ws.readyState !== 1) return null;
    return await new Promise((res) => {
      const t = setTimeout(() => { ws.removeEventListener('message', h); res(null); }, 2500);
      const h = (ev) => {
        const b = new Uint8Array(ev.data);
        if (String.fromCharCode(b[0], b[1]) !== 'GS') return;
        clearTimeout(t); ws.removeEventListener('message', h);
        try {
          const j = JSON.parse(new TextDecoder().decode(b.subarray(9)).replace(/\0+$/, ''));
          const p = j.perif_pairing || (j.data && j.data.perif_pairing) || {};
          res({ lk: p.link || {}, ct: p.cam_trig || {} });
        } catch { res(null); }
      };
      ws.addEventListener('message', h);
      const body = new TextEncoder().encode(JSON.stringify({ items: ['perif_pairing'] }));
      const u = new Uint8Array(9 + body.length + 1);
      u[0] = 71; u[1] = 83; u[2] = 0;
      const g = (window.__GSPG__ = (window.__GSPG__ || 60000) + 1);
      u[3] = g >> 8; u[4] = g & 255;
      const l = u.length - 9;
      u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
      u.set(body, 9);
      ws.send(u);
    });
  } catch { return null; }
}).catch(() => null);

// The tail bucket count as a DELTA. A cumulative histogram hides WHEN the tail
// happened, and cam_max_us is a high-water that only ever says "at some point".
const HI = (h) => (Array.isArray(h) ? (h[5] || 0) + (h[6] || 0) + (h[7] || 0) : 0);

let prev = null;
console.log('');
console.log('t_min,heapMB,totalMB,elRSS_MB,elCPU,rendMB,gpuMB,coreRSS_MB,coreCPU,cpuTotal,domNodes,'
          + 'sup_missed,sup_unresp,state,'
          + 'seen_s,admit_s,sorted_s,na_s,skip,unans,'
          + 'nm_orphan,nm_window,nm_consec,statwin_ms,'
          + 'ackfalse,locked,unapplied,frame_gap,frame_lost,'
          + 'lat_avg_ms,lat_max_ms,lat_tail_n,'
          + 'resid_us,resid_max_us,dmax_us,ts_rej,cal_fail,cal_lost,win_us,drift_us_s,'
          + 'rafHz,imgHz,imgKBps,frameKB,imgW,imgScale,gradeMismatch,tagActive,tagApplied,tagDrift,vis,arrays,nodes,gc_before,gc_after,gc_freed,'
          + 'dom_nodes,dom_listeners,dom_docs,'
          + 'err,panel,census,domcensus,livedom,divsample');
const t0 = Date.now();
let faults = 0;
let shotCards = false;

// Zoom test: does zooming IN restore full resolution?
//
// The stream level is chosen from how many sensor pixels land on one canvas
// pixel, so a zoomed-out view legitimately gets a small image -- 204 px wide
// when the part is drawn 145 px across. The failure mode that matters is the
// other direction: if nothing re-negotiates on zoom IN, the operator magnifies
// the part and gets a magnified 204 px image, which looks like a broken camera
// rather than a stale setting. Only the mouse handlers emit, so this exercises
// the real wheel path rather than calling the emitter directly.
const ZOOM_TEST = process.env.SOAK_ZOOM_TEST === '1';
const zoomProbe = async () => {
  const d = await page.evaluate(() => ({
    dg: window.__DIAG__ ? window.__DIAG__() : null,
    ov: window.__STREAM_OVERSAMPLE__,
  })).catch(() => null);
  return d && d.dg
    ? { w: d.dg.imgW, scale: d.dg.imgScale, ov: d.ov }
    : { w: null, scale: null, ov: null };
};
if (ZOOM_TEST) {
  const box = await page.evaluate(() => {
    // The biggest canvas on the page is the live view; the others are the
    // thumbnail strip and the amplifier's scratch buffer.
    let best = null;
    for (const c of document.querySelectorAll('canvas')) {
      const r = c.getBoundingClientRect();
      if (r.width < 50 || r.height < 50) continue;
      if (!best || r.width * r.height > best.w * best.h)
        best = { x: r.x + r.width / 2, y: r.y + r.height / 2, w: r.width, h: r.height };
    }
    return best;
  }).catch(() => null);
  if (!box) console.log('[zoom] no canvas found -- skipping');
  else {
    console.log(`[zoom] canvas ${Math.round(box.w)}x${Math.round(box.h)}`);
    await page.mouse.move(box.x, box.y);
    // BOTH directions, and a screenshot at each stop.
    //
    // The first version drove 12 notches of one sign, saw the level stay at 4
    // and called it a failure. That proves nothing on its own: 4 is the top of
    // the range, so a sign convention that zooms OUT produces exactly the same
    // reading as a wheel event that never arrived. The shots settle which of
    // the three actually happened.
    // Dispatched on the element, not driven through page.mouse.wheel.
    //
    // page.mouse.wheel produced no effect at all here: twelve notches left the
    // part drawn the same ~150 px across, so the reading it produced was a
    // statement about the harness and not about the code under test. The
    // canvas listens with a plain addEventListener('wheel', ...) and the
    // handler does not check isTrusted, so a dispatched event exercises the
    // identical path -- onmouseswheel -> scaleCanvas -> debounce_zoom_emit.
    const wheelOn = (dy, n) => page.evaluate(({ dy, n }) => {
      let best = null;
      for (const c of document.querySelectorAll('canvas')) {
        const r = c.getBoundingClientRect();
        if (r.width < 50 || r.height < 50) continue;
        if (!best || r.width * r.height > best.a) best = { el: c, r, a: r.width * r.height };
      }
      if (!best) return false;
      const cx = best.r.x + best.r.width / 2, cy = best.r.y + best.r.height / 2;
      best.el.dispatchEvent(new MouseEvent('mousemove',
        { clientX: cx, clientY: cy, bubbles: true }));
      for (let i = 0; i < n; i++) {
        best.el.dispatchEvent(new WheelEvent('wheel',
          { deltaY: dy, clientX: cx, clientY: cy, bubbles: true, cancelable: true }));
      }
      return true;
    }, { dy, n }).catch(() => false);

    const stage = async (label, dy, n) => {
      if (n > 0) await wheelOn(dy, n);
      // The emit is throttled at 500 ms and the core then has to encode and
      // send at least one frame at the new level; reading sooner reports the
      // old number and calls it a failure.
      await sleep(4000);
      const p2 = await zoomProbe();
      await page.screenshot({ path: `${OUT}/zoom_${label}.png` }).catch(() => {});
      console.log(`[zoom] ${label.padEnd(9)} imgW=${p2.w} scale=${p2.scale}`
                + ` oversample=${p2.ov == null ? '?' : (+p2.ov).toFixed(2)}`);
      return p2;
    };
    // NEGATIVE deltaY is zoom IN, and the step is small: twelve notches
    // moved the part about 1.14x, while crossing from oversample ~5.4 down
    // past the level-2 threshold needs roughly 1.5x. Both earlier FAILs were
    // the test's fault, not the code's -- the first drove page.mouse.wheel,
    // which never reached the canvas at all, and the second used too few
    // notches to cross a threshold. Neither was evidence about zoom handling.
    //
    // The verdict below reads imgScale, which comes from the IM header, rather
    // than anyone's reading of a screenshot: two attempts at eyeballing the
    // drawn width from these shots gave contradictory answers.
    const before = await stage('base', 0, 0);
    // 150 notches. Sixty moved oversample 8.16 -> 4.49, which is a working
    // renegotiation and still above the level-4 threshold of 3.6, so the level
    // correctly did not move -- and the test called that a failure three times
    // running. Crossing the boundary from 8.16 needs about 2.3x.
    const zin = await stage('zoom_in', -240, 150);
    const zout = await stage('zoom_out', 240, 300);
    const ok = before.scale != null && zin.scale != null && zin.scale < before.scale;
    console.log(`[zoom] ${ok ? 'PASS' : 'FAIL'} -- zooming in ${ok ? 'lowered' : 'did NOT lower'}`
              + ` the level (base ${before.scale} -> in ${zin.scale} -> out ${zout.scale})`);
  }
}
const TICKS = Math.max(1, Math.round((MIN * 60) / TICK_S));
for (let i = 0; i <= TICKS; i++) {
  const v = await page.evaluate(() => { const m = performance.memory || {};
    return { h: +(m.usedJSHeapSize / 1048576).toFixed(1), t: +(m.totalJSHeapSize / 1048576).toFixed(1),
             d: document.getElementsByTagName('*').length }; }).catch(() => null);
  // reset_stat_maximum: every row's lat_max / resid / dmax is "the worst in
  // THIS tick", not a lifetime high-water. stat_max_window_ms proves what it
  // covers -- if another reader resets in between, the window comes back short
  // and the row says so instead of quietly reporting a 60 s max it never had.
  const s = await ask({ type: 'get_running_stat', reset_stat_maximum: true }, 1500);
  const pt = await panelText();
  const cc = await coreCounters();
  const em = await metrics();
  const sv = await supervisor();
  const dg = await diag();
  const dc = await domCounters();
  // Expensive (it collects first), so only every fourth sample.
  const ld = (i % 4 === 0) ? await liveDom() : null;

  // Every tenth minute, and only then: forcing a collection changes what is
  // being measured, so it must not happen on every sample.
  let gcBefore = '', gcAfter = '', gcFreed = '';
  if (GC_S > 0 && i > 0 && (i * TICK_S) % GC_S === 0) {
    const b0 = em ? em.rssMB : null;
    if (await forceGC()) {
      await sleep(1200);
      const a0 = await metrics();
      if (b0 != null && a0) console.log(`  [gc] ${b0} -> ${a0.rssMB} MB (freed ${b0 - a0.rssMB})`);
    }
  }
  if (GC_S === 0 && i > 0 && i % Math.max(1, Math.round(600 / TICK_S)) === 0) {
    const b = em ? em.rssMB : null;
    if (await forceGC()) {
      await sleep(2500);
      const a2 = await metrics();
      if (b != null && a2) {
        gcBefore = b; gcAfter = a2.rssMB; gcFreed = b - a2.rssMB;
        console.log(`  [gc] forced collection at t=${((Date.now() - t0) / 60000).toFixed(1)} min: `
                  + `${b} -> ${a2.rssMB} MB (freed ${gcFreed})`);
      }
    }
  }
  const errs = JSON.stringify((s && s.error_hist) || []);

  const dt = prev ? (Date.now() - prev.at) / 1000 : 0;
  const r = (now, was) => (dt > 0 && now >= 0 && was >= 0 ? ((now - was) / dt).toFixed(1) : '');
  const cp = coreProc();
  const cur = { at: Date.now(), seen: edges(s), adm: (s && s.yield && s.yield.gate ? s.yield.gate.out : -1),
                srt: so(s).out !== undefined ? so(s).out : -1, na: so(s).na !== undefined ? so(s).na : -1,
                hi: HI(rl(s).cam_hist), cpuSec: cp.cpuSec };
  const us2ms = (u) => (u === undefined ? '' : (u / 1000).toFixed(1));
  const byType = (k, f) => (em && em.byType[k] ? Math.round(em.byType[k][f]) : '');
  // Percent of ONE core, matching how app.getAppMetrics() reports Electron's,
  // so the two can be added.
  const coreCPU = (prev && prev.cpuSec != null && cp.cpuSec != null && dt > 0)
    ? +(((cp.cpuSec - prev.cpuSec) / dt) * 100).toFixed(1) : '';
  const cpuTotal = (coreCPU !== '' && em) ? +(coreCPU + em.cpu).toFixed(1) : '';

  console.log([((Date.now() - t0) / 60000).toFixed(1),
               v ? v.h : 'err', v ? v.t : 'err',
               em ? em.rssMB : 'err', em ? em.cpu : 'err',
               byType('Tab', 'mb'), byType('GPU', 'mb'),
               cp.rss, coreCPU, cpuTotal, v ? v.d : 'err',
               sv ? sv.missed : '', sv ? (sv.unresponsive ? 1 : 0) : '',
               s ? s.state : '?',
               prev ? r(cur.seen, prev.seen) : '', prev ? r(cur.adm, prev.adm) : '',
               prev ? r(cur.srt, prev.srt) : '', prev ? r(cur.na, prev.na) : '',
               vd(s).skip !== undefined ? vd(s).skip : '',
               vd(s).unanswered !== undefined ? vd(s).unanswered : '',
               ct(s).NOMATCH_ORPHAN !== undefined ? ct(s).NOMATCH_ORPHAN : '',
               ct(s).NOMATCH_WINDOW !== undefined ? ct(s).NOMATCH_WINDOW : '',
               ct(s).NOMATCH_CONSEC !== undefined ? ct(s).NOMATCH_CONSEC : '',
               s && s.stat_max_window_ms !== undefined ? s.stat_max_window_ms : '',
               cc && cc.lk ? (cc.lk.ack_false ?? '') : '',
               cc && cc.lk ? (cc.lk.locked_seen ?? '') : '',
               cc && cc.lk ? (cc.lk.unapplied ?? '') : '',
               cc && cc.ct ? (cc.ct.frame_gap_n ?? '') : '',
               cc && cc.ct ? (cc.ct.frame_lost ?? '') : '',
               us2ms(rl(s).cam_avg_us), us2ms(rl(s).cam_max_us),
               prev ? cur.hi - prev.hi : '',
               cs(s).resid_us !== undefined ? cs(s).resid_us : '',
               cs(s).resid_max_us !== undefined ? cs(s).resid_max_us : '',
               cs(s).delta_max_us !== undefined ? cs(s).delta_max_us : '',
               cs(s).rejected !== undefined ? cs(s).rejected : '',
               cs(s).cal_fails !== undefined ? cs(s).cal_fails : '',
               cs(s).cal_pulse_lost !== undefined ? cs(s).cal_pulse_lost : '',
               cs(s).window_us !== undefined ? cs(s).window_us : '',
               cs(s).drift_us_per_s !== undefined ? Number(cs(s).drift_us_per_s).toFixed(1) : '',
               dg ? dg.rafHz : '', dg ? dg.msgHz : '',
               dg ? (dg.imgKBps ?? '') : '', dg ? (dg.lastFrameKB ?? '') : '',
               dg ? (dg.imgW ?? '') : '', dg ? (dg.imgScale ?? '') : '',
               dg ? (dg.gradeMismatch ?? '') : '',
               dg ? (dg.tagActive ?? '') : '', dg ? (dg.tagApplied ?? '') : '',
               dg ? (dg.tagDrift ?? '') : '',
               dg ? (dg.vis + (dg.hidden ? '/hidden' : '')) : '',
               dg ? dg.arrayCount : '', dg ? dg.nodes : '',
               gcBefore, gcAfter, gcFreed,
               dc ? dc.nodes : '', dc ? dc.jsEventListeners : '', dc ? dc.documents : '',
               errs, '"' + pt.slice(0, 90) + '"',
               // The census goes LAST and quoted: it is a list, it is wide, and
               // nothing after it needs a stable column position. Two samples
               // diffed against each other name the collection that is growing.
               '"' + (dg ? dg.top.join(' ') : '') + '"',
               '"' + (dg && dg.dom ? dg.dom.top.join(' ') : '') + '"',
               '"' + (ld ? Object.entries(ld).filter(([k]) => k !== '__sample')
                              .map(([k, v]) => k + '=' + v).join(' ') : '') + '"',
               '"' + (ld && ld.__sample ? String(ld.__sample).replace(/"/g, "'") : '') + '"'].join(','));
  prev = cur;

  // Report a fault, do NOT abort: how the machine behaves after one is part of
  // what a soak is for.
  if (s && (s.state !== 101 || ((s.error_hist || []).length))) {
    if (faults++ === 0) await page.screenshot({ path: OUT + '/esoak_fault.png' });
  }
  // The launcher losing the core is a DIFFERENT fault from the board erroring,
  // and it is the one the old soak could not see at all.
  if (sv && !sv.running) {
    console.log(`  [launcher] THE CORE IS GONE at t=${((Date.now() - t0) / 60000).toFixed(1)} min`);
    await page.screenshot({ path: OUT + '/esoak_core_gone.png' });
    break;
  }
  // Capture the FIRST tick where a measurement card is actually on screen.
  // Cards only render while an object is under the ROI, so a screenshot on a
  // fixed tick catches an empty panel about as often as not.
  if (!shotCards) {
    // A measurement card, not merely an element that shares its class: it has
    // to be VISIBLE and to contain a reading. The first version matched
    // `.s.black` alone, fired on the first tick, and captured a panel with no
    // object under the ROI -- a screenshot of the thing not happening.
    // The rows are TABLE ROWS now, not `.s.black` divs. That selector went
    // stale with the layout change and this shot silently stopped being taken
    // -- a check that cannot fail is not a check. Match on the shape that
    // actually matters instead: a visible row carrying a decimal reading.
    const has = await page.evaluate(() => {
      for (const e of document.querySelectorAll('tr')) {
        if (e.offsetParent && /\d\.\d/.test(e.textContent || '')) return true;
      }
      return false;
    }).catch(() => false);
    if (has) { shotCards = true; await page.screenshot({ path: OUT + '/esoak_cards.png' }); }
  }
  // A fresh screenshot every tick, for iterating on the UI against a live
  // machine. The card shot fires once and then never again, which is right for
  // a soak and useless for a style change: every edit needs a new picture, and
  // restarting the run to get one costs the whole bring-up.
  if (SHOT_EVERY_TICK) {
    await page.screenshot({ path: OUT + '/esoak_live.png' }).catch(() => {});
  }

  if (SNAP_S > 0 && (i * TICK_S) >= SNAP_S) {
    const out = OUT + '/renderer.heapsnapshot';
    console.log(`  [heap] taking a snapshot at t=${((Date.now() - t0) / 60000).toFixed(1)} min`);
    try {
      if (!gcSession) gcSession = await page.context().newCDPSession(page);
      const parts = [];
      gcSession.on('HeapProfiler.addHeapSnapshotChunk', (p) => parts.push(p.chunk));
      await gcSession.send('HeapProfiler.takeHeapSnapshot', { reportProgress: false });
      fs.writeFileSync(out, parts.join(''));
      console.log(`  [heap] wrote ${out} (${(fs.statSync(out).size / 1048576).toFixed(1)} MB)`);
    } catch (e) { console.log('  [heap] failed: ' + e.message); }
    break;
  }
  if (i === 2) await page.screenshot({ path: OUT + '/esoak_t2.png' });
  if (i % Math.max(1, Math.round(3600 / TICK_S)) === 0 && i) {
    await page.screenshot({ path: OUT + `/esoak_t${Math.round(i * TICK_S / 60)}min.png` });
  }
  await sleep(TICK_S * 1000);
}
await page.screenshot({ path: OUT + '/esoak_end.png' });
console.log(`\nsoak done: ${MIN} min, ${faults} sampled ticks with a fault or non-101 state`);
// Closing the window is what stops the machine, and it goes through the
// launcher's graceful path -- so the run also exercises the shutdown it will do
// on the line every day.
await app.close();
fs.rmSync(userData, { recursive: true, force: true });
