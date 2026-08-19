# WebUI QA Loop — Summary

> Written on `webui/editor-refactor`. Development has since moved to
> `ct/win-bench-bringup`; the suites still run, and as of 2026-08-19 they run
> off the Mac too. Read "Off the Mac bench" at the bottom before you trust a
> red line -- none of the 7 current failures is a newly-introduced defect.

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

---

## Off the Mac bench (2026-08-19)

These suites were written and maintained on macOS. Getting them to run
elsewhere took one change, and it is worth knowing what it bought.

### What was wrong

22 of the 39 hardcoded

```
/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify
```

as their `WEBCTL_MODEL` default. Off that machine the def load failed -- and
each suite then classified the failure with its own private copy of

```js
/not connected|timeout|did not load|reconnect|ECONNREF/i
```

A missing def produces "did not load", which matches, so the suite reported
**`SKIP (core down)`** with the core up and answering on 4090 the whole time.
A wrong diagnosis pointed at the wrong subsystem is worse than none.

There was no shared module to fix this in: `MODEL_PATH` was copy-pasted 22
times and `isCoreDown` 23 times, with **zero imports between the 39 files**.

### The fix

`lib_model.mjs` now owns both. Each suite lost its local default and its local
`throw`, and gained one import. `diagnoseLoadFailure()` asks the *page* whether
the core is there (`ConnInfo.CORE_ID_CONN_INFO.type === 'WS_CONNECTED'`) rather
than inferring it from the text of an error, so "no core" and "no model" are
now different messages.

`reset()` was deliberately left alone -- it exists in 24-line and 27-line
variants and the differences have not been audited. `r1_resilience` also keeps
its own `CORE_DOWN` sentinel (underscore, not hyphen); it passes, so it was not
worth the churn.

### What it bought — measured, same bench, same day

| | Before the fixture fix | After it | After the test fixes |
|---|---|---|---|
| PASS | 15 | 32 | **35** |
| SKIP | 21 | 0 | **0** |
| FAIL | 3 | 7 | **4** |
| Wall clock | 954 s | 182 s | **187 s** |

The runtime collapsed because ~44 s of that per skipped suite was retry against
a path that could never exist. **FAIL went up because five suites that had been
skipping were finally allowed to run** -- those failures were always there.

### The failures, classified (7 after unlocking, 4 after the test fixes)

None is a newly-introduced defect. Do not treat this list as a regression set:

| Suite | Class | What it is |
|---|---|---|
| `r3_serialize` S4 | **stale test — FIXED** | Fired only the `input` event. §9.15 of TEAM_HANDOFF: `JsonEditBlock` moved to commit-on-blur/Enter, so redux never saw it. Now uses real `/fill` + Enter, and the field being `type="number"` means the non-numeric half is typed as keys instead |
| `r10_bpgfuzz` F3 | **stale test — FIXED** | Asserted `camera_id`/`session_id` on `raw2Obj_IM` output. The 15-byte IM header has no such fields and never did. Now asserts what is parsed, plus that image type follows format (`Uint8Array` for JPEG, `Uint8ClampedArray` for raw) |
| `r8_matching` T1/T6/T7/T9 | **dead feature — SKIPped** | Asserted `intrusionSizeLimitRatio`, removed from the app 2026-08-07 (`fd5f1f4a`); the identifier is not in `src/` at all. Failing for 12 days, invisible behind the SKIP. T6/T7 now SKIP naming that commit rather than being deleted — whether the cases go or the feature returns is not a test fix's call |
| `r4_purelib` | **false red** | Every assertion passes, then libuv aborts in teardown (`UV_HANDLE_CLOSING`, exit 127). Standalone 4/4 crash; under `run.mjs` it sometimes exits 0 — a teardown race, not a stable verdict |
| `r6_decorator` T6 | **flaky** | 2 of 4 runs fail on `addId2(null)Kept=false`. Untriaged |
| `r6_inspection` T1, `r7_inspbug` T1 | **timing — IMPROVED, still intermittent** | See the section below. Fixed waits replaced by polling plus re-send. Standalone: pass. Immediately after `r6_decorator`: pass. Under the full 39-suite `run.mjs`: passed once, failed twice. **Root cause not established** — do not read the current green as fixed |
| `r6_inspection` T2/T4/T5 | **consequence — resolved** | All three were downstream of T1. In the run where T1 entered, they passed (`stillInsp=true`, `value="MAIN"`). T4 had been sending EXIT from MAIN, and MAIN+EXIT lands in SPLASH — trap 4b, reproduced exactly |
| `r10_smoke` (2) | **mixed** | `S1 ws=false`, plus `S11` counting React dev-mode warnings as errors. Counting them at all is the test's problem: a dev bundle emits them by design (the antd `Drawer visible` deprecation is another). The specific `React does not recognize the `%s` prop` line was NOT traced to a source -- webctld logs the unformatted string and the arg is lost, and hooking `console.error` does not survive the `/reload` that triggers the mount. It is not the `data-*` hooks added 2026-08-18: React does not warn on `data-*`, and all of them were checked |
| `r8_matching` (3) | **needs triage** | `intrusion_ratio` reads `undefined` from baseline onward. Plausibly a field the tagged fixture does not carry rather than a reducer bug — check the def before blaming the code |

