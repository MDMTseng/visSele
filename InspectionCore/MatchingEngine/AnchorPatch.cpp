#include "AnchorPatch.h"
#include <math.h>

// ZNCC of tpl over win at one integer offset (dx,dy). Returns NaN if any sampled
// cell is NaN or a patch is flat (zero variance).
static float zncc_at(const float *tpl, int wt, int ht,
                     const float *win, int ww, int dx, int dy)
{
  int n = wt * ht;
  double sa = 0, sb = 0;
  for (int y = 0; y < ht; y++)
    for (int x = 0; x < wt; x++)
    {
      float a = tpl[y * wt + x];
      float b = win[(y + dy) * ww + (x + dx)];
      if (a != a || b != b) return NAN; // NaN guard
      sa += a; sb += b;
    }
  double ma = sa / n, mb = sb / n;
  double num = 0, da = 0, db = 0;
  for (int y = 0; y < ht; y++)
    for (int x = 0; x < wt; x++)
    {
      double a = tpl[y * wt + x] - ma;
      double b = win[(y + dy) * ww + (x + dx)] - mb;
      num += a * b; da += a * a; db += b * b;
    }
  if (da <= 0 || db <= 0) return NAN; // flat patch -> undefined correlation
  return (float)(num / sqrt(da * db));
}

float anchor_patch_zncc(const float *tpl, int wt, int ht,
                        const float *win, int ww, int wh,
                        int *bestDx, int *bestDy)
{
  if (!tpl || !win || wt < 2 || ht < 2 || ww < wt || wh < ht) return -2.0f;
  float best = -2.0f; int bdx = 0, bdy = 0;
  for (int dy = 0; dy <= wh - ht; dy++)
    for (int dx = 0; dx <= ww - wt; dx++)
    {
      float s = zncc_at(tpl, wt, ht, win, ww, dx, dy);
      if (s == s && s > best) { best = s; bdx = dx; bdy = dy; }
    }
  if (best <= -2.0f) return -2.0f; // every placement invalid
  if (bestDx) *bestDx = bdx;
  if (bestDy) *bestDy = bdy;
  return best;
}
