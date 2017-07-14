#include <stdio.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

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
  ret=acvLoadBitmapFile(ss,"data/test1.bmp");
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
  //ret=acvSaveBitmapFile("data/uu_harris.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  //acvDrawCrossX(ss,200,200,12,255,0,0,7);
  //acvTurn(ss);
  ret=acvSaveBitmapFile("data/uu_harris.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  acvThreshold(ss,128);

  acvDeleteFrame(ss,5);

  acvComponentLabeling(ss);
  acvLabeledColorDispersion(ss,ss,5);
  ret=acvSaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());

  delete(ss);
  delete(buff);
  // printf() displays the string inside quotation
}

void acvScalingSobelResult(acvImage *src)
{
  int m1=255;
  int M1=0;
  int i,j;
  int TmpPixelH,TmpPixelV;
  BYTE *L;
  for(i=1;i<src->GetHeight()-1;i++)
  {
    L=&(src->CVector[i][1*3]);
    for(j=1;j<src->GetWidth()-1;j++,L+=3)
    {
      char Lp0,Lp1;
      Lp0=L[0];
      Lp1=L[1];

      if(m1>Lp0)m1=Lp0;
      if(M1<Lp0)M1=Lp0;
      if(m1>Lp1)m1=Lp1;
      if(M1<Lp1)M1=Lp1;

    }
  }

  if(-m1>M1)M1=-m1;
  printf("M1:%d %d\n",M1,m1);

  for(i=1;i<src->GetHeight()-1;i++)
  {
    L=&(src->CVector[i][0]);
    for(j=1;j<src->GetWidth()-1;j++,L+=3)
    {
      char Lp0,Lp1;
      Lp0=L[0];
      Lp1=L[1];

      L[0]=div_round((int)Lp0*127,M1);
      L[1]=div_round((int)Lp1*127,M1);


    }
  }
}

void DistGradientTest(acvImage *distSobelMap,acv_XY *XY,acv_XY *Force)
{
  int rX=round(XY->X);
  int rY=round(XY->Y);

  Force->Y=(char)distSobelMap->CVector[rY][rX*3];
  Force->X=(char)distSobelMap->CVector[rY][rX*3+1];
}

