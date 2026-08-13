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

### A1. The entire sort path has never been exercised — PENDING

This is the largest hole and everything else is smaller than it.

Eight hours, 394k parts, and `SEL1` / `SEL2` / `SEL3` were **all zero** —
393537 `NA`. The inspection half soaked for eight hours; the sorting half did
not move once. Three mechanisms are consequently at zero coverage:

| mechanism | soak result | why it never fired |
|---|---|---|
| `act_cap` | 2324809 grows, **0** caps | needs real verdicts and a tight SEL1 window (win/pitch 1.80) |
| `SEL_SUPPRESSED` | 0 | same |
| `FREQ_TXN` / `_TIMEOUT` | 0 / 0 | nothing stages since the band was removed — the whole transaction path is now effectively dead code |

**"All zero" is no longer true, 2026-08-13.** Short operator-driven runs moved
SEL1 108 -> 191 and SEL3 423 -> 717 against NA 445, so the sort path does
actuate and the three mechanisms above are no longer at structurally zero
coverage. This is minutes, not the eight-hour soak, so A1 stays open — but it is
now "not soaked" rather than "never fired", and the next attempt has no reason
to expect all-NA.

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
| `SEL_SUPPRESSED` | **4** ✅ | covered 08-12 with B6's `sel_suppress` + phantom objects + injected verdicts. No parts, no camera, no driver cut at speed. `tools/fault_sel_test.py` |

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

### A2. `rej_width` is a function of plate speed — DIAGNOSED 08-12, PENDING

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

### A7. One malformed frame makes the machine deaf until a power cycle

Promoted from B5 on the strength of the measurement, 2026-08-12. Full method and
numbers in `UINSP_CAVEATS.md`, "B5 answered".

Provoked with the core stopped and bytes written straight down the UART, so
there is no ambiguity about which end broke:

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

**Corrected the same day:** it IS the documented `SERIAL_PROTOCOL_ERROR` latch,
and the latch has exactly one escape — `{"type":"RESET"}`, matched out of the
raw buffer because no frame is delivered once the parser has latched. The first
conclusion ("no way back") came from trying `PING` and `clear_error`, neither of
which is the one that works.

What is actually wrong is narrower and still worth Tier A:

1. **`clear_error` cannot work here** — it is the command an operator or a
   generic host will reach for, and its handler sits downstream of the latched
   parser.
2. **Nothing recovers on its own.** No idle timeout, no resync on a delimiter.
   The core sends RESET on CONNECT, which is why a reconnect fixes it — so the
   machine stays deaf until a human reconnects.
3. **It is silent.** `error_hist` empty, `rx_crc_fail` 0, nothing in the core's
   log.

And it is silent at both ends: `error_hist` empty, `rx_crc_fail` 0 (the reset
that recovers it also clears them), nothing in the core's log. A machine that
has gone deaf this way is indistinguishable from one that is idle.

Why this is Tier A and not B: the soak's 0 CRC failures in 426840 frames says
the happy path is solid, and this is what happens the first time it is not — any
line noise, any partial write, any host that dies mid-frame. The cost is the
whole machine, silently, until someone power-cycles it. B4 (host heartbeat →
safe state) does not cover it either: the board is the deaf end, so a host-side
timeout cannot reach it.

**Action:** three small things, not a rewrite.
- ~~let `clear_error` out of the latch too~~ — DONE 2026-08-13, and it was
  half-done before in a way that read as working. `clear_error` already escaped
  the PARSER latch (matched from the raw buffer beside `RESET`), but
  `commsErrorLatched` lives in LegacyFirmware and was cleared only by
  `handleResetCommand` — so a `clear_error` while wedged answered
  `CLEAR_ERROR_OK` and every command after it was still refused with
  `serial_error_locked`. Worse than not working: it looked like it worked.
  Both latches now clear. **Verified against a real latch**, not a contrived
  one: the device wedged during a firmware upload and `clear_error` brought it
  back, follow-up commands and the PONG stream included.
- ~~count the latch, and keep the count across it~~ — DONE. `rx_latch_n` is
  incremented on the transition (not per byte) and survives `RESET`.
