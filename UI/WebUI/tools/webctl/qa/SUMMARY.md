# WebUI QA Loop — Summary (branch `webui/editor-refactor`)

This directory is the QA-loop deliverable for the legacy React-16 machine-vision
WebUI editor refactor. The suites here drive the running app through the
`webctld` daemon (HTTP `:8765`) against the dev bundle on `:8081`, observing
behavior through dev hooks exposed in `src/script.jsx`:

- `window.__GP_STORE__`    — live Redux store
- `window.__GP_DEF__`      — serialized def (`GenerateFeature_sig360_circle_line()`)
- `window.__GP_LOAD_BY_PATH__` — load a def by absolute path
- `window.__GP_DIAG__`     — ring-buffer diagnostics (`diagText`, `diagCount`)
- `window.__GP_DB_QUEUE__` — durable IndexedDB insert queue
- `window.__GP_BPG__`      — `BPG_Protocol` codec
- `window.__GP_MEASURE__`  — `applyMeasureLimitCoupling`
- `window.__GP_UTIL__`     — pure utilities (`PostfixExpCalc`, `round`,
  `GetObjElement`, `dictLookUp`, `CircularCounter`, `ConsumeQueue`)

## Constraints

- The daemon owns **ONE** shared headless browser. Suites MUST be run serially
  — `run.mjs` enforces this.
- Core-dependent suites SKIP (exit 0) when the core backend is unavailable.
- Each suite leaves the browser clean (final `/reload`) when it mutates state.

## How to run

```
node tools/webctl/qa/run.mjs                       # everything
node tools/webctl/qa/run.mjs r3_diag r5_wslife     # selected suites
WEBCTL_MODEL=/abs/path node tools/webctl/qa/run.mjs
```

`run.mjs` categorizes each suite (PASS / SKIP / FAIL), prints per-suite timing,
mines "REAL PRODUCT BUG", "GAP:", "NOTE:" and `TypeError` markers from
output, and exits non-zero iff any suite FAILed.

---

## Rounds

### R1 — Foundations
Viewpoints: comm / editor / pure-logic / resilience.
- `r1_comm.mjs` — WS handshake, HR-case dispatch, def-decode end-to-end (HR/IM/FL/SS).
- `r1_editor.mjs` — Shape CRUD via Shape_Set + lock-gate basics.
- `r1_purelogic.mjs` — `diagLog`, `inspDBQueue`, measure-coupling oracle.
- `r1_resilience.mjs` — sha1 hard-block, def-integrity guard, global error handlers.

Real product bugs fixed (Wave-1):
- BPG HR-case decode break (handshake never fired WS_CONNECTED).
- `raw2Obj_IM` bounds: oversize claim used to throw; now returns `image=null`.
- `urlConcat` typo regression patched.

### R2 — Protocol + Mode + Canvas
- `r2_bpg.mjs` — BPG header/IM/JSON decoders direct via `__GP_BPG__`.
- `r2_canvas.mjs` — canvas DOM presence + redux-driven re-render survives.
- `r2_measure.mjs` — `applyMeasureLimitCoupling` against verbatim oracle.
- `r2_modes.mjs` — xstate top-level mode SM + lock-gate truth.

### R3 — Diag + IDB + Refs + Serialize
- `r3_diag.mjs` — diagLog ring (capture, levels, cap invariant, global handlers).
- `r3_idbqueue.mjs` — IndexedDB durability across `/reload`, source isolation.
- `r3_refbind.mjs` — shape ref-binding 2-dispatch handshake.
- `r3_serialize.mjs` — def serialization stable, parseFloat input-guard
  (the Wave-1 NaN-guard fix).

Real product bug fixed:
- Global `window.error` / `unhandledrejection` handlers had been bypassed by
  an earlier refactor; reinstated so diag ring captures them (commit
  `ac430bad`-era).

### R4 — Multi-shape + Locking + Pure utils + Reports
- `r4_lockgate.mjs` — UICtrlReducer:607 gate (whitelist, IGNORE_DEFCONF_LOCK,
  mode-bypass).
- `r4_multishape.mjs` — add/delete/reorder, `__decorator.list_id_order`,
  `inherentShapeList` recomputation.
- `r4_purelib.mjs` — pure utilities reachability probe + reference oracle.
- `r4_report.mjs` — `Inspection_Report` accumulator (`sig360_circle_line`).

### R5 — Middleware + WS lifecycle + Report subtypes
- `r5_bpgroundtrip.mjs` — `objbarr2raw` -> `raw2obj` round-trip (single +
  multi-packet).
- `r5_middleware.mjs` — ActionThrottle express/posEdge, ECStateMachine
  unknown-event safety.
- `r5_reportvariants.mjs` — sig360_extractor / camera_calibration ok+err /
  stage_light_report / unknown subtype.
- `r5_wslife.mjs` — WS open / auto-reconnect, BPG decode robustness as
  onmessage-guard complement.

