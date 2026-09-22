// Long soak: ONE browser session that brings the machine up and then never
// lets go of it, sampling the UI's memory and the board's state together.
//
//   node soak6h.mjs [minutes] [url]
//     env SOAK_FREQ=10000   plate frequency (default 8000)
//     env SOAK_NOCLEAN=1    drop the station's clean_regions for the run
//
// SOAK_FREQ IS CAPPED, AND THE CAP IS PHYSICAL, NOT ARBITRARY: above it the
// plate throws parts off. The firmware clamp is 60000, which is no help here --
// what matters is the mechanical limit of THIS plate, which only the operator
// knows. A soak is not the place to find the throwing point by experiment.
//
// The cap was 10000. Raised to 12000 on 2026-08-30 at the operator's explicit
// instruction ("soak run 6 hr 12000speed"), asked and confirmed before the run.
// It is a number somebody decided, not a number anybody measured -- if a later
// run scatters parts, this line is the reason and it should come back down.
//
// SOAK_NOCLEAN is how the load is raised without touching the plate. This
// station's clean gate blocks ~82% of frames, and a blocked frame costs 0.003ms
// instead of 12.4ms, so turning it off multiplies the engine's work by ~5.3x at
// the same rate. Combined with freq 10000 that is ~25 parts/s each paying a
// full inspection: ~31% of a core against the ~5% normal operation costs.
// Runtime only -- machine_setting.json is never written, and a core restart
// brings the regions back by itself.
//
// Why this exists rather than "pw_bringup then _pw_soak":
//
//   THE BROWSER IS THE MACHINE'S HOST. The core opens its peripheral channel on
//   a UI PD CONNECT, so when the bring-up's browser closes, the channel goes
//   with it, the board loses its host and raises error 12 HOST_LINK_TIMEOUT --
//   BY DESIGN, rather than keep sorting parts nobody is judging. The plate
//   stops. Measured 2026-08-21: pw_bringup PASSed with edges climbing, exited,
//   and 90 seconds later _pw_soak's first two samples read
//   "ERROR . 盤停止 . 12: host link timeout" with a flat 16.3 MB heap.
//
//   A flat heap on a stopped machine is the most convincing wrong answer this
//   bench can produce, which is why the panel text is sampled every tick and
//   the board is polled every tick. Neither number means anything alone.
//
// The bring-up sequence here is pw_bringup's, minus the parts a soak does not
// need. The click ORDER is not arbitrary (recipe declares which 製程 are legal;
// picking one outside the set greys the play button), and plate freq must be
// non-zero before the machine's start button does anything at all.
import { chromium } from 'playwright';
import { execSync } from 'node:child_process';
import fs from 'node:fs';
import net from 'node:net';

const MIN = Number(process.argv[2] || 360);
const URL = process.argv[3] || 'http://localhost:8082/';
const FREQ_WANT = Number(process.env.SOAK_FREQ || 8000);
const FREQ_CAP = Number(process.env.SOAK_FREQ_CAP || 12000);   // see the note above
const FREQ = Math.min(FREQ_WANT, FREQ_CAP);
const NOCLEAN = process.env.SOAK_NOCLEAN === '1';
const MSET_PATH = 'C:/Users/w2110/Documents/workspace/visSele/InspectionCore/Core0_1/data/machine_setting.json';
const OUT = 'C:/Users/w2110/Downloads/pw';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const browser = await chromium.launch({ headless: true });
const ctx = await browser.newContext({ viewport: { width: 1600, height: 950 } });
const page = await ctx.newPage();
// ST goes over the PAGE's socket. A second WS peer that connects first takes
// the peripheral-channel slot and the channel never comes up.
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
console.log(`[0] attaching to ${URL} (this is what opens the peripheral channel)`);
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

