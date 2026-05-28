#ifndef TELECENTRIC_CALIB_H
#define TELECENTRIC_CALIB_H

#include <vector>

// Telecentric (orthographic) lens calibration, dependency-free C++ port of the
// reference telecentric_calib.py model:
//
//   P_c   = R * P_w + (tx, ty, 0)        # Tz unobservable, fixed 0
//   xn,yn = P_c.x, P_c.y                 # orthographic projection
//   xd,yd = distort(xn,yn)               # radial(k1,k2,k3)+tangential(p1,p2)
//                                        #   + thin-prism(s1,s2,s3,s4)
//   u,v   = m*(xd,yd) + (u0,v0)          # mm -> pixels
//
// Fit: Levenberg-Marquardt (numeric Jacobian) over 12 intrinsics + per-view
// (rvec, tx, ty).  Intended for offline calibration (runs once).

struct TelecentricIntrinsics
{
  double m = 0;             // magnification, px/mm
  double u0 = 0, v0 = 0;    // principal point, px
  double k1 = 0, k2 = 0, k3 = 0;   // radial
  double p1 = 0, p2 = 0;           // tangential
  double s1 = 0, s2 = 0, s3 = 0, s4 = 0; // thin-prism
};

struct TelecentricView
{
  double rvec[3] = {0, 0, 0};
  double tx = 0, ty = 0;
};

// One view's correspondences: obj (Nx3, mm, Z=0) and img (Nx2, px), row-major.
struct TelecentricViewData
{
  std::vector<double> obj; // 3*N
  std::vector<double> img; // 2*N
  int n = 0;
};

struct TelecentricCalibResult
{
  TelecentricIntrinsics intr;
  std::vector<TelecentricView> views;
  double overall_rms_px = 0;
  std::vector<double> per_view_rms_px;
  bool ok = false;
};

// Project object points of one view to pixels (outUV row-major 2*N).
void telecentric_project(const TelecentricIntrinsics &in, const TelecentricView &v,
                         const double *obj, int n, double *outUV);

// Calibrate from N views. imgW/imgH only used to initialise the principal point.
TelecentricCalibResult telecentric_calibrate(const std::vector<TelecentricViewData> &views,
                                             int imgW, int imgH,
                                             int max_iter = 100, double tol = 1e-12);

#endif
