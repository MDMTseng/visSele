#ifndef CAMERALAYER_ARAVIS_HPP
#define CAMERALAYER_ARAVIS_HPP
#ifdef __WIN32__
#include <windows.h>
#endif
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <queue>
#include <thread>
#include <arv.h>
class CameraLayer_Aravis : public CameraLayer
{

protected:
  ArvCamera *camera;
  ArvStream *stream;

  char *arv_option_camera_name = NULL;
  char *arv_option_debug_domains = NULL;
  gboolean arv_option_snaphot = FALSE;
  char *arv_option_trigger = NULL;
  double arv_option_software_trigger = -1;
  double arv_option_frequency = -1.0;
  int arv_option_width = -1;
  int arv_option_height = -1;
  int arv_option_horizontal_binning = -1;
  int arv_option_vertical_binning = -1;
  double arv_option_exposure_time_us = -1;
  int arv_option_gain = -1;
  gboolean arv_option_auto_socket_buffer = FALSE;
  char *arv_option_packet_size_adjustment = NULL;
  gboolean arv_option_no_packet_resend = FALSE;
  double arv_option_packet_request_ratio = -1.0;
  unsigned int arv_option_packet_timeout = 20;
  unsigned int arv_option_frame_retention = 100;
  int arv_option_gv_stream_channel = -1;
  int arv_option_gv_packet_delay = -1;
  int arv_option_gv_packet_size = -1;
  gboolean arv_option_realtime = FALSE;
  gboolean arv_option_high_priority = FALSE;
  gboolean arv_option_no_packet_socket = FALSE;
  char *arv_option_chunks = NULL;
  unsigned int arv_option_bandwidth_limit = -1;

  const GOptionEntry arv_option_entries[];


public:
  CameraLayer_Aravis(CameraLayer_Callback cb, void *context);
  // CameraLayer::status EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums);
  // CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);

  CameraLayer::status TriggerMode(int type);

  CameraLayer::status TriggerCount(int count);
  CameraLayer::status Trigger();
  CameraLayer::status RUN();
  CameraLayer::status SetResolution(int width, int height);
  CameraLayer::status SetAnalogGain(int gain);
  CameraLayer::status SetROI(float x, float y, float w, float h, int zw, int zh);

  CameraLayer::status GetROI(float *x, float *y, float *w, float *h, int *zw, int *zh);
  CameraLayer::status SnapFrame();

  CameraLayer::status SetOnceWB();

  CameraLayer::status SetMirror(int Dir, int en);
  CameraLayer::status SetROIMirror(int Dir, int en);

  CameraLayer::status SetFrameRateMode(int mode);
  CameraLayer::status GetAnalogGain(int *ret_min, int *ret_max);
  CameraLayer::status SetExposureTime(double time_us);
  CameraLayer::status GetExposureTime(double *ret_time_us);
  void ContTriggerThread();
  void ContTriggerThreadTermination();
  acvImage *GetFrame();

  CameraLayer::status L_TriggerMode(int type);
  CameraLayer::status L_SetFrameRateMode(int type);

  ~CameraLayer_Aravis();
};

#endif