# visSele Architecture Reference

System topology, on-wire protocol, frontend↔backend interactions, and where to look in the build to follow each path. For a 30-day onboarding read, see [`TEAM_ONBOARDING.md`](TEAM_ONBOARDING.md). For step-by-step build/run instructions, see [`Core0_1/BUILD.md`](../Core0_1/BUILD.md).

---

## 1. System topology

Two coordinated C++ processes + a React WebUI, talking over WebSockets:

```
        ┌──────────────────────────┐
        │  visSele  (C++ core)     │
        │  InspectionCore/Core0_1  │
        │                          │
        │  ┌────────────────────┐  │
        │  │  CameraLayer       │  │  HikRobot / Aravis / MindVision / BMP_carousel
        │  ├────────────────────┤  │
        │  │  MatchingEngine    │  │  sig360 + circle + line + caliper + search_point
        │  ├────────────────────┤  │
        │  │  BPG_Protocol      │  │  binary framing
        │  ├────────────────────┤  │
        │  │  ws_server (libws) │  │  port 4090
        │  └────────────────────┘  │
        └─────────┬────────────────┘
                  │ shm ring (smem_channel)
                  ▼
        ┌──────────────────────────┐
        │  inspd_log               │  port 4091 (opt-in log fan-out + crash dump)
        │  InspectionCore/logctrl  │
        └──────────────────────────┘

        ┌──────────────────────────┐
        │  WebUI (React + Redux)   │
        │  UI/WebUI                │
        │                          │
        │  ws://core:4090   ◄──────┼── BPG: control, def files, image stream, reports
        │  ws://core:4091   ◄──────┼── logs (Core Logs panel)
        └──────────────────────────┘
```

* **Core ↔ WebUI** is one bidirectional WebSocket per peer. Multiple peers may connect at once; broadcast vs unicast is handled by the `pushToSubscribers` / `fromUpperLayer(peer)` split in `Core0_1/include/main.h`.
* **Log channel** is separate so a browser refresh won't drop log history.
* **Cameras** are abstracted by `CameraLayer`; one concrete implementation is loaded at runtime (`CameraLayerManager`). `BMP_carousel` is a fake camera that cycles through PNG/BMP files for offline development.

---

## 2. BPG protocol (the wire format)

BPG = "Binary Protocol Glue". Every WebSocket frame is one packet:

```
+-----+-----+-----+-----+-----+-----+-----+-----+-----+--- … ---+
| TL[2]                | prop| pgID[2]    | length[4]           | payload …
+-----+-----+-----+-----+-----+-----+-----+-----+-----+--- … ---+
  0     1     2     3     4     5     6     7     8     9        9+length
```

* `TL` (2 ASCII chars) — packet *type*. Eg `"HR"`, `"IM"`, `"GS"`, `"RC"`.
* `prop` (1 byte) — packet flags / sub-type, varies by TL.
* `pgID` (2 bytes BE) — *page id*, used to correlate requests with replies (set by the requester, echoed in the reply).
* `length` (4 bytes BE) — payload length in bytes. Unsigned; parsed with explicit multiplication to dodge sign extension.
* `payload` — `length` bytes. Usually a UTF-8 JSON string, sometimes binary (`IM` image payload).

Defined in `BPG_Protocol/include/BPG_Protocol.hpp` (C++) and `UI/WebUI/src/UTIL/BPG_Protocol.js` (JS). Stay in sync.

### TL reference

