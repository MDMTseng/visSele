#ifndef ACV_IMG_SD_DOMAIN_H
#define ACV_IMG_SD_DOMAIN_H


#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"


typedef struct acvDT_distRotate
{
    int dist;
    int distX;
} acvDT_distRotate;

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



inline acv_XY acvSignedMap2Sampling(acvImage *signedMap2, const acv_XY &XY)
{
    acv_XY sample;
    int rX = (int)(XY.X);
    int rY = (int)(XY.Y);
    float resX = XY.X - rX;
    float resY = XY.Y - rY;

    float c00, c10, c11, c01;
    BYTE *cvL1=signedMap2->CVector[rY]+rX*3;
    BYTE *cvL2=signedMap2->CVector[rY+1]+rX*3;
    c00 = (char)cvL1[0];
    c01 = (char)cvL1[3];
    c10 = (char)cvL2[0];
    c11 = (char)cvL2[3];
    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);
    sample.X = (c00);

    c00 = (char)cvL1[0+1];
    c01 = (char)cvL1[3+1];
    c10 = (char)cvL2[0+1];
    c11 = (char)cvL2[3+1];

    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);
    sample.Y = (c00);

    return sample;
}

inline float acvUnsignedMap1Sampling(acvImage *unsignedMap1, const acv_XY &XY)
{
    int rX = (int)(XY.X);
    int rY = (int)(XY.Y);
    float resX = XY.X - rX;
    float resY = XY.Y - rY;

    float c00, c10, c11, c01;
    c00 = unsignedMap1->CVector[rY][rX * 3];
    c01 = unsignedMap1->CVector[rY][(rX + 1) * 3];
    c10 = unsignedMap1->CVector[rY + 1][rX * 3];
    c11 = unsignedMap1->CVector[rY + 1][(rX + 1) * 3];
    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);

    return c00;
}

inline acv_XY acvSignedMap2Sampling_Nearest(acvImage *signedMap2, const acv_XY &XY)
{

    acv_XY sample;
    int rX = (int)round(XY.X);
    int rY = (int)round(XY.Y);
    sample.X = (char)signedMap2->CVector[rY][rX * 3];
    sample.Y = (char)signedMap2->CVector[rY][rX * 3 + 1];
    return sample;
}

inline float acvUnsignedMap1Sampling_Nearest(acvImage *unsignedMap1, const acv_XY &XY)
{

    int rX = (int)round(XY.X);
    int rY = (int)round(XY.Y);
    return unsignedMap1->CVector[rY][rX * 3];
}
#endif
