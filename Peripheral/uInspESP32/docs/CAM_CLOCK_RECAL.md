# The camera clock, its idle top-up, and what is left to improve

Measured 2026-08-31 on the bench, real camera, plate at 10000.

## What the clock is for

The board fires the camera and the host answers with a verdict. Placing that
verdict against the right object needs the two clocks related, and that relation
is `CamClockSync::offset_us`. Get it wrong and a report lands on the wrong
part — which is the one failure the project's priority order does not tolerate:

    不可檢錯  >  best effort  >  盡量不停機

That is why `valid == false` is the only condition under which an unplaceable
report halts the machine immediately instead of being absorbed by the tolerance
counters. Everything below is about not entering that state unnecessarily.

## The offset does not need calibrating to stay fresh

Once `valid`, **any** accepted in-window report re-measures the offset outright:

```c
offset_us  = (int64_t)cam_ts - (int64_t)nearest_cam_us;   // measured, not blended
est_cam_us = nearest_cam_us;
```

So a running line maintains its own clock, for free, on every part. The eight
sample boot (`BOOT_N`, median plus majority) exists for the COLD case, where
there is no prior offset to check a sample against and the only available
evidence is that several samples agree with each other.

The idle top-up exists only because an idle line supplies no reports.

## What the top-up used to cost, and why that was the wrong trade

The original top-up entered `INSPECTION_MODE_RECAL`, which meant:

* `GATE_DISABLED = true`, feeder held off
* the drain guard — sync pulses must not be in flight beside real parts,
  because `calFireNow` bypasses the registration order
* `CAL_RESET_PENDING`, i.e. **`CAM_SYNC.reset()`**: `valid = false`, rebuild
  from eight samples

The last one is the expensive part, and not because of the eight pulses. It
opened a window, every `recal_idle_ms` of idleness, in which a single stray
report was an immediate stop — to correct 0.038 mm of drift.

## What it costs now: one stealth object

`recalService()` fires one **stealth** injected object in place. No state
transition, no reset, no shut gate, `valid` stays true throughout. The full
RECAL remains as a fallback, taken only on evidence: `recalPendingService()`
watches `est_cam_us`, which advances only on an ACCEPTED report, and falls back
if it has not moved in 4 s.

A stealth object goes through the ORDINARY gate in the ORDINARY registration
order, which is the whole reason it can be used here: it cannot collide with a
real part, it queues behind one. That is the property `calFireNow` does not
have and that all the RECAL machinery existed to compensate for.

Stealth means three things, and all three are needed for it to be honest:

| | where it is implemented | how |
|---|---|---|
| no light | `ActRegister_pipeLineInfo` | L1A and L2A stage tasks are not pushed |
| no blow | the SWITCH task body | the verdict `switch` is skipped, so no SEL task is ever queued |
| no counts | same skip | `SEL*_Count`, `NA_Count` and `CONSEC_UNANSWERED` all live in that switch |

None of them is an early exit. Nothing is queued and then suppressed, so a
stealth object does not appear as `SEL_SUPPRESSED_N` either — that counts an
actuation asked for and not delivered. The retirement block sits OUTSIDE the
switch and runs unconditionally, so the object is recycled like any other and
RBuf does not leak a slot per top-up.

CAM1/CAM2 are still pushed: the trigger is the whole point, and a timestamp does
not care about exposure. A dark frame is the correct outcome here.

## Measured, 2026-08-31

Two minutes running, then two minutes with the gate shut.

**Running** — 2503 objects at 20.9/s, 2177 judged, 287 NA:

```
殘差 -1 ~ -3 us    valid 1    拒絕 0    重建 0    回報 29 -> 35 ms
```

**Gate shut, 11 top-ups:**

```
recal 11 / ok 11 / fallback 0     every single stealth object landed
valid 1 throughout                the model was never dropped
重建 0                            no eight-sample rebuild happened
判定/NA frozen                    stealth objects contributed nothing to the tally
進料 +13 = 2 real parts in flight + 11 stealth
```

The residual across the idle intervals:

