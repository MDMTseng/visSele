# Regression tests — what exists and how to run it (2026-08-16)

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
| `station_probe.mjs` | station block: `skip_inspection`, `ignore_calib`, region, `area_bypass` | prints one frame's station JSON |
| `calib_sticky.mjs <def>` | ignore_calib is session-scoped (AUDIT 1.3) | self-judging PASS/FAIL |
| `link_fault.mjs` | perif link chain: tx_fail → suspect → reopen → dropped_no_channel (AUDIT 1.4) | five-line trace; counters move exactly when they should |
| `slow_client.mjs` | a paused subscriber must not wedge the WS layer for others | healthy client recovers ≤ ~5s while the stuck one is still paused |
| `slow_client_sort.mjs` | verdicts keep flowing to the (fake) board during a WS wedge | board_bytes steady through the pause |
| `fd_leak.mjs <n>` | failed TCP CONNECTs leak no fds | lsof count unchanged |
| `churn.mjs [rounds]` | WS teardown under fire: waves of subscribed clients hard-destroyed mid-stream + a stalled client FIN'd mid-backpressure; fd-reuse probes assert clean first bytes; freeze-gap ≤6s asserted | RESULT line all-true, exit 0 (~60s). Optionally grep core stdout for `deferred close:` — nearly unreachable by design (subscribersLock ordering), it is a safety net, not a common path |
| `doorbell.mjs` | state doorbells end-to-end: 15s suppression (0 pkts on 0xCA11) + RC `cam_doorbell_ping` → SS/GS/SS triplet + perif doorbell (0xCA12) on REAL PD CONNECT/DISCONNECT transitions | suppression/triplet/perif all PASS (~30s, non-destructive) |
| `dv_bench.mjs <secs>` | image-stream bytes/fps, raw vs JPEG | default ~105KB/IM msg (JPEG 85) |
| `perifstat.mjs` / `caminfo.mjs` | GS readouts: `perif_pairing.link`, `camera_info.setup_failed`, `lens_calib_loaded` | eyeball |
| browser `window.__GP_PERIF_LINKS__()` | the WebUI perif link store (states + core link counters) — feeds PerifStatus | four ids registered; linkHealth mirrors perifstat |

## 3. WebUI (webctld + vite dev server + core)

| Test | Run | Covers |
|---|---|---|
| flows suite | `node flows.mjs verify` | 9 user flows: the 8 editor flows (load / select / edit / editInput / add / addArc / addMeasure / addThenDelete; `editInput` proves a REAL keystroke lands — USL edit → UCL recompute) plus **inspCycle**: recipe with tag margins → REAL menu entry into the Inspection UI (drawer / mode tag / play, with camera-reconnect-modal and WS-SPLASH-bounce handling) → asserts the tag's USL override applies in inspection and is RESTORED on exit |
| def oracle | `node golden.mjs verify caliper_verify <hydef>` | load → serialize byte-identical (what the UI sends the core wholesale) |

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
5. uInsp board-attached harnesses (~25) are separate:
   `Peripheral/uInspESP32/TESTS.md`.

## Gaps (nothing covers these today)

- Offline golden joins at FeatureMatching: frame admission, skip sizes and the
  perif path are live-probe-only.
- Margin-editor dirty check and drag reorder: store-level assertions only, no
  UI-level flow yet. (Inspection enter/exit itself is now covered by
  `inspCycle`.)
- The MinGW/Windows deploy path: syntax-checked only, never executed on a
  bench.
