// The manual blow, spoken to the board directly. Read-only by default.
//   node blow_probe.mjs         what the board says about its selector state
//   node blow_probe.mjs 1       fire SEL1 once
import net from 'node:net';
const s = net.connect(4099, '127.0.0.1');
let buf = '', got = null;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
s.on('data', (d) => {
  buf += d.toString('utf8');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).trim(); buf = buf.slice(nl + 1);
    if (!line.startsWith('{')) continue;
    try { const j = JSON.parse(line);
      if (j.type === 'blow' || j.plate_running !== undefined || j.sys_state !== undefined) got = j;
    } catch {}
  }
});
s.on('error', (e) => { console.error('console 4099:', e.message); process.exit(1); });
await new Promise((r) => s.on('connect', r));
async function ask(o, ms = 3000) {
  got = null; s.write(JSON.stringify(o) + '\n');
  const t = Date.now(); while (!got && Date.now() - t < ms) await sleep(50);
  return got;
}
const sel = process.argv[2];
if (sel) console.log('blow:', JSON.stringify(await ask({ type: 'blow', sel: Number(sel) })));
else {
  const st = await ask({ type: 'get_running_stat' });
  console.log(st ? JSON.stringify({ state: st.sys_state, plate: st.plate_running,
    dry: st.dry_run, min_sep_us: st.min_sep_us }) : '(no reply)');
}
s.end();
