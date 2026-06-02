// Per-shape module: AUX_POINT.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { closestPointOnPoints } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { buildWhiteListKeyFromFields } from './_schemaHelpers';

export const type = 'aux_point';

export const fields = {
  angleDeg: { editor: 'AngleRangeSetup' },
  // Two ref slots: ref[0] = line, ref[1] = search_point. Same convention as
  // search_point.js — outer __OBJ__:'div' renders a header label; inner
  // numbered keys render __OBJ__:'btn' which become ref-pick buttons.
  ref: { editor: {
    __OBJ__: 'div',
    '0': { __OBJ__: 'btn', id: 'div', element: 'div' },
    '1': { __OBJ__: 'btn', id: 'div', element: 'div' },
  } },
};

export function buildWhiteListKey(ctx) {
  return buildWhiteListKeyFromFields(fields, ctx);
}

// canvasCtrl: aux_point refs a line + a search_point (their intersection).
export function availableRefShapes(shapeList /*, subtype */) {
  return shapeList.filter((s) => s.type === 'line' || s.type === 'search_point');
}

// canvasCtrl: aux_point's center is its RESOLVED intersection point (computed
// by the model via auxPointParse). Returns null if can't resolve.
export function fitCameraCenter(shape, db_obj) {
  const pt = db_obj && db_obj.auxPointParse ? db_obj.auxPointParse(shape) : null;
  return pt || null;
}

// (no applyDefaults — legacy Shape_Attr_Fill has no case for aux_point; pass-through.)

// Draw an aux_point — extracted verbatim from renderUTIL.drawShapeList.case SHAPE_TYPE.aux_point.
// Aux_point is the intersection of its two referenced shapes; we draw dashed
// crosshairs from the resolved intersection to the closest point on each ref line.
export function draw(ctx, shape, renderer, {
  inFullDisplay = true, shapeList = [], next_ShapeColor = null,
  skip_id_list = [], unitConvert = { unit: 'mm', mult: 1 }, drawSubObjs = false,
} = {}) {
  let shapeColor = SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default;
  shapeColor = Color(shapeColor).alpha(0.8);

  if (true || inFullDisplay) {
    ctx.lineWidth = renderer.getSearchDirectionLineSize();
    ctx.strokeStyle = shapeColor.alpha(1);
    let db_obj = renderer.db_obj;
    let subObjs = shape.ref
      .map((ref) => db_obj.FindShape('id', ref.id, shapeList))
      .map((idx) => { return idx >= 0 ? shapeList[idx] : null; });
    if (drawSubObjs)
      renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs, inFullDisplay);
    if (shape.id === undefined) return;

    let point = renderer.db_obj.auxPointParse(shape, shapeList);
    if (point !== undefined && subObjs.length == 2) { // Draw crosssect line
      ctx.setLineDash([2 * renderer.getPrimitiveSize(), renderer.getPrimitiveSize()]);

      ctx.beginPath();
      ctx.moveTo(point.x, point.y);
      let closestPt = closestPointOnPoints(point, [subObjs[0].pt1, subObjs[0].pt2]);
      ctx.lineTo(closestPt.x, closestPt.y);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(point.x, point.y);
      closestPt = closestPointOnPoints(point, [subObjs[1].pt1, subObjs[1].pt2]);
      ctx.lineTo(closestPt.x, closestPt.y);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.strokeStyle = 'gray';
      renderer.drawpoint(ctx, point);
    }
  }
}

// Inspection-mode draw — dashed crosshairs to refs, gray cross at intersection.
// Extracted from renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.aux_point.
export function drawInspection(ctx, shape, renderer, { shapeList = [] } = {}) {
  let db_obj = renderer.db_obj;
  let subObjs = shape.ref
    .map((ref) => db_obj.FindShape('id', ref.id, shapeList))
    .map((idx) => { return idx >= 0 ? shapeList[idx] : null; });

  if (shape.id === undefined) return;

  ctx.lineWidth = renderer.getIndicationLineSize();
  let point = renderer.db_obj.auxPointParse(shape, shapeList);
  if (point !== undefined && subObjs.length == 2) {
    ctx.setLineDash([renderer.getPrimitiveSize(), renderer.getPrimitiveSize()]);
    ctx.beginPath();
    ctx.moveTo(point.x, point.y);
    let closestPt = closestPointOnPoints(point, [subObjs[0].pt1, subObjs[0].pt2]);
    ctx.lineTo(closestPt.x, closestPt.y);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(point.x, point.y);
    closestPt = closestPointOnPoints(point, [subObjs[1].pt1, subObjs[1].pt2]);
    ctx.lineTo(closestPt.x, closestPt.y);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.strokeStyle = 'gray';
    renderer.drawcross(ctx, point, renderer.getPointSize() * 2);
  }
}

// Inherent-shape-list draw — a black rect at the resolved aux_point.
// Extracted from renderUTIL.drawInherentShapeList.case SHAPE_TYPE.aux_point.
export function drawInherent(ctx, shape, renderer) {
  let point = renderer.db_obj.auxPointParse(shape);
  if (point != null) {
    ctx.strokeStyle = 'black';
    renderer.drawpoint(ctx, point, 'rect');
  }
}
