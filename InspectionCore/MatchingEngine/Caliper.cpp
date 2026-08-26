#include "Caliper.h"
#include "CvBridge.h"                // cvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <math.h>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <string>

// Lens-undistort a gray-local point in place. Lifts (p + imgOffset) into full-
// sensor px, undistorts via the calibrated lens model, then drops back to gray-
// local coords. No-op when bacpac/lensCalib is absent or invalid (handled here
// and also inside lens_undistort_point for defense in depth). Returns true iff
// the result is finite -- the Newton solve in lens_undistort_points can diverge
// for points outside the calibration's validity region; callers must skip a
// non-finite result to avoid poisoning the downstream WLS / Kasa fit.
static inline bool caliper_undist_in_place(acv_XY &p, FeatureManager_BacPac *bacpac, acv_XY imgOffset)
{
  if (!(bacpac && bacpac->lensCalib && bacpac->lensCalib->ok)) return true;
  // The lens model is in FULL-SENSOR pixel coords. To lift a point sitting in
  // the inspection image's local frame, we need BOTH offsets:
  //   imgOffset                 = internal labeling crop within the inspection image
  //   sampler->originOffset()   = user-applied camera ROI (the "crop" they configured)
  // Adding only `imgOffset` (the labeling crop, usually 0) was leaving the user's
  // ROI uncompensated -- distortion correction landed at the wrong (x,y) on the
  // lens model whenever a non-zero ROI was active.
  acv_XY sOff = (bacpac->sampler) ? bacpac->sampler->getOriginOffset() : acv_XY{0.f, 0.f};
  float x = p.x + imgOffset.x + sOff.x;
  float y = p.y + imgOffset.y + sOff.y;
  lens_undistort_point(*bacpac->lensCalib, x, y);
  if (!std::isfinite(x) || !std::isfinite(y)) return false;
  p.x = x - imgOffset.x - sOff.x;
  p.y = y - imgOffset.y - sOff.y;
  return true;
}

// Combine each caliper's across-edge profile into ONE image for a single primitive:
// x = caliper index along the line, y = across-edge search position. The picked edge
// per caliper is marked (green = fit inlier, red = outlier, gray = no edge found).
// The primitive name is drawn on the image and used in the filename.
static void caliper_dump_line_strip(const char *prefix, const char *name, const EdgeSelectParams &edge,
                                    const std::vector<std::vector<float>> &profs,
                                    const std::vector<float> &pos,
                                    const std::vector<float> &conf,      // per-caliper confidence (-1 none)
                                    const std::vector<char> *use,        // over VALID pts only
                                    const std::vector<int> &ptCaliper,   // pts idx -> caliper idx
                                    int count)
{
  // nAcross = profile length (same for all valid calipers)
  int nAcross = 0; for (auto &p : profs) if ((int)p.size() > nAcross) nAcross = (int)p.size();
  if (nAcross < 3 || count < 1) return;
  // map caliper index -> inlier/outlier (from `use` over valid pts)
  std::vector<int> inlier(count, -1); // -1 no edge, 0 outlier, 1 inlier
  if (use) for (size_t k = 0; k < ptCaliper.size() && k < use->size(); k++)
    inlier[ptCaliper[k]] = (*use)[k] ? 1 : 0;
  else for (int c : ptCaliper) inlier[c] = 1; // pre-fit: all valid as inlier

  // grayscale strip, one column per caliper
  cv::Mat strip(nAcross, count, CV_8U, cv::Scalar(0));
  for (int i = 0; i < count && i < (int)profs.size(); i++)
  {
    const auto &pr = profs[i];
    for (int y = 0; y < (int)pr.size() && y < nAcross; y++)
    {
      float v = pr[y]; unsigned char b = (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)(v + 0.5f));
      strip.at<unsigned char>(y, i) = b;
    }
  }
  cv::Mat vis; std::vector<cv::Mat> ch = {strip, strip, strip}; cv::merge(ch, vis);
  float maxConf = 0; for (float c : conf) if (c > maxConf) maxConf = c; // normalize for display
  for (int i = 0; i < count && i < (int)pos.size(); i++)
  {
    if (pos[i] < 0) continue;
    int y = (int)lroundf(pos[i]); if (y < 0 || y >= nAcross) continue;
    cv::Vec3b col;
    if (inlier[i] == 0) col = cv::Vec3b(0,0,255);            // outlier: red
    else if (inlier[i] < 0) col = cv::Vec3b(160,160,160);    // no edge: gray
    else {                                                    // inlier: green, brightness ~ confidence
      float cn = (maxConf > 0 && i < (int)conf.size() && conf[i] >= 0) ? conf[i] / maxConf : 1.f;
      col = cv::Vec3b(0, (uchar)(60 + 195 * cn), 0);
    }
    vis.at<cv::Vec3b>(y, i) = col;
  }
  // upscale for visibility (wide in x = caliper index, tall in y = across-edge)
  int sx = std::max(1, 600 / std::max(1,count)), sy = std::max(1, 400 / nAcross);
  cv::Mat big; cv::resize(vis, big, cv::Size(), sx, sy, cv::INTER_NEAREST);
  const char *mName[] = {"STRONGEST","FIRST","LAST","MIDDLE","NTH"};
  const char *pName[] = {"ANY","RISING","FALLING"};
  char label[160];
  snprintf(label, sizeof(label), "%s  method=%s pol=%s nth=%d min_str=%.0f",
           name ? name : "line",
           (edge.method>=0&&edge.method<5)?mName[edge.method]:"?",
           (edge.polarity>=0&&edge.polarity<3)?pName[edge.polarity]:"?",
           edge.nth, edge.min_strength);
  cv::putText(big, label, cv::Point(4, 16), cv::FONT_HERSHEY_SIMPLEX, 0.5,
              cv::Scalar(0,255,255), 1, cv::LINE_AA);
  // sanitize name for filename
  std::string fn = std::string("/tmp/calip_") + (prefix?prefix:"line") + "_";
  for (const char *p = name ? name : "x"; *p; ++p)
    fn += (*p=='/'||*p==' '||*p=='\\') ? '_' : *p;
  fn += ".png";
  cv::imwrite(fn, big);
  fprintf(stderr, "[CALIP] %s %s: %d calipers, strip %dx%d -> %s\n", prefix?prefix:"line", name?name:"x", count, count, nAcross, fn.c_str());
}


