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
  acvScalingSobelResult_n(buff);
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

void Target_prep_dist(acvImage *target,acvImage *target_DistGradient, vector<acv_XY> &signature)
{
  acvLoadBitmapFile(target,"data/target.bmp");
  acvImage *sign = new acvImage();
  acvImage *tmp = new acvImage();
  target_DistGradient->ReSize(target->GetWidth(),target->GetHeight());
  tmp->ReSize(target->GetWidth(),target->GetHeight());
  sign->ReSize(target->GetWidth(),target->GetHeight());

  acvCloneImage(target,sign,1);

  acvThreshold(target,128);
  acvBoxFilter(tmp,target,3);
  acvBoxFilter(tmp,target,3);
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


  acvImageAdd(target_DistGradient,128);
  acvBoxFilter(tmp,target_DistGradient,5);
  acvBoxFilter(tmp,target_DistGradient,5);
  acvImageAdd(target_DistGradient,-128);
  target_DistGradient->ChannelOffset(1);
  acvImageAdd(target_DistGradient,128);
  acvBoxFilter(tmp,target_DistGradient,5);
  acvBoxFilter(tmp,target_DistGradient,5);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(target_DistGradient,-128);
  target_DistGradient->ChannelOffset(-1);
  //acvScalingSobelResult_n(target_DistGradient);


  //Generate signature

  acvBoxFilter(tmp,sign,4);
  acvThreshold(sign,100);

    acvBoxFilter(tmp,sign,5);
    acvThreshold(sign,255-15);
  acvComponentLabeling(sign);

  std::vector<acv_LabeledData> ldData;
  acvLabeledRegionInfo(sign,&ldData);
  for (int i=1;i<ldData.size();i++)
  {
    //printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvContourCircleSignature(sign,ldData[i],i,signature);

  }

  delete(sign);
  delete(tmp);
  return;
}


void Target_prep_sobel(acvImage *target,acvImage *target_DistGradient)
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

/*

  acvImageAdd(target_DistGradient,128);
  acvBoxFilter(tmp,target_DistGradient,15);
  acvBoxFilter(tmp,target_DistGradient,15);
  acvImageAdd(target_DistGradient,-128);
  target_DistGradient->ChannelOffset(1);
  acvImageAdd(target_DistGradient,128);
  acvBoxFilter(tmp,target_DistGradient,15);
  acvBoxFilter(tmp,target_DistGradient,15);
  //acvBoxFilter(ss,distGradient,15);
  acvImageAdd(target_DistGradient,-128);
  target_DistGradient->ChannelOffset(-1);
  acvScalingSobelResult_n(target_DistGradient);

*/
  delete(tmp);
  return;
}
void DotsTransform(std::vector<acv_XY> &XY,std::vector<acv_XY> &tXY,MLNN &NN,acv_XY transOffset,float scale)
{
  vector<vector<float> > &in_vec=NN.get_input_vec();
  tXY.resize(XY.size());
  for(int j=0;j<in_vec.size();j++)
  {
    in_vec[j][0]=(XY[j].X-transOffset.X)*scale;
    in_vec[j][1]=(XY[j].Y-transOffset.Y)*scale;
  }

  NN.ForwardPass();
  for(int j=0;j<NN.p_pred_Y->size();j++)
  {
      tXY[j].X=(*NN.p_pred_Y)[j][0]/scale+transOffset.X;
      tXY[j].Y=(*NN.p_pred_Y)[j][1]/scale+transOffset.Y;
  }

}

void sampleXYFromRegion(vector<acv_XY> &sampleXY,const vector<acv_XY> &regionXY,int sampleCount)
{
  sampleXY.clear();
  static int offset = rand()%(regionXY.size()/sampleCount);
  for(int i=0;i<sampleCount;i++)
  {

    int randIdx=i*regionXY.size()/sampleCount+offset;
    randIdx=randIdx%regionXY.size();
    sampleXY.push_back(regionXY[randIdx]);
  }
}


void sampleXYFromRegion_Seq(vector<acv_XY> &sampleXY,const vector<acv_XY> &regionXY,int from,int sampleCount)
{
  sampleXY.clear();
  for(int i=0;i<sampleCount;i++)
  {
    sampleXY.push_back(regionXY[from+i]);
  }
}

#include<unistd.h>

