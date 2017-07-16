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

#endif
