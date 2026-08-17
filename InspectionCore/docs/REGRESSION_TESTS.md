# Regression tests — what exists and how to run it (2026-08-17)

One page, three layers. The offline golden gates every measurement change;
the live probes cover what `--insp` cannot see (admission, perif, WS); the
WebUI suite covers the editor. Known traps at the bottom — read them first.

## 1. Core, offline (no daemon, seconds)

| Test | Run | Covers |
|---|---|---|
| Caliper golden | `InspectionCore/run_caliper_test.sh nobuild` | caliper locate vs legacy (= GT) on the 10221 sample: line angle/offset, circle center/radius |
| `--insp` leaf-diff | `visSele --insp <img> <def> out.json`, then full-precision numeric leaf diff (filter `_ms/_us/time/seq`) | the whole measurement pipeline, bit-level. THE gate for measurement changes — byte diff false-alarms on timing, rounded diff hides drift |
| ii_dump | `UI/WebUI/tools/webctl/ii_dump.mjs <def> <img…> > out.txt` | II (INST_CHECK) path, full-precision, A/B across builds |
| pointSobel channels | `test_suite/test_pointSobel_channels.cpp` | 1ch vs 3ch of identical content must measure identically |
| legacy suite | `test_suite/suite.py`, `daemon_smoke.py`, `daemon_fuzz.py`, `migration_gate.py` | older smoke/fuzz/migration gates |

## 2. Core, live (webctl probes; core on :4090)

All in `UI/WebUI/tools/webctl/`.

| Probe | Verifies | Healthy looks like |
|---|---|---|
| `soak.mjs <def> <secs>` | CI stream rate + drops | ~25 reports/s steady on the bench |
| `phantom_feed.mjs <pps> <secs>` | feeds simulated parts (`trig_phantom_pulse`) via console 4099, no PD CONNECT | full-loop load with a real board; pair with the Inspection UI |
| `census.py <dump> [prev]` | log census by (level, file:line); second arg = diff two dumps | reads ONLY the ring section — the dump prints every line twice |
| `enter_inspection.mjs --mode 全檢` | drives the real menu into the production Inspection UI | `--mode` added 2026-08-18; 測試 is still the default |
| `logdump.mjs` | headless `SC log_dump` — writes the core's whole log ring to `latest_dump.dump` | the only way to see INFO/DEBUG from a running core (disk persist keeps WARN+) |
| `pulse_load.mjs <rate> <secs>` | drives CAM1 hardware triggers via the core's perif console (4099) | production-shaped load for per-frame paths; pair with `fi_hold.mjs` |
| `station_probe.mjs` | station block: `skip_inspection`, `ignore_calib`, region, `area_bypass` | prints one frame's station JSON |
| `calib_sticky.mjs <def>` | ignore_calib is session-scoped (AUDIT 1.3) | self-judging PASS/FAIL |
| `link_fault.mjs` | perif link chain: tx_fail → suspect → reopen → dropped_no_channel (AUDIT 1.4) | five-line trace; counters move exactly when they should |
| `slow_client.mjs` | a paused subscriber must not wedge the WS layer for others | healthy client recovers ≤ ~5s while the stuck one is still paused |
| `slow_client_sort.mjs` | verdicts keep flowing to the (fake) board during a WS wedge | board_bytes steady through the pause |
| `fd_leak.mjs <n>` | failed TCP CONNECTs leak no fds | lsof count unchanged |
| `churn.mjs [rounds]` | WS teardown under fire: waves of subscribed clients hard-destroyed mid-stream + a stalled client FIN'd mid-backpressure; fd-reuse probes assert clean first bytes; freeze-gap ≤6s asserted | RESULT line all-true, exit 0 (~60s). Optionally grep core stdout for `deferred close:` — nearly unreachable by design (subscribersLock ordering), it is a safety net, not a common path |
| `doorbell.mjs` | state doorbells end-to-end: 15s suppression (0 pkts on 0xCA11) + RC `cam_doorbell_ping` → SS/GS/SS triplet + perif doorbell (0xCA12) on REAL PD CONNECT/DISCONNECT transitions | suppression/triplet/perif all PASS (~30s, non-destructive) |
| `bpg_sweep.mjs [--include-crashers]` | 15 of the 17 handled TLs (EX + SF excluded as heavy/stateful) × {valid → reply-shape+ACK, malformed → error path, framing abuse → truncated/lying/giant headers, unknown TL, NUL-guard, multi-packet}; canonical GS liveness after EVERY case. Safe variants for the destructive TLs (SV/CI/FI/RC/PD/SC). `--include-crashers` adds the PD-no-type case (SIGSEGV'd an unpatched core; the sweep's negative control reproduced it) | 35/35 with crashers (34 without), ~30s. Far tighter reply-shape coverage than daemon_fuzz (5 TLs, HR-liveness only) |
| `dv_bench.mjs <secs>` | image-stream bytes/fps, raw vs JPEG | default ~105KB/IM msg (JPEG 85) |
| `rc_hammer.mjs` | RC `camera_ez_reconnect` + GS `camera_info` interplay: 5 rounds of reconnect + 6 GS each. Catches camera-lifecycle UAFs AND stale-build ABI mismatches (its first catch: "mutex lock failed: Invalid argument" in GetFolderName = TUs compiled against different CameraLayer.hpp layouts) | 5 rounds, 30 GS replies, exit 0, core alive (~25s) |
| `perifstat.mjs` / `caminfo.mjs` | GS readouts: `perif_pairing.link`, `camera_info.setup_failed`, `lens_calib_loaded` | eyeball |
| browser `window.__GP_PERIF_LINKS__()` | the WebUI perif link store (states + core link counters) — feeds PerifStatus | four ids registered; linkHealth mirrors perifstat |

