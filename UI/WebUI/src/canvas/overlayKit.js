// Overlay design tokens + drawing vocabulary, in ONE place.
//
// Every per-shape draw module used to pick its own colours, dash patterns and
// sizes inline, so the same red meant four different things and tuning any of
// it meant editing a dozen files. This module owns those numbers; the modules
// ask for them by role.
//
// The organising rule is COLOUR = ROLE, NOT TYPE. The same fitted line is drawn
// in the datum colour when something else is measured against it and in the
// feature colour when it is the thing being measured. Verdict colours (ok/ng)
// never touch geometry -- they appear only on chips and gauges -- so a red
// outline always and only means "exclude region".
//
// Sizes are expressed as MULTIPLES OF renderer.getPrimitiveSize() (`ps`), which
// already scales with the camera, so nothing here is in pixels or in mm.
//
// LIVE TUNING. From the browser console:
//     OVERLAY_TUNE({ size: { gauge_r: 8 }, role: { reading: '#e08a00' } })
//     OVERLAY_TUNE.reset()            // back to the defaults below
//     OVERLAY_TUNE.dump()             // current values, ready to paste back here
// Overrides are merged (deep, per leaf) and kept in localStorage under
// OVERLAY_TUNE_KEY, so a reload keeps them. That is deliberate: it lets the
// numbers be settled on a real part in front of the machine, then pasted back
// into OVERLAY_DEFAULTS as a commit.

