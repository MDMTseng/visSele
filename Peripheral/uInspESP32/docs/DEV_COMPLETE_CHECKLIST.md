# uInspESP32 — what is left before this is dev complete

Written 2026-08-12, immediately after the 8-hour endurance soak (see
`UINSP_CAVEATS.md`, "An 8-hour soak on real parts, with the speed never still").

This is a **cut line**, not a wish list. Three companion docs already exist and
this one does not duplicate them:

| doc | what it holds |
|---|---|
| `RELIABILITY_ROADMAP.md` | the three-layer gap analysis against industry practice, and the hardware track |
| `PAIRING_MIGRATION_STATUS.md` | the frame↔object pairing handover, its evidence, and its remaining steps |
| `UINSP_CAVEATS.md` | append-only: every measurement, and every wrong reason that was corrected |

What is new here is the **ordering** and the **line**: what blocks calling the
firmware done, and what explicitly does not.

---

## The state to start from

The firmware side is in good shape and that is measured, not assumed. From the
8-hour soak — 364465 gate edges, 412.8 M step ticks, 1992 speed changes between
2998 and 10003 Hz:

- `isr_overrun_n` **0**; `pending > 0` in **0** of 8689 samples
- heap, stack high-water and ring-buffer peak **bit-identical across four
  two-hour quarters**
- `rx_crc_fail` **0** of 426840 frames
- `UNANSWERED` / `SKIP` **0 / 0** across 394040 reports
- conservation `accept + Σrej == edges` **exact** over the run's own edges

So the remaining work is not stabilisation. It is **coverage** and **four known
defects**.

---

## Tier A — blocks dev complete

### A1. The entire sort path has never been exercised

This is the largest hole and everything else is smaller than it.

Eight hours, 394k parts, and `SEL1` / `SEL2` / `SEL3` were **all zero** —
393537 `NA`. The inspection half soaked for eight hours; the sorting half did
not move once. Three mechanisms are consequently at zero coverage:

| mechanism | soak result | why it never fired |
|---|---|---|
| `act_cap` | 2324809 grows, **0** caps | needs real verdicts and a tight SEL1 window (win/pitch 1.80) |
| `SEL_SUPPRESSED` | 0 | same |
| `FREQ_TXN` / `_TIMEOUT` | 0 / 0 | nothing stages since the band was removed — the whole transaction path is now effectively dead code |

**The all-NA was never the machine.** A headless core loads no def, so it answers
NA to every part — the sorting half was not broken, it was never asked. Load the
recipe with `!fi` (`!ld` loads a def and never opens a session) and the backlight
must be on (it is driven by the board's stage tasks, so a headless rig sees black
frames). `tools/soak_verdicts.py` does both.

**Status 08-12 — the positive cases are covered.** 60 s, FI, plate 10000:

```
SEL1 245   SEL3 643   NA 204   SEL2 0    UNANSWERED 0   SKIP 0
edges 1380 == accept 1091 + Sigma rej 289     residual 0
```

The remaining three, after a run that deliberately drove speed changes and a
stop (full numbers in `UINSP_CAVEATS.md`):

| mechanism | now | what it took / what is left |
|---|---|---|
| `act_cap` | **202** ✅ | NOT the tight window this table predicted — one 9000→13000 ramp did all of it, `act_cap_max_t` 586 |
| `FREQ_TXN` | **still 0** | 44% changes in both directions with real verdicts. The deletion condition below is now MET |
| `SEL_SUPPRESSED` | still 0 | unreachable by stopping the plate — see below. Needs B6 |

**`FREQ_TXN`: delete it.** The condition this checklist set — "if a real-verdict
soak also leaves it at zero, the honest outcome is to delete the transaction
machinery" — has been tested and met. Nothing stages because `speed_band_pct` is
0 and the band was removed (`3becdfd6`), so the staging path has no trigger left.
This is now a deliberate decision with evidence behind it, not an omission.

**`SEL_SUPPRESSED` is not reachable the obvious way.** Stopping the plate does
not suppress anything: `ACT_SEL` needs `PLATE_RUNNING`, which is
`PLATE_FREQ_CURRENT > 0`, and the step timer's alarm is off at zero — so
`Run_ACTS` never runs and the blows are not suppressed, they are never reached
(the teardown discards them; `discard_stop` 34). Same unreachability as A4.

