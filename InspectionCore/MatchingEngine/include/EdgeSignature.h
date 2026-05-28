#ifndef EDGE_SIGNATURE_H
#define EDGE_SIGNATURE_H

// Edge-response rotational signature for lighting-robust orientation matching.
//
// Instead of the radius of the BINARY silhouette per angle (threshold/lighting
// sensitive), this casts a ray from the object centroid through the GRAYSCALE
// image and records the outermost strong intensity-gradient edge per angle:
//   - sub-pixel radius (geometry) -- exposure-invariant: a gradient edge's
//     position does not move with brightness offset/gain, unlike a fixed
//     threshold's silhouette.
//   - edge strength (|gradient|)  -- used to gate weak responses (dust/faint
//     scratch) and to weight the match.
// Interior scratches/texture are ignored (we take the OUTERMOST strong edge);
// whiskers/dust on the boundary perturb a few bins and are absorbed by the
// robust, strength-weighted correlation.
//
// Output radius/strength arrays are length nBins (angle bin i -> 2*pi*i/nBins).
// A strength of 0 marks an invalid/unfound bin (excluded from matching).

#include "acvImage.hpp"
#include "MatchingCore.h"
#include <vector>

struct EdgeSignatureParams
{
  int nBins = 360;
  float rInner = 2.0f;     // skip pixels near the centroid
  float rMax = 0.0f;       // 0 => auto from search bound
  float step = 0.5f;       // ray sampling step (px)
  float minEdgeStrength = 8.0f; // |gradient| threshold to accept an edge
};

// Build the edge-response signature. center & rMax are in the same (pixel)
// coordinates as grayImg. channel: which image channel to read (0 for gray).
bool edge_signature_from_gray(acvImage *grayImg, acv_XY center, float searchRadius,
                              const EdgeSignatureParams &p,
                              std::vector<float> &out_radius,
                              std::vector<float> &out_strength,
                              int channel = 0);

struct EdgeSigMatch { float angle_rad; bool flipped; float similarity; float error; };

// Robust, strength-weighted circular correlation of two edge signatures
// (same nBins). Searches angle offsets within +/-searchRange_bins; if allowFlip,
// also tries the mirrored signature. Returns best angle/flip/similarity.
EdgeSigMatch edge_signature_match(const std::vector<float> &ref_r, const std::vector<float> &ref_w,
                                  const std::vector<float> &cur_r, const std::vector<float> &cur_w,
                                  int searchRange_bins, bool allowFlip);

#endif
