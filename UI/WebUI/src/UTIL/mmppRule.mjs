// Which mm/px a def measures in. No imports, on purpose.
//
// Every dimension in a recipe is px * mmpp, so a wrong value here does not
// produce a visible fault -- it produces a whole def measured to a consistent,
// plausible, wrong scale. That is why the ordering is a named function with a
// test rather than a chain of `||` inside a class method.
//
// The ordering, strongest evidence first:
//
//   1. a sig360 report      a measurement OF THIS IMAGE, so nothing beats it
//   2. the instrument       data/lens_calib.json -- this machine, this lens,
//                           this standoff. Set by TAKE when the frame came off
//                           the camera, and deliberately NOT set when the def's
//                           own image is reused (that picture really does belong
//                           to the def's scale).
//   3. cam_param            mmpb2b/ppb2b carried in the def; better than nothing
//   4. 1                    last resort, and it means "unscaled pixels"
//
// The sig360 value arrives as 1 when there is no report (getsig360info_mmpp
// returns 1 from its catch), so 1 has to be read as "absent" rather than as a
// scale -- a real machine is never 1 mm per pixel.
export function pickMmpp({ sigMmpp, instrumentMmpp, camParam } = {}) {
  const ok = (v) => Number.isFinite(v) && v > 0;

  if (ok(sigMmpp) && sigMmpp !== 1) return sigMmpp;
  if (ok(instrumentMmpp)) return instrumentMmpp;

  const cp = camParam;
  if (cp && Number.isFinite(cp.mmpb2b) && Number.isFinite(cp.ppb2b) && cp.ppb2b > 0)
    return cp.mmpb2b / cp.ppb2b;

  return ok(sigMmpp) ? sigMmpp : 1;
}

// data/lens_calib.json -> mm/px.
//
// um_per_px is what lens calibration produces and what the core writes back;
// `m` (px/mm) is the same number inverted and is kept as a fallback for older
// files. Returns undefined when the file says neither -- the caller must SAY SO
// rather than substitute a plausible number, because a def with no honest scale
// is worse than one that cannot measure yet.
export function mmppFromLensCalib(d) {
  if (!d) return undefined;
  const um = +d.um_per_px;
  if (Number.isFinite(um) && um > 0) return um / 1000;
  const m = +d.m;
  if (Number.isFinite(m) && m > 0) return 1 / m;
  return undefined;
}
