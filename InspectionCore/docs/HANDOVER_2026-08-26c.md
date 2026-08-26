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
