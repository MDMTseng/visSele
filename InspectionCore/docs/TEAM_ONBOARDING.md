# Core0_1 + WebUI Team Onboarding

You're working on a **factory-deployed machine vision inspection system**. There are two coordinated processes:

```
              ┌──────────────────────┐
              │  visSele (C++ core)  │   ── port 4090 (BPG WebSocket: control + image stream)
              │   = "Core0_1"        │   ── port 4091 (log WebSocket, opt-in)
              │  build/mac-arm64/    │
              └─────────┬────────────┘
                        │ shm
                        │
              ┌─────────┴────────────┐
              │  inspd_log (drainer) │   ── sibling process, log fan-out + crash dump
              └──────────────────────┘

                  ┌─────────────────┐
                  │  WebUI (React)  │   ── connects to both
                  │  UI/WebUI/      │     :4090 (BPG)  for live ops
                  └─────────────────┘     :4091 (log)  for the Core Logs panel
```

This doc is a 30-day map. Read it once, skim it weekly, deep-dive into the linked detail docs when you hit the relevant topic.

---

## 1. What's actually here, and why

### Core0_1 (1st generation)

`InspectionCore/Core0_1/` — the **factory-deployed** inspection core. C++17. Runs on hardware (line scan / area cameras), receives images, runs the matching pipeline (`MatchingEngine/`), publishes results over the BPG WebSocket to the WebUI.

**Rule #1 for Core0_1:** *backward compatibility is the law*. Already-shipped factory configs (`*.hydef`) must continue to produce identical reports. Improvements are opt-in via new def fields, not behaviour swaps.

### CoreHub (next generation)

`InspectionCore/CoreHub/` — newer architecture in development. Pulls proven features back from Core0_1 (remap + sobel + topmost search-point e.g.). **Do not** assume Core0_1 changes flow to CoreHub or vice versa; they're separate codebases with selective transplants.

### WebUI (1st generation)

`UI/WebUI/` — React + Redux app shipped with the factory. This is what operators see. Active rework happens on branches like `webui/editor-refactor`. There's also `UI/WebUI2/` which is the new editor (TypeScript, partial migration — don't confuse them).

### inspd_log (the log drainer)

`InspectionCore/logctrl/inspd_log_main.cpp` — sibling process spawned by `visSele` when `INSP_LOG_DAEMON=1`. Owns the rotating disk log, the WS port for the WebUI's "Core Logs" panel, and the symbolicated crash dump. Lives outside the producer's crash blast radius. See `LOGGING_WEBUI.md` for the wire contract.

---

## 2. Day 1: get something running

### Prerequisites (macOS arm64 dev box)

```
brew install cmake ninja opencv  # opencv 4.x, full Homebrew install
# DO NOT link opencv_dnn -- it's broken on this setup; see CAVEATS A2
```

Windows deploy uses MSYS2-MinGW. See `project_build_toolchain_notes` (in `MEMORY.md`) for known fixes.

### Build

```
cd InspectionCore
mkdir -p build/mac-arm64 && cd build/mac-arm64
cmake -G Ninja -DFEATURE_OPENCV=ON ../..
cmake --build . -j8
```

Artifacts: `build/mac-arm64/visSele` (the core) and `build/mac-arm64/inspd_log` (the drainer).

### Run headless inspection (the fastest dev loop)

```
cd InspectionCore/Core0_1   # ⚠ matters — see CAVEATS B1
../build/mac-arm64/visSele --insp <image.png> <def.hydef> <out.json>
```

This loads camera calibration via the **relative path** `data/default_camera_param.json`. Wrong cwd ⇒ every line/circle fit returns NaN. Hard-won trap; written up in CAVEATS B1.

For repeat-loop profiling: `INSP_LOOP_N=300 ./visSele --insp ...` runs the inspection 300× in-process so `sample` / `xctrace` can attach.

### Run the full app

```
cd InspectionCore/Core0_1
DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele
```

Then open the WebUI (your dev server — `cd UI/WebUI && npm run dev` or whatever your team uses). It connects to `ws://localhost:4090` automatically.

### Turn on logs (Core Logs panel)

```
INSP_LOG_DAEMON=1 INSP_LOG_WS_PORT=4091 INSP_LOG_PERSIST_LEVEL=info \
DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele
```

Then click **Core Logs** in the WebUI (next to "下載診斷紀錄 / Download Diagnostics"). Drawer auto-connects to `ws://127.0.0.1:4091/log` with subprotocol `inspd_log.v1`.

### When something feels weird, check first:

