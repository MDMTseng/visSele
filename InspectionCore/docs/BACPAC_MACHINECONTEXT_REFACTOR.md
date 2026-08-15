# Inspection-path context refactor (BacPac + flying globals)

Status: **PLAN ONLY — not implemented.** Date: 2026-08-15.
Updated 2026-08-15 (later the same day): the Tier-1 audit fixes (`0338671f`,
`5548f454`) landed parts of P3 and P5 ahead of this plan — see the strikethrough
notes in those phases. The remaining work is unchanged.
Scope: everything that can change **whether a frame is inspected, how it is measured, or what verdict the sorter sees**. Engine ABI (`FeatureManager_BacPac*`) stays.

**Out of scope:** loading a def (`LD` / `FI`/`CI` `definfo` / `AddMatchingFeature` / sidecar PNG train). The def is the product. This plan is the machine + pipe around it.

BacPac is already supposed to be the machine bag. It is not the only bag. Frame admission, gray prep, ROI origin, verdict reduction, and peripheral mapping all live as file-scope names, `getenv` caches, or literals in `ImgPipeProcessCenter_imp`. Same bug class as BacPac: write one copy, read another, or a number with no owner.

Companion: [UINSP_VERDICT_PATH.md](UINSP_VERDICT_PATH.md) (every branch that can alter a verdict), [AUDIT_BACKLOG_2026-08-15.md](AUDIT_BACKLOG_2026-08-15.md) item 1.3, [CORE0_1_CAVEATS.md](CORE0_1_CAVEATS.md) §L, [ARCHITECTURE.md](ARCHITECTURE.md) §5.

---

## 1. Goal

One process-owned `InspectionContext` holds machine + pipe + verdict-policy state for the live inspection path. The matching engine still takes `FeatureManager_BacPac*`. Two BacPac **views** remain (production vs authoring).

Success:

- After each phase, `--insp` on the golden def is byte-identical.
- Machine twins (`g_insp_region`, `g_clean_regions`, `g_full_inspection`, `g_area_gates_bypass`, `g_lens_calib`, `g_field_calib`) cease to exist as file-scope names.
- Pipe / verdict knobs that currently fly (`imageQueueSkipSize`, `img_transpose`, `skip_inspection()`, `cat_ok`/`cat_ng` *as used on the verdict path*) have one owner.
- `ignoreCalib` cannot stick on the production sampler (AUDIT 1.3).
- Grep for inspection-path policy hits `InspectionContext`, not a scatter of `g_*`, `getenv` IIFEs, and literals.

Non-goals:

- Changing millimetre results, def schema, or BPG wire format.
- Collapsing the two samplers.
- Persisting `area_bypass` into `machine_setting.json`.
- Implementing `backPackLoad` / `backPackDump`.
- Full DI through every BPG handler.
- Def-file load / train / `__shape_cache`.
- Display-only knobs (`downSampLevel`, `ImageCrop*`, JPEG quality) except to *name* them so they are not mistaken for match inputs.
- Debug dump env (`SHAPE_DBG`, `CALIP_DUMP`, `SP_PT_DUMP`, …) unless it **changes the answer**.

---

## 2. The path this plan covers

```
camera callback
  → admit or drop the frame          (imageQueueSkipSize, pool empty)
  → gray prep + transpose            (img_transpose)
  → stamp ROI origin + station       (originOffset, g_insp_region, g_full_inspection, bypass)
  → FeatureMatching                  (BacPac sampler/lens/field; engine literals)
  → reduce judges → one status       (InspStatusReduce, srep.size()==1, skip_inspection)
  → clean_regions fold-in
  → map status → selector category   (cat_ok / cat_ng / PERIF_CAT_NA)
  → maybe replace the number         (INSP_PERIF_VERDICT_PATTERN and friends)
  → send / snap / preview
```

`--insp` joins at FeatureMatching (no camera admit, no perif). II joins the same way on `neutral_bacpac`.

---

## 3. Current split

| Place | What it actually holds | Who reads it |
|---|---|---|
| **Def** | Product: features, judges, `cam_param` scale | Engine parse / train. **Not this plan.** |
| **`FeatureManager_BacPac`** | `sampler`, `cam`, `lensCalib*`, `fieldCal*`, `insp_region_*` | Engine at match time |
| **`wiringPanel.cpp` file-scope** | Station, session, calib blobs, skip sizes, transpose, snap flags | Live pipe, reports, `--insp` |
| **`bpg_pi`** | `camera`, `perifCH` (`cat_ok`/`cat_ng`), `resPool` | Callback, send thread, GS |
| **`getenv` IIFEs** | Skip inspection, pairing, verdict replace, fault inject | Frozen at first call |
| **Literals in the match/reduce** | `areaThres = 100*dsamp²`, `srep.size()==1`, NA-absorbing reduce | Every frame, no knob |

