# Pairing validation: what was proved on 2026-08-06, and what it cost to prove

Companion to `PAIRING_MIGRATION_STATUS.md`, which is the state-of-the-machine
document. This one is about the *evidence*: what is now actually established,
what every measurement is worth, and — at least as important — which
measurements turned out to be worthless and why. Several instruments here
returned clean results while testing nothing, and that failure mode is the main
thing worth carrying forward.

---

## The one-paragraph version

`report_match_ts` is now true and validated on real parts. The device's
timestamp match agreed with the core's tid on 2473 real parts with zero
disagreement, and a verdict-tracing probe confirmed 2392 real parts were each
answered with their own verdict and not a neighbour's. Turning the core's
pairing off (`PERIF_CORE_PAIRING 0`) is functional and exposed two firmware
defects that the tid had been masking. A reproducible mis-sort was finally
found and attributed — to the **core's** pairing, not the device's, which is
the migration's whole premise.

---

## What is established

| Claim | Evidence | Worth |
|---|---|---|
| Timestamp match agrees with tid on real traffic | 2473 real parts, `agree=2472 / disagree=0` | Strong. This is the evidence the promotion always lacked; everything before was phantom pulses on an empty plate. |
| Verdicts land on the right part | 2392 real parts, `SEL1=1273 SEL2=1210`, zero misplaced | Strong for the operating point. See the discount below. |
| Sustained overload halts rather than mis-sorts | `state=112 err=[13]`, 8 verdicts emitted, 0 misplaced, recovers via `clear_error` and sorts correctly afterwards | Strong. "Refuse" is fine, "answer wrongly" is not, and it refuses. |
| Sporadic frame loss self-heals | 9 runs, 8–16 parts lost each, no halt, zero misplaced | Strong. This is the case a real line meets and it does not stop for it. |
| The match window cannot be configured wider than the object spacing | clamp verified: sep 4000→window 2000, 2000→1000, 300→200 + warning | Guard, not a measurement. |
| Residual distribution is thin-tailed | 6685 samples over 12 min: 6681 under 64 µs, 3 in 64–128, 1 in 128–256. `delta_max=150 µs` | Strong for 12 min. This is the number that decides whether the window can be tightened. |
| busy → idle → RECAL → busy works with real parts | 3 forced idle windows, `recals=3 recal_skipped=0 cal_fails=0 rejected=0`, `disagree=0` throughout | Strong. The path where both RECAL bugs lived, now exercised with parts in the machine. |
| No heap leak per RECAL | 45 consecutive recals, `free_heap` flat at 192320, not one byte | Strong. See "the leak that wasn't" below. |

### The discount on every "zero misplaced" result

At the operating point (33 ms object spacing, 5 ms window) the neighbour sits
**6.6 windows away**. A slip is arithmetically unreachable there regardless of
how good or bad the algorithm is. Those runs measure *the operating point*, not
the algorithm's robustness. They are worth having — they say the machine is far
from the edge — but they are not evidence that the pairing would survive a
tighter regime.

---

## The reproducible mis-sort, and who owns it

Found with a seeded-noise object train (see "Instruments that lied" below):

```
gate 12000us:  n=591  BAD=0   DISAGREE=0    delta_max=4168us  err=[]
gate  2000us:  n=481  BAD=62  DISAGREE=140  delta_max=54us    err=[]  state=101
```

`delta_max=54us` says the **device** was landing on its object to within 54 µs
of dead-on, against a 5000 µs window. That is not the side that mis-assigned.
`DISAGREE=140` says the core and the device chose different objects, and the
verdict content follows the core's tid — so the part receives a neighbour's
verdict while the device places the frame correctly.

**The core's pairing is the one that slips.** That is exactly what
`PERIF_CORE_PAIRING 0` removes, and this is the first direct measurement of it
rather than an inference from the header's history section.

