# Headless-test fixture — the machine as it stood on 2026-08-07

Everything a headless core/uInsp test needs to reproduce this station's geometry
without the machine. Captured from the running system at the end of the
2026-08-07 session (core `3ae3426d`, WebUI `fe6ad5db`), with the plate stopped
and the board IDLE.

Every number below is a **snapshot of one machine on one day**, not a spec.
Re-read it from the machine before trusting it.

---

## 1. Coordinate frame — read this first

There are three frames and mixing them is the main way to get a wrong answer.

| Frame | Origin | Used by |
|---|---|---|
| **full-sensor px** | sensor (0,0), ignoring any camera ROI | `inspection_region`, `clean_regions`, the lens model |
| **inspection-image px** | the ROI's top-left corner | everything inside the locator, at `dsampLevel` |
| **object-frame mm** | the located part | report `cx/cy`, all measurements |

The core converts with

```
full_px = centre_in_image_px * dsampLevel + sampler->getOriginOffset()
```

where `getOriginOffset()` is the **camera hardware ROI origin**. So a test that
sets the region but forgets the ROI will place the station in the wrong place by
exactly the ROI origin.

---

## 2. Station geometry (`Core0_1/data/machine_setting.json`)

Verbatim, full-sensor px:

```json
"inspection_region": { "x": 1380, "y": 432, "w": 318, "h": 424, "fit": "contain" },

"clean_regions": [
  { "x": 1251, "y": 429, "w": 133, "h": 447,
    "dark_thresh": 128, "dark_area_max": 0.05, "on_fail": "na" },
  { "x": 1697, "y": 441, "w":  95, "h": 410,
    "dark_thresh": 128, "dark_area_max": 0.05, "on_fail": "na" }
]
```

**Layout:** the two clean regions sit either side of the inspection region —
`clean[0]` ends at x=1384 (region starts 1380), `clean[1]` starts at x=1697
(region ends 1698). They are the gaps between parts: the station is what is
being judged, the gaps have to be empty.

**`fit: "contain"`** = the object's whole bounding box must be inside the region.
The alternative is `"center"` (centroid only). Contain is the default and is why
a 318px-wide box can hold exactly one part: parts are ~350px and the feed spacing
gets down to 234px, so a second part cannot also fit.

**`dark_thresh` 128** = pixels darker than this count as "something there".
**`dark_area_max` 0.05** = mm² of dark allowed before the region is dirty.
Measured separation on this machine: **empty ~0.03 mm², occupied ~1.11 mm²** —
about 5000×, so the threshold is nowhere near either population.
**`on_fail: "na"`** = a dirty region forces the part's verdict to NA (it goes
round again); `"ng"` would eject it.

**FI ONLY.** In a CI session the region is published as zero-size, i.e. the same
path as a machine with no region at all. `--insp` applies it unconditionally
(it *is* a full inspection of one frame). See `CORE0_1_CAVEATS.md` §L.

---

## 3. Camera ROI — NOT on disk, it lives in the browser

`data/default_camera_setting.json` has **no `ROI` key**, so a core started cold
runs full-sensor. The inspection ROI is pushed by the WebUI when it enters
inspection mode:

```
ST { CameraSetting: { ROI: [x, y, w, h] } }      // full-sensor px
```

and the WebUI keeps it in **browser localStorage**, key `LS_INSP_ROI`
(`InspectionUI.js:74`). To read the live value, in the operator's browser:

```js
localStorage.getItem('LS_INSP_ROI')     // "[x,y,w,h]"
```

Measured origin during this session: **(1016, 328)**. Width/height were not
captured — read them from the key above, or from any report's
`station.roi_origin` plus the frame size.

**It is per browser profile.** localStorage belongs to the browser, not the
machine, so a second browser (or a private window, or a headless harness)
operating this station enters inspection mode with a *different* ROI — its own,
or none. Nothing on the machine records what the ROI should be, and nothing
notices the disagreement. The region coordinates above are full-sensor and so
are immune; anything reading `roi_origin` is not. Worth fixing by moving it into
`machine_setting.json` next to the region it has to agree with.