// Shared: 1-D across-edge profile -> sub-pixel edge point. central-diff gradient,
// edge_select (method/polarity/strength), map the chosen index back to an image
// point (center + s * t_edge). Used by both the per-caliper and rectify-once paths
// so they pick edges identically.
static bool profile_to_edge(const float *profile, int nAcross, float step, float L,
                            const EdgeSelectParams &edge, acv_XY center, acv_XY s,
                            acv_XY *outPt, float *outStrength, EdgeSelectInfo *outInfo,
                            float *outPos)
{
  std::vector<float> grad(nAcross, 0);
  for (int i = 1; i < nAcross - 1; i++) grad[i] = profile[i + 1] - profile[i - 1];
  grad[0] = grad[1]; grad[nAcross - 1] = grad[nAcross - 2];
  float pos, str;
  if (!edge_select(grad.data(), nAcross, edge, &pos, &str, outInfo)) return false;
  if (outPos) *outPos = pos;
  float t_edge = -L + pos * step;
  if (outPt) *outPt = acvVecAdd(center, acvVecMult(s, t_edge));
  if (outStrength) *outStrength = str;
  return true;
}

bool caliper_measure(const cv::Mat &gray, acv_XY center, acv_XY searchDir,
                     const CaliperParams &p, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outStrength, EdgeSelectInfo *outInfo,
                     std::vector<float> *outProfile, float *outPos)
{
  if (outInfo) *outInfo = EdgeSelectInfo();
  if (gray.empty()) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.x != s.x || s.y != s.y) return false;
  acv_XY edgeDir = { -s.y, s.x }; // along the edge (projection direction)

  float L = p.length, W = p.width, step = (p.step > 0 ? p.step : 1.0f);
  // Size the profile in double BEFORE the int conversion. cal_step is only
  // checked for <= 0 and is divided by mmpp upstream, so a def value of 0.001
  // (1 um -- a plausible-looking entry) inflates this ~11x, and small enough
  // values overflow the (int) conversion, which is UB -- arm64 saturates to
  // INT_MAX and sails right past the < 3 guard into the allocation.
  double nAcross_d = 2.0 * L / step + 1.0;
  if (!(nAcross_d >= 3)) return false;          // also rejects NaN
  if (nAcross_d > 1e6)
  {
    printf("caliper: refusing a %.0f-sample profile -- check that "
           "caliper step is in mm, not px\n", nAcross_d);
    return false;
  }
  int nAcross = (int)nAcross_d;
  int halfW = (int)(W / 2);

  std::vector<float> profile(nAcross, 0);
  for (int i = 0; i < nAcross; i++)
  {
    float t = -L + i * step;
    acv_XY c = acvVecAdd(center, acvVecMult(s, t));
    double sum = 0; int cnt = 0;
    for (int w = -halfW; w <= halfW; w++)
    {
      acv_XY pt = acvVecAdd(c, acvVecMult(edgeDir, (float)w));
      float v = cvUnsignedMap1Sampling(gray, pt.x, pt.y, 0);
      if (v != v) continue;   // off-image sample: drop it, don't NaN the whole row
      if (bacpac && bacpac->sampler)
        v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(pt);
      if (v != v) continue;
      sum += v; cnt++;
    }
    profile[i] = (cnt > 0) ? (float)(sum / cnt) : 0;
  }
  if (outProfile) *outProfile = profile;
  return profile_to_edge(profile.data(), nAcross, step, L, p.edge, center, s,
                         outPt, outStrength, outInfo, outPos);
}