Note `state=101, err=[]` — **no halt**. Silent. The gate setting that produces
it (2000 µs = 500 parts/s) is far past anything physical, but the mechanism is
real and it does not announce itself.

---

## Two firmware defects, both masked by the tid

Turning the switch off is what found them. With no tid, `byTs` is the *only*
way to place a real part's frame and `byTs` needs a valid clock — so any window
in which the device deliberately invalidates its own clock while a part can
still report is a halt. RECAL was such a window and it halted at `accept=2`:

```
NOMATCH state=104 tid=-1 valid=0 nearest=0 rb_real=2 rb_sync=1
```

1. **`calFireNow` was gated on `blockNewDetectedObject`.** So CAL and RECAL had
   to *clear* that flag merely to be able to fire, which also opened the sensor
   path to real parts for the whole phase — exactly contradicting the comment
   directly above it. `calFireNow` is the only caller and fires deliberately, so
   it no longer consults the flag and calibration keeps the sensor shut for real.

2. **RECAL dropped the clock estimate on entry.** The entry guard checks RBuf,
   but registration can complete just after it passes, so "empty at entry" is
   not "empty". The reset is deferred until `syncPulseService` finds the
   pipeline genuinely clear. If it never clears, the recal is skipped and the
   previous offset kept — that is not a failure and must not stop the machine
   (`recal_skipped` counts it).

Also: the core **silenced itself** at `PERIF_CORE_PAIRING 0`, because the TX
gate refused any report with `tid < 0`, which at 0 is every report. No reports
means no verdicts and no device-side calibration either, since the device learns
from them.

> Generalises: **the tid is load-bearing in more places than `grep byTid` shows.**

---

## Instruments that lied

Every one of these produced a *clean result* while testing nothing. This is the
part of the day most worth remembering.

**Host-side pacing.** Every phantom test until now paced pulses from Python over
TCP. Measured spacing against a requested 33000 µs: min 42379, median 80519,
p95 90246 — jitter of ±47 ms, **nine times the 5000 µs match window**. The
harness could not hit its own set point, so the regime where a slip is possible
was never reachable. Replaced by the device's own `trig_phantom_train`.

**A regular verdict pattern.** The slip probe started as blocks of 5 OK / 5 NG.
That is periodic with 10, so a slip of exactly 10 shifts the pattern onto itself
and passes perfectly — measured, not theorised: **a real 10-part slip gave a
clean pass over 510 parts, every verdict "correct"**. Any multiple of the period
does the same. Now a seeded hash of the object id: no period, every nonzero slip
disagrees on ~half the parts, chance of hiding ~2⁻ᵏ.

**A perfectly even object train** would have been the next mistake, and was
caught before it was made: a regular train is degenerate — every object at the
same offset from its neighbour, so the match either always works or always fails
and the boundary is never explored. Nominal pitch **plus seeded jitter** is what
finds the edge, because the risk is a tail event.

**A too-narrow shift scan.** The probe reports the slip's magnitude by re-fitting
the pattern at shifts ±k. At ±8 against a real slip of 10 it reported "no single
shift explains it", which reads as scattered corruption — the opposite of the
truth. Widened to ±64. **A scan that cannot reach the answer is worse than no
scan**, because it gives a confident wrong diagnosis.

**Gate protecting the camera.** The first sporadic-loss test held the gate at the
safe spacing and injected faster. The gate simply rejected the burst, nothing
extra reached the camera, `accept == verdicts`, nothing lost. Correct behaviour
by the machine, but the test measured the gate rather than the pairing.

**Treating any state ≠ READY as a halt.** Reported FAIL on a run sitting in
RECAL (104) with an empty `error_hist`. Only 112/113 are halts.