## 3. WebUI (webctld + vite dev server + core)

| Test | Run | Covers |
|---|---|---|
| flows suite | `node flows.mjs verify` | 9 user flows: the 8 editor flows (load / select / edit / editInput / add / addArc / addMeasure / addThenDelete; `editInput` proves a REAL keystroke lands — USL edit → UCL recompute) plus **inspCycle**: recipe with tag margins → REAL menu entry into the Inspection UI (drawer / mode tag / play, with camera-reconnect-modal and WS-SPLASH-bounce handling) → asserts the tag's USL override applies in inspection and is RESTORED on exit |
| def oracle | `node golden.mjs verify caliper_verify <hydef>` | load → serialize byte-identical (what the UI sends the core wholesale) |
| mode round-trip | `node cycle.mjs [laps]` | N laps of the operator's day: editor + REAL inst-check (EX → sig360info lands) → recipe with alternating NG-range setup (tag TAGX vs none) → inspection via the real menu road → NG range in force asserted → exit → USL restored (+ the component's restore log) and the def hash stable across all laps. Catches repetition-only leaks (double-apply, SM dead ends, modal zombies). ~10s/lap; 15/15 green 2026-08-17 |

Re-baseline with `capture` ONLY after an intended behaviour change, and diff
the new snapshot by eye before committing it.

## Known traps

0. **The bench's "1006 disconnect" was never the network** (root-caused
   2026-08-16): queryCam polls camera_info ~2s; a fake camera that is not
   acquiring (trigger_mode 1 between sessions) has cam_status != 0, the app
   dispatches WS_ERROR → NOT_READY → SPLASH, and ALLOW_SOFT_CAM=false blocked
   the auto-reconnect → a ~5s SPLASH↔MAIN loop forever. Dev builds now set
   ALLOW_SOFT_CAM=true (info.js debug_SysSetting); production is unchanged.
   If SPLASH cycling ever returns, check cam_status first, not the network.
1. **The 10221 golden reads `machine_setting.json`'s `inspection_region`** —
   a leftover uInsp station region silently drops all labels (0 objects, no
   error). Remove `inspection_region`/`clean_regions` for the run, restore
   after. Bitten twice on 2026-08-15/16.
