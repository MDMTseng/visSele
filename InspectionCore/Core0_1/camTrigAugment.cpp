#include "camTrigAugment.hpp"

#include <logctrl.h>
#include <opencv2/imgproc.hpp>
#include <cstdlib>
#include <cmath>

LOG_MODULE("core.aug");

namespace camTrigAugment
{
  static float envF(const char *k, float dflt)
  {
    const char *e = getenv(k);
    if (!e || !*e) return dflt;
    return (float)atof(e);
  }

  bool on()
  {
    static const bool v = []{
      const char *e = getenv("INSP_CAM_AUG");
      const bool y = e && atoi(e) != 0;
      if (y)
        LOGW("INSP_CAM_AUG=1 -- every released frame is warped before it enters "
             "the pipeline: rot=%.2fdeg skew=%.3f scale=%.3f jitter=%.1fpx "
             "noise=%.1f blur=%.2f place=%d seed=%d. Bench only: the frames the "
             "engine sees are NOT the frames on disk.",
             envF("INSP_CAM_AUG_ROT", 0), envF("INSP_CAM_AUG_SKEW", 0),
             envF("INSP_CAM_AUG_SCALE", 0), envF("INSP_CAM_AUG_JITTER", 0),
             envF("INSP_CAM_AUG_NOISE", 0), envF("INSP_CAM_AUG_BLUR", 0),
             (int)envF("INSP_CAM_AUG_PLACE", 1), (int)envF("INSP_CAM_AUG_SEED", 1));
      return y;
    }();
    return v;
  }

