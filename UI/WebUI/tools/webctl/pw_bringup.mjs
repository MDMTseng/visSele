// Cold WebUI -> plate turning -> parts being inspected, driven with Playwright.
//
//   node pw_bringup.mjs [--url http://localhost:8082/] [--recipe test1.hydef]
//                       [--process 11沖壓成形] [--mode 全檢] [--freq 8000]
//                       [--shots C:/Users/w2110/Downloads/pw] [--headed]
//
// WHY THIS EXISTS ALONGSIDE run_machine.mjs
//
// run_machine.mjs reaches for window.__GP_STORE__, which ONLY the dev bundle
// exposes. The machine the operator actually uses is the production bundle on
// :8082, and there the harness cannot read a single thing -- so it drove the UI
// into a state it could not verify and reported "0 reports" for a machine that
// was running fine. This one clicks the real buttons and reads the real panel,
// so it works against either bundle.
//
// THE ORDER IS NOT ARBITRARY. Each of these cost an hour on 2026-08-21:
//
//   1. The camera-reconnect modal ("跳過相機連線") sits over everything on a
//      cold load. It is not always there; dismiss it if it is.
//
//   2. THE RECIPE GOES FIRST. A .hydef declares which 製程 it is valid for
//      ("已設定範圍"). Select a 製程 outside that set and the play button stays
//      GREY with only a small warning triangle next to 製程 to say why. Loading
//      the recipe first means the legal set is on screen before you pick.
//
//   3. 製程 must come from the recipe's own list. test1.hydef declares
//      11沖壓成形 / 02首件熱後 / 01首件熱前 -- NOT 全檢, which is a different
//      row entirely.
//
//   4. "全檢" appears in BOTH the 製程 row and the 檢測方式 row. Matching on
//      text alone picks whichever comes first in the DOM, which is the wrong
//      one. Every click here filters on VISIBILITY and exact text, and the two
//      rows are reached in separate steps.
//
//   5. Play is pressed TWICE, and they are different buttons: the first enters
//      the Inspection UI, the second starts the machine. The second one
//      TOGGLES -- pressing it on a running machine stops it. Hence --no-start.
//
//   6. The device's plate freq must be non-zero before the machine's start
//      button does anything. A freq of 0 makes it a silent no-op, and the UI
//      looks identical either way.
//
// VERIFICATION IS AGAINST THE MACHINE, NOT THE SCREEN. The UI can look
// perfectly normal while nothing turns -- that is how an afternoon went. The
// final check reads gate edges off the board through the dev console and
// requires them to be CLIMBING.
//
// ONE MORE TRAP, not in this file but fatal to it: whatever connects to the
// core FIRST gets the image stream. Leave perif_hold.mjs or webctld running
// and the browser gets no stream, every count reads 0, and nothing looks
// broken. Stop them before running this.
import crypto from 'node:crypto';
import fs from 'node:fs';
import net from 'node:net';
import { chromium } from 'playwright';
import { dirtied } from './_rc_clean.mjs';

const arg = (n, d) => {
  const i = process.argv.indexOf('--' + n);
  if (i >= 0 && process.argv[i + 1] && !process.argv[i + 1].startsWith('--')) return process.argv[i + 1];
  return process.argv.includes('--' + n) ? true : d;
};
const URL = arg('url', 'http://localhost:8082/');
const RECIPE = arg('recipe', 'test1.hydef');
const PROCESS = arg('process', '11沖壓成形');
const MODE = arg('mode', '全檢');
const FREQ = Number(arg('freq', 8000));
const SHOTS = arg('shots', 'C:/Users/w2110/Downloads/pw');
const HEADED = !!arg('headed', false);
const NOSTART = !!arg('no-start', false);
// Seconds to keep watching AFTER the machine is up. A bring-up that checks
// once and declares success is checking the easiest moment there is: the
// machine can fault seconds later and did (error 1 at 30 rpm arrives ~100s in).
const WATCH = Number(arg('watch', 90));
// Hash the image canvas during the watch. A frozen picture and a live one look
// identical in a single screenshot, and the difference is the whole question of
// whether the operator is seeing the machine or a photograph of it.
const LIVE = !!arg('live-check', false);
// Count BPG frames as they arrive off the WebSocket, before the app sees them.
// The production bundle is minified, so hooking raw2Obj_IM by name is not an
// option -- but the wire format is fixed (tl[2] | prop | pgID[2] | size[4]) and
// wrapping WebSocket needs nothing from the bundle at all. This answers the
// only question that matters first: do IM frames reach the browser?
const WSTAP = !!arg('ws-tap', false);
// Relay the page's own console. The image path is already instrumented -- the
// IM parser logs the first 20 frames' metadata and warns on every bail, and the
// canvas warns on a failed decode or a size mismatch. Reading those beats
// adding new probes to a minified bundle.
const CONSOLE = !!arg('console', false);
// Dev-bundle only: watch edit_info.img itself. The IM frames parse and reach
// redux; the picture on screen does not move. This splits those two -- if the
// store's image changes and the canvas does not, the break is in the component,
// not in the wire or the reducer.
const STORE = !!arg('store-probe', false);
// --diag reads window.__CANVAS_DIAG__, the counters compiled into SetImg /
// the decode callback / the secCanvas blit / InspectionUI.updateCanvas. The
// store probe proves redux gets new images; these say which of the four
// steps after redux stops touching them.
const DIAG = !!arg('diag', false);
// --zoom N: wheel-zoom the inspection canvas in N notches, centred on where the
// picture actually is, then capture two frames a few seconds apart.
//
// This exists because the picture is drawn ~100 device px wide for an 816-px
// bitmap -- roughly 1/8 of the view -- and at that size "the image is frozen"
// and "the image is live but the parts recirculate unchanged" look identical.
// Magnifying is not cosmetic here; it is what makes the difference observable.
const ZOOM = Number(arg('zoom', 0));

