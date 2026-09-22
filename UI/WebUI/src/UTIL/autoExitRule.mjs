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
// TOUCHING THE SCREEN IS BEING THERE. Both clocks are measured from the last
// time somebody touched the canvas as well as from their own start, so a person
// standing at the machine looking at a part is never thrown out from under it.
// Without that the watchdog answers a question nobody asked -- "has the picture
// changed lately" -- when the one that matters is "is anyone here".
//
// AND ONE THING IT CANNOT DO, which is worth knowing before trusting it: this
// is driven by arriving reports. No report, no decision. That is harmless when
// the core has stopped producing (nothing is being burned either), but it does
// mean the watchdog cannot rescue a session whose reports have stalled while
// the camera keeps grabbing.

// Returns { reason, noObjSince, remainMs } -- reason is 'no_obj', 'same_obj' or
// null, and remainMs is how long the soonest trigger has left (null when no
// clock is running, so there is nothing to count down).
//
// noObjSince is carried in and out rather than held here: the caller owns the
// clock and the state, and a rule that remembers things cannot be tested by
// calling it.
//
// remainMs is returned by the rule rather than worked out again by whatever
// displays it: a countdown computed separately from the decision is a countdown
// that will one day reach zero without anything happening, or hit zero after
// the fact, and either makes the screen a liar about what the machine is about
// to do.
export function autoExitDecision({
  now, hasObject, noObjSince, trackingWindow, lastInteractAt,
  noObjMs = 300000, sameObjMs = 300000,
} = {}) {
  const t = Number.isFinite(now) ? now : 0;
  // A touch restarts both clocks, so every deadline is measured from whichever
  // is later: the streak's own start, or the last time somebody was here.
  const touch = Number.isFinite(lastInteractAt) ? lastInteractAt : -Infinity;
  const from = (start) => Math.max(start, touch);
  let soonest = null;
  const note = (deadline) => {
    const left = deadline - t;
    if (soonest === null || left < soonest) soonest = left;
  };

  // The plate is empty.
  //
  // The FIRST empty report only starts the clock -- it does not exit. An
  // inspection that begins on an empty plate would otherwise leave immediately,
  // before the operator has put anything down.
  let since = noObjSince;
  if (!hasObject) {
    if (since == null) return { reason: null, noObjSince: t, remainMs: noObjMs };
    if (t - from(since) > noObjMs) return { reason: 'no_obj', noObjSince: since, remainMs: 0 };
    note(from(since) + noObjMs);
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
      if (!e || !Number.isFinite(e.add_time_ms)) continue;
      if (t - from(e.add_time_ms) > sameObjMs)
        return { reason: 'same_obj', noObjSince: since, remainMs: 0 };
      note(from(e.add_time_ms) + sameObjMs);
    }
  }

  return { reason: null, noObjSince: since, remainMs: soonest };
}

// SI IS IDLE WHEN NOBODY HAS ASKED IT FOR ANYTHING.
//
// The CI rule cannot be reused here, for two reasons. It is driven by arriving
// reports, and SI stops sending them once the accumulator is full and the
// picture is settled -- which is precisely the moment the machine is idle, so
// the watchdog would go quiet exactly when it is needed. And "the same object
// is still there" is SI's NORMAL state: a part sitting still in front of the
// camera is what the mode is for, not evidence that anyone has left.
//
// So the signal is the operator instead. SI is one press per part; no press
// and no touch means nobody is here. Nothing about the picture enters into it.
//
// This is NOT about compute. SI costs an accumulate and a downsampled diff per
// frame, one or two milliseconds against the twenty-odd CI spends on matching
// and measuring -- SI is by far the cheaper of the two to leave running, and
// the first version of this comment had that backwards. What it does leave
// running is the camera, free-running, and the sensor and the panel with it.
// That is a heat and a wear argument, not a CPU one, and it is worth the same
// five minutes CI gets rather than a rule of its own.
//
// Same { reason, remainMs } shape as autoExitDecision, so one countdown on the
// screen serves both.
export function siAutoExitDecision({ now, lastActivityAt, idleMs = 300000 } = {}) {
  const t = Number.isFinite(now) ? now : 0;
  // No activity recorded yet means the screen has only just opened, and the
  // clock has not started. Exiting on that would leave before the operator had
  // reached for the first part.
  if (!Number.isFinite(lastActivityAt)) return { reason: null, remainMs: idleMs };
  const left = lastActivityAt + idleMs - t;
  return { reason: left <= 0 ? 'si_idle' : null, remainMs: Math.max(0, left) };
}

// Whether the OBJECT rule above applies. CI only.
//
// FI is production: a line legitimately has gaps, and a machine that took
// itself out of full inspection because nothing came past for thirty seconds
// would be a far worse failure than the CPU it saved.
export function autoExitApplies(inspectionMode) {
  return inspectionMode === 'CI';
}

// Whether the press rule applies. SI only, for the same reason FI is excluded
// above: FI is production and a gap in the line is not an idle machine.
export function siAutoExitApplies(inspectionMode) {
  return inspectionMode === 'SI';
}
