#include "BinarizeCV.h"
#include <opencv2/opencv.hpp>

void binarize_bg_flatten_cv(acvImage *src, acvImage *dst, int closeKernel, float ratio, int downscale)
{
  int W = src->GetWidth(), H = src->GetHeight();
  int ox = src->GetROIOffsetX(), oy = src->GetROIOffsetY();
  if (W <= 0 || H <= 0) return;
  if (downscale < 1) downscale = 1;
  if (closeKernel < 3) closeKernel = 3;
  if ((closeKernel & 1) == 0) closeKernel++; // odd

  // pull channel 0 into a gray cv::Mat
  cv::Mat g(H, W, CV_8U);
  for (int y = 0; y < H; y++)
  {
    unsigned char *s = src->CVector[oy + y] + ox * 3;
    unsigned char *d = g.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++) d[x] = s[x * 3];
  }

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
  for (int y = 0; y < H; y++)
  {
    unsigned char *gp = g.ptr<unsigned char>(y);
    unsigned char *bp = bg.ptr<unsigned char>(y);
    unsigned char *o = dst->CVector[oy + y] + ox * 3;
    for (int x = 0; x < W; x++)
    {
      float thr = ratio * (float)bp[x];
      unsigned char v = (gp[x] < thr) ? 0 : 255; // object dark -> 0, bg -> 255
      o[x * 3] = o[x * 3 + 1] = o[x * 3 + 2] = v;
    }
  }
}
