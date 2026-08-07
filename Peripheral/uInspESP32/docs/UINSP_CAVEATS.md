# uInspESP32 — Caveats & Traps

Things that cost hours on 2026-08-06 and will cost them again. Written from
failures that actually happened on the machine, not from reading the code.

Companion docs: `PAIRING_MIGRATION_STATUS.md` (what the migration is),
`PAIRING_VALIDATION_2026-08-06.md` (what was measured),
`../../InspectionCore/docs/CORE0_1_CAVEATS.md` J11–J13 (the host side).

---

## A. Three plate frequencies, and they are not interchangeable

```
PLATE_FREQ_SETPOINT   the configuration. set_setup writes it, get_setup returns it.
PLATE_FREQ_TARGET     what the ramp is aiming at. get_running_stat returns it.
                      ZERO WHILE IDLE, even when the setpoint is not.
PLATE_FREQ_CURRENT    the actual speed.
```

Confused **three separate times in one afternoon**, in three different places:

1. The RUN button's barrier read `get_running_stat.plate_freq` (TARGET) to
   confirm a write to SETPOINT. In IDLE that test can never pass, so the first
   press turned the driver on, set the speed, failed its own check and silently
   declined to enter inspection: plate turning, switch snapping back, nothing
   else happening. The second press "worked" only because the first had left
   the machine somewhere that had loaded TARGET.
2. The panel converted microsecond widths to ticks against TARGET (0 in IDLE,
   which then fell back to a 15000 default) while the device converts against
   SETPOINT — so the displayed width was 6.7x off.
3. The us→tick conversion in firmware had to pick one; it uses SETPOINT
   deliberately (see C).

If you are confirming a write, read `get_setup`. If you are showing what the
machine is doing right now, read `get_running_stat`. They are different
questions.

## B. Leaving inspection mode does NOT stop the plate

`SYS_STATE::IDLE`'s loop body is:

```cpp
PLATE_FREQ_TARGET = PLATE_FREQ_SETPOINT;
```

IDLE drives the plate at the setpoint, every pass. So:

- **STOP must write `plate_freq: 0`.** `exit_insp_mode` alone leaves it turning.
- **Therefore stopping destroys the configured speed**, and anything that wants
  to start again has nothing to start at. The WebUI keeps the last non-zero
  speed in `localStorage` for exactly this reason, and refuses to start rather
  than substituting a compiled default — REF_FREQ is 30 rpm and a machine being
  set up at 4.5 rpm must not leap to it because someone pressed stop.
- **A speed slider on a stopped machine must not write.** Writing a non-zero
  plate_freq to an idle machine starts the plate. The sidebar slider only
  remembers while stopped, and applies live only while running.

## C. Width is a TIME, position is a DISTANCE

A stage offset is a position: 1 tick = 0.0126 mm of plate, so changing speed
does not move where anything fires. That is correct and should stay.

Width was stored the same way, and that was wrong. Nothing a station drives
cares about distance:

| station | what actually constrains it |
|---|---|
| camera | ~100 µs trigger floor; exposure is set in µs on the camera |
| backlight | ~300 µs to reach full brightness |
| SEL | solenoid open time + air transit |

The SEL blow tuned to 50 ms at 30 rpm is **500 ms at 3 rpm and 38 ms at
40 rpm** — a 13x spread out of one stored number, wrong at both ends.

`stage_pulse_width_us` now holds microseconds per station and the device
converts: `ticks = ceil(us × 2 × PLATE_FREQ_SETPOINT / 1e6)`. Rounded up, and
against SETPOINT rather than CURRENT, because the errors are not symmetric —
too short misses a trigger or fails to eject (loses a part), too long costs air
and LED duty (loses nothing). During SPINUP that makes the pulse longer than
asked, which is the safe side.

`0` means "not configured": the `*_off` offsets stay authoritative, so a machine
that has never been given a width behaves exactly as before.

