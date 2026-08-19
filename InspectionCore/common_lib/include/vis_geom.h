#ifndef VIS_GEOM_H
#define VIS_GEOM_H

// 2D geometry primitives for the inspection engine. Phase 3b extracts these
// from the legacy `acvImage/` translation units so the acvImage class can be
// dropped from the build entirely.
//
// `cv::Point2f` (`.x`, `.y`) replaces the old `acv_XY` struct. Line / Circle
// / fit / labeled-region structs stay custom because cv::Vec[34]f's
// positional `[0]/[1]/...` access reads worse than named fields here.

#include <cstdint>
#include <opencv2/core.hpp>
#include <vector>

// Transitional alias so the in-tree consumers can keep using `acv_XY`
// while migrating to `cv::Point2f`. Same struct under the hood.
using acv_XY        = cv::Point2f;
struct vis_Line; struct vis_Circle; struct vis_CircleFit;
struct vis_LineFit; struct vis_LabeledData;
using acv_Line      = vis_Line;
using acv_Circle    = vis_Circle;
using acv_CircleFit = vis_CircleFit;
using acv_LineFit   = vis_LineFit;
using acv_LabeledData = vis_LabeledData;

struct vis_Line {
  cv::Point2f line_vec;
  cv::Point2f line_anchor;
};

struct vis_Circle {
  cv::Point2f circumcenter;
  float radius;
};

struct vis_CircleFit {
  vis_Circle circle;
  int matching_pts;
  float s;             // sigma
  float confidence;    // mean inlier edge confidence (caliper path; 0 = N/A)
};

struct vis_LineFit {
  vis_Line line;
  int matching_pts;
  cv::Point2f end_pt1;
  cv::Point2f end_pt2;
  float s;
  float confidence;
};

struct vis_LabeledData {
  cv::Point2f LTBound;
  cv::Point2f RBBound;
  cv::Point2f Center;
  int area;
  int misc;
};

// Geometry helpers (function names preserved per user direction; only the
// struct types are renamed). Implementations live in common_lib/vis_geom.cpp.
double acvFAtan2(double y, double x);
double acvFAtan(double x);

cv::Point2f acvIntersectPoint(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3, cv::Point2f p4);
cv::Point2f acvCircumcenter(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3);
float acv2DCrossProduct(cv::Point2f v1, cv::Point2f v2);
float acv2DDotProduct(cv::Point2f v1, cv::Point2f v2);
float acvVectorOrder(cv::Point2f p1, cv::Point2f p2, cv::Point2f p3);
float acvDistance(cv::Point2f p1, cv::Point2f p2);
cv::Point2f acvVecNormal(cv::Point2f vec);
cv::Point2f acvVecNormalize(cv::Point2f vec);
cv::Point2f acvVecInterp(cv::Point2f vec1, cv::Point2f vec2, float alpha);
cv::Point2f acvVecAdd(cv::Point2f vec1, cv::Point2f vec2);
cv::Point2f acvVecSub(cv::Point2f vec1, cv::Point2f vec2);
cv::Point2f acvVecMult(cv::Point2f vec1, float mult);

cv::Point2f acvComplexAdd(cv::Point2f a, cv::Point2f b);
cv::Point2f acvComplexSub(cv::Point2f a, cv::Point2f b);
cv::Point2f acvComplexMult(cv::Point2f a, cv::Point2f b);
cv::Point2f acvComplexDiv(cv::Point2f a, cv::Point2f b);
cv::Point2f acvRotation(float sine, float cosine, float flip_f, cv::Point2f input);
cv::Point2f acvRotation(float sine, float cosine, cv::Point2f input);
cv::Point2f acvRotation(float angle, cv::Point2f input);

cv::Point2f acvLineIntersect(vis_Line line1, vis_Line line2);
cv::Point2f acvClosestPointOnLine(cv::Point2f point, vis_Line line);
cv::Point2f acvClosestPointOnCircle(cv::Point2f point, vis_Circle circle);
float acvDistance_Signed(vis_Line line, cv::Point2f point);
float acvDistance(vis_Line line, cv::Point2f point);
float acvDistance_Signed(vis_Circle cir, cv::Point2f point);
float acvDistance(vis_Circle cir, cv::Point2f point);
float acvLineAngle(vis_Line line1, vis_Line line2);
float acvVectorAngle(cv::Point2f v1, cv::Point2f v2);
bool acvFitLine(const cv::Point2f *pts, int ptsL, vis_Line *line, float *ret_sigma);
bool acvFitLine(const cv::Point2f *pts, const float *ptsw, int ptsL, vis_Line *line, float *ret_sigma);
bool acvFitLine(const void *pts_struct, int pts_step, const void *ptsw_struct, int ptsw_step, int ptsL, vis_Line *line, float *ret_sigma);


// 24-bit label pixel, moved here from acvImage_ComponentLabelingTool.hpp when
// acvImage was retired. A labeled image stores each label as 3 bytes; this
// union is how the engine reads those 3 bytes back as one integer. Nothing to
// do with the old image container -- it is part of how vis_LabeledData's
// labels are represented on the wire.
typedef struct _3BYTE { unsigned Num : 24; } _3BYTE;
typedef struct _2BYTE { uint16_t Num; uint8_t Empty; } _2BYTE;
typedef struct BYTE3  { uint8_t Num2; uint8_t Num1; uint8_t Num0; } BYTE3;
typedef union _24BitUnion {
  BYTE3  Byte3;
  _3BYTE _3Byte;
  _2BYTE _2Byte;
} _24BitUnion;

#endif
