#include "SearchPointCV.h"
#ifdef FEATURE_OPENCV
#include <opencv2/opencv.hpp>
#include "acvImage_SpDomainTool.hpp" // acvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <vector>
#include <cmath>

// Per-row edge position: strongest gradient "blob" centroid along the row, after
// polarity selection + noise suppression. Ported from CoreHub getCenter_LOCAL_AVGs.
static float rowEdgeCenter(int16_t *line, int n, int16_t noise,
                           SPEdgeType et, float &w, float &sigma)
{
  float blob_w = 0, blob_xw = 0, blob_xw_sq = 0;
  float blob_w_max = 0, blob_xw_max = 0, blob_sigma_max = 0;
  for (int x = 0; x < n; x++)
  {
    int16_t e = line[x];
    if (et == SP_LIGHT_TO_DARK) e = -e;
    else if (et == SP_BOTH && e < 0) e = -e;
    e -= noise; if (e < 0) e = 0;
    if (blob_w == 0)
    {
      if (e <= 0) continue;       // wait for a blob to start
    }
    else if (e == 0)              // blob ended
    {
      if (blob_w_max < blob_w)
      {
        blob_w_max = blob_w; blob_xw_max = blob_xw;
        float cx = blob_xw / blob_w;
        float var = (blob_xw_sq / blob_w) - cx * cx;
        blob_sigma_max = (var > 0) ? sqrtf(var) : 0;
      }
      blob_w = blob_xw = blob_xw_sq = 0;
      continue;
    }
    blob_w += e; blob_xw += x * e; blob_xw_sq += (float)x * x * e;
  }
  if (blob_w > 0 && blob_w_max < blob_w) // trailing blob
  {
    blob_w_max = blob_w; blob_xw_max = blob_xw;
    float cx = blob_xw / blob_w; float var = (blob_xw_sq / blob_w) - cx * cx;
    blob_sigma_max = (var > 0) ? sqrtf(var) : 0;
  }
  w = blob_w_max; sigma = blob_sigma_max;
  return (blob_w_max > 0) ? (blob_xw_max / blob_w_max) : NAN;
}

bool search_point_cv(acvImage *gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     int blurSize, float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outW)
{
  if (!gray) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.X != s.X || s.Y != s.Y) return false;
  acv_XY perp = { -s.Y, s.X };

  int W = (int)lroundf(margin);      if (W < 3) return false; // search-dir span (px), ONE-SIDED [pt, pt+margin]
  int H = (int)lroundf(width);       if (H < 1) H = 1;        // band across edge
  if (H < 3) H = 3;                                            // need >=3 rows for a stable per-row edge
  float cy = (H - 1) * 0.5f;

  // sample the rectified region ONE-SIDED from pt: col x -> pt + x*s (x in [0,margin]);
  // row y -> +(y-cy)*perp. xPosMin is then the FIRST hit going from pt along searchDir.
  cv::Mat g(H, W, CV_8U);
  for (int y = 0; y < H; y++)
  {
    unsigned char *d = g.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++)
    {
      acv_XY q = acvVecAdd(pt, acvVecAdd(acvVecMult(s, (float)x), acvVecMult(perp, y - cy)));
      float v = acvUnsignedMap1Sampling(gray, q, 0);
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      d[x] = (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
    }
  }

  if (blurSize > 1) cv::blur(g, g, cv::Size(1, blurSize)); // blur ALONG the edge only
  cv::Mat sob; cv::Sobel(g, sob, CV_16S, 1, 0);            // gradient along search dir

  std::vector<cv::Point3f> eps;
  for (int y = 0; y < H; y++)
  {
    float w, sg;
    float loc = rowEdgeCenter(sob.ptr<int16_t>(y), W, (int16_t)edgeSuppress, polarity, w, sg);
    if (w > 0 && loc >= 2 && loc < W - 2) eps.push_back(cv::Point3f(loc, (float)y, w / (sg + 1)));
  }
  if (eps.empty()) return false;

  float xmin = 1e9f;
  for (auto &e : eps) if (e.x < xmin) xmin = e.x;     // first hit (closest along search)
  if (considerRange <= 0) considerRange = 1;
  if (alphaKeep > considerRange) alphaKeep = considerRange;

  double Ws = 0, Xs = 0, Ys = 0;
  for (auto &e : eps)
  {
    float dist = e.x - xmin;
    if (dist > considerRange) continue;
    float a = 1.0f - (dist - alphaKeep) / (considerRange - alphaKeep);
    if (a > 1) a = 1; if (a < 0) a = 0;
    float ww = e.z * a;
    Ws += ww; Xs += (double)e.x * ww; Ys += (double)e.y * ww;
  }
  if (Ws <= 0) return false;
  float ex = (float)(Xs / Ws), ey = (float)(Ys / Ws);
  if (outPt) *outPt = acvVecAdd(pt, acvVecAdd(acvVecMult(s, ex), acvVecMult(perp, ey - cy)));
  if (outW) *outW = (float)Ws;
  return true;
}

#endif // FEATURE_OPENCV