// --force-dirty N: make the station's clean regions fail for N seconds, then
// put them back, and count -- from redux, i.e. from what the BROWSER actually
// received -- how many reports were blocked and whether images kept arriving
// while they were.
//
// This is the whole question the clean-area refactor was meant to settle. The
// old code skipped the inspection when the area was dirty and emitted a report
// with no `type` and no `reports`, which the WebUI reducer drops on its first
// line -- so a blocked part was counted by the machine and invisible in the UI.
// "Still gets the image" cannot be checked by looking; it needs a frame that is
// blocked ON PURPOSE while someone counts.
const FORCE_DIRTY = Number(arg('force-dirty', 0));
const MSET_PATH = arg('mset', 'C:/Users/w2110/Documents/workspace/visSele/InspectionCore/Core0_1/data/machine_setting.json');
const CLEANPROBE = FORCE_DIRTY > 0 || !!arg('clean-probe', false);
const PORT = Number(arg('port', 4099));

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---- the board, over the dev console -------------------------------------
// Single client at a time: the console refuses a second one with
// {"err":"console busy"}. Nothing else may hold it while this runs.
function boardLink() {
  const s = net.connect(PORT, '127.0.0.1');
  let buf = '', id = 7000;
  s.on('data', (d) => { buf += d.toString(); });
  s.on('error', () => {});
  return {
    ready: new Promise((r) => s.once('connect', () => r(true)).once('error', () => r(false))),
    ask(obj, ms = 2500) {
      const my = ++id; buf = '';
      s.write(JSON.stringify({ ...obj, id: my }) + '\n');
      return new Promise((r) => setTimeout(() => {
        const lines = buf.split(String.fromCharCode(10));
        const l = lines.find((x) => x.includes('"id":' + my));
        if (!l) {
          // The core answers {"err":"no perif channel"} with NO id when the
          // board link is not up. Returning a bare null for that made every
          // caller read "no answer" as "the board is stuck", and step 2
          // blamed the hardware for a link that was never established.
          const e = lines.find((x) => x.includes('"err"'));
          try { return r(e ? { __err: JSON.parse(e.slice(e.indexOf(String.fromCharCode(123)))).err } : null); }
          catch { return r(null); }
        }
        try { r(JSON.parse(l.slice(l.indexOf(String.fromCharCode(123))))); } catch { r(null); }
      }, ms));
    },
    end() { try { s.end(); } catch { /* already gone */ } },
  };
}

const board = boardLink();
const haveBoard = await board.ready;
if (!haveBoard) console.log('note: no dev console on :' + PORT + ' -- skipping the machine-side checks');

