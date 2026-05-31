#include "SearchPointCV.h"
#include <opencv2/opencv.hpp>
#include "CvBridge.h"                // cvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <vector>
#include <cmath>

// One edge candidate: centered region coords (searchCoord along s, perpCoord along
// acvVecNormal(s)) + the polarity-signed gradient peak that produced it.
struct SPEdgePt { float searchCoord, perpCoord, peak; };

// Is this labeled-image pixel part of an OBJECT (vs white background)?
// The labeling marks: background = white (R=G=B=255); object interior = small
// label value (R channel 0); object contour = (B,G,R)=(1,128,1). A THIN wire is
// almost all contour (no interior), so matching a specific label index fails and
// masks the wire itself. Instead treat "not white background" as object: the R
// channel (ch2) is 255 only on background, 0/1 on object interior/contour.
static inline bool isObjectPx(const cv::Mat &L, int x, int y)
{
  if (L.empty() || x < 0 || y < 0 || x >= L.cols || y >= L.rows) return false;
  const uint8_t *p = L.ptr<uint8_t>(y) + x * 3;
  return !(p[0] == 255 && p[1] == 255 && p[2] == 255); // non-white => object
}
static inline int labelAt(const cv::Mat &L, int x, int y)
{
  if (L.empty() || x < 0 || y < 0 || x >= L.cols || y >= L.rows) return -1;
  const uint8_t *p = L.ptr<uint8_t>(y) + x * 3;
  return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16);
}