- decide whether an idle period should resync on its own; the argument against
  is that a latch you can silently leave is a latch that hides corruption

**Updated 2026-08-13.**

The asymmetry is the thing to argue from: a bad CRC is ALREADY forgiven — the
frame is dropped, `rx_crc_fail` counts it, the machine keeps running (0 in
426840). The latch is the path taken when the parser cannot frame the bytes at
all. On the wire those are the same event, garbage, and it is hard to defend one
of them resyncing while the other goes deaf until a human intervenes. So:
resync, in line with the CRC path.

But resync ALONE converts this from a visible failure into an invisible one,
which is the corruption-hiding objection landing rather than being answered.
Forgiveness plus a counter nobody reads IS the hidden corruption. It needs a
threshold that escalates: one latch in a window is line noise, a run of them is
hardware, and those two must not look alike. `rx_latch_n` exists to build that
on; nothing consumes it yet.

Also: this item's "silent at both ends" is partly an artefact of something else.
`rx_latch_n` survives `RESET` — but the recovery people actually used was a
reconnect, and until 08-13 a reconnect reopened the serial port, which pulses
DTR/RTS and rebooted the board, zeroing every RAM counter including this one.
Now that CONNECT reuses a live channel, the evidence survives the recovery. A7
should be easier to observe than the original write-up implies.

Reproduced independently 2026-08-13, by accident: a bare `?` typed at the perif
console is forwarded verbatim to the device, latched it (`serial_error_locked`),
and `clear_error` did not clear it — only `{"type":"RESET"}` did. Exactly as
described, including the part where the obvious command is the wrong one.

### A6. `report_match_ts` PROMOTED — the host's 450 lines are what is left

**Steps 1 and 2 are done.** The paragraph that stood here said "`report_match_ts`
is still false", and that was stale: the board reads `report_match_ts: true`
from NVS with `cfg_from_nvs: true`, and has been running that way. The timestamp
match is the authority, not a shadow calculation compared against the tid.

The evidence that justified it, from the 8-hour soak with the flag OFF:
`agree` 337826, `disagree` **0**, `rejected` 0, `rebuilds` 0, `cal_fails` 0,
`delta_max_us` 121 against a 5000 µs tolerance — a long run on real parts at
production settings, which is the condition `PAIRING_MIGRATION_STATUS.md` set.

**`report_match_pcnt` is false, deliberately.** The camera's own trigger count
was the third candidate mechanism and it was measured unreliable — see
`6c88be34`, "ask the picture which mechanism is right, and it says pcnt is not".
Trigger-count numbers are not a second opinion worth having; the timestamp is.

**Step 3 is untouched and is now the whole item.** The host still compiles its
own pairing: `PerifTriggerPairing.hpp` is 645 lines and `PERIF_CORE_PAIRING` is
still `1`, with 21 conditional sites in `wiringPanel.cpp` — `tap_trigger_info`,
`keep_clock_warm`, the trigger wait, the early dump. All of it exists to
reconstruct a value the device now announces outright.

This is the payoff, and leaving it in place costs more than the lines: every one
of those sites is a second implementation of the thing that was just promoted,
so the two can disagree and the disagreement will look like a machine fault.

---

## IO safe mode — the outputs have no default

Added 2026-08-13. The eight actuator pins are **not** configured as outputs at
boot. They are left as inputs until a config has been read that says what ON
means on this machine.

**Why there is no default.** The compiled default is `IO_INV_MASK =
1<<IOI_FEEDER` — FEEDER active-low, the other seven active-high — and this
machine is active-low on all eight. So a board on compiled defaults has seven of
its eight outputs inverted, and inverted means energised. That is exactly how
the light and the air blow once switched themselves on with parts on the plate.

Storing the config as wire JSON fixed the version bump that caused it, but not
the shape of the failure: JSON's rule is that an **absent key keeps its compiled
default**, and for output polarity the compiled default is the opposite of the
truth. A renamed key, a dropped key, a future firmware whose table has nine
entries — all land back on the same defaults, and all look like a working
machine.

