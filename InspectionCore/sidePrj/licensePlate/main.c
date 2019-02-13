#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"

void printImgAscii(acvImage *img, int printwidth)
{
    int step = img->GetWidth() / printwidth;
    if (step < 1)
        step = 1;
    char grayAscii[] = "@%#x+=:-.. ";
    for (int i = img->GetROIOffsetY(); i < img->GetHeight(); i += step)
    {
        unsigned char *ImLine = img->CVector[i];
        for (int j = img->GetROIOffsetX(); j < img->GetWidth(); j += step)
        {
            int tmp = (255 - ImLine[3 * j]) * (sizeof(grayAscii) - 2) / 255;
            char c = grayAscii[tmp];
            //printf("%02x ",ImLine[3*j+2]);
            putchar(c);
        }
        putchar('\n');
    }
}
void ImageSquare16(acvImage *res, acvImage *src)
{

    int width = src->GetWidth();
    int height = src->GetHeight();

    BYTE *srcfront;
    BYTE *resfront;
    for (int i = 0; i < height; i++)
    {
        srcfront = &(src->CVector[i][0]);
        resfront = &(res->CVector[i][0]);
        for (int k = 0; k < width; k++, resfront += 3, srcfront += 3)
        {
            int I=*srcfront;
            I=I*I;
            uint16_t *res16=(uint16_t*)(resfront+1);
            *resfront=I>>8;
            *res16=I;
        }
    }
}

void ImageDIff16_half(acvImage *reshalf_16, acvImage *img1_u16,acvImage *img2_u16)
{

    int width = reshalf_16->GetWidth();
    int height = reshalf_16->GetHeight();

    for (int i = 0; i < height; i++)
    {

        BYTE *img1front = &(img1_u16->CVector[i][0]);
        BYTE *img2front = &(img2_u16->CVector[i][0]);
        BYTE *reshalffront = &(reshalf_16->CVector[i][0]);

        for (int k = 0; k < width; k++, img1front += 3, img2front += 3, reshalffront+=3)
        {
          uint16_t *img1_u16front = (uint16_t *)(img1front+1);
          uint16_t *img2_u16front = (uint16_t *)(img2front+1);
          int16_t *reshalf_16front = (int16_t *)(reshalffront+1);
          int diff = (int)*img1_u16front-*img2_u16front;
          *reshalffront = diff>>(1+8);//Including sign
          *reshalf_16front=diff/2;
        }
    }
}

void ImageRm16(acvImage *res_16)
{

    int width = res_16->GetWidth();
    int height = res_16->GetHeight();

    for (int i = 0; i < height; i++)
    {

        BYTE *res16front = &(res_16->CVector[i][0]);

        for (int k = 0; k < width; k++, res16front += 3)
        {
          uint16_t *res16 = (uint16_t *)(res16front+1);
          *res16=0;
        }
    }
}


void GuidedFilter(acvImage *res, acvImage *Img, int r,
  acvImage *buff1, acvImage *buff2, acvImage *buff3, acvImage *buff4)
{
    acvImage *mean=buff1;
    acvCloneImage(mean, Img, 0);
    //, acvImage *buff2, acvImage *buff3
    acvBoxFilterX(buff2, mean, r);

    acvImage *mean16_sq=buff2;
    ImageSquare16(mean16_sq, mean);

    acvImage *corr16=buff3;
    ImageSquare16(corr16, Img);

    //acvImage *var16_half=buff4;
    //ImageDIff16_half(var16_half,corr16,Img16_sq);

    //acvImage *a=1;
}

