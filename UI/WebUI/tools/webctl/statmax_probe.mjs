// Prove that get_running_stat's `reset_stat_maximum` does what it claims.
//
//   node statmax_probe.mjs
//
// Needs a browser attached (the peripheral channel is opened by a UI PD
// CONNECT), so it launches one and does nothing else with it.
//
// What it checks, in order:
//   1. WITHOUT the flag, the peak-hold does not decay -- the old envelope lost
//      a 7us peak in 0.3s, which is why a once-a-minute soak never saw one.
//   2. stat_max_window_ms grows between polls and RESETS when the flag is sent,
//      so a reader can prove what interval its max covers.
//   3. The flag clears the maxima...
//   4. ...and does NOT touch the cumulative counts (skip / unanswered /
//      gate.loss_n). That separation is the whole point: a soak polling every
//      minute must keep its own baseline.
import { chromium } from 'playwright';
import net from 'node:net';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const br = await chromium.launch({ headless: true });
const page = await br.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto('http://localhost:8082/', { waitUntil: 'domcontentloaded' });
await sleep(7000);
try {
  const b = page.locator('button', { hasText: '跳過相機連線' });
  if (await b.count() && await b.first().isVisible()) { await b.first().click(); await sleep(1500); }
} catch {}

let sock = null, buf = '', lines = [], id = 91000;
function connect() {
  return new Promise((res) => {
    const s = net.connect(4099, '127.0.0.1');
    let done = false;
    const fin = (v) => { if (!done) { done = true; res(v); } };
    s.on('data', (d) => { buf += d.toString('latin1'); let n;
      while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); } });
    s.on('error', () => fin(null));
    s.on('close', () => { if (sock === s) sock = null; });
    s.once('connect', () => fin(s));
  });
}
async function ask(o, ms = 1800) {
  if (!sock) { sock = await connect(); if (!sock) return null; buf = ''; }
  const my = id++; lines = [];
  try { sock.write(JSON.stringify({ ...o, id: my }) + '\n'); } catch { sock = null; return null; }
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return null; }
}

let st = null;
for (let i = 0; i < 20 && !st; i++) { st = await ask({ type: 'get_running_stat' }, 2500); if (!st) { sock = null; await sleep(3000); } }
if (!st) { console.log('FAILED: no board console'); await br.close(); process.exit(1); }

const cs = (s) => (s && s.cam_sync) || {};
const yg = (s) => (s && s.yield && s.yield.gate) || {};
const yv = (s) => (s && s.yield && s.yield.verdict) || {};
const row = (tag, s) => console.log(
  `${tag.padEnd(26)} win_ms ${String(s.stat_max_window_ms).padStart(7)}  ` +
  `resid_max ${String(cs(s).resid_max_us).padStart(5)}  dmax ${String(cs(s).delta_max_us).padStart(5)}  ` +
  `| skip ${yv(s).skip}  unans ${yv(s).unanswered}  loss_n ${yg(s).loss_n}`);

console.log(`board state ${st.state}  err ${JSON.stringify(st.error_hist)}`);
console.log(`stat_max_window_ms present: ${st.stat_max_window_ms !== undefined}`);
console.log('');
row('[1] plain poll', st);
await sleep(5000);
const b2 = await ask({ type: 'get_running_stat' });
row('[2] +5s, no flag', b2);
const b3 = await ask({ type: 'get_running_stat', reset_stat_maximum: true });
row('[3] WITH reset (reports first)', b3);
const b4 = await ask({ type: 'get_running_stat' });
row('[4] right after reset', b4);
await sleep(5000);
const b5 = await ask({ type: 'get_running_stat' });
row('[5] +5s after reset', b5);

const ok = [];
ok.push(['window grows without the flag', (b2.stat_max_window_ms > st.stat_max_window_ms)]);
ok.push(['window resets with the flag', (b4.stat_max_window_ms < b3.stat_max_window_ms)]);
ok.push(['counts survive the reset',
         yv(b4).skip === yv(b3).skip && yv(b4).unanswered === yv(b3).unanswered &&
         yg(b4).loss_n === yg(b3).loss_n]);
console.log('');
for (const [k, v] of ok) console.log(`  ${v ? 'PASS' : 'FAIL'}  ${k}`);
await br.close();
