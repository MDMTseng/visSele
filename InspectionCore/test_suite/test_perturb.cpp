// Is the robustness perturbation actually what it says it is?
//
//   export PATH=/c/msys64/mingw64/bin:$PATH
//   cd InspectionCore
//   g++ -std=c++17 -O2 -o build/test_perturb.exe test_suite/test_perturb.cpp \
//     -ICore0_1 -Icontrib/cJSON -Lbuild/win-mingw-msys -lcJSON \
//     $(pkg-config --cflags --libs opencv4) && ./build/test_perturb.exe
//
// A sweep is only worth reading if the axis it sweeps is the axis it names. If
// "rot_deg: 5" delivers 5.2 degrees, or the noise is added before the warp and
// gets smoothed by it, the sweep still produces a confident curve -- of the
// wrong quantity. That failure is invisible from the result, which is exactly
// why it gets its own test.
#include "TestPerturb.h"
#include <cstdio>
#include <cmath>

static int fails = 0;
static void ok(bool c, const char *msg, const char *detail = "")
{
  printf("%s%s%s%s", c ? "PASS  " : "FAIL  ", msg,
         detail[0] ? "  -- " : "", detail);
  printf("\n");
  if (!c) fails++;
}

// A single bright dot on a dark field: its centroid is unambiguous, so where it
// LANDS is a direct read of the transform.
static cv::Mat dotAt(int w, int h, float x, float y)
{
  cv::Mat m = cv::Mat::zeros(h, w, CV_8U);
  cv::circle(m, cv::Point((int)x, (int)y), 6, cv::Scalar(255), -1);
  return m;
}
static cv::Point2f centroid(const cv::Mat &m)
{
  cv::Moments mo = cv::moments(m, false);
  if (mo.m00 <= 0) return cv::Point2f(NAN, NAN);
  return cv::Point2f((float)(mo.m10 / mo.m00), (float)(mo.m01 / mo.m00));
}

