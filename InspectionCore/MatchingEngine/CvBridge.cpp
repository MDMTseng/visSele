#include "CvBridge.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

int loadImageCv(const char *path, cv::Mat &out_mat, acvImage &out_acv)
{
  if (!path) return -1;
  cv::Mat m = cv::imread(path, cv::IMREAD_COLOR);
  if (m.empty()) return -1;
  if (!m.isContinuous()) m = m.clone();   // acvImage assumes contiguous storage.
  out_mat = m;
  out_acv.useExtBuffer(out_mat.data,
                       (int)(out_mat.total() * out_mat.elemSize()),
                       out_mat.cols, out_mat.rows);
  return 0;
}

cv::Mat acvImageToGrayMat(acvImage *im)
{
  if (!im) return cv::Mat();
  int W = im->GetWidth(), H = im->GetHeight();
  if (W <= 0 || H <= 0) return cv::Mat();
  int ox = im->GetROIOffsetX(), oy = im->GetROIOffsetY();
  cv::Mat g(H, W, CV_8U);
  for (int y = 0; y < H; y++)
  {
    unsigned char *s = im->CVector[oy + y] + ox * 3;
    unsigned char *d = g.ptr<unsigned char>(y);
    for (int x = 0; x < W; x++) d[x] = s[x * 3];
  }
  return g;
}

cv::Mat acvImageToBgrMat(acvImage *im)
{
  if (!im) return cv::Mat();
  int W = im->GetWidth(), H = im->GetHeight();
  if (W <= 0 || H <= 0) return cv::Mat();
  int ox = im->GetROIOffsetX(), oy = im->GetROIOffsetY();
  cv::Mat m(H, W, CV_8UC3);
  for (int y = 0; y < H; y++)
  {
    unsigned char *s = im->CVector[oy + y] + ox * 3;
    unsigned char *d = m.ptr<unsigned char>(y);
    for (int x = 0; x < W * 3; x++) d[x] = s[x];
  }
  return m;
}

cv::Mat acvImageBgrView(acvImage *im)
{
  if (!im || !im->CVector) return cv::Mat();
  int W = im->GetWidth(), H = im->GetHeight();
  if (W <= 0 || H <= 0) return cv::Mat();
  int ox = im->GetROIOffsetX(), oy = im->GetROIOffsetY();
  // CVector[oy] points to the first row of the ROI; the ROI's first byte is
  // at CVector[oy] + ox*Channel. The stride between consecutive ROI rows is
  // the full RealWidth*Channel because the ROI is a sub-rectangle of the
  // RealWidth-wide storage (see acvImage::RESIZE: CVector[i+1] = CVector[i]
  // + RealWidth*Channel).
  unsigned char *roi0 = im->CVector[oy] + ox * im->Channel;
  size_t stride = (size_t)im->GetRealWidth() * (size_t)im->Channel;
  return cv::Mat(H, W, CV_8UC3, roi0, stride);
}

void grayMatToAcvImage(const cv::Mat &g, acvImage *im)
{
  if (!im || g.empty() || g.type() != CV_8U) return;
  int W = im->GetWidth(), H = im->GetHeight();
  int ox = im->GetROIOffsetX(), oy = im->GetROIOffsetY();
  int hh = (g.rows < H) ? g.rows : H;
  int ww = (g.cols < W) ? g.cols : W;
  for (int y = 0; y < hh; y++)
  {
    const unsigned char *s = g.ptr<unsigned char>(y);
    unsigned char *d = im->CVector[oy + y] + ox * 3;
    for (int x = 0; x < ww; x++) { d[x * 3] = d[x * 3 + 1] = d[x * 3 + 2] = s[x]; }
  }
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