int testEstXY()
{


  vector<acv_XY> tar_signature(100);
  acvImage *target = new acvImage();
  acvImage *target_DistGradient = new acvImage();
  Target_prep_dist (target,target_DistGradient,tar_signature);

  acvImage *image = new acvImage();
  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  std::vector<acv_LabeledData> ldData;

  acvLoadBitmapFile(image,"data/target.bmp");
  image->RGBToGray();
  buff->ReSize(image->GetWidth(),image->GetHeight());
  ss->ReSize(image->GetWidth(),image->GetHeight());

  acvCloneImage(image,ss,0);
  /*acvBoxFilter(buff,image,1);
  acvCloneImage(image,image,0);*/

  acvThreshold(ss,200);
  acvBoxFilter(buff,ss,1);
  acvBoxFilter(buff,ss,1);
  acvBoxFilter(buff,ss,1);
  acvBoxFilter(buff,ss,1);

  acvThreshold(ss,100);

    acvCloneImage(ss,image,0);
    acvBoxFilter(buff,image,1);
    acvBoxFilter(buff,image,1);
    acvThreshold(image,170);
    acvBoxFilter(buff,image,1);
    acvBoxFilter(buff,image,2);
    acvBoxFilter(buff,image,2);
    acvBoxFilter(buff,image,1);

  acvBoxFilter(buff,ss,5);
  acvBoxFilter(buff,ss,5);
  acvThreshold(ss,255-15);

  //acvDeleteFrame(ss,5);
  acvComponentLabeling(ss);
  acvLabeledRegionInfo(ss,&ldData);
  acvRemoveRegionLessThan(ss,&ldData,120);
  acvImage *labelImg=ss;


  //acvSaveBitmapFile("data/imageX.bmp",image->ImageData,ss->GetWidth(),ss->GetHeight());
  //acvSaveBitmapFile("data/targetX.bmp",target->ImageData,ss->GetWidth(),ss->GetHeight());


  std::vector<acv_XY> regionXY_;

  std::vector<acv_XY> regionSampleXY;
  std::vector<acv_XY> mappedXY;
  std::vector<acv_XY> errorXY;

  printf("**********************\n");
  for (int i=1;i<ldData.size();i++)
  {
    printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);

    acvLabeledPixelExtraction(labelImg,&ldData[i],i,&regionXY_);

    //******************************************


      int dim_in=2;
      int dim_out=2;
      int batchSize=300;
      MLNNUtil nu;
      vector<vector<float> > error_gradient;
      nu.Init2DVec(error_gradient,batchSize,dim_out);

      int NNDim[]={dim_in,dim_out};
      MLNN NN(batchSize,NNDim,sizeof(NNDim)/sizeof(*NNDim));

      MLOpt mo(NN.layers[0]);
      float theta=20*M_PI/180;
      float scale=1;
      NN.layers[0].W[0][0]=cos(theta)*scale;
      NN.layers[0].W[1][1]=cos(theta)*scale;
      NN.layers[0].W[0][1]=-sin(theta)*scale;
      NN.layers[0].W[1][0]=sin(theta)*scale;

      NN.layers[0].W[2][0]=50/50;
      NN.layers[0].W[2][1]=1/50;

  //******************************************

    clock_t t= clock();
    float alpha=5;
    for(int j=0;j<100+1;j++)
    {
      sampleXYFromRegion(regionSampleXY,regionXY_,batchSize);
      //printf("***********%d***********\n",j);
      //NN.layers[0].printW();
      DotsTransform(regionSampleXY,mappedXY,NN,ldData[i].Center,1);
      /*for (int k=0;k<batchSize;k+=1)
      {
        printf("%f %f\n",mappedXY[k].X,mappedXY[k].Y);
      }*/

      errorXY.resize(regionSampleXY.size());


      float error=acvSpatialMatchingGradient(image,&(regionSampleXY[0]),
      target,target_DistGradient,&(mappedXY[0]),
      &(errorXY[0]),regionSampleXY.size());
      for (int k=0;k<errorXY.size();k+=1)
      {
        error_gradient[k][0]=-errorXY[k].X/(errorXY.size()*256*128);
        error_gradient[k][1]=-errorXY[k].Y/(errorXY.size()*256*128);
          //printf("%f %f\n",error_gradient[k][0],error_gradient[k][1]);
      }
      //alpha*=0.95;
      NN.backProp(error_gradient);
      mo.update_dW();
      NN.updateW(alpha);
      //nu.printMat(NN.layers[0].dW);printf("\n");
      NN.reset_deltaW();
      /*printf("%f  %f %f %f\n",error,
      NN.layers[0].W[2][0]*100,NN.layers[0].W[2][1]*100,
      180/M_PI*atan2(NN.layers[0].W[1][0]-NN.layers[0].W[0][1],NN.layers[0].W[0][0]+NN.layers[0].W[1][1]));
*/

      float a00=(NN.layers[0].W[0][0]+NN.layers[0].W[1][1])/2;
      float a10=(NN.layers[0].W[1][0]-NN.layers[0].W[0][1])/2;
      float LL=hypot(a00, a10);
      a00/=LL;
      a10/=LL;
      NN.layers[0].W[0][0]=a00;
      NN.layers[0].W[0][1]=-a10;
      NN.layers[0].W[1][0]=a10;
      NN.layers[0].W[1][1]=a00;

      if(j%1!=0)continue;
      continue;
      sleep(1);

      acvCloneImage(target,buff,0);
      for(int j=0;j<regionXY_.size()/batchSize;j++)
      {
          sampleXYFromRegion_Seq(regionSampleXY,regionXY_,j*batchSize,batchSize);
          DotsTransform(regionSampleXY,mappedXY,NN,ldData[i].Center,1);
          for(int k=0;k<regionSampleXY.size();k++)
          {
            buff->CVector[(int)round(mappedXY[k].Y)][(int)round(mappedXY[k].X)*3+2]=
            255-image->CVector[(int)round(regionSampleXY[k].Y)][(int)round(regionSampleXY[k].X)*3];
          }

      }

      //acvSaveBitmapFile("data/target_test_cover.bmp",buff->ImageData,buff->GetWidth(),buff->GetHeight());
          //  NN.layers[0].printW();
      //return 0;
    }

      t = clock() - t;
      printf("%fms ..\n", ((double)t)/CLOCKS_PER_SEC*1000);
      t = clock();

  }



  //acvLabeledColorDispersion(ss,ss,ldData.size()/20+5);
  //acvSaveBitmapFile("data/uu_o.bmp",ss->ImageData,ss->GetWidth(),ss->GetHeight());
  return 0;
}



