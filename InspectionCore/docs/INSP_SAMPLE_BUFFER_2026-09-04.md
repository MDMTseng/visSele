# Kept inspection samples — why the WebUI version was removed, and what to build instead

**Status: IMPLEMENTED 2026-09-07 -- core (`insp_sample_*` in wiringPanel.cpp) + WebUI viewer (`component/InspSamplePanel.jsx`, 樣本 button on the inspection screen).** Design + measurements below are what it implements.
The WebUI implementation was built, run on a real machine, and removed the same
day. This records what it got right, the two things that killed it, and the
decisions already made so the core-side version does not re-derive them.

---

## The need

An operator watches a wrong verdict go past and has no way to look at it again.
The question is asked three parts too late, on the inspection screen, without
stopping the line — and it is usually narrower than "show me an NG": *"show me
the ones where measure 10 failed but measure 3 passed"*.

## What was built, and removed

A browser-side buffer (`UI/WebUI/src/UTIL/inspSampleStore.js`) fed from the
Redux report/image path, with a panel (`component/InspSamplePanel.jsx`) showing
thumbnails per filter group, the measurement overlay, and a save-to-`.xreps`
button. Removed in the same commit that added this document; recover it from
git history if the UI half is worth reusing (`d89227ad` and its parents).

### Why it cannot work from the browser

Both reasons are properties of the stream, not bugs that could be fixed:

1. **The frames are the DISPLAY stream, and it is down-sampled and lossy.**
   Observed on a real run: `584x259` out of a `2336x1036` sensor frame — a 4.00x
   reduction, then JPEG. Judging whether `1.2527` against a `[1.0, 1.2]` limit is
   a real defect or a measurement error needs the pixels the measurement was
   taken from. These are not those pixels, and no UI work changes that.

