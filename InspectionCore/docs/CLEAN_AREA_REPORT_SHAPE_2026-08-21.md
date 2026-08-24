# Clean area, report shape, and the bench tooling around them — 2026-08-21

Handover note. Written so the next session can pick this up cold.

The subject is one question: **what the machine shows, counts, and uploads for a
part whose station clean area was dirty.** Everything below either answers part
of that or is a trap met while answering it.

---

## 1. The state of the work

### Done and verified on the bench

`wiringPanel.cpp` — when the clean gate blocks a frame, the report keeps its
normal shape but carries **zero located objects**, so no dimensions reach the
screen while the part still counts.

- `blank_located_objects()` — `InspectionCore/Core0_1/wiringPanel.cpp:2873`
- called at `wiringPanel.cpp:10042`, right after `report_json` is built:

```cpp
imgPipe->datViewInfo.report_json = skip_inspection()
  ? cJSON_CreateObject()
  : matchingEng.FeatureReport2Json(report);
if (clean_blocked)
  blank_located_objects(imgPipe->datViewInfo.report_json);
```

It walks the TOP-level `reports` array (one entry per def FEATURE) and replaces
each entry's own `reports` array (the located objects) with an empty one.
Errors, `station`, `uInspResult`, `mmpp`, `subFeatureDefSha1`, `machine_hash`
are all left untouched.

Built and running: `InspectionCore/build/win-mingw-msys/visSele.exe`.

### Also done — the engine no longer inspects a blocked frame

The frame is still handed to the engine, but with **no candidate objects**, so
the locator yields nothing and no measurement runs. Measured 2026-08-21:

```
                insp_wall_ms   located objects
 not blocked     12.4 – 15.6         1
 blocked         0.002 – 0.029       0     <- was 11–14 ms
```

- `FeatureManager::no_candidate_frame` — `MatchingEngine/include/FeatureManager.h:128`,
  set through `MatchingEngine::setNoCandidateFrame()` (`MatchingEngine.cpp:157`)
- set every frame, both ways, at `wiringPanel.cpp:9828`. It is STATE on the
  managers, so the `false` is what releases the engine after a blocked frame.
- honoured in two places, and **both are needed** — see the next block.

`blank_located_objects()` stays as the backstop for any sub-feature type that
does not honour the flag; it is no longer the mechanism.

### Both localization schemes, and why one guard is not enough

| scheme | selected by | what stops it |
|---|---|---|
| legacy binarize → CCL → signature | default | empty `ldData` from the group |
| shape (line2Dup) | `locating_engine=1 && shape_ready` | the sig360 guard ONLY |

`FeatureManager_binary_processing_group::FeatureMatching()`
(`FeatureManager_group.cpp:315`) returns early with `ldData` empty — that is
where the saving comes from, binarize/cage/CCL/intrusion never run.

**An empty `ldData` does not stop the shape locator.** `FeatureMatching_shape()`
works off `originalImage_cv` and never looks at the group's candidate list, so
it would run line2Dup over the whole region regardless. The second guard is at
the TOP of `FeatureManager_sig360_circle_line::FeatureMatching()`
(`FeatureManager_sig360_circle_line.cpp:6162`), **above** the engine branch, so
one guard covers both engines.

Two invariants in both guards:

- **the sub-features are still CALLED.** `GetReport()` returns the engine's LAST
  report; skip the call and the PREVIOUS part's measurements are published as
  this frame's.
- **`report.type` and `bacpac` are untouched, error stays NONE.** That is the
  difference from `ClearReport()` (section 4) and it is what keeps the part in
  the UI statistics.

### Consequence: `result_obj` is now STATUS_UNSET

`station.result_obj` on a blocked frame changed from `0` (STATUS_SUCCESS) to
`-100` (STATUS_UNSET) — the part genuinely was not judged. The canvas overlay's
`(零件本身 OK,場地或守門擋下)` suffix is gated on `result_obj === 0` and so
stops appearing by itself, which is correct: it was a claim about a measurement
that no longer happens. Replaced with a `clean_err`-gated
`(淨空區不乾淨,未量測)` at `EverCheckCanvasComponent.js:1441`.

