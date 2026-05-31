#include "CvBridge.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

int loadImageCv(const char *path, cv::Mat &out_mat)
{
  if (!path) return -1;
  cv::Mat m = cv::imread(path, cv::IMREAD_COLOR);
  if (m.empty()) return -1;
  if (!m.isContinuous()) m = m.clone();
  out_mat = m;
  return 0;
}


void cvCloneImage(const cv::Mat &src, cv::Mat &dst, int mode)
{
  if (src.empty()) return;
  if (mode == -1) {
    src.copyTo(dst);
    return;
  }
  if (mode < 0 || mode > 2) {
    dst.create(src.rows, src.cols, CV_8UC3);
    dst.setTo(cv::Scalar(0, 0, 0));
    return;
  }
  cv::Mat ch;
  if (src.channels() == 1) ch = src;
  else cv::extractChannel(src, ch, mode);
  cv::cvtColor(ch, dst, cv::COLOR_GRAY2BGR);
}


void cvThresholdMap(cv::Mat &dst, const cv::Mat &src,
                    const float *threshMap, int mapW, int mapH, int channel)
{
  if (src.empty() || threshMap == NULL || mapW < 1 || mapH < 1) return;
  if (channel < 0 || channel >= src.channels()) return;
  const int H = src.rows, W = src.cols;
  dst.create(H, W, CV_8UC3);
  const int ch_n = src.channels();
  for (int i = 0; i < H; i++)
  {
    const uint8_t *sLine = src.ptr<uint8_t>(i);
    uint8_t *dLine = dst.ptr<uint8_t>(i);

    float gv = (mapH == 1) ? 0.0f : ((float)i / (H - 1)) * (mapH - 1);
    int gy0 = (int)gv; if (gy0 > mapH - 2 && mapH > 1) gy0 = mapH - 2; if (gy0 < 0) gy0 = 0;
    int gy1 = (mapH == 1) ? 0 : gy0 + 1;
    float fy = gv - gy0;

    for (int j = 0; j < W; j++)
    {
      float gu = (mapW == 1) ? 0.0f : ((float)j / (W - 1)) * (mapW - 1);
      int gx0 = (int)gu; if (gx0 > mapW - 2 && mapW > 1) gx0 = mapW - 2; if (gx0 < 0) gx0 = 0;
      int gx1 = (mapW == 1) ? 0 : gx0 + 1;
      float fx = gu - gx0;

      float t00 = threshMap[gy0 * mapW + gx0];
      float t01 = threshMap[gy0 * mapW + gx1];
      float t10 = threshMap[gy1 * mapW + gx0];
      float t11 = threshMap[gy1 * mapW + gx1];
      float T = (t00 * (1 - fx) + t01 * fx) * (1 - fy) +
                (t10 * (1 - fx) + t11 * fx) * fy;

      uint8_t v = (sLine[j * ch_n + channel] > T) ? 255 : 0;
      dLine[j * 3] = v;
      dLine[j * 3 + 1] = v;
      dLine[j * 3 + 2] = v;
    }
  }
}
