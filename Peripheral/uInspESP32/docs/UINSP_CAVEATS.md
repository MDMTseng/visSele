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

## 更正:`ImgInspection` 印的是 CPU 時間,不是 wall time(2026-08-08 當天稍後)

上一節把 `ImgInspection] Xms` 當成每幀的實耗時間用,**那是錯的**。

```c
clock_t t = clock();          // POSIX clock() = 行程 CPU 時間,跨所有執行緒累加
... me.FeatureMatching(test1_cv); ...
LOGI("%fms \n", (double)(new_t - t) / CLOCKS_PER_SEC * 1000);
```

shape matcher 是多執行緒的,所以這個數字是 wall time 乘上「用了幾個核心」。
拿它除 1000 去算幀率,會把機器的能力低估掉那個倍數。

抓到它的方式:板子放行 38.9/s、`inspected/admitted` 100.2%,而同一份 log
說每幀 62.75 ms —— 單一 `ImgPipeProcessThread` 下這在算術上不可能。改用
`New frame go` 的時間戳直接量連續幀間隔,得到 25.68 ms,正好是
62.75 / 2.4 個核心。

**正確的數字(以 `New frame go` 時間戳量的 wall time):**

| | 幀間隔 p50 | 幀間隔 mean | CPU-ms | 用了幾核 |
|---|---|---|---|---|
| 全幅 2448x2048 | 45.7 ms | 178.5 ms | 211.5 | ~1.2–4.6 |
| ROI 560x452 | **19.28 ms** | 25.68 ms | 62.75 | ~2.4 |

全幅那一列 mean 遠大於 p50,是因為那些量測期間機器一直在停機,停頓被算進去了;
p50 才是它跑順時的樣子。

**所以「檢測只能跑 4.7/s」這個結論是錯的** —— 那是 211.5 CPU-ms 除出來的。
實際上全幅順跑時 p50 45.7 ms = 21.9 幀/秒的能力,裁 ROI 後 19.28 ms =
51.9 幀/秒的能力。ROI 仍然是對的槓桿(2.4 倍),結論的方向沒變,錯的是倍率
和絕對值。

**任何用 `clock()` 量多執行緒區段的地方都有同一個問題。** 要 wall time 就用
`std::chrono::high_resolution_clock`(`INSP_PROF` 那段就是對的寫法)。

## 實際能跑多快(2026-08-08,ROI 560x452,真料,5 分鐘 soak)

| plate_freq | 閘門上限 | 感測器 | 放行 | 判定 | UNANS | 結果 |
|---|---|---|---|---|---|---|
| 26000 | 71/s | 41.0/s | **30.60/s** | 30.59/s | 0 | 300s 乾淨,9271 顆 |
| 32000 | 100/s | 49.9/s | **38.95/s** | 38.98/s | 0 | 300s 乾淨,11788 顆 |

兩趟每個 12 秒桶都穩(30.3–31.0 / 38.7–39.3),`inspected/admitted` 100%,
零 SKIP、零未答、無錯誤。**30+ 達成,而且 39/s 也是穩的。**

剩下擋住的是料件實際間距:感測器給 49.9/s,`rate` 那格丟掉 10.0/s ——
那不是設定值太保守,是相鄰料件真的靠得比 10 ms 近。

### 三次 soak 失敗是盤速不穩,不是速率

在拿到上面兩趟之前,300 秒 soak 連掛三次(`err [1]` / `err [2]`,
`UNANSWERED` 撞到 `unanswered_stop_after=10`)。同樣設定的 60 秒卻乾淨。
區別在閘門階梯的 `rej_unstable`:

```
失敗   unstab 263 / 158 / 399
乾淨   unstab 0
```

**`rej_unstable > 0` 是這件事的早期警訊**,而它只有在階梯被印出來時才看得見。
一度懷疑是自己的儀器(`INSP_LOG_KEEP_STDERR=1` 寫了 122 MB 同步 stderr),
關掉之後仍然失敗,所以不是 —— 但那個懷疑值得記著:**量測用的 log 不是生產
路徑**,生產下 stderr sink 是關的。

## 放寬間距限制:49/s,以及延遲的實際帳(2026-08-08)

§O 的上界是 `2*TOL_US <= GATE_SEP_EFF_us`,下界是「速率相關的殘差尾巴」
(§M:全幅 30 Hz 時 delta_max 爬到 3308 us)。**裁 ROI 之後那條下界不見了:**

```
plate 32000, sep 10000, window 5000
  39.01/s   delta_max 54 us   對 5000 us 視窗   agree 3927   DISAGREE 0
```

**92 倍餘裕。** 所以視窗可以縮,間距跟著縮:

```
window 5000 -> 1000,  sep 10000 -> 2000  (上限 500/s)
  49.16/s   rej_rate 0   delta_max 277 us   DISAGREE 0   inspected/admitted 100.1%
```

`rate` 那格**歸零** —— 閘門一顆都不丟了,5135 個邊緣進來、5041 顆放行,
差額只剩 94 個寬度濾掉的。從 39/s 到 49/s,而現在唯一的限制是盤子每秒
送幾顆(50.08/s)。

### 相機觸發到報告抵達的實際延遲

`report_latency` 量的是 **gate → 報告**(`trig_us` 在 `newPulseEvent` 打的,
不是相機觸發)。要相機起算就扣掉運輸段:

```
plate_freq 26000 -> 步進時脈 52000 步/秒   (pulses_per_rev 60000)
CAM1_on  9315 步  ->  gate→相機      179.1 ms
SWITCH  29900 步  ->  gate→SWITCH    575.0 ms
                      相機→SWITCH    395.9 ms  = 答案預算
```

實測(39.85/s,`unstab 0`,UNANS 0,n=5450):

```
gate→報告        avg 207.2 ms    max 488.2 ms
相機→報告        avg  28.1 ms    max 309.1 ms
                                 = 預算的 78%
```

**平均只吃掉預算的 7%,最差那顆吃掉 78%。** 這解釋了為什麼盤速不穩會致命:
運輸時間跟盤速成反比,盤子一快,395.9 ms 的預算就縮水,那個 78% 立刻破表 ——
`OBJECT_HAS_NO_INSP_RESULT`。餘裕不在平均值裡,在尾巴上。

### `rej_unstable` 仍然是頭號未解問題

放寬間距的五趟裡有三趟被它打斷(t+24s / t+50s / t+244s),而每一次
**配對本身都是健康的**(`delta_max` 48–93 us、`DISAGREE 0`、`rejected 0–1`)。
掛的是 `err 1`/`err 2`,起因是盤速不穩而不是速率或間距。plate_freq 32000
比出貨的 15000 快一倍以上,懷疑是步進在負載變化下守不住轉速 —— 未證實。
**要先解決這個,49/s 才談得上是可用的數字。**

`reset_running_stat` 順帶把 `cam_sync` 也清掉,而且在該趟窗口內沒有重新建立
(`agree`/`delta_max` 全程 0,判定退回 tid 路徑)。要留配對證據就別在量測前
呼叫它。

## 被忽略的觸發不留任何痕跡 —— 量測,不再是假設(2026-08-08)

整個 timestamp 配對機制的前提是「序列不可信」,而那一直是推論出來的,沒有人量過。
量了。

**條件:** ROI 560x452(相機能力 184.89 fps)、硬體觸發 Line0、
`trig_cam_burst` 發出精確數量的脈衝、主機端用 Aravis 收幀並讀 `frame_id`。

```
triggers fired    200        @ 400 Hz(遠超相機的 184.9)
frames received    94
frame id range    0 .. 93    span 94
id discontinuities  0
```

**106 個觸發被相機忽略,`frame_id` 完全連續。** 序數配對會從那一刻起偏移 106 格,
而且沒有任何一個訊號會顯示它發生過。這就是 C-1 存在的理由,現在它是量出來的。

原因很直接:觸發被拒絕時相機**不產生幀**,也就不消耗 `frame_id`。`frame_id` 數的
是相機**送出**的幀,不是它**收到**的觸發 —— 所以它抓得到傳輸掉幀(號碼跳號),
抓不到觸發被丟。而後者正是超速下的實際失效模式。

### 但這顆相機其實數得出來 —— `FrameSpecInfo`

MV-CA050-11UM **沒有 GenICam chunk**(`arv-tool features | grep -i chunk` 是空的,
所以 `CameraLayer_Aravis.cpp` 裡把 chunk 關掉是對的)。Hikrobot 用另一條路:
`FrameSpecInfoSelector` + `FrameSpecInfo`,把資訊**寫進影像像素**。

可選欄位:`ROIPosition` `LineInputOutput` **`ExtTriggerCount`** **`Framecounter`**
`BrightnessInfo` `Exposure` `Gain` `Timestamp`。

開啟後 **payload 完全不變**(253120 = 560×452),所以它是**覆寫**既有像素,不是
附加。沒有手冊,所以用行為把欄位找出來:

```
[慢] 25 觸發 @ 40 Hz  → 25 幀
     off=0 與 off=4(32-bit LE)都以 1 遞增

[快] 120 觸發 @ 400 Hz → 56 幀,frame_id 連續、零斷點
     off=0  仍然以 1 遞增                     -> Framecounter
     off=4  平均 2.16 遞增(min 2 max 3)      -> ExtTriggerCount
            值域 27..146,span = 120 = 發出的觸發數,完全吻合
```

**相機把它拒絕的 64 個觸發也數進去了。** 所以「哪一個觸發產生了這一幀」在硬體層
是可知的,只是必須從像素裡讀。

**代價與注意事項:**

- 浮水印佔第一列(ROI 的第 0 列 = 全幅 y=428)。目前的站點設定
  `inspection_region` 從 y=432 起、`clean1` 從 y=429 起 —— **剛好都不重疊**,但
  這是巧合不是設計。改動 ROI 或站點區域之前必須重新確認。
- 上面只驗證了 off=0..7 這 8 個 byte。整列其餘部分可能被其他啟用的欄位佔用,
  開之前要把完整佔用範圍量出來。
- 用完必須關掉。留著開會讓那些像素永遠是計數器而不是影像。

### 這對 C-1 的意義

**不是拿它取代 timestamp 配對。** 計數器是序數機制,需要兩端對起點、而且任何
desync 都會延續下去;時間戳是自我描述的證據,不累積誤差、不需要共享狀態。

真正的價值是**獨立的交叉稽核**:`ExtTriggerCount` 來自相機自己,不是另一個軟體
機制。它能回答目前完全無法區分的兩件事 ——

```
相機根本沒收到 / 收到了但拒絕     -> ExtTriggerCount 停住 vs 跳號
產生了但傳輸中掉了               -> frame_id 跳號
```

今天所有的「掉幀」在數據上長得一模一樣。有了這兩個計數器,它們就分得開,而且
`tid` 那套過渡期的交叉稽核可以退場(見 FIRMWARE_CONTRACT I-2b)——
換成一個不會從側面繞過 C-1 的稽核者。

工具:`$CLAUDE_JOB_DIR/tmp/trigcount.py`(觸發 vs frame_id)、
`specdecode.py`(用行為定位 FrameSpecInfo 欄位,退出時一定關閉)。

---

## 一夜的 soak,和五次量到空的(2026-08-08 夜 ~ 08-09 晨)

七項主題各跑一次,每項只宣稱一件事。跑完之後真正站得住的結論比預期少,
而**站不住的那些全都長得一模一樣**:治具沒有把機器帶到它宣稱的區域,
輸出卻讀起來像是它有。這一節記的是那個形狀,不是那些數字。

### 站得住的

- **`regress` 三小時,42989 筆判定 100% 對照 tid 圖樣,disagree 0,零錯誤。**
  這是 2026-08-06 那批「改完沒再看到」的修正第一次有時間替它們作證。
- **96 bytes/RECAL 不是洩漏。** 430 次 recal,`free_heap` 從第 10 分鐘起
  釘在 196252 一個 byte 沒動。閒置的 `recal_leak` 問不到這題(`recals=0`,
  因為 `recalService` 被 `!CAM_SYNC.valid` 擋住,而閒置機器永遠不 valid)。
- **注入的時間戳噪音與裝置量到的偏離精確吻合**(`miss_max` = 3003 / 6007 /
  20002 對應 ±3000 / ±6000 / ±20000)。儀器本身可信。
- **停機而不猜是真的。** 十段噪音實驗、多次 `CAM_CLOCK_LOST`,`BAD` 全 0。

### 五次量到空的,以及它們共同的形狀

1. **判定圖樣沒開** —— `jitter_sweep` / `regress_watch` 需要核心帶
   `INSP_PERIF_VERDICT_PATTERN`,否則每顆的判定都是 NA,而通過條件
   「零個錯置判定」在沒有判定時自動成立。
2. **沒逼到視窗** —— 標稱 40000us 加減 10000,最小間距還有 24736us,
   離視窗五倍遠。掃描通過了,但通過的是判準的前半段。
3. **閒置機器沒有 recal** —— 見上。
4. **一顆種子跑一整排** —— 三段大偏離停在 406/408/407,量值差 7 倍。
   那不是「量值無關」,是同一個事件跑了三次。改成獨立種子後變成
   208 / 0 / 66。**列與列不獨立的掃描,會在最沒有資訊量時看起來最紮實。**
5. **視窗被夾住而工具印的是請求值** —— 裝置把 `match_window_us` 夾到
   `min_detect_sep_us/2`。測試治具用 `gate 2000`,所以實際視窗一直是
   **1000us**,而每份 log 的表頭都寫著「window 5000us」。
   要掃視窗,必須同時把 `min_detect_sep_us` 拉到 `2×window` 以上。
   **生產設定不受影響**:`min_sep 28571` 的夾制上限是 14285,
   所以產線的 5000us 視窗是真的。

共同形狀:**工具忠實地印出它請求的參數,裝置安靜地夾住/忽略它,兩邊都沒說謊。**
判準過了,而機器從未進入會讓它失敗的區域。

### 治具自己弄死了相機

一批 run 全部 `never READY`,連一小時前乾淨跑完 1472 顆的最輕那段也一樣。
**那種一致性就是線索:受測物已經不是原本那個東西。** 無注入的對照重現它,
相機則直接拒絕 `AcquisitionStart`:

    USB3Vision write_memory error (invalid-parameter)

相機仍在列舉、仍接受設定,就是不出圖 —— 校準拿不到影像,永遠進不了 READY,
讀起來完全像是注入造成的結果。

兇手是排程器:`core_stop` 在 SIGINT 逾時後升級成 SIGTERM,而那批 run 都在
一分鐘內停機,core 被高頻啟停。**core 在釋放相機前被殺,相機就對之後每一顆
core 卡死。** 已改成只送 SIGINT、不升級 —— 卡住的行程只賠一個 run,
卡死的相機賠掉之後的每一個,而且無聲。

**板子的健康檢查抓不到這個**:相機卡死時板子是真的健康(`state 100`、
`error_hist []`)並如實回報。故障在板子上游,症狀在下游。所以 `never READY`
現在會直接問相機(此時 core 已停,問得到),試一次 `DeviceReset`
(這次成功,沒動 USB 線),回不來就中止整條隊列。

### 還開著的

- **`report_latency` avg 478ms / max 666ms**,而 CAM→SWITCH 預算是 396ms,
  所以 `real` 在 5 分鐘後以 `err=[2]` 停機。先前量到的是 28.1ms,差 17 倍,
  中間有東西變了。配對本身無辜:disagree 0,殘差 99.97% 在 64us 內。
- **一次機器毫無警覺的錯置**:視窗掃描中總有一段出現 `BAD`≈`n`/2 且
  `n` 只有正常的四分之一(357/184 與 332/170),`state=101`、`err=[]`。
  但**它會跟著我改的變數換一段**(gate 隨視窗變時壞 win7000,gate 固定時
  壞 win4000),所以不是視窗造成的。那個簽名比較像啟動/校準期的退化狀態。
  **未歸因,不要當成已知結論引用。**
- **`offset_us` 污染的假說未被測到。** `gate()` 最後一行是
  `offset_us = cam_ts - nearest_cam_us`(整個覆寫,不混合),
  推論上被接受的偏離會拉走時鐘 —— 但今晚沒有一組實驗真的問到它。
- 曝光延後:相機在過觸發時是**丟棄**而非排隊(200 發→94 幀,frame_id 連續),
  所以無限延後不成立。有界的那一種(觸發落在讀出期間)仍未量。
  關鍵是 `cam_ts` 指的是觸發到達還是曝光 —— 前者的話殘差對曝光時機一無所知。