`station.result` (-128) is unchanged, so the verdict and the board path are
unaffected.

---

## 2. The report shape — the thing everything hinges on

Captured off this machine (`--ws-tap` writes whole RP payloads):

```json
{ "type": "binary_processing_group", "error": 0, "mmpp": 0.0138859432190657,
  "subFeatureDefSha1": "d490c049801c0e5d27579a7ed99aa93397df6b03",
  "machine_hash": "0000000000000000",
  "reports": [ { "type": "sig360_circle_line", "error": 0, "reports": [] } ],
  "uInspResult": -128, "station": { ... },
  "insp_wall_ms": 1.881, "insp_cpu_ms": 0 }
```

Two levels, and the difference between them is the whole design:

| level | what an entry is | emptying it means |
|---|---|---|
| `reports` (top) | one per def **feature** | the reducer's per-feature loop runs zero times |
| `reports[i].reports` | the **located objects** | no dimensions, part still counted |

**A genuine zero-object frame already produces `reports[0].reports == []`.** The
fix copies that shape exactly rather than inventing one — which matters, because
the whole reason the older "skip the inspection" path was removed is that it
emitted a THIRD shape (no `type`, no `reports`) that the WebUI reducer drops on
its first line, making blocked parts invisible in the UI while the machine went
on counting them.

### Why the top level must not be emptied

`UI/WebUI/src/redux/reducer/UICtrlReducer.js`:

```js
action.data.reports.forEach((report) => {          // :181  per FEATURE
  switch (report.type) {
    case "sig360_circle_line":
      ...
      reportStatisticState.reportCount++;           // :240  INSIDE the loop
      if (report.reports === undefined || report.reports.length == 0)
        reportStatisticState.emptyReportCount++;    // :243
```

`reportCount++` is inside the per-feature loop. Empty the top-level array and
the loop never runs — blocked parts disappear from the statistics again. The
UI already models "a part with no measurements" as `emptyReportCount`; that is
the state the fix produces.

Also note `EVENT_Inspection_Report` bails on line 144 when `type` is undefined —
that is the line the old skip path died on.

---

## 3. Verification — what was actually measured

Run: `pw_bringup.mjs --url http://localhost:8081/ --freq 8000 --watch 40
--zoom 4 --force-dirty 30 --ws-tap`, 2026-08-21.

```
       reports blocked images  dark_ratio      reportCount  emptyReportCount
 pre     1271   1042    392   0.000..1.000    +1271         +1042
 dirty    548    547    166   0.000..1.000    + 547         + 547
 post     105     79     32   0.000..0.093    + 104         +  79
```

1. **The lever bit.** `dirty` = 547/548 blocked (99.8 %), both regions tripping
   every frame, sample `clean1 dark_ratio: 1, dark_area_mm2: 4.87`. `post` back
   to 0.093 confirms the restore.
2. **Blocked ⇒ empty, and still counted.** `dirty`: `reportCount +547` and
   `emptyReportCount +547` — *equal*. Every blocked part counted; every one
   empty.
3. **Negative control.** `pre`: 1271 reports, 1042 empty → **229 were NOT
   empty**. The blanking is selective, not global. (blocked 1042 == empty 1042
   is the one-to-one correspondence.)
4. **Images keep flowing:** 166 during the dirty window; screenshots
   `08_dirty_a.png` / `09_dirty_b.png` differ in content.

Screenshot during the dirty window: camera image present, both 淨空 boxes red,
overlay reads `NA → 不動作(零件本身 OK, 淨空區…)`, and **the left panel's
measurement rows are gone entirely** — before the fix the same situation showed
`2.388mm UCNG / 1.023mm LCNG / 2.731mm LCNG` and a verdict badge.

Structural check, from an earlier run's captured payloads (all blocked frames):
`feature[0] type=sig360_circle_line error=0 located_objects=0`, top-level keys
identical to a clean sample — no new shape.

### Re-verified after the engine change and the fused scan

Same four criteria, run 18:04 with `--watch 20 --force-dirty 20`:

