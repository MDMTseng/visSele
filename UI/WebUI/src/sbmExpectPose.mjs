// Where the part should BE, given a perturbation whose magnitude we chose.
// No imports, on purpose -- see tools/webctl/unit_expectpose.mjs.
//
// The studio picked its answer as poses[0], the highest-scoring candidate. That
// is a RANKING, not a part. With one object in frame the two coincide; with
// several they do not, and then
//
//     moved = thisStep.rotate - baseline.rotate
//
// is the angle between two different objects. Nothing downstream can tell:
// every step located something, every step scored well, and the curve looks
// like a measurement.
//
// The fix is that the perturbation is OURS. We chose the rotation, so we know
// where the part must have gone, and a candidate can be identified by position
// instead of by rank. That also does the job of excluding interference: a
// candidate that is nowhere near where the part must be is not the part,
// whatever it scored.
//
// TestPerturb.h applies geometry about the IMAGE CENTRE, in this order:
// rotate, then scale, then skew. This mirrors it.
//
// EVERYTHING HERE IS IN IMAGE MM -- the frame the reported poses are actually
// in. That sentence used to say "object-frame mm", and it was wrong: an
// unperturbed test1 reports cx/cy = (15.0250, 9.3044), which is its
// def_image_reg, not the (0,0) an object-frame pose would be. The pivot was
// therefore translated into a frame the poses were never in, and every
// prediction came out about one def_image_reg away -- 128 px on this bench,
// against a 20 px tolerance, so every candidate was rejected as interference.
//
// Only rot_deg = 0 survived, because with no rotation the prediction is the
// starting point whatever the pivot is. A sweep that reports "usable range
// 0.00deg ~ 0.00deg" is that bug, not a fragile locator.

// The image centre, in the same frame the poses use. No registration in it:
// translating by def_image_reg is what moved the pivot into the wrong frame.
export function imageCentre(widthPx, heightPx, mmpp) {
  return { x: (widthPx / 2) * mmpp, y: (heightPx / 2) * mmpp };
}

// Kept so an older caller still resolves; the registration argument is ignored
// on purpose and the name is the mistake it is named after.
export function imageCentreInObjectFrame(widthPx, heightPx, mmpp) {
  return imageCentre(widthPx, heightPx, mmpp);
}

// Where `from` ends up when the SCENE is perturbed. `from` and the result are
// object-frame mm; `pivot` is imageCentreInObjectFrame.
//
// gain, bias and noise are absent on purpose: they change what the pixels say,
// not where the part is, so a prediction that moved for them would be wrong.
export function expectedPosition(from, pivot, perturb) {
  const p = perturb || {};
  let x = from.cx - pivot.x;
  let y = from.cy - pivot.y;

  // NEGATED. getRotationMatrix2D's positive angle is counter-clockwise in a
  // y-up sense, and an image's y points down, so a +rot_deg perturbation moves
  // a point by -rot_deg in these coordinates. Measured against the core on five
  // angles; with the sign the other way the prediction lands on the far side of
  // the pivot and nothing is ever picked.
  const deg = Number.isFinite(p.rot_deg) ? -p.rot_deg : 0;
  if (deg) {
    const t = deg * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
    const nx = x * c - y * s;
    y = x * s + y * c;
    x = nx;
  }
  const sc = Number.isFinite(p.scale) ? p.scale : 1;
  if (sc !== 1) { x *= sc; y *= sc; }

  const sk = Number.isFinite(p.skew) ? p.skew : 0;
  if (sk) x += sk * y;

  return { x: x + pivot.x, y: y + pivot.y };
}

// Choose the candidate that IS the part, and say so when none of them is.
//
// `poses` are the reported objects; `expect` is where the part must be;
// `tolMm` is how far off it may be. Returns the index, or -1.
//
// Nearest-within-tolerance rather than nearest: with the tolerance, "the part
// is not in this frame" is an answer the caller can act on. Without it, the
// nearest of four wrong candidates is still returned and still looks located,
// which is the failure this whole function exists to remove.
export function pickByPosition(poses, expect, tolMm) {
  if (!Array.isArray(poses) || !poses.length || !expect) return -1;
  const tol = Number.isFinite(tolMm) && tolMm > 0 ? tolMm : Infinity;
  let best = -1, bestD = Infinity;
  for (let i = 0; i < poses.length; i++) {
    const p = poses[i];
    if (!p || !Number.isFinite(p.cx) || !Number.isFinite(p.cy)) continue;
    const d = Math.hypot(p.cx - expect.x, p.cy - expect.y);
    if (d < bestD) { bestD = d; best = i; }
  }
  return (best >= 0 && bestD <= tol) ? best : -1;
}

// How far the nearest candidate was, for the message when none passed. A
// rejection that does not say "the closest was 4.2 mm away" sends somebody to
// tune a threshold that has nothing to do with it.
export function nearestDistance(poses, expect) {
  if (!Array.isArray(poses) || !expect) return NaN;
  let d = Infinity;
  for (const p of poses) {
    if (!p || !Number.isFinite(p.cx) || !Number.isFinite(p.cy)) continue;
    d = Math.min(d, Math.hypot(p.cx - expect.x, p.cy - expect.y));
  }
  return Number.isFinite(d) ? d : NaN;
}