export const OVERLAY_DEFAULTS = {
  // ---- roles -------------------------------------------------------------
  // Taken from the KEYENCE IM in mech1_ref/OVERLAY_DESIGN.md, which has had two
  // decades of shop-floor use to settle four roles and no more:
  //
  //   fitted geometry  solid yellow, thin, drawn on the edge it found
  //   search region    translucent cyan AREA (never a fat stroke)
  //   the measurement  RED, always DASHED, with arrowheads -- and nothing else
  //                    on the canvas is red, so a dashed red line means
  //                    "this is a number", full stop
  //   construction     thin dotted, low contrast, no arrowheads
  //
  // The contrast between the last two is the point: an extension line must be
  // visibly quieter than the dimension it serves, or the eye cannot tell which
  // mark carries the value.
  role: {
    feature: 'rgba(232,214,26,1)',    // fitted geometry: line, arc, sig360 outline
    search:  'rgba(41,182,216,1)',    // caliper boxes, margin bands, scan direction
    reading: 'rgba(229,57,53,1)',     // dimension lines, angle arcs, the value
    region:  'rgba(240,168,58,1)',    // construction / projection / aux / loc regions
    // Ours, not the reference's: it has no datum colour because a datum is just
    // another element named in the label. We mark locating anchors and the
    // registration frame, which the operator does need to pick out.
    datum:   'rgba(66,133,214,1)',
    ok:      'rgba(18,135,74,1)',
    ng:      'rgba(207,42,42,1)',
    neutral: 'rgba(120,132,143,1)',
    paper:   'rgba(255,255,255,0.88)',  // chip background
  },
  // ---- fill opacities ----------------------------------------------------
  alpha: {
    wedge:   0.16,   // angle gap style: between datum ray and the measured line
    sector:  0.14,   // angle vertex style: the swept sector
    search:  0.15,   // search/margin bands
    region:  0.14,   // loc_include fill
    faint:   0.55,   // construction lines: quieter than what they serve
    band:    0.85,   // gauge tolerance band stroke
  },
  // ---- line vocabulary (dash patterns, in ps units) ----------------------
  dash: {
    meas:   [3, 2],               // THE measurement. Red + this = a number.
    datum:  [7, 2.5, 1.5, 2.5],   // dash-dot: a reference, never a real edge
    aux:    [5, 3],               // long dash: virtual extension / construction
    tie:    [1.5, 2.5],           // fine dot: construction, "this uses that"
    search: [4, 2.5],             // search-area outline
  },
  // ---- sizes, all in multiples of getPrimitiveSize() ---------------------
  size: {
    // Overall weight of every overlay stroke. getIndicationLineSize() carries
    // it, and every module's width is a multiple of that, so this thins or
    // fattens the whole drawing at once.
    stroke_scale: 0.49,   // 0.7 of 0.7 -- thinned twice, on the machine
    line_w:      1.0,    // x getIndicationLineSize()
    thin_w:      0.7,    // x getIndicationLineSize()
    heavy_w:     1.35,   // x getIndicationLineSize()
    arrow_head:  2.0,
    tick:        1.6,
    datum_tri_h: 2.2,
    datum_tri_w: 1.4,
    datum_lead:  2.4,    // triangle apex -> letter box
    chip_pad:    0.7,    // x font height
    chip_gap:    3.2,    // chip offset from what it labels
    ext_over:    2.0,    // how far an extension line runs past its foot
    gauge_r:     6.0,
    gauge_dy:    8.5,    // gauge centre above the label point
    span_min:    14.0,   // shortest drawn span (gap style)
    span_max:    60.0,   // longest drawn span
  },
  // ---- text --------------------------------------------------------------
  font: {
    chip:  0.85,   // x getFontHeightPx()
    tag:   0.80,
    small: 0.72,
  },
  // ---- labels ------------------------------------------------------------
  // The reference machine prints "[N] value[mode]" and its calculator inserts
  // that same [N] into a formula. Ours could: calc_f already references
  // measures as [id]. But the id is OURS, not the operator's -- they number
  // their measures the way the drawing numbers them, and two numbering schemes
  // on one label is worse than none. So the id stays an id, the name stays
  // theirs, and this is off. Turn it on for a session with
  //     OVERLAY_TUNE({ label: { show_index: true } })
  // when hunting for which measure an expression refers to.
  label: {
    show_index: false,
    // Primitives do not name themselves. A def has dozens of them and their
    // names are for the property sheet, not the picture -- printed on the
    // canvas they bury the thing they are naming, and they bury the MEASURE
    // labels, which are the only text anyone reads off the image.
    show_primitive_names: false,
  },

  // ---- control points ----------------------------------------------------
  // A line's endpoints and an arc's three picks are HANDLES: they exist to be
  // dragged. Drawn on every shape all the time they are just dots over the
  // part -- a def with twenty primitives puts fifty of them on the image. They
  // appear on the shape being edited, where they are the thing you reach for.
  // Hit-testing is unaffected, so a handle still grabs whether or not it is
  // painted.
  ctrl: { edit_only: true },

  // ---- level of detail ---------------------------------------------------
  // A shape whose on-screen extent is under `detail_ps` primitive-sizes gets
  // geometry only: no name plate, no dimension text. `size / ps` is
  // proportional to on-screen pixels (ps = base / cameraScale), so this is a
  // real zoom test, not a mm threshold.
  lod: { detail_ps: 20 },

  // ---- gauge -------------------------------------------------------------
  gauge: {
    enabled:   true,
    band_span: 1.8,    // full-scale = tolerance half-width x this
    face:      'rgba(255,255,255,0.85)',
  },
  // ---- angle measure specifics ------------------------------------------
  angle: {
    // 'simple' draws only what answers "which is the datum, which way does it
    // tilt, by how much": the filled wedge or sector, the datum ray, and one
    // value. 'full' adds the dimensioned standoffs, the ISO datum triangle,
    // the A/B plates and the direction arrowheads. Switch live with
    //     OVERLAY_TUNE({ angle: { detail: 'full' } })
    detail:         'simple',
    // The verdict gauge is for reading results, not for setting a def up, so
    // by default it appears only where there IS a result.
    gauge_on_insp_only: true,
    // A parallelism reading is a fraction of a degree; drawn to scale the arc
    // is nothing at all. This is the smallest arc the overlay will draw, in
    // degrees -- the number in the text stays the true one.
    // An angular dimension's arc ENDS ON ITS TWO EXTENSION LINES -- that is
    // what makes it a dimension rather than a decoration. Opening a small arc
    // out so it stays visible breaks exactly that: the opened end leaves the
    // line, and the drawing then says an angle nobody measured. So: 0, draw
    // the true angle. A fraction of a degree is a short arc, and the pair of
    // extension lines plus the value carry it. Set it non-zero only if you
    // would rather see the direction than have the arc land on the line.
    min_draw_deg:         0,
    // Radius of the arc when there is no vertex on screen (near-parallel), in
    // primitive-sizes. Big enough that the opened arc and the two direction
    // rays are legible next to the label.
    // How far past the arc's end a lead-out arc is still an arc.
    lead_arc_max_deg:    35,
    // Lay the value along the radius it hangs off (a drawing convention, and
    // what the reference machine does). Off = axis-aligned, as before.
    label_follows_radius: true,
    gap_style_max_deg:   25,    // below this the lines count as "near parallel"
    // Only a sanity guard now, not a style switch: near-parallel lines meet a
    // long way out and the arc is SUPPOSED to be drawn there, shallow and
    // passing close to the label -- which is exactly how a small angle looks
    // on the reference machine. This just stops a numerically absurd vertex
    // (effectively parallel) from being drawn to.
    vertex_max_ps:     4000,
    head_inside_min_deg:  8,    // narrower than this, arrowheads flip outside
    wedge_neg: 'rgba(120,80,220,1)',  // negative wedge fill hue (positive uses role.reading)
  },
};

