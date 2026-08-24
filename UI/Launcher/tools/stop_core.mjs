// Ask the core to stop, the way the launcher does, instead of killing it.
//
//   node tools/stop_core.mjs [port]
//
// Every hard kill in this session cost a bring-up: TerminateProcess does not
// run the core's teardown, the serial link to the board is left open, and the
// next run dies with "board not answering after 20 tries" until the port times
// out. The control socket exists precisely so that does not have to happen --
// it acks, sets g_shutdownRequested, and the core closes the link on its way
// out (measured 0.8-1.1 s).
import net from 'node:net';

const PORT = Number(process.argv[2] || 4098);
const ask = (msg, ms = 4000) => new Promise((resolve) => {
  const s = net.connect(PORT, '127.0.0.1');
  let buf = '';
  const done = (v) => { try { s.destroy(); } catch {} resolve(v); };
  const t = setTimeout(() => done(null), ms);
  s.on('connect', () => s.write(JSON.stringify(msg) + '\n'));
  s.on('data', (d) => {
    buf += d.toString();
    const nl = buf.indexOf('\n');
    if (nl < 0) return;
    clearTimeout(t);
    try { done(JSON.parse(buf.slice(0, nl))); } catch { done(null); }
  });
  s.on('error', () => { clearTimeout(t); done(null); });
});

const pong = await ask({ type: 'ping' });
if (!pong) { console.log('no core on ' + PORT); process.exit(0); }
console.log(`core pid ${pong.pid} up ${pong.uptime_s}s -- asking it to stop`);
await ask({ type: 'shutdown' });

// Wait for it to actually go, so a caller can start the next run straight
// after. Reporting "asked" and returning is how the race gets back in.
for (let i = 0; i < 60; i++) {
  await new Promise((r) => setTimeout(r, 250));
  if (!(await ask({ type: 'ping' }, 700))) { console.log('core is down'); process.exit(0); }
}
console.log('core still answering after 15 s');
process.exit(1);
