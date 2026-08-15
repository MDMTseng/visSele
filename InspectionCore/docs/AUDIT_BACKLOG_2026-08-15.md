# Audit backlog — findings not yet fixed (2026-08-15)

Everything below came out of a review sweep on `ct/core01_v2_mach_file_cleanup`.
The items that were fixed are in the git log for that day; this file is only
what is **still open**, so it can be worked through without re-deriving it.

Each entry says what breaks and, where it matters, **how it shows up on the
line** — because most of these are not crashes. A crash gets noticed. A machine
that keeps running and quietly measures wrong does not.

Reachability is marked where it was established. "unconfirmed" means the code
is genuinely suspicious but no concrete trigger was demonstrated — treat those
as "investigate", not "fix blindly".

---

## Tier 1 — silently wrong results

These produce plausible numbers that are wrong, or pass parts that should fail.
For a measuring instrument this is the worst category.

### 1.1 `value_A == value_B` divides by zero, and NaN is judged PASS — FIXED (`7cad540e`)
`MatchingEngine/FeatureManager_sig360_circle_line.cpp:911`

The two-point remap `(v - A) * (Y - X) / (B - A) + X` has no guard for `A == B`.
Parsing only rejects NaN. Normally the result is ±inf → every part NG, which at
least gets noticed. But when the measured value happens to equal `value_A` the
result is `0/0` = NaN, and `NaN > USL` and `NaN < LSL` are **both false**, so
status is set to SUCCESS — a part passes carrying a NaN measurement.

Trigger: entering the same reading for both calibration points. An ordinary
operator slip, not a hostile input.

### 1.2 Startup runs with no distortion model — FIXED, listed for context
Fixed in `d51624af` (auto-load `data/lens_calib.json`, report
`lens_calib_loaded` in `camera_info`). Left here because the *class* of problem
recurs: any state the WebUI is expected to push is absent on a headless or
restarted core, and nothing says so.

### 1.3 `ignoreCalib` is a sticky global with no restore path — FIXED (session-scoped assignment + EX scope guard + `station.ignore_calib` field)
`Core0_1/wiringPanel.cpp:4115-4118`, `4131`, `4179`

CI/FI set `sampler->ignoreCalib(true)` when `IMG_ignore_calib` is requested and
never clear it. The EX handler sets it on entry, and the `srcImg == NULL` early
exit at `:4179` skips the `ignoreCalib(false)` at `:4264`. Production inspection
shares that sampler (`calib_bacpac`), so from then on every frame is measured in
uncorrected coordinates. The flag is not in any status report.

Trigger: use the calibration preview once, or have the camera drop a frame
during EX — i.e. exactly when the camera is misbehaving.

### 1.4 Peripheral disconnect is invisible; verdicts are dropped uncounted — FIXED (perif_pairing.link counters, WIN32 WriteFile check, suspect-link reopen)
`Core0_1/wiringPanel.cpp:1202`, `:7403/7434`, `:8629`, `:5266`;
`contrib/simple_uart/simple_uart.c:162`

- `disconnected()` overrides the base teardown with a `printf` and leaves
  `perifCH` non-NULL.
- On Windows (the deployed platform) `WriteFile`'s BOOL return is discarded, so
  a failed write reports 0 bytes and the caller counts it as success.
- The whole send is wrapped in `if (perifCH != NULL)`; once the channel is
  deleted (closing the last browser tab does that) **every verdict is dropped
  silently, with no counter**.
- Reconnect's `reuse` path compares only the port description string, so it
  re-ACKs a dead channel.

On the line: UI green, FPS normal, PASS/FAIL counts ticking — and nothing is
being sorted. Found only when someone notices the reject bin is empty.

### 1.5 Line caliper does not filter NaN samples, and a prefix sum spreads it — FIXED (`7cad540e`)
`MatchingEngine/Caliper.cpp:286`, `:297`

`Brow[x] = v;` stores the sampler's NaN directly, then the column prefix sum
`Sc[x] = Sp[x] + Brow[x]` makes **every subsequent row of that column** NaN, so
all later calipers on the line lose that across-column. The arc path
(`caliper_measure:154`) has `if (v != v) continue;` — its comment even says
"don't NaN the whole row". Same def: arcs fine, lines broken.

Trigger: part near the frame edge, or a longer `caliper.length`. High.

Fix needs a parallel valid-sample-count prefix sum (memory cost — pair it with
2.2 below).