### 更正:相機卡死的兇手**不是** SIGTERM 升級(2026-08-09 07:00)

上一節把相機卡死歸咎於排程器的 `core_stop` 在 SIGINT 逾時後升級成 SIGTERM。
改成只送 SIGINT 之後,**同一個卡死在四小時內再次發生** —— 全程沒有任何
SIGTERM。所以那個根因沒有成立,它只是當時唯一看得順眼的解釋。

目前**確立**的只有三件事:

- 它會反覆發生,與 `core` 的開關相機週期有關。
- 症狀一律是 `AcquisitionStart` 回 `USB3Vision write_memory error
  (invalid-parameter)`,而列舉與設定都正常。
- **`DeviceReset` 每次都救得回來**(兩次成功),不必碰 USB 線。

**還不知道**的是什麼動作真正弄壞它。所以「只送 SIGINT」這個改動要留著
(它本來就是對的:硬殺一顆持有相機的行程沒有好處),但**不要**把它當成
已修好卡死問題。

實務上的防線不變,而且不依賴知道兇手:
`never READY` 一律直接問相機,因為板子在相機卡死時是真的健康並如實回報。
第一次寫這條時我以為是在描述一個已解決的問題;實際上它是唯一擋得住的東西。

另外 `DeviceReset` 會把幾何打回全幀(2448x2048 / 35.18 fps,對
560x452 / 184.89 fps),所以自動復原之後**必須把 region/exposure 放回去**,
否則後續每個 run 都在另一個操作點上跑而 log 不會說。

### 定位:弄壞相機的是核心的取像路徑(2026-08-09 07:30)

前一節說兇手未知。用一組乾淨的前後對照切出來了:

    core 開著閒置 20 秒 -> 停止   相機 OK        <- 開/關本身不會壞
    core 進入檢驗模式   -> 停止   WEDGED

而且失敗當下的證據更直接 —— 板子在 `state 102` 打了 **85 發校準脈衝,
核心回了 0 份報告**,30 秒後 `CAMSYNC CAL FAILED (learned=0 boot_n=0)`。

所以比較合理的解釋不是「core 事後把相機弄壞」,而是
**core 自己的 `AcquisitionStart` 在當下就失敗了,然後安靜地繼續跑**:
板子照打脈衝,核心根本沒在取像,而沒有任何一層說出這件事。
從板子看,這與「相機好好的但校準不收斂」完全無法區分。

排除掉的:

- **不是治具的訊號。** 只用 SIGINT 一樣復現(見上一節的更正)。
- **不是開關週期。** 閒置 20 秒的 core 停掉後相機正常。
- **不是設定組合無效。** 單機設成全幀 + `FrameBurstStart`/`Line0`/`TriggerMode On`
  (core 執行後留下的狀態),`AcquisitionStart` 正常。

還知道的兩件事:

- core 執行後 `region` 一律變回 **2448x2048**,所以幾何是 core 自己設的,
  外部先設好的 ROI 不會存活。ROI 消失的謎團到此結案。
- `DeviceReset` 每次都救得回來(三次),不必碰 USB 線;但它也把幾何打回全幀。

**這一條擋住了所有真實料件的量測**(`soak_real` 兩次都在啟動時就 `err 14`),
所以它現在是優先序第一,排在報告延遲之前 —— 延遲的儀器已經寫好、build 完,
但在相機能穩定取像之前跑不到。

下一刀應該切在核心的 `CameraLayer` 取像路徑:`AcquisitionStart` 的回傳值
有沒有被檢查?失敗時有沒有任何一層會說話?從目前的證據看,答案像是「沒有」。

### 更正二:相機故障是**間歇的**,而且我沒有隔離出來(2026-08-09 07:50)

上一節說「弄壞相機的是核心的取像路徑」,並附了一個乾淨的前後對照。
再往下追之後,那一節的**歸因也不成立**。完整的證據序列:

1. 外部先設 ROI 再開 core → 85 發脈衝、0 份報告、`err 14`。
2. 重置後**完全不碰幾何** → 校準 3952 ms 收斂(14 脈衝、learned=8)。
   → 當時我據此宣告「外部設幾何是觸發條件」。
3. 依此改核心建構子改用 `StartAquisition()`(它有串流重建與重試)
   → 外部設 ROI 的情境**仍然失敗**。
4. 退回原版 → **不碰幾何的路徑現在也失敗**(101 脈衝、learned=0)。

第 4 點推翻了第 2 點:那一次成功是**單一觀察**,不是「不碰幾何就好」的證據。
而第 3 點我一度誤判成「我造成的退步」,實際上只是同一個間歇故障。

所以現在**確立**的只剩:

- 症狀固定:`AcquisitionStart` 回 `USB3Vision write_memory error
  (invalid-parameter)`,而列舉與設定都正常。
- `DeviceReset` 每次都救得回來(四次),不必碰 USB 線。
- 核心在取像失敗時**照常運行**:板子打脈衝、零份報告、
  最後以 `CAM_CLOCK_CAL_FAILED` 停機。從板子看與時鐘問題無法區分。
- `set_region` 本身無害(單機設完 560x452 仍能 `AcquisitionStart`)。

**沒有確立**的:什麼動作真正觸發它。SIGTERM 升級、外部設幾何、payload 不匹配
—— 三個假說都被自己的實驗打掉。

給下一個人的建議:不要再從外部行為找觸發條件,那條路我走了三次都錯。
改從核心內部找 —— `CameraLayer_Aravis` 建構子那條路徑與 `StartAquisition()`
是重複的兩份程式碼(檔案自己的註解已經指出這件事害過一次 bug),
而失敗時 `acquisition_started` 留在 false、沒有任何上層檢查它。
**先讓那個失敗說話**(讓核心在取像啟動失敗時拒絕進入檢驗模式,
而不是安靜地跑一台沒有影像的機器),再談找觸發條件 ——
今晚有兩次一小時等級的誤診,都是因為這個故障偽裝成別的東西。

### 歸因完成:err 14 是我自己的啟動方式(2026-08-09 11:45)

`soak_real` 每次都以 `err 14` 停在啟動,而我為此提出並殺掉了九個假說:
SIGTERM 升級、外部設幾何、payload 不匹配、機電干擾、盤速、`stepper_enable`、
校準中輪詢、指令間隔、每跑一次退化。**九個都錯,而且錯得很有系統性** ——
它們全都是在同一個被我自己弄壞的啟動方式下觀察到的。

決定性的實驗不需要理解差異:同一顆 core、同一台相機、**交替**跑兩種序列各三次
—— 兩種都過。再把 `soak_real` 當子行程掛在治具的 `core_start` 底下 —— **通過**。
差別只在**怎麼啟動 core**:`core_start` 會等 4090、再等 4099、再給 6 秒,
並清掉所有 `INSP_PERIF_*` 後只設 console;我的手動 `nohup` 只等 4099。

累計 47 次受控試驗 + 6 次 A/B 全部通過。教訓不是「要等久一點」,而是:
**在一個自己沒有驗證過的啟動路徑上做的所有實驗都是廢的**,而它們看起來
完全像可靠證據 —— 每次失敗、症狀相同、可重現。

### 報告延遲的拆解(2026-08-09 11:45,2400 個樣本)

```
perif write   avg 0.43ms   max 24.14ms
queue wait    avg 2.20ms   max 707.71ms      qdepth 0  drops 0
端到端         avg 479ms    max 812ms
```

**平均與尾巴是兩個不同的問題:**

- 平均 479ms 裡,核心->板子只佔 **2.6ms**;其餘約 476ms 全在送出佇列的
  **上游**(相機曝光/讀出/傳輸 + 檢驗運算)。`PerifSendThread` 上方那句
  「串列寫入可能阻塞超過一秒」讓串列埠成為最自然的嫌犯,而它平均 0.43ms。
- 尾巴相反:812ms 的最壞端到端裡,**707ms 是單一報告卡在佇列**。
  取樣時 `qdepth` 是 0,所以不是積壓 —— 送出執行緒被餓到或阻塞在別處。

所以**平均往上游修,尾巴往佇列修**,是兩件工作。這個拆解只有在把
queue wait 與 write time 分開量之後才看得到;合成一個數字時,
兩者會互相掩蓋。

同一次跑出 `DISAGREE 1933/2374`(81%),而幻影測試一直是 0。
不是錯置(`delta_max 168us`,殘差 99.96% 在 32-64us,時間戳側很確定),
是核心的 FIFO 序數配對在報告晚 479ms 的情況下大量錯位 —— 未追。

### 序數配對落後的是「在途報告數」(2026-08-09 12:05)

真實料件下 `disagree` 高達 81%,幻影測試卻一直是 0。抓 `CAMSYNC MISMATCH`
明細之後,發現它**不是雜訊,是一個常數**,而且隨盤速等比放大:

```
盤速  5000   偏移 +1     agree 311 / disagree 131   (70% 同意)
盤速 10000   偏移 +2     agree   0 / disagree 1020  (100% 分歧)
盤速 15000   偏移 +4     agree   0 / disagree  768  (100% 分歧)
```

95% 以上的分歧都落在單一偏移值上(10000 時 1029/1084 是 +2),
而時間戳側的殘差中位數是 **1-3 us** —— 它非常確定自己指的是哪一顆。

偏移 = **速率 x 報告延遲** = 在途報告數。延遲固定在約 479ms(見上一節),
所以速率一提高,核心的 FIFO 序數配對就落後更多格。

**這解釋了這個缺陷為什麼躲得掉:盤速 5000 時有 70% 是對的。**
慢速試機看起來只是「偶爾不一致」,而不是「系統性錯誤」。
在產線目標的 30+ 顆/秒下,偏移會是十格以上 —— 序數配對不是有點風險,
是**每一顆都錯**。

目前機器是安全的:`REPORT_MATCH_TS` 讓時間戳成為權威
(`tarP = (REPORT_MATCH_TS && byTs) ? byTs : ...`),所以判定跟著時間戳走。
但這也意味著 `PERIF_CORE_PAIRING` 的 tid 稽核在真實料件下**永遠會吵**,
而它吵的是對的 —— 這是遷移到裝置端配對的實證理由,不是理論理由。

推論:降低報告延遲會直接減少偏移。兩者是同一個問題的兩面。

### 更正:`report_latency` 量的是 gate->report,不是相機->report(2026-08-09 13:00)

追了很久的「479ms 延遲」大部分是**料件在盤上走路的時間**。韌體自己寫著:

```c
// Registration wall time (lower 32 of esp_timer_get_time), for the
// gate->report latency stat.
uint32_t trig_us;
```

`trig_us` 在 `newPulseEvent`(閘門偵測)蓋章,不是相機觸發。所以
`report_latency` 包含閘門到相機的行程,而那在盤速 10000 下就是幾百毫秒。

連帶要撤回的三件事:

- **「479ms 延遲吃掉 396ms 的 CAM->SWITCH 預算」** —— 比錯了。預算的起點是
  相機觸發,不是閘門。
- **「先前量到 28.1ms,現在 479ms,差 17 倍,中間有東西變了」** —— 沒有東西
  變,那是兩個不同區段的數字。我把單位不一致當成異常追了很久。
- **真正的電子延遲**是核心端拆解出來的約 **13ms**
  (inspect 9.78 + queue 0.13 + wait 2.98 + write 0.47),對任何盤速的預算
  都綽綽有餘。

`cam_us`(相機觸發的真實時刻,完整 64 位元)已經在物件裡,只是沒有被拿來
算延遲。加一個 `now - cam_us` 的統計就能直接量電子延遲,不必用減法推。

`UNANSWERED` 的成因因此改寫:**平均沒問題,是尾巴。**
`wait max 808ms`、`write max 158ms`、`inspect max 96ms` —— 單筆極端值加上行程
時間才會超出預算。要修的是送出執行緒被餓死那件事,不是平均。

### 帶真實配方的拆解(2026-08-09 12:56,n=2880)

WebUI 載入 `10155 3G2570090B-1`、工位 318x424 @1380,432 已套用:

```
inspect  avg 9.78ms   max 96.19ms     ← 真實檢驗,比先前以為的 19-45ms 低
queue    avg 0.13ms   max 54.57ms
wait     avg 2.98ms   max 808.87ms
write    avg 0.47ms   max 158.38ms
端到端    avg 516.6ms  max 647.2ms    (gate->report,含行程)
判定      SEL3 1506 / NA 1360 / UNANSWERED 10
配對      agree 2866 / disagree 0 / delta_max 163us
```

**`disagree` 是 0** —— 而一小時前在**沒有配方**(全 NA、核心幾乎不運算)的
狀態下,同樣盤速量到的是 100% 分歧、偏移 +2,並已寫進上一節。
**那個結論必須重新驗證**:它可能是無配方狀態的產物,而不是序數配對的性質。
在有配方的條件下重跑盤速掃描之前,不要引用它。

### FrameSpecInfo 的佔用範圍,以及偏移不是固定的(2026-08-10)

上一節留了兩個未量的缺口:「只驗證了 off=0..7 這 8 個 byte,整列其餘可能被其他
欄位佔用」,以及浮水印是不是真的只有一列。兩個都量了(`specmap.py`)。

**方法上的教訓先講**,因為第一次量出來的結果是廢的。原本用「和全關的基線不同
且逐幀變動」判定佔用,結果在 row 1 也噴出 65 個「佔用」——而 row 1 按定義不該有
浮水印。曝光 50µs 配脈衝背光,影像本身每幀都在跳(逐 byte 噪聲 mean 18.8,
1120 個 byte 全部 noise>8),「與基線不同」這個判準在這種場景下沒有解析力。

改用**結構性判準**:計數器必然是單調遞增、步長有界的 32-bit LE。並且先跑一次
全關的對照,量出這個判準自己的假陽性率:

```
CONTROL(全關,1116 個 offset):通過單調測試者 0 個
```

零假陽性,下面的結果才有分母。**任何「這裡看起來像計數器」的宣稱,沒有對照就
沒有意義。**

**結果:**

```
[只開 Framecounter]     off=0        step 1.00
[只開 ExtTriggerCount]  off=0        step 1.00      <-- 注意,也在 0
[兩個都開]              off=0, off=4 step 1.00 both

row 1:兩種組態皆 0 個欄位  -> 浮水印確實只佔一列
row 0 佔用:byte 0..7,無其他
```

**偏移是按啟用順序打包的,不是固定位址。** 上一節記的「off=0=Framecounter、
off=4=ExtTriggerCount」**只在兩個都開時成立**。只開 `ExtTriggerCount` 時它在
off=0。解碼端若寫死 `offset=4`,日後任何人關掉 `Framecounter` 就會安靜地讀到
另一個計數器——不會報錯,只會開始配錯。

→ 解碼前必須先確立啟用集合,或乾脆**永遠兩個都開並校驗兩者關係**
(慢速時兩者步長都應為 1;`ExtTriggerCount` 步長 > `Framecounter` 步長時,
差值就是相機丟掉的觸發數)。

**幾何確認:** 浮水印列 = ROI 第 0 列 = 全幅 y=428。`clean1` 從 y=429、
`inspection_region` 從 y=432 —— 不重疊。但這仍然是巧合(ROI 的 y 是 428),
**動 ROI 的 y 或站點區域之前必須重跑 `specmap.py`。**

**這一輪沒有重現超速分歧**:跑 30 Hz,相機跟得上,兩個計數器都以 1 遞增。
分歧是上一節 specdecode 量的(120 觸發 / 56 幀,off=4 平均遞增 2.16,span 120)。
這次量的是佔用範圍,不是分歧。

## 2026-08-11 — a wedged camera reads as a perfectly configured one

The camera stopped delivering images and every diagnostic said it was fine.
GenICam queries answered normally: `TriggerMode On`, `TriggerSource Line0`,
`TriggerActivation RisingEdge`, `AcquisitionBurstFrameCount 1`, the correct
560x452 ROI, exposure 50us, payload 253120 bytes. The board was provably
firing -- `trig_cam_burst` replied `emitted 3`, offsets 11/5000/10001us,
jitter 12us. No error was raised anywhere.

It delivered zero bytes. Aravis on its own, with our core not running:

    n_completed_buffers = 0    n_transferred_bytes = 0    n_failures = 0

A physical replug of the camera fixed it. Same tool immediately after:
35 frames/s, 175 MiB/s, 564 buffers, 0 failures.

