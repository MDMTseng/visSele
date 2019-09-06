#define _USE_MATH_DEFINES
#include <math.h>
#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"

void acvThreshold(acvImage *Pic, BYTE Var, int channel)
{
    BYTE *BMPLine;
    int Height = Pic->GetROIOffsetY() + Pic->GetHeight(),
        Width = Pic->GetROIOffsetX() + Pic->GetWidth();
    for (int i = Pic->GetROIOffsetY(); i < Height; i++)
    {
        BMPLine = Pic->CVector[i] + Pic->GetROIOffsetX() * 3;
        for (int j = Pic->GetROIOffsetX(); j < Width; j++)

            if (BMPLine[channel] > Var)
            {
                *BMPLine++ = 255;
                *BMPLine++ = 255;
                *BMPLine++ = 255;
            }
            else
            {
                *BMPLine++ = 0;
                *BMPLine++ = 0;
                *BMPLine++ = 0;
            }
    }
}
void acvThreshold_single(acvImage *Pic, BYTE Var, int channel)
{
    BYTE *BMPLine;
    int Height = Pic->GetROIOffsetY() + Pic->GetHeight(),
        Width = Pic->GetROIOffsetX() + Pic->GetWidth();
    for (int i = Pic->GetROIOffsetY(); i < Height; i++)
    {
        BMPLine = Pic->CVector[i] + Pic->GetROIOffsetX() * 3 + channel;
        for (int j = Pic->GetROIOffsetX(); j < Width; j++, BMPLine += 3)

            if (*BMPLine > Var)
            {
                *BMPLine = 255;
            }
            else
            {
                *BMPLine = 0;
            }
    }
}

void acvContrast(acvImage *dst, acvImage *src, int offset, int shift,int channel)
{
    BYTE *srcLine,*dstLine;
    int Height = src->GetROIOffsetY() + src->GetHeight(),
        Width = src->GetROIOffsetX() + src->GetWidth();
    for (int i = src->GetROIOffsetY(); i < Height; i++)
    {
        srcLine = src->CVector[i] + src->GetROIOffsetX() * 3+channel;
        dstLine = dst->CVector[i] + dst->GetROIOffsetX() * 3+channel;
        for (int j = src->GetROIOffsetX(); j < Width; j++,srcLine+=3,dstLine+=3)
        {
          int level = (srcLine[0]+offset);
          if(level<0)
          {
            dstLine[0]=0;
            continue;
          }
          level<<=shift;
          if(level&(~0xFF))level=0xFF;
          dstLine[0]=level;
        }
    }
}



void acvThreshold(acvImage *Pic, BYTE Var)
{
    acvThreshold(Pic, Var, 0);
}
double acvFAtan(double x)
{
    double absx = fabs(x);
    return x * (M_PI_4 - (absx - 1) * (0.2447 + 0.0663 * absx));
}

double acvFAtan2(double y, double x)
{

    /*if(x==0)return y < 0 ? -M_PI_2 : M_PI_2;
    if(y==0)return 0;*/
    double atan;
    if (fabs(x) > fabs(y))
    {
        atan = acvFAtan(y / x);
        if (x < 0.0f)
        {
            if (y < 0.0f)
            {
                return atan - M_PI;
            }
            else
            {
                return atan + M_PI;
            }
        }
        return atan;
    }
    else
    {
        atan = acvFAtan(x / y);
        if (y < 0.0f)
        {
            return -M_PI_2 - atan;
        }
        else
        {
            return M_PI_2 - atan;
        }
    }
}

void acvDeleteFrame(acvImage *Pic, int line_width)
{
    int Xoffset = Pic->GetROIOffsetX(),
        Yoffset = Pic->GetROIOffsetY(),
        Height = Yoffset + Pic->GetHeight(),
        Width = Xoffset + Pic->GetWidth();
    for (int k = 0; k < line_width; k++)
    {
        for (int i = Yoffset; i < Height; i++)
        {
            Pic->CVector[i][3 * (Xoffset + k) + 0] =
                Pic->CVector[i][3 * (Xoffset + k) + 1] =
                    Pic->CVector[i][3 * (Xoffset + k) + 2] =
                        Pic->CVector[i][3 * (Width - k) - 1] =
                            Pic->CVector[i][3 * (Width - k) - 2] =
                                Pic->CVector[i][3 * (Width - k) - 3] = 255;
        }
        for (int i = Xoffset + line_width; i < Width - line_width; i++)
        {
            Pic->CVector[Yoffset + k][3 * i + 1] =
                Pic->CVector[Yoffset + k][3 * i + 0] =
                    Pic->CVector[Yoffset + k][3 * i + 2] =
                        Pic->CVector[Height - k - 1][3 * i + 1] =
                            Pic->CVector[Height - k - 1][3 * i + 0] =
                                Pic->CVector[Height - k - 1][3 * i + 2] = 255;
        }
    }
}
void acvDeleteFrame(acvImage *Pic)
{
    acvDeleteFrame(Pic, 1);
}