bool search_point_scan(const cv::Mat &gray, acv_XY start, acv_XY searchDir,
                       float length, float width, float step,
                       const EdgeSelectParams &edge, FeatureManager_BacPac *bacpac,
                       acv_XY *outPt, float *outStrength)
{
  if (gray.empty()) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.x != s.x || s.y != s.y) return false;
  acv_XY perp = { -s.y, s.x };               // across the ray (width direction)
  if (step <= 0) step = 1.0f;
  int nAlong = (int)(length / step) + 1;
  if (nAlong < 3) return false;
  int nCols = (int)width; if (nCols < 1) nCols = 1;
  int halfC = nCols / 2;

  std::vector<float> prof(nAlong), grad(nAlong);
  std::vector<float> dist, wgt;                // per-column first-hit distance + strength
  for (int c = -halfC; c <= halfC; c++)
  {
    for (int i = 0; i < nAlong; i++)
    {
      acv_XY pt = acvVecAdd(start, acvVecAdd(acvVecMult(s, i * step), acvVecMult(perp, (float)c)));
      float v = cvUnsignedMap1Sampling(gray, pt.x, pt.y, 0);
      if (bacpac && bacpac->sampler)
        v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(pt);
      prof[i] = v;
    }
    for (int i = 1; i < nAlong - 1; i++) grad[i] = prof[i + 1] - prof[i - 1];
    grad[0] = grad[1]; grad[nAlong - 1] = grad[nAlong - 2];
    float pos, str;
    if (edge_select(grad.data(), nAlong, edge, &pos, &str)) // method defaults FIRST = first hit
    { dist.push_back(pos * step); wgt.push_back(str); }
  }
  if (dist.empty()) return false;

  // robust combine: median first-hit distance, then strength-weighted mean of
  // inliers within tol of the median (rejects columns that hit a different edge).
  std::vector<float> sd = dist; std::sort(sd.begin(), sd.end());
  float med = sd[sd.size() / 2];
  float tol = 2.0f; if (tol < step) tol = step;
  double sumd = 0, sumw = 0; int nin = 0;
  for (size_t i = 0; i < dist.size(); i++)
    if (fabsf(dist[i] - med) <= tol) { sumd += (double)dist[i] * wgt[i]; sumw += wgt[i]; nin++; }
  float d = (sumw > 0) ? (float)(sumd / sumw) : med;
  if (outPt) *outPt = acvVecAdd(start, acvVecMult(s, d));
  if (outStrength) *outStrength = (nin > 0) ? (float)(sumw / nin) : wgt[0];
  return true;
}

// weighted total-least-squares line fit over points with weights w; returns
// anchor (weighted centroid), unit dir, and fills perpendicular residuals.
// Returns false when there is no line to be had: zero total weight, or all
// inlier points coincident (zero covariance -- atan2(0,0) would "fit" a
// horizontal line with rms 0, i.e. always in spec, which is the one answer a
// degenerate input must never produce).
static bool wlsLine(const std::vector<acv_XY> &pts, const std::vector<float> &w,
                    const std::vector<char> &use, acv_XY &anchor, acv_XY &dir)
{
  double sw = 0, mx = 0, my = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i]) { sw += w[i]; mx += w[i]*pts[i].x; my += w[i]*pts[i].y; }
  if (sw <= 0) return false;
  mx /= sw; my /= sw;
  double a = 0, b = 0, c = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { double dx = pts[i].x-mx, dy = pts[i].y-my; a += w[i]*dx*dx; b += w[i]*dx*dy; c += w[i]*dy*dy; }
  if (a + c < 1e-9) return false;   // all points in one spot
  double theta = 0.5 * atan2(2*b, a - c); // major axis of weighted covariance
  anchor = { (float)mx, (float)my };
  dir = { (float)cos(theta), (float)sin(theta) };
  return true;
}

