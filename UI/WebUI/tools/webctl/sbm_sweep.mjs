// Usage: node sbm_sweep.mjs <config.json> [out.jsonl]
//   -- core with INSP_ALLOW_MULTI_CLIENT=1 on $CORE_PORT (default 4093), cwd InspectionCore/Core0_1.
//
// MEASURE THE RESPONSE SURFACE, DECIDE LATER. For each recipe, each image, each parameter set and each augmentation
// point, run one inspection and write one row per located object. No pass/fail lives here: the acceptance profile is
// applied by sbm_sweep_report.mjs on the saved rows, so a different tolerance is a re-read, not a re-run. This replaced
// sbm_tune.mjs's search ("does step 2 pass six worst cases?"), whose yes/no flipped between runs on one image under 20%
// timing noise -- see SBM_TUNING doc.
//
// Config:
//   { "recipes": [ { "name": "test1", "def": "test1.hydef", "images": ["test1.png", "test1_*.png"], "max_images": 3 } ],
//     "params": { "shape_angle_step_deg": [1,2,3,4,6], "shape_match_scale": [0.5,0.3,0.25,0.2],
//                 "roi": [ {}, {"shape_roi_search":30}, {"shape_roi_prescale":0.5} ],
//                 "shape_num_features": [128, 64] },            // != the def's -> SF regenerate (cached per recipe)
//     "aug": [ {"rot_deg":0.5}, {"shift_x":0.5,"shift_y":0.5}, {"gain":0.8}, {"noise":8}, ... ],   // one point each
//     "seeds": [7] }
// Files are relative to InspectionCore/Core0_1/data. The def's own values are always included in every param axis.
//
// Row (JSONL): { recipe, image, pi, params:{...}, aug:{...}, seed, ms, n_base, n_found, n_extra, obj:{ idx, pos_err_px,
//   rot_err_deg, sim, judges:[{id,value,status,m}], min_m, n_ok } }   -- one row per found object matched to a base
//   object by the augmentation's expected transform; plus one row with obj:null per (image, params, aug) carrying the
//   counts, so misses and false positives are countable even when nothing was found. m = normalised judge margin:
//   min(USL-v, v-LSL) / ((USL-LSL)/2), 1 = dead centre, 0 = on the limit, <0 = out.
import fs from 'node:fs'; import path from 'node:path'; import WebSocket from 'ws';
const PORT = process.env.CORE_PORT || '4093'; const clone=(d)=>JSON.parse(JSON.stringify(d));
const D = '../../../../InspectionCore/Core0_1/data/'; const DATA_ABS = path.resolve(D) + path.sep; const HDR = 9, enc = new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws = new WebSocket('ws://127.0.0.1:' + PORT); ws.binaryType = 'arraybuffer'; let pg = 12000; const W = {};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='SF'){delete W[id];try{w.res(JSON.parse(txt));}catch(e){w.res(null);}return;}if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
// II takes its calibration from the def's featureSet[0].cam_param (mmpb2b / ppb2b), not from calibInfo. A magnified
// frame (scale aug) is therefore sent with mmpb2b / scale on a copy: the frame's honest mm-per-pixel, while the def's
// own mmpp stays the taught one -- which is exactly the magnification-portability case the matcher is written for.
const withMmpp=(def,scale)=>{ if(!scale||scale===1) return def; const d=clone(def); const cp=d.featureSet[0].cam_param; if(cp&&cp.mmpb2b) cp.mmpb2b=cp.mmpb2b/scale; return d; };
const ii=(def,img,perturb,scale)=>new Promise(res=>{const id=pg++;W[id]={res};const body={definfo:withMmpp(def,scale),imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(perturb)body.img_property.perturb=perturb;ws.send(frame('II',0,id,body));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
const sf=(def)=>new Promise(res=>{const id=pg++;W[id]={res};ws.send(frame('SF',0,id,{definfo:def,regenerate:true}));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},60000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));

const cfgPath = process.argv[2]; if (!cfgPath) { console.error('usage: node sbm_sweep.mjs <config.json> [out.jsonl]'); process.exit(2); }
const CFG = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
const OUT = process.argv[3] || cfgPath.replace(/\.json$/, '') + '.jsonl';
const out = fs.openSync(OUT, 'w'); const emit = (r) => fs.writeSync(out, JSON.stringify(r) + '\n');
const setk=(d,k,v)=>{ d[k]=v; d.featureSet[0][k]=v; };
const getk=(d,k,dflt)=> d[k] ?? d.featureSet[0][k] ?? dflt;
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return (g&&g.reports)||[];};
const globList = (pats) => { const all = fs.readdirSync(D); const re = (p) => new RegExp('^' + p.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*') + '$');
  const s = new Set(); for (const p of pats) for (const f of all) if (re(p).test(f)) s.add(f); return [...s].sort(); };

// Expected position of a base object after the augmentation: cv::getRotationMatrix2D(centre, rot, scale) then + shift
// (TestPerturb.h). Angle positive = CCW in image coordinates. Skew is not modelled (unused in configs here).
const expectPx = (x, y, W, H, a) => { const th = (a.rot_deg || 0) * Math.PI / 180, s = a.scale || 1, cx = W / 2, cy = H / 2;
  const A = s * Math.cos(th), B = s * Math.sin(th); return [A * x + B * y + (1 - A) * cx - B * cy + (a.shift_x || 0), -B * x + A * y + B * cx + (1 - A) * cy + (a.shift_y || 0)]; };
const pngSize = (f) => { const b = fs.readFileSync(D + f).subarray(16, 24); return [b.readUInt32BE(0), b.readUInt32BE(4)]; };
const judgeLimits = (def) => { const m = {}; for (const f of def.featureSet[0].features || []) if (f.type === 'measure' && f.USL != null && f.LSL != null) m[f.id] = { USL: f.USL, LSL: f.LSL }; return m; };
const judgeRows = (o, lim) => (o.judgeReports || []).map(j => { const L = lim[j.id]; const half = L ? (L.USL - L.LSL) / 2 : 0;
  const m = (L && half > 0 && Number.isFinite(j.value)) ? +(Math.min(L.USL - j.value, j.value - L.LSL) / half).toFixed(3) : null; return { id: j.id, value: j.value, status: j.status, m }; });

// Parameter sets: cartesian product over the axes present in the config; the def's own value is always a level.
const paramSets = (def) => {
  const axes = [];
  for (const k of ['shape_angle_step_deg', 'shape_match_scale', 'shape_num_features', 'shape_strong_thres', 'shape_weak_thres', 'shape_min_score']) {
    if (!CFG.params[k]) continue; const own = getk(def, k, { shape_angle_step_deg: 1, shape_match_scale: 1, shape_num_features: 128, shape_strong_thres: 80, shape_weak_thres: 50, shape_min_score: 50 }[k]);
    axes.push({ k, levels: [...new Set([own, ...CFG.params[k]])].sort((a, b) => a - b) });
  }
  let sets = [{}];
  for (const ax of axes) sets = sets.flatMap(s => ax.levels.map(v => ({ ...s, [ax.k]: v })));
  const roi = CFG.params.roi && CFG.params.roi.length ? CFG.params.roi : [{}];
  return sets.flatMap(s => roi.map(r => ({ ...s, ...r })));
};
const REGEN_KEYS = ['shape_num_features', 'shape_strong_thres', 'shape_weak_thres'];
const REGEN_DFLT = { shape_num_features: 128, shape_strong_thres: 80, shape_weak_thres: 50 };
const regenKey = (d) => REGEN_KEYS.map(k => getk(d, k, REGEN_DFLT[k])).join(',');

let nII = 0; const t0 = Date.now();
for (const R of CFG.recipes) {
  const def0 = JSON.parse(fs.readFileSync(D + R.def, 'utf8')); const fs0 = def0.featureSet[0]; const mmpp = fs0.mmpp;
  const sbmIdx = (fs0.inherentfeatures || []).findIndex(e => e && e.name === '@__SBM_INFO__');
  const lim = judgeLimits(def0);
  let images = globList(R.images || [R.def.replace(/\.hydef$/, '.png')]); if (R.max_images) images = images.slice(0, R.max_images);
  const sets = paramSets(def0);
  const augs = [{}, ...(CFG.aug || [])]; const seeds = CFG.seeds || [7];
  console.log(`${R.name}: ${images.length} image(s) x ${sets.length} param sets x ${augs.length} aug points x ${seeds.length} seed(s)`);
  // The def's own parameter set, named once so the report compares against the same run's measurement of it.
  const ownP = {}; for (const k of Object.keys(sets[0])) if (!k.startsWith('shape_roi_')) ownP[k] = getk(def0, k, REGEN_DFLT[k] ?? { shape_angle_step_deg: 1, shape_match_scale: 1, shape_min_score: 50 }[k]);
  const base_pi = sets.findIndex(s => Object.keys(s).every(k => k.startsWith('shape_roi_') ? false : s[k] === ownP[k]) );
  emit({ recipe: R.name, meta: true, base_pi, base_params: sets[base_pi], images, n_sets: sets.length });
  // Regenerated feature caches, one per distinct extraction-knob tuple that differs from the def's own.
  const regenCache = {}; const ownRegen = regenKey(def0);
  const defFor = async (p) => {
    const d = clone(def0); for (const [k, v] of Object.entries(p)) setk(d, k, v);
    const key = regenKey(d);
    if (key === ownRegen || sbmIdx < 0) return d;
    if (!(key in regenCache)) {
      const q = clone(d); q.featureSet[0]._ref_image_path = DATA_ABS + R.def.replace(/\.hydef$/, '.png');
      const r = await sf(q); regenCache[key] = (r && r.shape_cache && (r.features || []).length) ? r.shape_cache : null;
      console.log(`  regen ${key}: ${regenCache[key] ? 'ok' : 'FAILED'}`);
    }
    if (!regenCache[key]) return null;
    d.featureSet[0].inherentfeatures[sbmIdx].shape_cache = regenCache[key]; return d;
  };
  for (let pi = 0; pi < sets.length; pi++) {
    const p = sets[pi]; const d = await defFor(p); if (!d) { emit({ recipe: R.name, pi, params: p, error: 'regen failed' }); continue; }
    for (const image of images) {
      const [Wd, Ht] = pngSize(image);
      // the same def's own, unperturbed result on this image is the reference for pose repeatability
      const base = objs(await ii(d, 'data/' + image)); nII++;
      const basePx = base.map(o => [o.cx / mmpp, o.cy / mmpp]);
      for (const a of augs) for (const seed of seeds) {
        const pert = Object.keys(a).length ? { ...a, seed } : null;
        // A scale augmentation magnifies the picture; the honest calibration for it is mmpp / scale, otherwise every
        // distance judge shifts by the scale factor and reads as a locator failure. (This also exercises the matcher's
        // magnification portability: the def keeps its taught mmpp, the frame comes with another.)
        const mmppEff = mmpp / (a.scale || 1);
        const rp = pert ? await ii(d, 'data/' + image, pert, a.scale) : null; nII++;
        const found = pert ? objs(rp) : base; const ms = pert ? (rp && rp.insp_wall_ms) : null;
        // match found objects to base objects through the expected transform (greedy nearest, 40 px gate)
        const exp = basePx.map(([x, y]) => expectPx(x, y, Wd, Ht, a)); const used = new Set(); let nMatched = 0;
        const rows = [];
        for (const o of found) {
          const fx = o.cx / mmppEff, fy = o.cy / mmppEff; let bi = -1, bd = 40;
          exp.forEach(([ex, ey], i) => { if (used.has(i)) return; const dd = Math.hypot(fx - ex, fy - ey); if (dd < bd) { bd = dd; bi = i; } });
          if (bi < 0) continue; used.add(bi); nMatched++;
          const dr = (o.rotate - base[bi].rotate) * 180 / Math.PI; const rot = a.rot_deg || 0;
          const rotErr = Math.min(Math.abs(((dr - rot + 540) % 360) - 180), Math.abs(((dr + rot + 540) % 360) - 180));
          const js = judgeRows(o, lim); const ms_ = js.map(j => j.m).filter(m => m != null);
          rows.push({ idx: bi, pos_err_px: +bd.toFixed(3), rot_err_deg: +rotErr.toFixed(4), sim: o.similarity, judges: js, min_m: ms_.length ? Math.min(...ms_) : null, n_ok: js.filter(j => j.status === 0).length });
        }
        const head = { recipe: R.name, image, pi, params: p, aug: a, seed: pert ? seed : null, ms, n_base: base.length, n_found: found.length, n_extra: found.length - nMatched };
        emit({ ...head, obj: null });
        for (const r of rows) emit({ ...head, obj: r });
      }
    }
    if (pi % 10 === 9 || pi === sets.length - 1) console.log(`  ${pi + 1}/${sets.length} param sets, ${nII} inspections, ${((Date.now() - t0) / 60000).toFixed(1)} min`);
  }
}
fs.closeSync(out); console.log(`done: ${nII} inspections in ${((Date.now() - t0) / 60000).toFixed(1)} min -> ${OUT}`);
ws.close(); process.exit(0);
