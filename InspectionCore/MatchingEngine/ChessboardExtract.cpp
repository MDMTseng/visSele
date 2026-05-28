#include "ChessboardExtract.h"
#include <opencv2/opencv.hpp>
#include <map>
#include <algorithm>
#include <cmath>

using std::vector;
using cv::Point2f;
using cv::Point2d;

// run lengths of a circular binary sequence
static vector<int> circularBlobSizes(const vector<int> &b)
{
  vector<int> sizes; int n = (int)b.size(); if (!n) return sizes; int cur = 1;
  for (int i = 1; i < n; i++) { if (b[i] == b[i-1]) cur++; else { sizes.push_back(cur); cur = 1; } }
  if (b[0] == b[n-1] && !sizes.empty()) sizes[0] += cur; else sizes.push_back(cur);
  return sizes;
}

// un-ordered chessboard corner detection
static vector<Point2f> detectCorners(const cv::Mat &img)
{
  const int ringN = 32; const double ringR = 12.0; const int hlDiff = 100; const double minDist = 8.0;
  vector<Point2f> cands;
  cv::goodFeaturesToTrack(img, cands, 4000, 0.01, minDist, cv::noArray(), 3, false);
  int W = img.cols, H = img.rows;
  vector<Point2f> keep;
  for (auto &p : cands)
  {
    if (!(p.x >= ringR && p.x < W - ringR && p.y >= ringR && p.y < H - ringR)) continue;
    vector<int> vals(ringN); int vmin = 255, vmax = 0; double mean = 0;
    for (int k = 0; k < ringN; k++)
    {
      double a = 2 * CV_PI * k / ringN;
      int sx = std::min(std::max((int)lround(p.x + ringR*cos(a)), 0), W-1);
      int sy = std::min(std::max((int)lround(p.y + ringR*sin(a)), 0), H-1);
      int v = img.at<uchar>(sy, sx); vals[k] = v; vmin = std::min(vmin, v); vmax = std::max(vmax, v); mean += v;
    }
    if (vmax - vmin < hlDiff) continue;
    mean /= ringN;
    vector<int> bin(ringN); for (int k = 0; k < ringN; k++) bin[k] = vals[k] > mean ? 1 : 0;
    auto sizes = circularBlobSizes(bin);
    int blobMax = std::max(2, ringN/5);
    if (sizes.size() == 4)
    {
      int mn = *std::min_element(sizes.begin(), sizes.end());
      int mx = *std::max_element(sizes.begin(), sizes.end());
      if (mx - mn <= blobMax) keep.push_back(p);
    }
  }
  if (!keep.empty())
    cv::cornerSubPix(img, keep, cv::Size(11,11), cv::Size(5,5),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 100, 0.01));
  return keep;
}

// least-squares affine [i j 1]->z
static cv::Vec3d lstsqAffine(const vector<double> &I, const vector<double> &J, const vector<double> &Z)
{
  int n = (int)I.size(); cv::Mat A(n, 3, CV_64F), b(n, 1, CV_64F);
  for (int k = 0; k < n; k++) { A.at<double>(k,0)=I[k]; A.at<double>(k,1)=J[k]; A.at<double>(k,2)=1; b.at<double>(k,0)=Z[k]; }
  cv::Mat c; cv::solve(A, b, c, cv::DECOMP_SVD);
  return cv::Vec3d(c.at<double>(0), c.at<double>(1), c.at<double>(2));
}

