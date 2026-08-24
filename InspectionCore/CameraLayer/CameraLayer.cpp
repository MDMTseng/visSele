#include "CameraLayer.hpp"


#include "logctrl.h"
#include <future>

LOG_MODULE("cam");

CameraLayer::status CameraLayer::SNAP_Callback(CameraLayer &cl_obj, int type, void* obj)
{  

  CameraLayer &Cam=*((CameraLayer*)(&cl_obj));
  CameraLayer::status ret_st=Cam._snap_cb(cl_obj,type,obj);
  Cam.snapFlag=0;

  Cam.conV.notify_one();
  return ret_st;
}

// No longer used by SnapFrame -- it now owns its deadline via
// conV.wait_for. Kept because it is virtual public API, but note that calling
// it aborts whatever snap is in flight, not any particular one.
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

  // A negative timeout used to mean "wait forever": SnapAbort returns
  // immediately for timeout_ms<0, so nothing ever notifies conV and the wait
  // below never ends. That is not a survivable default here -- SnapFrame is
  // called from the WebSocket command handler on the main loop thread, so one
  // snap that never gets a frame stops the core serving EVERY client, with no
  // log and no way back except a restart.
  //
  // It is reachable in normal use: the WebUI's take-new-image sends
  // timeout=-1, and on this machine the camera trigger rides the backlight
  // line driven by the peripheral board -- with the plate stopped, no trigger
  // ever comes. Clamp to a bound generous enough for a trigger that is
  // genuinely on its way, so the failure is a NAK the UI can report instead of
  // a frozen core.
  if (timeout_ms < 0) timeout_ms = 30000;

  CameraLayer::status retStatus=NAK;
  snapFlag = 1;
  //trigger reset;
  {
    std::unique_lock<std::mutex> lock(m);

    CameraLayer_Callback _callback;
    void* _context;
    {
      // Swap the pair atomically w.r.t. the frame threads (they read it under
      // the same lock in invokeFrameCallback). Without this, a frame landing
      // between the two writes ran SNAP_Callback with the old context.
      std::lock_guard<std::mutex> _cb_guard(cb_swap_m);
      _callback=callback;
      _context=context;//replace the callback
      _snap_cb=snap_cb;
      callback=SNAP_Callback;
      context=cb_param;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // A trigger needs a running stream. This is not a belt-and-braces call:
    // one successful snap STOPS acquisition by design -- the burst counter
    // (takeCount) reaches 0 in the frame callback and calls
    // arv_camera_stop_acquisition -- so the camera is left unable to answer the
    // next one. Result, measured on this machine: the first 立即 after anything
    // that started the stream works, and every 立即 after that waits out the
    // full timeout and reports 圖像獲取失敗, until a CI session / reconnect /
    // ROI change happens to restart acquisition and re-arm exactly one shot.
    //
    // Idempotent where it is implemented (Aravis returns ACK immediately when
    // already streaming) and a no-op where it is not, so this costs nothing on
    // the paths that were already correct -- notably FI, which is streaming.
    StartAcquisition();

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

    // The timeout belongs to THIS snap.
    //
    // It used to be a detached thread that slept for timeout_ms and then wrote
    // snapFlag=-1 if it found snapFlag==1. That flag is shared by every snap,
    // and the thread had no way to tell whose 1 it was looking at -- so a snap
    // that finished early left a live timer that aborted whichever snap
    // happened to be in flight when it expired. Measured: with a 3s timeout and
    // presses ~400ms apart, snap 1 succeeded in 276ms and snap 2 died at
    // 2528ms -- exactly 3s after snap 1 started. The UI sends timeout -1, which
    // clamps to 30s, so each press armed a 30-second landmine for the next one.
    //
    // wait_for owns its deadline and cannot outlive the wait, so there is
    // nothing left behind to fire late.
    if (!conV.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                       [this] { return this->snapFlag != 1; }))
    {
      snapFlag = -1;   // nothing arrived in time
    }


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
    {
      // The restore waits out any invocation still in flight: on a timeout the
      // frame thread may be inside SNAP_Callback writing the caller's Mat, and
      // returning before it finishes hands it a destroyed stack object.
      std::lock_guard<std::mutex> _cb_guard(cb_swap_m);
      callback=_callback;
      context=_context;//recover the callback
      _snap_cb=NULL;
    }
  }
  return retStatus;
}