void acvInnerFramePixCopy(acvImage *Pic, int FrameX)
{
    if (FrameX < 0)
        return;
    int FrameX3 = FrameX * 3;
    int WX3 = 3 * (Pic->GetWidth());
    //Fill the edge
    for (int i = FrameX; i < Pic->GetHeight() - FrameX; i++)
    {
        Pic->CVector[i][FrameX3 - 3 + 0] = Pic->CVector[i][FrameX3 + 0];
        Pic->CVector[i][FrameX3 - 3 + 1] = Pic->CVector[i][FrameX3 + 1];
        Pic->CVector[i][FrameX3 - 3 + 2] = Pic->CVector[i][FrameX3 + 2];
        Pic->CVector[i][WX3 - FrameX3 + 0] = Pic->CVector[i][WX3 - FrameX3 - 3 + 0];
        Pic->CVector[i][WX3 - FrameX3 + 1] = Pic->CVector[i][WX3 - FrameX3 - 3 + 1];
        Pic->CVector[i][WX3 - FrameX3 + 2] = Pic->CVector[i][WX3 - FrameX3 - 3 + 2];
    }

    int HX = Pic->GetHeight() - FrameX;
    for (int j = FrameX; j < Pic->GetWidth() - FrameX; j++)
    {
        Pic->CVector[FrameX - 1][j * 3 + 0] = Pic->CVector[FrameX][j * 3 + 0];
        Pic->CVector[FrameX - 1][j * 3 + 1] = Pic->CVector[FrameX][j * 3 + 1];
        Pic->CVector[FrameX - 1][j * 3 + 2] = Pic->CVector[FrameX][j * 3 + 2];
        Pic->CVector[HX][j * 3 + 0] = Pic->CVector[HX - 1][j * 3 + 0];
        Pic->CVector[HX][j * 3 + 1] = Pic->CVector[HX - 1][j * 3 + 1];
        Pic->CVector[HX][j * 3 + 2] = Pic->CVector[HX - 1][j * 3 + 2];
    }

    //Copy cornor
    //^<
    Pic->CVector[FrameX - 1][FrameX3 - 3 + 0] = Pic->CVector[FrameX][FrameX3 + 0];
    Pic->CVector[FrameX - 1][FrameX3 - 3 + 1] = Pic->CVector[FrameX][FrameX3 + 1];
    Pic->CVector[FrameX - 1][FrameX3 - 3 + 2] = Pic->CVector[FrameX][FrameX3 + 2];
    //^>
    Pic->CVector[FrameX - 1][WX3 - FrameX3 + 0] = Pic->CVector[FrameX][WX3 - FrameX3 - 3 + 0];
    Pic->CVector[FrameX - 1][WX3 - FrameX3 + 1] = Pic->CVector[FrameX][WX3 - FrameX3 - 3 + 1];
    Pic->CVector[FrameX - 1][WX3 - FrameX3 + 2] = Pic->CVector[FrameX][WX3 - FrameX3 - 3 + 2];
    //V<
    Pic->CVector[HX][FrameX3 - 3 + 0] = Pic->CVector[HX - 1][FrameX3 + 0];
    Pic->CVector[HX][FrameX3 - 3 + 1] = Pic->CVector[HX - 1][FrameX3 + 1];
    Pic->CVector[HX][FrameX3 - 3 + 2] = Pic->CVector[HX - 1][FrameX3 + 2];
    //V>
    Pic->CVector[HX][WX3 - FrameX3 + 0] = Pic->CVector[HX - 1][WX3 - FrameX3 - 3 + 0];
    Pic->CVector[HX][WX3 - FrameX3 + 1] = Pic->CVector[HX - 1][WX3 - FrameX3 - 3 + 1];
    Pic->CVector[HX][WX3 - FrameX3 + 2] = Pic->CVector[HX - 1][WX3 - FrameX3 - 3 + 2];
}

void acvClear(acvImage *Pic, BYTE Var)
{
    BYTE *BMPLine;
    int Xoffset = Pic->GetROIOffsetX(),
        Yoffset = Pic->GetROIOffsetY(),
        Height = Yoffset + Pic->GetHeight(),
        Width = Xoffset + Pic->GetWidth();
    for (int i = Yoffset; i < Height; i++)
    {
        BMPLine = Pic->CVector[i] + Xoffset * 3;
        for (int j = Xoffset; j < Width; j++)
        {
            *BMPLine++ = Var;
            *BMPLine++ = Var;
            *BMPLine++ = Var;
        }
    }
}
void acvClear(acvImage *Pic, int channel, BYTE Var)
{
    BYTE *BMPLine;
    int Xoffset = Pic->GetROIOffsetX(),
        Yoffset = Pic->GetROIOffsetY(),
        Height = Yoffset + Pic->GetHeight(),
        Width = Xoffset + Pic->GetWidth();
    for (int i = Yoffset; i < Height; i++)
    {
        BMPLine = Pic->CVector[i] + Xoffset * 3 + channel;
        for (int j = Xoffset; j < Width; j++)
        {
            *BMPLine = Var;
            BMPLine += 3;
        }
    }
}

void acvTurn(acvImage *Pic)
{
    BYTE *BMPLine;
    for (int i = 0; i < Pic->GetHeight(); i++)
    {
        BMPLine = Pic->CVector[i];
        for (int j = 0; j < Pic->GetWidth(); j++)
        {
            *BMPLine++ = 255 - *BMPLine;
            *BMPLine++ = 255 - *BMPLine;
            *BMPLine++ = 255 - *BMPLine;
        }
    }
}
void acvFullB2W(acvImage *OriPic, acvImage *OutPic)
{

    BYTE *OutLine, *OriLine;

    for (int i = 0; i < OriPic->GetHeight(); i++)
    {
        OutLine = OutPic->CVector[i];
        OriLine = OriPic->CVector[i];
        for (int j = 0; j < OriPic->GetWidth(); j++)
        {
            if (*OriLine == 255)
            {
                *OutLine++ = 255;
                *OutLine++ = 255;
                *OutLine++ = 255;
                OriLine += 3;
            }
            else
            {
                *OutLine++ = *OriLine++;
                *OutLine++ = *OriLine++;
                *OutLine++ = *OriLine++;
            }
        }
    }
}

