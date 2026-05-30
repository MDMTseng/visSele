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
// COPIES (need to demux interleaved BGR -> single channel).
cv::Mat acvImageToGrayMat(acvImage *im);

// Full BGR ROI as a CV_8UC3 Mat. COPIES.
cv::Mat acvImageToBgrMat(acvImage *im);

// Write a CV_8UC1 gray Mat back into the acvImage ROI (replicated to BGR).
// Sizes must match the ROI; out-of-size is clipped. COPIES.
void grayMatToAcvImage(const cv::Mat &g, acvImage *im);

// Zero-copy BGR view: returns a cv::Mat header pointing into the acvImage's
// underlying storage. Writes via the returned Mat mutate the acvImage. The view
// is valid only while the acvImage's buffer is alive (and ROI not changed).
// acvImage row layout (CVector[i] = CVector[i-1] + RealWidth*Channel) is
// contiguous, so the BGR pixels can be wrapped without copying. Grayscale
// view is NOT offered because channel 0 lives every-3rd-byte and cv::Mat can't
// express that as a single-channel header without copying.
cv::Mat acvImageBgrView(acvImage *im);

#endif // CV_BRIDGE_H
