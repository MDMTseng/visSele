#ifndef CAMERALAYER_HIKROBOT_CAMERA_HPP
#define CAMERALAYER_HIKROBOT_CAMERA_HPP
#ifdef __WIN32__
#include <windows.h>
#endif
#include <CameraLayer.hpp>


#include "MvCameraControl.h"

#include "TSQueue.hpp"

#include "logctrl.h"
#include <cstring>
#include <vector>
#include <stdexcept>
// #include "common_lib.h"

using namespace std;


typedef uint32_t HIKERR;

class CameraLayer_HikRobot_Camera : public CameraLayer
{

public:
  struct cam_info
  {
    std::string id;
    std::string physical_id;
    std::string address;
    std::string vendor;
    std::string model;
    std::string serial_nbr;
    std::string protocol;
    bool available;
  };

  struct hikFrameInfo
  {
    unsigned char *pData;   // points into a _frameBufPool slot (stable until the ring wraps)
    size_t pDataL;

    MV_FRAME_OUT_INFO_EX frameInfo;
    void *context;
  };
  
  static std::string getDriverName(){
    return "HikRobot";
  }
protected:
  void *handle;

  cam_info self_info;
  int takeCount;
  bool bDevConnected;
  bool threadRunningState;
  std::thread imgQueueThread;

  TSQueue<hikFrameInfo> imgQueue;

  // Frame-copy ring. The MVS push callback hands us a pointer into an
  // SDK-owned buffer that is recycled the moment the callback returns; a
  // delayed consumer would otherwise read the NEXT frame's pixels. So we
  // memcpy each frame into one of these reusable slots inside the callback
  // and queue a pointer into the slot. The ring is larger than imgQueue's
  // capacity, so a slot is always fully consumed before it is reused.
  // Exposure-floor watch state. Mirrors CameraLayer_Aravis; see
  // frameInfo::rateSaturated. Off unless SetExposureFloorWatch(true).
  double   _floor_us = 0.0;
  uint64_t _prev_frame_us = 0;
  int      _saturated_run = 0;
  bool     _saturation_reported = false;
  static constexpr double SATURATION_TOL = 0.02;
  static constexpr int SATURATION_RUN_ALERT = 3;
  void refreshExposureFloor();

  static constexpr int FRAME_POOL_SIZE = 16;
  static constexpr int IMG_QUEUE_DEPTH = 10;
  // The pool's whole correctness argument is that a slot cannot be recycled
  // while a frame still sitting in the queue points at it. That holds only
  // while the ring is strictly larger than the queue can ever be -- which was
  // previously true by coincidence of two literals in two different files.
  static_assert(FRAME_POOL_SIZE > IMG_QUEUE_DEPTH,
                "frame pool must outnumber the queue, or a queued frame's slot "
                "can be overwritten before it is consumed");
  std::vector<std::vector<uint8_t>> _frameBufPool;
  int _frameBufIdx=0;
  unsigned int _lastFrameNum=0;
  bool _lastFrameNumValid=false;

  // Line0 edge accounting.
  //
  // nFrameNum only counts frames the sensor actually exposed, so it cannot see
  // a trigger the sensor refused (still integrating, or a readout that cannot
  // keep up) -- no frame was exposed, so there is no gap to detect. The camera's
  // own Line0RisingEdge event fires per electrical edge regardless, which makes
  // it an independent count to reconcile against.
  //
  // Rising vs falling is a signal-integrity check: they should be equal. A
  // divergence means the input is seeing bounce, or a level that only sometimes
  // clears the opto threshold -- the failure mode expected when a 3.3V GPIO
  // drives an input specified for 5-24V.
  std::atomic<unsigned long long> _line0RisingEdges{0};
  std::atomic<unsigned long long> _line0FallingEdges{0};
  std::atomic<unsigned long long> _framesDelivered{0};
  std::atomic<unsigned long long> _framesDroppedByGap{0};
  std::atomic<unsigned long long> _line0LastEdgeDevTick{0};

  static void sEventCallBack(MV_EVENT_OUT_INFO *pEventInfo, void *context);
  void EventCallBack(MV_EVENT_OUT_INFO *pEventInfo);
  void registerLineEvents();
  void logTriggerConfig(const char *when);

  bool acquisition_started=false;
  int mirrorFlag[2]={0,0};
  int ROI_mirrorFlag[2]={0,0};
  bool inNoError;
  
  uint8_t* _cached_pData=NULL;
  MV_FRAME_OUT_INFO_EX* _cached_frame_info=NULL;

  static void sExceptionCallBack(unsigned int nMsgType, void *context);
  void ExceptionCallBack(unsigned int nMsgType);

