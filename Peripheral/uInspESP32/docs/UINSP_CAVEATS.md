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

---

## O. How to set the match window — the question mis-verdicts actually reduce to

In practice every wrong-verdict question on this machine comes back to one
number, and that number is squeezed from both sides.

**Upper bound — uniqueness.** `2*TOL_US <= GATE_SEP_EFF_us` (L.2), and since the
gate separation is what sets the rate, `TOL <= 1/(2R)`. Enforced and clamped in
firmware. At the shipped 28571 us (35 Hz) the ceiling is 14285 us against the
5000 us in use: **2.86x of headroom**.

**Lower bound — do not refuse legitimate frames.** And here the intuition is
wrong: **clock drift is not the lower bound.** Drift measures 35 us/s (35 ppm
between the camera crystal and the ESP32's), but the offset is re-measured
outright from *every* accepted report, so the most it can accumulate is one
report interval:

```
10 parts/s          100 ms * 35 us/s  =   3.5 us
18 parts/s           55 ms * 35 us/s  =     2 us
idle to recal (10s)   10 s * 35 us/s  =   350 us
                                window   5000 us
```

Three orders of magnitude below the window. The "updating from every report"
design (which replaced a 1/16 EWMA carrying a permanent -3430 us lag) removed
drift as a constraint rather than managing it, and the recal idle timer covers
the only case where it could accumulate.

What actually binds the lower bound is the **rate-dependent residual tail** from
section M:

```
R        upper 1/(2R)     lower (measured delta max)     ratio
10 Hz       50000 us              110 us                  454
20 Hz       25000 us              105 us                  238
25 Hz       20000 us             1737 us                   11.5
30 Hz       16666 us             3308 us                    5.0
```

The upper bound falls linearly with rate. The lower bound is flat to 20 Hz and
then climbs sharply. **They converge, and where they meet is this machine's real
throughput ceiling** — not the 100 Hz that L.2 derives from geometry alone.
Extrapolating the tail (doubling per 5 Hz) puts the crossing near 40 Hz, but
that is two points of growth and a weak basis; it is a reason to run the sweep,
not a number to quote.

**Consequences worth keeping straight:**

1. At production rates (10-20 Hz) the window is not the constraint on anything.
   5000 us sits 450x above the residual and 10x below the uniqueness limit. It
   is well chosen and there is nothing to tune.
2. The question only bites above 25 Hz, and there it **cannot currently be
   answered**, because the cause of the tail is unknown. Setting the window
   above 25 Hz means guessing at a lower bound.
3. **The window is a shared knob.** Its width is also the physical uncertainty
   band the inspection must be invariant over — 5000 us is 0.88 mm of plate at
   7 rpm (N). Widening it for throughput widens the band the optics and the
   locating anchor have to absorb. Throughput and inspection precision are
   trading against each other through one number.

**Open, and now the highest-value thing in this area:**

- Sweep 25-45 Hz with the gate opened, recording `delta_hist`, and find where
  the bounds cross. ~20 minutes, and it replaces the extrapolation with the
  machine's actual ceiling.
- Explain the tail. Three suspects: the camera (frame scheduling / exposure),
  the host pipeline (variance in processing latency), and the device's own loop.
  The third is the most suspicious purely on magnitude — measured loop lateness
  at 30 Hz is 5.4 ms (M) against a 3.3 ms tail, the same order — but that is a
  coincidence of scale, not evidence, and it has not been tested. If it IS the
  loop, the tail is a firmware scheduling problem rather than an optical one,
  it is fixable, and fixing it moves the throughput ceiling directly.

---

## P. The CONNECT config is not in the WebUI — it is in the core's data dir

`InspectionCore/Core0_1/data/machine_setting.json`, key
`uInspESP32_peripheral_conn_info`:

```json
{ "uart_name": "/dev/cu.usbserial-0001", "baudrate": 230400,
  "machine_type": "uInspESP32", "cat_ok": 3, "cat_ng": 1,
  "cam_idx": 1, "pairing": "timestamp" }
```

Grepping the WebUI for `baudrate` or `uart_name` finds **nothing** — the panel
never builds a CONNECT, it only rides one. So this file is where a link setting
actually lives, and it is **not git-tracked**: back it up before editing, there
is no `git checkout` to fall back on.

Two things were stale in it after the 2026-08-07 bench day, and both were found
only by starting the app and reading `speed:115200` in the core's stdout:

1. **`baudrate` was still 115200** while the firmware had moved to 230400. The
   ten tool scripts were updated in the same commit as the firmware; this file
   was not, and nothing warns — the panel would simply have talked garbage.
2. **`cat_ok`/`cat_ng` were 1 and 2**, i.e. good parts routed to SEL1, the most
   severe reject station (N). Corrected to `cat_ok: 3, cat_ng: 1` after
   confirming the plumbing, so good parts go to SEL3 (GPIO 32) and rejects to
   SEL1 (GPIO 25). Note the machine now uses SEL3 as its pass-through, where the
   old config used only SEL1/SEL2 and left SEL3 unassigned.

**SEL1/2/3 carry no semantic labels anywhere in the code** — they are
`PIN_O_SEL1 25`, `PIN_O_SEL2 26`, `PIN_O_SEL3 32` in `HardwareConfig.hpp` and
nothing else. Which chute each drives is a property of the machine, not of the
source, so a `cat_ok`/`cat_ng` change cannot be verified by reading code. Ask
before changing it: the failure mode is good parts in the reject bin, or
defective parts passing.

**Checklist when the firmware's link settings change:** the firmware
(`Serial.begin`), `platformio.ini` (`monitor_speed`), the ten `tools/*.py`
CONNs, and this file. Missing the last one leaves the app broken while every
test script still passes.

## 幻影脈衝與真實料件不能同盤混跑(2026-08-08)

`chaos` 反覆出現的 `INSP_CAM_TRIG_INFO_CANNOT_BE_SENT`(comm queue overflow)
**是治具造成的,不是產線缺陷。** 產線上沒有幻影脈衝,不會發生。

`phantomEmitOne()` 刻意把 `gate_pulse` 往回填一整個 `L1A_on`:

```c
uint32_t tatPulse = SYS_STEP_COUNT - STAGE_PULSE_OFFSET.L1A_on + _PLAT_DIST_step(3000);
```

= `now - 9076`(本機 L1A_on 9314、`_PLAT_DIST_step(3000)` 238;實測 9076–9077
一個 tick 不差)。用意是讓注入的物件**立刻**抵達燈光/相機站,不必等一整圈。

代價是同一時刻登記的兩種物件,CAM 目標差了 9236 tick:

| 來源 | gate_pulse | CAM1 目標 |
|---|---|---|
| 幻影 | `now − 9076` | `now + 239`(馬上) |
| 真實料 | `now` | `now + 9315`(一圈後) |

`ACT_CAM1` 是**按登記順序**的 FIFO,`ACT_TRY_RUN_TASK` 只看隊尾、每個 tick 發
一個。一顆真實料插進幻影流裡,它「一圈後」的目標就卡住隊首,後面所有「馬上」
的幻影一路等到它到期 —— 然後整批已過期的以每 tick 1–2 筆噴出。32 格的
`ISRTrigQ`(每顆料 2 筆 = 16 顆餘裕)接不住。

**實測證據**(`poll` 的 `pushlog` / `act_late_max`):

```
tid=13 gate=30576 at=39652   gate = at − 9076   幻影
tid=14 gate=39728 at=39730   gate = at          真實料 → 目標 +9152
tid=15 gate=30974 at=40050   目標 −8754  ← 反轉，此後全部過期
```

任務發射時已過期 8973–9079 tick,五次刷新全部逼近一個完整的 CAM1_on。

**因此:**

- 用 `trig_phantom_pulse` / `trig_phantom_train` 的測試(bench、stress、chaos、
  edge)**必須在空盤上跑**,否則量到的是這個假象。盤子上有料時
  `gate.accept` 會自己走(見 [[project_uinsp_throughput_ceiling]]:
  plate_freq 15000 時盤子自己送 23–26/s)。
- 想在有料的盤子上測,就別注入幻影 —— 真實料本身就是負載。
- **已於 2026-08-08 修掉:`phantomEmitOne` 不再回填 `gate_pulse`。** 校正早已
  有專屬路徑(`calFireNow`,直接驅動相機、不註冊 stage task),回填是那之前
  的遺留。改後幻影與真實料件的登記完全一致,目標單調遞增。
  代價:注入的物件現在要走一整個 `CAM1_on/(2*plate_freq)` 才宣告(plate_freq
  1000 時 4.66s,實測 4.71s)。各套件的等待本來就是「等抵達」而非固定延遲,
  而且物件是管線化的,一輪只付一次 transit。
  效果:`act_late_max` 9079→0,`tq_hwm` 28→2,chaos 同一個從未通過的 seed
  變成 5/5(6322 顆料、203 次擾動、佇列尖峰 4/32、零溢位)。
- **`bench` 現在自己 `set_gate_disable`,結束再還原。** 它是幻影套件,不該
  靠人記得清空盤子;`GATE_DISABLED` 是 volatile、預設 false,燒錄一次就會
  重新武裝感測器。加上之後 bench 從 11/2 變 14/0。

順帶,追這個東西時補上的儀表(都在 `poll`):`tq/tqhwm/tqcap/tqovf`(那個會
溢位的佇列,`Qs` 是 RBuf,是另一個)、`tqburst`、`act_late_max`、`loopn/
loopmax_us`、`svc/st/rx/tx_us` 分段計時,以及 `pushlog` 指令。

## M 的答案:那個拐點是相機的讀出時間(2026-08-08,首次接上核心與真相機)

M 節把 25 Hz 拐點列為未定案 ——「可能是相機、主機管線、或裝置」,並說在
調高速率之前該先查清楚。查清楚了,**是相機**,而且數字精確到不需要爭論。

核心一啟動,相機自己就把答案印出來:

```
model MV-CA050-11UM   region 2448x2048
ResultingFrameRate = 35.18 fps        exposure floor: 28425 us
```

`28425 us` 是全幅讀出的每幀時間。而 NVS 裡出貨的 `min_detect_sep_us` 是
**28571** —— 兩者差 0.5%。那不是巧合,那個設定本來就是照相機配的。

真料實測(盤上有料、感測器餵料、核心+相機在迴路裡、`plate_freq 20000`):

| `min_detect_sep_us` | 上限 | 實際進料 | `delta_max` | 結果 |
|---|---|---|---|---|
| 28571(出貨值) | 35/s | 19.2–22.3/s | **51 us** | 乾淨 |
| 24000 | 41/s | 22.3/s | **3953 us** | 乾淨,但視窗只剩 21% |
| 20000 | 50/s | — | — | **停機 err 13** |
| 5000 | 200/s | — | — | **停機 err 13** |

err 13 = `CAM_CLOCK_LOST`。相機跟不上就丟幀,回來的幀配到的最近物件落在
5000 us 視窗外,連兩次即停。**這是 fail-closed 正確運作,不是缺陷** ——
它寧可停也不猜哪一幀屬於哪一顆。

`delta_max` 是這件事的儀表,而且非常靈敏:35/s 上限時 51 us,只開到 41/s
就跳到 3953 us(視窗的 79%)。M 節說的「45x 餘裕變 1.5x」就是同一個現象,
只是當時看不出來源。

**槓桿是 ROI,不是閘門。** 相機的 fps 幾乎跟讀出行數成反比,量過:

```
Height 2048  ->  35.18 fps      (全幅,現況)
Height 1024  ->  68.72 fps
Height  512  -> 131.33 fps
Height  256  -> 241.20 fps
Height  128  -> 414.65 fps
```

裁一半高度就換到兩倍的 fps。所以要跑 30+/s 的正確順序是:**先在
InspectionUI 設好 crop ROI,再照新的 fps 重算 `min_detect_sep_us`** ——
反過來做(先開閘門)只會撞上 err 13。ROI 屬於站點層設定,在
`machine_setting.json` 裡以全幅 px 表示,不在 def 裡
(見 [[project_insp_clean_regions_machine_level]])。

**還沒量到的一項:** 這台機器上沒有任何 def(browser profile 空的、repo 裡
只有 expected report),所以以上全部是「相機觸發 → 曝光 → USB3 傳輸 → 配對
→ 判定回送」的數字,**不含檢測運算**。要補上這一項需要對現在盤上的料建一份
配方。上面每一趟 844–1323 顆全部是 NA,原因就是這個,不是判定失敗。

順帶,這趟先踩到的坑:`real_parts.py` 還在送平鍵 `plate_freq`。設定重組成
plate/gate/cam 之後那個鍵不再被解析,**而且不會被拒絕,是靜默忽略** ——
盤子從頭到尾沒轉,60 秒跑完報告 `=> clean`、`accept=0`。凡是還在送舊平鍵的
工具與核心路徑都有同一個問題,「乾淨」不代表「有跑」。

## 檢測運算才是真正的天花板,而且差了一個數量級(2026-08-08,載入 test1.hydef)

上一節說相機的 35.18 fps 是牆。把 `data/machine_setting.json` 跟
`data/test1.hydef` 放進來、開一個 FI session(硬體觸發 + station region
enforced)之後,那面牆連碰都碰不到:

```
ImgInspection  n=2214   p50 210.8 ms   p90 301.9   p99 356.2   mean 211.5 ms
相機實際交幀   n=2214   p50 207.5 ms  →  4.8 fps
```

**檢測一幀 211 ms = 4.7 幀/秒。** 相機交幀率 4.8 fps 與之完全一致 ——
相機不是慢,是被管線拖住。板子同期放行 20+/s,**大部分料件根本沒有影像**。

211 ms 花在哪,log 直接講:

```
[shape] matches=  平均 13.6 個/幀（9–20）
insp_region: ... outside the station -- dropped     25921 個 = 86%
存活                                                 每幀約 1.9 個
```

檢測在 **2448×2048 = 5,013,504 px** 上全域比對,找出整盤的料,再把 86% 丟掉,
因為不在 **318×424 = 134,832 px** 的站點框內。**多算了 37 倍面積。**
`inspection_region` 是**比對之後**的篩選,不是相機 ROI —— 它省的是判定,
不是運算。

### 裁 ROI 的實測效果

相機 ROI 走 `data/default_camera_setting.json` 的 `"ROI": [x,y,w,h]`,
不是 `inspection_region`。設成涵蓋站點框 + 兩個 clean region 的聯集
(`[1248,428,560,452]`):

| | 全幅 2448x2048 | ROI 560x452 |
|---|---|---|
| 相機上限 | 35.18 fps | **184.89 fps** |
| 曝光下限 | 28425 us | **5409 us** |
| 檢測 mean | 211.5 ms | **81.1 ms** |
| 實際交幀 | 4.8 fps | **12.4 fps** |
| matches/幀 | 13.6 | 4.4 |
| 60s @ 20+/s 進料 | **停機 err 1** | **乾淨,1289 顆,UNANS 0** |

2.6 倍,而且順帶把穩定性問題解掉:全幅時同樣的設定會在 200 顆內停機
(`error_hist=[1]` = `INSP_RESULT_MATCHES_NO_OBJECT`,`miss_max` 179 ms),
裁完之後 1289 顆零未答、零錯誤。

**還沒榨完:** 裁完仍有 74%(4477/6032)的 match 被站點框丟掉,因為
560x452 為了含住 clean region 比站點框大得多。要再快就得處理這件事 ——
clean region 需要那塊面積,但形狀比對不需要在那裡跑。

### 因此,先前關於速率的結論全部要重排

- 相機 35.18 fps:全幅時是牆,裁完不是。
- `min_detect_sep_us` 28571(35/s):**從來不是實際瓶頸**,實際只跑到 4.7/s。
- **檢測運算是唯一真正的限制**,而且要走到 30+/s 還差 2.4 倍(現況 12.4)。

順序:**先裁 ROI(相機 + 比對範圍),量到檢測 mean < 33 ms,再談閘門。**
反過來調閘門只會換到 err 1 或 err 13。

工具:`UI/WebUI/tools/webctl/fi_hold.mjs` —— 開一個 FI session 並掛著,
讓 `real_parts.py` 在底下轉盤子。`insp_driver.mjs` 送的是 CI 且跑完 N 幀就
離開,量串流可以,當負載不行:CI 是軟體觸發、station region 不生效,
兩者都不是生產路徑。
