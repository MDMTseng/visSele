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

//IIRCoeff



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

int testX()
{
  acvImage *target = new acvImage();

  int ret=acvLoadBitmapFile(target, "data/test1.bmp");
  acvImage *target_buff = new acvImage();

  target_buff->ReSize(target->GetWidth(), target->GetHeight());


  clock_t t = clock();



  int BUFFX[2000];
  acvBoxFilterY_BL(target_buff, target, 1,BUFFX,sizeof(BUFFX)/sizeof(*BUFFX));
  XDiffDetect(target, target_buff, 3,BUFFX,sizeof(BUFFX)/sizeof(*BUFFX));

  acvContrast(target,target,-20,4,0);
  acvBoxFilterX(target_buff, target, 1);
  acvBoxFilterX(target, target_buff, 2);
  acvTurnX(target);
  acvThreshold(target,250,0);

  if(1)
  {

  acvDrawBlock(target, 1, 1, target->GetWidth() - 2, target->GetHeight() - 2);
  acvComponentLabeling(target);
  std::vector<acv_LabeledData> ldData;
  acvLabeledRegionInfo(target, &ldData);
  ldData[1].area = 0;
  acvRemoveRegionLessThan(target, &ldData, 1000);
  acvLabeledColorDispersion(target,target,ldData.size()/20+5);
  }
  else
  {

    acvCloneImage(target,target,0);
  }

  /*acvContrast(target,target,-15,5,0);
  acvBoxFilter(target_buff, target, 5);
  acvContrast(target,target,0,5,0);*/

  t = clock() - t;
  printf("%fms ..\n", ((double)t) / CLOCKS_PER_SEC * 1000);
  t = clock();
  acvSaveBitmapFile("data/target_buff.bmp", target);
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
