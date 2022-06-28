#ifndef CAMERALAYER_ARAVIS_HPP
#define CAMERALAYER_ARAVIS_HPP
#ifdef __WIN32__
#include <windows.h>
#endif
#include <CameraLayer.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <arv.h>

class CameraLayer_Aravis : public CameraLayer
{

public:

  struct cam_info{
    std::string id;
    std::string physical_id;
    std::string address;
    std::string vendor;
    std::string model;
    std::string serial_nbr;
    std::string protocol;
    bool available;
  } ;
protected:
  ArvCamera *camera;
  ArvStream *stream;
  ArvChunkParser *chunk_parser;
  char** chunks=NULL;//the string set/array will end with NULL
  int payloadSize;
  cam_info self_info;
  int takeCount;
  bool acquisition_started;

  static void s_STREAM_NEW_BUFFER_CB(ArvStream *stream, CameraLayer_Aravis *self);
  void STREAM_NEW_BUFFER_CB(ArvStream *stream);
  static void s_STREAM_CONTROL_LOST_CB(ArvStream *stream, CameraLayer_Aravis *self);
  void STREAM_CONTROL_LOST_CB(ArvStream *stream);

  ArvBuffer *_frame_cache_buffer;
public:

  static std::string getDriverName(){
    return "Aravis";
  }

  // CameraLayer_Aravis(const char* deviceID,CameraLayer_Callback cb, void *context);
  CameraLayer_Aravis(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context);
  // CameraLayer::status EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums);
  // CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);
  static void listDevices(std::vector<cam_info> &ret_infoList,bool tryOpen=false);
  static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist);
  CameraLayer::status TriggerMode(int type);



  CameraLayer::status isInOperation();
  CameraLayer::status TriggerCount(int count);
  CameraLayer::status Trigger();
  CameraLayer::status RUN();
  cam_info getCameraInfo();
  CameraLayer::status SetResolution(int width, int height);
  CameraLayer::status SetAnalogGain(float gain);
  CameraLayer::status SetROI(int x, int y, int w, int h, int zw, int zh);

  CameraLayer::status ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount);
  CameraLayer::status GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh);
  CameraLayer::status SetOnceWB();
  
  CameraLayer::status SetRGain(float gain);
  CameraLayer::status SetGGain(float gain);
  CameraLayer::status SetBGain(float gain);
  CameraLayer::status SetBalckLevel(float blvl);
  CameraLayer::status SetMirror(int Dir, int en);
  CameraLayer::status SetROIMirror(int Dir, int en);
  CameraLayer::status SetFrameRate(float frame_rate);
  CameraLayer::status SetFrameRateMode(int mode);
  
  CameraLayer::status SetGamma(float gamma);
  CameraLayer::status GetAnalogGain(int *ret_min, int *ret_max);
  CameraLayer::status SetExposureTime(float time_us);
  CameraLayer::status GetExposureTime(float *ret_time_us);
  void ContTriggerThread();
  void ContTriggerThreadTermination();
  CameraLayer::status L_TriggerMode(int type);
  CameraLayer::status L_SetFrameRateMode(int type);
  
  CameraLayer::FrameExtractPixelFormat GetFrameFormat();

  ~CameraLayer_Aravis();
};

#endif