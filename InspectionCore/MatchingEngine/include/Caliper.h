#ifndef CALIPER_H
#define CALIPER_H

// Caliper / section measurement unit (mainstream metrology model; see
// docs/caliper_primitive_locating_design.md). A caliper straddles an expected
// edge: it samples the grayscale on a length(across-edge) x width(along-edge)
// grid, PROJECTS (averages) along the edge for high SNR, then runs the
// edge-selector toolbox on the 1-D profile to get one sub-pixel edge point.
//
// Replaces per-contour-pixel scanning: fewer, cleaner, more accurate points;
// works directly on grayscale (no binary contour needed).

#include "acvImage.hpp"
#include "FeatureManager.h"   // acv_XY, FeatureManager_BacPac (sampler: light comp)
#include "EdgeSelect.h"

struct CaliperParams
{
  float length = 20.0f; // search half-length ACROSS the edge (px); total span 2*length
  float width = 9.0f;   // projection width ALONG the edge (averaged), px
  float step = 1.0f;    // sampling step across the edge (px)
  EdgeSelectParams edge;
};

// Measure one caliper. center = caliper center (image px). searchDir = direction
// ACROSS the edge (need not be unit). bacpac optional (backlight compensation +
// could be used for light factor). On success fills the sub-pixel edge point
// (image px) and its strength, returns true.
bool caliper_measure(acvImage *gray, acv_XY center, acv_XY searchDir,
                     const CaliperParams &p, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outStrength, EdgeSelectInfo *outInfo = nullptr);

// ---- Phase 2: line locating via a row of calipers ----------------------------
struct CaliperLineResult
{
  acv_XY anchor;   // a point on the fitted line (weighted centroid of inliers)
  acv_XY dir;      // unit direction of the line
  float rms;       // inlier RMS perpendicular residual (px)
  int nValid;      // calipers that found an edge
  int nInlier;     // calipers kept after robust rejection
  bool ok;
};

// Place `count` calipers evenly along p0->p1 (caliper search direction =
// perpendicular to the line), measure each edge, and robust-fit the line
// (weighted TLS + MAD outlier rejection). A few wrong caliper points are
// rejected, so defects/non-standard spots don't drag the fit.
CaliperLineResult caliper_locate_line(acvImage *gray, acv_XY p0, acv_XY p1,
                                      int count, const CaliperParams &cal,
                                      FeatureManager_BacPac *bacpac);

// ---- Phase 3: circle/arc locating via radial calipers ------------------------
struct CaliperCircleResult
{
  acv_XY center;
  float radius;
  float rms;     // inlier RMS radial residual (px)
  int nValid;
  int nInlier;
  bool ok;
};

// Place `count` calipers along the arc [angStart,angEnd] (rad) of the nominal
// circle (center,radius); each caliper searches RADIALLY across the edge.
// Robust algebraic (Kasa) circle fit + MAD outlier rejection.
CaliperCircleResult caliper_locate_circle(acvImage *gray, acv_XY center, float radius,
                                          float angStart, float angEnd, int count,
                                          const CaliperParams &cal, FeatureManager_BacPac *bacpac);

#endif
