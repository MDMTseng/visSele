# Frame ↔ object pairing: where it is and what to do next

Start here. This is the live state of the uInspESP32 bring-up as of the end of
the 2026-08-05 session, written so the next person (or the next session) can
pick it up without re-deriving anything.

---

## The one-paragraph version

The machine decides which camera frame belongs to which physical part. That
mapping used to be inferred on the host from arrival order, which is wrong by
construction and silently mis-sorts parts. It now matches on timestamps, and is
in the middle of being moved onto the ESP32 — which knew the answer all along
and was throwing it away. Everything works at 35 parts/s with zero losses.

The device-side machinery is flashed and running, but it is **not yet the
authority**: `report_match_ts` is false, so the device still acts on the tid the
core names and only *watches* its own timestamp match. Flipping that flag is the
actual handover, and it is not done — see "Tomorrow, in order".

---

## State of the machine

| | |
|---|---|
| Safe gate rate | **35 parts/s** (`min_detect_sep_us` 28571) — 5400 triggers, 0 losses |
| Practical ceiling | 40/s (0.25% loss, stops after ~286 parts at `stop_after` 2) |
| Cliff | 45/s → 6%, 50/s → 42% |
| Throughput at 35/s | 14–18 parts/s accepted, ~27% deferred by clustering |
| Parts on plate | ~45 (measured: 32.4 detections/rev × 1.40 parts/detection) |
| Plate speed | 15000 is the throughput optimum; slower is monotonically worse |
| Persisted to NVS | gate **18/s** (deliberately conservative for inspection work; the measured ceiling is 35-36/s), `unanswered_policy` 1, `unanswered_stop_after` 2 |

Physical: parts jam / stop circulating after long unattended runs. Normal
operation (vibratory feeder in, ejected at the last station) will not do this;
the current recirculating setup is roughly **8× harsher** than production.

---

## What is flashed

Everything is. Flashing needs no BOOT press since the 4k resistor (below), so
the device side is no longer a bottleneck on iteration.

Device: gate counters and `gate_disable`, `SKIP_Count`, SKIP+UNSET sharing the
consecutive-unjudged escalation, `cam_us` + `CamClockSync` (with outlier
rejection) + dual tid/timestamp matching, `trig_cam_burst`,
`trig_phantom_train`, `light`, `reboot_bootloader` (an ordinary software reboot
— it does *not* reach the ROM bootloader on this chip).

Host: `PerifTriggerPairing.hpp` (timestamp matching), gate throttle UI, pairing
stats in the panel, link RESYNC, idle heartbeat, and the dev console
(`INSP_PERIF_CONSOLE`, with `!pd` injection).

**Still off:** `report_match_ts`. The device computes the timestamp match on
every report and compares it against the tid match, but acts on the tid.

---

## 2026-08-05: the evidence arrived

The firmware is flashed and the migration's evidence base is real:

| | |
|---|---|
| Reports observed by the device | 6249 |
| Timestamp match **agreed** with tid match | 6237 |
| **Disagreed** | **0** |
| Clock residual | -12 .. -21us, against a 5000us tolerance |
| UNANSWERED / SKIP over the whole run | **0 / 0** |

~5000 real parts at plate_freq 15000, gate 35/s, ~19 parts/s accepted.

The clock offset moved **5.9ms during the run** (~24us/s) -- more than the
match tolerance -- while the residual never left +-21us. A fixed offset would
have failed inside this one run; the EWMA is doing real work, not riding luck.

**What this does and does not prove.** It proves the timestamp match is never
*worse* than the tid match. It does not yet prove it is *better*, because
better only shows up when a frame is lost, and no frame was lost in this run.
That case still needs inducing.

### The camera ignores triggers; it does not drop frames

Measured directly with on-device `trig_cam_burst` (40 pulses, mean interval
19999us at 50Hz -- the firmware generator is exact, unlike the old host-driven
attempt that smeared 10ms into 10-30ms):

| trigger rate | frames delivered | loss |
|---|---|---|
| <= 35 Hz | 40 / 40 | 0% |
| 38 Hz | 38 | 5% |
| 40 Hz | 36 | 10% |
| 45 Hz | 32 | 20% |
| 50 Hz | 29 | 27.5% |
| 60 Hz | 24 | 40% |

The ceiling is **35-36 Hz**, which lands exactly on the camera's 35.18 fps
(28425us) figure. The gate setting of 35/s is now a measured number rather than
one inferred from stop events.

Crucially, `camera dropped N frame(s)` **never fired once** through any of it.
`frameNum` only numbers buffers the camera actually delivered, so a trigger it
was too busy to service leaves no gap. The core's frame-gap compensation is
therefore blind to this failure mode -- it watches for something that does not
happen. Triggers are lost silently, and the only downstream evidence is a part
with no verdict.

