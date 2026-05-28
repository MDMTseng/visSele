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
                     acv_XY *outPt, float *outStrength);

#endif
