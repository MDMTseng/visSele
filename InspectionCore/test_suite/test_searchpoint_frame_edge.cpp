// Does an object near the frame edge make search_point_cv invent an edge?
//
// SearchPointCV rectifies a band around the search point and fills any sample
// that falls outside the image with 0. A 0<->bright step is a full-scale
// gradient, so if one ever reaches the Sobel it competes with -- and on a
// low-contrast feature beats -- the real edge. The guard is the `valid` mask.
//
// The bug this pins: the Sobel reads rows i-1, i and i+1, but the guard used to
// check only row i.
//
// Whether that gap is reachable depends on the angle between the scan and the
// frame edge, and the reason is worth stating because it is not the obvious
// one. In the rectified band, row index i steps along searchDir and column
// index j steps along its perpendicular; the Sobel differences and the guard
// both run along j. When searchDir is PARALLEL to the frame edge being crossed,
// a given column is at a constant distance from that edge, so going out of
// frame happens whole-columns-at-a-time -- exactly the axis the 1x3 guard
// covers, and nothing leaks. When searchDir is SLANTED, the frame boundary cuts
// across the band diagonally, so row i can be fully inside while row i-1 is
// partly outside; the guard looks only at row i, passes, and the 0-filled
// samples in row i-1 reach the Sobel.
//
// Measured before the fix, on a UNIFORM image with no feature anywhere:
//   slanted 10/25/45 deg -> ok=1, y = 198.02 / 198.13 / 197.78 on a 200px-tall
//   image, strength 200 -- an edge reported exactly on the frame boundary.
// After the fix all three report nothing, and the real-edge cases are
// unchanged to the digit (--insp leaf diff over test1: 959 leaves, 0 differ).
//
// AXES -- read this before changing a parameter, it is genuinely confusing:
// the `searchDir` argument is NOT the direction that gets scanned. Per the
// caller (FeatureManager_sig360_circle_line.cpp:1146) it is the BAR axis along
// which the parallel scan columns are laid out; the actual scan runs along its
// PERPENDICULAR. So `width` is the number of columns laid along searchDir, and
// `margin` is the half-depth of the scan across it. SearchPointCV's own locals
// make this worse by naming the searchDir-aligned rows "search-depth". The
// header comment is right; the internal naming is not.
//   searchDir (1,0) => columns along x, scanning along y (vertical scan)
//
// BUILD (MSYS2 MinGW64; the include/lib set is fiddly, hence spelling it out):
//   export PATH=/c/msys64/mingw64/bin:$PATH
//   cd InspectionCore
//   g++ -std=c++17 -O2 -DFEATURE_OPENCV -o /tmp/t.exe \
//     test_suite/test_searchpoint_frame_edge.cpp \
//     -IMatchingEngine/include -IMatchingEngine/include_priv \
//     -IMatchingEngine/MorphEngine/include -IacvImage/include \
//     -Icommon_lib/include -Icontrib/cJSON -Ilogctrl/include \
//     -ICameraLayer -ICameraLayer/include $(pkg-config --cflags opencv4) \
//     -Lbuild/nohik-cv4 -lMatchingEngine -lacvImage -lcJSON -lcommon_lib \
//     -lCircleFitting -llogctrl -lpolyfit -lCameraLayer $(pkg-config --libs opencv4)
//   /tmp/t.exe    # exit 0 = pass
#include <opencv2/opencv.hpp>
#include "SearchPointCV.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

// Run one search and report. `expectHit`: -1 = must find nothing, otherwise the
// y the hit must land on (within tol).
static void probe(const char *name, const cv::Mat &img, acv_XY pt, acv_XY dir,
                  float margin, float width, float expectY, float tol)
{
  acv_XY out{}; float w = 0;
  std::vector<CaliperHit> hits;
  bool ok = search_point_cv(img, pt, dir, margin, width, SP_BOTH,
                            0.f, 2.f, 0.5f, nullptr,
                            cv::Mat(), 0, &out, &w, -1, &hits);
  bool pass;
  char detail[160];
  if (expectY < 0)
  {
    pass = !ok;
    snprintf(detail, sizeof detail, "expected NO hit, got ok=%d%s", (int)ok,
             ok ? "" : "");
    if (ok) snprintf(detail, sizeof detail,
                     "FABRICATED edge at (%.2f,%.2f) w=%.0f from %zu hits",
                     out.x, out.y, w, hits.size());
  }
  else
  {
    float err = ok ? std::fabs(out.y - expectY) : 1e9f;
    pass = ok && err <= tol;
    snprintf(detail, sizeof detail, "y=%.2f (want %.1f +/-%.1f) err=%.2f",
             ok ? out.y : -1.f, expectY, tol, ok ? err : -1.f);
  }
  printf("  [%s] %-44s %s\n", pass ? "PASS" : "FAIL", name, detail);
  if (!pass) failures++;
}

int main()
{
  const int W = 400, H = 200;

  // Case group 1: a real edge exists at y=150. Everything below is bright, so
  // the frame's bottom row is itself a bright->offimage step -- the decoy.
  cv::Mat img(H, W, CV_8U, cv::Scalar(30));
  cv::rectangle(img, cv::Rect(0, 150, W, H - 150), cv::Scalar(220), cv::FILLED);
  printf("real edge at y=150, frame bottom at y=%d:\n", H - 1);
  probe("vertical scan, runs off bottom",  img, {200, 170}, {1, 0}, 60, 20, 150.f, 3.f);
  for (float deg : {25.f, 45.f})
  {
    float r = deg * (float)CV_PI / 180.f;
    char nm[80]; snprintf(nm, sizeof nm, "slanted scan %2.0f deg, off bottom", deg);
    probe(nm, img, {200, 170}, {std::cos(r), std::sin(r)}, 60, 20, 150.f, 3.f);
  }

  // Case group 2: nothing to find. Any hit at all is invented, and before the
  // fix every slanted one landed on the frame boundary.
  cv::Mat flat(H, W, CV_8U, cv::Scalar(200));
  printf("uniform image, no feature anywhere -- any hit is fabricated:\n");
  probe("uniform, vertical scan, off bottom", flat, {200, 190}, {1, 0}, 40, 20, -1.f, 0.f);
  for (float deg : {10.f, 25.f, 45.f})
  {
    float r = deg * (float)CV_PI / 180.f;
    char nm[80]; snprintf(nm, sizeof nm, "uniform, slanted %2.0f deg, off bottom", deg);
    probe(nm, flat, {200, 190}, {std::cos(r), std::sin(r)}, 40, 20, -1.f, 0.f);
  }

  // Case group 3: the same against the TOP and the two side edges, since the
  // fill is symmetric and nothing about the bug is specific to y.
  printf("other three frame edges, uniform image:\n");
  probe("uniform, off top",   flat, {200,  10}, { 0.95f, 0.30f}, 40, 20, -1.f, 0.f);
  probe("uniform, off left",  flat, { 10, 100}, { 0.30f, 0.95f}, 40, 20, -1.f, 0.f);
  probe("uniform, off right", flat, {390, 100}, { 0.30f, 0.95f}, 40, 20, -1.f, 0.f);

  printf("\n%s (%d failure%s)\n", failures ? "FAIL" : "ALL PASS",
         failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