void XDiffDetect(acvImage *res, acvImage *src, int Size,int *LineBuff,int LineBuffL)
{

    int width = src->GetWidth();
    int height = src->GetHeight();

    if(width > LineBuffL)
    {//Line buff is not enough
      return;
    }
    memset(LineBuff,0,sizeof(*LineBuff)*width);
    BYTE *srcfront;
    BYTE *resfront;
    for (int i = 0; i < height; i++)
    {
        srcfront = &(src->CVector[i][0]);
        LineBuff[0]=*srcfront;
        srcfront += 3;
        for (int k = 1; k < width; k++, srcfront += 3)
        {
            LineBuff[k]=LineBuff[k-1]+*srcfront;
        }

        int difSpace=Size;
        int xoffset=difSpace*2;

          srcfront = &(src->CVector[i][0]);
        resfront = &(res->CVector[i][0])+xoffset*3;
        for (int k = xoffset; k < width; k++, resfront += 3, srcfront += 3)
        {
          int XX=
          -(LineBuff[k  ]-LineBuff[k-difSpace])
          +(LineBuff[k-(difSpace+1)]-LineBuff[k-(difSpace*2+1)]);
          /*XX/=6;
          XX+=128;*/
          XX/=difSpace;
          if(XX<0)XX=-XX;
          XX*=*srcfront;
          XX/=128;
          if(XX>255)XX=255;
            *resfront=XX;
        }
    }
}


void SimpleXDiff(acvImage *res, acvImage *src)
{

    int width = src->GetWidth();
    int height = src->GetHeight();

    BYTE *srcfront;
    BYTE *resfront;
    for (int i = 0; i < height; i++)
    {
        srcfront = &(src->CVector[i][0]);
        resfront = &(res->CVector[i][0]);
        for (int k = 1; k < width; k++)
        {
            int diff=(int)srcfront[k*3]-srcfront[k*3-3];
            if(diff<0)diff=-diff;
            resfront[k*3]=diff;
        }
    }
}

void acvTurnX(acvImage *Pic)
{
    BYTE *BMPLine;
    for (int i = 0; i < Pic->GetHeight(); i++)
    {
        BMPLine = Pic->CVector[i];
        for (int j = 0; j < Pic->GetWidth(); j++,BMPLine+=3)
        {
            *BMPLine = ~*BMPLine;
        }
    }
}

void acvBiliteralX(acvImage *res, acvImage *Img,int r,int tolerate)
{
    int i, j,k,m;
    BYTE *resFront;
    BYTE *imgLEdge;
    BYTE *imgFront;
    for (i = r; i < Img->GetHeight()-r; i++)
    {
        resFront = &(res->CVector[i][r*3]);
        imgFront = &(Img->CVector[i][r*3]);
        imgLEdge = &(Img->CVector[i-r][0]);
        for (j = r; j < Img->GetWidth()-r; j++, resFront += 3, imgFront+=3, imgLEdge+=3)
        {
          int S=0;
          int C=0;
          for(k=0;k<(2*r+1);k++)for(m=0;m<(2*r+1);m++)
          {
            int swin=imgLEdge[(k*Img->GetWidth()+m)*3];
            int diff=swin-(int)*imgFront;
            if(diff<0)diff=-diff;
            if(diff <tolerate)
            {
              S+=swin;
              C+=1;
            }
          }
          *resFront=S/C;

        }
    }
}

void acvDDDD(acvImage *res, acvImage *Img,int r)
{
    int i, j,k,m;
    BYTE *resFront;
    BYTE *imgLEdge;
    BYTE *imgFront;
    for (i = r; i < Img->GetHeight()-r; i++)
    {
        resFront = &(res->CVector[i][r*3]);
        imgFront = &(Img->CVector[i][r*3]);
        imgLEdge = &(Img->CVector[i-r][0]);
        for (j = r; j < Img->GetWidth()-r; j++, resFront += 3, imgFront+=3, imgLEdge+=3)
        {
          int Max=imgLEdge[0];
          int Min=Max;
          for(k=0;k<(2*r+1);k++)for(m=0;m<(2*r+1);m++)
          {
            int swin=imgLEdge[(k*Img->GetWidth()+m)*3];
            if(Min>swin)Min=swin;
            else if(Max<swin)Max=swin;
          }
          int S=0;

          S=(Max+Min)/2;
          S=*imgFront-S+128;
          if(S<0)S=0;
          if(S>255)S=255;
          *resFront=S;
        }
    }
}