float SignatureMatchingError
(const acv_XY *signature,int offset,
  const acv_XY *tar_signature,int arrsize,int stride)
{
  if( stride==0)return -1;
  float errorSum=0;
  int signIdx=offset%arrsize;
  if(signIdx<0)signIdx+=arrsize;
  int size=(arrsize-signIdx);
  int i=0;

  for(;i<size;i+=stride)
  {
    float error=signature[(signIdx)].X-tar_signature[i].X;
    signIdx+=stride;
    errorSum+=error*error;
  }
  signIdx-=arrsize;
  size=arrsize;
  for(;i<size;i+=stride)
  {
    float error=signature[(signIdx)].X-tar_signature[i].X;
    signIdx+=stride;
    errorSum+=error*error;
  }
  return errorSum;
}


float SignatureMatchingError
(const vector<acv_XY> &signature,int offset,
  const vector<acv_XY> &tar_signature,int stride)
{
  return SignatureMatchingError(&(signature[0]),offset,&(tar_signature[0]),signature.size(),stride);
}
#include <float.h>
int SignareIdxOffsetMatching
(const vector<acv_XY> &signature,
  const vector<acv_XY> &tar_signature,int roughSearchSampleRate,float *min_error)
{
  int fineSreachRadious=roughSearchSampleRate;
  int minErrOffset=0;
  float minErr=FLT_MAX;//rough search
  for (int j=0;j<tar_signature.size();j+=roughSearchSampleRate)
  {
    float error=SignatureMatchingError(signature,j,tar_signature,roughSearchSampleRate);
    if(minErr>error)
    {
      minErr=error;
      minErrOffset=j;
    }
  }
  minErr=FLT_MAX;
  int searchHead=minErrOffset-fineSreachRadious;
  minErrOffset=-1;

  float error;
  //fine search
  for(int i=0;i<2*fineSreachRadious+1;i++,searchHead++)
  {

    error=SignatureMatchingError(signature,searchHead,tar_signature,1);
    if(minErr>error)
    {
      minErr=error;
      minErrOffset=searchHead;
    }
  }
  if(minErrOffset<0)minErrOffset+=tar_signature.size();
  else if(minErrOffset>=tar_signature.size())minErrOffset-=tar_signature.size();
  if(min_error)*min_error=minErr;
  return minErrOffset;
}

float SignatureAngleMatching
(const vector<acv_XY> &signature,
  const vector<acv_XY> &tar_signature,float *min_error)
{
  int matchingIdx=SignareIdxOffsetMatching(signature,tar_signature,signature.size()/160,min_error);//magic number
  float angle=matchingIdx*2*M_PI/signature.size();
  if(angle<-M_PI)
    angle+=2*M_PI;
  else if(angle>M_PI)
    angle-=2*M_PI;
  return angle;
/*
  float signAngle=0;
  float tar_signAngle=0;
  float addWeight=0;
  float aveWeight=0;
  float tar_addWeight=0;
  float tar_aveWeight=0;

  for(int i=0;i<signature.size();i++)
  {
    aveWeight+=(signature[i].X);
    tar_aveWeight+=(tar_signature[i].X);
  }
  aveWeight/=signature.size();
  tar_aveWeight/=signature.size();


  int signOffsetIdx=matchingIdx;
  for(int i=0;i<signature.size();i++,signOffsetIdx++)
  {
    if(signOffsetIdx>=signature.size())signOffsetIdx-=signature.size();
    if(signature[signOffsetIdx].X<aveWeight)continue;
    float diff=signature[signOffsetIdx].Y-tar_signature[i].Y;
    if(diff<-M_PI)
      diff+=2*M_PI;
    else if(diff>M_PI)
      diff-=2*M_PI;
    printf("%.3f>>%.3f\n",signature[signOffsetIdx].X,diff*180/M_PI);
    signAngle+=diff*(signature[signOffsetIdx].X-aveWeight);
    addWeight+=(signature[signOffsetIdx].X-aveWeight);
  }
  float diffAngle=signAngle/addWeight;

  return diffAngle;*/
}