**A statistic that can only move one way.** A 12-minute soak showed `min_heap`
stepping down by exactly 96 bytes on every RECAL and at no other moment — four
points, perfectly correlated. Projected: 34.5 KB/day, heap exhausted in ~5.5
days. **All of it wrong.** `min_heap` is an all-time *minimum*: it can only fall,
so any repeated transient dip that happens to get slightly deeper each time
draws a straight line that looks exactly like a leak. Measuring the *current*
free heap over 45 consecutive recals showed it dead flat at 192320 — not one
byte. The structural clue that should have prevented the projection was already
in hand: the firmware does essentially **no dynamic allocation** on that path
(`ERROR_HIST` is a RingBuf over a static array, `retdoc` is a
`StaticJsonDocument`, `dbg_printf` and `send_json_string` use stack buffers
only). Nothing that never mallocs can leak. `tools/recal_leak.py` runs the
two-phase check (recal ON vs OFF) if this needs re-testing.

**Unreproducible findings.** One run showed `misplaced=21 / 523` with no halt;
thirteen repeats could not bring it back, and it predated the `disagree` print
so it could not be attributed either. That is most of the way to not having
found it at all. Everything stochastic here is now seeded.

---

## Numbers worth not re-deriving

Geometry: 240 mm plate, 60000 ticks/rev → **0.012566 mm/tick**.

| plate_freq | tick | speed | 1 tick | 0.2 mm tolerance |
|---|---|---|---|---|
| 10000 | 50 µs | 251.3 mm/s | 0.0126 mm | **796 µs** |
| 15000 | 33 µs | 377.0 mm/s | 0.0126 mm | **531 µs** |

- Object spacing 33000 µs at pf 10000 = **8.3 mm** between neighbours.
- Match window in use: 5000 µs = **1.26 mm** at pf 10000 — six times looser than
  the 0.2 mm tolerance.
- Measured matching residual: 152–244 µs = 0.038–0.061 mm.
- Idle drift over the 10 s recal threshold: 350 µs = 0.088 mm — **44 % of the
  796 µs budget**. Tightening the window requires shortening the recal idle too.
- Camera ceiling ~35–36 Hz; it *silently ignores* triggers it cannot service.
- L1A window `654..672` = 18 ticks = **0.226 mm** of travel. Only bites if the
  light gates exposure (camera exposure is currently shorter, so it does not).

### Two effects that look like one

The trigger fires from the step ISR (`Run_ACTS` inside `onTimer`, beside
`StepGo`), so it is locked to plate **position**, not wall time — a stalled ISR
stalls the motor too.

- **Clock offset error does not move the part in the image.** It only decides
  which object record a frame matches, and neighbours are 33 ms / 8.3 mm apart.
- **Camera trigger→exposure latency jitter does move it**, and lands in the same
  matching residual.

`recalService`'s 300 µs threshold was justified as a position tolerance. That
applies to the jitter term, not the drift term; drift's real cost is *spending
window*. The comment now says so.

---

## Tools

| Tool | What it is for |
|---|---|
| `tools/real_parts.py` | Plate turning, real sensor detections. The other tools all use phantom pulses. |
| `tools/slip_probe.py` | Does a verdict ever land on the wrong part? Verdict keyed to the object id, traced on the device. `--real`, `--boundary`, `--sporadic`. |
| `tools/jitter_sweep.py` | Nominal pitch + swept seeded jitter, device-side. Finds the edge as a distribution. |
| `tools/soak_real.py` | Long real-parts soak with forced idle windows; residual histogram, busy↔idle↔RECAL transition, degradation. |
| `tools/edge_sweep.sh` | Deterministic single-report fault injection. **Its `ts+33000` row is mis-calibrated** (assumed configured spacing, not measured). Superseded by `jitter_sweep.py`. |

Core-side knobs (env, all off by default):

```
INSP_PERIF_VERDICT_PATTERN=<seed>   verdict = hash(tid, seed): OK/NG noise
INSP_PERIF_VERDICT_SLIP=<k>         fault injection: key on tid+k
INSP_PERIF_FAULT_EVERY=<n>          apply the fault below to every nth report
INSP_PERIF_FAULT_TS_US=<k>          shift that report's cam_ts by k µs
INSP_PERIF_FAULT_DROP=1             send nothing for it
INSP_PERIF_FAULT_DUP=1              send it twice
```