void acvMax(acvImage *res, acvImage *Img,int r)
{
    int i, j,k,m;
    BYTE *resFront;
    BYTE *imgLEdge;
    BYTE *imgFront;
    for (i = r; i < Img->GetHeight()-r; i++)
    {
        resFront = &(res->CVector[i][r*3]);
        imgFront = &(Img->CVector[i][r*3]);
        imgLEdge = &(Img->CVector[i-r][0]);
        for (j = r; j < Img->GetWidth()-r; j++, resFront += 3, imgFront+=3, imgLEdge+=3)
        {
          int Max=imgLEdge[0];
          for(k=0;k<(2*r+1);k++)for(m=0;m<(2*r+1);m++)
          {
            int swin=imgLEdge[(k*Img->GetWidth()+m)*3];
            if(Max<swin)Max=swin;
          }
          *resFront=Max;
        }
    }
}
void acvMin(acvImage *res, acvImage *Img,int r)
{
    int i, j,k,m;
    BYTE *resFront;
    BYTE *imgLEdge;
    BYTE *imgFront;
    for (i = r; i < Img->GetHeight()-r; i++)
    {
        resFront = &(res->CVector[i][r*3]);
        imgFront = &(Img->CVector[i][r*3]);
        imgLEdge = &(Img->CVector[i-r][0]);
        for (j = r; j < Img->GetWidth()-r; j++, resFront += 3, imgFront+=3, imgLEdge+=3)
        {
          int Min=imgLEdge[0];
          for(k=0;k<(2*r+1);k++)for(m=0;m<(2*r+1);m++)
          {
            int swin=imgLEdge[(k*Img->GetWidth()+m)*3];
            if(Min>swin)Min=swin;
          }
          *resFront=Min;
        }
    }
}

void acvDiff(acvImage *res, acvImage *Img1, acvImage *Img2)
{
    int i, j,k,m;
    BYTE *resFront;
    BYTE *Img1Front;
    BYTE *Img2Front;
    for (i = 0; i < res->GetHeight(); i++)
    {
        resFront = &(res->CVector[i][0]);
        Img1Front = &(Img1->CVector[i][0]);
        Img2Front = &(Img2->CVector[i][0]);
        for (j = 0; j < res->GetWidth(); j++, resFront += 3, Img1Front+=3, Img2Front+=3)
        {
          int diff=(int)*Img1Front-*Img2Front;
          if(diff<0)diff=-diff;
          *resFront=diff;
        }
    }
}
int testX()
{
  acvImage *target = new acvImage();

  int ret=acvLoadBitmapFile(target, "data/test1.bmp");
  acvImage *opening = new acvImage();
  acvImage *closing = new acvImage();
  acvImage *target_buff = new acvImage();
  acvImage *result = new acvImage();

  opening->ReSize(target->GetWidth(), target->GetHeight());
  closing->ReSize(target->GetWidth(), target->GetHeight());
  result->ReSize(target->GetWidth(), target->GetHeight());
  target_buff->ReSize(target->GetWidth(), target->GetHeight());

  clock_t t = clock();
  acvDDDD(target_buff, target,5);
  acvCloneImage(target_buff,target,0);


  acvMin(target_buff, target,1);
  acvMax(opening, target_buff,1);
  acvCloneImage(opening,opening,0);

  acvMax(target_buff, target,1);
  acvMin(closing, target_buff,1);
  acvCloneImage(closing,closing,0);
  acvDiff(target_buff, opening, closing);
  acvCloneImage(target_buff,result,0);

    acvMin(opening, target_buff,1);
    acvMax(closing, opening,1);

  acvCloneImage(closing,closing,0);
  acvSaveBitmapFile("data/closing.bmp", closing);


  SimpleXDiff(target_buff,result);
  acvContrast(target_buff,target_buff,-20,2,0);
  acvBoxFilterY(result, target_buff, 1);
  acvMax(target_buff, result,1);
  acvBoxFilterX(result, target_buff, 8);
  acvContrast(result,result,-5,8,0);

  acvCloneImage(result,result,0);

  //acvThreshold(target_buff,50);

  t = clock() - t;
  printf("%fms ..\n", ((double)t) / CLOCKS_PER_SEC * 1000);
  t = clock();
  //acvSaveBitmapFile("data/closing.bmp", closing);
  //acvSaveBitmapFile("data/opening.bmp", opening);
  acvSaveBitmapFile("data/result.bmp", result);
  return 0;
}

#include <vector>
int main()
{
    int ret = 0;
    ret = testX();

    printf("Hello, World! %d", ret);
    return ret;
}
