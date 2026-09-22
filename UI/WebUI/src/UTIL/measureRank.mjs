// What level a measurement belongs to, and what a level shows.
//
// NO RANK MEANS RANK 0. It is the base level, not the absence of one, and the
// def editor has always read it that way (DefConfUI: `(shape.rank===undefined)
// ? 0 : shape.rank`). The inspection screen did not: it treated an unranked
// measurement as "outside the system", always drawn and never counted as a
// level. The two halves of the app therefore disagreed about the same file.
//
// What that cost, on a real recipe: a def with one ranked item and twenty
// unranked ones offered ONE level, and the slider and the badge both hide
// themselves when there is nothing to choose between -- so a machine whose
// recipe had lost its rank tags looked exactly like a machine whose recipe
// never had any. The control that would have shown the difference was the one
// that disappeared. Counting the unranked as level 0 makes that def two levels,
// which is what it is.
//
// Numeric strings are accepted. A def edited by hand or written by an older
// tool can carry "3", and Number.isFinite("3") is false -- which silently
// dropped the whole level, with nothing on screen to say so.

export const RANK_BASE = 0;

// The level of one measurement, shape or report row.
export function rankOf(o) {
  if (!o) return RANK_BASE;
  const n = Number(o.rank);
  return Number.isFinite(n) ? n : RANK_BASE;
}

// The levels a def actually uses, ascending, base level included.
//
// The base is only included when something is actually sitting on it: a def
// where every item carries a rank has no level 0, and inventing one would put
// an empty level at the bottom of every slider.
export function ranksOf(list) {
  const set = new Set();
  for (const d of (list || [])) {
    if (!d) continue;
    set.add(rankOf(d));
  }
  return [...set].sort((a, b) => a - b);
}

// Is this measurement shown at the given viewing level?
//
// `limit` of undefined or Infinity shows everything, which is what a screen
// that has not chosen a level yet means.
export function rankShown(o, limit) {
  if (limit === undefined || limit === null) return true;
  return rankOf(o) <= limit;
}
