# Regression tests — what exists and how to run it (2026-08-19)

One page, four layers. The offline golden gates every measurement change; the
live probes cover what `--insp` cannot see (admission, perif, WS); the WebUI
suite covers the editor and the operator's road into inspection; and `qa/`
holds 39 focused scripts against the app's internals. Known traps at the
bottom — read them first.

**If all you want is to run something:**

```sh
node UI/WebUI/tools/webctl/suite_nohw.mjs          # 14 probes, no camera, no board
node UI/WebUI/tools/webctl/suite_nohw.mjs --list   # just the plan, runs nothing
```

Last full run on this bench, 2026-08-19: **11 pass / 2 skip / 1 fail of 14**,
about 10 minutes. The single fail is `qa/run.mjs`, which carries its own four
(all classified, none a new defect — see `qa/SUMMARY.md`). Both skips print a
stated reason and neither is a pass.

It checks its own preconditions first and exits **2** — not 1 — when the core,
vite or webctld is down, so a red line always means the code and never the
bench. Three outcomes, kept apart on purpose: PASS asserted, **SKIP declined
for a printed reason (this is NOT a pass)**, NEEDS never ran here at all and
says what it wants. A suite reporting all-green while a third of it never ran
is worse than one reporting two thirds.

## 1. Core, offline (no daemon, seconds)

| Test | Run | Covers |
|---|---|---|
| Caliper golden | `InspectionCore/run_caliper_test.sh nobuild` | caliper locate vs legacy (= GT) on the 10221 sample: line angle/offset, circle center/radius |
| `--insp` leaf-diff | `visSele --insp <img> <def> out.json`, then full-precision numeric leaf diff (filter `_ms/_us/time/seq`) | the whole measurement pipeline, bit-level. THE gate for measurement changes — byte diff false-alarms on timing, rounded diff hides drift |
| ii_dump | `UI/WebUI/tools/webctl/ii_dump.mjs <def> <img…> > out.txt` | II (INST_CHECK) path, full-precision, A/B across builds |
| pointSobel channels | `test_suite/test_pointSobel_channels.cpp` | 1ch vs 3ch of identical content must measure identically |
| legacy suite | `test_suite/suite.py`, `daemon_smoke.py`, `daemon_fuzz.py`, `migration_gate.py` | older smoke/fuzz/migration gates. **Mac-bench only, and not merely for path reasons** — they want the 10221 golden, which is not in the repository. See the backlog entry |
| `test_suite/qa/` (9 modules) | `test_suite/qa/run_all.py` | **not runnable anywhere as it stands** — two modules point at a temporary worktree that no longer exists, the build layout is hardcoded to `build/mac-arm64`, and 5 of the 9 need the 10221 golden. Re-scoped 2026-08-19; do not "just fix the paths" |
| fmt unit | `node UI/WebUI/tools/webctl/unit_fmt.mjs` | `compactN`'s width bound, swept over every integer 0..1e6 (it shipped violating it at exactly 99999, a rounding-carry a spot check cannot see) + monotonicity + non-numeric input. No core, no browser, <1s |
| no hardcoded selector | `node UI/WebUI/tools/webctl/unit_no_hardcoded_sel.mjs` | source guard: nothing may claim NG/OK while naming SEL1/2/3 — that mapping is wiring (`cat_ng`/`cat_ok`), and hardcoding it once hid 106 real rejects and then survived its own fix in the history modal. Verified to fail on the reintroduced bug, not just to pass |

## 2. Core, live (webctl probes; core on :4090)

All in `UI/WebUI/tools/webctl/`.