**Smear is not set by the pulse width.** The camera is in timed-exposure mode
(`SetExposureTime`, µs). Blur is `speed × ExposureTime`: 0.0126 mm at 20 rpm
with a 50 µs exposure. A panel warning that computed smear from the trigger
pulse was wrong and has been fixed.

## D. Never `printf` in firmware — it writes to the protocol link

`printf` on this board goes to UART0, which **is** the host link. Raw text there
is a stray byte to the device's own parser: `INIT_CHAR_ERROR` → err 11 →
**latched** → machine stopped, and the host sees a dead link.

This was diagnosed and fixed in the morning and then reintroduced ten minutes
later, in the same file, by a one-line warning added to
`STAGE_PULSE_WIDTH_apply()`. Use `djrl.dbg_printf` (which frames the text as
`{"dbg":...}`) or set a flag and let a caller that has `djrl` emit it.

Related: err 11 is **latched**. CRC errors are not — a bad trailer is dropped
and the newline resyncs — but a framing error stops the machine and it does not
come back on its own. Whether that is right for a production machine is an open
question; it is certainly harsh for one stray byte.

## E. Calibration: the frame always arrives before its announcement

The camera **free-runs** (~70 fps measured); the trigger line only controls
illumination. So a calibration pulse's frame reaches the core almost
immediately, while `cam_trig{tid}` is still crossing 115200 baud — measured at
130 ms, 420 ms and 652 ms round trip on a busy link.

The core has a wait for exactly this, and it was gated on
`last_dev_state == 101 (INSPECTION)`. **Calibration runs in state 102.** So the
core asked the pairing, got EMPTY, did not wait, and logged
`result with no paired tid -- not sent`; the device reported the pulse
unanswered, retried 1500 ms later, and failed after 30 s with
`CAM_CLOCK_CAL_FAILED`. Fixed by waiting in 101, 102 and 104, and raising the
cap from 150 ms to 700 ms.

Diagnostic value: `CAMSYNC CAL FAILED after 30001 ms (learned=2 boot_n=2
boot_fail=0)` says the median/majority test never ran — it never collected
8 samples. `boot_fail` counts convergence failures; `boot_n` counts samples.
If `boot_fail` is 0 the calibration parameters are irrelevant and something
upstream is not answering.

**Watching the backlight flash proves the trigger fired**, nothing more. Between
2026-08-05 and 2026-08-06 the trigger was spliced onto the light line; it is on
GPIO17 again now (`HardwareConfig.hpp` keeps the history because it changes what
the timestamps mean).

## F. The host's SETTABLE_KEYS whitelist silently eats new settings

`uInspESP32_API.SETTABLE_KEYS` in `UI/WebUI/src/script.jsx` lists what
`machineSetupUpdate` is allowed to send. Anything absent is reclassified as
read-only device state and **never transmitted** — no error, no log, and the
panel keeps showing the value you typed.

Adding a firmware setting means adding it here too. Cross-checking the list
against every `JSON_SETIF_ABLE(...,jdoc,"...")` target found **nine** missing,
including the whole camera-clock group: `cam_match_window_us`,
`cam_recal_idle_ms`, `cam_drift_comp`, `report_match_ts`, `auto_rate`,
`auto_rate_floor_us`, `auto_rate_recover_n`, plus `cal_pulse_us` and
`stage_pulse_width_us`. All displayable, none writable — the match window
recommended repeatedly that afternoon could not have been set from the UI at
all.

Keep them in sync:

```bash
grep -o 'JSON_SETIF_ABLE([^,]*,jdoc,"[a-z_0-9]*")' src/app/LegacyFirmware.cpp \
  | sed 's/.*"\(.*\)")/\1/' | sort -u
```

## G. Only ONE client owns the peripheral link

The core keeps a single serial connection and the last `CONNECT` wins. A second
browser tab — or a headless tool — takes `CONN_ID` from the first, and the
first's commands then go nowhere while its UI still shows a green
"connected" tag (that tag is latched at CONNECT and never rechecked).

