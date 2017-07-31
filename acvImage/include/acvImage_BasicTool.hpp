#ifndef ACV_IMG_BASIC_TOOL_H
#define ACV_IMG_BASIC_TOOL_H
#include "acvImage.hpp"


#pragma pack(push,1)
typedef struct __attribute__((__packed__)) tagBITMAPFILEHEADER
{
    int16_t bfType;  //specifies the file type
    int32_t bfSize;  //specifies the size in bytes of the bitmap file
    int32_t bfReserved;  //reserved; must be 0
    int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
} BITMAPFILEHEADER;
typedef struct __attribute__((__packed__)) tagBITMAPINFOHEADER
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
} BITMAPINFOHEADER;
#pragma pack(pop)

typedef struct acv_XY
{
    float X,Y;
} acv_XY;

void acvThreshold(acvImage *Pic,BYTE Var);
void acvThreshold(acvImage *Pic,BYTE Var,int channel);
void acvThreshold_single(acvImage *Pic,BYTE Var,int channel);
void acvContrast(acvImage *dst, acvImage *src, int offset, int shift,int channel);
void acvDeleteFrame(acvImage *Pic,int width);
void acvDeleteFrame(acvImage *Pic);
void acvClear(acvImage *Pic,BYTE Var);
void acvClear(acvImage *Pic,int channel,BYTE Var);
void acvTurn(acvImage *Pic);
double acvFAtan2( double y, double x );
double acvFAtan(double x);
void acvFullB2W(acvImage *OriPic,acvImage *OutPic);
void acvClone_B2Gray(acvImage *OriPic,acvImage *OutPic);
void acvCloneImage(acvImage *OriPic,acvImage *OutPic,int Mode);
void acvCloneImage_single(acvImage *OriPic, int layer_ori, acvImage *OutPic, int layer_out);

char *PrintHexArr_buff(char *strBuff,int strBuffL,char *data, int dataL);
char *PrintHexArr(char *data, int dataL);
unsigned char *acvLoadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader);
int acvLoadBitmapFile(acvImage *img,char *filename);
int acvSaveBitmapFile(char *filename,unsigned char* pixData,int width,int height);
int acvSaveBitmapFile(char *filename,acvImage *img);
#define div_round(dividend, divisor) (((int)(dividend) + ((int)(divisor) >>1)) / (int)(divisor))
void acvImageAdd(acvImage *src,int num);
#define DoubleRoundInt(Num) ((int)round(Num))
void acvInnerFramePixCopy(acvImage *Pic,int FrameX);
#endif