This also reconciles yesterday's numbers with today's: 40/s cost 0.25% in parts
but 10% in triggers, because real parts do not arrive uniformly and the gate
spaces them. Both are right; they measure different things.

### The A/B, finally: timestamp vs positional under induced loss

Run on a **still plate with no parts**. `stepper_disable` drops the driver's
enable pin, so the glass does not turn, while the stage timer keeps advancing
and objects still travel L1A -> CAM -> SWITCH. It also gates the real sensor
path, so nothing real enters. `trig_phantom_pulse` calls `newPulseEvent`
directly, bypassing that gate, so objects can be injected at any rate.

Each trial starts from a **hard reset**: re-issuing CONNECT tears the channel
down and reopens the UART, which toggles DTR and reboots the ESP32.
`reset_running_stat` does *not* clear the CAM_SYNC counters, so without the
reboot every trial is contaminated by the previous one -- an earlier "control"
appeared to show 12 disagreements that were entirely leftovers.

| core pairing | 30/s (under the ceiling) | 45/s (over it) |
|---|---|---|
| **timestamp** | judged 601, SKIP 2, disagree 0, resid **-9us** | judged 239, SKIP 627, disagree 35, resid **-1.4ms** |
| **positional** | judged 612, SKIP 0, disagree 0, resid -157us | judged **716**, SKIP 160, disagree 61, resid **-274ms** |

**Without frame loss the two are indistinguishable.** That is why ~5000 real
parts this morning produced 0 disagreements: nothing was lost, so the two
algorithms never had an opportunity to differ. Agreement in that regime says
nothing about which is right.

**With frame loss they differ in kind, not degree.** Positional judged three
times as many parts -- because it never refuses. It pops the head of the queue
for whatever frame arrives, and after a loss the queue is offset, so those
verdicts belong to other parts. The proof is the residual: feeding the device
mismatched (object, timestamp) pairs drives its clock model to **-274ms**,
while timestamp mode stays at -1.4ms. Timestamp's 627 SKIPs are it declining to
guess.

Every verdict in these runs was NA, so a wrong assignment had no visible
consequence. In production with real OK/NG, positional's extra 477 "judged"
parts are 477 parts sorted on another part's verdict.

### The estimator was amplifying its own mistakes

`CamClockSync::observe()` folded every sample into the EWMA unconditionally.
One pair wrong by 400ms moves the offset by 400ms/16 = 25ms in a single step,
against a 5000us match window — so the next frame matches the wrong object,
which produces another bad pair. It was a positive feedback loop wearing an
averaging filter, not a starved estimator (239 reports over 20s is ~12
samples/s; starvation never explained a 381ms residual).

Samples further from the estimate than the match window are now refused before
they touch the offset, and `LOST_N` consecutive refusals abandon and rebuild it
rather than creeping toward the truth a sixteenth at a time. Same 900 objects at
45/s:

| | judged | SKIP | disagree | rejected |
|---|---|---|---|---|
| timestamp, before | 239 | 627 | 35 | — |
| timestamp, after | **491** | 381 | 19 | 51 |
| positional, after | 715 | 173 | **101** | **341** |

Judged doubles because the estimate stays usable. In positional mode the guard
refuses 341 of the pairs it is handed, and `disagree` *rises* 61→101 — the
device's clock model is no longer dragged along by positional's mistakes, so it
can finally see them. No regression under the ceiling: 30/s gives 604/604
judged, SKIP 0, disagree 0, rejected 0, resid -30us.

`reset_running_stat` clears CAM_SYNC now.

### The load is steady now

Host-paced injection made every interval a serial round trip. `trig_phantom_train
{count, hz}` schedules on-device against the absolute clock, one object per
main-loop pass so reports keep flowing. Measured on cam_trig timestamps at a
33.3ms target:

| | min | p50 | max | stdev |
|---|---|---|---|---|
| host-paced | 25.90 | 33.30 | 39.80 | 1.760ms |
| device | 32.27 | 33.33 | 34.40 | **0.121ms** |

The device's own emission intervals are looser (min 22.9ms): absolute scheduling
means one late pass is followed by a short gap as the train returns to phase.
That lateness is ~10ms — the same order as the unexplained `isr_gap_max_us` —
but it does not reach the trigger times, because object positions are in plate
pulses and the stage machine is ISR-driven, so the pipeline absorbs main-loop
jitter.

### This is the part-free test rig

The sweep above needs no parts on the plate and takes about two minutes. It is
the repeatable bench measurement that was missing. Wiring a motor pulse gate
into it would extend it from "camera ceiling" to "object pairing under
induced loss", which is the one case still unproven.

### Also today

