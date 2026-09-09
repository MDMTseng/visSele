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
  const R = 9 * ps;            // ray length
  const rS = 5 * ps;           // sector radius
  const toRad = Math.PI / 180;
  const refA = aA + nominal * toRad;           // A's direction after the nominal
  const raw = wrap360((aB - refA) / toRad);    // B relative to that, (-180, 180]

  // WHAT EACH RANGE MEASURES, as a sector [s, e] from the A side to the B
  // side, plus whether ray heads matter (vector ranges) and any extra marker.
  //   vector heads: signed180 / deg360 read pt1->pt2 as a direction.
  //   comp: the sector between B and the perpendicular to A (90 - acute).
  //   supp: the sector between B and A's opposite end (180 - theta).
  let sDeg, eDeg, heads = false, marker = null, tag;
  switch (range) {
    case 'abs90':     { const d = wrap180(raw); sDeg = 0; eDeg = d; tag = '0~90'; break; }
    case 'deg180':    { sDeg = 0; eDeg = pos180(raw); tag = '0~180'; break; }
    case 'signed180': { sDeg = 0; eDeg = wrap360(raw); heads = true; tag = '±180'; break; }
    case 'deg360':    { sDeg = 0; eDeg = pos360(raw); heads = true; tag = '0~360'; break; }
    case 'supp':      { const t = pos180(raw); sDeg = t; eDeg = 180; tag = '補角'; break; }
    case 'comp':      { const d = wrap180(raw); const sg = Math.sign(d) || 1; sDeg = d; eDeg = sg * 90; marker = sg * 90; tag = '餘角'; break; }
    default:          { sDeg = 0; eDeg = wrap180(raw); tag = '±90'; break; }
  }
  const s0 = refA + sDeg * toRad, e0 = refA + eDeg * toRad;
  const ccw = eDeg < sDeg;                       // canvas arc direction

  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();

  // Ties from the label point to the two lines it compares.
  ctx.setLineDash([ps, ps]);
  ctx.strokeStyle = 'rgba(0,0,0,0.35)';
  const tA = closestPointOnPoints(P, [A0, A1]);
  const tB = closestPointOnPoints(P, [B0, B1]);
  renderer.drawReportLine(ctx, { x0: P.x, y0: P.y, x1: tA.x, y1: tA.y });
  renderer.drawReportLine(ctx, { x0: P.x, y0: P.y, x1: tB.x, y1: tB.y });

  // The two directions through P: A (after the nominal) dashed, B solid.
  // Full lines, not stubs, so two nearly parallel lines still read as two.
  const ray = (ang, len) => ({ x: P.x + len * Math.cos(ang), y: P.y + len * Math.sin(ang) });
  ctx.setLineDash([2 * ps, ps]);
  ctx.strokeStyle = 'rgba(30,60,200,0.9)';
  { const p1 = ray(refA, R), p2 = ray(refA + Math.PI, R);
    renderer.drawReportLine(ctx, { x0: p2.x, y0: p2.y, x1: p1.x, y1: p1.y }); }
  ctx.setLineDash([]);
  ctx.strokeStyle = 'rgba(200,60,30,0.9)';
  { const p1 = ray(aB, R), p2 = ray(aB + Math.PI, R);
    renderer.drawReportLine(ctx, { x0: p2.x, y0: p2.y, x1: p1.x, y1: p1.y }); }
  // Heads on the + ends when direction matters.
  if (heads) {
    const hl = 2.5 * ps;
    ctx.strokeStyle = ctx.fillStyle = 'rgba(30,60,200,0.9)';
    { const t = ray(refA, R), f = ray(refA, R - hl); renderer.canvas_arrow(ctx, f.x, f.y, t.x, t.y, hl); }
    ctx.strokeStyle = ctx.fillStyle = 'rgba(200,60,30,0.9)';
    { const t = ray(aB, R), f = ray(aB, R - hl); renderer.canvas_arrow(ctx, f.x, f.y, t.x, t.y, hl); }
  }

  // The measured sector, shaded, with an arrow at its B end. True geometry:
  // a 0.3 deg tilt is a sliver, which is honest; the exaggerated arrow below
  // says which way it leans.
  ctx.fillStyle = 'rgba(255,170,0,0.28)';
  ctx.beginPath();
  ctx.moveTo(P.x, P.y);
  ctx.arc(P.x, P.y, rS, s0, e0, ccw);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = ctx.fillStyle = 'rgba(200,110,0,1)';
  const span = Math.abs(eDeg - sDeg);
  if (span >= 8) {
    renderer.drawArcArrow(ctx, P.x, P.y, rS, s0, e0, ccw);
  } else {
    // Too thin to carry an arrow: draw the sense on a wider, dotted arc
    // (15 deg, marked as exaggerated) so the sign is still readable.
    const sg = Math.sign(eDeg - sDeg) || 1;
    ctx.setLineDash([ps * 0.6, ps * 0.6]);
    renderer.drawArcArrow(ctx, P.x, P.y, rS * 1.35, s0, s0 + sg * 15 * toRad, sg < 0);
    ctx.setLineDash([]);
  }
  // Right-angle marker for the complementary reading.
  if (marker !== null) {
    const m = marker * toRad, q = 1.6 * ps;
    const c1 = ray(refA + m, q), c2 = ray(refA, q);
    const c3 = { x: c1.x + c2.x - P.x, y: c1.y + c2.y - P.y };
    ctx.strokeStyle = 'rgba(30,60,200,0.9)';
    renderer.drawReportLine(ctx, { x0: c1.x, y0: c1.y, x1: c3.x, y1: c3.y });
    renderer.drawReportLine(ctx, { x0: c3.x, y0: c3.y, x1: c2.x, y1: c2.y });
    // and the perpendicular itself, dotted
    ctx.setLineDash([ps * 0.6, ps * 0.6]);
    const pp = ray(refA + m, R);
    renderer.drawReportLine(ctx, { x0: P.x, y0: P.y, x1: pp.x, y1: pp.y });
    ctx.setLineDash([]);
  }
  // Ray labels: A (with the nominal when it is not 0) and B.
  ctx.font = renderer.getFontStyle(renderer.getFontHeightPx() * 0.8);
  ctx.fillStyle = 'rgba(30,60,200,1)';
  { const t = ray(refA, R + 1.5 * ps); renderer.draw_Text(ctx, nominal ? `A${nominal > 0 ? '+' : ''}${nominal}º` : 'A', renderer.getFontHeightPx() * 0.8, t.x, t.y); }
  ctx.fillStyle = 'rgba(200,60,30,1)';
  { const t = ray(aB, R + 1.5 * ps); renderer.draw_Text(ctx, 'B', renderer.getFontHeightPx() * 0.8, t.x, t.y); }
  renderer.drawpoint(ctx, P);
  ctx.restore();

  measValueAdjStrTag = ' ' + tag;
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
