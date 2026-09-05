// Usage: node sbm_sweep_report.mjs <rows.jsonl> [profile.json]
//
// THE ACCEPTANCE PROFILE IS THE DIAL. It reads the rows sbm_sweep.mjs saved and, per recipe, aggregates every parameter
// set over all images and augmentation points, then applies each profile as a filter and picks the fastest survivor.
// Changing a tolerance is a re-read of the same data. Two built-in profiles; a JSON file with the same shape overrides.
//
// Per parameter set the aggregate is:
//   fail      any (image, aug) with fewer objects than the same def found unperturbed, or an extra one, or an object
//             whose count of passing judges is below its own unperturbed count           -- HARD, both profiles
//   pos_p95   p95 of position repeatability error (px) over all objects and aug points
//   rot_p95   p95 of rotation repeatability error (deg)
//   m_ratio   min over aug of (normalised judge margin / the same object's unperturbed margin); 1 = margins untouched
//   sim_drop  max over aug of (own similarity - aug similarity)
//   ms        median insp_wall_ms over aug rows
// and the profile compares them with the DEF'S OWN parameter set measured in the same run (base), so timing drift and
// the picture's own difficulty cancel.
import fs from 'node:fs';
const rows = fs.readFileSync(process.argv[2], 'utf8').trim().split('\n').map(JSON.parse);
const PROFILES = {
  normal:     { pos_ratio: 1.2, pos_floor: 0.3, rot_ratio: 1.2, rot_floor: 0.05, m_ratio: 0.9, sim_drop: 0.05 },
  aggressive: { pos_ratio: 2.0, pos_floor: 0.5, rot_ratio: 2.0, rot_floor: 0.10, m_ratio: 0.7, sim_drop: 0.10 },
};
if (process.argv[3]) Object.assign(PROFILES, JSON.parse(fs.readFileSync(process.argv[3], 'utf8')));
const q = (arr, p) => { if (!arr.length) return null; const a = [...arr].sort((x, y) => x - y); return a[Math.min(a.length - 1, Math.floor(p * a.length))]; };
const isOwn = (a) => Object.keys(a).length === 0;
const pkey = (p) => JSON.stringify(p);
const fmt = (p) => Object.entries(p).map(([k, v]) => k.replace('shape_', '').replace('angle_step_deg', 'step').replace('match_scale', 'scale').replace('num_features', 'nf').replace('roi_search', 'search').replace('roi_prescale', 'pre') + '=' + v).join(' ');

