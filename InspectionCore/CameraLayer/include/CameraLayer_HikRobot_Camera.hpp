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
    unsigned char *pData;
    size_t pDataL;
    uint32_t sampleCheckSum;

    MV_FRAME_OUT_INFO_EX frameInfo;
    void *context;
  };
protected:
  void *handle;

  cam_info self_info;
  int takeCount;
  bool bDevConnected;
  bool threadRunningState;
  std::thread imgQueueThread;

  TSQueue<hikFrameInfo> imgQueue;
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
  CameraLayer_HikRobot_Camera(MV_CC_DEVICE_INFO *devInfo, CameraLayer_Callback cb, void *context);

  // CameraLayer::status EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums);
  // CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);
  static int32_t listDevices(MV_CC_DEVICE_INFO_LIST *stDeviceList);

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
  // CameraLayer::status SetOnceWB();
  CameraLayer::status isInOperation();
  CameraLayer::status SetMirror(int Dir, int en);
  CameraLayer::status SetROIMirror(int Dir, int en);
  CameraLayer::status SetFrameRate(float frame_rate);
  CameraLayer::status SetFrameRateMode(int mode);
  CameraLayer::status SetExposureTime(float time_us);
  CameraLayer::status GetExposureTime(float *ret_time_us);
  
  CameraLayer::status SetBalckLevel(float blvl);

  CameraLayer::status SetGamma(float Gamma);

  CameraLayer::status StartAquisition();
  CameraLayer::status StopAquisition();
  
  // CameraLayer::status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param);
  // CameraLayer::status SnapFrame(int type,CameraLayer_Callback snap_cb,void *cb_param);
  // void ContTriggerThread();
  // void ContTriggerThreadTermination();
  // acvImage *GetFrame();

  ~CameraLayer_HikRobot_Camera();
};



#endif