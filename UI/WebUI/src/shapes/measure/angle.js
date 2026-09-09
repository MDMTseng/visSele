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
import { overlayKit, OVERLAY } from 'JSSRCROOT/canvas/overlayKit';
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
  const toRad = Math.PI / 180;
  const refA = aA + nominal * toRad;          // the datum direction (A rotated by the nominal)
  const raw = wrap360((aB - refA) / toRad);

  // Every colour, dash and size below comes from canvas/overlayKit -- tune it
  // there (or live, via OVERLAY_TUNE in the console), never here.
  const K = overlayKit(ctx, renderer);
  const { C, ps, lw, S, dash, at, seg, projOn, arrow, chip, datumMark, extendTo, gauge, withAlpha } = K;
  const A = OVERLAY.angle;

  const tA = projOn(P, A0, A1), tB = projOn(P, B0, B1);
  const fmtV = (v) => (v > 0 ? '+' : '') + v.toFixed(renderer.fixedDigit.A) + 'º';
  const fmtL = (v) => (v * unitConvert.mult).toFixed(renderer.fixedDigit.R) + unitConvert.unit;
  const datumName = nominal ? `A${nominal > 0 ? '+' : ''}${nominal}º` : 'A';

  ctx.save();
  ctx.lineWidth = lw * S.line_w;
  ctx.font = renderer.getFontStyle(1);
  ctx.setLineDash([]);

  // Where is the vertex, and is it usable? Nearly-parallel lines have none.
  let V = intersectPoint(A0, A1, B0, B1);
  const vertexOK = V && Number.isFinite(V.x) && Number.isFinite(V.y)
                   && Math.hypot(V.x - P.x, V.y - P.y) < A.vertex_max_ps * ps;
  const needsVertex = (range === 'deg180' || range === 'signed180' || range === 'deg360' || range === 'supp');
  const gapStyle = !vertexOK || (!needsVertex && Math.abs(wrap180(raw)) < A.gap_style_max_deg);

  const TAGS = { signed90: '±90', abs90: '0~90', deg180: '0~180', signed180: '±180', deg360: '0~360', supp: '補角', comp: '餘角' };
  measValueAdjStrTag = ' ' + (TAGS[range] || '±90');

  if (gapStyle) {
    // ---- GAP STYLE. What a fitter does with a height gauge: hold the datum,
    // measure the standoff at each end of the feature, and read the difference.
    // The wedge between the datum ray and B is filled, so which end opens is
    // visible even when the angle is a fraction of a degree; the two standoffs
    // are dimensioned so the reading is traceable to something measurable.
    const segLen = Math.hypot(B1.y - B0.y, B1.x - B0.x);
    const L = Math.max(S.span_min * ps, Math.min(segLen / 2, S.span_max * ps));
    const Q1 = at(tB, aB + Math.PI, L), Q2 = at(tB, aB, L);
    const D1 = projOn(Q1, tA, at(tA, refA, 1)), D2 = projOn(Q2, tA, at(tA, refA, 1));
    extendTo(tA, A0, A1, withAlpha(C.datum, 0.75));
    extendTo(tB, B0, B1, withAlpha(C.feature, 0.75));
    // the wedge: hue carries the sign
    ctx.save();
    ctx.fillStyle = withAlpha(shownDeg >= 0 ? C.reading : A.wedge_neg, OVERLAY.alpha.wedge);
    ctx.beginPath(); ctx.moveTo(D1.x, D1.y); ctx.lineTo(Q1.x, Q1.y); ctx.lineTo(Q2.x, Q2.y); ctx.lineTo(D2.x, D2.y); ctx.closePath(); ctx.fill();
    ctx.restore();
    // datum ray, dash-dot
    ctx.save();
    ctx.strokeStyle = C.datum; ctx.lineWidth = lw * S.line_w; ctx.setLineDash(dash('datum'));
    seg(at(D1, refA + Math.PI, 3 * ps), at(D2, refA, 3 * ps));
    ctx.restore();
    // the measured line over the same span
    ctx.save();
    ctx.strokeStyle = C.feature; ctx.lineWidth = lw * S.heavy_w;
    seg(Q1, Q2);
    ctx.restore();
    // the two standoffs, with ticks and values
    ctx.save();
    ctx.strokeStyle = ctx.fillStyle = C.reading; ctx.lineWidth = lw * S.line_w * 0.9;
    for (const [q, d, first] of [[Q1, D1, true], [Q2, D2, false]]) {
      seg(d, q);
      const dir = Math.atan2(q.y - d.y, q.x - d.x);
      if (Math.hypot(q.y - d.y, q.x - d.x) > 5 * ps) { arrow(q, dir, S.tick * ps); arrow(d, dir + Math.PI, S.tick * ps); }
      const mid = { x: (q.x + d.x) / 2, y: (q.y + d.y) / 2 };
      const off = at(mid, refA + (first ? Math.PI : 0), S.chip_gap * ps);
      chip(fmtL(Math.hypot(q.y - d.y, q.x - d.x)), off.x, off.y, C.reading, OVERLAY.font.small);
    }
    ctx.restore();
    datumMark(tA, aA, datumName, P);
    const bTag = at(Q2, aB, 2.5 * ps);
    chip('B', bTag.x, bTag.y, C.feature, OVERLAY.font.tag);
    // the reading, next to the wide end of the wedge
    const g1 = Math.hypot(Q1.y - D1.y, Q1.x - D1.x), g2 = Math.hypot(Q2.y - D2.y, Q2.x - D2.x);
    const wide = (g2 >= g1) ? Q2 : Q1;
    chip(`${fmtV(shownDeg)}  Δ${fmtL(g2 - g1)}`, wide.x, at(wide, aB + Math.PI / 2, 4 * ps).y, C.reading);
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
    ctx.save();
    ctx.fillStyle = withAlpha(C.reading, OVERLAY.alpha.sector);
    ctx.beginPath(); ctx.moveTo(V.x, V.y); ctx.arc(V.x, V.y, r, s0, e0, ccw); ctx.closePath(); ctx.fill();
    ctx.restore();
    // the two sides, each drawn out to the arc, with its extension shown
    for (const [ang, L0, L1, col] of [[s0, A0, A1, C.datum], [e0, B0, B1, C.feature]]) {
      const end = at(V, ang, r + 3 * ps), foot = projOn(end, L0, L1);
      extendTo(foot, L0, L1, withAlpha(col, 0.75));
      ctx.save(); ctx.strokeStyle = col; ctx.lineWidth = lw * S.line_w; ctx.setLineDash(dash('aux'));
      seg(V, end); ctx.restore();
    }
    // dimension arc, heads outside when the span is too small to hold them
    ctx.save();
    ctx.strokeStyle = ctx.fillStyle = C.reading; ctx.lineWidth = lw * S.line_w;
    ctx.beginPath(); ctx.arc(V.x, V.y, r, s0, e0, ccw); ctx.stroke();
    const hl = S.arrow_head * ps, dir = ccw ? -1 : 1;
    const inside = Math.abs(eDeg - sDeg) >= A.head_inside_min_deg;
    arrow(at(V, e0, r), e0 + (inside ? dir : -dir) * Math.PI / 2, hl);
    arrow(at(V, s0, r), s0 - (inside ? dir : -dir) * Math.PI / 2, hl);
    ctx.restore();
    if (heads) {   // vector ranges: the head shows which way pt1->pt2 points
      ctx.save();
      ctx.strokeStyle = ctx.fillStyle = C.datum; arrow(at(tA, refA, 7 * ps), refA, S.arrow_head * ps);
      ctx.strokeStyle = ctx.fillStyle = C.feature; arrow(at(tB, aB, 7 * ps), aB, S.arrow_head * ps);
      ctx.restore();
    }
    if (range === 'comp') {   // the 90º the reading is taken from
      const q = 1.8 * ps;
      const c1 = at(V, e0, q), c2 = at(V, refA, q), c3 = { x: c1.x + c2.x - V.x, y: c1.y + c2.y - V.y };
      ctx.save(); ctx.setLineDash([]); ctx.strokeStyle = C.datum; seg(c1, c3); seg(c3, c2); ctx.restore();
    }
    datumMark(at(V, s0, r * 0.55), aA, datumName, P);
    const bAt = at(V, e0, r * 0.55);
    chip('B', bAt.x, bAt.y, C.feature, OVERLAY.font.tag);
    const mid = at(V, (s0 + e0) / 2 + (ccw && e0 > s0 ? Math.PI : 0), r + S.chip_gap * ps);
    chip(fmtV(shownDeg), mid.x, mid.y, C.reading);
  }
  gauge(P.x, P.y - S.gauge_dy * ps, shownDeg,
        { nominal: shape.value, lo: shape.LSL, hi: shape.USL });
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
