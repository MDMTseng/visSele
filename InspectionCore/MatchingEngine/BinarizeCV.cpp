#include "BinarizeCV.h"
#include <opencv2/imgproc.hpp>

void binarize_bg_flatten_cv(const cv::Mat &src, cv::Mat &dst, int closeKernel, float ratio, int downscale)
{
  if (src.empty()) return;
  const int W = src.cols, H = src.rows;
  if (downscale < 1) downscale = 1;
  if (closeKernel < 3) closeKernel = 3;
  if ((closeKernel & 1) == 0) closeKernel++; // odd

  // pull channel 0 into a gray cv::Mat (BGR images replicate gray; we just want
  // one channel either way).
  cv::Mat g;
  if (src.channels() == 1) g = src;
  else cv::extractChannel(src, g, 0);

  // estimate background illumination at low res: morphological CLOSE fills dark
  // objects with the surrounding bright field (the vignette / gradient).
  cv::Mat small, bgSmall, bg;
  if (downscale > 1) cv::resize(g, small, cv::Size(W / downscale, H / downscale), 0, 0, cv::INTER_AREA);
  else small = g;
  int k = std::max(3, closeKernel / (downscale > 1 ? downscale : 1));
  if ((k & 1) == 0) k++;
  cv::morphologyEx(small, bgSmall, cv::MORPH_CLOSE,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k)));
  if (downscale > 1) cv::resize(bgSmall, bg, cv::Size(W, H), 0, 0, cv::INTER_LINEAR);
  else bg = bgSmall;

  // flat-field divide + threshold at `ratio` of local background.
  // object where g < ratio*bg  -> black(0); background -> white(255).
  dst.create(H, W, CV_8UC3);
  for (int y = 0; y < H; y++)
  {
    const unsigned char *gp = g.ptr<unsigned char>(y);
    const unsigned char *bp = bg.ptr<unsigned char>(y);
    unsigned char *o = dst.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++)
    {
      float thr = ratio * (float)bp[x];
      unsigned char v = (gp[x] < thr) ? 0 : 255;
      o[x * 3] = o[x * 3 + 1] = o[x * 3 + 2] = v;
    }
  }
}