void acvClone_B2Gray(acvImage *OriPic, acvImage *OutPic)
{

    BYTE *OutLine, *OriLine;

    for (int i = 0; i < OriPic->GetHeight(); i++)
    {
        OutLine = OutPic->CVector[i];
        OriLine = OriPic->CVector[i];
        for (int j = 0; j < OriPic->GetWidth(); j++)
        {
            *OutLine++ = *OriLine;
            *OutLine++ = *OriLine;
            *OutLine++ = *OriLine;
            OriLine += 3;
        }
    }
}

#define CloneImage_FullCopy -1

#define CloneImage_B2Gray 0
#define CloneImage_G2Gray 1
#define CloneImage_R2Gray 2

/*
#define CloneImage_B2B       3
#define CloneImage_G2G       4
#define CloneImage_R2R       5     */

void acvCloneImage(acvImage *OriPic, acvImage *OutPic, int Mode)
{

    BYTE *OutLine, *OriLine;

    switch (Mode)
    {
    case -1:

        memcpy(OutPic->ImageData, OriPic->ImageData, OriPic->GetHeight() * OriPic->GetWidth() * 3);
        break;
    case 0:
    case 1:
    case 2:
        for (int i = 0; i < OriPic->GetHeight(); i++)
        {
            OutLine = OutPic->CVector[i];
            OriLine = OriPic->CVector[i] + Mode;
            for (int j = 0; j < OriPic->GetWidth(); j++)
            {
                BYTE tmp = *OriLine;
                *OutLine++ = tmp;
                *OutLine++ = tmp;
                *OutLine++ = tmp;
                OriLine += 3;
            }
        }
        break;

defalut:
        for (int i = 0; i < OriPic->GetHeight(); i++)
        {
            OutLine = OutPic->CVector[i];
            for (int j = 0; j < OriPic->GetWidth(); j++)
            {
                *OutLine++ = 0;
                *OutLine++ = 0;
                *OutLine++ = 0;
            }
        }
    }
}
void acvCloneImage_single(acvImage *OriPic, int layer_ori, acvImage *OutPic, int layer_out)
{

    BYTE *OutLine, *OriLine;

    for (int i = 0; i < OriPic->GetHeight(); i++)
    {
        OutLine = OutPic->CVector[i] + layer_out;
        OriLine = OriPic->CVector[i] + layer_ori;
        for (int j = 0; j < OriPic->GetWidth(); j++,OutLine+=3,OriLine+=3)
        {
            OutLine[0]=OriLine[0];
        }
    }
}
char *PrintHexArr_buff(char *strBuff, int strBuffL, char *data, int dataL)
{
    char *buffptr = strBuff;

    if (data == NULL ||
            dataL * 3 > strBuffL)
    {
        return NULL;
    }
    int i;
    int plen = 0;
    for (i = 0; i < dataL; i++)
    {
        plen = snprintf(buffptr, strBuffL, "%02x", (unsigned char)data[i]);
        if (plen < 0)
        {
            break;
        }
        buffptr += plen;
        strBuffL -= plen;
        if ((i & 3) == 3)
        {
            plen = snprintf(buffptr, strBuffL, " ");
            if (plen < 0)
            {
                break;
            }
            buffptr += plen;
            strBuffL -= plen;
        }
    }
    return strBuff;
}

char *PrintHexArr(char *data, int dataL)
{
    static char bufferStr[500];

    return PrintHexArr_buff(bufferStr, sizeof(bufferStr), data, dataL);
}

unsigned char *acvLoadBitmapFile(const char *filename, acv_BITMAPINFOHEADER *bitmapInfoHeader)
{
    FILE *filePtr;                     //our file pointer
    acv_BITMAPFILEHEADER bitmapFileHeader; //our bitmap file header
    unsigned char *bitmapImage;        //store image data
    int imageIdx = 0;                  //image index counter
    unsigned char tempRGB;             //our swap variable

    //open filename in read binary mode
    filePtr = fopen(filename, "rb");
    if (filePtr == NULL)
        return NULL;

    //read the bitmap file header
    fread(&bitmapFileHeader, sizeof(acv_BITMAPFILEHEADER), 1, filePtr);

    //verify that this is a bmp file by check bitmap id
    if (bitmapFileHeader.bfType != 0x4D42)
    {
        fclose(filePtr);
        return NULL;
    }

    //read the bitmap info header
    fread(bitmapInfoHeader, sizeof(acv_BITMAPINFOHEADER), 1, filePtr); // small edit. forgot to add the closing bracket at sizeof

    int bmpRawSize = bitmapFileHeader.bfSize - bitmapFileHeader.bOffBits;

    //allocate enough memory for the bitmap image data
    bitmapImage = new unsigned char[bmpRawSize];

    //verify memory allocation
    if (!bitmapImage)
    {
        delete (bitmapImage);
        fclose(filePtr);
        return NULL;
    }

    //move file point to the begging of bitmap data
    fseek(filePtr, bitmapFileHeader.bOffBits, SEEK_SET);
    //read in the bitmap image data
    int rL = fread(bitmapImage, 1, bmpRawSize, filePtr);

    //make sure bitmap image data was read
    if (rL != bmpRawSize)
    {
        fclose(filePtr);
        return NULL;
    }
    //close file and return bitmap iamge data
    fclose(filePtr);
    return bitmapImage;
}