// ---- the board console ----------------------------------------------------
//
// RECONNECTING, and that is the point. The peripheral channel is opened by the
// PAGE's PD CONNECT, not by the core starting, so connecting the moment the
// page paints races it: the core accepts the TCP connect and then RESETS the
// socket because it has no channel behind it yet. Measured 2026-08-21 right
// after a previous run's browser had closed -- `read ECONNRESET`, reported as
// "board not answering", which reads like dead hardware and is a dead SOCKET.
//
// So `ask` owns the socket: it dials on demand, drops it on any failure, and
// the caller simply retries. One-shot connect at start-up cannot express
// "the channel is not up YET".
let sock = null, buf = '', lines = [];
let id = 96000;
function connectConsole() {
  return new Promise((resolve) => {
    const s2 = net.connect(4099, '127.0.0.1');
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

console.log('[1] waiting for the board console');
let st = null;
for (let i = 0; i < 20 && !st; i++) {
  st = await ask({ type: 'get_running_stat' }, 2500);
  if (!st) { sock = null; await sleep(3000); }
}
if (!st) { console.log('FAILED: board not answering after 20 tries (no peripheral channel?)'); await browser.close(); process.exit(1); }
console.log(`    state ${st.state}  err ${JSON.stringify(st.error_hist)}`);

// exit_insp_mode BEFORE clear_error, or 112 never clears: the board is still IN
// inspection mode and clear_error alone does not take it out.
if (st.state !== 100 || (st.error_hist || []).length) {
  console.log('[2] clearing');
  await ask({ type: 'set_setup', plate: { freq: 0 } }, 1500);
  await sleep(2500);
  await ask({ type: 'exit_insp_mode' }, 2000);
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'clear_error_history' }, 1200);
  await sleep(1500);
  // clear_error puts the board back INTO inspection mode; take it out again.
  await ask({ type: 'exit_insp_mode' }, 1500);
  st = await ask({ type: 'get_running_stat' }, 1800);
  console.log(`    now state ${st && st.state}  err ${JSON.stringify(st && st.error_hist)}`);
}

// Pin the gate's rate ceiling instead of inheriting whatever the board happens
// to hold. `min_detect_sep_us` lives in RAM unless save_setup was called, so a
// firmware flash silently reverts it to the built-in 30000 (33.3/s) -- measured
// 2026-08-22, admitted fell 22.4 -> 15.7 parts/s against an unchanged 23.3/s
// arrival, and the run stopped being comparable to the previous ones. An
// operating point a soak did not choose is an operating point it cannot report.
const SEP = Number(process.env.SOAK_SEP_US || 14286);   // 70/s ceiling
const sr = await ask({ type: 'set_setup', gate: { min_detect_sep_us: SEP } }, 1800);
const sc = await ask({ type: 'get_setup' }, 2000);
const sepNow = sc && sc.gate ? sc.gate.min_detect_sep_us : -1;
console.log(`[3a] min_detect_sep_us = ${sepNow} (${sepNow > 0 ? (1e6/sepNow).toFixed(1) : '?'}/s ceiling)`);
if (sepNow !== SEP) { console.log(`FAILED: gate ceiling did not take (wanted ${SEP})`); await browser.close(); process.exit(1); }

const fr = await ask({ type: 'set_setup', plate: { freq: FREQ } }, 1800);
console.log(`[3] plate freq = ${FREQ}${FREQ_WANT > FREQ ? ` (capped from ${FREQ_WANT})` : ''} `
          + `-> ${fr && fr.ack ? 'ack' : 'NOT ACKED'}`);

console.log('[4] recipe + 製程 + 檢測方式');
await clickIcon('anticon-folder-open'); await sleep(3000);
await clickText('test1.hydef'); await sleep(900);
await page.evaluate(() => { for (const e of document.querySelectorAll('*')) {
  if ((e.innerText || '').trim() === 'test1.hydef' && !e.children.length && e.offsetParent) {
    e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true })); return; } } });
await sleep(5000);
await clickText('11沖壓成形'); await sleep(1200);
await clickText('全檢'); await sleep(1200);

console.log('[5] into the Inspection UI');
await clickIcon('anticon-caret-right'); await sleep(9000);

// Clean regions OFF BEFORE the machine starts, so not one part is judged under
// a configuration this run is not measuring. Sending it after start would put a
// few seconds of 82%-blocked frames at the head of the data -- the very state
// the run exists to exclude.
if (NOCLEAN) {
  const mset = JSON.parse(fs.readFileSync(MSET_PATH, 'utf8'));
  const sent = await page.evaluate((m) => window.__SEND_ST__(m),
                                   { inspection_region: mset.inspection_region, clean_regions: [] });
  console.log(`[6] clean_regions -> [] : ${sent} (was ${(mset.clean_regions || []).length}; runtime only)`);
  if (sent !== 'sent') { console.log('FAILED: could not send ST'); await browser.close(); process.exit(1); }
  await sleep(4000);
}

console.log('[7] start the machine');
await clickIcon('anticon-caret-right'); await sleep(9000);
await page.evaluate(() => { const x = document.querySelector('.ant-drawer-close');
  if (x && x.offsetParent) x.click(); });
await sleep(2000);

