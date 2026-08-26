// The score a match has to beat, and where that number comes from.
//
// It is asked in three places -- the sweep's bar, the studio's verdict, and the
// inspection panel's headroom colouring -- and all three have to mean the same
// thing or the same def reads as healthy on one screen and marginal on another.
//
// THE FLOOR DEPENDS ON WHICH LOCATOR RAN, and the two are nowhere near each
// other:
//
//   shape_based  line2Dup's own gate, `shape_min_score`, 0-100 in the def.
//                Core default 50, i.e. 0.50.
//   sig360       `sig_match_sim_thres`, already a fraction.
//                Core default when the KEY IS ABSENT is 0.7 -- not the 0.9 the
//                editor shows as its starting value. They rarely diverge in
//                practice because this WebUI seeds 0.9 and MISC_Util emits it,
//                so a def saved here always carries the key; a def that has
//                never been through this editor is the case where it matters.
//
// Getting this wrong is not cosmetic. A shape_based def scoring 0.986 has
// enormous headroom over 0.50 and none at all over an assumed 0.99 -- and the
// whole reason the score is on screen is to judge headroom.
export const CORE_SIG_MATCH_THRES_DEFAULT = 0.7;    // JFetch_NUMBER_ex(root, "sig_match_sim_thres", 0.7)
export const CORE_SHAPE_MIN_SCORE_DEFAULT = 0.50;   // shape_min_score = 50.0f, /100

export function acceptanceFloor(edit_info) {
  const ei = edit_info || {};
  if (ei.locating_engine === 'shape_based') {
    const v = ei.shape_min_score;
    return {
      floor: Number.isFinite(v) ? v / 100 : CORE_SHAPE_MIN_SCORE_DEFAULT,
      key: 'shape_min_score',
      engine: 'shape_based',
      explicit: Number.isFinite(v),
    };
  }
  const v = ei.sig_match_sim_thres;
  return {
    floor: Number.isFinite(v) ? v : CORE_SIG_MATCH_THRES_DEFAULT,
    key: 'sig_match_sim_thres',
    engine: 'sig360',
    explicit: Number.isFinite(v),
  };
}

// How much room a score has above the floor, 0..1. This is what a bar should
// draw -- NOT the score itself, and not the score rescaled to whatever range a
// particular run happened to produce.
export function headroom(score, floor) {
  if (!Number.isFinite(score) || !Number.isFinite(floor) || floor >= 1) return NaN;
  return Math.max(0, Math.min(1, (score - floor) / (1 - floor)));
}