Device commands added: `get_verdict_log` / `clear_verdict_log` (64-entry ring of
`(tid, cat)` in application order), `trig_phantom_train` now takes `jitter_us`
and `seed`. New stats: `delta_hist`, `sync_late`, `recal_skipped`.

---

## Open, in priority order

1. **Where does the core's pairing start to slip inside a *plausible* config
   range?** The reproduction used a 2000 µs gate (500 parts/s), which is absurd.
   The onset point is the number that says how urgent `PERIF_CORE_PAIRING 0` is.
2. **Residual distribution over hours**, not 12 minutes. 6681 of 6685 samples
   under 64 µs and a max of 150 µs says the window has 5.3x margin at its
   measured worst and >12x at the 99.94th percentile — but a thin tail measured
   over 12 minutes is not a thin tail. The evidence is now good enough to act
   on; it is not yet good enough to stop looking.
3. **Tighten the window to the 0.2 mm tolerance** (≈796 µs at pf 10000) —
   deferred, with the recal-idle coupling above. Deliberately **not** derived
   from plate speed: a window that moves makes failures hard to reproduce, and
   the same test at two speeds stops being the same test. Residual hazard,
   recorded in the code: raising `plate_freq` silently loosens the tolerance.
4. **`PERIF_CORE_PAIRING 0` with real verdicts** cannot use the slip probe —
   with no tid there is no object id to key the pattern on. Needs a different
   oracle, or accept that the assignment path is identical either way and the
   only untested part at 0 is the absence of the tid fallback.

## Not defects, do not "fix"

- `recals=0` on a busy line is **correct**. While parts flow, every report
  re-measures the offset, so it is never more than one part old and the recal
  trigger cannot fire by construction.
- `extra_area_ratio < 0.1` is legacy and being removed. Do not tune it.
- `min_heap` is a high-water mark, not a level. Do not read a slope off it —
  see "the leak that wasn't". Use `health.free_heap` for that.
- All-NA verdicts on real parts are a vision/def matter, not a pairing one — but
  they hide a slip completely, which is how the original positional off-by-five
  stayed hidden. Never validate pairing on NA-only traffic.

---

# The residual is filter lag, not measurement error (2026-08-06 PM)

`pairing.resid_last_us` was assumed to be how well the clock offset is *known*.
It is not. It is how far the EWMA that tracks the offset is *behind*, and it is
governed by the sampling rate, not by any accuracy limit.

`PerifTriggerPairing.hpp:310`:

```cpp
double resid = (cam_ts_us - _q[best_i].dev_us) - _offset_us;
_offset_us += resid * 0.05;      // slow EWMA
```

An EWMA tracking a **ramp** (the crystals drift, they do not jump) has a
standing lag. With gain `a = 0.05`, gap `T` between matches, drift rate `r`, and
`N` matches arriving in a burst each revolution:

```
resid_first = r · T / (1 − (1−a)^N)          resid_last = resid_first · (1−a)^N
```

## Measured, over a 170x range in T

| condition | T (estimate age) | predicted | measured |
|---|---|---|---|
| 20 rpm, full plate | 33 ms | 9–22 µs | −20 |
| 3 rpm | 220 ms | 63 µs | −70 |
| 6.5 rpm, ONE object | 9.23 s | (r fitted here) | −2622 |
| 7 rpm, ONE object | 8.57 s | 2443 | −3408 |
| 7 rpm, train N≈13 | 7.83 s | 413 / 212 | −400 / −200 |
| 7 rpm, train N≈27 | 7.09 s | **~290** | **−290** |

