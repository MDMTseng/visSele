# Handover — visSele, 2026-08-26 (third part)

Branch `ct/win-bench-bringup`, head `9b048c98`. Continues
`HANDOVER_2026-08-26.md` and `_2026-08-26b.md`; this covers the SBM studio work
and what it turned up.

**Bench state right now:** `export_v2/app/1.1.105` is running, built from
`178542d4`. `current.json` points at 1.1.105, `previous` is 1.1.104. **1.1.104
is untouched and still shippable.**

**The extraction gate is deployed and ARMED.** T1 passed — see below.

---

## T1 — done, and it passed

Regenerated from the studio (生成特徵點 then save), then run with the refusal
armed:

```
objects=1  sim 0.9935  rot -0.1395 deg  cx 15.026 cy 9.305
searchPoints 9/9 ok, 3 judgements, none NG or NA
```

The gate did not fire, so the cache was taken and built a working model. The
pose lands 0.0124 deg and a few microns from `def_image_reg`, which is the
localization error, not a fault. A rotation sweep through the same armed path:

| applied | sim | reported | residual | points |
|---|---|---|---|---|
| 0 | 0.9935 | -0.1395 | — | 9/9 |
| +5 | 0.9903 | +4.7130 | -0.1475 | 9/9 |
| -8 | 0.9984 | -8.1669 | -0.0274 | 9/9 |
| +15 | 0.9968 | +14.6130 | -0.2475 | 9/9 |

Residuals grow with angle (-0.03 at 8 deg, -0.25 at 15 deg), which is worth a
look on its own but is not what this test was about.

### An earlier note in this file was WRONG and is retracted

`--insp` is **not** broken for SBM defs. `machine_setting`'s
`inspection_region` is `x1269 w285`, and the part in the TRAINING image sits at
`x920-1244`, so the station filter correctly excluded it and line2Dup was handed
a scene with no part in it. `--insp` enforces the station; the II path
deliberately does not (that is a documented fix in the II handler). Use
`INSP_AREA_BYPASS=1` when inspecting a training image from a shell.

## Still blocked on the machine

| | what to do | why |
|---|---|---|
| **T3** | 1.1.104 on the bench, watching the yield | 6 edges change: one `min_strength` 10 to 0, five `include_range` 2.0px to 0 |
| **T4** | Flash the firmware fixes to a production board | only the COM3 bench board has them |
| **T5** | Regenerate `test1_ms1.hydef` and `test1_x0.5.hydef` | both are shape_based with NO cache, so under the armed gate they will refuse until 生成特徵點 + save |

## What was measured after arming, and one thing that broke

### The cache does not change the answer — verified

Same def, same image, once from the cache and once forced to re-extract:

```
leaves 1391 vs 1391, differing 0
```

