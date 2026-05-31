#include "vis_geom.h"
#include <math.h>
#include <stdint.h>

double acvFAtan(double x)
{
  double absx = fabs(x);
  return x * (M_PI_4 - (absx - 1) * (0.2447 + 0.0663 * absx));
}

double acvFAtan2(double y, double x)
{
  double atan;
  if (fabs(x) > fabs(y))
  {
    atan = acvFAtan(y / x);
    if (x < 0.0f)
    {
      if (y < 0.0f) return atan - M_PI;
      else return atan + M_PI;
    }
    return atan;
  }
  else
  {
    atan = acvFAtan(x / y);
    if (y < 0.0f) return -M_PI_2 - atan;
    else return M_PI_2 - atan;
  }
}

cv::Point2f acvRotation(float sine, float cosine, float flip_f, cv::Point2f input)
{
  cv::Point2f output;
  input.y *= flip_f;
  output.x = input.x * cosine - input.y * sine;
  output.y = input.x * sine + input.y * cosine;
  return output;
}
cv::Point2f acvRotation(float sine, float cosine, cv::Point2f input) { return acvRotation(sine, cosine, 1, input); }
cv::Point2f acvRotation(float angle, cv::Point2f input) { return acvRotation(sin(angle), cos(angle), 1, input); }

cv::Point2f acvIntersectPoint(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3, cv::Point2f p4)
{
  cv::Point2f intersec;
  float V1 = (p1.x - p2.x);
  float V2 = (p3.x - p4.x);
  float V3 = (p1.y - p2.y);
  float V4 = (p3.y - p4.y);
  float denominator = V1 * V4 - V3 * V2;
  float V12 = (p1.x * p2.y - p1.y * p2.x);
  float V34 = (p3.x * p4.y - p3.y * p4.x);
  intersec.x = (V12 * V2 - V1 * V34) / denominator;
  intersec.y = (V12 * V4 - V3 * V34) / denominator;
  return intersec;
}

cv::Point2f acvCircumcenter(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3)
{
  cv::Point2f c12; c12.x = (p1.x + p2.x) / 2; c12.y = (p1.y + p2.y) / 2;
  cv::Point2f c23; c23.x = (p2.x + p3.x) / 2; c23.y = (p2.y + p3.y) / 2;
  cv::Point2f N12; N12.y = (p1.x - p2.x); N12.x = -(p1.y - p2.y);
  cv::Point2f N23; N23.y = (p2.x - p3.x); N23.x = -(p2.y - p3.y);
  cv::Point2f c12_; c12_.x = c12.x + N12.x; c12_.y = c12.y + N12.y;
  cv::Point2f C23_; C23_.x = c23.x + N23.x; C23_.y = c23.y + N23.y;
  return acvIntersectPoint(c12, c12_, c23, C23_);
}

cv::Point2f acvVecNormal(cv::Point2f vec) { return cv::Point2f(-vec.y, vec.x); }

cv::Point2f acvVecNormalize(cv::Point2f vec)
{
  float dist = hypot(vec.x, vec.y);
  return cv::Point2f(vec.x / dist, vec.y / dist);
}

cv::Point2f acvVecInterp(cv::Point2f vec1, cv::Point2f vec2, float alpha)
{
  vec1.x += alpha * (vec2.x - vec1.x);
  vec1.y += alpha * (vec2.y - vec1.y);
  return vec1;
}

cv::Point2f acvVecAdd(cv::Point2f vec1, cv::Point2f vec2) { return cv::Point2f(vec1.x + vec2.x, vec1.y + vec2.y); }
cv::Point2f acvVecSub(cv::Point2f vec1, cv::Point2f vec2) { return cv::Point2f(vec1.x - vec2.x, vec1.y - vec2.y); }
cv::Point2f acvVecMult(cv::Point2f vec1, float mult)      { return cv::Point2f(vec1.x * mult, vec1.y * mult); }

cv::Point2f acvComplexAdd(cv::Point2f a, cv::Point2f b) { return acvVecAdd(a, b); }
cv::Point2f acvComplexSub(cv::Point2f a, cv::Point2f b) { return acvVecSub(a, b); }
cv::Point2f acvComplexMult(cv::Point2f a, cv::Point2f b) { return cv::Point2f(a.x*b.x - a.y*b.y, a.y*b.x + a.x*b.y); }
cv::Point2f acvComplexDiv(cv::Point2f a, cv::Point2f b)
{
  float deno = b.x * b.x + b.y * b.y;
  return cv::Point2f((a.x*b.x + a.y*b.y) / deno, (a.y*b.x - a.x*b.y) / deno);
}

float acvDistance(cv::Point2f p1, cv::Point2f p2) { return hypot(p2.x - p1.x, p2.y - p1.y); }
float acv2DCrossProduct(cv::Point2f v1, cv::Point2f v2) { return v1.x * v2.y - v2.x * v1.y; }
float acv2DDotProduct(cv::Point2f v1, cv::Point2f v2)   { return v1.x * v2.x + v2.y * v1.y; }

float acvVectorOrder(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3)
{
  cv::Point2f v1(p2.x - p1.x, p2.y - p1.y);
  cv::Point2f v2(p3.x - p2.x, p3.y - p2.y);
  return acv2DCrossProduct(v1, v2);
}

cv::Point2f acvLineIntersect(vis_Line line1, vis_Line line2)
{
  cv::Point2f p1 = line1.line_anchor;
  float d1 = acvDistance_Signed(line2, p1);
  cv::Point2f p2 = acvVecAdd(line1.line_anchor, line1.line_vec);
  float d2 = acvDistance_Signed(line2, p2);
  return acvVecInterp(p1, p2, d1 / (d1 - d2));
}

