// Stability watch: sample the machine every 20s and print only what moves.
import net from 'node:net';
const MIN = Number(process.argv[2] || 20);
const s = net.connect(4099, '127.0.0.1');
let buf = '', id = 5000;
s.on('data', d => buf += d.toString());
const ask = () => new Promise(r => {
  const my = ++id; buf = '';
  s.write(JSON.stringify({ type: 'get_running_stat', id: my }) + '\n');
  setTimeout(() => {
    const l = buf.split('\n').find(x => x.includes('"id":' + my));
    try { r(l ? JSON.parse(l.slice(l.indexOf('{'))) : null); } catch { r(null); }
  }, 2500);
});
await new Promise(r => s.once('connect', r));
console.log('t_min,state,err,plate,edges,accept,NA,SEL1,SEL3,cal_fails,cal_lost,consec_unans,free_heap');
const t0 = Date.now();
for (let i = 0; i <= MIN * 3; i++) {
  const o = await ask();
  if (o) {
    console.log([((Date.now() - t0) / 60000).toFixed(1), o.state, JSON.stringify(o.error_hist),
      Math.round(o.plate_freq_meas), o.gate.edges, o.yield.gate.out, o.count.NA, o.count.SEL1, o.count.SEL3,
      o.cam_sync.cal_fails, o.cam_sync.cal_pulse_lost, o.health.consec_unanswered, o.health.free_heap].join(','));
  } else console.log(((Date.now() - t0) / 60000).toFixed(1) + ',(no reply)');
  await new Promise(r => setTimeout(r, 17500));
}
s.end();
