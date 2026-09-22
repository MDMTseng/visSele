#ifndef EDGE_SELECT_H
#define EDGE_SELECT_H

// Extensible sub-pixel edge selector for primitive locating. Replaces the old
// single "centroid of the gradient profile" (which lands between peaks on
// non-standard edges) with explicit peak detection + selection rule + polarity.
//
// Given a gradient profile along a scan ray, it finds the candidate gradient
// PEAKS (local maxima of |grad|) of the requested polarity and picks ONE by a
// configurable rule. Sub-pixel position comes from a parabola at the chosen
// peak. Designed to grow: add a Method enum value + a case in edge_select.
//
// Backward-compat: on a clean symmetric step edge there is a single peak whose
// location equals the gradient centroid, so STRONGEST matches the old result;
// only non-standard (multi/asymmetric) edges change (where the old was wrong).

struct EdgeSelectParams
{
  enum Method { STRONGEST = 0, FIRST, LAST, MIDDLE, NTH };
  enum Polarity { ANY = 0, RISING, FALLING }; // RISING = dark->light (grad>0)
  int method = STRONGEST;
  int polarity = ANY;
  int nth = 0;            // index for NTH (0-based, clamped)
  float min_strength = 0; // ignore peaks weaker than this (|grad|)
  // The floor is the LARGER of min_strength and rel_strength * (strongest
  // |grad| in the profile). 0.15 is what the selector always did, silently;
  // it moves with whatever enters the window, so a measured min_strength
  // sets this to 0 to make the floor absolute. See EdgeProfileView.jsx.
  float rel_strength = 0.15f;
  // Across-edge Gaussian on the intensity profile before differencing, in
  // px. 0 = none (the historical behaviour). A wide soft edge splits into
  // several adjacent gradient maxima without it, and first/nth then pick an
  // edge of the hump rather than its centre.
  float sigma = 0;
};

// Extra characteristics of the chosen edge, for quality/abnormality checks
// (e.g. anchor/datum validation). Filled when an EdgeSelectInfo* is passed.
struct EdgeSelectInfo
{
  float strength = 0;       // chosen peak |grad| (== outStrength)
  float runnerUp = 0;       // 2nd-strongest peak |grad| among all candidates (0 if none)
  float signedStrength = 0; // signed grad at the chosen peak (>0 rising, <0 falling)
  int   peakCount = 0;      // # candidate peaks of the requested polarity
  float sharpness = 0;      // normalized peak curvature |a-2b+c|/b (~0 blurry .. ~2 crisp)
  // ambiguity ratio = runnerUp/strength in [0,1]; closer to 1 = more ambiguous.
};

// signedGrad[n] = signed intensity gradient along the ray. Returns true and the
// sub-pixel peak position (in [0,n-1]) + its |grad| strength, or false if no
// matching peak. quality is the peak strength (higher = sharper/cleaner).
// outInfo (optional) receives runner-up/polarity/peak-count for abnormality checks.
bool edge_select(const float *signedGrad, int n, const EdgeSelectParams &p,
                 float *outPos, float *outStrength, EdgeSelectInfo *outInfo = nullptr);

// Map config strings (for def parsing).
int edge_method_from_string(const char *s);   // "strongest"/"first"/"last"/"middle"/"nth"
int edge_polarity_from_string(const char *s); // "any"/"rising"/"falling"

#endif
