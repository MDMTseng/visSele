// Per-shape module: LINE.
// Each shape type owns one file declaring its defaults, editor schema (later),
// and draw logic. Adding a new primitive becomes a single-file change.
import Color from 'color';
import { LineCentralNormal } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from './_schemaHelpers';

export const type = 'line';

// Single source for both editor schema and applyDefaults.
// caliper locating mode: "contour" (legacy) | "caliper". Core treats anything
// != "caliper" as contour, so normalize keeps the dropdown value sane.
export const fields = {
  vertex_touch_searching: { editor: 'switch', default: false, normalize: (v) => v === true },
  locating: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['contour', 'caliper'] }),
    default: 'contour',
    normalize: (v) => (v === 'caliper' ? 'caliper' : 'contour'),
  },
};

export function buildWhiteListKey(ctx) {
  return buildWhiteListKeyFromFields(fields, ctx);
}

// canvasCtrl: what shapes are valid refs when creating/editing a line. Lines
// have no refs — they're constructed from raw pt1/pt2.
export function availableRefShapes(shapeList /*, subtype */) {
  return [];
}

// canvasCtrl: center point for "fit camera to this shape" — midpoint of pt1/pt2.
export function fitCameraCenter(shape /*, db_obj */) {
  return {
    x: (shape.pt1.x + shape.pt2.x) / 2,
    y: (shape.pt1.y + shape.pt2.y) / 2,
  };
}

export function applyDefaults(shape) {
  return applyDefaultsFromFields(shape, fields);
}

// Draw a line in the editor canvas. Extracted verbatim from renderUTIL.js
// drawShapeList.case SHAPE_TYPE.line (the "full" draw — not the inspection-
// result overlay). The renderer arg exposes the helpers this draw needs from
// the legacy renderUTIL class (drawReportLine, drawpoint, line-size helper).
// Keystone step 3: per-shape draw owns its rendering.
export function draw(ctx, shape, renderer, { inFullDisplay = true } = {}) {
  let shapeColor = SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default;
  shapeColor = Color(shapeColor).alpha(0.8);

  let cnormal = LineCentralNormal(shape);
  let drawMargin = renderer.getSearchDirectionLineSize();
  if (inFullDisplay) drawMargin = shape.margin;
  ctx.lineWidth = drawMargin * 2;
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x, y0: shape.pt1.y,
    x1: shape.pt2.x, y1: shape.pt2.y,
  });

  ctx.lineWidth = renderer.getSearchDirectionLineSize();
  ctx.strokeStyle = shapeColor;
  let marginOffset = drawMargin + ctx.lineWidth / 2;
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x + cnormal.vx * marginOffset, y0: shape.pt1.y + cnormal.vy * marginOffset,
    x1: shape.pt2.x + cnormal.vx * marginOffset, y1: shape.pt2.y + cnormal.vy * marginOffset,
  });

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  renderer.drawpoint(ctx, shape.pt2);
}

// Inspection-mode draw — minimal (no margin overlays). Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.line.
export function drawInspection(ctx, shape, renderer) {
  ctx.lineWidth = renderer.getIndicationLineSize();
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x, y0: shape.pt1.y,
    x1: shape.pt2.x, y1: shape.pt2.y,
  });
}
