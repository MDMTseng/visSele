// Get the board out of state 112 (INSPECTION_MODE_ERROR), and SAY WHY first.
//
//   node board_rescue.mjs [--url http://localhost:8081/] [--headed]
//
// Two things this exists to handle that the bringup's park step does not:
//
//   1. THE CONSOLE NEEDS A BROWSER. The core opens its peripheral channel on a
//      UI PD CONNECT, not at start-up, so with no page attached port 4099
//      answers {"err":"no perif channel"} and every board command silently goes
//      nowhere. This attaches a page first and does nothing else with it.
//
//   2. READ THE HISTORY BEFORE CLEARING IT. pw_bringup's park calls
//      clear_error_history as part of getting to a clean start -- which is
//      right for a bringup and wrong for a diagnosis, because it destroys the
//      only record of why the board faulted. This prints error_hist first.
//
// 112 is not cleared by clear_error alone: the board is IN inspection mode and
// has to be taken out of it. exit_insp_mode first, then clear.
import { chromium } from 'playwright';
import net from 'node:net';

const arg = (n, d) => { const i = process.argv.indexOf('--' + n);
  return i >= 0 && process.argv[i + 1] && !process.argv[i + 1].startsWith('--') ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8081/');
const HEADED = process.argv.includes('--headed');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const br = await chromium.launch({ headless: !HEADED });
const page = await br.newPage({ viewport: { width: 1600, height: 950 } });
console.log(`attaching a page to ${URL} (this is what opens the peripheral channel)`);
await page.goto(URL, { waitUntil: 'domcontentloaded' });
await sleep(7000);
try {
  const btn = page.locator('button', { hasText: '跳過相機連線' });
  if (await btn.count() && await btn.first().isVisible()) { await btn.first().click(); await sleep(1500); }
} catch {}

const s = net.connect(4099, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => { buf += d.toString('latin1'); let n;
  while ((n = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, n)); buf = buf.slice(n + 1); } });
s.on('error', (e) => { console.log('console 4099:', e.message); });
await new Promise((r) => s.once('connect', r));
let id = 95000;
async function ask(o, ms = 1800) {
  const my = id++; lines = [];
  s.write(JSON.stringify({ ...o, id: my }) + '\n');
  await sleep(ms);
  const h = lines.find((l) => l.includes(`"id":${my}`));
  if (!h) return lines.length ? { raw: lines[0] } : null;
  try { return JSON.parse(h.slice(h.indexOf('{'))); } catch { return { raw: h }; }
}

const st0 = await ask({ type: 'get_running_stat' }, 2500);
if (!st0) { console.log('board not answering at all'); await br.close(); process.exit(1); }
console.log('state      :', st0.state);
console.log('error_hist :', JSON.stringify(st0.error_hist));
console.log('health     :', JSON.stringify(st0.health));
console.log('cam_sync   :', JSON.stringify(st0.cam_sync));

if (st0.state === 112 || st0.state === undefined) {
  console.log('\nrecovering: stop -> exit_insp_mode -> clear_error');
  await ask({ type: 'set_setup', plate: { freq: 0 } }, 1500);
  await sleep(3000);
  console.log('  exit_insp_mode ->', JSON.stringify(await ask({ type: 'exit_insp_mode' }, 2500)));
  await ask({ type: 'clear_error' }, 1500);
  for (let i = 0; i < 20; i++) {
    const st = await ask({ type: 'get_running_stat' }, 1200);
    if (st && st.state !== 112) { console.log(`  board now at state ${st.state} after ${i + 1} polls`); break; }
    if (i === 19) console.log(`  STILL 112 -- err ${JSON.stringify(st && st.error_hist)}`);
    await sleep(1200);
  }
}
const st1 = await ask({ type: 'get_running_stat' }, 1800);
console.log('\nfinal state:', st1 && st1.state, ' err', JSON.stringify(st1 && st1.error_hist));
s.end();
await br.close();
