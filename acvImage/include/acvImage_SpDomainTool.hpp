#ifndef ACV_IMG_SD_DOMAIN_H
#define ACV_IMG_SD_DOMAIN_H


#include "acvImage.hpp"

void acvbBottomEdge(acvImage *Pic);
void acvbBottomEdge(acvImage *OutPic,acvImage *OriPic);
void acvbEdgeDetect(acvImage *OutPic,acvImage *OriPic);
void acvBoxFilter(acvImage *BuffPic,acvImage *Pic,int Size);
void acvBoxFilterX(acvImage *res,acvImage *src,int Size);
void acvBoxFilterY(acvImage *res,acvImage *src,int Size);
void acvMasking(acvImage *OutPic,acvImage *OriPic,unsigned char size,char** Mask);
void acvSharp(acvImage *OutPic,acvImage *OriPic,float SharpLevel);
void acvGSharp(acvImage *OutPic,acvImage *OriPic,float SharpLevel);
void acvGFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve);
void acvSFilter_De(acvImage *Pic,BYTE Valve);
void acvDEGFilter_Separate(acvImage *BuffPic,acvImage *Pic,BYTE Valve,int Channel);
void acvDeGFilter_Gauss(acvImage *BuffPic,acvImage *Pic,int FilterSize,double Deviation,BYTE Valve);
void acvDeGFilter(acvImage *BuffPic,acvImage *Pic,int FilterSize,BYTE Valve);
void acvMidianFilter(acvImage *OutPic,acvImage *OriPic);
void acvVMidianFilter(acvImage *OutPic,acvImage *OriPic);
void acvWExtremeFilter(acvImage *OutPic,acvImage *OriPic);
void acvDeSingularFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve);
void acvErrorFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve);
void acvSDeSpikeFilter(acvImage *OutPic,acvImage *OriPic);
void acvDeSpikeFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve);
void acvGEdgeDetect(acvImage *OutPic,acvImage *OriPic);
void acvLaplace(acvImage *OutPic,acvImage *OriPic);
void acvCLaplace(acvImage *OutPic,acvImage *OriPic);
void acvSobelFilter(acvImage *res,acvImage *src);
void acvSobelFilterX(acvImage *res,acvImage *src);
void acvHarrisCornorResponse(acvImage *buff,acvImage *src);
void acvSGEdgeDetect(acvImage *OutPic,acvImage *OriPic);
void acvCEdgeDetect(acvImage *OutPic,acvImage *OriPic);
void acvCEdgeDetect2(acvImage *OutPic,acvImage *OriPic);



#endif
