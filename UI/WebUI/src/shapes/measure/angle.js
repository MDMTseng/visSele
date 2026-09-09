// Per-subtype draw module for measure.angle
// Extracted verbatim from measure/index.js — body unchanged. Signature:
//   draw(ctx, shape, subObjs, renderer, sctx) -> measureValue
// where sctx = { db_obj, shapeList, unitConvert, measValueAdjStr }.
// Receives subObjs (already resolved) so we don't duplicate the lookup.
// Returns measureValue (number) or undefined; the caller pushes to measureValueCache.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { threePointToArc, intersectPoint, LineCentralNormal, closestPointOnLine, closestPointOnPoints, distance_point_point } from 'UTIL/MathTools';
import dclone from 'clone';
import { mkLog } from "UTIL/logger";
const log = mkLog("editor.shapes");

// canvasCtrl: angle refs two lines or search_points (intersection).
export function availableRefShapes(shapeList) {
  return shapeList.filter((s) => s.type === 'line' || s.type === 'aux_line' || s.type === 'search_point');
}

// SIGNED MODE (parallelism / squareness): the rotation from line A (the
// reference) to line B, modulo 180, minus the nominal, wrapped into
// (-90, 90]. Same formula as the core's ANGLE judge (signed_mode branch);
// change both or neither. CCW-positive in the def frame (y-down canvas: the
// atan2 sign is the same one the core uses on the image, so they agree).
export const ANGLE_RANGES = [
  { key: 'signed90',  label: '±90 平行度',      hint: '線 B 相對線 A 的轉角,−90~+90,線不分頭尾。平行 = 0。' },
  { key: 'abs90',     label: '0~90 銳角',       hint: '兩線間的銳角,無正負。' },
  { key: 'deg180',    label: '0~180',           hint: 'A 逆時針轉到 B,0~180,線不分頭尾。' },
  { key: 'signed180', label: '±180 向量',       hint: '把線當有頭尾的向量(pt1→pt2),−180~+180。' },
  { key: 'deg360',    label: '0~360 向量',      hint: 'A 逆時針轉到 B,0~360,向量有頭尾。' },
  { key: 'supp',      label: '補角 180−θ',      hint: '180 減去 0~180 的角。' },
  { key: 'comp',      label: '餘角 90−θ',       hint: '90 減去銳角。' },
];
const wrap180 = (v) => { v = v % 180; if (v > 90) v -= 180; else if (v <= -90) v += 180; return v; };
const wrap360 = (v) => { v = v % 360; if (v > 180) v -= 360; else if (v <= -180) v += 360; return v; };
const pos180  = (v) => { v = v % 180; if (v < 0) v += 180; return v; };
const pos360  = (v) => { v = v % 360; if (v < 0) v += 360; return v; };
// Same seven readings as the core's ANGLE judge (signed_mode branch).
export function vectorAngleDeg(a1, a2, nominal, range) {
  const d = (a2 - a1) * 180 / Math.PI - (nominal || 0);
  switch (range) {
    case 'abs90':     return Math.abs(wrap180(d));
    case 'deg180':    return pos180(d);
    case 'signed180': return wrap360(d);
    case 'deg360':    return pos360(d);
    case 'supp':      return 180 - pos180(d);
    case 'comp':      return 90 - Math.abs(wrap180(d));
    default:          return wrap180(d);
  }
}
export function signedAngleDeg(a1, a2, nominal) { return vectorAngleDeg(a1, a2, nominal, 'signed90'); }

