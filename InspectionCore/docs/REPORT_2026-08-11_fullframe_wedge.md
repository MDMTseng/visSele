# Full frame stops the camera dead — 2026-08-11

**Status: NOT SOLVED.** The cause is not identified. This records what was
measured, what was ruled out and how, and — at least as important — the four
conclusions that were drawn and then had to be withdrawn, each because the
instrument was wrong rather than the reasoning.

## The symptom

With the camera ROI at full frame (2448x2048) and the machine triggering, the
camera delivers a handful of images and then delivers nothing. No error is
raised anywhere. Every setting reads back correct — `TriggerMode On`,
`TriggerSource Line0`, `TriggerActivation RisingEdge`,
`AcquisitionBurstFrameCount 1`, the right ROI, the right exposure — and the
host's buffer pool stays healthy throughout.

It **survives a core restart**. Only `DeviceReset` (or a physical replug)
clears it:

    arv-tool-0.8 -n Hikrobot-<serial> control DeviceReset

Nothing may be holding the device when that is issued.

Frames delivered before the stop, across runs: **3, 11, 19, 21, 25**. It is not
a fixed count, and it is not a fixed elapsed time.

The production crop (560x452) does not do this. A 5-hour soak, a 12.5-minute
run and a 6.3-minute run all ran clean.

## Ruled out, and by what

| candidate | how it was excluded |
|---|---|
| USB link is 2.0 | `ioreg`: `MV-CA050-11UM  Device Speed = 3` behind a USB3.1 hub also at 3 |
| Bandwidth | Mono8 full frame is ~150 MB/s at 30/s; the same camera sustains 175 MiB/s free-running (`arv-camera-test`, 567 buffers, 2.84 GB) |
| BGR8 tripling the payload | Real and worth fixing (`INSP_CAM_PIXEL_FORMAT`), but the wedge reproduces on Mono8 |
| The ROI *change* mid-stream | Reproduces on a cold start at full frame with no UI client and no runtime ROI application |
| The UI pushing its own ROI | Same — no `fi_hold`, still wedges |
| The frame watermark | `PayloadSize` is 5013504 with FrameSpecInfo on and off; A/B wedged at 19 frames both ways |
| Host buffer starvation | `arv_stream_get_n_buffers` reads `n_input=8, n_output=0` on every frame up to and including the last |
| `TriggerMode()` re-application | Made a no-op when the mode is unchanged (12 calls skipped, no register writes, no stop/restart) — still wedged, at 11 frames |
| Trigger rate | A ladder at full frame delivered **30/30 at every rate from 20 to 36 fps**, 210 frames with zero loss, no wedge |

That last row also retires the camera's own `ResultingFrameRate` (35.18 fps,
28425 us) as a hard ceiling: 36 fps at 27777 us lost nothing.

## Withdrawn — four conclusions that were wrong

Each of these was stated with confidence and each was produced by a broken
instrument. They are recorded because the failure mode is the point.

1. **"SIZE_MISMATCH is the mechanism."** 27 `bufferStatus:5` entries were read
   out of a crash dump. The log ring **accumulates across core restarts**, so
   those belonged to other runs. A clean run with a dedicated stderr
   diagnostic showed no SIZE_MISMATCH at all in the triggered wedge. (This trap
   is already documented in PAIRING_MIGRATION_STATUS and was walked into
   anyway.)
2. **"The wedge is deterministic at 19 frames."** Two runs of 19 is not
   determinism. It is 3, 11, 19, 21, 25.
3. **"Creating the stream before SetROI is the bug."** Tested with a free-run
   reproducer — which was invalid (see below) — and the negative result was
   accepted. The reordering may or may not help; it has never been tested
   validly. The change is still in the tree.
4. **"The core free-runs one frame while `arv-camera-test` does 567, so the
   fault is ours."** The `INSP_CAM_FREERUN` switch bypasses the path that sets
   `takeCount = -1`, and the callback stops acquisition when `takeCount` hits
   0. It returned **exactly 1 frame at every ROI size from 2448x2048 down to
   the production crop** — including the size that demonstrably runs for hours.
   Everything measured through that switch is void.

The common thread: **the instrument was never checked against a known-good
case first.** One 30-second run of the production crop through the free-run
switch would have shown 1 frame and exposed it immediately. The same mistake
was made earlier the same day with a watermark A/B judged by `n_valid`, a
counter that only exists when the watermark is on.

## Still open

The wedge is pattern-dependent in a way that is not isolated. Same build, same
full-frame ROI, same day:

- rate ladder — 7 rates, 30 pulses each, ~2.5 s between: **210 frames, clean**
- confirmation run — 12 pulses, 1 s gap, sustained 45 s: **wedged at 25**

The instantaneous rate in the ladder was never lower than the confirmation
run's, so rate is not the difference. Round length, gap and total duration have
not been bisected against each other; that is the next measurement, and each
arm is about 40 s.

## What this means for the machine

Full frame must not go to production until the trigger is understood. The
failure is not a dropped frame — it is the whole camera stopping until a
DeviceReset, which on a running line means the machine images nothing while
appearing correctly configured.

The production crop is unaffected in every test run so far. Note the weaker
claim that is actually supported: the crop has never been *seen* to fail, and
the sustained-burst pattern that wedges full frame has not been run against it
for hours. "The crop is safe" and "the crop has not been pushed" are not the
same statement.

## Instruments left behind

All off by default, all stderr rather than the log ring — which was unreadable
at exactly the moments it was needed:

- `INSP_CAM_FRAME_TRACE=<path>` — one CSV line per delivered frame: camera
  timestamp, ExtTriggerCount, frame number, mean brightness. Counts frames
  without depending on the watermark.
- `INSP_CAM_TRACE_ROW=<y>` — which row the brightness is sampled from. The lit
  band is a property of the station, not a fraction of the frame.
- `INSP_CAM_PIXEL_FORMAT=mono8|bgr8` — the layer prefers BGR8 wherever it is
  offered, including on this monochrome body, which triples the payload.
- `INSP_CAM_TRIGMODE_ONCE=1` — make a repeat `TriggerMode()` a no-op.
- `INSP_CAM_FORCE_STREAM_REBUILD=1` — rebuild the stream once on first start.
- `INSP_CAM_FREERUN=1` — **do not use for anything until the `takeCount`
  interaction above is fixed.** It stops after one frame at every ROI size.
- Per-frame `n_input`/`n_output` pool trace, first 40 frames.
- `[camdiag] SIZE_MISMATCH buffer=N payloadSize=N camera_payload=N` — prints
  all three, because the interesting case turned out to be all three equal.
