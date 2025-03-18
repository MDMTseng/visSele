#include "InspectProcess.h"
#include "ConversionUtil.h"
#include <opencv2/opencv.hpp>

// No plugin interface functions defined here anymore

void processImage(ImageInfo* imgInfo) {
    if (!imgInfo || !imgInfo->buffer) {
        return;
    }
    
    // Convert ImageInfo to Mat without copying (shared buffer)
    cv::Mat image = ImageInfoToMat(imgInfo, false);
    
    // Draw a green line on the image
    cv::line(image, cv::Point(50, 300), cv::Point(450, 300), 
             cv::Scalar(0, 255, 0), 3);
    
    // No need to convert back, as we're directly modifying the shared buffer
    // Changes to the Mat are immediately reflected in the ImageInfo's buffer
}
