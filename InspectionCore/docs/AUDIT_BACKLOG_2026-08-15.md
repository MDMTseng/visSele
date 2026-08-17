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

### 2.1 `ws_conn`'s socket, `sendBuf` and connection pool are shared unlocked — FIXED (2026-08-16): per-conn `sendMutex` funnels every send (incl. main-thread PONG/handshake) and gates teardown; `doClosing` = shutdown-now + DEFERRED close/RESET via try_lock (never blocks the select loop; fd can't be recycled while a sender holds it; slot not reusable until finalized); pool `find()` locked against `push_back` realloc. Senders were already mutually serialized by BPG `linkLayerLock` — the missing halves were teardown and the main thread's own sends. Survived churn.mjs: 90 subscribed clients hard-destroyed mid-stream + a 5s-stalled client; stream recovered, core alive
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

### 2.6 `conn_peer` write side is unlocked, and the documented contract is not kept — FIXED (2026-08-16): the CLOSING handler now holds `bpg_pi.subscribersLock` across `peers.erase` + `default_peer` promotion decision + `conn_peer = NULL`, closing the loop with the UART reply thread's guarded conn_peer read; `subscribeStream`/`unsubscribeStream` moved outside the guard (they self-lock, non-recursive). MT_LOCK remains a no-op and remains labeled as such
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
- **FIXED (restore-on-exit; E2E untested — needs the menu entry flow + a tagged machine def, see REGRESSION_TESTS gaps)** Entering inspection mode writes tag-specific limits back into the editor
  def — `InspectionUI.js:1766`, `:1789-1812`, `:1840-1844`. After one
  inspection run the editor reports unsaved changes the operator never made; if
  they save, the tag's limits are baked into the base def and the recorded def
  hash no longer matches any file on disk.
- **PARTLY FIXED (the station panel now read-merge-writes only its two keys; MAINUI still writes whole-file from its cache — acceptable: it IS the settings editor)** `machine_setting.json` has two writers that overwrite each other —
  `InspectionUI.js:858-869` and `MAINUI.js:1522-1548` both write the whole file
  from a copy cached at connect time and never refreshed. Browser B silently
  reverts browser A's `InspectionMode`. Re-read before merging.
- **DEFERRED (needs a save-flow conflict check: LD the on-disk def, compare featureSet_sha1 vs load-time hash, confirm dialog on mismatch — a UX decision, not a patch)** `loadedDefFile` is a load-time snapshot re-expanded on every save —
  `InspectionEditorLogic.js:281`, `MISC_Util.js:228-234`. Any field changed
  outside the editor (hand-edited `.hydef`, a core-side migration) is quietly
  resurrected on the next save.
- **`UpdateInherentShapeList()` early-return** — FIXED in `bcfa8883`.

---

## Tier 6 — found by the 2026-08-16/17 review + broad-test sweep

Two adversarial review waves (8 agents) over the 08-15/16 sprint, then a
four-viewpoint "widest single test" design pass. What each turned up:

### 6.1 `PD` with no `"type"` key SIGSEGV'd the core — FIXED (`f76bb2a9`)
`Core0_1/wiringPanel.cpp` PD handler did `strcmp(JFetch_STRING(json,"type"), …)`
with no NULL guard, so any client sending `PD` without `"type"` (or non-JSON)
crashed the daemon. Identical bug/fix to SC at `:4891`; PD was missed. Found by
`bpg_sweep.mjs`; negative control (revert + rebuild) reproduced the crash
(liveness lost, :4090 gone), patched core survives 34/34.

### 6.2 `websock_data.type` uninitialized in the link layer — FIXED (`703e1b9b`)
`BPG_Link_Interface_WebSocket.cpp` fromUpperLayer left the outer `type` field
stack-garbage; `ws_conn::send_pkt` branches on it first. Garbage == CLOSING (7)
would run `doClosing()` on a SENDER thread inside its own subscribersLock
(self-deadlock) and break the deferred-close design's main-thread-only
invariant. One-line init. The whole WS lock rework's soundness rested on it.

### 6.3 `pokeNow()` forked poll chains (camera doorbell) — FIXED (`703e1b9b`)
`script.jsx` Cam_Stat_Query: a doorbell landing while a camera_info query was
in flight cleared a stale timer and started a SECOND poll chain; chains only
accumulated (flapping camera → one leaked per second). inFlight + pokePending
coalescing. Found independently by two review agents. The same shape was then
avoided by construction in the perif-link poll (`96214eed`).

