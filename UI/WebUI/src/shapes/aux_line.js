// Per-shape module: AUX_LINE.
// See shapes/line.js for the pattern + rationale.
import { buildWhiteListKeyFromFields } from './_schemaHelpers';

export const type = 'aux_line';

// Two ref slots: ref[0] = arc, ref[1] = search_point. See aux_point.js for
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

// canvasCtrl: aux_line refs an arc + a search_point.
export function availableRefShapes(shapeList /*, subtype */) {
  return shapeList.filter((s) => s.type === 'line');
}

// (no fitCameraCenter — aux_line doesn't pan-to-shape in the legacy code.)

// (no applyDefaults — legacy Shape_Attr_Fill has no case for aux_line; pass-through.)

// Draw an aux_line — extracted verbatim from renderUTIL.drawShapeList.case SHAPE_TYPE.aux_line.
// Aux_line connects the pt1 of two referenced shapes with a dashed gray line.
export function draw(ctx, shape, renderer, {
  inFullDisplay = true, shapeList = [], next_ShapeColor = null,
  skip_id_list = [], unitConvert = { unit: 'mm', mult: 1 }, drawSubObjs = false,
} = {}) {
  let db_obj = renderer.db_obj;
  let subObjs = shape.ref
    .map((ref) => db_obj.FindShape('id', ref.id, shapeList))
    .map((idx) => { return idx >= 0 ? shapeList[idx] : null; });
  if (drawSubObjs)
    renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs, inFullDisplay);
  if (shape.id === undefined) return;

  if (subObjs.length == 2) { // Draw crosssect line
    ctx.setLineDash([renderer.getPrimitiveSize(), renderer.getPrimitiveSize()]);
    ctx.strokeStyle = 'gray';
    ctx.beginPath();
    ctx.moveTo(subObjs[0].pt1.x, subObjs[0].pt1.y);
    ctx.lineTo(subObjs[1].pt1.x, subObjs[1].pt1.y);
    ctx.stroke();
    ctx.setLineDash([]);
    //renderer.drawpoint(ctx, point);
  }
}
