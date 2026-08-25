// Which 製程's control-margin row applies to a part.
//
// Its own module because BOTH sides need the same answer and they live far
// apart: InspectionUI folds the row into shape_list (which the WIRE DEF sent to
// the core is generated from, so it decides how the machine SORTS), and
// UICtrlReducer's resultGrading overlays it to decide how the screen COLOURS.
//
// They used to disagree. InspectionUI took the FIRST matching tag (.find) and
// the reducer took the LAST (a reduce that kept overwriting). A part carrying
// two tags that both have rows was therefore judged by the core on one 製程 and
// painted on screen by the other -- order-dependent, and enough on its own to
// make the mismatch counter fire with nobody having touched a limit.
//
// FIRST match wins, because that is what the wire def has always carried and
// therefore what the machine has actually been doing. When the display and the
// sorter disagree, the display is the half to change.
//
// `ambiguous` lists every tag that had a row when more than one did. That is a
// configuration question only a person can answer -- two 製程 claiming the same
// measurement is not something to resolve silently by document order, even
// though document order is what we do until someone fixes it.
export function pickCtrlMargin(tags, control_margin_info) {
  if (!control_margin_info) return { tag: undefined, info: undefined, ambiguous: [] };
  const list = Array.isArray(tags) ? tags : [];
  const hits = list.filter((t) => control_margin_info[t] !== undefined);
  return {
    tag: hits[0],
    info: hits.length ? control_margin_info[hits[0]] : undefined,
    ambiguous: hits.length > 1 ? hits : [],
  };
}