| TL | Direction | Purpose |
|----|-----------|---------|
| `HR` | core → UI | "Hello" / handshake. Sent on every new peer connect with `{version}`. |
| `SB` | UI → core | Subscribe / unsubscribe from the live inspection image+report stream. Default-first-peer subscribed automatically. |
| `GS` | UI → core | **G**et **S**etting. Generic get-many endpoint. UI sends `[{itemType:"camera_info"}, {itemType:"version"}, …]`; core replies with same TL and a flat object. |
| `SV` | UI → core | **S**a**V**e file. Used for def files, camera-setting JSON, calib snaps. |
| `LD` | UI → core | **L**oa**D** def / data file by path. |
| `LB` | UI → core | **L**oad **B**inary (e.g. thumbnail PNG). |
| `DF` | core → UI | **D**ef **F**ile push (sent after `LD` succeeds, or unsolicited on reload). |
| `FL` | core → UI | Failure / error payload tied to a request. |
| `FB` | UI → core | **F**ile **B**rowse — directory listing. |
| `FS` | core → UI | **F**older **S**truct reply (used by FB). |
| `II` | UI → core | **I**mage **I**nspection — run inspection on a single supplied image (used for "Quick Verify"). |
| `CI` | UI → core | **C**ontinuous **I**nspection mode start/stop. |
| `EX` | UI → core | Feature **EX**traction — sig360 etc. |
| `SG` | core → UI | sig360 / **S**i**G**nature report. |
| `IM` | core → UI | Live **IM**age frame. Binary payload (raw RGBA or JPEG). |
| `IR` / `RP` | core → UI | **I**nspection **R**eport / **R**eport **P**acket — JSON. |
| `RC` | UI → core | **R**emote **C**ommand. Generic verb-with-payload RPC. See §3.4. |
| `SC` | UI → core | **S**pecial **C**ommand (e.g. `exec` shell). Dev-only. |
| `ST` | UI → core | **S**tream **T**uning (downsample level, etc.). |
| `PD` / `PR` | core → UI | **P**ush data / **P**ush report variants used by peripheral channels. |

### `IM` packet layout (image frame)

`IM` payload is a 15-byte extra header followed by the image bitstream:

```
byte  0    format        (0=raw RGBA, 1=BGR JPEG, 2=grayscale JPEG)
byte  1    jpeg quality  (0 for raw)
byte  2-3  offsetX BE    (crop offset of this image within the full sensor)
byte  4-5  offsetY BE
byte  6-7  width BE
byte  8-9  height BE
byte 10    downsample scale (1 = full res)
byte 11-12 full_width  BE  (full sensor width before crop)
byte 13-14 full_height BE
byte 15+   payload       (RGBA bytes OR JPEG bitstream)
```

Defined in `InspectionCore/docs/IMG_TRANSFER_JPEG.md`. The WebUI decodes via `createImageBitmap` for JPEG (zero main-thread cost on Worker-capable browsers).

---

## 3. Frontend ↔ backend interactions

### 3.1 Connection lifecycle

```
   WebUI                                  Core (visSele)
   ─────                                  ──────────────
     │                                          │
     │ ─── WS connect ws://core:4090 ─────────► │ ws_callback OPENING
     │ ◄── HR {version}     pgID:0xFF ───────── │ ws_callback HAND_SHAKING_FINISHED
     │                                          │ peers.insert(peer); first peer also
     │                                          │ subscribed to live stream
     │ ─── GS {camera_info, version, …} ──────► │ (poll on a timer in BPG_WS.js)
     │ ◄── GS reply                  ────────── │
     │                                          │
     │ ─── LD {filename:".../def.hydef"} ─────► │ load def file
     │ ◄── DF {def file contents}    ────────── │
     │                                          │
     │ ─── CI {start:1, framerate:…} ─────────► │ start continuous inspection
     │ ◄── (stream) IM + IR per frame ───────── │
     │     IM binary, IR JSON, sent together    │
     │                                          │
     │ ─── browser refresh                      │ ws_callback CLOSING
     │                                          │ MT lock + dropPeerState + unsubscribe
     │                                          │
```

Multi-peer: each peer has its own pgID space. Image broadcast iterates `stream_subscribers` under a real `std::mutex` (the older `MT_LOCK` macro is a no-op). See `main.h:subscribersLock`.

### 3.2 Request/reply via pgID

For request/reply TLs (`GS`, `LD`, `LB`, …) the UI picks a unique `pgID`, sends, and listens for a reply with the matching `pgID`. The WebUI's tracking-window pattern is in `UI/WebUI/src/script.jsx` (`this.trackingWindow[id] = {resolve, reject}`). The core just echoes `bpg_dat.pgID = dat->pgID`.

### 3.3 Streaming inspection (CI mode)

Triggered by `CI` packet `{start:1, framerate:N}`. Core's `ImgPipeProcessThread` + `ImgPipeDatViewThread` (`wiringPanel.cpp`) push **per frame**:

1. `SS` `{start:true}` — frame-start marker (one).
2. `RP` — inspection report JSON, contains `sig360_circle_line.reports[]` with detected lines/circles/search-points/measurements.
3. `IM` — image frame (binary).
4. `SS` `{end:true}` — frame-end (optional).

`__surpress_display=true` flag on an `RP` tells the UI "we sent stats but no image this tick — don't redraw the canvas". See `InspectionUI.js` `insp_resolve`.

### 3.4 RC ("Remote Command") RPC pattern

`RC` carries `{target:"...", ...payload}` for one-off verbs. Common targets:

| target | payload | effect |
|--------|---------|--------|
| `camera_ez_reconnect` | — | tear down + re-init the current camera |
| `camera_setting_refresh` / `calib_files_load` | `{camera_setting_dir, lens_calib_path, field_calib_path}` | reload selected files |
| `lens_calibrate` | `{dir, out, lens_model, square_mm}` | offline chessboard calib → JSON |
| `field_calib_capture` / `field_calib_finalize` / `field_calib_clear_pending` | — | 16×16 B/D backlight uniformity capture flow |
| `bmp_carousel` | `{action: next/prev/replay/jump/pause/resume/setfolder/setfps/setaug, …}` | fake-camera drawer controls |

The core processes RC under the MT lock and replies via the GS path (the UI tags an `session_ACK` flag).

### 3.5 WebUI architecture (high-level)

```
script.jsx (root)
├── react-redux store
│   ├── reducer/UICtrlReducer.js   ◄── EVENT_Inspection_Report sink
│   └── actions/UIAct.js
├── comm/BPG_WS.js                 ◄── single WS connection driver
├── UTIL/BPG_Protocol.js           ◄── raw2obj / objbarr2raw, IM decode
├── DefConfUI.js                   ◄── definition editor
├── InspectionUI.js                ◄── live inspection screen
├── CalibrationUI.js               ◄── lens + field calib wizard
└── EverCheckCanvasComponent.js    ◄── shared image+overlay canvas
```

Key Redux mass: `state.UIData.edit_info` carries `inspReport`, `_obj` (the `InspectionEditorLogic` instance), `cameraParam`, plus per-frame stats. The whole canvas state derives from this slice.

---

## 4. Inspection pipeline (core side)

For each captured frame:

```
camera frame ─► CameraLayer extracts to imgPipe->img (with ROI applied)
              │
              ▼
       ImgPipeProcessThread
              │
              ▼
   ImgPipeProcessCenter_imp
              │
              ├── pre-process (rotate / mirror / brightness, BMP layer)
              ├── labeling                   (acvImage_ComponentLabelingTool)
              ├── feature extraction         (FeatureManager_sig360_circle_line)
              │     ├── line caliper / contour
              │     ├── arc / circle caliper
              │     ├── search_point         (sub-pixel centroid; lens-corrected)
              │     └── measure expressions
              ├── lens undistort hits         (Caliper.cpp caliper_undist_in_place,
              │                                lifts crop-local + user ROI)
              ├── status reduce              (per-object PASS/FAIL/NA)
              ▼
        ImgPipeDatViewThread
              │
              ▼ pushToSubscribers (under subscribersLock):
        SS / RP / IM packets to each subscribed peer
```

Key invariants:

* **Image coords are sensor-px.** The user's camera ROI shifts inspection-image (0,0) away from full-sensor (0,0). The sampler's `origin_offset` carries that shift; every per-pixel calibration lookup (lens map, backlight grid, mmpp) lifts inspection-frame px → full sensor by adding `origin_offset`.
* `eT.getImgOffset()` is the *internal labeling crop*, almost always (0,0). Don't confuse it with the user ROI.
* `cal_hits` for line/arc/circle are in **object-frame mm** (def coord system) after `PixDomain_TO_TemplateDomain`. Search-point `cal_hits` stay in raw image-frame px (visualization-only).

---

## 5. Calibration data flow