// ---- browser -------------------------------------------------------------
const browser = await chromium.launch({ headless: !HEADED });
const ctx = await browser.newContext({ viewport: { width: 1600, height: 950 } });
const page = await ctx.newPage();
page.on('pageerror', (e) => console.log('  [pageerror]', e.message.slice(0, 110)));
if (CONSOLE) {
  const seen = new Map();
  page.on('console', (m) => {
    const t = m.text();
    if (!/IM#|raw2Obj|SetImg|JPEG|Image_Update|drawImage|stream-/i.test(t)) return;
    // Collapse repeats: a per-frame warning would otherwise bury everything.
    const key = t.replace(/\d+/g, '#');
    const n = (seen.get(key) || 0) + 1;
    seen.set(key, n);
    if (n <= 3 || n % 100 === 0) console.log(`  [page:${m.type()}] ${t.slice(0, 150)}${n > 3 ? '  (x' + n + ')' : ''}`);
  });
}

let step = 0;
const shot = async (name) => {
  step++;
  const p = `${SHOTS}/${String(step).padStart(2, '0')}_${name}.png`;
  await page.screenshot({ path: p }).catch(() => {});
  return p;
};

// Click the first VISIBLE leaf element whose trimmed text matches exactly.
// Leaf (no children) and visible (offsetParent) between them are what keep this
// off hidden duplicates and off the container that merely contains the text.
const clickText = (label, tag = '*') => page.evaluate(({ label, tag }) => {
  for (const e of document.querySelectorAll(tag)) {
    if (e.children.length || !e.offsetParent) continue;
    if ((e.innerText || '').trim() !== label) continue;
    const r = e.getBoundingClientRect();
    if (r.width < 5 || r.height < 5) continue;
    e.click(); return true;
  }
  return false;
}, { label, tag });

// nth ENABLED visible button carrying an antd icon class.
const clickIcon = (icon, nth = 0) => page.evaluate(({ icon, nth }) => {
  let k = 0;
  for (const b of document.querySelectorAll('button')) {
    if (!b.offsetParent || b.disabled) continue;
    if (!b.querySelector('[class*="' + icon + '"]')) continue;
    if (k++ === nth) { b.click(); return true; }
  }
  return false;
}, { icon, nth });

const playEnabled = () => page.evaluate(() => {
  const p = [...document.querySelectorAll('button')].find((b) => b.querySelector('[class*=caret-right]'));
  return p ? !p.disabled : null;
});

// The 全檢設備 panel's own text: "RUN · 13.6 rpm · 檢測中 進料 854". The most
// honest thing the production bundle will tell you without a debug handle.
const panel = () => page.evaluate(() => {
  for (const e of document.querySelectorAll('div')) {
    const t = e.innerText || '';
    if (t.includes('rpm') && t.length < 300) return t.replace(/\s+/g, ' ').trim();
  }
  return '';
}).catch(() => '');

// Exits rather than returning: this file is an ES module, so a top-level
// `return fail(x)` is a syntax error, and every call site wants to stop anyway.
// The panel prints its faults as "⚠ 1: result matched no object" with a running
// count. Reading it means a failure is reported in the operator's words, not
// just as a state number.
const uiWarnings = () => page.evaluate(() => {
  const out = [];
  for (const e of document.querySelectorAll('div,span')) {
    if (e.children.length || !e.offsetParent) continue;
    const t = (e.innerText || '').trim();
    if (t.startsWith('⚠') || /result matched no object|ERROR/.test(t)) out.push(t.slice(0, 80));
  }
  return [...new Set(out)].slice(0, 5);
}).catch(() => []);

const fail = async (why) => {
  console.log('FAILED: ' + why);
  console.log('  shot: ' + await shot('FAILED'));
  await browser.close().catch(() => {});
  board.end();
  process.exit(1);
};

if (WSTAP) {
  await page.addInitScript(() => {
    window.__WSTAP__ = { byTL: {}, frames: 0, bytes: 0, shortIM: 0,
                         rpBlocked: [], rpClean: [], rpDirty: [], captureDirty: false };
    const Native = window.WebSocket;
    window.WebSocket = function (...a) {
      const ws = new Native(...a);
      ws.addEventListener('message', (ev) => {
        const d = ev.data;
        if (!(d instanceof ArrayBuffer) && !(d && d.byteLength)) return;
        const done = (buf) => {
          const u = new Uint8Array(buf);
          const t = window.__WSTAP__;
          t.frames++; t.bytes += u.length;
          if (u.length < 9) return;
          const tl = String.fromCharCode(u[0], u[1]);
          t.byTL[tl] = (t.byTL[tl] || 0) + 1;
          // Declared payload length vs what actually arrived: raw2Obj_IM drops
          // the frame when they disagree, and drops it SILENTLY.
          const len = (u[5] << 24) | (u[6] << 16) | (u[7] << 8) | u[8];
          if (tl === 'IM' && u.length - 9 < len) t.shortIM++;
          // Keep a few whole RP payloads. The report's exact shape decides
          // where "zero located objects" has to be expressed, and the shape is
          // not guessable from the C++ -- the top-level `reports` array is one
          // entry per def FEATURE, not per located object, and the statistics
          // counter in the reducer sits inside a per-feature case.
          if (tl === 'RP') {
            try {
              const txt = new TextDecoder().decode(u.subarray(9)).replace(/\0+$/, '');
              if (t.captureDirty) { if (t.rpDirty.length < 3) t.rpDirty.push(txt); }
              else {
                // Bucket by whether the clean gate blocked this frame. The
                // NOT-blocked samples are the negative control: without them
                // "blocked frames carry no objects" is equally consistent with
                // having blanked every frame on the machine.
                let blocked = false;
                try { const st = JSON.parse(txt).station;
                      blocked = !!(st && st.clean_err !== undefined && st.clean_err !== null); } catch { }
                if (blocked) { if (t.rpBlocked.length < 3) t.rpBlocked.push(txt); }
                else if (t.rpClean.length < 3) t.rpClean.push(txt);
              }
            } catch {}
          }
        };
        if (d instanceof ArrayBuffer) done(d);
        else if (d instanceof Blob) d.arrayBuffer().then(done);
      });
      return ws;
    };
    window.WebSocket.prototype = Native.prototype;
    Object.assign(window.WebSocket, Native);
  });
}

if (STORE) {
  await page.addInitScript(() => {
    // Poll rather than subscribe: the store handle appears some time after
    // load, and a missed subscription window would read as "never changed".
    window.__IMGPROBE__ = { n: 0, distinct: 0, sizes: [] };
    let lastRef = null;
    setInterval(() => {
      const st = window.__GP_STORE__ && window.__GP_STORE__.getState();
      const img = st && st.UIData && st.UIData.edit_info && st.UIData.edit_info.img;
      if (!img) return;
      const p = window.__IMGPROBE__;
      p.n++;
      if (img !== lastRef) { p.distinct++; lastRef = img; }
      const sz = img.jpegBlob ? img.jpegBlob.size : (img.img ? img.img.data.length : -1);
      if (p.sizes[p.sizes.length - 1] !== sz) p.sizes.push(sz);
    }, 300);
  });
}

if (FORCE_DIRTY > 0) {
  // Send the ST on the BROWSER'S OWN SOCKET -- do not open a second peer.
  //
  // A separate WS client opened BEFORE the page stopped the peripheral channel
  // from coming up at all: the core links to the board on the first peer's PD
  // CONNECT, and a peer that never sends one holds the slot. Measured
  // 2026-08-21 -- two runs died at "no perif channel", and the channel appeared
  // immediately once nothing but the page was attached. This is the same
  // first-peer trap perif_hold.mjs exists to demonstrate.
  //
  // Opening it AFTER the page instead is not reliable either: on a freshly
  // restarted core the late peer's ST was accepted and never acted on.
  //
  // So: no extra peer. The real UI sends ST over this exact socket
  // (WSCMD_CB("ST", 0, {...})), and so does this.
  await page.addInitScript(() => {
    const Native = window.WebSocket;
    window.WebSocket = function (...a) {
      const ws = new Native(...a);
      try { if (String(a[0]).includes('4090')) window.__APPWS__ = ws; } catch { }
      return ws;
    };
    window.WebSocket.prototype = Native.prototype;
    Object.assign(window.WebSocket, Native);
    window.__SEND_ST__ = (mset) => {
      const ws = window.__APPWS__;
      if (!ws || ws.readyState !== 1) return 'no app socket';
      const b = new TextEncoder().encode(JSON.stringify({ MachineSetting: mset }));
      const u = new Uint8Array(9 + b.length + 1);
      u[0] = 83; u[1] = 84; u[2] = 0;                       // 'S','T'
      const g = (window.__STPG__ = (window.__STPG__ || 50000) + 1);
      u[3] = g >> 8; u[4] = g & 255;
      const l = u.length - 9;
      u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
      u.set(b, 9);
      ws.send(u);
      return 'sent';
    };
  });
}

if (CLEANPROBE) {
  await page.addInitScript(() => {
    // Tally against the STORE, not the wire: the wire proves the core sent
    // something, the store proves the WebUI kept it. The old failure was
    // precisely a report that arrived and was then dropped by the reducer.
    window.__CLEANPROBE__ = { phase: 'pre', pre: mk(), dirty: mk(), post: mk() };
    function mk() { return { reports: 0, blocked: 0, imgs: 0, errs: {}, sample: null,
                             statFirst: null, statLast: null }; }
    let lastStation = null, lastImg = null, hooked = false;
    setInterval(() => {
      const store = window.__GP_STORE__;
      if (!store) return;
      if (!hooked) {
        hooked = true;
        store.subscribe(() => {
          const st = store.getState();
          const ei = st && st.UIData && st.UIData.edit_info;
          if (!ei) return;
          const b = window.__CLEANPROBE__[window.__CLEANPROBE__.phase];
          const stn = ei.station;
          if (stn && stn !== lastStation) {
            lastStation = stn; b.reports++;
            const ce = stn.clean_err;
            if (ce !== undefined && ce !== null) {
              b.blocked++;
              b.errs[String(ce)] = (b.errs[String(ce)] || 0) + 1;
              if (!b.sample) { try { b.sample = JSON.stringify(stn).slice(0, 300); } catch {} }
              b.last = null; try { b.last = JSON.stringify(stn).slice(0, 300); } catch {}
            }
          }
          if (stn && Array.isArray(stn.clean)) {
            // Did the threshold change actually bite? ratio 1.0 means every
            // pixel counted as dark, i.e. dark_thresh=255 is live. Anything
            // else means the command was accepted and did nothing.
            for (const c of stn.clean) {
              const r = c.dark_ratio;
              if (typeof r !== 'number') continue;
              if (b.rmin === undefined || r < b.rmin) b.rmin = r;
              if (b.rmax === undefined || r > b.rmax) b.rmax = r;
              b.dirtyRegions = b.dirtyRegions || {};
              if (c.dirty) b.dirtyRegions[c.name] = (b.dirtyRegions[c.name] || 0) + 1;
            }
          }
          // The UI's OWN counters. A blocked part must keep advancing these:
          // reportCount++ sits inside the reducer's per-feature loop, so an
          // empty TOP-level array would stop counting entirely and the part
          // would vanish from the statistics -- the exact regression the
          // clean-area work exists to prevent.
          const rss = ei.reportStatisticState;
          if (rss) {
            const snap = { n: rss.reportCount, empty: rss.emptyReportCount };
            if (!b.statFirst) b.statFirst = snap;
            b.statLast = snap;
          }
          if (ei.img && ei.img !== lastImg) { lastImg = ei.img; b.imgs++; }
        });
      }
    }, 250);
  });
}

console.log(`[1] cold load ${URL}`);
await page.goto(URL, { waitUntil: 'domcontentloaded' });
await sleep(6000);
if (await clickText('跳過相機連線', 'button')) { console.log('    dismissed the camera-reconnect modal'); await sleep(2000); }
await shot('landing');

if (haveBoard) {
  // STOP, then clear, THEN set the speed. Doing it the other way round --
  // speed first, clear second -- makes the board spin straight up to the new
  // speed the instant the fault clears, and at a speed it cannot sustain it is
  // back in 112 before the browser has finished loading. The UI then greys its
  // start button ("需先在設定面板清除") and the whole run fails at step 7 with
  // no hint that the cause was two steps earlier.
  // The peripheral channel is opened by the UI's PD CONNECT, not by the core
  // at start-up, so until the page is up every board command lands on
  // {"err":"no perif channel"} and quietly does nothing. Wait for a real
  // running_stat before touching anything.
  {
    let st = null;
    for (let i = 0; i < 20; i++) {
      st = await board.ask({ type: 'get_running_stat' }, 1200);
      if (st && st.state !== undefined) break;
      if (i === 0 && st && st.__err) console.log(`    waiting for the peripheral channel (${st.__err})`);
      await sleep(1200);
    }
    if (!st || st.state === undefined)
      await fail('no peripheral channel: the core never linked to the board'
              + (st && st.__err ? ` (${st.__err})` : ' (no answer on the dev console)'));
    console.log(`    peripheral channel up, board at state ${st.state}  err ${JSON.stringify(st.error_hist)}`);
  }
  console.log('[2] parking and clearing the board');
  await board.ask({ type: 'set_setup', plate: { freq: 0 } }, 1500);
  await sleep(4000);
  // exit_insp_mode BEFORE clear_error, or 112 never clears.
  //
  // 112 is INSPECTION_MODE_ERROR: the board is still IN inspection mode and
  // clear_error does not take it out. Measured 2026-08-21 -- a board sat in 112
  // through the full 22s of polling below and the run aborted with "board will
  // not leave state 112", which reads like a hardware fault; one exit_insp_mode
  // moved it to 100 (IDLE) on the first poll.
  //
  // How it got there is normal and worth knowing: stopping the core while the
  // machine is inspecting costs the board its host, and it faults with
  // error 12 HOST_LINK_TIMEOUT by design rather than keep sorting unjudged
  // parts. So every core rebuild lands the board here.
  await board.ask({ type: 'exit_insp_mode' }, 2000);
  await board.ask({ type: 'clear_error' }, 1200);
  await board.ask({ type: 'clear_error_history' }, 1200);

  // A board that will not leave 112 is the run's answer, not a step to push
  // past. Say so here rather than failing later on a disabled button.
  let ok = false;
  for (let i = 0; i < 15; i++) {
    const st = await board.ask({ type: 'get_running_stat' }, 1500);
    if (st && st.state !== 112) { ok = true; console.log(`    board at state ${st.state}`); break; }
    await sleep(1500);
  }
  if (!ok) {
    const st = await board.ask({ type: 'get_running_stat' }, 1500);
    if (!st || st.state === undefined)
      await fail('the board stopped answering during the park'
              + (st && st.__err ? ` (${st.__err})` : '') + ' -- not a 112 problem');
    await fail(`board will not leave 112 even after exit_insp_mode + clear_error; `
             + `err ${JSON.stringify(st.error_hist)}`);
  }

  // Back to IDLE so the UI does the starting.
  //
  // clear_error does not just clear -- it puts the board back INTO inspection
  // mode (112 -> 102 CAL -> 103 -> 101 READY), so the machine is already
  // running by the time the browser arrives. The UI then shows a STOP button
  // where the script looks for a play caret, and step 7 fails with "start
  // button not clickable" while everything is in fact fine. Parking at 100
  // makes the UI's start button a real start, which is the thing worth testing.
  await board.ask({ type: 'exit_insp_mode' }, 1500);
  for (let i = 0; i < 12; i++) {
    const st = await board.ask({ type: 'get_running_stat' }, 1200);
    if (st && st.state === 100) { console.log('    board parked at 100 (IDLE)'); break; }
    await sleep(1200);
  }

  if (FREQ > 0) {
    const r = await board.ask({ type: 'set_setup', plate: { freq: FREQ } });
    console.log(`    plate freq = ${FREQ} -> ${r && r.ack ? 'ack' : 'NOT ACKED'}`);
  }
}

console.log(`[3] recipe: ${RECIPE}`);
if (!await clickIcon('anticon-folder-open')) await fail('no recipe (folder) button');
await sleep(3000);
await clickText(RECIPE);
await sleep(900);
await page.evaluate((name) => {
  for (const e of document.querySelectorAll('*')) {
    if (e.children.length || !e.offsetParent) continue;
    if ((e.innerText || '').trim() !== name) continue;
    e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true })); return;
  }
}, RECIPE);
await sleep(5000);
await shot('recipe');

