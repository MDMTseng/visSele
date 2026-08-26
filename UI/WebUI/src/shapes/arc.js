// Per-shape module: ARC.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { threePointToArc, arcSweep } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from './_schemaHelpers';
import { caliperField, edgeField, drawArcCalipers, drawCaliperHits } from './_caliperFields';
export { ArcPropertySheet as PropertySheet } from './_propertySheet/ArcPropertySheet';

export const type = 'arc';

// `direction` is stored as ±1 (not a boolean) — the legacy UI binds it to a
// switch editor that emits a boolean; coerce maps checked→-1, unchecked→+1.
// caliper/edge sub-groups appear only when locating == 'caliper' (core default
// for arc: count=36).
export const fields = {
  direction: {
    editor: 'switch',
    coerce: (evt) => (evt.target.checked ? -1 : 1),
  },
  locating: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['contour', 'caliper'] }),
    default: 'contour',
    normalize: (v) => (v === 'caliper' ? 'caliper' : 'contour'),
    // Persist caliper/edge defaults onto the STORED shape — see line.js for
    // rationale. Arc default count=10; width seeded from arc length.
    onChange: (obj) => {
      if (obj.locating !== 'caliper') return;
      const count = 10;
      if (obj.caliper === undefined) {
        // arcSweep, not a local pt1->pt3 span: pt2 is what says WHICH of the
        // two arcs this is, and ignoring it seeds the complement.
        const arcLen = (obj.pt1 && obj.pt2 && obj.pt3)
          ? arcSweep(obj.pt1, obj.pt2, obj.pt3).length : 0;
        obj.caliper = {
          count,
          width: (arcLen > 0 ? arcLen / count : 0.1),
          min_inliers: 5,
          max_error: 0.1,
        };
      }
      if (obj.edge === undefined) {
        obj.edge = { method: 'strongest', polarity: 'falling', nth: 0, min_strength: 60 };
      }
    },
  },
  // Envelope fit mode — keeps LS center, uses max/min |center-hit| for the
  // radius. Default 'ls' (no change vs legacy). Core: featureDef_circle.fit_mode.
  fit_mode: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['ls', 'outer', 'inner'] }),
    default: 'ls',
    normalize: (v) => (v === 'outer' || v === 'inner') ? v : 'ls',
  },
  caliper: caliperField(10, (s) => (
    (s.pt1 && s.pt2 && s.pt3) ? arcSweep(s.pt1, s.pt2, s.pt3).length : 0
  )),
  edge:    edgeField({ method: 'strongest', polarity: 'falling', min_strength: 60 }),
};

export function buildWhiteListKey(ctx) {
  return buildWhiteListKeyFromFields(fields, ctx);
}

// canvasCtrl: no refs — arc is constructed from raw pt1/pt2/pt3.
export function availableRefShapes(shapeList /*, subtype */) {
  return [];
}

// canvasCtrl: arc center (or midpoint if the radius is huge — pan would otherwise
// land off-screen).
export function fitCameraCenter(shape /*, db_obj */) {
  const arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  if (arc.r > 500) {
    return { x: (shape.pt1.x + shape.pt3.x) / 2, y: (shape.pt1.y + shape.pt3.y) / 2 };
  }
  return { x: arc.x, y: arc.y };
}

export function applyDefaults(shape) {
  return applyDefaultsFromFields(shape, fields);
}

// Draw an arc — extracted verbatim from renderUTIL.drawShapeList.case SHAPE_TYPE.arc.
export function draw(ctx, shape, renderer, { inFullDisplay = true } = {}) {
  let shapeColor = SHAPE_TYPE_COLOR[type] || SHAPE_TYPE_COLOR.default;
  shapeColor = Color(shapeColor).alpha(0.8);

  let arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  const isCaliper = (shape.locating === 'caliper');

  // Contour-mode margin band + offset arc: the wide search-range strip.
  // Replaced by the caliper boxes in caliper mode (see below).
  if (!isCaliper) {
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
  } else {
    // Caliper mode: stroke the nominal arc thinly; boxes show the search range.
    ctx.lineWidth = renderer.getIndicationLineSize();
    ctx.strokeStyle = shapeColor;
    renderer.drawReportArc(ctx, arc);
  }

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  renderer.drawpoint(ctx, shape.pt2);
  renderer.drawpoint(ctx, shape.pt3);

  // Caliper-mode overlay: N radial caliper boxes along the arc. Editor-mode only.
  if (inFullDisplay && shape.locating === 'caliper') {
    // Recompute the un-inflated arc — the `arc.r` above was bumped by
    // marginOffset for the visualization band; we want the def geometry.
    // The same arcSweep the width was seeded from -- that is the point. These
    // were two implementations of one idea, the drawing one correct and the
    // seeding one not, so the boxes disagreed with the width that placed them.
    const arcBase = arcSweep(shape.pt1, shape.pt2, shape.pt3);
    const a0 = arcBase.a0, a1 = arcBase.a1;
    // Per-caliper hits color the boxes (missed → gray) and the X markers
    // (inlier=green, outlier=red). See line.js for source priority.
    const hits = shape.cal_hits
      || (renderer.cal_hits_by_id && renderer.cal_hits_by_id[shape.id]);
    drawArcCalipers(ctx, arcBase.x, arcBase.y, arcBase.r, a0, a1, shape.caliper, renderer, shape.margin, hits);
    if (hits) drawCaliperHits(ctx, hits, renderer);
  }
}

// Inspection-mode draw — just the arc (no margin overlay). Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.arc. Caliper-mode reports
// carry per-caliper hits (shape.cal_hits); overlay them when the System_Setting
// flag is on.
export function drawInspection(ctx, shape, renderer) {
  let arc = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  ctx.lineWidth = renderer.getIndicationLineSize();
  renderer.drawReportArc(ctx, arc);
  if (renderer.show_caliper_hits !== false && shape.cal_hits) {
    drawCaliperHits(ctx, shape.cal_hits, renderer);
  }
}