  static void sImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *context);
  void ImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo);

  void CLOSE();

  int GetIntValue(const char *strKey, MVCC_INTVALUE_EX *pIntValue)
  {
    return MV_CC_GetIntValueEx(handle, strKey, pIntValue);
  }

  int SetIntValue(const char *strKey, int64_t nValue)
  {
    return MV_CC_SetIntValueEx(handle, strKey, nValue);
  }

  int SetIntValue_w_Check(const char *strKey, int64_t target)
  {
    MVCC_INTVALUE_EX intValInfo = {0};
    int ret = GetIntValue(strKey, &intValInfo);
    
    // LOGI("GetIntV ret :%x  %d %d %d %d",ret,intValInfo.nCurValue,intValInfo.nMin,intValInfo.nMax,intValInfo.nInc,intValInfo.nReserved);
    int value = leastSatiValue(target, intValInfo);
    return SetIntValue(strKey, value);
  }

  int GetEnumValue(const char *strKey, MVCC_ENUMVALUE *pEnumValue)
  {
    return MV_CC_GetEnumValue(handle, strKey, pEnumValue);
  }

  int SetEnumValue(const char *strKey, unsigned int nValue)
  {
    return MV_CC_SetEnumValue(handle, strKey, nValue);
  }

  int SetEnumValueByString(const char *strKey, const char *sValue)
  {
    return MV_CC_SetEnumValueByString(handle, strKey, sValue);
  }

  int GetFloatValue(const char *strKey, MVCC_FLOATVALUE *pFloatValue)
  {
    return MV_CC_GetFloatValue(handle, strKey, pFloatValue);
  }

  int SetFloatValue(const char *strKey, float fValue)
  {
    return MV_CC_SetFloatValue(handle, strKey, fValue);
  }

  int GetBoolValue(const char *strKey, bool *pbValue)
  {
    return MV_CC_GetBoolValue(handle, strKey, pbValue);
  }

  int SetBoolValue(const char *strKey, bool bValue)
  {
    return MV_CC_SetBoolValue(handle, strKey, bValue);
  }

  int GetStringValue(const char *strKey, MVCC_STRINGVALUE *pStringValue)
  {
    return MV_CC_GetStringValue(handle, strKey, pStringValue);
  }

  int SetStringValue(const char *strKey, const char *strValue)
  {
    return MV_CC_SetStringValue(handle, strKey, strValue);
  }

  int CommandExecute(const char *strKey)
  {
    return MV_CC_SetCommandValue(handle, strKey);
  }

  static int leastSatiValue(int target, MVCC_INTVALUE_EX range)
  {
    if(range.nMin!=range.nMax && range.nMax!=0)
    {
      if (target < range.nMin)
        return range.nMin;
      if (target > range.nMax)
        return range.nMax;
    }
    int value = range.nMin + ((target - range.nMin) / range.nInc) * range.nInc;
    
    // LOGI("m:%d M:%d I:%d v:%d tar:%d",range.nMin,range.nMax,range.nInc,value,target);
    return (target > value) ? value + range.nInc : value;
  }


  void imgQThreadFunc();
  

public:

  CameraLayer_HikRobot_Camera(
    CameraLayer::BasicCameraInfo camInfo,
    std::string misc,
    CameraLayer_Callback cb, 
    void *context);
  // CameraLayer::status EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums);
  // CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);
  // static int32_t listDevices(MV_CC_DEVICE_INFO_LIST *stDeviceList);
  static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist);

  CameraLayer::status TriggerMode(int type);
  CameraLayer::status Trigger();

  int getXML(char *buffer, int bufferSize);

  CameraLayer::status ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount);
  CameraLayer::status SetROI(int x, int y, int w, int h, int zw, int zh);

  CameraLayer::status GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh);

  // CameraLayer::status SetResolution(int width, int height);

  CameraLayer::status TriggerCount(int count);
  // CameraLayer::status RUN();
  CameraLayer::status SetAnalogGain(float gain);
  CameraLayer::status SetRGain(float gain);
  CameraLayer::status SetGGain(float gain);
  CameraLayer::status SetBGain(float gain);
  // CameraLayer::status SetOnceWB();
  CameraLayer::status isInOperation();
  CameraLayer::status SetMirror(int Dir, int en);
  CameraLayer::status SetROIMirror(int Dir, int en);
  CameraLayer::status SetFrameRate(float frame_rate);
  CameraLayer::status SetExposureTime(float time_us);
  CameraLayer::status GetExposureTime(float *ret_time_us);
  
  CameraLayer::status SetBalckLevel(float blvl);

  CameraLayer::status SetGamma(float Gamma);

  CameraLayer::status StartAcquisition();
  CameraLayer::status StopAcquisition();

  // Trigger/frame accounting, for reconciling against whatever external source
  // is driving the trigger line. rising==falling means the pulses arrived
  // cleanly; rising>delivered+droppedByGap means the sensor refused triggers.
  struct TriggerStats {
    unsigned long long line0RisingEdges;
    unsigned long long line0FallingEdges;
    unsigned long long framesDelivered;
    unsigned long long framesDroppedByGap;
    unsigned long long line0LastEdgeDevTick;
  };
  TriggerStats GetTriggerStats() const {
    TriggerStats s;
    s.line0RisingEdges     = _line0RisingEdges.load();
    s.line0FallingEdges    = _line0FallingEdges.load();
    s.framesDelivered      = _framesDelivered.load();
    s.framesDroppedByGap   = _framesDroppedByGap.load();
    s.line0LastEdgeDevTick = _line0LastEdgeDevTick.load();
    return s;
  }
  
  // CameraLayer::status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param);
  // CameraLayer::status SnapFrame(int type,CameraLayer_Callback snap_cb,void *cb_param);
  // void ContTriggerThread();
  // void ContTriggerThreadTermination();
  // acvImage *GetFrame();

  ~CameraLayer_HikRobot_Camera();
};

#endif