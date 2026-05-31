#include "LabelingCV.h"
#include <opencv2/opencv.hpp>
#include <vector>

static void labeling_impl(cv::Mat &Pic, cv::Mat &labelOut, std::vector<acv_LabeledData> &ld, int connectivity)
{
  int W = Pic.cols, H = Pic.rows;
  ld.clear();
  if (W <= 0 || H <= 0) return;
  if (Pic.type() != CV_8UC1 && Pic.type() != CV_8UC3) return;

  // Mask for cv::connectedComponentsWithStats: nonzero = foreground.
  //   CV_8UC1 input: bg=255 / fg=0, invert in place. (Phase 1 fast path.)
  //   CV_8UC3 input: BGR-replicated grayscale, extract R then invert.
  cv::Mat m;
  if (Pic.type() == CV_8UC1) {
    cv::bitwise_not(Pic, m);
  } else {
    cv::Mat r_ch; cv::extractChannel(Pic, r_ch, 2);
    cv::bitwise_not(r_ch, m);
  }

  cv::Mat lbl, stats, cent;
  int n = cv::connectedComponentsWithStats(m, lbl, stats, cent, connectivity, CV_32S);

  // cage = foreground component with largest bbox -> label 1; objects -> 2,3,...
  int cage = -1; long bestbb = -1;
  for (int L = 1; L < n; L++)
  {
    long bb = (long)stats.at<int>(L, cv::CC_STAT_WIDTH) * stats.at<int>(L, cv::CC_STAT_HEIGHT);
    if (bb > bestbb) { bestbb = bb; cage = L; }
  }
  std::vector<int> remap(n, 0);
  int nxt = 2;
  for (int L = 1; L < n; L++) remap[L] = (L == cage) ? 1 : nxt++;

  ld.resize(nxt);
  for (int i = 0; i < nxt; i++) { ld[i].area = 0; ld[i].misc = 0; }
  for (int L = 1; L < n; L++)
  {
    int idx = remap[L];
    acv_LabeledData &d = ld[idx];
    int left = stats.at<int>(L, cv::CC_STAT_LEFT), top = stats.at<int>(L, cv::CC_STAT_TOP);
    int w = stats.at<int>(L, cv::CC_STAT_WIDTH), h = stats.at<int>(L, cv::CC_STAT_HEIGHT);
    d.area = stats.at<int>(L, cv::CC_STAT_AREA);
    d.LTBound = acv_XY((float)left, (float)top);
    d.RBBound = acv_XY((float)(left + w - 1), (float)(top + h - 1));
    d.Center  = acv_XY((float)cent.at<double>(L, 0), (float)cent.at<double>(L, 1));
  }

  // Pack labels back into BGR.
  //   L == 0   -> (255, 255, 255)  (background)
  //   L  > 0   -> (idx&0xFF, (idx>>8)&0xFF, (idx>>16)&0xFF)
  // If caller didn't pass a separate buffer (legacy single-arg overload), pack
  // in place into Pic; cv::Mat::create reallocates Pic to CV_8UC3 if needed.
  cv::Mat &out = labelOut.empty() && Pic.type() == CV_8UC3 ? Pic : labelOut;
  if (&out == &labelOut) {
    out.create(H, W, CV_8UC3);
  } else {
    // in-place CV_8UC1 -> CV_8UC3 reallocates; that's the legacy path.
    Pic.create(H, W, CV_8UC3);
  }
  cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rng){
    for (int y = rng.start; y < rng.end; y++)
    {
      const int *ll = lbl.ptr<int>(y);
      unsigned char *dst = out.ptr<unsigned char>(y);
      for (int x = 0; x < W; x++)
      {
        int L = ll[x];
        unsigned char *p = dst + x * 3;
        if (L == 0) { p[0] = 255; p[1] = 255; p[2] = 255; }
        else {
          int idx = remap[L];
          p[0] = (unsigned char)(idx);
          p[1] = (unsigned char)(idx >> 8);
          p[2] = (unsigned char)(idx >> 16);
        }
      }
    }
  });
}

void acvComponentLabeling_cv(cv::Mat &Pic, std::vector<acv_LabeledData> &ld, int connectivity)
{
  cv::Mat noOut;
  labeling_impl(Pic, noOut, ld, connectivity);
}

void acvComponentLabeling_cv(cv::Mat &Pic, cv::Mat &labelOut, std::vector<acv_LabeledData> &ld, int connectivity)
{
  labeling_impl(Pic, labelOut, ld, connectivity);
}