2. **Measurement baselines must come from a freshly started core** — the core
   is stateful (CORE0_1_CAVEATS §core 是有狀態的).
3. **flows/golden depend on `machine_setting.InspectionMode`** — FI auto-loads
   the machine def ~1.5s after connect and LD acts are dropped outside the
   editor. The harness works around it (settle + Edit_Mode first); if flows
   ever snapshot a def nobody asked for, suspect this first.
4. **Synthetic DOM events no-op on the per-shape PropertySheets** (and on antd
   div controls) — drive the UI with webctld's real Playwright `/fill`,
   `/click`, `/press`, never `dispatchEvent`.
4b. **Never dispatch EXIT from a separate round-trip after reading the state**
   — the app may have exited on its own in between, your EXIT lands on MAIN,
   and MAIN+EXIT → SPLASH, which only leaves on REMOTE_SYSTEM_READY (an HR =
   a reconnect): a dead end with the WS still up. Check-and-dispatch in ONE
   in-page eval (see toMain in cycle.mjs/flows.mjs), and kick the socket if
   SPLASH persists.
5. uInsp board-attached harnesses (~25) are separate:
   `Peripheral/uInspESP32/tools/TESTS.md`.
6. **After editing a CameraLayer header, make sure the build is CONSISTENT**
   (2026-08-17): a member added to `CameraLayer.hpp` shifted every field after
   it; a build that relinked without recompiling all dependent TUs produced a
   core where header-inline accessors (GetFolderName) read the wrong offset —
   crash signature `mutex lock failed: Invalid argument` on a VALID object.
   It looked exactly like a UAF and cost an hour of lldb. `rc_hammer.mjs`
   catches it in 5s; when in doubt after a header change, delete the
   affected `CMakeFiles/*.dir` objects and rebuild.
7. **doorbell.mjs phase 3's "environment" FAIL was two real bugs** (found the
   moment the real board was attached, fixed 2026-08-17): (a) the perif
   doorbell was only level-sampled at 1s — a DISCONNECT reconnected within one
   period was invisible; the PD CONNECT/DISCONNECT handlers now ring it
   event-driven; (b) a CONN_ID-less DISCONNECT always failed — the `-1`
   wildcard branch was dead code behind an earlier guard. Phase 3 passes WITH
   the real board attached now. Still true: a serial CONNECT DTR-power-cycles
   the board — don't hammer PD while a run is in progress. churn.mjs freeze
   bounds ran 29-52s (vs 16s baseline) on a loaded bench on both A/B binaries —
   re-baseline quiet before believing a regression.

## Gaps (nothing covers these today)

- Offline golden joins at FeatureMatching: frame admission, skip sizes and the
  perif path are live-probe-only.
- Margin-editor dirty check and drag reorder: store-level assertions only, no
  UI-level flow yet. (Inspection enter/exit itself is now covered by
  `inspCycle`.)
- The save-conflict dialog (on-disk sha1 changed since load → 仍要覆蓋/取消):
  the normal save path is covered by flows, but no flow drives the file
  picker, so the dialog branch is manual-verify only.
- The MinGW/Windows deploy path: syntax-checked only, never executed on a
  bench.

## Log census (how to prove a logging change did anything)

Same load, two binaries, a **fresh ring each** — the ring is a named shm segment
that survives restarts, so without a new name the dump is contaminated with
every previous generation (`CORE0_1_CAVEATS.md`).

```sh
INSP_LOG_RING_NAME=run_$$ ... ./visSele          # startup must say (created)
node fi_hold.mjs <def> &  node pulse_load.mjs 25 20
node logdump.mjs                                  # -> latest_dump.dump
# census by (level, file:line), dedupe exact repeats (dump writes disk+ephemeral)
```

2026-08-18 baseline, 20s / 488 frames / NA snapshots on:
**3982 → 494 lines (8.14 → 1.01 per frame), ERROR 1226 → 26.**
