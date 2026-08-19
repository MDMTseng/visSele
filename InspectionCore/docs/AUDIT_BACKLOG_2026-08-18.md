# Audit backlog — 2026-08-18

Found by four review agents reading the sources, plus bench work the same day.
Everything here is recorded, not fixed. Items fixed on the day are listed at
the bottom so this file is the whole picture rather than the remainder.

**Verification status is per-item and it matters.** VERIFIED means the code was
read at the cited line, or reproduced on the bench. REPORTED means an agent
found it and nobody has confirmed it yet — treat those as leads, not facts.

---

## P1 — silent wrong answers

**Memory safety: write past `dataBuff`** — VERIFIED and FIXED 2026-08-19.
The corruption is real; the mechanism reported was not the one.

Reported as: `tryRecoverResetFromErrorBuffer` returns without compacting when
`firstBrace <= 0`, so `buffIdx` stays pinned at 2048 and `recv_data` keeps
appending past the end. That branch does exist at
`Data_Layer_Protocol.cpp:104-121`, but the cited range stops just short of the
tail of the same function, which handles precisely that case:

    if(firstBrace>0)                     { ...compact... }
    else if(buffIdx>=sizeof(dataBuff))   { buffIdx=0; }     // line 136

Driving the real code from a host harness — no board — found the corruption by
a different route, and going the other way. In the "found a complete
RESET/clear_error" branch:

    if(viaClear) handleClearErrorRecovery();   // both call clearProtocolError(),
    else         handleResetRecovery();        // which sets buffIdx = 0
    int shift=endIdx+1;
    if(shift<buffIdx) memmove(...);            // false: buffIdx is 0 now
    buffIdx-=shift;                            // 0 - shift  =>  NEGATIVE

Measured `buffIdx` of **-2104** and **-16** on different inputs, at which point
`dataBuff[buffIdx++]=c` writes *before* the array — worse than overrunning it,
because it lands on whatever precedes `dataBuff` in the object. Reachable from
three shapes, all of them ordinary traffic after a link has latched: a partial
RESET that never closes, a complete RESET arriving after the buffer filled, and
a RESET straddling the 2048 boundary.

Fixed by deleting the compaction: both handlers have already emptied the buffer
and switched to RESYNC (discard to the next newline), so there is nothing left
to compact. Regression test at
`Peripheral/uInspESP32/tools/test_data_layer_overflow.cpp` — host build, no
hardware, 400 fuzz trials plus the targeted shapes, checking `buffIdx` and a
canary after every byte. Confirmed failing before the fix and passing after.

**`EdgeSelect` NTH clamps instead of failing** — VERIFIED
`InspectionCore/MatchingEngine/EdgeSelect.cpp:68`
`sel = std::min(std::max(p.nth, 0), (int)peaks.size() - 1)`. A def asking for
the 5th peak on a 2-peak profile silently measures the last one and reports
SUCCESS. The requested edge not existing is a measurement error, not a reason
to substitute a different edge.

**Duplicate-verdict resolution is order-dependent on the cat numbers** —
VERIFIED, guarded 2026-08-18
`Peripheral/uInspESP32/src/app/LegacyFirmware.cpp:6731`
`if(cat < tarP->insp_status)` keeps the smaller cat and calls it worst-wins.
Only true while `cat_ng < cat_ok`. The core now refuses the inverted wiring
(`wiringPanel.cpp:5803`), so this is contained — but the firmware rule itself
still encodes the assumption implicitly. Consider naming it in the firmware.

**Camera identity fields are rotated** — VERIFIED
`InspectionCore/CameraLayer/CameraLayerManager.cpp:14-16`
`"vendor"<-info.model`, `"model"<-info.serial_number`,
`"serial_nbr"<-info.vender`. Every camera the WebUI lists is mislabelled. The
commented-out reference mapping at `CameraLayerManager.hpp:49-51` shows the
1:1 intent.

**Play is ready while a rendered tag group is unsatisfied** — VERIFIED
`UI/WebUI/src/MAINUI.js:580` vs `:574`
Readiness is computed against `tagGroupsPreset`; the picker renders
`new_tagGroupsPreset`, which additionally carries the recipe's `已設定範圍`
margin group with `maxCount:1`. Select two margin tags: the group draws its
warning triangle, play stays enabled, and which margin applies is then decided
by selection order (`InspectionUI.js:1847`, REPORTED). Hooks and a probe exist
(`tools/webctl/play_readiness.mjs`); the probe SKIPs on the current fixture,
which carries only one margin tag. **Not fixed on purpose** — refuse to start,
drop the extra, or merge the margins is a machine-owner decision.

---

## P2 — coverage holes that let P1s through

