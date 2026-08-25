// Shared `caliper` + `edge` field declarations for primitives that support
// caliper/section locating mode (line, arc, search_point). Wire-format
// matches the core parser in MatchingEngine/FeatureManager_sig360_circle_line.cpp
// (search for JFetch on "caliper"/"edge").
//
//   shape.locating = "contour" | "caliper"
//   shape.caliper  = { count, width, length, step }     // line+arc only
//   shape.edge     = { method, polarity, nth, min_strength }
//
// Both sub-objects are HIDDEN in the property sheet when `locating != "caliper"`
// (editor function returns undefined → buildWhiteListKeyFromFields skips it).
// They are LAZY-INITIALIZED via `derive` when locating flips to "caliper" so
// the user immediately sees the core's defaults. Tweaks survive a round-trip
// through contour mode because we don't strip the sub-objects on toggle-off.

export const EDGE_METHODS    = ['strongest', 'first', 'last', 'middle', 'nth'];
export const EDGE_POLARITIES = ['any', 'rising', 'falling'];

// Build the `caliper` field decl. User-facing fields:
//   count       — # calipers along the edge
//   width       — projection width per caliper (mm)
//   min_inliers — fit gate: result is NA if final inlier count is below this
//   max_error   — outlier gate: any hit whose residual to the fit exceeds
//                 this distance (mm) is forced to outlier regardless of MAD
//   - length: core uses initMatchingMargin (= shape.margin) — no override.
//   - step:   core uses 1px — no override.
// `geomLengthFn(shape)` returns the along-edge length in def-mm so the
// auto-default tiles cleanly: width = geomLength / count.
export function caliperField(countDefault, geomLengthFn) {
  return {
    editor: (ctx) => (ctx.edit_tar.locating === 'caliper') ? {
      __OBJ__: 'div',
      count: 'input-number',
      width: 'input-number',
      min_inliers: 'input-number',
      max_error: 'input-number',
    } : undefined,
    derive: (shape) => {
      if (shape.locating !== 'caliper') return undefined;
      let width = 0.1;
      if (typeof geomLengthFn === 'function') {
        const L = geomLengthFn(shape);
        if (L > 0 && Number.isFinite(L)) width = L / countDefault;
      }
      return { count: countDefault, width, min_inliers: 5, max_error: 0.1 };
    },
  };
}

// `edge` field decl, parameterized by per-shape defaults. line/arc default to
// {strongest, falling}; search_point defaults to {first, any} (matches core).
export function edgeField({ method = 'strongest', polarity = 'falling', min_strength = 0 } = {}) {
  return {
    editor: (ctx) => (ctx.edit_tar.locating === 'caliper') ? {
      __OBJ__: 'div',
      method:       { __OBJ__: ctx.renderMethods.Dropdown_List, list: EDGE_METHODS },
      polarity:     { __OBJ__: ctx.renderMethods.Dropdown_List, list: EDGE_POLARITIES },
      nth:          'input-number',
      min_strength: 'input-number',
    } : undefined,
    derive: (shape) => (shape.locating === 'caliper') ? {
      method, polarity, nth: 0, min_strength,
    } : undefined,
  };
}

// ─── Canvas rendering ──────────────────────────────────────────────────────
// Helpers used by line/arc draw to overlay the caliper geometry when
// shape.locating === 'caliper'. Kept here so the geometry math + styling
// stay in one place (the per-shape draw stays focused on shape geometry).

// Distribute N caliper anchor points evenly along p0→p1, ENDPOINT-INCLUSIVE
// (i = 0 sits at p0, i = count-1 at p1). Matches the core's distribution in
// Caliper.cpp:caliper_locate_line (`u = (float)i / (count - 1)`), so the
// def-conf overlay shows boxes where the real calipers actually run.
export function lineCaliperAnchors(p0, p1, count) {
  const out = [];
  const dx = p1.x - p0.x, dy = p1.y - p0.y;
  const len = Math.hypot(dx, dy) || 1;
  const tx = dx / len, ty = dy / len;            // along-line unit
  const nx = -ty,      ny = tx;                  // perpendicular unit
  const denom = (count > 1) ? (count - 1) : 1;
  for (let i = 0; i < count; i++) {
    const t = (count > 1) ? (i / denom) : 0.5;   // single caliper sits at midpoint
    out.push({ x: p0.x + dx * t, y: p0.y + dy * t });
  }
  return { anchors: out, normal: { x: nx, y: ny }, tangent: { x: tx, y: ty } };
}

