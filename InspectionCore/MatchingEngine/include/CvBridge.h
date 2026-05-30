#ifndef CV_BRIDGE_H
#define CV_BRIDGE_H

// acvImage <-> cv::Mat bridge for the measurement rework (M1).
// acvImage is BGR interleaved (CVector[oy+y][ (ox+x)*3 + c ], c: 0=B,1=G,2=R),
// rows reached via the CVector row-pointer table, ROI-aware via GetROIOffsetX/Y.
// These COPY (measurement regions are small; correctness first).

#include <opencv2/core.hpp>
#include "acvImage.hpp"

// Channel 0 (B) of the acvImage ROI as a CV_8UC1 Mat. Grayscale images in this
// codebase replicate gray across BGR, so channel 0 == the gray value.
cv::Mat acvImageToGrayMat(acvImage *im);

// Full BGR ROI as a CV_8UC3 Mat.
cv::Mat acvImageToBgrMat(acvImage *im);

// Write a CV_8UC1 gray Mat back into the acvImage ROI (replicated to BGR).
// Sizes must match the ROI; out-of-size is clipped.
void grayMatToAcvImage(const cv::Mat &g, acvImage *im);

#endif // CV_BRIDGE_H
