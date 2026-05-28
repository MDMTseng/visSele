#include "LabelingCV.h"
#include <opencv2/opencv.hpp>
#include <vector>

void acvComponentLabeling_cv(acvImage *Pic, int connectivity)
{
  int W = Pic->GetWidth(), H = Pic->GetHeight();
  int ox = Pic->GetROIOffsetX(), oy = Pic->GetROIOffsetY();
  if (W <= 0 || H <= 0) return;

  // foreground = "not background". The pipeline marks background as 255 in the
  // R channel (channel +2); object/cage pixels are non-255.
  cv::Mat m(H, W, CV_8U);
  for (int y = 0; y < H; y++)
  {
    unsigned char *src = Pic->CVector[oy + y] + ox * 3;
    unsigned char *d = m.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++) { d[x] = (src[x * 3 + 2] != 255) ? 255 : 0; }
  }

  cv::Mat lbl, stats, cent;
  int n = cv::connectedComponentsWithStats(m, lbl, stats, cent, connectivity, CV_32S);

  // cage/frame = foreground component with the largest bounding box (it spans
  // the whole image). Remap it to label 1; remaining objects to 2,3,...
  int cage = -1; long bestbb = -1;
  for (int L = 1; L < n; L++)
  {
    long bb = (long)stats.at<int>(L, cv::CC_STAT_WIDTH) * stats.at<int>(L, cv::CC_STAT_HEIGHT);
    if (bb > bestbb) { bestbb = bb; cage = L; }
  }
  std::vector<int> remap(n, 0);
  int nxt = 2;
  for (int L = 1; L < n; L++) remap[L] = (L == cage) ? 1 : nxt++;

  // write the remapped label back, 24-bit packed in B,G,R (bg -> 255,255,255).
  for (int y = 0; y < H; y++)
  {
    const int *ll = lbl.ptr<int>(y);
    unsigned char *dst = Pic->CVector[oy + y] + ox * 3;
    for (int x = 0; x < W; x++)
    {
      int L = ll[x];
      unsigned char *p = dst + x * 3;
      if (L == 0) { p[0] = p[1] = p[2] = 255; }
      else { int idx = remap[L]; p[0] = idx & 0xFF; p[1] = (idx >> 8) & 0xFF; p[2] = (idx >> 16) & 0xFF; }
    }
  }
}
