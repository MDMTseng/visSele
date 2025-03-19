#include "ConversionUtil.h"
#include <cstring>

ImageInfo* MatToImageInfo(const cv::Mat& mat, bool refrenceBuffer) {
    if (mat.empty()) {
        return nullptr;
    }

    ImageInfo* imgInfo = new ImageInfo();
    
    // Fill in the metadata
    imgInfo->width = mat.cols;
    imgInfo->height = mat.rows;
    imgInfo->channels = mat.channels();
    imgInfo->step = static_cast<int>(mat.step);
    imgInfo->type = mat.type();
    imgInfo->elemSize = static_cast<int>(mat.elemSize());
    imgInfo->totalSize = static_cast<int>(mat.total() * mat.elemSize());
    
    if (!refrenceBuffer) {
        // Allocate and copy the image data
        imgInfo->buffer = malloc(imgInfo->totalSize);
        if (imgInfo->buffer) {
            memcpy(imgInfo->buffer, mat.data, imgInfo->totalSize);
        }
        imgInfo->refCount = 1;  // We own this buffer
    } else {
        // Just reference the Mat's data buffer
        imgInfo->buffer = mat.data;
        imgInfo->refCount = 0;  // We don't own this buffer
    }
    
    return imgInfo;
}

cv::Mat ImageInfoToMat(const ImageInfo* imgInfo, bool refrenceBuffer) {
    if (!imgInfo || !imgInfo->buffer) {
        return cv::Mat();
    }
    
    // Create a Mat header for the existing data
    cv::Mat mat(imgInfo->height, imgInfo->width, 
                imgInfo->type, 
                imgInfo->buffer, 
                imgInfo->step);
    
    if (!refrenceBuffer) {
        // Create a deep copy to ensure the data is owned by the Mat
        return mat.clone();
    } else {
        // Return a Mat that references the ImageInfo buffer
        // Note: The ImageInfo must remain valid as long as this Mat is used
        return mat;
    }
}

void FreeImageInfo(ImageInfo* imgInfo, bool releaseBuffer) {
    if (imgInfo) {
        if (imgInfo->buffer && releaseBuffer && imgInfo->refCount > 0) {
            // Only free the buffer if we own it (refCount > 0) and are asked to release it
            free(imgInfo->buffer);
            imgInfo->buffer = nullptr;
        }
        delete imgInfo;
    }
} 