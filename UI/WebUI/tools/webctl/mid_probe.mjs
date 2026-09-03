// The 單顆 (middle) blow test, spoken to the board directly.
//
//   node mid_probe.mjs            read the state
//   node mid_probe.mjs mid        enter middle mode (HOLD: nothing is blown)
//   node mid_probe.mjs start      arm: the 2nd part judged from now gets SELn
//   node mid_probe.mjs off        back to real verdicts
//
// Needs the core running with INSP_PERIF_CONSOLE=4099.
import net from 'node:net';
const PORT = 4099;
const s = net.connect(PORT, '127.0.0.1');
let buf = '', got = null;
const send = (o) => s.write(JSON.stringify(o) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
s.on('data', (d) => {
  buf += d.toString('utf8');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).trim(); buf = buf.slice(nl + 1);
    if (!line.startsWith('{')) continue;
    try { const j = JSON.parse(line); if (j.type === 'sel_test' || j.sel_test !== undefined) got = j; } catch {}
  }
});
s.on('error', (e) => { console.error(`console ${PORT}:`, e.message); process.exit(1); });
await new Promise((r) => s.on('connect', r));

async function ask(o, ms = 3000) {
  got = null; send(o);
  const t = Date.now();
  while (!got && Date.now() - t < ms) await sleep(50);
  return got;
}

const what = process.argv[2] || 'stat';
if (what === 'stat') {
  const st = await ask({ type: 'get_running_stat' });
  console.log('sel_test:', st ? JSON.stringify({ mode: st.sel_test, sel: st.sel_test_sel,
    armed: st.sel_test_armed }) : '(off / no reply)');
} else if (what === 'start') {
  console.log('start :', JSON.stringify(await ask({ type: 'sel_test', start: true })));
} else {
  console.log(what, ':', JSON.stringify(await ask({ type: 'sel_test', mode: what, sel: 1 })));
}
s.end();