Two BacPac instances (keep both):

- **`calib_bacpac`** — live FI/CI pipe. `headImgPipe` always points here.
- **`neutral_bacpac`** — II / `--insp` / `--convert`. Separate sampler so CHECK cannot poison production scale.

---

## 4. Inventory — inspection path only

All sites `Core0_1/wiringPanel.cpp` unless noted. **Verdict** = can change PASS/FAIL/NA or which part is judged. **Admit** = can drop the frame before a verdict exists. **Measure** = can change millimetres. **Display** = UI/snap only.

### 4.1 Machine bag (BacPac twins)

| Symbol | Site | Class | Notes |
|---|---|---|---|
| `calib_bacpac` / `neutral_bacpac` | `:1463` | measure | Two views, shared lens/field pointers, separate samplers |
| `g_lens_calib` / `g_field_calib` | `:1469` | measure | Owned blobs; both BacPacs alias them |
| `g_insp_region` | `:2221` | verdict | Copied onto `bacpac.insp_region_*` **every live frame** (`:8486`) |
| `g_clean_regions` | `:2303` | verdict | Never on BacPac; `eval_clean_regions` reads the global |
| `g_full_inspection` | `:2235` | verdict | FI publishes region; CI publishes zero-size |
| `g_area_gates_bypass` | `:2256` | verdict | ST + `INSP_AREA_BYPASS`; must not be persisted |
| `sampler->ignoreCalib` | each sampler | measure | Sticky on **production** sampler (AUDIT 1.3) |
| `g_pending_*` field-calib capture | `:1478` | measure | Scratch next to the live calib |
| `g_calib_autoloaded` | `:1567` | measure | `camera_info` flag |

### 4.2 Frame admission (before match)

| Symbol | Site | Class | Notes |
|---|---|---|---|
| `imageQueueSkipSize` | `:86`, set at CI/FI open `:4211/:4235` | **admit** | CI `= 1` → queue of 2 already sheds frames. FI `= capacity` → never. Logged `skip image`. |
| `doImgProcessThread` | `:1442` | admit | If false, callback does not enqueue inspection |
| `doInspActionThread` | `:1443` | admit | Parallel flag; same family |
| `poolEmptyDropCount` / empty `resPool` | callback `:6194` | admit | Drop this frame; do not stall the camera |
| `inspQueueDropCount` | | admit | Counter only |

Admission is why CI and FI disagree on the same machine with the same def. It is not on BacPac today.

### 4.3 Frame prep (the pixels the engine sees)

| Symbol | Site | Class | Notes |
|---|---|---|---|
| `img_transpose` | `:124` | **measure** | Applied to the working gray **before** match (`:8399`). A leftover true inspects a rotated world. |
| Camera `InspectionROI` | `default_camera_setting.json` / ST | **measure** | Hardware crop. Sampler `originOffset` is updated per frame from `fi.offset` or `GetROI()` (`:8463`). Station coords are full-sensor and add this offset. |
| `ImageCropX/Y/W/H` | `:1436` | display | Preview crop for `IM` send, **not** the match. ST currently zeros them then maybe sets them (`:5022`). Easy to confuse with InspectionROI. |
| `downSampLevel` / `downSampWithCalib` | `:1434/:1440` | display | Preview downsample. Match runs on the captured gray, not this. |

### 4.4 Match-time literals (engine, not wiringPanel globals)

These are not `g_*`. They are still magic on the inspection path.

| Literal / env | Site | Class | Notes |
|---|---|---|---|
| `areaThres = 100 * dsampLevel²` | `FeatureManager_sig360_circle_line.cpp:6314` | verdict | `//HACK:100 no particular reason`. Labels smaller than this are ignored. **[SILENT]** |
| `single_result_area_ratio` | def key, engine `:6291` | verdict | Off when ≤ 0. If on, not-largest → empty reports. Def-owned; listed so it is not “fixed in MachineContext.” |
| `INSP_CALIPER_PASSTHROUGH` | Caliper path | measure | Env: skip real caliper. Changes millimetres. |
| `INSP_ANG_OFFSET` | shape match `:7475` | measure | Env: add degrees to the match. |
| `INSP_MATCH_BRUTEFORCE` / `INSP_V2_NO_CENTROID` | sig360 | measure | Env matcher switches. |
| `MORPH_FIELD_AMP` (+ C/SIG/DIR/TYPE) | morph | measure | Env synthetic deformation field. |
| `MORPH_ALPHA` | morph | measure | Env override of def morph alpha. |
| `INSP_SKIP_INSPECTION` | `:456` | **verdict** | No match; fixed SUCCESS so the sorter path still runs. |