### 1.6 `ptSubdivision` writes the tail segment to the wrong index — FIXED (`7cad540e`)
`MatchingEngine/ContourGrid.cpp:110-118`

`sec[j].pt = ...` should be `sec[(preSize-1)*times + j].pt`. The main loop stops
at `(preSize-1)*times`, so the tail entries are never written and keep their
value-initialised `{0,0}`, while the already-correct first segment is
overwritten. Every contour gains `times-1` phantom points at the origin, which
drag line/circle fits and propagate through `contourDir` to neighbours.

Trigger: `inspection_downsample > 1`.

### 1.7 Degenerate fits report the nominal shape as if measured — FIXED (fitOk gate on both paths; wlsLine detects zero covariance)
`MatchingEngine/Caliper.cpp:491-516` (Kasa), `:228-230` (`wlsLine`)

If `kasaCircle` fails on the first iteration the centre/radius stay at the
*nominal* values, residuals are then ~0, every point counts as an inlier, and
`r.ok` is true — the def's own nominal circle is returned as the measurement
with `rms = 0`. `wlsLine` similarly returns a horizontal line with `rms = 0`
when all inliers coincide. Neither flags the degeneracy.

Reachability: needs near-collinear hits (very short arc) — unconfirmed but the
failure mode is "always in spec", so it deserves a guard regardless.

### 1.8 `INSP_SKIP_INSPECTION=1` passes every part, and is not in the status — FIXED (`station.skip_inspection`, every frame)
`Core0_1/wiringPanel.cpp:8418` — one LOGE at startup and nothing else. Compare
`area_bypass`, which is deliberately reported on every frame; the same reasoning
applies here.

### 1.9 `perifSendQueue` drops the OLDEST on overflow — MITIGATED (link.queue_dropped counter + explicit off-by-one LOGE for positional pairing; a real fix needs a protocol placeholder)
`Core0_1/wiringPanel.cpp:8757-8765` — for the position-based uInspMEGA pairing
that shifts the whole verdict train by one, and the dropped part gets no NA
substitute.

### 1.10 Calibration files load "successfully" when malformed — FIXED (degenerate scale marks ok=false; load refuses to install and keeps the previous model)
`Core0_1/wiringPanel.cpp:1518-1531`; `MatchingEngine/LensCalib.cpp:169`

`load_lens_calib` only fails on `fopen`. `lens_calib_from_json` defaults `ok` to
**true** when the key is missing while every numeric defaults to 0 — so `{}`
yields `ok=true, m=0`, and `(u-u0)/m` divides by zero into the coordinate path.
The operator gets a green ACK.

### 1.11 Camera settings are never verified, and one ACK test is inverted — PARTLY FIXED (setter returns checked + `camera_info.setup_failed`; device read-back still absent)
`Core0_1/wiringPanel.cpp:1836/1843/1897/2003`, `:2021`, `:4980`

Every `SetExposureTime`/`SetAnalogGain`/`SetFrameRate`/`SetROI` return status is
discarded and `CameraSetup` ends in a hard-coded `return 0`. The base class
returns NAK by default, so an unimplemented driver setter fails silently;
Aravis silently clamps ROI to the increment and returns ACK. `camera_info`
echoes a string cached at construction, not a device read-back.

(The inverted `CameraSettingFile` ACK at `:4971` was fixed in `d51624af`.)

### 1.12 `JFetch_NUMBER_ex(...ppb2b)` unguarded in three more places — FIXED (`apply_def_cam_param` helper at all four sites)
`Core0_1/wiringPanel.cpp:5680`, `:9738`, `:9887` — the default is NaN, which
lands straight in the calibration map. `:3751` already has the correct guard to
copy.

---

## Tier 2 — crashes and resource failures

### 2.1 `ws_conn`'s socket, `sendBuf` and connection pool are shared unlocked
`BPG_Protocol/ws_server_util.cpp:233`, `:752`, `:769` (senders) vs `:182`,
`:272`, `:436`, `:444` (main loop)

Two concrete interleavings: (a) a worker is about to `send(sock,…)` while main
has already `close(sock)`d and `accept()` has handed the same fd to a new
browser — the previous client's image bytes go into the new connection and the
frame stream is permanently misaligned; (b) a worker holds
`frameBuffer = &sendBuf[0]` and main's `RESET()` resizes the vector underneath
it. `image_send_lock` does not cover either.