cv::Point2f acvClosestPointOnLine(cv::Point2f point, vis_Line line)
{
  line.line_vec = acvVecNormalize(line.line_vec);
  point.x -= line.line_anchor.x;
  point.y -= line.line_anchor.y;
  float dist = line.line_vec.x * point.x + line.line_vec.y * point.y;
  line.line_anchor.x += dist * line.line_vec.x;
  line.line_anchor.y += dist * line.line_vec.y;
  return line.line_anchor;
}

cv::Point2f acvClosestPointOnCircle(cv::Point2f point, vis_Circle circle)
{
  cv::Point2f vec2pt = acvVecSub(point, circle.circumcenter);
  float L = hypot(vec2pt.x, vec2pt.y);
  return acvVecAdd(circle.circumcenter, acvVecMult(vec2pt, circle.radius / L));
}

float acvDistance_Signed(vis_Circle cir, cv::Point2f point)
{
  // NOTE: preserves the legacy code's bug-or-feature: dy uses
  // circumcenter.x-point.y (not .y). Migration_gate is the source of truth;
  // changing this would shift values.
  float dist = hypot(cir.circumcenter.x - point.x, cir.circumcenter.x - point.y);
  return cir.radius - dist;
}

float acvDistance(vis_Circle cir, cv::Point2f point)
{
  float d = acvDistance_Signed(cir, point);
  return d < 0 ? -d : d;
}

float acvDistance_Signed(vis_Line line, cv::Point2f point)
{
  float a = -line.line_vec.y;
  float b = line.line_vec.x;
  float X0 = point.x - line.line_anchor.x;
  float Y0 = point.y - line.line_anchor.y;
  return (a * X0 + b * Y0) / hypot(a, b);
}

float acvDistance(vis_Line line, cv::Point2f point)
{
  float d = acvDistance_Signed(line, point);
  return d < 0 ? -d : d;
}

float acvVectorAngle(cv::Point2f v1, cv::Point2f v2)
{
  float ang = atan2(v1.x, v1.y) - atan2(v2.x, v2.y);
  if (ang > M_PI) ang -= 2 * M_PI;
  else if (ang <= -M_PI) ang += 2 * M_PI;
  return ang;
}

float acvLineAngle(vis_Line line1, vis_Line line2)
{
  float reg = hypot(line1.line_vec.x, line1.line_vec.y) * hypot(line2.line_vec.x, line2.line_vec.y);
  return acos((line1.line_vec.x * line2.line_vec.x + line1.line_vec.y * line2.line_vec.y) / reg);
}

bool acvFitLine(const void *pts_struct, int pts_step, const void *ptsw_struct, int ptsw_step, int ptsL, vis_Line *line, float *ret_sigma)
{
  vis_Line tarline = *line;
  if (ptsL < 2 || pts_struct == NULL || line == NULL) return false;

  float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0, wsum = 0;
  for (int i = 0; i < ptsL; i++)
  {
    float w = (ptsw_struct) ? (*(float *)((uint8_t *)ptsw_struct + i * ptsw_step)) : 1;
    cv::Point2f pt = *(cv::Point2f *)((uint8_t *)pts_struct + i * pts_step);
    sumX += pt.x * w; sumY += pt.y * w;
    sumXY += pt.x * pt.y * w;
    sumX2 += pt.x * pt.x * w; sumY2 += pt.y * pt.y * w;
    wsum += w;
  }
  float xMean = sumX / wsum;
  float yMean = sumY / wsum;
  float denominatorX = sumX2 - sumX * xMean;
  float denominatorY = sumY2 - sumY * yMean;

  float denominator = 0;
  if (fabs(denominatorX) > fabs(denominatorY))
  {
    line->line_vec.y = (sumXY - sumX * yMean);
    line->line_vec.x = denominatorX;
    denominator = hypot(line->line_vec.x, line->line_vec.y);
  }
  else
  {
    line->line_vec.y = denominatorY;
    line->line_vec.x = (sumXY - sumY * xMean);
    denominator = hypot(line->line_vec.x, line->line_vec.y);
  }
  if (denominator < 1e-7) return false;

  line->line_vec.x /= denominator;
  line->line_vec.y /= denominator;
  if ((line->line_vec.x * tarline.line_vec.x + line->line_vec.y * tarline.line_vec.y) < 0)
  {
    line->line_vec.x = -line->line_vec.x;
    line->line_vec.y = -line->line_vec.y;
  }
  line->line_anchor.x = xMean;
  line->line_anchor.y = yMean;

  if (ret_sigma)
  {
    float sigma = 0; wsum = 0;
    for (int i = 0; i < ptsL; i++)
    {
      float w = (ptsw_struct) ? (*(float *)((uint8_t *)ptsw_struct + i * ptsw_step)) : 1;
      cv::Point2f pt = *(cv::Point2f *)((uint8_t *)pts_struct + i * pts_step);
      float d = acvDistance_Signed(*line, pt);
      sigma += d * d * w; wsum += w;
    }
    *ret_sigma = sqrt(sigma / wsum);
  }
  return true;
}

bool acvFitLine(const cv::Point2f *pts, const float *ptsw, int ptsL, vis_Line *line, float *ret_sigma)
{
  return acvFitLine(pts, sizeof(cv::Point2f), ptsw, sizeof(float), ptsL, line, ret_sigma);
}

bool acvFitLine(const cv::Point2f *pts, int ptsL, vis_Line *line, float *ret_sigma)
{
  return acvFitLine(pts, NULL, ptsL, line, ret_sigma);
}
