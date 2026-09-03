// Drive the firmware's own crash_test and watch what reaches the host.
//
// Two things are being checked, and they are separate:
//   * the panic backtrace, which is not our protocol and used to be dropped
//     one byte at a time in recv_ERROR -- it should now surface as ONE line
//   * the boot announcement, which the firmware sends on the existing
//     system_info event once a frame has arrived from the host
import net from 'node:net';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const s = net.connect(4099, '127.0.0.1');
let buf = '';
s.on('data', (d) => {
  buf += d.toString('utf8');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).replace(/\r/g, ''); buf = buf.slice(nl + 1);
    if (!line.trim()) continue;
    const t = Date.now() - t0;
    if (/system_info|dbg|boot|reset_reason|panic|Backtrace|rst:/i.test(line))
      console.log(`[${t}ms] ${line.slice(0, 220)}`);
  }
});
s.on('error', (e) => { console.error('4099:', e.message); process.exit(1); });
const t0 = Date.now();
await new Promise((r) => s.on('connect', r));
console.log('console attached; settling 4s');
await sleep(4000);
console.log('--- sending crash_test ---');
s.write(JSON.stringify({ type: 'crash_test', confirm: true }) + '\n');
await sleep(25000);
console.log('--- done ---');
s.end();
process.exit(0);
