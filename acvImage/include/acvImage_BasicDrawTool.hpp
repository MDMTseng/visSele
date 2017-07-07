#ifndef ACV_IMG_BASIC_DRAW_TOOL_H
#define ACV_IMG_BASIC_DRAW_TOOL_H
#include "acvImage.hpp"

int acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB);
void acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB,int LineWidth);
void acvDrawLine_P(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB,int LineWidth);
void acvDrawDots(acvImage *Pic,double* Dots,int XMin,int XMax,double YMin,double YMax,BYTE LineR,BYTE LineG,BYTE LineB);
void acvDrawDots(acvImage *Pic,int* Dots,int XMin,int XMax,double YMin,double YMax,BYTE LineR,BYTE LineG,BYTE LineB);
int acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2);
void acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,int LineWidth);
void acvDrawDots(acvImage *Pic,const double* Dots,int XMin,int XMax,double YMin,double YMax);
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize);
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,int LineWidth);
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B);
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B,int LineWidth);
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize);
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,int LineWidth);
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B);
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B,int LineWidth);
void acvDrawBlock(acvImage *Pic,int X1,int Y1,int X2,int Y2);
void acvDrawReverseFillBlock(acvImage *Pic,int X1,int Y1,int X2,int Y2);

#endif
