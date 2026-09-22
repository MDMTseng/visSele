// Probe against declared discount, on one machine and one batch of parts.
//
//   node compare_throttle.mjs [minutesPerConfig]
//
// The two mechanisms were each measured on a different machine on a different
// day, which is not a comparison. This runs them back to back on the same
// hardware with the same parts flowing, changing one setting between blocks.
//
// Assumes the machine is already RUNNING (soak_start.ps1). It changes only the
// throttle configuration and puts it back at the end.
import net from 'node:net';
import fs from 'node:fs';

const MIN = Number(process.argv[2] || 4);
const OUT = 'compare_throttle.json';

function perif(cmd, want, ms = 8000) {
  return new Promise((r) => {
    const s = net.connect(4099, '127.0.0.1');
    let b = '';
    const d = (v) => { try { s.end(); } catch (e) {} r(v); };
    s.on('connect', () => s.write(JSON.stringify(cmd) + String.fromCharCode(10)));
    s.on('data', (x) => {
      b += x.toString();
      for (const l of b.split(String.fromCharCode(10))) {
        const t = l.trim();
        if (!t.startsWith('{') || (want && t.indexOf(want) < 0)) continue;
        try { return d(JSON.parse(t)); } catch (e) { /* partial */ }
      }
    });
    s.on('error', () => d(null));
    setTimeout(() => d(null), ms);
  });
}
const stat = () => perif({ type: 'get_running_stat' }, '"gate"');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function block(label, patch) {
  await perif({ type: 'set_setup', gate: patch }, '"ack"');
  // A settling window before measuring. The probe needs it most -- it walks
  // downward at ~6%/s -- and giving every block the same one keeps the
  // comparison about the mechanisms rather than about who was measured sooner.
  console.log(`   ${label}: settling 60s`);
  await sleep(60000);

  const a = await stat();
  const rows = [];
  const t0 = Date.now();
  while (Date.now() - t0 < MIN * 60000) {
    await sleep(5000);
    const s = await stat();
    if (s) rows.push(s);
  }
  const b = rows[rows.length - 1] || a;
  const d = (k, g) => (g ? (b.gate[k] | 0) - (a.gate[k] | 0) : (b.count[k] | 0) - (a.count[k] | 0));
  const mins = (Date.now() - t0) / 60000;
  const hz = (v) => (v > 0 ? 1e6 / v : 0);
  const avg = (f) => rows.reduce((x, r) => x + (f(r) || 0), 0) / Math.max(1, rows.length);

  const res = {
    label,
    cap_hz: avg((r) => hz(r.gate.proc_eff_us)),
    admit_hz: d('accept', true) / (mins * 60),
    judged: d('SEL1') + d('SEL3') + d('NA'),
    unans: d('UNANSWERED'), skip: d('SKIP'),
    rej_load: d('rej_load', true),
    rho: avg((r) => r.gate.proc_rho_pct),
    svc_med: avg((r) => r.gate.proc_svc_us) / 1000,
    svc_mean: avg((r) => r.gate.proc_svc_mean_us) / 1000,
    waiting: avg((r) => (r.pipe || {}).waiting),
    probe: (b.gate.proc_probe_up_n | 0) - (a.gate.proc_probe_up_n | 0),
    backoff: (b.gate.proc_backoff_n | 0) - (a.gate.proc_backoff_n | 0),
    lat_avg: avg((r) => (r.report_latency || {}).cam_avg_us) / 1000,
    state: [...new Set(rows.map((r) => r.state))],
  };
  console.log(`   ${label.padEnd(14)} 上限 ${res.cap_hz.toFixed(1)}/s  實際 ${res.admit_hz.toFixed(1)}/s`
    + `  rho ${res.rho.toFixed(0)}%  服務 ${res.svc_med.toFixed(0)}/${res.svc_mean.toFixed(0)}ms`
    + `  佇列 ${res.waiting.toFixed(1)}  無判決 ${res.unans}  擋下 ${res.rej_load}`
    + `  探測 ${res.probe} 退讓 ${res.backoff}`);
  return res;
}

const before = await stat();
if (!before || before.state !== 101) {
  console.log('machine is not READY (state=' + (before && before.state) + ') -- start it first');
  process.exit(1);
}
console.log(`comparing, ${MIN} min per block after a 60s settle\n`);
const out = [];
try {
  out.push(await block('探測（自動）', { proc_mode: 'auto', proc_discount_pct: 0, proc_sep_us: 0 }));
  out.push(await block('折扣 70%', { proc_mode: 'auto', proc_discount_pct: 70 }));
  out.push(await block('折扣 55%', { proc_mode: 'auto', proc_discount_pct: 55 }));
} finally {
  fs.writeFileSync(OUT, JSON.stringify(out, null, 1));
  await perif({ type: 'set_setup', gate: { proc_discount_pct: 0 } }, '"ack"');
  console.log('\n   restored: discount off (probe)');
}
