#include "EdgeSignature.h"
#include <math.h>
#include <algorithm>

// Bilinear grayscale sample at (x,y); returns -1 if out of bounds.
static float sampleBilinear(acvImage *img, float x, float y, int ch)
{
  int W = img->GetWidth(), H = img->GetHeight();
  if (x < 0 || y < 0 || x >= W - 1 || y >= H - 1) return -1;
  int x0 = (int)x, y0 = (int)y;
  float fx = x - x0, fy = y - y0;
  unsigned char *r0 = img->CVector[y0];
  unsigned char *r1 = img->CVector[y0 + 1];
  float a = r0[x0*3+ch], b = r0[(x0+1)*3+ch];
  float c = r1[x0*3+ch], d = r1[(x0+1)*3+ch];
  return (a*(1-fx)+b*fx)*(1-fy) + (c*(1-fx)+d*fx)*fy;
}

bool edge_signature_from_gray(acvImage *grayImg, acv_XY center, float searchRadius,
                              const EdgeSignatureParams &p,
                              std::vector<float> &out_radius,
                              std::vector<float> &out_strength,
                              int channel)
{
  if (!grayImg) return false;
  int N = p.nBins;
  float rMax = (p.rMax > 0) ? p.rMax : searchRadius;
  if (rMax <= p.rInner) return false;
  out_radius.assign(N, 0);
  out_strength.assign(N, 0);

  for (int i = 0; i < N; i++)
  {
    float th = 2.0f * (float)M_PI * i / N;
    float dx = cosf(th), dy = sinf(th);

    // Walk outward; track gradient |dI/dr| along the ray. Keep the OUTERMOST
    // peak above threshold (the silhouette), with sub-pixel parabolic refine.
    float bestR = 0, bestStr = 0;
    float gPrev2 = 0, gPrev = 0; float rPrev = 0;
    bool havePrev = false, havePrev2 = false;
    for (float r = p.rInner; r <= rMax; r += p.step)
    {
      float iIn  = sampleBilinear(grayImg, center.X + dx*(r - p.step), center.Y + dy*(r - p.step), channel);
      float iOut = sampleBilinear(grayImg, center.X + dx*(r + p.step), center.Y + dy*(r + p.step), channel);
      if (iIn < 0 || iOut < 0) { havePrev = havePrev2 = false; continue; }
      float g = fabsf(iOut - iIn); // |gradient| along the ray (exposure-offset invariant)

      // local maximum at gPrev (peak detection on the previous sample)
      if (havePrev2 && gPrev >= gPrev2 && gPrev >= g && gPrev >= p.minEdgeStrength)
      {
        // parabolic sub-pixel peak around rPrev
        float denom = (gPrev2 - 2*gPrev + g);
        float delta = (denom != 0) ? 0.5f * (gPrev2 - g) / denom : 0.0f;
        float rPeak = rPrev + delta * p.step;
        bestR = rPeak; bestStr = gPrev;   // keep updating -> ends at OUTERMOST peak
      }
      gPrev2 = gPrev; gPrev = g; rPrev = r;
      havePrev2 = havePrev; havePrev = true;
    }
    out_radius[i]   = bestR;
    out_strength[i] = bestStr;
  }
  return true;
}

EdgeSigMatch edge_signature_match(const std::vector<float> &ref_r, const std::vector<float> &ref_w,
                                  const std::vector<float> &cur_r, const std::vector<float> &cur_w,
                                  int searchRange_bins, bool allowFlip)
{
  int N = (int)ref_r.size();
  EdgeSigMatch best{0, false, 0, 1e30f};
  if (N == 0 || (int)cur_r.size() != N) return best;

  // mean reference radius (for similarity normalisation)
  double mr = 0; int mc = 0;
  for (int i = 0; i < N; i++) if (ref_w[i] > 0) { mr += ref_r[i]; mc++; }
  mr = (mc > 0) ? mr / mc : 1.0;

  int lo = (searchRange_bins <= 0) ? 0 : -searchRange_bins;
  int hi = (searchRange_bins <= 0) ? N - 1 : searchRange_bins;

  for (int flip = 0; flip <= (allowFlip ? 1 : 0); flip++)
  {
    for (int off = lo; off <= hi; off++)
    {
      // robust weighted error with trimming of the worst bins (whisker/dust)
      std::vector<float> resid; resid.reserve(N);
      std::vector<float> wts; wts.reserve(N);
      for (int i = 0; i < N; i++)
      {
        int j = flip ? ((N - i) % N) : i;       // mirror for flip
        int k = ((j + off) % N + N) % N;         // rotate reference
        float wc = cur_w[i], wr = ref_w[k];
        if (wc <= 0 || wr <= 0) continue;         // skip invalid bins
        float w = (wc < wr) ? wc : wr;
        float d = ref_r[k] - cur_r[i];
        resid.push_back(d * d);
        wts.push_back(w);
      }
      if (resid.size() < (size_t)(N / 4)) continue; // too few overlapping bins

      // trim worst 10% (defect bins) before the weighted average
      std::vector<int> idx(resid.size());
      for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;
      std::sort(idx.begin(), idx.end(), [&](int a, int b){ return resid[a] < resid[b]; });
      int keep = (int)(idx.size() * 0.9);
      double sw = 0, swe = 0;
      for (int t = 0; t < keep; t++) { int id = idx[t]; sw += wts[id]; swe += wts[id]*resid[id]; }
      if (sw <= 0) continue;
      float err = (float)(swe / sw);
      if (err < best.error)
      {
        best.error = err;
        best.angle_rad = -2.0f * (float)M_PI * off / N; // rotation to apply (sign matches existing)
        best.flipped = (flip != 0);
        best.similarity = 1.0f - sqrtf(err) / (float)mr;
        if (best.similarity < 0) best.similarity = 0;
      }
    }
  }
  return best;
}
