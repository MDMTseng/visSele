#include "LabelingCV.h"
#include <opencv2/opencv.hpp>
#include <vector>

void acvComponentLabeling_cv(cv::Mat &Pic, std::vector<acv_LabeledData> &ld, int connectivity)
{
  int W = Pic.cols, H = Pic.rows;
  ld.clear();
  if (W <= 0 || H <= 0 || Pic.type() != CV_8UC3) return;

  // Mask for cv::connectedComponentsWithStats: nonzero = foreground. Input is
  // a BGR-replicated binarised image where background=255 / foreground=0;
  // inverting the R channel gives the right mask in one pass (bitwise_not is
  // ~2x faster than the compare-equal-to-255 we used before).
  cv::Mat r_ch; cv::extractChannel(Pic, r_ch, 2);
  cv::Mat m; cv::bitwise_not(r_ch, m);

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

  // Pack labels back into BGR. Parallel rows + tight inner loop cuts this
  // ~3x vs a single-threaded scalar pass. Same encoding as before:
  //   L == 0   -> (255, 255, 255)  (background)
  //   L  > 0   -> (idx&0xFF, (idx>>8)&0xFF, (idx>>16)&0xFF)
  cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rng){
    for (int y = rng.start; y < rng.end; y++)
    {
      const int *ll = lbl.ptr<int>(y);
      unsigned char *dst = Pic.ptr<unsigned char>(y);
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

