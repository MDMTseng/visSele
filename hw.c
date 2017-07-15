#include <stdio.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "MLNN.hpp"



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
  for(i=0;i<src->GetHeight();i++)
  {
    L=&(src->CVector[i][0]);
    for(j=0;j<src->GetWidth();j++,L+=3)
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

  for(i=0;i<src->GetHeight();i++)
  {
    L=&(src->CVector[i][0]);
    for(j=0;j<src->GetWidth();j++,L+=3)
    {
      char Lp0,Lp1;
      Lp0=L[0];
      Lp1=L[1];

      L[0]=div_round((int)Lp0*127,M1);
      L[1]=div_round((int)Lp1*127,M1);


    }
  }
}


void acvScalingSobelResult_n(acvImage *src)
{
  int i,j;
  BYTE *L;

  for(i=0;i<src->GetHeight();i++)
  {
    L=&(src->CVector[i][0]);
    for(j=0;j<src->GetWidth();j++,L+=3)
    {
      char Lp0,Lp1;
      Lp0=L[0];
      Lp1=L[1];
      if(Lp0!=0||Lp1!=0)
      {
        float length = sqrt(Lp0 * Lp0 + Lp1 * Lp1);
        L[0]=(int)round((int)Lp0*127/length);
        L[1]=(int)round((int)Lp1*127/length);
      }


    }
  }
}

