// Two minutes running, then two minutes with the gate shut, watching the
// residual across the idle recalibration.
//
//   node run_recal_watch.mjs
//
// The point of the second half: recalService now fires ONE stealth object
// instead of transitioning to RECAL, and `valid` is meant to stay true the
// whole way through. `last_resid_us` is the evidence -- it is the difference
// between the newest sample and the offset in force, so it shows the drift
// accumulating while the line is idle and shows it collapsing the moment the
// stealth object lands. If the model were being dropped and rebuilt instead,
// the residual would not fall, it would VANISH (valid=0, no offset to be a
// residual against) and then reappear from zero.
//
// THIS MOVES THE MACHINE. The plate turns and READY turns the feeder on.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { sleep } from './_rf_lib.mjs';
import net from 'node:net';

const ctl = makeCtl('http://127.0.0.1:8765');
const MODEL = process.argv[2] || 'data/testNew2';

function perif(cmd, want, waitMs = 3000) {
  return new Promise((res) => {
    const sock = net.connect(4099, '127.0.0.1');
    let buf = '';
    const done = (v) => { try { sock.end(); } catch (e) {} res(v); };
    sock.on('connect', () => sock.write(JSON.stringify(cmd) + String.fromCharCode(10)));
    sock.on('data', (d) => {
      buf += d.toString();
      for (const line of buf.split(String.fromCharCode(10))) {
        const t = line.trim();
        if (!t.startsWith('{') || (want && t.indexOf(want) < 0)) continue;
        try { return done(JSON.parse(t)); } catch (e) { /* partial */ }
      }
    });
    sock.on('error', () => done(null));
    setTimeout(() => done(null), waitMs);
  });
}
const stat = () => perif({ type: 'get_running_stat' }, '"gate"');

function row(t, s) {
  const g = (s && s.gate) || {}, c = (s && s.count) || {},
        h = (s && s.health) || {}, y = (s && s.cam_sync) || {},
        l = (s && s.report_latency) || {};
  const p = (v, w) => String(v === undefined ? '-' : v).padStart(w);
  return `${p(t, 3)}s  state ${p(s && s.state, 3)}  進料 ${p(g.accept, 6)}  `
    + `判定 ${p((c.SEL1 | 0) + (c.SEL3 | 0), 6)}  NA ${p(c.NA, 5)}  `
    + `殘差 ${p(y.resid_us, 6)}us  最大 ${p(y.resid_max_us, 6)}  `
    + `齡 ${p(y.est_age_s, 3)}s  漂移 ${p(y.drift_us_per_s, 5)}us/s  `
    + `valid ${p(y.valid === undefined ? '-' : (y.valid ? 1 : 0), 1)}  `
    + `拒絕 ${p(y.rejected, 2)}  重建 ${p(y.rebuilds, 3)}  建立 ${p(y.established, 3)}  `
    + `recal ${p(h.recal_stealth, 3)}/${p(h.recal_stealth_ok, 3)}/fb ${p(h.recal_fallback, 2)}  `
    + `回報 ${p(l.cam_avg_us ? Math.round(l.cam_avg_us / 1000) : 0, 4)}ms`;
}

let running = false;
async function stopAll(why) {
  console.log(`\n   stopping (${why})`);
  await perif({ type: 'set_gate_disable', on: false }, '"type"', 2000);
  await perif({ type: 'exit_insp_mode' }, '"ack"', 2000);
  await perif({ type: 'set_setup', plate: { freq: 0 } }, '"ack"', 2000);
  // The plate coasts down under accel; do not walk away before it has.
  for (let i = 0; i < 30; i++) {
    const s = await stat();
    if (s && (s.plate_freq_meas === 0 || s.plate_freq_meas === undefined)
          && s.plate_freq === 0) break;
    await sleep(1000);
  }
  const s = await stat();
  console.log('   final: ' + row('end', s));
}
process.on('SIGINT', async () => { await stopAll('interrupted'); process.exit(1); });

try {
  const s0 = await stat();
  if (!s0) throw new Error('no board on 4099 -- is the core up with INSP_PERIF_CONSOLE?');
  console.log('   start: ' + row(0, s0));

  await freshPage(ctl, 'http://127.0.0.1:8081/');
  await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  // 全檢, not 測試: this is the real run. The 檢測方式 group offers
  // 抽檢 / 全檢 / 測試 -- there is no tag called 檢測.
  const entered = await enterInspection(ctl, { mode: '全檢', log: (m) => console.log('   ' + m) });
  console.log('   app entered:', entered);

  // The board, not the screen -- they are separate state machines and only the
  // board's decides whether objects are registered.
  // The setpoint is 0 on this bench between runs, and inspection mode needs a
  // turning plate to have a stage clock at all.
  await perif({ type: 'set_setup', plate: { freq: 10000 } }, '"ack"', 3000);
  const em = await perif({ type: 'enter_insp_mode' }, '"type"', 4000);
  console.log('   board enter_insp_mode:', JSON.stringify(em));
  running = true;

  console.log('\n=== 第一段:實跑 2 分鐘 ===');
  for (let t = 10; t <= 120; t += 10) {
    await sleep(10000);
    console.log('   ' + row(t, await stat()));
  }

  console.log('\n=== 第二段:關閘門,等閒置 recal,看殘差 ===');
  const gd = await perif({ type: 'set_gate_disable', on: true }, '"type"', 3000);
  console.log('   gate disabled:', JSON.stringify(gd));
  // Every 5s: the recal fires at recal_idle_ms (10s) of idleness, and the
  // stealth object needs a lap to land, so this samples fast enough to see the
  // residual grow and collapse rather than only its endpoints.
  for (let t = 5; t <= 120; t += 5) {
    await sleep(5000);
    console.log('   ' + row(120 + t, await stat()));
  }
} catch (e) {
  console.log('\n   ERROR: ' + e.message);
} finally {
  await stopAll('done');
}
