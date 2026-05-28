#ifndef LABELING_CV_H
#define LABELING_CV_H

// OpenCV-backed connected-component labeling, drop-in for acvComponentLabeling.
// Produces the SAME 24-bit-RGB-packed label image (background = 255,255,255),
// so the existing acvLabeledRegionInfo and downstream contour extraction work
// unchanged. Faster and correct vs the contour-trace+scanline scheme (which can
// miss thin/small parts). Compiled only with FEATURE_OPENCV.
//
// Convention preserved: the component containing the drawn cage/frame (largest
// bbox) is label 1; other objects are 2,3,...  -> ld[1] stays the frame and the
// frame-intrusion check (an object connected to the cage merges into label 1)
// works naturally.

#include "acvImage.hpp"

void acvComponentLabeling_cv(acvImage *Pic, int connectivity = 8);

#endif