**A replug is not required -- `DeviceReset` clears it** (2026-08-11, later the
same day, after the third occurrence):

    arv-tool-0.8 -n Hikrobot-<serial> control DeviceReset

"DeviceReset executed", the camera re-enumerates in ~10 s, and a free-run test
immediately after returned 567 buffers / 2.84 GB at full frame. Nothing may be
holding the device when this is issued -- stop the core first, and check for
stray `arv-camera-test` processes, one of which was left behind earlier and
made a later diagnosis read as LIBUSB_ERROR_ACCESS.

### What actually triggers it

Three occurrences, and every one followed a switch to the FULL-FRAME ROI.
Nothing wedged across a 5-hour run, a 12.5-minute run or a 6.3-minute run at
the production crop.

The first was attributed to bandwidth: BGR8 at full frame is 15.04 MB/frame,
556 MB/s at 37/s, well past USB3. **That explanation is not sufficient.** The
third occurrence was on Mono8 -- 5.01 MB/frame, ~150 MB/s at 30/s, and the
camera demonstrably sustains 175 MiB/s. It still wedged, and the settings still
read back perfectly (Mono8, 2448x2048, TriggerMode On, burst count 1).

So the suspect is the ROI CHANGE itself -- SetROI stops and restarts the
stream and re-sizes the payload -- rather than the steady-state data rate. Not
established; what is established is that full frame is where it happens and
the crop is where it does not.

**Check `cam_max_fps` first.** It is derived from the frame interval and needs
no watermark, no pairing and no board, so it separates "no frames" from every
other explanation in one read. Zero means stop diagnosing everything else. The
confirmation that owes nothing to our code is `arv-camera-test-0.8` with
`n_completed_buffers 0`.

Roughly two hours of runs were spent attributing this to the firmware, to the
camera ROI, and to the watermark decode -- all of which read as broken because
nothing was being inspected. Note also that `n_valid 0` on `cam_trig` does NOT
mean "the watermark is off": it is equally consistent with no frames at all,
and it was read the wrong way here.

## 2026-08-11 — over the camera's floor, pcnt is not merely blind, it is wrong

Measured with a per-pulse backlight pattern, so the IMAGE says which pulse
exposed each frame and neither pairing mechanism has to be taken on trust.
Plate stopped, production crop, `trig_cam_burst` driving the train directly.

| | 150 Hz | 200 Hz |
|---|---|---|
| inter-frame `cam_ts` | 6666 us = the fired period | **5420 us = 184.5 fps, the camera's own floor** |
| `ext` step, median | 1.0 | 1.0 |
| exposure vs its own trigger, spread | **15 us** | **4966 us — 99% of one period** |
| flash pattern correct | 104/104 | 53/92 (chance) |
| frames that caught their own light | 48 of 104 | **3 of 96** |

Below the floor the camera exposes on the trigger edge and everything agrees.
Above it the camera keeps producing frames **at its own cadence** while the
counter keeps advancing roughly 1:1, so each frame slides ~420 us further from
the pulse it is labelled with and wraps a whole period every ~12 frames. 14 of
110 triggers also produced no frame at all, so BOTH failure modes are present
at once.

The consequence is the one that matters: **a frame's `pcnt` can name a trigger
it was not exposed with.** This is worse than the mechanism being unable to
detect its own failure -- it returns a confident, plausible, wrong answer, and
nothing inside the count can tell. `cam_ts` catches it, because the deviation
from the claimed trigger grows without bound.

So the two mechanisms are NOT peers. `cam_ts` is a measurement of the imaging
event and can abstain; `pcnt` is bookkeeping of the request and cannot.
Anything that treats them as interchangeable second opinions is wrong -- see
the dual-mode policy note in PAIRING_MIGRATION_STATUS.

### Two ways this measurement lied before it was believed

Both were defects in the instrument, and both produced confident nonsense:

- **A period-2 flash pattern cannot name a shift.** 010101 agrees with itself
  at every even offset, so scanning shifts and taking the best picks between
  ties on noise. It reported "SLIP of -4" on a run where all 240 triggers
  produced a frame and nothing had moved. Use a PRBS to measure a shift; the
  alternating pattern can only decide phase.
- **Median-centering hides a uniform spread.** Deviations spread over a whole
  trigger period, once centred on their median, all land within +/- half a
  period -- so a window test at half a period declares every one of them
  "inside". That is how the 200 Hz run first read as "96 usable frames,
  behaviour 2 (skip)" when the exposures had no relationship to their triggers
  at all. Judge the SPREAD, not the centred magnitude.

## 2026-08-11 — a short burst is absorbed at the slack rate, and it is measurable

Full frame, Mono8, 30 fps base with a tight cluster spliced in after pulse 55
(2 extra pulses at 100 Hz). Flash-identity pattern, anchored to the board's own
per-pulse emission times.

    ext 55 -> 57   (the first extra pulse produced no frame)
    ext 57  dev +8437 us
        58      +23544
        59      +18650       recovering ~4894 us per frame
        60      +13756
        61       +8861
        62       +3968
        63          -6       fully recovered

Three pulses landed in one 33333 us slot, so the camera was asked for 3 frames'
work. Its floor at full frame is 29051 us, giving an excess of 24769 us --
against a measured first deviation of 23544 us. Per-frame slack is
33333 - 29051 = 4282 us, against a measured recovery of 4894 us/frame. So

    frames disturbed  ~=  burst excess / per-frame slack

which is a leaky bucket with a drain rate that can be MEASURED rather than
guessed. A burst limit expressed as depth-plus-drain is therefore sound, and
one expressed as an instantaneous minimum gap is stricter than the hardware
requires.

What is NOT safe is sustained operation above the floor: at 200 Hz on the crop
(floor 184.5 fps) the deviation does not recover, it spreads over 83% of a
period and stays there. A burst is a bounded, self-healing disturbance; being
over the floor is a structural failure. Those two look identical to a max-minus-
min spread test and completely different to a p10-p90 one -- the first version
of this analysis called the burst run "numbering describes nothing" while its
own detail lines read 99/99 correct.

**During the disturbance the frames exist, are numbered, and are wrong.**
2 of 6 carried the right light and none were lit. Every one was caught by the
timestamp, and all 99 frames that landed inside the window carried exactly the
light their own trigger drove, with a brightness range of 1.8 grey levels. That
is the case for timestamp as the authority stated as strongly as this rig can
state it: both failure modes present at once, and it still separated usable
from unusable without a single error.


## The step ISR does not fit in its own tick when the act queue is loaded (2026-08-11)

`onTimer` runs at `2 * plate_freq`. Measured with `health.isr_dur_max_us`:

| act queue | ISR avg | ISR max | tick at pf 8000 | overruns |
|---|---|---|---|---|
| empty (plate turning, no inspection) | 2 us | 33 us | 62.5 us | 0 |
| loaded (virtual train, inspection on) | 3 us | **77 us** | 62.5 us | 3 -> 63 and climbing |

77 us does not fit in a tick above ~6500. At the production 10500 the tick is
47.6 us, so the peak is 162% of it; at 15000, 231%.

It survives on rarity -- 63 overruns in 1.37M ticks, against a 3 us mean -- not
on margin. Two things push it over:

* **Ramping with parts in the pipeline.** The tick shrinks while the queue
  stays loaded. Admitting parts mid-ramp was tried twice on 2026-08-11 and hung
  the board both times: complete UART silence, no boot banner, cleared only by
  a DTR reset. A starved main loop is exactly that symptom. The gate's
  `SYS_FREQ_STABLE` check is what has always kept the queue empty during a
  ramp, so the two had never met.
* **Higher speed.** The budget shrinks linearly with `plate_freq` while the
  ISR's work does not.

`health.isr_overrun_n` is the number to watch. Non-zero is not yet a failure;
climbing fast, or at a speed where `isr_dur_max_us` exceeds `1e6/(2*plate_freq)`
by much, is how far in it is.

## Floating point in the step ISR needs the FPU registers saved first

The first version of the measurement above read `PLATE_FREQ_CURRENT` (a float)
and computed `240000000.0f/(2.0f*f)` inside `onTimer`. The board went silent the
instant the plate was told to turn -- which is exactly when this ISR begins
running -- and stayed silent until a reset.

Not because floating point is forbidden there, but because using it without
saving and restoring the FPU registers corrupts whatever the interrupted code
had in them. That is what the "Restore FPU / and turn it back off" note at the
bottom of `onTimer` is about.

So there are two correct options, and only one of them is cheap:

* Save the FPU state around the arithmetic. Correct, and it costs cycles in an
  ISR that already does not fit its tick (see above).
* Do the arithmetic in the main loop and hand the ISR an integer. This is what
  `ISR_BUDGET_CY` does, and it is the default answer here precisely because the
  tick budget has no room to spend.

### Where the 77 us went: cold flash, not expensive work (2026-08-11)

It was never `Run_ACTS`. The table above compares an empty act queue against a
loaded one, but the queue was loaded with `virt_pulse`, which also switches on
`phantomServiceISR` and its `newPulseEvent` -- object admission. One experiment
moved two things and the write-up blamed one of them. A `trig_report on/off`
bisect built on that reading was also VOID for an unrelated reason (see below),
so nothing contradicted it.

`onTimer` now stamps `XTHAL_GET_CCOUNT` at four points and reports both each
segment's high-water (`health.isr_seg_max_cy`) and the breakdown OF THE TICK
that set the overall record (`health.isr_worst_seg_cy`), in CYCLES because
`StepGo` is under a microsecond. The second array is the one that answers the
question: 63 overruns in 1.37M ticks is a rare event, and a rare event does not
have to live where the averages do.

At pf 8000 (tick 62.5 us), worst-tick breakdown in us:

| build | step | gate | phantom | acts | total | overruns / ticks |
|---|---|---|---|---|---|---|
| all in flash | 1.5 | 60.3 | 0.2 | 17.7 | **79.7** | 291 / 946k |
| admission in IRAM | 0.4 | 25.8 | 0.2 | 10.9 | 37.3 | 0 / 946k |
| whole ISR path in IRAM | 0.3 | 0.4 | 23.1 | 7.9 | **31.7** | 0 / 946k |

`acts` never exceeded 27 us even in the worst build. The spike alternates
between `gate` and `phantom` and is never in both on the same tick, because
those are the two callers of `newPulseEvent`.

**Correction (2026-08-11, from soak analysis).** The reason first written here
-- "only one of them admits on any given tick" because of the sensor/phantom
ordering -- is WRONG. There is no interlock: `GateSensing()` and
`phantomServiceISR()` both call `newPulseEvent` unconditionally, in the same
tick. What actually keeps them apart is the fire-rate limiter at the top of
`newPulseEvent`, `if(curTime-_preTime < GATE_SEP_EFF_us) return -8;` -- the
second caller short-circuits after ~169 cycles. The conclusion holds and the
stated reason did not, which matters because it makes the guarantee CONDITIONAL
on `GATE_SEP_EFF_us` being non-zero. Both paths were genuinely active during the
soak (~17.5 real sensor edges/s plus ~12 injected/s). So the cost is admission, and it is once per object -- which is
exactly the rarity that was mistaken for a rare expensive task body.

It is not computation. Admission runs once per ~1200 ticks, so every line of it
was a cold instruction-cache miss against flash. `IRAM_ATTR` on `newPulseEvent`
and `ActRegister_pipeLineInfo` alone took the worst tick from 79.7 to 37.3 us
and the overruns to zero; `StepGo`, `GateSensing`, `phantomServiceISR` and
`Run_ACTS` followed. Note that the empty-queue case improved too (18.2 -> 6.8 us)
and so did segments that were not touched -- keeping admission out of the cache
stops it evicting everyone else's code.

What is left is real work: ~22 us inside `newPulseEvent`, in IRAM, per admitted
object. Budget now: 51% of the tick at 8000, 67% at the production 10500, 95% at
15000. It no longer overruns at any speed the machine runs, but 15000 is the
wall, and the next place to look is the `double` divide in `_PLAT_DIST_step` and
the two `esp_timer_get_time()` calls.

**The measurement rule this establishes:** on this chip, "is this code slow" and
"is this code cold" are different questions, and for anything that runs rarely
inside an ISR the second one is usually the answer. Measure with segments, not
with feature on/off -- a feature toggle moves cache locality too.

#### What the remaining ~23 us is, and what removing it would cost (2026-08-11)

Split inside `newPulseEvent` the same way (`health.isr_npe_worst_cy`, order
`[pre, ringhead, fill, actreg, tail]`), at pf 8000:

    pre 0.7   ringhead 0.2   fill 1.6   actreg 20.5   tail 1.6   = 24.6 us

All of it is `ActRegister_pipeLineInfo`, which is nine `ACT_PUSH_TASK` and a
`space()` check. Note that `size()`, `space()`, `getHead()` and `getTail()` do
NOT lock -- only `pushHead`/`consumeTail`/`pullHead`/`clear` do -- so admission
takes nine critical sections, not one per call site.

Measured directly by rebuilding `RingBuf`'s critical section as plain interrupt
masking instead of a per-instance `portMUX` (bench only, reverted):

| build | actreg | newPulseEvent | worst tick | acts seg | overruns |
|---|---|---|---|---|---|
| portMUX (shipping) | 20.5 us | 24.6 us | 32 us | 9.0 us | 0 |
| interrupt mask (experiment) | 10.5 us | 13.4 us | **20 us** | 6.5 us | 0 |

So the lock is worth about 1.1 us per push, 10 us per admitted object, 12 us off
the worst tick. `Run_ACTS` improves too, because it also pushes and consumes.

**That experiment must not be shipped as-is.** This file's own comment says
masking interrupts is sufficient because the two contexts share a core -- true
for the ACT rings and RBuf (timer ISR + main loop, both core 1), false for
`AUX2CommInfoQ`, which `AUX_task` touches from core 0 (`xTaskCreatePinnedToCore`
..., 0). A blanket swap is a silent correctness regression on that queue. Any
real version has to be a per-instance policy, and the cost of it is the audit of
every `RingBuf` instance, not the twenty lines of macro.

Speed ceiling implied by the worst tick (it must fit `1e6/(2f)`):

| | worst tick | plate_freq wall | at production 10500 |
|---|---|---|---|
| now | 32 us | ~15600 | 67% of tick |
| per-instance lock policy | 20 us | ~25000 | 42% of tick |

The ISR is not currently the binding constraint -- the gate's rate limit
(`eff_sep_us` 28571, 35/s) and the camera are. Worth revisiting only if
something needs plate_freq above 15000.

#### The VOID bisect, kept as a warning

A `trig_report on/off` bisect was attempted and proved nothing -- do not repeat
it the same way. With `virt_pulse` running, the 4099 console is flooded with
`cam_trig` lines, and a probe that returns the first reply containing "health"
picks a queued older one. Eight samples came back byte-identical, including a
frozen `isr_ticks` while the plate was demonstrably turning. Any probe used
under a console flood needs to match a reply to its request (an id echo) before
its numbers mean anything.


## A task is an anchor plus a live offset (2026-08-11)

`ACT_INFO` stores `gate_pulse` (where the object was detected) and the `offset`
it was pushed with. `ACT_TRY_RUN_TASK` fires on
`(cur_pulse - gate_pulse) >= offset`. It used to store a baked
`targetPulse = gate_pulse + offset`, which froze an object's windows at the
moment it was admitted.

Why that mattered: a window is a tick count, ticks are distance, so a window is
an ARC and its duration is arc/speed. Frozen at one speed it tells the wrong
time at another, and the numbers are not small -- gate-to-chute transit is
1874 ms at plate_freq 8000 while an 8000 -> 12000 ramp is 2000 ms, so a part
admitted anywhere near a speed change spends most of its journey at a speed its
own windows never knew about. Re-deriving the windows in the main loop could not
reach it, because its arcs were already baked into absolute counts.

The two kinds of offset are not the same thing:

* **ON offsets are DISTANCES.** The part has to be under the nozzle. A fixed arc
  is already right at any speed, so these are used exactly as pushed.
* **OFF offsets are DURATIONS.** These are read live from `SPO_active` at fire
  time, so the pulse lasts the right number of microseconds for the speed the
  plate is at now.

### The one hazard, and how it is bounded

The act queues are FIFO and only the tail is ever examined. So a deadline that
moves LATER can sit in front of the next object's ON edge and delay it. Nothing
else about a live offset is dangerous -- this is the whole risk.