### 2.2 Line caliper band allocation has no upper bound — FIXED
Fixed in `5ceabee7` (8M cell cap). Noted because the *cause* is still live: the
def clamps (`count<=512, width<=64, length<=256`) are in **millimetres** and are
applied before `/= mmpp`, so at 72–135 px/mm they do not bound anything in
pixels. `caliper.step` has the same problem and is **not** fixed — see 2.3.

### 2.3 `caliper.step` has no lower bound — FIXED (double-first sizing + 1e6/8e6 sample caps on both paths)
`MatchingEngine/Caliper.cpp:140`, `:255` — `nAcross = (int)(2*L/step) + 1`.
`cal_step` is only checked for `<= 0`, then divided by mmpp. `step: 0.001` (1 µm
— a perfectly reasonable-looking entry) inflates `nAcross` ~11×; small enough
values overflow the `(int)` conversion, which is UB (arm64 saturates to INT_MAX
and sails past the `nAcross < 3` guard).

### 2.4 `delete_PeripheralChannel` deadlock — FIXED
Fixed in `d51624af`. Listed so the pattern is on record: holding a lock across a
thread `join()` where that thread can block on a socket write.

### 2.5 `camera_lifetime_lock` ↔ `CameraLayer::m` lock-order inversion
`CameraLayer/CameraLayer_GIGE_MindVision.cpp:137` (callback thread takes `m`,
then runs the whole consumer callback inside it, reaching
`camera_lifetime_lock`) vs `Core0_1/wiringPanel.cpp:4302` (WS thread takes
`camera_lifetime_lock`, then calls into the driver which takes `m`).

ABBA deadlock if `camera_ez_reconnect` coincides with a frame in flight. The
HikRobot side (the production path) only confirms direction B; direction A there
is **unconfirmed**. Even without the deadlock, holding `m` across a full
inspection makes every camera setting call wait a frame period.

### 2.6 `conn_peer` write side is unlocked, and the documented contract is not kept
`Core0_1/wiringPanel.cpp:8991-9031`

`main.h:195-198` states WS CLOSING/ERROR must hold `subscribersLock` before
freeing a peer. In practice only `unsubscribeStream` takes it (and releases
immediately); `peers.erase`, `default_peer = …`, `conn_peer = NULL` and the ws
layer's `RESET()` all run outside it, under `MT_LOCK` — which is a **no-op**.
This is code whose correctness depends on MT_LOCK actually working.
`bpg_pi.camera->TriggerMode(1)` at `:9027` is in the same position.

### 2.7 `static cv::Mat test1_buff` written by two threads — FIXED (thread_local)
`Core0_1/wiringPanel.cpp:6548`, `:6577` — written by ActionThread (`:8098`) and
by the WS thread's `LAST_FRAME_RESEND` (`:5158`). `image_send_lock` only
serialises the inside of `SEND_acvImage`; the `ImageDownSampling` write is
outside it. Same shape as the bug fixed in 138790f3, different buffer.
Trigger: `downSampLevel > 1` (default 1, settable from the WebUI).

### 2.8 `lastDatViewCache_lock` held across disk I/O and full sends — PARTLY MITIGATED (2026-08-16): a stalled client now wedges the WS layer for at most ~5s (SO_SNDTIMEO at accept + shutdown-on-send-failure in safeSend; measured RP 27/s→0→recovered while the stuck client stayed paused). The lock structure itself is unchanged — encode/imwrite/copyTo still run inside the lock
`Core0_1/wiringPanel.cpp:5150-5161`, `:2946-2956`, `:2859-2868`, `:1457-1459`

JPEG encode + whole-image WS send, directory creation + multi-MB `imwrite`, and
a 5MP `copyTo` all happen inside the lock. ActionThread's
`image_pipe_info_resendCache_swap_and_gc` waits on it, so slots stop returning
to the pool and acquisition starts dropping frames. With an unresponsive browser
the send can hold it indefinitely.

### 2.9 Unbounded contour walk in the teaching path — FIXED (bbox-area step cap + NULL check, mirrors acvContourExtraction)
`MatchingEngine/MatchingCore.cpp:176-183` — `acvOuterContourExtraction` has no
step cap, no visited marking and no NULL check on `cvContourWalk`, unlike
`acvContourExtraction:126` which has both guards. A 1px whisker is enough for a
Moore walk to enter a loop that never revisits the start point → hang + unbounded
growth. This is the path that produces `feature_signature` / `ref_orientation`
(`FeatureExtractor.cpp:90`).