**Why high impedance is the safe state, measured 2026-08-13.** The driver inputs
are common-anode opto: the GPIO must sink for the output to energise, so an
input pin is no path and no actuation. Confirmed on the SEL valves rather than
assumed — an undriven pin reads inconsistently (`[0,0,0]` at boot, `[1,0,1]`
later, which is what floating looks like), but `INPUT_PULLUP` takes all three to
1 through the internal ~45kΩ. Nothing external holds them low; a conducting opto
could not be pulled up by 45kΩ. None of the eight is a strapping pin, so this is
also the state every reset already passes through.

**The rules.**

| | |
|---|---|
| checked | `io_on_level` present, all 8 names from `IO_POL_TAB`, each 0 or 1, no extras |
| against | the stored/incoming **document**, never the globals — the globals always hold a mask, and what they hold when a key is absent is the failure being guarded |
| on fail | pins stay inputs; `enter_insp_mode` refused with `io_not_configured`; `get_setup` reports `io_armed:false` and `io_safe_why` |
| way out | a `set_setup` carrying a complete valid `io_on_level`. Not persisted — a reboot returns to safe mode unless the operator also saves |
| legacy blob | armed if `version >= 4`, when `io_inv_mask` is inside the trusted prefix; older is refused |

`get_schema` reports the required block. It answers the direction
`cfgUnknownKeys` cannot: that one says "you sent me keys I do not recognise",
which is the harmless case, since an unknown key is named back and ignored. The
dangerous direction is silent — a key this firmware expects that the stored
config does not carry does not fail, it defaults.

**Verified by simulating the real failure.** A test build renamed `SEL3` to
`SEL3B` in `IO_POL_TAB` — a key rename, the failure mode that defaults
silently — and the board came up `io_armed:false`, `io_safe_why:
"io_on_level.SEL3B missing"`, with `get_setup` showing `SEL3B: 1` in plain
sight: the renamed output sitting at the compiled default, which on this machine
is inverted. `enter_insp_mode` was refused. A `set_setup` with a conforming
`io_on_level` armed it and the machine ran. Note that a broken stored config
**cannot be produced through the API** — the firmware always writes a complete
`io_on_level` — so the firmware side is what has to be simulated.

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

### ~~B2. The auto-rate ratchet is silent~~ — CLOSED 2026-08-12, feature removed

The ratchet was real: above a SKIP density of 1/50 the recovery branch never
executed and `GATE_SEP_EFF_us` walked monotonically to its 5/s floor with no
fault and state `READY` throughout. Three ways to make it visible were open.

None were built, because the feature could not do its job. It backed the gate
off to "admit fewer parts than we can judge" — but **the feed rate is set by
the vibratory bowl, and a part refused at the gate is not removed**. It stays on
the plate and returns next lap. So the loop shed no load; it deferred the same
parts while running the machine at a second, drifting rate the operator never
typed and could not see. The silent ratchet was the visible symptom of that.

Removed: `AUTO_RATE*`, `GATE_SEP_EFF_us`, both AIMD functions, six `get_running_stat`
keys, two `skip_policy` tuning keys, and the "自動放慢進料" switch. The gate now
enforces `SYS_MIN_PULSE_TIME_SEP_us` directly — one rate, the configured one.

The **stop** half is untouched and remains the guard: it reacts to CONSECUTIVE
skips, which is the failure slowing down could never fix. `skip_policy.mode` is
now `stop_only` | `none`; the firmware still parses the four older spellings, so
an existing NVS image carries its stop setting across (verified on this machine:
`slow_and_stop` → `stop_only`, `stop_after` 10 preserved).

Verified after removal: the rate gate still refuses at the configured threshold
(28.2 ms offered against a 28.571 ms gate → `rej_rate` 29 vs `accept` 28, exact
alternation, since a refused pulse does not advance `_preTime`), and 30 s at
35.2 ms ran 1056 edges / 758 accepted with `error_hist` empty and state 101.

### B3. The host does not verify `cfg_crc`

The firmware reports it in `get_setup`. Nothing checks it at connect. This is
the guard against NVS and host drifting apart, and the NVS version-bump incident
that wiped `io_on_level` is exactly the failure it prevents. Cheap.

### B4. ~~Host heartbeat → safe state~~ — DONE 2026-08-13