const TUNE_KEY = 'OVERLAY_TUNE_KEY';

const clone = (o) => JSON.parse(JSON.stringify(o));
function deepMerge(dst, src) {
  for (const k of Object.keys(src || {})) {
    const v = src[k];
    if (v && typeof v === 'object' && !Array.isArray(v)) { dst[k] = deepMerge(dst[k] || {}, v); }
    else dst[k] = v;
  }
  return dst;
}

// The live token set. Modules import this binding and read through it, so a
// tune applies on the next repaint without a reload.
export const OVERLAY = clone(OVERLAY_DEFAULTS);

export function configureOverlay(patch, { persist = true } = {}) {
  deepMerge(OVERLAY, patch || {});
  if (persist && typeof localStorage !== 'undefined') {
    try {
      const prev = JSON.parse(localStorage.getItem(TUNE_KEY) || '{}');
      localStorage.setItem(TUNE_KEY, JSON.stringify(deepMerge(prev, patch || {})));
    } catch (e) { /* private mode / quota: tuning is a convenience, never fatal */ }
  }
  return OVERLAY;
}
export function resetOverlay() {
  deepMerge(OVERLAY, clone(OVERLAY_DEFAULTS));
  try { localStorage.removeItem(TUNE_KEY); } catch (e) { /* ignore */ }
  return OVERLAY;
}

// Restore a previous tuning session, then expose the console entry point.
if (typeof window !== 'undefined') {
  try {
    const saved = JSON.parse(localStorage.getItem(TUNE_KEY) || 'null');
    if (saved) deepMerge(OVERLAY, saved);
  } catch (e) { /* ignore */ }
  const tune = (patch) => configureOverlay(patch);
  tune.reset = resetOverlay;
  tune.dump = () => JSON.parse(JSON.stringify(OVERLAY));
  tune.defaults = () => clone(OVERLAY_DEFAULTS);
  window.OVERLAY_TUNE = tune;
  window.OVERLAY = OVERLAY;
}

// "[12] 外徑" -- the index the calc expression editor uses, in front of the
// operator's own name. Standalone so a caller with no canvas can use it too.
export function measureLabelName(shape) {
  const nm = (shape && shape.name) || '';
  if (!OVERLAY.label.show_index || !shape || shape.id === undefined) return nm;
  return '[' + shape.id + '] ' + nm;
}

// Turn 'rgba(r,g,b,a)' into the same colour at another alpha. Kept dumb on
// purpose: every colour in this file is written in that one form.
export function withAlpha(rgba, a) {
  const m = /^rgba?\(([^)]+)\)$/.exec(rgba);
  if (!m) return rgba;
  const p = m[1].split(',').map((s) => s.trim());
  return `rgba(${p[0]},${p[1]},${p[2]},${a})`;
}