console.log(`[4] 製程 = ${PROCESS}   (must be one the recipe declares)`);
if (!await clickText(PROCESS)) await fail(`製程 "${PROCESS}" not on screen -- is it in this recipe's 已設定範圍?`);
await sleep(1500);
if (await playEnabled() === false)
  await fail(`play is still disabled after 製程 "${PROCESS}" -- that 製程 is outside the recipe's range`);

console.log(`[5] 檢測方式 = ${MODE}`);
if (!await clickText(MODE)) await fail(`檢測方式 "${MODE}" not on screen`);
await sleep(1500);
await shot('ready');

console.log('[6] into the Inspection UI');
if (!await clickIcon('anticon-caret-right')) await fail('play button not clickable');
await sleep(9000);
// The diagnostics drawer covers the panel this script reads.
await page.evaluate(() => { const x = document.querySelector('.ant-drawer-close'); if (x && x.offsetParent) x.click(); });
await sleep(1500);
await shot('inspection');

const before = haveBoard ? (await board.ask({ type: 'get_running_stat' }))?.gate?.edges ?? -1 : -1;

if (NOSTART) {
  console.log('[7] --no-start: leaving the machine as it is');
} else {
  console.log('[7] start the machine');
  if (!await clickIcon('anticon-caret-right')) await fail('machine start button not clickable');
  await sleep(12000);
}
console.log('    panel: ' + (await panel()).slice(0, 120));
await shot('running');

