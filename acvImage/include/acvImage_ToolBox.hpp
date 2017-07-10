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

double GetRMSE(acvImage *Pic1,acvImage *Pic2);
int RoundTypeChoose(float Num,char *SearchType);
void acvRingDataDiffFilter(int * OutRingArray,int * InRingArray,int RingSize);
void acvRingDataDeZero(int * OutRingArray,int * InRingArray,int RingSize);
void acvRingDataDeZero2(int * OutRingArray,int * InRingArray,int RingSize);
int RingDataTotalRMSE(int *DataArray,int * StadardDataArray,int RingSize);
int RingDataRMSE(int *DataArray,int * StadardDataArray,int RingSize,int CompareChannel);
double RingDataTotalBhattacharyya(int *DataArray,int * StadardDataArray,int RingSize);
double RingDataBhattacharyya(int *DataArray,int * StadardDataArray,int RingSize,int CompareChannel);
void acvGetHisto(acvImage *Pic,int *Histo,int ROIX1,int ROIY1,int ROIX2,int ROIY2);
int RingDataGetMean(int * RingArray,int RingSize);
double GetBarCodeRMSE(acvImage *BarCodeRule,acvImage *CheckPic);
void acvSignatureFindPalmFinger(int* MeanSign,int* Signature,int AngleCentral,int* FingerNum);
void acvContourSignature(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature,int InitDir);
void acvContourSignatureWPA(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature,int *PresciseAngle,int InitDir);
void acvContourSignature2(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature);
void acvDrawContour(acvImage  *Pic,int FromX,int FromY,BYTE B,BYTE G,BYTE R,char InitDir);
void acvComponentLabelingSim(acvImage *Pic);
int acvFindContourCentroid(acvImage  *Pic,int Pos[2],BYTE RecordVar,int InitDir);
void acvCornerMap(acvImage *OutPic,acvImage *OriPic);    //Hue 000~126 127~251
int SignatureFindPolygonWPA(int* VertexPosition,int* Signature,int *PresciseAngle);

#endif
