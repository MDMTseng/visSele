# The geometry contract

`geom_vectors.json` is what the CORE does. `UI/WebUI/tools/geom_contract.mjs`
asserts the WebUI still agrees.

```bash
# regenerate from the core (only when the core changed ON PURPOSE)
export PATH=/c/msys64/mingw64/bin:$PATH
cd InspectionCore
g++ -std=c++17 -O2 -DFEATURE_OPENCV -fopenmp -o build/geom_emit.exe \
  test_suite/geom_vectors_emit.cpp \
  -IMatchingEngine/include -IMatchingEngine/include_priv \
  -IMatchingEngine/MorphEngine/include -Icommon_lib/include \
  -Icontrib/cJSON -Ilogctrl/include -ICameraLayer -ICameraLayer/include \
  $(pkg-config --cflags opencv4) \
  -Lbuild/win-mingw-msys -lMatchingEngine -lcJSON -lcommon_lib -lCircleFitting \
  -llogctrl -lsmem_channel -lpolyfit -lshape_based_matching -lCameraLayer \
  $(pkg-config --libs opencv4)
./build/geom_emit.exe > test_suite/geom_vectors.json

# check the WebUI against it
cd ../UI/WebUI && node tools/geom_contract.mjs
```

## Why this is a test and not just "one file per side"

Some geometry has to exist twice: at edit time the core has not run, so the
editor must predict what the machine will do.

Putting each side in one tidy file makes drift **reviewable**. It does not make
it **impossible** — and that is not a theoretical distinction. When B3 was found
on 2026-08-26, the CORRECT port of `convert3Pts2ArcData` was sitting a few dozen
lines away from three wrong copies **in the same file**, and the wrong ones
seeded arc caliper width from the complement of the arc, 11.00× too wide.

Same file did not help. A failing test does.

## Two things this exercise taught

**Test where the calipers land, not the circle parameters.** The first version
compared `cx`/`cy`/`r` and reported two failures that no amount of care in the
JS could have fixed: the circumcentre of nearly-collinear points is
ill-conditioned, and float and double genuinely disagree about it — measured 8.3
units out of 50000. But two circles that far apart at the centre still pass
through nearly the same points over the span actually used, and the points are
where the boxes are drawn and the measurement is taken. Switching to sampled
anchors took `large radius` from a 3.5e-3 failure to a 1.2e-5 pass.

**A known numerical limit gets its own tolerance and a reason, not a wider
global one.** `near collinear` still diverges, at 2.8e-3. It carries `tol` and a
`note`, and the runner reports it as `LIMIT` rather than `PASS` or `FAIL`: a
permanently red test is one people learn to ignore, and widening the global
tolerance to cover it would hide the drift the file exists to catch. Verified
that the relaxed bound does not mask a real bug — with B3 deliberately
reintroduced, that same vector fails at 1e5 against its 5e-3 allowance.

## Verified to fail

A harness that cannot fail is not a harness. With the pt2-blind version of
`arcSweep` put back:

```
4 of 13 vectors DISAGREE with the core
  caliper anchor 2 is 2.000e+1 away from where the core puts it
```

20 units on a radius-10 arc is the full diameter — the box lands on the opposite
side of the circle. Note only 4 of 13 fail: where `pt1 → pt3` counter-clockwise
happens to be the right way round, the bug is invisible. That is exactly why it
survived.

## When it fails

Do **not** regenerate the vectors to make it green. The vectors are what the
machine does; regenerating writes the disagreement down as the new truth. A
failure means the screen and the machine have parted company, and the question
is which one is wrong.
