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
    float ROI_X,ROI_Y,ROI_W,ROI_H;
    int MIRROR_X,MIRROR_Y;
    public:
    CameraLayer_BMP(CameraLayer_Callback cb,void* context);
    
    status LoadBMP(std::string fileName);
    std::string GetCurrentFileName(){return this->fileName;}
    status SetMirror(int Dir,int en);
    status SetROI(float x, float y, float w, float h,int zw,int zh);
};



class CameraLayer_BMP_carousel : public CameraLayer_BMP{
    int frameInterval_ms=100;
    int ThreadTerminationFlag=0;
    int imageTakingCount=0;
    int triggerMode=1;
    std::thread *cameraThread;
    std::string folderName;
    std::string fileName;
    std::vector<std::string> files_in_folder;

    void ContTriggerThread();
    public:
    
    int fileIdx;
    CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName);
    status updateFolder(std::string folderName);
    status SetFrameRateMode(int mode);
    status Trigger();
    status LoadNext();
    status TriggerMode(int mode);
    ~CameraLayer_BMP_carousel();
};


#endif