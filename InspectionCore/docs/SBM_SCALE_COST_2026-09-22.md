# What scaling SBM template features costs

**Question asked:** does scaling SBM features arbitrarily break some optimal
arrangement and make matching slower?

**Answer:** no. Feature arrangement cannot slow the match down, and nine times
the templates costs 35% more time, not nine times. What scaling costs is
accuracy, and that is the part that does not announce itself.

Measured 2026-09-22 with `InspectionCore/tools/sbm_scale_probe.cpp`, on
`Core0_1/data/test1.png`, AVX2 build.

---

## How the scaling is done

`shape_matcher.cpp::addModelAtScaleTo` scales an EXISTING feature set by
multiplying coordinates and rounding:

```cpp
f.x = (int)(f.x * scale + 0.5f);
f.y = (int)(f.y * scale + 0.5f);
```

No re-selection, no de-duplication. The file itself records the consequence, at
the `match_scale` branch that does it properly:

> fall back to scaling the full-res feature coordinates (**crowds features,
> mismatches orientations**)

---

## 1. Geometry

Template = centred quarter of the scene, 1224x1024, 143 features at level 0.

```
[level 0 spans 649x939 px, nearest pair 7.0 px -> merging needs scale < 0.143]
  s=1.00   collapsed 0 (0.0%)   jitter 0.00 px   (in original pixels 0.00)
  s=0.90   collapsed 0 (0.0%)   jitter 0.38 px   (in original pixels 0.42)
  s=0.80   collapsed 0 (0.0%)   jitter 0.37 px   (in original pixels 0.46)
  s=0.70   collapsed 0 (0.0%)   jitter 0.39 px   (in original pixels 0.55)
  s=0.50   collapsed 0 (0.0%)   jitter 0.41 px   (in original pixels 0.82)
  s=0.35   collapsed 0 (0.0%)   jitter 0.40 px   (in original pixels 1.13)
  s=0.25   collapsed 0 (0.0%)   jitter 0.39 px   (in original pixels 1.54)
  s=0.15   collapsed 0 (0.0%)   jitter 0.38 px   (in original pixels 2.50)
```

**No features merged, at any scale down to 0.15.** The prediction before
measuring was "collapse below about 0.25"; that was wrong. Features are kept
apart by `distance = candidates/num_features + 1`, tuned until the count comes
out right, and on this template the closest pair is 7.0 px -- so two of them
need a scale under 0.143 to land on one pixel.

**That threshold belongs to the template, not to the algorithm.** A recipe whose
template is a tight ROI around a small part has far denser edges, a much smaller
spacing, and therefore a collapse threshold inside the range people actually
use. The probe prints the threshold for exactly this reason: read it for YOUR
recipe rather than reusing 0.143.

**What is universal is the jitter.** Each coordinate rounds independently, so
the error is ~0.4 px at the scaled resolution whatever the template -- which in
ORIGINAL pixels is ~0.4/s, and passes one pixel at about s=0.45.

Note the probe measures only the SCALING rounding. The extraction rounding
(features are integers to begin with) adds up to another 0.5 px, and a finer
extraction grid would shrink only that term -- the scaled coordinate still has
to land on an integer pixel, because that is what `accessLinearMemory` indexes.

---

## 2. Time

Same template, angle step 4 degrees, median of 7 matches.

| scales | templates | median |
|---|---|---|
| 1 (1.0) | 90 | 51.7 ms |
| 3 (0.9-1.1) | 270 | 54.6 ms |
| 5 (0.8-1.2) | 450 | 58.8 ms |
| 9 (0.6-1.4) | 810 | 69.8 ms |

Nine times the templates, **+35%**. The prediction before measuring was "linear
in template count"; that was wrong too.

### Why it is nearly flat

The inner loop is one vectorised pass over the response map PER FEATURE, and
the features are independent -- there is no ordering or packing for scaling to
disturb:

```cpp
for (int i = 0; i < templ.features.size(); ++i) {
    const uchar *lm_ptr = accessLinearMemory(linear_memories, f, T, W);
    for (; j <= template_positions - mipp::N<uint8_t>(); j += ...)   // over POSITIONS
}
```

And the expensive part happens before any template is looked at. line2Dup
quantises gradient direction into 16 buckets and folds them to 8 with `& 7`
(so opposite directions share a label -- polarity is deliberately discarded),
spreads each over a TxT neighbourhood as a bitmask, builds **8** response maps
through a 8x256 table, and linearises each into `T*T` rows:

```cpp
linearized.create(T * T, mem_width * mem_height, CV_8U);
```

That is `8 * T^2 * (W/T)(H/T)` = **8 x W x H bytes, once per scene**, no matter
how many templates, angles or scales exist. The templates then read contiguous
rows out of it. Most of the bill is paid before the template loop starts.

---

## What this does and does not license

**Safe:** a narrow scale range around 1.0 (0.8-1.2). Under half an original
pixel of template error, a few percent of time.

**Costly in accuracy, not time:** below about 0.5 the template geometry is off
by more than a pixel. Re-extract at the target scale instead of transforming
coordinates -- `match_scale` already has that path (the image overload of
`addModel`); the matching scale list does not.

**Not where to save time:** cutting the number of scales buys 35% at most.

**Reassurance:** the final pose does not come from these features. ICP refines
against `fs.cached_templ_scene`, built from the original template image in
float (`buildTemplateScene`), and ROI refine resizes the template image to
`matched_scale` and scales its sample points. So the jitter costs score and
discrimination -- a weaker peak, a worse ICP starting point, a part more likely
to be missed -- rather than a wrong measurement.

**One gap found while reading this, not yet measured:** ICP refine never
receives `matched_scale`. See
`contrib/shape_based_matching/icp_scale_gap.md`.

---

## Caveat on the test object

`data/test1.png` is a backlit wire part on a mostly empty field, and the probe's
template is a centred quarter crop -- not a recipe's trained template. The
features span 649x939 px, which is far larger than the part, so a good number of
them sit on the vignette and the background gradient rather than on the part.

The TIME result is structural and carries over. The geometry THRESHOLD does not:
run the probe on the recipe you care about.

```
sbm_scale_probe <scene.png>
```

---

## How the measurement went wrong first

Three runs produced complete, plausible tables that measured the wrong thing.
Worth recording, because each looked like a result:

1. **`hits 0`** -- `addModel` was given the FeatureSet overload, which leaves
   refine without the caches it builds from the image, so every candidate was
   dropped and the per-candidate work never ran. The timing was the coarse
   stage alone, and reported scale as nearly free.
2. **Template the size of the scene** -- almost nowhere to search, so the
   per-position work barely happened.
3. **A build that failed** -- `Error 2` scrolled past and the previous
   executable ran, printing a table without the newly added line.

The probe carries comments about 1 and 2 so the next person does not repeat
them.