// Same convention as line — endpoint-inclusive distribution along the arc
// (matches Caliper.cpp:caliper_locate_circle `a = angStart + span*i/(count-1)`).
export function arcCaliperAnchors(cx, cy, r, a0, a1, count) {
  const out = [];
  const span = a1 - a0;
  const denom = (count > 1) ? (count - 1) : 1;
  for (let i = 0; i < count; i++) {
    const t = (count > 1) ? (i / denom) : 0.5;
    const a = a0 + span * t;
    const cosA = Math.cos(a), sinA = Math.sin(a);
    out.push({
      x: cx + r * cosA, y: cy + r * sinA,
      nx: cosA, ny: sinA,                        // outward radial
    });
  }
  return out;
}

// Caliper overlay style. One box per caliper; box dimensions: along-line
// tangent = cal_width, across-edge normal = 2 * cal_length. Default blue;
// when a per-caliper status is available (cal_hits), boxes whose caliper
// MISSED switch to gray so the user can see which calipers found nothing
// without an extra X mark on top.
const CAL_STROKE         = 'rgba( 70, 110, 255, 0.95)';  // default / inlier-zone
const CAL_STROKE_MISSED  = 'rgba(160, 160, 160, 0.70)';  // grayed (no edge found)

// Resolve the per-anchor stroke color from cal_hits status. Index-aligned
// with anchors (core's caliper_locate_* produces hits in the same i-order).
// When cal_hits is missing, every caliper renders default-colored.
function strokeForAnchor(calHits, i) {
  if (!calHits || !calHits[i]) return CAL_STROKE;
  return (calHits[i].st === 0) ? CAL_STROKE_MISSED : CAL_STROKE;
}

function drawOrientedBox(ctx, cx, cy, tx, ty, nx, ny, halfAlong, halfAcross) {
  // Corner = center ± tangent*halfAlong ± normal*halfAcross.
  const ax = tx * halfAlong, ay = ty * halfAlong;
  const bx = nx * halfAcross, by = ny * halfAcross;
  ctx.beginPath();
  ctx.moveTo(cx - ax - bx, cy - ay - by);
  ctx.lineTo(cx + ax - bx, cy + ay - by);
  ctx.lineTo(cx + ax + bx, cy + ay + by);
  ctx.lineTo(cx - ax + bx, cy - ay + by);
  ctx.closePath();
  ctx.stroke();
}

// Draw N caliper boxes along a line shape. Across-edge length = shape.margin
// (matches core's behavior — caliper engine uses initMatchingMargin as the
// search half-length). shape.caliper may be undefined right after the user
// flips locating to 'caliper'; helper falls back to count=10 / width=0.1.
// `calHits` (optional, index-aligned with the caliper anchors): per-caliper
// status array from the latest inspection report. Used to gray the boxes
// of calipers that found nothing.
export function drawLineCalipers(ctx, p0, p1, caliper, renderer, margin = 0, calHits) {
  caliper = caliper || {};
  const count  = (caliper.count > 0) ? Math.min(caliper.count, 512) : 10;
  const width  = (caliper.width > 0) ? Math.min(caliper.width, 64)  : 0.1;
  const length = margin > 0 ? margin : 0;
  if (length <= 0 || width <= 0 || count <= 0) return;

  const { anchors, normal, tangent } = lineCaliperAnchors(p0, p1, count);
  const halfAlong  = width  / 2;
  const halfAcross = length;       // full search half-span across edge

  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  anchors.forEach((a, i) => {
    ctx.strokeStyle = strokeForAnchor(calHits, i);
    drawOrientedBox(ctx, a.x, a.y, tangent.x, tangent.y, normal.x, normal.y, halfAlong, halfAcross);
  });
  // Scan-direction arrows at the FIRST and LAST calipers — outermost
  // tangential side. Offset derived from neighbor chord (point-order safe).
  drawEndScanArrows(ctx, anchors, halfAlong, halfAcross, () => normal, renderer);
  ctx.restore();
}

// Draw N radial caliper boxes along an arc (cx,cy,r, [a0,a1]). Across-edge
// length = shape.margin (matches core: caliper uses initMatchingMargin).
// `calHits` (optional): index-aligned per-caliper status; missed → gray box.
export function drawArcCalipers(ctx, cx, cy, r, a0, a1, caliper, renderer, margin = 0, calHits) {
  caliper = caliper || {};
  const count = (caliper.count > 0) ? Math.min(caliper.count, 512) : 10;
  const width = (caliper.width > 0) ? Math.min(caliper.width, 64)  : 0.1;
  const length = margin > 0 ? margin : 0;
  if (length <= 0 || width <= 0 || count <= 0) return;

  const anchors = arcCaliperAnchors(cx, cy, r, a0, a1, count);
  const halfAlong  = width  / 2;
  const halfAcross = length;

  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  anchors.forEach((a, i) => {
    ctx.strokeStyle = strokeForAnchor(calHits, i);
    // tangent at this anchor = perp to radial
    drawOrientedBox(ctx, a.x, a.y, -a.ny, a.nx, a.nx, a.ny, halfAlong, halfAcross);
  });
  // Radial scan-direction arrows at the FIRST and LAST calipers; each uses
  // its own outward radial. Outer-tangent direction derived from neighbors
  // (handles both CCW and CW arc sweeps consistently).
  drawEndScanArrows(ctx, anchors, halfAlong, halfAcross, (a) => ({ x: a.nx, y: a.ny }), renderer);
  ctx.restore();
}

