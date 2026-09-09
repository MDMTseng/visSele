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
  const { measValueAdjStr } = sctx;
  let measValueAdjStrTag = '';
  const aA = Math.atan2(A1.y - A0.y, A1.x - A0.x);
  const aB = Math.atan2(B1.y - B0.y, B1.x - B0.x);
  const nominal = shape.nominal_deg || 0;
  const range = shape.angle_range || 'signed90';
  const measureDeg = vectorAngleDeg(aA, aB, nominal, range);
  const P = shape.pt1;
  const ps = renderer.getPrimitiveSize();
  const toRad = Math.PI / 180;
  const refA = aA + nominal * toRad;
  const raw = wrap360((aB - refA) / toRad);
  const fpx = renderer.getFontHeightPx();

  // Geometry helpers on the two INFINITE lines.
  const proj = (Q, L0, L1) => { const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((Q.x - L0.x) * vx + (Q.y - L0.y) * vy) / n2; return { x: L0.x + t * vx, y: L0.y + t * vy }; };
  const tA = proj(P, A0, A1), tB = proj(P, B0, B1);
  const seg = (p, q) => renderer.drawReportLine(ctx, { x0: p.x, y0: p.y, x1: q.x, y1: q.y });
  const at = (c, ang, r) => ({ x: c.x + r * Math.cos(ang), y: c.y + r * Math.sin(ang) });
  const arrowHead = (tip, ang, len) => {   // filled head pointing along ang, tip at `tip`
    const f = at(tip, ang + Math.PI, len);
    renderer.canvas_arrow(ctx, f.x, f.y, tip.x, tip.y, len);
  };
  // draw_Text leaves ctx.lineWidth at a SCREEN-independent value (base size
  // x 0.013), which at high zoom is many image pixels: every line stroked
  // after a label came out as a thick band. Restore it around each label.
  const label = (text, x, y, scale = 1) => { const lw = ctx.lineWidth; renderer.draw_Text(ctx, text, fpx * scale, x, y); ctx.lineWidth = lw; };
  const fmtV = (v) => (v > 0 ? '+' : '') + v.toFixed(renderer.fixedDigit.A) + 'º';

  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.font = renderer.getFontStyle(1);
  ctx.setLineDash([]);

  // VIRTUAL EXTENSION LINES. The datum symbol and the leader's foot sit on
  // the infinite line, which can be well outside the part (CT's screenshot,
  // 2026-09-09: the foot on B was 10 mm below the segment, floating). A thin
  // dashed line from the nearest end of the real segment to the foot says
  // "this is that line, extended". A search-point line is a point + unit
  // vector, so its "segment" is that point: the extension runs from there.
  const extendTo = (foot, L0, L1, colour) => {
    const vx = L1.x - L0.x, vy = L1.y - L0.y, n2 = vx * vx + vy * vy || 1;
    const t = ((foot.x - L0.x) * vx + (foot.y - L0.y) * vy) / n2;
    if (t >= 0 && t <= 1) return;                       // foot lies on the segment
    const from = (t < 0) ? L0 : L1;
    ctx.save();
    ctx.strokeStyle = colour; ctx.setLineDash([2 * ps, 1.5 * ps]); ctx.lineWidth = 0.6 * renderer.getIndicationLineSize();
    const over = at(foot, Math.atan2(foot.y - from.y, foot.x - from.x), 3 * ps);   // a little past the foot
    ctx.beginPath(); ctx.moveTo(from.x, from.y); ctx.lineTo(over.x, over.y); ctx.stroke();
    ctx.restore();
  };
  extendTo(tA, A0, A1, 'rgba(30,60,200,0.8)');
  extendTo(tB, B0, B1, 'rgba(200,60,30,0.8)');

  const isGDT = (range === 'signed90');
  if (isGDT) {
    // ---- GD&T (ISO 1101 / ASME Y14.5): datum symbol on A, feature control
    // frame with a leader to B. Parallelism at nominal 0, perpendicularity at
    // 90, angularity otherwise. The value is an angle here, and signed; the
    // standard's band-width tolerance is not what this measure reports, so
    // the frame says degrees.
    const sym = (nominal === 0) ? '∥' : (Math.abs(nominal) === 90 ? '⊥' : '∠');
    measValueAdjStrTag = ' ' + sym;
    // Datum: filled triangle sitting on line A (base on the line, apex toward
    // the label point), leader to a boxed "A".
    const nrm = Math.atan2(P.y - tA.y, P.x - tA.x);       // from the line toward P
    const side = Number.isFinite(nrm) ? nrm : aA + Math.PI / 2;
    const h = 2.5 * ps, w = 1.6 * ps;
    const apex = at(tA, side, h), b1 = at(tA, aA, w), b2 = at(tA, aA + Math.PI, w);
    ctx.fillStyle = ctx.strokeStyle = 'rgba(30,60,200,1)';
    ctx.beginPath(); ctx.moveTo(b1.x, b1.y); ctx.lineTo(apex.x, apex.y); ctx.lineTo(b2.x, b2.y); ctx.closePath(); ctx.fill();
    const boxA = at(apex, side, 3 * ps), bs = 1.1 * fpx;
    seg(apex, boxA);
    ctx.strokeRect(boxA.x - bs / 2, boxA.y - bs / 2, bs, bs);
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    label('A', boxA.x, boxA.y, 0.9);
    // Feature control frame at the label point, leader with a head on line B.
    const cells = [sym, fmtV(measureDeg), 'A'];
    const cw = cells.map((c) => Math.max(1.2 * fpx, 0.62 * fpx * c.length + 0.5 * fpx));
    const fw = cw.reduce((x, y) => x + y, 0), fh = 1.3 * fpx;
    const fx = P.x - fw / 2, fy = P.y - 1.9 * fpx - fh;   // above the value text
    ctx.fillStyle = 'rgba(255,255,255,0.85)';
    ctx.fillRect(fx, fy, fw, fh);
    ctx.strokeStyle = 'rgba(200,60,30,1)'; ctx.fillStyle = 'rgba(200,60,30,1)';
    ctx.strokeRect(fx, fy, fw, fh);
    let cx = fx;
    cells.forEach((c, i) => { if (i) seg({ x: cx, y: fy }, { x: cx, y: fy + fh }); label(c, cx + cw[i] / 2, fy + fh / 2, 0.9); cx += cw[i]; });
    // leader: from the frame's nearest edge to the foot on B, arrowhead at B
    const from = { x: P.x, y: fy + fh };
    ctx.setLineDash([]);
    seg(from, tB);
    arrowHead(tB, Math.atan2(tB.y - from.y, tB.x - from.x), 2 * ps);
    // the sense of the tilt: a short arc at the foot, from A's direction
    // toward B's, exaggerated so a fraction of a degree still shows which way
    const sg = Math.sign(measureDeg) || 1;
    ctx.setLineDash([ps * 0.5, ps * 0.5]);
    renderer.drawArcArrow(ctx, tB.x, tB.y, 4 * ps, refA, refA + sg * 25 * toRad, sg < 0);
    ctx.setLineDash([]);
    ctx.textAlign = 'start'; ctx.textBaseline = 'alphabetic';
    label('B', tB.x + 1.5 * ps, tB.y - 1.5 * ps, 0.8);
  } else {
    // ---- Angular dimension (ISO 129-1): arc centred on the vertex, both
    // ends with arrowheads, extension lines along the two sides, value
    // horizontal. The label point sets the radius. When the two lines are
    // (nearly) parallel there is no usable vertex; the arc is then centred on
    // the label point itself.
    let sDeg, eDeg, heads = false, tag;
    switch (range) {
      case 'abs90':     { sDeg = 0; eDeg = wrap180(raw); tag = '0~90'; break; }
      case 'deg180':    { sDeg = 0; eDeg = pos180(raw); tag = '0~180'; break; }
      case 'signed180': { sDeg = 0; eDeg = wrap360(raw); heads = true; tag = '±180'; break; }
      case 'deg360':    { sDeg = 0; eDeg = pos360(raw); heads = true; tag = '0~360'; break; }
      case 'supp':      { sDeg = pos180(raw); eDeg = 180; tag = '補角'; break; }
      case 'comp':      { const d = wrap180(raw); const sg = Math.sign(d) || 1; sDeg = d; eDeg = sg * 90; tag = '餘角'; break; }
      default:          { sDeg = 0; eDeg = wrap180(raw); tag = '±90'; break; }
    }
    measValueAdjStrTag = ' ' + tag;
    let V = intersectPoint(A0, A1, B0, B1);
    const far = 400 * ps;
    if (!V || !Number.isFinite(V.x) || !Number.isFinite(V.y) || Math.hypot(V.x - P.x, V.y - P.y) > far) V = P;
    const r = Math.max(6 * ps, Math.hypot(P.x - V.x, P.y - V.y));
    const s0 = refA + sDeg * toRad, e0 = refA + eDeg * toRad, ccw = eDeg < sDeg;
    // extension lines: from each line's foot (nearest the arc end) out past the arc
    ctx.strokeStyle = 'rgba(0,0,0,0.6)';
    ctx.setLineDash([ps, ps]);
    for (const [ang, L0, L1, col] of [[s0, A0, A1, 'rgba(30,60,200,0.8)'], [e0, B0, B1, 'rgba(200,60,30,0.8)']]) {
      const end = at(V, ang, r + 2 * ps);
      const foot = proj(end, L0, L1);
      extendTo(foot, L0, L1, col);
      seg(foot, end);
    }
    ctx.setLineDash([]);
    // the dimension arc with a head at each end
    ctx.strokeStyle = ctx.fillStyle = 'rgba(200,110,0,1)';
    const span = Math.abs(eDeg - sDeg);
    ctx.beginPath(); ctx.arc(V.x, V.y, r, s0, e0, ccw); ctx.stroke();
    const hl = 2 * ps, dir = ccw ? -1 : 1;
    if (span >= 8) {
      arrowHead(at(V, e0, r), e0 + dir * Math.PI / 2, hl);
      arrowHead(at(V, s0, r), s0 - dir * Math.PI / 2, hl);
    } else {
      // too small for heads inside: heads outside, pointing in (ISO)
      arrowHead(at(V, e0, r), e0 - dir * Math.PI / 2, hl);
      arrowHead(at(V, s0, r), s0 + dir * Math.PI / 2, hl);
    }
    if (heads) {   // vector ranges: mark the + end of each line
      ctx.strokeStyle = ctx.fillStyle = 'rgba(30,60,200,1)';
      arrowHead(at(tA, refA, 6 * ps), refA, 2 * ps);
      ctx.strokeStyle = ctx.fillStyle = 'rgba(200,60,30,1)';
      arrowHead(at(tB, aB, 6 * ps), aB, 2 * ps);
    }
    if (range === 'comp') {   // right-angle marker at the perpendicular
      const q = 1.6 * ps, m = e0;
      const c1 = at(V, m, q), c2 = at(V, refA, q), c3 = { x: c1.x + c2.x - V.x, y: c1.y + c2.y - V.y };
      ctx.strokeStyle = 'rgba(30,60,200,1)';
      seg(c1, c3); seg(c3, c2);
    }
    // line names at their feet
    ctx.textAlign = 'start'; ctx.textBaseline = 'alphabetic';
    ctx.fillStyle = 'rgba(30,60,200,1)'; label(nominal ? `A${nominal > 0 ? '+' : ''}${nominal}º` : 'A', tA.x + 1.5 * ps, tA.y - 1.5 * ps, 0.8);
    ctx.fillStyle = 'rgba(200,60,30,1)'; label('B', tB.x + 1.5 * ps, tB.y - 1.5 * ps, 0.8);
  }
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