int testSignature()
{


  vector<acv_XY> tar_signature(500);
  acvImage *target = new acvImage();
  acvImage *target_DistGradient = new acvImage();
  Target_prep_dist (target,target_DistGradient,tar_signature);


  acvImage *image = new acvImage();
  acvImage *ss = new acvImage();
  acvImage *buff = new acvImage();
  std::vector<acv_LabeledData> ldData;

  acvLoadBitmapFile(image,"data/test1.bmp");
  image->RGBToGray();
  buff->ReSize(image->GetWidth(),image->GetHeight());
  ss->ReSize(image->GetWidth(),image->GetHeight());

  vector<acv_XY> signature(500);

  clock_t t= clock();

    acvCloneImage(image,ss,0);
    /*acvBoxFilter(buff,image,1);
    acvCloneImage(image,image,0);*/

    acvBoxFilter(buff,ss,4);
    acvThreshold(ss,100);

      t = clock() - t;
      printf("%fms ..\n", ((double)t)/CLOCKS_PER_SEC*1000);
      t = clock();

    acvBoxFilter(buff,ss,5);
    acvThreshold(ss,255-15);
  //acvDeleteFrame(ss,5);
  acvComponentLabeling(ss);


  acvLabeledRegionInfo(ss,&ldData);
  acvRemoveRegionLessThan(ss,&ldData,120);
  acvImage *labelImg=ss;

  acvCloneImage(ss,image,-1);

  t = clock() - t;
  printf("%fms ..\n", ((double)t)/CLOCKS_PER_SEC*1000);
  t = clock();
  for (int i=1;i<ldData.size();i++)
  {
    //printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    acvContourCircleSignature(ss,ldData[i],i,signature);

    //acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);
    acv_XY preXY;
    int preR=0;
    int pretar_R=0;
    float error;
    float AngleDiff=SignatureAngleMatching(signature,tar_signature,&error);


    t = clock() - t;
    printf("%fms .Match. AngleDiff:%f,er:%f\n", ((double)t)/CLOCKS_PER_SEC*1000,
      AngleDiff*180/M_PI,error);
    t = clock();

    if(1)for (int j=0;j<signature.size();j++)
    {
      if(signature[j].X==0)printf(">(%d)",j);
      acv_XY nowXY;
      nowXY.Y=signature[j].X*sin(signature[j].Y)+ldData[i].Center.Y;
      nowXY.X=signature[j].X*cos(signature[j].Y)+ldData[i].Center.X;
      if(j==0)preXY=nowXY;
      acvDrawLine(image,round(preXY.X),round(preXY.Y),
        round(nowXY.X),round(nowXY.Y),i+2,0,0,5);
      preXY=nowXY;


      int R=image->GetHeight()-signature[j].X;
      acvDrawLine(image,(j-1)*image->GetWidth()/signature.size(),preR,
        j*image->GetWidth()/signature.size(),R,i+3,0,0,1);


      int tar_R=image->GetHeight()-tar_signature[j].X;
      acvDrawLine(image,(j-1)*image->GetWidth()/tar_signature.size(),pretar_R,
        j*image->GetWidth()/tar_signature.size(),tar_R,i+5,0,0,1);
      preR=R;
      pretar_R=tar_R;
    }
  }

  acvLabeledColorDispersion(image,image,ldData.size()/20+5);
  acvSaveBitmapFile("data/uu_o.bmp",image->ImageData,ss->GetWidth(),ss->GetHeight());
}

#include <vector>
int main()
{
  testSignature();
  //clock_t t= clock();
//TargetPrep();
  //testEstXY();
  //NNTest();
  //t = clock() - t;
  //printf("fun() took %f seconds to execute \n", ((double)t)/CLOCKS_PER_SEC);
  //testEstXY();
  //testEstXY();
  int ret = 0;
  printf("Hello, World! %d",ret);
  return ret;
}
