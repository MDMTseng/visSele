// Emit the geometry contract the WebUI has to match, computed by the CORE.
//
//   export PATH=/c/msys64/mingw64/bin:$PATH
//   cd InspectionCore
//   g++ -std=c++17 -O2 -DFEATURE_OPENCV -o build/geom_emit.exe \
//     test_suite/geom_vectors_emit.cpp \
//     -IMatchingEngine/include -IMatchingEngine/include_priv \
//     -IMatchingEngine/MorphEngine/include -IacvImage/include \
//     -Icommon_lib/include -Icontrib/cJSON -Ilogctrl/include \
//     -ICameraLayer -ICameraLayer/include $(pkg-config --cflags opencv4) \
//     -Lbuild/win-mingw-msys -lMatchingEngine -lacvImage -lcJSON -lcommon_lib \
//     -lCircleFitting -llogctrl -lpolyfit -lCameraLayer $(pkg-config --libs opencv4)
//   ./build/geom_emit.exe > test_suite/geom_vectors.json
//
// WHY THIS EXISTS
// ---------------
// Some geometry is necessarily implemented twice: at EDIT time the core has not
// run, so the editor has to predict what the machine will do. That second
// implementation then drifts, silently, and the screen ends up arguing against
// the machine on exactly the defs where somebody is already confused.
//
// Measured on 2026-08-26, before this file existed:
//   * arc caliper width was seeded from the COMPLEMENT of the arc -- 11.00x too
//     wide -- while the drawing code a few dozen lines away had the correct port
//     of convert3Pts2ArcData. Three copies, one right.
//   * degenerate caliper params disagreed in 15 of 24 swept cases.
//
// Extracting each side into one function makes drift REVIEWABLE. It does not
// make it impossible: the correct version of the arc sweep was already sitting
// in the same file as the three wrong ones. What makes it impossible to ship
// quietly is a shared expectation that FAILS A TEST, which is what this file
// produces.
//
// The vectors are computed by calling the core's own convert3Pts2ArcData, not
// by transcribing it here. A transcription would be a fourth copy, and would
// agree with the UI right up to the moment the core changed.
//
// ON TOLERANCE
// ------------
// The core computes in `float`, the WebUI in JavaScript `double`. These cannot
// be compared for equality and it would be wrong to try: the contract is the
// GEOMETRY, not the rounding. The runner uses an absolute tolerance, and the
// emitter records it here so both sides quote the same number.
#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <vector>

#include "vis_geom.h"   // acv_XY (= cv::Point2f) and acv_Circle
#include "Caliper.h"    // the caliper contract: defaults, caps, per-primitive floor

// convert3Pts2ArcData is a free function in FeatureManager_sig360_circle_line.cpp
// and is in no header. Declared rather than copied -- see the note above about
// what a fourth copy would be worth.
typedef struct arc_data
{
  acv_XY pt1, pt2, pt3;
  acv_Circle circleTar;
  float sAngle;
  float eAngle;
} arc_data;
arc_data convert3Pts2ArcData(acv_XY pt1, acv_XY pt2, acv_XY pt3);

static acv_XY at(double deg, double r, double cx = 0, double cy = 0)
{
  const double rad = deg * M_PI / 180.0;
  return acv_XY((float)(cx + r * cos(rad)), (float)(cy + r * sin(rad)));
}

// The span the CALLER derives from sAngle/eAngle. caliper_locate_circle does
// exactly this (Caliper.cpp), so it is part of the contract even though
// convert3Pts2ArcData does not return it.
static double spanOf(const arc_data &a)
{
  double span = (double)a.eAngle - (double)a.sAngle;
  while (span < 0) span += 2 * M_PI;
  while (span >= 2 * M_PI) span -= 2 * M_PI;
  return span;
}

struct Case {
  const char *name; acv_XY p1, p2, p3;
  double tol = 0;           // 0 = use the global tolerance
  const char *note = "";    // why this one needs its own
};

