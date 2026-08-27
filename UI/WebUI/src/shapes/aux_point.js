// Per-shape module: AUX_POINT.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { closestPointOnPoints } from 'UTIL/MathTools';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { buildWhiteListKeyFromFields } from './_schemaHelpers';

// Endpoint for the dashed crosshair drawn from the aux_point's intersection
// back toward each ref shape. Two-endpoint shapes (line) pick the closer of
// their endpoints — same as the legacy code. search_point has no endpoint
// pair, so we anchor the dash at its pt1 (the search target / virtual-line
// anchor; matches the core's ParseLocatePosition convention for SEARCH_POINT
// — see CORE0_1_CAVEATS.md §I for the line vs search_point convention).
// Returns null when the shape can't supply a meaningful foot — caller skips
// drawing that dash (intersection dot still renders).
function refFoot(refShape, fromPoint) {
  if (!refShape) return null;
  if (refShape.type === 'line' && refShape.pt1 && refShape.pt2) {
    return closestPointOnPoints(fromPoint, [refShape.pt1, refShape.pt2]);
  }
  if (refShape.type === 'search_point' && refShape.pt1) {
    return refShape.pt1;
  }
  return null;
}

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

// canvasCtrl: aux_point refs lines or search_points (their intersection).
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

      for (const sub of subObjs) {
        const foot = refFoot(sub, point);
        if (!foot) continue;
        ctx.beginPath();
        ctx.moveTo(point.x, point.y);
        ctx.lineTo(foot.x, foot.y);
        ctx.stroke();
      }
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
  // The core's own point first. It reports x/y for every non-NA aux_point and
  // the UI used to ignore them, re-deriving the intersection in JS -- so the
  // cross was drawn where the BROWSER thought the lines met, not where the
  // machine measured.
  //
  // The JS derivation stays as a fallback for a report that has no point: a
  // core older than this change, and the def-conf preview before anything has
  // run. It is drawn differently so the two cannot be mistaken for each other.
  const reported = shape.reported_pt;
  const derived  = renderer.db_obj.auxPointParse(shape, shapeList);
  let point = reported || derived;
  const isReported = !!reported;
  if (point !== undefined && subObjs.length == 2) {
    ctx.setLineDash([renderer.getPrimitiveSize(), renderer.getPrimitiveSize()]);
    for (const sub of subObjs) {
      const foot = refFoot(sub, point);
      if (!foot) continue;
      ctx.beginPath();
      ctx.moveTo(point.x, point.y);
      ctx.lineTo(foot.x, foot.y);
      ctx.stroke();
    }
    ctx.setLineDash([]);
    // Gray for the core's answer, hollow amber for a JS guess. A guess that
    // looks identical to a measurement is how the divergence stayed invisible.
    ctx.strokeStyle = isReported ? 'gray' : 'rgba(255, 190, 60, 0.9)';
    renderer.drawcross(ctx, point, renderer.getPointSize() * (isReported ? 2 : 1.4));
  }
  // na_reason is printed centrally by renderUTIL.drawNAReason, which is
  // called for every NA shape. The copy that used to live here called
  // drawText(), whose hard-coded lineWidth=1 is a one-MILLIMETRE stroke on
  // this canvas -- the label rendered as a black mass across the frame.

}

// Inherent-shape-list draw — a black rect at the resolved aux_point.
// Extracted from renderUTIL.drawInherentShapeList.case SHAPE_TYPE.aux_point.
export function drawInherent(ctx, shape, renderer, { shapeList } = {}) {
  // X7: this passed NO shapeList, so auxPointParse fell back to the DEF and the
  // marker was drawn at the taught position while everything around it was
  // drawn at the measured one. During inspection that is a black rect sitting
  // where the part is not.
  //
  // The core's reported point wins here too, for the same reason as above.
  let point = shape.reported_pt
    || (shapeList ? renderer.db_obj.auxPointParse(shape, shapeList)
                  : renderer.db_obj.auxPointParse(shape));
  if (point != null) {
    ctx.strokeStyle = 'black';
    renderer.drawpoint(ctx, point, 'rect');
  }
}
