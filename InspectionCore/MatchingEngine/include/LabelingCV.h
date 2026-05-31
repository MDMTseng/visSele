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

#endif