int main()
{
  const int W = 400, H = 300;
  const cv::Point2f C(W * 0.5f, H * 0.5f);

  // --- rotation is the angle it says, about the image centre ---------------
  {
    const float R = 100, deg = 20;
    cv::Mat img = dotAt(W, H, C.x + R, C.y);          // dot on the +x axis
    TestPerturb t; t.rot_deg = deg;
    test_perturb_apply(img, t, 0);
    cv::Point2f p = centroid(img);
    // getRotationMatrix2D's positive angle is COUNTER-clockwise in a y-down
    // image, i.e. the dot moves to NEGATIVE y. Asserting the SIGN matters more
    // than the magnitude: a sweep that reports the locator lagging by 2*theta
    // is a sign error here, and it looks exactly like a real finding.
    float ang = atan2f(-(p.y - C.y), p.x - C.x) * 180.0f / (float)CV_PI;
    float rad = hypotf(p.x - C.x, p.y - C.y);
    char d[160]; snprintf(d, sizeof(d), "landed at %.2f deg, r=%.2f (asked %.0f, r=%.0f)",
                          ang, rad, deg, R);
    ok(fabsf(ang - deg) < 0.5f, "rot_deg rotates by exactly that, CCW", d);
    ok(fabsf(rad - R) < 1.0f, "and about the image CENTRE, not a corner", d);
  }

  // --- scale ---------------------------------------------------------------
  {
    const float R = 80, sc = 1.25f;
    cv::Mat img = dotAt(W, H, C.x + R, C.y);
    TestPerturb t; t.scale = sc;
    test_perturb_apply(img, t, 0);
    cv::Point2f p = centroid(img);
    float rad = hypotf(p.x - C.x, p.y - C.y);
    char d[160]; snprintf(d, sizeof(d), "r %.1f -> %.1f (expected %.1f)", R, rad, R * sc);
    ok(fabsf(rad - R * sc) < 1.5f, "scale scales about the centre", d);
  }

  // --- skew ----------------------------------------------------------------
  {
    const float dy = 60, k = 0.3f;
    cv::Mat img = dotAt(W, H, C.x, C.y - dy);
    TestPerturb t; t.skew = k;
    test_perturb_apply(img, t, 0);
    cv::Point2f p = centroid(img);
    // x += skew * (y - cy). The dot is ABOVE centre, so it shifts by -k*dy.
    float want = C.x - k * dy;
    char d[160]; snprintf(d, sizeof(d), "x %.1f -> %.1f (expected %.1f)", C.x, p.x, want);
    ok(fabsf(p.x - want) < 1.5f, "skew shears x by skew*(y-cy)", d);
    ok(fabsf(p.y - (C.y - dy)) < 1.5f, "and leaves y alone", d);
  }

  // --- gain / bias ---------------------------------------------------------
  {
    cv::Mat img(H, W, CV_8U, cv::Scalar(100));
    TestPerturb t; t.gain = 1.5f; t.bias = 10;
    test_perturb_apply(img, t, 0);
    char d[96]; snprintf(d, sizeof(d), "100 -> %d (expected 160)",
                         (int)img.at<uint8_t>(H/2, W/2));
    ok(img.at<uint8_t>(H/2, W/2) == 160, "gain then bias, in that order", d);

    cv::Mat hi(H, W, CV_8U, cv::Scalar(200));
    TestPerturb t2; t2.gain = 2;
    test_perturb_apply(hi, t2, 0);
    ok(hi.at<uint8_t>(H/2, W/2) == 255,
       "and it SATURATES rather than wrapping",
       "a wrap would turn the brightest part of the image black");
  }

  // --- noise ---------------------------------------------------------------
  {
    cv::Mat img(H, W, CV_8U, cv::Scalar(128));
    TestPerturb t; t.noise = 12;
    test_perturb_apply(img, t, 7);
    cv::Scalar mean, sd; cv::meanStdDev(img, mean, sd);
    char d[128]; snprintf(d, sizeof(d), "sigma asked 12, measured %.2f", sd[0]);
    ok(fabs(sd[0] - 12.0) < 1.5, "noise sigma is the sigma it says", d);

    // THE ORDER TEST. Noise added before the warp would be smoothed by the
    // interpolation, and the sweep would then report the matcher tolerating far
    // more noise than a real sensor produces -- a lie in the flattering
    // direction. Same sigma, with a rotation: the measured sigma must NOT drop.
    cv::Mat img2(H, W, CV_8U, cv::Scalar(128));
    TestPerturb t2; t2.noise = 12; t2.rot_deg = 7;
    test_perturb_apply(img2, t2, 7);
    cv::Scalar m2, sd2; cv::meanStdDev(img2, m2, sd2);
    char d2[192]; snprintf(d2, sizeof(d2),
        "without warp %.2f, with a 7deg warp %.2f -- a drop means noise is resampled",
        sd[0], sd2[0]);
    ok(sd2[0] > sd[0] * 0.9, "and it is added AFTER the warp, not smoothed by it", d2);
  }

  // --- refusal -------------------------------------------------------------
  {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "rot_deg", 4);
    cJSON_AddNumberToObject(p, "scale", 500);        // absurd
    TestPerturb t = test_perturb_parse(p);
    ok(t.rot_deg == 4 && t.scale == 1,
       "an out-of-range knob is DROPPED, and the sane ones still apply",
       "clamping would silently sweep an axis nobody asked for");
    cJSON_Delete(p);
  }

  // --- repeatability -------------------------------------------------------
  {
    cv::Mat a(H, W, CV_8U, cv::Scalar(128)), b(H, W, CV_8U, cv::Scalar(128));
    TestPerturb t; t.noise = 20;
    test_perturb_apply(a, t, 3);
    test_perturb_apply(b, t, 3);
    ok(cv::countNonZero(a != b) == 0,
       "the same seed gives the same image",
       "a robustness figure that moves when nothing changed cannot be acted on");
    cv::Mat cc(H, W, CV_8U, cv::Scalar(128));
    test_perturb_apply(cc, t, 4);
    ok(cv::countNonZero(a != cc) > 0, "and a different seed gives a different one");
  }

  printf("\n");
  if (fails) printf("%d FAILURES\n", fails);
  else       printf("--- the perturbation is what it says it is ---\n");
  return fails ? 1 : 0;
}
