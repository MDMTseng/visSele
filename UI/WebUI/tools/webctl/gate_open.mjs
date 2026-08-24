// Open the gate's rate limiter and see what the machine actually does.
//
//   node gate_open.mjs [--target 70] [--no-clean] [--hold 90]
//                      [--steps 33,40,50,60,70] [--url http://localhost:8082/]
//
// WHAT IS ACTUALLY BEING CHANGED
//
// Not the plate speed. `min_detect_sep_us` (firmware SYS_MIN_PULSE_TIME_SEP_us,
// default 30000) is the minimum time between two parts ADMITTED at the gate:
// 30000 us = 33.3 parts/s, and that is the ceiling this machine currently runs
// against. Parts arriving sooner are turned away and counted in
// yield.gate.loss_n with loss="rate" -- 46918 of them at last look, against
// 176379 seen and 106894 admitted (60.6%).
//
// A part refused at the gate is NOT removed from the plate. It stays on and
// comes round again next lap (this is why the old AIMD rate loop was deleted in
// 2026-08-12 -- widening the gate sheds no load, it only defers it). So opening
// the limiter does not create work out of nothing; it stops recirculating work
// that is already there.
//
// Setting it to 70/s therefore does not MAKE the machine run at 70/s. It
// removes the limit, and the rate settles wherever the plate and the bowl feed
// put it -- expected ~30/s at plate freq 8000. Plate freq is NOT touched here.
//
// set_setup only, never save_setup: this stays in the board's RAM, so a reboot
// restores the configured 30000 by itself. Same discipline as the runtime-only
// machine_setting changes.
//
// SIDE EFFECT, CHECKED: CamClockSync::TOL_US is clamped to half the separation
// (LegacyFirmware.cpp:9361). At 30000 the cap is 15000; at 14286 it is 7143.
// The live window is 5000, so neither clamps it. Printed each step anyway --
// if the pairing window ever moves, that is the camera-sync safety margin
// changing underneath the measurement.
import { chromium } from 'playwright';
import fs from 'node:fs';
import net from 'node:net';

const arg = (n, d) => { const i = process.argv.indexOf('--' + n);
  if (i >= 0 && process.argv[i + 1] && !process.argv[i + 1].startsWith('--')) return process.argv[i + 1];
  return process.argv.includes('--' + n) ? true : d; };
const URL = arg('url', 'http://localhost:8082/');
const TARGET = Number(arg('target', 70));
const HOLD = Number(arg('hold', 90));
const NOCLEAN = !!arg('no-clean', false);
const STEPS = String(arg('steps', '')).split(',').filter(Boolean).map(Number);
const MSET_PATH = 'C:/Users/w2110/Documents/workspace/visSele/InspectionCore/Core0_1/data/machine_setting.json';
const OUT = 'C:/Users/w2110/Downloads/pw';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const browser = await chromium.launch({ headless: true });
const page = await (await browser.newContext({ viewport: { width: 1600, height: 950 } })).newPage();
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
console.log(`[0] attaching to ${URL}`);
await page.goto(URL, { waitUntil: 'domcontentloaded' });
await sleep(7000);

const clickText = async (label, tag = '*') => page.evaluate(({ label, tag }) => {
  for (const e of document.querySelectorAll(tag)) {
    const t = (e.innerText || '').trim();
    if (t !== label || e.children.length || !e.offsetParent) continue;
    e.click(); return true;
  } return false; }, { label, tag });
const clickIcon = async (icon, nth = 0) => page.evaluate(({ icon, nth }) => {
  let k = 0;
  for (const b of document.querySelectorAll('button')) {
    if (!b.offsetParent || b.disabled) continue;
    if (b.querySelector('[class*="' + icon + '"]')) { if (k++ === nth) { b.click(); return true; } }
  } return false; }, { icon, nth });
await clickText('跳過相機連線', 'button'); await sleep(1500);

const sock = net.connect(4099, '127.0.0.1');
let buf = '', lines = [];
sock.on('data', (d) => { buf += d.toString('latin1'); let n;
  while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); } });