```
running     gap 0.05s   resid -1 .. -4 us
gate shut   gap 10.4s   resid -481 us   (first top-up)
thereafter              resid -477 us   -- and it does NOT grow
```

−481 µs is not error, it is the drift over the interval: measured
−45.86 µs/s × 10.4 s ≈ −477 µs. It stays there rather than accumulating because
each top-up re-measures outright. Against a 5000 µs window that is 10%, nowhere
near `LOST_N`.

Note `resid_us` is the RAW residual and is documented as diagnostic only
("this never corrects anything") — it is the INPUT to the drift estimate, not
the error left after compensation. Do not read −477 as "compensation is broken".

### An unplanned result worth keeping

Drift is measured far better from idle samples than from production ones.
During production the gap is ~0.05 s, so a ±3 µs residual divides into ±60 µs/s
of noise, and `drift_us_per_s` wanders between −26 and −90. At a 10 s gap it
settles at −45.9 consistently. An earlier impression that "drift is much higher
than expected" was reading production-gap noise.

## What is left to improve

The gap weighting is already there and is correct — this was checked before
concluding anything:

```c
num = g_ms*g_ms;  den = num + SLOPE_GAP_REF_MS*SLOPE_GAP_REF_MS;   // 2000ms
slope_ppb += (inst_ppb - slope_ppb) * num / (den * 8);
```

A 10 s sample gets weight 0.96/8 ≈ 0.12; a 50 ms sample gets ≈ 7.8e-6.
Production samples do not swamp idle ones.

The headroom is in the gain cap. Measured state after the run above:

```
slope_ppb        -48254 ppb    what the model predicts with
drift_us_per_s   -45860 ppb    what the samples say
                 ─────────
                 2394 ppb  ≈  2.4 us/s
```

Eleven independent idle measurements read −481, −477, −478, −477, −477, −478,
−477, −477, −477, −477, −477. A ±2 µs spread over a 10.4 s gap means **a single
idle sample fixes the drift to about ±0.2 µs/s** — twenty times finer than the
bias still standing in `slope_ppb`.

So: a measurement good to 0.2 µs/s is being fed to a filter whose maximum gain
is 1/8. As `g → ∞`, `den → num`, so even a perfect sample moves the estimate by
only 12.5%, and convergence needs ~20 idle samples ≈ 3.5 minutes of idle time.
That is over-damped for the quality of evidence now available.

**Cost of leaving it:** prediction error ≈ 2.4 µs/s × 10 s ≈ 24 µs, i.e. 0.5% of
the window. Uncompensated drift would be 46 µs/s and would exhaust the window in
143 s; compensation already buys a factor of twenty.

The principled fix is the one the code already names:

> The full form of this is a scalar Kalman gain, K = P/(P+R), which also tracks
> its own confidence and would damp the 2.09ppm of wander measured over half an
> hour. Not done: it needs a P state and a guessed Q, and **this shape had to be
> shown correct first**.

The run above is that demonstration. Stealth top-ups also make idle samples
*schedulable*, which is what K = P/(P+R) needs: R for an idle sample is now
known and consistent (fixed ~10 s gap, ±2 µs noise).

**Not recommended yet.** It changes the clock path, and losing the clock is the
one condition that halts immediately — the thing rule 1 is guarding. 24 µs is
not worth touching that for right now. When it is done it deserves its own pass
with an A/B measured on the DISTRIBUTION of `resid_us`, not on `delta_max_us`:
that is a since-boot high-water mark, and comparing it is what produced the
earlier false conclusion that drift compensation "showed no improvement".

## Knobs

| key | default | what it does |
|---|---|---|
| `cam.recal_idle_ms` | 10000 | idle before a top-up; 0 disables |
| `cam.drift_comp` | true | project the offset forward by `slope_ppb` |
| `cam.match_window_us` | 5000 | `TOL_US`; must stay under `min_detect_sep_us/2` |

`health.recal_stealth` / `recal_stealth_ok` / `recal_fallback` are the counters
to watch. A `recal_fallback` that climbs means the single sample is not landing,
which is a different fault from the one this change avoids.

`{"type":"trig_phantom_pulse","stealth":true}` injects one by hand.
