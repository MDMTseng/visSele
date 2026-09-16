// Times the SBM match path alone -- no core, no websocket, no camera. Exists to
// compare the two ISA halves (SBM_FORCE_ISA=avx2 vs base) on the same machine and
// the same image, which the full-core bench cannot do cleanly: there the match
// is a minority of a frame that also carries capture, decode and judging.
//
//   sbm_bench <scene.png> [iters] [angle_step]
//
// Reports median and p95 of match() alone. Template is a centred crop of the
// scene, so the run is self-contained and identical across builds -- this is a
// speed probe, not a recipe: absolute numbers mean nothing outside this
// comparison, the RATIO between two builds is the whole point.
#include <opencv2/opencv.hpp>
#include "shape_matcher.h"
#include "detector_iface.h"
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: sbm_bench <scene.png> [iters] [angle_step]\n"); return 2; }
  const int iters = (argc > 2) ? std::atoi(argv[2]) : 20;
  const float step = (argc > 3) ? (float)std::atof(argv[3]) : 2.0f;

  cv::Mat scene = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
  if (scene.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

  // A centred crop, half the scene in each dimension, is the template.
  const cv::Rect roi(scene.cols / 4, scene.rows / 4, scene.cols / 2, scene.rows / 2);
  cv::Mat templ = scene(roi).clone();

  sbm::MatchConfig mc;
  mc.refine = sbm::RefineMode::ICP;
  sbm::ModelConfig gc;
  gc.angle.step = step;

  sbm::ShapeMatcher m(mc);
  auto t_add = std::chrono::steady_clock::now();
  m.addModel("t", templ, cv::Mat(), gc);
  const double add_ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - t_add).count();

  std::printf("ISA %s\n", line2Dup::activeISA());
  std::printf("scene %dx%d  templ %dx%d  angle_step %.1f  addModel %.0f ms\n",
              scene.cols, scene.rows, templ.cols, templ.rows, step, add_ms);

  std::vector<double> ms;
  int found = 0;
  for (int i = 0; i < iters; i++) {
    auto t0 = std::chrono::steady_clock::now();
    auto r = m.match(scene);
    ms.push_back(std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count());
    if (i == 0) found = (int)r.size();
  }
  // Print the pose itself, not just the count: comparing two ISA builds is the
  // reason this exists, and "1 result" is true of a wrong answer too.
  {
    auto r = m.match(scene);
    for (auto &x : r)
      std::printf("  result: x=%.4f y=%.4f angle=%.4f scale=%.4f score=%.4f\n",
                  x.x, x.y, x.angle, x.scale, x.score);
  }
  std::sort(ms.begin(), ms.end());
  std::printf("match x%d: median %.2f ms  p95 %.2f ms  min %.2f ms  (%d results)\n",
              iters, ms[ms.size() / 2], ms[(size_t)(ms.size() * 0.95)], ms.front(), found);
  return 0;
}
