#ifndef ACV_IMG_SD_DOMAIN_H
#define ACV_IMG_SD_DOMAIN_H


#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"


typedef struct acvDT_distRotate
{
  int dist;
  int distX;
}acvDT_distRotate;

void acvBoxFilter(acvImage *BuffPic,acvImage *Pic,int Size);
void acvBoxFilterX(acvImage *res,acvImage *src,int Size);
void acvBoxFilterY(acvImage *res,acvImage *src,int Size);
void acvBoxFilter_round(acvImage *BuffPic,acvImage *Pic,int Size);
void acvBoxFilterX_round(acvImage *res,acvImage *src,int Size);
void acvBoxFilterY_round(acvImage *res,acvImage *src,int Size);
void acvIIROrder1FilterX(acvImage *res,acvImage *src,int shifter);
void acvIIROrder1FilterY(acvImage *res,acvImage *src,int shifter);
void acvIIROrder1Filter(acvImage *BuffPic,acvImage *Pic,int shifter);
void acvMasking(acvImage *OutPic,acvImage *OriPic,unsigned char size,char** Mask);
void acvSobelFilter(acvImage *res,acvImage *src);
void acvSobelFilterX(acvImage *res,acvImage *src);
void acvHarrisCornorResponse(acvImage *buff,acvImage *src);
void acvDistanceTransform_Chamfer(acvImage *src,int dist,int distX);
void acvDistanceTransform_ChamferX(acvImage *src);
void acvDistanceTransform_Chamfer(acvImage *src,acvDT_distRotate *distList,int distListL);
void acvDistanceTransform_Sobel(acvImage *res,acvImage *src);
acv_XY acvSignedMap2Sampling(acvImage *signedMap2,const acv_XY &XY);
float acvUnsignedMap1Sampling(acvImage *unsignedMap1,const acv_XY &XY);
acv_XY acvSignedMap2Sampling_Nearest(acvImage *signedMap2,const acv_XY &XY);
float acvUnsignedMap1Sampling_Nearest(acvImage *unsignedMap1,const acv_XY &XY);
void acvBinaryImageEdge(acvImage *res,acvImage *src);
#endif