  // Where the part is now.
  //
  // Parts are dark silhouettes on a bright backlit background -- the same
  // assumption search_point_cv's default polarity is built on -- so Otsu on the
  // inverted image finds the object without a threshold to tune per fixture.
  // The centroid of the largest dark blob is the anchor; the largest, not all
  // dark pixels, so a vignette corner or a speck does not drag it.
  //
  // Returns false when nothing convincing is found, and the caller then leaves
  // the placement alone rather than translating the frame by a guess.
  static bool findObject(const cv::Mat &grey, cv::Point2f &c)
  {
    cv::Mat bin;
    cv::threshold(grey, bin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> cont;
    cv::findContours(bin, cont, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double best = 0; int bi = -1;
    for (size_t i = 0; i < cont.size(); i++)
    {
      const double a = cv::contourArea(cont[i]);
      if (a > best) { best = a; bi = (int)i; }
    }
    // A blob smaller than this is a speck, and anchoring a whole frame to a
    // speck moves the real part OUT of the region rather than into it.
    if (bi < 0 || best < 200.0) return false;
    const cv::Moments m = cv::moments(cont[bi]);
    if (m.m00 <= 0) return false;
    c = cv::Point2f((float)(m.m10 / m.m00), (float)(m.m01 / m.m00));
    return true;
  }

  // Deterministic per frame: same run, same sequence, same image. A fixture
  // whose output cannot be reproduced turns every difference between two runs
  // into an open question.
  static uint32_t mix(uint64_t seq, uint32_t seed, uint32_t salt)
  {
    uint64_t x = seq * 0x9E3779B97F4A7C15ULL + seed * 0x632BE59BD9B4E019ULL + salt;
    x ^= x >> 33; x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33; x *= 0xC4CEB9FE1A85EC53ULL;
    x ^= x >> 33;
    return (uint32_t)x;
  }
  // Uniform in +/-amp.
  static float sym(uint64_t seq, uint32_t seed, uint32_t salt, float amp)
  {
    if (amp == 0) return 0;
    return (float)((mix(seq, seed, salt) / 4294967295.0) * 2.0 - 1.0) * amp;
  }

  void apply(cv::Mat &img, uint64_t seq, float regionCx, float regionCy)
  {
    if (!on() || img.empty()) return;

    const uint32_t seed = (uint32_t)envF("INSP_CAM_AUG_SEED", 1);
    const float rot     = sym(seq, seed, 1, envF("INSP_CAM_AUG_ROT", 0));
    const float skew    = sym(seq, seed, 2, envF("INSP_CAM_AUG_SKEW", 0));
    const float sc      = 1.0f + sym(seq, seed, 3, envF("INSP_CAM_AUG_SCALE", 0));
    const float jx      = sym(seq, seed, 4, envF("INSP_CAM_AUG_JITTER", 0));
    const float jy      = sym(seq, seed, 5, envF("INSP_CAM_AUG_JITTER", 0));
    const float noise   = envF("INSP_CAM_AUG_NOISE", 0);
    const float blur    = envF("INSP_CAM_AUG_BLUR", 0);
    const bool  place   = envF("INSP_CAM_AUG_PLACE", 1) != 0;

    cv::Mat grey;
    if (img.channels() == 1) grey = img;
    else cv::cvtColor(img, grey, cv::COLOR_BGR2GRAY);

    // Anchor: the object's own centre, so a rotation turns the PART rather than
    // sweeping it round the frame on a radius.
    cv::Point2f src(img.cols * 0.5f, img.rows * 0.5f);
    const bool found = findObject(grey, src);

    cv::Point2f dst = src;
    if (place && found)
    {
      if (regionCx >= 0 && regionCy >= 0
          && regionCx < img.cols && regionCy < img.rows)
        dst = cv::Point2f(regionCx, regionCy);
      else
      {
        // Loud, and once. A region that does not intersect the frame means the
        // recipe and the canned images are in different coordinate spaces, and
        // silently centring the part instead would make the run LOOK like it
        // was testing the region when it was not.
        static bool said = false;
        if (!said)
        {
          said = true;
          LOGE("INSP_CAM_AUG_PLACE: the inspection region centre (%.0f,%.0f) is "
               "not inside the %dx%d frame -- placing at the frame centre "
               "instead. The region and these canned frames are in different "
               "coordinate spaces; the run is NOT exercising the region test.",
               regionCx, regionCy, img.cols, img.rows);
        }
        dst = cv::Point2f(img.cols * 0.5f, img.rows * 0.5f);
      }
    }
    dst.x += jx; dst.y += jy;

    // One affine, composed: rotate+scale about the object, shear, then move the
    // object's centre to the target. Composed rather than applied in stages --
    // three warps means three resamplings, and the softening that adds is
    // indistinguishable from a focus problem the fixture did not intend.
    const double th = rot * CV_PI / 180.0;
    const double ca = std::cos(th) * sc, sa = std::sin(th) * sc;
    // [ca -sa][1 skew] = [ca  ca*skew-sa]
    // [sa  ca][0    1]   [sa  sa*skew+ca]
    const double a11 = ca,               a12 = ca * skew - sa;
    const double a21 = sa,               a22 = sa * skew + ca;
    cv::Mat M = (cv::Mat_<double>(2, 3) <<
                 a11, a12, dst.x - (a11 * src.x + a12 * src.y),
                 a21, a22, dst.y - (a21 * src.x + a22 * src.y));

    // The border must be BACKGROUND, not black: a backlit part is dark on
    // bright, so a black border reads as more object and the locator can lock
    // onto the frame edge. Sample the actual corner rather than assuming 255 --
    // these frames are ROI crops and their background is not saturated.
    const cv::Scalar bg = cv::mean(grey(cv::Rect(0, 0,
                                                 std::min(16, grey.cols),
                                                 std::min(16, grey.rows))));
    cv::Mat out;
    cv::warpAffine(img, out, M, img.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar::all(bg[0]));

    if (blur > 0) cv::GaussianBlur(out, out, cv::Size(0, 0), blur);
    if (noise > 0)
    {
      // Seeded from the frame, like everything else here.
      cv::RNG rng(mix(seq, seed, 6));
      cv::Mat n(out.size(), CV_16SC(out.channels()));
      rng.fill(n, cv::RNG::NORMAL, 0, noise);
      cv::Mat o16; out.convertTo(o16, CV_16S);
      o16 += n;
      o16.convertTo(out, CV_8U);   // saturate_cast clamps
    }

    img = out;
    LOGI_EVERY_N(200, "aug: rot=%.2f skew=%.3f scale=%.3f -> (%.0f,%.0f)%s",
              rot, skew, sc, dst.x, dst.y, found ? "" : " [no object found]");
  }
}