// The overlay for signed mode. There is no vertex to draw at (parallel lines
// have none), so everything anchors on the label point: a dashed stub along
// A's direction, a solid stub along B's, a small arc arrow between them whose
// direction IS the sign, and a faint tie from the label point to each line so
// the two lines being compared are unmistakable. The arc is drawn with a
// minimum opening (15 deg) because the real angle is usually well under 1 deg
// and would be invisible; the text carries the true value.
function drawSigned(ctx, shape, subObjs, renderer, sctx, A0, A1, B0, B1) {
  const { measValueAdjStr, unitConvert = { unit: 'mm', mult: 1 } } = sctx;
  let measValueAdjStrTag = '';
  const aA = Math.atan2(A1.y - A0.y, A1.x - A0.x);
  const aB = Math.atan2(B1.y - B0.y, B1.x - B0.x);
  const nominal = shape.nominal_deg || 0;
  const range = shape.angle_range || 'signed90';
  const measureDeg = vectorAngleDeg(aA, aB, nominal, range);
  const shownDeg = (shape.inspection_value !== undefined) ? shape.inspection_value : measureDeg;
  const P = shape.pt1;
  const ps = renderer.getPrimitiveSize();
  const toRad = Math.PI / 180;
  const refA = aA + nominal * toRad;          // the datum direction (A rotated by the nominal)
  const raw = wrap360((aB - refA) / toRad);
  const fpx = renderer.getFontHeightPx();
  const lwBase = renderer.getIndicationLineSize();

  const COL_A = 'rgba(30,90,220,1)';          // datum / line A
  const COL_B = 'rgba(215,70,40,1)';          // measured / line B
  const COL_V = 'rgba(230,140,0,1)';          // the quantity being read
  const COL_OK = 'rgba(40,160,90,1)';
  const COL_NG = 'rgba(220,50,50,1)';

  // Geometry helpers on the two INFINITE lines.
  const proj = (Q, L0, L1) => { const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((Q.x - L0.x) * vx + (Q.y - L0.y) * vy) / n2; return { x: L0.x + t * vx, y: L0.y + t * vy }; };
  const tA = proj(P, A0, A1), tB = proj(P, B0, B1);
  const seg = (p, q) => { ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(q.x, q.y); ctx.stroke(); };
  const at = (c, ang, r) => ({ x: c.x + r * Math.cos(ang), y: c.y + r * Math.sin(ang) });
  const arrowHead = (tip, ang, len) => { const f = at(tip, ang + Math.PI, len); renderer.canvas_arrow(ctx, f.x, f.y, tip.x, tip.y, len); };
  // draw_Text leaves ctx.lineWidth at a screen-size value; restore it or every
  // line stroked after a label comes out as a band.
  const label = (text, x, y, scale = 1) => { const lw = ctx.lineWidth; renderer.draw_Text(ctx, text, fpx * scale, x, y); ctx.lineWidth = lw; };
  const chip = (text, x, y, col, scale = 0.85) => {
    const w = 0.62 * fpx * scale * text.length + 0.7 * fpx * scale, h = 1.25 * fpx * scale;
    ctx.save(); ctx.fillStyle = 'rgba(255,255,255,0.88)';
    ctx.fillRect(x - w / 2, y - h / 2, w, h);
    ctx.strokeStyle = col; ctx.lineWidth = lwBase * 0.7; ctx.strokeRect(x - w / 2, y - h / 2, w, h);
    ctx.fillStyle = col; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    label(text, x, y, scale); ctx.restore();
  };
  const fmtV = (v) => (v > 0 ? '+' : '') + v.toFixed(renderer.fixedDigit.A) + 'º';
  const fmtL = (v) => (v * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit;

  ctx.save();
  ctx.lineWidth = lwBase;
  ctx.font = renderer.getFontStyle(1);
  ctx.setLineDash([]);

  // A virtual extension line says "this is that line, extended": the datum ray
  // and the feet often sit outside the real segment.
  const extendTo = (foot, L0, L1, colour) => {
    const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((foot.x - L0.x) * vx + (foot.y - L0.y) * vy) / n2;
    if (t >= 0 && t <= 1) return;
    const from = (t < 0) ? L0 : L1;
    ctx.save();
    ctx.strokeStyle = colour; ctx.setLineDash([2 * ps, 1.5 * ps]); ctx.lineWidth = 0.6 * lwBase;
    seg(from, at(foot, Math.atan2(foot.y - from.y, foot.x - from.x), 2 * ps));
    ctx.restore();
  };

  // The datum marker: filled triangle on line A with a boxed letter, ISO 1101.
  const datumMark = (anchor, dirAng, letter) => {
    const nrm = Math.atan2(P.y - anchor.y, P.x - anchor.x);
    const side = Number.isFinite(nrm) && Math.hypot(P.y - anchor.y, P.x - anchor.x) > 1e-6 ? nrm : dirAng + Math.PI / 2;
    const h = 2.2 * ps, w = 1.4 * ps;
    const apex = at(anchor, side, h), b1 = at(anchor, dirAng, w), b2 = at(anchor, dirAng + Math.PI, w);
    ctx.save();
    ctx.fillStyle = ctx.strokeStyle = COL_A; ctx.lineWidth = lwBase * 0.8;
    ctx.beginPath(); ctx.moveTo(b1.x, b1.y); ctx.lineTo(apex.x, apex.y); ctx.lineTo(b2.x, b2.y); ctx.closePath(); ctx.fill();
    const box = at(apex, side, 2.4 * ps);
    seg(apex, box);
    chip(letter, box.x, box.y, COL_A, 0.8);
    ctx.restore();
  };

  // The tilt gauge: how far the reading sits inside the tolerance band. This is
  // the part an operator reads at a glance -- the drawing above says WHAT is
  // measured, the gauge says HOW BAD. Only drawn when the def has limits.
  const gauge = (cx, cy, r) => {
    const nom = Number.isFinite(shape.value) ? shape.value : 0;
    const lo = shape.LSL, hi = shape.USL;
    if (!Number.isFinite(lo) || !Number.isFinite(hi) || hi <= lo) return;
    const half = Math.max(Math.abs(hi - nom), Math.abs(nom - lo));
    const full = Math.max(half * 1.8, Math.abs(shownDeg - nom) * 1.25, 1e-6);
    const ang = (v) => -Math.PI / 2 + Math.max(-1, Math.min(1, (v - nom) / full)) * (Math.PI / 2);
    const ok = shownDeg >= lo && shownDeg <= hi;
    ctx.save();
    ctx.lineWidth = lwBase * 0.8;
    ctx.fillStyle = 'rgba(255,255,255,0.85)';
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0); ctx.closePath(); ctx.fill();
    ctx.strokeStyle = 'rgba(90,90,90,0.9)';
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0); ctx.stroke();
    seg({ x: cx - r, y: cy }, { x: cx + r, y: cy });
    // tolerance band
    ctx.strokeStyle = 'rgba(40,160,90,0.85)'; ctx.lineWidth = lwBase * 2.2;
    ctx.beginPath(); ctx.arc(cx, cy, r * 0.78, ang(lo), ang(hi)); ctx.stroke();
    // nominal tick
    ctx.strokeStyle = 'rgba(60,60,60,0.9)'; ctx.lineWidth = lwBase * 0.8;
    seg(at({ x: cx, y: cy }, ang(nom), r * 0.55), at({ x: cx, y: cy }, ang(nom), r));
    // needle
    ctx.strokeStyle = ok ? COL_OK : COL_NG; ctx.lineWidth = lwBase * 1.4;
    seg({ x: cx, y: cy }, at({ x: cx, y: cy }, ang(shownDeg), r * 0.92));
    ctx.fillStyle = ok ? COL_OK : COL_NG;
    ctx.beginPath(); ctx.arc(cx, cy, ps * 0.6, 0, 2 * Math.PI); ctx.fill();
    ctx.restore();
  };

  // Where is the vertex, and is it usable? Nearly-parallel lines have none.
  let V = intersectPoint(A0, A1, B0, B1);
  const vertexOK = V && Number.isFinite(V.x) && Number.isFinite(V.y)
                   && Math.hypot(V.x - P.x, V.y - P.y) < 120 * ps;
  const needsVertex = (range === 'deg180' || range === 'signed180' || range === 'deg360' || range === 'supp');
  const gapStyle = !vertexOK || (!needsVertex && Math.abs(wrap180(raw)) < 25);

  const TAGS = { signed90: '±90', abs90: '0~90', deg180: '0~180', signed180: '±180', deg360: '0~360', supp: '補角', comp: '餘角' };
  measValueAdjStrTag = ' ' + (TAGS[range] || '±90');

  if (gapStyle) {
    // ---- GAP STYLE. What a fitter does with a height gauge: hold the datum,
    // measure the standoff at each end of the feature, and read the difference.
    // The wedge between the datum ray and B is filled, so which end opens is
    // visible even when the angle is a fraction of a degree; the two standoffs
    // are dimensioned so the reading is traceable to something measurable.
    const segLen = Math.hypot(B1.y - B0.y, B1.x - B0.x);
    const L = Math.max(14 * ps, Math.min(segLen / 2, 60 * ps));
    const Q1 = at(tB, aB + Math.PI, L), Q2 = at(tB, aB, L);
    // The datum ray runs under the span, anchored at the foot on A.
    const D1 = proj(Q1, tA, at(tA, refA, 1)), D2 = proj(Q2, tA, at(tA, refA, 1));
    extendTo(tA, A0, A1, 'rgba(30,90,220,0.75)');
    extendTo(tB, B0, B1, 'rgba(215,70,40,0.75)');
    // the wedge
    ctx.save();
    ctx.fillStyle = (shownDeg >= 0) ? 'rgba(230,140,0,0.16)' : 'rgba(120,80,220,0.16)';
    ctx.beginPath(); ctx.moveTo(D1.x, D1.y); ctx.lineTo(Q1.x, Q1.y); ctx.lineTo(Q2.x, Q2.y); ctx.lineTo(D2.x, D2.y); ctx.closePath(); ctx.fill();
    ctx.restore();
    // datum ray (dash-dot, the drawing convention for a reference)
    ctx.save();
    ctx.strokeStyle = COL_A; ctx.lineWidth = lwBase; ctx.setLineDash([4 * ps, 1.5 * ps, 0.8 * ps, 1.5 * ps]);
    seg(at(D1, refA + Math.PI, 3 * ps), at(D2, refA, 3 * ps));
    ctx.restore();
    // the measured line, solid over the same span
    ctx.save();
    ctx.strokeStyle = COL_B; ctx.lineWidth = lwBase * 1.3;
    seg(Q1, Q2);
    ctx.restore();
    // the two standoffs, with ticks and values
    ctx.save();
    ctx.strokeStyle = COL_V; ctx.fillStyle = COL_V; ctx.lineWidth = lwBase * 0.9;
    const gaps = [[Q1, D1, 'd1'], [Q2, D2, 'd2']];
    for (const [q, d, nm] of gaps) {
      seg(d, q);
      const dir = Math.atan2(q.y - d.y, q.x - d.x);
      if (Math.hypot(q.y - d.y, q.x - d.x) > 5 * ps) { arrowHead(q, dir, 1.6 * ps); arrowHead(d, dir + Math.PI, 1.6 * ps); }
      const mid = { x: (q.x + d.x) / 2, y: (q.y + d.y) / 2 };
      const off = at(mid, refA + (nm === 'd1' ? Math.PI : 0), 3.2 * ps);
      chip(fmtL(Math.hypot(q.y - d.y, q.x - d.x)), off.x, off.y, COL_V, 0.72);
    }
    ctx.restore();
    datumMark(tA, aA, nominal ? `A${nominal > 0 ? '+' : ''}${nominal}º` : 'A');
    ctx.save(); ctx.fillStyle = COL_B; ctx.textAlign = 'start'; ctx.textBaseline = 'alphabetic';
    chip('B', at(Q2, aB, 2.5 * ps).x, at(Q2, aB, 2.5 * ps).y, COL_B, 0.8);
    ctx.restore();
    // the reading itself, next to the wide end of the wedge
    const wide = (Math.hypot(Q2.y - D2.y, Q2.x - D2.x) >= Math.hypot(Q1.y - D1.y, Q1.x - D1.x)) ? Q2 : Q1;
    const dDelta = Math.hypot(Q2.y - D2.y, Q2.x - D2.x) - Math.hypot(Q1.y - D1.y, Q1.x - D1.x);
    chip(`${fmtV(shownDeg)}  Δ${fmtL(dDelta)}`, wide.x, at(wide, aB + Math.PI / 2, 4 * ps).y, COL_V, 0.85);
  } else {
    // ---- VERTEX STYLE (ISO 129-1 angular dimension): the two sides really do
    // meet on screen, so the classic arc with arrowheads is the clearest thing
    // to draw. The sector is filled so the swept side is unambiguous.
    let sDeg, eDeg, heads = false;
    switch (range) {
      case 'abs90':     { sDeg = 0; eDeg = wrap180(raw); break; }
      case 'deg180':    { sDeg = 0; eDeg = pos180(raw); break; }
      case 'signed180': { sDeg = 0; eDeg = wrap360(raw); heads = true; break; }
      case 'deg360':    { sDeg = 0; eDeg = pos360(raw); heads = true; break; }
      case 'supp':      { sDeg = pos180(raw); eDeg = 180; break; }
      case 'comp':      { const d = wrap180(raw); const sg = Math.sign(d) || 1; sDeg = d; eDeg = sg * 90; break; }
      default:          { sDeg = 0; eDeg = wrap180(raw); break; }
    }
    const r = Math.max(10 * ps, Math.min(Math.hypot(P.x - V.x, P.y - V.y), 70 * ps));
    const s0 = refA + sDeg * toRad, e0 = refA + eDeg * toRad, ccw = eDeg < sDeg;
    // sector fill
    ctx.save();
    ctx.fillStyle = 'rgba(230,140,0,0.14)';
    ctx.beginPath(); ctx.moveTo(V.x, V.y); ctx.arc(V.x, V.y, r, s0, e0, ccw); ctx.closePath(); ctx.fill();
    ctx.restore();
    // the two sides, each drawn out to the arc, with its extension shown
    for (const [ang, L0, L1, col] of [[s0, A0, A1, COL_A], [e0, B0, B1, COL_B]]) {
      const end = at(V, ang, r + 3 * ps), foot = proj(end, L0, L1);
      extendTo(foot, L0, L1, col.replace('1)', '0.75)'));
      ctx.save(); ctx.strokeStyle = col; ctx.lineWidth = lwBase; ctx.setLineDash([ps, ps]);
      seg(V, end); ctx.restore();
    }
    // dimension arc, heads outside when the span is too small to hold them
    ctx.save();
    ctx.strokeStyle = ctx.fillStyle = COL_V; ctx.lineWidth = lwBase;
    ctx.beginPath(); ctx.arc(V.x, V.y, r, s0, e0, ccw); ctx.stroke();
    const hl = 2 * ps, dir = ccw ? -1 : 1, inside = Math.abs(eDeg - sDeg) >= 8;
    arrowHead(at(V, e0, r), e0 + (inside ? dir : -dir) * Math.PI / 2, hl);
    arrowHead(at(V, s0, r), s0 - (inside ? dir : -dir) * Math.PI / 2, hl);
    ctx.restore();
    if (heads) {   // vector ranges: the head shows which way pt1->pt2 points
      ctx.save();
      ctx.strokeStyle = ctx.fillStyle = COL_A; arrowHead(at(tA, refA, 7 * ps), refA, 2 * ps);
      ctx.strokeStyle = ctx.fillStyle = COL_B; arrowHead(at(tB, aB, 7 * ps), aB, 2 * ps);
      ctx.restore();
    }
    if (range === 'comp') {   // the 90º the reading is taken from
      const q = 1.8 * ps;
      const c1 = at(V, e0, q), c2 = at(V, refA, q), c3 = { x: c1.x + c2.x - V.x, y: c1.y + c2.y - V.y };
      ctx.save(); ctx.strokeStyle = COL_A; seg(c1, c3); seg(c3, c2); ctx.restore();
    }
    datumMark(at(V, s0, r * 0.55), aA, nominal ? `A${nominal > 0 ? '+' : ''}${nominal}º` : 'A');
    const bAt = at(V, e0, r * 0.55);
    chip('B', bAt.x, bAt.y, COL_B, 0.8);
    // the reading on the arc's midpoint
    const mid = at(V, (s0 + e0) / 2 + (ccw && e0 > s0 ? Math.PI : 0), r + 3.2 * ps);
    chip(fmtV(shownDeg), mid.x, mid.y, COL_V, 0.85);
  }
  gauge(P.x, P.y - 8.5 * ps, 6 * ps);
  renderer.drawpoint(ctx, P);
  ctx.restore();

  const fontPx = renderer.getFontHeightPx();
  ctx.font = renderer.getFontStyle(1);
  ctx.save();
  ctx.translate(P.x, P.y);
  ctx.strokeStyle = "black";
  const fmt = (v) => (v > 0 ? '+' : '') + v.toFixed(renderer.fixedDigit.A) + 'º';
  let measureValue;
  if (shape.inspection_value !== undefined) {
    const iv = shape.inspection_value;
    const marginPC = (iv > shape.value)
      ? (iv - shape.value) / (shape.USL - shape.value)
      : -(iv - shape.value) / (shape.LSL - shape.value);
    renderer.drawInspMeasureInfoText(ctx, shape.name, fmt(iv), marginPC, fontPx);
    measureValue = iv;
  } else {
    renderer.drawDefMeasureInfoText(ctx, shape.name,
      fmt(shape.value),
      "L:" + fmt(shape.LSL) + " U:" + fmt(shape.USL),
      "Now:" + fmt(measureDeg) + measValueAdjStrTag + measValueAdjStr,
      fontPx);
    measureValue = measureDeg;
  }
  ctx.restore();
  return measureValue;
}