Note it is not really a speed question. It is the static question of whether a
pulse is wider than the gap to the next part, which `STAGE_WIDTH_SEL_WARN`
already checks at config time and which is currently TRUE on this machine: the
SEL blow is 50 ms against a `min_detect_sep_us` of 28571.

There is a speed component on top of it, though, and it only bites one way.
Part spacing on the plate is a DISTANCE, so the gap in ticks does not change
with speed; a pulse width in ticks does. **Accelerating therefore makes
width/gap worse and can create an inversion that did not exist at push time.**

`ACT_TRY_RUN_TASK` caps the live offset at the next queued task's own deadline:

```
cap = (next->gate_pulse - task->gate_pulse) + next->offset
```

expressed relative to THIS task's gate, so it is one unsigned subtraction and
stays wrap-correct. `RingBuf::getTail(1)` reads the next entry -- no lock, index
arithmetic. Inversion becomes impossible by construction and the pulse is right
in both directions.

**Consult the cap only when the deadline actually grew past what was pushed.**
A shrinking or unchanged deadline cannot overtake anything, and in steady state
live == pushed, so the lookup costs nothing. Unguarded, seven queues each paid
for a lookup that nearly always finds nothing and the worst tick went 32 -> 41
us, which is 98% of the tick at plate_freq 12000.

#### The superseded version, and why it was not enough

The first version used `min(pushed, live)` -- refuse to grow at all. That made
deceleration exact and left acceleration short by the speed ratio, on the
argument that short is the safe direction for the neighbour.

The residue was not small, and it has a closed form. A part covers S ticks from
the gate to a station, ticks accrue at 2f, and a linear ramp at accel `a` gives

```
f_at_station = sqrt(f0^2 + a*S)          S = SEL1_on = 30000 ticks
```

so the shortfall is `f0 / sqrt(f0^2 + a*S)`:

| f0 | accel | arrives at | pulse delivered |
|---|---|---|---|
| 8000 | 2000 | 11136 | 71.8% |
| 10500 | 2000 | 13048 | **80.5%** |
| 10500 | 1000 | 11843 | 88.7% |
| 10500 | 500 | 11191 | 93.8% |

At the production 10500 with the configured accel 2000, a 50 ms blow came out
40 ms. Note `speed_band_pct` does not appear anywhere in that expression -- the
full rule is `f0 / min(f0+delta, sqrt(f0^2+a*S))`, and the band only ever bounded
the left term. Capping instead of refusing removes the speed term from the pulse
duration altogether.

(Sanity check on S: 30000 ticks at 8000 is 30000/16000 = 1.875 s, which is the
1874 ms drain measured independently. The numbers are real.)

### Measured

In-band, 8000 -> 8700 -> 8000, parts flowing, under the superseded `min()` rule:
SEL1 tracked 798 -> 865 -> 870 -> 813 -> 800 ticks with the duration holding at
49.0-50.4 ms.

With the cap, a **+50% acceleration** and no drain (`speed_band_pct` raised to
50 for the test, restored after), which is the case `min()` could not do:

```
window        54t -> 58 -> 78 -> 80t
DELIVERED     3315-3398 us against 3333 asked, both directions
              (min(pushed,live) would have given 2222 us here)
ISR max       27 us steady, 30 us at 12000 (72% of tick), overruns 0
admission     never paused -- accept 214 -> 834 continuous
```

### The instrument that makes this checkable

`health.cam1_pw_{min,max,last}_us` is CAM1's pulse as DELIVERED, timed in the
ISR between its own two edges. Everything else in this firmware reports INTENT:
`stage_pulse_offset` gives a tick count, and a tick count is only a duration if
you also know the speed. It is on CAM1 rather than SEL1 because CAM1 fires for
every part and is therefore observable on a bench with no verdicts; the physics
is identical.

### Editing station geometry mid-run may mis-actuate, and that is accepted

An object's ON edges are the ones current when it was admitted; its SEL offsets
are read later, in the SWITCH branch. Edit `stage_pulse_offset` while parts are
on the plate and the ones past SWITCH keep the old geometry while the ones
behind them get the new -- two parts, two behaviours, one of them wrong. Unlike
a speed change this is unbounded: a position can be dragged from 800 to 3000 in
one gesture.

Decided 2026-08-11: **acceptable.** Editing station positions IS the deliberate
setup, the machine is not producing while somebody dials a position in by eye,
and a few mis-sorted parts there cost nothing worth engineering against.

The fix was designed and priced before being declined, so that it does not get
re-derived: defer the whole `set_setup` document and replay it after the
pipeline drains, reusing `PLATE_FREQ_PENDING`'s machinery. Rejected because it
makes `set_setup` ack a change it has not applied, which the WebUI reads back as
"the setting did not take" -- worse, for this case, than the thing it fixes.

This does NOT extend to speed. A speed change happens during production, so a
large one drains first and a small one is bounded by `speed_band_pct`.


## Changing plate speed without stopping inspection (2026-08-11)

Before this, ANY speed change shut the gate for the whole ramp -- the admission
test was `SYS_FREQ_STABLE`, which is literally `CURRENT == TARGET` -- and every
part on the plate during it was lost. Tolerable with a manual knob, fatal to the
closed-loop speed control that is coming, which would spend its life ramping.

Three independent layers. They are worth keeping straight, because two of them
are about CORRECTNESS and the third is only POLICY.

### Layer 1 -- the windows follow the speed (correctness)

`STAGE_PULSE_WIDTH_apply()` is called from the ramp service in `firmwareLoop`,
guarded by a 0.4% relative threshold (below that the derived tick counts do not
change, so it would burn seven divides writing back what is already there) and
by `f >= PLATE_FREQ_TARGET*0.25` (a plate ramping to a stop walks through
arbitrarily small speeds, and deriving there leaves every window at `us2t`'s
one-tick floor -- 50 ms of blow stored as 1 tick, observed on a real stop).

**Main loop only.** The ISR reads one pointer, `SPO_active`, a double buffer
committed with a single atomic store.

### Layer 2 -- the windows reach parts already in flight (correctness)

Layer 1 alone does nothing for a part that is already moving, because its arcs
used to be baked into an absolute `targetPulse` at admission. See "A task is an
anchor plus a live offset" above -- that is what makes layer 1 reach them.

### Layer 3 -- who is admitted, and when (POLICY)

This is the layer that is now a choice rather than a requirement.

* **Inside `speed_band_pct` (default 10%)**: applied immediately, admission never
  pauses. Measured 12000 -> 12800: `accept` went 832 -> 1081 without a gap.
* **Outside it**: staged as a TRANSACTION. The gate closes, the plate holds its
  OLD speed and its OLD windows until the pipeline is empty, and only then do
  setpoint, windows and target move together. Nothing is ever in flight across
  the change.

```
8000 -> 12000    accept froze at 162, in_flight 30 -> 0, drain 1874 ms, then ramp
4000 -> 6000     drain 3689 ms                       (drain scales as 1/plate_freq)
```

`plate.freq_pending` reports a staged change; `count.FREQ_TXN`,
`FREQ_TXN_TIMEOUT`, `FREQ_TXN_DRAIN_MAX_MS` report the outcome.

**A change arriving during a drain RETARGETS it.** Without that, a small change
during a drain writes the setpoint directly (it is small, so nothing stages it)
and the drain then commits the old pending value on top of it -- the operator's
most recent instruction silently overwritten by one they had already superseded.
While a transaction is open the setpoint is not the operator's to write; the
pending value is. Measured: stage 8000 -> 12000, send 8200 mid-drain, machine
settles at 8209.

**The drain timeout is 10 s and must not be tightened.** It is a safety net
against a pipeline that never empties, not a schedule. A real drain is the last
part's transit time and scales as 1/plate_freq: 1874 ms at 8000. The first value
written was 3 s, which passes at 8000 and would trip below about 5000 --
committing with parts in flight, which is the exact failure it exists to
prevent, while reporting a clean transaction.

### An actuation that was asked for and did not happen is counted

`count.SEL_SUPPRESSED`. A verdict scheduled a blow and no air came out, because
the plate was out of band. Silence here was the entire problem: during an
8000 -> 12000 change there are 26-39 parts in flight for the whole 2.2 s ramp,
they are judged and NOT actuated, and any NG among them leaves in the OK stream.
Nothing counted it before. (The hole predates the band -- the gate required
`SYS_FREQ_STABLE`, false through any ramp too.)

Non-zero in production means parts were judged and not sorted. It is not a
performance counter.

### What is correctness and what is policy, now

With layers 1 and 2 in place the pulse duration no longer depends on plate
speed in either direction. So:

* `speed_band_pct` and the drain transaction are a decision about **whether to
  keep admitting parts during a large speed change**, not a correctness
  requirement. Widening the band or dropping the drain does not break the blow.
* What remains genuinely constrained is FIFO ordering -- pulse width versus part
  gap -- and that is bounded in the ISR by the next-task cap.

Decided 2026-08-11 to KEEP the two-tier policy: small changes apply directly,
large ones go silent and drain. Not for correctness, but because a large change
is rare and operator-driven and 2-4 s of not admitting costs nothing.

### Where the speed is actually computed

All in `src/app/LegacyFirmware.cpp`, none of it in the ISR:

| what | where |
|---|---|
| setpoint written, transaction staged | `setMachineSetup()`, at the `_freq_before` capture |
| windows converted us -> ticks | `STAGE_PULSE_WIDTH_apply()`; called from `setMachineSetup` and from the ramp service |
| setpoint -> ramp target | the state machine, `PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT` in CAL / SPINUP / READY loop passes |
| the ramp itself | `firmwareLoop()`, `step = SYS_FREQ_ACCEL*dt`, `dt` clamped to 0.25 s |
| **the only line that changes the actual speed** | `timerAlarmWrite(timer, (_TICK2SEC_BASE_>>1)/PLATE_FREQ_CURRENT, true)` |
| integers published for the ISR | `ISR_BUDGET_CY`, `PLATE_IN_BAND`, `GATE_MIN_DIST_STEPS`, same block |
| transaction commit | `freqTxnService()`, called from `firmwareLoop` after `spinupService()` |
| measured speed (reporting only) | `PLATE_FREQ_MEAS`, from a `SYS_STEP_COUNT` delta over >=100 ms |

Four variables, and mixing them up is the usual source of confusion:
`SETPOINT` (config, persisted) -> `TARGET` (ramp destination) -> `CURRENT`
(ramped actual, the only one written to hardware), plus `MEAS` (observed).

### What to watch if this misbehaves

| symptom | look at |
|---|---|
| parts judged but not sorted | `count.SEL_SUPPRESSED` |
| a speed change that never applied | `plate.freq_pending`, `count.FREQ_TXN_TIMEOUT` |
| blow the wrong length | `health.cam1_pw_{min,max,last}_us` -- delivered, not intended |
| the board goes silent on a speed change | `health.isr_overrun_n` and `health.isr_worst_seg_cy` FIRST. Two independent causes have already done this and both are fixed; those two fields are what say whether one came back. Do NOT respond by re-tightening the gate. |

## A refused `set_setup` is loud, and a tool that does not look will lie (2026-08-11)

`set_setup` refuses a document containing ANY key outside the schema -- it does
not apply the half it understood. That is deliberate (applying half and acking
true is how eight tools spent a week configuring nothing after the regroup), and
it is not quiet about it:

```
{"dbg":"SET_SETUP REFUSED: 1 unknown key(s): speed_band_pct"}
{"type":"set_setup","err":"unknown_keys","unknown":"speed_band_pct",
 "n_unknown":1,"ack":false}
```

A teardown script sent `{"plate":{"freq":0}}` with a stray TOP-LEVEL
`speed_band_pct` beside it -- the key exists, but it lives inside `plate` -- and
never looked at the reply. It printed "stopped" and the plate ran at 12000 for
several minutes.

The device was right and the tool was wrong. `tools/peek.py` now has `cmd()`,
which raises on `ack:false` or on no reply, and `stop_plate()`, which stops the
plate and then READS BACK `plate_freq_meas` and raises if it is still turning. A
teardown that cannot confirm the plate stopped must not report success.

Worth generalising: on this link an `ack` is cheap to read and every script here
had been throwing it away.


## What a 31-minute speed soak found, and what it could not (2026-08-11)

`tools/speed_soak.py`, 1846 s, 81 speed changes (108 including retargets), 53
transactions, 30787 measured pulses, 30.7M ISR ticks, plate random-walking
3337..14003. Four independent analyses over the same log.

### Clean, with the numbers that make it a real check

* **Zero ISR overruns**, and the counter is arithmetically consistent, not
  broken: the record tick was 7772 cycles against a budget of 8980 at the speed
  it occurred, and 8569 at the fastest speed reached.
* **Zero device-side stalls.** Steady-state inter-tick gap high-water grew only
  124 -> 232 us over the whole run, i.e. the worst stall in 31 minutes was one
  deferred alarm. **The 12696 us `isr_gap_max` is a teardown artifact** -- it
  appears only in the post-stop sample, and at `PLATE_FREQ_CURRENT` walking down
  to the 10 Hz cutoff a "gap" of 12.7 ms is definitional. Read `isr_gap_max`
  from the last IN-RUN sample or it will report a fake 12.7 ms stall on every
  stop.
* **No heap movement at all.** `free_heap` was bit-identical in 534 of 535
  samples. This is `free_heap`, not `min_heap` -- the instantaneous value never
  moved, so no extrapolation is needed and none is offered.
* **Speed changes leave no signature in ISR duration.** Rows within 4 s of a
  change: mean 19.7 us, max 28. Rows more than 10 s away: mean 21.6, max 26. 81
  changes with parts admitted throughout is the exact scenario that hung the
  board twice; it did not recur.
* **Every admitted part got a verdict.** `accept - verdicts - pipe_registered`
  was 0 or 1 in every sample of the running phase, for 31 minutes.
* **Transactions reconcile exactly**: 41 primary out-of-band changes + 12
  retarget commands that arrived after the prior transaction had already
  committed = 53 = `FREQ_TXN`. Zero timeouts, worst drain 4474 ms against a
  10 s limit.

### Found and fixed

* `stageWidthRefFreq()` returned the SETPOINT -- see the commit and the
  `stageWidthRefFreq` comment. Every delivered-width extreme in the run was this.

### Found, NOT fixed -- these are real and open

**1. `GATE_EDGES` counts only real sensor edges, but injected pulses land in the
same outcome counters.** `phantomServiceISR` and `virt_pulse` call
`newPulseEvent` directly and never touch `GATE_EDGES`, so with the injector
armed `edges != accept + sum(rejections)`: 32639 versus 61121 here, and
`d_accept > d_edges` in 278 of 534 intervals. The firmware publishes
`yield_.gate.pct = 100*accept/edges` and `overall_pct = 100*acted/edges`, both
of which are then unbounded above 100% -- this run peaked at 96.3% by luck. The
comment calling edges "the honest denominator" is false whenever the injector is
on. **Production is unaffected (no injector), but every soak number derived from
`edges` is.**

**2. The injected path bypasses the band test.** `PLATE_IN_BAND` is checked in
`GateSensing`, not in `newPulseEvent`, so injected pulses are admitted while the
plate is out of band -- the one condition the design says must not admit.
`blockNewDetectedObject` IS inside `newPulseEvent`, so the drain still blocks
both paths and the transaction results stand. But any claim of the form
"admission paused because the plate left the band", measured with `virt_pulse`,
is only evidence about the sensor path.

**3. `rej_width` has a 5x speed dependence** -- 3.27% of edges at 2000-4000 Hz
falling monotonically to 0.61% at 12000-14000, and it is not a ramp artifact
(1.18% steady versus 0.87% while ramping). Pulse width is measured in step
ticks and a fixed part subtends a fixed number of steps, so first principles
predict NO speed dependence. Something real-time is leaking into a step-domain
measurement; the sensor's fixed electrical edge rate smearing across more ticks
when each tick is shorter is the obvious suspect. Small (378 parts in 31 min)
but systematic, and it grows as production speed rises.

**4. Roughly one pipeline-depth of parts is discarded unattributed at every
stop** (22 here): admitted, never given a verdict, counted in no rejection
bucket.

### What this run could NOT test -- do not read these zeros as passes

* **`SEL_SUPPRESSED` is vacuous here.** All 30951 verdicts were NA; SEL1/2/3
  never fired, so the branch that increments it was never entered.
* **The `act_cap` guard is untested where it matters.** `act_cap_n` is 0 across
  195733 grow events, but CAM1's window is 0.12 of the part pitch -- it cannot
  bind at any speed in range. SEL1's is up to **1.80**, which is exactly the
  inversion the cap exists to prevent, and the SEL queues were never populated.
  **Do not conclude from this run that the cap is dead code.**
