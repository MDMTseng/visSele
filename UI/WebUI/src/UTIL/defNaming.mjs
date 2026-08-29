// Path and name arithmetic for def files. No imports, on purpose.
//
// These are the two places where a wrong string does not look wrong: a sidecar
// path that resolves to a file that exists but is the wrong picture, and a new
// object's file name that resolves to a recipe somebody is still running. Both
// have already happened. Kept dependency-free so `tools/webctl/unit_defnaming.mjs`
// can exercise them in plain Node -- no browser, no core, no fixtures.

// <defModelPath> -> the sidecar .png path, stripping an extension ONLY when the
// LAST path segment has one.
//
// defModelPath is normally the stem with NO extension (it is what LD sends as
// `imgsrc`), so a bare /\.[^.]+$/ has nothing to strip and eats backwards to
// whatever dot it can find -- and `[^.]` excludes dots, not separators, so it
// will happily swallow directory names. Installed under a folder called `X2.0`
// that turned <root>/X2.0/data/testNew2 into <root>/X2.png: the core could not
// read the template, SBM training failed, and the def silently ran on sig360
// while the studio reported "no features extracted". A dot in a FOLDER name did it.
export function refPngPathOf(defModelPath) {
  const p = String(defModelPath);
  const cut = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'));
  const dir = p.slice(0, cut + 1);          // '' when there is no separator
  const base = p.slice(cut + 1);
  const dot = base.lastIndexOf('.');
  // dot > 0 so a dotfile keeps its name; the segment must have an ext to lose one.
  return dir + (dot > 0 ? base.slice(0, dot) : base) + '.png';
}

// Characters a file name may not carry on Windows, plus the separators.
const ILLEGAL = /[\\/:*?"<>|]/g;

// The name a newly created object should be saved under, given what the folder
// already holds. `taken` is a Set of lower-cased base names (no extension).
//
// Returns the plain name when it is free, and appends [1], [2], ... when it is
// not. It never returns a name that is in `taken`, which is the whole point:
// TAKE turns the editor into a DIFFERENT part, and offering the previous
// recipe's file name would let one careless save overwrite a recipe that is
// running on a line.
export function nextFreeName(rawName, taken, limit = 999) {
  const safe = String(rawName == null ? '' : rawName).replace(ILLEGAL, '_').trim() || 'Sample';
  const has = (n) => !!(taken && typeof taken.has === 'function' && taken.has(n.toLowerCase()));
  if (!has(safe)) return safe;
  for (let i = 1; i < limit; i++) {
    const cand = safe + '[' + i + ']';
    if (!has(cand)) return cand;
  }
  // Every suffix up to the limit is taken. Returning the plain name would be a
  // silent collision, so return something that cannot collide and is obviously
  // machine-made, and let the save browser's exists-prompt be the backstop.
  return safe + '[' + Date.now() + ']';
}

// A folder listing (whatever shape the FB reply happens to use) -> the Set that
// nextFreeName wants. Tolerant by design: this runs on a reply from the core,
// and a listing that cannot be parsed must degrade to "nothing is taken" rather
// than throw inside a capture.
export function takenNamesFrom(entries) {
  const out = new Set();
  // Array.isArray, not `entries || []`: a for...of over a number or a plain
  // object THROWS, and the one caller is inside a capture whose failure mode
  // must be "no names are taken", never an exception on the way to a photo.
  if (!Array.isArray(entries)) return out;
  for (const f of entries) {
    const nm = (typeof f === 'string') ? f : (f && (f.name || f.path));
    if (!nm) continue;
    out.add(String(nm).split(/[\\/]/).pop().replace(/\.[^.]+$/, '').toLowerCase());
  }
  return out;
}
