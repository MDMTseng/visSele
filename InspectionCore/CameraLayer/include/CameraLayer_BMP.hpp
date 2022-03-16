#ifndef CAMERALAYER_BMP_HPP
#define CAMERALAYER_BMP_HPP
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <vector>
#include <thread>
class CameraLayer_BMP : public CameraLayer{
    std::mutex m;
    std::string fileName;
    std::vector <int> gaussianNoiseTable_M;

    protected:
    int cacheUseCounter=0;
    acvImage img_load;
    const float exp_time_100ExpUs=5000;
    float exp_time_us=1000;
    float a_gain=1;
    float ROI_X=0,ROI_Y=0,ROI_W=9999999,ROI_H=9999999;
    int MIRROR_X=0,MIRROR_Y=0;
    public:
    CameraLayer_BMP(CameraLayer_Callback cb,void* context);
    
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
    int frameInterval_ms=100;
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
    
    int fileIdx;
    CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName);
    status updateFolder(std::string folderName);
    status SetFrameRate(float frame_rate);
    status TriggerCount(int count);
    status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param);
    status Trigger();
    status LoadNext(bool call_cb=true);
    status TriggerMode(int mode);
    ~CameraLayer_BMP_carousel();

    status isInOperation();
};


#endif