void test2()
{
  acvImage *ss = new acvImage();
  acvImage *distGradient = new acvImage();

  acvLoadBitmapFile(ss,"data/target_s.bmp");
  distGradient->ReSize(ss->GetWidth(),ss->GetHeight());
  acvThreshold(ss,128);
  int mul=1;
  acvDistanceTransform_Chamfer(ss,5*mul,7*mul);
  //acvDistanceTransform_ChamferX(ss);
  acvInnerFramePixCopy(ss,1);
  acvSaveBitmapFile("data/target_s_dist.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());

  acvDistanceTransform_Sobel(distGradient,ss);
  acvInnerFramePixCopy(distGradient,2);
  acvInnerFramePixCopy(distGradient,1);
  acvScalingSobelResult_n(distGradient);

  /*acvImageAdd(distGradient,128);
  acvBoxFilter(ss,distGradient,15);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(distGradient,-128);
  distGradient->ChannelOffset(1);
  acvImageAdd(distGradient,128);
  acvBoxFilter(ss,distGradient,15);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(distGradient,-128);
  distGradient->ChannelOffset(-1);
  acvScalingSobelResult_n(distGradient);*/

  acvClear(ss,128);
  int lineN=4;
  acv_XY sumXY={0};
  for(int k=0;k<lineN;k++)
  {
    BYTE    Color[3]={HSV_VMax,HSV_VMax,(k)*(HSV_HMax-50)/lineN};
    distGradient->RGBFromHSV(Color,Color);

    acv_XY preXY={
      k/2.0*distGradient->GetWidth()/lineN,
      k/2.0*distGradient->GetHeight()/lineN};
    acv_XY XY;
    acv_XY preForce={0};
    acv_XY Force={0};
    acv_XY speed={0,20};
    for(int i=0;i<100;i++)
    {

      Force=acvSignedMap2Sampling(distGradient,preXY);
      //printf(">%f %f\n",Force.X,Force.Y);
      speed.X+=Force.X/128;
      speed.Y+=Force.Y/128;

      if(preForce.X*Force.X<0)speed.X=0;
      if(preForce.Y*Force.Y<0)speed.Y=0;
      speed.X*=0.95;
      speed.Y*=0.95;
      XY.X=preXY.X+speed.X;
      XY.Y=preXY.Y+speed.Y;
      if(XY.X<0)XY.X=0;
      if(XY.X>distGradient->GetWidth()-1)XY.X=distGradient->GetWidth()-1;
      if(XY.Y<0)XY.Y=0;
      if(XY.Y>distGradient->GetWidth()-1)XY.Y=distGradient->GetWidth()-1;
      acvDrawLine(ss,(int)XY.X,(int)XY.Y,(int)preXY.X,(int)preXY.Y,Color[2],Color[1],Color[0],1);
      preXY=XY;
      preForce=Force;
    }
    sumXY.X+=XY.X;
    sumXY.Y+=XY.Y;
    printf(">%f %f\n",XY.X,XY.Y);
  }
  printf(">%f %f\n",(sumXY.X/lineN),(sumXY.Y/lineN));

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




//Displacement, Scale, Aangle
void acvLabeledPixelExtraction(acvImage  *LabelPic,acv_LabeledData *target_info,int target_idx,std::vector<acv_XY> *retData)
{
  retData->clear();
  int i,j;
  BYTE *L;

  for(i=target_info->LTBound.Y;i<target_info->RBBound.Y+1;i++)
  {
    L=&(LabelPic->CVector[i][(int)target_info->LTBound.X*3]);
    for(j=target_info->LTBound.X;j<target_info->RBBound.X+1;j++,L+=3)
    {
      _24BitUnion *lebel=(_24BitUnion *)L;

      if(lebel->_3Byte.Num==target_idx)
      {
        acv_XY XY={.X=j,.Y=i};
        retData->push_back(XY);
      }
    }
  }
}

void Target_prep(acvImage *target,acvImage *target_DistGradient)
{
  acvLoadBitmapFile(target,"data/target.bmp");
  acvImage *tmp = new acvImage();
  target_DistGradient->ReSize(target->GetWidth(),target->GetHeight());
  tmp->ReSize(target->GetWidth(),target->GetHeight());

  acvThreshold(target,128);
  acvBoxFilter(tmp,target,1);
  acvBoxFilter(tmp,target,1);
  acvCloneImage(target,tmp,1);
  acvCloneImage(target,target,0);


  int mul=1;
  acvDistanceTransform_Chamfer(tmp,5*mul,7*mul);
  //acvDistanceTransform_ChamferX(ss);
  acvInnerFramePixCopy(tmp,1);

  acvDistanceTransform_Sobel(target_DistGradient,tmp);
  acvInnerFramePixCopy(target_DistGradient,2);
  acvInnerFramePixCopy(target_DistGradient,1);
  acvScalingSobelResult_n(target_DistGradient);
  delete(tmp);
  return;
}
void DotsTransform(std::vector<acv_XY> &XY,std::vector<acv_XY> &tXY,float x,float y)
{
  tXY.resize(XY.size());
  for (int i=0;i<tXY.size();i++)
  {
    tXY[i].X=XY[i].X+x;
    tXY[i].Y=XY[i].Y+y;
  }
}
int testEstXY()
{

  acvImage *target = new acvImage();
  acvImage *target_DistGradient = new acvImage();
  Target_prep(target,target_DistGradient);

  acvImage *image = new acvImage();
  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  std::vector<acv_LabeledData> ldData;

  acvLoadBitmapFile(image,"data/test1.bmp");
  image->RGBToGray();
  buff->ReSize(image->GetWidth(),image->GetHeight());
  ss->ReSize(image->GetWidth(),image->GetHeight());

  acvCloneImage(image,ss,0);
  /*acvBoxFilter(buff,image,1);
  acvCloneImage(image,image,0);*/

  acvThreshold(ss,200);
  acvBoxFilter(buff,ss,1);
  acvBoxFilter(buff,ss,1);

  acvCloneImage(ss,image,0);
  acvThreshold(ss,80);
  acvBoxFilter(buff,ss,5);
  acvBoxFilter(buff,ss,5);
  acvThreshold(ss,255-5);

  //acvDeleteFrame(ss,5);
  acvComponentLabeling(ss);
  acvLabeledRegionInfo(ss,&ldData);
  acvRemoveRegionLessThan(ss,&ldData,120);
  acvImage *labelImg=ss;


  acvSaveBitmapFile("data/imageX.bmp",image->ImageData,ss->GetWidth(),ss->GetHeight());
  acvSaveBitmapFile("data/targetX.bmp",target->ImageData,ss->GetWidth(),ss->GetHeight());

  std::vector<acv_XY> regionXY;
  std::vector<acv_XY> mappedXY;
  std::vector<acv_XY> errorXY;

  acv_XY adjXY={0};
  acv_XY deltaXY={0};
  for (int i=1;i<ldData.size();i++)
  {
    printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);

    acvLabeledPixelExtraction(labelImg,&ldData[i],i,&regionXY);


    float alpha=70.0;
    for(int j=0;j<20;j++)
    {
      DotsTransform(regionXY,mappedXY,adjXY.X,adjXY.Y);
      errorXY.resize(regionXY.size());

      acvSpatialMatchingGradient(image,&(regionXY[0]),
      target,target_DistGradient,&(mappedXY[0]),
      &(errorXY[0]),regionXY.size());


      deltaXY.X=0;
      deltaXY.Y=0;
      for (int j=0;j<errorXY.size();j+=1)
      {
        deltaXY.X+=errorXY[j].X;
        deltaXY.Y+=errorXY[j].Y;
      }
      deltaXY.X/=errorXY.size()*256*128;
      deltaXY.Y/=errorXY.size()*256*128;


      alpha*=0.95;
      adjXY.X+=-deltaXY.X*alpha;
      adjXY.Y+=-deltaXY.Y*alpha;
      printf("d:%+02.2f %+02.2f,a:%+02.2f %+02.2f,m:%+02.2f %+02.2f\n",
      deltaXY.X,deltaXY.Y,adjXY.X,adjXY.Y,mappedXY[0].X,mappedXY[0].Y);
    }



  }

  acvLabeledColorDispersion(ss,ss,ldData.size()/20+5);
  acvSaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  return 0;
}


void NNTest()
{
    int NNDim[]={2,4,2};
    MLNN NN(1,NNDim,3);
}
#include <vector>
int main()
{
  //clock_t t= clock();

  //testEstXY();
  NNTest();
  //t = clock() - t;
  //printf("fun() took %f seconds to execute \n", ((double)t)/CLOCKS_PER_SEC);
  //testEstXY();
  //testEstXY();
  int ret = 0;
  printf("Hello, World! %d",ret);
  return ret;
}
