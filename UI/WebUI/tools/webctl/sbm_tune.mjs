// Usage: node sbm_tune.mjs [name ...]   -- core with INSP_ALLOW_MULTI_CLIENT=1 on $CORE_PORT (default 4093).
// Per recipe (data/<name>_sbm.hydef + data/<name>.png): predict the coarse-locate knobs from the refine's capture
// budget, verify with worst-case perturbations, probe feature count / edge threshold, report time saved.
//
// The refine matches each ROI point along its edge normal within search_half = 15 px (roi_refine.cpp, hard-coded),
// 3 iterations, outliers > 2x median dropped. A coarse pose error moves a ROI point by
//     angle_err * lever + trans_err,     lever = |x*ny - y*nx|  (point relative to the template centre, edge normal n)
// and only the normal component is seen. Worst case per knob: angle_err = step/2, trans_err = 0.71/scale px (half a
// coarse pixel, diagonal). The budget C = 12 px keeps 3 px of the 15 in hand. Because of the outlier rejection the
// median lever, not the maximum, predicts the measured limit (mig_CON 4.3 vs 3-4 measured, MODEL3131 1.6 vs 2-3,
// 8G 6.3 vs 8). The prediction only picks the candidates; every choice is verified by inspection.
//
// NOT swept here: shape_num_features / shape_weak_thres / shape_strong_thres. A def that carries a self-contained
// shape_cache loads its features from the cache; the fingerprint is stored but not compared, so changing those knobs
// at inspection time changes nothing (first run of this tool "accepted" 32 features on all 148 recipes for that
// reason). They act at feature generation (SF) and need a regenerate per candidate -- a separate, slower sweep.
//
// Pass/fail is RELATIVE to the base def's own worst-case error, not an absolute tolerance: the perturbation itself
// has sub-pixel noise (base defs read a median 0.33 px shift error, 10% over 1 px), so "no worse than the base by
// more than 0.05 deg / 0.1 px" is the question a tuned setting can be held to.
import fs from 'node:fs'; import WebSocket from 'ws';
const PORT = process.env.CORE_PORT || '4093';
const D = '../../../../InspectionCore/Core0_1/data/'; const HDR = 9, enc = new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws = new WebSocket('ws://127.0.0.1:' + PORT); ws.binaryType = 'arraybuffer'; let pg = 11000; const W = {};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img,perturb)=>new Promise(res=>{const id=pg++;W[id]={res};const body={definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(perturb)body.img_property.perturb=perturb;ws.send(frame('II',0,id,body));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));

const C_BUDGET = 12;                       // px of the refine's 15 px search half-range we allow the coarse stage to use
const ROT_TOL = 0.1, SHIFT_TOL = 0.2;      // deg / px: "the refine recovered the pose"
const SIM_DROP = 0.05;                     // similarity may not fall more than this below the base under the tuned knobs
const setk=(d,k,v)=>{ d[k]=v; d.featureSet[0][k]=v; };
const getk=(d,k,dflt)=> d[k] ?? d.featureSet[0][k] ?? dflt;
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return (g&&g.reports)||[];};
const okc=(o)=>(o.judgeReports||[]).filter(j=>j.status===0).length;
const clone=(d)=>JSON.parse(JSON.stringify(d));

// One perturbed inspection compared with the base object: found, pose error, judges, similarity, time.
async function probe(def, img, base, mmpp, pert, ref) {
  const rp = await ii(def, img, pert); const os = objs(rp);
  if (!os.length) return { ok:false, why:'not found' };
  const o = os[0];
  const dr = (o.rotate - base.rotate) * 180 / Math.PI; const rotErr = Math.abs(Math.abs(dr) - Math.abs(pert.rot_deg || 0));
  const dx = (o.cx - base.cx) / mmpp, dy = (o.cy - base.cy) / mmpp; const sx = pert.shift_x || 0, sy = pert.shift_y || 0;
  const shiftErr = Math.min(...[[dx-sx,dy-sy],[dx+sx,dy+sy],[dx-sx,dy+sy],[dx+sx,dy-sy]].map(([a,b])=>Math.hypot(a,b)));
  // A rotation is applied about the IMAGE centre, so it moves the object centre too: check the pose component the
  // perturbation actually exercised.
  const rotLim = ref ? Math.max(ROT_TOL, ref.rot + 0.05) : Infinity, shLim = ref ? Math.max(SHIFT_TOL, ref.shift + 0.1) : Infinity;
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
  let ms = [], rot = 0, shift = 0, judges = Infinity;
  for (const p of worstSet(S, scale)) { const r = await probe(def, img, base, mmpp, p, ref); if (process.env.DEBUG) console.log('   probe', JSON.stringify(p), r.ok ? 'ok' : r.why);
    if (!r.ok) return { ok:false, why:r.why }; ms.push(r.ms); if (p.rot_deg) rot = Math.max(rot, r.rotErr); else shift = Math.max(shift, r.shiftErr); judges = Math.min(judges, r.judges); }
  ms.sort((a,b)=>a-b); return { ok:true, ms: ms[Math.floor(ms.length/2)], rot, shift, judges };
}

const names = process.argv.slice(2).length ? process.argv.slice(2)
  : fs.readFileSync('_ok_names.txt','utf8').split(String.fromCharCode(10)).map(s=>s.trim()).filter(Boolean);
