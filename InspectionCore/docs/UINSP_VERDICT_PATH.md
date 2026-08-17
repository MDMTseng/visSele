# Frame → selector: every place the verdict can change

A trace of the whole path from a camera frame to a blow selector, listing **every
branch that can alter, suppress or replace the inspection result**, in execution
order. The point is not to defend any of them — it is to make sure none of them
is a surprise.

Written 2026-08-07 against `ct/uinsp_2mach`. Line numbers drift; the function
names do not.

Legend: **[SILENT]** = takes effect with no log line of its own.

---

## 0. Frames that never get inspected

| # | Where | What |
|---|---|---|
| 0.1 | `wiringPanel.cpp:4648` `CameraLayer_Callback_GIGEMV` | `inspQueue.size() > imageQueueSkipSize` → the frame is dropped before inspection. In `"C"` (continuous) mode `imageQueueSkipSize = 1` (`:2996`), so a queue of 2 already sheds frames. In `"FI"` (full inspection) it is set to the queue capacity, i.e. never. Logged as `skip image, inspQueue.size():...`. |
| 0.2 | `wiringPanel.cpp:6248` | `perifSendQueue` full → **the oldest pending verdict is discarded** to make room. Counted in `perifSendDropCount`, logged only every 50th. |
| 0.3 | `wiringPanel.cpp:5076` | `withinMinInterval = cur_FPS < datViewMaxFPS` → image transfer to the WebUI is skipped. Affects display only, not the verdict — but it is why "the camera is pushing frames and I see nothing". |

---

## 1. `FeatureManager_binary_processing_group::FeatureMatching`

| # | Where | What |
|---|---|---|
| 1.1 | `FeatureManager_group.cpp:286` | **Raw-gray fast path.** If no sub-feature `needsBinaryPreprocessing()` (i.e. `locating_engine: "shape_based"`), binarize → cage → CCL are all skipped and **`ldData.resize(0)`** — the labeled-data vector stays empty for the whole frame. Logged only under `SHAPE_DBG=1`. **[SILENT]** See §2.3 for why this matters more than it looks. |
| 1.2 | `FeatureManager_group.cpp:~410` | `ldData[1].area -= FENCE_AREA`. Label 1 is the border cage plus everything touching it; the subtraction leaves the intruding part. Note this can go slightly **negative** on a clean frame, which is why `extra_area_ratio` (§2.4) is often negative. |
| 1.3 | `FeatureManager_group.cpp:441` | `ldData.size() <= 1` (nothing but the cage) → `error = GENERIC`, every sub-report cleared. |
| 1.4 | *removed 2026-08-07* | `intrusionSizeLimitRatio` gate. See `CORE0_1_CAVEATS.md` §K. |

`FeatureExtractor.cpp:80` also raises `ONLY_ONE_COMPONENT_IS_ALLOWED` when no
label survives — that is the **teach** path, not inspection.

`DIRTY_BACKGROUND = 4` is declared and **never raised anywhere**. Dead code.

---

## 2. `FeatureManager_sig360_circle_line::FeatureMatching`

| # | Where | What |
|---|---|---|
| 2.0 | `:6120` | **Station region** (`machine_setting.json` `inspection_region`, added 2026-08-07). Labels whose bounding box is not inside the region are dropped before anything above runs, and before `single_result_area_ratio` sums its total. Logged per frame (`region: dropped N of M`). **FI ONLY** — in CI the region is published as zero-size, so the editor sees every object; `station.region.active` in the report says which. |
| 2.1 | `:6147` | **`single_result_area_ratio`** (def key, off when ≤ 0). If the largest label is not at least this fraction of total area → `return -1`, **zero reports for the frame**. When it is on it also forces `onlyIdx` = the largest label and every other label is skipped (`:6236`). **[SILENT]** — the ratio is logged, the rejection is not. |
| 2.2 | `:6192` | `areaThres = 100 * dsampLevel²`, marked `//HACK:100 no particular reason, just a hack filter` **in the source**. Labels smaller than this are marked ignore. Hardcoded, no def knob. **[SILENT]** |
| 2.3 | `:6464` | Stage-1 signature gate: `meanRatio < 0.5 \|\| stage1Sim < sig_st1_matching_sim_thres` (default **0.3**) → `continue`, that label produces no report. **[SILENT]** at default log level (`LOGV`). |
| 2.4 | `:5518` | `SingleMatching` returns **-40** when `sqrt_MaxSimF / globeMaxSimF_ALL < sigRelativeMatchSimThres` (default **0.8**) — "this match is too much worse than the best one". A non-zero return means the report is **not pushed** (`:6524`). **[SILENT]** |
| 2.5 | `:2002` | `sig_match_sim_thres` default 0.7, `sig_relative_match_sim_thres` default 0.8, `sig_st1_matching_sim_thres` default 0.3 — all three are silent def-level defaults that decide whether an object exists at all. |