bool search_point_cv(const cv::Mat &gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     int blurSize, float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     const cv::Mat &labelImg, int objLabel, int maskDilate,
                     acv_XY *outPt, float *outW, int spId)
{
  if (gray.empty()) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.x != s.x || s.y != s.y) return false;
  acv_XY perp = { -s.y, s.x };

  // Legacy band axes (verified): |proj onto SEARCH dir| < width/2, |proj onto PERP| < margin.
  // The rectified buffer is rotated 90deg CCW vs the old layout (search was cols, perp was
  // rows) for cache-friendliness + a fused gradient/local-max pass: now ROWS = search
  // direction (depth `width`), COLS = perp (lateral `2*margin`). The perp axis is therefore
  // CONTIGUOUS, so per search row we walk the perp line ONCE -- computing the polarity-signed
  // perp-gradient and detecting its local maxima inline (no separate Sobel Mat, no strided
  // column access).  row i -> searchCoord = cs - i  (i=0 top = +search end; CCW)
  //                   col j -> perpCoord   = j  - cp
  //                   q = pt + s*searchCoord + perp*perpCoord
  int nS = (int)lroundf(width);          if (nS < 3) return false; // search-depth (rows)
  int nP = (int)lroundf(2.0f * margin);  if (nP < 3) nP = 3;       // perp/lateral (cols)
  float cs = (nS - 1) * 0.5f;            // row -> searchCoord (cs - i)
  float cp = (nP - 1) * 0.5f;            // col -> perpCoord   (j - cp)

  const bool dbg = (getenv("SPCV_DUMP") != nullptr);
  const bool useMask = (!labelImg.empty() && objLabel >= 0);
  int gW = gray.cols, gH = gray.rows;
  cv::Mat g(nS, nP, CV_8U);                     // rows = search dir, cols = perp
  cv::Mat valid(nS, nP, CV_8U, cv::Scalar(1));  // 1 = sampled in-image
  cv::Mat mask;                                 // object-allow mask; only built when labelImg given
  if (useMask) mask = cv::Mat(nS, nP, CV_8U, cv::Scalar(255));
  for (int i = 0; i < nS; i++)
  {
    float searchCoord = cs - i;
    unsigned char *d = g.ptr<unsigned char>(i), *vv = valid.ptr<unsigned char>(i);
    unsigned char *m = useMask ? mask.ptr<unsigned char>(i) : nullptr;
    for (int j = 0; j < nP; j++)
    {
      acv_XY q = acvVecAdd(pt, acvVecAdd(acvVecMult(s, searchCoord), acvVecMult(perp, j - cp)));
      if (q.x < 1 || q.y < 1 || q.x >= gW - 1 || q.y >= gH - 1) { d[j] = 0; vv[j] = 0; if (m) m[j] = 0; continue; }
      float v = cvUnsignedMap1Sampling(gray, q.x, q.y, 0);
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      d[j] = (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
      if (m) m[j] = isObjectPx(labelImg, (int)(q.x + 0.5f), (int)(q.y + 0.5f)) ? 255 : 0;
    }
  }

  // Optional object-boundary RING mask (silhouette only): ring = dilate AND NOT erode,
  // so interior gradients are excluded. Disabled unless labelImg is provided.
  if (useMask && maskDilate > 0)
  {
    int k = 2 * maskDilate + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k)), md, me;
    cv::dilate(mask, md, se); cv::erode(mask, me, se);
    for (int i = 0; i < nS; i++) {
      unsigned char *pd = md.ptr<unsigned char>(i), *pe = me.ptr<unsigned char>(i), *m = mask.ptr<unsigned char>(i);
      for (int j = 0; j < nP; j++) m[j] = (pd[j] && !pe[j]) ? 255 : 0;
    }
  }

  // FUSED PASS: per interior search row, walk the contiguous perp line once. Gradient along
  // perp = 3x3 Sobel (rows i-1,i,i+1 give the [1,2,1] smoothing across search; cols are the
  // +/-1 perp difference) -- this also subsumes the old "blur along the edge". Sign by
  // polarity, subtract the noise floor, then emit every strict local maximum (sub-pixel via
  // a 3-point parabola). One row buffer, no strided access.
  auto sgn = [&](int gx) -> float {
    float e = (float)gx;
    if (polarity == SP_LIGHT_TO_DARK) e = -e;
    else if (polarity == SP_BOTH) e = fabsf(e);
    e -= edgeSuppress; return e < 0 ? 0.f : e;
  };
  std::vector<SPEdgePt> cand;
  cv::Mat sobViz; if (dbg) sobViz = cv::Mat::zeros(nS, nP, CV_16S);
  std::vector<float> eline(nP);
  float maxPeak = 0;
  for (int i = 1; i < nS - 1; i++)
  {
    const unsigned char *r0 = g.ptr<unsigned char>(i-1), *r1 = g.ptr<unsigned char>(i), *r2 = g.ptr<unsigned char>(i+1);
    const unsigned char *vr = valid.ptr<unsigned char>(i);
    const unsigned char *m = useMask ? mask.ptr<unsigned char>(i) : nullptr;
    int16_t *sv = dbg ? sobViz.ptr<int16_t>(i) : nullptr;
    eline[0] = eline[nP-1] = 0.f;
    for (int j = 1; j < nP - 1; j++)
    {
      int gx = (r0[j+1] + 2*r1[j+1] + r2[j+1]) - (r0[j-1] + 2*r1[j-1] + r2[j-1]); // perp gradient
      if (sv) sv[j] = (int16_t)gx;
      float e = sgn(gx);
      if (!vr[j-1] || !vr[j] || !vr[j+1]) e = 0;     // off-image -> drop spurious border edge
      if (useMask && !m[j]) e = 0;
      eline[j] = e;
    }
    for (int j = 1; j < nP - 1; j++)               // local maxima along the contiguous perp line
    {
      float e = eline[j];
      if (e <= 0 || e < eline[j-1] || e < eline[j+1]) continue;
      float denom = eline[j-1] - 2.f*e + eline[j+1];
      float sub = (denom != 0.f) ? 0.5f * (eline[j-1] - eline[j+1]) / denom : 0.f;
      if (sub > 1) sub = 1; if (sub < -1) sub = -1;
      cand.push_back({cs - i, (j + sub) - cp, e});  // {searchCoord, perpCoord, peak}
      if (e > maxPeak) maxPeak = e;
    }
  }
  if (cand.empty()) return false;

  // STRENGTH GATE: keep only edges whose peak gradient is a strong fraction of the strongest.
  const float peakFrac = 0.40f;
  float peakThresh = maxPeak * peakFrac;
  std::vector<SPEdgePt> eps;
  for (auto &c : cand) if (c.peak >= peakThresh) eps.push_back(c);
  if (eps.empty()) return false;

  // TOP selection: of all per-row edge maxima, take the one nearest the search origin along
  // the perpendicular (min perpCoord = "top" of the cap). perp = acvVecNormal(s) matches the
  // legacy searchVec and flips with search_far, so min-perp == legacy's most-negative-perp
  // extreme. Average the edges within `considerRange` of the top (legacy reng), peak-weighted.
  float pMin = 1e9f;
  for (auto &e : eps) if (e.perpCoord < pMin) pMin = e.perpCoord;   // top along perpendicular
  if (dbg) {
    float pa=1e9,pb=-1e9,sa=1e9,sb=-1e9; for(auto&e:eps){pa=std::min(pa,e.perpCoord);pb=std::max(pb,e.perpCoord);sa=std::min(sa,e.searchCoord);sb=std::max(sb,e.searchCoord);}
    fprintf(stderr,"[SPCV] pt=(%.0f,%.0f) eps=%zu perp[%.0f,%.0f] search[%.0f,%.0f] perpTop=%.0f\n",pt.x,pt.y,eps.size(),pa,pb,sa,sb,pMin);
    if (const char *en = getenv("SPCV_N")) considerRange = atof(en); // debug sweep of n
  }
  if (considerRange <= 0) considerRange = 1;
  if (alphaKeep > considerRange) alphaKeep = considerRange;

  // Collect every edge within n of the top (perpCoord in [pMin, pMin+n]) and peak-weighted-
  // average both coords for a stable apex.
  double Ws = 0, Ss = 0, Ps = 0;
  int nUsed = 0;
  for (auto &e : eps)
  {
    float dist = e.perpCoord - pMin;                  // perp distance below the top
    if (dist > considerRange) continue;
    float a = 1.0f - (dist - alphaKeep) / (considerRange - alphaKeep);
    if (a > 1) a = 1; if (a < 0) a = 0;
    float ww = e.peak * a;
    Ws += ww; Ss += (double)e.searchCoord * ww; Ps += (double)e.perpCoord * ww; nUsed++;
  }
  if (Ws <= 0) return false;
  float eS = (float)(Ss / Ws), eP = (float)(Ps / Ws);  // centered region coords
  if (dbg) fprintf(stderr, "[SPCV] n=%.1f nUsed=%d final(search,perp)=(%.2f,%.2f)\n", considerRange, nUsed, eS, eP);

  if (dbg) // debug: save rectified gray | mask | edge marker
  {
    // buffer pos of a centered coord: col = perpCoord + cp, row = cs - searchCoord
    auto bx = [&](float perpCoord){ return (int)lroundf(perpCoord + cp); };
    auto by = [&](float searchCoord){ return (int)lroundf(cs - searchCoord); };
    cv::Mat vis; std::vector<cv::Mat> ch = {g, g, g}; cv::merge(ch, vis);
    if (useMask) for (int i=0;i<nS;i++){ unsigned char*m=mask.ptr<unsigned char>(i); unsigned char*v=vis.ptr<unsigned char>(i);
        for(int j=0;j<nP;j++) if(!m[j]) v[j*3+2]=(unsigned char)std::min(255,v[j*3+2]+90); } // background tinted red
    for (auto &e: eps){ int xx=bx(e.perpCoord), yy=by(e.searchCoord); if(yy>=0&&yy<nS&&xx>=0&&xx<nP) cv::circle(vis,cv::Point(xx,yy),2,cv::Scalar(0,255,0),-1); }
    { int xx=bx(eP), yy=by(eS); if(yy>=0&&yy<nS&&xx>=0&&xx<nP){ cv::circle(vis,cv::Point(xx,yy),5,cv::Scalar(255,0,0),2); cv::drawMarker(vis,cv::Point(xx,yy),cv::Scalar(255,0,0),cv::MARKER_CROSS,11,1);} } // final blue
    int sc = (std::max(nS, nP) < 400) ? 3 : 1;  // uniform upscale for small remaps (keep aspect ratio)
    cv::Mat visBig; cv::resize(vis, visBig, cv::Size(), sc, sc, cv::INTER_NEAREST);
    char fn[256]; snprintf(fn,sizeof(fn),"/tmp/spcv_sp%d_pt%d_%d_%dx%d.png",spId,(int)pt.x,(int)pt.y,nP,nS); cv::imwrite(fn,visBig);
    char fn2[256]; snprintf(fn2,sizeof(fn2),"/tmp/spcvraw_sp%d_pt%d_%d.png",spId,(int)pt.x,(int)pt.y); cv::imwrite(fn2,g);
    if (useMask) { char fn3[256]; snprintf(fn3,sizeof(fn3),"/tmp/spcvmask_sp%d_pt%d_%d.png",spId,(int)pt.x,(int)pt.y); cv::imwrite(fn3,mask); }
    // signed gradient mapped to 8U: 128 = zero gradient, brighter = +grad, darker = -grad.
    cv::Mat sob8; sobViz.convertTo(sob8, CV_8U, 0.5, 128.0);
    char fn4[256]; snprintf(fn4,sizeof(fn4),"/tmp/spcvsobel_sp%d_pt%d_%d.png",spId,(int)pt.x,(int)pt.y); cv::imwrite(fn4,sob8);
    // image-space dump for full-image overlay: region corners, edges, final pt
    FILE *cf = fopen("/tmp/spcv_imgpts.csv", "a");
    if (cf) {
      acv_XY fp  = acvVecAdd(pt, acvVecAdd(acvVecMult(s, eS), acvVecMult(perp, eP)));
      acv_XY c00 = acvVecAdd(pt, acvVecAdd(acvVecMult(s, cs), acvVecMult(perp, -cp)));
      acv_XY c11 = acvVecAdd(pt, acvVecAdd(acvVecMult(s, -cs), acvVecMult(perp, cp)));
      fprintf(cf, "FINAL,%.0f,%.0f,%.2f,%.2f\n", pt.x, pt.y, fp.x, fp.y);
      fprintf(cf, "BOX,%.0f,%.0f,%.2f,%.2f,%.2f,%.2f\n", pt.x, pt.y, c00.x, c00.y, c11.x, c11.y);
      for (auto &e: eps){ acv_XY ep = acvVecAdd(pt, acvVecAdd(acvVecMult(s, e.searchCoord), acvVecMult(perp, e.perpCoord))); fprintf(cf, "EDGE,%.0f,%.0f,%.2f,%.2f\n", pt.x, pt.y, ep.x, ep.y); }
      fclose(cf);
    }
  }
  if (outPt) *outPt = acvVecAdd(pt, acvVecAdd(acvVecMult(s, eS), acvVecMult(perp, eP)));
  if (outW) *outW = (float)Ws;

  return true;
}

