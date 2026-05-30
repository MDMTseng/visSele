// Per-shape `fields` schema helpers (Q3 whiteListKey-typed-schema).
//
// A shape module can declare ONE `fields` object instead of two parallel
// functions. Each entry:
//   { editor, default?, derive?, normalize?, skipEditor? }
//     editor    — JsonEditBlock spec; either a literal ('switch', 'input', ...)
//                 or `(ctx) => spec` when it depends on renderMethods.
//     default   — value used when the field is undefined (after derive).
//     derive    — `(shape) => value | undefined`; legacy-migration hook that
//                 runs before `default` (e.g. map old `search_style` → bool).
//     normalize — `(currentValue) => newValue`, runs every time even if defined
//                 (used for legacy values that must be coerced, e.g. caliper).
//     skipEditor — true if the field has a default but no editor row (rare).
//
// Both `applyDefaults(shape, fields)` and `buildWhiteListKey(fields, ctx)`
// derive from the same declaration, so adding a field is a one-spot edit.
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

export function buildWhiteListKeyFromFields(fields, ctx) {
  const slice = {};
  for (const key of Object.keys(fields)) {
    const f = fields[key];
    if (f.skipEditor || f.editor === undefined) continue;
    slice[key] = (typeof f.editor === 'function') ? f.editor(ctx) : f.editor;
  }
  return slice;
}