---

## 3. The part verdict — `ImgPipeProcessCenter_imp` (`wiringPanel.cpp:6126-6205`)

This is where "an inspection report" becomes "a number the sorter acts on", and
it is the densest concentration of unlogged branching in the whole path.

```cpp
int stat = STATUS_NA;                       // <- the default, and it survives a lot
if (report && type == binary_processing_group && reports && labeledData) {
  if (reports.size() == 1 && reports[0] && ...) {
    if (srep.size() == 1 &&
        srep[0].labeling_idx >= 0 &&
        srep[0].labeling_idx < (int)ldat->size()) {   // "only one detected object allowed"
      ...
      if (extra_area_ratio < 0.1) {                   // hardcoded
        stat = InspStatusReduce(jrep);
        // obj_detect clean-space regions fold in here (added 2026-08-07)
        stat_sec = stat;
      }
    }
  }
}
```

| # | Condition | Consequence |
|---|---|---|
| 3.1 | `reports.size() != 1` | `stat` stays **NA**. Two matched objects in frame → NA, same as zero. **[SILENT]** |
| 3.2 | `srep.size() != 1` | NA. **[SILENT]** |
| 3.3 | `labeling_idx >= ldat->size()` | NA. **[SILENT]** — and on the §1.1 fast path `ldat` is **empty**, so this is `0 < 0` = false for *every* frame. **A `shape_based` def appears to send NA for every part.** This is read from the code, **not measured**; it is the single most important thing in this document to confirm or refute. |
| 3.4 | `extra_area_ratio >= 0.1` | NA. The 0.1 is a literal in the source. Logged (`totalArea:.. extra_area_ratio:..`) — the **only** branch here that leaves a trace. |
| 3.5 | otherwise | `stat = InspStatusReduce(judgeReports)`, then obj_detect regions fold in. |

`stat` → `datViewInfo.uInspStatus` → **the peripheral**.
`stat_sec` → `datViewInfo.finspStatus` → **snapshot saving only** (`:5921-5934`).
They differ in exactly one way: `stat_sec` stays `UNSET (-100)` when any of
3.1-3.4 rejected. So **`stat:-128 stat_sec:-100` in the log means "rejected by a
guard", and `stat:-128 stat_sec:-128` means "the judges said NA"** — that pair is
the only way to tell them apart from outside.

### `InspStatusReduce` (`wiringPanel.cpp:4607`)

| # | What |
|---|---|
| 3.6 | Judges with `quality_essential == false` are **skipped entirely** — they appear in the report and change nothing. |
| 3.7 | `NAasNG` per judge: an NA is rewritten to FAILURE. |
| 3.8 | `NGasNA` per judge: a FAILURE is rewritten to NA. Both are per-judge def flags. **[SILENT]** (the debug LOGI is commented out). |
| 3.9 | `jrep.size() == 0` → NA. |
| 3.10 | `InspStatusReducer`: **NA is absorbing.** `FAILURE + NA = NA`. A genuinely bad part whose def also has one NA judge comes out NA and is *not* ejected. |

---

## 4. Verdict → selector category (`wiringPanel.cpp:5037`)

```cpp
SUCCESS -> cat_ok (or NA if cat_ok == 0)
FAILURE -> cat_ng (or NA if cat_ng == 0)
everything else -> PERIF_CAT_NA (0xFFFF)
```

4.1 `cat_ok` / `cat_ng` come from `machine_setting.json` → `uInspESP32_peripheral_conn_info`. **If either is 0 or missing, every part of that class becomes NA** — logged once at connect (`:4104`).

---

