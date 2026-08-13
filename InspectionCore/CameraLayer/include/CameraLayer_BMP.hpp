#ifndef CAMERALAYER_BMP_HPP
#define CAMERALAYER_BMP_HPP
#include <CameraLayer.hpp>
#include <string>
#include "acvImage_BasicTool.hpp"   // acv_XY geometry
#include <opencv2/core.hpp>
#include <mutex>
#include <vector>
#include <thread>
class CameraLayer_BMP : public CameraLayer{
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
    std::string GetCurrentFileName(){return this->fileName;}
    status SetMirror(int Dir,int en);
    status SetROI(int x, int y, int w, int h,int zw,int zh);
    
    status TriggerMode(int mode){return NAK;}
    status Trigger(){return NAK;}
    status ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount) override;
    status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param){return NAK;}
    status GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh);
    status CalcROI(int* X,int* Y,int* W,int* H);
    status SetAnalogGain(float gain);
    status SetExposureTime(float time_us);
    status GetExposureTime(float *ret_time_us);
};



class CameraLayer_BMP_carousel : public CameraLayer_BMP{

    int modeTriggerSim_sleep=0;
    int frameInterval_ms=1000; // 1 fps default for fake-camera testing
    int ThreadTerminationFlag=0;
    int imageTakingCount=0;
    int triggerMode=1;
    std::thread *cameraThread;
    std::string folderName;
    std::string fileName;
    std::vector<std::string> files_in_folder;
    bool isThreadWorking;
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
    const std::vector<std::string>& GetFileList(){ updateFolder(folderName); return files_in_folder; }
    std::string GetFolderName() const { return folderName; }
    status TriggerMode(int mode);
    ~CameraLayer_BMP_carousel();
    static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist);

    status isInOperation();
};

#endif