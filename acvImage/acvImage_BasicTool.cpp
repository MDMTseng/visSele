
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
    if (biBitCount != 32 && biBitCount != 24)
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
            ImLine[0] = bmp_ptr[0];
            ImLine[1] = bmp_ptr[1];
            ImLine[2] = bmp_ptr[2];
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


acv_XY acvVecAdd(acv_XY vec1,acv_XY vec2)
{
  vec1.X+=vec2.X;
  vec1.Y+=vec2.Y;
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