int acvLoadBitmapFile(acvImage *img,const  char *filename)
{

    acv_BITMAPINFOHEADER bitmapInfoHeader;
    unsigned char *bitmap = acvLoadBitmapFile(filename, &bitmapInfoHeader);
    if (bitmap == NULL)
    {
        return -1;
    }
    int biBitCount = bitmapInfoHeader.biBitCount;
    if (biBitCount != 32 && biBitCount != 24 && biBitCount != 8)
    {
        delete (bitmap);
        return -1;
    }
    int W = bitmapInfoHeader.biWidth, H = bitmapInfoHeader.biHeight;
    W = (W < 0) ? -W : W;
    H = (H < 0) ? -H : H;
    img->ReSize(W, H);

    unsigned char *bmp_ptr = bitmap;
    int BpP = biBitCount / 8;
    int padding4Skip = (4 - ((img->GetWidth() * BpP) % 4)) % 4;

    for (int i = img->GetROIOffsetY(); i < img->GetHeight(); i++)
    {
        unsigned char *ImLine;
        if (bitmapInfoHeader.biHeight < 0)
            ImLine = img->CVector[i];
        else
            ImLine = img->CVector[img->GetHeight() - 1 - i];

        for (int j = img->GetROIOffsetX(); j < img->GetWidth(); j++)
        {
            if(BpP==1)
            {
                ImLine[0] = bmp_ptr[0];
                ImLine[1] = bmp_ptr[0];
                ImLine[2] = bmp_ptr[0];
            }
            else
            {
                ImLine[0] = bmp_ptr[0];
                ImLine[1] = bmp_ptr[1];
                ImLine[2] = bmp_ptr[2];
            }
            ImLine += 3;
            bmp_ptr += BpP;
        }
        bmp_ptr += padding4Skip;
    }

    delete (bitmap);

    return 0;
}

int acvSaveBitmapFile(const char *filename, unsigned char *pixData, int width, int height)
{
    FILE *f;
    f = fopen(filename, "wb");
    if(f==NULL)return -1;

    int padding = (4 - ((width * 3) % 4)) % 4;
    acv_BITMAPFILEHEADER bm_header = {0};
    acv_BITMAPINFOHEADER bm_info_header = {0};
    bm_info_header.biSize = sizeof(bm_info_header);
    bm_info_header.biWidth = width;
    bm_info_header.biHeight = height;
    bm_info_header.biPlanes = 1;
    bm_info_header.biBitCount = 8 * 3;
    bm_info_header.biCompression = 0;
    bm_info_header.biSizeImage = width * height * 3 + padding * height;

    bm_header.bfType = 0x4D42;
    bm_header.bOffBits = sizeof(bm_header) + sizeof(bm_info_header);
    bm_header.bfSize = bm_info_header.biSizeImage + bm_header.bOffBits;

    unsigned char bmppad[3] = {0, 0, 0};

    fwrite(&bm_header, 1, sizeof(bm_header), f);
    fwrite(&bm_info_header, 1, sizeof(bm_info_header), f);
    for (int i = 0; i < height; i++)
    {
        fwrite(pixData + (width * (height - i - 1) * 3), 3, width, f);
        fwrite(bmppad, 1, (4 - (width * 3) % 4) % 4, f);
    }
    fclose(f);
    return 0;
}

int acvSaveBitmapFile(const char *filename, acvImage *img)
{
    return acvSaveBitmapFile(filename, img->ImageData, img->GetWidth(), img->GetHeight());
}
void acvImageAdd(acvImage *src, int num)
{
    int i, j;
    int TmpPixelH, TmpPixelV;
    BYTE *L;
    for (i = 0; i < src->GetHeight(); i++)
    {
        L = &(src->CVector[i][0]);
        for (j = 0; j < src->GetWidth(); j++, L += 3)
        {
            L[0] += num;
        }
    }
}

acv_XY acvRotation(float sine,float cosine,float flip_f,acv_XY input)
{
  acv_XY output;
  output.X = input.X*cosine-flip_f*input.Y*sine;
  output.Y = input.X*sine  +flip_f*input.Y*cosine;
  return output;
}
acv_XY acvRotation(float sine,float cosine,acv_XY input)
{
  return acvRotation(sine,cosine,1,input);
}
acv_XY acvRotation(float angle,acv_XY input)
{
  return acvRotation(sin(angle),cos(angle),1,input);
}


acv_XY acvIntersectPoint(acv_XY p1,acv_XY p2,acv_XY p3,acv_XY p4)
{
  acv_XY intersec;
  float denominator;

  float V1 = (p1.X-p2.X);
  float V2 = (p3.X-p4.X);
  float V3 = (p1.Y-p2.Y);
  float V4 = (p3.Y-p4.Y);

  denominator = V1* V4 - V3* V2;

  float V12 = (p1.X*p2.Y-p1.Y*p2.X);
  float V34 = (p3.X*p4.Y-p3.Y*p4.X);
  intersec.X=( V12 * V2 - V1 * V34 )/denominator;
  intersec.Y=( V12 * V4 - V3 * V34 )/denominator;

  /*printf("%f %f %f %f\r\n", V1,V2,V3,V4);
  printf("%f %f %f\r\n", V12,V34,denominator);*/
  return intersec;
}


