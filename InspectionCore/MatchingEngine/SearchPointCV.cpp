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

// Is this labeled-image pixel part of an OBJECT (vs white background)?
// The labeling marks: background = white (R=G=B=255); object interior = small
// label value (R channel 0); object contour = (B,G,R)=(1,128,1). A THIN wire is
// almost all contour (no interior), so matching a specific label index fails and
// masks the wire itself. Instead treat "not white background" as object: the R
// channel (ch2) is 255 only on background, 0/1 on object interior/contour.
static inline bool isObjectPx(acvImage *L, int x, int y)
{
  if (x < 0 || y < 0 || x >= L->GetWidth() || y >= L->GetHeight()) return false;
  int ox = L->GetROIOffsetX(), oy = L->GetROIOffsetY();
  unsigned char *p = L->CVector[oy + y] + (ox + x) * 3;
  return !(p[0] == 255 && p[1] == 255 && p[2] == 255); // non-white => object
}
static inline int labelAt(acvImage *L, int x, int y)
{
  if (x < 0 || y < 0 || x >= L->GetWidth() || y >= L->GetHeight()) return -1;
  int ox = L->GetROIOffsetX(), oy = L->GetROIOffsetY();
  unsigned char *p = L->CVector[oy + y] + (ox + x) * 3;
  return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16);
}

bool search_point_cv(acvImage *gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     int blurSize, float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     acvImage *labelImg, int objLabel, int maskDilate,
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
  // Track per-row validity: a row that samples OFF-IMAGE must be discarded -- off-image
  // returns 0, which would otherwise create a spurious strong edge at the image border
  // (the legacy contour search never hits background since no contour exists there).
  int gW = gray->GetWidth(), gH = gray->GetHeight();
  cv::Mat g(H, W, CV_8U);
  cv::Mat mask(H, W, CV_8U, cv::Scalar(255)); // 255 = allowed; default all-allowed
  bool useMask = (labelImg != nullptr && objLabel >= 0);
  if (getenv("SPCV_DUMP") && useMask) {
    int c0=labelAt(labelImg,(int)pt.X,(int)pt.Y);
    int c1=labelAt(labelImg,5,5); // a corner (likely background)
    fprintf(stderr,"[SPCV] objLabel=%d  labelAt(pt)=%d  labelAt(corner)=%d  W=%d H=%d\n",objLabel,c0,c1,W,H);
  }
  std::vector<char> rowValid(H, 1);
  for (int y = 0; y < H; y++)
  {
    unsigned char *d = g.ptr<unsigned char>(y);
    unsigned char *m = mask.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++)
    {
      acv_XY q = acvVecAdd(pt, acvVecAdd(acvVecMult(s, (float)x), acvVecMult(perp, y - cy)));
      if (q.X < 1 || q.Y < 1 || q.X >= gW - 1 || q.Y >= gH - 1) { rowValid[y] = 0; d[x] = 0; m[x] = 0; continue; }
      float v = acvUnsignedMap1Sampling(gray, q, 0);
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      d[x] = (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
      if (useMask) m[x] = isObjectPx(labelImg, (int)(q.X + 0.5f), (int)(q.Y + 0.5f)) ? 255 : 0;
    }
  }

  if (blurSize > 1) cv::blur(g, g, cv::Size(1, blurSize)); // blur ALONG the edge only
  cv::Mat sob; cv::Sobel(g, sob, CV_16S, 1, 0);            // gradient along search dir

  // MASK step: dilate the object label region a bit (the edge sits on the boundary,
  // so grow it to keep the edge), then zero the sobel outside it -> the local-max
  // search below cannot pick background specks/dust.
  if (useMask)
  {
    if (maskDilate > 0)
    {
      int k = 2 * maskDilate + 1;
      cv::Mat se = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
      cv::dilate(mask, mask, se);
    }
    for (int y = 0; y < H; y++)
    {
      int16_t *sp = sob.ptr<int16_t>(y);
      unsigned char *m = mask.ptr<unsigned char>(y);
      for (int x = 0; x < W; x++) if (!m[x]) sp[x] = 0;
    }
  }

  std::vector<cv::Point3f> eps;
  for (int y = 0; y < H; y++)
  {
    if (!rowValid[y]) continue;                            // skip off-image rows
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

  if (getenv("SPCV_DUMP")) // debug: save rectified gray | mask | edge marker
  {
    cv::Mat vis; std::vector<cv::Mat> ch = {g, g, g}; cv::merge(ch, vis);
    if (useMask) for (int y=0;y<H;y++){ unsigned char*m=mask.ptr<unsigned char>(y); unsigned char*v=vis.ptr<unsigned char>(y);
        for(int x=0;x<W;x++) if(!m[x]) v[x*3+2]=255; } // background tinted red
    for (auto &e: eps){ int yy=(int)e.y, xx=(int)e.x; if(yy>=0&&yy<H&&xx>=0&&xx<W){ unsigned char*v=vis.ptr<unsigned char>(yy); v[xx*3]=0;v[xx*3+1]=255;v[xx*3+2]=0; } } // per-row edges green
    { int yy=(int)ey,xx=(int)ex; if(yy>=0&&yy<H&&xx>=0&&xx<W){ cv::circle(vis,cv::Point(xx,yy),3,cv::Scalar(255,0,0),-1); } } // final blue
    char fn[256]; snprintf(fn,sizeof(fn),"/tmp/spcv_pt%d_%d_%dx%d.png",(int)pt.X,(int)pt.Y,W,H); cv::imwrite(fn,vis);
    // also save the raw rectified gray + the (dilated) mask for clean inspection
    char fn2[256]; snprintf(fn2,sizeof(fn2),"/tmp/spcvraw_pt%d_%d.png",(int)pt.X,(int)pt.Y); cv::imwrite(fn2,g);
    char fn3[256]; snprintf(fn3,sizeof(fn3),"/tmp/spcvmask_pt%d_%d.png",(int)pt.X,(int)pt.Y); cv::imwrite(fn3,mask);
  }
  if (outPt) *outPt = acvVecAdd(pt, acvVecAdd(acvVecMult(s, ex), acvVecMult(perp, ey - cy)));
  if (outW) *outW = (float)Ws;
  return true;
}

#endif // FEATURE_OPENCV
