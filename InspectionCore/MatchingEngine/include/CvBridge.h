#ifndef CV_BRIDGE_H
#define CV_BRIDGE_H

// acvImage <-> cv::Mat bridge for the measurement rework (M1).
// acvImage is BGR interleaved (CVector[oy+y][ (ox+x)*3 + c ], c: 0=B,1=G,2=R),
// rows reached via the CVector row-pointer table, ROI-aware via GetROIOffsetX/Y.
// These COPY (measurement regions are small; correctness first).

#include <opencv2/core.hpp>
#include "acvImage.hpp"

// Channel 0 (B) of the acvImage ROI as a CV_8UC1 Mat. Grayscale images in this
// codebase replicate gray across BGR, so channel 0 == the gray value.
// COPIES (need to demux interleaved BGR -> single channel).
cv::Mat acvImageToGrayMat(acvImage *im);

// Full BGR ROI as a CV_8UC3 Mat. COPIES.
cv::Mat acvImageToBgrMat(acvImage *im);

// Write a CV_8UC1 gray Mat back into the acvImage ROI (replicated to BGR).
// Sizes must match the ROI; out-of-size is clipped. COPIES.
void grayMatToAcvImage(const cv::Mat &g, acvImage *im);

// Load an image from disk via cv::imread (3-channel BGR) and attach an
// acvImage shim over the same memory via useExtBuffer. After this call:
//   - `out_mat` owns the bytes (BGR, contiguous, CV_8UC3).
//   - `out_acv` is a thin wrapper that the legacy engine entry points
//     (FeatureMatching etc.) can consume without copying.
// Both must live in the same scope (out_acv's CVector points into out_mat's
// data). Returns 0 on success, -1 if the image cannot be loaded. This is the
// validated migration primitive for any LoadIMGFile->cv swap; it is exercised
// by the `--insp` path and gated by test_suite/migration_gate.py.
int loadImageCv(const char *path, cv::Mat &out_mat, acvImage &out_acv);
// cv::Mat-only overload (no acvImage shim).
int loadImageCv(const char *path, cv::Mat &out_mat);

// Zero-copy BGR view: returns a cv::Mat header pointing into the acvImage's
// underlying storage. Writes via the returned Mat mutate the acvImage. The view
// is valid only while the acvImage's buffer is alive (and ROI not changed).
// acvImage row layout (CVector[i] = CVector[i-1] + RealWidth*Channel) is
// contiguous, so the BGR pixels can be wrapped without copying. Grayscale
// view is NOT offered because channel 0 lives every-3rd-byte and cv::Mat can't
// express that as a single-channel header without copying.
cv::Mat acvImageBgrView(acvImage *im);

// cv::Mat-native equivalent of acvCloneImage(src, dst, mode).
// mode == -1: dst = src (full BGR copy).
// mode == 0/1/2: extract channel B/G/R, replicate to a CV_8UC3 BGR dst.
// Reallocates dst if its size/type doesn't match.
void cvCloneImage(const cv::Mat &src, cv::Mat &dst, int mode);

// cv::Mat-native equivalent of acvThresholdMap(dst, src, map, mapW, mapH, ch).
// For each pixel, T is bilinearly interpolated from the (mapW x mapH) low-res
// threshold grid spanning the full src extent; dst is BGR with all three
// channels set to 255 if src[channel] > T, else 0. Matches the acv body
// strictly (strict `>`, same fy/fx clamping). Reallocates dst if needed.
void cvThresholdMap(cv::Mat &dst, const cv::Mat &src,
                    const float *threshMap, int mapW, int mapH, int channel);

// cv::Mat-native bilinear sampler. Equivalent to acvUnsignedMap1Sampling on a
// BGR (CV_8UC3) image: returns the float-valued bilinear interpolation of the
// requested channel at (XY.X, XY.Y); NaN if out of bounds (X<0 or Y<0 or
// X+1 > cols-1 or Y+1 > rows-1).
// Nearest-neighbor variant (rounds to int, no interp). Out-of-bounds -> 0
// to match acvUnsignedMap1Sampling_Nearest.
inline uint8_t cvUnsignedMap1Sampling_Nearest(const cv::Mat &m, float x, float y, int channel)
{
    int rX = (int)std::round(x);
    int rY = (int)std::round(y);
    if (rX < 0 || rY < 0 || rX > m.cols - 1 || rY > m.rows - 1) return 0;
    return m.ptr<uint8_t>(rY)[rX * m.channels() + channel];
}

inline float cvUnsignedMap1Sampling(const cv::Mat &m, float x, float y, int channel)
{
    int rX = (int)(x);
    int rY = (int)(y);
    float resX = x - rX;
    float resY = y - rY;
    if (rX < 0 || rY < 0 || rX + 1 > m.cols - 1 || rY + 1 > m.rows - 1)
        return std::nan("");
    const int ch = m.channels();
    const uint8_t *r0 = m.ptr<uint8_t>(rY);
    const uint8_t *r1 = m.ptr<uint8_t>(rY + 1);
    float c00 = r0[rX * ch + channel];
    float c01 = r0[(rX + 1) * ch + channel];
    float c10 = r1[rX * ch + channel];
    float c11 = r1[(rX + 1) * ch + channel];
    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);
    return c00;
}

#endif // CV_BRIDGE_H
