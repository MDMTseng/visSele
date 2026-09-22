// The edge parameters a picture can decide, decided from the picture.
//
// One function, two callers: the property sheet's 自動 button and the one-click
// 升級. Both used to have their own rule -- the sheet took the geometric mean
// of "strongest peak" and "second peak", the upgrade flipped polarities by
// trial and halved min_strength on NA -- and the two gave different answers
// for the same primitive on the same picture. Now they cannot.
//
// Input is the core's ungated `edge_profile` payload (DEBUG_EMIT edge_profile):
//
//   caliper (line/arc)  { step, L, g: [[signed grad]...], sel: [sample|-1] }
//                       sample j of caliper k sits at (-L + j*step) px across
//                       the edge; the TAUGHT edge is at the centre, j = L/step.
//   search point        { span, p: [pos], s: [str], a: [along], sel_p }
//                       one candidate per row; pos is px from the window's
//                       near end; the taught point sits at span/2.
//
// The truth is the taught position, not the strongest peak. The operator put
// the primitive on the edge they meant; a stronger edge elsewhere in the
// window is exactly the thing the parameters have to keep out.
//
// Pure: no React, no store, so the bench scripts import it too.

const sq = (v) => v * v;

// Local maxima of the polarity-selected gradient -- edge_select's own rule.
function peaksOf(g, polarity) {
  const f = polarity === 'rising' ? (v) => v : polarity === 'falling' ? (v) => -v : Math.abs;
  const out = [];
  for (let i = 1; i < g.length - 1; i++) {
    const v = f(g[i]), a = f(g[i - 1]), b = f(g[i + 1]);
    if (v > 0 && v >= a && v >= b && (v > a || v > b)) out.push({ i, v });
  }
  return out;
}

// Full width at half maximum of the |grad| bump around sample i, in samples.
function fwhmAt(g, i) {
  const h = Math.abs(g[i]) / 2;
  let l = i, r = i;
  while (l > 0 && Math.abs(g[l - 1]) >= h && Math.sign(g[l - 1]) === Math.sign(g[i])) l--;
  while (r < g.length - 1 && Math.abs(g[r + 1]) >= h && Math.sign(g[r + 1]) === Math.sign(g[i])) r++;
  return r - l + 1;
}

// THE FLOOR, from the two numbers that bound it.
//   signal = the weakest taught edge across the calipers/rows: above this a
//            measurement disappears.
//   noise  = the strongest competing peak: below this a competitor can be
//            picked instead, which is a wrong number, not a missing one.
// Geometric mean between them -- scale-free, so a lighting or lens change
// that rescales the gradient units moves it in proportion. Never above 85% of
// the signal: a suggestion that drops a measurement when applied is not one.
function floorBetween(signal, noise) {
  if (!(signal > 0)) return { min_strength: 0, clean: false, ratio: 0 };
  const clean = signal > noise * 1.25;
  let t = clean ? Math.sqrt(signal * Math.max(noise, 1)) : signal * 0.5;
  t = Math.min(t, signal * 0.85);
  return { min_strength: Math.max(1, Math.round(t)), clean, ratio: noise > 0 ? signal / noise : Infinity };
}

// Caliper (line/arc). `edge` is the shape's current edge block.
export function edgeAutoCaliper(profile, edge = {}) {
  const g = profile && profile.g;
  if (!g || !g.length || !g[0].length || !(profile.step > 0)) return null;
  const n = g[0].length, c = (n - 1) / 2;         // taught edge = centre sample
  const step = profile.step;
  // How far from the centre the taught edge may sit in the picture: the
  // primitive is placed by hand, and the picture is the one it was placed on.
  const near = Math.max(3, Math.round(Math.min(0.25 * n, 6 / step)));

  // POLARITY: the sign of the strongest |grad| peak near the centre, voted
  // across calipers. A caliper with no peak there abstains.
  let vr = 0, vf = 0;
  const taught = [];                                 // per caliper: {i, v, sign}
  for (const one of g) {
    let best = null;
    for (const p of peaksOf(one, 'any')) {
      if (Math.abs(p.i - c) > near) continue;
      // nearer wins unless a clearly stronger one is also near
      const score = p.v / (1 + sq((p.i - c) / near));
      if (!best || score > best.score) best = { i: p.i, v: p.v, sign: Math.sign(one[p.i]), score };
    }
    taught.push(best);
    if (best) { if (best.sign > 0) vr++; else vf++; }
  }
  if (!vr && !vf) return { ok: false, reason: 'no_edge_at_taught' };
  const polarity = vr > vf ? 'rising' : vf > vr ? 'falling' : (edge.polarity || 'falling');
  const sgn = polarity === 'rising' ? 1 : -1;

  // SIGNAL and NOISE, with the polarity settled.
  let signal = Infinity, noise = 0, missing = 0, competitorDist = Infinity, fwhm = 0, nF = 0;
  g.forEach((one, k) => {
    const t = taught[k];
    if (!t || t.sign !== sgn) { missing++; return; }
    signal = Math.min(signal, t.v);
    fwhm += fwhmAt(one, t.i); nF++;
    for (const p of peaksOf(one, polarity)) {
      if (p.i === t.i) continue;
      noise = Math.max(noise, p.v);
      // a competitor that the floor cannot remove has to be kept out by L
      if (p.v >= t.v * 0.8) competitorDist = Math.min(competitorDist, Math.abs(p.i - c) * step);
    }
  });
  if (!(signal < Infinity)) return { ok: false, reason: 'no_edge_of_polarity', polarity };
  const fl = floorBetween(signal, noise);

  // SIGMA: a soft edge is a wide gradient bump; smoothing to a third of its
  // width keeps one maximum on it without moving the centre. Crisp edges
  // (bump under ~3 px) get none.
  const widthPx = nF ? (fwhm / nF) * step : 0;
  const sigma = widthPx > 3 ? Math.min(3, Math.round((widthPx / 3) * 10) / 10) : 0;

  // LENGTH: if a competitor as strong as the edge sits inside the window, no
  // floor separates them; shrink the half-span to keep it out, never below
  // 3 px or 40% of what it was.
  let lengthPx = null;
  if (competitorDist < profile.L) lengthPx = Math.max(3, 0.4 * profile.L, 0.8 * competitorDist);

  return {
    ok: true, kind: 'caliper', polarity, polarityChanged: !!edge.polarity && edge.polarity !== polarity,
    min_strength: fl.min_strength, rel_strength: 0, sigma,
    lengthPx, signal: Math.round(signal), noise: Math.round(noise), ratio: fl.ratio, clean: fl.clean,
    calipers: g.length, missing,
  };
}

