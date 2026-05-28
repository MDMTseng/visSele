#include "Caliper.h"
#include "acvImage_SpDomainTool.hpp" // acvUnsignedMap1Sampling
#include "MatchingCore.h"            // acvVec* helpers
#include <math.h>
#include <vector>

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
