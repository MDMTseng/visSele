// Synthetic variation for the trigger-driven fake camera.
//
// The carousel replays the same two PNGs for ever, so a bench run exercises the
// matching engine without ever CHALLENGING it: the same pose, the same noise,
// the same answer, twenty thousand times. A soak like that proves the pipeline
// moves parts; it proves nothing about locating one.
//
// This warps each released frame before it enters the pipeline: put the object
// where the recipe is looking, then vary it -- rotate, skew, scale, noise --
// within bounds the caller sets. Everything downstream (SBM/sig360 locating,
// the caliper scans, the limits) then sees a part it has not seen before.
//
// OFF unless INSP_CAM_AUG is set. It is a camera-less bench fixture and has no
// business anywhere near a machine with a lens on it.
//
//   INSP_CAM_AUG=1              enable
//   INSP_CAM_AUG_ROT=<deg>      rotation, uniform in +/-deg     (default 0)
//   INSP_CAM_AUG_SKEW=<f>       shear coefficient, +/-f          (default 0)
//   INSP_CAM_AUG_SCALE=<f>      scale, uniform in 1+/-f          (default 0)
//   INSP_CAM_AUG_JITTER=<px>    extra translation, +/-px         (default 0)
//   INSP_CAM_AUG_NOISE=<sigma>  additive gaussian, grey levels   (default 0)
//   INSP_CAM_AUG_BLUR=<px>      gaussian blur sigma              (default 0)
//   INSP_CAM_AUG_PLACE=0|1      move the object into the inspection region
//                                                                (default 1)
//   INSP_CAM_AUG_SEED=<n>       base seed; the frame counter is mixed in so a
//                               run is reproducible and a frame is not
//                               identical to its neighbour        (default 1)
//
// The transform is ONE affine, composed and applied once. Doing the steps as
// separate warps would resample the image three times and the blur that
// introduces is indistinguishable from a genuine focus problem -- which is
// exactly the kind of thing a locating test must not invent.
#pragma once
#include <opencv2/core.hpp>
#include <cstdint>

namespace camTrigAugment
{
  bool on();
  // seq: the frame's ordinal, so the same run replays identically.
  // regionCx/Cy: the inspection region's centre in IMAGE space (already
  // sampler-offset corrected), or negative if there is none to aim at.
  void apply(cv::Mat &img, uint64_t seq, float regionCx, float regionCy);
}