* **The retarget branch got one sample.** The harness aimed to land a second
  change 0.4-1.5 s into a drain; measured delays were 2.92-3.93 s, because
  `peek.cmd` blocked its full window on every command. Mean drain is ~1.13 s
  (2088 blocked / 53 transactions / 35 per s), so 26 of 27 "retargets" hit a
  closed transaction. `cmd` now returns on the id match in ~7 ms, so a rerun
  will actually reach the branch.
* **`FREQ_TXN_TIMEOUT` never ran** -- worst drain was 4474 ms against 10000.
* **The slowest condition was never reached**: the walk bottomed at 3337, not
  the intended 3000, so the longest drain is extrapolated.
* **Nothing camera.** `sync_disagree`, `sync_rejected`, `sync_rebuilds`,
  `sync_cal_fails` are all zero because nothing ever challenged the clock model.
* **`act_late_max` was null in all 535 samples** -- the firmware emits it from
  the `poll` handler, and the harness only sent status commands. The whole
  `LATE_*` diagnostic bundle went unread.

### The margin that is actually tight

Worst tick 32 us against a 35.7 us budget at 14000 -- **3.3 us of headroom, and
it was reached, not extrapolated.** ISR duration is driven by part rate, not
plate speed: work per tick is flat at ~21 us envelope across 3000-14000 while
only the budget shrinks (env/tick correlates with speed at r=0.94, env itself at
r=0.03). An empty pipeline halves it; beyond ~20 objects in flight there is no
further growth. Anything added to the ISR path spends that 3.3 us directly.


## The band is gone (2026-08-11)

`SPEED_BAND_PCT` no longer gates anything. It defaults to 0, and 0 means a speed
change never stages -- it just applies. The ISR-side bool it used to publish is
now `PLATE_RUNNING`, which asks only "is the plate turning", the one thing the
band was doing that was always real work.

### Why it was safe to remove

The band existed to bound an error that no longer exists. A station window is a
tick count -- an arc -- and its duration is arc/speed, so a window derived at one
speed was wrong at another and the band bounded how wrong. Since the anchor +
live-offset rework the delivered pulse is right at any speed:

* +50% acceleration with parts flowing: delivered 3315-3398 us against 3333
  asked, both directions
* accel swept 2000 / 10000 / 50000 / 100000 Hz/s at a fixed speed: error max
  64, 64, 64, 65 us -- the one-tick quantisation floor, unmoved. Acceleration is
  free as far as the firmware is concerned.

### What it was costing

A 31-minute soak spent 52.6 s (2.8%) out of band, refusing parts, and SEL never
fired during any ramp at all -- so an NG judged mid-ramp left in the OK stream.
`SEL_SUPPRESSED` was added to count exactly that.

### Measured after removal, +75% and back

```
8000 -> 14000    pending 0, FREQ_TXN 0, rej_unstable 0, rej_blocked frozen
                 accept 242 -> 570 continuous, no pause
                 delivered 3347-3382 us, err max 71 us, overruns 0
14000 -> 8000    same
```

The old behaviour on that change was a 1.9 s drain, ~30 parts refused, and the
whole in-flight population judged but not sorted.

### What is left, and the one guard that is NOT the band

`PLATE_RUNNING` is `PLATE_FREQ_CURRENT > 0`, published from the ramp service
because the step ISR must not touch the FPU. Admission and actuation still
require it. Spin-up from a standstill is handled by the state machine
(`blockNewDetectedObject` is true until READY), not by this.

**Known gap, deliberately left:** during a ramp DOWN to a stop the window
re-derivation freezes (`PLATE_FREQ_TARGET > 0 && f >= TARGET*0.25`, which exists
because deriving at arbitrarily small speeds collapses every window onto `us2t`'s
one-tick floor). So while the plate coasts to a halt the windows are stale at the
last derived speed and a blow gets longer in time as the plate slows. It is
bounded by the stop path -- `exit_insp_mode` sets `blockNewDetectedObject` and
`ALL_OUTPUTS_SAFE()` runs -- but it is the one place where a window still lies
about its duration.

### The transaction is kept, not deleted

Set `speed_band_pct` to a percentage and the drain-before-ramp machinery
(`PLATE_FREQ_PENDING`, `freqTxnService`) wakes up unchanged. Draining before a
large change is still the right thing if a reason to want it appears; there just
is not one today.

### Why this matters for closed-loop speed

The point of removing it is the density-following speed control. The binding
timescale there is not the ramp, it is the DEAD TIME: a part travels 30000 ticks
from the gate to SEL1, which is 1.43 s at the production 10500, and a speed
change cannot affect anything already on the plate. So the ramp only has to be
fast relative to that:

```
ramp time < 1/4 of dead time   =>   accel > delta_f * f / 3750

10% correction at 10500  ->  accel > 2940     (the old default was 2000)
30% correction at 10500  ->  accel > 8820
```

The firmware charges nothing for accel up to its 100000 clamp, so the remaining
limit is **mechanical and unmeasured**: parts ride the plate on friction, and
past some acceleration they slide -- which breaks "one tick is a fixed distance",
the assumption the whole scheme rests on. That number is not in any log here. To
find it, ramp hard at increasing accel with REAL parts and watch the gate edge
spacing and `rej_dist`; sliding shows up as the spacing distribution changing.
`virt_pulse` cannot see it.


## Holding an object rate: feedforward beats the loop, on this plant (2026-08-12)

First run of speed control against a target rate, with REAL parts, target
15 obj/s. Result: **do the division, do not close a loop.**

### The plant

The parts on this plate are a FIXED set riding a rotating disc, so

```
rate = N_parts * revs_per_second,   revs_per_second = plate_freq / 30000
```

`N` is a constant and the correct speed is a division, not a control problem.
Measured open-loop at 7800 Hz: 15.215 obj/s, so **N = 58.52 parts per
revolution**, and 15.0/s wants `15 * 30000 / 58.52 = 7690`.

### Measured, same parts, same session

| approach | rate error | per-window sd |
|---|---|---|
| **feedforward, 7690 Hz, no loop** | **+0.29%** | **1.56%** |
| P + I loop | -1.3% | ~3% |
| P only, 8% deadband | +5.3% | ~3% |

The feedforward cumulative mean walked to 15.00 within 3 minutes and held.
**Both closed loops made the rate about three times noisier than leaving it
alone**, because they were correcting a plant with nothing to correct: every
speed change is a disturbance, and the measurement noise gets fed back as one.

### The noise model was wrong by 7x, and it cost a tuning cycle

The first loop limit-cycled and it was blamed on Poisson counting noise -- 60
parts in a 4 s window, sigma 1/sqrt(60) = 12.9%, against a 3% deadband. The
deadband was widened to 8%. That stopped the oscillation and parked the loop
5.3% high; a deadband cannot correct what it is busy ignoring.

**Arrivals here are not Poisson.** A fixed set of parts at fixed angular spacing
is quasi-periodic. Measured open-loop at a fixed 7800 Hz for 6.2 minutes, 12 s
windows: **sd 1.06% against the 7.40% Poisson prediction.** The entire
justification for that deadband was a distribution that does not apply here.

Keep the Poisson reasoning anyway: if the feeder ever runs continuously,
arrivals become much closer to random and it applies again. The two regimes want
different tuning, and one open-loop run tells you which you are in.

### How to tell a drifting plant from an oscillating controller

The P+I run held the rate at 15 while the SPEED wandered 7158 -> 8182 -> 7365
with a ~340 s period. That fits "the density is drifting and the loop is
tracking it" exactly as well as "the integral is oscillating". **Open the loop.**
At a fixed speed `rate = density * speed`, so rate drift IS density drift. Six
minutes at a fixed 7800 gave a cumulative mean flat at 15.22 from t=100 s
onward, so the density was constant and the wander was the controller.

### What should actually be built

Feedforward on `N` with a slow trim, not a fast loop:

* estimate `N` from one open-loop window at any speed, `N = rate * 30000 / freq`
* set `plate_freq = target_rate * 30000 / N`
* trim only for a CHANGE in N -- parts ejected to a chute, added, or lost. That
  is slow and monotone, so the trim wants a long time constant and a deadband
  above the measured 1.06%.

The delivered CAM1 pulse held 3378-3382 us against 3333 asked for the whole
feedforward run, with zero ISR overruns across every run above.

### What still bounds a correction when one is needed

A correction must COMPLETE before the part reaches the camera, which is 9315
ticks from the gate -- not SEL1's 30000, where an earlier version of this note
wrongly put it. Solving "ramp finished before arrival":

```
delta_f_max = sqrt(f^2 + 9315 * accel) - f
```

At the production 10500, accel 2000 buys only 853 Hz (8.1%); 10% needs 2485,
30% needs 10000, 64% needs 20000. The firmware charges nothing for accel up to
its 100000 clamp -- a sweep of 2000/10000/50000/100000 moved the delivered-pulse
error not at all. The remaining limit is mechanical and still unmeasured: the
acceleration at which parts SLIDE, which would break "one tick is a fixed
distance". With a fixed part set that is now easy to test, because N is a
constant: a step in `rate * 30000 / freq` after a hard ramp IS parts moving.


## Auto plate-speed: kept, and the two traps that make it look broken (2026-08-12)

The feature is `tools/rate_hold.py`. It is an ESTIMATOR AND A DIVISION, not a
loop, and it is worth reading the reasons before changing it, because every
wrong version tried first looked plausible.

### It only applies when the plate speed sets the arrival rate

A fixed or recirculating set of parts carried by the plate gives

```
rate = N * plate_freq / 30000        N = parts per revolution, constant
```

With an independent feeder -- a vibratory bowl -- it does not. At steady state
the gate sees `feed_rate / removal_fraction` regardless of plate speed; the
plate only changes how far apart the parts sit. Measuring `rate ∝ speed` on a
fixed part set and carrying that over to a fed machine is the trap, and it was
walked into during development: hours of controller tuning against a plant that
does not exist in that configuration.

**Decided 2026-08-12: the feature stays** -- there is hardware that needs it --
but the applicability test above is part of it, not a footnote.

### Trap 1: control `gate.edges`, never `gate.accept`

`accept` is what survives the fire-rate limiter, the minimum-distance gate and
the width filter. The limiter is a fixed TIME while part spacing is a fixed
DISTANCE, so a faster plate puts more pairs inside the window and rejects more.
Targeting `accept` therefore closes a POSITIVE feedback around the limiter: too
few accepted -> spin faster -> reject more -> spin faster still.

Measured: a loop chasing 20 accepted/s drove the plate to 14400 Hz where the
linear model said 10253, with `rej_rate` climbing at 5.5/s, and the accept rate
appeared to saturate. On `edges` the plant is exactly linear again and the
rejections are visible as the loss they are.

### Trap 2: count per REVOLUTION, not per second

The parts are not evenly spaced around the disc, so a window that is not a whole
number of revolutions ALIASES the angular distribution -- which parts you count
depends on the phase. Same speed, same material:

| window | revolutions | apparent spread |
|---|---|---|
| 5 s at 9400 Hz | 1.57 | **+-12%** |
| 12 s at 7800 Hz | 3.12 | 1.06% |

That is not counting noise, and treating it as counting noise is what sent three
tuning attempts wrong. Worse, the first diagnosis was Poisson counting noise --
predicted 7.4% for a 12 s window against the 1.06% actually measured -- and the
8% deadband chosen to cover that imaginary noise then parked the loop 5.3% off
target. Arrivals from a fixed part set at fixed angular spacing are
quasi-periodic, not random.

So count against the step counter, not the clock:

```
N = delta_edges * 60000 / delta_ticks       60000 steps per revolution
plate_freq = target_rate * 30000 / N
```

`N` is speed-independent by construction and genuinely constant, so it can be
smoothed hard for nothing.

### Why there is no loop

Three were tried on real parts in one session on the same material:

| approach | rate error | per-window sd |
|---|---|---|
| **feedforward on N, no loop** | **+0.29%** | **1.56%** |
| P + I | -1.3% | ~3% |
| P only, 8% deadband | +5.3% | ~3% |

Both loops made the rate about three times noisier than leaving it alone, because
they were correcting a plant with nothing to correct: every speed change is a
disturbance and the measurement noise comes straight back as one. A loop earns
its keep when N CHANGES -- parts ejected, added, lost -- and that is slow, so the
answer is a slow estimator rather than a fast controller.

### How to tell a drifting plant from an oscillating controller

The P+I run held the rate at target while the SPEED wandered 7158 -> 8182 ->
7365 with a ~340 s period, which fits "the density is drifting and the loop is
tracking it" exactly as well as "the integral is oscillating". **Open the loop.**
At fixed speed `rate = density * speed`, so rate drift IS density drift. Six
minutes at a fixed 7800 held a cumulative mean flat at 15.22, so the density was
constant and the wander was the controller.

### The bound a correction still has to respect

It must COMPLETE before the part reaches the camera, 9315 ticks from the gate:

```
delta_f_max = sqrt(f^2 + 9315 * accel) - f
```

At the production 10500 that is 853 Hz (8.1%) at accel 2000, and ~36% at accel
10000. The firmware charges nothing for accel up to its 100000 clamp -- a sweep
of 2000/10000/50000/100000 moved the delivered-pulse error not at all. The
remaining limit is mechanical and still unmeasured: the acceleration at which
parts SLIDE, which breaks "one tick is a fixed distance". With a fixed part set
that is now easy to test, because N is constant -- a step in
`edges * 60000 / ticks` after a hard ramp IS parts moving.

## An 8-hour soak on real parts, with the speed never still (2026-08-12)

Everything before this was measured one condition at a time, and the longest
run on the new firmware was 31 minutes on virtual parts. This is the endurance
check for the three changes made on 08-11 -- the band removed, a task as an
anchor plus a live offset, and the station windows re-derived continuously from
`PLATE_FREQ_CURRENT`. Real material, no `virt_pulse`, plate speed walked at
random for eight hours.

```
01:56:44 -> 09:56:29   8689 samples
1992 speed changes + 226 accel changes   (498 of them double-tapped <1.2 s apart)
speed 2998 .. 10003 Hz    accel 1000 / 2000 / 5000 / 10000
364465 gate edges         412.8 M step ticks
```

### What it proved

| | |
|---|---|
| `isr_overrun_n` | **0** over 412.8 M ticks (max 38 us, env 9, avg 2) |
| `pending > 0` | **0** of 8689 samples -- no change ever staged or drained |
| `FREQ_TXN` / `_TIMEOUT` | 0 / 0 |
| `band_out_ms`, `SEL_SUPPRESSED` | 0, 0 |
| `UNANSWERED` / `SKIP` | 0 / 0 |
| `rx_crc_fail` | 0 of 426840 frames |
| `free_heap` / `min_heap` / `stack_hwm` / `rbuf_peak` | 190688 / 184120 / 4020 / 45 -- **identical in all four quarters** |
| counter regressions | none |

The heap figures being bit-identical across four two-hour quarters is the
strongest single line here: there is no leak and no fragmentation drift, and
`min_heap` never moved below its first-quarter value.

### Conservation holds exactly, once you notice the offset is inherited

`accept + sum(rej) - edges` sat at a constant **716** for the entire run,
oscillating 715<->716 only because `edges` and `accept` are sampled at slightly
different instants while the reply document is built. Constant means it was
accrued BEFORE the soak: the soak does not call `reset_running_stat`, and the
716 is left over from the earlier `virt_pulse` work, whose injector calls
`newPulseEvent` without touching `GATE_EDGES`. Over the soak's own 364465
edges the identity is **exact**.

Read a non-zero residual as "an injected-path pulse happened at some point
since the last reset", not as an ongoing accounting leak. To check
conservation, reset the counters first.

### `act_cap` still has never fired

`act_grow_n` 2324809, `act_cap_n` **0**. The deadline grew on essentially every
task -- expected, since the live offset is read at fire time -- and it never
once grew past the next queued task's deadline. The cap is correct-by-
construction and remains **unexercised**; it needs a run with real SEL verdicts
and a tight SEL1 window (win/pitch 1.80) before it can be called tested.

### `rej_width` is a function of plate speed, and that is still unfixed

| plate freq | edges | rej_width |
|---|---|---|
| 3000-3999 | 36449 | **5.10%** |
| 5000-5999 | 61209 | 3.53% |
| 7000-7999 | 53247 | 2.26% |
| 9000-9999 | 45635 | **1.83%** |

