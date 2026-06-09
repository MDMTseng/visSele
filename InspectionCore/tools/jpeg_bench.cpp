// JPEG encode micro-benchmark for the inspection-core image transport.
// Measures cv::imencode(".jpg") time for a ~5MP frame, 3-channel BGR vs
// 1-channel grayscale, across a few quality levels (85 = what the WebUI sends).
// Uses the same OpenCV (vcpkg, libjpeg-turbo) the core links against.
#include <opencv2/opencv.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using clk = std::chrono::high_resolution_clock;

static void bench(const char* label, const cv::Mat& img, int q, int iters) {
  std::vector<unsigned char> buf;
  std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, q };
  cv::imencode(".jpg", img, buf, params);             // warm-up (alloc, code paths)
  auto t0 = clk::now();
  for (int i = 0; i < iters; ++i) cv::imencode(".jpg", img, buf, params);
  auto t1 = clk::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
  std::printf("  %-12s q=%-3d  %7.2f ms   %8.1f KB\n", label, q, ms, buf.size() / 1024.0);
}

int main(int argc, char** argv) {
  int iters = 100;
  cv::Mat bgr;
  const char* srcName = "synthetic gradient+noise";

  // Usage: jpeg_bench <image-path> [iters]   OR   jpeg_bench <W> <H> [iters]
  if (argc >= 2) {
    cv::Mat loaded = cv::imread(argv[1], cv::IMREAD_COLOR);   // real image path?
    if (!loaded.empty()) {
      bgr = loaded; srcName = argv[1];
      if (argc >= 3) iters = std::atoi(argv[2]);
    }
  }
  if (bgr.empty()) {                                          // fall back to synthetic W x H
    int W = 2448, H = 2048;
    if (argc >= 3) { W = std::atoi(argv[1]); H = std::atoi(argv[2]); }
    if (argc >= 4) iters = std::atoi(argv[3]);
    bgr.create(H, W, CV_8UC3);
    for (int y = 0; y < H; ++y) {
      cv::Vec3b* row = bgr.ptr<cv::Vec3b>(y);
      for (int x = 0; x < W; ++x)
        row[x] = cv::Vec3b((uchar)((x * 255) / W), (uchar)((y * 255) / H), (uchar)(((x + y) * 255) / (W + H)));
    }
    cv::Mat noise(H, W, CV_8UC3); cv::randu(noise, 0, 48); bgr += noise;
  }
  int W = bgr.cols, H = bgr.rows;
  cv::Mat gray; cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

  // optional mode: "color" or "gray" -> measure ONLY that one, from cold
  // (fair color-vs-gray comparison with identical thermal state per process).
  const char* mode = (argc >= 4) ? argv[3] : "";
  std::printf("source: %s\nimage: %dx%d (%.2f MP)   iters=%d   OpenCV %s\n",
              srcName, W, H, (W * (double)H) / 1e6, iters, CV_VERSION);

  if (std::string(mode) == "color") { bench("BGR(cold)", bgr, 85, iters); return 0; }
  if (std::string(mode) == "gray")  { bench("GRAY(cold)", gray, 85, iters); return 0; }

  // capture: isolate the SNAP_Callback per-frame allocation cost --
  // fresh `cv::Mat raw`/`r` every frame vs reused (thread_local) buffers.
  // (ExtractFrame is simulated by a memcpy of the frame bytes into raw.)
  if (std::string(mode) == "capture") {
    int N = (iters > 0) ? iters : 300;
    auto run = [&](bool reuse) {
      cv::Mat dst;                                   // dst reused in both (cvtColor)
      thread_local cv::Mat rawTL, rTL;               // reused path buffers
      auto t0 = clk::now();
      for (int i = 0; i < N; ++i) {
        cv::Mat rawL, rL;                            // fresh path buffers
        cv::Mat& raw = reuse ? rawTL : rawL;
        cv::Mat& r   = reuse ? rTL   : rL;
        raw.create(H, W, CV_8UC3);                   // fresh: first create allocs; reuse: no-op after 1st
        std::memcpy(raw.data, bgr.data, (size_t)W * H * 3);   // ~ ExtractFrame
        cv::extractChannel(raw, r, 2);
        cv::cvtColor(r, dst, cv::COLOR_GRAY2BGR);
      }
      return std::chrono::duration<double, std::milli>(clk::now() - t0).count() / N;
    };
    run(false); run(true);                           // warm both paths
    double fresh = run(false), reused = run(true);
    std::printf("--- capture path (extract+cvtColor), %d frames ---\n", N);
    std::printf("  fresh  (new raw/r each frame):  %6.2f ms/frame\n", fresh);
    std::printf("  reused (thread_local buffers):  %6.2f ms/frame   (saved %.2f ms)\n",
                reused, fresh - reused);
    return 0;
  }

  // sustained: encode continuously for ~15s, print mean ms per 1s window.
  // Reveals the burst(turbo) -> steady-state(power-limited) transition.
  if (std::string(mode) == "sustained" || std::string(mode) == "sustained-gray") {
    const cv::Mat& src = (std::string(mode) == "sustained-gray") ? gray : bgr;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 85 };
    std::vector<unsigned char> buf;
    cv::imencode(".jpg", src, buf, params);
    std::printf("--- sustained q85 (%s), per-second mean ms ---\n",
                (std::string(mode) == "sustained-gray") ? "GRAY" : "BGR");
    for (int sec = 1; sec <= 15; ++sec) {
      int n = 0; auto wstart = clk::now();
      while (std::chrono::duration<double>(clk::now() - wstart).count() < 1.0) {
        cv::imencode(".jpg", src, buf, params); ++n;
      }
      double ms = 1000.0 / n;
      std::printf("  t=%2ds   %6.2f ms   (%d enc/s)\n", sec, ms, n);
    }
    return 0;
  }

  std::printf("--- 3-channel BGR JPEG (format=1) ---\n");
  for (int q : {75, 85, 90, 95}) bench("BGR", bgr, q, iters);
  std::printf("--- 1-channel GRAY JPEG (format=2) ---\n");
  for (int q : {75, 85, 90, 95}) bench("GRAY-1ch", gray, q, iters);
  cv::Mat gray3; cv::cvtColor(gray, gray3, cv::COLOR_GRAY2BGR);
  std::printf("--- gray content as 3-channel JPEG (isolation) ---\n");
  bench("GRAY-3ch", gray3, 85, iters);
  return 0;
}