// ---- is the plate ACTUALLY turning? gate edges must climb ------------------
const edges = (s) => (s && s.yield && s.yield.gate ? s.yield.gate.in : -1);
const a = await ask({ type: 'get_running_stat' }, 1800);
await sleep(10000);
const b = await ask({ type: 'get_running_stat' }, 1800);
console.log(`[6] state ${b && b.state}  edges ${edges(a)} -> ${edges(b)}`);
if (!b || b.state !== 101 || edges(b) <= edges(a)) {
  console.log('FAILED: machine is not running (state not 101, or gate edges not climbing)');
  await page.screenshot({ path: OUT + '/soak_FAILED.png' });
  await browser.close(); process.exit(1);
}
await page.screenshot({ path: OUT + '/soak_start.png' });

// chrome-headless-shell, not chrome: that is the binary Playwright launches
// headless, and filtering on the wrong name silently reports 0 MB forever.
const rss = () => { try {
  const o = execSync('tasklist /FI "IMAGENAME eq chrome-headless-shell.exe" /FO CSV /NH', { encoding: 'utf8', timeout: 15000 });
  let kb = 0; for (const l of o.split(/\r?\n/)) { const m = l.match(/"([\d,]+) K"\s*$/); if (m) kb += Number(m[1].replace(/,/g, '')); }
  return Math.round(kb / 1024);
} catch { return -1; } };
const coreRSS = () => { try {
  const o = execSync('tasklist /FI "IMAGENAME eq visSele.exe" /FO CSV /NH', { encoding: 'utf8', timeout: 15000 });
  const m = o.match(/"([\d,]+) K"\s*$/m);
  return m ? Math.round(Number(m[1].replace(/,/g, '')) / 1024) : -1;
} catch { return -1; } };
const panelText = () => page.evaluate(() => {
  for (const e of document.querySelectorAll('div')) {
    const t = (e.innerText || '');
    if (t.includes('rpm') && t.includes('/s') && t.length < 260) return t.replace(/\s+/g, ' ').trim();
  } return '';
}).catch(() => '');

// The pipeline stages, not just the plate. `skip` and `unanswered` are the two
// numbers that say the CORE fell behind -- a verdict that arrived after its
// part had passed SWITCH. Sampling memory without them measures a machine that
// might already be dropping parts.
const vd = (s) => (s && s.yield && s.yield.verdict) || {};
const so = (s) => (s && s.yield && s.yield.sort) || {};
const rl = (s) => (s && s.report_latency) || {};
const ct = (s) => (s && s.count) || {};
// Core-side counters, fetched once a minute alongside the board's. These are
// the three detections added 2026-08-22: what the device answered (locked /
// ack:false / unapplied) and camera frames that never arrived. All are
// cumulative -- a nonzero value means it happened, not that it is happening.
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
const cs = (s) => (s && s.cam_sync) || {};

// ---- the two numbers that decide whether a verdict still MEANS anything ----
//
// LATENCY vs THE BUDGET. The part is judged at the camera and acted on at the
// selector, and the time between them is
//     CAM->SWITCH = (29900 - 9315) / (2 * plate_freq)
// = 1029 ms at freq 10000. A verdict later than that arrives after its part has
// passed the nozzle.
//
// It must be `cam_avg_us`, NEVER `avg_us`: avg_us starts at the GATE and so
// includes the part walking to the camera (497 ms of it), while the budget
// starts where the camera fires. MACHINE_FLOW.md records that comparison being
// got wrong twice in one day.
//
// The average is not the risk -- 31.6 ms against 1029 ms is 3%. The TAIL is:
// one 215 ms serial-write block once pushed parts past the deadline and took
// UNANSWERED from 4-10 to 31. So the tail bucket count is sampled as a DELTA
// (how many landed there since the last tick), because a cumulative histogram
// hides when it happened, and `cam_max_us` is a high-water that only ever says
// "at some point".
//
// TIMESTAMP MATCHING RESIDUAL. Every report is paired to a camera frame by
// timestamp; `resid_us` is how far off that fit currently is and `delta_max_us`
// the worst pairing gap. If the residual grows, pairings drift toward the edge
// of `window_us` and eventually mis-pair -- a part judged by ANOTHER part's
// image, which is the worst failure this machine has, because nothing about it
// looks like an error. `rejected` / `cal_fails` / `cal_pulse_lost` are the
// counters that say the clock discipline itself is struggling.
const HI = (h) => (Array.isArray(h) ? (h[5] || 0) + (h[6] || 0) + (h[7] || 0) : 0);
let prev = null;
console.log('');
console.log('t_min,heapMB,totalMB,uiRSS_MB,coreRSS_MB,domNodes,state,'
          + 'seen_s,admit_s,sorted_s,na_s,skip,unans,'
          + 'nm_orphan,nm_window,nm_consec,statwin_ms,'
          + 'ackfalse,locked,unapplied,frame_gap,frame_lost,'
          + 'lat_avg_ms,lat_max_ms,lat_tail_n,'
          + 'resid_us,resid_max_us,dmax_us,miss_last_us,miss_max_us,rebuilds,'
          + 'ts_rej,cal_fail,cal_lost,win_us,drift_us_s,'
          + 'err,panel');