```
       reports blocked  reportCount  emptyReportCount
 pre      730     595     +730         +595      <- 135 NOT empty: negative control
 dirty    369     369     +368         +368      <- equal
 post     105      86     +104         + 85
```

Plus two numbers this run added:

- `insp_wall_ms` on blocked frames **0.002–0.029** against 12.4–15.6 on
  measured ones. The engine really is not running; the earlier version of this
  work was 11–14 ms on both.
- `dark_area_mm2` on a forced-dirty region is **4.86772632598877**, the same
  value to every digit as the pre-change run in the table above. That is the
  fused single-pass scan reproducing `cv::threshold` + `countNonZero` exactly.

**Caveat on the earlier run:** the canvas-pixel sampler reported
`0 distinct frames in 60 samples (60 unreadable)`. It depended on
`window.__CANVAS_DIAG__.geo`, and those probes were removed from the source
(section 10). Image liveness rests on the redux `images` count and the differing
screenshots, not on that sampler.

---

## 4. Two shortcuts that look right and are not

Both were investigated and rejected with evidence. Do not re-try them without
reading this.

### Skipping `ImgInspection`

`matchingEng.GetReport()` returns the engine's **last** report. Skip the
inspection and it hands back the PREVIOUS part's measurements as this frame's.
Documented at `wiringPanel.cpp:9829`.

### Calling the engine's `ClearReport()`

The name fits perfectly and the group version even keeps `sub_reports` sized per
feature (`FeatureManager_group.cpp:612`). But every sub-feature's `ClearReport()`
chains down to `FeatureManager::ClearReport()`
(`include/FeatureManager.h:137`):

```cpp
virtual void ClearReport(){
  bacpac=NULL;
  report.type=FeatureReport::NONE;   // <-- type erased
  report.bacpac=bacpac;
}
```

- `type = NONE` → the reducer's `switch (report.type)` never reaches
  `case "sig360_circle_line"` → `reportCount++` never runs → **blocked parts
  vanish from the statistics**, the exact regression this work exists to
  prevent.
- `bacpac = NULL` detaches state the next real inspection needs.

It is a construction/reset helper, not a per-frame "locate nothing" path.

---

## 5. The clean-area scan itself — where it runs, and what it costs

The gate is `eval_clean_regions()`, `wiringPanel.cpp:2776`. It is **not** the
inspection region and **not** the whole frame: two separately configured
rectangles that flank the station.

```
 inspection_region  1222,498  366x294   contain
 clean1             1589,493   85x297   dark_thresh 20  dark_area_max 0.001 mm2
 clean2             1097,494  126x288   same
```

clean2's right edge (1223) meets the station's left edge (1222); clean1's left
edge (1589) meets its right edge (1588). They are the lanes the part passes
through on the way in and out.

Coordinate space, and all three parts matter:

- it scans **`capImg`, the full inspection frame** — not the WebUI stream, not
  the engine's binary. The engine's downsample and threshold do not touch it.
- config is **full-sensor px**, so the camera's hardware ROI origin comes off
  first (`roi_origin [1000,376]` here → clean1 lands at (589,117) in image px).
  Same convention as `inspection_region`: one space for the whole station.
- **no sampler, no calibration.** Raw rectangle indexing; no lens-distortion
  remap. Only `dark_area_mm2` uses `mmpp`; the test is pixel counting.
- a rectangle is clipped to the image and **silently skipped** below 2x2. A box
  dragged off-frame fails quiet, with one log line.

### Cost, measured (`station.clean_us`, added 2026-08-21)

```
 61,533 px total (clean1 25,245 + clean2 36,288)
 clean_us  min 95   median 121   max 177   -> ~2 ns/px
```

That is **1 % of a 12.4 ms inspection**. But on a BLOCKED frame the inspection
is now 0.003 ms, so this scan is ~97 % of what the frame costs — the whole
frame went from ~12.5 ms to ~0.12 ms.

### Pixel-skipping: implemented, defaulted OFF, and not recommended here

