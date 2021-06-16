#include "CameraLayer.hpp"


#include "logctrl.h"
#include <future>

CameraLayer::status CameraLayer::SNAP_Callback(CameraLayer &cl_obj, int type, void* obj)
{  

  CameraLayer &Cam=*((CameraLayer*)(&cl_obj));
  CameraLayer::status ret_st=Cam._snap_cb(cl_obj,type,obj);
  Cam.snapFlag=0;

  Cam.conV.notify_one();
  return ret_st;
}

CameraLayer::status CameraLayer::SnapAbort(int timeout_ms)
{
  if(timeout_ms<0)
  {
    return NAK;
  }
  

  LOGI(">>timeout_ms:%d>snapFlag:%d>",timeout_ms,snapFlag);
  if(timeout_ms>0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
  }
  if(snapFlag==1)
  {
    snapFlag=-1;
    conV.notify_one();
    return ACK;
  }
  return NAK;
}

CameraLayer::status  CameraLayer::SnapFrame(CameraLayer_Callback snap_cb,void *cb_param,int type,int timeout_ms)
{
  if(type==1 ||type==0 )
  {
    TriggerMode(1);
  }
  else if(type==2 )
  {
    TriggerMode(2);
  }
  else
  {
    return NAK;
  }

  CameraLayer::status retStatus=NAK; 
  snapFlag = 1;
  //trigger reset;
  {
    std::unique_lock<std::mutex> lock(m);

    CameraLayer_Callback _callback=callback;
    void* _context=context;//replace the callback
    _snap_cb=snap_cb;
    callback=SNAP_Callback;
    context=cb_param;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    LOGI(">>>>");
    if(type==0)
    {
    LOGI(">>>>");
      for (int i = 0; TriggerCount(1) == CameraLayer::NAK; i++)
      {
        if (i > 5)
        {
          return CameraLayer::NAK;
        }
      }
    }
    else if(type==1 || type==2)
    {
      //1:wait for trigger from other thread
      //2:wait for trigger from hardware

    }

    LOGI(">>>>");

    std::thread(&CameraLayer::SnapAbort, this, timeout_ms).detach();
    // auto f = std::async(&CameraLayer::SnapAbort, this, timeout_ms);
    
    conV.wait(lock, [this]
              { return this->snapFlag != 1; });
    
    LOGI(">>>>snapFlag:%d",snapFlag);
    if(snapFlag==0)
    {
      retStatus=ACK;
    }
    else if(snapFlag==-1)
    {
      retStatus=NAK;
    }
    snapFlag=0;
    callback=_callback;
    context=_context;//recover the callback
    _snap_cb=NULL;
  }
  return retStatus;
}
