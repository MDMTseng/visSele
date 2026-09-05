#include "EdgeSelect.h"
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>

int edge_method_from_string(const char *s)
{
  if (!s) return EdgeSelectParams::STRONGEST;
  if (!strcmp(s, "first"))  return EdgeSelectParams::FIRST;
  if (!strcmp(s, "last"))   return EdgeSelectParams::LAST;
  if (!strcmp(s, "middle")) return EdgeSelectParams::MIDDLE;
  if (!strcmp(s, "nth"))    return EdgeSelectParams::NTH;
  return EdgeSelectParams::STRONGEST;
}
int edge_polarity_from_string(const char *s)
{
  if (!s) return EdgeSelectParams::ANY;
  if (!strcmp(s, "rising"))  return EdgeSelectParams::RISING;
  if (!strcmp(s, "falling")) return EdgeSelectParams::FALLING;
  return EdgeSelectParams::ANY;
}

bool edge_select(const float *g, int n, const EdgeSelectParams &p,
                 float *outPos, float *outStrength, EdgeSelectInfo *outInfo)
{
  if (outInfo) *outInfo = EdgeSelectInfo();
  if (!g || n < 3) return false;

  // adaptive noise floor: a real peak must exceed a fraction of the profile's
  // max gradient (rejects flat regions and noise that would otherwise register
  // as spurious "peaks", e.g. boundary zeros).
  float gmax = 0;
  for (int i = 0; i < n; i++) { float a = fabsf(g[i]); if (a > gmax) gmax = a; }
  float floor_ = p.min_strength;
  float adaptive = (p.rel_strength > 0 ? p.rel_strength : 0.f) * gmax;
  if (adaptive > floor_) floor_ = adaptive;
  if (gmax <= 0) return false;

  struct Peak { float pos; float strength; float signedV; float sharp; };
  std::vector<Peak> peaks;
  for (int i = 1; i < n - 1; i++)
  {
    float gi = g[i];
    // polarity gate
    if (p.polarity == EdgeSelectParams::RISING  && gi <= 0) continue;
    if (p.polarity == EdgeSelectParams::FALLING && gi >= 0) continue;
    float a = fabsf(g[i - 1]), b = fabsf(gi), c = fabsf(g[i + 1]);
    if (!(b >= a && b >= c && (b > a || b > c))) continue; // strict local max of |grad|
    if (b <= floor_) continue;
    // also require the neighbors share polarity-ish (avoid noise spikes): allow
    float denom = (a - 2 * b + c);
    float delta = (denom != 0) ? 0.5f * (a - c) / denom : 0.0f;
    if (delta < -1) delta = -1; if (delta > 1) delta = 1;
    // normalized peak sharpness: curvature |a-2b+c| relative to peak height. A crisp
    // edge has a tight gradient bump (neighbors << peak -> ~2); a blurry one ~0.
    float sharp = (b > 0) ? (-denom / b) : 0.0f; if (sharp < 0) sharp = 0;
    peaks.push_back({ (float)i + delta, b, gi, sharp });
  }
  if (peaks.empty()) return false;

  int sel = 0;
  switch (p.method)
  {
    case EdgeSelectParams::FIRST:  sel = 0; break;
    case EdgeSelectParams::LAST:   sel = (int)peaks.size() - 1; break;
    case EdgeSelectParams::MIDDLE: sel = (int)peaks.size() / 2; break;
    case EdgeSelectParams::NTH:    sel = std::min(std::max(p.nth, 0), (int)peaks.size() - 1); break;
    case EdgeSelectParams::STRONGEST:
    default:
    {
      sel = 0; float best = peaks[0].strength;
      for (int k = 1; k < (int)peaks.size(); k++) if (peaks[k].strength > best) { best = peaks[k].strength; sel = k; }
      break;
    }
  }
  if (outPos) *outPos = peaks[sel].pos;
  if (outStrength) *outStrength = peaks[sel].strength;
  if (outInfo)
  {
    float runnerUp = 0;
    for (int k = 0; k < (int)peaks.size(); k++)
      if (k != sel && peaks[k].strength > runnerUp) runnerUp = peaks[k].strength;
    outInfo->strength = peaks[sel].strength;
    outInfo->runnerUp = runnerUp;
    outInfo->signedStrength = peaks[sel].signedV;
    outInfo->peakCount = (int)peaks.size();
    outInfo->sharpness = peaks[sel].sharp;
  }
  return true;
}