A monotone 2.8x across the range. The width test is what decides whether a gate
pulse is a part, so a criterion that rejects 5% at 3000 and 1.8% at 9000 is not
measuring the part. Slower plate -> longer shadow -> more pulses land outside a
window that is not being scaled the same way the station windows now are. This
is the obvious next fix and it was NOT touched by the 08-11 work.

### Latency, and which number is the design number

```
gate   -> verdict   avg 767 ms   max 1750 ms
camera -> verdict   avg 19.8 ms  max  300 ms      (394040 reports)
```

`avg_us` / `max_us` are dominated by TRANSIT -- the part must travel 9315 ticks
to reach the camera, which is 1.55 s at 3000 and 0.47 s at 10000. They measure
the plate, not the system, and they are easy to misread as a system latency.

**`cam_*` is the design number.** Its budget is CAM1 -> SWITCH,
`29900 - 9315 = 20585` ticks, which at the 10000 ceiling is 1029 ms. A 300 ms
worst case is 29% of that. And it need not be argued from the margin:
`UNANSWERED` and `SKIP` were both 0 across 394040 reports, so no verdict was
ever late to the switch.

Note `cam_max_us` grew 211 -> 300 ms over the last two hours of the run. It is a
since-boot high-water on the HOST path, well inside budget, but it is drifting
and worth re-checking rather than assuming 300 is the ceiling.

### Two polls in 8689 went unanswered

`sample_partial` twice (08:08:06, 09:55:20): neither document came back inside
the 1.2 s poll window. 0.023%, no correlation with a speed change, and the very
next poll was normal. Recorded because a poller with a tighter timeout and no
retry would read this as a dead board.

### The largest thing this soak did NOT cover

Every verdict in eight hours was `NA` — `SEL1`/`SEL2`/`SEL3` all zero. The
inspection half ran for eight hours; the **sorting half never moved once**, and
`act_cap`, `SEL_SUPPRESSED` and `FREQ_TXN` are at zero coverage as a direct
consequence. Do not read "8 hours clean" as covering the machine. What is left,
and in what order, is in `DEV_COMPLETE_CHECKLIST.md`.

---

## The width test rejects at the LOW edge, and the drift is a fixed TIME

2026-08-12, three speeds, 60 s each, real parts, through the core.

```
 freq   edges  rej_width      lo   hi   w_mean   w_min  w_max
 3000     409   19  4.65%     19    0   262.67     44    611
 6000     789   34  4.31%     34    0   275.12     67    638
 9000    1163   32  2.75%     32    0   283.81     75    642
```

**`rej_width_hi` is 0 at every speed.** Nothing is ever rejected for being too
wide, so half of the "which edge" question is closed and stays closed.

`w_mean` fits `w = W_geom + t0*f` with residuals of ±1.25 ticks over the three
points:

```
t0      = 3.52 ms      a fixed sensor response, in TIME
W_geom  = 252.7 ticks  = 3.177 mm
```

The intercept is the part of this worth trusting. 3.18 mm is a real part's
shadow -- parts are specified 3 mm apart -- so the model lands on a physical
number rather than an arbitrary one. A tick-domain measurement of a
fixed-distance shadow cannot depend on speed; a fixed response time in µs
becomes more ticks the faster the plate turns, and that is what is seen.

**Do not implement the correction from this alone.** `t0*f` is 10.6 ticks at
3000 against 35.2 at 10000 -- a ~25 tick swing on a 120 tick threshold. Whether
that accounts for a 2.8x change in rejection rate depends on how dense the low
tail is right at 120, which was NOT measured. `w_min` of 44-75 says there is a
population far below the threshold that is probably not parts at all (debris, a
noise edge), and lowering the threshold at low speed admits those too. The
rejection percentages here are also weakly powered -- 19/34/32 events, Poisson
±1.07 / ±0.74 / ±0.49 -- and only reproduce the soak's DIRECTION, not its 2.8x.

What is missing before the fix: a width histogram near the threshold.

### Serial-direct cannot run inspection mode, by design

The first attempt drove the board over the UART with no core attached and
halted immediately: `CAM_CLOCK_CAL_FAILED` (err 14). CAL needs the host to
report frame timestamps, so a headless board can never converge and refusing to
start is correct.

`INSPECTION_MODE_TEST` (state 140) is exactly the camera-free rig that would
have worked -- it opens the gate and turns the plate with no CAL -- but **no
command can reach it**. Nothing in the firmware emits
`ENTER_INSPECTION_TEST_MODE`; the state and its transition are dead code.
Making it reachable is the cheapest first step B6 has.

### get_running_stat was ~200 bytes from its ceiling

Adding four keys overflowed it (`stat_doc_overflow`). The old response
serialised to 2864 bytes against a `StaticJsonDocument<3072>`. Raised to 3584.

The real ceiling is the HOST's: the core reads the peripheral line with
`if (line.size() < 4096) line += c` (`wiringPanel.cpp:6703`), so a reply past
4096 bytes is silently truncated upstream, where no device-side guard can see
it. Anything wanting more room has to raise the host's limit first.

### The injected path's PLATE_RUNNING guard is unreachable

Verified 2026-08-12 while checking A3/A4. `PLATE_RUNNING` is
`PLATE_FREQ_CURRENT > 0`, and the step timer's alarm is disabled at zero. The
injector runs inside that ISR, so the one state where the guard is false is the
one state where the injector never executes. "Injection is not gated on
PLATE_RUNNING" was true in the code and could not happen on the machine.

The `GATE_EDGES` half of the same fix is NOT cosmetic and was measured: dry run
plus `virt_pulse` in IDLE, 671 injected edges, `accept + Sigma rej == edges`
with a residual of exactly 0. Before the fix that residual would have been -671.

Dry run is unaffected by the guard, which is the thing worth knowing before
touching this again: a dry run has `PLATE_FREQ_CURRENT > 0` with `StepGo` muted,
so the plate stands still while `PLATE_RUNNING` reads true. Every phantom rig
that relies on injection keeps working.

---

## The all-NA soak was a headless core, not a blind inspection

2026-08-12. The 8-hour soak's 393537 `NA` with SEL1/SEL2/SEL3 all zero was read
as "the sorting half never moved". True, but the cause was upstream of the
machine entirely, and the core says so itself (`wiringPanel.cpp`, the perif
console): **a headless core loads no def, so it answers NA to every part.**

Nothing was wrong with the gate, the pairing, or the selectors. The run simply
had no recipe. `act_cap`, `SEL_SUPPRESSED` and `FREQ_TXN` sitting at zero
coverage is a direct consequence, not an independent finding.

With a def loaded from the WebUI the machine produces real verdicts within
seconds: SEL3 234 / NA 121 on a first look, and NG -> SEL1 on a deliberately
out-of-tolerance part (D2.438mm).

### `!ld` and `!st` do not exist in the shipped binary

The perif console's `!` injection was generalised in source to any two-letter
BPG type, so a rig can send `!ld` (load def) or `!st` (settings) without a
browser. **The binary in `build/mac-arm64/` at the time of writing (Aug 7) only
has the hardcoded `!pd`.**

The failure mode is the dangerous kind. An unrecognised `!xx` line does not
error -- it falls through to the else branch and is **forwarded verbatim to the
ESP32** as if it were a device command. Check the binary before relying on any
injection other than `!pd`:

```
strings build/mac-arm64/visSele | grep injected     # "pd injected" == old
```

That binary was ~20 InspectionCore commits behind source. A stale core is worth
ruling out first whenever behaviour disagrees with the code being read.

### NG snapshots are switched OFF at the start of every session

`data/SAMPLE/` has been empty for a week, including across the 8-hour soak, even
though `machine_setting.json` sets `FI_INSP_NG_SNAP: true`.

Every CI/FI session start does `saveInspFailSnap = false; saveInspNASnap =
false` (`wiringPanel.cpp:3784`) unconditionally, and the only thing that turns
them back on is the `ST` handler's `INSP_NG_SNAP` / `INSP_NA_SNAP`. So the
machine_setting value is overridden at every session start, and an
`INSP_NG_SNAP` sent BEFORE the session is wiped by it. It has to be sent after.

The cost is that no NG has left an image behind, so nothing can be re-inspected
offline against a def afterwards.

### A stopped plate cannot produce an FI frame

FI runs `camera->TriggerMode(2)` -- hardware trigger, and on this machine the
trigger rides the backlight line and is produced by plate motion. Plate stopped
means no trigger, no frame, and an FI session that looks hung when it is
correctly waiting.

CI runs `TriggerMode(0)` (free run), which is why the setup view has a live
picture with the plate at rest. So any "static shot" is necessarily a CI result,
and CI also turns station-region enforcement OFF (`insp session: CI -- station
region off`). A verdict seen in the setup view is the core's category mapping;
the board's SEL counters only move with the peripheral channel open and parts
actually passing the gate.

---

## The sorting half finally moved, and what moved it was not what we expected

2026-08-12, first real-verdict runs. Two things had to be fixed before the
machine could produce a non-NA verdict at all: the core must have a def loaded
(`!fi`, not `!ld` -- the latter loads a recipe and never opens a session), and
the backlight must be on (it is driven by the board's stage tasks, so a headless
rig sees black frames and a def that matches nothing).

60 s, FI, plate 10000:

```
SEL1 245   SEL3 643   NA 204   SEL2 0     UNANSWERED 0   SKIP 0
edges 1380 == accept 1091 + Sigma rej 289      residual 0
discard_stop 19 at teardown
```

That is A1's headline gap closed for the positive cases, and A3/A5 confirmed
under a real run rather than an injector.

### act_cap fires on a SPEED CHANGE, not on a tight window

`DEV_COMPLETE_CHECKLIST` predicted `act_cap` would need "a tight SEL1 window
(win/pitch 1.80)". It does not. A single large speed change is enough:

```
steady   9000            act_cap 0
9000 -> 13000            act_cap 202     <- all of it, here
13000 -> 9000            act_cap 202     <- nothing further
```

202 actuations capped by one ramp, `act_cap_max_t` 586. Whatever tightens during
a ramp reaches the cap far more easily than the window geometry does, and any
run that changes speed is already exercising this.

### FREQ_TXN stayed zero through 44% speed changes, in both directions

9000 -> 13000 -> 9000 with real verdicts flowing: `FREQ_TXN`,
`FREQ_TXN_TIMEOUT` and `FREQ_TXN_DRAIN_MAX_MS` all remained 0.

This is the condition the checklist set for deciding its fate: "if a real-verdict
soak also leaves it at zero, the honest outcome is to DELETE the transaction
machinery". Nothing stages because `speed_band_pct` is 0 and the band itself was
removed (`3becdfd6 gate: remove the speed band`), so the staging path has no
trigger left. The evidence for deletion is now in hand.

### SEL_SUPPRESSED cannot be reached by stopping the plate

Stopping the plate with judged parts in flight produced **no** suppression, and
the reason is the same unreachability found in A4:

`ACT_SEL` requires `PLATE_RUNNING && !SYS_STEPPER_DISABLED && !DRY_RUN`, and
`PLATE_RUNNING` is `PLATE_FREQ_CURRENT > 0` -- but the step timer's alarm is
disabled at zero, so `Run_ACTS` does not execute at all. Those blows were never
suppressed; they were never *reached*, and the teardown discarded them
(`discard_stop` 34).

The one reachable path is `SYS_STEPPER_DISABLED` going true while the plate is
still turning: the ISR keeps running, the condition fails, the counter moves.
That means de-energising the driver at speed, which lets a loaded plate coast
and can throw parts -- the same hazard the `pin_on` guard already warns about.
So this belongs to **B6 (device-side fault injection)**, as a hook that makes the
condition false without touching the driver. Do not cover it by pulling ENABLE
on a running machine.

---

## Station placement: the plate as a positioner, and what it measured

2026-08-12 afternoon. Setting a station offset was trial and error -- guess a
number, run, watch where the blow lands, guess again -- so the plate was made a
positioner instead: `jog_arm` catches the next part at the gate and stops,
`jog offset:N` puts it AT N, `jog_end` hands back the number to paste into
`SEL1_on` / `CAM1_on`. Absolute, because a UI cannot know where the plate
stopped; braking distance is not predictable from outside.

Building it turned up more about the machine than about itself.

### pulses_per_rev is 70400, not 60000

The configured value was documented as a rough estimate and every mm<->tick
conversion went through it.

```
2816001 ticks over 40 revolutions -> 70400.025 ticks/rev, +-0.07
70400 / 2 ticks-per-step = 35200 steps = 3200 microsteps x 11:1
```

One tick of residual across forty laps. Method: one part on the plate, the gate
edge as the lap marker (`jog.origin` is the absolute tick of that edge, and the
braking coast happens after it, so it cancels), laps counted by `GATE_EDGES`.
Measuring one long interval rather than averaging single laps divides the ~3
ticks of edge noise by the lap count.

The estimate was **17.3% out**, and the visible consequence was invisible:
`min_detect_dist_um` 2000 enforced 159 ticks, which is 1.70 mm, not the 2.00 mm
it claimed. The request was reported; the ticks it resolved to were not. They
are now (`gate.min_dist_ticks`).

Geometry is a per-machine SETTING now, not a `#define`. `pulses_per_rev` and
`diameter_mm` were already set_setup keys and already persisted -- they were
simply never consumed by the arithmetic. Left with `min_detect_dist_um`
compensated to 1703 so the enforced distance is the same 159 ticks it always
was: the change is in the honesty of the numbers, not in what the machine does.

**`diameter_mm` 240 is still an assumption.** Parts do not ride at the plate's
rim, so micrometres-per-tick for a PART is smaller than the firmware computes.
The tick count per revolution is exact; the millimetres are not.

### The gate zero was the trailing edge; centre is available now

`middle_pulse` was computed and thrown away with a `(void)` cast. Trailing is
the worst available reference and this firmware's own measurements say why: the
sensor has a fixed TIME response (the A2 fit, t0 = 3.52 ms) which inflates the
measured width by `t0*f` -- 21 ticks between plate 3000 and 9000 -- and that
inflation lands on the edges, so the trailing edge carries it and the centre
cancels it. A zero that moves with plate speed is what a station offset must not
have. The trailing edge is also a point on the PART, so it moves with the part's
length and orientation; the centre halves that too.

`gate.gate_ref` = `trailing` (default, what every shipped offset was calibrated
against) or `center`. Switching moves every station by half a part, ~142 ticks
at the measured `w_mean` of 285.

### Accuracy, measured

Same part, same speeds, gate_ref=center, n=5:

```
jog landing      -7..-4 ticks      band of 4 ticks, ~0.05 mm
catch + jog      15.3 px = 0.212 mm   (scale 0.0138859 mm/px)
```

Removing the landing error from the cx spread only takes it from 15.3 to 12.8
px, so **the catch dominates and the motion barely contributes**. To do better,
do not re-catch: catch once and drive the same part to every station in turn.

### Three claims from this afternoon that did NOT survive

Recorded because the reasoning looked sound each time.

1. **"The parts slide on the plate."** An arm-speed sweep showed cx spreading
   1.77 mm across 3000-9000, and it was attributed to the part sliding during
   the long coast. That attribution was unfounded: stepper step-loss produces an
   identical signature and was never ruled out, and the sweep had **no
   repeatability baseline** -- four speeds, n=1 each, so nothing separated a
   speed effect from the catch's own spread. The operator then also found the
   plate itself had been loose. Do not cite that 1.77 mm as evidence of
   anything.
2. **"The part track radius is ~108 mm."** Derived from a measured 0.01135
   mm/tick against the nominal 0.01257. Both terms were wrong -- the tick
   conversion used the 60000 estimate, and the measurement predated the guide
   plate being moved.
3. **"3.18 mm is a real part's shadow"** (the A2 intercept). With 70400 the same
   intercept is 2.71 mm. The A2 model itself is unaffected -- it is a fit in
   ticks -- but the millimetre coincidence that made it persuasive is not there.

### A manual light hold does not survive a peripheral-channel rebuild

Symptom: the backlight dies a few seconds after `light on`, with a 30 s hold
requested, `state` IDLE and the plate stopped throughout. With the core stopped
it holds for as long as asked.

Cause: any BPG client joining or leaving makes the core rebuild the peripheral
channel, `CONNECT` sends `RESET`, and the board's `handleResetCommand()` calls
`ALL_OUTPUTS_SAFE()`. So the act of connecting a client to take a photograph
turned the light off, and the photograph came out black.

