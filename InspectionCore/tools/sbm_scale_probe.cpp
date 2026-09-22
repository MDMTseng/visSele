// What arbitrary scaling of SBM template features actually costs.
//
//   sbm_scale_probe <template.png> [scene.png]
//
// Two questions, measured separately because they have different answers:
//
//   1. GEOMETRY. addModelAtScaleTo scales an existing feature set by
//      multiplying coordinates and rounding -- f.x = (int)(f.x*s + 0.5) -- with
//      no re-selection and no de-duplication. So: how many features land on a
//      pixel another feature already holds, and how far does each one move from
//      where it should be? Those are paid for in discrimination, not time: a
//      collapsed feature still costs a full pass over the response map while
//      adding the same response twice.
//
//   2. TIME. The match loop is one vectorised pass over the response map PER
//      FEATURE, and features are independent -- there is no ordering or packing
//      for scaling to disturb. What scaling does change is the number of
//      TEMPLATES, because every scale gets a full set of angles. This times a
//      scale list against a single scale to show where the cost actually is.
#include <opencv2/opencv.hpp>
#include "shape_matcher.h"
#include "detector_iface.h"
#include <chrono>
#include <cstdio>
#include <set>
#include <vector>
#include <cmath>
#include <algorithm>

static void geometry(const sbm::FeatureSet &fs, float s) {
  // Level 0 is the one the fine stage scores on.
  if (fs.levels.empty()) return;
  const auto &lv = fs.levels[0];
  std::set<std::pair<int, int>> seen;
  int collapsed = 0;
  double sum = 0, worst = 0;
  for (const auto &f : lv.features) {
    const float ex = f.x * s, ey = f.y * s;        // where it should be
    const int rx = (int)(ex + 0.5f), ry = (int)(ey + 0.5f);  // where it lands
    if (!seen.insert({rx, ry}).second) collapsed++;
    const double d = std::hypot(rx - ex, ry - ey);
    sum += d;
    worst = std::max(worst, d);
  }
  // WHERE THE FEATURES ACTUALLY ARE, and how close the closest pair is. This is
  // what decides whether scaling can merge two of them: they collide when the
  // nearest-pair distance times the scale falls under a pixel. Printed because
  // "no collapse" is only meaningful next to the spacing that produced it.
  int minx=1<<30,maxx=-(1<<30),miny=1<<30,maxy=-(1<<30);
  double nearest=1e18;
  for (size_t a=0;a<lv.features.size();++a){
    minx=std::min(minx,lv.features[a].x); maxx=std::max(maxx,lv.features[a].x);
    miny=std::min(miny,lv.features[a].y); maxy=std::max(maxy,lv.features[a].y);
    for (size_t b=a+1;b<lv.features.size();++b){
      const double d=std::hypot(lv.features[a].x-lv.features[b].x,
                                lv.features[a].y-lv.features[b].y);
      nearest=std::min(nearest,d);
    }
  }
  const int n = (int)lv.features.size();
  if (s == 1.0f)
    std::printf("  [level 0 spans %dx%d px, nearest pair %.1f px -> merging needs scale < %.3f]\n",
                maxx-minx, maxy-miny, nearest, nearest>0 ? 1.0/nearest : 0.0);
  std::printf("  s=%.2f  features %3d  collapsed %3d (%4.1f%%)  "
              "jitter mean %.2f px  max %.2f px  (in original pixels: %.2f)\n",
              s, n, collapsed, n ? 100.0 * collapsed / n : 0.0,
              n ? sum / n : 0.0, worst, (n ? sum / n : 0.0) / (s > 0 ? s : 1));
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: sbm_scale_probe <template.png> [scene.png]\n");
    return 2;
  }
  cv::Mat scene = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
  if (scene.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
  if (argc > 2) {
    cv::Mat s2 = cv::imread(argv[2], cv::IMREAD_GRAYSCALE);
    if (s2.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    scene = s2;
  }
  // A CENTRED CROP, not the whole picture. A template the size of the scene
  // leaves almost nowhere to search, so the per-position work barely happens
  // and the timing stops being about matching at all.
  const cv::Rect roi(scene.cols / 4, scene.rows / 4, scene.cols / 2, scene.rows / 2);
  cv::Mat templ = scene(roi).clone();

  std::printf("ISA %s\n", line2Dup::activeISA());
  std::printf("template %dx%d   scene %dx%d\n", templ.cols, templ.rows, scene.cols, scene.rows);

  sbm::FeatureSet fs = sbm::extractFeatures(templ);
  std::printf("\n-- 1. what scaling does to the features (level 0) --\n");
  for (float s : {1.0f, 0.9f, 0.8f, 0.7f, 0.5f, 0.35f, 0.25f, 0.15f}) geometry(fs, s);

  std::printf("\n-- 2. where the time goes --\n");
  struct Run { const char *what; float smin, smax, sstep; };
  const Run runs[] = {
    { "one scale (1.0)",            1.0f, 1.0f, 0.1f },
    { "3 scales (0.9-1.1, .1)",     0.9f, 1.1f, 0.1f },
    { "5 scales (0.8-1.2, .1)",     0.8f, 1.2f, 0.1f },
    { "9 scales (0.6-1.4, .1)",     0.6f, 1.4f, 0.1f },
  };
  for (const auto &r : runs) {
    sbm::MatchConfig mc;
    mc.refine = sbm::RefineMode::ICP;
    sbm::ModelConfig gc;
    gc.angle.step = 4.0f;
    gc.scale.min = r.smin; gc.scale.max = r.smax; gc.scale.step = r.sstep;

    sbm::ShapeMatcher m(mc);
    // The IMAGE overload, not the FeatureSet one. The feature-set form leaves
    // the refine stage without the caches it builds from the picture, so every
    // candidate is dropped and the match returns nothing -- and a run with no
    // hits never reaches the per-candidate work, which is precisely the part
    // that is supposed to grow with the number of templates.
    const int variants = m.addModel("t", templ, cv::Mat(), gc);

    std::vector<double> ms;
    for (int i = 0; i < 7; i++) {
      auto t0 = std::chrono::steady_clock::now();
      auto res = m.match(scene);
      ms.push_back(std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - t0).count());
      if (i == 0) std::printf("  %-26s templates %5d  hits %zu", r.what, variants, res.size());
    }
    std::sort(ms.begin(), ms.end());
    std::printf("  median %6.1f ms\n", ms[ms.size() / 2]);
  }
  return 0;
}