A hung vision program should stop the line without needing the host's
cooperation. Built: the core sends a 10/s idle ping (only when the link is
quiet), `host_timeout_ms` (500 here) turns its absence into
`HOST_LINK_TIMEOUT`, and that stops the plate and saves the counters to NVS.

Two things it needed that were not obvious:

- The watchdog is licensed by an explicit runtime flag (`comm_lost_backup`),
  false on every boot and armed by the host's own heartbeat — not by the stored
  `host_timeout_ms`, or a board that had never met a core would stop itself on
  the bench.
- It had to cover every state where parts move, not just READY. RECAL is
  entered automatically whenever the pipeline empties, and killing the host
  there did nothing at all until 08-13.

**A7 still is not covered by this, exactly as A7 predicted**: if the parser has
latched, the ping cannot get in either, so the arming never happens and the
timeout never fires. The deaf-board case needs A7's own fix.

### B5. ~~The framing resync path is still untested~~ — TESTED 08-12, FAILS.
### Promoted to A7.

### B6. Device-side fault injection hooks — DONE 08-12

`{"type":"fault", ...}`, all bounded at 1000, all reported in
`get_running_stat.fault` (absent when nothing is armed — an invisible armed
injector is what A3 was), none cleared by `reset_running_stat`.

| knob | what it does | what it is for |
|---|---|---|
| `sel_suppress:N` | fails the ACT_SEL guard for N actuations | `SEL_SUPPRESSED`, A1's last uncovered mechanism. The only path a real machine offers is de-energising the driver at speed, which throws parts |
| `skip_trig:N` | swallows a trigger whole — no pulse, no `CAM_PULSE_N`, no announcement | a part that never produced a frame |
| `tid_n:N` + `tid_offset:X` | lies about the tid **in the announcement only** | the pairing mutation. `disagree` has read 0 for want of any way to move it |

It found A7 the day it was built. Still to run with parts flowing:
`sel_suppress` against `SEL_SUPPRESSED`, and `tid_offset` against `disagree`.

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
- ~~**Alpha-beta joint clock estimator**~~ — **CLOSED 08-12, measured.** The
  premise was that estimating offset and slope separately leaves recoverable
  error. `virt_pulse` at exact 2/4/8 s periods gives |delta| mean 0.54 / 0.39 /
  0.59 us — flat, so the residual is a per-sample constant at the 1 us timestamp
  granularity, not a fractional slope error. There is nothing for a joint
  estimator to recover. Reopen only if B1 tightens the window enough for a
  microsecond to matter.
- **The slope's 1-second learning threshold is a cliff** (new, 08-12). It only
  learns from gaps >= 1 s, so traffic at just under that never teaches it: a
  0.94 s block left the estimate stale and |delta| 8x worse. About one part per
  second lands in the hole. Cost measured at 3.84 us against a 5000 us window,
  so not urgent — but it wants a taper, not a threshold.

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
3. ~~**`report_match_ts` is `true` in the board's NVS** while A6 is written on
   it being false.~~ RESOLVED 08-12: the board was right and the checklist was
   stale. A6 is rewritten; only its step 3 remains.

## Not yet measured

1. **The acceleration at which parts slide.** This is the mechanical limit on
   `Δf_max = √(f² + 9315·accel) − f`, and it breaks the "one tick is a fixed
   distance" assumption the whole position clock rests on. With a fixed part
   set it is easy: N is constant, so a step in `edges·pulses_per_rev/ticks`
   after a hard ramp **is** parts moving.

   **Still open.** An arm-speed sweep on 08-12 looked like it had measured this
   and had not: no repeatability baseline (n=1 per speed), stepper step-loss
   never ruled out, and the plate itself turned out to be loose. See
   `UINSP_CAVEATS.md`, "Three claims from this afternoon that did NOT survive".
   Whatever measures this needs a fiducial fixed to the PLATE, so the part and
   the plate can be told apart.
2. **Why `cam_max_us` drifted 211 → 300 ms** over the soak's last two hours. It
   is a since-boot high-water on the host path and well inside the
   CAM1→SWITCH budget (1029 ms at 10000), but it is moving, and 300 should not
   be assumed to be the ceiling.