### R6 — Decorator + Inspection + Shape attrs + SPC
- `r6_decorator.mjs` — `__decorator` reorder, phantom-filter, repair-missing,
  control-margin, extra-info DEAD/LIVE classification.
- `r6_inspection.mjs` — Insp_Mode / InstInsp_Mode SM entry & exit.
- `r6_shapeattr.mjs` — `Shape_Attr_Fill` defaults per type
  (line/arc/sp/measure + pass-through types).
- `r6_stats.mjs` — `statReducer` accumulator via age-out path.

Real product bug surfaced:
- **InspectionUI `CanvasComponent` `INSP_MODE` TypeError**
  ("Cannot read properties of undefined (reading '0')") — caught by
  `RootErrorBoundary`. Pinned in `r7_inspbug.mjs`; open follow-up.

### R7 — i18n + DownSamp + InspBug pin + Save/Load
- `r7_dictlookup.mjs` — `dictLookUp` string/array-key fallback, theme fallback,
  hardcoded-CJK smell metric.
- `r7_downsamp.mjs` — four copy-pasted `down_samp_level_update` sites +
  drift inventory.
- `r7_inspbug.mjs` — pin test for the R6 CanvasComponent crash.
- `r7_savefload.mjs` — idempotent load, sha1 lineage, serialize stability.

Real product bugs (open follow-ups):
- **`down_samp_level` NaN escape** at all 4 sites (different formulas, clamp
  bounds, gating, and mmpp paths). Open follow-up.

### R8 — HR config + Matching + Property sheet + Substates
- `r8_hrconfig.mjs` — HR-triggered LD: `machine_custom_setting`,
  `System_Setting`, `MachTag`, `FILE_default_camera_setting`, camera-param
  FL pipe.
- `r8_matching.mjs` — `Matching_Angle_Margin_Deg_Update`,
  `Matching_Face_Update`, `IntrusionSizeLimitRatio_Update` (numeric-guard),
  nonexistent action safety.
- `r8_propsheet.mjs` — per-shape-type property-sheet DOM signature.
- `r8_substates.mjs` — DEFCONF_MODE substate transitions, SUCCESS/FAIL,
  gate active in children.

Real product bugs (open follow-ups):
- **`Matching_*` reducers don't clamp / type-guard** (`angle_margin_deg`
  accepts negatives, strings stored). Open follow-up.

### R9 — Boundary + BPG edge + ConsumeQueue + Expr deep
- `r9_boundary.mjs` — `RootErrorBoundary` fallback UI positive test (uses
  the R6 InspectionUI crash as the deliberate trigger).
- `r9_bpgedge.mjs` — pgID overflow, length high-bit signed-shift, oversize
  claims, NUL-mid-JSON, deep nesting, barr-append offset.
- `r9_consumequeue.mjs` — `ConsumeQueue` FIFO, resolve-chains, reject-stops,
  termDrains, enqDuringConsume.
- `r9_expr_deep.mjs` — `PostfixExpCalc` precedence, unary-minus gap,
  funcSet extension, whitespace tolerance.

Real product bugs fixed:
- **BPG `raw2header` signed-shift** on 32-bit length (high bit produced
  negative length -> `Uint8ClampedArray` throw). Switched to unsigned (`>>>`).
- **BPG `raw2obj` bounds**: clip oversize `length` to remaining buffer rather
  than throwing.

### R10 — Meta-runner (this round)
Deliverable: `run.mjs` (the meta-runner) + this `SUMMARY.md`. No new test
suites added. Round-10 surfaces no new product bugs — its job is to document
the QA loop's full output for the parent.

---

## Known-bug markers cross-reference

| Round | Marker / message                                       | Status      |
|-------|---------------------------------------------------------|-------------|
| R1    | HR-case break (no WS_CONNECTED)                         | FIXED       |
| R1    | `raw2Obj_IM` oversize throw                             | FIXED       |
| R1    | `urlConcat` typo                                        | FIXED       |
| R3    | Global error handlers bypassed                          | FIXED       |
| R3    | `parseFloat` -> NaN in jsonChange input-number          | FIXED       |
| R6    | `CanvasComponent` `INSP_MODE` TypeError                 | OPEN (pin)  |
| R7    | `down_samp_level` NaN escape (4 sites)                  | OPEN        |
| R8    | `Matching_*` reducers no clamp / no type-guard          | OPEN        |
| R9    | BPG `raw2header` signed-shift on length                 | FIXED       |
| R9    | BPG `raw2obj` oversize bounds throw                     | FIXED       |

## Notes for future rounds

- Add dev hook `window.__GP_STATS__` exposing `statReducer` so R6 SPC math can
  be unit-tested in milliseconds instead of via the 4-second age-out path.
- Add `__GP_BPG__.objArr2raw` so multi-packet encoding is symmetric with the
  receiver walk in `BPG_WS.onmessage`.
- Consider a structured machine-readable JSON report from `run.mjs` (one
  line per suite) — currently text-only; trivial follow-up.