for (const recipe of [...new Set(rows.map(r => r.recipe))]) {
  const R = rows.filter(r => r.recipe === recipe);
  const byP = new Map();
  for (const r of R) { if (r.meta) continue; if (!byP.has(r.pi)) byP.set(r.pi, { params: r.params, heads: [], objs: [], error: r.error }); const g = byP.get(r.pi); if (r.obj) g.objs.push(r); else if (!r.error) g.heads.push(r); }
  const agg = [];
  for (const [pi, g] of byP) {
    if (g.error) { agg.push({ pi, params: g.params, error: g.error }); continue; }
    const ownByImg = {}; for (const h of g.heads) if (isOwn(h.aug)) ownByImg[h.image] = h.n_base;
    const ownObj = {}; for (const o of g.objs) if (isOwn(o.aug)) ownObj[o.image + '#' + o.obj.idx] = o.obj;
    let fail = [], pos = [], rot = [], mr = [], sd = [], ms = [];
    for (const h of g.heads) { if (isOwn(h.aug)) continue; if (h.n_found < h.n_base) fail.push(`${h.image} ${JSON.stringify(h.aug)}: found ${h.n_found}/${h.n_base}`); if (h.n_extra > 0) fail.push(`${h.image} ${JSON.stringify(h.aug)}: +${h.n_extra} extra`); if (h.ms) ms.push(h.ms); }
    for (const o of g.objs) { if (isOwn(o.aug)) continue; const own = ownObj[o.image + '#' + o.obj.idx]; if (!own) continue;
      pos.push(o.obj.pos_err_px); rot.push(o.obj.rot_err_deg); sd.push(own.sim - o.obj.sim);
      if (o.obj.n_ok < own.n_ok) fail.push(`${o.image}#${o.obj.idx} ${JSON.stringify(o.aug)}: judges ${o.obj.n_ok}/${own.n_ok}`);
      if (own.min_m != null && own.min_m > 0 && o.obj.min_m != null) mr.push(o.obj.min_m / own.min_m); }
    const nOwn = Object.values(ownByImg).reduce((a, b) => a + b, 0);
    agg.push({ pi, params: g.params, fail, nOwn, pos_p95: q(pos, 0.95), rot_p95: q(rot, 0.95), m_ratio: mr.length ? Math.min(...mr) : null, sim_drop: sd.length ? Math.max(...sd) : 0, ms: q(ms, 0.5), ms_p95: q(ms, 0.95) });
  }
  const meta = R.find(r => r.meta); const base = agg.find(a => a.pi === (meta ? meta.base_pi : 0));
  if (!base || base.error) { console.log(`
== ${recipe}: base parameter set missing or failed; cannot compare`); continue; }
  console.log(`\n== ${recipe}: ${agg.length} parameter sets, ${nObjs(R)} object rows; base = [${fmt(base.params)}] ms ${base.ms?.toFixed(1)} pos_p95 ${base.pos_p95} rot_p95 ${base.rot_p95} m_ratio ${base.m_ratio?.toFixed(2)} fail ${base.fail.length}`);
  if (base.fail.length) console.log('   base fails its own augmentation set:\n     ' + base.fail.slice(0, 6).join('\n     ') + (base.fail.length > 6 ? `\n     ... ${base.fail.length} total` : ''));
  // sensitivity of the base: which augmentation points hurt it most (by judge-margin ratio)
  const sens = {}; for (const o of R.filter(r => r.obj && r.pi === base.pi && !isOwn(r.aug))) { const own = R.find(x => x.obj && x.pi === base.pi && isOwn(x.aug) && x.image === o.image && x.obj.idx === o.obj.idx); if (!own || own.obj.min_m == null || own.obj.min_m <= 0 || o.obj.min_m == null) continue; const k = JSON.stringify(o.aug); sens[k] = Math.min(sens[k] ?? Infinity, o.obj.min_m / own.obj.min_m); }
  const sensArr = Object.entries(sens).sort((a, b) => a[1] - b[1]).slice(0, 5); if (sensArr.length) console.log('   thinnest margins under: ' + sensArr.map(([k, v]) => `${k} x${v.toFixed(2)}`).join(', '));
  for (const [name, P] of Object.entries(PROFILES)) {
    const ok = agg.filter(a => !a.error && a.fail.length === 0
      && a.pos_p95 <= Math.max(P.pos_floor, base.pos_p95 * P.pos_ratio)
      && a.rot_p95 <= Math.max(P.rot_floor, base.rot_p95 * P.rot_ratio)
      && (a.m_ratio == null || base.m_ratio == null || a.m_ratio >= base.m_ratio * P.m_ratio)
      && a.sim_drop <= base.sim_drop + P.sim_drop && a.ms != null).sort((a, b) => a.ms - b.ms);
    console.log(`   ${name.padEnd(10)} ${ok.length}/${agg.length} sets accepted` + (ok.length ? `; fastest: [${fmt(ok[0].params)}] ms ${ok[0].ms.toFixed(1)} (${(100 * (1 - ok[0].ms / base.ms)).toFixed(0)}% vs base) pos_p95 ${ok[0].pos_p95} rot_p95 ${ok[0].rot_p95} m_ratio ${ok[0].m_ratio?.toFixed(2)}` : ''));
    for (const a of ok.slice(1, 4)) console.log(`              next: [${fmt(a.params)}] ms ${a.ms.toFixed(1)} pos_p95 ${a.pos_p95} rot_p95 ${a.rot_p95} m_ratio ${a.m_ratio?.toFixed(2)}`);
  }
  // marginal view: fail share per level of each axis -- says which knob breaks things, independent of the profile
  const axes = {}; for (const a of agg) if (!a.error) for (const [k, v] of Object.entries(a.params)) { (axes[k] ??= {}); (axes[k][v] ??= { n: 0, f: 0, ms: [] }); axes[k][v].n++; if (a.fail.length) axes[k][v].f++; if (a.ms) axes[k][v].ms.push(a.ms); }
  const noRoi = agg.filter(a => !a.error && !('shape_roi_search' in a.params) && !('shape_roi_prescale' in a.params)); const withS = agg.filter(a => !a.error && 'shape_roi_search' in a.params), withP = agg.filter(a => !a.error && 'shape_roi_prescale' in a.params);
  console.log('   fail share by level: ' + Object.entries(axes).map(([k, L]) => k.replace('shape_', '') + ' {' + Object.entries(L).map(([v, s]) => `${v}: ${s.f}/${s.n}`).join(', ') + '}').join('  ')
    + `  roi {none: ${noRoi.filter(a => a.fail.length).length}/${noRoi.length}, search30: ${withS.filter(a => a.fail.length).length}/${withS.length}, pre0.5: ${withP.filter(a => a.fail.length).length}/${withP.length}}`);
}
function nObjs(R) { return R.filter(r => r.obj).length; }
