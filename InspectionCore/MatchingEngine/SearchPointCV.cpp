#include "SearchPointCV.h"
#include "MEPhase.h"
#include <opencv2/opencv.hpp>
#include "CvBridge.h"                // cvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <vector>
#include <cmath>

// One edge candidate: centered region coords (searchCoord along s, perpCoord along
// acvVecNormal(s)) + the polarity-signed gradient peak that produced it.
struct SPEdgePt { float searchCoord, perpCoord, peak; };

// The background mask that used to live here is GONE, and this is the note that
// stops it being reinvented.
//
// It masked the sobel response outside a dilated object silhouette so the scan
// could not lock onto a background speck. It was switched off on 2026-05-29 in a
// wip commit ("temporarily ... for edge-finding debugging"), the labeled image it
// read was deleted, and it then sat here for three months as a parameter the call
// site passed empty -- taking `maskDilate` with it, a knob honoured all the way
// down to nothing.
//
// A polygon successor was built on 2026-08-26 and rejected the same day, on a
// better argument than the feature: the caliper's own margin/width ALREADY
// define the band the scan walks, so an outer polygon is a second, coarser copy
// of the same constraint -- and the search point follows the part through the
// morph while a polygon is rigid, so on a deforming part the fence stops
// tracking and becomes a new source of false NA. If a scan is picking up the
// wrong edge, tighten the primitive's own region, or let the morph follow the
// part more closely. Do not put a mask back here.

