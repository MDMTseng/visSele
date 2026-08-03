#ifndef CAMERALAYER_ARAVIS_HPP
#define CAMERALAYER_ARAVIS_HPP
#ifdef __WIN32__
#include <windows.h>
#endif
#include <CameraLayer.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <climits>
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
  // Every one of these had NO initializer and NO member-init list in the
  // constructor, yet the constructor arms the stream callback
  // (arv_stream_set_emit_signals) partway through -- so the stream thread
  // could read them while they still held whatever was on the heap.
  // takeCount was the dangerous one: the callback treats 0 as "budget spent"
  // and stops acquisition, so a stray 0 killed the camera on the first frame.
  // Defaults chosen to match what the constructor eventually establishes:
  // free-running (-1), nothing cached, not yet acquiring.
  ArvCamera *camera = NULL;
  ArvStream *stream = NULL;
  ArvChunkParser *chunk_parser = NULL;
  char** chunks=NULL;//the string set/array will end with NULL
  int payloadSize = 0;
  cam_info self_info;
  int takeCount = -1;
  bool acquisition_started = false;

  // Set once the stream callback has been armed, so teardown knows it has to
  // disconnect before it can safely free anything the callback touches.
  gulong _sig_new_buffer = 0;
  gulong _sig_control_lost = 0;

  // Bounds error_code_list, which the base class never clears and nothing ever
  // reads: on a flaky link it grew by one entry per dropped frame, forever.
  static constexpr size_t ERROR_LIST_MAX = 256;
  void pushErrorCode(uint64_t code);

  // The stream pushes this many buffers; StartAquisition re-pushes the same
  // number after a payload change. It was two independent literal 8s.
  static constexpr int STREAM_BUFFER_COUNT = 8;

  // Device-timestamp scaling.
  //
  // arv_buffer_get_timestamp() is NOT a fixed unit across cameras: Aravis
  // normalises GigE timestamps to nanoseconds, but a USB3Vision device's
  // timestamp comes through in whatever the device uses. Measured on a
  // MV-CA050-12UC: 99.957 MHz, i.e. 10 ns ticks -- so the old code, which
  // assigned the raw value straight to timeStamp_us, was out by 100x.
  //
  // Rather than hardcode a divisor (which would then be wrong for GigE), work
  // it out at runtime: every buffer also carries a host system timestamp in
  // nanoseconds, so the ratio of the two elapsed times IS the tick rate.
  // Calibrate over the first few frames, then snap to the nearest real-world
  // clock convention so the arithmetic stays exact instead of inheriting the
  // noise in the estimate.
  static constexpr int TS_CALIB_FRAMES = 8;
  uint64_t _ts_calib_dev0 = 0;      // first device timestamp seen
  uint64_t _ts_calib_sys0 = 0;      // matching host timestamp (ns)
  int      _ts_calib_n = 0;
  double   _ts_ticks_per_us = 1000.0;  // assume ns until proven otherwise
  bool     _ts_calibrated = false;
  // Returns microseconds, feeding the calibration as it goes.
  uint64_t deviceTimestampToUs(uint64_t dev_ticks, uint64_t sys_ns);

  // Log what the camera actually settled on (not what we asked for).
  void logCameraState(const char *when);

  static void s_STREAM_NEW_BUFFER_CB(ArvStream *stream, CameraLayer_Aravis *self);
  void STREAM_NEW_BUFFER_CB(ArvStream *stream);
  static void s_STREAM_CONTROL_LOST_CB(ArvStream *stream, CameraLayer_Aravis *self);
  void STREAM_CONTROL_LOST_CB(ArvStream *stream);

  // Valid ONLY for the duration of the EV_IMG callback: the stream thread
  // parks the popped buffer here, invokes the consumer synchronously, then
  // pushes the buffer back and clears this. ExtractFrame/GetFrameFormat must
  // therefore run inside that callback -- anything deferred reads a buffer the
  // driver has already taken back.
  ArvBuffer *_frame_cache_buffer = NULL;
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
  CameraLayer::status TriggerMode(int type) override;



  CameraLayer::status isInOperation() override;
  CameraLayer::status TriggerCount(int count) override;
  CameraLayer::status Trigger() override;
  CameraLayer::status RUN();
  cam_info getCameraInfo();
  CameraLayer::status SetResolution(int width, int height) override;
  CameraLayer::status SetAnalogGain(float gain) override;
  CameraLayer::status SetROI(int x, int y, int w, int h, int zw, int zh) override;

  CameraLayer::status ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount) override;
  CameraLayer::status GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh) override;
  CameraLayer::status SetOnceWB() override;
  
  CameraLayer::status SetRGain(float gain) override;
  CameraLayer::status SetGGain(float gain) override;
  CameraLayer::status SetBGain(float gain) override;
  CameraLayer::status SetBalckLevel(float blvl) override;
  CameraLayer::status SetMirror(int Dir, int en) override;
  CameraLayer::status SetROIMirror(int Dir, int en) override;
  CameraLayer::status SetFrameRate(float frame_rate) override;

  CameraLayer::status SetGamma(float gamma) override;
  CameraLayer::status GetAnalogGain(int *ret_min, int *ret_max) override;
  CameraLayer::status SetExposureTime(float time_us) override;
  CameraLayer::status GetExposureTime(float *ret_time_us) override;
  // Aravis-specific overrides so CameraSetup's StopAquisition()/StartAquisition()
  // actually take effect (the base class defaults are no-op NAKs).
  CameraLayer::status StartAquisition() override;
  CameraLayer::status StopAquisition() override;
  // (ContTriggerThread, ContTriggerThreadTermination, L_TriggerMode and
  // L_SetFrameRateMode were declared here with no definition anywhere in the
  // tree -- calling any of them was an instant link error. Removed rather than
  // left as traps. SetFrameRateMode above was the same and is gone too.)

  CameraLayer::FrameExtractPixelFormat GetFrameFormat() override;

  // "Have we already applied this value?" caches. They were uninitialized, and
  // the constructor's SetROI runs before anything sets them -- so if the
  // garbage happened to equal the requested ROI, the ROI was silently never
  // applied at all. INT_MIN can never be a legal value, so the first call
  // always goes through.
  int prev_MirrorX = INT_MIN;
  int prev_MirrorY = INT_MIN;
  int prev_ROI_X = INT_MIN;
  int prev_ROI_Y = INT_MIN;
  int prev_ROI_W = INT_MIN;
  int prev_ROI_H = INT_MIN;


  ~CameraLayer_Aravis();
};

#endif