if (ZOOM > 0) {
  // Aim at the picture, not at the canvas centre. secCanvas is blitted under
  // the current transform, so the CTM is the only thing that knows where the
  // image landed; ask the page rather than guessing from the screenshot.
  const at = await page.evaluate(() => {
    const c = document.querySelector('canvas');
    if (!c) return null;
    const r = c.getBoundingClientRect();
    const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
    // Bounding box of everything actually painted (alpha > 0). On a cleared
    // canvas this comes back null, which is itself the answer.
    let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
    for (let y = 0; y < c.height; y += 2) for (let x = 0; x < c.width; x += 2) {
      if (d[(y * c.width + x) * 4 + 3] === 0) continue;
      if (x < x0) x0 = x; if (x > x1) x1 = x;
      if (y < y0) y0 = y; if (y > y1) y1 = y;
    }
    if (x1 < 0) return { painted: false, rect: r.toJSON() };
    const sx = r.width / c.width, sy = r.height / c.height;
    return { painted: true, box: { x0, y0, x1, y1 },
             cssX: r.left + ((x0 + x1) / 2) * sx, cssY: r.top + ((y0 + y1) / 2) * sy,
             rect: r.toJSON(), canvas: { w: c.width, h: c.height } };
  });
  if (!at || !at.painted) {
    console.log('[9z] nothing is painted on the canvas at all -- not a zoom problem');
  } else {
    const bw = at.box.x1 - at.box.x0, bh = at.box.y1 - at.box.y0;
    console.log(`[9z] painted content spans ${bw}x${bh} device px of a ${at.canvas.w}x${at.canvas.h} canvas`);
    console.log(`     zooming ${ZOOM} notches at (${at.cssX.toFixed(0)}, ${at.cssY.toFixed(0)})`);
    await page.mouse.move(at.cssX, at.cssY);
    for (let i = 0; i < ZOOM; i++) { await page.mouse.wheel(0, -120); await sleep(250); }
    await sleep(2500);
    const a = await shot('zoom_a');
    await sleep(4000);
    const b = await shot('zoom_b');
    console.log(`     ${a}`);
    console.log(`     ${b}`);
  }
}

