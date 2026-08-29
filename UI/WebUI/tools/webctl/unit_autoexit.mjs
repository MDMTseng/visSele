// Unit test for the idle auto-exit rule. No core, no browser, no 30s wait.
//
//   node unit_autoexit.mjs
//
// This is the only thing in the app that stops the camera on its own -- the
// difference between "somebody walked away" and a machine grabbing and
// inspecting frames until they come back -- and it shipped with no test.
//
// The cases below are the ones that are wrong in an obvious direction and the
// ones that are wrong in a quiet direction, and the quiet ones are why this
// file exists: a watchdog that fires too eagerly is reported within a day
// ("it keeps kicking me out"), while one that never fires is reported as
// nothing at all, because the machine simply runs.
import { autoExitDecision, autoExitApplies } from '../../src/UTIL/autoExitRule.mjs';

let fails = 0;
const check = (c, w) => { if (!c) { console.log('  FAIL ' + w); fails++; } return c; };
const NO = 30000, SAME = 60000;
const call = (o) => autoExitDecision({ noObjMs: NO, sameObjMs: SAME, ...o });

console.log('CI only:');
check(autoExitApplies('CI') === true, 'CI should be eligible');
for (const m of ['FI', undefined, null, '', 'ci', 'XX'])
  check(autoExitApplies(m) === false, `${JSON.stringify(m)} must not auto-exit`);
console.log('  FI and everything unrecognised are excluded');

console.log('empty plate:');
{
  // The FIRST empty report starts the clock and must NOT exit. An inspection
  // opened on an empty plate would otherwise leave before the operator had put
  // anything down -- the eager failure, and the one that gets reported.
  const a = call({ now: 1000, hasObject: false, noObjSince: null });
  check(a.reason === null, `first empty report exited immediately (${a.reason})`);
  check(a.noObjSince === 1000, `clock not started (${a.noObjSince})`);

  // Still inside the window: no exit, and the clock must not be restarted --
  // restarting it on every empty report is a watchdog that can never fire.
  const b = call({ now: 1000 + NO - 1, hasObject: false, noObjSince: 1000 });
  check(b.reason === null, 'exited one millisecond early');
  check(b.noObjSince === 1000, `clock was restarted (${b.noObjSince})`);

  // Past it.
  const c = call({ now: 1000 + NO + 1, hasObject: false, noObjSince: 1000 });
  check(c.reason === 'no_obj', `should exit, got ${c.reason}`);

  // Exactly at the boundary is NOT past it (>, not >=). Stated so a later
  // rewrite cannot quietly change which side the boundary falls on.
  const d = call({ now: 1000 + NO, hasObject: false, noObjSince: 1000 });
  check(d.reason === null, 'the boundary itself must not trigger');
  console.log('  start / hold / fire / boundary');
}

console.log('an object resets the clock:');
{
  const a = call({ now: 5000, hasObject: true, noObjSince: 1000 });
  check(a.reason === null && a.noObjSince === null, `not reset (${a.noObjSince})`);
  // And having been reset, the full window has to elapse again. A part passing
  // by must buy the operator another thirty seconds, or a busy line with gaps
  // gets kicked out mid-shift.
  const b = call({ now: 5000 + NO + 1, hasObject: false, noObjSince: null });
  check(b.reason === null, 'exited on the first empty report after a part');
  check(b.noObjSince === 5000 + NO + 1, 'clock not restarted after a part');
  console.log('  reset, then the window starts over');
}

console.log('the same object left sitting there:');
{
  const tw = (ageMs) => [{ add_time_ms: 10000 - ageMs }];
  const a = call({ now: 10000, hasObject: true, noObjSince: null, trackingWindow: tw(SAME + 1) });
  check(a.reason === 'same_obj', `should exit, got ${a.reason}`);
  const b = call({ now: 10000, hasObject: true, noObjSince: null, trackingWindow: tw(SAME - 1) });
  check(b.reason === null, 'exited before the same-object window elapsed');

  // ANY entry old enough is enough -- the newest part does not excuse the one
  // that has been sitting there. A loop that stopped at the first entry, or
  // looked only at the last, would pass the simple case and fail this.
  const mixed = [{ add_time_ms: 9999 }, { add_time_ms: 10000 - (SAME + 5) }, { add_time_ms: 9998 }];
  check(call({ now: 10000, hasObject: true, trackingWindow: mixed }).reason === 'same_obj',
        'an old entry among fresh ones was missed');

  // A stream of DIFFERENT parts must never trip it: the reducer ages entries
  // out, so nothing in the window is ever old. This is the case that makes the
  // rule usable on a running line.
  const fresh = Array.from({ length: 20 }, (_, i) => ({ add_time_ms: 10000 - i * 100 }));
  check(call({ now: 10000, hasObject: true, trackingWindow: fresh }).reason === null,
        'a busy line with fresh entries was kicked out');
  console.log('  fire / hold / any-old-entry / a busy line is safe');
}

console.log('no-object wins over same-object when both apply:');
{
  // Both conditions can be true at once only if the tracking window still holds
  // an aged entry while the plate reads empty. Either answer stops the machine,
  // so this pins WHICH reason is reported -- the operator is told why, and two
  // builds disagreeing about the message is a support call.
  const r = call({ now: 100000, hasObject: false, noObjSince: 100000 - NO - 1,
                   trackingWindow: [{ add_time_ms: 0 }] });
  check(r.reason === 'no_obj', `expected no_obj, got ${r.reason}`);
  console.log('  no_obj');
}

console.log('junk must not fire and must not throw:');
{
  const cases = [
    {},
    { now: NaN, hasObject: false, noObjSince: 0 },
    { now: 1000, hasObject: true, trackingWindow: 'not-a-list' },
    { now: 1000, hasObject: true, trackingWindow: [null, undefined, {}, 5] },
    { now: 1000, hasObject: true, trackingWindow: [{ add_time_ms: 'old' }] },
    { now: 1000, hasObject: true, trackingWindow: [{ add_time_ms: NaN }] },
  ];
  for (const c of cases) {
    let threw = false, r = null;
    try { r = call(c); } catch { threw = true; }
    check(!threw, `threw on ${JSON.stringify(c)}`);
    check(r && r.reason === null, `fired on junk ${JSON.stringify(c)} -> ${r && r.reason}`);
  }
  console.log(`  ${cases.length} malformed inputs`);
}

// THE PROPERTY: a session that never sees an object exits exactly once, at the
// right time, when driven report by report the way the component drives it.
console.log('driven as a report stream:');
{
  let since = null, fired = 0, firedAt = null;
  for (let t = 0; t <= 60000; t += 250) {           // 4 Hz of empty reports
    const r = call({ now: t, hasObject: false, noObjSince: since });
    since = r.noObjSince;
    if (r.reason) { fired++; if (firedAt === null) firedAt = t; break; }
  }
  check(fired === 1, `fired ${fired} times`);
  check(firedAt > NO && firedAt <= NO + 250,
        `fired at ${firedAt}, expected just after ${NO}`);
  console.log(`  fired once, at ${firedAt} ms`);
}

console.log(fails ? `\nFAIL: ${fails} assertion(s)`
  : '\nPASS: idle exits once, at the right time, and a busy line is never kicked out');
process.exit(fails ? 1 : 0);
