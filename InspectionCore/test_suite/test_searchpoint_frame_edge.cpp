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
                            &out, &w, -1, &hits);
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

  // Case group 4: a REAL step at the frame boundary, competing with a real
  // object edge. A DIFFERENT bug from groups 1-3, and their fix does not touch
  // it -- those cases still pass alongside this one.
  //
  // Groups 1-3 are about a FABRICATED edge: samples outside the image filled
  // with 0, reaching the Sobel through a guard that only checked the middle
  // row. Fixed, and it stays fixed.
  //
  // What gets reported from the machine is not fabricated. A real frame has a
  // real step near its border -- the ROI crop, the edge of the backlight, a
  // vignette -- and it is often the STRONGEST gradient in the band. The scan
  // takes the FIRST hit along its direction, so once the part drifts close
  // enough to the border, the border is what the band meets first and the
  // measurement moves onto it. Nothing is invented; the wrong REAL edge wins.
  //
  // The sweep is the point of this group. "Sometimes it steals the edge" is not
  // actionable; "it steals it once the object edge is within N px of the
  // border" is. The number is printed so a change to the scan can be compared
  // against it. The ASSERTION is only on the far cases: pinning the near ones
  // would freeze today's behaviour as correct, and whether it is correct is the
  // open question this test exists to make measurable.
  {
    printf("border step vs object edge -- which one the scan lands on:%c", 10);
    const int BW = 400, BH = 200;
    int flip = -1;
    // TWO start positions per gap, because which edge is "first" depends on
    // where the band is centred, and that is exactly what moves when a part
    // drifts towards the frame edge:
    //   outside  the search point sits beyond the part, scanning inward -- the
    //            normal case, and the part is met first
    //   inside   the search point sits BETWEEN the part and the border, which
    //            is what a pose placed on a part near the edge produces. Now
    //            the border is the first thing the band meets.
    for (int gap = 40; gap >= 4; gap -= 4)
    {
      cv::Mat im(BH, BW, CV_8U, cv::Scalar(210));
      const int borderY = BH - 6;
      cv::rectangle(im, cv::Rect(0, borderY, BW, BH - borderY),
                    cv::Scalar(15), cv::FILLED);
      const int objY = borderY - gap;
      cv::rectangle(im, cv::Rect(0, objY, BW, 3), cv::Scalar(60), cv::FILLED);

      for (int side = 0; side < 2; side++)
      {
        const float startY = (side == 0) ? (float)(objY - 25)
                                         : (float)(objY + gap / 2);
        acv_XY out{}; float w = 0;
        std::vector<CaliperHit> hits;
        const bool ok = search_point_cv(im, {200.f, startY}, {1, 0},
                                        45.f, 20.f, SP_BOTH, 0.f, 2.f, 0.5f,
                                        nullptr, &out, &w, -1, &hits);
        const bool onObject = ok && std::fabs(out.y - objY) <= 4.f;
        const bool onBorder = ok && std::fabs(out.y - borderY) <= 4.f;
        // hits.size() is the count that matters, not just where the answer
        // landed: a caliper that measures the right edge from 3 of its 19 scan
        // lines has lost its statistical basis and still reports SUCCESS. The
        // live bench showed exactly that -- id=7 fell from 19 hits on the full
        // frame to 3 on the ROI-cropped one, status 0 both times, and the
        // measurement moved 0.2mm.
        printf("    gap %2dpx %-7s start y=%3.0f: %s y=%.1f hits=%2zu (object %d, border %d)%s%c",
               gap, side == 0 ? "outside" : "inside", startY,
               ok ? "hit " : "MISS", ok ? out.y : -1.f, hits.size(), objY, borderY,
               onBorder ? "   <-- BORDER WON" : (onObject ? "" : "   <-- neither"),
               10);
        if (onBorder && flip < 0) flip = gap;
        // Asserted only for the outside start with the part well clear. The
        // inside start is the case under investigation; pinning it would freeze
        // today's answer as the correct one, and that is the open question.
        if (side == 0 && gap >= 24)
        {
          char nm[80]; snprintf(nm, sizeof nm, "gap %dpx outside: measures the object", gap);
          printf("  [%s] %-44s y=%.1f want %d%c", onObject ? "PASS" : "FAIL",
                 nm, ok ? out.y : -1.f, objY, 10);
          if (!onObject) failures++;
        }
      }
    }
    if (flip >= 0)
      printf("  NOTE: the border wins from gap %dpx and closer.%c", flip, 10);
    else
      printf("  NOTE: the border never won in this sweep.%c", 10);
  }


  printf("\n%s (%d failure%s)\n", failures ? "FAIL" : "ALL PASS",
         failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