`dark_step` (per region, 1–16) samples every Nth pixel on both axes, cost 1/N².
`INSP_CLEAN_STEP` overrides every region for a live A/B. The scan is also now a
single fused pass (compare + count in place, no destination Mat, one walk
instead of two) — bit-identical to `cv::threshold` + `countNonZero` at step 1,
and that costs nothing.

The stride does cost something, and this station sits exactly where it hurts:

| step | samples | area one dark sample carries |
|---|---|---|
| 1 | 61,533 | 0.00019 mm² |
| 2 | 15,383 | 0.00077 mm² |
| 3 | 6,837 | **0.00174 mm²** |
| 4 | 3,845 | **0.00309 mm²** |

`dark_area_max` is 0.001 mm² = **5.2 px**. At step ≥ 3 a SINGLE dark sample
already blows the whole budget, so the gate degrades to all-or-nothing; and a
speck narrower than N px can fall between samples and be missed entirely. Both
failure modes at once, in opposite directions.

What it buys: step 2 saves ~91 µs/frame = 1.6 ms/s = **0.16 % of a core**.
Shrinking the boxes is the safe way to buy the same microseconds — cost is
linear in pixel count either way, and it leaves no gaps inside the box.

## 6. Throughput: what actually limits this station

Measured 2026-08-21 with `gate_open.mjs`, clean regions OFF, plate freq 8000.

The gate has a rate limiter -- firmware `SYS_MIN_PULSE_TIME_SEP_us`
(`LegacyFirmware.cpp:257`), settable as `set_setup {gate:{min_detect_sep_us}}`.
Parts arriving closer than that are refused and counted as
`yield.gate.loss_n` with `loss:"rate"`.

```
ceiling  sep_us   seen/s  admit/s  rate_loss/s   sorted/s  na/s   pass%
   33     30303    19.0    14.2       4.2
   45     22222    19.0    18.1       0.2         18.9     0.90   95.4%
   55     18182    19.0    18.3       0.0         19.2     0.90   95.5%
   70     14286    19.1    18.4       0.0         19.3     0.87   95.7%
```

Three findings, and the first two say the limiter is the wrong knob:

1. **It was already at 70/s on this machine** (14286 us), not the 30000 default.
2. **It stops binding at ~45/s.** Cumulative `loss_n` froze at 47353 from that
   step on. Going 45 -> 70 bought 0.3 parts/s.
3. **The limit is the ARRIVAL rate**, ~19-21/s seen at the gate, which is plate
   speed (13.6 rpm at freq 8000) and bowl feed. To raise throughput you raise
   those -- but the plate throws parts above freq **10000**, which is the real
   ceiling and is not in any config file.

A part refused at the gate is NOT removed; it stays on the plate and comes back
next lap (which is why the AIMD rate loop was deleted 2026-08-12 -- widening the
gate sheds no load, it defers it). So opening the limiter does not create work,
it stops recirculating work that already exists.

### The clean gate is what actually costs yield

With clean regions OFF the sort stage passes **95.7%**. In normal operation the
clean regions block ~82% of frames, so roughly four parts in five are NA'd for a
dirty clean area rather than for anything about the part. Section 9 notes this
as a station/config question; these numbers are its size.

### The engine is not the limit either

At 19/s with clean OFF -- every part paying a full 12.4 ms inspection, ~24% of a
core -- `yield.verdict` stayed at `unanswered 0, skip 4` (a lifetime count that
did not move), `pct 99.97`. Camera pairing window held at 5000 us throughout
(`min_detect_sep_us` halves are 15000 and 7143, so it was never clamped).

## 7. What a further fix would need

An engine path that yields **zero candidate objects for this frame** while each
sub-feature still emits its own `type` and leaves `bacpac` alone. Then the
per-object measuring never happens (the cost genuinely goes away) and the JSON
is the same natural zero-object shape.

Entry points worth reading first:

- `FeatureManager_binary_processing_group::FeatureMatching()` —
  `FeatureManager_group.cpp:288`. Note the raw-gray fast path at the top
  (`needsBinaryPreprocessing()`), which skips binarize → cage → CCL entirely.
- `ldData` is the candidate list the sub-features consume via
  `setLabeledData(&ldData)`. An empty `ldData` plus the sub-features still being
  called is the shape of the answer.
