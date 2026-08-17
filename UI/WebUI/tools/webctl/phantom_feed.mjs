// Feed the running machine simulated parts: one trig_phantom_pulse per part,
// straight down the core's perif console (4099). No PD CONNECT -- that would
// toggle DTR and reset the board mid-run (peek.py has the same discipline).
//   node phantom_feed.mjs <parts-per-sec> <secs>
import net from 'node:net';
const rate = Number(process.argv[2] || 25), secs = Number(process.argv[3] || 60);
const s = net.connect(4099, '127.0.0.1');
let n = 0;
s.on('connect', () => {
  const iv = setInterval(() => { s.write('{"type":"trig_phantom_pulse"}\n'); n++; }, 1000 / rate);
  setTimeout(() => { clearInterval(iv); console.log('phantoms sent:', n); s.end(); process.exit(0); }, secs * 1000);
});
s.on('error', e => { console.error('console 4099:', e.message); process.exit(1); });
