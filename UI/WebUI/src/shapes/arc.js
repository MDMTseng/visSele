// Per-shape module: ARC.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { threePointToArc } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';

export const type = 'arc';

export function applyDefaults(shape) {
  let out = { ...shape };
  if (out.locating !== 'caliper') out.locating = 'contour';
  return out;
}

// Draw an arc — extracted verbatim from renderUTIL.drawShapeList.case SHAPE_TYPE.arc.
export function draw(ctx, shape, renderer, { inFullDisplay = true } = {}) {
  let shapeColor = SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default;
  shapeColor = Color(shapeColor).alpha(0.8);

  let arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  let margin = renderer.getSearchDirectionLineSize();
  if (inFullDisplay) margin = shape.margin;
  ctx.lineWidth = margin * 2;
  renderer.drawReportArc(ctx, arc);

  ctx.lineWidth = renderer.getSearchDirectionLineSize();
  ctx.strokeStyle = shapeColor;

  let marginOffset = margin + ctx.lineWidth / 2;
  if (shape.direction < 0) marginOffset = -marginOffset;
  arc.r += marginOffset;
  if (arc.r < 0.0001) arc.r = 0.0001;

  renderer.drawReportArc(ctx, arc);

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  renderer.drawpoint(ctx, shape.pt2);
  renderer.drawpoint(ctx, shape.pt3);
}

// Inspection-mode draw — just the arc (no margin overlay). Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.arc.
export function drawInspection(ctx, shape, renderer) {
  let arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  ctx.lineWidth = renderer.getIndicationLineSize();
  renderer.drawReportArc(ctx, arc);
}
