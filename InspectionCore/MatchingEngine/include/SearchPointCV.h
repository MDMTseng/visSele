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

#include <opencv2/core.hpp>
#include <vector>
#include "FeatureManager.h"   // acv_XY, FeatureManager_BacPac
#include "FeatureReport.h"    // CaliperHit (for optional per-edge plumb-out)

enum SPEdgeType { SP_DARK_TO_LIGHT = 0, SP_LIGHT_TO_DARK = 1, SP_BOTH = 2 };

// gray: source image (the edgeTracking crop). pt/searchDir in that image's px.
// margin = search half-depth (px, region spans +/-margin along searchDir).
// width  = band across the edge (px). polarity per SPEdgeType (search-dir gradient
// sign). On success fills the sub-pixel edge point (image px) + total weight.
// There is no mask parameter, deliberately -- see the note at the top of
// SearchPointCV.cpp before adding one back.
// outHits (optional): when non-null, populated with one CaliperHit per
// strength-gated row edge (the `eps` set). status=2 if within considerRange
// of the perp-top (contributed to the final point), else 1; strength=peak
// gradient. Coords in the SAME frame as outPt (gray's image coords).
// outPeaks (optional): every candidate the selector saw, UNGATED -- the
// evidence a min_strength is set against. See SearchPointPeaks.
bool search_point_cv(const cv::Mat &gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outW, int spId = -1,
                     std::vector<CaliperHit> *outHits = nullptr,
                     // Set when ANY sample of the scan band fell outside the
                     // image. The band is then not the band the def asked for,
                     // and the answer is a best-effort over whatever was left --
                     // which is exactly what a measurement must not silently be.
                     bool *outClipped = nullptr,
                     SearchPointPeaks *outPeaks = nullptr,
                     // relStrength: keep candidates at or above this fraction of
                     // the strongest peak in the window, then take the nearest
                     // survivor. 0.40 is what this was hard-coded to; 0 turns it
                     // off and leaves min_strength as the only floor. See
                     // featureDef_searchPoint::rel_strength for why it is a
                     // number now rather than a constant.
                     float relStrength = 0.40f,
                     // Set to the number of candidates that cleared
                     // min_strength, sat NEARER than the one measured, and were
                     // dropped by relStrength -- i.e. how much of the answer
                     // came from the relative rule rather than the def's floor.
                     int *outRelMoved = nullptr);

#endif // SEARCH_POINT_CV_H