void test2()
{
  acvImage *ss = new acvImage();
  acvImage *distGradient = new acvImage();

  acvLoadBitmapFile(ss,"data/target_s.bmp");
  distGradient->ReSize(ss->GetWidth(),ss->GetHeight());
  acvThreshold(ss,128);
  int mul=1;
  //acvDistanceTransform_Chamfer(ss,3*mul,4*mul);
  acvDistanceTransform_ChamferX(ss);
  acvInnerFramePixCopy(ss,1);
  acvSaveBitmapFile("data/target_s_dist.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());

  acvDistanceTransform_Sobel(distGradient,ss);
  acvInnerFramePixCopy(distGradient,1);


  acvScalingSobelResult(distGradient);
  acvImageAdd(distGradient,128);
  acvBoxFilter(ss,distGradient,25);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(distGradient,-128);
  distGradient->ChannelOffset(1);
  acvImageAdd(distGradient,128);
  acvBoxFilter(ss,distGradient,25);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(distGradient,-128);
  distGradient->ChannelOffset(-1);


  acvClear(ss,128);
  int lineN=10;
  for(int k=0;k<lineN;k++)
  {
    BYTE    Color[3]={HSV_VMax,HSV_VMax,(k)*(HSV_HMax-50)/lineN};
    distGradient->RGBFromHSV(Color,Color);

    acv_XY preXY={
      0*distGradient->GetWidth()/lineN,
      0*distGradient->GetHeight()/lineN};
    acv_XY XY;
    acv_XY Force={0};
    acv_XY speed={0,25};
    for(int i=0;i<1000;i++)
    {

      DistGradientTest(distGradient,&preXY,&Force);
      speed.X+=Force.X/100;
      speed.Y+=Force.Y/100;
      speed.X/=1.01+0.05*k/lineN;
      speed.Y/=1.01+0.05*k/lineN;
      XY.X=preXY.X+speed.X;
      XY.Y=preXY.Y+speed.Y;
      if(XY.X<0)XY.X=0;
      if(XY.X>distGradient->GetWidth()-1)XY.X=distGradient->GetWidth()-1;
      if(XY.Y<0)XY.Y=0;
      if(XY.Y>distGradient->GetWidth()-1)XY.Y=distGradient->GetWidth()-1;
      acvDrawLine(ss,(int)XY.X,(int)XY.Y,(int)preXY.X,(int)preXY.Y,Color[2],Color[1],Color[0],1);
      preXY=XY;
      //printf(">%f %f\n",XY.X,XY.Y);
    }
  }
  acvSaveBitmapFile("data/target_ss.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());


  acvImageAdd(distGradient,128);
  distGradient->ChannelOffset(1);
  acvImageAdd(distGradient,128);
  distGradient->ChannelOffset(-1);

  //acvCloneImage(distGradient,distGradient,2);
  acvSaveBitmapFile("data/target_s_dist_grad.bmp",distGradient->ImageData,ss->GetWidth(),ss->GetHeight());
}
void TargetPrep()
{
  acvImage *tar = new acvImage();
  acvImage *buff = new acvImage();
  acvLoadBitmapFile(tar,"data/target.bmp");
  buff->ReSize(tar->GetWidth(),tar->GetHeight());

  acvBoxFilter(buff,tar,5);
  acvBoxFilter(buff,tar,10);
  acvCloneImage(tar,tar,0);
  acvSaveBitmapFile("data/target_soft.bmp",tar->ImageData,buff->GetWidth(),buff->GetHeight());

  acvSobelFilter(buff,tar);
  acvScalingSobelResult(buff);
  acvImageAdd(buff,128);

  acvBoxFilter(tar,buff,5);
  buff->ChannelOffset(1);
  acvImageAdd(buff,128);
  acvBoxFilter(tar,buff,5);
  buff->ChannelOffset(-1);
  //acvCloneImage(buff,buff,0);
  acvSaveBitmapFile("data/target_sobel.bmp",buff->ImageData,buff->GetWidth(),buff->GetHeight());

}
int testEstXY()
{
  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  std::vector<acv_LabeledData> ldData;

  acvLoadBitmapFile(ss,"data/test1.bmp");
  buff->ReSize(ss->GetWidth(),ss->GetHeight());
  ss->RGBToGray();
  acvThreshold(ss,200);
  acvBoxFilter(buff,ss,3);
  acvThreshold(ss,80);
  acvBoxFilter(buff,ss,3);
  acvThreshold(ss,255-5);



  /*acvBoxFilter(buff,ss,1);
  acvThreshold(ss,200);
  acvTurn(ss);
  acvComponentLabeling(ss);
  acvLabeledRegionExtraction(ss,&ldData);
  acvRemoveRegionLessThan(ss,&ldData,2500);

  acvCloneImage(ss,ss,2);
  acvThreshold(ss,254);
  acvTurn(ss);
  acvBoxFilter(buff,ss,1);

  acvCloneImage(ss,ss,0);
  acvThreshold(ss,255-200);*/

  acvSaveBitmapFile("data/uu_oXX.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());


  //acvDeleteFrame(ss,5);
  acvComponentLabeling(ss);
  acvLabeledRegionExtraction(ss,&ldData);
  acvRemoveRegionLessThan(ss,&ldData,120);

  acvLabeledColorDispersion(ss,ss,ldData.size()/20+5);

  for (int i=1;i<ldData.size();i++)
  {
    printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);
  }

  acvSaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  return 0;
}

#include <vector>
int main()
{
  //clock_t t= clock();

  test2();

  //t = clock() - t;
  //printf("fun() took %f seconds to execute \n", ((double)t)/CLOCKS_PER_SEC);
  //testEstXY();
  //testEstXY();
  //TargetPrep();
  int ret = 0;
  printf("Hello, World! %d",ret);
  return ret;
}