### 6.4 `usePerifConn` minted a fresh object every render — FIXED (`703e1b9b`)
`perif/PerifAPI.js`: the non-memoized return turned MAINUI's readiness effect
into a setState loop spinning at commit speed for the whole "camera
reconnecting" window on a uInsp machine. Memoized on the store snapshot.

### 6.5 WriteBytesToFileAtomic had no fsync on Windows — FIXED (`703e1b9b`)
`common_lib/Util.c`: the durable path was fflush-only on the DEPLOYED platform
(fsync was `#ifndef _WIN32`), so a power cut after the pre-rename `remove` could
lose both the old and the new file. `_commit` added.

### 6.6 lens_calibrate green-ACKed a fit the load-side guard refuses — FIXED (`703e1b9b`)
`Core0_1/wiringPanel.cpp`: `session_ACK = r.ok` (LM "converged", not "sane"),
then `load_lens_calib(out)`'s false return was ignored — operator sees success,
the OLD calibration silently stays active, next restart has none. Reloading
through the guard IS the produce-side validation now.

### 6.7 `abs`→`fabsf` in ContourGrid shifts real measurements on Windows — OPEN (deploy gate)
`MatchingEngine/ContourGrid.cpp:380,:914`. libc++ (Mac) already had float `abs`,
so the golden proves nothing about it; old MinGW `int abs(int)` truncated, making
the 0.15 curvature filter near-inert. A/B (08-17, replicating the old int-abs on
the Mac, `--insp` leaf diff): 10321 bit-identical, but **10155 (factory def) line
fit loses ~19 points and judge #11 moves 1.9460 → 1.9922 mm (+46 µm)**. The new
filter is correct; the risk is that factory limits were tuned against the inert
one. **Before the next Windows deploy: revalidate affected defs' limits.**

### 6.8 `doClosing`'s CLOSING callback can stall the select loop ≤5s — OPEN (deferred)
`ws_server_util.cpp` doClosing fires the CLOSING callback synchronously, which
takes `subscribersLock`; `pushToSubscribers` holds that lock across a full image
send (≤5s SO_SNDTIMEO). So a disconnect can head-of-line-block accept/recv for
one stuck subscriber's timeout. Same bound as 2.8, bounded not unbounded. The
naive fix (snapshot the set, send outside the lock) reopens a wrong-stream
window on slot reuse; proper fix is generation-stamped peers, with the 2.8
lock-structure rework.

### 6.9 safeSend killed a healthy connection on EINTR — FIXED (`703e1b9b`)
`ws_server_util.cpp`: `written == -1` was unconditionally fatal, but EINTR
(a caught SIGINT/SIGTERM delivered mid-send, zero bytes moved) leaves the
stream aligned. Now retried.

### 6.10 station-save fell back to the cached whole-file copy — FIXED (`703e1b9b`)
`InspectionUI.js`: on an LD re-read failure the handler wrote the connect-time
cache as the whole file — the exact browser-B-reverts-browser-A bug the
read-merge-write exists to kill, resurrecting other writers' deletions. Now
refuses (message + no save) instead.

### 6.11 10s perif poll flipped a locally-held SUSPECT green — FIXED (`96214eed`)
`perif/PerifAPI.js`: the poll promoted SUSPECT→CONNECTED on core-side
`suspect:false` without consulting the local PING watchdog, flapping the chip
every ≤10s during the outage it exists to show. Tagged `suspectSrc`; the poll
only promotes what it demoted.

### 6.12 `soak.mjs` reads `region_dropped` at the wrong path — OPEN (cheap)
`UI/WebUI/tools/webctl/soak.mjs:52` reads `j.station.region_dropped`, but the
core emits `region_dropped` at RP top level (`FeatureReport_UTIL.cpp:523`), so
soak's dropped counter always reads 0. One-line fix. (Found by the cross-layer
test-design pass.)

### 6.13 `CameraLayer_BMP.cpp` cache-hit path never assigns `ret` — OPEN (latent, benign)
`CameraLayer/CameraLayer_BMP.cpp:427-445`: a decode failure on the cache-hit
path is only reported via the load path. Fake-camera only, harmless today.

### 6.14 `INSP_PERIF_PCNT_SLIP` fault injection is a no-op on the fake camera — NOTED
The carousel never stamps `pcnt` (always -1), so the pairing-disagreement path
is untestable without a real camera or a carousel pcnt stamp. Not a defect;
a test-coverage hole to remember.

