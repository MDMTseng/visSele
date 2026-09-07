// Per-shape module: AUX_LINE.
// See shapes/line.js for the pattern + rationale.
import { buildWhiteListKeyFromFields } from './_schemaHelpers';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';

export const type = 'aux_line';

// Two ref slots: the two points the line goes through. See aux_point.js for
// the same pattern. JsonEditBlock fallback renders these as ref-pick buttons.
export const fields = {
  ref: { editor: {
    __OBJ__: 'div',
    '0': { __OBJ__: 'btn', id: 'div', element: 'div' },
    '1': { __OBJ__: 'btn', id: 'div', element: 'div' },
  } },
};

export function buildWhiteListKey(ctx) {
  return buildWhiteListKeyFromFields(fields, ctx);
}

// canvasCtrl: an aux_line goes THROUGH two points -- a search point, a
// crossing (aux_point) or an arc's centre. Mirrors the core's parse_auxLineData
// / ParseLocatePosition: whatever the core can resolve to a point.
export function availableRefShapes(shapeList /*, subtype */) {
  return shapeList.filter((s) => s.type === 'search_point' || s.type === 'aux_point' || s.type === 'arc');
}

// canvasCtrl: pan to the midpoint of the resolved line.
export function fitCameraCenter(shape, db_obj) {
  const g = db_obj && db_obj.auxLineParse ? db_obj.auxLineParse(shape) : undefined;
  return g ? { x: (g.pt1.x + g.pt2.x) / 2, y: (g.pt1.y + g.pt2.y) / 2 } : null;
}

// (no fitCameraCenter — aux_line doesn't pan-to-shape in the legacy code.)

// (no applyDefaults — legacy Shape_Attr_Fill has no case for aux_line; pass-through.)

// Draw an aux_line: the line through its two referenced points, extended a
// little past both so it reads as a construction line rather than a segment.
// pt1/pt2 are materialised on the shape by the model (refreshAuxLines) and,
// on the inspection overlay, replaced by the core's located endpoints.
export function draw(ctx, shape, renderer, {
  inFullDisplay = true, shapeList = [], next_ShapeColor = null,
  skip_id_list = [], unitConvert = { unit: 'mm', mult: 1 }, drawSubObjs = false,
} = {}) {
  let db_obj = renderer.db_obj;
  let subObjs = (shape.ref || [])
    .map((ref) => db_obj.FindShape('id', ref && ref.id, shapeList))
    .map((idx) => { return idx >= 0 ? shapeList[idx] : null; });
  if (drawSubObjs)
    renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs, inFullDisplay);
  if (shape.id === undefined) return;

  let a = shape.pt1, b = shape.pt2;
  if (!(a && b) && db_obj.auxLineParse) {
    const g = db_obj.auxLineParse(shape, shapeList);
    if (g) { a = g.pt1; b = g.pt2; }
  }
  if (!(a && b)) return;    // a ref is missing: nothing honest to draw
  const dx = b.x - a.x, dy = b.y - a.y;
  const L = Math.hypot(dx, dy);
  if (!(L > 0)) return;
  const ext = 0.25 * L;
  const ux = dx / L, uy = dy / L;
  const na = shape.inspection_status !== undefined && shape.inspection_status !== 0;
  ctx.lineWidth = renderer.getSearchDirectionLineSize();
  ctx.strokeStyle = na ? 'rgba(200,60,60,0.9)' : (SHAPE_TYPE_COLOR[type] || 'gray');
  ctx.setLineDash([renderer.getPrimitiveSize() * 2, renderer.getPrimitiveSize()]);
  ctx.beginPath();
  ctx.moveTo(a.x - ux * ext, a.y - uy * ext);
  ctx.lineTo(b.x + ux * ext, b.y + uy * ext);
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = ctx.strokeStyle;
  renderer.drawpoint(ctx, a);
  renderer.drawpoint(ctx, b);
  // The line's own handle: what a measure / crossing / search point picks to
  // reference it (FindClosestCtrlPointInfo). Endpoints belong to other shapes.
  const mid = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
  const r = renderer.getPrimitiveSize() * 1.2;
  ctx.beginPath();
  ctx.rect(mid.x - r, mid.y - r, 2 * r, 2 * r);
  ctx.stroke();
}