## 5. The send thread (`PerifSendThread`, `wiringPanel.cpp:5540+`) — test scaffolding that REPLACES the verdict

**These are environment variables. If one is set, the number on the wire is not
the inspection result, and nothing in the WebUI says so.**

| Env var | Effect |
|---|---|
| `INSP_PERIF_VERDICT_PATTERN=<seed>` | **Discards the real verdict** and substitutes a hash of the tid. Pairing-validation scaffolding. |
| `INSP_PERIF_VERDICT_SLIP=<k>` | Keys that pattern on `tid+k` — deliberate mis-pairing. |
| `INSP_PERIF_FAULT_EVERY=<n>` | Apply the faults below to every nth report. |
| `INSP_PERIF_FAULT_TS_US=<k>` | Shift that report's `cam_ts` by k µs. |
| `INSP_PERIF_FAULT_DROP=1` | Send nothing for it. |
| `INSP_PERIF_FAULT_DUP=1` | Send it twice. |

5.1 `have_identity`: at `PERIF_CORE_PAIRING 1`, `tid < 0` → **the report is not sent at all**. At 0, every report goes out with `tid -1` by design.

5.2 `perifPairFrameForReport` sets `skip = true` for a clock-sync frame → nothing sent, pairing still learns from it.

---

## 6. On the device (`LegacyFirmware.cpp`)

| # | What |
|---|---|
| 6.1 | **Worst-wins on a repeated report**: a second verdict for the same tid only replaces the first if it is *more* severe (`cat < insp_status`). Counted in `repeat` / `repeat_diff` / `repeat_worse`. |
| 6.2 | A report whose `cam_ts` lands outside `cam_match_window_us` (5000) of any known object is refused. |
| 6.3 | At SWITCH: `SKIP` / `UNSET` → no actuation, `CONSEC_UNANSWERED++`. At `unanswered_stop_after` consecutive → `OBJECT_HAS_NO_INSP_RESULT` (err 2), line stops. |
| 6.4 | `autoRateBackoff()` on SKIP — the feeder rate is reduced by the *inspection* result path. |
| 6.5 | `SYS_FREQ_STABLE == false`, `SYS_STEPPER_DISABLED`, `DRY_RUN` → the selector coil is not energised even with a valid verdict. |

---

## Measured on the live machine (2026-08-07, 10995 verdicts from the log ring)

```
NA, rejected by a §3.1-3.3 guard, no log line     5122   46.6%
reached the area gate                             5873   53.4%
```

`extra_area_ratio` distribution over those 5873 (gate is `< 0.10`):

```
  [-1.00, 0.00)   2363   40.2%  ########################   clean frame
  [ 0.00, 0.02)    198    3.4%  ##
  [ 0.02, 0.05)     73    1.2%
  [ 0.05, 0.08)    149    2.5%  #
  [ 0.08, 0.10)    133    2.3%  #                          <- passes, barely
  [ 0.10, 0.15)    198    3.4%  ##                         <- NA, purely on the constant
  [ 0.15, 0.30)   1311   22.3%  #############
  [ 0.30, 0.50)   1142   19.4%  ###########
  [ 0.50, 0.80)    117    2.0%  #
  [ 0.80, 1.01)    189    3.2%  ##
```

**14.8% of frames land within ±0.05 of the hardcoded 0.1.** The population is not
cleanly bimodal — there is a real band straddling the threshold, so the constant
is deciding the verdict for roughly one frame in seven.

On the wire over the same period: `cat:65535` (NA) 5501, `cat:3` (OK) 2916,
`cat:1` (NG) **0**.

---

## The short answer

Three things account for nearly all of the "hidden" behaviour:

1. **`stat` defaults to NA and four unlogged guards can leave it there** (§3.1-3.4).
   46.6% of live verdicts are decided by a branch that writes nothing to the log.
   Adding one log line at each of those four points would remove most of the
   mystery in this document.
2. **A hardcoded `0.1`** decides one frame in seven on this machine (§3.4), with
   no def key and no UI.
3. **Six environment variables can silently replace the verdict** (§5) — they
   exist for pairing validation and they do exactly what they say, but nothing
   surfaces the fact that they are active.

And one open question that is more serious than any of them: **§3.3 — whether a
`shape_based` def can produce any verdict other than NA.** Read from the code it
cannot. It has not been measured.