// Search point. The scan is FIRST hit from the near end; the taught point is
// at span/2. The payload is already folded to the def's polarity, so polarity
// cannot be read here -- the caller decides it (the upgrade probes both).
export function edgeAutoSearchPoint(profile, edge = {}) {
  if (!profile || !profile.p || !profile.p.length || !(profile.span > 0)) return null;
  const c = profile.span / 2;
  const tol = Math.max(2, profile.span * 0.06);
  const pts = profile.p.map((pos, i) => ({ pos, str: profile.s[i] }));
  // The taught edge: candidates within tol of the centre; signal = their
  // weakest (every row has to clear the floor). Competitors: anything NEARER
  // the start -- only those can be "first" instead.
  const at = pts.filter((q) => Math.abs(q.pos - c) <= tol);
  if (!at.length) return { ok: false, reason: 'no_edge_at_taught' };
  const signal = Math.min(...at.map((q) => q.str));
  const nearer = pts.filter((q) => q.pos < c - tol);
  const noise = nearer.length ? Math.max(...nearer.map((q) => q.str)) : 0;
  const fl = floorBetween(signal, noise);
  // margin: a nearer competitor as strong as the edge cannot be floored out;
  // the half-window has to stop short of it.
  let marginPx = null;
  const strong = nearer.filter((q) => q.str >= signal * 0.8);
  if (strong.length) {
    const d = Math.min(...strong.map((q) => c - q.pos));
    marginPx = Math.max(3, 0.4 * c, 0.8 * d);
  }
  return {
    ok: true, kind: 'search_point', min_strength: fl.min_strength, rel_strength: 0,
    marginPx, signal: Math.round(signal), noise: Math.round(noise), ratio: fl.ratio, clean: fl.clean,
    rows: at.length, nearer: nearer.length,
  };
}

export function edgeAuto(profile, edge) {
  if (!profile) return null;
  if (profile.kind === 'peaks' || profile.p) return edgeAutoSearchPoint(profile, edge);
  return edgeAutoCaliper(profile, edge);
}

// The def patch for a shape from an `edgeAuto` result. Lengths come back in
// px; the def wants mm.
//
// WHAT GETS WRITTEN, AND WHAT STAYS A HINT. Measured on 247 field recipes
// (2026-09-05, same seed, judges OK on the reference image): the floor is
// worth +27 where the edge is clean; zeroing rel_strength added nothing;
// sigma added nothing; shrinking windows to the taught edge added +24 on the
// reference image and takes field tolerance to do it. So the default patch is
// polarity + min_strength. `full` writes the rest too, for a caller that has
// decided it wants them; the panel shows them as sentences instead.
export function edgeAutoPatch(shape, r, mmpp, { full = false } = {}) {
  if (!r || !r.ok) return null;
  const edge = { ...(shape.edge || {}), min_strength: r.min_strength };
  const out = { edge };
  if (r.kind === 'caliper') edge.polarity = r.polarity;
  if (!full) return out;
  edge.rel_strength = 0;
  if (r.kind === 'caliper') {
    edge.sigma = r.sigma;
    if (r.lengthPx != null && mmpp > 0) out.caliper = { ...(shape.caliper || {}), length: Math.round(r.lengthPx * mmpp * 1e4) / 1e4 };
  } else if (r.marginPx != null && mmpp > 0) {
    out.margin = Math.round(r.marginPx * mmpp * 1e4) / 1e4;
  }
  return out;
}