### The systemic one: fixed sleeps standing in for state

Measured 2026-08-19 on this bench: after `store.dispatch({type:'Insp_Mode'})`
the machine is **still in MAIN at 0 ms and reaches INSP_MODE at ~900 ms**.

There are 27 `dispatchSM(...)` sites across five suites, and the waits after
them are **80 ms, 120 ms, 200 ms, 250 ms and 300 ms** — every one of them below
the measured figure. The suites that pass today pass by luck of scheduling, not
by design; `r6_inspection` and `r7_inspbug` were the two where the luck ran out
on a slower box.

Worse, the delay is not the whole story. ActionThrottle (TEAM_HANDOFF §9.3) can
**swallow** a dispatch outright when a burst is in flight, and `reset()` leaves
exactly such a burst (`MW_API_CALL` ×8, `WS_UPDATE` ×4). Polling longer does not
rescue a message that was never delivered. `r6_inspection` needed the event
re-sent during the poll before it would enter at all — waiting alone, even for
4 s, was not enough.

Fixed so far (`r7_inspbug` T1, `r6_inspection` T1/T5): poll for the state, and
re-send the event every second while polling. Re-sending is safe because
`Insp_Mode` is only a transition out of `MAIN`.

**Not yet fixed:** the 80 ms waits in `r8_substates` and the 250–300 ms ones in
`r10_smoke`. Those suites were left alone because they are not the ones failing
— but if either starts flaking, this is the first thing to look at, not the
app.

**And it is not fully solved.** `r6_inspection` T1 and `r7_inspbug` T1 now pass
standalone, and pass when run immediately after `r6_decorator`, but under the
full 39-suite `run.mjs` they passed on one run and failed on the next two. The
polling and the re-send moved them from *always* failing to *sometimes*
failing; whatever the remaining factor is, it shows up only deep into a long
serial run, which points at accumulated browser or store state rather than at
the dispatch itself.

Do not read a green T1 as a fixed T1. If you pick this up, the next thing to
try is `ActionThrottle_type: 'express'` on the dispatch -- `ActionThrottle.js`
short-circuits on it -- which would settle whether throttling is involved at
all. Reproducing it needs the long run, not the suite on its own; that is what
made it expensive to chase, and why it is written down rather than guessed at.


### Still true regardless of platform

- **Nothing else may touch webctld while `run.mjs` is going.** It serialises
  its own suites because the daemon owns ONE browser, but it cannot stop a
  second terminal. In the 2026-08-19 baseline run `r1_comm` FAILed for exactly
  this reason -- alone it is 6 PASS / 1 SKIP, exit 0.
- **`fixtures/test1.hydef` ships without its image.** Def-only probes are fine;
  a def+image pair (`ii_dump`, `--insp`, `calib_sticky`) still cannot run from
  a clean clone.
- Read a suite's own last line before believing `run.mjs`'s verdict; it
  categorises on the exit code alone.
