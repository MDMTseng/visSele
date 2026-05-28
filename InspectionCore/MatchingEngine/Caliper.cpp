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
    double x = pts[i].X, y = pts[i].Y, ww = w[i];
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

CaliperCircleResult caliper_locate_circle(acvImage *gray, acv_XY center0, float radius0,
                                          float angStart, float angEnd, int count,
                                          const CaliperParams &cal, FeatureManager_BacPac *bacpac)
{
  CaliperCircleResult r = {}; r.ok = false; r.center = center0; r.radius = radius0;
  if (count < 3) count = 3;
  std::vector<acv_XY> pts; std::vector<float> w;
  for (int i = 0; i < count; i++)
  {
    float a = angStart + (angEnd - angStart) * (float)i / (count - 1);
    acv_XY dir = { cosf(a), sinf(a) };               // radial = across-edge
    acv_XY c = acvVecAdd(center0, acvVecMult(dir, radius0));
    acv_XY pt; float str;
    if (caliper_measure(gray, c, dir, cal, bacpac, &pt, &str)) { pts.push_back(pt); w.push_back(str); }
  }
  r.nValid = (int)pts.size();
  if (r.nValid < 3) return r;

  std::vector<char> use(pts.size(), 1);
  acv_XY cen = center0; float rad = radius0;
  for (int iter = 0; iter < 3; iter++)
  {
    if (!kasaCircle(pts, w, use, cen, rad)) break;
    std::vector<float> absr;
    for (size_t i = 0; i < pts.size(); i++) if (use[i])
      absr.push_back(fabsf(hypotf(pts[i].X-cen.X, pts[i].Y-cen.Y) - rad));
    if (absr.empty()) break;
    std::sort(absr.begin(), absr.end());
    float med = absr[absr.size()/2];
    float thr = 3.0f * 1.4826f * med + 0.5f;
    int changed = 0;
    for (size_t i = 0; i < pts.size(); i++)
    { float d = fabsf(hypotf(pts[i].X-cen.X, pts[i].Y-cen.Y) - rad); char nu = d<=thr?1:0; if (nu!=use[i])changed++; use[i]=nu; }
    if (!changed) break;
  }
  double sq = 0; int ni = 0;
  for (size_t i = 0; i < pts.size(); i++) if (use[i])
  { float d = hypotf(pts[i].X-cen.X, pts[i].Y-cen.Y) - rad; sq += d*d; ni++; }
  r.center = cen; r.radius = rad; r.nInlier = ni;
  r.rms = (ni>0)?sqrtf(sq/ni):0;
  r.ok = (ni >= 3);
  return r;
}