// Place a scan-direction arrow at each end caliper, offset to its outermost
// tangential side (the side facing AWAY from the rest of the strip). The
// "away from strip" direction is computed from the chord to the next-inward
// neighbor, so it stays correct regardless of point order / sweep direction.
//   anchors      — array of {x, y, ...}; need length >= 1
//   halfAlong    — offset distance along the chord (= caliper width / 2)
//   halfAcross   — half search span across edge
//   normalOf(a)  — returns {x, y} unit scan direction for anchor `a`
function drawEndScanArrows(ctx, anchors, halfAlong, halfAcross, normalOf, renderer) {
  const n = anchors.length;
  if (n === 0) return;
  const place = (anchor, neighbor) => {
    let ox = 1, oy = 0;
    if (neighbor) {
      const dx = anchor.x - neighbor.x, dy = anchor.y - neighbor.y;
      const m = Math.hypot(dx, dy) || 1;
      ox = dx / m; oy = dy / m;
    }
    const norm = normalOf(anchor);
    drawScanArrow(
      ctx,
      anchor.x + ox * halfAlong, anchor.y + oy * halfAlong,
      norm.x, norm.y, halfAcross, renderer,
    );
  };
  place(anchors[0], anchors[1]);
  if (n > 1) place(anchors[n - 1], anchors[n - 2]);
}

// Scan-direction arrow at the outermost caliper's outermost side. Shaft
// spans the full across-edge (inner edge → outer edge), arrowhead sits at
// the outermost corner. Red + thicker to dominate visually over the blue
// box outlines.
const SCAN_ARROW_STROKE = 'rgba(230, 50, 30, 0.95)';
function drawScanArrow(ctx, cx, cy, nx, ny, halfAcross, renderer) {
  const x0 = cx - nx * halfAcross, y0 = cy - ny * halfAcross;
  const x1 = cx + nx * halfAcross, y1 = cy + ny * halfAcross;
  const arrowSize = renderer.getPrimitiveSize() * 2.5;
  ctx.save();
  ctx.strokeStyle = SCAN_ARROW_STROKE;
  ctx.fillStyle   = SCAN_ARROW_STROKE;
  ctx.lineWidth = renderer.getIndicationLineSize() * 2;
  if (typeof renderer.canvas_arrow === 'function') {
    renderer.canvas_arrow(ctx, x0, y0, x1, y1, arrowSize);
  } else {
    ctx.beginPath();
    ctx.moveTo(x0, y0); ctx.lineTo(x1, y1);
    ctx.stroke();
  }
  ctx.restore();
}

// Per-caliper hit overlay. Each hit: { x, y, st, s }.
//   0 = missed   — caliper found nothing. NOT drawn as an X (the caliper
//                  BOX is grayed out instead — see drawLineCalipers /
//                  drawArcCalipers).
//   1 = outlier  — peak found but rejected by MAD / max_error.
//   2 = inlier   — peak found, kept in the fit.
const HIT_COLOR = {
  1: 'rgba(220, 60, 60,0.95)',  // outlier (red)
  2: 'rgba( 60,220, 80,0.95)',  // inlier (green)
};

// `style`: 'cross' (default — X marker, used by line/arc) or 'dot' (filled
// circle, used by search_point where many hits sit on a short search vector
// and crosses overlap visually).
// ONE path per colour, not one per hit.
//
// There are ~300 hits per shape and the inspection screen redraws them at part
// rate: the previous shape -- beginPath/stroke for every single hit -- was on
// the order of 18,000 independent stroke() calls a second on this bench. Each
// one is a separate rasteriser submission, and on a machine with no GPU
// acceleration (a fresh PC still on the Microsoft Basic Display Adapter,
// software-rasterising) that is the difference between a responsive screen and
// a renderer blocked three quarters of every second.
//
// Hits only ever take two visible colours, so two paths cover the whole
// overlay: collect the segments, stroke twice. Same pixels, same order of
// colours (outlier over inlier is not meaningful -- they do not overlap).
export function drawCaliperHits(ctx, hits, renderer, { style = 'cross' } = {}) {
  if (!hits || hits.length === 0) return;
  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  const a = renderer.getPointSize() * 1.0;

  // Bucket by the colour actually used, so an unexpected st value still lands
  // somewhere rather than being dropped.
  const byColor = new Map();
  for (const h of hits) {
    if (h.st === 0) continue;  // missed — box-grayed instead
    const color = HIT_COLOR[h.st] || HIT_COLOR[1];
    let arr = byColor.get(color);
    if (arr === undefined) { arr = []; byColor.set(color, arr); }
    arr.push(h);
  }

  for (const [color, list] of byColor) {
    ctx.beginPath();
    if (style === 'dot') {
      for (const h of list) {
        // moveTo before arc: without it the path carries a line from the end
        // of the previous arc to the start of this one.
        ctx.moveTo(h.x + a * 0.6, h.y);
        ctx.arc(h.x, h.y, a * 0.6, 0, Math.PI * 2);
      }
      ctx.fillStyle = color;
      ctx.fill();
    } else {
      for (const h of list) {
        ctx.moveTo(h.x - a, h.y - a);
        ctx.lineTo(h.x + a, h.y + a);
        ctx.moveTo(h.x - a, h.y + a);
        ctx.lineTo(h.x + a, h.y - a);
      }
      ctx.strokeStyle = color;
      ctx.stroke();
    }
  }
  ctx.restore();
}

