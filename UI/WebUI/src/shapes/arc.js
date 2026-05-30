// Per-shape module: ARC.
// See shapes/line.js for the pattern + rationale.
export const type = 'arc';

export function applyDefaults(shape) {
  let out = { ...shape };
  if (out.locating !== 'caliper') out.locating = 'contour';
  return out;
}