bool search_point_cv(const cv::Mat &gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outW, int spId,
                     std::vector<CaliperHit> *outHits, bool *outClipped,
                     SearchPointPeaks *outPeaks, float relStrength,
                     int *outRelMoved)
{
  if (outClipped) *outClipped = false;
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
  // margin and width arrive from the def UNCLAMPED -- unlike the caliper fields,
  // which are capped at 512/64/256 during parse -- and they size the buffers
  // below directly. Reject in FLOAT, before the (int) conversion, because
  // casting an out-of-range float to int is UB and lroundf(1e30) is not
  // INT_MAX on every target (same note as Caliper.cpp). The comparison shape is
  // NaN-safe: !(x >= n) is true for NaN, whereas (x < n) is not.
  if (!(width >= 3.0f) || !(margin > 0.0f)) return false;
  int nS = (int)lroundf(width);          if (nS < 3) return false; // search-depth (rows)
  int nP = (int)lroundf(2.0f * margin);  if (nP < 3) nP = 3;       // perp/lateral (cols)
  // The failure Caliper.cpp guards with CELL_LIMIT, and the same realistic
  // trigger: not a hostile def, but a pixel figure typed into a field that
  // wants millimetres. Measured before this guard: margin=width=3e4 allocated
  // ~900M cells across g/valid and ran to completion, and margin=1e9 threw
  // cv::Exception out of the allocator. The live path catches that (the
  // acquisition callback drops the frame), but it cannot catch the one that
  // merely succeeds and eats gigabytes. A real search point is a few tens of px
  // each way -- thousands of cells, not millions.
  {
    const size_t cells = (size_t)nS * (size_t)nP;
    const size_t CELL_LIMIT = 8u * 1024u * 1024u;   // 8M cells, up to 3 CV_8U planes
    if (cells > CELL_LIMIT)
    {
      printf("search_point: refusing a %dx%d band (%zu cells) -- check that "
             "margin/width are px and sane (margin=%.1f width=%.1f)\n",
             nS, nP, cells, margin, width);
      return false;
    }
  }
  mephase::count("spcv_scan", 1);
  mephase::count("spcv_samp", (double)nS * (double)nP);
  // Where an early exit COULD have stopped, counted before writing one.
  //
  // This is a first-hit search that samples the entire band before looking at
  // any of it. Scanning the perp axis from the near end and stopping once the
  // answer is bracketed is the obvious optimisation -- but it needs the global
  // maximum gone, i.e. edge.rel_strength = 0, and it is a rewrite of a
  // numerically delicate loop. So the saving is measured first, with the answer
  // the scan actually produced, and the loop is left alone until the number
  // says it is worth the risk. See mephase::count in Caliper.cpp for the same
  // discipline applied to the line band.
  const int spcv_nS = nS, spcv_nP = nP;

  float cs = (nS - 1) * 0.5f;            // row -> searchCoord (cs - i)
  float cp = (nP - 1) * 0.5f;            // col -> perpCoord   (j - cp)

  const bool dbg = (getenv("SPCV_DUMP") != nullptr);
  int gW = gray.cols, gH = gray.rows;
  cv::Mat g(nS, nP, CV_8U);                     // rows = search dir, cols = perp
  cv::Mat valid(nS, nP, CV_8U, cv::Scalar(1));  // 1 = sampled in-image
  // GATHER vs SCAN, because they answer different questions.
  //
  // An early exit along the perp axis only saves the part of the work that is
  // proportional to how far it scans. If the cost is the GATHER -- a bilinear
  // remap plus a backlight-factor lookup per sample, on rotated coordinates
  // that touch the source image all over -- then stopping early saves it. If
  // the cost is the SCAN, a fused Sobel over a band small enough to sit in L1,
  // then stopping early saves almost nothing and the rewrite is not worth its
  // risk. Nobody knew which, so it is measured before it is optimised.
  { mephase::Timer _g("sp_gather");
  for (int i = 0; i < nS; i++)
  {
    float searchCoord = cs - i;
    unsigned char *d = g.ptr<unsigned char>(i), *vv = valid.ptr<unsigned char>(i);
    for (int j = 0; j < nP; j++)
    {
      acv_XY q = acvVecAdd(pt, acvVecAdd(acvVecMult(s, searchCoord), acvVecMult(perp, j - cp)));
      if (q.x < 1 || q.y < 1 || q.x >= gW - 1 || q.y >= gH - 1)
      {
        // Not just an unusable sample -- evidence that the WINDOW is not the
        // one the def specified. Reported up so the caller can refuse the
        // measurement rather than average what is left.
        d[j] = 0; vv[j] = 0;
        if (outClipped) *outClipped = true;
        continue;
      }
      float v = cvUnsignedMap1Sampling(gray, q.x, q.y, 0);
      // The backlight factor already costs nothing when there is no calibration
      // to apply: sampleBackLightFactor_ImgCoord returns 1 on a NaN exposure.
      // Measured on this bench, which has none -- gather 0.64 ms with it and
      // 0.63/0.64 without, i.e. inside the noise. On a station that IS
      // calibrated, factorSampling() runs per sample and its cost is unmeasured.
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      // !(v > 0) catches NaN as well as negatives: the backlight factor is
      // NaN outside the calibration grid, and casting a NaN float to
      // unsigned char is UB, not "some grey value".
      d[j] = !(v > 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
    }
  }
  }

  // FUSED PASS: per interior search row, walk the contiguous perp line once. Gradient along
  // perp = 3x3 Sobel (rows i-1,i,i+1 give the [1,2,1] smoothing across search; cols are the
  // +/-1 perp difference) -- this also subsumes the old "blur along the edge". Sign by
  // polarity, subtract the noise floor, then emit every strict local maximum (sub-pixel via
  // a 3-point parabola). One row buffer, no strided access.
  auto pol = [&](int gx) -> float {
    float e = (float)gx;
    if (polarity == SP_LIGHT_TO_DARK) e = -e;
    else if (polarity == SP_BOTH) e = fabsf(e);
    return e;
  };
  auto sgn = [&](int gx) -> float {
    float e = pol(gx) - edgeSuppress; return e < 0 ? 0.f : e;
  };
  std::vector<SPEdgePt> cand;
  // The SAME peaks, without min_strength taken out of them.
  //
  // sgn() subtracts the floor before the local-maximum search, so a candidate
  // weaker than the current setting does not merely fail a test -- it never
  // exists. Reporting `cand` as "the evidence" would therefore show only what
  // the floor already admits, and a panel for LOWERING a threshold that cannot
  // show anything below it is worse than none: it looks like proof that there
  // is nothing down there.
  //
  // So when someone is going to look, the peaks are found a second time on the
  // unsuppressed gradient. Strictly extra work, and only when the payload is
  // asked for; the measurement below still runs on `cand` and is untouched.
  std::vector<SPEdgePt> candRaw;
  cv::Mat sobViz; if (dbg) sobViz = cv::Mat::zeros(nS, nP, CV_16S);
  std::vector<float> eline(nP);
  std::vector<float> eraw(outPeaks ? nP : 0);
  float maxPeak = 0;
  mephase::Timer _sc("sp_scan");
  for (int i = 1; i < nS - 1; i++)
  {
    const unsigned char *r0 = g.ptr<unsigned char>(i-1), *r1 = g.ptr<unsigned char>(i), *r2 = g.ptr<unsigned char>(i+1);
    // The Sobel below reads rows i-1, i and i+1, so the off-image test has to
    // cover all three. Checking only row i left a one-row gap.
    //
    // Off-image samples are filled with 0 above, and a 0<->bright step is a
    // full-scale gradient, so any that reach this Sobel outrank a real edge on
    // a low-contrast feature. Whether they reach it depends on the angle: the
    // differences and the guard both run along j, so when searchDir is PARALLEL
    // to the frame edge being crossed a column is at constant distance from it
    // and goes out of frame whole-columns-at-a-time -- the axis the 1x3 guard
    // already covers. When searchDir is SLANTED the boundary cuts the band
    // diagonally, row i can be fully in-image while row i-1 is partly outside,
    // and those zeros get through.
    //
    // Measured on a UNIFORM 400x200 image with no feature anywhere: slanted
    // scans at 10/25/45 deg reported an edge at y = 198.02 / 198.13 / 197.78,
    // strength 200 -- fabricated, and sitting on the frame boundary. After this
    // fix they report nothing, and real measurements are bit-identical
    // (--insp leaf diff over test1: 959 leaves, 0 differ).
    // Regression: test_suite/test_searchpoint_frame_edge.cpp
    const unsigned char *v0 = valid.ptr<unsigned char>(i-1);
    const unsigned char *vr = valid.ptr<unsigned char>(i);
    const unsigned char *v2 = valid.ptr<unsigned char>(i+1);
    int16_t *sv = dbg ? sobViz.ptr<int16_t>(i) : nullptr;
    eline[0] = eline[nP-1] = 0.f;
    for (int j = 1; j < nP - 1; j++)
    {
      int gx = (r0[j+1] + 2*r1[j+1] + r2[j+1]) - (r0[j-1] + 2*r1[j-1] + r2[j-1]); // perp gradient
      if (sv) sv[j] = (int16_t)gx;
      const bool bad = (!vr[j-1] || !vr[j] || !vr[j+1] ||
                        !v0[j-1] || !v0[j+1] || !v2[j-1] || !v2[j+1]);
      float e = sgn(gx);
      if (bad) e = 0;  // off-image -> drop spurious border edge
      eline[j] = e;
      if (outPeaks) { float r = pol(gx); eraw[j] = (bad || r < 0) ? 0.f : r; }
    }
    if (outPeaks)
    {
      eraw[0] = eraw[nP-1] = 0.f;
      for (int j = 1; j < nP - 1; j++)
      {
        float e = eraw[j];
        if (e <= 0 || e < eraw[j-1] || e < eraw[j+1]) continue;
        float den = eraw[j-1] - 2.f*e + eraw[j+1];
        float sub = (den != 0.f) ? 0.5f * (eraw[j-1] - eraw[j+1]) / den : 0.f;
        if (sub > 1) sub = 1; if (sub < -1) sub = -1;
        candRaw.push_back({cs - i, (j + sub) - cp, e});
      }
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
  // Every candidate, before the selector's gate touches them. Emitted even
  // when the scan goes on to fail: "nothing here cleared the floor" and "there
  // is no edge here" are the same outcome and completely different pictures.
  //
  // perpCoord is signed and centred; the panel wants a distance along the
  // search, so it is shifted to start at 0 at the near end. Sign follows the
  // first-hit rule (min perpCoord), so smaller stays nearer.
  if (outPeaks)
  {
    outPeaks->span = (float)nP;
    outPeaks->pos.reserve(cand.size());
    outPeaks->str.reserve(cand.size());
    for (const SPEdgePt &c : candRaw)
    { outPeaks->pos.push_back(c.perpCoord + cp); outPeaks->str.push_back(c.peak); }
  }
  _sc.stop();
  if (cand.empty()) return false;

  // STRENGTH GATE, relative to the strongest peak in the window.
  //
  // A number rather than a constant since 2026-09-02, defaulting to the 0.40 it
  // was hard-coded to. 0 leaves min_strength as the only floor -- which is
  // where this should end up, once floors are set against the edge profile
  // instead of guessed. See featureDef_searchPoint::rel_strength.
  const float peakFrac = (relStrength > 0) ? relStrength : 0.0f;
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
  // How much of that answer came from the relative rule: candidates that
  // cleared min_strength (every entry of `cand` has, by construction) and sit
  // NEARER than the one chosen, but did not survive peakThresh. Zero means the
  // def's own floor would have produced the same point.
  if (outRelMoved)
  {
    int n = 0;
    for (auto &c : cand) if (c.perpCoord < pMin && c.peak < peakThresh) n++;
    *outRelMoved = n;
  }
  if (dbg) {
    float pa=1e9,pb=-1e9,sa=1e9,sb=-1e9; for(auto&e:eps){pa=std::min(pa,e.perpCoord);pb=std::max(pb,e.perpCoord);sa=std::min(sa,e.searchCoord);sb=std::max(sb,e.searchCoord);}
    fprintf(stderr,"[SPCV] pt=(%.0f,%.0f) eps=%zu perp[%.0f,%.0f] search[%.0f,%.0f] perpTop=%.0f\n",pt.x,pt.y,eps.size(),pa,pb,sa,sb,pMin);
    if (const char *en = getenv("SPCV_N")) considerRange = atof(en); // debug sweep of n
  }
  if (considerRange <= 0) considerRange = 1;
  // Strictly below considerRange: equal makes the (considerRange-alphaKeep)
  // denominator below 0, and 0/0 = NaN poisons every weight and the result.
  if (alphaKeep >= considerRange) alphaKeep = considerRange * 0.999f;

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
  if (!(Ws > 0)) return false;                         // also catches NaN
  float eS = (float)(Ss / Ws), eP = (float)(Ps / Ws);  // centered region coords
  if (!std::isfinite(eS) || !std::isfinite(eP)) return false;
  if (dbg) fprintf(stderr, "[SPCV] n=%.1f nUsed=%d final(search,perp)=(%.2f,%.2f)\n", considerRange, nUsed, eS, eP);

  if (dbg) // debug: save rectified gray | edge marker
  {
    // buffer pos of a centered coord: col = perpCoord + cp, row = cs - searchCoord
    auto bx = [&](float perpCoord){ return (int)lroundf(perpCoord + cp); };
    auto by = [&](float searchCoord){ return (int)lroundf(cs - searchCoord); };
    cv::Mat vis; std::vector<cv::Mat> ch = {g, g, g}; cv::merge(ch, vis);
    for (auto &e: eps){ int xx=bx(e.perpCoord), yy=by(e.searchCoord); if(yy>=0&&yy<nS&&xx>=0&&xx<nP) cv::circle(vis,cv::Point(xx,yy),2,cv::Scalar(0,255,0),-1); }
    { int xx=bx(eP), yy=by(eS); if(yy>=0&&yy<nS&&xx>=0&&xx<nP){ cv::circle(vis,cv::Point(xx,yy),5,cv::Scalar(255,0,0),2); cv::drawMarker(vis,cv::Point(xx,yy),cv::Scalar(255,0,0),cv::MARKER_CROSS,11,1);} } // final blue
    int sc = (std::max(nS, nP) < 400) ? 3 : 1;  // uniform upscale for small remaps (keep aspect ratio)
    cv::Mat visBig; cv::resize(vis, visBig, cv::Size(), sc, sc, cv::INTER_NEAREST);
    char fn[256]; snprintf(fn,sizeof(fn),"/tmp/spcv_sp%d_pt%d_%d_%dx%d.png",spId,(int)pt.x,(int)pt.y,nP,nS); cv::imwrite(fn,visBig);
    char fn2[256]; snprintf(fn2,sizeof(fn2),"/tmp/spcvraw_sp%d_pt%d_%d.png",spId,(int)pt.x,(int)pt.y); cv::imwrite(fn2,g);
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
  {
    // Columns needed to bracket the answer: everything up to the first hit,
    // plus the consider band it averages over, plus the two-column lag any
    // 3x3-based scan carries.
    const double needed = (double)(pMin + cp) + (double)considerRange + 2.0;
    const double cols = needed < 3 ? 3 : (needed > spcv_nP ? spcv_nP : needed);
    mephase::count("spcv_samp_min", (double)spcv_nS * cols);
  }
  if (outPt) *outPt = acvVecAdd(pt, acvVecAdd(acvVecMult(s, eS), acvVecMult(perp, eP)));
  if (outW) *outW = (float)Ws;

  // Per-edge hits for caliper-mode visualization. Each strength-gated row
  // edge becomes one CaliperHit; status=2 if within considerRange of pMin
  // (contributed to the final average), 1 otherwise.
  if (outHits) {
    outHits->clear();
    outHits->reserve(eps.size());
    for (auto &e : eps) {
      acv_XY ep = acvVecAdd(pt, acvVecAdd(acvVecMult(s, e.searchCoord),
                                          acvVecMult(perp, e.perpCoord)));
      int st = (e.perpCoord - pMin <= considerRange) ? 2 : 1;
      outHits->push_back(CaliperHit{ep, st, e.peak});
    }
  }
  return true;
}

