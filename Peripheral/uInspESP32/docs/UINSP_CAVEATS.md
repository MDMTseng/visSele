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

