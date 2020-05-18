
#include "CameraLayer_BMP.hpp"

#include <logctrl.h> 
#include <dirent.h> 
#include <thread>

#include<sys/time.h>

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

CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();
    this->fileName = fileName;

    int ret = 0;
    
    //if(img.GetWidth()<100)//Just to skip image loading
    {
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

      float tmpW=ROI_W;
      float tmpH=ROI_H;
      float tmpX=ROI_X;
      float tmpY=ROI_Y;

      LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
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

      LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
      if(tmpW+tmpX>img_load.GetWidth())
      {
        tmpW=img_load.GetWidth()-tmpX;
      }
      if(tmpH+tmpY>img_load.GetHeight())
      {
        tmpH=img_load.GetHeight()-tmpY;
      }




      int newX=tmpX,newY=tmpY;
      int newH = tmpH;
      int newW = tmpW;

      int tExp=(1<<11)*exp_time_us*a_gain/exp_time_100ExpUs;
      LOGI("tExp:%d",tExp);
      img.ReSize(newW,newH);
      for(int i=0;i<img.GetHeight();i++)//Add noise
      {
        int li=i+newY;
        if(li<0 || li>=img_load.GetHeight())continue;
        for(int j=0;j<img.GetWidth();j++)
        {
          int lj=j+newX;
          if(lj<0 || lj>=img_load.GetWidth())continue;
          // img.CVector[i][j*3]=img_load.CVector[li][lj*3];
          // img.CVector[i][j*3+1]=img_load.CVector[li][lj*3+1];
          // img.CVector[i][j*3+2]=img_load.CVector[li][lj*3+2];


          int d = (img_load.CVector[li][lj*3]*tExp)>>11;
          
          // if(i==img.GetHeight()/2&&j==img_load.GetWidth()/2)
          //   LOGI("%d>>>%d",img.CVector[li][lj*3],d);

          if(d<0)d=0;
          else if(d>255)d=255;
          
          img.CVector[li][lj*3] = 
          img.CVector[li][lj*3+1] =
          img.CVector[li][lj*3+2] = d;

        }
      }

      if(0)for(int i=0;i<img.GetHeight();i++)//Add noise
      {
          for(int j=0;j<img.GetWidth();j++)
          {
              int u = rand()%(gaussianNoiseTable_M.size());
              int x = gaussianNoiseTable_M[u] * 3/1000000 + 0;

              int d = img.CVector[i][j*3];
              d+=x;
              if(d<0)d=0;
              else if(d>255)d=255;
              
              img.CVector[i][j*3] = d;
              img.CVector[i][j*3+1] = d;
              img.CVector[i][j*3+2] = d;

          }
      }


      ret_status=ACK;
      struct timeval tp;
      gettimeofday(&tp, NULL);
      long int _100us = tp.tv_sec * 10000 + tp.tv_usec / 100; //get current timestamp in milliseconds

      CameraLayer::frameInfo fi_={
        timeStamp_100us:(uint64_t)_100us,
        width:(uint32_t)img.GetWidth(),
        height:(uint32_t)img.GetHeight(),
      };
      fi = fi_;
      callback(*this,CameraLayer::EV_IMG,context);
    }
    m.unlock();
    return ret_status;
}



CameraLayer::status CameraLayer_BMP::SetROI(float x, float y, float w, float h,int zw,int zh)
{
  ROI_X=x;
  ROI_Y=y;
  ROI_W=w;
  ROI_H=h;
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_BMP::SetMirror(int Dir,int en)
{
  if(Dir)
  {
    MIRROR_X=en;
  }
  else
  {
    MIRROR_Y=en;
  }
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetAnalogGain(int gain)
{
  a_gain=gain;
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetExposureTime(double time_us)
{
  //default 5ms as 100% exposure
  exp_time_us=time_us;

  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP::GetExposureTime(double *ret_time_us)
{
  if(ret_time_us)
  {
    *ret_time_us=exp_time_us;
    return CameraLayer::ACK;
  }
  return CameraLayer::NAK;
}

CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName):
    CameraLayer_BMP(cb,context)
{
    updateFolder(folderName);
    fileIdx=0;
    cameraThread=NULL;
    isThreadWorking=false;
}

CameraLayer_BMP_carousel::~CameraLayer_BMP_carousel()
{
    LOGI("Descructor...");
}



CameraLayer::status CameraLayer_BMP_carousel::updateFolder(std::string folderName)
{
    this->folderName  = folderName;
    files_in_folder.resize(0);
    DIR *d;
    struct dirent *dir;
    d = opendir(folderName.c_str());
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0]=='.')continue;
            std::string str(dir->d_name);
            str = folderName+"/"+str;
            //LOGV("FILE::%s",str.c_str());
            files_in_folder.push_back(str);
        }
        closedir(d);
        
        return ACK;
    }
    else
    {
        return NAK;
    }
}


