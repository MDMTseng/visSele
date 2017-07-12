#include <stdio.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"

#include "acvImage_ToolBox.hpp"
#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"



void printImgAscii(acvImage *img,int printwidth)
{
  int step=img->GetWidth()/printwidth;
  if(step<1)step=1;
  char grayAscii[] =  "@%#x+=:-.. ";
  for(int i=img->GetROIOffsetY();i<img->GetHeight();i+=step)
  {unsigned char *ImLine=img->CVector[i];
          for(int j=img->GetROIOffsetX();j<img->GetWidth();j+=step)
          {
            int tmp = (255-ImLine[3*j])*(sizeof(grayAscii)-2)/255;
            char c = grayAscii[tmp];
            //printf("%02x ",ImLine[3*j+2]);
            putchar(c);
          }
          putchar('\n');
  }
}

void test1()
{
  clock_t t= clock();

  printf(">%f %f \n", atan2(-20,-5),acvFAtan2(-20,-5));
  for(int i=0;i<10000000;i++)
  {
    acvFAtan2(i,5);
  }
  t = clock() - t;
  printf("fun() took %f seconds to execute \n", ((double)t)/CLOCKS_PER_SEC);



  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  int ret=0;
  ss->SetROI(0,0,5,5);
  BITMAPINFOHEADER bitmapInfoHeader;
  ret=LoadBitmapFile(ss,"data/test1.bmp");
  ss->RGBToGray();

  buff->ReSize(ss->GetWidth(),ss->GetHeight());
  //printf("%s\n",PrintHexArr((char*)ss->CVector[0], 10*4));
  //printf("%s\n",PrintHexArr((char*)ss->CVector[1], 10*4));

  printImgAscii(ss,70);
  acvBoxFilter(buff,ss,1);
  acvThreshold(ss,128);
  acvBoxFilter(buff,ss,1);
  //acvHarrisCornorResponse(buff,ss);
  //acvCloneImage(ss,ss,0);
  //ret=SaveBitmapFile("data/uu_harris.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  //acvDrawCrossX(ss,200,200,12,255,0,0,7);
  //acvTurn(ss);
  ret=SaveBitmapFile("data/uu_harris.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  acvThreshold(ss,128);

  acvDeletFrame(ss,5);

  acvComponentLabelingSim(ss);
  acvLabeledColorDispersion(ss,ss,5);
  ret=SaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());

  delete(ss);
  delete(buff);
  // printf() displays the string inside quotation
}
void test2()
{
  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();

  LoadBitmapFile(ss,"data/test1.bmp");
  ss->RGBToGray();
  acvThreshold(ss,128);
  acvDeletFrame(ss,5);
  acvComponentLabelingSim(ss);
  std::vector<acv_LabeledData> ldData;
  acvLabeledRegionExtraction(ss,&ldData);
  acvLabeledColorDispersion(ss,ss,ldData.size());

  for (int i=1;i<ldData.size();i++)
  {
    if(ldData[i].area<10)continue;
    printf("%03d %f %f\n",ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvDrawCross(ss,(int)ldData[i].Center.X,(int)ldData[i].Center.Y,5,0,0,0,2);

    acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);
  }

  SaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
}

int testEstXY()
{
  acvImage *ss = new acvImage();

  return 0;
}

#include <vector>
int main()
{
  test2();
  int ret = 0;
  printf("Hello, World! %d",ret);
  return ret;
}
