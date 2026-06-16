// Per-shape registry — central dispatcher for the per-shape vertical slice.
// Adding a new primitive becomes: create src/shapes/<type>.js exporting
// `applyDefaults` (and later: `whiteListKey`, `draw`, `canvasCtrl`...) then
// register it here. The legacy InspectionEditorLogic.Shape_Attr_Fill +
// DefConfUI's whiteListKey switch will delegate to this registry.
//
// Types registered for draw only (no applyDefaults — legacy Shape_Attr_Fill has
// no case for them, so getShapeModule returns the module but applyDefaults is
// undefined and the InspectionEditorLogic wrapper passes through):
//   aux_point, aux_line
// Types NOT registered (no per-shape defaults or draw today, pass-through):
//   point, sign360
//
// This is OPENQUESTION's "PER-SHAPE SCHEMA + render-stability (keystone step 1)"
// and "per-shape draw" (step 3).

import * as line from './line';
import * as arc from './arc';
import * as search_point from './search_point';
import * as measure from './measure/index.js';
import * as aux_point from './aux_point';
import * as aux_line from './aux_line';
import * as loc_include from './loc_include';
import * as loc_exclude from './loc_exclude';

export const SHAPE_REGISTRY = { line, arc, search_point, measure, aux_point, aux_line, loc_include, loc_exclude };

// Resolve the per-shape module by type. Returns undefined for unregistered types
// — callers must treat that as a pass-through (no defaults / no schema).
export function getShapeModule(type) {
  return SHAPE_REGISTRY[type];
}

// Resolve a per-field schema entry for a shape. Most shapes have a flat
// `fields` decl keyed by property name; measure dispatches by subtype via
// its own `fieldFor` (subtype slices like circle_info.info_type can declare
// their own fields/onChange in the future). Returns undefined when no decl
// exists — callers pass that through to `applyFieldChange` which then falls
// back to the generic per-event-type value extraction.
export function fieldFor(edit_tar, key) {
  const mod = getShapeModule(edit_tar && edit_tar.type);
  if (!mod) return undefined;
  if (typeof mod.fieldFor === 'function') return mod.fieldFor(edit_tar, key);
  return mod.fields ? mod.fields[key] : undefined;
}