---

## Suggested order

1. ~~**B6** (fault injection)~~ — DONE 08-12, and it answered B5 immediately.
   Its remaining runs need parts flowing.

0. **A7** — one malformed frame takes the machine deaf until a power cycle, and
   nothing anywhere records it. This is now the first thing on the list: it is
   cheap to trigger, impossible to diagnose from outside, and costs the whole
   machine.

   Placement is no longer part of this: `jog_arm` / `jog offset:N` / `jog_end`
   turn the plate into a positioner and hand back the number to paste into a
   station (0.05 mm for the move, 0.21 mm including the catch). It needs no
   camera and no core either, so it is also the cheapest existing example of
   the rig B6 wants.
2. ~~**A2, A3, A4, A5**~~ — A3/A4/A5 landed 08-12; A2 is diagnosed and its fix
   is waiting on one histogram. A3/A4 still need an injection run to confirm.
3. ~~**A1** — real-verdict soak.~~ Positive cases done 08-12 (`SEL1` 245 in 60 s);
   `act_cap` covered too. `FREQ_TXN`'s fate is **decided: delete**.
   `SEL_SUPPRESSED` is the one negative case left and it waits on B6.
4. **A6** — flip `report_match_ts`, re-soak, promote, delete the host's 450
   lines.
5. **B1, B3, B4** — tolerance and the two link guards. (**B2** closed by
   deleting the feature: the bowl feeder sets the rate, so the gate could not
   shed load. Operator decision, 08-12.)
6. **B5** falls out of B6 for free once the injector can corrupt a byte.

## A8. Opening the serial port reboots the board — half fixed, half cannot be

Added 2026-08-13. Not on this list before, and it belongs in Tier A on severity
alone: an everyday action power-cycles a running machine.

Anything that **opens** `/dev/cu.usbserial-0001` pulses DTR/RTS, which on this
dev board is wired to EN. Signature: `reset_reason_name` `POWERON`, `uptime_s`
restarting.

- **Browser refresh** — the WebUI sends PD `CONNECT`, the core tore the channel
  down and reopened the port: **uptime 611s -> 1s**, plate stopped, everything
  in flight lost. **Fixed** — CONNECT now reuses a channel whose `conn_desc`
  matches, and sends no RESET on that path.
- **Closing** the port is harmless (`c_cflag &= ~HUPCL`); a `kill -9` core left
  the board running for the 12s nobody held the port.
- **Core restart still reboots it, and userspace cannot stop it.** Clearing
  DTR|RTS immediately after `open()` was implemented and measured to change
  nothing — the pulse is inside `open()`, before any ioctl lands. Reverted; do
  not spend the afternoon on it again. Only a hardware change (cap on EN, or
  cutting the auto-reset traces) removes it, at the cost of manual BOOT for
  flashing.

The counter backup (B4) exists because of this last bullet: the machine cannot
prevent the reset, so it saves before it. See UINSP_CAVEATS for why the save
hangs off state entry rather than the comm timeout.

## Future work — an NA must say why

Noted 2026-08-13, not scheduled.

Today an NA is a verdict with no reason attached, at every level, and the
cost is that diagnosing one is guesswork. Found the hard way twice in one
session:

- The def editor's CHECK "did not snap". The object had been dropped by the
  working region before any measurement ran, and the only evidence anywhere
  was one `LOGI` line that happened to be read. Fixed by reporting
  `region_dropped` — but that is one reason out of many, added because
  somebody hit it.
- A whole eight-hour soak came back 393537 NA and it took a separate
  investigation to learn the def had never loaded (`!ld` silently does
  nothing). `match ~0.004ms` was the only hint, and it reads as "cheap
  inspection" rather than "no inspection".

The shape of the fix: every path that produces NA already knows why it did --
outside the region, no object found, fit below min_inliers, edge below
min_strength, no def, unanswered at the selector. That reason should ride
with the verdict to the report and to the UI, not be reconstructible only
from logs that are off by default (see the logging defects in
CORE0_1_CAVEATS).

Worth doing when NA rates next need explaining on a live line, which is
where the guessing is most expensive.