- **`cameraFramesLeft` is gone.** It gated the entire inspection pipeline on a
  frame budget no client ever set: the WebUI never sends `frame_count`, and its
  only consumer was behind `if(false&&...)`. A headless core therefore announced
  triggers, inspected nothing, reported nothing, and the device stopped the
  machine on unanswered parts -- which reads exactly like a pairing failure and
  is not one. Cost about an hour of chasing the device before the guard turned
  up in `ImgPipeProcessCenter_imp`.
- **Dev console**: `INSP_PERIF_CONSOLE=<port>` (loopback TCP, one line of JSON
  in, device replies out). The core owns the serial port exclusively, so this is
  the only way to question the device while parts are moving. Not routed through
  the log, deliberately -- `get_running_stat` is ~1kB and the log transport
  corrupts records that long.
- **The BOOT press is not solvable in software on this chip.** See below.

## Tomorrow, in order

1. ~~One BOOT press, flash everything.~~ **Done 2026-08-05.**
2. ~~Watch `cam_sync.agree` / `cam_sync.disagree`.~~ **Done: 6237 / 0.**
3. ~~Induce frame loss with objects in the pipeline.~~ **Done** — see the A/B
   above. Positional is confidently wrong under loss; timestamp refuses.
4. ~~Find out why the device's clock residual blows up under loss.~~ **Done** —
   no outlier rejection in `observe()`; see above.

   One thing to keep from that investigation: do **not** try to feed the
   estimator from `cam_trig`. `cam_trig.t_us` and `pipeLineInfo.cam_us` are both
   the **device's** clock (`esp_timer` / the CAM ISR). The **camera's** clock
   reaches the device only as `cam_ts` inside a report, so reports are the only
   possible source for the second clock in the pair.

5. ~~Decide on promotion from the load sweep.~~ **The evidence is in** (see the
   sync-pulse section). `disagree` is now 0 at 35Hz *and* at 41Hz, which is over
   the camera ceiling with a third of the frames genuinely lost. `rejected` and
   `rebuilds` are both 0 — bad pairs no longer reach the estimator at all.

   `report_match_ts` has NOT been flipped. What is still missing is duration:
   every trial above is ~500 objects over ~15s. Promotion should follow a long
   run on real parts at production settings, not a rig.

6. **Push it to the limit — deliberately, at night.** The rig can now apply a
   steady, known load with no parts on the plate, so the overload regime is
   worth exploring properly rather than in passing. Deferred: the machine is
   needed for inspection work, and the gate is set to 18/s so the condition
   cannot arise in the meantime.
7. **Then promote**: `report_match_ts: true`.
8. **Then delete from the host** — `PerifTriggerPairing.hpp`,
   `tap_trigger_info`, `keep_clock_warm`, the trigger wait, the early dump.
   ~450 lines that exist only to reconstruct what the device already knows.

### The BOOT press: SOLVED — a 4k resistor from IO0 to GND

`pio run -t upload` now flashes with no button, repeatably.

The board is a stock ESP32-WROOM-32 DevKitC, so the two-transistor auto-reset
circuit is present; DTR->EN always worked (`rst:0x1` observed), only IO0 refused
to go low. The RTS->IO0 leg was evidently there but too weak to pull IO0 below
VIL against its pull-up. **4k from IO0 to GND shifts the operating point just
enough**: RTS asserted now crosses the threshold, while the resting level stays
above VIH so a normal boot is unaffected.

Verified: 28/28 resets started the application, 3/3 uploads succeeded with no
BOOT press. Noise margin on IO0 is by definition reduced, so if flaky boots ever
appear, this is the first thing to suspect.

Three software attempts preceded this, all measured, all dead ends — do not
retry them:

| attempt | result |
|---|---|
| `RTC_CNTL_FORCE_DOWNLOAD_BOOT` | the register does not exist on the original ESP32 (S2/S3/C3 only); absent from every SDK header |
| RTC pad hold on IO0 + `esp_restart()` | `rst:0xc` SW_CPU_RESET — does not re-latch `GPIO_STRAP`, so the pad level is irrelevant |
| RTC pad hold + `RTC_CNTL_SW_SYS_RST` | `rst:0x3` SW_RESET — straps re-latch correctly, but the hold is a deep-sleep facility and the pad is not held at latch time |