const out = {}; let sumBase = 0, sumTuned = 0, nTuned = 0, nSkip = 0;
for (const name of names) {
  const def0 = JSON.parse(fs.readFileSync(D + name + '_sbm.hydef', 'utf8')); const fs0 = def0.featureSet[0]; const mmpp = fs0.mmpp; const img = 'data/' + name + '.png';
  const sbm = (fs0.inherentfeatures || []).find(e => e && e.name === '@__SBM_INFO__'); const pts = sbm && sbm.shape_cache && sbm.shape_cache.roi && sbm.shape_cache.roi.pts;
  const base = objs(await ii(def0, img))[0];
  if (!base || !pts || !pts.length) { console.log(name.padEnd(40) + ' skip: ' + (!base ? 'no object on its own picture' : 'no ROI points (coarse-only def)')); nSkip++; continue; }
  const S0 = getk(def0, 'shape_angle_step_deg', 1), sc0 = getk(def0, 'shape_match_scale', 0.5), nf0 = getk(def0, 'shape_num_features', 128), st0 = getk(def0, 'shape_strong_thres', 80);
  // base time on the same worst-case set (fair comparison: the same perturbed frames)
  const baseRun = await passes(def0, img, base, mmpp, S0, sc0, null);
  if (!baseRun.ok) { console.log(name.padEnd(40) + ' skip: base loses the part or judges under its own worst case: ' + baseRun.why); nSkip++; continue; }
  const baseMs = baseRun.ms; const ref = { rot: baseRun.rot, shift: baseRun.shift };
  const unstable = baseRun.shift > 1.0 || baseRun.rot > 0.5;   // the CURRENT setting already wobbles: worth a look regardless
  // 1. predicted limits from the ROI geometry
  const lever = pts.map(p => Math.abs(p[0]*p[3] - p[1]*p[2])).sort((a,b)=>a-b); const lmed = lever[Math.floor(lever.length/2)] || 1;
  const Spred = 2 * ((C_BUDGET - 0.71/sc0) / lmed) * 180 / Math.PI;
  // 2. angle step: candidates around the prediction, verified, largest passing then one grid step back as margin
  const grid = [1,2,3,4,5,6,8,10,12];
  let Spass = S0; const cands = grid.filter(s => s >= S0 && s <= Math.max(S0, Math.ceil(Spred) + 1));
  for (const S of cands) { const d = clone(def0); setk(d, 'shape_angle_step_deg', S); const r = await passes(d, img, base, mmpp, S, sc0, ref); if (r.ok) Spass = S; else break; }
  const Srec = Spass > S0 ? grid[Math.max(0, grid.indexOf(Spass) - 1)] : S0;
  // 3. scale: try smaller, verified at the recommended step, then one step back
  const scales = [0.5, 0.4, 0.3, 0.25, 0.2].filter(s => s < sc0);
  let scPass = sc0;
  for (const sc of scales) { const d = clone(def0); setk(d, 'shape_angle_step_deg', Srec); setk(d, 'shape_match_scale', sc); const r = await passes(d, img, base, mmpp, Srec, sc, ref); if (r.ok) scPass = sc; else break; }
  const scRec = scPass < sc0 ? [0.5,0.4,0.3,0.25,0.2][Math.max(0, [0.5,0.4,0.3,0.25,0.2].indexOf(scPass) - 1)] : sc0;
  const scRec2 = scRec > sc0 ? sc0 : scRec;
  const tuned = clone(def0); setk(tuned, 'shape_angle_step_deg', Srec); setk(tuned, 'shape_match_scale', scRec2);
  const nfRec = nf0, stRec = st0;
  const tunedRun = await passes(tuned, img, base, mmpp, Srec, scRec2, ref);
  const tunedMs = tunedRun.ok ? tunedRun.ms : null;
  const gain = tunedMs != null && baseMs ? (1 - tunedMs / baseMs) : 0;
  // Adopt only when it buys something: a tuned set that saves under 15% is inside the timing noise and not worth
  // moving four knobs the operator has to understand. The verified limits are still reported.
  const adopt = tunedMs != null && gain >= 0.15;
  out[name] = { adopt, unstable, baseRot: +baseRun.rot.toFixed(3), baseShift: +baseRun.shift.toFixed(2), S0, Spred: +Spred.toFixed(1), Spass, Srec, sc0, scPass, scRec: scRec2, nf0, nfRec, st0, stRec, baseMs, tunedMs, gain: +gain.toFixed(3), leverMed: Math.round(lmed) };
  if (tunedMs != null) { sumBase += baseMs; sumTuned += adopt ? tunedMs : baseMs; if (adopt) nTuned++; }
  console.log(name.padEnd(40) + ` lever ${String(Math.round(lmed)).padStart(4)}  S pred ${Spred.toFixed(1).padStart(4)} pass ${String(Spass).padStart(2)} -> ${String(Srec).padStart(2)}  scale ${sc0} pass ${scPass} -> ${scRec2}  ms ${baseMs.toFixed(1)} -> ${tunedMs != null ? tunedMs.toFixed(1) : 'FAIL'}  (${(gain*100).toFixed(0)}%) ${adopt ? 'ADOPT' : 'keep'}${unstable ? '  UNSTABLE-BASE rot ' + baseRun.rot.toFixed(2) + ' shift ' + baseRun.shift.toFixed(2) : ''}`);
  if (process.env.WRITE && adopt) fs.writeFileSync(D + name + '_tuned.hydef', JSON.stringify(tuned));
}
console.log(`\n${nTuned} recipes tuned, ${nSkip} skipped.  insp ms sum ${sumBase.toFixed(0)} -> ${sumTuned.toFixed(0)} (${(100*(1-sumTuned/Math.max(1,sumBase))).toFixed(0)}% less), every choice verified on worst-case perturbations.`);
fs.writeFileSync(process.env.OUT || '_sbm_tune.json', JSON.stringify(out, null, 1));
ws.close(); process.exit(0);