Env that only dump (`SHAPE_DBG`, `CALIP_DUMP`, `SP_PT_DUMP`, `INSP_DUMP_CALIPER_DEBUG`) stay out.

### 4.5 Verdict reduction (report → one integer)

| Symbol / rule | Site | Class | Notes |
|---|---|---|---|
| default `stat = STATUS_NA` | `:8648` | verdict | Survives any failed guard |
| `srep.size() != 1` | `:8693` | verdict | Zero or two objects at the station → NA. **[SILENT]** except `LOG_EVERY` |
| `reports.size() != 1` | `:8679` | verdict | Same |
| `InspStatusReducer` NA-absorbing | `:6091` | verdict | `FAILURE + NA = NA`. A bad part with one NA judge is **not** ejected |
| `quality_essential == false` | `:6125` | verdict | Judge drawn, ignored. Def-owned |
| `NAasNG` / `NGasNA` | `:6130` | verdict | Per-judge def flags |
| `skip_inspection()` | `:8641/:8646` | verdict | Forces SUCCESS, `report = NULL` |
| `stat` vs `stat_sec` | `:8648/:8650` | snap | `stat` → sorter; `stat_sec` → NG snap. UNSET vs NA is how “guard rejected” is told apart |

### 4.6 Peripheral mapping (integer → selector)

Lives on `bpg_pi.perifCH`, loaded from `machine_setting.json` `uInspESP32_peripheral_conn_info`. Not a `g_*`, still inspection-path magic.

| Symbol | Site | Class | Notes |
|---|---|---|---|
| `cat_ok` / `cat_ng` | PerifChannel, connect `:5513` | **verdict** | `0` or missing → that class becomes NA. Logged once at connect |
| `PERIF_CAT_NA` (`0xFFFF`) | `:6501` | verdict | “do nothing, let it go round” |
| `device_pairing()` / `INSP_PERIF_DEVICE_PAIRING` | `:443` | admit/place | Skip announcement wait; reports carry `cam_ts` |
| `INSP_PERIF_VERDICT_PATTERN` / `_SLIP` | send thread `:7430` | **verdict** | **Replaces** the inspection result. Nothing in the WebUI says so |
| `INSP_PERIF_FAULT_*` | send thread | verdict | Drop/dup/shift timestamps. Test scaffolding on the production send path |
| `INSP_PERIF_DIRECT_SEND` | `:8964` | timing | Bypass send queue |
| `INSP_PERIF_PCNT_SLIP` | `:6567` | verdict | Lie about pulse count |

### 4.7 Snap / preview (not the sorter, but the same session)

| Symbol | Site | Class | Notes |
|---|---|---|---|
| `saveInspFailSnap` / `saveInspNASnap` | `:125` | snap | ST `INSP_NG_SNAP` / `INSP_NA_SNAP` |
| `InspSampleSavePath` / `MaxCount` | `:141` | snap | |
| `DoImageTransfer` | `:2448` | display | ST; skip `IM` to UI |
| `SKIP_NA_DATA_VIEW` | `:84` | display | ST `IMG_STREAMING_SKIP_NA` |
| `DATA_VIEW_MAX_FPS`, `OK/NG/NA_MAX_FPS` | `:88/:98` | display | Preview cap |
| `DataView_JPEG_quality` | `:95` | display | |
| `datViewQueueSkipSize` | `:87` | display | CI `= 1`, FI `= 2` |
| `cache_deffile_JSON` | `:103` | snap | **Def cache for snap-save, not match.** Leave it. |

### 4.8 Explicitly not this plan

| Thing | Why |
|---|---|
| `matchingEng.AddMatchingFeature` / `ResetFeature` / `definfo` | Def load |
| `def_stamp_context` / `_ref_image_path` / sidecar PNG | Train, not per-frame inspect |
| `__shape_cache` | Train cache inside the def |
| Latency hists, NA counters, thread beats | Telemetry |
| `inspQueue` / `datViewQueue` / `resPool` themselves | Plumbing; **the skip-size policy** is in scope, the queues are not |
| `INSP_ALLOW_EXEC`, `FORCE_BMP_CAROUSEL`, `INSP_CV_THREADS` | Process setup, not per-frame |
| `INSP_LOOP_N` / `INSP_PROF` | `--insp` profiling |