Consequences worth knowing:
- do not churn BPG clients around anything holding an output
- a snapshot tool should stay connected (see `tools/webctl/snapd.mjs`)

### trig_cam_pulse strobed the light AFTER the trigger

It always was the one-packet snap -- light and camera in a single command,
board-timed -- but the order was camera, wait `light_delay`, light. The camera
starts integrating on the CAM edge, so it spent the exposure in the dark and
returned an almost black frame, and a longer `light_duration` could not help
because the light was still arriving after the shutter closed. That is why the
WebUI grew a separate three-round-trip `camSnapWithLight` to hold the light
steady instead.

Light first now, 500 us for the backlight to reach brightness (~300 us
measured), then the trigger, then the light held through the exposure. Defaults
were 100/100, tuned for the broken order; now 500/15000.

### `!sv {"type":"__CACHE_IMG__"}` through the perif console writes zeros

It looks like the WebUI's save and it ACKs. The SV handler picks between "dump
the cache image" and "write these raw bytes" on
`dat->size - strlen(json) - 1 == 0`, and a console-injected packet cannot
satisfy that, so it takes the raw branch and writes ~16 MB of whatever was in
the buffer to a file named `.png`. Silently, with `ack: true`.

---

## B5 answered: the framing layer does not resync, and nothing records it

2026-08-12. `PAIRING_MIGRATION_STATUS.md` recorded the resync path as "Untested:
no framing error has occurred since", and the 8-hour soak's 0 CRC failures meant
it still had not been walked. Provoked deliberately, it fails.

Method, isolated so there is no ambiguity about which end broke: core stopped,
serial port opened directly, board confirmed healthy, then malformed bytes
written straight down the UART.

```
healthy baseline            ANSWERS
right after garbage         silent
after 8 newlines            silent
after 64 more newlines      silent
after 5 s idle              silent
PING while wedged           silent
clear_error while wedged    silent
after a DTR reset           ANSWERS
```

**One malformed frame silences the device link.** It does not recover on its
own and no amount of delimiter helps.

**CORRECTED the same day.** The paragraph that stood here said it was not the
documented `SERIAL_PROTOCOL_ERROR` latch, on the reasoning that a latched error
would still answer `clear_error`. That reasoning was wrong, and so was the
conclusion. It IS the latch, and the latch has an escape hatch -- exactly one:

```
after garbage:            silent
PING:                     silent
clear_error:              silent
{"type":"RESET"}:         ANSWERS
```

`Data_Layer_Protocol.cpp` says so plainly at `matchResetKeyAt`: once the parser
is in `RTYPE::ERROR` no frame is delivered, so the ordinary command handler is
unreachable, and the raw buffer is scanned for `"type":"RESET"` alone. The
tolerant match was itself a fix -- a host whose `json.dumps` emitted
`{"type": "RESET"}` with a space used to have no way back at all.

The test tried the two commands an operator would try and neither is the one
that works, so "no way back" was concluded from two data points that were never
going to be it.

Worse, it is silent at both ends. `error_hist` is empty and `rx_crc_fail` is 0
afterwards (a reset clears them, and the reset is the only way back), and the
core logged nothing. From outside, a machine that has gone deaf this way looks
exactly like a machine that is idle.

The first attempt at this went through the perif console with the core running,
and `!pd CONNECT` recovered it -- which rebuilds the core's channel AND resets
the board, so it proved nothing about which end had wedged. Stopping the core
first is what made the answer unambiguous.

What this costs in production, with the correction applied: any line noise, any
partial write, any host that dies mid-frame takes the machine deaf until
something sends it a RESET. The core does that on CONNECT, which is why
`!pd CONNECT` recovered it -- but nothing sends one on its own, so the link
stays dead until a human reconnects.

Three things are wrong with that, and none of them is "unrecoverable":

1. `clear_error` is the command an operator or a generic host will try, and it
   is the one command that cannot work here, because the handler it would reach
   is downstream of the parser that is latched.
2. Nothing recovers automatically. There is no idle timeout, no resync on a
   delimiter, no retry.
3. It is silent. `error_hist` empty, `rx_crc_fail` 0, nothing in the core's log.
   A machine that has gone deaf this way is indistinguishable from one that is
   idle -- and that part of the original finding stands unchanged.

The framing/CRC half of B4 is proven (0 failures in 426840 frames) -- but that
is the happy path, and this is what happens the first time it is not.

---

## SEL_SUPPRESSED covered, with virtual objects and two failed attempts

2026-08-12. A1's last uncovered mechanism, exercised without parts, without the
camera, and without de-energising the driver at speed (the only path a real
machine offers, and it throws parts off a loaded plate).

```
trig_phantom_pulse   one object per request; the board ANNOUNCES its tid
report tid cat:1     that exact object gets an NG verdict
SWITCH               schedules its SEL1 actuation
fault sel_suppress:M fails the guard for M of them
```

Result, `tools/fault_sel_test.py`:

```
6 NG verdicts reported
  SEL1              = 2   (6 - 4)
  SEL_SUPPRESSED    = 4
  sel_suppress_used = 4
  UNANSWERED 0   NA 6   SKIP 0
```

Three numbers that have to agree and do.

### Both wrong turns are worth keeping

**Dry run is one of the three things the guard tests.** The first attempt used
it to keep the plate still while injecting. Every actuation was therefore
suppressed by the harness itself, SEL_SUPPRESSED came out exactly equal to the
armed count, and it read as a pass. `sel_suppress_used` was 0 -- the injected
fault had never been consumed -- and that is the only reason it was caught. A
counter that says "the instrument fired" is worth as much as the instrument.

**Do not guess a tid.** `tid_counter` is not reset by `reset_running_stat` and
CAL consumes tids of its own, so 1..N is a guess that silently reports verdicts
against objects that do not exist. The `cam_trig` announcement carries the real
one:

```
{"type":"cam_trig","tid":2,"cam":1,"t_us":19160985,"gate_pulse":32640,"w":20}
```

Read it. And widen the capture window: four of ten announcements were missed
here simply by not listening long enough, which costs objects rather than
correctness but makes the arithmetic harder to check.

---

## The pairing migration is promoted; the host has not been told

2026-08-12, closing A6's first two steps and correcting the checklist that
carried them.

The board runs `report_match_ts: true`, out of NVS, and has been. The checklist
still said "still false" and planned a re-soak to justify promoting it — the
promotion had already happened. The evidence that supports it is the 8-hour soak
taken with the flag OFF, which is the harder test: the device computed the
timestamp match on every report and compared it against the tid match, and
across 337826 reports they never once disagreed, with `delta_max_us` 121 against
a 5000 µs tolerance.

