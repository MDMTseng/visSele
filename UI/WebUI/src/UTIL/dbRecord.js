// What the traceability DB is NOT for.
//
// The inspection record is sent to the DB as the whole report object, so every
// field the core attaches for the screen's benefit is archived along with the
// measurements -- forever, for every part, at twenty parts a second.
//
// cal_hits is the expensive one. It is a per-caliper overlay: one entry per
// caliper the fit tried, INCLUDING the ones that hit nothing (status 0 exists so
// the canvas can draw a placeholder where a caliper failed), so its size does
// not fall when the parts are good. Each entry serialises as
// {"x":…,"y":…,"st":…,"s":…} with cJSON's full-precision doubles, which is on
// the order of 60-80 bytes for four numbers whose real information content is a
// micron-resolution coordinate, a three-valued status and a 0..1 confidence.
//
// Nothing reads it back out of the DB. It exists to be drawn.
//
// ONE KEY. The core now carries every optional debug payload under a report's
// "extra" object -- caliper hits today, edge-strength profiles next -- so the
// archive excludes them by structure rather than by anyone remembering to add
// the next field to this list. 'cal_hits' stays for a core older than that
// change, which still sends it at the top level.
export const OVERLAY_ONLY_FIELDS = ['extra', 'cal_hits'];

// Prune with STRUCTURAL SHARING: subtrees that contain nothing to remove are
// returned as-is, not copied. This runs on the live redux report at the
// inspection rate, and a deep clone per part would be a bigger cost than the
// bytes it saves. Nothing here mutates the input -- the same objects are still
// on screen while this runs.
export function stripOverlayOnly(node, fields = OVERLAY_ONLY_FIELDS) {
  if (Array.isArray(node)) {
    let out = null;
    for (let i = 0; i < node.length; i++) {
      const v = stripOverlayOnly(node[i], fields);
      if (out === null && v !== node[i]) out = node.slice(0, i);
      if (out !== null) out.push(v);
    }
    return out === null ? node : out;
  }
  if (node === null || typeof node !== 'object') return node;

  let out = null;
  for (const k of Object.keys(node)) {
    if (fields.indexOf(k) !== -1) {
      if (out === null) out = { ...node };
      delete out[k];
      continue;
    }
    const v = stripOverlayOnly(node[k], fields);
    if (v !== node[k]) {
      if (out === null) out = { ...node };
      out[k] = v;
    }
  }
  return out === null ? node : out;
}