| Probe | Verifies | Healthy looks like |
|---|---|---|
| `soak.mjs <def> <secs>` | CI stream rate + drops | ~25 reports/s steady on the bench |
| `qwatch.mjs <secs>` | 2Hz poll of the three queue depths (high-water marks) + the three snapshot-loss counters | proves a load did/didn't back the pipeline up |
| `phantom_feed.mjs <pps> <secs>` | feeds simulated parts (`trig_phantom_pulse`) via console 4099, no PD CONNECT | full-loop load with a real board; pair with the Inspection UI |
| `census.py <dump> [prev]` | log census by (level, file:line); second arg = diff two dumps | reads ONLY the ring section — the dump prints every line twice |
| `enter_inspection.mjs [--mode 全檢] [--def <base>]` | drives the real menu into the production Inspection UI | sequence shared with flows.mjs via `lib_enter.mjs` (2026-08-18); loads a recipe first — play does nothing without one, which is why the standalone copy had stopped working. Defaults to the checked-in `fixtures/caliper_verify_tagged` |
| `hist_wiring.mjs` | the history modal's 目前 row reads the machine's actual outlet wiring, cross-checked against the strip's own NG/OK tags | both sides publish `data-bin`/`data-sel`/`data-value`, so it asserts semantics rather than scanning label text. Verified to fail on the reintroduced SEL2 hardcoding. SKIPs (exit 0, loudly) when cat_ng/cat_ok are not declared |
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

## 2b. Bare board — no camera, no plate (core must be launched for it)

These need the core started with the synth fixture, and they will silently
measure the wrong thing without it. The multiplier is the ground truth the
whole group is built on, so `camsync_drift.mjs` takes `--log` and
refuses to run if that log does not show the multiplier it was told to expect —
an orphaned core from an earlier run holds 4099 and the serial port, the newly
launched one exits quietly, and every command then reaches the OLD process at
the OLD multiplier while looking completely normal. That happened.

```sh
# from InspectionCore/Core0_1, with /mingw64/bin and the build dir on PATH
INSP_PERIF_CONSOLE=4099 INSP_CAM_TS_SYNTH=1 INSP_CAM_TS_OFFSET_US=800 INSP_CAM_TS_MULT=1.0000833   ../build/nohik-cv4/visSele.exe > core.log 2>&1 &
```

| script | what it proves | pass looks like |
|---|---|---|
| `bareboard_up.mjs [--uart COM3] [--freq 15000]` | cold board -> INSPECTION_MODE_READY headless: PD CONNECT (once — each one reboots the ESP32 via DTR), dry run, nested `plate.freq`, `enter_insp_mode` | `READY at t+Ns` with `valid=true`. State 112 + error 14 means the synth is not on |
| `camsync_drift.mjs <secs> --rate 1 --mult M --log <core.log>` | CAMSYNC learns the drift slope, checked against the multiplier that produced it | `slope_ppb` within ~0.5% of `(M-1)*1e9`, `rejected=0`, `rebuilds=0`, state 101 throughout |
| `camsync_lost.mjs --window 200 --rate 0.2` | drift compensation is load-bearing: A/B on one board, `cam:{drift_comp}` the only variable | A survives with `delta_last_us` ~3; B halts. **Halt is error 1, not 13** — see UINSP_CAVEATS |

Rate matters and is not a throughput knob here: the slope learns by
inverse-variance weighting against `SLOPE_GAP_REF_MS = 2000`, so 1/s gives a
~40-sample time constant while 3/s gives ~300. And the error the window sees
grows with the gap, which is the only lever that works — `match_window_us`
floors at 200us and the uncompensated error at 1/s is 84us, under the floor.

## 3. WebUI (webctld + vite dev server + core)