- `FeatureManager_sig360_circle_line::ClearReport()`
  (`FeatureManager_sig360_circle_line.cpp:2484`) does `reports.resize(0)` —
  that part is exactly right; it is only the base-class chain underneath it that
  is destructive.

Re-verify with the same four criteria from section 3. The negative control is
not optional: without it, "blocked frames have no objects" is equally consistent
with having blanked every frame on the machine.

---

## 8. Bench tooling

All under `UI/WebUI/tools/webctl/`. Run from that directory (`node_modules`,
incl. `ws` and `playwright`, live there — a script copied to `/tmp` fails with
`ERR_MODULE_NOT_FOUND`).

### `pw_bringup.mjs` — the main driver

Cold WebUI → recipe → inspection → running, then measures. Clicks real buttons
and reads the real panel, so it works against the production bundle too; the
dev bundle (`:8081`) is needed only for `--store-probe` / `--clean-probe`, which
reach `window.__GP_STORE__`.

| flag | what it does |
|---|---|
| `--url` | `:8081` dev (store visible) or `:8082` prod. Default `:8082`. |
| `--freq` | plate freq. Must be non-zero or start is a silent no-op. |
| `--watch N` | seconds of fault watching; requires gate edges to CLIMB |
| `--no-start` | do not press the second play (it TOGGLES) |
| `--zoom N` | wheel-zoom the canvas N notches on the painted content, then two shots |
| `--force-dirty N` | make the clean regions fail for N s, then restore; tallies per phase |
| `--clean-probe` | per-phase tally from redux, incl. the UI's own `reportCount` |
| `--ws-tap` | frame census + writes whole RP payloads (see below) |
| `--store-probe` | `edit_info.img` distinct-reference count |
| `--diag` | canvas counters — **currently prints "absent"**, see section 10 |
| `--live-check` | canvas tile grid |

`--ws-tap` writes to `--shots` (default `C:/Users/w2110/Downloads/pw`):
`rp_notblocked_*.json` (the negative control), `rp_blocked_*.json`,
`rp_forced_*.json` (during `--force-dirty`). Bucketing is by
`station.clean_err` presence.

### `soak6h.mjs` — long soak, one browser session

`node soak6h.mjs [minutes] [url]` (default 360, `:8082`). Brings the machine up
itself — clear → freq → recipe → 製程 → 檢測方式 → inspection UI → start →
**verify gate edges climb** — then samples every 60 s:

```
t_min,heapMB,totalMB,uiRSS_MB,coreRSS_MB,domNodes,state,edges,err,panel
```

It exists because the obvious composition does not work. See the next trap.

On a fault it REPORTS and keeps going (how the machine behaves after a fault is
part of what a soak is for), screenshotting the first one. It also recovers a
board it finds already faulted, which is what makes it safe to start unattended.

### `board_rescue.mjs`

Attaches a browser (which is what opens the peripheral channel), prints
`state` / `error_hist` / `health` / `cam_sync` **before** touching anything, then
`freq 0 → exit_insp_mode → clear_error`. Use this rather than the bringup's park
when diagnosing: the park calls `clear_error_history` and destroys the evidence.

### `_rc_clean.mjs`

`dirtied(mset, 255)` builds the always-fail clean-region payload.
`openSettingLink()` still exists but **is not used by pw_bringup any more** —
see the first-peer trap in section 9. The ST now goes over the page's own
socket via an injected `window.__SEND_ST__`.

### `crop_zoom.mjs`

`node crop_zoom.mjs in.png out.png x y w h [zoom]` — magnify a screenshot region
with Chromium (no image library on this machine). Written because the picture on
the inspection canvas is ~100 device px and unjudgeable at that size.

### Building and running the core

```bash
cd InspectionCore
export PATH="/c/msys64/mingw64/bin:$PATH"      # cmake is NOT on the default PATH
./build.sh -p win-mingw-msys --no-configure -j 8
export INSP_PERIF_CONSOLE=4099                 # or there is NO dev console
./run_core.sh                                  # foreground; --bench also forces synth cam_ts
```