// ---------------------------------------------------------------------------
// The drawing vocabulary. One call per draw() gives every helper the renderer
// and ctx it needs, with sizes already resolved against the current camera.
// ---------------------------------------------------------------------------
export function overlayKit(ctx, renderer) {
  const T = OVERLAY;
  const ps = renderer.getPrimitiveSize();
  const lw = renderer.getIndicationLineSize();
  const fpx = renderer.getFontHeightPx();
  const S = T.size;
  const dash = (name) => (T.dash[name] || []).map((v) => v * ps);

  const at = (c, ang, r) => ({ x: c.x + r * Math.cos(ang), y: c.y + r * Math.sin(ang) });
  const seg = (p, q) => { ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(q.x, q.y); ctx.stroke(); };
  const projOn = (Q, L0, L1) => {
    const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((Q.x - L0.x) * vx + (Q.y - L0.y) * vy) / n2;
    return { x: L0.x + t * vx, y: L0.y + t * vy };
  };

  // A filled head whose TIP sits at `tip`, pointing along `ang`.
  const arrow = (tip, ang, len = S.arrow_head * ps) => {
    const f = at(tip, ang + Math.PI, len);
    renderer.canvas_arrow(ctx, f.x, f.y, tip.x, tip.y, len);
  };

  // draw_Text's 3rd argument is a SCALE for a 1px font, and it leaves
  // ctx.lineWidth at a screen-size value -- every line stroked after a label
  // came out as a thick band. Both traps are handled here, once.
  const text = (str, x, y, scale = T.font.chip) => {
    const keep = ctx.lineWidth;
    renderer.draw_Text(ctx, str, fpx * scale, x, y);
    ctx.lineWidth = keep;
  };

  // The name plate: white box, role-coloured border, always horizontal, always
  // on top of the geometry. `x,y` is its CENTRE.
  const chip = (str, x, y, colour, scale = T.font.chip) => {
    const w = 0.62 * fpx * scale * String(str).length + S.chip_pad * fpx * scale;
    const h = 1.25 * fpx * scale;
    ctx.save();
    ctx.setLineDash([]);
    ctx.fillStyle = T.role.paper;
    ctx.fillRect(x - w / 2, y - h / 2, w, h);
    ctx.strokeStyle = colour; ctx.lineWidth = lw * S.thin_w;
    ctx.strokeRect(x - w / 2, y - h / 2, w, h);
    ctx.fillStyle = colour; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    text(str, x, y, scale);
    ctx.restore();
    return { w, h };
  };

  // ISO 1101 datum: a filled triangle standing on the line, a leader, a boxed
  // letter. `toward` is the side the reader is on (usually the label point).
  const datumMark = (anchor, dirAng, letter, toward) => {
    let side = dirAng + Math.PI / 2;
    if (toward) {
      const dx = toward.x - anchor.x, dy = toward.y - anchor.y;
      if (Math.hypot(dx, dy) > 1e-6) side = Math.atan2(dy, dx);
    }
    const apex = at(anchor, side, S.datum_tri_h * ps);
    const b1 = at(anchor, dirAng, S.datum_tri_w * ps);
    const b2 = at(anchor, dirAng + Math.PI, S.datum_tri_w * ps);
    ctx.save();
    ctx.setLineDash([]);
    ctx.fillStyle = ctx.strokeStyle = T.role.datum; ctx.lineWidth = lw * S.thin_w;
    ctx.beginPath(); ctx.moveTo(b1.x, b1.y); ctx.lineTo(apex.x, apex.y); ctx.lineTo(b2.x, b2.y); ctx.closePath(); ctx.fill();
    const box = at(apex, side, S.datum_lead * ps);
    seg(apex, box);
    chip(letter, box.x, box.y, T.role.datum, T.font.tag);
    ctx.restore();
    return box;
  };

  // "This is that line, extended": a thin dashed run from the end of the real
  // segment to a foot that lies outside it. Silent when the foot is on the
  // segment, which is the common case.
  const extendTo = (foot, L0, L1, colour) => {
    const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((foot.x - L0.x) * vx + (foot.y - L0.y) * vy) / n2;
    if (t >= 0 && t <= 1) return;
    const from = (t < 0) ? L0 : L1;
    ctx.save();
    ctx.strokeStyle = colour; ctx.lineWidth = lw * S.thin_w;
    ctx.setLineDash(dash('aux'));
    seg(from, at(foot, Math.atan2(foot.y - from.y, foot.x - from.x), S.ext_over * ps));
    ctx.restore();
  };

  // The verdict gauge: where the reading sits inside its tolerance band. The
  // drawing says WHAT is measured; this says HOW BAD, at a glance, without
  // reading a number. Drawn only when the def actually carries limits.
  const gauge = (cx, cy, value, { nominal = 0, lo, hi, r = S.gauge_r * ps } = {}) => {
    if (!T.gauge.enabled) return false;
    if (!Number.isFinite(lo) || !Number.isFinite(hi) || hi <= lo || !Number.isFinite(value)) return false;
    const half = Math.max(Math.abs(hi - nominal), Math.abs(nominal - lo));
    const full = Math.max(half * T.gauge.band_span, Math.abs(value - nominal) * 1.25, 1e-9);
    const ang = (v) => -Math.PI / 2 + Math.max(-1, Math.min(1, (v - nominal) / full)) * (Math.PI / 2);
    const ok = value >= lo && value <= hi;
    const c = { x: cx, y: cy };
    ctx.save();
    ctx.setLineDash([]);
    ctx.lineWidth = lw * S.thin_w;
    ctx.fillStyle = T.gauge.face;
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0); ctx.closePath(); ctx.fill();
    ctx.strokeStyle = withAlpha(T.role.neutral, 0.9);
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0); ctx.stroke();
    seg({ x: cx - r, y: cy }, { x: cx + r, y: cy });
    ctx.strokeStyle = withAlpha(T.role.ok, T.alpha.band); ctx.lineWidth = lw * 2.2;
    ctx.beginPath(); ctx.arc(cx, cy, r * 0.78, ang(lo), ang(hi)); ctx.stroke();
    ctx.strokeStyle = withAlpha(T.role.neutral, 1); ctx.lineWidth = lw * S.thin_w;
    seg(at(c, ang(nominal), r * 0.55), at(c, ang(nominal), r));
    ctx.strokeStyle = ok ? T.role.ok : T.role.ng; ctx.lineWidth = lw * S.heavy_w;
    seg(c, at(c, ang(value), r * 0.92));
    ctx.fillStyle = ok ? T.role.ok : T.role.ng;
    ctx.beginPath(); ctx.arc(cx, cy, ps * 0.6, 0, 2 * Math.PI); ctx.fill();
    ctx.restore();
    return true;
  };

  // True when a thing of this size (in def mm) is big enough on screen to be
  // worth labelling. Callers pass their own extent, so a small feature stays
  // clean while a large one in the same frame still gets its plate.
  const showDetail = (sizeMM) => !(sizeMM > 0) || (sizeMM / ps) > T.lod.detail_ps;

  // "[12] 外徑" -- the index the calc expression editor uses, in front of the
  // name the operator gave it.
  const measureName = (shape) => {
    const nm = (shape && shape.name) || '';
    if (!T.label.show_index || !shape || shape.id === undefined) return nm;
    return '[' + shape.id + '] ' + nm;
  };

  // A construction line: quiet, dotted, thin. Never competes with a dimension.
  const construction = (p, q, colour) => {
    ctx.save();
    ctx.strokeStyle = withAlpha(colour || T.role.region, T.alpha.faint);
    ctx.lineWidth = lw * S.thin_w;
    ctx.setLineDash(dash('tie'));
    seg(p, q);
    ctx.restore();
  };

  return { T, C: T.role, ps, lw, fpx, S, dash, at, seg, projOn, arrow, text, chip,
           datumMark, extendTo, gauge, withAlpha, showDetail, measureName, construction,
           // True while THIS shape is the one being edited.
           isEditing: (shape) => {
             const e = renderer.EditShape;
             return !!(e && e.id !== undefined && shape && e.id === shape.id);
           } };
}

export default overlayKit;