sock.on('error', (e) => console.log('console 4099:', e.message));
await new Promise((r) => sock.once('connect', r));
let id = 98000;
async function ask(o, ms = 1800) {
  const my = id++; lines = [];
  sock.write(JSON.stringify({ ...o, id: my }) + '\n');
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return null; }
}
const gate = (s) => (s && s.yield && s.yield.gate) || {};
const edges = (s) => (gate(s).in !== undefined ? gate(s).in : -1);
const admitted = (s) => (gate(s).out !== undefined ? gate(s).out : -1);

let st = await ask({ type: 'get_running_stat' }, 2500);
if (!st) { console.log('FAILED: board not answering'); await browser.close(); process.exit(1); }
console.log(`[1] board state ${st.state} err ${JSON.stringify(st.error_hist)}`);
if (st.state !== 100 && st.state !== 101) {
  await ask({ type: 'set_setup', plate: { freq: 0 } }, 1500); await sleep(2500);
  await ask({ type: 'exit_insp_mode' }, 2000);
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'clear_error_history' }, 1200); await sleep(1500);
  await ask({ type: 'exit_insp_mode' }, 1500);
  st = await ask({ type: 'get_running_stat' }, 1800);
  console.log(`    cleared -> state ${st && st.state} err ${JSON.stringify(st && st.error_hist)}`);
}

const setup0 = await ask({ type: 'get_setup' }, 2500);
const SEP0 = setup0 && setup0.gate ? setup0.gate.min_detect_sep_us : 30000;
console.log(`[2] min_detect_sep_us is ${SEP0} us = ${(1e6 / SEP0).toFixed(1)} parts/s ceiling`);

if (st.state !== 101) {
  await ask({ type: 'set_setup', plate: { freq: 8000 } }, 1800);
  console.log('[3] recipe + 製程 + 檢測方式');
  await clickIcon('anticon-folder-open'); await sleep(3000);
  await clickText('test1.hydef'); await sleep(900);
  await page.evaluate(() => { for (const e of document.querySelectorAll('*')) {
    if ((e.innerText || '').trim() === 'test1.hydef' && !e.children.length && e.offsetParent) {
      e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true })); return; } } });
  await sleep(5000);
  await clickText('11沖壓成形'); await sleep(1200);
  await clickText('全檢'); await sleep(1200);
  console.log('[4] into the Inspection UI, then start');
  await clickIcon('anticon-caret-right'); await sleep(9000);
  await clickIcon('anticon-caret-right'); await sleep(9000);
} else {
  console.log('[3] machine already running -- entering the view only');
  await clickIcon('anticon-folder-open'); await sleep(3000);
  await clickText('test1.hydef'); await sleep(900);
  await page.evaluate(() => { for (const e of document.querySelectorAll('*')) {
    if ((e.innerText || '').trim() === 'test1.hydef' && !e.children.length && e.offsetParent) {
      e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true })); return; } } });
  await sleep(5000);
  await clickText('11沖壓成形'); await sleep(1200);
  await clickText('全檢'); await sleep(1200);
  await clickIcon('anticon-caret-right'); await sleep(9000);
}
await page.evaluate(() => { const x = document.querySelector('.ant-drawer-close');
  if (x && x.offsetParent) x.click(); });
await sleep(3000);

let a = await ask({ type: 'get_running_stat' }, 1500);
await sleep(15000);
let b = await ask({ type: 'get_running_stat' }, 1500);
if (!b || b.state !== 101 || admitted(b) <= admitted(a)) {
  console.log(`FAILED: not running (state ${b && b.state}, admitted ${admitted(a)} -> ${admitted(b)})`);
  await page.screenshot({ path: OUT + '/gate_FAILED.png' });
  await browser.close(); process.exit(1);
}
console.log(`[5] baseline ${((admitted(b) - admitted(a)) / 15).toFixed(1)} parts/s admitted, `
          + `${((edges(b) - edges(a)) / 15).toFixed(1)}/s seen at the gate`);

