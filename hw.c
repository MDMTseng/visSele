#include <stdio.h>
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
            printf("%02x ",ImLine[3*j+2]);
            //putchar(c);putchar(' ');
          }
          putchar('\n');
  }
}


int main()
{

  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  int ret=0;
  ss->SetROI(0,0,5,5);
  BITMAPINFOHEADER bitmapInfoHeader;
  ret=LoadBitmapFile(ss,"data/test1.bmp");
  buff->ReSize(ss->GetWidth(),ss->GetHeight());
  //printf("%s\n",PrintHexArr((char*)ss->CVector[0], 10*4));
  //printf("%s\n",PrintHexArr((char*)ss->CVector[1], 10*4));

  acvThreshold(ss,200);
  acvSmooth(buff,ss,4);
  acvThreshold(ss,128);

  //acvbErosion(buff,ss,10);
  //acvbDilation(buff,ss,10);
  acvCloneImage(ss,ss,0);

  acvComponentLabelingSim(ss);
  printImgAscii(ss,30);
  acvLabeledColorDispersion(ss,ss,5);
  ret=SaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());



  delete(ss);
  // printf() displays the string inside quotation
  printf("Hello, World! %d",ret);
  return 0;
}
