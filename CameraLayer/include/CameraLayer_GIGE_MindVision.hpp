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

#include "CameraApi.h"
class CameraLayer_GIGE_MindVision : public CameraLayer{

    protected:
    std::mutex m;
    CameraHandle    m_hCamera=0;	//the handle of the camera we use
    BYTE*           m_pFrameBuffer=NULL;
    static void sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);
    void GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);

    public:
    CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context);
    CameraLayer::status EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums);
    CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);

    CameraLayer::status TriggerMode(int type);

    CameraLayer::status TriggerCount(int count);
    CameraLayer::status Trigger();
    CameraLayer::status RUN();
    CameraLayer::status SetCrop(int x,int y, int width,int height);
    CameraLayer::status SetResolution(int width,int height);
    CameraLayer::status SetAnalogGain(int gain);
    CameraLayer::status GetAnalogGain(int *ret_gain);
    CameraLayer::status SetExposureTime(double time_ms);
    CameraLayer::status GetExposureTime(double *ret_time_ms);
    acvImage* GetImg();

    ~CameraLayer_GIGE_MindVision();
};


#endif