### 2.10 Failed TCP CONNECT leaks a descriptor each time — FIXED, but the finding was overstated: refused/timeout connects already closed (connect_nonb). The real leaks were the rare paths (immediate connect() failure, getsockopt/setsockopt failure), plus a caller-unfriendly half-and-half close contract. Now: connect_nonb closes on EVERY failure (both platforms), ctor closes its own throws. Measured 30 failed CONNECTs: 0 fd growth
`common_lib/Data_Layer_PHY.cpp:193-201` — no `close()` before the throw. The
WebUI retries, so this can exhaust the fd table and take the WebSocket server
with it.

### 2.11 Acquisition callback has no try/catch — FIXED (catch + slot return unless enqueued)
`Core0_1/wiringPanel.cpp:5947-6148` — a `cv::Mat::create` throw permanently
loses a pool slot and the exception escapes into the SDK's callback thread.

### 2.12 `resourcePool` latent defects
`common_lib/include/TSQueue.hpp:375-385` `fetchResrc_blocking` self-deadlocks on
the second iteration (non-recursive mutex) — currently **zero callers**, but it
is the obvious thing to reach for if someone wants acquisition to stop dropping
frames. `:440` unlocks a never-locked mutex every frame (UB, harmless in
practice on this toolchain). `:345-350` double-unlocks; unreachable today.

---

## Tier 3 — bounded / low reachability, cheap to fix

- **FIXED** `RArray[thdegInt]` clamps only the upper bound —
  `FeatureManager_sig360_circle_line.cpp:4541-4548`. A NaN `theta_deg` gives
  `(int)NaN` (UB) then indexes a 360-entry stack array. Reachability
  unconfirmed, fix is one line.
- **FIXED** `caliper_locate_line` never checks `lineDir` for NaN — `Caliper.cpp:240`;
  `caliper_measure:136` does. Zero-length line (`pt1 == pt2`) or a NaN endpoint
  from morph reaches `(int)` conversion of NaN.
- **FIXED** `graySampleBilinear`'s guard misses NaN —
  `FeatureManager_sig360_circle_line.cpp:5963`; four comparisons against NaN are
  all false, so it falls through to `(int)` and a wild row pointer.
- **FIXED** (rejects the negative band in float, NaN-safe) `cvUnsignedMap1Sampling` truncates instead of flooring — `CvBridge.h:41`.
  For `-1 < x < 0` it extrapolates with a negative weight instead of returning
  NaN. One-pixel band at the left/top edge, 100% reachable, small error.
- **FIXED** (float-domain check first) `cvUnsignedMap1Sampling_Nearest` converts NaN before bounds-checking —
  `CvBridge.h:30`. Currently **no callers**; either delete it or reorder the
  check, so it is not a trap for the next user.
- **FIXED** `SearchPointCV.cpp:74-76` casts a NaN brightness to `unsigned char` — the
  coordinate is clamped so the sampler is safe, but the backlight factor can be
  NaN (`ImageSampler.cpp:1033-1035` returns NAN outside the grid).
- **`contourConcatLastTo` deletes a single self-merged section** —
  `ContourGrid.cpp:134-157`. When a contour yields one section, `toIdx ==
  endIdx`, it self-merges and the final `erase` removes it. Trigger looks like
  the *good* case (a circular hole entirely inside the epsilon band). Silent
  miss. Unconfirmed whether intentional.
- **FIXED** (`fabsf`) `abs()` on a float curvature — `ContourGrid.cpp:374`, `:908`. Only
  `<math.h>` is included; if it resolves to `int abs(int)` every `|curvature| <
  1` becomes 0 and the filter silently stops working. Fine on the current
  toolchain, a live risk on the MinGW deployment path. Use `fabsf`.
- **Same line's `continue` skips gap accounting** — a high-curvature point
  updates neither `ptInSection` nor `gapCount`, so a corner is invisible to
  section splitting and an L-shape's two edges merge into one section.
- **`ReadText`'s short-read check is commented out** — `common_lib/Util.c:406-414`.
- **`ImageStackAddUp` does not accumulate anything** — `Core0_1/wiringPanel.cpp:1277`.
  The only `Add()` call sites (`:8373`, `:8382`) are commented out, so
  `stackingC` never leaves 0 and `Export` divides an untouched buffer. The
  `__START_STACKING_IMG__` endpoint (`:2959`) therefore returns a blank image.
  Separately, `Add()`, the no-arg `Export()` and `ReSize()` each self-deadlock
  on the class's own non-recursive mutex, so the feature could not be re-enabled
  by simply uncommenting those lines. It also indexes its source with a
  hard-coded `*3`, which is why it appeared in the grayscale sweep — but there
  is nothing to fix for grayscale until the feature works at all.