> For a headless test the simplest thing is to **set no ROI at all** (full
> sensor, origin 0,0) and use the region coordinates above unchanged. They are
> full-sensor, so they do not move. Only add an ROI when the test is
> specifically about the offset path.

Camera and optics:

```
model          Hikrobot MV-CA050-11UM  (USB3Vision, via Aravis)
full sensor    2448 x 2048
default_camera_setting.json   exposure 50us, gain 10, gamma 0.7, blacklevel 254,
                              framerate -1 (free), ww 1
def cam_param  ppb2b 73.191696, mmpb2b 0.648320962394
               -> mmpp = 0.0088578 mm/px   (def "10155  3G2570090B-1")
               RNormalFactor 2160, calibrationCenter (1302.5, 974.229)
```

Note the two mmpp values that appear in logs: **0.0088578** is the def's
calibrated one; **0.0138859** is what `camera_info` reports from the sampler
before any session has primed it. They are not the same number and only the
first is a measurement.

---

## 4. Peripheral (uInspESP32)

```json
"uInspESP32_peripheral_conn_info": {
  "uart_name": "/dev/cu.usbserial-0001", "baudrate": 230400,
  "machine_type": "uInspESP32", "cat_ok": 3, "cat_ng": 1,
  "cam_idx": 1, "pairing": "timestamp"
}
```

`cat_ok 3` / `cat_ng 1` are **selector categories, and smaller is worse** — SEL1
is the reject bin, SEL3 is pass, `PERIF_CAT_NA = 0xFFFF` means "do nothing, let
it go round". Nothing is wired to SEL1 on this machine.

Board config lives in the board's **NVS**, not here; the host copy is read-only.
Relevant setting for tests: `UNANSWERED_STOP_AFTER = 10` (consecutive unjudged
parts before error 2, `OBJECT_HAS_NO_INSP_RESULT`). `pairing: "timestamp"` pairs
frames to parts by clock rather than by position.

---

## 5. Reference def and images

```
def     Core0_1/data/10155  3G2570090B-1.hydef        (+ .png of the same name)
type    binary_processing_group -> sig360_circle_line
```

Golden pair used by the QA suite (a different part, telecentric bench shot):

```
/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.png
/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.hydef
```

---

## 6. Running it headless

**One frame through the full measurement path:**

```
cd <a dir containing data/machine_setting.json>
visSele --insp <image.png> <def.hydef> <out.json>
```

The offline path loads `data/machine_setting.json` deliberately, so the region
and clean regions apply exactly as they do live. That is what
`test_suite/qa/qa_insp_region.py` (14 checks) and `qa_objdetect_dark.py` drive.

The pattern those tests use, and the one to copy: build a scratch cwd with
`data/` symlinked and only `machine_setting.json` written fresh, so the live
machine's settings are never touched.

**The take-image / snap path** (needs a running core, no browser):

```
node UI/WebUI/tools/webctl/snap_probe.mjs 10 -1
```

Presses DefConfUI's 立即 ten times over the same wire the browser uses and
reports ACK + whether a frame actually came back. Press it **more than once** —
the failure mode it was written for only appeared on the second press
(`CORE0_1_CAVEATS.md` §M).

---

## 7. Verdict semantics a test must not get wrong

```
STATUS_SUCCESS   0      STATUS_FAILURE  -1
STATUS_NA     -128      STATUS_UNSET  -100
```

`InspStatusReduce` makes **NA absorbing**: once any judge says NA the object is
NA regardless of what else passed.

In the report, `station.result` is the verdict **as the machine receives it**,
`station.result_obj` is what the inspection alone produced. They differ whenever
a clean region or a guard intervened — `result -128` with `result_obj 0` means
"the part was fine, the field was not". `station.region.active` says whether the
station filter was actually enforced (FI) or only drawn (CI).

The full list of every branch that can change a verdict is
`InspectionCore/docs/UINSP_VERDICT_PATH.md`.