| What | Where it lives | Captured by | Applied by |
|------|----------------|-------------|------------|
| Camera mmpp / `ppb2b` / `mmpb2b` | `data/default_camera_param.json` (loaded at boot) and `cam_param` in WS reports | manual + `sig360_extractor` | every coord ↔ mm transform |
| Lens calib (k1/k2/k3/p1/p2 + principal pt) | `data/lens_calib.json` | `tools/calib_chessboard` or `RC lens_calibrate` from the wizard | `Caliper.cpp caliper_undist_in_place`, `search_point` final centroid |
| Field calib (16×16 bright/dark grid) | `data/field_calib.json` | `RC field_calib_capture` + `_finalize` | `ImageSampler::sampleBackLightFactor_ImgCoord` during edge profile sampling |
| Def file | `*.hydef` JSON, factory-shipped | `DefConfUI.js` editor | `MatchingEngine` parse + run |

All calibration models live in **full-sensor px**. The runtime caliper/search-point code lifts the local point with the sampler's `origin_offset` so that swapping ROIs doesn't shift the calibration anchor. See [`measurement_pipeline_and_caveats.md`](measurement_pipeline_and_caveats.md) for the deeper coord-frame story.

---

## 6. Build & run (summary; full guide in BUILD.md)

```bash
./InspectionCore/build.sh -p <preset> [-c Debug|Release] [-e <export-dir>] [--clean]
```

| Preset | Host | Notes |
|--------|------|-------|
| `mac-arm64` | macOS arm64 | Homebrew OpenCV; default for dev |
| `mac-arm64-opencv` | macOS arm64 | vcpkg OpenCV (matches Windows pipeline) |
| `linux` (= `linux-x64`) | Ubuntu/Debian | apt `libaravis-dev` etc. |
| `win-mingw` | Windows MSYS2/MinGW64 | vcpkg `x64-mingw-static`; HikRobot enabled |
| `win-cross` | macOS arm64 → Windows | mingw-w64 + vcpkg `x64-mingw-dynamic`; bundle is ~210MB |

Run flow:

```bash
cd InspectionCore/Core0_1
../build/mac-arm64/visSele      # binds 0.0.0.0:4090
# In another terminal
cd UI/WebUI && npm run dev      # serves the React WebUI; point at ws://localhost:4090
```

The WebUI auto-loads `data/default_camera_setting.json` + auto-applies any saved BMP_carousel folder / FPS / augmentation from localStorage (`BMPCarouselAutoBoot`).

---

## 7. Where to look first

| Symptom | Look here |
|--------|-----------|
| WebUI can't connect | `BPG_WS.js`, `script.jsx`'s reconnection watchdog, core `ws_callback` |
| Frame arrives but corruption | `BPG_Protocol.js:raw2Obj_IM`, `EverCheckCanvasComponent.js:SetImg`, IM header `format`/`width`/`height` |
| Inspection result drifts with ROI | `sampler->getOriginOffset()` plumbing; `caliper_undist_in_place`; search_point final-pt undist |
| Crash on browser refresh during inspection | `pushToSubscribers` + `subscribersLock` lifecycle (`main.h`) |
| Caliper hits wrong side on flipped object | `LineMatching_caliper`'s p0↔p1 swap under `flip_f<0` |
| Calibration buttons silently fail | `RC` target name vs core handler in `wiringPanel.cpp:checkTL("RC", …)` block |

---

## 8. Related docs

* [`Core0_1/BUILD.md`](../Core0_1/BUILD.md) — full per-platform build instructions.
* [`TEAM_ONBOARDING.md`](TEAM_ONBOARDING.md) — 30-day map of the codebase.
* [`RUNNING_CORE0_1.md`](RUNNING_CORE0_1.md) — runtime / configuration knobs.
* [`IMG_TRANSFER_JPEG.md`](IMG_TRANSFER_JPEG.md) — full IM packet spec + grayscale-JPEG selection.
* [`measurement_pipeline_and_caveats.md`](measurement_pipeline_and_caveats.md) — coord-frame transforms, mmpp, flip semantics.
* [`caliper_primitive_locating_design.md`](caliper_primitive_locating_design.md) — line/arc/circle caliper design.
* [`search_point_rework.md`](search_point_rework.md) — sub-pixel centroid + lens correction.
* [`CORE0_1_CAVEATS.md`](CORE0_1_CAVEATS.md) — backward-compat gotchas.
* [`LOGGING_WEBUI.md`](LOGGING_WEBUI.md) — log channel + Core Logs panel.
