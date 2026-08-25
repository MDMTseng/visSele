# The report's `extra` slot, and who decides a part — 2026-08-25

Two contracts settled on the bench today, plus the defects found on the way and
what is still open. Both are about the same thing: **one source of truth, in a
place that makes the wrong version impossible rather than unlikely.**

---

## 1. `extra`: what a report may carry beyond the measurement

### The contract

A judge/geometry report may carry an optional `extra` object. Everything that
exists to be *looked at* rather than *recorded* goes in there:

```json
{ "id": 3, "value": 2.31, "status": 1,
  "extra": { "cal_hits": [ {"x":…,"y":…,"st":…,"s":…}, … ] } }
```

Which payloads are produced is chosen at runtime, at the ST root:

```json
ST { "DEBUG_EMIT": { "cal_hits": false } }
```

The registry lives in `MatchingEngine/FeatureReport_UTIL.cpp` (`g_dbg_emit`,
`DbgEmit()`, `DbgEmitSet()`). **An unknown name is logged and ignored, never an
error** — a newer WebUI asking an older core for a payload it has never heard of
must degrade to "not sent", not to a rejected command.

### Why one slot instead of one field per payload

The inspection record is sent to the traceability DB *whole*. Every debug field
added over the years would otherwise have to be remembered and stripped one by
one. With `extra`, exclusion is structural — a payload added tomorrow is out of
the archive by construction:

```js
// UI/WebUI/src/UTIL/dbRecord.js
export const OVERLAY_ONLY_FIELDS = ['extra', 'cal_hits'];
```

(`cal_hits` stays in that list only for a core older than this change, which
still sends it at the top level.)

`stripOverlayOnly()` prunes with **structural sharing** — subtrees containing
nothing to remove are returned as-is. It runs on the live redux report at
inspection rate, and a deep clone per part would cost more than the bytes it
saves. It never mutates its input: the same objects are still on screen.

### What it is worth, measured

Bench, 20 parts/s, one part's record:

| | `rep_bytes` | `cal_hits` | `sorted_s` |
|---|---|---|---|
| `cal_hits: true` | 22,923 B | 311 | 20.2/s |
| `cal_hits: false` | **3,526 B** | 0 | 19.5/s |

**85% of the record was the caliper overlay** — ~300 hits a part, each
`{x,y,st,s}` printed by cJSON at `%1.15g`. Detection is unaffected. With it off,
`rep_bytes == rep_lean`: nothing strippable is left, which is itself the proof
that `extra` is the only exit.

At 20 parts/s that is ~418 KB/s against ~61 KB/s.

### Deliberately all-or-nothing

There is no "send only the calipers that worked" mode. A missed caliper
(`st:0`) is as informative as a hit — it is why the overlay can draw a
placeholder where a caliper tried and failed. Either the overlay is wanted or it
is not.

### Two traps, both now commented in place

- **The gate belongs at the serialisation point**, not at the producers. Three
  code paths fill `cal_hits` (line/arc caliper, search-point, circle fit) and
  there is exactly one exit, `AddCalHits2JSON`. The vectors themselves are a
  by-product of a fit that already happened and are nearly free; what costs is
  the printing.
- **A root-level setting must not travel inside `MachineSetting`.** The core
  answers a `MachineSetting` object by running `setup_machine_setting()`, which
  calls `load_clean_regions()` — and an absent key there means "no clean
  regions", so a one-key `MachineSetting` silently wipes the station's clean
  areas. `soak.mjs` therefore has `__SEND_ST_RAW__` alongside `__SEND_ST__`.
  (`DEBUG_EMIT` was first written inside the `ImageTransferSetup` sub-block and
  was never reached at all; that sub-block only runs when the payload carries
  that object.)

### Next payload

`edge_profile` is one name in this registry away. `Caliper.cpp` already builds
the picture — `caliper_dump_line_strip()` renders every caliper's across-edge
profile into one image (x = caliper index, y = across-edge position, marks for
inlier / outlier / no-edge). It is currently gated on `getenv("CALIP_DUMP")` and
written with `cv::imwrite`. The work is to make the flag per-request and hand
the `cv::Mat` back for the DefConf image channel instead of writing a file.

---

