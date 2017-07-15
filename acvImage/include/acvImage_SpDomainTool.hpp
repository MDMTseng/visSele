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
#endif