`run_core.sh` only sets `INSP_PERIF_CONSOLE` under `--bench`, and `--bench` also
turns on synthetic camera timestamps, which you do not want with a real camera.
Export the variable yourself.

---

## 9. Traps — every one of these cost bench time today

**The first WS peer owns the peripheral channel.** The core links to the board
on the first peer's `PD CONNECT`. A peer that connects first and never sends one
holds the slot, and the channel never comes up: two runs died at
`no perif channel` because a settings link was opened *before* the browser. The
channel appeared immediately once nothing but the page was attached. This is the
same trap `perif_hold.mjs` exists to demonstrate — and it is documented in
`pw_bringup.mjs`'s own header, which did not stop it happening.

**Closing the bring-up's browser faults the machine.** The same mechanism as
killing the core, and it is easy to walk into: the browser IS the board's host,
so `pw_bringup` PASSing and then EXITING drops the peripheral channel, the board
loses its host and raises error 12 `HOST_LINK_TIMEOUT`, and the plate stops.
Measured 2026-08-21: bringup passed with edges climbing at 13.6 rpm, exited, and
90 s later the soak's first samples read `ERROR · 盤停止 · 12: host link
timeout` against a perfectly flat 16.3 MB heap.

**A flat heap on a stopped machine is the most convincing wrong answer this
bench produces.** It looks exactly like "six hours, no leak". Any long run must
own the bring-up and hold ONE browser for its whole duration (`soak6h.mjs`), and
must sample a load witness — the panel text and the board's gate edges — beside
every memory number.

**The core's COM3 link does not always come back.** Every browser attach/detach
opens and closes the serial port (`[simple_uart] cfg: 230400 8N1` ... `uart
close` ... `UART DESTRUCT!!!!`). After six such cycles in one evening the last
close was never followed by an open: the core stayed alive with 4090 and 4099
listening, but 4099 answered `ECONNRESET` to every client and no new page could
bring the channel back. Only a core restart fixed it. Two soak attempts died on
this and were reported as "board not answering", which reads like dead hardware.
NOT root-caused -- normal operation should not need a core restart to recover a
peripheral channel.

**There is no peripheral channel without a browser.** With no page attached,
port 4099 answers `{"err":"no perif channel"}` and every board command silently
goes nowhere. This is normal, not a fault. A dropped channel after a run ends is
just the browser closing.

**`{"err":"no perif channel"}` carries no `id`.** `board.ask()` used to return a
bare `null` for it, and `if (st && st.state !== 112)` is false for `null` too —
so "the board is not answering" was reported as "board will not leave state
112", blaming the hardware for a link that was never established. `ask()` now
returns `{__err}` and the failure messages distinguish the two.

**`MachineSetting` is an `ST` command, not `RC`.** Handled in
`checkTL("ST")` at `wiringPanel.cpp:5595`. Send it to `RC` and RC answers
`ACK:true` having ignored the key — a green light for a change that never
happened.

**`ACK:false` is the normal answer to a MachineSetting-only `ST`.** `session_ACK`
is only set by keys like `DoImageTransfer` / `InspAreaBypass`. Verify the change
in the DATA (`dark_ratio`), never by the ack.

**`dark_thresh` must be 255, not 250.** `eval_clean_regions` uses
`THRESH_BINARY_INV`: a pixel is dark when NOT `(src > thresh)`. This is a
BACKLIT station, so the bright field is saturated at 255 and `255 > 250` leaves
`dark_ratio` at exactly 0. A threshold picked as "nearly white" measures
nothing. At 255 nothing can exceed it → ratio 1.0 → trips every frame.

**A bare `ST` resets `ImageCropX/Y/W/H`** to the whole frame unconditionally
(`wiringPanel.cpp:5629`). Harmless while `downSampLevel == 1` (that path sends
`capImg` untouched and never reads the crop) but it is a real side effect.

**`setup_machine_setting()` reads an absent key as "none configured."** Send
`clean_regions` without `inspection_region` and the station box is silently
wiped. Always send both. (`wiringPanel.cpp:5874` documents this; `InspRegionLive`
exists precisely to avoid it.)

**Stopping the core while inspecting faults the board.** It loses its host and
raises error **12 `HOST_LINK_TIMEOUT`** by design rather than keep sorting
unjudged parts, landing in state **112 `INSPECTION_MODE_ERROR`**. So every core
rebuild puts the board there. **`clear_error` alone never clears 112** — the
board is still IN inspection mode; `exit_insp_mode` moves it to 100 on the first
poll. The bringup's park now does this first.

**Park before killing the core.** No board → no triggers → the camera is not
streaming, which is the safe moment. Killing mid-grab has previously left the
camera un-enumerable.

**`taskkill /IM node.exe /F` takes the Vite servers with it.** Both WebUI
servers are node: `:8081` (dev, `npm run dev`) and `:8082` (prod preview,
`npx vite preview --config vite.config.prod.mjs --port 8082 --strictPort`).
Killing by image name stopped a bench script and both servers, and the next run
died on `ERR_CONNECTION_REFUSED` at `:8082` — which reads like a WebUI fault and
is not one. Kill by PID.

**Heredocs eat one backslash level.** Writing `'\\0'` through a heredoc put a
real NUL byte into `pw_bringup.mjs`; the code worked (the regex still stripped
trailing NULs) but git and grep treated the file as binary. Same class of bug as
the `LegacyFirmware.cpp` NUL earlier this month. Use the Write tool or a
file-based script for anything containing backslashes, and check with
`tr -dc '\000' < file | wc -c`.

---

## 10. Loose ends

**Canvas probes removed.** The temporary counters in
`EverCheckCanvasComponent.js` / `InspectionUI.js` (`window.__CANVAS_DIAG__`)
were removed after the image investigation. `--diag` therefore prints
`canvas diag: absent -- probes not in this bundle`, and `--force-dirty`'s
picture-region sampler (which reads `__CANVAS_DIAG__.geo` for the blit
rectangle) reports everything unreadable. Re-add the probes if that measurement
is needed, or re-derive the rectangle from the DOM.

**The inspection view draws the picture ~16× too small.** Root-caused and
deliberately NOT fixed (user: 我覺得不用). The view scale is applied ONCE, in
`EverCheckCanvasComponent.js:1286`, and `doImageFitting` is then false forever.
Measured at that moment:

```
imgW 2448  imgH 2048   (the def's stored preview, not the stream)
mmpp_used 0.0138859    canvasW 300    camScale_after 8.8254
```

`300 / (2448 × 0.0138859) = 8.825` — exactly the observed `camScale`. Two errors
multiply: fitted against a 2448-wide image while the stream is 816×528 (3×), and
fitted while the canvas was still 300 px instead of 1600 (5.33×). 3 × 5.33 = 16.
The image was always updating — 600 decodes and 602 blits in 90 s — just drawn
100 device px wide. A fix means re-fitting when the canvas size or the stream
resolution changes.

**Clean regions trip ~82 % of the time in normal operation** (`clean2` most
often), with `dark_ratio` reaching 1.0 naturally when a part covers the region.
That is a station/config question for the operator, not a software defect, but
it is why the forced-dirty lever added so little: the condition was already the
common case.

**Board health numbers worth a second look** (observed, not investigated; board
had not rebooted, `consec_unanswered` 0, so unrelated to today's work):
`isr_gap_max_us` 303780, `isr_overrun_n` 279, `cam1_pw_err_max_us` 9398 against
a requested 1000 µs.

---

## 11. Machine state as of this writing

- Core `build/win-mingw-msys/visSele.exe`, rebuilt 18:03 with the engine-level
  no-candidate path, the fused clean scan and `station.clean_us`, running with
  `INSP_PERIF_CONSOLE=4099`.
- `machine_setting.json` **never written**; the forced thresholds were runtime
  only and are restored (`dark_ratio` back to the 0.09 range).
- Board recovered from 112 and running; `error_hist [12]` is the
  `HOST_LINK_TIMEOUT` from the rebuild, not a live fault.
- `UI/WebUI/src/` carries no debug globals — checked with
  `grep -rn __CANVAS_DIAG__ src/`.