Two people debugging the same machine will silently break each other. So will
one person and their own automation.

## H. Reading list for a "the machine is not responding" report

In the order that actually produced answers:

1. `/tmp/insplog/insp` if persistence is on — see CORE0_1_CAVEATS J13, it is
   off by default and the log system cannot be trusted.
2. The peripheral console tap: `INSP_PERIF_CONSOLE=<port>` gives the device's
   bytes verbatim, before any framing or truncation. This is what caught err 11.
3. `Core0_1/crash_*.dump` — includes the entire retained log ring, which is how
   the camera crash loop was solved.
4. `get_running_stat`: `error_hist`, `cam_sync.{recals,cal_fails,disagree}`,
   `health.free_heap` (NOT `min_heap` — it is a high-water mark, it only falls,
   and a slope read off it is meaningless).

---

## I. A rig that cannot prove its own stimulus proves nothing

2026-08-06 ran a 4.5 hour unattended soak that reported clean. It had injected
**zero** objects.

`regress_watch.py` sent `{"type":"trig_phantom_train","n":20,"sep_us":60000}`.
The firmware reads `count` and `period_us` (`LegacyFirmware.cpp:4011`), absent
keys default to 0, and **`count:0` cancels a running train**. Every command was
a cancel. The traffic actually being measured was the real gate sensor at
10.15 objects/s — unseeded and unreproducible, the opposite of the point.

The board reported it in every single reply, for 4.5 hours:

```
{"type":"trig_phantom_train","prev_emitted":0,...,"count":0,...}
```

The script sent with `wait=0.5` and discarded the reply.

**Rules this produced, all now in the script:**

1. **Verify the ack, every time.** The firmware echoes `count`, `period_us`,
   and the *previous* train's `prev_emitted` / `prev_min_us` / `prev_max_us`,
   so a rig can state the load it applied rather than the one it asked for.
   Re-checked every cycle, not just at startup: a board that reboots mid-run
   comes back with the train cancelled.
2. **Correlate replies by `id`, not by `"type"`.** Every reply echoes the
   request's id and an ack flag unconditionally (`LegacyFirmware.cpp:4191`),
   but `get_running_stat`, `get_setup` and `set_gate_disable` never set
   `retdoc["type"]` — matching those on type times out against a board that is
   answering perfectly.
3. **Silence is not a pass.** The old criterion was
   `bad = halt or err11 or dumps`; a core that died in hour one produced
   `(no answer)` on every poll, was never counted, and printed "clean".
4. **A detection must never be swallowed by the report.** The `samples == 0`
   branch returned before the failure list was printed, so an injected slip
   that *was* detected on cycle one displayed as "NO DATA".
5. **`kill -INT` does nothing to a backgrounded run.** The shell sets SIGINT to
   ignore for background jobs, so the only thing that stops it is SIGTERM,
   which skips the cleanup and leaves the plate turning and the real gate
   disabled. The script now routes SIGTERM into the same KeyboardInterrupt.

## J. Instruments that silently under-sample

Three found in one night, all of the same shape — the number was real, what it
covered was not:

- **`error_hist` is a 20-deep ring** (`errorBuf[20]`, drops oldest). Counting
  occurrences of err 11 in a snapshot every 15 minutes answers "is 11 among the
  last 20 errors", not "did 11 happen". Poll every cycle and absorb by overlap.
- **The verdict log is 64 records deep** (`VERD_LOG_N`) and a burst is 100
  parts, so one harvest per cycle returns only the last 64 — the *same* first
  36 of every burst never checked. A systematic blind spot presented as
  "512 verdicts checked". Harvest during the burst, 50 parts of headroom.
