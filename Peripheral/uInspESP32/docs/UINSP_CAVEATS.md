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