let restore = null;
if (NOCLEAN) {
  const mset = JSON.parse(fs.readFileSync(MSET_PATH, 'utf8'));
  restore = { inspection_region: mset.inspection_region, clean_regions: mset.clean_regions };
  const sent = await page.evaluate((m) => window.__SEND_ST__(m),
                                   { inspection_region: mset.inspection_region, clean_regions: [] });
  console.log(`[6] clean_regions -> [] : ${sent} (was ${(mset.clean_regions || []).length}; runtime only)`);
  if (sent !== 'sent') { console.log('FAILED: could not send ST'); await browser.close(); process.exit(1); }
  await sleep(4000);
}

const plan = STEPS.length ? STEPS : [33, 45, 55, TARGET].filter((v, i, r) => r.indexOf(v) === i);
console.log('');
console.log('ceiling_per_s,sep_us,seen_per_s,admitted_per_s,pct,rate_loss_per_s,state,err,camwin_us,panel');

async function measure(want) {
  const sep = Math.round(1e6 / want);
  const r = await ask({ type: 'set_setup', gate: { min_detect_sep_us: sep } }, 2000);
  if (!r || r.ack === false) console.log(`    (set_setup min_detect_sep_us=${sep} -> ${JSON.stringify(r)})`);
  await sleep(6000);
  const s0 = await ask({ type: 'get_running_stat' }, 1500);
  await sleep(HOLD * 1000);
  const s1 = await ask({ type: 'get_running_stat' }, 1500);
  if (!s0 || !s1) { console.log(`${want},${sep},,,,,no-reply`); return null; }
  const seen = (edges(s1) - edges(s0)) / HOLD;
  const adm = (admitted(s1) - admitted(s0)) / HOLD;
  const loss = ((gate(s1).loss_n || 0) - (gate(s0).loss_n || 0)) / HOLD;
  const pt = await page.evaluate(() => {
    for (const e of document.querySelectorAll('div')) {
      const t = (e.innerText || '');
      if (t.includes('rpm') && t.includes('/s') && t.length < 260) return t.replace(/\s+/g, ' ').trim();
    } return ''; }).catch(() => '');
  console.log([want, sep, seen.toFixed(1), adm.toFixed(1),
               gate(s1).pct !== undefined ? gate(s1).pct.toFixed(1) : '',
               loss.toFixed(1), s1.state, JSON.stringify(s1.error_hist || []),
               s1.cam_sync ? s1.cam_sync.window_us : '',
               '"' + pt.slice(0, 80) + '"'].join(','));
  fs.writeFileSync(`${OUT}/gate_stat_${want}.json`, JSON.stringify(s1, null, 1));
  await page.screenshot({ path: OUT + `/gate_${want}.png` });
  return s1;
}

let lastGood = null;
for (const want of plan) {
  const s = await measure(want);
  if (!s || s.state !== 101 || (s.error_hist || []).length) {
    console.log(`\nSTOPPED at ceiling ${want}/s -- state ${s && s.state} err ${JSON.stringify(s && s.error_hist)}`);
    break;
  }
  lastGood = want;
}
console.log(`\nlast clean ceiling: ${lastGood === null ? 'none' : lastGood + '/s'}`);

console.log('[9] restoring');
await ask({ type: 'set_setup', gate: { min_detect_sep_us: SEP0 } }, 2000);
if (restore) console.log(`    clean_regions restored: ${await page.evaluate((m) => window.__SEND_ST__(m), restore)}`);
await sleep(3000);
const fin = await ask({ type: 'get_running_stat' }, 1800);
const setupF = await ask({ type: 'get_setup' }, 2000);
console.log(`    min_detect_sep_us back to ${setupF && setupF.gate ? setupF.gate.min_detect_sep_us : '?'} `
          + `(never saved to NVS), state ${fin && fin.state} err ${JSON.stringify(fin && fin.error_hist)}`);
await browser.close();
