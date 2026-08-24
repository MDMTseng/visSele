// Show the board's IO map, and optionally pulse one output to check its wiring.
//
//   node io_probe.mjs                 -- print the map
//   node io_probe.mjs SEL2 [ms]       -- print the map, then pulse SEL2
//
// Needs a browser attached: the peripheral channel is opened by the UI's PD
// CONNECT, so this launches one and does nothing else with it.
//
// io_test drives through io_drive(), the same path the machine uses in
// production, so polarity is applied. pin_on/pin_off are raw digitalWrite and
// would drive the physical level -- on this machine's active-low channels that
// is backwards, and a test that takes a different path than production proves
// nothing about production.
import { chromium } from 'playwright';
import net from 'node:net';

const NAME = process.argv[2] || null;
const MS = Number(process.argv[3] || 50);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const br = await chromium.launch({ headless: true });
const pg = await br.newPage();
await pg.goto('http://localhost:8082/', { waitUntil: 'domcontentloaded' });
await sleep(7000);
try {
  const b = pg.locator('button', { hasText: '跳過相機連線' });
  if (await b.count() && await b.first().isVisible()) { await b.first().click(); await sleep(1500); }
} catch {}

let sock = null, buf = '', lines = [], id = 95500;
function conn() {
  return new Promise((res) => {
    const s = net.connect(4099, '127.0.0.1');
    let d = false; const f = (v) => { if (!d) { d = true; res(v); } };
    s.on('data', (x) => { buf += x.toString('latin1'); let n;
      while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); } });
    s.on('error', () => f(null));
    s.on('close', () => { if (sock === s) sock = null; });
    s.once('connect', () => f(s));
  });
}
async function ask(o, ms = 2500) {
  if (!sock) { sock = await conn(); if (!sock) return null; buf = ''; }
  const my = id++; lines = [];
  try { sock.write(JSON.stringify({ ...o, id: my }) + '\n'); } catch { sock = null; return null; }
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return null; }
}

let st = null;
for (let i = 0; i < 15 && !st; i++) { st = await ask({ type: 'get_running_stat' }); if (!st) { sock = null; await sleep(3000); } }
if (!st) { console.log('FAILED: no board console'); await br.close(); process.exit(1); }
console.log(`board state ${st.state}  err ${JSON.stringify(st.error_hist)}`);

const su = await ask({ type: 'get_setup' }, 3000);
if (!su || !su.io_map) { console.log('FAILED: get_setup has no io_map'); await br.close(); process.exit(1); }
console.log('');
console.log('name          pin  dir  on_level');
for (const e of su.io_map) {
  console.log(`${String(e.name).padEnd(13)} ${String(e.pin).padStart(3)}  ${String(e.dir).padEnd(4)} ` +
              `${e.on_level === undefined ? '-' : e.on_level}`);
}

if (NAME) {
  console.log('');
  console.log(`pulsing ${NAME} for ${MS} ms ...`);
  const r = await ask({ type: 'io_test', name: NAME, ms: MS }, 2500 + MS);
  console.log('  ->', JSON.stringify(r));
}
await br.close();
