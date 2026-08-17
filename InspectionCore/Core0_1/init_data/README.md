# init_data — the seed for a fresh clone

`data/` is this MACHINE's state: its calibration, its station geometry, its
identity, its snapshots. It is gitignored, and it should stay that way — a
machine's crop or its `machine_info` string arriving from someone else's
checkout is a defect, not a convenience.

This directory is the tracked seed that gets a new clone to a running core:

```sh
cd InspectionCore/Core0_1
../build/<platform>/visSele --init-data      # copies MISSING files into data/
```

**It never overwrites.** A file already in `data/` is this machine's, so
`--init-data` prints `keep` and moves on. That makes it safe to re-run, and safe
to put in a setup script someone runs twice.

## What is here, and what is deliberately NOT

| file | why it is seeded |
|---|---|
| `default_camera_setting.json` | exposure / gain / gamma / blacklevel — a working starting point |
| `featureDetect.json` | read by the EX blob-detect path |
| `machine_setting.json` | the SHAPE of the machine config: DB URLs, peripheral blocks, snapshot policy |
| `machine_info` | `UNSET` on purpose — see below |

**No `InspectionROI`** in the camera setting. The crop is where the part sits
when the camera fires; inheriting another machine's is worse than having none,
and the full sensor is the honest default. Set it from the Inspection UI's crop
gesture, which is the only writer (`save_insp_roi`).

**No `inspection_region` / `clean_regions`** in the machine setting, for the same
reason — they describe a station, not a product.

**`machine_info` is `UNSET`, not a real name.** It identifies the machine in
every inspection record. A clone that silently inherits `SLID008` or `DEV_uInsp`
files its results under someone else's name.

The peripheral blocks keep real-looking values (`/dev/cu.usbserial-0001`,
230400) because they are examples of the right SHAPE. Expect to edit the port
on any machine — it is a COM port on the Windows deployment.

## What used to be here

Removed 2026-08-18, ~6 MB of files nothing read: `CalibMap.bin`,
`cache_def.hydef`, `cache_def.png`, `cameraCalibration.json`,
`stageLightCalib.json`, and `default_camera_param.json` — the last of which the
core's own comments describe as gone (see `wiringPanel.cpp`'s note where
`saveInspectionSample` falls back to the frame's own calibration). They are in
git history if a 2022-era question ever needs them.

Calibration is NOT seeded and cannot be: `lens_calib.json` and
`field_calib.json` are measurements of one camera and one lens. Produce them
from the 相機校正 page.
