#include "camTrigAugment.hpp"

#include <logctrl.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
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
    // Find the part on a SMALL copy.
    //
    // Otsu, findContours and moments on a full-frame 2448x2048 cost hundreds of
    // milliseconds, and this runs on the camera callback thread -- so on a
    // bench driven by hand it is felt directly as "I clicked and nothing
    // happened for seconds". None of that resolution buys anything: the output
    // is one centroid, and a centroid measured at 1/4 scale is accurate to
    // about a pixel once scaled back, against a placement whose whole point is
    // to be deliberately jittered by six.
    cv::Mat small_;
    int shrink = 1;
    while ((grey.cols / (shrink * 2)) >= 500 && shrink < 8) shrink *= 2;
    if (shrink > 1) cv::resize(grey, small_, cv::Size(), 1.0 / shrink, 1.0 / shrink,
                               cv::INTER_AREA);
    const cv::Mat &src = (shrink > 1) ? small_ : grey;

    cv::Mat bin;
    cv::threshold(src, bin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
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
    // The area floor is in FULL-frame pixels, so it has to be scaled with the
    // image or a 1/4-scale run would reject parts a 1/1 run accepts.
    if (bi < 0 || best < 200.0 / (double)(shrink * shrink)) return false;
    const cv::Moments m = cv::moments(cont[bi]);
    if (m.m00 <= 0) return false;
    c = cv::Point2f((float)(m.m10 / m.m00) * shrink,
                    (float)(m.m01 / m.m00) * shrink);
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
    // Timed, because this runs on the camera callback thread and any cost here
    // is felt as UI lag with nothing pointing at it. Only the slow ones are
    // reported: a line per frame would itself be a cost.
    const int64_t _t0 = cv::getTickCount();

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
    // A fixed push off the target, on top of the random jitter.
    //
    // Placement centres the part in the inspection region, which is the right
    // default and the wrong thing for the case this was added for: a part near
    // the FRAME edge, where a real border step (ROI crop, backlight edge,
    // vignette) competes with the edge the caliper is meant to measure. Random
    // jitter cannot get there -- it is symmetric and small by design -- so the
    // push is separate, constant, and signed.
    //
    //   INSP_CAM_AUG_OFFSET_X=<px>  INSP_CAM_AUG_OFFSET_Y=<px>
    dst.x += jx + envF("INSP_CAM_AUG_OFFSET_X", 0);
    dst.y += jy + envF("INSP_CAM_AUG_OFFSET_Y", 0);

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

    // INSP_CAM_AUG_DUMP=<dir> writes the first few warped frames to disk.
    //
    // Without this the only evidence of what the engine saw is a verdict count,
    // and "NA went from 99 to 2548" cannot tell a fixture that is exercising
    // the locator from one that is warping the part off the edge of the frame.
    // Bounded on purpose (INSP_CAM_AUG_DUMP_N, default 12): this runs on the
    // camera callback thread, and a soak that writes a PNG per frame for twelve
    // hours measures the disk.
    {
      static const char *dumpDir = getenv("INSP_CAM_AUG_DUMP");
      if (dumpDir && *dumpDir)
      {
        const uint64_t lim = (uint64_t)envF("INSP_CAM_AUG_DUMP_N", 12);
        if (seq < lim)
        {
          char path[512];
          snprintf(path, sizeof(path), "%s/aug_%03llu.png",
                   dumpDir, (unsigned long long)seq);
          if (cv::imwrite(path, out))
            LOGI("aug: wrote %s (rot=%.2f skew=%.3f scale=%.3f -> %.0f,%.0f)",
                 path, rot, skew, sc, dst.x, dst.y);
          else
            LOGE("aug: could not write %s -- does the directory exist?", path);
        }
      }
    }

    {
      const double ms = (cv::getTickCount() - _t0) * 1000.0 / cv::getTickFrequency();
      if (ms > 50.0)
        LOGE_EVERY_N(20, "aug: %.0fms on a %dx%d frame -- this is on the camera "
                         "callback thread and is felt as UI lag.",
                     ms, img.cols, img.rows);
    }
    LOGI_EVERY_N(200, "aug: rot=%.2f skew=%.3f scale=%.3f -> (%.0f,%.0f)%s",
              rot, skew, sc, dst.x, dst.y, found ? "" : " [no object found]");
  }
}
