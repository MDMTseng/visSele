# samples — a machine you can run without a machine

Everything here is synthetic and self-contained. Clone, build, run: no camera, no
controller board, no database, no calibration rig, no customer recipe.

That matters twice. It is what a **new platform** needs to answer "does this port
work?", and it is the only kind of sample this repository can carry — visSele is
public, and the machines it runs are not. The part in these images was drawn, not
photographed. `data/` on a real machine is gitignored on purpose.

There are two layers, and they are not alternatives. Start with the first.

| | what it proves | needs |
|---|---|---|
| `headless/` | the engine computes the right numbers | the core binary |
| `bringup/` | the whole product runs | the core + a browser |

---

## 1. Build

OpenCV is required. Everything else is optional, and on Linux the camera SDKs
default OFF-except-Aravis already:

```sh
cmake --preset linux-x64 -DFEATURE_ARAVIS=OFF
cmake --build InspectionCore/build/linux-x64 --target visSele -j
```

The preset name is **`linux-x64`** (`InspectionCore/CMakePresets.json`); plain
`linux` is not one and cmake will refuse it. Presets put the binary in
`InspectionCore/build/<presetName>/`, which is where the scripts here look. If
you would rather not use a preset:

```sh
cmake -S InspectionCore -B InspectionCore/build/linux-x64 -DFEATURE_ARAVIS=OFF
cmake --build InspectionCore/build/linux-x64 --target visSele -j
```

`-DFEATURE_ARAVIS=OFF` is worth doing for a first build. `CameraLayer_BMP` and
`CameraLayer_BMP_carousel` — the file-backed cameras everything here uses — are
compiled **unconditionally**, so a build with every camera SDK off still runs
this whole directory. Get that working before adding a GigE stack.

What the platform defaults are (`InspectionCore/CMakeLists.txt`):

| | Aravis | MindVision | HikRobot |
|---|---|---|---|
| Linux / macOS | **ON** | off | off |
| Windows | off | **ON** | **ON** |

MindVision ships prebuilt in `contrib/MindVision_GIGE/` for `win_x64`, `win_x86`
and `mac_x64` — there is **no Linux build of it**, and that is fine, because
Linux does not default to it. On Windows the MindVision DLL is a hard load-time
dependency of the process even for the offline path below, which touches no
camera at all; if the binary exits 127 naming `MVCAMSDK_X64.DLL`, that is this,
and the DLL is sitting next to the binary.

## 2. The headless gate — run this first

```sh
samples/headless/run.sh
```

One synthetic frame, one def, one answer, compared against pinned numbers:

```
objects: 1 (expected 1)
  cx_mm       15.3125            expected 15.3125            diff 0   ok
  cy_mm       12.7750005722046   expected 12.7750005722046   diff 0   ok
  rotate_deg  0                  expected 0.0                diff 0   ok
  scale       1                  expected 1.0                diff 0   ok
  similarity  0.99609375         floor    0.99                        ok
PASS
```

Exit 0 means the numbers match. It runs in an empty temporary directory — no
`data/`, nothing to set up — which is deliberate: if a future change makes the
offline path read machine state from disk, this fails and says so.

**Read the object count before anything else.** An empty report is not an error:
it means nothing was located, and `--insp` still exits **0**. A port with a
broken locator looks perfectly healthy until something checks that there is an
object in the report, which is what `run.sh` is for. To see why a report came
back empty, run the core with its log on stderr:

```sh
INSP_LOG_KEEP_STDERR=1 visSele --insp <image> <def> out.json
```

### What this gate covers

**The full locating chain, coarse plus ROI refine, from a def that carries its
own pixels.** `sample1.hydef` stores the ROI windows the refiner reads -- eight
56x56 patches, about 1 kB of PNG -- along with the point selection they belong
to. Nothing is read from disk to locate: `sample1.png` is the frame being
inspected, not the template. Upgrade an older def with
`UI/WebUI/tools/webctl/upgrade_defs.mjs`.

The pinned numbers can be judged rather than merely reproduced, because the
answer is known: `make_sbm_fixture_image.py` centres the part on the frame, so
the true position is (15.3000, 12.8000) mm in this def's frame.

| | cx error | cy error |
|---|---|---|
| coarse only | +1.0 px | -2.0 px |
| with ROI refine | -0.0016 px | -0.0035 px |

The 10 um tolerance is set against that gap on purpose: **a def that loses ROI
refine fails this check** rather than passing quietly, which is the failure mode
the gate exists for. Since 2026-09-02 the core also says so at load:

```
[shape] '<def>' loads from its cache and carries no roi_refine_points,
        so ROI refine will NOT run -- this def locates at coarse accuracy only.
```

Two things this sample is NOT. It is a **locator** fixture, not a metrology one:
the def's `mmpp` (0.0125) is nominal and does not match the scale the image was
actually drawn at, so positions in the def frame are exact but absolute
millimetres are not physical. And it runs `shape_weak_thres` /
`shape_strong_thres` at **30/30** where the core defaults to 50/80 -- worth
knowing before using it as a performance baseline, since `skip_voting` is
documented as safe only at the higher pair.

If a number moves for a reason you understand and accept, `run.sh --bless`
rewrites `expect.json` from the current run. Say in the commit message why it
moved; a blessed number with no reason is how a regression becomes the baseline.

### What the def needs, and how each requirement fails

Worth knowing before authoring another sample, because none of these announce
themselves:

1. **`featureSet[0].cam_param` must be present.** Without it the core keeps the
   scale at 1 mm/px and then refuses the def's own 0.0125 as
   `scale out of [0.2,5]`. The report comes back empty. `cam_param.mmpb2b`
   should equal the def's `mmpp`.
2. **The template image is found by the DEF's filename**, not by any key inside
   it. `sample1.hydef` trains from `sample1.png` beside it. Rename one and the
   locator silently falls back and finds nothing.
3. **`_ref_image_path` is deliberately absent.** It is an absolute path on
   whoever authored the def, and it must not travel; requirement 2 is how the
   template is found instead.

## 3. Bring-up with a fake camera

```sh
samples/bringup/run.sh              # core on 4090, working dir ./samples/bringup/run
```

It seeds a working directory from `bringup/data/` (never overwriting an existing
file), then starts the core with `FORCE_BMP_CAROUSEL` pointed at
`bringup/frames/` — a folder of images replayed through the **real** ingress:
the same trigger path, the same inspection, the same data view a camera would
drive. The frames are identical copies of the headless sample's image, so the
live path and the offline path are looking at the same part and must agree about
where it is.

Then serve the WebUI:

```sh
cd UI/WebUI && npm ci && npm run dev        # http://127.0.0.1:8081/
```

Open it. If it connects to a different core port than the one you started:

```js
localStorage.setItem('coreport', '4090'); location.reload()
```

Load the recipe `data/sample1` from the UI's recipe list.

**Verified on this bench:** the WebUI connects, `data/sample1` loads, and the
camera reports as `CameraLayer_BMP_carousel`.

### What `bringup/data/` is, and what it deliberately is not

`InspectionCore/Core0_1/init_data/` is the seed for a real machine, and
`visSele --init-data` copies it into `data/` without ever overwriting. It is not
enough to inspect anything: it seeds no def, no images, and **no
`lens_calib.json`** — so a core started from it alone measures at 1 mm/px and
locates nothing. This directory is that seed plus the missing pieces:

| file | why |
|---|---|
| `machine_setting.json` | fake camera, no peripheral, no database |
| `lens_calib.json` | `m = 72.015 px/mm` — the instrument these frames were drawn for |
| `sample1.hydef` + `.png` | the recipe and its template, same pair as `headless/` |
| `default_camera_setting.json`, `featureDetect.json`, `machine_info` | verbatim from `init_data` |

`machine_setting.json` names **no database and no peripheral**. The real file
carries three internal server addresses; a public sample must not, and nothing
here needs them — reports simply are not archived. There is no
`*_peripheral_conn_info` block either, so the UI reports the sorter as "not
configured" and never opens a serial port. Do not paste a real one in: the core
has exactly **one** peripheral channel, and any client that CONNECTs evicts
whatever was there.

## 4. Notes for a port

- **The log ring is named shared memory.** Two cores started with the default
  name share one ring: their logs interleave, and when one exits the other's
  drainer writes a spurious `producer died` crash dump. Both scripts here set
  `INSP_LOG_RING_NAME` per run for exactly this reason. Do the same in CI.
- **A one-shot CLI run still writes a crash dump.** `--insp` and `--init-data`
  exit normally and the drainer records `producer died (unexpected)` anyway. It
  is noise, not a fault — but it means "there are crash dumps" is not evidence
  of a crash.
- **`python3` on Windows may be the Microsoft Store stub**: executable, found by
  `command -v`, prints nothing, exits 49. `headless/run.sh` picks a python that
  actually runs rather than one that merely exists.
- The core resolves paths against its **working directory** and must be started
  from the directory that holds `data/`.
