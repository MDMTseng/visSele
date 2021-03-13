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


typedef struct acv_XY
{
    float X,Y;
} acv_XY;


typedef struct acv_Circle
{
    acv_XY circumcenter;
	float radius;
} acv_Circle;

typedef struct acv_Line {
  acv_XY line_vec;
  acv_XY line_anchor;
} acv_Line;


typedef struct acv_CircleFit
{
  acv_Circle circle;
  int matching_pts;
  float s;//sigma
}acv_CircleFit;

typedef struct acv_LineFit
{
  acv_Line line;
  int matching_pts;

  acv_XY end_pt1;
  acv_XY end_pt2;
  float s;//sigma
}acv_LineFit;




void acvThreshold(acvImage *Pic,BYTE Var);
void acvThreshold(acvImage *Pic,BYTE Var,int channel);
void acvThreshold(acvImage *dst,acvImage *src, BYTE Var, int channel);
void acvThreshold_single(acvImage *Pic,BYTE Var,int channel);
void acvHSVThreshold(acvImage *Pic,int HFrom,int HTo,int SMax,int SMin,int VMax,int VMin); //0V ~255  1S ~255  2H ~252
void acvContrast(acvImage *dst, acvImage *src, int offset, int shift,int channel);
void acvDeleteFrame(acvImage *Pic,int width=1,int val=255);
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
unsigned char *acvLoadBitmapFile(const char *filename, acv_BITMAPINFOHEADER *bitmapInfoHeader);
int acvLoadBitmapFile(acvImage *img,const char *filename);
int acvSaveBitmapFile(const char *filename,unsigned char* pixData,int width,int height);
int acvSaveBitmapFile(const char *filename,acvImage *img);
#define div_round(dividend, divisor) (((int)(dividend) + ((int)(divisor) >>1)) / (int)(divisor))
#define sh_round(dividend, shift) (((int)(dividend) + ((int)(1) <<(shift-1))) >>(shift))
void acvImageAdd(acvImage *src,int num);
#define DoubleRoundInt(Num) ((int)round(Num))
void acvInnerFramePixCopy(acvImage *Pic,int FrameX);
acv_XY acvIntersectPoint(acv_XY p1,acv_XY p2,acv_XY p3,acv_XY p4);
acv_XY acvCircumcenter(acv_XY p1,acv_XY p2,acv_XY p3);
float acv2DCrossProduct(acv_XY v1,acv_XY v2);
float acv2DDotProduct(acv_XY v1,acv_XY v2);
float acvVectorOrder(acv_XY p1,acv_XY p2,acv_XY p3);
float acvDistance(acv_XY p1,acv_XY p2);
acv_XY acvVecNormal(acv_XY vec);
acv_XY acvVecNormalize(acv_XY vec);
acv_XY acvVecInterp(acv_XY vec1,acv_XY vec2,float alpha);
acv_XY acvVecAdd(acv_XY vec1,acv_XY vec2);
acv_XY acvVecSub(acv_XY vec1,acv_XY vec2);
acv_XY acvVecMult(acv_XY vec1,float mult);


acv_XY acvComplexAdd(acv_XY a,acv_XY b);//a+b
acv_XY acvComplexSub(acv_XY a,acv_XY b);
acv_XY acvComplexMult(acv_XY a,acv_XY b);//a*b
acv_XY acvComplexDiv(acv_XY a,acv_XY b);//a/b
acv_XY acvRotation(float sine,float cosine,float flip_f,acv_XY input);
acv_XY acvRotation(float sine,float cosine,acv_XY input);
acv_XY acvRotation(float angle,acv_XY input);

acv_XY acvLineIntersect(acv_Line line1, acv_Line line2);
acv_XY acvClosestPointOnLine(acv_XY point, acv_Line line);
acv_XY acvClosestPointOnCircle(acv_XY point, acv_Circle circle);
float acvDistance_Signed(acv_Line line, acv_XY point);
float acvDistance(acv_Line line, acv_XY point);
float acvDistance_Signed(acv_Circle cir, acv_XY point);
float acvDistance(acv_Circle cir, acv_XY point);
float acvLineAngle(acv_Line line1,acv_Line line2);
float acvVectorAngle(acv_XY v1,acv_XY v2);
bool acvFitLine(const acv_XY *pts, int ptsL,acv_Line *line, float *ret_sigma);
bool acvFitLine(const acv_XY *pts, const float *ptsw, int ptsL,acv_Line *line, float *ret_sigma);
bool acvFitLine(const void *pts_struct,int pts_step, const void *ptsw_struct,int ptsw_step, int ptsL,acv_Line *line, float *ret_sigma);
#endif
