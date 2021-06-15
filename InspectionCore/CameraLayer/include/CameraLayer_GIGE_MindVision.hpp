#ifndef CAMERALAYER_GIGE_MINDVISION_HPP
#define CAMERALAYER_GIGE_MINDVISION_HPP
#ifdef __WIN32__
#include <windows.h>
#endif
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

#include "CameraApi.h"
class CameraLayer_GIGE_MindVision : public CameraLayer{

    protected:
    int mirrorFlag[2]={0,0};
    int ROI_mirrorFlag[2]={0,0};
    int snapFlag=0;
    std::mutex m;
    int takeCount=0;
    std::condition_variable conV;
    CameraHandle    m_hCamera=0;	//the handle of the camera we use
    int L_frameRateMode=2;
    int L_triggerMode=1;
    CameraLayer_Callback _snap_cb;

    BYTE*           m_pFrameBuffer=NULL;
    static void sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);
    void GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);
    static CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj);
    
    tSdkFrameHead *_cached_frame_info;
    public:
    
    CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context);
    static CameraLayer::status EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums);
    CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);

    CameraLayer::status TriggerMode(int type);

    CameraLayer::status TriggerCount(int count);
    CameraLayer::status Trigger();
    CameraLayer::status RUN();
    CameraLayer::status SetResolution(int width,int height);
    CameraLayer::status SetAnalogGain(float gain);
    CameraLayer::status SetROI(int x, int y, int w, int h,int zw,int zh);

    CameraLayer::status GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh);

    CameraLayer::status SetOnceWB();
    
    CameraLayer::status SetMirror(int Dir,int en);
    CameraLayer::status SetROIMirror(int Dir,int en);

    CameraLayer::status SetFrameRateMode(int mode);
    CameraLayer::status GetAnalogGain(int *ret_min,int *ret_max);
    CameraLayer::status SetExposureTime(float time_us);
    CameraLayer::status GetExposureTime(float *ret_time_us);
    
    CameraLayer::status L_TriggerMode(int type);
    CameraLayer::status L_SetFrameRateMode(int type);


    CameraLayer::status isInOperation();
    CameraLayer::status ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount);
    CameraLayer::status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param);
    ~CameraLayer_GIGE_MindVision();
};


#endif