CameraLayer::status CameraLayer_BMP_carousel::LoadNext()
{
    updateFolder(this->folderName);
    if(files_in_folder.size()==0)return NAK;
    fileIdx++;
    if(fileIdx>=files_in_folder.size())
    {
        fileIdx=0;
    }
    return LoadBMP(files_in_folder[fileIdx]);
}

CameraLayer::status CameraLayer_BMP_carousel::Trigger()
{
    imageTakingCount+=1;
    
    LOGI("imageTakingCount:%d",imageTakingCount);
    if(imageTakingCount==0)imageTakingCount=1;
    return TriggerMode(-1);
}

void CameraLayer_BMP_carousel::ContTriggerThread( )
{
  isThreadWorking=true;
    //The logic here is you always stay in the loop,
    //and execute LoadNext when imageTakingCount >0
    int idle_loop_interval_ms =100;
    while( ThreadTerminationFlag == 0)
    {
        LOGV("TTFlag:%d imageTakingCount:%d triggerMode:%d",
            ThreadTerminationFlag,imageTakingCount,triggerMode);
        int delay_time=0;
        if(imageTakingCount>0 || triggerMode==0)
        {
            LOGV("imageTakingCount:%d,ThreadTerminationFlag:%d",imageTakingCount,ThreadTerminationFlag);
            for(int i=0;i<10;i++)
            {
              
              if(LoadNext()==ACK)
              {
                break;
              }
              //otherwise retry
            }
            imageTakingCount--;
            
            if(triggerMode==0)
            {
                imageTakingCount=0;
            }
        }
        else
        {//So we need a delay when imageTakingCount<=0 to prevent loop becoming too fast
            //std::this_thread::sleep_for(std::chrono::milliseconds(idle_loop_interval_ms));
            delay_time+=idle_loop_interval_ms;
        }

        if(imageTakingCount==0 && triggerMode!=0)
        {
            break;
        }

        if(frameInterval_ms-delay_time>0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(frameInterval_ms-delay_time));
        }
    }
    //ThreadTerminationFlag = 0;

  isThreadWorking=false;
}


CameraLayer::status CameraLayer_BMP_carousel::SetFrameRateMode(int mode)
{
    switch(mode)
    {
        case 0:frameInterval_ms=1000;break;
        case 1:frameInterval_ms=100;break;
        case 2:frameInterval_ms=10;break;
        case 3:frameInterval_ms=0;break;
    }
    frameInterval_ms=150;
    return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP_carousel::TriggerMode(int mode)
{
    
    if(mode>=0)
    {
        triggerMode = mode;
    }
    if(mode==0 || mode==-1 )
    {
      LOGI("ThreadTerminationFlag:%d cameraThread:%p>>>",ThreadTerminationFlag,cameraThread);
        ThreadTerminationFlag = 0;
        if(isThreadWorking==false && cameraThread!=NULL)//Try to clean up the thread
        {
          cameraThread->join();
          delete cameraThread;
          cameraThread = NULL;
        }
        if(cameraThread == NULL)
        {
            cameraThread = new std::thread(&CameraLayer_BMP_carousel::ContTriggerThread, this);
        }
    }
    else
    {
        ThreadTerminationFlag = 1;
        if(cameraThread)
        {
            cameraThread->join();
            delete cameraThread;
            cameraThread = NULL;
        }
    }
    return ACK;
}