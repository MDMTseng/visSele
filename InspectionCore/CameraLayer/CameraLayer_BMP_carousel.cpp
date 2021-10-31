#include "CameraLayer_BMP.hpp"
#include "acvImage_SpDomainTool.hpp"

#include <logctrl.h> 
#include <dirent.h> 
#include <thread>

#include<sys/time.h>

CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName):
    CameraLayer_BMP(cb,context)
{
    updateFolder(folderName);
    fileIdx=0;
    cameraThread=NULL;
    isThreadWorking=false;


    char buff[300];
    snprintf(buff, sizeof(buff),
    "{\
      \"type\":\"CameraLayer_BMP_carousel\",\
      \"name\":\"BMP_CAM\",\
      \"folder_name\":\"%s\"\
    }",folderName.c_str());
    cam_json_info.assign(buff);
    LOGI(">>>%s",cam_json_info.c_str());
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


CameraLayer::status CameraLayer_BMP_carousel::LoadNext(bool call_cb)
{
    updateFolder(this->folderName);
    if(files_in_folder.size()==0)return NAK;
    fileIdx++;
    if(fileIdx>=files_in_folder.size())
    {
        fileIdx=0;
    }


    CameraLayer_BMP::status status=LoadBMP(files_in_folder[fileIdx]);
    if(status==ACK)
    {
      struct timeval tp;
      gettimeofday(&tp, NULL);
      uint64_t _100us = tp.tv_sec * 1000000 + tp.tv_usec; //get current timestamp in milliseconds

      int newX,newY;
      int newW,newH;
      CalcROI(&newX,&newY,&newW,&newH);
      
      CameraLayer::frameInfo fi_={
        timeStamp_us:(uint64_t)_100us,
        width:(uint32_t)newW,
        height:(uint32_t)newH,
      };
      fi = fi_;
      
      if(call_cb)
        callback(*this,CameraLayer::EV_IMG,context);
    }
    else
    {
      CameraLayer::frameInfo fi_={
        timeStamp_us:0,
        width:0,
        height:0,
      };
      fi = fi_;
      if(call_cb)
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    return status;
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
    // frameInterval_ms=500;
    return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP_carousel::TriggerCount(int count)
{
  imageTakingCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_BMP_carousel::isInOperation()
{
    return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_BMP_carousel::SnapFrame(CameraLayer_Callback snap_cb,void *cb_param)
{
  
  CameraLayer_Callback _callback=callback;
  void* _context=context;//replace the callback
  callback=snap_cb;
  context=cb_param;
  CameraLayer::status ret_s = LoadNext();

  callback=_callback;
  context=_context;//recover the callback
  return ret_s;
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