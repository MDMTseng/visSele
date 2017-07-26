/*#include <math.h>

//#include "DynamicIntArray.cpp"
//#include "acvImage.cpp"
#include "acvImage_EnhenceTool.cpp"
#include "acvImage_SpDomainTool.cpp"
#include "acvImage_MophologyTool.cpp"
#include "acvImage_BasicDrawTool.cpp"
#include "acvImage_ComponentLabelingTool.cpp"
#include "acvImage_ImageBlanderTool.cpp"
#include "acvImage_OtherTool.cpp"
#include "acvMatrix.cpp"*/


#ifndef ACV_IMG_TOOL_BOX_H
#define ACV_IMG_TOOL_BOX_H
#include <math.h>
#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"
#include "acvImage_ComponentLabelingTool.hpp"


float acvSpatialMatchingGradient(acvImage  *Pic,acv_XY *PicPtList,
                                 acvImage *targetMap,acvImage *targetSobel,acv_XY *TarPtList,
                                 acv_XY *ErrorGradientList,int ListL);

uint32_t acvSqMatchingError(acvImage  *Pic,acv_XY *PicPtList,acvImage *targetMap,acv_XY *TarPtList,int ListL);
bool acvContourCircleSignature(acvImage  *LabeledPic,acv_LabeledData ldata,int labelIdx,std::vector<acv_XY> &signature);
float SignatureMatchingError(const acv_XY *signature, int offset,
                             const acv_XY *tar_signature, int arrsize, int stride);
float SignatureMatchingError(const std::vector<acv_XY> &signature, int offset,
                             const std::vector<acv_XY> &tar_signature, int stride);
int SignareIdxOffsetMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature, int roughSearchSampleRate, float *min_error);
float SignatureAngleMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature, float *min_error);
#endif
