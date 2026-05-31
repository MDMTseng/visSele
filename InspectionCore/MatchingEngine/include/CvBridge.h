#ifndef CV_BRIDGE_H
#define CV_BRIDGE_H

// cv::Mat helpers used across MatchingEngine + Core0_1.

#include <opencv2/core.hpp>
#include <cmath>
#include <cstdint>

// Load an image from disk via cv::imread (3-channel BGR).
// Returns 0 on success, -1 if the image cannot be loaded.
int loadImageCv(const char *path, cv::Mat &out_mat);

// Equivalent of the legacy acvCloneImage(src, dst, mode):
// mode == -1: dst = src (full BGR copy).
// mode == 0/1/2: extract channel B/G/R, replicate to a CV_8UC3 BGR dst.
// Reallocates dst if its size/type doesn't match.
void cvCloneImage(const cv::Mat &src, cv::Mat &dst, int mode);

// For each pixel, T is bilinearly interpolated from the (mapW x mapH) low-res
// threshold grid spanning the full src extent; dst is BGR with all three
// channels set to 255 if src[channel] > T, else 0.
void cvThresholdMap(cv::Mat &dst, const cv::Mat &src,
                    const float *threshMap, int mapW, int mapH, int channel);

// Nearest-neighbor bilinear sampler (single channel from a BGR/multi-channel
// CV_8UC* image). Out-of-bounds -> 0.
inline uint8_t cvUnsignedMap1Sampling_Nearest(const cv::Mat &m, float x, float y, int channel)
{
    int rX = (int)std::round(x);
    int rY = (int)std::round(y);
    if (rX < 0 || rY < 0 || rX > m.cols - 1 || rY > m.rows - 1) return 0;
    return m.ptr<uint8_t>(rY)[rX * m.channels() + channel];
}

// Bilinear sampler (single channel from a BGR/multi-channel CV_8UC* image).
// Returns NaN when the 2x2 neighborhood would be out of bounds.
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
