// When an idle CI inspection should take itself off the machine. No imports.
//
// This is the only thing in the app that stops the camera on its own, so it is
// the only thing standing between "somebody walked away" and a machine grabbing
// and inspecting frames until someone comes back. It ran with no test at all.
//
// Extracted from InspectionUI.checkAutoExitForCI so the DECISION can be
// exercised without a browser, a core or a 30-second wait. The component keeps
// the effects -- stopping the camera, showing the message, leaving the screen --
// which is the half that genuinely needs a browser.
//
// TWO TRIGGERS, both time-based:
//
//   no_obj    nothing on the plate for noObjMs. The line is idle.
//   same_obj  the SAME object still present after sameObjMs. Somebody put a
//             part down, walked off, and left it there. Tracking-window
//             identity, so a stream of different parts never trips it however
//             long it runs.
//
// AND ONE THING IT CANNOT DO, which is worth knowing before trusting it: this
// is driven by arriving reports. No report, no decision. That is harmless when
// the core has stopped producing (nothing is being burned either), but it does
// mean the watchdog cannot rescue a session whose reports have stalled while
// the camera keeps grabbing.

// Returns { reason, noObjSince } -- reason is 'no_obj', 'same_obj' or null.
//
// noObjSince is carried in and out rather than held here: the caller owns the
// clock and the state, and a rule that remembers things cannot be tested by
// calling it.
export function autoExitDecision({
  now, hasObject, noObjSince, trackingWindow,
  noObjMs = 30000, sameObjMs = 60000,
} = {}) {
  const t = Number.isFinite(now) ? now : 0;

  // The plate is empty.
  //
  // The FIRST empty report only starts the clock -- it does not exit. An
  // inspection that begins on an empty plate would otherwise leave immediately,
  // before the operator has put anything down.
  let since = noObjSince;
  if (!hasObject) {
    if (since == null) return { reason: null, noObjSince: t };
    if (t - since > noObjMs) return { reason: 'no_obj', noObjSince: since };
  } else {
    since = null;
  }

  // The same object, sitting there.
  //
  // Entries stay in the tracking window only while they are still being seen
  // (the reducer ages them out), so an entry that is present with an old
  // add_time_ms IS an object that has persisted that long. repeatTime cannot be
  // used for this -- it saturates at maxReportRepeat and then stops counting.
  if (Array.isArray(trackingWindow)) {
    for (const e of trackingWindow) {
      if (e && Number.isFinite(e.add_time_ms) && (t - e.add_time_ms > sameObjMs))
        return { reason: 'same_obj', noObjSince: since };
    }
  }

  return { reason: null, noObjSince: since };
}

// Whether this session is even eligible. CI only.
//
// FI is production: a line legitimately has gaps, and a machine that took
// itself out of full inspection because nothing came past for thirty seconds
// would be a far worse failure than the CPU it saved.
export function autoExitApplies(inspectionMode) {
  return inspectionMode === 'CI';
}
