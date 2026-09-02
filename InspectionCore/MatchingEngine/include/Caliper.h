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

#include <opencv2/core.hpp>
#include "FeatureManager.h"   // acv_XY, FeatureManager_BacPac (sampler: light comp)
#include "EdgeSelect.h"
#include <vector>

struct CaliperParams
{
  float length = 20.0f; // search half-length ACROSS the edge (px); total span 2*length
  float width = 9.0f;   // projection width ALONG the edge (averaged), px
  float step = 1.0f;    // sampling step across the edge (px)
  EdgeSelectParams edge;
  // Robustness gates (0 / <=0 ⇒ "use engine default").
  //   min_inliers — caliper_locate_* returns ok=false if final nInlier < this.
  //                 Default fallback: line=2, circle=3.
  //   max_error   — px hard cap on the MAD-derived outlier threshold; any
  //                 point with |residual| > max_error is rejected as outlier
  //                 even if MAD would have kept it. <=0 ⇒ no cap.
  int   min_inliers = 0;
  float max_error   = 0;
};

// Measure one caliper. center = caliper center (image px). searchDir = direction
// ACROSS the edge (need not be unit). bacpac optional (backlight compensation +
// could be used for light factor). On success fills the sub-pixel edge point
// (image px) and its strength, returns true.
// outProfile/outPos (optional, for debug): the across-edge averaged grayscale
// profile and the sub-pixel edge index into it (0..nAcross-1).
// outGrad (optional): the SIGNED across-edge gradient the edge selector ran on
// -- the ungated evidence behind the answer, for the threshold UI. See
// CaliperProfiles in FeatureReport.h for why it is the whole profile and not
// the chosen peak.
bool caliper_measure(const cv::Mat &gray, acv_XY center, acv_XY searchDir,
                     const CaliperParams &p, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outStrength, EdgeSelectInfo *outInfo = nullptr,
                     std::vector<float> *outProfile = nullptr, float *outPos = nullptr,
                     std::vector<float> *outGrad = nullptr);

// ---- Search-point first-hit scan (CoreHub remap+sobel+topmost, ported) -------
// A search point SCANS for the FIRST edge hit along a ray; it must NOT average
// across the width (that smooths/shifts the first-hit point). Instead this lays
// `width` parallel scan columns across the ray, finds the first-hit edge per
// column independently (gradient along searchDir + edge_select, polarity), and
// robustly combines the per-column first-hit distances (median + strength-
// weighted mean of inliers). One-sided: scans [start, start+length] along
// searchDir. Returns the sub-pixel edge point on the ray (image px) + strength.
bool search_point_scan(const cv::Mat &gray, acv_XY start, acv_XY searchDir,
                       float length, float width, float step,
                       const EdgeSelectParams &edge, FeatureManager_BacPac *bacpac,
                       acv_XY *outPt, float *outStrength);

// CaliperHit is defined in FeatureReport.h (transitively reachable via
// FeatureManager.h above). Keeping its definition there avoids a circular
// include — FeatureReport.h is the public-facing consumer-side aggregator.

// ---- Phase 2: line locating via a row of calipers ----------------------------
struct CaliperLineResult
{
  acv_XY anchor;   // a point on the fitted line (weighted centroid of inliers)
  acv_XY dir;      // unit direction of the line
  float rms;       // inlier RMS perpendicular residual (px)
  int nValid;      // calipers that found an edge
  int nInlier;     // calipers kept after robust rejection
  float confidence;// mean inlier edge confidence (strength*unambiguity*sharpness)
  bool ok;
  std::vector<CaliperHit> hits; // length == count; entry i is the i'th caliper
  CaliperProfiles prof;         // only when DbgEmit("edge_profile")
};