**`CAM_CLOCK_LOST` (13) is unreachable from the running machine** — VERIFIED
2026-08-19, on hardware and in the source
`Peripheral/uInspESP32/src/app/LegacyFirmware.cpp:634` vs `:6563`
`gate()` rejects on `nearest_delta > TOL_US` and `byTs` is set on
`nearestDelta <= TOL_US` — one variable, one threshold, complementary. So any
frame `gate()` rejects also has `byTs` NULL, and in READY `bySync` is
NULL too (sync pulses fire only in CAL/RECAL). `tarP` is therefore NULL and
the same pass raises `INSP_RESULT_MATCHES_NO_OBJECT` at `:6777` and halts,
while `consec_reject` has reached 1 of `LOST_N`'s 2. The second frame never
comes, because the machine is already stopped. Reproduced three times with
`camsync_lost.mjs`: `rejected=1`, `rebuilds=0`, `error_hist=[1]`, never 13.

Not a safety defect — the machine does stop, and no part is sorted on a frame
it could not place. Two things are lost. The diagnosis: the operator is told "a
verdict arrived for no known object" and sent to look at pairing when the cause
is the clock, and the `CAMSYNC LOST` line carrying the delta and tolerance
never prints. And the hysteresis: `LOST_N=2` exists because "one is a lost
frame or a stray, two in a row is the clock", and that tolerance for a single
stray has never existed on this path.

Left open deliberately. The minimal fix distinguishes, before `:6777`, a
frame the gate refused for being out of window from one that genuinely matched
nothing, and lets only the former accumulate. That relaxes "a single unowned
verdict halts the machine", which is a policy decision about how much a
machine may tolerate before stopping — not a call a test should make in
passing.


**`--insp` never loads `clean_regions`** — VERIFIED
`InspectionCore/Core0_1/wiringPanel.cpp:10614` vs `:2528-2529`
The live path loads both region kinds; the offline path loads only
`inspection_region`. So the entire clean-space feature has no offline gate,
and the same def measured offline and live can disagree with no warning.

**The quietest failure has the quietest log** — VERIFIED
`MatchingEngine/FeatureManager_sig360_circle_line.cpp:2712` is `LOGI`
("Not able to find matching contour" — the SBM+contour all-NA path, seen 105×
in 20 frames this week), while the circle equivalent at `:4274` is `LOGE`.

**`min_strength` is silently raised** — VERIFIED, deliberate
`MatchingEngine/EdgeSelect.cpp:36-38` floors it at `0.15*gmax`. The rationale
is in the code and is sound; the problem is discoverability — a def author who
sets `0.05` has no way to learn it did not take effect.

**`fit:"contain"` means different things per locator** — VERIFIED, documented
`FeatureManager_sig360_circle_line.cpp:7636` passes `have_extent=false` for SBM
(the comment explains: a pose carries no size), so `contain` degrades to a
centre test, while the CCL path uses the real bbox. One setting, two rules.

**`test_suite/qa/` is dead outside one machine** — VERIFIED, and re-scoped
2026-08-19. The path count understated it: fixing the paths would NOT make this
runnable. Four separate blockers, in increasing order of cost:

1. Hardcoded roots — `qalib.py:21`, `suite.py:16`, `migration_gate.py:13`.
   Derivable from `__file__`; cheap.
2. **Two point at a temporary worktree** — `qa_insp_region.py:22` and
   `qa_objdetect_dark.py:25` reference
   `.claude/worktrees/uinsp-mini-compact/InspectionCore`. That directory is not
   expected to exist any more, so those two are dead even on the Mac.
3. Hardcoded macOS build layout — `BUILD = ROOT + "/build/mac-arm64"` and
   `VIS = BUILD + "/visSele"`. Windows is a different preset and needs `.exe`.
4. **The 10221 golden is not in the repository** — `qalib.py:25-27` wants
   `/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.png`
   and its `.hydef`. This is the hard one: 5 of the 9 qa modules plus
   `daemon_smoke.py`, `migration_gate.py` and `suite.py` need it, and no path
   fix substitutes for a missing file.

So do not "fix the paths" here expecting the suite to come back — that turns
an obviously-dead suite into one that looks runnable and fails on missing data.
Decide about the golden image first. Contrast with the WebUI `qa/` layer, where
the equivalent fix DID work (commit `91efefdf`) because a substitute fixture
was already in the repository.

Also `UI/WebUI/tools/webctl/fullframe_run.sh:11,15` hardcodes
`cd /Users/mdm/workspace/visSele/...` and `DYLD_LIBRARY_PATH`. It needs a
camera anyway, so it is not on the critical path, but it cannot run here at
all as written.

The four `test_suite/*.py` gates ARE listed in `REGRESSION_TESTS.md` §1;
as of 2026-08-19 that row says why they do not run. `test_suite/qa/` now has
its own row there too.

---

## P3 — reported, not yet verified

Camera layer (all `InspectionCore/CameraLayer/`):
- `CameraLayer_Aravis.cpp:2145,2166,2233` — SetFrameRate failure diagnostics use
  `LOGD`, a compile-time no-op. The silent half of the framerate bug, still
  present on the layer that was "already correct".
- `CameraLayer_BMP_carousel.cpp:396` — `1000/frame_rate` unguarded: `-1` gives a
  free-running imread loop, `0` divides by zero.
- `CameraLayer_GIGE_MindVision.cpp:628-644` — `<10` selects the SLOWEST speed
  mode, so `framerate:-1` means the opposite of uncapped there.