`report_match_pcnt` is false on purpose. The camera's own trigger count was the
third candidate and was measured unreliable (`6c88be34`, "ask the picture which
mechanism is right, and it says pcnt is not"), so trigger-count figures are not
a second opinion worth taking.

**What is still true is the part nobody did:** the host still compiles its own
pairing. `PERIF_CORE_PAIRING` is `1`, `PerifTriggerPairing.hpp` is 645 lines,
and `wiringPanel.cpp` has 21 conditional sites — `tap_trigger_info`,
`keep_clock_warm`, the trigger wait, the early dump — all reconstructing a value
the device now announces outright.

Two implementations of one decision is the shape a mis-sort hides in. Deleting
them is not tidying.

---

## drift_comp: 80x better, and the A/B that said otherwise measured a high-water

2026-08-12. `cam_drift_comp` was opt-in, on the strength of an A/B that "showed
no improvement". That A/B compared `delta_max_us`, which is a SINCE-BOOT
high-water and was never reset between arms — so the first arm's worst case was
simply inherited by the second, and the comparison could not have shown a
difference whatever the truth was. The same trap caught this session twice
before the number was read properly.

Per-sample `|delta_last_us|`, same traffic, ~2.6 s spacing:

```
drift_comp OFF   mean 74.5 us   median 74.0   p90 79.0   max 197
drift_comp ON    mean  0.9 us   median  1.0   p90  2.0   max   3
```

`delta` is what places a frame on an object. `resid_us` barely moves between the
two and never will: it is computed against the UNPROJECTED offset, so it is by
definition the raw drift since the last sample, compensated or not. Reading it
as the score is what made this look like a no-op — and it is also why the
residual is "always negative", which is not a calibration fault. Two crystals
differ at a fixed rate, so the accumulation between samples has a fixed sign.
The magnitude checks out: slope 22.7 ppm x 2.6 s gap = 59 us, measured -59.5.

Default is now ON.

### 30 minutes with it on

369 samples, phantom objects, a 90 s idle every 5 minutes:

```
ALL      mean 1.53   median 1.0   p90 2.0   p99 26.0   max 31
STEADY   mean 1.19   median 1.0   p90 2.0   p99 26.0   max 31   (t > 60 s)
slope    converged -23770..-21683 ppb over the half hour (2.09 ppm of wander)
rejected 0   recals 16   error_hist []
```

Every sample above 6 us after the first minute — 11 of 354, 3.1% — is an idle
recovery, at 6.6, 13.1, 19.6 and 26.1 minutes. Each costs one sample at 26-31 us
and is back to 1 us within two or three. Nothing else in half an hour exceeded
5 us.

The first minute is the slope converging, visibly: |delta| 23 -> 20 -> 14 -> 7
as slope_ppb walks -27347 -> -23449. It learns, and it learns the right thing.

### The long-idle case is held by RECAL, not by the slope

The firmware's own note worried that the slope was only ever tested over burst
gaps "of tens of ms" and that the case it is FOR — a slow line, parts minutes
apart — was never tested. Tested here, and the worry does not apply: with
`recal_idle_ms` at 10 s, a 90 s idle triggers a recal that re-measures the
offset outright, so the gap the slope is ever asked to extrapolate over stays
small. 90 s at 22 ppm would be 2000 us of accumulated error; the measurement
after each idle is 31.

So the slope's job is the gaps BELOW the recal threshold. That is a narrower job
than the comment assumed, and it is doing it at 1 us.

### Decaying peaks instead of since-boot maxima

`delta_max_us`, `miss_delta_max_us` and `max_resid_us` now decay:

```
max = (delta > max) ? delta : max*0.999
```

A plain maximum never forgets, which breaks both things it gets used for: it
cannot compare two conditions, because the first one's worst case is inherited
by the second (exactly how drift_comp was dismissed), and on a running machine
one outlier from an hour ago stays the headline long after the machine has been
fine. The envelope takes a new peak instantly and forgets an old one over about
a thousand samples, which is the "worst of the recent past" the number is
actually read as. `max_resid_us` keeps the SIGN of the peak it holds, because
the sign is the whole tell for a drift.

### delta does not scale with the gap, so the joint estimator has nothing to win

`virt_pulse` at exact tick periods, four blocks of 150 s, drift_comp on:

```
nominal gap   n    |delta| mean  median  max   slope_ppb settled
   1.0 s      91        3.84     4.0      5    -25567   (never learned)
   2.0 s      63        0.54     1.0      2    -21749
   4.0 s      36        0.39     0.0      2    -21760
   8.0 s      22        0.59     1.0      2    -21752
```

Flat across 2 s to 8 s. If the residual were a fractional error in the slope it
would grow with the gap; it does not, so what is left is a per-sample constant
at the timestamp granularity — 1 us is the quantisation, and the medians are
0 and 1.

That is the measurement the Tier C alpha-beta item was waiting for. Its premise
is that estimating offset and slope separately leaves recoverable error because
the two chase each other. There is no recoverable error at 0.5 us. **Closed, not
deferred** — reopen it only if B1 tightens the window far enough for a
microsecond to matter.

### The slope's 1-second learning threshold is a cliff, and ~1 part/s sits in it

The slope only learns from samples with `last_gap_us >= 1000000`. The 1.0 s
block above measured 0.94 s — just under — so 90 of its 91 samples taught it
nothing, the estimate stayed at a stale -25567 against the correct -21750, and
|delta| was **8x worse** than every other block.

The threshold has a real reason: `resid/gap` at short gaps is dominated by the
1 us quantisation (1 us / 55 ms is 18000 ppb of pure noise). But placing it at
exactly one second makes a cliff rather than a taper, and a machine running at
about one part per second lands in it — traffic that is regular, plausible, and
silently stops maintaining the clock model.

Not urgent: the cost measured 3.84 us against a 5000 us window. Worth fixing as
a taper (weight by gap) rather than a threshold if this is ever revisited.

## 計數器的斷線備份:存檔點是「機器停下來」,不是「通訊斷掉」

SEL/NA 計數只活在 RAM,而板子會被一件沒人要求的事重開:主機開啟序列埠會拉
DTR/RTS,那接在 EN 上。所以核心重啟 = 整班計數消失。韌體**看不到那個復位**——
它是硬體 EN 脈衝,沒有任何一行程式碼有機會執行——所以存檔必須更早發生。

存檔點是兩個**狀態進入事件**,不是通訊狀態:

- **進 ERROR**(任何原因,不只 `HOST_LINK_TIMEOUT`)。原本只掛 host 錯誤,
  結果任何其他故障把機器停在 ERROR 之後,主機若先死掉,那一輪的計數就沒了。
- **進 IDLE**,而且只在 `pre_state` 是執行中狀態時。開機是從 INIT 進 IDLE,
  在那裡武裝會用剛剛還原出來的值再寫一次 flash,毫無意義。

**不要改成「comm timeout 就存」。** 失去主機是一個*條件*不是*事件*:放在狀態機
外面每一圈迴圈都成立,必須加 latch,而 latch 寫錯就是在主機離開期間持續寫
flash——對一個下班關掉核心的操作員來說就是寫到隔天。而且在 IDLE 它寫的位元組
跟進 IDLE 時存過的一模一樣:IDLE 設 `blockNewDetectedObject`,沒有物件會被收進
管線,所以沒有 SEL、沒有 NA、`GATE_ACCEPT` 也不動。

### 存檔不等盤面停,而且不該等

`countersNvsService()` 只等**最後一發 SEL 打完**(`SEL_SAFE_AT_MS==0`,有界,
約一個吹氣寬度)。`SELn_Count` 是在吹氣**開始**時加的,吹到一半存檔會記下一個
零件還沒落袋的計數——這一段是正確性,不是保守。

刻意**不**等 `PLATE_FREQ_CURRENT==0`。`cfgPersistDeny()` 防的是 flash cache 被
關掉時、住在 flash 裡的 ISR 被進入就 fault——step 路徑不在此列:`onTimer` 及它
所觸及的 `StepGo` / `GateSensing` / `Run_ACTS` / `phantomServiceISR` /
`ActRegister_pipeLineInfo` 全是 `IRAM_ATTR`,始終有映射。等待的代價則是實打實
的:減速時間是 `plate_freq / plate_accel`,實測 15571Hz、accel 2000Hz/s 就是
**約 7.8 秒**計數只在 RAM 裡,而主機可能被自動重啟——重啟就重開板子。實測拿掉
等待後是 **55ms**。

(`CNT_NVS_CLEAR` 仍然等盤面停:`reset_running_stat` 允許跑批中下達,而且它不是
急救,提早碰 flash 沒有任何好處。)

### `comm_lost_backup` 由主機武裝,開機必定關閉

看門狗原本只看 `host_timeout_ms`,而那是存在 NVS 的數字——一顆從沒接過核心的
板子只要 NVS 有殘留設定,就會在工作台上把自己停掉。決定看門狗該不該生效的不是
一個存起來的數字,是**有沒有主機真的在**。`COMM_LOST_BACKUP` 因此是執行期旗標、
不持久化、每次開機都是 false,由核心的心跳每約 2 秒送一次來武裝。

### 看門狗要涵蓋所有「零件在動」的狀態,不只 READY

原本只測 `INSPECTION_MODE_READY`,於是在機器自己的常態循環裡是瞎的:管線一空就
會自動進 **RECAL**(`recalService`),SPINUP/CAL 則是進場必經。實測抓到:在 104
殺掉主機,什麼都沒發生——不停線、不存檔,盤面繼續轉過一個無人應答的分選器。
現在涵蓋 READY / RECAL / SPINUP / CAL / TEST;IDLE 排除(盤面停了不需要主機),
ERROR/FATAL 排除(已經停了)。

注意 `INSPECTION_MODE_TEST` 在轉移表裡**沒有** `INSPECTION_ERROR` 這條邊,所以
在 TEST 觸發時 `SYS_STATE_Transfer` 是靜默的 no-op。

### `cnt_nvs_lat_ms` 不能拿來判斷「有沒有發生新的存檔」

它量的是 `selHoldMs()`——由設定的吹氣寬度算出的固定值——所以**每次存檔都回報幾乎
同一個數字**(這台機器是 55)。舊記錄和新記錄長得一模一樣。我曾據此下過「這次沒
存成」的錯誤結論。要分辨請用 **`cnt_nvs_seq`**,它每次存檔遞增並寫進記錄本身。

### 這些欄位在 `get_backup_stat`,不在 `get_running_stat`

把它們加進 `get_running_stat` 會撐爆 `StaticJsonDocument<3072>`(加之前實測
2886),裝置開始回 `stat_doc_overflow`——丟掉的**不是新欄位,是整包機器狀態**。
放大那個 document 也不是選項:它是 loop task 上的 static 文件,而 `stack_hwm`
實測低到 2052。

### 沒有變化就不寫:讓寫入頻率跟著「計數」走,而不是跟著「故障」走

任何錯誤都會武裝存檔(那是刻意的,見上),但錯誤是會重複的:對著一個持續存在的
故障做 `clear_error → REDEEM → CAL → READY → 又錯`,每一圈都會武裝一次,每次寫入
都把 loop task 擋住數十毫秒,而寫進去的位元組跟已經存著的一模一樣。

所以 `countersNvsService()` 會先跟 `CNT_LAST_SAVED`(RAM 裡的「flash 現況」)比對,
相同就跳過,只累加 `cnt_nvs_skipped`。開機還原時 `CNT_LAST_SAVED` 由還原出來的
記錄填入,`countersClear()` 成功後歸零——否則清空之後的第一次存檔會誤判成相同。

### 燒錄後裝置可能是 latch 住的,而心跳救不回來

實測 2026-08-13:`pio run -t upload` 之後啟動核心,裝置持續送 `SYSTIME` 除錯訊息
但**完全不回任何命令、一個 PONG 也沒有**——parser latch 的樣子(latch 之後不會有
frame 被交付,所以連錯誤回覆都沒有,跟 `serial_error_locked` 那種「會回但拒絕」
不同)。最可能的成因是燒錄寫入與核心開啟 port(那會重開板子)撞在一起,開機 ROM
的輸出混進我們的位元組,餵給 parser 就是垃圾。

**平常的核心重啟不會這樣**——單獨測過一輪,回覆正常、PONG 正常。所以這是開發
路徑上的事,不是產線路徑上的。

要注意的是 **ping train 救不了它**:它送的是 `ping`,而 latch 只認 `RESET` 和
`clear_error`(從原始緩衝區比對)。所以一顆 latch 住的板子不會被自動武裝,
`comm_lost_backup` 永遠是 false,B4 那套完全不會啟動——A7 早就預告過這件事。

復原:送 `{"type":"clear_error"}`。

## 2026-08-18 — pulse-count pairing deleted; what to check after the flash

Removed end to end: firmware `CamPulseSync`/`CAM_PCNT`, `pipeLineInfo.cam_pcnt`,
`REPORT_MATCH_PCNT`, the dual-mode arbitration and its `CAM_PAIRING_DISAGREE`
halt; core-side the `pcnt` field in the report and `INSP_PERIF_PCNT_SLIP`.
Timestamp (`cam_ts` / CAM_SYNC) is the only pairing.

**Kept on purpose, do not delete as "leftovers":**
- `CAM_PULSE_N` — the board's own count of CAM1 edges it drove, reported as
  `cam_pcnt.dev_pulses`. A diagnostic about the BOARD.
- the camera's `ExtTriggerCount` watermark decode in CameraLayer_Aravis —
  `extTrigCount - frameNum` is how many triggers the camera REFUSED, which is a
  measurement about the CAMERA. Off by default; it overwrites row 0.

**Compatibility:** `set_setup` refuses a whole document containing an unknown
key, and old tools/backups still name `report_match_pcnt`. So the key stays in
the K_CAM schema and `false` is accepted silently; `true` is refused with
`err: "report_match_pcnt_removed"` rather than accepted and ignored — a machine
that silently declines a setting is worse than one that tells you what you chose.

**The mistake to avoid when doing this kind of deletion:** `CAM_PULSE_N` was
incremented on the same line that stamped the object (`task->src->cam_pcnt =
++CAM_PULSE_N;`). Deleting the stamp deleted the count with it, and the ISR's
CAM stage silently stopped counting — `dev_pulses` read 15 after a 1445-part run
instead of ~1900. Caught only because the first verification run looked at that
number. **After any pairing surgery, check `dev_pulses` against the parts fed.**

**Verified after flashing** (NVS survived: `report_match_ts` still true,
`skip_policy` stop_only/10, gate 28571, SEL1_on 30000 all intact):
1445 phantom parts at 25/s, 1806 RP frames — `cam_sync` valid + authoritative,
`disagree` 0, `rejected` 0, `rebuilds` 0, `resid_us` −2 against a 5000 µs
window, `dev_pulses` 1921. `agree` reads 0 and that is correct, not a
regression: the core sends `tid:-1`, so there is no second opinion to agree
with — which is the whole point of the end state.

## 2026-08-18 — the tid-vs-timestamp voting scheme is deleted too

After the pulse-count path went, the remaining vote was the core's `tid` against
the device's timestamp: `agree`/`disagree` counted how often they named the same
object, and the placement expression fell back to `byTid` when the timestamp
could not place a frame. Both are gone.

**Why the counters had to go, not just be ignored:** with the timestamp
authoritative the core sends `tid:-1`, so `byTid` is permanently NULL and
neither counter can ever move. `agree` 0 reads as "nothing ever agreed" — the
opposite of the truth. `disagree` 0 reads as "no contradiction" when it actually
means "never checked". **A number that can only be 0 is not evidence, it is a
trap**, and this machine's own harnesses were using `disagree == 0` as their
pass criterion.

**Why the tid fallback had to go:** a frame the clock cannot place is exactly
the frame that must NOT be sorted. The old expression handed it to the tid
instead — the confidently-wrong behaviour the timestamp was chosen over.
Now: `tarP = byTs ? byTs : bySync`, and `bySync` only ever fires during
CAL/RECAL where syncPulseService guarantees exactly one outstanding object.

**The replacement evidence is a distance, not a vote**, which is strictly more
informative: `resid_us` / `resid_max_us` / `delta_max_us` (how far each frame sat
from where the clock expected it), `rejected` (samples the outlier guard threw
out), and `CAM_CLOCK_LOST` after two consecutive frames outside the window.
Measured immediately after the change — 1442 parts at 25/s, 1805 frames:
`rejected` 0, `rebuilds` 0, `delta_max_us` **1** against a 5000 µs window,
`error_hist` empty.

**`report_match_ts` is no longer a selector.** It is forced true and reported
true; `set_setup` refuses `false` with `err: report_match_ts_is_mandatory`. The
key stays in the schema because set_setup refuses a whole document containing an
unknown key and NVS/backups name it.

**Nine bench harnesses judged on `disagree`** and were moved to `rejected` +
error 13 in the same commit (burst_pairing, dryrun_pairing, real_parts,
soak_real, soak_pairing, slip_probe, jitter_sweep, regress_watch, flatten_soak,
soak_sched). Deleting a reported field without doing this would have left them
either crashing on a KeyError or — worse — silently passing.

---

## 重複判定「最差者勝」依賴 cat 的數值順序(2026-08-18)

`LegacyFirmware.cpp:6731`:

```cpp
if(cat < tarP->insp_status) { REP_REPEAT_WORSE_N++; tarP->insp_status = cat; }
```

保留**數值較小**的 cat,計數器叫 `REP_REPEAT_WORSE_N`,意圖是最差者勝。
**那只有在 `cat_ng < cat_ok` 時成立。** 反過來接線,同一條規則變成最好者勝:
一個重複的判定把 NG 升級成 OK,壞件跟著好料出去,而計數器名字不變。

韌體本身沒有檢查。core 那邊現在會拒絕 `cat_ng >= cat_ok` 並讓分料維持關閉
(`wiringPanel.cpp`),所以已經被擋住 —— 但**韌體這條規則仍然只是隱含地假設那個順序**。
改動附近程式碼時要記得。

## machine_id 從晶片推導,不再可設定(2026-08-18)

以前是普通的可設定字串,所以會被包進任何設定匯出 —— 而匯出最常見的用途正是
「用一台調好的機器把新機帶起來」。兩塊板子就會回答同一個名字。**那不會有任何異常行為**:
兩台都正常跑,損害落在檢測紀錄上,歸到錯的機台,事後才在資料裡發現。

現在 `MachineConfig::machineId()` 從 eFuse MAC 推導成 `uI-<12 hex>`。
選它而不是「亂數寫進 NVS」,是為了實際會發生的情境:清 NVS 重置機器。
亂數會變成新 ID 並讓已歸檔的紀錄變孤兒;MAC 推導回來還是同一個,因為是同一塊板子。
**已存的 id 仍然優先**,所以用舊方案設定過的板子保住原名。

`set_setup` 的 `machine_id` 分支移除了,但是**接受並忽略**而不是拒絕整條指令 ——
舊備份和舊 host 還會帶這個鍵,為了一個誰都改不了的欄位讓整次匯入失敗不划算。
`get_setup` 仍然回報它。

## `board_query.py` 要先送 RESET_PACKET(2026-08-18)

它送裸 JSON + `\n`,**沒有先送 `{"type":"RESET"}`**。裝置的 parser 一旦進入
framing-error latch,那個位元組序列是唯一能解開的東西 —— 所以會出現
「板子每秒都在送 SYSTIME,但問什麼都沒有回應」。

手動補這一步就通:

```python
s.write(b'{"type":"RESET"}\n'); time.sleep(0.4); s.reset_input_buffer()
s.write(b'{"type":"get_setup"}\n')
```

工具本身還沒修。

## 回覆長度上限的理由是錯的(2026-08-18)

`LegacyFirmware.cpp` 把 `retdoc` 訂在 3584,註解說 host 會砍掉超過 4096 的行。
**那個 4096 在 core 的 `PerifConsoleThread` 裡,讀的是開發用 TCP console 的輸入,
而且 POSIX-only。** 裝置→core 的真正上限是 core 的 `dataBuff[20480]`,
loopback 實測到 20479 bytes 都完整往返。

已經有代價:`get_schema` 的欄位是為了這個想像中的限制才從 `get_setup` 搬出去的。

**但不代表就該調高。** 真正的成本不是 RAM(`retdoc` 是 static,走 `.bss`)而是
**線路時間**:230400 baud 下 8KB 約 350ms,期間握著 `perif_tx_lock`,
排在每一次判定寫入前面,直接壓到 CAM→SWITCH 的期限上。拆成多個指令比較便宜。

注意方向:裝置**自己**的 RX 緩衝是 `include/comm/Data_Layer_Protocol.hpp:26` 的
`dataBuff[2048]`,那是 core→裝置。core 能發的最大 `set_setup` 從來沒被量過。

## 裸板上餵 phantom 被距離過濾全擋(2026-08-19)— **結論已推翻,見章末更正**

沒有馬達時,`trig_phantom_pulse` 送多少都不會有零件通過 gate:

```
gate  {"in": 5, "out": 0, "pct": 0, "loss": "dist", "loss_n": 4}
pipe  {"registered": 0, "waiting": 0}
```

**phantom 有正常 register**(`gate accept`、`pipe registered` 都會動),
被擋的是距離,不是註冊。判定在 `LegacyFirmware.cpp:2858`:

```cpp
if(GATE_MIN_DIST_STEPS && middle_pulse - _prePulse_BK < GATE_MIN_DIST_STEPS)
{ GATE_REJ_DIST++; return -9; }
```

算術是這樣:cam_trig 宣告帶 `gate_pulse`,實測十秒內從 10627 走到
10874 —— **247 steps,約每秒 25**。而 9.8Hz、70400 steps/rev 的盤應該是每秒
**689,000**。沒有馬達時 `SYS_STEP_COUNT` 幾乎不前進,所以連續注入的兩個
phantom 只差約 10 steps,而門檻是 1703μm ÷ 10.7μm/step ≈ **159 steps**。

**拉長時間間隔沒有用。** 80ms 和 700ms 實測拒絕率完全相同 —— 那個過濾量的是
**盤子走了多遠**,不是過了多久,而盤沒有在動。

所以裸板上要讓零件通過,只能把間隔拉到 159 steps 以上,以實測的 25 steps/s 換算
是**每個零件約 6.4 秒**。那測不了吞吐,但足以驗證單顆零件的完整路徑。

**不要為此加繞道。** 一度加過一個 `ignore_dist` 參數讓 phantom 跳過這個
檢查,隨即回退:那個限制是真實機器行為(兩顆零件不能佔同一個位置),繞過它測到的
就不是會出貨的那條路。慢,但是真的。

### 更正(2026-08-19 稍晚):上面的 6.4 秒是我把轉速設錯量出來的

那些數字全部是在 `plate.freq = 12` 下量的 —— 不是 12000,是 **12**。
一圈要轉 50 分鐘。`SYS_STEP_COUNT` 「幾乎不前進」不是因為沒有馬達,
是因為我叫它不要動。出貨值是 **15000**(= 30000 steps/s)。

改成 15000 之後,**60 顆零件、間隔 300ms,gate 通過率 100%,零損失**。距離過濾
沒有任何問題。裸板餵料不受 6.4 秒的限制,那個限制不存在。

保留原文而不是刪掉,是因為它示範了一個更容易再犯的錯:`{"type":"plate","freq":N}`
會被靜靜忽略(正確寫法是 `set_setup` 底下的巢狀 `plate.freq`),
而一個沒生效的設定看起來和一個生效了的設定完全一樣。**餵料之前先讀回
`get_running_stat` 的 `freq_meas` 確認盤在轉。**


## `CAM_CLOCK_LOST`(13)在運轉中的機器上不可達(2026-08-19,實測 + 讀碼)

時鐘漂出配對窗口時,機器**會**停 —— 但停在 **error 1
`INSP_RESULT_MATCHES_NO_OBJECT`**,不是 error 13。三次實驗都一樣,
每次都是 `rejected=1`、`rebuilds=0`,13 一次都沒出現過。

原因是結構性的,不是時序運氣:

```
gate()  LegacyFirmware.cpp:634    拒絕   nearest_delta >  TOL_US
        LegacyFirmware.cpp:6563   byTs   nearestDelta <= TOL_US
```

同一個變數、同一個門檻、互補 —— 所以 `gate()` 拒絕的 frame **必然**
`byTs==NULL`。READY 期間 `bySync` 也是 NULL(sync pulse 只在
CAL/RECAL 發),於是 `tarP = (byTs != NULL) ? byTs : bySync` 是 NULL,
同一輪往下走到 `:6777` 就 raise error 1 停機。

`gate()` 在 `:6595`、`fault_pending` 檢查在 `:6597`,
兩者都在 error 1 之前 —— 但那時 `consec_reject` 才剛變成 1,而
`LOST_N=2`。第二個 frame 永遠不會來,因為機器已經停了。

**安全性質沒破:機器還是停了,沒有拿放不準的 frame 去分料。** 壞掉的是:

1. **診斷指向錯的地方。** 操作員看到「a verdict arrived for no known object」,
   會去查配對;真正的原因是時鐘。`CAMSYNC LOST` 那行連同它帶的
   `last delta / tol` 也永遠不會印出來。

2. **遲滯從來沒生效過。** `LOST_N=2` 的註解寫得很清楚:*"one is a lost
   frame or a stray, two in a row is the clock"* —— 這個容忍單一雜訊 frame 的
   設計意圖不存在,第一個出界的 frame 就停機。

反過來說也成立,而且值得記住:**一旦時鐘真的漂出窗口,你會看到 error 1,
永遠不會是 13。** 「實際使用上沒有 error 1 過」代表時鐘從來沒漂出去過,
不代表 error 1 是個不相干的錯誤碼。

要修的最小改法是在 `:6777` 之前分辨「這個 frame 是被 gate 以出界為由
拒絕的」,是的話讓 `consec_reject` 累積而不是立刻停。但那等於放寬
「單一無主 verdict 就停機」,是政策決定,**未處理**。

重現:`node camsync_lost.mjs --window 200 --rate 0.2`(需要 core 帶
`INSP_CAM_TS_MULT=1.0000833` 且 `slope_n` 已收斂)。
