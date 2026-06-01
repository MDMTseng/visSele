// Per-shape `fields` schema helpers (Q3 whiteListKey-typed-schema).
//
// A shape module can declare ONE `fields` object instead of two parallel
// functions. Each entry:
//   { editor, default?, derive?, normalize?, skipEditor?, coerce?, onChange? }
//     editor    — JsonEditBlock spec; either a literal ('switch', 'input', ...)
//                 or `(ctx) => spec` when it depends on renderMethods.
//     default   — value used when the field is undefined (after derive).
//     derive    — `(shape) => value | undefined`; legacy-migration hook that
//                 runs before `default` (e.g. map old `search_style` → bool).
//     normalize — `(currentValue) => newValue`, runs every time even if defined
//                 (used for legacy values that must be coerced, e.g. caliper).
//     skipEditor — true if the field has a default but no editor row (rare).
//     coerce    — `(evt, type) => value`; produces the new value from a UI
//                 event. Overrides the generic per-type extraction (parseFloat
//                 for input-number, evt.target.checked for switch, etc.) when
//                 the shape needs a custom mapping (e.g. arc.direction stores
//                 -1/+1 from a boolean switch). Return undefined to abort.
//     onChange  — `(obj, prevVal, ctx) => void`; runs AFTER the new value is
//                 written to `obj`. Use for cross-field side effects: derived
//                 fields (measure limit coupling), dep-key visibility (measure
//                 back_value_setup adding/removing the _b keys), etc.
//
// Both `applyDefaults(shape, fields)` and `buildWhiteListKey(fields, ctx)`
// derive from the same declaration, so adding a field is a one-spot edit.
// Field-level side effects via `coerce`/`onChange` keep DefConfUI's jsonChange
// dispatcher generic — adding a new field with custom behavior is a one-file
// change in the shape module.
//
// Field declaration order is preserved in the editor (object insertion order).

export function applyDefaultsFromFields(shape, fields) {
  const out = { ...shape };
  for (const key of Object.keys(fields)) {
    const f = fields[key];
    if (out[key] === undefined && typeof f.derive === 'function') {
      const d = f.derive(out);
      if (d !== undefined) out[key] = d;
    }
    if (out[key] === undefined && 'default' in f) out[key] = f.default;
    if (typeof f.normalize === 'function') out[key] = f.normalize(out[key]);
  }
  return out;
}

// Resolve a field's editor spec for the current context. Returns the spec to
// render or `undefined` to hide the field entirely. `editor` as a function may
// return undefined to express "hidden in this context" (e.g. caliper sub-group
// hidden when locating == 'contour'); the literal-spec form is always shown.
export function resolveFieldEditor(field, ctx) {
  if (!field || field.skipEditor || field.editor === undefined) return undefined;
  return (typeof field.editor === 'function') ? field.editor(ctx) : field.editor;
}

export function buildWhiteListKeyFromFields(fields, ctx) {
  const slice = {};
  for (const key of Object.keys(fields)) {
    const spec = resolveFieldEditor(fields[key], ctx);
    if (spec === undefined) continue;
    slice[key] = spec;
  }
  return slice;
}

// Generic per-event value extraction. Returns the new value or `undefined` to
// signal "abort, don't update". Mirrors the legacy DefConfUI jsonChange branches.
export function defaultValueFromEvent(evt, type) {
  switch (type) {
    case 'input-number': {
      const n = parseFloat(evt.target.value);
      return isNaN(n) ? undefined : n;
    }
    case 'input':
    case 'Dropdown_List':
      return evt.target.value;
    case 'switch':
      return evt.target.checked;
    default:
      return undefined;
  }
}

// Generic field-change dispatcher used by DefConfUI.GenTarEditUI.jsonChange.
// `field` is the resolved per-key entry from the shape module (see
// shapes/index.js#fieldFor) or undefined when no decl exists (e.g. base keys
// like `name`/`margin`). Computes the new value (field.coerce overrides
// defaultValueFromEvent), writes it, then fires field.onChange for any
// cross-field side effects. Returns true if the shape was mutated (caller
// should propagate via ec_canvas.SetShape).
export function applyFieldChange(field, target, evtType, evt, ctx) {
  const keyTrace = target.keyTrace;
  const lastKey = keyTrace[keyTrace.length - 1];

  const newVal = (field && typeof field.coerce === 'function')
    ? field.coerce(evt, evtType)
    : defaultValueFromEvent(evt, evtType);

  if (newVal === undefined) return false;

  const prevVal = target.obj[lastKey];
  target.obj[lastKey] = newVal;

  if (field && typeof field.onChange === 'function') {
    field.onChange(target.obj, prevVal, { ...ctx, evt, target, lastKey });
  }
  return true;
}
