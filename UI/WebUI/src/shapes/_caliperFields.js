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
      return { count: countDefault, width, min_inliers: 0, max_error: 0 };
    },
  };
}

// `edge` field decl, parameterized by per-shape defaults. line/arc default to
// {strongest, falling}; search_point defaults to {first, any} (matches core).
export function edgeField({ method = 'strongest', polarity = 'falling' } = {}) {
  return {
    editor: (ctx) => (ctx.edit_tar.locating === 'caliper') ? {
      __OBJ__: 'div',
      method:       { __OBJ__: ctx.renderMethods.Dropdown_List, list: EDGE_METHODS },
      polarity:     { __OBJ__: ctx.renderMethods.Dropdown_List, list: EDGE_POLARITIES },
      nth:          'input-number',
      min_strength: 'input-number',
    } : undefined,
    derive: (shape) => (shape.locating === 'caliper') ? {
      method, polarity, nth: 0, min_strength: 0,
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

// Caliper overlay style — blue outline boxes (matches the legacy in-house
// visualization). One box per caliper; box dimensions: along-line tangent =
// `cal_width` (the projection width — same axis the core averages over for
// SNR), across-edge normal = `2 * cal_length` (the full search span).
const CAL_STROKE = 'rgba( 70, 110, 255, 0.95)';

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
export function drawLineCalipers(ctx, p0, p1, caliper, renderer, margin = 0) {
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
  ctx.strokeStyle = CAL_STROKE;
  for (const a of anchors) {
    drawOrientedBox(ctx, a.x, a.y, tangent.x, tangent.y, normal.x, normal.y, halfAlong, halfAcross);
  }
  // Scan-direction arrows at the FIRST and LAST calipers — positioned at
  // each end's OUTERMOST tangential edge (the side facing away from the
  // strip). Offset direction derived from each end's neighbor so it works
  // regardless of pt1/pt2 order. Line scan direction is the same `normal`
  // at both ends.
  drawEndScanArrows(ctx, anchors, halfAlong, halfAcross, () => normal, renderer);
  ctx.restore();
}

// Draw N radial caliper boxes along an arc (cx,cy,r, [a0,a1]). Across-edge
// length = shape.margin (matches core: caliper uses initMatchingMargin).
export function drawArcCalipers(ctx, cx, cy, r, a0, a1, caliper, renderer, margin = 0) {
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
  ctx.strokeStyle = CAL_STROKE;
  for (const a of anchors) {
    // tangent at this anchor = perp to radial
    drawOrientedBox(ctx, a.x, a.y, -a.ny, a.nx, a.nx, a.ny, halfAlong, halfAcross);
  }
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

// Per-caliper hit overlay. Each hit: { x, y, st, s }. status (st):
//   0=missed (no peak found — x,y are 0/0, skip), 1=outlier (MAD-rejected),
//   2=inlier. Renders an "X" mark at each non-missed hit (matches the
// reference sketch — X is more visible against the line than a dot).
const HIT_COLOR = {
  1: 'rgba(220, 60, 60,0.95)',  // outlier
  2: 'rgba( 60,220, 80,0.95)',  // inlier
};

export function drawCaliperHits(ctx, hits, renderer) {
  if (!hits || hits.length === 0) return;
  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  const a = renderer.getPointSize() * 1.0; // half-arm length of the X
  for (const h of hits) {
    if (h.st === 0) continue; // missed — no useful coords emitted
    ctx.strokeStyle = HIT_COLOR[h.st] || HIT_COLOR[1];
    ctx.beginPath();
    ctx.moveTo(h.x - a, h.y - a);
    ctx.lineTo(h.x + a, h.y + a);
    ctx.moveTo(h.x - a, h.y + a);
    ctx.lineTo(h.x + a, h.y - a);
    ctx.stroke();
  }
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