CaliperLineResult caliper_locate_line(const cv::Mat &gray, acv_XY p0, acv_XY p1,
                                      int count, const CaliperParams &cal,
                                      FeatureManager_BacPac *bacpac,
                                      const char *dbgName, acv_XY imgOffset)
{
  CaliperLineResult r = {}; r.ok = false; r.dir = {1,0};
  count = caliper_effective_count(count, CALIPER_MIN_COUNT_LINE);
  acv_XY lineDir = acvVecNormalize(acvVecSub(p1, p0));
  // A zero-length line (p1 == p0) or a NaN endpoint from morph normalizes to
  // NaN, and caliper_measure's sibling check has no counterpart here -- the
  // NaN used to flow into the band geometry and the (int) conversions below.
  if (lineDir.x != lineDir.x || lineDir.y != lineDir.y) { r.nValid = 0; return r; }
  acv_XY perp = { -lineDir.y, lineDir.x }; // caliper search direction (across edge)

  const bool dbg = (dbgName != nullptr) && (getenv("CALIP_DUMP") != nullptr);
  std::vector<std::vector<float>> dProfs;   // per-caliper across-edge profile (dbg)
  std::vector<float> dPos;                   // per-caliper edge index (dbg), -1 if none
  std::vector<float> dConf;                  // per-caliper confidence (dbg), -1 if none
  std::vector<int> ptCaliper;                // pts index -> caliper index (dbg)

  // RECTIFY-ONCE + PREFIX-SUM. All calipers on a straight line share one rotated band,
  // so sample it a single time into a contiguous buffer B[nAlong][nAcross] (rows = along
  // the line, cols = across the edge), instead of re-gathering a diagonal grid per
  // caliper. The per-caliper along-edge box average then becomes an O(nAcross) prefix-sum
  // subtraction. Edge picking is identical (profile_to_edge) to caliper_measure.
  float L = cal.length, step = (cal.step > 0 ? cal.step : 1.0f);
  // Same double-first sizing as caliper_measure: a tiny step overflows the
  // (int) conversion (UB; arm64 saturates INT_MAX) before the CELL_LIMIT
  // below ever sees a sane number.
  double nAcross_d = 2.0 * L / step + 1.0;
  if (!(nAcross_d >= 3) || nAcross_d > 8e6)
  {
    if (nAcross_d > 8e6)
      printf("caliper: refusing a %.0f-sample band width -- check that "
             "caliper step is in mm, not px\n", nAcross_d);
    r.nValid = 0; return r;
  }
  int nAcross = (int)nAcross_d;
  // A negative width has no meaning, and it used to KILL THE PROCESS: halfW
  // went negative, nAlong = alongLen + 2*halfW + 1 went negative with it, and
  // (size_t)negative * nAcross is astronomically large -> std::length_error ->
  // uncaught -> abort. Not a crash in this frame: the whole core, i.e. the
  // machine, on one mistyped def value. (The sibling caliper_measure path
  // survives the same input by accident -- its `for (w=-halfW; w<=halfW; w++)`
  // simply never executes -- which is why this only ever showed up here.)
  int halfW = (int)(cal.width / 2);
  if (halfW < 0) halfW = 0;
  float alongLen = (float)hypot(p1.x - p0.x, p1.y - p0.y);
  std::vector<acv_XY> pts; std::vector<float> w;
  if (nAcross < 3) { r.nValid = 0; if (dbg) caliper_dump_line_strip("line", dbgName, cal.edge, dProfs, dPos, dConf, nullptr, ptCaliper, count); return r; }

  int nAlong = (int)lroundf(alongLen) + 2 * halfW + 1; // a in [0,nAlong) -> along dist (a - halfW)
  // Belt and braces: alongLen comes from the def's own endpoints and nothing
  // upstream promises it is finite or positive. The allocation below is the
  // one place where a bad number stops being a bad measurement and becomes a
  // dead process, so it does not get to trust its inputs.
  if (nAlong < 1) { r.nValid = 0; return r; }
  // The other end of the same range. The def's clamps (count<=512, width<=64,
  // length<=256) are in MILLIMETRES and are applied before the /= mmpp
  // conversion; at this machine's ~72-135 px/mm they still permit nAcross in
  // the tens of thousands and halfW in the thousands, so the two buffers below
  // reach gigabytes and the std::bad_alloc aborts the process -- the same
  // "one mistyped def value kills the machine" failure as the negative width,
  // approached from above. The realistic trigger is not a hostile def, it is
  // someone typing a pixel figure into a field that wants millimetres.
  //
  // Those clamps were sized against caliper_measure's cost model, which only
  // allocates nAcross floats per caliper; this path allocates nAlong*nAcross
  // for the whole band, so the clamps bound the CPU but not this memory.
  //
  // A real caliper is far below this: a 5mm span at 0.01mm steps with a 1mm
  // width is well under a million cells.
  {
    const size_t cells = (size_t)nAlong * (size_t)nAcross;
    const size_t CELL_LIMIT = 8u * 1024u * 1024u;   // ~32MB float + ~64MB double
    if (cells > CELL_LIMIT)
    {
      printf("caliper: refusing a %dx%d band (%zu cells) -- check that "
             "caliper length/width/step are in mm, not px\n",
             nAlong, nAcross, cells);
      r.nValid = 0;
      return r;
    }
  }
  acv_XY perpStep = acvVecMult(perp, step);
  std::vector<float> B((size_t)nAlong * nAcross);
  // Off-image samples are NaN, and a NaN stored into B poisons the column
  // prefix sum below: every LATER caliper window touching that column averages
  // to NaN, so one corner of the band off-frame silently blinds all downstream
  // calipers on the line. The arc path (caliper_measure) drops such samples and
  // averages what is left ("don't NaN the whole row"); this path now does the
  // same via a parallel valid-count prefix sum. Bvalid is allocated lazily so
  // the common all-on-image band pays nothing.
  std::vector<uint8_t> Bvalid;
  bool hasNaN = false;
  for (int a = 0; a < nAlong; a++)
  {
    acv_XY rowBase = acvVecAdd(p0, acvVecMult(lineDir, (float)(a - halfW)));
    acv_XY q = acvVecAdd(rowBase, acvVecMult(perp, -L));
    float *Brow = &B[(size_t)a * nAcross];
    for (int x = 0; x < nAcross; x++)
    {
      float v = cvUnsignedMap1Sampling(gray, q.x, q.y, 0);
      if (bacpac && bacpac->sampler) v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(q);
      if (v != v) // off-image (or NaN backlight factor): drop the sample
      {
        if (!hasNaN) { hasNaN = true; Bvalid.assign(B.size(), 1); }
        Bvalid[(size_t)a * nAcross + x] = 0;
        v = 0;
      }
      Brow[x] = v;
      q = acvVecAdd(q, perpStep);
    }
  }
  // column prefix sums over the along axis: S[a+1][x] = S[a][x] + B[a][x]; S[0]=0.
  std::vector<double> S((size_t)(nAlong + 1) * nAcross, 0.0);
  // valid-sample-count prefix, same layout; only materialised when needed.
  std::vector<int> Scnt;
  if (hasNaN) Scnt.assign((size_t)(nAlong + 1) * nAcross, 0);
  for (int a = 0; a < nAlong; a++)
  {
    const float *Brow = &B[(size_t)a * nAcross];
    const double *Sp = &S[(size_t)a * nAcross];
    double *Sc = &S[(size_t)(a + 1) * nAcross];
    for (int x = 0; x < nAcross; x++) Sc[x] = Sp[x] + Brow[x];
    if (hasNaN)
    {
      const uint8_t *Vrow = &Bvalid[(size_t)a * nAcross];
      const int *Cp = &Scnt[(size_t)a * nAcross];
      int *Cc = &Scnt[(size_t)(a + 1) * nAcross];
      for (int x = 0; x < nAcross; x++) Cc[x] = Cp[x] + Vrow[x];
    }
  }

  std::vector<float> profile(nAcross);
  // Per-caliper hits, always tracked (one entry per caliper i in [0,count)).
  // status starts at 0=missed; flipped to 2=inlier after the fit (then some may
  // be demoted to 1=outlier when MAD rejection runs).
  r.hits.assign(count, CaliperHit{{0,0}, 0, 0.0f});
  // ptCaliper maps pts[]-index -> caliper-index so we can attribute MAD outlier
  // decisions back to per-caliper status; always populated now (was dbg-only).
  ptCaliper.clear(); ptCaliper.reserve(count);
  for (int i = 0; i < count; i++)
  {
    float u = (float)i / (count - 1);
    acv_XY c = acvVecAdd(p0, acvVecMult(acvVecSub(p1, p0), u)); // caliper center on the line
    int aCtr = (int)lroundf(u * alongLen) + halfW;             // buffer row of the center
    int aLo = aCtr - halfW, aHi = aCtr + halfW;                // window rows [aLo, aHi]
    if (aLo < 0) aLo = 0; if (aHi > nAlong - 1) aHi = nAlong - 1;
    int cnt = aHi - aLo + 1;
    const double *Shi = &S[(size_t)(aHi + 1) * nAcross];
    const double *Slo = &S[(size_t)aLo * nAcross];
    if (!hasNaN)
    {
      for (int x = 0; x < nAcross; x++) profile[x] = (float)((Shi[x] - Slo[x]) / cnt);
    }
    else
    {
      // average over the VALID samples in the window; a fully-off-image column
      // yields 0, same as caliper_measure's `(cnt > 0) ? sum/cnt : 0`.
      const int *Chi = &Scnt[(size_t)(aHi + 1) * nAcross];
      const int *Clo = &Scnt[(size_t)aLo * nAcross];
      for (int x = 0; x < nAcross; x++)
      {
        int vc = Chi[x] - Clo[x];
        profile[x] = (vc > 0) ? (float)((Shi[x] - Slo[x]) / vc) : 0.0f;
      }
    }

    acv_XY pt; float str, pos = -1; EdgeSelectInfo info;
    bool ok = profile_to_edge(profile.data(), nAcross, step, L, cal.edge, c, perp, &pt, &str, &info, &pos);
    // Lens-undistort BEFORE the fit so a distortion-curved edge fits the true
    // straight line, not a biased one. We also undistort the nominal anchor
    // `c` so the missed-caliper visualization (stored as a CaliperHit with
    // status=0 below) lives in the same coord frame as inlier hits. A non-
    // finite result (Newton diverged) demotes the caliper to a miss so the
    // bad point can't poison the WLS aggregate.
    if (ok && !caliper_undist_in_place(pt, bacpac, imgOffset)) ok = false;
    caliper_undist_in_place(c, bacpac, imgOffset); // missed-anchor (used at status=0)
    // per-caliper CONFIDENCE used as the fit weight, combining three factors:
    //  - strength : strong gradient = real edge.
    //  - unambiguity (1 - 0.7*runnerUp/strength) : a near-equal competing peak means the
    //    caliper may have grabbed the wrong edge -> downweight.
    //  - sharpness : a crisp (tight) gradient bump localizes the sub-pixel edge better
    //    than a smeared one. sharp ~[0..2]; factor 0.5..1 (blurry never fully zeroed).
    // (keeps weight > 0 so it still contributes; MAD still hard-rejects gross outliers.)
    float conf = 0;
    if (ok)
    {
      float ratio = (info.strength > 0) ? (info.runnerUp / info.strength) : 0;
      if (ratio > 1) ratio = 1;
      float sharpN = info.sharpness; if (sharpN > 1) sharpN = 1; if (sharpN < 0) sharpN = 0;
      float sharpF = 0.5f + 0.5f * sharpN;
      conf = info.strength * (1.0f - 0.7f * ratio) * sharpF;
      pts.push_back(pt); w.push_back(conf); ptCaliper.push_back(i);
      r.hits[i] = CaliperHit{pt, 2 /*inlier-until-MAD-says-otherwise*/, conf};
    }
    else
    {
      // Missed caliper — no edge passed the polarity/min_strength filter.
      // Stash the nominal anchor `c` so consumers (WebUI overlay) can show
      // *which* caliper missed; status=0 keeps it distinct from inlier/outlier.
      r.hits[i] = CaliperHit{c, 0, 0.0f};
    }
    if (dbg) { dProfs.push_back(profile); dPos.push_back(ok ? pos : -1.f); dConf.push_back(ok ? conf : -1.f); }
  }
  r.nValid = (int)pts.size();
  if (r.nValid < 2) { if (dbg) caliper_dump_line_strip("line", dbgName, cal.edge, dProfs, dPos, dConf, nullptr, ptCaliper, count); return r; }

  std::vector<char> use(pts.size(), 1);
  acv_XY anchor = {0,0}, dir = {1,0};
  // If the fit NEVER succeeds, r must not be ok: anchor/dir would still hold
  // their initialisers and the "measurement" below would be a fabrication with
  // rms ~0 -- always in spec (backlog 1.7).
  bool fitOk = false;
  for (int iter = 0; iter < 3; iter++)
  {
    if (!wlsLine(pts, w, use, anchor, dir)) break;
    fitOk = true;
    acv_XY n = { -dir.y, dir.x };
    std::vector<float> res(pts.size());
    std::vector<float> absr;
    for (size_t i = 0; i < pts.size(); i++)
    { res[i] = (pts[i].x-anchor.x)*n.x + (pts[i].y-anchor.y)*n.y; if (use[i]) absr.push_back(fabsf(res[i])); }
    if (absr.empty()) break;
    std::sort(absr.begin(), absr.end());
    float med = absr[absr.size()/2];
    float thr = 3.0f * 1.4826f * med + 0.5f; // MAD-based, +0.5px floor
    if (cal.max_error > 0 && thr > cal.max_error) thr = cal.max_error;
    int changed = 0;
    for (size_t i = 0; i < pts.size(); i++) { char nu = fabsf(res[i]) <= thr ? 1 : 0; if (nu != use[i]) changed++; use[i] = nu; }
    if (!changed) break;
  }
  // final stats
  acv_XY n = { -dir.y, dir.x };
  double sq = 0, sumw = 0; int ni = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { float d = (pts[i].x-anchor.x)*n.x + (pts[i].y-anchor.y)*n.y; sq += d*d; sumw += w[i]; ni++; }
  // Demote MAD-rejected hits from inlier (2) to outlier (1). Missed (0) stays 0.
  for (size_t k = 0; k < pts.size(); k++)
    if (!use[k]) r.hits[ ptCaliper[k] ].status = 1;
  r.anchor = anchor; r.dir = dir; r.nInlier = ni;
  r.rms = (ni > 0) ? sqrtf(sq / ni) : 0;
  r.confidence = (ni > 0) ? (float)(sumw / ni) : 0;
  const int minInliers = (cal.min_inliers > 0) ? cal.min_inliers : 2;
  r.ok = fitOk && (ni >= minInliers);
  if (dbg) caliper_dump_line_strip("line", dbgName, cal.edge, dProfs, dPos, dConf, &use, ptCaliper, count);
  return r;
}