// Single caliper-box outline — for search_point caliper mode where the core
// runs ONE caliper covering the whole search vector (no count distribution).
// Geometry: `centerPt` is the box center; `tangent` runs along the box's
// long edge (length = halfAlong*2); `normal` runs across the search edge
// (length = halfAcross*2). Default stroke = the same blue used by
// line/arc caliper boxes for visual consistency.
export function drawSingleCaliperBox(ctx, centerPt, tangent, normal, halfAlong, halfAcross, renderer) {
  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.strokeStyle = CAL_STROKE;
  drawOrientedBox(ctx, centerPt.x, centerPt.y, tangent.x, tangent.y, normal.x, normal.y, halfAlong, halfAcross);
  // Scan-direction arrow on the +tangent short edge: shaft spans the full
  // search depth (inner → outer along `normal`). Same convention as
  // drawLineCalipers/drawArcCalipers' end-caliper arrows.
  const ax = centerPt.x + tangent.x * halfAlong;
  const ay = centerPt.y + tangent.y * halfAlong;
  drawScanArrow(ctx, ax, ay, normal.x, normal.y, halfAcross, renderer);
  ctx.restore();
}

// Small directional arrow indicating the chosen edge polarity. Drawn at the
// shape's reference point; direction = `dir` (unit vector). RISING points
// "into light" (dark→light); FALLING the opposite; ANY shows a double-headed
// arrow. `len` is in px.
export function drawEdgePolarityArrow(ctx, x, y, dir, polarity, len, renderer) {
  if (!dir || !polarity) return;
  ctx.save();
  ctx.strokeStyle = CAL_STROKE;
  ctx.fillStyle = CAL_STROKE;
  ctx.lineWidth = renderer.getIndicationLineSize();
  const arrowSize = renderer.getPrimitiveSize() * 2;

  const sign = (polarity === 'falling') ? -1 : 1;
  const x0 = x - dir.x * len / 2 * sign;
  const y0 = y - dir.y * len / 2 * sign;
  const x1 = x + dir.x * len / 2 * sign;
  const y1 = y + dir.y * len / 2 * sign;
  renderer.canvas_arrow(ctx, x0, y0, x1, y1, arrowSize);

  if (polarity === 'any') {
    // Second arrowhead at the start to indicate bidirectional.
    renderer.canvas_arrow(ctx, x1, y1, x0, y0, arrowSize);
  }
  ctx.restore();
}


// Is this caliper configuration one the core can ever satisfy?
//
// min_inliers is compared against the number of calipers that FOUND an edge, so
// min_inliers > count can never be met: `ok` is false for every part, forever,
// and the shape reports NA. Nothing says why -- and the overlay makes it worse
// than silent, because it draws `count` perfectly good green inlier crosses on
// the edge. The one screen that could explain the NA argues against it.
//
// Returns a human sentence or null. The caller decides how loudly to say it;
// this decides what is true.
export function caliperConfigProblem(caliper) {
  if (!caliper) return null;
  const n = Number(caliper.count);
  const mi = Number(caliper.min_inliers);
  if (!isFinite(n) || !isFinite(mi)) return null;
  if (mi > n)
    return `min_inliers (${mi}) 大於 count (${n}) — 永遠無法滿足,這個特徵會一直是 NA`;
  // The core floors count at 2 for a line fit and 3 for an arc; below that it
  // silently runs more calipers than the def asked for, so the overlay and the
  // fit disagree about how many there are.
  if (n > 0 && n < 2)
    return `count (${n}) 太小 — 核心至少會跑 2 支(弧為 3),畫面與實際不符`;
  return null;
}
