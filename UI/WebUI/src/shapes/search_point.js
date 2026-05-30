// Per-shape module: SEARCH_POINT.
// See shapes/line.js for the pattern + rationale.
export const type = 'search_point';

export function applyDefaults(shape) {
  let out = { ...shape };
  if (out.search_far === undefined) {
    if (out.search_style === undefined) {
      out.search_far = false;
    } else {
      out.search_far = (out.search_style == 1) ? true : false;
    }
  }
  if (out.locating_anchor != true) out.locating_anchor = false;
  if (typeof out.line_thickness_value != 'number') out.line_thickness_value = 0;
  return out;
}
