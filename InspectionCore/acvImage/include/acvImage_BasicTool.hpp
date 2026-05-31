#ifndef ACV_IMG_BASIC_TOOL_H
#define ACV_IMG_BASIC_TOOL_H
#include "acvImage.hpp"

#include <vector> 
#ifdef _MSC_VER
#pragma pack(push,1)
typedef struct acv_tagBITMAPFILEHEADER
{
	int16_t bfType;  //specifies the file type
	int32_t bfSize;  //specifies the size in bytes of the bitmap file
	int32_t bfReserved;  //reserved; must be 0
	int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
} acv_BITMAPFILEHEADER;
typedef struct acv_tagBITMAPINFOHEADER
{
	int32_t biSize;  //specifies the number of bytes required by the struct
	int32_t biWidth;  //specifies width in pixels
	int32_t biHeight;  //species height in pixels
	int16_t biPlanes; //specifies the number of color planes, must be 1
	int16_t biBitCount; //specifies the number of bit per pixel
	int32_t biCompression;//spcifies the type of compression
	int32_t biSizeImage;  //size of image in bytes
	int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
	int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
	int32_t biClrUsed;  //number of colors used by th ebitmap
	int32_t biClrImportant;  //number of colors that are important
} acv_BITMAPINFOHEADER;
#pragma pack(pop)
#else

#pragma pack(push,1)
typedef struct __attribute__((__packed__)) acv_tagBITMAPFILEHEADER
{
	int16_t bfType;  //specifies the file type
	int32_t bfSize;  //specifies the size in bytes of the bitmap file
	int32_t bfReserved;  //reserved; must be 0
	int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
} acv_BITMAPFILEHEADER;
typedef struct __attribute__((__packed__)) acv_tagBITMAPINFOHEADER
{
	int32_t biSize;  //specifies the number of bytes required by the struct
	int32_t biWidth;  //specifies width in pixels
	int32_t biHeight;  //species height in pixels
	int16_t biPlanes; //specifies the number of color planes, must be 1
	int16_t biBitCount; //specifies the number of bit per pixel
	int32_t biCompression;//spcifies the type of compression
	int32_t biSizeImage;  //size of image in bytes
	int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
	int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
	int32_t biClrUsed;  //number of colors used by th ebitmap
	int32_t biClrImportant;  //number of colors that are important
} acv_BITMAPINFOHEADER;
#pragma pack(pop)
#endif


// Phase 3b: geometry POD types relocated to common_lib/include/vis_geom.h.
// acv_XY is now `cv::Point2f` (`.x`, `.y`); the structs below alias the new
// `vis_*` types for transitional backward-compat across the rest of the code.
#include "vis_geom.h"
using acv_XY        = cv::Point2f;
using acv_Line      = vis_Line;
using acv_Circle    = vis_Circle;
using acv_CircleFit = vis_CircleFit;
using acv_LineFit   = vis_LineFit;




void acvThreshold(acvImage *Pic,BYTE Var);
void acvThreshold(acvImage *Pic,BYTE Var,int channel);
void acvThreshold(acvImage *dst,acvImage *src, BYTE Var, int channel);
// Adaptive per-region threshold from a low-res grid (mapW x mapH) spanning the src ROI.
void acvThresholdMap(acvImage *dst, acvImage *src, const float *threshMap, int mapW, int mapH, int channel);
void acvThreshold_single(acvImage *Pic,BYTE Var,int channel);
void acvHSVThreshold(acvImage *Pic,int HFrom,int HTo,int SMax,int SMin,int VMax,int VMin); //0V ~255  1S ~255  2H ~252
void acvContrast(acvImage *dst, acvImage *src, int offset, int shift,int channel);
void acvDeleteFrame(acvImage *Pic,int width=1,int val=255);
void acvClear(acvImage *Pic,BYTE Var);
void acvClear(acvImage *Pic,int channel,BYTE Var);
void acvTurn(acvImage *Pic);
void acvFullB2W(acvImage *OriPic,acvImage *OutPic);
void acvClone_B2Gray(acvImage *OriPic,acvImage *OutPic);
void acvCloneImage(acvImage *OriPic,acvImage *OutPic,int Mode);
void acvCropImage(acvImage *OriPic, acvImage *OutPic, int X,int Y,int W,int H);
void acvCloneImage_single(acvImage *OriPic, int layer_ori, acvImage *OutPic, int layer_out);

char *PrintHexArr_buff(char *strBuff,int strBuffL,char *data, int dataL);
char *PrintHexArr(char *data, int dataL);
unsigned char *acvLoadBitmapFile(const char *filename, acv_BITMAPINFOHEADER *bitmapInfoHeader);
int acvLoadBitmapFile(acvImage *img,const char *filename);
int acvSaveBitmapFile(const char *filename,unsigned char* pixData,int width,int height);
int acvSaveBitmapFile(const char *filename,acvImage *img);
#define div_round(dividend, divisor) (((int)(dividend) + ((int)(divisor) >>1)) / (int)(divisor))
#define sh_round(dividend, shift) (((int)(dividend) + ((int)(1) <<(shift-1))) >>(shift))
void acvImageAdd(acvImage *src,int num);
#define DoubleRoundInt(Num) ((int)round(Num))
void acvInnerFramePixCopy(acvImage *Pic,int FrameX);
// Geometry function declarations moved to vis_geom.h.
#endif