acv_XY acvCircumcenter(acv_XY p1,acv_XY p2,acv_XY p3)
{
  acv_XY c12;
  c12.X=(p1.X+p2.X)/2; c12.Y=(p1.Y+p2.Y)/2;
  acv_XY c23;
  c23.X=(p2.X+p3.X)/2; c23.Y=(p2.Y+p3.Y)/2;

  acv_XY N12;
  N12.Y=(p1.X-p2.X); N12.X=-(p1.Y-p2.Y);
  acv_XY N23;
  N23.Y=(p2.X-p3.X); N23.X=-(p2.Y-p3.Y);


  acv_XY c12_;
  c12_.X=c12.X+N12.X; c12_.Y=c12.Y+N12.Y;
  acv_XY C23_;
  C23_.X=c23.X+N23.X; C23_.Y=c23.Y+N23.Y;

  return acvIntersectPoint(c12,c12_,c23,C23_);

}



acv_XY acvVecNormal(acv_XY vec)
{
  acv_XY nvec={-vec.Y,vec.X};
  return nvec;
}

acv_XY acvVecNormalize(acv_XY vec)
{
  acv_XY nvec={vec.X,vec.Y};
  float dist = hypot(vec.X,vec.Y);
  nvec.X/=dist;
  nvec.Y/=dist;
  return nvec;
}


acv_XY acvVecInterp(acv_XY vec1,acv_XY vec2,float alpha)
{
    vec1.X+=alpha*(vec2.X-vec1.X);
    vec1.Y+=alpha*(vec2.Y-vec1.Y);
    return vec1;
}

acv_XY acvVecAdd(acv_XY vec1,acv_XY vec2)
{
  vec1.X+=vec2.X;
  vec1.Y+=vec2.Y;
  return vec1;
}

acv_XY acvVecSub(acv_XY vec1,acv_XY vec2)
{
  vec1.X-=vec2.X;
  vec1.Y-=vec2.Y;
  return vec1;
}

acv_XY acvVecMult(acv_XY vec1,float mult)
{
  vec1.X*=mult;
  vec1.Y*=mult;
  return vec1;
}

float acvDistance(acv_XY p1,acv_XY p2)
{
  return hypot(p2.X-p1.X,p2.Y-p1.Y);
}


float acv2DCrossProduct(acv_XY v1,acv_XY v2)
{
	return v1.X*v2.Y - v2.X*v1.Y;
}

float acv2DDotProduct(acv_XY v1,acv_XY v2)
{
	return v1.X*v2.X + v2.Y*v1.Y;
}

float acvVectorOrder(acv_XY p1,acv_XY p2,acv_XY p3)
{
  acv_XY v1={.X=p2.X-p1.X,.Y=p2.Y-p1.Y};
  acv_XY v2={.X=p3.X-p2.X,.Y=p3.Y-p2.Y};
  return acv2DCrossProduct(v1,v2);
}

acv_XY acvClosestPointOnLine(acv_XY point, acv_Line line)
{
  line.line_vec=acvVecNormalize(line.line_vec);
  point.X-=line.line_anchor.X;
  point.Y-=line.line_anchor.Y;
  float dist = line.line_vec.X * point.X + line.line_vec.Y * point.Y;
  line.line_anchor.X+=dist*line.line_vec.X;
  line.line_anchor.Y+=dist*line.line_vec.Y;
  return line.line_anchor;
}

float acvDistance_Signed(acv_Line line, acv_XY point)
{
  // P1=(x1,y1)=line_anchor
  // P2=(x2,y2)=P1+line_vec then the distance of
  // point = (x0,y0)
  float denominator = hypot(line.line_vec.X,line.line_vec.Y);
  float XX = +line.line_vec.Y*point.X
             -line.line_vec.X*point.Y
             +(line.line_vec.X+line.line_anchor.X)*line.line_anchor.Y
             -(line.line_vec.Y+line.line_anchor.Y)*line.line_anchor.X ;

  return XX/denominator;
}

float acvDistance(acv_Line line, acv_XY point)
{

  float dist_signed = acvDistance_Signed(line, point);

  if (dist_signed < 0)dist_signed=-dist_signed;
  return dist_signed;
}


float acvVectorAngle(acv_XY v1,acv_XY v2)
{//acos range 0~pi

  float ang=atan2(v1.X,v1.Y);
  ang-=atan2(v2.X,v2.Y);
  if(ang>M_PI)
  {
    ang-=2*M_PI;
  }
  else if(ang<=-M_PI)
  {
    ang+=2*M_PI;
  }

  return ang;//-pi~pi
}
float acvLineAngle(acv_Line line1,acv_Line line2)
{//acos range 0~pi
  float reg=hypot(line1.line_vec.X,line1.line_vec.Y)*hypot(line2.line_vec.X,line2.line_vec.Y);
  return acos((line1.line_vec.X*line2.line_vec.X+line1.line_vec.Y*line2.line_vec.Y)/reg);
}
// Construct line from points

bool acvFitLine(const acv_XY *pts, const float *ptsw, int ptsL,acv_Line *line, float *ret_sigma) 
{
    
    return acvFitLine(pts,sizeof(acv_XY),ptsw,sizeof(float),ptsL,line, ret_sigma);
}