// solve 3x3 A x = b in place (Gaussian elimination); false if singular.
static bool solve3(double A[9], double b[3])
{
  for (int col = 0; col < 3; col++)
  {
    int piv = col; double best = fabs(A[col*3+col]);
    for (int r = col+1; r < 3; r++) { double v = fabs(A[r*3+col]); if (v > best) { best = v; piv = r; } }
    if (best < 1e-12) return false;
    if (piv != col) { for (int c=0;c<3;c++){double t=A[col*3+c];A[col*3+c]=A[piv*3+c];A[piv*3+c]=t;} double tb=b[col];b[col]=b[piv];b[piv]=tb; }
    for (int r=0;r<3;r++){ if(r==col)continue; double f=A[r*3+col]/A[col*3+col]; for(int c=col;c<3;c++)A[r*3+c]-=f*A[col*3+c]; b[r]-=f*b[col]; }
  }
  for (int i=0;i<3;i++) b[i]/=A[i*3+i];
  return true;
}

// weighted Kasa algebraic circle fit over used points -> center, radius
static bool kasaCircle(const std::vector<acv_XY> &pts, const std::vector<float> &w,
                       const std::vector<char> &use, acv_XY &center, float &radius)
{
  // minimize sum w*(x^2+y^2 + A x + B y + C)^2 ; center=(-A/2,-B/2), r=sqrt(A^2/4+B^2/4 - C)
  double M[9] = {0}; double rhs[3] = {0};
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  {
    double x = pts[i].x, y = pts[i].y, ww = w[i];
    double z = -(x*x + y*y);
    double phi[3] = { x, y, 1.0 };
    for (int a=0;a<3;a++){ rhs[a]+=ww*phi[a]*z; for(int c=0;c<3;c++) M[a*3+c]+=ww*phi[a]*phi[c]; }
  }
  double sol[3] = { rhs[0], rhs[1], rhs[2] };
  if (!solve3(M, sol)) return false;
  double A = sol[0], B = sol[1], C = sol[2];
  center = { (float)(-A/2), (float)(-B/2) };
  double r2 = A*A/4 + B*B/4 - C;
  if (r2 <= 0) return false;
  radius = (float)sqrt(r2);
  return true;
}