// ---- the only check that counts -------------------------------------------
if (haveBoard) {
  console.log('[8] is the plate actually turning? (gate edges must climb)');
  await sleep(8000);
  const st = await board.ask({ type: 'get_running_stat' });
  const after = st?.gate?.edges ?? -1;
  const rpm = st ? (st.plate_freq_meas * 2 / 70400 * 60).toFixed(1) : '?';
  console.log(`    state ${st?.state}  err ${JSON.stringify(st?.error_hist)}  ${rpm} rpm`);
  console.log(`    gate edges ${before} -> ${after}`);
  if (after <= before)
    await fail('gate edges did not move -- the UI is up but no part reached the sensor');
  console.log(`    gate ${JSON.stringify(st.yield.gate)}`);

  // ---- and then keep watching -------------------------------------------
  // state 101 is INSPECTION_MODE_READY; 112 is INSPECTION_MODE_ERROR, which
  // the machine enters on a tracking-integrity fault and which STOPS THE PLATE.
  // error_hist is a list, so a fault that has already been cleared still shows
  // here -- which is what we want: it means "this run was not clean".
  const READY = 101;
  if (st.state !== READY || (st.error_hist || []).length)
    await fail(`machine is not clean at start: state ${st.state} err ${JSON.stringify(st.error_hist)}`);

  if (WATCH > 0) {
    console.log(`[9] watching ${WATCH}s for faults`);
    const t0 = Date.now(); let last = after, ticks = 0;
    let lastGrid = null, gridSamples = 0;
    const tileHits = Array.from({length:8},()=>Array(8).fill(0));
    while ((Date.now() - t0) / 1000 < WATCH) {
      await sleep(5000);
      const w = await board.ask({ type: 'get_running_stat' });
      if (!w) { console.log('    (no reply)'); continue; }
      ticks++;
      const errs = w.error_hist || [];
      if (w.state !== READY || errs.length) {
        const ui = await uiWarnings();
        console.log(`    t+${((Date.now() - t0) / 1000).toFixed(0)}s  state ${w.state}  err ${JSON.stringify(errs)}`);
        if (ui.length) console.log('    panel says: ' + ui.join(' | '));
        console.log(`    gate ${JSON.stringify(w.yield.gate)}  rpm ${(w.plate_freq_meas * 2 / 70400 * 60).toFixed(1)}`);
        await fail(`faulted after ${((Date.now() - t0) / 1000).toFixed(0)}s -- state ${w.state}, err ${JSON.stringify(errs)}`);
      }
      if (LIVE) {
        // Read the CANVAS PIXELS, tile by tile. Clipping a screenshot and
        // hashing it was wrong twice in opposite directions: a clip covering
        // the machine panel changes whenever a counter ticks ("live!"), and a
        // clip landing on the page background outside the picture never changes
        // ("frozen!"). Both were confident and neither measured the photograph.
        //
        // A grid says WHICH tiles move. The overlay lives in a few of them; the
        // picture covers many. That distinction is the whole question and it is
        // visible rather than asserted.
        const g = await page.evaluate(() => {
          const el = document.querySelector('canvas'); if (!el) return null;
          const ctx = el.getContext('2d'); if (!ctx) return 'no2d';
          const W = el.width, H = el.height, N = 8, out = [];
          for (let ty = 0; ty < N; ty++) { const row = [];
            for (let tx = 0; tx < N; tx++) {
              const d = ctx.getImageData(Math.floor(tx * W / N), Math.floor(ty * H / N),
                                         Math.floor(W / N), Math.floor(H / N)).data;
              let v = 0; for (let k = 0; k < d.length; k += 97) v = (v * 31 + d[k]) >>> 0;
              row.push(v);
            } out.push(row); }
          return out;
        }).catch(() => null);
        if (Array.isArray(g)) {
          if (lastGrid) for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++)
            if (lastGrid[y][x] !== g[y][x]) tileHits[y][x]++;
          lastGrid = g; gridSamples++;
        }
      }
      if (ticks % 4 === 0)
        console.log(`    t+${((Date.now() - t0) / 1000).toFixed(0)}s  ok  edges ${w.gate.edges} (+${w.gate.edges - last})  cal ${w.cam_sync.cal_fails}/${w.cam_sync.cal_pulse_lost}`);
      last = w.gate.edges;
    }
    console.log(`    ${WATCH}s clean`);
    if (STORE) {
      const r = await page.evaluate(() => window.__IMGPROBE__ || null).catch(() => null);
      if (r) {
        console.log(`    store edit_info.img: ${r.n} samples, ${r.distinct} distinct refs, `
                  + `${r.sizes.length} distinct payload sizes`);
        console.log('    last sizes: ' + r.sizes.slice(-6).join(', '));
      } else console.log('    store probe: no data (dev bundle only)');
    }
    if (FORCE_DIRTY > 0) {
      const setPhase = (ph) => page.evaluate((p) => { if (window.__CLEANPROBE__) window.__CLEANPROBE__.phase = p; }, ph);
      let mset;
      try { mset = JSON.parse(fs.readFileSync(MSET_PATH, 'utf8')); }
      catch (e) { console.log(`[9d] cannot read ${MSET_PATH}: ${e.message}`); mset = null; }
      if (mset && (mset.clean_regions || []).length === 0) {
        console.log('[9d] no clean_regions configured -- nothing to make dirty');
      } else if (mset) {
        // Send BOTH keys. setup_machine_setting reloads the station box too and
        // reads an absent key as "none", so a clean_regions-only payload wipes
        // inspection_region and the whole run stops meaning anything.
        const keep = { inspection_region: mset.inspection_region, clean_regions: mset.clean_regions };
        console.log(`[9d] forcing the clean regions dirty for ${FORCE_DIRTY}s `
                  + `(dark_thresh ${keep.clean_regions.map((c) => c.dark_thresh).join('/')} -> 255, runtime only)`);
        try {
          await setPhase('dirty');
          await page.evaluate(() => { if (window.__WSTAP__) window.__WSTAP__.captureDirty = true; }).catch(() => {});
          const sent = await page.evaluate((m) => window.__SEND_ST__(m), dirtied(keep, 255));
          console.log('     ST via the page socket: ' + sent);
          if (sent !== 'sent') throw new Error('could not send ST from the page');
          // Watch the PICTURE, not just the counters. Sample the rectangle the
          // bitmap is actually blitted into -- taken from the live CTM, so it
          // follows any zoom -- and hash it. Distinct hashes over the window is
          // the number that says the image kept moving while every frame was
          // being rejected.
          const sampleImg = () => page.evaluate(() => {
            const g = window.__CANVAS_DIAG__ && window.__CANVAS_DIAG__.geo;
            const c = document.querySelector('canvas');
            if (!g || !c) return null;
            const [a, , , d, e, f] = g.ctm.split(' ').map(Number);
            const [sw, sh] = g.sec.split('x').map(Number);
            const x = Math.max(0, Math.round(e)), y = Math.max(0, Math.round(f));
            const w = Math.min(c.width - x, Math.round(Math.abs(a) * sw));
            const h = Math.min(c.height - y, Math.round(Math.abs(d) * sh));
            if (w < 2 || h < 2) return null;
            const px = c.getContext('2d').getImageData(x, y, w, h).data;
            let hash = 2166136261, opaque = 0;
            for (let i = 0; i < px.length; i += 4) {
              if (px[i + 3] !== 0) opaque++;
              hash = ((hash ^ px[i]) * 16777619) >>> 0;
              hash = ((hash ^ px[i + 1]) * 16777619) >>> 0;
            }
            return { hash, w, h, opaquePct: Math.round((100 * opaque) / (px.length / 4)) };
          }).catch(() => null);

          const seen = new Set();
          let shape = null, nulls = 0;
          const ticks = Math.max(1, Math.round((FORCE_DIRTY * 1000) / 500));
          for (let i = 0; i < ticks; i++) {
            const r = await sampleImg();
            if (!r) nulls++; else { seen.add(r.hash); shape = r; }
            if (i === Math.floor(ticks / 3)) console.log('     ' + await shot('dirty_a'));
            if (i === Math.floor((2 * ticks) / 3)) console.log('     ' + await shot('dirty_b'));
            await sleep(500);
          }
          console.log(`     picture region ${shape ? shape.w + 'x' + shape.h : '?'} px, `
                    + `${shape ? shape.opaquePct : '?'}% painted: `
                    + `${seen.size} distinct frames in ${ticks} samples`
                    + (nulls ? ` (${nulls} unreadable)` : ''));
        } catch (e) {
          console.log('     FAILED to apply: ' + e.message);
        } finally {
          // Restore even if the wait threw. Leaving a machine with its clean
          // regions tripping on every frame is a worse outcome than a failed
          // measurement.
          try {
            console.log('     restoring: '
                      + await page.evaluate((m) => window.__SEND_ST__(m), keep));
          } catch (e) { console.log('     RESTORE FAILED: ' + e.message + ' -- restart the core'); }
          await page.evaluate(() => { if (window.__WSTAP__) window.__WSTAP__.captureDirty = false; }).catch(() => {});
          await setPhase('post');
          await sleep(6000);
        }
      }
    }
    if (CLEANPROBE) {
      const cp = await page.evaluate(() => window.__CLEANPROBE__ || null).catch(() => null);
      if (!cp) console.log('    clean probe: no data (needs the dev bundle for __GP_STORE__)');
      else {
        console.log('    clean-area tally, as seen by redux in the browser:');
        for (const ph of ['pre', 'dirty', 'post']) {
          const b = cp[ph];
          const errs = Object.entries(b.errs).map(([k, v]) => `err${k}x${v}`).join(' ') || '-';
          const rng = (b.rmin === undefined) ? 'n/a'
            : `${b.rmin.toFixed(3)}..${b.rmax.toFixed(3)}`;
          const dr = Object.entries(b.dirtyRegions || {}).map(([k, v]) => `${k}x${v}`).join(' ') || 'none';
          console.log(`      ${ph.padEnd(5)} reports ${String(b.reports).padStart(4)}  `
                    + `blocked ${String(b.blocked).padStart(4)}  images ${String(b.imgs).padStart(4)}  ${errs}`);
          console.log(`            dark_ratio ${rng}   regions tripping: ${dr}`);
          if (b.statFirst && b.statLast) {
            console.log(`            UI statistics: reportCount ${b.statFirst.n} -> ${b.statLast.n}`
                      + `  (+${b.statLast.n - b.statFirst.n}),  emptyReportCount `
                      + `${b.statFirst.empty} -> ${b.statLast.empty}`
                      + `  (+${b.statLast.empty - b.statFirst.empty})`);
          }
        }
        if (cp.dirty.sample) console.log('      a blocked station block: ' + cp.dirty.sample);
      }
    }
    if (DIAG) {
      // Alpha is the only honest discriminator on a backlit station. A cleared
      // canvas is transparent black and composites to the page's white, which
      // looks EXACTLY like the bright field of a real backlit frame. Nothing in
      // an RGB screenshot can tell those apart; alpha can. Sampled on a grid so
      // "the picture is drawn" is a fact about pixels, not about how a
      // screenshot happens to read.
      const alpha = await page.evaluate(() => {
        const c = document.querySelector('canvas');
        if (!c) return null;
        const g = c.getContext('2d');
        const d = g.getImageData(0, 0, c.width, c.height).data;
        let opaque = 0, clear = 0, bright = 0, dark = 0, n = 0;
        for (let y = 0; y < c.height; y += 8) for (let x = 0; x < c.width; x += 8) {
          const i = (y * c.width + x) * 4; n++;
          if (d[i + 3] === 0) { clear++; continue; }
          opaque++;
          const lum = (d[i] + d[i + 1] + d[i + 2]) / 3;
          if (lum > 200) bright++; else if (lum < 90) dark++;
        }
        return { n, opaque, clear, bright, dark, w: c.width, h: c.height };
      }).catch(() => null);
      if (alpha) {
        const pct = (v) => ((100 * v) / alpha.n).toFixed(1) + '%';
        console.log(`    canvas ${alpha.w}x${alpha.h}, ${alpha.n} sampled: `
                  + `painted ${pct(alpha.opaque)}, untouched ${pct(alpha.clear)}`);
        console.log(`      of the painted: bright ${pct(alpha.bright)}  dark ${pct(alpha.dark)}`);
      }
      const d = await page.evaluate(() => window.__CANVAS_DIAG__ || null).catch(() => null);
      if (!d) console.log('    canvas diag: absent -- probes not in this bundle');
      else {
        console.log('    canvas diag:');
        console.log(`      updateCanvas ${d.updCanvas|0}   of which img changed ${d.imgChanged|0}   surpressed ${d.surpress|0}`);
        console.log(`      SetImg calls ${d.setImg|0}   null ${d.setImg_null|0}   same-ref early-out ${d.setImg_same|0}`);
        console.log(`      decodes done ${d.decoded|0}   stale-token drops ${d.stale|0}   last bitmap ${d.lastBmp||'-'}`);
        console.log(`      secCanvas blits ${d.blit|0}   blit source ${d.blitSrc||'-'}`);
        console.log(`      props.img present ${d.hasImg}   carries jpegBlob ${d.jpegBlob}`);
        if (d.fit) { console.log('      the one-shot view fit (never recomputed):');
          for (const [k,v] of Object.entries(d.fit)) console.log(`        ${k} = ${v}`); }
        if (d.geo) { console.log('      geometry at the last blit:');
          for (const [k,v] of Object.entries(d.geo)) console.log(`        ${k} = ${v}`); }
      }
    }
    if (WSTAP) {
      const t = await page.evaluate(() => window.__WSTAP__).catch(() => null);
      if (t) {
        console.log(`    ws frames ${t.frames}, ${(t.bytes / 1048576).toFixed(1)} MB`);
        console.log('    by type: ' + Object.entries(t.byTL).map(([k, v]) => `${k}=${v}`).join('  '));
        console.log(`    IM frames whose payload was shorter than declared: ${t.shortIM}`);
        if ((t.rpClean || []).length || (t.rpBlocked || []).length || (t.rpDirty || []).length) {
          const dir = SHOTS;
          [...(t.rpClean || []).map((x, i) => ['rp_notblocked_' + i, x]),
           ...(t.rpBlocked || []).map((x, i) => ['rp_blocked_' + i, x]),
           ...(t.rpDirty || []).map((x, i) => ['rp_forced_' + i, x])].forEach(([name, txt]) => {
            const f = `${dir}/${name}.json`;
            try { fs.writeFileSync(f, txt); console.log(`    wrote ${f} (${txt.length} B)`); }
            catch (e) { console.log(`    could not write ${f}: ${e.message}`); }
          });
        }
      }
    }
    if (LIVE) {
      const moved = tileHits.flat().filter((n) => n > 0).length;
      console.log(`    canvas tiles, ${gridSamples} samples:`);
      for (const row of tileHits)
        console.log('      ' + row.map((n) => (n ? String(Math.min(9, n)) : '.')).join(' '));
      console.log(`    ${moved}/64 tiles changed at least once`);
      console.log(moved >= 8
        ? '    -> PHOTO IS LIVE: the picture area is being repainted'
        : moved > 0
          ? '    -> OVERLAY ONLY: a few tiles move, the picture behind them does not'
          : '    -> FROZEN: nothing on the canvas changed at all');
    }
  }
  console.log('PASS: machine is running, parts reaching the gate, no fault raised');
} else {
  console.log('PASS (UI only): no console, so the machine side was not verified');
}

await browser.close();
board.end();
