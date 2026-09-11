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
import { overlayKit, OVERLAY, measureLabelName } from 'JSSRCROOT/canvas/overlayKit';
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

  const K = overlayKit(ctx, renderer);

  const TAGS = { signed90: '±90', abs90: '0~90', deg180: '0~180', signed180: '±180', deg360: '0~360', supp: '補角', comp: '餘角' };
  const measValueAdjStrTag = ' ' + (TAGS[range] || '±90');

  // This is the classic angle overlay -- the same arc-with-one-arrowhead the
  // quadrant mode has always drawn, in the caller's colour -- with only what
  // the vector mode actually needs added on top:
  //   * the sweep starts at the DATUM direction (A + nominal), so the drawn
  //     arc is the reading, and the arrowhead's direction IS the sign;
  //   * each range sweeps its own span, so 補角/餘角 draw the angle they
  //     report rather than the raw one;
  //   * parallel lines have no vertex, so the arc then centres on the label
  //     point instead of vanishing to infinity.
  let sDeg, eDeg;
  switch (range) {
    case 'abs90':     { sDeg = 0; eDeg = wrap180(raw); break; }
    case 'deg180':    { sDeg = 0; eDeg = pos180(raw); break; }
    case 'signed180': { sDeg = 0; eDeg = wrap360(raw); break; }
    case 'deg360':    { sDeg = 0; eDeg = pos360(raw); break; }
    case 'supp':      { sDeg = pos180(raw); eDeg = 180; break; }
    case 'comp':      { const d = wrap180(raw); const sg = Math.sign(d) || 1; sDeg = d; eDeg = sg * 90; break; }
    default:          { sDeg = 0; eDeg = wrap180(raw); break; }
  }

  // A MIRRORED PART REPORTS THE OPPOSITE SIGN TO THE ONE ON SCREEN.
  //
  // The core measures in the OBJECT's frame, where the part is not mirrored;
  // this file measures the lines where they are drawn, in the image. A mirror
  // reverses the sense of rotation, so for a part detected as 反 the two
  // disagree in sign while agreeing exactly in magnitude. Measured on a real
  // frame: aA 161.83 deg, aB 168.21 deg -- a +6.38 deg turn on screen -- with
  // the core reporting -6.38, and viewFlip false, so the canvas really is
  // showing the unmirrored image.
  //
  // Neither number is wrong, and the fix is NOT to sweep the reported sign from
  // the datum: that draws the other wedge entirely, away from line B, and the
  // arc then ends nowhere near the line it is supposed to reach.
  //
  // The wedge between two lines is the same set of points whichever end you
  // start from -- only the direction, and therefore the arrowhead, changes. So
  // when the reading's sign disagrees with the drawn turn, the two ends are
  // SWAPPED: the same wedge, traversed from B back to A. The arc covers exactly
  // what it did before, and the arrowhead now points the way the printed number
  // says. The magnitudes must match for this to be the mirror case and not some
  // unrelated disagreement, which is what the tolerance below checks.
  {
    const reported = shape.inspection_value;
    const turn = eDeg - sDeg;
    if (Number.isFinite(reported) && Math.abs(turn) > 1e-9
        && Math.sign(reported) !== Math.sign(turn)
        && Math.abs(Math.abs(reported) - Math.abs(turn)) < 0.5) {
      const t = sDeg; sDeg = eDeg; eDeg = t;
    }
  }

  // WHERE THE ANGLE IS DRAWN.
  //
  // With a usable vertex the arc goes on it, and each side's extension runs
  // ALONG ITS OWN LINE from the end of the real segment, through the vertex,
  // out past the arc -- which is what an extension line means.
  //
  // Near-parallel lines have no vertex on screen. The old fallback centred the
  // arc on the label point with a minimum radius, which made the arc a dot and
  // turned the two extensions into a V pointing at the label -- neither line
  // extended, nothing to read. So the label point becomes a LOCAL vertex: the
  // two directions are drawn as rays from it at a fixed radius, each tied back
  // to its own line by a dotted line from the foot of the perpendicular. The
  // rays are the lines' directions, the arc between them is the reading.
  // A vertex is only worth drawing on when the angle is actually READ there.
  // The construction is always the same one, and it is the one a person draws
  // by hand: continue each line, dotted and collinear, past the end of the
  // real segment until the two meet; put the arc on that vertex; run a leader
  // from the arc to the label. The label's distance from the vertex IS the
  // radius, so where the operator parks it decides how big the arc is.
  //
  // There is deliberately no second style. A local arc drawn near the label,
  // tied to the lines by perpendiculars, was tried and is wrong: those
  // perpendicular ties are not extension lines, they cross the lines instead
  // of continuing them, and nothing on the canvas then says where the angle
  // actually is.
  let V = intersectPoint(A0, A1, B0, B1);
  const vOK = V && Number.isFinite(V.x) && Number.isFinite(V.y)
              && Math.hypot(V.x - P.x, V.y - P.y) <= OVERLAY.angle.vertex_max_ps * ps;
  const dist = vOK ? Math.max(Math.hypot(P.x - V.x, P.y - V.y), 10 * ps) : 0;
  let s0 = refA + sDeg * toRad, e0 = refA + eDeg * toRad;

  // WHICH SIDE OF THE VERTEX. Vertical angles are equal, so the same reading
  // can be drawn on either side -- and one of them is the side the part and
  // the label are on. Drawn on the far side the whole construction walks off
  // into empty image, which is what a near-parallel pair did: the vertex is a
  // long way out, and the arc opened away from everything worth looking at.
  //
  // Both rays flip together, so the angle between them -- the reading -- is
  // untouched; only the side changes.
  if (vOK) {
    const toLabel = Math.atan2(P.y - V.y, P.x - V.x);
    const mid = (s0 + e0) / 2;
    if (Math.cos(toLabel - mid) < 0) { s0 += Math.PI; e0 += Math.PI; }
  }

  // A fraction of a degree is invisible as an arc. Below min_draw_deg the arc
  // is opened out to that much so the direction still reads; the text carries
  // the true value.
  const minSpan = (OVERLAY.angle.min_draw_deg || 0) * toRad;
  let e0d = e0;
  if (Math.abs(e0 - s0) < minSpan) e0d = s0 + Math.sign(e0 - s0 || 1) * minSpan;

  // Quiet construction first, then the dashed red arc with one arrowhead. The
  // extensions are deliberately much lighter -- the arc has to be the only
  // thing on the canvas that reads as "this is the number".
  // (mech1_ref/OVERLAY_DESIGN.md 3.3)
  ctx.save();
  if (vOK) {
    // AN EXTENSION LINE IS THE LINE, CONTINUED -- so it is built from the
    // line's own direction and from nothing else. Deriving it from the arc's
    // angles is what bent it: s0/e0d have been flipped to the near side and
    // opened out to min_draw_deg, so by then they are no longer the lines'
    // directions and the "extensions" left the lines at an angle.
    for (const [ang, L0, L1] of [[s0, A0, A1], [e0d, B0, B1]]) {
      // Work in the line's own parameter, so the extension covers whatever it
      // has to and cannot leave the line. Both the vertex and this side's end
      // of the arc lie ON this line; the extension is simply the run from the
      // real segment out to whichever of them is furthest, DRAWN ON BOTH SIDES
      // -- the operator can drag the label past the far end of the segment,
      // and then the arc needs the extension going the other way.
      const ux = Math.cos(Math.atan2(L1.y - L0.y, L1.x - L0.x));
      const uy = Math.sin(Math.atan2(L1.y - L0.y, L1.x - L0.x));
      const t = (q) => (q.x - L0.x) * ux + (q.y - L0.y) * uy;
      const at_t = (tt) => ({ x: L0.x + tt * ux, y: L0.y + tt * uy });
      const segLo = Math.min(t(L0), t(L1)), segHi = Math.max(t(L0), t(L1));
      const E = { x: V.x + (dist + 3 * ps) * Math.cos(ang),
                  y: V.y + (dist + 3 * ps) * Math.sin(ang) };
      const lo = Math.min(segLo, t(V), t(E)), hi = Math.max(segHi, t(V), t(E));
      if (lo < segLo - 1e-9) K.construction(at_t(lo), at_t(segLo));
      if (hi > segHi + 1e-9) K.construction(at_t(segHi), at_t(hi));
    }
  } else {
    // Truly parallel (or a vertex so far out that drawing to it is nonsense).
    // There is no angle to draw, so draw none -- just say which two lines the
    // number came from, and let the text carry it.
    K.construction(K.projOn(P, A0, A1), P);
    K.construction(K.projOn(P, B0, B1), P);
  }
  // THE LEAD-OUT ARC (from WebUI2's _Draw_FeatureElement_Edit_Measure_Angle).
  //
  // The label point sets the arc's radius, but the operator is free to park it
  // anywhere -- including well outside the angle being swept, which is the
  // normal case for a small angle. Then the arc and the number it belongs to
  // sit apart with nothing joining them. A lighter arc continued from whichever
  // end is nearer, at the same radius, out to the label's own bearing, says
  // "this number belongs to that arc" without adding a second thing that reads
  // like a measurement.
  const TWO_PI = Math.PI * 2;
  const norm = (a) => { a = a % TWO_PI; return a < 0 ? a + TWO_PI : a; };
  let labelTheta = NaN;
  if (vOK) {
    labelTheta = Math.atan2(P.y - V.y, P.x - V.x);
    const fromStart = norm(labelTheta - s0);
    const swept = norm(e0d - s0);
    const inside = (e0d >= s0) ? (fromStart <= swept) : (norm(s0 - labelTheta) <= norm(s0 - e0d));
    if (!inside) {
      const gapEnd = norm((e0d >= s0) ? labelTheta - e0d : e0d - labelTheta);
      const gapStart = norm((e0d >= s0) ? s0 - labelTheta : labelTheta - s0);
      const gap = Math.min(gapEnd, gapStart);
      ctx.save();
      ctx.strokeStyle = K.withAlpha(K.C.reading, OVERLAY.alpha.faint);
      ctx.lineWidth = K.lw * K.S.thin_w;
      ctx.setLineDash(K.dash('tie'));
      // Continue the arc only while the label is just past its end. Parked
      // right round the other side -- which is where it ends up when the
      // vertex is far off the part -- an arc all the way there sweeps half the
      // screen and reads as a measurement of something. Past that, a straight
      // leader, which is what a drawing uses and what nobody can misread.
      // BOTH caps have to pass. The angular one keeps the connector from
      // wrapping round the circle; the length one keeps it from being a
      // thousand pixels of curve when the vertex is far and the radius is
      // therefore huge. `gap * dist` is the arc length this would draw.
      const leadLenOK = (gap * dist) <= OVERLAY.angle.lead_arc_max_len_ps * ps;
      if (gap <= OVERLAY.angle.lead_arc_max_deg * toRad && leadLenOK) {
        // What the canvas will ACTUALLY sweep, by its own rule: anticlockwise
        // false sweeps start->end increasing, true sweeps decreasing. If this
        // does not equal `gap` then the arc goes the long way round, which is
        // the whole complaint -- so it is computed rather than assumed.
        const _aS = (gapEnd <= gapStart) ? e0d : labelTheta;
        const _aE = (gapEnd <= gapStart) ? labelTheta : s0;
        const _ccw = e0d < s0;
        const _sweep = _ccw ? norm(_aS - _aE) : norm(_aE - _aS);
        if (typeof window !== 'undefined' && window.__ANGLE_DEBUG__) {
          const D = (v) => (typeof v === 'number' ? +v.toFixed(2) : v);
          const deg = (r) => D(r * 180 / Math.PI);
          // eslint-disable-next-line no-console
          console.log('[angle lead]', measureLabelName(shape), {
            branch: (gapEnd <= gapStart) ? 'end->label' : 'label->start',
            gap_deg: deg(gap), gapEnd_deg: deg(gapEnd), gapStart_deg: deg(gapStart),
            ccw: _ccw, s0_deg: deg(s0), e0d_deg: deg(e0d), labelTheta_deg: deg(labelTheta),
            actual_sweep_deg: deg(_sweep),
            wrong_way: Math.abs(_sweep - gap) > 1e-6,
            arc_len_ps: D(_sweep * dist / ps), chord_len_ps: D(2 * dist * Math.sin(gap / 2) / ps),
            cap_ps: OVERLAY.angle.lead_arc_max_len_ps, dist_ps: D(dist / ps),
          });
        }
        ctx.beginPath();
        ctx.arc(V.x, V.y, dist, _aS, _aE, _ccw);
        ctx.stroke();
      } else {
        const from = (gapEnd <= gapStart) ? e0d : s0;
        K.seg({ x: V.x + dist * Math.cos(from), y: V.y + dist * Math.sin(from) }, P);
      }
      ctx.restore();
    }
  }

  if (vOK) {
    ctx.strokeStyle = ctx.fillStyle = K.C.reading;
    ctx.lineWidth = K.lw * K.S.line_w;
    ctx.setLineDash(K.dash('meas'));
    renderer.drawArcArrow(ctx, V.x, V.y, dist, s0, e0d, e0d < s0);
    ctx.setLineDash([]);
  }

  // TEMPORARY DIAGNOSTIC -- turn on in the console with:
  //     window.__ANGLE_DEBUG__ = true
  // and off again by deleting it. Off by default and costs one property read
  // per draw when off, which matters because this runs per shape per frame.
  //
  // Here because three plausible explanations for a wrong angle overlay were
  // argued from screenshots and each was contradicted by the next clue. These
  // are the numbers that decide it: what the core said, what this file
  // computed, which range it used, and what it therefore swept.
  if (typeof window !== 'undefined' && window.__ANGLE_DEBUG__) {
    const D = (v) => (typeof v === 'number' ? +v.toFixed(2) : v);
    // eslint-disable-next-line no-console
    console.log('[angle]', measureLabelName(shape), {
      range,
      core_value: D(shape.inspection_value),   // what the label prints
      js_measureDeg: D(measureDeg),            // what this file measured
      raw: D(raw), nominal: D(nominal),
      aA_deg: D(aA * 180 / Math.PI), aB_deg: D(aB * 180 / Math.PI),
      sweep_deg: D((e0d - s0) * 180 / Math.PI),   // what the arc ACTUALLY draws
      sDeg: D(sDeg), eDeg: D(eDeg),
      vOK, vertex_dist_ps: vOK ? D(Math.hypot(V.x - P.x, V.y - P.y) / ps) : null,
      radius_ps: D(dist / ps),
      viewFlip: !!renderer.viewFlip, viewRotation: D(renderer.viewRotation || 0),
    });
  }
  renderer.drawpoint(ctx, P);
  ctx.restore();

  const fontPx = renderer.getFontHeightPx();
  ctx.font = renderer.getFontStyle(1);
  ctx.save();
  ctx.translate(P.x, P.y);
  // Lay the text along the radius it hangs off, the way a drawing does -- and
  // flip it end-for-end when that would put it upside down, so it is always
  // read left-to-right. draw_Text cancels the VIEW rotation to keep text
  // upright, so the view's own rotation is added back here; without that the
  // label would follow the radius in image space and not on screen.
  if (OVERLAY.angle.label_follows_radius && Number.isFinite(labelTheta) && !renderer.viewFlip) {
    let th = labelTheta + (renderer.viewRotation || 0);
    th = Math.atan2(Math.sin(th), Math.cos(th));
    if (th > Math.PI / 2 || th < -Math.PI / 2) th += Math.PI;
    ctx.rotate(th);
  }
  ctx.strokeStyle = "black";
  const fmt = (v) => (v > 0 ? '+' : '') + v.toFixed(renderer.fixedDigit.A) + 'º';
  let measureValue;
  if (shape.inspection_value !== undefined) {
    const iv = shape.inspection_value;
    const marginPC = (iv > shape.value)
      ? (iv - shape.value) / (shape.USL - shape.value)
      : -(iv - shape.value) / (shape.LSL - shape.value);
    renderer.drawInspMeasureInfoText(ctx, measureLabelName(shape), fmt(iv), marginPC, fontPx);
    measureValue = iv;
  } else {
    renderer.drawDefMeasureInfoText(ctx, measureLabelName(shape),
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
                      measureLabelName(shape),
                      (shape.inspection_value).toFixed(renderer.fixedDigit.A) + "º",
                      marginPC,fontPx);
                    measureValue=shape.inspection_value;
                  }
                  else {
            
                    
                    renderer.drawDefMeasureInfoText(ctx,
                      measureLabelName(shape),
                      ""+shape.value.toFixed(renderer.fixedDigit.A) + "º",
                      "L:" + shape.LSL.toFixed(renderer.fixedDigit.A) + "º U:" + shape.USL.toFixed(renderer.fixedDigit.A) + "º",
                      "Now:" + (measureDeg).toFixed(renderer.fixedDigit.A) + "º" + measValueAdjStr,
                      fontPx)
                    
                    measureValue=measureDeg;
                  }
                  ctx.restore();
  return measureValue;
}