2. **The overlay did not land on the frame.** The exact cause was never
   established. Two candidates, both real: the pairing rule ("the core sends RP
   then IM inside one group, so the image that arrives next belongs to the
   reports that arrived last") does not hold for an IM that arrives with no RP
   in its group; and the stored `cam_param` describes the full sensor while the
   frame is a quarter of it. Not worth chasing — see below, the core has neither
   problem.

   Worth recording as process: the fixture that "verified" the overlay used a
   `321x287` record at `scale=1`, one object, no throttling. Ratio 1 masks a
   scale bug; one object masks a pairing bug. It could not have failed.

### Why the core does not have either problem

The core holds the full-resolution frame **and** the report that came from it,
in the same function, as the same object. There is no pairing question and no
scale mismatch: `saveInspectionSample()` already writes
`(report_json, camera_param, deffile, image)` as one self-consistent record, and
the disk snapshots it produces are exactly what `載入 xrep` reads back correctly.

---

## Decisions already made

| Decision | Choice |
|---|---|
| Grouping | Per-measurement conditions (`OK` / `NG` / `NA` / don't care) plus one on the part verdict. `OK`/`NG`/`NA` are presets, not built-in buckets — they were only ever one condition on the verdict. |
| Multiple matches | **First match wins**, like a firewall rule chain. One sample, one group, so "where did that one go" has one answer — and group ORDER becomes a real setting: narrow above, broad below. |
| No match | Dropped. The groups are the question; not matching is an answer. |
| Cap | **Fill and stop** by default, with a per-group opt-in to rotate instead. A ring loses the part you just saw: at this machine's rate a 20-deep bucket turns over in about a second. |
| Where the samples live | **Core memory. Not disk.** Transient evidence for the shift, not a retention policy. The existing disk snapshot path (`INSP_SNAP_POLICY`) stays what it is and is not replaced by this. |
| Where the config lives | `machine_setting.json`, read by `setup_machine_setting()`. Evidence policy follows the machine, not a browser — otherwise swapping laptops or clearing site data silently stops collection. Pushable live over `ST` the way `INSP_SNAP_POLICY` already is. |
| The viewer | A panel that lists what the core is holding and renders each record. |

---

## What the core implementation has to deal with

**Memory is the whole engineering problem.** A `2336x1036` gray frame is 2.4 MB
raw and a 5 MP one is 5 MB; three groups of 20 is 145–300 MB of raw frames,
which is not acceptable on this machine. Keep them **JPEG-encoded** — the core
already encodes for the display stream (`DataView_JPEG_quality`, default 85), so
a full-res frame is a few hundred kB and 60 of them is tens of MB. Fill-and-stop
bounds the number of encodes, so the CPU cost is paid once per kept sample and
then never again.

**Matching is per-FRAME, not per-part.** The UI version filed each located object
separately; the core's record is one frame image plus a `report_json` whose
`reports[]` holds every object in it. A frame should match a group if ANY of its
objects does. This is a real semantic difference and the UI must not present the
count as a per-part number.

**Matching is cheap.** `report_json` is a live `cJSON*`, not a string, so a
condition test is a walk over `reports[].judgeReports[]` — no parse. It can run
at the existing gate rather than after a queue.

### Verified hook points (`InspectionCore/Core0_1/wiringPanel.cpp`)

| Line | What is there |
|---|---|
| ~171 | `struct SnapPolicy` + `g_snap_policy[3]`, everything defaulting to off |
| ~181 | `snap_verdict_of(finspStatus)` — the 3-bucket classifier the group matcher replaces |
| ~3178 | `saveInspectionSample(report, camera_param, deffile, image, path, ...)` — what a record contains |
| ~3665 | `setup_machine_setting()` — where a new `machine_setting.json` key is read |
| ~7196 | the `ST { INSP_SNAP_POLICY: … }` handler — the pattern for pushing config live |
| ~11189 | the cheap gate: does anything want this frame at all |
| ~11258 | the save site, on `InspSnapSaveThread` — off the inspection hot path |

Note `InspSampleSaveMaxCount` (default 1000) is enforced by `removeOldestRep()`,
i.e. the disk path is a **ring**. The in-memory buffer is deliberately not.

### The WebUI side is mostly solved already

`RepDisplay` (`UI/WebUI/src/RepDisplayUI.js:149`) takes
`{def, camera_param, reports, image}` and draws the full overlay — search
points, fitted lines and circles, caliper hits. It is what the report-playback
screen uses and what `載入 xrep` was verified against. The viewer needs a
transport for "list what you are holding" / "give me record N", not a renderer.

Two things it must get right, both learned the hard way:

* `isCurObj` must be **true** on a stored report. The canvas draws
  `trackingWindow.filter(x => x.isCurObj)`, and a FINALISED report has it false
  — it means "matched in the frame being processed". Without the stamp the
  overlay draws nothing and looks exactly like the geometry having been dropped.
* the def must be a **fresh copy per record**: `rootDefInfoLoading` deletes
  `featureSet_sha1` off whatever object it is handed.

---

## Deliberately out of scope

**CI mode.** A CI verdict is settled when its object times out of the tracking
window — `keepInTrackingTime_ms`, one second on this bench — so the current frame
is a later one, quite possibly of a different part. For a tool whose only job is
explaining a verdict, a confidently wrong picture is worse than none. FI only
until a frame can be carried on the tracking entry itself.

---

## Core implementation (2026-09-07)

Config, in `machine_setting.json` and pushable live:

```json
"INSP_SAMPLE_GROUPS": [
  { "name": "m10 NG, m3 OK", "cap": 20, "rotate": false, "verdict": "*",
    "measures": { "10": "NG", "3": "OK" } },
  { "name": "any NG", "cap": 20, "verdict": "NG" }
]
```

`verdict` = frame verdict `OK|NG|NA|*`; `measures` keys are judge ids (`judgeReports[].id`),
values `OK|NG|NA|*`; all conditions must hold on the SAME object, any object in the frame
qualifies the frame. First matching group wins; no match = dropped. `cap` 1..200 (default
20), `rotate` false = fill-and-stop. Replacing the list empties the buffer.

* `ST { "INSP_SAMPLE_GROUPS": [...] }` replace live; `ST { "INSP_SAMPLE_CLEAR": true | "<name>" }`.
* `SL {}` -> `{groups:[{name,cap,rotate,verdict,count,items:[{id,ts_ms,verdict,jpg_bytes}]}], kept_total, dropped_full}`
* `SG {id}` -> `{id, ts_ms, group, w, h, report, def, jpg_fmt, jpg_b64}` -- report and def are the
  frame's own (raw JSON as kept), image is the full-res frame as JPEG (`DataView_JPEG_quality`),
  base64 so the viewer needs no second transport. Unknown id -> `{id, error}`.

Where it runs: the snapshot thread (`InspSnapSaveThread`), right after the frame ring push, so
the match + encode are off the inspection hot path; the datView gate feeds the queue whenever a
group still has room. Verified on the bench (`_samples_test.mjs`): 63 frames -> NG-only 3/3
(fill-and-stop), OK-rotate holds the latest 4, catch-all 5/5, dropped_full counted, SG returns a
decodable 2328x1035 JPEG (650 KB) with the report and def, clear empties all.

## WebUI viewer (2026-09-07)

`component/InspSamplePanel.jsx`, opened from the 樣本 button beside 旋轉標的 on the
inspection screen. Pure viewer: `SL` on open / 重新整理, `SG` on click, `ST INSP_SAMPLE_CLEAR`
for the clear buttons. Renders each record with `RepDisplay` (the playback component): the
record's own def (fresh copy), `reports[0].cam_param`, the report, and the full-res JPEG turned
into the IM-shaped image object (`jpegBytes`, scale 1) the canvas already decodes -- so the
overlay lands on the pixels the measurement was taken from, which is the whole point. Under the
picture: per object, the judges worst-first with id/name/value/OK-NG-NA, and the trust code if
one tripped. Group configuration stays in `machine_setting.json`; the panel says so.
Not exercised in a browser in this session -- parse-checked only; run it once on the bench.