bool acvFitLine(const void *pts_struct,int pts_step, const void *ptsw_struct,int ptsw_step, int ptsL,acv_Line *line, float *ret_sigma) 
{
  acv_Line tarline = *line;
  if( ptsL < 2 || pts_struct==NULL || line==NULL) {
    // Fail: infinitely many lines passing through this single point
    return false;
  }

  float sumX=0, sumY=0, sumXY=0, sumX2=0, sumY2=0;
  float wsum=0;
  
  for(int i=0; i<ptsL; i++) {
    float w = (ptsw_struct)?(*(float*)((uint8_t*)ptsw_struct+i*ptsw_step)):1;
    acv_XY pt = *(acv_XY*)((uint8_t*)pts_struct+i*pts_step);
    sumX += pt.X*w;
    sumY += pt.Y*w;
    sumXY += pt.X * pt.Y*w;
    sumX2 += pt.X * pt.X*w;
    sumY2 += pt.Y * pt.Y*w;
    wsum+=w;
    
  }


  float xMean = sumX / wsum;
  float yMean = sumY / wsum;
  float denominatorX = sumX2 - sumX * xMean;
  float denominatorY = sumY2 - sumY * yMean;

  float denominator=0;
  if( fabs( denominatorX ) > fabs( denominatorY ) )
  {
      line->line_vec.Y =  (sumXY - sumX * yMean);
      line->line_vec.X =  denominatorX;
      denominator = hypot(line->line_vec.X,line->line_vec.Y);
  }
  else
  {
      line->line_vec.Y =  denominatorY;
      line->line_vec.X =  (sumXY - sumY * xMean);
      denominator = hypot(line->line_vec.X,line->line_vec.Y);
  }


  if(denominator< 1e-7)
  {
    return false;
  }

  line->line_vec.X /=denominator;
  line->line_vec.Y /=denominator;

  if( (line->line_vec.X*tarline.line_vec.X + line->line_vec.Y*tarline.line_vec.Y )<0)
  {
      line->line_vec.X =-line->line_vec.X;
      line->line_vec.Y =-line->line_vec.Y;
  }

  line->line_anchor.X = xMean;
  line->line_anchor.Y = yMean;

  /*
  a/b = (sumXY - sumX * yMean) / denominator;
  a*X/b+C = Y
  => (0,c)+n*(1,a/b)
  => (0,c)+n*(b,a)
  line_vec = (b,a)
  line_anchor = (0,c)+n*(b,a)
  */
  if(ret_sigma)
  {
    float sigma=0;
    wsum=0;
    for(int i=0; i<ptsL; i++) {
      float w = (ptsw_struct)?(*(float*)((uint8_t*)ptsw_struct+i*ptsw_step)):1;
      acv_XY pt = *(acv_XY*)((uint8_t*)pts_struct+i*pts_step);
      float dist=acvDistance_Signed(*line, pt);
      sigma+=dist*dist*w;
      wsum+=w;
    }
    *ret_sigma=sqrt(sigma/wsum);
  }
  return true;
}

bool acvFitLine(const acv_XY *pts, int ptsL,acv_Line *line, float *ret_sigma)
{
  return acvFitLine(pts, NULL, ptsL,line, ret_sigma);
}

acv_XY acvVecRadialDistortionRemove(acv_XY distortedVec,acvRadialDistortionParam param)
{

    if(param.map)
    {

        float imgVec[2]={distortedVec.X,distortedVec.Y};
        param.map->i2c(imgVec);
        distortedVec.X=imgVec[0];
        distortedVec.Y=imgVec[1];
        return distortedVec;
    }
    acv_XY v1 = acvVecSub(distortedVec,param.calibrationCenter);
    float R = hypot(v1.Y,v1.X)/param.RNormalFactor;

        
    float R_sq=R*R;
    float mult = param.K0+param.K1*R_sq+param.K2*R_sq*R_sq;

    return acvVecAdd(acvVecMult(v1,mult),param.calibrationCenter);
}
acv_XY acvVecRadialDistortionApply(acv_XY Vec,acvRadialDistortionParam param)//Still not perfect, there has some error for the conversion
{

    if(param.map)
    {
        float imgVec[2]={Vec.X,Vec.Y};
        param.map->c2i(imgVec);
        Vec.X=imgVec[0];
        Vec.Y=imgVec[1];
        return Vec;
    }
    acv_XY v1 = acvVecSub(Vec,param.calibrationCenter);
    float R1 = hypot(v1.Y,v1.X)/param.RNormalFactor;
    float R2 = R1/param.K0;
    

    double C1=param.K1/param.K0;
    double C2=param.K2/param.K0;

        
    float R2_sq=R2*R2;
    //float mult = 1-C1*R2_sq+(3*C1*C1-C2)*R2_sq*R2_sq;//r/r"

    //HACK: SUPER hacky formula, just trial and error...
    float mult = 1+0.001*(C2/0.03572)-C1*R2_sq+(3*C1*C1-0.9*C2)*R2_sq*R2_sq;
    float mult2 = mult/param.K0;//K0* r/r"

    return acvVecAdd(acvVecMult(v1,mult2),param.calibrationCenter);
}

