#ifndef ACV_IMG_MOPH_TOOL_H
#define ACV_IMG_MOPH_TOOL_H

#include "acvImage.hpp"

void acvbDilation(acvImage *BuffPic,acvImage *Pic,int Size);       //Max
void acvbErosion(acvImage *BuffPic,acvImage *Pic,int Size);       //Min
void acvbOpening(acvImage *BuffPic,acvImage *OriPic,int Size);
void acvbClosing(acvImage *BuffPic,acvImage *OriPic,int Size);
void acvImAnd(acvImage *Pic1,acvImage *Pic2);
void acvImAndColor(acvImage *Pic1,acvImage *Pic2);
void acvImOr(acvImage *Pic1,acvImage *Pic2);
void acvImNot(acvImage *Pic);
void acvImXor(acvImage *Pic1,acvImage *Pic2);
void acvCountXB(int* X,int*B,acvImage *Pic,int i,int j,BYTE Valve);
void acvReFresh(acvImage *Pic);
void acvZ_S_Skelet(acvImage *Pic);
void acvWindowMax(acvImage *Pic, int Size);

#endif
