// Per-shape module: LINE.
// Each shape type owns one file declaring its defaults, editor schema (later),
// and draw logic. Adding a new primitive becomes a single-file change.
import Color from 'color';
import { LineCentralNormal } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';

export const type = 'line';

// applyDefaults: PURE — may return the input untouched or a shallow clone with
// missing fields filled. Mirrors the legacy Shape_Attr_Fill behavior verbatim.
export function applyDefaults(shape) {
  let out = { ...shape };
  if (out.vertex_touch_searching !== true) out.vertex_touch_searching = false;
  // caliper locating mode: "contour" (legacy) | "caliper". Normalize so the
  // dropdown always has a string; core treats anything != "caliper" as contour.
  if (out.locating !== 'caliper') out.locating = 'contour';
  return out;
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
