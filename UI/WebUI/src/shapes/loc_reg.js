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
  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.strokeStyle = '#ffab00';   // amber = registration

  // Shaft: origin -> direction tip.
  ctx.beginPath();
  ctx.moveTo(o.x, o.y);
  ctx.lineTo(t.x, t.y);
  ctx.stroke();

  // Arrowhead at the tip.
  const ang = Math.atan2(t.y - o.y, t.x - o.x);
  const len = Math.hypot(t.x - o.x, t.y - o.y);
  const h = Math.max(len * 0.18, renderer.getPointSize() * 2);
  const a1 = ang + Math.PI * 0.85, a2 = ang - Math.PI * 0.85;
  ctx.beginPath();
  ctx.moveTo(t.x, t.y);
  ctx.lineTo(t.x + Math.cos(a1) * h, t.y + Math.sin(a1) * h);
  ctx.moveTo(t.x, t.y);
  ctx.lineTo(t.x + Math.cos(a2) * h, t.y + Math.sin(a2) * h);
  ctx.stroke();

  // Origin marker.
  ctx.strokeStyle = '#ff6d00';
  renderer.drawpoint(ctx, o, 2 * renderer.getPointSize());
  ctx.restore();
}
