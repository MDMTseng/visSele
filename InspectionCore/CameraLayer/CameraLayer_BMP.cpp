
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
}

CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();
    this->fileName = fileName;

    int ret = 0;
    
    //if(img.GetWidth()<100)//Just to skip image loading
    {
        LOGV("Loading:%s",fileName.c_str());
        ret = acvLoadBitmapFile(&img_load, fileName.c_str());
    }
    if(ret!=0)
    {
        ret_status=NAK;
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
      int newH = (ROI_H<img_load.GetHeight()-ROI_Y)?ROI_H:img_load.GetHeight()-ROI_Y;
      int newW = (ROI_W<img_load.GetWidth() -ROI_X)?ROI_W:img_load.GetWidth() -ROI_X;

      img.ReSize(newW,newH);
      for(int i=0;i<img.GetHeight();i++)//Add noise
      {
        int li=i+ROI_Y;
        if(li<0 || li>=img_load.GetHeight())continue;
        for(int j=0;j<img.GetWidth();j++)
        {
          int lj=j+ROI_X;
          if(lj<0 || lj>=img_load.GetWidth())continue;
          img.CVector[i][j*3]=img_load.CVector[li][lj*3];
          img.CVector[i][j*3+1]=img_load.CVector[li][lj*3+1];
          img.CVector[i][j*3+2]=img_load.CVector[li][lj*3+2];
        }
      }

        if(0)for(int i=0;i<img.GetHeight();i++)//Add noise
        {
            for(int j=0;j<img.GetWidth();j++)
            {
                int u = rand()%(gaussianNoiseTable_M.size());
                int x = gaussianNoiseTable_M[u] * 10/1000000 + 0;

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


CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName):
    CameraLayer_BMP(cb,context)
{
    updateFolder(folderName);
    fileIdx=0;
    cameraThread=NULL;
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
    
    LOGV("imageTakingCount:%d",imageTakingCount);
    if(imageTakingCount==0)imageTakingCount=1;
    return TriggerMode(-1);
}

void CameraLayer_BMP_carousel::ContTriggerThread( )
{
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
            LoadNext();
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
    frameInterval_ms=2000;
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
        ThreadTerminationFlag = 0;
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