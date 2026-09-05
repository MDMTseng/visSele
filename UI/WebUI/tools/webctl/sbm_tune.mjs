// Usage: PROFILE=normal|aggressive node sbm_tune.mjs [name ...]
//   -- core with INSP_ALLOW_MULTI_CLIENT=1 on $CORE_PORT (default 4093), cwd InspectionCore/Core0_1.
// Per recipe (data/<name>_sbm.hydef + data/<name>.png): predict the coarse-locate knobs from the refine's capture
// budget, verify with worst-case perturbations, report time saved. Two profiles, one mechanism:
//   normal      angle step + match scale; largest passing step then ONE GRID STEP BACK; pose no worse than the base
//               by 0.05 deg / 0.1 px; similarity within 0.05; adopt only when >= 15% faster.
//   aggressive  takes the largest passing step as is; looser pose margin (0.1 deg / 0.3 px), similarity within 0.10;
//               adopts any gain >= 3%; ALSO sweeps (a) the refine capture knobs shape_roi_prescale / shape_roi_search,
//               which let big parts run a coarser step (SBM_TUNING doc, section 4), and (b) shape_num_features /
//               shape_strong_thres -- via an SF regenerate per candidate, because a cached def ignores them otherwise.
// Both profiles keep the hard gates: exactly one object found, judges not fewer than the base, on every worst case.
//
// The refine matches each ROI point along its edge normal within search_half = 15 px (roi_refine.cpp, hard-coded),
// 3 iterations, outliers > 2x median dropped. A coarse pose error moves a ROI point by
//     angle_err * lever + trans_err,     lever = |x*ny - y*nx|  (point relative to the template centre, edge normal n)
// and only the normal component is seen. Worst case per knob: angle_err = step/2, trans_err = 0.71/scale px (half a
// coarse pixel, diagonal). The budget C = 12 px keeps 3 px of the 15 in hand. Because of the outlier rejection the
// median lever, not the maximum, predicts the measured limit (mig_CON 4.3 vs 3-4 measured, MODEL3131 1.6 vs 2-3,
// 8G 6.3 vs 8). The prediction only picks the candidates; every choice is verified by inspection.
//
// shape_num_features / shape_strong_thres act at feature GENERATION: a def that carries a self-contained shape_cache
// loads its features from the cache and the knobs do nothing (the core now reports locate.code=cache_stale when they
// disagree). The aggressive profile therefore sends SF {regenerate:true} per candidate and patches the returned
// shape_cache into the def before verifying it. The normal profile leaves them alone.
//
// Pass/fail is RELATIVE to the base def's own worst-case error, not an absolute tolerance: the perturbation itself
// has sub-pixel noise (base defs read a median 0.33 px shift error, 10% over 1 px), so "no worse than the base by
// more than 0.05 deg / 0.1 px" is the question a tuned setting can be held to.
import fs from 'node:fs'; import path from 'node:path'; import WebSocket from 'ws';
const PORT = process.env.CORE_PORT || '4093';
const D = '../../../../InspectionCore/Core0_1/data/'; const HDR = 9, enc = new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws = new WebSocket('ws://127.0.0.1:' + PORT); ws.binaryType = 'arraybuffer'; let pg = 11000; const W = {};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='SF'){delete W[id];try{w.res(JSON.parse(txt));}catch(e){w.res(null);}return;}if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img,perturb)=>new Promise(res=>{const id=pg++;W[id]={res};const body={definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(perturb)body.img_property.perturb=perturb;ws.send(frame('II',0,id,body));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
// SF regenerate: the core extracts from the reference image and returns the shape_cache the def should carry.
const sf=(def)=>new Promise(res=>{const id=pg++;W[id]={res};ws.send(frame('SF',0,id,{definfo:def,regenerate:true}));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},60000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));

const PROFILES = {
  normal:     { backoff: 1, rotMargin: 0.05, shiftMargin: 0.1, simDrop: 0.05, adoptGain: 0.15, scales: [0.5, 0.4, 0.3, 0.25],      roiKnobs: false, featKnobs: false },
  aggressive: { backoff: 0, rotMargin: 0.10, shiftMargin: 0.3, simDrop: 0.10, adoptGain: 0.03, scales: [0.5, 0.4, 0.3, 0.25, 0.2], roiKnobs: true,  featKnobs: true  },
};
const PROFILE = process.env.PROFILE || 'normal'; const P = PROFILES[PROFILE]; if (!P) { console.error('PROFILE must be normal|aggressive'); process.exit(2); }
const C_BUDGET = 12;                       // px of the refine's 15 px search half-range we allow the coarse stage to use
const ROT_TOL = 0.1, SHIFT_TOL = 0.2;      // deg / px: "the refine recovered the pose"
const SIM_DROP = P.simDrop;                // similarity may not fall more than this below the base under the tuned knobs
const DATA_ABS = path.resolve(D) + path.sep;
const setk=(d,k,v)=>{ d[k]=v; d.featureSet[0][k]=v; };
const getk=(d,k,dflt)=> d[k] ?? d.featureSet[0][k] ?? dflt;
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return (g&&g.reports)||[];};
const okc=(o)=>(o.judgeReports||[]).filter(j=>j.status===0).length;
const clone=(d)=>JSON.parse(JSON.stringify(d));

// One perturbed inspection: pose error against the SAME def's unperturbed pose (own), judges / similarity against the
// original def (base). The two differ: a refine knob can move the converged pose by a few tenths of a degree (ok42 with
// the pre-pass: 0.33 deg, the default refine had not converged), which is not a repeatability error -- but a judge that
// flips because of it is still a change the operator must see, so that gate stays on the original.
async function probe(def, img, base, own, mmpp, pert, ref) {
  const rp = await ii(def, img, pert); const os = objs(rp);
  if (!os.length) return { ok:false, why:'not found' };
  const o = os[0];
  const dr = (o.rotate - own.rotate) * 180 / Math.PI; const rotErr = Math.abs(Math.abs(dr) - Math.abs(pert.rot_deg || 0));
  const dx = (o.cx - own.cx) / mmpp, dy = (o.cy - own.cy) / mmpp; const sx = pert.shift_x || 0, sy = pert.shift_y || 0;
  const shiftErr = Math.min(...[[dx-sx,dy-sy],[dx+sx,dy+sy],[dx-sx,dy+sy],[dx+sx,dy-sy]].map(([a,b])=>Math.hypot(a,b)));
  // A rotation is applied about the IMAGE centre, so it moves the object centre too: check the pose component the
  // perturbation actually exercised.
  const rotLim = ref ? Math.max(ROT_TOL, ref.rot + P.rotMargin) : Infinity, shLim = ref ? Math.max(SHIFT_TOL, ref.shift + P.shiftMargin) : Infinity;
  const poseOk = pert.rot_deg ? rotErr <= rotLim : shiftErr <= shLim;
  const ok = poseOk && okc(o) >= okc(base) && os.length === 1 && o.similarity >= base.similarity - SIM_DROP;
  return { ok, why: ok ? '' : `rot ${rotErr.toFixed(2)} shift ${shiftErr.toFixed(2)} judges ${okc(o)}/${okc(base)} sim ${o.similarity.toFixed(3)}`, ms: rp.insp_wall_ms, sim: o.similarity, rotErr, shiftErr, judges: okc(o) };
}
// Worst-case perturbation set for a given angle step and scale: between templates, between coarse pixels, and both.
// Rotation and shift are NOT combined: the perturbation rotates about the image centre, so a combined case moves the
// object centre by the rotation too and the shift check would need the sign conventions the sweep deliberately avoids.
const worstSet = (S, scale) => { const h = 0.5 / scale; return [
  { rot_deg: S/2, seed: 7 }, { rot_deg: -S/2, seed: 7 }, { rot_deg: 1.5*S, seed: 7 },
  { shift_x: h, shift_y: 0, seed: 7 }, { shift_x: 0, shift_y: h, seed: 7 }, { shift_x: h, shift_y: h, seed: 7 } ]; };
async function passes(def, img, base, mmpp, S, scale, ref) {
  const own = objs(await ii(def, img))[0]; if (!own) return { ok:false, why:'not found unperturbed' };
  if (own.similarity < base.similarity - SIM_DROP || okc(own) < okc(base)) return { ok:false, why:`unperturbed: judges ${okc(own)}/${okc(base)} sim ${own.similarity.toFixed(3)}` };
  let ms = [], rot = 0, shift = 0, judges = Infinity;
  for (const p of worstSet(S, scale)) { const r = await probe(def, img, base, own, mmpp, p, ref); if (process.env.DEBUG) console.log('   probe', JSON.stringify(p), r.ok ? 'ok' : r.why);
    if (!r.ok) return { ok:false, why:r.why }; ms.push(r.ms); if (p.rot_deg) rot = Math.max(rot, r.rotErr); else shift = Math.max(shift, r.shiftErr); judges = Math.min(judges, r.judges); }
  ms.sort((a,b)=>a-b); return { ok:true, ms: ms[Math.floor(ms.length/2)], rot, shift, judges };
}

const names = process.argv.slice(2).length ? process.argv.slice(2)
  : fs.readFileSync('_ok_names.txt','utf8').split(String.fromCharCode(10)).map(s=>s.trim()).filter(Boolean);
const out = {}; let sumBase = 0, sumTuned = 0, nTuned = 0, nSkip = 0;
for (const name of names) {
  let def0 = JSON.parse(fs.readFileSync(D + name + '_sbm.hydef', 'utf8')); const fs0 = def0.featureSet[0]; const mmpp = fs0.mmpp; const img = 'data/' + name + '.png';
  const sbm = (fs0.inherentfeatures || []).find(e => e && e.name === '@__SBM_INFO__'); const pts = sbm && sbm.shape_cache && sbm.shape_cache.roi && sbm.shape_cache.roi.pts;
  const base = objs(await ii(def0, img))[0];
  if (!base || !pts || !pts.length) { console.log(name.padEnd(40) + ' skip: ' + (!base ? 'no object on its own picture' : 'no ROI points (coarse-only def)')); nSkip++; continue; }
  const S0 = getk(def0, 'shape_angle_step_deg', 1), sc0 = getk(def0, 'shape_match_scale', 0.5), nf0 = getk(def0, 'shape_num_features', 128), st0 = getk(def0, 'shape_strong_thres', 80);
  // base time on the same worst-case set (fair comparison: the same perturbed frames)
  let baseRun = await passes(def0, img, base, mmpp, S0, sc0, null); let rescued = ''; const rescueWhy = [];
  // (aggressive) A base that fails its OWN worst case is often a big part whose refine does not converge in 15 px
  // (ok42: 8 px pose jump under a half-pixel shift). The capture knobs are the fix for exactly that, so try them as
  // the new base before giving up; the tuned result is then measured against the rescued base, and says so.
  if (!baseRun.ok && P.roiKnobs) {
    for (const [k, v] of [['shape_roi_prescale', 0.5], ['shape_roi_search', 30]]) {
      const d = clone(def0); setk(d, k, v); const r = await passes(d, img, base, mmpp, S0, sc0, null);
      if (r.ok) { def0 = d; baseRun = r; rescued = k + '=' + v; break; }
      rescueWhy.push(k + '=' + v + ': ' + r.why);
    }
  }
  if (!baseRun.ok) { console.log(name.padEnd(40) + ' skip: base loses the part or judges under its own worst case: ' + baseRun.why + (rescueWhy.length ? '  [rescue tried: ' + rescueWhy.join(' | ') + ']' : '')); out[name] = { profile: PROFILE, skip: baseRun.why, rescueWhy }; nSkip++; continue; }
  const baseMs = baseRun.ms; const ref = { rot: baseRun.rot, shift: baseRun.shift };
  const unstable = baseRun.shift > 1.0 || baseRun.rot > 0.5;   // the CURRENT setting already wobbles: worth a look regardless
  // 1. predicted limits from the ROI geometry
  const lever = pts.map(p => Math.abs(p[0]*p[3] - p[1]*p[2])).sort((a,b)=>a-b); const lmed = lever[Math.floor(lever.length/2)] || 1;
  const Spred = 2 * ((C_BUDGET - 0.71/sc0) / lmed) * 180 / Math.PI;
  // 2. angle step: candidates around the prediction, verified, largest passing then (normal) one grid step back
  const grid = [1,2,3,4,5,6,8,10,12];
  const back = (Spass) => Spass > S0 ? grid[Math.max(0, grid.indexOf(Spass) - P.backoff)] : S0;
  const stepSearch = async (base_def, from, sc) => {   // largest passing grid step >= from, at scale sc
    let Sp = from; const cands = grid.filter(s => s > from && s <= Math.max(from, Math.ceil(Spred * (P.roiKnobs ? 2 : 1)) + 1));
    for (const S of cands) { const d = clone(base_def); setk(d, 'shape_angle_step_deg', S); const r = await passes(d, img, base, mmpp, S, sc, ref); if (r.ok) Sp = S; else break; }
    return Sp; };
  const Spass = await stepSearch(def0, S0, sc0);
  const Srec = back(Spass);
  // 3. scale: try smaller, verified at the recommended step, then one step back
  const scales = P.scales.filter(s => s < sc0);
  let scPass = sc0;
  for (const sc of scales) { const d = clone(def0); setk(d, 'shape_angle_step_deg', Srec); setk(d, 'shape_match_scale', sc); const r = await passes(d, img, base, mmpp, Srec, sc, ref); if (r.ok) scPass = sc; else break; }
  const scRec = scPass < sc0 ? P.scales[Math.max(0, P.scales.indexOf(scPass) - P.backoff)] : sc0;
  const scRec2 = scRec > sc0 ? sc0 : scRec;
  let tuned = clone(def0); setk(tuned, 'shape_angle_step_deg', Srec); setk(tuned, 'shape_match_scale', scRec2);
  let tunedRun = await passes(tuned, img, base, mmpp, Srec, scRec2, ref);
  let tunedMs = tunedRun.ok ? tunedRun.ms : null; let SrecF = Srec; const extra = rescued ? ['rescued:' + rescued] : [];
  // 4. (aggressive) refine capture knobs: does a wider capture let this recipe run a coarser step, and is it faster?
  //    Each knob is verified on the same worst-case set, from Spass upward, and kept only if the frame gets faster.
  if (P.roiKnobs && tunedMs != null) {
    for (const [k, v] of [['shape_roi_prescale', 0.5], ['shape_roi_search', 30]]) {
      const d0 = clone(tuned); setk(d0, k, v);
      const r0 = await passes(d0, img, base, mmpp, SrecF, scRec2, ref); if (!r0.ok) { extra.push(k + '=' + v + ':breaks'); continue; }
      const Sp = await stepSearch(d0, SrecF, scRec2); const Sr = back(Sp); if (Sr <= SrecF) { extra.push(k + '=' + v + ':nostep'); continue; }
      const d1 = clone(d0); setk(d1, 'shape_angle_step_deg', Sr); const r1 = await passes(d1, img, base, mmpp, Sr, scRec2, ref);
      if (r1.ok && r1.ms < tunedMs) { tuned = d1; tunedRun = r1; tunedMs = r1.ms; SrecF = Sr; extra.push(k + '=' + v + '@' + Sr); }
      else extra.push(k + '=' + v + ':slower');
    }
  }
  // 5. (aggressive) feature count / strong threshold: regenerate per candidate, verify, keep if faster.
  let nfRec = nf0, stRec = st0;
  if (P.featKnobs && tunedMs != null) {
    const sbmIdx = fs0.inherentfeatures.findIndex(e => e && e.name === '@__SBM_INFO__');
    const regen = async (d) => { const q = clone(d); q.featureSet[0]._ref_image_path = DATA_ABS + name + '.png'; const r = await sf(q); const cache = r && r.shape_cache; if (!cache || !(r.features||[]).length) return null; const t = clone(d); t.featureSet[0].inherentfeatures[sbmIdx].shape_cache = cache; return t; };
    let nfFailed = false;
    for (const [k, v] of [['shape_num_features', 64], ['shape_num_features', 32], ['shape_strong_thres', 120]]) {
      if (k === 'shape_num_features' && (v >= nfRec || nfFailed)) continue;
      const d0 = clone(tuned); setk(d0, k, v); const d1 = await regen(d0); if (!d1) { extra.push(k + '=' + v + ':noregen'); continue; }
      const r1 = await passes(d1, img, base, mmpp, SrecF, scRec2, ref);
      if (r1.ok && r1.ms < tunedMs) { tuned = d1; tunedRun = r1; tunedMs = r1.ms; if (k === 'shape_num_features') nfRec = v; else stRec = v; extra.push(k + '=' + v); }
      else { extra.push(k + '=' + v + (r1.ok ? ':slower' : ':fails')); if (k === 'shape_num_features') nfFailed = true; }
    }
  }
  const gain = tunedMs != null && baseMs ? (1 - tunedMs / baseMs) : 0;
  // Adopt only when it buys something: a tuned set that saves under the profile's gain is inside the timing noise and
  // not worth moving knobs the operator has to understand. The verified limits are still reported.
  const adopt = tunedMs != null && gain >= P.adoptGain;
  out[name] = { profile: PROFILE, adopt, unstable, baseRot: +baseRun.rot.toFixed(3), baseShift: +baseRun.shift.toFixed(2), S0, Spred: +Spred.toFixed(1), Spass, Srec: SrecF, sc0, scPass, scRec: scRec2, nf0, nfRec, st0, stRec, extra, baseMs, tunedMs, gain: +gain.toFixed(3), leverMed: Math.round(lmed) };
  if (tunedMs != null) { sumBase += baseMs; sumTuned += adopt ? tunedMs : baseMs; if (adopt) nTuned++; }
  console.log(name.padEnd(40) + ` lever ${String(Math.round(lmed)).padStart(4)}  S pred ${Spred.toFixed(1).padStart(4)} pass ${String(Spass).padStart(2)} -> ${String(SrecF).padStart(2)}  scale ${sc0} pass ${scPass} -> ${scRec2}  ms ${baseMs.toFixed(1)} -> ${tunedMs != null ? tunedMs.toFixed(1) : 'FAIL'}  (${(gain*100).toFixed(0)}%) ${adopt ? 'ADOPT' : 'keep'}${extra.length ? '  [' + extra.join(' ') + ']' : ''}${unstable ? '  UNSTABLE-BASE rot ' + baseRun.rot.toFixed(2) + ' shift ' + baseRun.shift.toFixed(2) : ''}`);
  if (process.env.WRITE && adopt) fs.writeFileSync(D + name + '_tuned_' + PROFILE + '.hydef', JSON.stringify(tuned));
}
console.log(`\n${nTuned} recipes tuned, ${nSkip} skipped.  insp ms sum ${sumBase.toFixed(0)} -> ${sumTuned.toFixed(0)} (${(100*(1-sumTuned/Math.max(1,sumBase))).toFixed(0)}% less), every choice verified on worst-case perturbations.`);
fs.writeFileSync(process.env.OUT || ('_sbm_tune_' + PROFILE + '.json'), JSON.stringify(out, null, 1));
ws.close(); process.exit(0);
