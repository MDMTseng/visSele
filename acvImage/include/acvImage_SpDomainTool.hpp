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
void acvBoxFilter_inCH(acvImage *Pic, int Size);
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
void acvBinaryImageEdge(acvImage *res,acvImage *src);

void acvBoxFilterY_BL(acvImage *res, acvImage *src, int Size,int *LineBuff,int LineBuffL);
void acvBoxFilter_naive(acvImage *BuffPic, acvImage *Pic, int Size);

inline acv_XY acvSignedMap2Sampling(acvImage *signedMap2, const acv_XY &XY)
{
    acv_XY sample;
    int rX = (int)(XY.X);
    int rY = (int)(XY.Y);
    float resX = XY.X - rX;
    float resY = XY.Y - rY;

    if(rX<0 || rY <0 || rX+1 >signedMap2->GetWidth()-1 ||  rY+1 >signedMap2->GetHeight()-1)
    {
      sample.X=0;
      sample.Y=0;
      return sample;
    }

    float c00, c10, c11, c01;
    int8_t *cvL1=(int8_t *)(signedMap2->CVector[rY]+rX*3);
    int8_t *cvL2=(int8_t *)(signedMap2->CVector[rY+1]+rX*3);
    c00 = cvL1[0];
    c01 = cvL1[3];
    c10 = cvL2[0];
    c11 = cvL2[3];
    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);
    sample.X = (c00);

    c00 = cvL1[0+1];
    c01 = cvL1[3+1];
    c10 = cvL2[0+1];
    c11 = cvL2[3+1];

    c00 += resX * (c01 - c00);
    c10 += resX * (c11 - c10);
    c00 += resY * (c10 - c00);
    sample.Y = (c00);

    return sample;
}

inline float acvUnsignedMap1Sampling(acvImage *unsignedMap1, acv_XY XY, int channel)
{
    int rX = (int)(XY.X);
    int rY = (int)(XY.Y);
    float resX = XY.X - rX;
    float resY = XY.Y - rY;
    if(rX<0 || rY <0 || rX+1 >unsignedMap1->GetWidth()-1 ||  rY+1 >unsignedMap1->GetHeight()-1)
      return 0;
    float c00, c10, c11, c01;
    c00 = unsignedMap1->CVector[rY][rX * 3+channel];
    c01 = unsignedMap1->CVector[rY][(rX + 1) * 3+channel];
    c10 = unsignedMap1->CVector[rY + 1][rX * 3+channel];
    c11 = unsignedMap1->CVector[rY + 1][(rX + 1) * 3+channel];
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

    if(rX<0 || rY <0 || rX >signedMap2->GetWidth()-1 ||  rY >signedMap2->GetHeight()-1)
    {
      sample.X=0;
      sample.Y=0;
      return sample;
    }
    sample.X = (int8_t)signedMap2->CVector[rY][rX * 3];
    sample.Y = (int8_t)signedMap2->CVector[rY][rX * 3 + 1];
    return sample;
}

inline BYTE acvUnsignedMap1Sampling_Nearest(acvImage *unsignedMap1, const acv_XY &XY, int channel)
{

    int rX = (int)round(XY.X);
    int rY = (int)round(XY.Y);
      if(rX<0 || rY <0 || rX >unsignedMap1->GetWidth()-1 ||  rY >unsignedMap1->GetHeight()-1)
        return 0;
    return unsignedMap1->CVector[rY][rX * 3+channel];
}


inline float acvLinearInterpolation( float v1, float v2, float alpha)
{
    v1 += alpha * (v2 - v1);
    return v1;
}
#endif