## Tier 6b — found by the 8-agent re-review (2026-08-17)

Eight agents re-checked this sprint's commits, tests, and docs. All four
commit clusters (BACPAC P0, WS locks, doorbells, WebUI runtime) verified
CORRECT. What the review turned up:

### 6.15 `_queryCam` reply-with-no-GS wedges the camera poll forever — FIXED (`script.jsx`)
The camera poll's resolve callback only acted `if(GS!==undefined)`; a reply
carrying no GS packet ran neither resolve nor reject, so queryCam's `_next`
never fired, `inFlight` stuck true, and the whole camera poll chain died
silently and permanently. Pre-existing (not from the doorbell work, which
merely made the stuck-inFlight state reachable). Fixed: a GS-less reply now
takes the reject/retry path. (PerifAPI's `try/finally` was the pattern.)

### 6.16 station-save refuse left a misleading dirty half-state — FIXED (`StationRegionPanel.jsx` / `InspectionUI.js`)
Exposed by 6.10's fallback→refuse change: the panel cleared `dirty`
unconditionally after `onSave`, so a REFUSED save disabled the Save button and
hid 放棄 while the "請重試" toast said otherwise — and `onApply` had already
pushed the region live, so live state diverged from disk with no "未存檔"
indicator. Fixed: `onSave(setting, onDone)` reports success; `setDirty(false)`
is now gated on `onDone(true)`.

### 6.17 perif SUSPECT can latch orange on an idle-but-healthy link — OPEN (core-side, medium)
A link suspected by 3 failed verdicts stays SUSPECT because the WebUI clear
(`PerifAPI.js` `_sendPing` recovery) requires the CORE's `g_perifLinkSuspect`
to be false, and the core only clears that on a successful tx or a port
reopen. On an idle machine no verdicts are sent, so the flag — hence the
orange chip — persists on an actually-healthy link until the next real verdict
or a manual reconnect. Pre-existing; a clean fix needs a core-side idle-clear
(e.g. clear suspect after N successful PINGs, not only verdicts).

### 6.18 bare `perifCH` read in the perif doorbell — NOTED (LOW, benign)
`CamStateWatchThread` / `pushPerifStateDoorbell` read `bpg_pi.perifCH != NULL`
without `perif_tx_lock` (which `delete_PeripheralChannel` swaps under). Both
sites only TEST for NULL, never dereference, and the delete is of a different
captured pointer — worst case reads a stale non-NULL that is never touched.
A stricter fix is `std::atomic<PerifChannel*>`; not required for correctness.

### 6.19 test false-greens in cycle.mjs — FIXED (all three)
The 8-agent pass caught three cycle.mjs assertions that passed without testing
what they claimed: (a) the inst-check required `sig360info.reports.length>0`,
but loading the def SEEDS that from its embedded signature, so EX's own effect
was never observed — fixed by clearing sig360info before EX and requiring
repopulation; (b) def-hash "stability" re-read the hash from a freshly loaded
file each lap, wiping the round-trip's in-memory mutations before the read —
replaced with a within-lap full-limit-set round-trip snapshot; (c) the NG-range
check accepted ANY change, not the tag's specific 16.47 — now asserts the exact
value. Plus toMain's SPLASH socket-kick made robust (fresh state, >10s
threshold, skip while CONNECTING) and doorbell.mjs's suppression window widened
15s→40s with a best-effort camera_state currency cross-check.

---

## Tier 7 — 8-agent audit round 2 (2026-08-17): crash/lock/memory/perf

Eight agents (4 × crash/lock/memory/race, 4 × hotpath/measurement/WebUI perf).
FIXED in `6541c07b` (crash/race/UB) + `2d43370e` (perf quick wins); everything
verified with the pre/post `--insp` gate (10321/10155/10221 bit-identical) and
the live suite. Items below are the OPEN remainder, ranked.

