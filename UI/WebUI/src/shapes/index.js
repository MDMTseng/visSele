// Per-shape registry — central dispatcher for the per-shape vertical slice.
// Adding a new primitive becomes: create src/shapes/<type>.js exporting
// `applyDefaults` (and later: `whiteListKey`, `draw`, `canvasCtrl`...) then
// register it here. The legacy InspectionEditorLogic.Shape_Attr_Fill +
// DefConfUI's whiteListKey switch will delegate to this registry.
//
// Types intentionally NOT registered (no per-shape defaults today, pass-through):
//   aux_point, aux_line, point, sign360
//
// This is OPENQUESTION's "PER-SHAPE SCHEMA + render-stability (keystone step 1)".

import * as line from './line';
import * as arc from './arc';
import * as search_point from './search_point';
import * as measure from './measure';

export const SHAPE_REGISTRY = { line, arc, search_point, measure };

// Resolve the per-shape module by type. Returns undefined for unregistered types
// — callers must treat that as a pass-through (no defaults / no schema).
export function getShapeModule(type) {
  return SHAPE_REGISTRY[type];
}