const t0 = Date.now();
let faults = 0;
for (let i = 0; i <= MIN; i++) {
  const v = await page.evaluate(() => { const m = performance.memory || {};
    return { h: +(m.usedJSHeapSize / 1048576).toFixed(1), t: +(m.totalJSHeapSize / 1048576).toFixed(1),
             d: document.getElementsByTagName('*').length }; }).catch(() => null);
  // reset_stat_maximum: every row's lat_max / resid / dmax is then "the worst
  // in THIS tick", not a lifetime high-water that a decaying envelope used to
  // erase before the next sample. stat_max_window_ms proves what it covers --
  // if another reader resets in between, the window comes back short and the
  // row says so instead of quietly reporting a 60s max it never had.
  const s = await ask({ type: 'get_running_stat', reset_stat_maximum: true }, 1500);
  const pt = await panelText();
  const cc = await coreCounters();
  const errs = JSON.stringify((s && s.error_hist) || []);
  // Rates from the deltas between ticks, so a stall shows as a rate going to
  // zero rather than as a counter that merely stops growing in a wide column.
  const dt = prev ? (Date.now() - prev.at) / 1000 : 0;
  const r = (now, was) => (dt > 0 && now >= 0 && was >= 0 ? ((now - was) / dt).toFixed(1) : '');
  const cur = { at: Date.now(), seen: edges(s), adm: (s && s.yield && s.yield.gate ? s.yield.gate.out : -1),
                srt: so(s).out !== undefined ? so(s).out : -1, na: so(s).na !== undefined ? so(s).na : -1,
                hi: HI(rl(s).cam_hist) };
  const us2ms = (u) => (u === undefined ? '' : (u / 1000).toFixed(1));
  console.log([((Date.now() - t0) / 60000).toFixed(1),
               v ? v.h : 'err', v ? v.t : 'err', rss(), coreRSS(), v ? v.d : 'err',
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
               // The deltas of frames the gate REFUSED, and how often the clock
               // had to be rebuilt. delta_max_us records only what was ACCEPTED,
               // so without these a halt says "two frames were outside the
               // window" and nothing about whether they were 6 ms out or 400 ms
               // out -- and those have completely different causes. The 12000
               // run stopped on exactly that question and could not answer it.
               cs(s).miss_delta_last_us !== undefined ? cs(s).miss_delta_last_us : '',
               cs(s).miss_delta_max_us !== undefined ? cs(s).miss_delta_max_us : '',
               cs(s).rebuilds !== undefined ? cs(s).rebuilds : '',
               cs(s).rejected !== undefined ? cs(s).rejected : '',
               cs(s).cal_fails !== undefined ? cs(s).cal_fails : '',
               cs(s).cal_pulse_lost !== undefined ? cs(s).cal_pulse_lost : '',
               cs(s).window_us !== undefined ? cs(s).window_us : '',
               cs(s).drift_us_per_s !== undefined ? Number(cs(s).drift_us_per_s).toFixed(1) : '',
               errs, '"' + pt.slice(0, 90) + '"'].join(','));
  prev = cur;
  // Report a fault, do NOT abort: how the machine behaves after one is part of
  // what a soak is for. Screenshot the first one, which is the one with the
  // cause still on screen.
  if (s && (s.state !== 101 || ((s.error_hist || []).length))) {
    if (faults++ === 0) await page.screenshot({ path: OUT + '/soak_fault.png' });
  }
  if (i === 2) await page.screenshot({ path: OUT + '/soak_t2.png' });
  if (i % 60 === 0 && i) await page.screenshot({ path: OUT + `/soak_t${i}min.png` });
  await sleep(60000);
}
await page.screenshot({ path: OUT + '/soak_end.png' });
console.log(`\nsoak done: ${MIN} min, ${faults} sampled ticks with a fault or non-101 state`);
await browser.close();