static bool autoGrid(const vector<Point2f> &C, double sq, vector<double> &obj, vector<double> &img)
{
  int N = (int)C.size(); if (N < 9) return false;
  vector<double> nn(N, 1e18);
  for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) if (i != j) { double d = cv::norm(C[i]-C[j]); if (d < nn[i]) nn[i] = d; }
  vector<double> tmp = nn; std::sort(tmp.begin(), tmp.end()); double ds = tmp[N/2];
  if (ds <= 0) return false;
  vector<double> angs;
  for (int i = 0; i < N; i++) for (int j = i+1; j < N; j++) { double d = cv::norm(C[i]-C[j]); if (d > 0.5*ds && d < 1.4*ds) { Point2f v = C[j]-C[i]; double a = fmod(atan2(v.y, v.x), CV_PI/2); if (a < 0) a += CV_PI/2; angs.push_back(a); } }
  if (angs.size() < 4) return false;
  int bins = 90; vector<int> hist(bins, 0); double bw = (CV_PI/2)/bins;
  for (double a : angs) { int b = std::min((int)(a/bw), bins-1); hist[b]++; }
  int bi = (int)(std::max_element(hist.begin(), hist.end()) - hist.begin());
  double lo = bi*bw, hi = (bi+1)*bw, sum = 0; int cnt = 0;
  for (double a : angs) if (a >= lo && a < hi) { sum += a; cnt++; }
  double theta = cnt ? sum/cnt : 0.5*(lo+hi);
  Point2d e1(cos(theta), sin(theta)), e2(-sin(theta), cos(theta));
  Point2d cen(0, 0); for (auto &p : C) cen += Point2d(p.x, p.y); cen *= 1.0/N;
  vector<double> ii(N), jj(N), cx(N), cy(N);
  for (int k = 0; k < N; k++) { Point2d r(C[k].x-cen.x, C[k].y-cen.y); ii[k] = round((r.x*e1.x + r.y*e1.y)/ds); jj[k] = round((r.x*e2.x + r.y*e2.y)/ds); cx[k] = C[k].x; cy[k] = C[k].y; }
  cv::Vec3d cu, cv_;
  for (int it = 0; it < 8; it++)
  {
    cu = lstsqAffine(ii, jj, cx); cv_ = lstsqAffine(ii, jj, cy);
    double det = cu[0]*cv_[1] - cu[1]*cv_[0]; if (fabs(det) < 1e-12) return false;
    double mi00 = cv_[1]/det, mi01 = -cu[1]/det, mi10 = -cv_[0]/det, mi11 = cu[0]/det;
    bool same = true;
    for (int k = 0; k < N; k++)
    {
      double dx = C[k].x - cu[2], dy = C[k].y - cv_[2];
      double ni = round(dx*mi00 + dy*mi01), nj = round(dx*mi10 + dy*mi11);
      if (ni != ii[k] || nj != jj[k]) same = false; ii[k] = ni; jj[k] = nj;
    }
    if (same) break;
  }
  cu = lstsqAffine(ii, jj, cx); cv_ = lstsqAffine(ii, jj, cy);
  cv::Mat M = (cv::Mat_<double>(2,2) << cu[0], cu[1], cv_[0], cv_[1]); cv::Mat sv; cv::SVD::compute(M, sv);
  double spacing = sv.at<double>(1);
  std::map<std::pair<int,int>, std::pair<int,double>> seen;
  for (int k = 0; k < N; k++)
  {
    double px = ii[k]*cu[0] + jj[k]*cu[1] + cu[2], py = ii[k]*cv_[0] + jj[k]*cv_[1] + cv_[2];
    double res = hypot(C[k].x-px, C[k].y-py);
    if (res < 0.25*spacing)
    {
      auto key = std::make_pair((int)ii[k], (int)jj[k]);
      auto f = seen.find(key);
      if (f == seen.end() || res < f->second.second) seen[key] = std::make_pair(k, res);
    }
  }
  if (seen.size() < 9) return false;
  for (auto &kv : seen)
  {
    int k = kv.second.first;
    obj.push_back(ii[k]*sq); obj.push_back(jj[k]*sq); obj.push_back(0);
    img.push_back(C[k].x);   img.push_back(C[k].y);
  }
  return true;
}

bool chessboard_detect_and_grid(const std::string &imagePath, double square_mm,
                                TelecentricViewData &out, int *imgW, int *imgH)
{
  cv::Mat img = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
  if (img.empty()) return false;
  if (imgW) *imgW = img.cols;
  if (imgH) *imgH = img.rows;
  auto C = detectCorners(img);
  if ((int)C.size() < 9) return false;
  vector<double> obj, im;
  if (!autoGrid(C, square_mm, obj, im)) return false;
  out.obj = obj; out.img = im; out.n = (int)im.size()/2;
  return true;
}

int chessboard_extract_views(const std::vector<std::string> &imagePaths, double square_mm,
                             std::vector<TelecentricViewData> &outViews, int &imgW, int &imgH)
{
  outViews.clear(); imgW = imgH = 0;
  for (auto &p : imagePaths)
  {
    TelecentricViewData d; int w = 0, h = 0;
    if (chessboard_detect_and_grid(p, square_mm, d, &w, &h)) { outViews.push_back(d); imgW = w; imgH = h; }
  }
  return (int)outViews.size();
}
