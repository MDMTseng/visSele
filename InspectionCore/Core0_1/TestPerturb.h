#ifndef TEST_PERTURB_H
#define TEST_PERTURB_H
// Deliberately degrade an image, to find out where the localizer stops coping.
//
// WHY IT IS HERE AND NOT IN THE WEBUI
//
// The obvious place is the editor: transform the bitmap on a canvas and send it
// up. Two things make that the wrong answer. The transferred image is a second,
// different resampling of the data -- so the test measures the browser's
// interpolation as much as the machine's; and every variant of a sweep is then
// a full image over the wire, which turns a 40-step sweep into a transfer test.
// Applied here, one small JSON per variant perturbs the exact buffer the
// inspection is about to run on.
//
// THE PERTURBATION IS THE GROUND TRUTH
//
// This is the point of the whole thing. Rotate the image by a known theta and
// the locator must report theta; the residual IS the localization error, with
// no reference measurement needed and nothing to trust but arithmetic. "Did it
// find the part" is the weak version of the question this can answer.
//
// ORDER, WHICH IS NOT ARBITRARY
//
//   1. geometry (rotate / scale / skew) -- what the optics and the fixture do
//   2. gain and bias                    -- what the lighting does
//   3. noise                            -- what the sensor does, LAST
//
// Noise added before the warp would be resampled by it, and interpolation is a
// low-pass filter: the sweep would report the matcher surviving far more noise
// than a real sensor produces. Getting this order wrong makes the test lie in
// the flattering direction, which is the worst kind.
#include <opencv2/opencv.hpp>
#include <cmath>
#include "cJSON.h"

struct TestPerturb
{
  float rot_deg = 0;     // + is CCW in image coords, about the image centre
  float scale   = 1;     // 1 = none
  float skew    = 0;     // x += skew * y, about the centre
  float gain    = 1;     // pixel *= gain
  float bias    = 0;     // pixel += bias  (in 8-bit levels)
  float noise   = 0;     // additive gaussian sigma, in 8-bit levels
  float shift_x = 0;     // translate, image px, + is right
  float shift_y = 0;     // translate, image px, + is down
  bool  any() const {
    return rot_deg != 0 || scale != 1 || skew != 0 || shift_x != 0 || shift_y != 0 ||
           gain != 1 || bias != 0 || noise > 0;
  }
};

// Parse, with every field optional and absent meaning "leave it alone".
// A NaN or an absurd value is dropped rather than clamped: this is a test knob,
// and silently testing something other than what was asked for is worse than
// testing nothing.
static inline TestPerturb test_perturb_parse(cJSON *p)
{
  TestPerturb t;
  if (p == NULL || !cJSON_IsObject(p)) return t;
  auto num = [&](const char *k, float lo, float hi, float *dst) {
    cJSON *j = cJSON_GetObjectItem(p, k);
    if (!cJSON_IsNumber(j)) return;
    double v = j->valuedouble;
    if (!(v >= lo && v <= hi)) {                 // NaN-safe
      printf("test_perturb: %s=%g outside [%g,%g] -- ignored\n", k, v, lo, hi);
      return;
    }
    *dst = (float)v;
  };
  num("rot_deg", -180, 180,  &t.rot_deg);
  num("scale",   0.2f, 5,    &t.scale);
  num("skew",    -2,   2,    &t.skew);
  num("gain",    0,    8,    &t.gain);
  num("bias",    -255, 255,  &t.bias);
  num("noise",   0,    128,  &t.noise);
  num("shift_x", -4096, 4096, &t.shift_x);
  num("shift_y", -4096, 4096, &t.shift_y);
  return t;
}

// Apply in place. `seed` makes the noise repeatable, so re-running a sweep step
// gives the same answer -- a robustness figure that moves when nothing changed
// is one nobody can act on.
static inline void test_perturb_apply(cv::Mat &img, const TestPerturb &t, int seed)
{
  if (img.empty() || !t.any()) return;

  if (t.rot_deg != 0 || t.scale != 1 || t.skew != 0 || t.shift_x != 0 || t.shift_y != 0)
  {
    const cv::Point2f c(img.cols * 0.5f, img.rows * 0.5f);
    cv::Mat M = cv::getRotationMatrix2D(c, t.rot_deg, t.scale);
    // Translation rides on the same matrix: one resample for the whole
    // geometric step, same as the shear below. Applied AFTER rotation/scale
    // about the centre, so a located part should move by exactly (shift_x,
    // shift_y) px on top of whatever the rotation did.
    M.at<double>(0, 2) += t.shift_x;
    M.at<double>(1, 2) += t.shift_y;
    if (t.skew != 0)
    {
      // Shear about the centre, composed with the rotation so the whole
      // geometric step is ONE resample. Two warpAffine calls would blur twice
      // and the sweep would blame the matcher for it.
      cv::Mat S = (cv::Mat_<double>(3, 3) <<
                   1, t.skew, -t.skew * c.y,
                   0, 1,      0,
                   0, 0,      1);
      cv::Mat M3 = cv::Mat::eye(3, 3, CV_64F);
      M.copyTo(M3(cv::Rect(0, 0, 3, 2)));
      cv::Mat C = M3 * S;
      M = C(cv::Rect(0, 0, 3, 2)).clone();
    }
    cv::Mat out;
    // BORDER_REPLICATE, not a black fill. On a backlit part the background is
    // bright, and black corners are a full-scale edge the locator would either
    // lock onto or be distracted by -- so a rotation sweep would measure the
    // border treatment rather than the rotation.
    cv::warpAffine(img, out, M, img.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    img = out;
  }

  if (t.gain != 1 || t.bias != 0)
    img.convertTo(img, img.type(), t.gain, t.bias);

  if (t.noise > 0)
  {
    cv::RNG rng((uint64_t)seed * 6364136223846793005ULL + 1442695040888963407ULL);
    cv::Mat n(img.size(), CV_32FC(img.channels()));
    rng.fill(n, cv::RNG::NORMAL, 0, t.noise);
    cv::Mat f; img.convertTo(f, n.type());
    f += n;
    f.convertTo(img, img.type());          // convertTo saturates, so no wrap
  }
}

#endif // TEST_PERTURB_H
