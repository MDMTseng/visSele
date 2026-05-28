# Fragment-Vote Contour Matching (experimental / parked)

**Status: TOY / PARKED.** Not built into the system, not on the `matching_method`
path. Kept here as a reference experiment. For production occlusion/clamp-robust
matching, the recommendation is **line2Dup** (see "Conclusion" below).

## Goal
Match an object's pose (position + rotation) when its silhouette is **broken,
partially occluded, or connected to a fixture/clamp** — cases that defeat the
centroid + closed-contour signature methods (a connected clamp shifts the blob
centroid and corrupts the polar signature).

## Approach
Generalized-Hough / contour-fragment family:
1. Template = closed contour as a **turning function** (tangent angle vs uniform
   arc length; rotation/translation invariant).
2. Runtime: edges traced into **fragments**; each fragment's turning function is
   slide-matched against the template (1D correlation) → candidate arc offset(s).
3. Each candidate → **closed-form Procrustes** on the matched point pairs →
   precise (R, t) → a pose vote (x, y, θ).
4. Cluster votes; consensus **peaks** = object instances. Object fragments agree;
   clamp/clutter fragments scatter and are ignored.

## What was validated
- `proto_rotation_only.cpp`: rotation-only proof. A connected clamp's straight
  edges vote scattered rotations while object fragments agree → **consensus peak
  recovers the object rotation (0.31° err) and ignores the clamp.** PASS.
  (Key lesson: aggregate by consensus PEAK/clustering, NOT mean or residual
  threshold — straight clamp edges get low residual and would pollute a mean.)
- Speed (`bench_vs_polar_signature.cpp`): the fragment-vote core is fast
  (~0.17 ms for a 6-fragment scene). **Speed is not the blocker.**

## Why it's parked (the failure mode)
`test_multiobject_clamp.cpp` (3 objects + clamp + occlusion + noise) exposed the
hard part. Even with the right techniques — **Lowe-style ratio test**, **Procrustes
refine**, and **top-K candidates + voting consensus** (all from the SOTA
literature) — reliable per-fragment **rotation** did not emerge:
- Positions clustered correctly per object, but rotations scattered.
- Root cause: a **partial arc of a smooth/near-circular shape cannot reliably be
  matched to the correct template arc** by its turning function. Wrong
  correspondences are "confidently wrong" (clearly beat other wrong ones, passing
  the ratio test). For a near-circular object, `R·origin + t` lands near the true
  center for ANY correspondence, so position is recovered but rotation needs the
  correct arc — which is exactly what's unreliable.
- Making this production-grade needs the full heavy pipeline: GACS multi-segment
  grouping for distinctiveness, multi-scale fragments, coverage/residual
  verification, etc. High effort, uncertain payoff, and weak on near-circular parts.

## Conclusion / recommendation
Use **line2Dup** (gradient-orientation template matching; SOTA #1 and already in
this repo at `contrib/shape_based_matching`, used by `MatchingEngine/FM_GenMatching.cpp`).
It sidesteps contour correspondence entirely (2D gradient-orientation search),
degrades linearly under occlusion (handles clamps), is fast, multi-object, and
sub-pixel — which is why Halcon/Cognex use this family. See the agent memory
`reference_shape_matching_sota.md` for the full survey and the fragment-vote
production-pipeline notes if this is ever revived.

## Build / run the experiments (standalone, from repo root)
```sh
INC=$(pwd); B=$INC/build/mac-arm64   # after a normal FEATURE_OPENCV build
g++ -std=c++14 -O2 -w \
  -I lab/fragment_vote_match \
  -I MatchingEngine/include -I MatchingEngine/include_priv -I acvImage/include \
  -I contrib/circleFitting -I common_lib/include -I contrib/cJSON \
  -I CameraLayer/include -I logctrl/include -I contrib/polyfit/include \
  lab/fragment_vote_match/test_multiobject_clamp.cpp \
  lab/fragment_vote_match/FragmentVoteMatch.cpp \
  $B/lib*.a -L/opt/homebrew/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_calib3d \
  -o /tmp/fv_test && /tmp/fv_test
```
`proto_rotation_only.cpp` is self-contained (`g++ -std=c++14 -O2 proto_rotation_only.cpp -o /tmp/p && /tmp/p`).
Add `-DFV_DEBUG` to `FragmentVoteMatch.cpp` for per-fragment vote diagnostics.

## Files
- `FragmentVoteMatch.{cpp,h}` — the matcher (turning-fn match + top-K + Procrustes + cluster).
- `proto_rotation_only.cpp` — the working rotation-only consensus proof (clamp rejection).
- `test_multiobject_clamp.cpp` — multi-object + clamp + occlusion harness (exposes the limit).
- `bench_vs_polar_signature.cpp` — speed micro-benchmark.
