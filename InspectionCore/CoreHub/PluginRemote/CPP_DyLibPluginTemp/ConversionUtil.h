#ifndef CONVERSION_UTIL_H
#define CONVERSION_UTIL_H

#include <opencv2/opencv.hpp>
#include "PluginInterface.h"


// Convert OpenCV Mat to ImageInfo
// If copyData is false, ImageInfo will reference Mat's data (Mat must stay alive)
ImageInfo* MatToImageInfo(const cv::Mat& mat, bool copyData = true);

// Convert ImageInfo back to OpenCV Mat
// If copyData is false, Mat will reference ImageInfo's buffer (ImageInfo must stay alive)
cv::Mat ImageInfoToMat(const ImageInfo* imgInfo, bool copyData = true);

// Free memory allocated for ImageInfo
// If releaseBuffer is false, the buffer pointer will not be freed (useful when buffer is shared)
void FreeImageInfo(ImageInfo* imgInfo, bool releaseBuffer = true);


#endif // CONVERSION_UTIL_H 