---

## 5. Approaches considered

**A. Accessor singleton** over current names. One door, twins remain.

**B. `InspectionContext` with named sub-bags (this plan).** Machine + pipe + verdict policy in one process object. BacPac stays the engine ABI. Two views.

**C. Full DI through every BPG handler.** 10k-line mechanical rewrite. Defer.

Chosen: **B**.

---

## 6. Target shape

```
InspectionContext                 // one, created in cp_main
  CalibStore                      // lens, field, pending capture
  Station                         // region, clean_regions, area_bypass
  Session                         // FI | CI | II | InspCli
  Pipe                            // skip sizes, doImgProcessThread, img_transpose
  Verdict                         // skip_inspection, NA-absorbing policy *as flags*
  Perif                           // cat_ok/cat_ng pointers or copies used on this path
  production : BacPac             // calib_bacpac
  authoring  : BacPac             // neutral_bacpac
```

`bpg_pi.camera` / `bpg_pi.perifCH` / `resPool` stay where they are in P0–P3. Perif **policy numbers** (`cat_ok`/`cat_ng`) are what Verdict/Perif need, not the UART object.

**Copy caveat:** the CONNECT *reuse* path re-reads conn_info and updates
`cat_ok`/`cat_ng` on the live channel **without reopening the port** (that is
its whole point — see the comment at the reuse block). If Perif holds copies,
they MUST be refreshed on both the open and the reuse path, or a changed
conn_info silently stops applying. Either refresh-on-connect/reuse, or read the
channel under `perif_tx_lock`. Pick one and write it down in the code.

