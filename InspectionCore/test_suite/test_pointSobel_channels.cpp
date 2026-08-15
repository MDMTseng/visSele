// Does pointSobel give the same answer for a 1-channel image and the identical
// content expanded to 3 channels? It must: the pixels are the same, only the
// stride differs. Before the fix the 1-channel case indexed as if the row were
// three times as wide.
#include "MatchingCore.h"
acv_XY pointSobel(const cv::Mat &graylevelImg, acv_XY point, int range);
#include <opencv2/opencv.hpp>
#include <cstdio>

int main()
{
  const int W = 200, H = 120;
  cv::Mat g1(H, W, CV_8UC1);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      g1.at<uint8_t>(y, x) = (uint8_t)((x * 7 + y * 13) & 0xFF);   // structured, not flat
  cv::Mat g3; cv::cvtColor(g1, g3, cv::COLOR_GRAY2BGR);            // same content, 3ch

  int mismatch = 0, checked = 0;
  for (int y = 3; y < H - 3; y += 7)
    for (int x = 3; x < W - 3; x += 5)
    {
      acv_XY p((float)x, (float)y);
      acv_XY s1 = pointSobel(g1, p, 2);
      acv_XY s3 = pointSobel(g3, p, 2);
      checked++;
      if (s1.x != s3.x || s1.y != s3.y)
      {
        if (mismatch < 4)
          printf("  x=%3d y=%3d  1ch=(%.0f,%.0f)  3ch=(%.0f,%.0f)\n",
                 x, y, s1.x, s1.y, s3.x, s3.y);
        mismatch++;
      }
    }
  printf("檢查 %d 點, 不一致 %d 點 -> %s\n", checked, mismatch,
         mismatch ? "**1 通道與 3 通道結果不同 (bug)**" : "一致 (正確)");
  return mismatch ? 1 : 0;
}