### Fixed (for the record)
camera-NULL family in II/CI/FI/EX after a failed reconnect (getImage + session
guards); the derived ws_callback's reintroduced `raw[rawL]=NUL` write (base had
removed it with full diagnosis — the override shadowed the fix); calib/station
config swap races (loaders under `matchingEnglock`, new `g_station_cfg_lock`
with snapshot reads — the wrong-verdict class); PerifPingThread pre-lock UAF
read; `lastDatViewCache_lock` bare across imwrite (throw = permanent pipeline
hang); `resourcePool::retResrc` per-frame unowned unlock (UB, fatal on
winpthreads) + dead `fetchResrc_blocking` deleted; `ReadJson` delete-on-malloc
+ `ReadText/ReadByte` ftell(-1) overflow; SnapFrame callback/context swap race
(`cb_swap_m` + `invokeFrameCallback` at all 14 driver sites — the "立即拍照
during FI" SIGSEGV); Aravis destructor now drains in-flight stream callbacks
(the bench-green/factory-crash asymmetry — carousel/Hik join, Aravis had
nothing); stream-rate RP `cJSON_PrintUnformatted`; machine-hash hexed once.

### 7.1 FIXED 2026-08-18 — JPEG encode hoisted out of the send locks
The fan-out is `subscribersLock { for peer: fromUpperLayer -> linkLayerLock ->
SEND_acvImage }`, and `SEND_acvImage` did the `cv::imencode` itself -- so the
encode ran INSIDE both locks, once PER PEER, on every frame.

**Fix**: `encode_acvImage_jpeg()` extracted and called ONCE at the call site,
before `pushToSubscribers`; the encoded bytes + fmt + the quality they were
encoded at travel in `BPG_protocol_data_acvImage_Send_info`. `SEND_acvImage`
frames the cached bytes when present and still encodes inline otherwise (the
command paths -- LD thumbnail, calibration snap -- send once to one peer, so
there is nothing to hoist). The quality travels with the bytes because an ST
can change `DataView_JPEG_quality` between the encode and the send, and the
metadata header must describe the bytes actually going out.

**Measured** (full sensor, q85, ~500KB JPEG, 2 subscribers, 10/s x 45s,
identical harness `tools/webctl/fullframe_run.sh`):

| | img avg | img max |
|---|---|---|
| before | 14.2 ms | 76.9 ms |
| after  |  4.9 ms | 12.7 ms |

−65% average, −84% worst case, and that whole span used to be held under both
locks. One subscriber after the change measures the same 4.9 ms avg as two,
which is the per-peer multiplication gone.

**Wire output proven identical**, not assumed: `INSP_IM_ENCODE_VERIFY=1` makes
`SEND_acvImage` re-encode inline and `memcmp` against the cached bytes. 11
sampled comparisons over ~550 frames, 0 mismatches.

Note for whoever reads the original 12-20ms estimate: that is the FULL-SENSOR
cost. At the production ROI crop (560x508, ~31KB JPEG) the encode is ~0.4ms,
so this change matters most to the editor/calibration/InstInsp paths and to
session start, not to the cropped production loop.

Still open from the original entry: the lock-scope rework itself (2.8/6.8) --
the blocking send (SO_SNDTIMEO 5s) is still inside `linkLayerLock`, so one
stalled client still parks the others. The encode is simply no longer part of
what it parks.

### 7.2 FIXED 2026-08-18 — NG-snapshot saving was silently dead
`cache_camera_param` (wiringPanel.cpp:132) is never assigned anywhere;
`saveInspectionSample` requires `reports[0]` from it → returns −11 → every
snapshot save (WS path AND snap thread) failed silently. Functional regression,
not a leak.

**Confirmed, not inferred**: every dated folder under `data/SAMPLE/` from
20260812 to 20260817 contained **0 files**, while the log printed
`SAVE::<path>` for each one — the call site discarded the return value. It
surfaced the hour the save started reporting its own result (see the log-census
work in the same commit): 972 × `snapshot WRITE FAILED (-11)` in a 20s run.

**Fix**: the legacy global stays honoured if anything ever sets it, but the
fallback is the frame's own calibration — `FeatureReport_UTIL` already writes
`cameraCalib2JSON`'s output to `reports[0].cam_param`, and that is the same
`{"type":"camera_calibration",...}` object the historical `.xreps` files carry
under `"camera_param"`. Verified: same 20s run now writes files, and the
`.xreps` `camera_param` reads
`{"ppb2b":1,"mmpb2b":0.0138859432190657,"exposure_time":50}`.

`data/default_camera_param.json`, the original source, has not existed for a
long time — see the stale-claims table in `docs/SYSTEM_MAP.md` §8.

### 7.3 OPEN — measurement-pipeline bit-exact recoveries (~2-3ms of 8.5ms)
Ranked, all verified as bit-exact by the perf agent: (a) per-frame camera
REGISTER reads on the measurement thread — `cameraCalib2JSON` GetExposureTime
(0.5-10ms device I/O!) + the GetROI 4-read fallback when offset==0,0 (cache in
CameraLayer, invalidate on set/reconnect); (b) full-frame extractChannel
re-derived up to 5× (ROI-first + member buffers); (c) dead `roughness()`
passes feeding commented-out logs (sig360 ~3088, ~1302, ~4536); (d) binarize
LUT + per-column tables + parallel_for (BinarizeCV/CvBridge); (e) caliper
undistort-on-hit moved to miss branch + batched + index-ordered parallel;
(f) v2 signature build re-labels and computes ALL labels (~1900 speck frames);
(g) `ContourFetch` by value → const& (100-400KB/candidate); (h) hot-loop
`getenv` → static (pattern exists at sig360:7480); (i) xrefine template DFT
cached at parse. Behavior-touching (golden-gate first): shape-path crop before
match, circle-caliper rectify-once, ContourGrid acceleration, 24-bit walker.

### 7.4 OPEN — WebUI stream-rate fixes
(a) `reqWindow` never cleaned on disconnect (`BPG_WS.js:491`): leaks in-flight
entries (incl. IM ArrayBuffer views), permanently kills the camera poll
(`inFlight` stuck — the OTHER half of 6.15, disconnect-shaped), and a full
window hard-hangs `send()`. Fix: reject+clear on close. (b) ImageBitmap not
`.close()`d on frame swap (`EverCheckCanvasComponent.js:341-357`) — GPU/native
pile at ~6 fps. (c) `edit_info` new identity per WS message → RepDisplayUI
redraws unconditionally per report; gate like APP_INSP_MODE already does.
(d) `shape_fingerprints` JSON.stringify per report (cache per def edit);
1Hz whole-app re-render from `WS_UPDATE`; IndexedDB pending queue unbounded
when insp-DB is down (durability trade-off — needs owner sign-off).

### 7.5 OPEN — camera layer, data integrity + stalls
(a) Aravis `Trigger()` borrows TriggerSource (Software↔Line0) — a hardware
edge in the window is silently dropped → frame↔part pairing shifts by one
(the 449-vs-213 drift is the documented extreme). (b) CamStateWatchThread
holds `camera_lifetime_lock` across `arv_camera_get_integer` — a dying GigE
link stalls inspection at exactly the wrong time (cache/timeout). (c) GS
camera_info touches the camera WITHOUT `camera_lifetime_lock` — safe only by
WS-thread confinement; must be fixed before comm-refactor Phase 4 multi-thread
dispatch. (d) Hik `SetFrameRate`/Aravis `SetROI` clamp against uninitialized
maxima when the get fails. (e) Bayer GR decoded with GB pattern (wrong colors,
not a crash).

### 7.6 OPEN — smaller memory/race items
Sub-feature sibling leak when a later ctor throws mid-parse (group dtor never
runs); `shape_cache_in` dangles after failed reload (latent);
`GetReport()` hands out reports whose def pointers a reload invalidated;
`reportDataPool` ratchets to the worst frame ever; WS-thread
`__LAST_DATA_VIEW_CACHE_INFO__` mutates `cache_deffile_JSON` without
`snap_cfg_lock` vs the snap thread's Duplicate (per-coincidence UAF);
`ws_conn::sock` plain-int cross-thread (make atomic to make the invariant
real); PerifProt 8-byte load at +2 (misaligned/aliasing UB, works on our
targets); `ImageStackAddUp` self-deadlocks if anyone re-enables stacking;
`RingBuf` locking UB (CoreHub-only); FI `payload_file` deliberate def-tree
leak (fine for the tool, wrong if scripted).

### Environment regressions noticed this round (NOT code)
- doorbell.mjs phase 3 (perif doorbell) now FAILS on both pre- and post-change
  binaries — a REAL usb-serial device (`/dev/cu.usbserial-0001`) is attached
  to the bench, changing PD CONNECT behavior. Do not hammer PD paths while the
  physical board is attached (serial open = DTR power-cycle).
- churn.mjs freeze 29-52s (limit 20) on BOTH binaries — environmental (loaded
  Mac and/or the attached serial device's 5s timeouts); previously 16s ×3.
  Re-baseline on a quiet bench before treating as a regression.

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
