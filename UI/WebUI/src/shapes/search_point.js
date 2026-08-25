// Per-shape module: SEARCH_POINT.
// See shapes/line.js for the pattern + rationale.
import Color from 'color';
import { SHAPE_TYPE_COLOR } from 'JSSRCROOT/canvas/renderConst';
import { applyDefaultsFromFields, buildWhiteListKeyFromFields } from './_schemaHelpers';
import { edgeField, drawSingleCaliperBox, drawCaliperHits } from './_caliperFields';
export { SearchPointPropertySheet as PropertySheet } from './_propertySheet/SearchPointPropertySheet';

export const type = 'search_point';

// search_far has a migration shim: legacy defs used `search_style` (0/1) before
// the boolean; preserve that mapping when the field is missing. The schema's
// `default` only fires when `derive` doesn't produce a value.
//
// `locating` (contour|caliper) is distinct from `locating_anchor` (a bool for
// "use this as the deformation-correction anchor"). They're both used by the
// core but unrelated semantically. When `locating == 'caliper'` the core runs
// a single caliper_measure along the search vector (no cal_count geometry,
// unlike line/arc), so search_point gets the `edge` sub-group only.
// Defaults match what the core RUNS (method='first', polarity='falling').
// The core's parse default is ANY, which used to be executed as falling and is
// now bidirectional -- see FeatureManager_sig360_circle_line.cpp.
export const fields = {
  angleDeg:         { editor: 'AngleRangeSetup' },
  search_far:       {
    editor: 'switch',
    default: false,
    derive: (shape) => (shape.search_style !== undefined ? shape.search_style == 1 : undefined),
  },
  locating: {
    editor: (ctx) => ({ __OBJ__: ctx.renderMethods.Dropdown_List, list: ['contour', 'caliper'] }),
    default: 'contour',
    normalize: (v) => (v === 'caliper' ? 'caliper' : 'contour'),
  },
  // See EverCheckCanvasComponent's seed: 'falling' is what the core has always
  // run here; ANY now means bidirectional and must be chosen deliberately.
  edge:             edgeField({ method: 'first', polarity: 'falling' }),
  locating_anchor:  { editor: 'switch', default: false, normalize: (v) => v === true },
  // Anchor corner tag (used by the morph: corner => 2D-localized, constrains both
  // axes; edge => 1D, constrains only along the search normal). Only meaningful
  // when locating_anchor is true. Default false (edge).
  anchor_corner:    { editor: 'switch', default: false, normalize: (v) => v === true },
  line_thickness_value: { skipEditor: true, default: 0, normalize: (v) => (typeof v === 'number' ? v : 0) },
  // Reference slot — search_point references one line (ref[0]). The schema
  // matches the legacy ref-button convention: outer __OBJ__:'div' renders
  // a header label ("參考物件"); inner '0' __OBJ__:'btn' renders the ref
  // entry's header as a clickable button (DefConfUI jsonChange routes the
  // click to ACT_EDIT_TAR_ELE_TRACE_UPDATE for ref-pick mode).
  ref: { editor: { __OBJ__: 'div', '0': { __OBJ__: 'btn', id: 'div', element: 'div' } } },
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
  // Flip the perpendicular for search_far to match the core's scan
  // direction (`vec.x *= -1; vec.y *= -1` in searchPoint_process), so the
  // arrow on the caliper box points where the scan actually walks.
  const scanSign = shape.search_far ? 1 : -1;
  let cnormal = { x: -vector.y * scanSign, y: vector.x * scanSign };
  let mag = shape.width / 2;
  let tangent = { x: vector.x, y: vector.y };  // unit, along width-bar
  vector.x *= mag;
  vector.y *= mag;

  let margin = renderer.getSearchDirectionLineSize();
  if (inFullDisplay) margin = shape.margin;

  const isCaliper = (shape.locating === 'caliper');

  if (!isCaliper) {
    // Contour mode: legacy thick margin band + offset visualization line.
    ctx.lineWidth = margin * 2;
    renderer.drawReportLine(ctx, {
      x0: shape.pt1.x - vector.x, y0: shape.pt1.y - vector.y,
      x1: shape.pt1.x + vector.x, y1: shape.pt1.y + vector.y,
    });

    ctx.lineWidth = renderer.getSearchDirectionLineSize();
    ctx.strokeStyle = shapeColor;
    let marginOffset = margin + ctx.lineWidth / 2;
    // The scanSign flip on `cnormal` is CALIPER-specific (it points the caliper
    // box's arrow along the core's scan direction). Reusing it for the contour
    // offset band put it 180deg off when search_far is false. Use the plain
    // perpendicular of the (unit) width-bar tangent instead.
    const cnC = { x: -tangent.y, y: tangent.x };
    renderer.drawReportLine(ctx, {
      x0: shape.pt1.x - vector.x + cnC.x * marginOffset, y0: shape.pt1.y - vector.y + cnC.y * marginOffset,
      x1: shape.pt1.x + vector.x + cnC.x * marginOffset, y1: shape.pt1.y + vector.y + cnC.y * marginOffset,
    });
  } else if (inFullDisplay) {
    // Caliper mode: single big caliper box covering the entire search area
    // (along width-bar tangent = shape.width, across edge = 2*margin).
    drawSingleCaliperBox(ctx, shape.pt1, tangent, cnormal, mag, margin, renderer);
  }

  if (drawSubObjs)
    renderer.drawShapeList(ctx, subObjs, next_ShapeColor, skip_id_list, shapeList, unitConvert, drawSubObjs, inFullDisplay);

  ctx.strokeStyle = 'gray';
  renderer.drawpoint(ctx, shape.pt1);
  if (shape.locating_anchor) {
    ctx.strokeStyle = 'red';
    renderer.draw_aimcross(ctx, shape.pt1, renderer.getPointSize() * 3, 0.3);
  }

  // Caliper-mode per-hit overlay (dots, not crosses — search_point clusters
  // many hits along a short search vector; crosses overlap visually).
  if (inFullDisplay && isCaliper) {
    const hits = shape.cal_hits
      || (renderer.cal_hits_by_id && renderer.cal_hits_by_id[shape.id]);
    if (hits) drawCaliperHits(ctx, hits, renderer, { style: 'dot' });
  }
}

// Inspection-mode draw — just a red cross at pt1. Extracted from
// renderUTIL.drawInspectionShapeList.case SHAPE_TYPE.search_point.
export function drawInspection(ctx, shape, renderer) {
  ctx.strokeStyle = 'rgba(179, 0, 0,0.5)';
  renderer.drawcross(ctx, shape.pt1, renderer.getPointSize() * 3);
  ctx.lineWidth = renderer.getIndicationLineSize();
  if (renderer.show_caliper_hits !== false && shape.cal_hits) {
    drawCaliperHits(ctx, shape.cal_hits, renderer, { style: 'dot' });
  }
}
