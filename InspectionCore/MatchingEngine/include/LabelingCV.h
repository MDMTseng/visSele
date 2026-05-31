#ifndef LABELING_CV_H
#define LABELING_CV_H

// OpenCV-backed connected-component labeling. Produces a 24-bit-RGB-packed
// label image (background = 255,255,255) so the existing contour extraction
// works unchanged. Faster and correct vs the legacy contour-trace+scanline
// scheme (which could miss thin/small parts).
//
// Convention: the component containing the drawn cage/frame (largest bbox) is
// label 1; other objects are 2,3,...  -> ld[1] stays the frame and the
// frame-intrusion check (an object connected to the cage merges into label 1)
// works naturally.

#include <opencv2/core.hpp>
#include <vector>
#include "acvImage_ComponentLabelingTool.hpp"  // acv_LabeledData

// One connectedComponentsWithStats pass produces BOTH the packed label image
// AND the acv_LabeledData list (area/bbox/centroid).
//   Pic       : in/out, CV_8UC1 binary (bg=255/fg=0) OR CV_8UC3 BGR-replicated.
//               When `labelOut` is not provided, Pic is reallocated in-place
//               to CV_8UC3 and receives the BGR-packed labels (legacy).
//   labelOut  : optional separate output buffer (CV_8UC3 BGR-packed labels).
//               Use this to avoid the in-place type-change reallocation when
//               Pic is CV_8UC1 (Phase 1 fast path).
void acvComponentLabeling_cv(cv::Mat &Pic, std::vector<acv_LabeledData> &ld, int connectivity = 8);
void acvComponentLabeling_cv(cv::Mat &Pic, cv::Mat &labelOut, std::vector<acv_LabeledData> &ld, int connectivity = 8);


// =========================================================================
// Phase 2: combined CCL + morphological boundary + polar signature build.
// Replaces the BGR-packed-label + contour-walker + convertContourGrid2Signature
// pipeline with: CV_8UC1 binary -> CCL (32S labels + stats) -> erode+xor
// boundary mask -> per-pixel splat into dual signatures (A=anchors at direct
// hits, B=coverage via angular diffusion span ~ 1/R) -> per-label single-pass
// anchor-respecting linear interp (skips real angular gaps where B==0).
//
//   binary_uc1        : in, CV_8UC1, bg=255 / fg=0, cage already drawn.
//   ld                : out, indexed [0..numLabels-1] (label 0 = bg sentinel;
//                       label 1 = cage / largest-bbox fg; 2..N = real objects).
//   perLabelSignature : out, perLabelSignature[L][bin].x = R (in input-px),
//                       .y = theta (radians).  Empty for L=0.  Length N_bins.
//   perLabelCartesian : out, ordered ONLY by row-scan (not by walk order);
//                       v2 centroid iter uses them set-wise so order doesn't
//                       matter.  Coordinates are in input-px.
//   N_bins, K_min     : signature resolution and minimum diffusion span (bins).
//   connectivity      : CCL connectivity, 8 (default) or 4.
//
// All output coords / R values are in the input image's pixel space; callers
// that need mm or ideal coord must apply their own scaling (same as today
// for the legacy convertContourGrid2Signature output).
void buildLabeledSignatures_phase2(const cv::Mat &binary_uc1,
                                   std::vector<acv_LabeledData> &ld,
                                   std::vector<std::vector<acv_XY>> &perLabelSignature,
                                   std::vector<std::vector<acv_XY>> &perLabelCartesian,
                                   int N_bins = 360,
                                   int K_min = 2,
                                   int connectivity = 8);

#endif