One parameter, `r ≈ 21–26 µs/s`. The last row was predicted **before** it was
measured. The `N≈13` row was not assumed either — it was *derived* from the
observed first/last ratio (200/400 = 0.95^N → N = 13.5) and matched the train
that was actually on the plate.

Nothing else produces a monotone dependence on the number of objects at a fixed
speed, fixed spacing and fixed exposure window. The mechanism is settled.

## What this means

- **The floor is `r · T_gap`, and it is causal, not a precision limit.** No
  train, however long, can correct drift that accumulated *before* its first
  object arrived. Getting the first object under 100 µs at 7 rpm needs ~60% of
  the circumference filled. The true noise floor is far lower — the
  speed-independent term in the 3/20 rpm fit is about −11 µs, which is the
  camera's trigger→exposure latency.
- **Clustering objects buys the tail, not the head.** first/last is the `a`
  term; the absolute value of first is the gap term. Two different things.
- **Single-object bring-up sits on the worst point of the formula**: N smallest,
  T largest. −3408 µs is 68% of the 5000 µs window. That is a setup-mode
  artifact, not a production hazard — but during setup it is real, and it shows
  up as good frames being refused.
- **The window is doing no useful work in that regime.** With one object the
  mis-pair threshold is half a revolution (≈4.3 s); the window is 1000x
  tighter. It cannot prevent an error that cannot happen, and it can cause
  false rejections. During single-object work raise
  `cam_match_window_us` to ~15000 (cap is `min_detect_sep_us/2` = 16666) and
  **do not persist it** — production spacing needs it back at 5000.

## The recal cliff at 6.0 rpm

With one object, "no parts registered" is the revolution period, so
`cam_recal_idle_ms = 10000` puts a hard edge at **60/10 = 6.0 rpm**:

- ≤ 6.0 rpm — a recal fires every revolution, and its sync pulse feeds the core
  EWMA too (`gate_pulse == 0` triggers are paired for the clock, never
  reported), so the estimate gets a second anchor per turn.
- ≥ 6.5 rpm — the gap never reaches 10 s and recals stop entirely.

Crossing it looks like "speeding up made the residual worse", which the lag
formula alone forbids. It is not a defect, but it is invisible, and it makes any
A/B that changes plate speed near 6 rpm untrustworthy: **two variables move at
once.** Vary the object count instead — that moves N and nothing else.

## Two real fixes, neither of them recal

- **(A) scale `a` with the match rate.** Lag ∝ T/a. Cheap, but it trades noise
  rejection for bias, and outliers are exactly what causes mis-pairing.
- **(B) add a slope term.** A first-order filter must lag a ramp; a filter that
  models the ramp does not. The firmware already has `cam_drift_comp` /
  `slope_ppb` (default off); the core has no equivalent.

(B) is the better shape, but it is only safe if `r` is stable. The 7 rpm point
hinted that it is not (15 → 21 µs/s across the session, plausibly thermal). Log
`cam_sync.drift_us_per_s` over hours before fitting a slope to it — a slope term
chasing a wandering rate is worse than the lag it replaces.

## Corrections to earlier claims in this document's session

Recorded because each was stated with more confidence than it had earned:

1. "Sparse parts suppress RECAL, so the estimate ages without bound." **Wrong.**
   Every successful match feeds the EWMA, ordinary parts included; the estimate
   is never older than one gap. RECAL is not load-bearing here.
2. "Clustered samples are nearly redundant, so a second object barely helps."
   **Wrong in steady state.** Per-sample correction is 5%, but the steady-state
   residual is `r·T/(1−0.95^N)` — N=2 nearly halves it.
3. "The residual scales with the exposure window, which is defined in position."
   **Wrong.** 18 ticks is 6000 µs at 3 rpm and 900 µs at 20 rpm, against
   measured residuals of 70 and 20 µs. Off by two orders of magnitude. The
   2622 µs residual at 6.5 rpm landing near that speed's 2769 µs window was a
   coincidence, and a seductive one.