The only reachable path is `SYS_STEPPER_DISABLED` going true while the plate
still turns, which means de-energising the driver at speed — a loaded plate then
coasts and can throw parts. **Do not cover it that way.** It belongs to B6 as a
hook that makes the condition false without touching the driver.

### A2. `rej_width` is a function of plate speed — DIAGNOSED 08-12, fix open

| plate freq | edges | rej_width |
|---|---|---|
| 3000-3999 | 36449 | **5.10%** |
| 5000-5999 | 61209 | 3.53% |
| 7000-7999 | 53247 | 2.26% |
| 9000-9999 | 45635 | **1.83%** |

Monotone, 2.8× across the range. The width test is what decides whether a gate
pulse is a part at all, so a criterion that discards 5% of edges at 3000 and
1.8% at 9000 is measuring the plate, not the part. Slower plate → longer
shadow → more pulses fall outside a window that is not scaled the way the
station windows now are (`stageWidthRefFreq`, 08-11).

This is the one functional defect that changes which parts get inspected, so it
ranks above the rest of Tier A on severity.

**Measured 2026-08-12** (full numbers in `UINSP_CAVEATS.md`, "The width test
rejects at the LOW edge"). `rej_width_lo` / `rej_width_hi` and the width
distribution went in as instruments; three speeds, 60 s each, real parts:

- **`rej_width_hi` is 0 at every speed.** The "too wide" branch is closed.
- `w_mean` fits `w = W_geom + t0*f` to ±1.25 ticks: **t0 = 3.52 ms** fixed
  sensor response, **W_geom = 252.7 ticks = 3.177 mm** — a real part's shadow.

So the additive-time model is the one standing, and the correction is to take
`t0*f` off the measured width (or add it to the threshold) rather than to scale
the window proportionally.

**Still open, and deliberately not implemented yet:** `t0*f` is 10.6 ticks at
3000 against 35.2 at 10000, a ~25 tick swing on a 120 tick threshold. Whether
that accounts for a 2.8× rejection change depends on the density of the low tail
right at 120, which is not measured — and `w_min` of 44-75 says part of that
tail is probably not parts at all. **Needs a width histogram near the threshold
before the criterion is changed.** This is the item that decides which parts get
inspected; it does not get a guess.

### A3. `GATE_EDGES` is not incremented on the injected path — DONE 08-12

`injectPulseEvent()` now wraps both injectors (host phantom and the virtual
train). It increments `GATE_EDGES`, so `edges == accept + Σrej` holds
unconditionally and a residual means what it should mean again.

**Verified 08-12**, dry run + `virt_pulse` in IDLE (blocked path, no camera
needed): 671 injected edges, all counted, `accept + Σrej == edges` with a
residual of exactly **0**. Before the fix that residual would have been −671.

### A4. The injected path bypasses `PLATE_RUNNING` — guard added, but it is
### unreachable, and that is the real finding

Same helper gates injection on `PLATE_RUNNING`, attributing a block the way the
sensor path does (`stepper_off` / `dryrun` / `unstable`, in that order).
`GATE_DISABLED` is deliberately still not tested — injecting while the real
sensor is ignored is that flag's whole purpose.

**The guard cannot fire today.** `PLATE_RUNNING` is `PLATE_FREQ_CURRENT > 0`,
and the step timer's alarm is disabled at zero — so the only state in which the
guard is false is the state in which `phantomServiceISR`, which lives inside
that ISR, never executes. The original defect is real in the code and
unreachable in practice.

Kept anyway: it costs one bool, it makes the injected path state the same
precondition the sensor path states, and it becomes live the moment anything
decouples the tick source from plate speed. But it fixed no live behaviour, and
recording that is worth more than the line of code.

Note this also means **dry run is not affected** — a dry run has
`PLATE_FREQ_CURRENT > 0` with `StepGo` muted, so the plate stands still while
`PLATE_RUNNING` is true and injection proceeds. Every phantom rig keeps working;
that was checked before the guard went in, not after.

### A5. Parts discarded unattributed at stop — DONE 08-12

`RESET_ALL_PIPELINE_QUEUE()` counts live RBuf entries into `GATE_DISCARD_STOP`
before clearing (`retired==1` excluded — those are judged and merely awaiting
the drain). Reported as `gate.discard_stop`.

Confirmed on the 08-12 sweep: **17** at teardown, previously invisible. The
identity is now `accept − judged − discard_stop == what is in RBuf`.

### A6. Promote `report_match_ts`, then delete the host's 450 lines

`report_match_ts` is still **false**. The device computes the timestamp match on
every report and compares it against the tid match, then acts on the tid.

The evidence for promotion is overwhelming and now includes the soak:
`agree` 337826, `disagree` **0**, `rejected` 0, `rebuilds` 0, `cal_fails` 0,
`delta_max_us` 121 against a 5000 µs tolerance.

`PAIRING_MIGRATION_STATUS.md` set the promotion condition as "a long run on real
parts at production settings, not a rig". **The 8-hour soak is that run, with
the flag off.** So the remaining work is small and specific:

1. Re-run the same soak with `report_match_ts: true`.
2. Promote.
3. Delete from the host: `PerifTriggerPairing.hpp`, `tap_trigger_info`,
   `keep_clock_warm`, the trigger wait, the early dump. ~450 lines that exist
   only to reconstruct a value the device already announces.

Step 3 is the actual payoff and it should not be left dangling after step 2.

---

## Tier B — should land with dev complete

### B1. `match_window_us` is a position tolerance wearing a time unit

`LegacyFirmware.cpp:7838` (deferred 2026-08-06). What the window absorbs is
trigger-to-exposure jitter, which is a real **displacement** of the part in the
image. The accepted tolerance is 0.2 mm; on this machine (240 mm plate, 60000
ticks/rev, 0.01257 mm/tick) that is 796 µs at plate_freq 10000.

In use: **5000 µs = 1.26 mm at 10000, six times looser than intended.** Measured
residual across the soak was 121 µs. There is a lot of room to tighten and no
measurement arguing against it.

### B2. The auto-rate ratchet is silent

`RELIABILITY_ROADMAP.md` §"auto-rate 棘輪". Above a SKIP density of 1/50 the
recovery branch never executes and `GATE_SEP_EFF_us` walks monotonically to the
`AUTO_RATE_FLOOR_us` floor — 5/s — **with no fault, no `error_hist`, and state
`READY` throughout**. From outside, the machine is merely "slow today".

The design (fast backoff, slow recovery) is deliberate and stays. Three things
make it visible, and all three are still open:

1. alarm when `eff_sep_us` deviates from the configured value by more than X%
2. WebUI surfaces `auto_backoffs` / `auto_recovers` — the first rising while the
   second is flat is the signature of falling toward the floor
3. decide whether hitting the floor escalates to a fault (today it is silent)

### B3. The host does not verify `cfg_crc`

The firmware reports it in `get_setup`. Nothing checks it at connect. This is
the guard against NVS and host drifting apart, and the NVS version-bump incident
that wiped `io_on_level` is exactly the failure it prevents. Cheap.

### B4. Host heartbeat → safe state

A hung vision program should stop the line without needing the host's
cooperation. `max_duration` on the link, every output with a declared safe
default. Layer 3 item 2 in the roadmap; the framing/CRC half of that item is
already done and proven (0 failures in 426840 frames).

### B5. The framing resync path is still untested

`PAIRING_MIGRATION_STATUS.md` records it plainly: "Untested: no framing error has
occurred since." The soak's 0 CRC failures is good news that also means this
path still has not been walked. It needs **fault injection**, not more uptime —
which is the same tool B6 needs.

### B6. Device-side fault injection hooks

Skip a trigger, corrupt a tid, drop a frame. Mutation testing for the pipeline.
Roadmap item; it is the instrument B5 and A1's negative cases both depend on, so
it is worth building before them rather than after.

---

## Tier C — explicitly NOT blocking dev complete

Listed so they are not silently dropped, and so nobody re-litigates them.

- **Hardware supervision** — index mark, eject-confirm sensor, air-pressure
  switch, downstream parity sensor, commissioning subcommand. These are the
  roadmap's first layer and they need mechanism work. Without them the
  "tracking integrity" tier is empty, but that is a machine-level gap, not a
  firmware one. Separate track.
- **`std::string` on the comm path** — **downgraded, with evidence.** It was
  filed as the number-one killer of long-month operation via heap fragmentation.
  The soak's `free_heap` / `min_heap` were bit-identical across four two-hour
  quarters. That is a counter-measurement. Ordinary cleanup now, not a risk.
- **Power-cut injection rig, NVS wear counter, flash temperature telemetry,
  emulator in CI** — roadmap items 3–5, all real, none blocking.
- **Alpha-beta joint clock estimator** (`LegacyFirmware.cpp:415`) — offset and
  slope are estimated separately today, which lets them chase each other. The
  A/B showed the slope estimated correctly (−17.5 µs/s vs −20.6 actual) while
  `delta_max` did not improve, consistent with exactly that. But `delta_max_us`
  is 121 against a 5000 µs tolerance, so the error budget is not close to
  binding. Revisit if B1 tightens the window enough to make it bind.

---

## Parked — known, low confidence of biting, fix when it does

Found on 2026-08-12 while doing A2-A5. None of them blocks anything; they are
here so that when one does bite, the diagnosis is already written down.

1. **`INSPECTION_MODE_TEST` (state 140) is unreachable.** It opens the gate and
   turns the plate with **no CAL**, which is exactly the camera-free rig B6
   wants — and nothing in the firmware emits `ENTER_INSPECTION_TEST_MODE`, so
   the state and its transition are dead code. Serial-direct runs cannot enter
   inspection mode at all today: they halt on `CAM_CLOCK_CAL_FAILED` (err 14),
   correctly, because CAL needs a host to report frame timestamps. Making this
   state reachable is the cheapest first step B6 has.
2. **`get_running_stat` sits near a silent host-side ceiling.** Raised device
   side 3072 → 3584 after four new keys overflowed it. The binding limit is the
   HOST's: the core reads the peripheral line with `if (line.size() < 4096)`
   (`wiringPanel.cpp:6703`) and truncates past that with no device-side guard
   able to see it. The next few diagnostics fit; a batch does not.
3. **`report_match_ts` is `true` in the board's NVS** (read 08-12,
   `cfg_from_nvs: true`), while A6 below is written on it being false. One of
   the two is stale. A6's plan — re-soak, promote, delete the host's 450 lines —
   does not survive that being unresolved, so resolve it before starting A6.