| Test | Run | Covers |
|---|---|---|
| flows suite | `node flows.mjs verify` | 9 user flows: the 8 editor flows (load / select / edit / editInput / add / addArc / addMeasure / addThenDelete; `editInput` proves a REAL keystroke lands — USL edit → UCL recompute) plus **inspCycle**: recipe with tag margins → REAL menu entry into the Inspection UI (drawer / mode tag / play, with camera-reconnect-modal and WS-SPLASH-bounce handling) → asserts the tag's USL override applies in inspection and is RESTORED on exit |
| def oracle | `node golden.mjs verify caliper_verify <hydef>` | load → serialize byte-identical (what the UI sends the core wholesale) |
| mode round-trip | `node cycle.mjs [laps]` | N laps of the operator's day: editor + REAL inst-check (EX → sig360info lands) → recipe with alternating NG-range setup (tag TAGX vs none) → inspection via the real menu road → NG range in force asserted → exit → USL restored (+ the component's restore log) and the def hash stable across all laps. Catches repetition-only leaks (double-apply, SM dead ends, modal zombies). ~10s/lap; 15/15 green 2026-08-17 |
| play readiness | `node play_readiness.mjs` | play is enabled iff EVERY tag group the operator can SEE is satisfied. Reads `data-testid="tag-group"`'s own `data-count`/`data-min`/`data-max`/`data-fulfilled` and ANDs them, then compares against `main-play`'s `data-ready`. **Pins an open defect** (readiness is computed against the base preset, the picker renders the base preset PLUS the recipe's margin group) — it SKIPs on the current fixture, which carries one margin tag, so `maxCount:1` cannot be breached. Needs a two-margin-tag fixture to actually drive |
| no-hardware runner | `node suite_nohw.mjs [--list]` | the 14 above/below that need neither camera nor board, plus the 8 that do and are therefore listed-but-not-run. Materialises `data/BMP_carousel_test` from `fixtures/carousel/` before starting |
| shared entry sequence | `lib_enter.mjs` (imported, not run) | `makeCtl` / `toMain` / `dismissCamModal` / `loadRecipe` / `enterInspection`. The one implementation of "get the app into the Inspection UI"; it had been copied three times and two copies had rotted. Prefers `data-testid`, keeps the old heuristics as fallbacks, and logs `legacy …` when it falls back so quiet dependence on a guess stays visible |

Re-baseline with `capture` ONLY after an intended behaviour change, and diff
the new snapshot by eye before committing it.

### 3b. `qa/` — 39 focused scripts against the app's internals

`node UI/WebUI/tools/webctl/qa/run.mjs [suite …]`, catalogued in
`qa/SUMMARY.md`. A different layer from the three tables above: these drive
the **dev hooks** `script.jsx` exposes (`__GP_STORE__`, `__GP_DEF__`,
`__GP_BPG__`, `__GP_UTIL__`, `__GP_DB_QUEUE__`, `__GP_DIAG__`,
`__GP_MEASURE__`, `__GP_PERIF_LINKS__`, `__GP_LOG__`, `__GP_LOAD_BY_PATH__`)
rather than the rendered DOM, so they reach the codec, the IndexedDB queue, the
expression evaluator and the middleware — none of which any flow can see. Named
`r<round>_<topic>.mjs`; the runner serialises them because webctld owns ONE
browser, and it categorises PASS / SKIP / FAIL by exit code plus a SKIP marker
in the output.

**Full run, this bench, 2026-08-19: 32 PASS, 0 SKIP, 7 FAIL, 182 s.**

That is after the fixture fix below. Before it, the same command on the same
bench read **15 PASS, 21 SKIP, 3 FAIL, 954 s** — because 22 of the 39 suites
defaulted `WEBCTL_MODEL` to a path on one developer's Mac, and each then
misreported the failed load as `SKIP (core down)` with the core up and
answering on 4090 throughout. `qa/lib_model.mjs` now owns both the default and
the diagnosis; `diagnoseLoadFailure()` asks the page whether the core is
actually connected instead of pattern-matching an error string.

| | Before | After |
|---|---|---|
| PASS | 15 | **32** |
| SKIP | 21 | **0** |
| FAIL | 3 | 7 |
| Wall clock | 954 s | **182 s** |

**FAIL rose because five suites that had been skipping were finally allowed to
run.** Those failures were always there. None is a newly-introduced defect, and
the full classification is in `qa/SUMMARY.md`; in short: two stale tests
(`r3_serialize` S4 fires `input` without blur — TEAM_HANDOFF §9.15;
`r10_bpgfuzz` F3 asserts fields `raw2Obj_IM` has never set), one false red
(`r4_purelib` passes everything then aborts in libuv teardown), one flake
(`r6_decorator` T6, 2 of 4 runs), three that never reach the Inspection UI
because they roll their own entry sequence instead of importing
`lib_enter.mjs`, and one (`r8_matching`) that needs triage against the fixture
before the code is blamed.


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
4a. **Select by `data-testid`, never by position, geometry or label text**
   (2026-08-18). Three harnesses used to find the mode tag as "the LAST
   element reading 測試" (it appears in 製程, in 檢測方式, and as a title chip)
   and play as "the widest button in the bottom-right corner". Checked against
   a live page, that second rule resolves to the FILE BROWSER when MAIN is in
   another state — and every candidate there is an icon-only text button, so
   the wrong pick clicks silently instead of failing. Label text is no better:
   it is translated, and 測試 is three different things.
   Every hook that exists, verified against source 2026-08-19:

   | `data-testid` | Where | Publishes |
   |---|---|---|
   | `main-play` | `MAINUI.js:887` | `data-ready` 0/1, `data-reason` |
   | `cam-reconnect-skip` | `MAINUI.js:393` | — |
   | `tag-group` | `rdxComponent.jsx:614` | `data-group`, `data-count`, `data-min`, `data-max` (`''` when undefined), `data-fulfilled` |
   | `tag-option` | `rdxComponent.jsx:635` | `data-group`, `data-tag`, `data-checked` |
   | `uinsp-count` | `uInspESP32_UI.jsx:1870` | `data-bin`, `data-sel`, `data-value` |
   | `uinsp-hist-current` | `uInspESP32_UI.jsx:2175` | the 目前 row |
   | `uinsp-hist-cell` | `uInspESP32_UI.jsx:2178` | `data-bin`, `data-sel`, `data-value` |
   | `open-slid-modal` | `InspectionUI.js:2843` | — (predates this rule) |
   | `slid-bl-on` / `slid-bl-off` / `slid-em-stop` / `slid-comm-diag` / `slid-comm-diag-result` | `rdxComponent.jsx:1362-1414` | — (predates this rule) |

   The `data-*` half is the point. `main-play` alone lets you click it;
   `data-ready`+`data-reason` let you assert WHY it is or is not ready, and
   `tag-group`'s counts let `play_readiness.mjs` recompute readiness from what
   is on screen instead of trusting the same code it is testing.
   Publish the SEMANTICS, not just a handle: the assertion worth making is
   usually "the cell claiming to be NG reads the outlet the wiring says is NG",
   and the rendered digits have thrown that mapping away. If a control you need
   has no hook, add one to the component — that is cheaper than the selector
   you would otherwise write, and it cannot rot silently.
   `lib_enter.mjs` keeps the old heuristics as fallbacks and logs `legacy …`
   when it uses one, so quiet dependence on a guess stays visible.
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

8. **Windows: three things that break the suite and none of them are the code**
   (2026-08-18/19). This suite had never once passed off the Mac bench.
   (a) `core.autocrlf=true` with no `.gitattributes`: git stores LF, checks out
   CRLF, and `flows.mjs` compares that file byte-for-byte against a snapshot it
   just built with `JSON.stringify` — LF. Every flow failed over a diff whose
   two sides were character-for-character identical. Fixed by the root
   `.gitattributes` (`baseline/**` and `fixtures/**` are `-text`; **never**
   normalise a byte artifact). (b) Starting webctld killed vite: the Playwright
   persistent profile lives at `tools/webctl/.userdata`, inside vite's watch
   root, and Windows Chromium holds those files with an EXCLUSIVE lock —
   chokidar throws `EBUSY` on `Default/Network/Cookies` and takes the server
   down. macOS locks advisorily, which is the whole reason this is
   platform-specific. The watcher now skips it. (c) The harness dependencies
   had simply never been installed — `node_modules` held only `ws`, and no
   browser had been downloaded. `npm install && npx playwright install
   chromium` in `tools/webctl`.
   Also: there is no `lsof` here. Use `netstat -an | grep LISTENING`.
9. **A qa suite can print ALL PASS and still exit non-zero.**
   `qa/r4_purelib.mjs` ends with `Assertion failed: !(handle->flags &
   UV_HANDLE_CLOSING), file src\win\async.c, line 94` and exit 127 — a libuv
   teardown crash AFTER every assertion has passed. Run standalone it crashes
   4 times out of 4; run under `qa/run.mjs` it has also been seen to exit 0.
   It is a teardown race, not a stable verdict either way. `run.mjs`
   categorises on the exit code alone, so it files the crash as FAIL. **Read
   the suite's own last line before believing the summary table.**
   (An earlier version of this entry called it "deterministic across runs".
   That was two observations; four more contradicted it.)
10. **`data/BMP_carousel_test` is inside gitignored `data/`.** It is what
   stands in for a camera when nothing is attached. When it is missing, every
   stream-dependent probe fails with "observer got no stream" and nothing says
   why — it was deleted once during a cleanup and cost a debugging session.
   `suite_nohw.mjs` materialises it from `tools/webctl/fixtures/carousel/`;
   anything else you run standalone, check the folder first.
11. **`bpg_sweep`'s C11 used to fail on every single run, and it was the
   test's own arithmetic** (2026-08-18). C11 queues 133,333 packets
   deliberately and then asserted liveness with a flat 600 ms budget. The core
   answers all of them and returns in **2.26 s** — ~59k packets/s, no crash, no
   leak, deterministic, measured with `flood.mjs`. The budget is per-case now.
   A test that has always been red teaches everyone to ignore it.

12. **Three probes are intermittent, and re-running them proves nothing**
   (2026-08-19). Counted, because the counts are the whole point:

   | Probe | Standalone | Inside its runner |
   |---|---|---|
   | `doorbell.mjs` | PASS (suppression/triplet/perif all green) | 1 FAIL, then 1 PASS at the same position |
   | `r6_inspection` T1 | PASS ×2 | 1 PASS, 2 FAIL over three `qa/run.mjs` runs |
   | `r7_inspbug` T1 | PASS ×2 | 2 FAIL |

   An earlier version of this entry called it sequence coupling and blamed
   `bpg_sweep --include-crashers` running before `doorbell`. The next full run
   passed at that exact position, which kills that theory. What is left is
   plain intermittency of unknown origin; load late in a 20-minute run, the
   core's cross-session state, and webctld's single browser accumulating state
   over ~39 navigations are all still candidates, and none is established.

   The practical rule holds regardless: **a probe that fails in a runner and
   passes alone has told you it is flaky, not that it is fine.** Record both
   results. The temptation is to re-run until green and move on, which is how
   `r7_inspbug` was nearly signed off after polling alone "fixed" it — it still
   failed under `run.mjs`.


13. **`link_fault.mjs` takes the perif slot and does not give it back**
   (2026-08-19, found the first time it ran with a board attached). It does NOT
   need a real board -- it drives a fake TCP listener on 127.0.0.1:5999, and
   `suite_nohw`'s NEEDS list describing it as "a real board" is wrong. But it
   `PD CONNECT`s into the SAME conn slot the real board occupies: the
   `dropped_no_channel` counter it reports is the board's own. Run it with a
   board attached and the board is left **SUSPECT**, and it does not recover on
   its own -- observed still SUSPECT after six polls.

   Recovery is one `PD CONNECT` carrying the serial descriptor instead of the
   fake TCP one, sent from inside `tools/webctl` so `ws` resolves:

   ```js
   ws.send(frame('PD',0,pg++,{type:'CONNECT', uart_name:'COM3', baudrate:230400,
                              machine_type:'uInspESP32', cam_idx:1,
                              pairing:'timestamp', cat_ok:3, cat_ng:1}));
   ```

   Took two polls to return to CONNECTED. The probe should restore the slot
   itself; until it does, check `__GP_PERIF_LINKS__()` after running it.

   **And send that CONNECT once, not in a retry loop** (2026-08-19). A serial
   `PD CONNECT` re-opens the port, and opening the port DTR-resets the
   ESP32 -- so a script that polls `!pd` until the board answers reboots it
   on every attempt and it never gets the ~8s it needs to finish booting.
   Measured: 12 polls at 3s produced 19 `perif: link RESYNC requested`
   lines and a board that never answered at all. One CONNECT, then wait.

14. **After `clear_error`, drift compensation is off even though the flag says
    on.** `expectedCamUs()` compensates only
    `if(DRIFT_COMP && slope_n && est_cam_us)`, and the recovery zeroes
    `slope_n`. A/B tests started straight after a recovery have no control
    — the first run of `camsync_lost.mjs` halted in its *control* phase and
    read as evidence that compensation does not work. The script now refuses to
    start on `slope_n=0`; feed at ~1/s for a couple of minutes first. It is
    also true of the machine, not just the test: for minutes after an error
    recovery there is no drift compensation at all.

## Gaps (nothing covers these today)

- **`fixtures/test1.hydef` has no matching image in the repo.** It is the
  default def for the five core-side probes, and those only need a def — but
  anything wanting a def+image *pair* (`ii_dump`, `--insp`, `calib_sticky`)
  still cannot run from a clean clone. The images live in gitignored
  `Core0_1/data/` (`test1.png` 7.3MB, `test1_20260813_170712.png` 2.6MB). The
  smaller one is the same order as the already-committed
  `caliper_verify_tagged.png` (2.9MB), so committing it would complete the pair
  and cost about what the existing fixture costs.
- Offline golden joins at FeatureMatching: frame admission, skip sizes and the
  perif path are live-probe-only.
- Margin-editor dirty check and drag reorder: store-level assertions only, no
  UI-level flow yet. (Inspection enter/exit itself is now covered by
  `inspCycle`.)
- The save-conflict dialog (on-disk sha1 changed since load → 仍要覆蓋/取消):
  the normal save path is covered by flows, but no flow drives the file
  picker, so the dialog branch is manual-verify only.
- ~~The MinGW/Windows deploy path: syntax-checked only, never executed on a
  bench.~~ **No longer true (2026-08-18):** the core builds and runs under
  MSYS2/MinGW64 on the Windows bench with a real HikRobot camera, and the WebUI
  suite passes there (`flows.mjs verify` 9/9). What is still untested on
  Windows: `setThreadPriority` fails for all 7 threads (`rc=129`, no SCHED_RR)
  so they run at default policy — **latency measured here is not latency
  measured on macOS**.

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

## Trap: anything that opens the full sensor must put the crop back

`roi_full.mjs` / the calibration page / the def editor all set the runtime ROI
to `[0,0,99999,99999]`. Leave it there and the next hardware-triggered session
can kill the camera: **hardware trigger yields 0 frames while the soft trigger
(II snap) still works**, `cam_status` still reads 0 and `present` still true,
so it looks exactly like a wiring or board fault. It is not — the board's
`cam_trig` reply proves the pulse went out.

**Recovery, in this order** (2026-08-18, worked without a DeviceReset or an
unplug): `node roi_restore.mjs` → `node rc_once.mjs` → re-run the count.
Verified after recovery: 193 pulses at 20/s → 193 RP, zero loss.

`fullframe_run.sh` now restores the crop itself. Any new harness that opens the
sensor must do the same.