int main()
{
  std::vector<Case> cases;

  // The bug this whole file is named after: the same arc, clicked both ways.
  cases.push_back({ "30deg ccw",        at(0,10),   at(15,10),  at(30,10)  });
  cases.push_back({ "30deg cw",         at(30,10),  at(15,10),  at(0,10)   });
  // pt2 on the long way round -- genuinely the major arc, must stay 330.
  cases.push_back({ "major via pt2",    at(0,10),   at(180,10), at(30,10)  });
  cases.push_back({ "major via pt2 cw", at(30,10),  at(180,10), at(0,10)   });
  // Wrapping across the atan2 discontinuity, both directions.
  cases.push_back({ "wraps -pi",        at(170,10), at(180,10), at(190,10) });
  cases.push_back({ "wraps -pi cw",     at(190,10), at(180,10), at(170,10) });
  // Half turns, where angle21 and angle31 are closest to each other.
  cases.push_back({ "180deg",           at(0,10),   at(90,10),  at(180,10) });
  cases.push_back({ "270deg",           at(0,10),   at(135,10), at(270,10) });
  cases.push_back({ "350deg",           at(0,10),   at(175,10), at(350,10) });
  // Off-origin and a different radius: the centre must not be assumed.
  cases.push_back({ "offset centre",    at(20,7,30,-12), at(50,7,30,-12), at(80,7,30,-12) });
  cases.push_back({ "small radius",     at(0,0.4), at(40,0.4), at(80,0.4) });
  cases.push_back({ "large radius",     at(0,500), at(1,500),  at(2,500)  });
  // Near-degenerate: nearly collinear. Included precisely because it is where
  // the two implementations are most likely to part company.
  // Deliberately ill-conditioned, and kept BOUNDED rather than passed or
  // dropped. Three points 20 apart with a 0.001 sagitta give r ~ 50000, where
  // the circumcentre is a subtraction of nearly equal large numbers and float
  // and double genuinely part company -- measured 8.3 units apart at the
  // centre, while the anchors stay within 2.8e-3.
  //
  // It carries its own tolerance so the runner reports it as a known limit
  // WITH A NUMBER, because a permanently red test is one people learn to
  // ignore, and widening the global tolerance to cover it would hide exactly
  // the drift this file exists to catch.
  cases.push_back({ "near collinear",   acv_XY(0,0), acv_XY(10,0.001f), acv_XY(20,0), 5e-3,
                    "ill-conditioned circumcentre: both sides draw the same arc, they disagree about its centre" });

  printf("{\n");
  printf("  \"_\": \"GENERATED by test_suite/geom_vectors_emit.cpp -- do not hand-edit.\",\n");
  printf("  \"_source\": \"core convert3Pts2ArcData, called directly\",\n");
  printf("  \"tolerance\": { \"abs\": 1e-4, \"why\": \"core is float, WebUI is double; the contract is the geometry, not the rounding\" },\n");
  printf("  \"arcSweep\": [\n");
  for (size_t i = 0; i < cases.size(); i++)
  {
    const Case &c = cases[i];
    arc_data a = convert3Pts2ArcData(c.p1, c.p2, c.p3);
    printf("    { \"name\": \"%s\",\n", c.name);
    if (c.tol > 0)
      printf("      \"tol\": %g, \"note\": \"%s\",\n", c.tol, c.note);
    printf("      \"pt1\": [%.9g, %.9g], \"pt2\": [%.9g, %.9g], \"pt3\": [%.9g, %.9g],\n",
           c.p1.x, c.p1.y, c.p2.x, c.p2.y, c.p3.x, c.p3.y);
    printf("      \"cx\": %.9g, \"cy\": %.9g, \"r\": %.9g,\n",
           a.circleTar.circumcenter.x, a.circleTar.circumcenter.y, a.circleTar.radius);
    printf("      \"sAngle\": %.9g, \"eAngle\": %.9g, \"span\": %.9g, \"length\": %.9g,\n",
           a.sAngle, a.eAngle, spanOf(a), spanOf(a) * a.circleTar.radius);
    // WHERE THE CALIPERS LAND, which is the contract that matters.
    //
    // Comparing cx/cy/r compares PARAMETERS, and those are ill-conditioned:
    // three nearly-collinear points have a circumcentre that float and double
    // disagree about by a wide margin -- measured 8.3 units out of 50000 on the
    // near-collinear case here. But two circles whose centres differ by that
    // much can still pass through nearly the same points across the span that
    // is actually used, and the POINTS are where the boxes get drawn and the
    // measurement gets taken.
    //
    // Same formula as caliper_locate_circle: a = angStart + span*i/(count-1).
    printf("      \"anchors\": [");
    const int N = 5;
    for (int k = 0; k < N; k++)
    {
      double t = (double)k / (N - 1);
      double ang = (double)a.sAngle + spanOf(a) * t;
      printf("%s[%.9g, %.9g]", k ? ", " : "",
             a.circleTar.circumcenter.x + a.circleTar.radius * cos(ang),
             a.circleTar.circumcenter.y + a.circleTar.radius * sin(ang));
    }
    printf("] }%s\n", (i + 1 == cases.size()) ? "" : ",");
  }
  printf("  ],\n");

  // --- caliper resolution ------------------------------------------------
  //
  // Emitted from the SAME constants and the SAME floor function the parser and
  // the locate functions use (Caliper.h). Transcribing the rule here would have
  // produced a file that agrees with the WebUI and with nothing else.
  printf("  \"resolveCaliper\": [\n");
  struct CalCase { const char *name; bool hasObj, hasCount, hasWidth; double count, width; };
  CalCase cc[] = {
    { "object absent",   false, false, false, 0, 0 },
    { "object empty",    true,  false, false, 0, 0 },
    { "count 0",         true,  true,  true,  0,    1 },
    { "count 1",         true,  true,  true,  1,    1 },
    { "count 2",         true,  true,  true,  2,    1 },
    { "count 3",         true,  true,  true,  3,    1 },
    { "count 10",        true,  true,  true,  10,   1 },
    { "count over cap",  true,  true,  true,  9999, 1 },
    { "count negative",  true,  true,  true,  -5,   1 },
    { "width absent",    true,  true,  false, 10,   0 },
    { "width 0",         true,  true,  true,  10,   0 },
    { "width over cap",  true,  true,  true,  10,   999 },
  };
  const int nCC = (int)(sizeof(cc)/sizeof(cc[0]));
  for (int i = 0; i < nCC; i++)
    for (int lineNotArc = 1; lineNotArc >= 0; lineNotArc--)
    {
      const int minCount = lineNotArc ? CALIPER_MIN_COUNT_LINE : CALIPER_MIN_COUNT_ARC;
      // parse stage, exactly as parse_arcData / parse_lineData do it
      int   count = CALIPER_PARSE_DEFAULT_COUNT;
      float width = CALIPER_PARSE_DEFAULT_WIDTH;
      if (cc[i].hasObj)
      {
        count = cc[i].hasCount ? (int)cc[i].count : CALIPER_PARSE_DEFAULT_COUNT;
        width = cc[i].hasWidth ? (float)cc[i].width : CALIPER_PARSE_DEFAULT_WIDTH;
        if (count > CALIPER_MAX_COUNT) count = CALIPER_MAX_COUNT;
        if (width > CALIPER_MAX_WIDTH) width = CALIPER_MAX_WIDTH;
      }
      // execute stage, via the function the locate functions themselves call
      count = caliper_effective_count(count, minCount);
      printf("    { \"name\": \"%s\", \"primitive\": \"%s\", \"minCount\": %d,\n",
             cc[i].name, lineNotArc ? "line" : "arc", minCount);
      printf("      \"in\": ");
      if (!cc[i].hasObj) printf("null");
      else {
        printf("{");
        if (cc[i].hasCount) printf("\"count\": %g", cc[i].count);
        if (cc[i].hasCount && cc[i].hasWidth) printf(", ");
        if (cc[i].hasWidth) printf("\"width\": %g", cc[i].width);
        printf("}");
      }
      printf(",\n      \"count\": %d, \"width\": %.9g }%s\n",
             count, width,
             (i + 1 == nCC && lineNotArc == 0) ? "" : ",");
    }
  printf("  ]\n}\n");
  
  return 0;
}
