#ifndef CAMERALAYER_BMP_HPP
#define CAMERALAYER_BMP_HPP
#include <CameraLayer.hpp>
#include <string>
#include "acvImage_BasicTool.hpp"   // acv_XY geometry
#include <opencv2/core.hpp>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <thread>
class CameraLayer_BMP : public CameraLayer{
    // Guards img_load and fileName. Both are replaced by LoadBMP on whichever
    // thread asked for a new picture (the carousel's own thread, or the WS
    // thread through the bmp_carousel actions) and read by ExtractFrame /
    // CalcROI on the consumer's thread. Only the write side used to take it,
    // so a WS-driven imread could free the pixel buffer that ExtractFrame was
    // mid-loop over.
    std::mutex m;
    std::string fileName;
    std::vector <int> gaussianNoiseTable_M;

    protected:
    int cacheUseCounter=0;
    cv::Mat img_load;
    static constexpr float exp_time_100ExpUs_v=5000;
    const float exp_time_100ExpUs=exp_time_100ExpUs_v;
    float exp_time_us=1000;
    float a_gain=1;
    float ROI_X=0,ROI_Y=0,ROI_W=9999999,ROI_H=9999999;
    int MIRROR_X=0,MIRROR_Y=0;
    public:
    // --- fake-camera augmentation knobs (defaults preserve the original
    //     hardcoded ExtractFrame behavior).
    struct Augment {
      bool  brightness_jitter_en = true;
      float brightness_jitter_pct = 20.0f; // ±%
      bool  rotate_en = true;
      float rotate_step_deg = 0.5f;        // per ExtractFrame call
      bool  noise_en = true;
      int   noise_range = 15;              // ±pixel value
      bool  y_offset_en = true;
      float y_offset_r = 50.0f;            // px, wobble amplitude (tied to rotate)
      // Exposure simulation. OFF by default, which is a deliberate change of
      // behaviour: this used to be unconditional and driven by the SHARED
      // camera settings, so a real machine's exposure (50us against a 5000us
      // reference) rendered every loaded BMP at 1% brightness. A file already
      // has its exposure baked in; simulating a second one is a thing you opt
      // into, with the fake camera's own numbers, not the real camera's.
      bool  expo_sim_en = false;
      float expo_us = exp_time_100ExpUs_v;  // 100% by definition
      float expo_gain = 1.0f;
    };
    Augment aug;
    void SetAugment(const Augment &a) { aug = a; }
    Augment GetAugment() const { return aug; }

    
    static std::string getDriverName(){
      return "BMP";
    }
    CameraLayer_BMP(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context);
    
    status LoadBMP(std::string fileName);
    // Read from the WS thread (camera_info) while LoadBMP assigns it -- a
    // std::string copy out of a string being reassigned is not a benign race.
    std::string GetCurrentFileName(){std::lock_guard<std::mutex> lk(m); return this->fileName;}
    // Channel count of the image currently loaded, for frameInfo.channelCount.
    // Without this the fake camera never reports 1 and the core's
    // `(channelCount == 1) ? 1 : 3` always picks 3, so a grayscale file still
    // arrives as a 3-channel frame -- which is why a mono sensor's code path
    // could not be exercised on a bench at all. 0 when nothing is loaded.
    int GetLoadedChannels(){std::lock_guard<std::mutex> lk(m); return img_load.empty() ? 0 : img_load.channels();}
    status SetMirror(int Dir,int en);
    status SetROI(int x, int y, int w, int h,int zw,int zh);
    
    status TriggerMode(int mode){return NAK;}
    status Trigger(){return NAK;}
    status ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount) override;
    status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param){return NAK;}
    status GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh);
    status CalcROI(int* X,int* Y,int* W,int* H);
    // Same computation with `m` assumed already held. ExtractFrame has to hold
    // the lock across its whole pixel loop AND compute the ROI from the same
    // img_load, and std::mutex is not recursive, so the two entry points share
    // one body rather than one of them locking twice.
    status CalcROI_nolock(int* X,int* Y,int* W,int* H);
    status SetAnalogGain(float gain);
    status SetExposureTime(float time_us);
    status GetExposureTime(float *ret_time_us);
};



class CameraLayer_BMP_carousel : public CameraLayer_BMP{

    int modeTriggerSim_sleep=0;
    int frameInterval_ms=1000; // 1 fps default for fake-camera testing
    std::atomic<int> ThreadTerminationFlag{0};
    int imageTakingCount=0;
    int triggerMode=1;
    std::thread *cameraThread;
    std::string folderName;
    std::string fileName;
    std::vector<std::string> files_in_folder;
    // folderName + files_in_folder. updateFolder rebuilds the vector from the
    // acquisition thread (every LoadNext) while the WS thread walks it for
    // camera_info, which the WebUI polls -- an iteration straight through a
    // resize(0) and a reallocation.
    mutable std::mutex files_m;
    // The thread sleeps on this instead of sleep_for, so a stop request is
    // seen at once. frameInterval_ms is operator-settable (setfps), so a naked
    // sleep_for made the worst-case join wait 1000/fps ms -- seconds to
    // minutes, with camera_lifetime_lock held and inspection stalled behind it.
    std::mutex stop_m;
    std::condition_variable stop_cv;
    // Returns true if a stop was requested during (or before) the wait.
    bool WaitOrStop(int ms);
    // The one teardown path, shared by TriggerMode(stop), StopAquisition and
    // the destructor.
    void StopThread();
    // Read by TriggerMode to decide whether a join would return.
    std::atomic<bool> isThreadWorking{false};
    void ContTriggerThread();
    public:
    
    static std::string getDriverName(){
      return "BMP_carousel";
    }
    int fileIdx;
    // CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName);
    CameraLayer_BMP_carousel(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context);
    status updateFolder(std::string folderName);
    status SetFrameRate(float frame_rate);
    status TriggerCount(int count);
    status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param);
    status Trigger();
    status LoadNext(bool call_cb=true);
    status LoadPrev(bool call_cb=true);
    status LoadAt(int idx, bool call_cb=true);
    status ReloadCurrent(bool call_cb=true);
    int    GetCurrentIndex() const { return fileIdx; }
    float  GetFPS() const { return frameInterval_ms>0 ? 1000.0f/frameInterval_ms : 0.0f; }
    // By value, not by reference: the returned vector is rebuilt by every
    // LoadNext on the acquisition thread, so a reference handed to the WS
    // thread is a promise this class cannot keep no matter what it locks.
    std::vector<std::string> GetFileList();
    std::string GetFolderName() const { std::lock_guard<std::mutex> lk(files_m); return folderName; }
    status TriggerMode(int mode);
    // camera_ez_reconnect does StopAquisition() then delete. The base class
    // returns NAK and does nothing, so this layer's own thread kept running
    // over the freed object -- LoadNext into a destroyed files_in_folder, and
    // frames injected into the pipeline from a dead camera.
    status StopAquisition() override;
    status StartAquisition() override;
    ~CameraLayer_BMP_carousel();
    static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist);

    status isInOperation();
};

#endif