export function draw(ctx, shape, subObjs, renderer, sctx) {
  const { db_obj, shapeList, unitConvert, measValueAdjStr } = sctx;
  let measureValue;
                  let obj0_pt2=subObjs[0].pt2;

                  if(obj0_pt2===undefined)
                  {
                    
                    obj0_pt2= db_obj.shapeVectorParse(subObjs[0], shapeList);
                    obj0_pt2.x+=subObjs[0].pt1.x;
                    obj0_pt2.y+=subObjs[0].pt1.y;
                  }

                  let obj1_pt2=subObjs[1].pt2;
                  
                  if(obj1_pt2===undefined)
                  {
                    
                    obj1_pt2 = db_obj.shapeVectorParse(subObjs[1], shapeList);
                    obj1_pt2.x+=subObjs[1].pt1.x;
                    obj1_pt2.y+=subObjs[1].pt1.y;
                  }
                  //console.log(shape,subObjs,obj0_pt2,obj1_pt2);

                  
                  if (shape.angle_mode === 'signed')
                    return drawSigned(ctx, shape, subObjs, renderer, sctx,
                                      subObjs[0].pt1, obj0_pt2, subObjs[1].pt1, obj1_pt2);

                  let srcPt =
                    intersectPoint(subObjs[0].pt1, obj0_pt2, subObjs[1].pt1, obj1_pt2);
                  // Parallel lines have no vertex, so there is no angle to draw
                  // at one. Before intersectPoint guarded its denominator this
                  // arrived as Infinity and every atan2 below it drew garbage.
                  // Returning measureValue (still undefined here) is this
                  // function's documented "no value", and is what the caller
                  // already handles -- it pushes only defined values.
                  if (!Number.isFinite(srcPt.x) || !Number.isFinite(srcPt.y)) return measureValue;

                  ctx.lineWidth = renderer.getIndicationLineSize();
                  //ctx.strokeStyle=renderer.colorSet.measure_info; 

                  ///ctx.fillStyle=renderer.colorSet.measure_info; 
                  //renderer.drawpoint(ctx, srcPt,"cross");

                  let sAngle = Math.atan2(subObjs[0].pt1.y - srcPt.y, subObjs[0].pt1.x - srcPt.x);
                  let eAngle = Math.atan2(subObjs[1].pt1.y - srcPt.y, subObjs[1].pt1.x - srcPt.x);
                  //eAngle+=Math.PI;

                  let angleDiff = (eAngle - sAngle) % (2 * Math.PI);
                  if (angleDiff < 0) {
                    angleDiff += Math.PI * 2;
                  }
                  if (angleDiff > Math.PI) {
                    angleDiff -= Math.PI;
                  }


                  let quadrant = 0;

                  //if(shape.quadrant===undefined)
                  {

                    let midwayAngle = Math.atan2(shape.pt1.y - srcPt.y, shape.pt1.x - srcPt.x);//-PI~PI

                    let angleDiff_midway = (midwayAngle - sAngle) % (2 * Math.PI);
                    if (angleDiff_midway < 0) {
                      angleDiff_midway += Math.PI * 2;
                    }

                    if (angleDiff_midway < angleDiff) {
                      quadrant = 1;
                    }
                    else if (angleDiff_midway < Math.PI) {
                      quadrant = 2;
                    }
                    else if (angleDiff_midway < (Math.PI + angleDiff)) {
                      quadrant = 3;
                    }
                    else {
                      quadrant = 4;
                    }


                  }

                  {
                    shape.quadrant = quadrant;
                  }

                  let dist = Math.hypot(shape.pt1.y - srcPt.y, shape.pt1.x - srcPt.x);
                  let margin_deg = shape.margin * Math.PI / 180;
                  let draw_sAngle = sAngle, draw_eAngle = eAngle;
                  let ext_Angle1 = sAngle, ext_Angle2 = eAngle;
                  switch (quadrant % 4) {
                    case 1:
                      {

                      }
                      break;

                    case 2:
                      {
                        draw_sAngle += angleDiff;
                      }
                      break;
                    case 3:
                      {
                        draw_sAngle += Math.PI;

                      }
                      break;
                    case 0:
                      {
                        draw_sAngle = draw_sAngle + angleDiff + Math.PI;

                      }
                      break;
                  }
                  //log.debug(angleDiff*180/Math.PI,sAngle*180/Math.PI,eAngle*180/Math.PI);
                  if (quadrant % 2 == 0)//if our target quadrant is 2 or 4..., find the complement angle 
                  {
                    angleDiff = Math.PI - angleDiff;
                  }

                  draw_eAngle = draw_sAngle + angleDiff;

                  if (quadrant % 2 == 0) {
                    ext_Angle1 = draw_eAngle;
                    ext_Angle2 = draw_sAngle;
                  }
                  else {
                    ext_Angle1 = draw_sAngle;
                    ext_Angle2 = draw_eAngle;
                  }



                  renderer.drawArcArrow(ctx, srcPt.x, srcPt.y, dist, draw_sAngle, draw_eAngle);

                  renderer.drawpoint(ctx, shape.pt1);

                  let measureDeg = angleDiff * 180 / Math.PI;


                  {

                    ctx.lineWidth = renderer.getIndicationLineSize();
                    ctx.setLineDash([renderer.getPrimitiveSize(), 1*renderer.getPrimitiveSize()])

                    let arcPt={x:srcPt.x + dist * Math.cos(ext_Angle1),y:srcPt.y + dist * Math.sin(ext_Angle1)};
                    let closestPt=closestPointOnPoints(arcPt,[subObjs[0].pt1,obj0_pt2]);
                    renderer.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    arcPt={x:srcPt.x + dist * Math.cos(ext_Angle2),y:srcPt.y + dist * Math.sin(ext_Angle2)};
                    closestPt=closestPointOnPoints(arcPt,[subObjs[1].pt1,obj1_pt2]);
                    renderer.drawReportLine(ctx, {
                      x0: closestPt.x, y0: closestPt.y,
                      x1: arcPt.x, y1: arcPt.y
                    });

                    ctx.setLineDash([]);
                  }

                  let x = shape.pt1.x + (shape.pt1.x - srcPt.x) / dist * 4 * renderer.getPrimitiveSize();
                  let y = shape.pt1.y + (shape.pt1.y - srcPt.y) / dist * 4 * renderer.getPrimitiveSize();



                  let fontPx = renderer.getFontHeightPx();
                  ctx.font = renderer.getFontStyle(1);


                  ctx.save();
                  ctx.translate(shape.pt1.x, shape.pt1.y);
                  
                  ctx.strokeStyle = "black";
                  if (shape.inspection_value !== undefined) {
                    
                    let marginPC = (shape.inspection_value > shape.value) ?
                      (shape.inspection_value - shape.value) / (shape.USL - shape.value) :
                      -(shape.inspection_value - shape.value) / (shape.LSL - shape.value);
                    renderer.drawInspMeasureInfoText(ctx,
                      shape.name,
                      (shape.inspection_value).toFixed(renderer.fixedDigit.A) + "º",
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      shape.name,
                      ""+shape.value.toFixed(renderer.fixedDigit.A) + "º",
                      "L:" + shape.LSL.toFixed(renderer.fixedDigit.A) + "º U:" + shape.USL.toFixed(renderer.fixedDigit.A) + "º",
                      "Now:" + (measureDeg).toFixed(renderer.fixedDigit.A) + "º" + measValueAdjStr,
                      fontPx)
                    
                    measureValue=measureDeg;
                  }
                  ctx.restore();
  return measureValue;
}