---

## Tier 4 — write path durability

- **FIXED** `common_lib/Util.c:474-485` `WriteBytesToFile` checks neither `fwrite` nor
  `fclose` (ENOSPC surfaces at `fclose`) — returns success as long as `fopen`
  worked. `SaveJson` inherits this.
- **FIXED** (`WriteBytesToFileAtomic`: tmp + fsync + rename) `Core0_1/wiringPanel.cpp:2970-2988` the `SV` endpoint (how the WebUI saves
  defs and machine setup) truncates in place with no `rename(tmp, final)` and no
  `fsync` — a full disk or a crash destroys the machine's def with no backup.
- **FIXED** (fputs/fclose checked) `MatchingEngine/FieldCalib.cpp:171-175`, `LensCalib.cpp:199-200` report
  "saved" if the file merely opened.
- **FIXED** (fallback recomputes from the new root; double-failure skips the snap instead of exit(-100)) `Core0_1/wiringPanel.cpp:7926-7936` when the sample directory cannot be
  created the fallback re-tries **the same unrecomputed path**, so it fails
  again and calls `exit(-100)`. The documented "fall back to the default path"
  is dead code; the actual behaviour is that the core vanishes.

---

## Tier 5 — WebUI

All from the front-end sweep; none has been touched.

- **PARTLY FIXED** (Shape_Decoration_ID_Order_Update gets new identities — the drag-springback case; the applyEditTarSubstate path unchanged) **Reducer mutates in place and only spreads the top level** —
  `redux/reducer/UICtrlReducer.js:1000-1007`, `:1096-1101`. None of the seven
  mapped props change identity, so react-redux bails out. Operator-visible as:
  dragging a measurement row **springs back** (the order was recorded, it
  applies later when something else forces a redraw), and canvas overlays keep
  stale USL/LSL after editing.
- **FIXED** (dclone at mount like measureInfo, row-array copy on update, element copies in cleanUpDumpInfo) **`control_margin_info` is written straight into Redux state** —
  `DefConfUI.js:599-605`, `:740-757`. `measureInfo` is cloned, this is not.
  Editing a limit updates the table but nothing else knows: the dirty check
  compares an unchanged reference and says "no changes" — while the reducer's
  live grading path already reads the mutated array. **The UI says nothing
  changed while the verdict has already changed.**
- **Entering inspection mode writes tag-specific limits back into the editor
  def** — `InspectionUI.js:1766`, `:1789-1812`, `:1840-1844`. After one
  inspection run the editor reports unsaved changes the operator never made; if
  they save, the tag's limits are baked into the base def and the recorded def
  hash no longer matches any file on disk.
- **`machine_setting.json` has two writers that overwrite each other** —
  `InspectionUI.js:858-869` and `MAINUI.js:1522-1548` both write the whole file
  from a copy cached at connect time and never refreshed. Browser B silently
  reverts browser A's `InspectionMode`. Re-read before merging.
- **`loadedDefFile` is a load-time snapshot re-expanded on every save** —
  `InspectionEditorLogic.js:281`, `MISC_Util.js:228-234`. Any field changed
  outside the editor (hand-edited `.hydef`, a core-side migration) is quietly
  resurrected on the next save.
- **`UpdateInherentShapeList()` early-return** — FIXED in `bcfa8883`.

---

## Deliberately not changed — needs a decision, not a patch

- **`SC {"type":"exec"}` runs `popen()`**, and 4090 binds `INADDR_ANY` with no
  Origin check on the handshake — any page an operator visits can reach it
  (WebSocket is not subject to CORS). Same interface also offers arbitrary file
  write (`SV`) and directory listing (`FB`). `exec` is now opt-in behind
  `INSP_ALLOW_EXEC` (`d51624af`); the bind address and the missing Origin check
  are untouched, because restricting them changes how the machine is deployed.
- **`MT_LOCK` is a no-op** and naively "fixing" it deadlocks. Items 2.6 and
  others depend on it; the resolution is per-call-site, not global.
- **`ImageSampler.h:303-304` may add `origin_offset` twice** (again inside
  `factorSampling`, `ImageSampler.cpp:1024`). The caliper path passes image
  coordinates and is unaffected. Needs the origin_offset contract settled first.
