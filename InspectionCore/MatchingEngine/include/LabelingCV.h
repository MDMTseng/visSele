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
#include "acvImage_ComponentLabelingTool.hpp"
#include <vector>

void acvComponentLabeling_cv(acvImage *Pic, int connectivity = 8);

// Optimized: one connectedComponentsWithStats pass produces BOTH the packed
// label image AND the acv_LabeledData list (area/bbox/centroid taken straight
// from CC stats), so the separate full-image acvLabeledRegionInfo re-scan is
// skipped. Same conventions: ld[1]=cage(frame), ld[2+]=objects, ld[0] empty.
void acvComponentLabeling_cv(acvImage *Pic, std::vector<acv_LabeledData> &ld, int connectivity = 8);

#endif
