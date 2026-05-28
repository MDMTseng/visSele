#ifndef FRAGMENT_VOTE_MATCH_H
#define FRAGMENT_VOTE_MATCH_H

// Contour-fragment matching + pose voting (generalized-Hough family) for
// orientation/position of objects whose silhouette may be BROKEN, PARTIALLY
// OCCLUDED, or CONNECTED to a fixture/clamp -- cases that defeat centroid +
// closed-contour signature methods.
//
// Idea (validated): represent the template contour as a turning function
// (tangent angle vs arc length, rotation/translation invariant). At runtime,
// trace edges into continuous FRAGMENTS; match each fragment's turning function
// to the template (1D correlation) -> candidate (rotation, arc offset); each
// candidate votes a full object pose (x,y,theta). Object fragments agree -> a
// consensus PEAK; clamp/clutter fragments scatter -> ignored. A least-squares
// (Procrustes) refine on the peak's inlier correspondences gives a precise pose.
// Multiple peaks => multiple object instances in one frame.
//
// Pure C++ (no OpenCV). Input contours/fragments are ordered acv_XY point sets
// (the image edge-tracing step that produces fragments is separate).

#include "MatchingCore.h"
#include <vector>

struct FVTemplate
{
  std::vector<acv_XY> pts; // resampled at uniform arc-length ds, CLOSED
  std::vector<float> tau;  // turning function (periodic), same length as pts
  acv_XY origin;           // object reference point (template centroid)
  float ds;                // arc-length spacing
};

struct FVPose
{
  float x, y, theta; // object reference point position + rotation (rad)
  float score;       // total vote weight
  int inliers;       // # fragments supporting this pose
};

struct FVParams
{
  float ds = 4.0f;              // arc-length resample spacing (px)
  float residual_thresh = 0.05f;// max mean-sq tangent residual to accept a match (rad^2)
  int   max_cands_per_frag = 3; // keep this many best arc-offsets per fragment
  float pos_tol = 8.0f;         // clustering position tolerance (px)
  float ang_tol = 0.05f;        // clustering angle tolerance (rad)
  int   min_inliers = 3;        // min supporting fragments for a reported pose
  float min_curv_span = 0.3f;   // reject near-straight fragments (low info) below this
  // Ratio test (Lowe-style): a fragment is only allowed to vote if its BEST
  // template match is clearly better than its 2nd-best (distinctive). best <
  // peak_ratio * second_best. Straight/ambiguous fragments (clamp edges) have
  // best ~= second_best -> rejected automatically.
  float peak_ratio = 0.6f;
  int   guard_bins = 12;        // exclude this neighborhood of the best when finding 2nd
};

// Build a template from an ordered (closed) contour.
FVTemplate fv_build_template(const std::vector<acv_XY> &contour, const FVParams &p);

// Match fragments (each an ordered open polyline) -> object poses (peaks).
std::vector<FVPose> fv_match(const FVTemplate &tmpl,
                             const std::vector<std::vector<acv_XY>> &fragments,
                             const FVParams &p);

#endif