All three ended at `boot:0x13` (SPI_FAST_FLASH_BOOT); download mode is `0x03`
and the differing bit is IO0. Tying IO0 to EN also cannot work — they rise
together, so the straps latch after both are already high (tried, measured,
`boot:0x13`, and it additionally makes esptool's DTR/RTS fight each other).

`reboot_bootloader` remains in the firmware as a working software reboot; it
does not reach the ROM bootloader and nothing depends on it. `flash_no_boot.sh`
is deleted — plain `pio run -t upload` is the flashing path now.

---

## Caveats — the ones that cost time today

### The device knew all along
The camera-fire timestamp was announced and then discarded. Every host-side
mechanism built this session — bootstrap, drift EWMA, staleness sweep, TTL,
resync, early dump, idle heartbeat — is compensation for that one thrown-away
value. When a subsystem needs this much scaffolding, check whether the
information is being reconstructed rather than passed.

### Positional pairing is wrong by construction
One trigger that yields no frame offsets the FIFO **permanently**: every later
frame is then reported against the object behind it. Measured: 2596
announcements, 2591 frames, standing offset 5, object 690's image reported as
tid 685. Not late — wrong. It hid only because every verdict was NA.

### SKIP is a silencer, not a safety net
Reporting tid N marks every older unjudged object SKIP, and SKIP raises no
error. So err=2 systematically under-reports: the parts swept into SKIP are
exactly the ones that would otherwise have faulted. A machine can look clean on
errors while quietly not judging parts. `SKIP_Count` existed from the first
build and was reported nowhere.

### The clock offset has a shelf life
It only refreshes on a successful match, so idle time freezes it while the
crystals keep separating (~22ms drift per run against a 5ms tolerance). The
failure is self-sealing: stale → nothing matches → nothing updates it → never
recovers. Five idle minutes was enough to lose the next 13 frames and fault on
the second part.

### One corrupted byte kills the link until a reconnect
The framing state machine leaves its error state on exactly one thing: the
RESET_PACKET byte sequence. It does not resynchronise on a normal frame start.
So every well-formed command after a glitch is ignored. Reconnecting fixes it —
because CONNECT sends RESET — but reopening the port hard-resets the ESP32 and
ends the run. Host-side RESYNC now tries RESET alone first. **Untested: no
framing error has occurred since.**

### Runtime config does not survive a core restart
Opening the serial port toggles DTR, which resets the board, which reloads from
NVS. Anything set at runtime and not persisted is gone. This is how
`unanswered_policy` silently reverted to 0 mid-session, leaving the machine
running at 18.8% unjudged with a clean error log.

### The gate rate limit protects the camera, not the sensor
Rejections are all the time gate. The sensor resolves parts fine: `rej_dist` is
always 0, and widening `pulse_max_width` from 12.6mm to 37.7mm changes nothing.
Measured pulse-width distribution (2mm parts): 72.8% single, 19.4% two touching,
2.5% three, 5.2% more. **Merged parts are not filtered — they pass as one
object and one verdict covers both.**

### `pulse_min_width` is 0
No noise floor at all: a single 12.6µm pulse counts as a part. Now set to 120
(1.5mm), justified by the measured minimum of 2.51mm — not by the A/B that
tried to verify it, which was run while nothing was passing the sensor and
therefore proved nothing.

---

## Traps in the tooling, not the machine

These produced wrong conclusions today. All of them.

- **`insp.log` accumulates across core restarts** while each run's uptime
  restarts at 0, so a "this run only" filter on the timestamp matches every run.
  Reported 1340 triggers on an empty plate. Move the log aside before a
  measurement you intend to trust.
- **`perif trig:` prints every 100 writes.** On a low-traffic run the last line
  is minutes stale. Query the live `perif_pairing` GS item instead.
- **Long log lines get corrupted** — the record is overwritten while being read,
  and the next record appears inline with no newline. Confirmed at the producer:
  `rawL=178 strlen=178` with only ~110 characters surviving. Any long
  diagnostic may be silently truncated. **Not fixed.**
- **`matched` counted only the timestamp branch** until fixed, reading 2 when
  455 frames had paired.
- Grepping `"tid":12,"cam":1` for `[0-9]+` returns `12` and `1`.

---

## Still open

- **The A/B is inconclusive.** Timestamp vs positional has never been compared
  under real frame loss, because host-driven phantom pulses cannot make a burst
  tight enough for the camera to refuse one — every pulse is a serial round trip
  and a 10ms-spaced burst arrives 10–30ms apart. Needs a firmware-side
  generator. `cam_sync.agree/disagree` supersedes this anyway once flashed.
- **ISR gap of 10ms** (`isr_gap_max_us`), against a 33µs tick period — 300×.
  Something blocks the timer ISR for a long time; cause unknown. This should be
  understood before scaling to more cameras or adding WiFi.
- **Serial is the scaling limit, not CPU.** At 115200: 1 camera 22%, 2 cameras
  44%, 10 cameras 221%. Device-side matching removes the `cam_trig`
  announcements entirely (−64% traffic) and 921600 gives 10 cameras ~10%.
- **Multi-camera verdict aggregation** does not exist: `insp_status` is a single
  value, set once. N cameras per object needs a different shape.
- **Record + replay** of gate-pulse sequences, for reproducible tests: store
  pulse *diffs* (not µs — pulses are speed-invariant), `uint16` covers a full
  revolution at 2 bytes/event. Recording needs no firmware at all — `gate_pulse`
  is already on the wire in every `cam_trig`.