## Not yet measured

Two numbers nobody has, both cheap now:

1. **The acceleration at which parts slide.** This is the mechanical limit on
   `Δf_max = √(f² + 9315·accel) − f`, and it breaks the "one tick is a fixed
   distance" assumption the whole position clock rests on. With a fixed part
   set it is easy: N is constant, so a step in `edges·60000/ticks` after a hard
   ramp **is** parts moving.
2. **Why `cam_max_us` drifted 211 → 300 ms** over the soak's last two hours. It
   is a since-boot high-water on the host path and well inside the
   CAM1→SWITCH budget (1029 ms at 10000), but it is moving, and 300 should not
   be assumed to be the ceiling.

---

## Suggested order

1. **B6** (fault injection) — it is the instrument the rest needs. Start by
   making `INSPECTION_MODE_TEST` reachable (parked item 1): it is a rig that
   needs no camera and no core, and A3/A4's verification wants exactly that.
2. ~~**A2, A3, A4, A5**~~ — A3/A4/A5 landed 08-12; A2 is diagnosed and its fix
   is waiting on one histogram. A3/A4 still need an injection run to confirm.
3. ~~**A1** — real-verdict soak.~~ Positive cases done 08-12 (`SEL1` 245 in 60 s);
   `act_cap` covered too. `FREQ_TXN`'s fate is **decided: delete**.
   `SEL_SUPPRESSED` is the one negative case left and it waits on B6.
4. **A6** — flip `report_match_ts`, re-soak, promote, delete the host's 450
   lines.
5. **B1, B2, B3, B4** — tolerance, visibility, and the two link guards.
6. **B5** falls out of B6 for free once the injector can corrupt a byte.