Threading (P0–P5): **no new locks, same races as today — deliberately.** These
names are written by the WS thread and read bare by the inspection threads now;
moving them into a struct neither fixes nor worsens that. Adding locking here
is a behaviour change with its own deadlock surface (see AUDIT 2.5's ABBA) and
is NOT part of this refactor. Each sub-bag has one owner thread on the write
side: `Station`/`CalibStore`/`Verdict`/`Perif` — WS handlers; `Pipe` — session
start only; `Session` — WS. Readers stay unsynchronised.

Rules:

- `Station.publishOnto(bacpac)` is the **only** writer of `insp_region_*`.
- `eval_clean_regions` reads `Station`.
- Both BacPac views point into `CalibStore`.
- `ignoreCalib` is a scoped guard on a view.
- `Pipe.imageQueueSkipSize` is set **only** by Session start (CI=1, FI=capacity). No other writer.
- Env that **replace the verdict** (`INSP_SKIP_INSPECTION`, `INSP_PERIF_VERDICT_PATTERN`, …) load once into `Verdict` / `Perif` at process start and are **visible** on `camera_info` / `perif_pairing`. Today they are silent.
- Do **not** put `clean_regions` or skip sizes onto `FeatureManager_BacPac` in P0–P3.
- Do **not** move `areaThres = 100*dsamp²` in P0–P4. Name it in this doc; promoting it to a Station/def knob is a behaviour change and needs its own golden.

Session overlay:

| Session | Region on BacPac | Calib | `imageQueueSkipSize` |
|---|---|---|---|
| FI | Station unless bypass | on | queue capacity (never skip) |
| CI | zero-size | on, unless scoped ignore | `1` (skip when queued) |
| II | request `work_region` only | off | n/a (no camera queue) |
| `--insp` | Station unless bypass | on | n/a |

---

## 7. Phases

Each phase is a separate, reviewable change. Gate: `--insp` golden byte-identical.

### P0 — freeze machine twins

Move the six `g_*` + two BacPacs into `InspectionContext`. `inspection()` accessor. Zero behavior change.

### P1 — Station

Delete `g_insp_region` / `g_clean_regions` / `g_full_inspection` / `g_area_gates_bypass` as names. One `publishOnto`.

Also: LOGI the full effective Station **once at startup**. The 2026-08-15
golden-test incident — a leftover `inspection_region` in machine_setting.json
silently dropped all 7 labels of an unrelated test image — is exactly the bug
class this plan exists for, and it was found by reading a crash dump, not a log
line at boot.

### P2 — CalibStore

Delete `g_lens_calib` / `g_field_calib` / pending grids as file-scope.

### P3 — `ignoreCalib` RAII

AUDIT 1.3. No production path sets the flag without a destructor that clears it.

**Mostly done in `0338671f`** (before this plan started): CI/FI session open now
*assigns* the flag unconditionally (a plain session resets it), the EX handler
holds a scope guard (the srcImg==NULL early exit no longer sticks), and the
station block reports `ignore_calib` per frame — verified end-to-end
(`calib_sticky.mjs`). What P3 still owns: formalising the guard as RAII **on
the view** so no future call site can regress to bare `ignoreCalib(true)`.

### P4 — Pipe

Move `imageQueueSkipSize`, `datViewQueueSkipSize`, `doImgProcessThread`, `img_transpose` into `Pipe`. Session start is the only writer of skip sizes. `img_transpose` is logged at session start (it is a measure change).

Display knobs (`downSampLevel`, `ImageCrop*`, `DoImageTransfer`, JPEG, FPS caps) may ride on a `Preview` nested struct in the same phase so ST has one object, but they must be named **preview**, not pipe.

### P5 — Verdict + Perif policy

Move `skip_inspection()`, `device_pairing()`, and the `INSP_PERIF_VERDICT_*` / `FAULT_*` IIFEs into `Verdict` / `Perif`. Report them on GS `perif_pairing` so a pattern that replaces the result is not invisible.

**Partly done in `5548f454` / `0338671f`:** `station.skip_inspection` is on
every frame, `camera_info.setup_failed` names refused camera setters, and
`perif_pairing.link` carries tx_fail / dropped_no_channel / suspect. Still
silent and still P5's job: `INSP_PERIF_VERDICT_PATTERN` / `_SLIP` /
`FAULT_*` / `PCNT_SLIP` — the envs that *replace* the verdict.

Do **not** change `InspStatusReducer` NA-absorbing behaviour. Optionally *name* it (`Verdict.na_is_absorbing = true`) so the next person does not “fix” it in passing.

### P6 — optional

`matchingEng` + camera lifetime into the same context. Not mixed with P0–P5. Not a DI rewrite.

---

## 8. Constraints

- Do not change millimetre results. `--insp` golden is the gate after every phase.
- Do not collapse the two samplers.
- Do not persist `area_bypass` into `machine_setting.json`.
- `backPackLoad` / `backPackDump` stay dead.
- `INSP_AREA_BYPASS=1` remains the headless seed; ST still overrides; both die with the process.
- Env that replace the verdict stay process-lifetime (current IIFE behaviour) until someone explicitly wants runtime flips.
- Do not treat preview downsample/crop as inspection inputs.

---

## 9. Suggested file split (P0)

| File | Responsibility |
|---|---|
| `Core0_1/include/InspectionContext.h` | Context + nested `CalibStore`, `Station`, `Session`, `Pipe`, `Verdict`. Accessor `inspection()`. |
| `Core0_1/InspectionContext.cpp` | Loaders and `publishOnto`. |
| `Core0_1/wiringPanel.cpp` | Handlers, pipe threads, queues. Call `inspection()`. |
| `MatchingEngine/include/FeatureManager.h` | **Unchanged** in P0–P5. |

---

## 10. Verification

After **every** phase:

```
cd InspectionCore/Core0_1
../build/mac-arm64/visSele --insp \
  "/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.png" \
  "/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.hydef" \
  /tmp/insp_ctx_out.json
```

Diff against the known-good report — as a **full-precision numeric leaf diff**
(the method that validated the Tier-1 fixes), not a byte diff: filter
`_ms/_us/time/seq` keys, compare every numeric leaf at full precision. A byte
diff fails on timing noise; a rounded diff hides real drift.

The `--insp` golden joins at FeatureMatching, so it CANNOT see P4's admission
knobs or P5's perif path. Those phases use the live probes in
`UI/WebUI/tools/webctl/`:

- P1: `qa_insp_region`. Remember the golden image must run with the station
  region absent (see the 2026-08-15 incident note in P1).
- P2: `camera_info.lens_calib_loaded` / `_autoloaded`; `--insp` with and without `data/lens_calib.json`.
- P3: `calib_sticky.mjs` (already passing — keep it passing). EX dropped frame then live FI — still undistorted.
- P4: `soak.mjs` (report rate + drops): CI still sheds at queue 2; FI does not. `img_transpose` flipped is a deliberate measure change — do not flip it in the test.
- P5: `station_probe.mjs` (`skip_inspection` field), `link_fault.mjs` (perif link counters), `perifstat.mjs` (GS reply). `INSP_SKIP_INSPECTION=1` still forces SUCCESS; GS reports that it is on.