- **Core RSS oscillates 83–100 MB with a ~10 minute period.** Hourly single
  samples read 90.1, 90.9, 97.9 — which invites "+7 MB in an hour" and a leak
  hunt. It is sampling phase. A 2-minute min/max window is still too short to
  find the trough of a 10-minute cycle; do not quote a trend from it either.

## K. What a valid soak looked like (2026-08-07, 5h18m)

For comparison when the next one reports "clean":

```
stimulus    75600 phantoms CONFIRMED emitted (100 @ 100ms +-4ms, seed 1)
judged      75710 @ 4.0/s average, 10.0/s during bursts (96-104 ms measured)
verdicts    75703 checked against the tid pattern = 100% coverage, 0 slips
pairing     agree 75710  disagree 0  rejected 0  rebuilds 0
calibration runs 758  fails 0   recals 757
delta       32-64us: 74952   64-128us: 758   max 110us  (window 5 ms = 45x)
heap        193056 -> 192288, one step, then flat for 5 hours
errors 0    dumps 0    unanswered polls 0
```

The 758 samples in the 64–128 µs bucket are exactly the 758 calibrations —
every ordinary part sits under 64 µs. `set_gate_disable` was on throughout, so
the phantoms were the only objects by construction and every idle gap was real,
which is why RECAL ran 757 times instead of the previous run's zero.

The negative control matters as much as the result: with
`INSP_PERIF_VERDICT_SLIP=1` the same check fires on the first cycle and
disagrees on 30/64 and 23/50 verdicts (~47%, the half a period-free hash
predicts). A check that has never failed is not evidence.

## L. The one silent failure mode, and why irregular feeding is what closes it

Established 2026-08-07 by injection (`INSP_PERIF_FAULT_TS_US`, every 50th
report) plus 24288 measured real-part intervals.

**A corrupted frame timestamp is normally harmless.** With `report_match_ts`
off, tid decides which object a verdict belongs to and the timestamp only feeds
the clock model, so a shifted timestamp costs a clock sample rather than a
part. A sweep from 0 to 50000 us at 15 Hz spacing produced 18 refusals and
**zero** mis-sorts at every value.

**With `report_match_ts` on, exactly one shift value mis-sorts**: an integer
multiple of the object spacing. Measured at 66666 us against 66666 us spacing:
`disagree` 1, and nothing else noticed.

The reason it is silent is worth understanding, because it is not a bug:

```cpp
offset_us = (int64_t)cam_ts - (int64_t)nearest_cam_us;   // measured, not blended
```

When the shift equals the spacing, `cam_ts` is one spacing late AND
`nearest_cam_us` is the next object, one spacing later — so the shift cancels
exactly and the offset comes out *correct*. No residual, no rejection, no
`rebuilds`, no halt. Observed: the run continued for 597 more parts with
delta 121 us. The clock model is undamaged; only that one part is mis-sorted.
Any other shift lands outside the 5000 us window, is refused, and two refusals
running halt the machine — which is the designed and correct outcome.

**Two things kept it to 1 hit in 12 injections, and only one of them is
permanent:**

1. A *positive* shift points at an object that has not been registered yet, so
   there is nothing to match and it is refused. **A negative shift has no such
   barrier** — it points into the queue of objects already seen. Not yet swept
   (2026-08-07), and it is the next thing to test.
2. Real parts do not arrive regularly:

```
real inter-arrival, n=24288:  p1 42982  p50 85107  p99 269893 us
                              mean 99796   sigma 60350 us
match window TOL_US                          5000 us
```

Sigma is **12x the window**. A clock error of exactly one median spacing lands
inside a neighbour's window 10.0% of the time, 1.0% twice running, 0.001% five
times running — and every miss is a refusal, so a persistent error of this kind
halts the machine within about three parts, mis-sorting at most one.

**The guarantee is a property of the FEED, not of the code.** If this machine
is ever fed by a fixed-pitch carrier — an indexing wheel, a pocketed plate,
anything that places parts at a constant pitch — the spacing spread collapses,
the hit rate goes from 10% toward 100%, and a persistent one-spacing clock
error becomes a systematic mis-sort that never halts. Re-derive this number
before any change that makes the feed more regular.