- `CameraLayer_HikRobot_Camera.cpp:999,1008,1016` — RGain/GGain/BGain discard
  the write result and always return ACK; `wiringPanel.cpp:2101-2118` does not
  `_chk` them either, so a rejected white balance is invisible on both ends.
- `CameraLayer_HikRobot_Camera.cpp:911` — `TriggerMode(0)` calls
  `SetFrameRate(60)`; Aravis's TriggerMode touches neither. A session switch
  re-caps one platform and not the other.
- `include/CameraLayer.hpp:44-52,154` — `frameInfo` has no NSDMI and the
  constructor does not initialise `fi`; plain `CameraLayer_BMP` never assigns
  it, and consumers feed it to `cv::Mat::create`.
- `CMakeLists.txt:245-247` — `FEATURE_UVC=ON` compiles a TU with no libuvc glue
  and no CameraLayerManager branch; `:362-367` hardcodes a win64 lib, making
  HikRobot structurally Windows-only.

Core / config:
- `wiringPanel.cpp:2372-2396` — `load_insp_region` resets to a default
  `InspRegionCfg` when the key is absent, so a partial `MachineSetting` push
  silently disables the station for the session.
- `wiringPanel.cpp:2533` — `%s` with a possibly-NULL `path` (UB; the NULL check
  is the next line), and `[E]` on three non-error lines.
- `wiringPanel.cpp:8996-9002` — the sampler-origin fallback treats `(0,0)` as
  "not reported"; on BMP that then reads a software-crop origin, a different
  coordinate space.

Firmware:
- `FirmwareTypes.hpp:84-86` vs `LegacyFirmware.cpp:8232` — the host-link
  watchdog is unreachable in `INSPECTION_MODE_TEST`; `SYS_STATE_Transfer`
  (`:2766-2775`) drops an unmatched act with no log and no counter.
- `FirmwareTypes.hpp:36,51,52` — `INSPECTION_MODE_FATAL` (113) has no
  transition rows and no references; anything testing for it tests nothing.
- Largest legitimate `set_setup` vs the device's 2048-byte RX buffer has never
  been measured (config alone is ~1.6 kB minified).
- `LegacyFirmware.cpp:1539-1542`, `wiringPanel.cpp:7940-7950` — tid-pairing
  fault injector and the `have_identity` else-branch are dead code; the
  injector still reports itself as armed.
- `docs/INTEGRATION_MAP.md:160-172,262-266` — still says `cat=3` fires no valve
  and warns against letting it reach the report path. That is now the
  production OK route.

WebUI:
- `InspectionUI.js:1847` — first matching margin tag wins, silently.
- `InspectionUI.js:300` — a plain `NG` verdict renders the label "SNG";
  `canvas/renderConst.js` has no NG/OK entries, so the colour is `undefined`.
- `InspectionUI.js:883` — the station panel hardcodes
  `data/machine_setting.json` while the settings panel honours `__priv.path`.
- `PerifAPI.js:928` — `getMachineId()` reads `this.machineSetup`, which
  `machineSetupUpdate` deliberately clears; always undefined. Uncalled, so a
  trap rather than a live bug.
- `PerifAPI.js:340` — `LoadFileToMachine`'s 1 s timeout is never cleared and is
  tight for a file load over the WS.
- `MAINUI.js:1663` — the cancel branch calls `onOK()`.
- `UTIL/BPG_Protocol.js:120-128` — a `TEMP probe` marked "Remove once
  diagnosed" is still in production source: it `console.log`s meta for the
  first 20 IM frames of every session, gated on `window.__IM_PROBE_N`. Found
  2026-08-19 while triaging `r10_bpgfuzz`. Harmless but it is noise in the
  factory console, and TEAM_HANDOFF §9.12 notes that terminal has no
  devtools to filter it.
- `qa/r10_bpgfuzz.mjs` F3 — asserts `camera_id`/`session_id` on the object
  `raw2Obj_IM` returns. The 15-byte IM extra-header has no such fields and
  never did, so all 500 iterations fail and have always failed. The **test** is
  wrong, not the decoder; verified live against the running app 2026-08-19.

---

## Fixed 2026-08-18 (listed so this file is the whole picture)

- Settings save pushed the panel's stale cache to the running core even when
  the disk write was refused (`MAINUI.js`).
- `importSetupP` wiped `deviceState`, so a batch archived after a config import
  was filed against `machine: null` (`PerifAPI.js`).
- `send()` id collision overwrote the in-flight entry: the original caller
  never settled and its reply resolved the wrong one (`PerifAPI.js`).
- `sendP` had no deadline: a dropped reply leaked a pending promise and its id
  for the rest of the session (`PerifAPI.js`).
- The custom-tag input called `this.setState` in a function component
  (`rdxComponent.jsx`).
- `cat_ng >= cat_ok` is now refused (`wiringPanel.cpp`).
- `framerate: -1` meant "uncapped" on Aravis and "cap at whatever is stored" on
  HikRobot (`CameraLayer_HikRobot_Camera.cpp`).
- `machine_id` travelled inside config exports (firmware + `PerifAPI.js`).
- The eight editor flows could not run off the Mac bench (`flows.mjs`).
