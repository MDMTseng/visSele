// Per-shape module: LINE.
// Step 1 of the per-shape vertical-slice keystone (see OPENQUESTION). Each shape
// type owns one file declaring its defaults (Shape_Attr_Fill), eventually its
// editor schema (whiteListKey), and any per-shape pure logic. Adding a new
// primitive then becomes a single-file change.
//
// applyDefaults: PURE — may return the input untouched or a shallow clone with
// missing fields filled. Mirrors the legacy Shape_Attr_Fill behavior verbatim.
export const type = 'line';

export function applyDefaults(shape) {
  let out = { ...shape };
  if (out.vertex_touch_searching !== true) out.vertex_touch_searching = false;
  // caliper locating mode: "contour" (legacy) | "caliper". Normalize so the
  // dropdown always has a string; core treats anything != "caliper" as contour.
  if (out.locating !== 'caliper') out.locating = 'contour';
  return out;
}