| Symptom | Likely cause | Reference |
|---|---|---|
| New feature wrong in live app, correct in `--insp` | Stale core process still running | CAVEATS B2 |
| All fits return NaN | wrong cwd for `--insp` | CAVEATS B1 |
| Core Logs panel won't connect | drainer crashed after producer went idle; or stale mock on :4091 | LOGGING_WEBUI.md §6, recent fix `5fc8b106` |
| `inspection_downsample` doesn't speed anything up | def doesn't set it | this doc §5 |
| Build links fine but runtime crashes in `dnn` | linked opencv_dnn | CAVEATS A2 |
| WebUI shows hello but no log lines | drainer alive, producer idle (no LOG_* emits to drain). Open another tab in the app, trigger an inspection. |

---

## 3. The inspection pipeline (mental model)

```
  cv::Mat src
       │
       ▼
  binary_processing_group  ── input crop + downsample (def: inspection_downsample)
       │                      labeling: cv::connectedComponentsWithStats
       │                      per-label signature build (contour OR edge_sig)
       │                      signature matching against templates
       ▼
  sig360_circle_line       ── per primitive (line / circle / search point):
       │                       • locating == 1  → caliper (image-space ray sweep)
       │                       • locating != 1  → legacy contour pre-pass
       │                      → TreeExecution drives per-primitive measurement
       ▼
  FeatureReport            ── JSON via cJSON (see CAVEATS for precision tweak)
       │
       ▼
  BPG broadcast / disk
```

Three concepts to memorize:

1. **The def (`*.hydef`)** is JSON: a `binary_processing_group` containing `featureSet[]` with primitives (line, circle, search point, etc.). Each primitive has a `locating` field — set `1` for caliper path (new, accurate, ~sub-pixel), other values for the legacy contour path (still supported for back-compat).

2. **`bacpac`** (Backpack) is the per-inspection state: calibration map, sampler, settings. Don't try to pass it around as a singleton; it's deliberately scoped.

3. **`acv_*` vs `cv::*`** — `acvImage` is the legacy image type, gradually being replaced by `cv::Mat`. Mixed code is the current state. See `OPENCV_MIGRATION.md` for the migration plan and `CvBridge` for the adapter. The acv geometry types (`acv_XY`, `acv_Line`, `acv_Circle`, `acv_LineFit`) are still load-bearing and changing them affects on-disk schema. Don't replace them casually.

---

## 4. The matching pipeline's hot spots (current performance profile)

Profiled with `xctrace` on `caliper_verify.hydef` (4 primitives) at static-init time, 21 ms / inspection on M-series (commit `afb5a2d9` baseline):

| Stage | Share | What |
|---|---|---|
| `connectedComponentsWithStats` (labeling) | ~45% | Bolelli parallel. Dominant on small inspections. |
| `sig360_circle_line` matching | ~38% | per-primitive caliper + circle/line fits |
| `factorSampling` (stage light correction) | ~13% leaf | bilinear interp; called per caliper sample |
| cJSON output | ~7% | report serialization |
| TBB worker idle/spin | ~87% of total samples | parallelism is short-lived; workers spin waiting |

Two cheap wins for production:
- **`"inspection_downsample": 2`** on the def → ~3× speedup, ~0.025 mm² accuracy cost on area fields. **Tolerance call** — confirm against your golden.
- Pre-pass skip for circle-caliper mode landed in `afb5a2d9` — already there.

Real next moves if you need more: parallelize independent caliper sweeps, ROI-crop before labeling, batch `factorSampling` for vectorization.

---

## 5. WebUI ↔ Core communication

### `:4090` — BPG (live ops)

The classic. Image stream, inspection commands, parameter edits. See `BPG_Protocol/` for the wire format. Single long-running connection per client; if the core crashes the connection dies.

### `:4091` — `inspd_log` (logs only, opt-in)

