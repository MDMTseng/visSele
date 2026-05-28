#include "LensCalib.h"
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>

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

void lens_undistort_points(const LensCalibResult &r, const double *uv, int n, double *outUV)
{
  if (r.model == LensModel::TELECENTRIC)
  {
    const TelecentricIntrinsics &in = r.tele;
    for (int i = 0; i < n; i++)
    {
      double xd = (uv[i*2]   - in.u0) / in.m;
      double yd = (uv[i*2+1] - in.v0) / in.m;
      double xn = xd, yn = yd;
      for (int it = 0; it < 30; it++) // iterative inverse of the distortion
      {
        double r2 = xn*xn + yn*yn, r4 = r2*r2, r6 = r4*r2;
        double radial = 1.0 + in.k1*r2 + in.k2*r4 + in.k3*r6;
        double dx = 2.0*in.p1*xn*yn + in.p2*(r2+2.0*xn*xn) + in.s1*r2 + in.s2*r4;
        double dy = in.p1*(r2+2.0*yn*yn) + 2.0*in.p2*xn*yn + in.s3*r2 + in.s4*r4;
        double xnn = (xd - dx)/radial, ynn = (yd - dy)/radial;
        if (fabs(xnn-xn) < 1e-12 && fabs(ynn-yn) < 1e-12) { xn=xnn; yn=ynn; break; }
        xn = xnn; yn = ynn;
      }
      // ideal (distortion-free) pixel position
      outUV[i*2]   = in.m*xn + in.u0;
      outUV[i*2+1] = in.m*yn + in.v0;
    }
    return;
  }
  // perspective: OpenCV iterative inverse, reproject with K (-> undistorted px)
  cv::Mat K = (cv::Mat_<double>(3,3) << r.fx,0,r.cx, 0,r.fy,r.cy, 0,0,1);
  cv::Mat D = (cv::Mat_<double>(5,1) << r.dist[0],r.dist[1],r.dist[2],r.dist[3],r.dist[4]);
  std::vector<cv::Point2d> src(n), dst;
  for (int i = 0; i < n; i++) src[i] = cv::Point2d(uv[i*2], uv[i*2+1]);
  cv::undistortPoints(src, dst, K, D, cv::noArray(), K);
  for (int i = 0; i < n; i++) { outUV[i*2]=dst[i].x; outUV[i*2+1]=dst[i].y; }
}

char *lens_calib_to_json(const LensCalibResult &r)
{
  char buf[1024];
  if (r.model == LensModel::TELECENTRIC)
  {
    const TelecentricIntrinsics &t = r.tele;
    snprintf(buf, sizeof(buf),
      "{\"lens_model\":\"telecentric\",\"ok\":%s,\"rms_px\":%.6g,"
      "\"m\":%.10g,\"u0\":%.10g,\"v0\":%.10g,"
      "\"k1\":%.10g,\"k2\":%.10g,\"k3\":%.10g,\"p1\":%.10g,\"p2\":%.10g,"
      "\"s1\":%.10g,\"s2\":%.10g,\"s3\":%.10g,\"s4\":%.10g,"
      "\"um_per_px\":%.10g}",
      r.ok?"true":"false", r.overall_rms_px,
      t.m,t.u0,t.v0,t.k1,t.k2,t.k3,t.p1,t.p2,t.s1,t.s2,t.s3,t.s4,
      (t.m!=0)?1000.0/t.m:0.0);
  }
  else
  {
    snprintf(buf, sizeof(buf),
      "{\"lens_model\":\"perspective\",\"ok\":%s,\"rms_px\":%.6g,"
      "\"fx\":%.10g,\"fy\":%.10g,\"cx\":%.10g,\"cy\":%.10g,"
      "\"k1\":%.10g,\"k2\":%.10g,\"p1\":%.10g,\"p2\":%.10g,\"k3\":%.10g}",
      r.ok?"true":"false", r.overall_rms_px,
      r.fx,r.fy,r.cx,r.cy,r.dist[0],r.dist[1],r.dist[2],r.dist[3],r.dist[4]);
  }
  char *out = (char*)malloc(strlen(buf)+1);
  strcpy(out, buf);
  return out;
}
