// Per-shape module: LINE.
// Each shape type owns one file declaring its defaults, editor schema (later),
// and draw logic. Adding a new primitive becomes a single-file change.
import Color from 'color';
import { LineCentralNormal } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from './_schemaHelpers';
import { caliperField, edgeField, drawLineCalipers, drawCaliperHits } from './_caliperFields';
export { LinePropertySheet as PropertySheet } from './_propertySheet/LinePropertySheet';

export const type = 'line';

// Single source for both editor schema and applyDefaults.
// caliper locating mode: "contour" (legacy) | "caliper". Core treats anything
// != "caliper" as contour, so normalize keeps the dropdown value sane.
// caliper/edge sub-groups are hidden + zero-cost when locating == "contour";
// switching to "caliper" lazy-inits them with the core's defaults (count=30).
export const fields = {
  vertex_touch_searching: { editor: 'switch', default: false, normalize: (v) => v === true },
  locating: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['contour', 'caliper'] }),
    default: 'contour',
    normalize: (v) => (v === 'caliper' ? 'caliper' : 'contour'),
    // Persist caliper/edge defaults onto the STORED shape when the user
    // first flips locating to 'caliper'. Without this, the property sheet
    // shows the derive-defaults (on the clone) while the canvas reads the
    // stored shape (no caliper sub-object) and uses its own fallback —
    // they disagree until the user edits a sub-field.
    onChange: (obj) => {
      if (obj.locating !== 'caliper') return;
      const count = 10;
      if (obj.caliper === undefined) {
        const len = (obj.pt1 && obj.pt2)
          ? Math.hypot(obj.pt2.x - obj.pt1.x, obj.pt2.y - obj.pt1.y) : 0;
        obj.caliper = {
          count,
          width: (len > 0 ? len / count : 0.1),
          min_inliers: 5,
          max_error: 0.1,
        };
      }
      if (obj.edge === undefined) {
        obj.edge = { method: 'strongest', polarity: 'falling', nth: 0, min_strength: 60 };
      }
    },
  },
  caliper: caliperField(10, (s) =>
    (s.pt1 && s.pt2) ? Math.hypot(s.pt2.x - s.pt1.x, s.pt2.y - s.pt1.y) : 0
  ),
  edge:    edgeField({ method: 'strongest', polarity: 'falling', min_strength: 60 }),
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
  const isCaliper = (shape.locating === 'caliper');

  // Contour-mode margin band + search-vector offset line: the wide red strip
  // visualizing the contour-path search range. Meaningless in caliper mode
  // (calipers define their own search via cal_length/cal_width), so the
  // caliper boxes replace it entirely.
  if (!isCaliper) {
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
  } else {
    // Caliper mode: just stroke the nominal line itself thinly so the user
    // still sees pt1→pt2; the boxes carry the search-range visualization.
    ctx.lineWidth = renderer.getIndicationLineSize();
    ctx.strokeStyle = shapeColor;
    renderer.drawReportLine(ctx, {
      x0: shape.pt1.x, y0: shape.pt1.y,
      x1: shape.pt2.x, y1: shape.pt2.y,
    });
  }

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  renderer.drawpoint(ctx, shape.pt2);

  // Caliper-mode overlay: N caliper boxes along the line. Editor-mode only.
  // shape.caliper may be undefined just after the user toggles locating to
  // 'caliper' — the helper falls back to the core defaults.
  if (inFullDisplay && isCaliper) {
    // Per-caliper hits drive two visuals: the box's stroke color
    // (gray when status=missed) and the X marker (inlier=green, outlier=red).
    // Sources: shape.cal_hits (inspection-mode merge by
    // ShapeAdjustsWithInspectionResult) preferred, fall back to
    // renderer.cal_hits_by_id[id] (def-conf overlay from edit_info.inspReport).
    const hits = shape.cal_hits
      || (renderer.cal_hits_by_id && renderer.cal_hits_by_id[shape.id]);
    drawLineCalipers(ctx, shape.pt1, shape.pt2, shape.caliper, renderer, shape.margin, hits);
    if (hits) drawCaliperHits(ctx, hits, renderer);
  }
}

// Inspection-mode draw — minimal (no margin overlays). Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.line. Caliper-mode reports
// carry per-caliper hits (shape.cal_hits, merged in ShapeAdjustsWithInspectionResult).
// Overlay them when renderer.show_caliper_hits is on (System_Setting flag).
export function drawInspection(ctx, shape, renderer) {
  ctx.lineWidth = renderer.getIndicationLineSize();
  renderer.drawReportLine(ctx, {
    x0: shape.pt1.x, y0: shape.pt1.y,
    x1: shape.pt2.x, y1: shape.pt2.y,
  });
  if (renderer.show_caliper_hits !== false && shape.cal_hits) {
    drawCaliperHits(ctx, shape.cal_hits, renderer);
  }
}