Added recently (Phase F.2 in this repo's history). Subprotocol `inspd_log.v1`. JSON shape follows **OpenTelemetry Logs Data Model** so a standard otel-collector can scrape it with zero adapter code. Full schema: `docs/LOGGING_WEBUI.md`.

WS messages worth knowing:
- `hello` (server-initiated on connect): version, ring stats, resource block
- `log` / `logBatch`: live tail entries (`severityNumber`, `severityText`, `body`, `attributes.{module, code.*}`, `timeUnixNano`)
- `backlogChunk`: replay of last N from the shm ring
- `crash`: producer died, drainer captured stack + path to dump file
- `dropped`: backpressure marker (don't silently skip; render a gap)
- `setLevel` (client → server): adjust producer's log threshold at runtime
- `getModules` (client → server): fetch registered LOG_MODULE names + effective levels for the panel's module tree

The drainer survives the producer's crash so the WebUI can show the post-mortem.

**Why two ports?** If `visSele` crashes, its `:4090` WS dies with it — exactly when you most need the last log lines + stack trace. The drainer is a sibling process that lives through the crash and serves the dump. Worth keeping the architectural split.

---

## 6. The recent log system overhaul (so you understand "why so many moving parts")

Logs used to be `printf` → stdout. That's gone through five phases:

| Phase | What | Where |
|---|---|---|
| A  | `logctrl` lib: levels, modules, mutex, env config, sink registry | `logctrl/logctrl.cpp` |
| A2 | shm ring buffer producer sink (16 MB, 65k slots × 256 B) | `log_ring.h` |
| B  | `LOG_MODULE("cam.bmp")` annotations across the top 30 noisy files | grep `LOG_MODULE` |
| F.1 | Sibling drainer process `inspd_log` (rolling disk + ephemeral RAM tier) | `logctrl/inspd_log_main.cpp` |
| F.2 | WebSocket server `:4091/log` + OTel-shaped JSON | `logctrl/inspd_log_ws.cpp` |
| G  | Crash handler + symbolicated dump (SIGSEGV/ABRT/FPE/BUS / SEH) | `logctrl/log_crash_*.cpp` |
| #29 | Drainer→producer control IPC (live `setLevel` from WS) | extension in `log_ring.h` header pad |
| #30 | Module registry export via separate shm region | `log_modules_region.h` |

What's still **TODO** (visible to a new team):
- **Phase C**: replace ~270 raw `printf` callsites with `LOG*` macros. Mechanical.
- **Phase D**: demote per-feature `LOGI` chatter to `LOGD/LOGV`. `match.sig360` alone emits ~700 INFO/inspection. Audit + demote.
- **Phase E doc**: producer-side `LOGGING.md` (WebUI-facing doc already shipped: `LOGGING_WEBUI.md`).
- **Mac symbolication**: crash dump frames show `?` on Mach-O without `-rdynamic` / `dladdr` fallback. Tracked.
- **Windows parent-death detection**: drainer uses `getppid()==1` on POSIX (clean); Windows uses heartbeat (works but coarser; should be Job Object).

---

## 7. The "do not touch without thinking" list

These have specific reasons and burned us before. Read the linked context before changing.

### a. Backward compatibility for shipped `.hydef`s
Production cells run defs frozen at deployment. Adding new behaviour ⇒ new opt-in field. Changing default behaviour ⇒ field with explicit version (`matching_version: 2`). **Never** change a default that affects existing reports.

### b. `acv_*` POD geometry types
`acv_XY`, `acv_Line`, `acv_Circle`, `acv_LineFit` are reflected in on-disk JSON and BPG wire format. The OpenCV migration plan converts them gradually with bridges. Don't rename or restructure them without coordinating with the BPG / WebUI side.

### c. `g_log_t0` static-init clock
`logctrl.cpp`'s timestamp anchor is `steady_clock::now()` at static-init time, which is **not the same** as the producer's `clock_gettime(CLOCK_REALTIME)` at shm-attach time. There's a small constant offset between per-line `tsMsSinceStart` and absolute `timeUnixNano`. Documented. If you "fix" this, validate that no downstream consumer relies on the current alignment.

### d. cJSON `%1.15g` (drop the `%1.17g` fallback)
Patched in `afb5a2d9`. Upstream cJSON sscanf-roundtrips every number; we skip that. 15 significant digits is plenty for measurements. **Don't restore the upstream code path** — it doubles the JSON serialization cost.

### e. Slot format in the log ring (`LogSlot` 256B, `LogRingHeader` 512B)
The header's pad is the IPC scratchpad (`ctrl_cmd_*`, `producer_started_unix_nano`). Adding fields = check the static_assert still holds. Changing slot size = bump `LOG_RING_VERSION` and update both producer and drainer in lockstep.

### f. Subprotocol echo in cwebsocket
Patched in `200bfb68`. Upstream rejected any client that asked for `Sec-WebSocket-Protocol`. We capture + echo. Don't restore the upstream rejection behaviour.

### g. Working directory for `--insp` and the live app
Calibration loads via the relative path `data/default_camera_param.json`. Run from `Core0_1/`. CAVEATS B1 covers the NaN-everywhere symptom.

### h. `FEATURE_OPENCV` macro
Set by CMake, never by `#define` in a `.cpp`. CAVEATS A1 has the history.

---

## 8. Common workflows for new devs

### "I want to add a new measurement primitive"

1. Read `caliper_primitive_locating_design.md` (existing caliper toolbox).
2. Add the def fields to `featureDef_*` in `MatchingEngine/include/`.
3. Wire the parser in `parse_*` next to existing primitives.
4. Implement matching in `FeatureManager_sig360_circle_line.cpp` (or a new file under the same pattern).
5. Add a regression input under `test_suite/qa/` and a golden output.
6. **Add a `LOG_MODULE` annotation** at the top of any new `.cpp` so the WebUI's module tree picks it up.
7. Run `--insp` against the regression def, diff against golden.

### "I want to add a new BPG message"

1. Define the message in `BPG_Protocol/include/` next to the existing patterns.
2. Add the handler in `Core0_1/wiringPanel.cpp` (yes, it's the dispatcher; yes, it's a big file).
3. Add the WebUI side in `UI/WebUI/src/comm/BPG_WS.js`.
4. Test through the live app, not `--insp` (BPG isn't on the headless path).

### "I want to fix a perf hotspot"

1. `INSP_LOOP_N=300 INSP_LOG_LEVEL=WARN ./visSele --insp ...` to give the sampler enough wall time.
2. `sample <pid> 10 -file /tmp/sample.txt` (or xctrace) while it's running.
3. Find the leaf functions and parent stacks; aim for self-time > 5% before touching anything.
4. Validate the change: `--insp` diff against golden, `INSP_LOG_LEVEL=DEBUG` to spot any new warnings.

### "I want to wire a new field through to the WebUI log panel"

1. Add the field on the producer side (LogRecord / writer).
2. Echo it in the drainer's JSON build (`inspd_log_ws.cpp`).
3. Update `LOGGING_WEBUI.md` with the field shape.
4. Get the WebUI team to add it to the panel's renderer. Don't both sides land at the same time — schema-only PR first, render PR after.

---

## 9. Detail docs index

| Doc | What it covers |
|---|---|
| `RUNNING_CORE0_1.md` | Local-run recipe + the cameraless soft-cam fallback |
| `CORE0_1_CAVEATS.md` | Hard-won gotchas (the must-read companion to this file) |
| `LOGGING_WEBUI.md` | The `inspd_log.v1` WS contract (OTel-shaped) |
| `PROJECT_NOTES.md` | Higher-level project narrative |
| `OPENCV_MIGRATION.md` | acv → OpenCV migration plan |
| `OPENCV_MIGRATION_OPEN_QUESTIONS.md` | The known unknowns |
| `caliper_primitive_locating_design.md` | Caliper toolbox design (edge selector etc.) |
| `search_point_rework.md` | Search-point CV rework (separate from caliper) |
| `measurement_pipeline_and_caveats.md` | Pipeline-level traps |
| `IMG_TRANSFER_JPEG.md` | Opt-in JPEG compression for image transfer |

---

## 10. Outside this directory but you'll need them

- `UI/WebUI/src/script.jsx` — main React shell. The "Core Logs" mount is around line 1865 (`show_core_log_panel` state).
- `UI/WebUI/src/comm/CoreLogClient.js` — the WS client for the log panel.
- `UI/WebUI/src/component/CoreLogPanel.jsx` — the panel itself.
- `UI/WebUI/tools/webctl/` — the WebUI regression harness (browser-driven + mock servers).
- `UI/WebUI/tools/webctl/mock_inspd_log.mjs` — replays a captured `insp.log` over `:4091` for offline UI dev.
- `UI/WebUI/tools/webctl/baseline/inspd_log/caliper_verify_x60.log.gz` — 48 800-line fixture for the panel's regression.
- `tools/webctl/` and `qa/` directories carry the agreed-upon WebUI test patterns. Use them.

---

## 11. Onboarding checklist for week 1

- [ ] Build the project (both `visSele` and `inspd_log`).
- [ ] Run `--insp` against `/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify.hydef` (or your team's golden). Confirm the output JSON matches the known-good file at ≤0.001 mm radius diff.
- [ ] Start the live app with `INSP_LOG_DAEMON=1 INSP_LOG_WS_PORT=4091`. Open the WebUI. Click **Core Logs**. Confirm the panel populates with the module tree + live log lines.
- [ ] Read `CAVEATS.md` cover to cover. Each item burned someone for hours.
- [ ] Pick one `LOG_MODULE` you don't recognize. Grep for it. Read what it logs. Write down one sentence about what subsystem it represents.
- [ ] Skim `BPG_Protocol/include/` to understand the WS message catalog.
- [ ] Run the WebUI's regression suite (`UI/WebUI/tools/webctl/regress.mjs` or your team's equivalent). Understand what it covers.
- [ ] Pair with someone for one debug session. Watch them use the Core Logs panel + a `--insp` repro loop. That's the dev workflow.

---

## 12. The two questions to ask before changing anything

1. **Does this affect already-deployed factory defs?** If yes, make it opt-in.
2. **Does this change what the WebUI sees on the wire?** If yes, coordinate the schema PR before the render PR, and bump the protocol version (or add a new field).

Almost every regression in this repo came from violating one of those.
