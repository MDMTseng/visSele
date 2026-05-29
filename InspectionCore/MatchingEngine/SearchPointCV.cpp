#include "SearchPointCV.h"
#ifdef FEATURE_OPENCV
#include <opencv2/opencv.hpp>
#include "acvImage_SpDomainTool.hpp" // acvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <vector>
#include <cmath>

// One edge candidate in the rectified frame: sub-pixel (x=col, y=row) + gradient peak.
struct SPEdgePt { float x, y, peak; };

// Per-COLUMN local maxima of the polarity-signed Y-gradient (SobelY). The arc/edge of
// interest runs ACROSS the columns (a cap/dome), so its gradient is along the rows; a
// per-row X-scan misses the apex (boundary parallel to columns there). For column `x`
// we walk the rows, sign the gradient by polarity, subtract the noise floor, and emit
// EVERY local maximum (a row whose signed gradient exceeds both row-neighbors), refined
// to sub-pixel via a 3-point parabola. The caller then takes the top (min-Y) candidate.
static void colLocalMaxima(const cv::Mat &sob, int x, int H, int16_t noise,
                           SPEdgeType et, const std::vector<char> &rowValid,
                           std::vector<SPEdgePt> &out)
{
  auto sgn = [&](int16_t v) -> float {
    float e = (float)v;
    if (et == SP_LIGHT_TO_DARK) e = -e;
    else if (et == SP_BOTH) e = fabsf(e);
    e -= noise; return e < 0 ? 0.f : e;
  };
  for (int y = 1; y < H - 1; y++)
  {
    if (!rowValid[y] || !rowValid[y - 1] || !rowValid[y + 1]) continue;
    float em1 = sgn(sob.ptr<int16_t>(y - 1)[x]);
    float e0  = sgn(sob.ptr<int16_t>(y)[x]);
    float ep1 = sgn(sob.ptr<int16_t>(y + 1)[x]);
    if (e0 <= 0 || e0 < em1 || e0 < ep1) continue;       // strict local max along the column
    float denom = em1 - 2.f * e0 + ep1;
    float sub = (denom != 0.f) ? 0.5f * (em1 - ep1) / denom : 0.f;
    if (sub > 1) sub = 1; if (sub < -1) sub = -1;         // parabolic sub-pixel row
    out.push_back({(float)x, y + sub, e0});
  }
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
                     acv_XY *outPt, float *outW, int spId)
{
  if (!gray) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.X != s.X || s.Y != s.Y) return false;
  acv_XY perp = { -s.Y, s.X };

  // Legacy band axes (getContourPointsWithInLineContour + acvRotation, verified):
  //   |proj onto SEARCH dir|  < width/2   -> width  = scan DEPTH along searchDir
  //   |proj onto PERP|        < margin    -> margin = lateral HALF-extent
  // So the rectified region is `width` deep along search (centered +/-width/2) and
  // `2*margin` wide across (centered +/-margin). Columns = search dir, rows = perp.
  int W = (int)lroundf(width);            if (W < 3) return false; // scan-depth span (px)
  int H = (int)lroundf(2.0f * margin);    if (H < 3) H = 3;        // lateral band, >=3 rows for stable per-row edge
  float cy = (H - 1) * 0.5f;              // row -> perp coord (y-cy)
  float cx = (W - 1) * 0.5f;              // col -> search-dir coord (x-cx), pt at center

  // sample the rectified region CENTERED on pt: col x -> pt + (x-cx)*s (search dir),
  // row y -> pt + (y-cy)*perp (lateral). xPosMin (smallest col) = the FIRST edge hit
  // scanning from the near end along searchDir. Track per-row validity: a row that
  // samples OFF-IMAGE must be discarded -- off-image returns 0, which would otherwise
  // create a spurious strong edge at the image border.
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
      acv_XY q = acvVecAdd(pt, acvVecAdd(acvVecMult(s, x - cx), acvVecMult(perp, y - cy)));
      if (q.X < 1 || q.Y < 1 || q.X >= gW - 1 || q.Y >= gH - 1) { rowValid[y] = 0; d[x] = 0; m[x] = 0; continue; }
      float v = acvUnsignedMap1Sampling(gray, q, 0);
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      d[x] = (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
      if (useMask) m[x] = isObjectPx(labelImg, (int)(q.X + 0.5f), (int)(q.Y + 0.5f)) ? 255 : 0;
    }
  }

  if (blurSize > 1) cv::blur(g, g, cv::Size(blurSize, 1)); // blur ALONG the cap edge (cols) only
  cv::Mat sob; cv::Sobel(g, sob, CV_16S, 0, 1);            // gradient along perp (Y/rows): detects cap edge incl. apex

  // MASK step: keep sobel only on the object SILHOUETTE BOUNDARY RING, not the whole
  // object interior. Legacy operates on the binary object contour (silhouette) only;
  // a plain "inside object" mask would also keep INTERIOR gradients (texture, holes),
  // and the lateral-extreme selection below would then lock onto a spurious interior
  // edge instead of the silhouette (this is exactly why sp5/sp20 were far off). The
  // ring = dilate(mask) AND NOT erode(mask) isolates the background<->object transition
  // band (+/- maskDilate px), so per-row local-max = the silhouette edge -- matching
  // the legacy contour while staying grayscale/sub-pixel and tunable for soft edges.
  if (useMask && maskDilate > 0)
  {
    int k = 2 * maskDilate + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
    cv::Mat md, me;
    cv::dilate(mask, md, se);
    cv::erode(mask, me, se);
    for (int y = 0; y < H; y++)
    {
      unsigned char *pd = md.ptr<unsigned char>(y), *pe = me.ptr<unsigned char>(y);
      unsigned char *m = mask.ptr<unsigned char>(y);
      for (int x = 0; x < W; x++) m[x] = (pd[x] && !pe[x]) ? 255 : 0; // boundary ring
    }
  }
  if (useMask)
  {
    for (int y = 0; y < H; y++)
    {
      int16_t *sp = sob.ptr<int16_t>(y);
      unsigned char *m = mask.ptr<unsigned char>(y);
      for (int x = 0; x < W; x++) if (!m[x]) sp[x] = 0;
    }
  }

  // First pass: per-COLUMN local maxima of the Y-gradient + each peak. Collect candidates.
  std::vector<SPEdgePt> cand;
  float maxPeak = 0;
  for (int x = 0; x < W; x++)
    colLocalMaxima(sob, x, H, (int16_t)edgeSuppress, polarity, rowValid, cand);
  for (auto &c : cand) if (c.peak > maxPeak) maxPeak = c.peak;

  // STRENGTH GATE: keep only edges whose peak gradient is a strong fraction of the
  // region's strongest edge. The true silhouette gradient (bright<->dark) dwarfs the
  // backlight/background noise, so this isolates the real edge without an object mask.
  const float peakFrac = 0.40f;
  float peakThresh = maxPeak * peakFrac;
  std::vector<cv::Point3f> eps;
  for (auto &c : cand) if (c.peak >= peakThresh) eps.push_back(cv::Point3f(c.x, c.y, c.peak));
  if (eps.empty()) return false;

  // TOP selection: of all per-column edge maxima, take the one nearest the search
  // origin along the perpendicular (min row y = "top" of the cap). `perp = acvVecNormal(s)`
  // matches the legacy searchVec and flips with search_far, so min-y == legacy's
  // most-negative-perp extreme. Average the edges within `considerRange` rows of the top
  // (legacy reng), weighted by edge strength, for a stable sub-pixel apex point.
  float yMin = 1e9f;
  for (auto &e : eps) if (e.y < yMin) yMin = e.y;     // top along perpendicular
  if (getenv("SPCV_DUMP")) {
    float ya=1e9,yb=-1e9,xa=1e9,xb=-1e9; for(auto&e:eps){if(e.y<ya)ya=e.y;if(e.y>yb)yb=e.y;if(e.x<xa)xa=e.x;if(e.x>xb)xb=e.x;}
    fprintf(stderr,"[SPCV] pt=(%.0f,%.0f) eps=%zu rowPerp[%.0f,%.0f](cy=%.0f) col[%.0f,%.0f](cx=%.0f) yMin=%.0f\n",pt.X,pt.Y,eps.size(),ya-cy,yb-cy,cy,xa-cx,xb-cx,cx,yMin-cy);
  }
  if (const char *en = getenv("SPCV_N")) considerRange = atof(en); // debug sweep of n
  if (considerRange <= 0) considerRange = 1;
  if (alphaKeep > considerRange) alphaKeep = considerRange;

  // Collect every edge point within n rows below the top (y in [yMin, yMin+n]) and
  // peak-weighted-average them for a stable apex (more points -> less jitter).
  double Ws = 0, Xs = 0, Ys = 0;
  int nUsed = 0;
  for (auto &e : eps)
  {
    float dist = e.y - yMin;                          // rows below the top
    if (dist > considerRange) continue;
    float a = 1.0f - (dist - alphaKeep) / (considerRange - alphaKeep);
    if (a > 1) a = 1; if (a < 0) a = 0;
    float ww = e.z * a;
    Ws += ww; Xs += (double)e.x * ww; Ys += (double)e.y * ww; nUsed++;
  }
  if (Ws <= 0) return false;
  float ex = (float)(Xs / Ws), ey = (float)(Ys / Ws);
  if (getenv("SPCV_DUMP")) fprintf(stderr, "[SPCV] n=%.1f nUsed=%d final=(%.2f,%.2f)\n", considerRange, nUsed, ex - cx, ey - cy);

  if (getenv("SPCV_DUMP")) // debug: save rectified gray | mask | edge marker
  {
    cv::Mat vis; std::vector<cv::Mat> ch = {g, g, g}; cv::merge(ch, vis);
    if (useMask) for (int y=0;y<H;y++){ unsigned char*m=mask.ptr<unsigned char>(y); unsigned char*v=vis.ptr<unsigned char>(y);
        for(int x=0;x<W;x++) if(!m[x]) v[x*3+2]=(unsigned char)std::min(255,v[x*3+2]+90); } // background tinted red
    // per-row detected edges: clear green markers (circle so they're visible at any scale)
    for (auto &e: eps){ int yy=(int)e.y, xx=(int)e.x; if(yy>=0&&yy<H&&xx>=0&&xx<W) cv::circle(vis,cv::Point(xx,yy),2,cv::Scalar(0,255,0),-1); }
    { int yy=(int)ey,xx=(int)ex; if(yy>=0&&yy<H&&xx>=0&&xx<W){ cv::circle(vis,cv::Point(xx,yy),5,cv::Scalar(255,0,0),2); cv::drawMarker(vis,cv::Point(xx,yy),cv::Scalar(255,0,0),cv::MARKER_CROSS,11,1);} } // final blue
    int sc = (std::max(W, H) < 400) ? 3 : 1;  // uniform upscale for small remaps (keep aspect ratio)
    cv::Mat visBig; cv::resize(vis, visBig, cv::Size(), sc, sc, cv::INTER_NEAREST);
    char fn[256]; snprintf(fn,sizeof(fn),"/tmp/spcv_sp%d_pt%d_%d_%dx%d.png",spId,(int)pt.X,(int)pt.Y,W,H); cv::imwrite(fn,visBig);
    // also save the raw rectified gray + the (dilated) mask for clean inspection
    char fn2[256]; snprintf(fn2,sizeof(fn2),"/tmp/spcvraw_sp%d_pt%d_%d.png",spId,(int)pt.X,(int)pt.Y); cv::imwrite(fn2,g);
    char fn3[256]; snprintf(fn3,sizeof(fn3),"/tmp/spcvmask_sp%d_pt%d_%d.png",spId,(int)pt.X,(int)pt.Y); cv::imwrite(fn3,mask);
    // signed sobel (post-mask) mapped to 8U: 128 = zero gradient, brighter = +grad, darker = -grad.
    cv::Mat sob8; sob.convertTo(sob8, CV_8U, 0.5, 128.0);
    char fn4[256]; snprintf(fn4,sizeof(fn4),"/tmp/spcvsobel_sp%d_pt%d_%d.png",spId,(int)pt.X,(int)pt.Y); cv::imwrite(fn4,sob8);
    // image-space dump for full-image overlay: region corners, edges, final pt
    FILE *cf = fopen("/tmp/spcv_imgpts.csv", "a");
    if (cf) {
      acv_XY fp = acvVecAdd(pt, acvVecAdd(acvVecMult(s, ex-cx), acvVecMult(perp, ey-cy)));
      acv_XY c00 = acvVecAdd(pt, acvVecAdd(acvVecMult(s,-cx), acvVecMult(perp,-cy)));
      acv_XY c11 = acvVecAdd(pt, acvVecAdd(acvVecMult(s,(W-1)-cx), acvVecMult(perp,(H-1)-cy)));
      fprintf(cf, "FINAL,%.0f,%.0f,%.2f,%.2f\n", pt.X, pt.Y, fp.X, fp.Y);
      fprintf(cf, "BOX,%.0f,%.0f,%.2f,%.2f,%.2f,%.2f\n", pt.X, pt.Y, c00.X, c00.Y, c11.X, c11.Y);
      for (auto &e: eps){ acv_XY ep = acvVecAdd(pt, acvVecAdd(acvVecMult(s, e.x-cx), acvVecMult(perp, e.y-cy))); fprintf(cf, "EDGE,%.0f,%.0f,%.2f,%.2f\n", pt.X, pt.Y, ep.X, ep.Y); }
      fclose(cf);
    }
  }
  if (outPt) *outPt = acvVecAdd(pt, acvVecAdd(acvVecMult(s, ex - cx), acvVecMult(perp, ey - cy)));
  if (outW) *outW = (float)Ws;

  return true;
}

#endif // FEATURE_OPENCV
