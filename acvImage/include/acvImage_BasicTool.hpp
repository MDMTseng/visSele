#ifndef ACV_IMG_BASIC_TOOL_H
#define ACV_IMG_BASIC_TOOL_H
#include "acvImage.hpp"



typedef struct __attribute__((__packed__)) tagBITMAPFILEHEADER
{
    __int16_t bfType;  //specifies the file type
    __int32_t bfSize;  //specifies the size in bytes of the bitmap file
    __int32_t bfReserved;  //reserved; must be 0
    __int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
}BITMAPFILEHEADER;
typedef struct __attribute__((__packed__)) tagBITMAPINFOHEADER
{
    __int32_t biSize;  //specifies the number of bytes required by the struct
    __int32_t biWidth;  //specifies width in pixels
    __int32_t biHeight;  //species height in pixels
    __int16_t biPlanes; //specifies the number of color planes, must be 1
    __int16_t biBitCount; //specifies the number of bit per pixel
    __int32_t biCompression;//spcifies the type of compression
    __int32_t biSizeImage;  //size of image in bytes
    __int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
    __int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
    __int32_t biClrUsed;  //number of colors used by th ebitmap
    __int32_t biClrImportant;  //number of colors that are important
}BITMAPINFOHEADER;



void acvThreshold(acvImage *Pic,BYTE Var);
void acvDeletFrame(acvImage *Pic);
void acvClear(acvImage *Pic,BYTE Var);
void acvTurn(acvImage *Pic);
void acvFullB2W(acvImage *OriPic,acvImage *OutPic);
void acvClone_B2Gray(acvImage *OriPic,acvImage *OutPic);
void acvCloneImage(acvImage *OriPic,acvImage *OutPic,int Mode);
int DoubleRoundInt(double Num);
char *PrintHexArr_buff(char *strBuff,int strBuffL,char *data, int dataL);
char *PrintHexArr(char *data, int dataL);
unsigned char *LoadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader);
unsigned int LoadBitmapFile(acvImage *img,char *filename);
int SaveBitmapFile(char *filename,unsigned char* pixData,int width,int height);
#endif
