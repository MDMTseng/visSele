#include "Caliper.h"
#include "acvImage_SpDomainTool.hpp" // acvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <math.h>
#include <vector>
#include <algorithm>

bool caliper_measure(acvImage *gray, acv_XY center, acv_XY searchDir,
                     const CaliperParams &p, FeatureManager_BacPac *bacpac,
                     acv_XY *outPt, float *outStrength)
{
  if (!gray) return false;
  acv_XY s = acvVecNormalize(searchDir);
  if (s.X != s.X || s.Y != s.Y) return false;
  acv_XY edgeDir = { -s.Y, s.X }; // along the edge (projection direction)

  float L = p.length, W = p.width, step = (p.step > 0 ? p.step : 1.0f);
  int nAcross = (int)(2 * L / step) + 1;
  if (nAcross < 3) return false;
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
      float v = acvUnsignedMap1Sampling(gray, pt, 0);
      if (bacpac && bacpac->sampler)
        v *= bacpac->sampler->sampleBackLightFactor_ImgCoord(pt);
      sum += v; cnt++;
    }
    profile[i] = (cnt > 0) ? (float)(sum / cnt) : 0;
  }

  // signed gradient along the across-edge axis
  std::vector<float> grad(nAcross, 0);
  for (int i = 1; i < nAcross - 1; i++) grad[i] = profile[i + 1] - profile[i - 1];
  grad[0] = grad[1]; grad[nAcross - 1] = grad[nAcross - 2];

  float pos, str;
  if (!edge_select(grad.data(), nAcross, p.edge, &pos, &str)) return false;

  float t_edge = -L + pos * step;
  if (outPt) *outPt = acvVecAdd(center, acvVecMult(s, t_edge));
  if (outStrength) *outStrength = str;
  return true;
}

// weighted total-least-squares line fit over points with weights w; returns
// anchor (weighted centroid), unit dir, and fills perpendicular residuals.
static void wlsLine(const std::vector<acv_XY> &pts, const std::vector<float> &w,
                    const std::vector<char> &use, acv_XY &anchor, acv_XY &dir)
{
  double sw = 0, mx = 0, my = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i]) { sw += w[i]; mx += w[i]*pts[i].X; my += w[i]*pts[i].Y; }
  if (sw <= 0) { anchor = {0,0}; dir = {1,0}; return; }
  mx /= sw; my /= sw;
  double a = 0, b = 0, c = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { double dx = pts[i].X-mx, dy = pts[i].Y-my; a += w[i]*dx*dx; b += w[i]*dx*dy; c += w[i]*dy*dy; }
  double theta = 0.5 * atan2(2*b, a - c); // major axis of weighted covariance
  anchor = { (float)mx, (float)my };
  dir = { (float)cos(theta), (float)sin(theta) };
}

CaliperLineResult caliper_locate_line(acvImage *gray, acv_XY p0, acv_XY p1,
                                      int count, const CaliperParams &cal,
                                      FeatureManager_BacPac *bacpac)
{
  CaliperLineResult r = {}; r.ok = false; r.dir = {1,0};
  if (count < 2) count = 2;
  acv_XY lineDir = acvVecNormalize(acvVecSub(p1, p0));
  acv_XY perp = { -lineDir.Y, lineDir.X }; // caliper search direction (across edge)

  std::vector<acv_XY> pts; std::vector<float> w;
  for (int i = 0; i < count; i++)
  {
    float u = (float)i / (count - 1);
    acv_XY c = acvVecAdd(p0, acvVecMult(acvVecSub(p1, p0), u));
    acv_XY pt; float str;
    if (caliper_measure(gray, c, perp, cal, bacpac, &pt, &str)) { pts.push_back(pt); w.push_back(str); }
  }
  r.nValid = (int)pts.size();
  if (r.nValid < 2) return r;

  std::vector<char> use(pts.size(), 1);
  acv_XY anchor = {0,0}, dir = {1,0};
  for (int iter = 0; iter < 3; iter++)
  {
    wlsLine(pts, w, use, anchor, dir);
    acv_XY n = { -dir.Y, dir.X };
    std::vector<float> res(pts.size());
    std::vector<float> absr;
    for (size_t i = 0; i < pts.size(); i++)
    { res[i] = (pts[i].X-anchor.X)*n.X + (pts[i].Y-anchor.Y)*n.Y; if (use[i]) absr.push_back(fabsf(res[i])); }
    if (absr.empty()) break;
    std::sort(absr.begin(), absr.end());
    float med = absr[absr.size()/2];
    float thr = 3.0f * 1.4826f * med + 0.5f; // MAD-based, +0.5px floor
    int changed = 0;
    for (size_t i = 0; i < pts.size(); i++) { char nu = fabsf(res[i]) <= thr ? 1 : 0; if (nu != use[i]) changed++; use[i] = nu; }
    if (!changed) break;
  }
  // final stats
  acv_XY n = { -dir.Y, dir.X };
  double sq = 0; int ni = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { float d = (pts[i].X-anchor.X)*n.X + (pts[i].Y-anchor.Y)*n.Y; sq += d*d; ni++; }
  r.anchor = anchor; r.dir = dir; r.nInlier = ni;
  r.rms = (ni > 0) ? sqrtf(sq / ni) : 0;
  r.ok = (ni >= 2);
  return r;
}
