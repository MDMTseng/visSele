import { overlayKit, OVERLAY } from 'JSSRCROOT/canvas/overlayKit';
// Per-shape module: LOC_REG — the transient "localization line" used to set the
// shape-based localizer registration (def_image_reg). It is NEVER persisted: the
// canvas tool (ctrlLogic_DEFCONF, DEFCONF_MODE_LOC_REG_CREATE) draws it while the
// user drags, and on release writes {cx,cy,angle} to edit_info.def_image_reg and
// drops the shape. This module only provides the draw (origin dot + direction arrow).
export const type = 'loc_reg';

export function availableRefShapes() { return []; }

export function draw(ctx, shape, renderer) {
  const o = shape.pt1, t = shape.pt2;
  if (!o || !t) return;
  const K = overlayKit(ctx, renderer);
  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.setLineDash([]);
  ctx.strokeStyle = K.C.datum;   // a registration frame IS a datum

  // Shaft: origin -> direction tip.
  ctx.beginPath();
  ctx.moveTo(o.x, o.y);
  ctx.lineTo(t.x, t.y);
  ctx.stroke();

  // Arrowhead at the tip.
  const ang = Math.atan2(t.y - o.y, t.x - o.x);
  const len = Math.hypot(t.x - o.x, t.y - o.y);
  // A proper filled head. The old one was two strokes at a 0.85pi half-angle,
  // which reads as a wide V, not an arrow.
  ctx.fillStyle = K.C.datum;
  K.arrow(t, ang, Math.max(len * 0.14, K.S.arrow_head * K.ps));

  // Origin: the drafting origin symbol (circle + cross), plus the angle the
  // frame is set to, dimensioned off horizontal.
  ctx.beginPath(); ctx.arc(o.x, o.y, 2.6 * K.ps, 0, 2 * Math.PI); ctx.stroke();
  ctx.lineWidth = K.lw * K.S.thin_w;
  K.seg({ x: o.x - 4 * K.ps, y: o.y }, { x: o.x + 4 * K.ps, y: o.y });
  K.seg({ x: o.x, y: o.y - 4 * K.ps }, { x: o.x, y: o.y + 4 * K.ps });
  const r = Math.min(len * 0.45, 14 * K.ps);
  ctx.strokeStyle = K.withAlpha(K.C.datum, 0.7);
  ctx.setLineDash(K.dash('tie'));
  K.seg(o, { x: o.x + r * 1.25, y: o.y });
  ctx.setLineDash([]);
  ctx.strokeStyle = K.C.reading;
  ctx.beginPath(); ctx.arc(o.x, o.y, r, 0, ang, ang < 0); ctx.stroke();
  const lp = K.at(o, ang / 2, r + K.S.chip_gap * K.ps);
  K.chip((ang * 180 / Math.PI).toFixed(renderer.fixedDigit.A) + 'º', lp.x, lp.y, K.C.reading, OVERLAY.font.tag);
  ctx.restore();
}
