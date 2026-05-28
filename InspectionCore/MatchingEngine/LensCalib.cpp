#include "LensCalib.h"
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>

LensModel lens_model_from_string(const std::string &s)
{
  if (s == "perspective" || s == "normal" || s == "pinhole")
    return LensModel::PERSPECTIVE;
  return LensModel::TELECENTRIC;
}

const char *lens_model_to_string(LensModel m)
{
  return (m == LensModel::PERSPECTIVE) ? "perspective" : "telecentric";
}

LensCalibResult perspective_calibrate(const std::vector<TelecentricViewData> &views,
                                      int imgW, int imgH, bool fixK3)
{
  LensCalibResult res;
  res.model = LensModel::PERSPECTIVE;
  if (views.size() < 3) return res;

  std::vector<std::vector<cv::Point3f>> objPts;
  std::vector<std::vector<cv::Point2f>> imgPts;
  for (const auto &v : views)
  {
    std::vector<cv::Point3f> o; std::vector<cv::Point2f> im;
    for (int i = 0; i < v.n; i++)
    {
      o.emplace_back((float)v.obj[i*3+0], (float)v.obj[i*3+1], (float)v.obj[i*3+2]);
      im.emplace_back((float)v.img[i*2+0], (float)v.img[i*2+1]);
    }
    objPts.push_back(o); imgPts.push_back(im);
  }

  cv::Mat K = cv::Mat::eye(3, 3, CV_64F);
  cv::Mat D = cv::Mat::zeros(5, 1, CV_64F);
  std::vector<cv::Mat> rvecs, tvecs;
  int flags = 0;
  if (fixK3) flags |= cv::CALIB_FIX_K3;
  double rms = cv::calibrateCamera(objPts, imgPts, cv::Size(imgW, imgH), K, D,
                                   rvecs, tvecs, flags);
  res.fx = K.at<double>(0,0); res.fy = K.at<double>(1,1);
  res.cx = K.at<double>(0,2); res.cy = K.at<double>(1,2);
  for (int i = 0; i < 5 && i < D.rows; i++) res.dist[i] = D.at<double>(i,0);
  res.overall_rms_px = rms;
  res.ok = true;
  return res;
}

LensCalibResult lens_calibrate(LensModel model, const std::vector<TelecentricViewData> &views,
                               int imgW, int imgH)
{
  if (model == LensModel::PERSPECTIVE)
    return perspective_calibrate(views, imgW, imgH);

  LensCalibResult res;
  res.model = LensModel::TELECENTRIC;
  TelecentricCalibResult t = telecentric_calibrate(views, imgW, imgH);
  res.ok = t.ok;
  res.overall_rms_px = t.overall_rms_px;
  res.tele = t.intr;
  return res;
}
