#ifndef SEARCH_POINT_CV_H
#define SEARCH_POINT_CV_H

// Robust search-point first-hit locator, ported from the CoreHub 2nd-gen engine
// (CoreHub/InspectionTarget/InspTar_DimMeasure.cpp InspectSearchPointFeature).
// A search point SCANS for the FIRST edge hit along a ray. Method:
//   1. Rectify a region centered at pt: X axis = search direction (+/-margin),
//      Y axis = the width band (across the edge). Sampled directly (no warp).
//   2. Blur ALONG the edge (Y) to denoise without moving the edge position.
//   3. X-Sobel -> per-row gradient along the search direction.
//   4. Per row, strongest-blob centroid = that row's edge position (polarity +
//      noise-suppress applied).
//   5. xPosMin across rows = the FIRST hit (closest edge along search).
//   6. Alpha-weighted average of edge points within `considerRange` of xPosMin
//      -> robust sub-pixel first-hit point. Map back to image px.
// FEATURE_OPENCV only.

#ifdef FEATURE_OPENCV
#include "acvImage.hpp"
#include "FeatureManager.h"   // acv_XY, FeatureManager_BacPac

enum SPEdgeType { SP_DARK_TO_LIGHT = 0, SP_LIGHT_TO_DARK = 1, SP_BOTH = 2 };

// gray: source image (the edgeTracking crop). pt/searchDir in that image's px.
// margin = search half-depth (px, region spans +/-margin along searchDir).
// width  = band across the edge (px). polarity per SPEdgeType (search-dir gradient
// sign). On success fills the sub-pixel edge point (image px) + total weight.
// labelImg/objLabel (optional): the labeled image + this object's label. When
// provided, a DILATED object mask (label==objLabel, grown by maskDilate px) zeroes
// the sobel response in background BEFORE the local-max search, so the scan can't
// lock onto background specks/dust (matches the legacy contour search's object
// constraint while staying grayscale -> still tunable for soft edges). labelImg
// must share `gray`'s coordinate frame (same crop/offset). Pass null to skip.
bool search_point_cv(acvImage *gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     int blurSize, float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     acvImage *labelImg, int objLabel, int maskDilate,
                     acv_XY *outPt, float *outW);

#endif // FEATURE_OPENCV
#endif // SEARCH_POINT_CV_H
