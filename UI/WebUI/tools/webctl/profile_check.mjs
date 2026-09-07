// Where does inspection time actually go?
//
//   node profile_check.mjs [recipe]
//
// Runs 快速驗證 against the live camera -- no plate, no feeder -- and then reads
// the per-stage timers the core already keeps. The question this answers is the
// one that decides whether any optimisation is worth doing: the machine's
// service time is what the host throttle is sized from, and until the stages are
// split out, "the inspection takes 30-66ms" is not actionable.
//
// wall AND cpu per stage, because they answer different questions: wall time is
// what the deadline sees, cpu time is what optimising can remove. A stage where
// they diverge is waiting for something, and no amount of SIMD will help it.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';

const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;
const MODEL = process.argv[2] || 'data/test1';

const gs = (items) => ev(`(function(){
  window.__GSR__ = null;
  var st = window.__GP_STORE__; var C = st.getState().ConnInfo.CORE_ID;
  st.dispatch({ type:'MW_API_CALL', id:C, method:'send', param:{ tl:'GS', prop:0,
    data:{ items: ${JSON.stringify(items)} },
    promiseCBs:{
      resolve:function(p){ var g=(p||[]).find(function(x){return x&&x.type==='GS'});
        window.__GSR__ = g ? JSON.stringify(g.data) : 'no GS packet'; },
      reject:function(e){ window.__GSR__='REJECT '+e; } } } });
  return 'sent';})()`);

async function readGS(items, tries = 40) {
  await gs(items);
  for (let i = 0; i < tries; i++) {
    const r = await ev(`window.__GSR__`);
    if (r) { try { return JSON.parse(r); } catch (e) { return null; } }
    await sleep(250);
  }
  return null;
}

await freshPage(ctl, 'http://127.0.0.1:8081/');
await toMain(ctl); await dismissCamModal(ctl);
const name = await loadRecipe(ctl, MODEL);
console.log('recipe:', name);

await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
for (let i = 0; i < 60; i++) {
  const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
  if (String(s).indexOf('DEFCONF') >= 0) break;
  await sleep(400);
}
console.log('editor:', await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`));

const click = (id) => ev(`(function(){var e=document.querySelector('[data-testid="${id}"]');
  if(!e) return false; e.click(); return true;})()`);
console.log('快速驗證:', await click('quick-verify'));
await sleep(1500);
console.log('CI 模式:', await click('quick-verify-ci'));

// Let it run long enough for the histograms to have a population worth reading.
console.log('collecting for 45s ...');
for (let t = 0; t < 45; t += 5) {
  await sleep(5000);
  const hz = await ev(`(function(){try{return window.__DIAG__().msgHz.toFixed(2);}catch(e){return '?';}})()`);
  process.stdout.write(`  ${t + 5}s  msgHz ${hz}\n`);
}

const d = await readGS(['perif_pairing']);
const lat = d && d.perif_pairing && d.perif_pairing.lat_hist;
if (!lat) { console.log('no lat_hist in perif_pairing:', JSON.stringify(d).slice(0, 300)); process.exit(1); }

console.log('\nper-stage (featureBundle order):');
console.log('   stage       n    avg ms    max ms   cpu avg  cpu/wall');
let total = 0;
for (let i = 0; i < 8; i++) {
  const s = lat['stage' + i];
  if (!s || !s.n) continue;
  const cpu = s.cpu_avg_ms !== undefined ? s.cpu_avg_ms : NaN;
  total += s.avg_ms || 0;
  console.log(`${String(i).padStart(8)}${String(s.n).padStart(8)}` +
    `${(s.avg_ms || 0).toFixed(2).padStart(10)}${(s.max_ms || 0).toFixed(2).padStart(10)}` +
    `${(isFinite(cpu) ? cpu.toFixed(2) : '-').padStart(10)}` +
    `${(isFinite(cpu) && s.avg_ms ? (cpu / s.avg_ms).toFixed(2) : '-').padStart(10)}`);
}
console.log(`${'sum'.padStart(8)}${''.padStart(8)}${total.toFixed(2).padStart(10)}`);
const phases = Object.keys(lat).filter(k => k.startsWith('ph_'));
if (phases.length) {
  console.log('\nphases inside the stage:');
  console.log('       phase       n    avg ms    max ms   % of match');
  const mm = lat.match && lat.match.avg_ms ? lat.match.avg_ms : 0;
  for (const k of phases) {
    const p = lat[k];
    console.log(k.slice(3).padStart(12) + String(p.n).padStart(8) +
      (p.avg_ms || 0).toFixed(2).padStart(10) + (p.max_ms || 0).toFixed(2).padStart(10) +
      (mm ? (100 * p.avg_ms / mm).toFixed(1) + '%' : '-').padStart(13));
  }
}
const counts = Object.keys(lat).filter(k => k.startsWith('cnt_'));
if (counts.length) {
  // Counters are keyed "<phase>/<name>", so a ratio has a numerator and a
  // denominator that describe the same work. Dividing one phase's time by every
  // phase's scans is how the first version of this flattered the caliper.
  console.log('\nwork per frame, and the unit cost that follows from it:');
  for (const k of counts) {
    const c = lat[k];
    console.log('  ' + k.slice(4).padEnd(20) + (c.avg_ms || 0).toFixed(1).padStart(10) +
      '  per frame   (max ' + (c.max_ms || 0).toFixed(0) + ')');
  }
  // Per-primitive-kind unit cost. An arc gathers a grid per caliper; a line
  // rectifies one band for all of them. Averaging the two hides the difference
  // that decides what a bigger part costs.
  const g = (k) => (lat['cnt_measure/' + k] ? lat['cnt_measure/' + k].avg_ms : 0);
  const meas = lat.ph_measure ? lat.ph_measure.avg_ms : 0;
  const tot = g('cal_samp') + g('spcv_samp') + g('band_samp');
  if (meas && tot) {
    console.log('  measure ' + meas.toFixed(2) + ' ms over ' + tot.toFixed(0) +
      ' samples = ' + (meas * 1e6 / tot).toFixed(0) + ' ns/sample (all paths pooled)');
    if (g('band_prim'))
      console.log('    line band : ' + g('band_prim').toFixed(0) + ' primitives, ' +
        g('band_cal').toFixed(0) + ' calipers, ' + g('band_samp').toFixed(0) + ' samples = ' +
        (g('band_samp') / g('band_cal')).toFixed(0) + ' samples/caliper');
    if (g('cal_scan'))
      console.log('    caliper   : ' + g('cal_scan').toFixed(0) + ' calipers, ' +
        g('cal_samp').toFixed(0) + ' samples = ' +
        (g('cal_samp') / g('cal_scan')).toFixed(0) + ' samples/caliper');
    if (g('spcv_scan'))
      console.log('    searchpt  : ' + g('spcv_scan').toFixed(0) + ' scans, ' +
        g('spcv_samp').toFixed(0) + ' samples = ' +
        (g('spcv_samp') / g('spcv_scan')).toFixed(0) + ' samples/scan');
  }
}
console.log('\nwhole-match and frame timers:');
for (const k of Object.keys(lat)) {
  if (k.startsWith('stage') || k.startsWith('ph_') || k.startsWith('cnt_') || k === 'edges_ms') continue;
  console.log(`  ${k}: ${JSON.stringify(lat[k]).slice(0, 200)}`);
}