That is the invariant the cache code claims for itself ("a cache that changes
the answer is worse than no cache"), and it now has a number behind it. **Run
the cache-free copy BESIDE the sidecar**: the first attempt put it in a temp
directory, so the template resolved to a `.png` that did not exist, training
failed, and the "difference" was two failures agreeing with each other.

### The bench def stopped using SBM, and nothing looked wrong

`data/test1.hydef` was rewritten at 19:18 with **`def_image_reg.angle` set to
exactly 0** (it was -0.00221875). `angle_offset_deg` is in the cache
fingerprint, so that invalidated the features generated eight minutes earlier:

```
was: ...|roi1:11|ao-0.1271|1.4028,-0.5096|...
now: ...|roi1:11|ao 0.0000|1.4028,-0.5096|...
```

With the gate armed the def then falls back to sig360 — and on its own training
image sig360 still locates it, 9/9 points, no NG. **So the screen looks fine
while the localizer the def was built around is not running at all.** That is
the failure mode to watch for, and it is why the report now says
`SBM features not trained (sig360 fallback in use)` rather than implying the
object was missed.

**Exactly 0** is the signature of a zero-length drag: `atan2(0,0)` is 0.
Releasing the locline tool without moving rewrote a registration somebody had
measured, and moved the origin to wherever the pointer was. Guarded now — a
release under 12 px is discarded, the same threshold the polygon tool uses.

**It self-heals**: press 生成特徵點 and save, and the new cache is fingerprinted
against `ao0.0000`, which matches.

### What the ROI refine points are worth — measured

Same def and image, three ways, each with a fresh extraction:

| | sim | cx mm | cy mm | rot | worst point vs 11-pt |
|---|---|---|---|---|---|
| 11 explicit ROI points | 0.9894 | 15.02500 | 9.30438 | -0.1271 | — |
| ROI list EMPTY (coarse only) | 0.9894 | 15.05293 | 9.30547 | +0.0000 | **13.7 um** (mean 1.8 um) |
| no key at all (auto-select) | 0.9894 | 15.02524 | 9.30523 | +0.0067 | 0.3 um (mean 0.2 um) |

Two things worth keeping:

- **ROI refine is doing real work.** Turning it off moves the pose 28 um in x
  and the measured points by up to 13.7 um. On this class of measurement that is
  not a rounding difference.
- **Explicit vs auto-selected is a wash here** — 0.3 um. The eleven hand-placed
  points are not buying anything over the core's own selection on THIS def and
  THIS image. They may on a harder one; the point is that it is now measurable
  rather than assumed.

Note the coarse-only row reports `rot` as exactly `+0.0000`: with no refine the
pose comes straight off line2Dup's discrete angle grid. **That is a lead for the
rotation residual** (-0.03 deg at 8 deg, -0.25 deg at 15 deg) — if refine only
partly corrects the grid snap, the residual would grow with angle exactly like
that.

### The rotation residual, chased down — and it is not a measurement problem

Fine sweep, 0 to 4 deg in 0.2 deg steps, ROI refine on and off.

**The line2Dup angle grid is exactly 1.0 deg.** With refine OFF every reported
rotation is an integer: `0, -1, +2, +1, +3, +4, +5, +6`. Refine ON brings the
angle residual from +-2 deg down to +-0.16 deg — about 15x — but does not
remove it.

The question that matters is whether that reaches the measurements. Putting each
reported point back into the OBJECT frame using the run's own reported pose, and
comparing with the def `pt1` it belongs to:

| | refine ON | refine OFF |
|---|---|---|
| mean residual across 0-4 deg | **2.2 - 6.6 um** | 28 - 80 um |
| worst single point | 23.8 um | 132 um |
| drift with angle | **none visible** | wanders +-25 um |

So the pose angle error does **not** propagate into the measured points, because
each search point is independently refined by its own caliper. The angle is
worth watching for anything that READS the pose, but the measurements over this
range are stable to a few microns.

**ROI refine is worth roughly 15x on measured position** — 3.7 um mean against
50 um. That is a much bigger number than the 13.7 um the single-image comparison
suggested, and it is the one to quote.

Only tested to 4 deg. The coarse sweep's -0.25 deg residual at 15 deg is
unexplained and outside this range.

#### The first version of this measurement was WRONG, and how it was caught

The obvious method — un-rotate each reported point by the perturbation I
applied, then compare — produced an error growing perfectly linearly to 938 um
at 4 deg. It was my inverse transform, not the machine. It gave itself away by
being **identical for refine ON and OFF to within 1 um**, while those two runs
reported angles differing by up to 2 deg: a quantity that ignores a 2 deg
difference in its own input is not measuring what it claims to.

Same lesson as the orientation stub earlier the same day. Do not hand-derive a
transform when the report already carries one; use the run's own pose.

### These frames do not match this def

| image | with the gate | with SBM forced to run |
|---|---|---|
| `snap_2026-08-25_06-23-41-214` | 0 objects | 0 objects, no reason given |
| `snap_2026-08-25_06-24-01-990` | 0 objects | 0 objects, no reason given |
| `test1_20260813_170712` | 0 objects | 0 objects, no candidate |
| `test1_20260813_170720` | 0 objects | 0 objects, no candidate |

So the def matches its own training image and nothing else on this bench. They
may simply be a different product or setup — but it means **there is no second
image here to test a def against**, which is what the robustness sweep exists to
work around.

The two `snap_*` frames return no `locate` comment at all, which is its own
small gap: "no object and nothing to say" is the one case the field was added to
remove.

## The I family — closed

### I1 — the rotation residual does NOT grow with angle. I was wrong.

Swept to 30 deg:

| applied | sim | reported | angle residual | obj-frame residual |
|---|---|---|---|---|
| 0 | 0.9894 | -0.1271 | — | 2.2 um mean |
| 4 | 0.9858 | +3.8904 | +0.0175 | 3.0 um |
| 8 | 0.9929 | +7.7500 | -0.1229 | 5.0 um |
| 16 | 0.9965 | +15.6750 | -0.1979 | 7.6 um |
| 20 | 0.9929 | +20.0105 | +0.1377 | 5.3 um |
| 30 | 0.9929 | +29.8544 | -0.0185 | 4.3 um |

**Flat scatter of +-0.2 deg, not a slope**, and 9/9 points at every angle with a
2-8 um object-frame residual throughout. The "-0.03 at 8 deg, -0.25 at 15 deg"
in the earlier note was a pattern read into four noisy samples. The +-0.2 deg is
what ROI refine leaves of the 1.0 deg line2Dup grid.

**Result worth keeping: this def locates over at least +-30 deg with no
measurable degradation.**

### I3 — not a defect. The launcher refuses to restart the core ON PURPOSE.

Stated in two places (`src/supervisor.js` header, `main.js`): *"The core decides
whether a part passes. A supervisor that quietly brings it back after an
unexplained death lets bad parts through while the line keeps running."* And the
operator is not left guessing — the exit raises the launcher shell with
`kind: 'core-exited'`. The earlier note here was wrong to call it a possible gap.

The one thing it does NOT do is signal anything outward, so an unattended
machine shows a screen nobody is looking at. Whether that needs an alarm is a
question for the peripheral layer, not the launcher.

### I3 follow-up — unattended recovery is a SESSION problem, and there is a
### live hole in it

Raised while the above was being written, and it changes the shape of the
question: restarting the core is not the hard part. **The session is defined by
what the UI pushed**, and entering inspection is a `CI`/`FI` carrying the wire
def with the 製程 margins already folded into `shape_list`, plus `ST` settings,
plus the snap policy, plus the trigger mode. The core resets all of it on entry
by design — `g_snap_policy` to false/false/false, `SKIP_NA_DATA_VIEW`, the FPS
ceilings, `saveInspQFullSkipCount`.

So a core that comes back holds **no recipe**. And:

**Nothing re-pushes any of it on reconnect.** `WS_CONNECTED` is consumed for
link COLOUR and for gating panels — that is all it does. There is no re-arm
path anywhere.

That is a present-day property, not a hypothetical about a feature nobody built:
if the core is restarted by any means while the UI stays up, the link goes
**green** and the machine holds nothing. Green link, no recipe, a line that
looks like it is running.

It could not even be detected before, because `HR` carried build provenance and
the same binary restarted is byte-identical provenance. `HR` now carries a
**session id**, generated once per core process. The WebUI remembers it, and a
CHANGED id means "a different run of the core than the one I configured":

- logged as an error naming both ids,
- and the core link renders **red**, not green.

This does not restore anything — that is the bigger decision — but the machine
no longer looks healthy while holding no recipe. **Anyone who later adds
automatic restart must add session replay first**, or the auto-restart makes
this failure routine instead of rare.

### I2 — 「跑全部影像」

A sweep degrades ONE image and asks how much it survives. The question people
actually ask out loud is the other one — "I tried five samples and three were
off" — and that needed switching images by hand and remembering. The studio now
runs the same test once per sample beside the def, and reports how many located
plus the worst pose offset among them. It awaits each image switch, because the
core holds one cached image and a fire-and-forget switch would inspect whichever
was loaded. It puts you back on the image you were looking at.

### I4 — still needs one press each

`test1_ms1.hydef` and `test1_x0.5.hydef` have no `__shape_cache`, so under the
armed gate they refuse. Open each, press 生成特徵點, save. **A headless
regenerator (`--sbm-train`) would fix this for a fleet in one command and is
deliberately NOT built yet** — it would be a second place that extracts
features, which is the thing the gate exists to prevent. Worth deciding
alongside D2 rather than on its own.

## Decisions waiting — the detail

### D1 — should SAVING rewrite `def_image_reg` from a live inspection?

Narrower than it first looked. There are exactly two writers:

```js
// DefConfUI, on save, ONLY when the target file did not exist:
if (!existed) { report.def_image_reg = { cx, cy, angle: reg.rotate, ... } }   // from edit_info.inspReport
// MISC_Util, otherwise:
if (edit_info.def_image_reg) report.def_image_reg = edit_info.def_image_reg;  // pass through
```

So an ordinary overwrite passes the value through untouched. The `!existed`
branch bakes **whatever the last inspection said** into the recipe — and the
values now in `data/test1.hydef` (cx 15.02516, cy 9.30547, angle exactly 0)
match that path, not a hand edit.

The problem with it: if that inspection ran on the **sig360 fallback** — which
is what happens when the shape cache is stale — then the angle written into the
recipe is the fallback locator's answer, not the one the def is built around.
A recipe field silently sourced from whichever locator happened to run.

`angle_offset_deg` is in the shape-cache fingerprint, so any change also
invalidates the trained features. **That consequence is already closed**:
`EditInfo_Patch` drops `__shape_cache` when `def_image_reg` or
`roi_refine_points` changes, so a save can no longer write a cache that cannot
match its own registration.

What is left to decide is whether the `!existed` rewrite should exist at all.

- **Keep it** — a brand-new def has no registration and something must seed it.
- **Narrow it** — seed only when `def_image_reg` is absent, never overwrite.
- **Require the locator** — refuse to seed from a report that came from the
  fallback, since that is the case that writes a wrong angle.

Recommendation: **narrow it.** Seeding an absent field is the legitimate job;
overwriting a measured one on a save the operator thinks is ordinary is not.

### D2 — the def-file write policy on the Resilio share

Unchanged and still the only fleet risk with **no mitigation at all**. The
facts, as established earlier:

- every def lives in the Resilio-synced folder, shared company-wide,
- fifteen machines,
- the launcher treats it read-only for updates, but **the WebUI writes defs
  straight into it**,
- so a save on one machine propagates to all fifteen, and a delete propagates
  as a delete.

There is no versioning, no review step, and no per-machine staging. The sync
artefacts (`.!sync`, `.Conflict.`) are filtered for DISPLAY, which means a
conflict is invisible rather than handled.

Options, cheapest first:

1. **Nothing** — accept it. Defensible only while one person edits defs.
2. **Per-machine staging** — the WebUI writes to a machine-local folder; a
   deliberate "publish" copies into the share. One extra button, and the blast
   radius of a mistake becomes one machine.
3. **Read-only share + publish from one place** — strongest, most disruptive.

This is also where a headless `--sbm-train` belongs (see I4): if defs are
published from one place, regenerating caches for the fleet is one command
there, and it does not become a second extraction path on every machine.

### D3 — `updateSource` per machine, and one real end-to-end update

`updateSource` defaults to `null` in `UI/Launcher/src/config.js`; each machine
needs it pointed at `data/sync/DEV/<機種>/update/`. Nothing has ever walked
install → verify → rename → run → mark-good on real hardware. Low risk to
decide, but it cannot be verified from here.

### D4 — per-item failure statistics — SMALLER than it looked

The reports **already go to a database**: `WS_SEND(Insp_DB_W_ID, ...)` pushes
every newly added report to `<inspection_db_ws_url>/insert/insp`, and
`machine_setting.json` on this bench points at `ws://db.xception.tech:8080/`.
There is a separate `inspection_monitor_url` dashboard on top of it.

So the raw data persists. What does not is the **live panel's rollup** —
`edit_info.reportStatisticState` (counts, histograms, Cpk) lives in the
renderer and dies with it. That is the same class as the session problem above:
state nobody restores.

So the decision is not "where do we store it":

- **Rebuild from the DB on entry** — the data is already there; this is a query,
  not a storage design.
- **Leave it** — accept that a renderer restart zeroes the panel, since the DB
  and the dashboard hold the history.

Recommendation: **check what the dashboard already answers before building
anything.** This may be finished work that nobody connected.

---|---|---|
| **D1** | The `def_image_reg` drift above | quantise `ao` in the fingerprint (0.13 deg cannot change a mask meaningfully), or stop DefConfUI rewriting the reg when it has not really changed. The second is the root fix; the first is one line. |
| **D2** | Def-file write policy on the Resilio share | still the one fleet risk with **no mitigation at all** (U4) |
| **D3** | `updateSource` per machine, and one real end-to-end update | U2/U3 |
| **D4** | Where per-item failure statistics live | S3; they do not persist today |

---

## Found today, NOT fixed

- **`--insp` does not locate a shape_based def.** `data/test1.hydef` against its
  own `data/test1.png` reports zero objects even with
  `SBM_ALLOW_IMPLICIT_EXTRACT=1`, so the gate is not the cause. The live path
  works, so something differs — probably calibration or scale, since `--insp`
  uses `neutral_bacpac`. **`--insp` is therefore not trustworthy for SBM defs**,
  which matters because it is otherwise the only way to drive the core from a
  shell.
- **The launcher did not restart the core after it exited.** Log shows
  `core exited code=1 signal=null after 3713.8 s` and nothing after. It was
  killed externally, so this may be deliberate crash-loop protection — but if it
  is not, a core crash on a line leaves the machine stopped with nobody told.
  Worth reading the spawn handling in `UI/Launcher/main.js` once.
- **A sweep result is not saved anywhere.** Run it, switch image, it is gone.
  No way to say "I tried five samples and three had a pose error over 0.1 mm".
- **~75 agent findings still unverified.** Four checked, four wrong about
  specifics. Verify each before acting; do not transcribe.

---

## Caveat worth having in writing

**Never build a deployable dist with `npx vite build`.** It uses
`vite.config.mjs` (dev, `base: '/'`) and emits absolute `/assets/...` URLs,
which render as a **black screen** under Electron. `npm run build` uses
`vite.config.prod.mjs` (`base: './'`). The configs differ by one line and
nothing warns you. `scripts/build_export.sh` uses `npm run build`, so packages
are safe; hand-built dists are not.

---

## What went in, and the one idea behind it

Twelve commits. The studio could only be set up, never tried; it now runs a real
inspection, sweeps robustness along six axes, and says why a locate failed.

Six bugs surfaced on the way, and **five were invisible for the same reason: the
quantity that would have exposed them is zero on this bench.**

- The measurement overlay was drawn in a frame the picture was not in — hidden
  because `def_image_reg.angle` is ~0 on every def here.
- `rotate` is not an image angle. Same reason.
- SBMStudio's locline tool wrote a raw image angle into a rotate-space field.
  Same reason, plus nobody has ever dragged one.
- `useSyncExternalStore` does not exist in React 16: builds clean, throws when
  the screen opens.
- `map_BPG_Packet2Act` was reachable only via the default export, so a namespace
  import got `undefined` inside a callback nobody sees.

The contract went from 20 assertions to 64, and two of the new ones are
**structural rather than behavioural**: no source may use a React API newer than
the installed React, and every `import * as NS` member must resolve to a real
named export. Both were verified by restoring the exact bug.

The habit that paid: **measure rather than derive.** The rotation sign was
settled by adding a perturbation argument to `--insp` and reading four numbers
off the machine, after two rounds of hand algebra gave plausible wrong answers.
`test_perturb.cpp` exists for the same reason — a sweep is only worth reading if
the axis it sweeps is the axis it names, and noise added before the warp would
have claimed 50% more tolerance than a sensor gives.

---

## Start here next time

```bash
node UI/WebUI/tools/geom_contract.mjs           # 37 vectors, generated BY the core
cd UI/WebUI && node tools/pipeline_contract.mjs # 64 assertions, real modules
cd InspectionCore && ./build/test_perturb.exe   # 12, the sweep's own axes
```