## 2. What decides a part

### The contract

```
effective quality_essential = (rank <= N) AND quality_essential
```

folded **once**, on entering inspection, into `_obj.shapeList`, before the wire
def is generated from it. After that nobody needs to know about rank again:
screen, wire def and `InspStatusReduce` all read the one field.

`N` arrives as a **`rankN` tag**, chosen in MainUI beside every other per-part
tag. The tag group is generated from the union of root ranks and per-製程
override ranks, and is hidden when there is no choice to make.

`NAasNG` / `NGasNA` are applied per item *before* the reduction — where the core
applies them.

### The defect this replaced

`finalResult` reduced over the **rank-filtered** list, so the operator's 檢測等級
slider changed the verdict drawn beside a machine that had never heard of it:
turn the level down and a part about to be rejected read OK. That is what made
the sorting station confusing. The slider is gone; rank is visibility, and a
viewing control must never decide what a part is.

The core has no notion of rank and cannot be told about one after the def is
sent — which is why a slider could never have closed this loop.

---

## Defects found and fixed today

| | |
|---|---|
| **Margin editor persisted a React element** | The per-製程 row carried `name = <><PlusOutlined/>{tag}</>` for display, and `update()` wrote the whole row into the def. On entering inspection the override is merged into the shape with a full spread, so the shape's name became an object in the wire def and **the core could not match that feature at all** — 20 parts/s admitted, 0.0 sorted, under the one 製程 that had been edited. Fixed with a **whitelist** of persisted keys, because the merge downstream copies every key. |
| **Bubble read the wrong limits** | It showed `shape_def` while the scale and the colour used `rep.lim`; `shape_def` does not resolve the flipped-part `_b` fields. Now both come from `effectiveLimits`, and the bubble says when the numbers are not the recipe's own. |
| **Caliper-hit toggle only worked on one screen** | `renderUTIL` defaults `show_caliper_hits` true and only InspectionUI mirrored it; DefConf drew them always, with no control. DefConf now reads the same `System_Setting`. |
| **`electron-packager --prune` deletes the test harness** | It prunes `node_modules` in place and `playwright` was never in `package.json`, so every `make v2` removed it. Pinned as a devDependency. |

## Caveats — read before trusting a number

- **`gradeMismatch` / `tagDrift` / `tagApplied` are vacuous when nothing is
  judged.** With the backlight out of position every part came back NA,
  `trackingWindow` stayed empty, and all three read 0 — which is "nothing was
  compared", not "core and screen agree". **Check `tagSeen` first.**
- **`tagApplied` cannot be measured on flipped parts** and is skipped for them
  (`tagWhy: flip=N` says so). On this bench every part is flipped, so coverage
  is currently unmeasurable here.
- **There is no untouched root at runtime.** Entering inspection writes the
  overrides into `_obj.shapeList`, and `loadedDefFile` is regenerated from it
  when the wire def is sent. A check that asks "does this override differ from
  the recipe's own number" therefore always answers "no". That question is a
  property of the *recipe*; the software's question is "did the verdict use the
  製程's number", which is what `tagDrift` now asks.
- **`Memory.getDOMCounters` is not a health signal.** It swings three-fold while
  the document never moves. Use the in-document count. See
  `LAUNCHER_REDESIGN_2026-08-23.md` and the soak chart's DOM panel.
- **The soak's `Runtime.queryObjects` census stalls the renderer for ~8 s** and
  is opt-in behind `SOAK_LIVEDOM=1`. Leave it off when hunting stalls — it
  manufactures the symptom.

## Open

- **Detection rate alternates between ~20/s and ~5/s across runs**, with the
  same binary, the same tag and `cal_hits` either way. Not explained. Suspect
  the parts on the plate rather than the code, but that is a hypothesis, not a
  finding. `SOAK_FRAME_PNG=1` dumps the decoded frame each tick — capture one
  during a bad run and compare.
- **UI freeze reported in the field** (~10 s, machine keeps inspecting). The
  stall detector is shipped: `stall_max_ms` high with `task_max_ms` similar
  means our JS; high with `task_max_ms` ≈ 0 means the thread was not scheduled.
- **`frame_lost` is an unsigned underflow** after a camera-counter reset.
