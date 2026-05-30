// Per-shape module: SEARCH_POINT.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from './_schemaHelpers';

export const type = 'search_point';

// search_far has a migration shim: legacy defs used `search_style` (0/1) before
// the boolean; preserve that mapping when the field is missing. The schema's
// `default` only fires when `derive` doesn't produce a value.
export const fields = {
  angleDeg:         { editor: 'AngleRangeSetup' },
  search_far:       {
    editor: 'switch',
    default: false,
    derive: (shape) => (shape.search_style !== undefined ? shape.search_style == 1 : undefined),
  },
  locating_anchor:  { editor: 'switch', default: false, normalize: (v) => v === true },
  line_thickness_value: { skipEditor: true, default: 0, normalize: (v) => (typeof v === 'number' ? v : 0) },
};

export function buildWhiteListKey(ctx) {
  return buildWhiteListKeyFromFields(fields, ctx);
}

// canvasCtrl: search_point refs only a line (ref[0]).
export function availableRefShapes(shapeList /*, subtype */) {
  return shapeList.filter((s) => s.type === 'line');
}

// canvasCtrl: search_point's pt1 is the search target — center on it.
export function fitCameraCenter(shape /*, db_obj */) {
  return shape.pt1;
}

export function applyDefaults(shape) {
  return applyDefaultsFromFields(shape, fields);
}

// Draw a search_point — extracted verbatim from renderUTIL.case SHAPE_TYPE.search_point.
// A search_point points at a line (its ref[0]) with a width-bar perpendicular to the
// vector; optionally draws a red aim-cross if it's a locating anchor.
export function draw(ctx, shape, renderer, {
  inFullDisplay = true, shapeList = [], next_ShapeColor = null,
  skip_id_list = [], unitConvert = { unit: 'mm', mult: 1 }, drawSubObjs = false,
} = {}) {
  let shapeColor = SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default;
  shapeColor = Color(shapeColor).alpha(0.8);

  let db_obj = renderer.db_obj;
  let subObjs = shape.ref
    .map((ref) => db_obj.FindShape('id', ref.id, shapeList))
    .map((idx) => { return idx >= 0 ? shapeList[idx] : null; });

  if (subObjs[0] == null) return;

  let vector = db_obj.shapeVectorParse(shape, shapeList);
  let cnormal = { x: -vector.y, y: vector.x };
  let mag = shape.width / 2;
  vector.x *= mag;
  vector.y *= mag;

  let margin = renderer.getSearchDirectionLineSize();
  if (inFullDisplay) margin = shape.margin;

  ctx.lineWidth = margin * 2;
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x - vector.x, y0: shape.pt1.y - vector.y,
    x1: shape.pt1.x + vector.x, y1: shape.pt1.y + vector.y,
  });

  ctx.lineWidth = renderer.getSearchDirectionLineSize();
  ctx.strokeStyle = shapeColor;
  let marginOffset = margin + ctx.lineWidth / 2;
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x - vector.x + cnormal.x * marginOffset, y0: shape.pt1.y - vector.y + cnormal.y * marginOffset,
    x1: shape.pt1.x + vector.x + cnormal.x * marginOffset, y1: shape.pt1.y + vector.y + cnormal.y * marginOffset,
  });

  if (drawSubObjs)
    renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs, inFullDisplay);

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  if (shape.locating_anchor) {
    ctx.strokeStyle = 'red';
    renderer.draw_aimcross(ctx, shape.pt1, renderer.getPointSize() * 3, 0.3);
  }
}

// Inspection-mode draw — just a red cross at pt1. Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.search_point.
export function drawInspection(ctx, shape, renderer) {
  ctx.strokeStyle = 'rgba(179, 0, 0,0.5)';
  renderer.drawcross(ctx, shape.pt1, renderer.getPointSize() * 3);
  ctx.lineWidth = renderer.getIndicationLineSize();
}
