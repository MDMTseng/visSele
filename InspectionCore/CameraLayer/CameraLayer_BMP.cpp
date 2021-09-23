
#include "CameraLayer_BMP.hpp"
#include "acvImage_SpDomainTool.hpp"

#include <logctrl.h> 
#include <dirent.h> 
#include <thread>

#include<sys/time.h>
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

CameraLayer_BMP::CameraLayer_BMP(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{
    gaussianNoiseTable_M.resize(256);
    for(int i=0;i<gaussianNoiseTable_M.size();i++)
    {
        float u = rand() / (float)RAND_MAX;
        float v = rand() / (float)RAND_MAX;
        float x = sqrt(-2 * log(u)) * cos(2 * M_PI * v);
        gaussianNoiseTable_M[i]=(int)(x*1000000);//million scale up
    }
    a_gain=1;
    SetExposureTime(exp_time_100ExpUs);
}



CameraLayer::status CameraLayer_BMP::ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount)
{

      int newX,newY;
      int newW,newH;

      CalcROI(&newX,&newY,&newW,&newH);

      // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
      if(pixelCount<newW*newH)
      {
        return NAK;
      }      

      if(exp_time_us==0)exp_time_us=exp_time_100ExpUs;
      float brightnessMult=((float)(rand()%2000)/1000)-1;//-1~1

      brightnessMult=brightnessMult*0.2+1;
      
      int tExp=(1<<13)*brightnessMult*exp_time_us*a_gain/exp_time_100ExpUs;
      LOGI("tExp:%d",tExp);
      static float rotate=0;
      
      int noiseRange=15;
      if(newX==0&&newY==0)
      {
        rotate=0;
      }
      else
      {
        float baseAngle=0*M_PI/180;
        float endAngle= 360*M_PI/180;

        if(rotate<baseAngle)rotate=baseAngle;
        else if(rotate>endAngle)rotate=baseAngle;
        else
        {
          rotate+=0*M_PI/180;
        }
        // rotate+=1*M_PI/180;
        LOGI("ROTATE:%f",rotate*180/M_PI);
        // 
      }

      
      // img.ReSize(newW,newH);
      acv_XY rcenter={.X=(float)(newW/2),.Y=(float)(newH/2)};

      if(rotate!=0)
      {
        for(int i=0;i<newH;i++)//Add noise
        for(int j=0;j<newW;j++)
        {
          acv_XY pixCoord=acvVecSub((acv_XY){(float)j,(float)i},rcenter);
          
          pixCoord = acvRotation(rotate,pixCoord);
          pixCoord=acvVecAdd(pixCoord,rcenter);
          pixCoord=acvVecAdd(pixCoord,(acv_XY){(float)newX,(float)newY});

          
          pixCoord=acvVecSub((acv_XY){
            MIRROR_X?(float)img_load.GetWidth():pixCoord.X*2,
            MIRROR_Y?(float)img_load.GetHeight():pixCoord.Y*2},pixCoord);


          // for(int k=0;k<channelCount;k++)
          // {
          //   float pix= acvUnsignedMap1Sampling(&img_load, pixCoord, k);
            
          //   int N=0;
          //   if(noiseRange>0)
          //     N= (rand()%(2*noiseRange+1))-noiseRange;

          //   int d = N+ ((uint64_t)(pix*tExp))>>13;

          //   if(d<0)d=0;
          //   else if(d>255)d=255;
            
          //   imgBuffer[(i*newW+j)*channelCount+k]=d;

          // }

          float pix= acvUnsignedMap1Sampling(&img_load, pixCoord, 0);
          
          int N=0;
          if(noiseRange>0)
            N= (rand()%(2*noiseRange+1))-noiseRange;

          int d = N+ (((uint64_t)(pix*tExp))>>13);

          if(d<0)d=0;
          else if(d>255)d=255;
          
          imgBuffer[(i*newW+j)*channelCount+0]=
          imgBuffer[(i*newW+j)*channelCount+1]=
          imgBuffer[(i*newW+j)*channelCount+2]=d;

        }
      }
      else if(1)
      {
        
        // LOGI(">>>:::W:%d H:%d\n",img->GetWidth(),img->GetHeight());
        // if(1)
        // {
        //   memcpy(imgBuffer,img_load.CVector[0],);
        //   acvCloneImage(&img_load,&img,-1);    

        // }  
        // else 
        for(int i=0;i<newH;i++)//exposure add
        {
          int li=i+newY;
          if(li<0 || li>=img_load.GetHeight())continue;
          for(int j=0;j<newW;j++)
          {
            int lj=j+newX;
            if(lj<0 || lj>=img_load.GetWidth())continue;
            int N=0;
            if(noiseRange>0)
              N= (rand()%(2*noiseRange+1))-noiseRange;
            int d =N+ ((img_load.CVector[li][lj*3]*tExp)>>13);
            
            if(d<0)d=0;
            else if(d>255)d=255;
            
            imgBuffer[(i*newW+j)*channelCount+0]=
            imgBuffer[(i*newW+j)*channelCount+1]=
            imgBuffer[(i*newW+j)*channelCount+2]=d;
            // img.CVector[i][j*3] = 
            // img.CVector[i][j*3+1] =
            // img.CVector[i][j*3+2] = d;

          }
        }
      }
      else
      {
        // acvCloneImage(&img_load,&img,-1);
        memcpy(imgBuffer,img_load.CVector[0],newH*newW*channelCount);
      }
      

      // int noiseRange=5;
      // for(int i=0;i<newH;i++)//Add noise
      // {
      //     for(int j=0;j<newW;j++)
      //     {
      //         // int u = rand()%(gaussianNoiseTable_M.size());
      //         // int x = gaussianNoiseTable_M[u] * 3/1000000 + 0;
      //         int x=(rand()%(2*noiseRange))-noiseRange;
      //         int d = imgBuffer[(i*newW+j)*channelCount+0];
      //         d+=x;
      //         if(d<0)d=0;
      //         else if(d>255)d=255;
              
      //         imgBuffer[(i*newW+j)*channelCount+0] =
      //         imgBuffer[(i*newW+j)*channelCount+1] =
      //         imgBuffer[(i*newW+j)*channelCount+2] =d;

      //     }
      // }


  return ACK;
}