acvCalibMap::acvCalibMap(double *MX_data, double *MY_data, int fw_,int fh_,int fullW,int fullH)
{
    
    fw=fw_;
    fh=fh_;
    this->fullW=fullW;
    this->fullH=fullW;
    origin_offset.X=0;
    origin_offset.Y=0;
    invMap=NULL;
    int pixCount = fw*fh;
    fwdMap=new float[fw*fh*2];
    for(int i=0;i<pixCount;i++)
    {
        fwdMap[i*2]=(float)MX_data[i];
        fwdMap[i*2+1]=(float)MY_data[i];
    }

    
    int pa=4;
    int detC=0;
    float detSum=0;
    for(int i=fh/pa;i<fh*(pa-1)/pa;i++)
    {
        for(int j=fw/pa;j<fw*(pa-1)/pa;j++)
        {
            float xyVec[4];
            float det;
            int ret = acvCalibMapUtil::map_vec(fwdMap,fw,fh,j,i,xyVec, &det);
            if(ret==0)
            {
                detSum+=det;
                detC++;
            }
        }
    }
    float aveDet = detSum/detC;
    fmapScale=sqrt(aveDet);
}
void acvCalibMap::generateInvMap(int iw_,int ih_)
{
    invMap=generateExtInvMap(iw_,ih_);
    iw=iw_;
    ih=ih_;
}

float* acvCalibMap::generateExtInvMap(int iw_,int ih_)
{
    
    float* _invMap=new float[iw_*ih_*2];

    int missCount=0;
    float errSum=0;

    float mapSeed_row[2]={fw/2.0f,fh/2.0f};
    for(int i=0;i<ih_;i++)
    {
        float mapSeed_ret[2]={mapSeed_row[0],mapSeed_row[1]};   
        for(int j=0;j<iw_;j++)
        {
            int idx = i*iw_+j;
            if(mapSeed_ret[0]!=mapSeed_ret[0])//NAN
            {
                mapSeed_ret[0]=fw/2.0f;
                mapSeed_ret[1]=fh/2.0f;
            }
            float tarLoc[2]={(float)j,(float)i};
            float error = acvCalibMapUtil::locateMapPosition(fwdMap,fw,fh,tarLoc[0],tarLoc[1],mapSeed_ret);
            
            if(j==0)
            {
                mapSeed_row[0]=mapSeed_ret[0];
                mapSeed_row[1]=mapSeed_ret[1];
            }
            if(error>0.02 || error!=error){
                _invMap[idx*2+1]=
                _invMap[idx*2]=NAN;
                missCount++;
            }else{
                errSum+=error;
                _invMap[idx*2]=mapSeed_ret[0];
                _invMap[idx*2+1]=mapSeed_ret[1];
            }
        }
    }
    return _invMap;
}
int acvCalibMap::fwdMapDownScale(int dscale_idx)
{
    if(dscale_idx<0)return -1;
    if(dscale_idx==0)return 0;

    int dscale=1<<dscale_idx;
    int dfw=fw/dscale;
    int dfh=fh/dscale;
    float *dfwdMap=new float[dfw*dfh*2];
    for(int di=0;di<dfh;di++)
    {
        int i = di*dscale;
        for(int dj=0;dj<dfw;dj++)
        {
            int j = dj*dscale;
            int didx=di*dfw+dj;
            int idx=i*fw+j;
            dfwdMap[didx*2]=fwdMap[idx*2];
            dfwdMap[didx*2+1]=fwdMap[idx*2+1];
        }
    }
    
    delete fwdMap;
    fwdMap=dfwdMap;
    fw=dfw;
    fh=dfh;
    downScale*=dscale;

    deleteInvMap();
    return 0;
}

void acvCalibMap::deleteInvMap()
{
    if(invMap)
    {
        delete invMap;
        invMap=NULL;
    }
}
acvCalibMap::~acvCalibMap()
{
    deleteInvMap();
    delete fwdMap;
}

int acvCalibMap::i2c(float coord[2],bool useInvMap)//real image coord to calibrated coord
{
    int ret;
    
    coord[0]+=origin_offset.X;
    coord[1]+=origin_offset.Y;
    if(invMap && useInvMap)
    {
        ret = acvCalibMapUtil::sample_vec(invMap,iw,ih,coord[0],coord[1],coord);
    }
    else
    {
        ret=0;
        //printf("coord:%d %d\n",fw,fh);
        float x=coord[0],y=coord[1];
        coord[0]=coord[1]=0;
        float error = acvCalibMapUtil::locateMapPosition(fwdMap,fw,fh,x,y,coord);
        //printf("----: %f %f\n",coord[0],coord[1]);
        if(error>0.01 || error!=error)
            ret=-1;
    }

    if(ret == 0)
    {
        coord[0]*=downScale*fmapScale;
        coord[1]*=downScale*fmapScale;
    }
    else
    {
        coord[0]=
        coord[1]=NAN;
    }
    return ret;
}

int acvCalibMap::i2c(acv_XY &coord,bool useInvMap)
{
    float _coord[2]={coord.X,coord.Y};
    int ret = i2c(_coord,useInvMap);
    coord.X=_coord[0];
    coord.Y=_coord[1];
    return ret;
}
int acvCalibMap::c2i(float coord[2])//calibrated coord to real image coord
{
    
    coord[0]/=downScale*fmapScale;
    coord[1]/=downScale*fmapScale;
    int ret = acvCalibMapUtil::sample_vec(fwdMap,fw,fh,coord[0],coord[1],coord);
    coord[0]-=origin_offset.X;
    coord[1]-=origin_offset.Y;
    return ret;
}
int acvCalibMap::c2i(acv_XY &coord)
{
    float _coord[2]={coord.X,coord.Y};
    int ret = c2i(_coord);
    coord.X=_coord[0];
    coord.Y=_coord[1];
    return ret;
}





