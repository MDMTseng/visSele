#ifndef SIGNATURE_TOOLS_H
#define SIGNATURE_TOOLS_H
// Signature matching, lifted out of acvImage/acvImage_ToolBox.hpp when acvImage
// was retired. Everything here is cv::Point2f + std::vector; none of it ever
// needed the old image container.
//
// Dropped in the move, because nothing outside acvImage referenced them:
// acvSpatialMatchingError, acvSpatialMatchingGradient, acvSqMatchingError,
// acvContourExtract, acvOuterContourExtraction, acvLabeledSignatureByContour,
// and the acvImage* overloads of acvContourCircleSignature.
#include <math.h>
#include <vector>
#include "vis_geom.h"     // acv_XY (= cv::Point2f), acv_LabeledData

// Contour -> circular signature (X = magnitude, Y = angle).
bool acvContourCircleSignature(std::vector<acv_XY> &contour,
                               std::vector<acv_XY> &signature);
bool acvContourCircleSignature(acv_XY center,
                               std::vector<acv_XY> &contour,
                               std::vector<acv_XY> &o_signature);

float SignatureMatchingError(const acv_XY *signature, int offset,
                             const acv_XY *tar_signature, int arrsize, int stride);
float SignatureMatchingError(const std::vector<acv_XY> &signature, int offset,
                             const std::vector<acv_XY> &tar_signature, int stride);
float SignatureMatchingError(const acv_XY *signature, float offset,
                             const acv_XY *tar_signature, int arrsize,
                             int stride = 1, bool reverse = false);

float SignareIdxOffsetMatching(const std::vector<acv_XY> &signature,
                               const std::vector<acv_XY> &tar_signature,
                               int roughSearchSampleRate, float *min_error);

// Angle from signature to tar_signature: signature o rotate(angle) == tar.
float SignatureAngleMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature,
                             float searchAngleOffset, float searchAngleMargin,
                             float *min_error);

// searchAngleOffset/Range and facing limit the matching range;
// facing is -1 (back only) / 0 (either) / 1 (front only).
float SignatureMinMatching(std::vector<acv_XY> &signature,
                           const std::vector<acv_XY> &tar_signature,
                           float searchAngleOffset, float searchAngleRange, int facing,
                           bool *ret_isInv, float *ret_angle);

void SignatureSoften(std::vector<acv_XY> &signature, int windowR = 1);
void SignatureSoften(std::vector<acv_XY> &signature,
                     std::vector<acv_XY> &output, int windowR);
void SignatureSharpen(std::vector<acv_XY> &signature, int windowR, float alpha);
void SignatureReverse(std::vector<acv_XY> &dst, std::vector<acv_XY> &src);

int interpolateSignData(std::vector<acv_XY> &signature, int start, int end);

#endif