This is the same principle as the slip-detection work, used in the other
direction: there, a regular verdict pattern hid a real 10-part slip across 510
parts. Regularity hides slips and enables them. Irregularity is the protection
in both cases.

**Instrument note:** the end-to-end slip check caught none of this. The one
real mis-sort was `tid=379 -> ts_tid=380`, and `f(379) == f(380) == 1` — the
wrong answer happened to equal the right one. That is the hash's designed 50%
per-event detection rate, and it is why `disagree` (deterministic, device-side)
and the verdict-pattern check (end-to-end, 50% per event) are both needed.
Neither replaces the other.

### L.1 What the window actually guarantees — and the correction to L

Section L was written as though tid-based routing were the safe fallback and
the timestamp the risk. That has it backwards, and the reason matters.

`cam_ts` is `arv_buffer_get_timestamp(buffer)` — the **camera's own stamp on its
own exposure** (`CameraLayer_Aravis.cpp:557`), not the host's receipt time
(`arv_buffer_get_system_timestamp` is used only to calibrate the tick rate). So
the timestamp and the image content cannot come apart: the timestamp points at
the part that was photographed, by construction.

Verdict routing is correct exactly when **the physical part that receives the
verdict is the part that was photographed**. Timestamp routing satisfies that
definitionally; tid routing satisfies it only while the announcement pairing is
itself correct. So `report_match_ts = true` is the *safer* policy on this
machine, and the migration was pointed the right way.

Which also means the `INSP_PERIF_FAULT_TS_US` sweep models a direction that
**cannot physically occur** — a timestamp displaced while the image is not. The
mis-sort it produced at 66666 us is an artefact of the injector. Useful for
mapping the boundary; not evidence of a real hazard.

`disagree` is a divergence detector, not a correctness detector. It fires when
the core's clock model and the device's clock model point at different objects
and says nothing about which one is right. Only image content can answer that,
and phantom traffic has no image — so no amount of further phantom testing can
close it. That needs real parts with a known physical marker.

**The window does not prevent mis-pairing. It prevents SILENT mis-pairing** —
it turns an error into a refusal and two refusals into a halt. Three
independent properties are what actually close the hole, and all three were
measured on 2026-08-07:

| leg | what it does | measured |
|---|---|---|
| tight window | any error that is not a near-exact multiple of the spacing is refused | TOL 5000 us vs median spacing 85107 us |
| irregular feed | makes "exactly one spacing" not a fixed number | sigma 60350 us = 12x the window; 10% hit, 1% twice |
| continuous error | drift must cross the refused region before reaching the hole | conversion drift is continuous; no jump mechanism |

Remove any one and the guarantee fails. The third is why **calibration — which
IS a jump** (`offset_us = med`, replaced outright, not blended) — runs with the
gate shut and phantoms as the only objects in the pipeline. With no neighbour
present there is nothing to latch onto, so the one moment that bypasses the
continuity protection is also the one moment the hole does not exist.

### L.2 The whole thing in one inequality

Trigger timing may be **arbitrarily irregular, up to and including random**.
Regularity is not a correctness requirement and never was. "This verdict
belongs to this part" depends on exactly two properties:

1. **`cam_ts` is bound to the exposure.** The camera stamps its own frame
   (`arv_buffer_get_timestamp`), so the timestamp points at the part that was
   photographed. Holds by construction.
2. **No two objects are within TOL_US of each other in the timestamp domain.**
   Then a frame inside the window has exactly one candidate: the match is the
   right object or there is no match. Enforced by the gate.

Which reduces the entire matching argument to:

```
2 * TOL_US  <=  GATE_SEP_EFF_us
2 * 5000    <=  28571                 2.86x margin
```

The firmware already enforces it and will not let configuration break it:

```cpp
int32_t cap = (int32_t)(SYS_MIN_PULSE_TIME_SEP_us/2);
if(CamClockSync::TOL_US > cap) { warn; CamClockSync::TOL_US = cap; }
```

Open the gate and the window is clamped to half of it; set the window too wide
and it is clamped back with a `dbg_printf`. The inequality cannot be violated
from `set_setup`.

**The boundary is 100 Hz** (`GATE_SEP = 2 * TOL = 10000 us`) — the same number
reached independently from "window versus half the object spacing". Two routes,
one answer.

This also corrects how the 30 Hz gate rejections were read on 2026-08-07. They
were logged as throughput limiting; they are not. The rate gate is what keeps
objects far enough apart for a match to be unambiguous at all, so a rejection
there is the invariant being defended, not a part being needlessly refused.
Raising `min_detect_sep_us` trades directly against that margin.

Randomness bears only on the *other* question — whether a systematic error can
hide (L.1, leg 2) — and there it is purely protective. Both questions answer
the same way: irregular is fine, and better.

### L.3 TODO — worst-wins is shipped but its branch is UNVERIFIED

2026-08-07. Parked deliberately: the path cannot be reached on this machine, so
verifying it is not worth more bench time now. Recorded so nobody later reads
the code and assumes it was tested.

**What is in the firmware:**

```cpp
const bool had_verdict = (tarP->insp_status!=insp_status_UNSET &&
                          tarP->insp_status!=insp_status_SKIP);
if(!had_verdict) tarP->insp_status=cat;
else { REP_REPEAT_N++;
       if(cat!=tarP->insp_status) REP_REPEAT_DIFF_N++;
       if(cat<tarP->insp_status){ REP_REPEAT_WORSE_N++; tarP->insp_status=cat; } }
```

Severity is the cat value and smaller is worse (SEL1 worst, last selector OK,
`PERIF_CAT_NA`=0xFFFF least severe). Replaces an unconditional
`tarP->insp_status = cat`, whose one bad direction was an NG overwritten by a
later OK — a defective part released, against the reverse costing only air.

**Verified:** the three counters. `INSP_PERIF_FAULT_DUP=1` with
`FAULT_EVERY=5` gave `repeat 188, repeat_diff 0, repeat_worse 0` against 944
first-writes — 188 ~= 944/5, so repeats are counted correctly and the blind
spot (an overwrite used to leave no trace at all) is closed.

**NOT verified:** the `cat < insp_status` branch has never executed.
`INSP_PERIF_FAULT_DUP` cannot produce a *differing* duplicate, because
`INSP_PERIF_VERDICT_SLIP` shifts the pattern for every report, so both copies
carry the same verdict and `repeat_diff` stays 0. Driving two reports by hand
failed in the harness, not the firmware (the object had been swept, so `tarP`
was NULL and nothing was counted).

**Why it is parked:** a second frame inside one object's window needs the frame
period below `2*TOL_US` = 10 ms, i.e. **above 100 fps**. The camera free-runs at
~70 fps (14.3 ms). Unreachable until a small ROI raises the frame rate.