float acvCalibMapUtil::NumRatio(float a,float b,float ratio)
{
    return a+ratio*(b-a);
}

int acvCalibMapUtil::sample_vec(float* map,int width,int height,float mapfX,float mapfY,float sampleXY[2])
{
    
    sampleXY[0]=sampleXY[1]=NAN;
    if(mapfX>width-2 || mapfY>height-2 || mapfX<0 || mapfY<0)
    {

        return -1;
    }
    
    int mapX=mapfX;
    int mapY=mapfY;

    float ratioX=mapfX-mapX;
    float ratioY=mapfY-mapY;

    int idxLT = (mapY)*width+mapX;
    int idxRT = (mapY)*width+mapX+1;
    int idxLB = (mapY+1)*width+mapX;
    int idxRB = (mapY+1)*width+mapX+1;
    /*
    LT  RT
    
    LB  RB
    */
    
    float v1[2]={NumRatio(map[idxLT*2  ],map[idxRT*2  ],ratioX),
                NumRatio(map[idxLT*2+1],map[idxRT*2+1],ratioX)};
    float v2[2]={NumRatio(map[idxLB*2  ],map[idxRB*2  ],ratioX),
                NumRatio(map[idxLB*2+1],map[idxRB*2+1],ratioX)};

    sampleXY[0]=NumRatio(v1[0],v2[0],ratioY);
    sampleXY[1]=NumRatio(v1[1],v2[1],ratioY);

    return 0;
}

int acvCalibMapUtil::map_vec(float* map,int width,int height,float mapfX,float mapfY,float xyVec[4], float *opt_det)
{
    if(mapfX>width-2)mapfX=width-2;
    if(mapfY>height-2)mapfY=height-2;
    if(mapfX<0)mapfX=0;
    if(mapfY<0)mapfY=0;
    
    int mapX=mapfX;
    int mapY=mapfY;

    float ratioX=mapfX-mapX;
    float ratioY=mapfY-mapY;

    int idxLT = (mapY)*width+mapX;
    int idxRT = (mapY)*width+mapX+1;
    int idxLB = (mapY+1)*width+mapX;
    int idxRB = (mapY+1)*width+mapX+1;
    /*
    
    LT  RT
    
    LB  RB
    */
    
    float v1[2]={NumRatio(
                    map[idxRT*2]-map[idxLT*2],
                    map[idxRB*2]-map[idxLB*2],
                    ratioX),
                NumRatio(
                    map[idxRT*2+1]-map[idxLT*2+1],
                    map[idxRB*2+1]-map[idxLB*2+1],
                    ratioX)};
    float v2[2]={NumRatio(
                    map[idxLB*2]-map[idxLT*2],
                    map[idxRB*2]-map[idxRT*2],
                    ratioY),
                NumRatio(
                    map[idxLB*2+1]-map[idxLT*2+1],
                    map[idxRB*2+1]-map[idxRT*2+1],
                    ratioY)};
                    

    // LOGI("%f  %f",v1[0],v1[1]);
    // LOGI("%f  %f",v2[0],v2[1]);
    float det = v1[0]*v2[1]-v1[1]*v2[0];
    if(opt_det)*opt_det=det;
    float invMat[4]={
        v2[1]/det,-v1[1]/det,
        -v2[0]/det, v1[0]/det
    };
    memcpy(xyVec,invMat,sizeof(invMat));
    return 0;
}

float acvCalibMapUtil::locateMapPosition(float* map,int width,int height,float tar_x,float tar_y,float mapSeed_ret[2],float maxError,int iterC)
{

    float sampleXY[2];
    float x=tar_x, y=tar_y;
    float error = hypot(sampleXY[0]-x,sampleXY[1]-y);
    for(int i=0;i<iterC;i++)
    {


        sample_vec(map,width,height,mapSeed_ret[0],mapSeed_ret[1],sampleXY);
        error = hypot(sampleXY[0]-x,sampleXY[1]-y);

        if(error<maxError)return  error;
        if(error!=error)
        {
            mapSeed_ret[0]=
            mapSeed_ret[1]=NAN;
            return NAN;
        }
        // LOGI("mapSeed_ret:%f  %f",mapSeed_ret[0],mapSeed_ret[1]);
        // LOGI("xy:%f  %f",x,y);
        // LOGI("sample_vec:%f  %f",sampleXY[0],sampleXY[1]);
        float diffXY[2]={
            x-sampleXY[0],
            y-sampleXY[1]
        };

        float xyVec[4];
        
        int ret= map_vec(map, width, height,mapSeed_ret[0],mapSeed_ret[1], xyVec);
        // LOGI("%f  %f",xyVec[0],xyVec[1]);
        // LOGI("%f  %f",xyVec[2],xyVec[3]);

        
        mapSeed_ret[0]+=diffXY[0]*xyVec[0]+diffXY[1]*xyVec[2];
        mapSeed_ret[1]+=diffXY[0]*xyVec[1]+diffXY[1]*xyVec[3];
    }

    sample_vec(map,width,height,mapSeed_ret[0],mapSeed_ret[1],sampleXY);

    error = hypot(sampleXY[0]-x,sampleXY[1]-y);
    return error;
}