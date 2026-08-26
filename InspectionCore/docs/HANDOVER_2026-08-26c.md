# Handover — visSele, 2026-08-26 (third part)

Branch `ct/win-bench-bringup`, head `9b048c98`. Continues
`HANDOVER_2026-08-26.md` and `_2026-08-26b.md`; this covers the SBM studio work
and what it turned up.

**Bench state right now:** `export_v2/app/1.1.105` is running, built from
`178542d4`. `current.json` points at 1.1.105, `previous` is 1.1.104. **1.1.104
is untouched and still shippable.**

**`9b048c98` (the extraction gate) is COMMITTED BUT NOT DEPLOYED**, deliberately
— see "Blocked on the machine". The exe in `build/win-mingw-msys/` has it; the
deployed one does not.

---

## Blocked on the machine

| | what to do | why it is blocking |
|---|---|---|
| **T1** | In the SBM studio press 生成特徵點, save, re-open, run 跑一次檢驗 | Decides whether `9b048c98` is safe to deploy |
| **T2** | Deploy the gate once T1 passes | `bash scripts/build_export.sh export_v2 --app-only --no-zip` |
| **T3** | 1.1.104 on the bench, watching the yield | 6 edges change: one `min_strength` 10→0, five `include_range` 2.0px→0 |
| **T4** | Flash the firmware fixes to a production board | only the COM3 bench board has them |

### T1 in detail, because there is a real risk behind it

The gate stops the core extracting line2Dup features during a def *parse*. It
still takes a cached feature set at any time. So a def needs a **usable**
`__shape_cache` or it stops locating.

`Core0_1/data/test1.hydef` has a cache and it is **stale**. The core now prints
both fingerprints on a mismatch, and exactly one field differs:

```
was: ...|w50.00|s80.00|roi1:11|ao0.0014|1.4028,-0.5096|...
now: ...|w50.00|s80.00|roi1:11|ao-0.1271|1.4028,-0.5096|...
                                  ^^^^^^^
```

Image dimensions, the image content sum (833348291, verified identical), both
thresholds, the pyramid, the feature count and all eleven ROI points match to
the digit. Only `ao` — `angle_offset_deg`, i.e. `def_image_reg.angle` — moved,
from 0.0014 deg to -0.1271 deg.

**`def_image_reg` drifts on every save.** DefConfUI rewrites it from a fresh
inspection result (`angle: reg.rotate`), so an ordinary save moves it by a hair
and invalidates the shape cache. That is the root cause and it is not specific
to this def.

And the reason T1 cannot be skipped: with the reg angle put *back* so the
fingerprint matches, the cache still fails — `addModel from cache failed`. So
that cache is unusable regardless. Whether a cache produced by *today's* build
loads back correctly is what T1 answers, and it cannot be answered from a shell:
`--insp` does not emit the cache, and a WS probe cannot run beside a live
session because the core drops the second client.

---

## Decisions waiting

| | question | options as they stand |
|---|---|---|
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