**To verify when it matters**, either:
- add `INSP_PERIF_FAULT_DUP_CAT=<n>` so the duplicate copy carries a chosen
  category (a few lines in the core's injection block), or
- fix the by-hand harness: `tmp/worstwins_test.py` has the right three cases
  (OK-then-NG must replace, NG-then-OK must not, NG-then-NG no change) and
  fails only because it cannot reliably get a live, unswept tid.

Risk of leaving it in: low. UNSET and SKIP behave exactly as before; only the
already-has-a-verdict path changed, and that path is the unreachable one.

---

## M. Where the rate actually tops out, and why 35 Hz is not the reason

Swept 2026-08-07 with the phantom train, gate disabled, `min_detect_sep_us`
opened to 25000 (40 Hz) so the rate gate was not the binding constraint:

```
 15 Hz   delta max    80us    32-64us: 906                       clean
 20 Hz   delta max   105us    32-64us: 904                       clean
 25 Hz   delta max  1737us    32-64us: 900 + 1024-2048us: 4      knee
 30 Hz   delta max  3308us    32-64us only 251/306               collapsing
```

**The knee is at 25 Hz, and it is not the gate.** At 30 Hz the match delta
distribution falls apart — 55 of 306 frames past 256 us, 14 of them past 2 ms
against a 5000 us window. Margin goes from 45x (at 10 Hz, measured over 75710
parts) to 1.5x. Cause not established: could be the camera, the host pipeline,
or the device. **This is the open question worth chasing before any rate
increase**, and none of the day's other findings bear on it.

So do not raise `min_detect_sep_us` to buy throughput. At the shipped 28571
(35 Hz), a 30 Hz nominal train loses ~9% of parts at the gate, which reads like
the gate being in the way. Opening it to 25000 removes those rejections and
exposes a 30x worse pairing margin instead — a visible, counted loss traded for
a hidden one. The gate was doing something useful (L.2: it is what keeps
objects further apart than 2*TOL_US, which is what makes a match unambiguous
at all).

Two separate limits, often confused:

| limit | value at 7 rpm | what it protects |
|---|---|---|
| `min_detect_sep_us` | 28571 us = 35 Hz | the `2*TOL_US` unambiguity invariant |
| `_PLAT_DIST_step(2000)` | 2 mm = 22.7 ms = 44 Hz | physical separation on the plate |

And a third thing that is not a limit at all: the device's own loop latency.
With jitter set to 0 and a perfectly scheduled train, measured intervals came
out **27964..33343 us** against a nominal 33333 — never late, up to 5.4 ms
early. That asymmetry is the absolute-phase schedule (`PH_TRAIN_NEXT_US +=
step`): one pulse held up by the loop makes the *following* interval short. At
30 Hz that alone puts 1.6% of intervals under the gate. Raise the requested
jitter and it compounds: 1.6% at 0 us, 7.5% at +-2000, 9.4% at +-4000.

## N. Severity is the cat value, and smaller is worse

The machine convention, stated 2026-08-07:

```
SEL1  most severe reject
SEL2  less severe
SEL3  OK   (the last selector is the pass-through)
PERIF_CAT_NA = 0xFFFF   no verdict -- larger than any selector, so least severe
```

The inspection software hands out a classification number and **a smaller
number is a worse part**. Two consequences worth stating because they are easy
to get backwards:

1. **"Keep the worse verdict" is `min()`**, and NA needs no special case — it
   is already the largest value, so any real verdict beats it. Only the device's
   negative sentinels (`insp_status_UNSET` -2000, `insp_status_SKIP` -2100) must
   be kept out of the comparison. See L.3.
2. **An ordered severity makes "is the verdict stable?" a graded question.**
   The window is a physical uncertainty band -- +-5000 us at 88 mm/s of plate is
   **+-0.44 mm, a 0.88 mm span** -- and the real specification is that
   inspection gives the same answer anywhere in it. With an ordering you can ask
   *how far apart* two answers are: a SEL2<->SEL3 flip is boundary noise, a
   SEL1<->SEL3 flip is a different defect being seen. For scale, exposure smear
   is 0.0126 mm at 20 rpm / 50 us -- **the window is 70x larger than the smear**,
   so positional uncertainty in the verdict comes from the match window, not
   from motion blur. That is what the locating anchor exists to correct.

Every host tool's `CONN` had this inverted (`cat_ok: 1, cat_ng: 2` -- good parts
routed to the most severe station). Phantom traffic can never expose it: nothing
is ejected, and the verdict-pattern slip check is self-consistent under any
relabelling. Fixed in all ten scripts; the reason is written next to the values
in `regress_watch.py` so it does not get "fixed" back.