CameraLayer_BMP::status CameraLayer_BMP::CalcROI(int* X,int* Y,int* W,int* H)
{
  
  int tmpW=ROI_W;
  int tmpH=ROI_H;
  int tmpX=ROI_X;
  int tmpY=ROI_Y;

  // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
  if(tmpX<0){
    tmpX=0;
  }
  if(tmpY<0){
    tmpY=0;
  }
  if(tmpX<1)
  {
    tmpX=(int)(tmpX*img_load.GetWidth());
  }
  if(tmpY<1)
  {
    tmpY=(int)(tmpY*img_load.GetHeight());
  }

  if(tmpX>=img_load.GetWidth()-5)
  {
    tmpX=img_load.GetWidth()-5-1;
  }
  if(tmpY>=img_load.GetHeight()-5)
  {
    tmpY=img_load.GetHeight()-5-1;
  }

  if(tmpW<0){
    tmpW=0;
  }
  if(tmpH<0){
    tmpH=0;
  }
  if(tmpW<1)
  {
    tmpW=(int)(tmpW*img_load.GetWidth());
  }
  if(tmpH<1)
  {
    tmpH=(int)(tmpH*img_load.GetHeight());
  }

  if(tmpW+tmpX>img_load.GetWidth())
  {
    tmpW=img_load.GetWidth()-tmpX;
  }
  if(tmpH+tmpY>img_load.GetHeight())
  {
    tmpH=img_load.GetHeight()-tmpY;
  }
  if(X)*X=tmpX;
  if(Y)*Y=tmpY;
  if(W)*W=tmpW;
  if(H)*H=tmpH;

  return ACK;
}
CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();

    int ret = 0;
    
    //if(img.GetWidth()<100)//Just to skip image loading
    cacheUseCounter++;
    if(this->fileName.compare(fileName)!=0 || cacheUseCounter>20)//check if the name isn't equal
    {
      cacheUseCounter=0;
      this->fileName = fileName;
        LOGI("Loading:%s",fileName.c_str());
        ret = acvLoadBitmapFile(&img_load, fileName.c_str());
    }
    if(ret!=0)
    {
      ret_status=NAK;
      callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
      ret_status=ACK;
      // ret_status=NAK;

      // int X,Y,W,H;

      // CalcROI(&X,&Y,&W,&H);

      
      // CameraLayer::frameInfo fi_={
      //   timeStamp_us:0,
      //   width:(uint32_t)W,
      //   height:(uint32_t)H,
      // };

      // callback(*this,CameraLayer::EV_IMG,context);

    }
    m.unlock();
    return ret_status;
}



CameraLayer::status CameraLayer_BMP::SetROI(int x, int y, int w, int h,int zw,int zh)
{
  ROI_X=x;
  ROI_Y=y;
  ROI_W=w;
  ROI_H=h;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP::GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh)
{
  return CalcROI(x,y,w,h);
}
CameraLayer::status CameraLayer_BMP::SetMirror(int Dir,int en)
{
  if(Dir==0)
  {
    MIRROR_X=en;
  }
  else if(Dir==1)
  {
    MIRROR_Y=en;
  }
  else
  {
    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetAnalogGain(float gain)
{
  a_gain=gain;
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetExposureTime(float time_us)
{
  //default 5ms as 100% exposure
  exp_time_us=time_us;

  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP::GetExposureTime(float *ret_time_us)
{
  if(ret_time_us)
  {
    *ret_time_us=exp_time_us;
    return CameraLayer::ACK;
  }
  return CameraLayer::NAK;
}
