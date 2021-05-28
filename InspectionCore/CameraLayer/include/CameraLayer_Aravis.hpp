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

#include "CameraApi.h"
class CameraLayer_Aravis : public CameraLayer{

    protected:
    static void sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);
    void GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);

    public:
    
    CameraLayer_Aravis(CameraLayer_Callback cb,void* context);
    CameraLayer::status EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums);
    CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);

    CameraLayer::status TriggerMode(int type);

    CameraLayer::status TriggerCount(int count);
    CameraLayer::status Trigger();
    CameraLayer::status RUN();
    CameraLayer::status SetResolution(int width,int height);
    CameraLayer::status SetAnalogGain(int gain);
    CameraLayer::status SetROI(float x, float y, float w, float h,int zw,int zh);

    CameraLayer::status GetROI(float *x, float *y, float *w, float *h,int*zw,int *zh);
    CameraLayer::status SnapFrame();

    CameraLayer::status SetOnceWB();
    
    CameraLayer::status SetMirror(int Dir,int en);
    CameraLayer::status SetROIMirror(int Dir,int en);

    CameraLayer::status SetFrameRateMode(int mode);
    CameraLayer::status GetAnalogGain(int *ret_min,int *ret_max);
    CameraLayer::status SetExposureTime(double time_us);
    CameraLayer::status GetExposureTime(double *ret_time_us);
    void ContTriggerThread();
    void ContTriggerThreadTermination();
    acvImage* GetFrame();


    
    CameraLayer::status L_TriggerMode(int type);
    CameraLayer::status L_SetFrameRateMode(int type);

    ~CameraLayer_Aravis();
};


#endif