CaliperCircleResult caliper_locate_circle(const cv::Mat &gray, acv_XY center0, float radius0,
                                          float angStart, float angEnd, int count,
                                          const CaliperParams &cal, FeatureManager_BacPac *bacpac,
                                          const char *dbgName, acv_XY imgOffset)
{
  CaliperCircleResult r = {}; r.ok = false; r.center = center0; r.radius = radius0;
  count = caliper_effective_count(count, CALIPER_MIN_COUNT_ARC);
  const bool dbg = (dbgName != nullptr) && (getenv("CALIP_DUMP") != nullptr);
  std::vector<std::vector<float>> dProfs; std::vector<float> dPos, dConf; std::vector<int> ptCaliper;

  // Sweep CCW from angStart to angEnd (the convert3Pts2ArcData convention: the arc
  // through the middle point is the CCW span, which may exceed pi). Using the raw
  // (angEnd-angStart) would go the wrong way around for reflex/wrapped arcs, placing
  // caliper centers on the opposite side of the circle (off the real edge).
  float span = angEnd - angStart;
  while (span < 0) span += 2 * (float)M_PI;
  while (span >= 2 * (float)M_PI) span -= 2 * (float)M_PI;

  std::vector<acv_XY> pts; std::vector<float> w;
  // Per-caliper hits, always tracked. See caliper_locate_line for the encoding.
  r.hits.assign(count, CaliperHit{{0,0}, 0, 0.0f});
  ptCaliper.clear(); ptCaliper.reserve(count);
  for (int i = 0; i < count; i++)
  {
    float a = angStart + span * (float)i / (count - 1);
    acv_XY dir = { cosf(a), sinf(a) };               // radial = across-edge
    acv_XY c = acvVecAdd(center0, acvVecMult(dir, radius0));
    acv_XY pt; float str, pos = -1; EdgeSelectInfo info;
    std::vector<float> prof;
    bool ok = caliper_measure(gray, c, dir, cal, bacpac, &pt, &str, &info,
                              dbg ? &prof : nullptr, &pos);
    // Undistort the inlier hit (used for the Kasa fit) and the nominal radial
    // anchor (used as the missed-caliper marker at status=0) -- see
    // caliper_locate_line for the rationale and the NaN demote-on-divergence.
    if (ok && !caliper_undist_in_place(pt, bacpac, imgOffset)) ok = false;
    caliper_undist_in_place(c, bacpac, imgOffset);
    float conf = 0;
    if (ok)
    {
      float ratio = (info.strength > 0) ? (info.runnerUp / info.strength) : 0; if (ratio > 1) ratio = 1;
      float sharpN = info.sharpness; if (sharpN > 1) sharpN = 1; if (sharpN < 0) sharpN = 0;
      conf = info.strength * (1.0f - 0.7f * ratio) * (0.5f + 0.5f * sharpN);
      pts.push_back(pt); w.push_back(conf); ptCaliper.push_back(i);
      r.hits[i] = CaliperHit{pt, 2, conf};
    }
    else
    {
      // Missed caliper — see caliper_locate_line for rationale.
      r.hits[i] = CaliperHit{c, 0, 0.0f};
    }
    if (dbg) { dProfs.push_back(prof); dPos.push_back(ok ? pos : -1.f); dConf.push_back(ok ? conf : -1.f); }
  }
  r.nValid = (int)pts.size();
  if (r.nValid < 3) { if (dbg) caliper_dump_line_strip("arc", dbgName, cal.edge, dProfs, dPos, dConf, nullptr, ptCaliper, count); return r; }

  std::vector<char> use(pts.size(), 1);
  acv_XY cen = center0; float rad = radius0;
  // Same guard as the line path: if Kasa never solves, cen/rad still hold the
  // def's NOMINAL circle, residuals against it are ~0 (the calipers were
  // centred on it), and the nominal would be reported as a measurement with
  // rms 0 (backlog 1.7).
  bool fitOk = false;
  for (int iter = 0; iter < 3; iter++)
  {
    if (!kasaCircle(pts, w, use, cen, rad)) break;
    fitOk = true;
    std::vector<float> absr;
    for (size_t i = 0; i < pts.size(); i++) if (use[i])
      absr.push_back(fabsf(hypotf(pts[i].x-cen.x, pts[i].y-cen.y) - rad));
    if (absr.empty()) break;
    std::sort(absr.begin(), absr.end());
    float med = absr[absr.size()/2];
    float thr = 3.0f * 1.4826f * med + 0.5f;
    if (cal.max_error > 0 && thr > cal.max_error) thr = cal.max_error;
    int changed = 0;
    for (size_t i = 0; i < pts.size(); i++)
    { float d = fabsf(hypotf(pts[i].x-cen.x, pts[i].y-cen.y) - rad); char nu = d<=thr?1:0; if (nu!=use[i])changed++; use[i]=nu; }
    if (!changed) break;
  }
  double sq = 0, sumw = 0; int ni = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { float d = hypotf(pts[i].x-cen.x, pts[i].y-cen.y) - rad; sq += d*d; sumw += w[i]; ni++; }
  for (size_t k = 0; k < pts.size(); k++)
    if (!use[k]) r.hits[ ptCaliper[k] ].status = 1;
  r.center = cen; r.radius = rad; r.nInlier = ni;
  r.rms = (ni>0)?sqrtf(sq/ni):0;
  r.confidence = (ni>0)?(float)(sumw/ni):0;
  const int minInliers = (cal.min_inliers > 0) ? cal.min_inliers : 3;
  r.ok = fitOk && (ni >= minInliers);
  if (dbg)
  {
    caliper_dump_line_strip("arc", dbgName, cal.edge, dProfs, dPos, dConf, &use, ptCaliper, count);
    fprintf(stderr, "[CALIP] arc %s: nominal=(%.2f,%.2f) r=%.2f  ->  fit=(%.2f,%.2f) r=%.2f  nInlier=%d/%d rms=%.3f  (search +/-%.0fpx)\n",
            dbgName, center0.x, center0.y, radius0, cen.x, cen.y, rad, ni, (int)pts.size(), r.rms, cal.length);
  }
  return r;
}