// Place `count` calipers evenly along p0->p1 (caliper search direction =
// perpendicular to the line), measure each edge, and robust-fit the line
// (weighted TLS + MAD outlier rejection). A few wrong caliper points are
// rejected, so defects/non-standard spots don't drag the fit.
// dbgName (optional): when set AND env CALIP_DUMP is present, writes
// /tmp/calip_line_<dbgName>.png -- each caliper's across-edge profile stacked as one
// column (caliper index = x, across-edge = y), with the picked edge marked
// (green = inlier, red = outlier, gray = no edge) and the primitive name drawn on it.
// imgOffset: the crop offset of `gray` within the full sensor image (i.e. a
// point at `gray` coord q lives at full-image px q+imgOffset). When bacpac
// carries a valid lensCalib, each edge hit is lens-undistorted in full-image px
// BEFORE the fit (distortion bends a straight edge into a curve, so undistorting
// only the fitted result would leave the fit biased). Default {0,0} = no crop.
// THE CALIPER CONTRACT: what a def's `caliper` block actually resolves to.
//
// Two stages, and the order matters because it is where the WebUI used to get
// this wrong -- 15 of 24 swept inputs disagreed before 2026-08-26.
//
//   PARSE    an absent `caliper` object, or an absent key inside one, takes the
//            parser default (count 10, width 0.5 mm) and then a cap. A PRESENT
//            0 is 0 here; it is not "unset".
//   EXECUTE  a floor, which differs by primitive because a line fit needs two
//            calipers and an arc needs three.
//
// Collected here so there is ONE statement of it. It lived in four places --
// the circle and line parsers, and the top of each locate function -- which is
// three more than can be kept in agreement, and is what the WebUI was mirroring
// by hand. test_suite/geom_vectors_emit.cpp calls this, so the WebUI is held to
// what the machine does rather than to a transcription of it.
#define CALIPER_PARSE_DEFAULT_COUNT   10
#define CALIPER_PARSE_DEFAULT_WIDTH   0.5f
#define CALIPER_MAX_COUNT             512
#define CALIPER_MAX_WIDTH             64
#define CALIPER_MAX_LENGTH            256
#define CALIPER_MIN_COUNT_LINE        2      // caliper_locate_line
#define CALIPER_MIN_COUNT_ARC         3      // caliper_locate_circle

// The execute-stage floor. Called by both locate functions AND by the vector
// emitter, so they cannot answer differently.
static inline int caliper_effective_count(int count, int minCount)
{
  return (count < minCount) ? minCount : count;
}

CaliperLineResult caliper_locate_line(const cv::Mat &gray, acv_XY p0, acv_XY p1,
                                      int count, const CaliperParams &cal,
                                      FeatureManager_BacPac *bacpac,
                                      const char *dbgName = nullptr,
                                      acv_XY imgOffset = {0.0f, 0.0f});

// ---- Phase 3: circle/arc locating via radial calipers ------------------------
struct CaliperCircleResult
{
  acv_XY center;
  float radius;
  float rms;     // inlier RMS radial residual (px)
  int nValid;
  int nInlier;
  float confidence;// mean inlier edge confidence
  bool ok;
  std::vector<CaliperHit> hits; // length == count; entry i is the i'th caliper
  CaliperProfiles prof;         // only when DbgEmit("edge_profile")
};

// Place `count` calipers along the arc [angStart,angEnd] (rad) of the nominal
// circle (center,radius); each caliper searches RADIALLY across the edge.
// Robust algebraic (Kasa) circle fit + MAD outlier rejection.
// dbgName (optional): with env CALIP_DUMP, writes /tmp/calip_arc_<dbgName>.png --
// each radial caliper's profile as one column (x = caliper index around the arc,
// y = radial across-edge), picked edge marked, like the line strip.
// imgOffset: see caliper_locate_line — full-image-px lens undistortion of each
// radial edge hit before the Kasa fit. Default {0,0} = no crop.
CaliperCircleResult caliper_locate_circle(const cv::Mat &gray, acv_XY center, float radius,
                                          float angStart, float angEnd, int count,
                                          const CaliperParams &cal, FeatureManager_BacPac *bacpac,
                                          const char *dbgName = nullptr,
                                          acv_XY imgOffset = {0.0f, 0.0f});

#endif
