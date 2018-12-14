#ifndef CAMERALAYER_GIGE_MINDVISION_HPP
#define CAMERALAYER_GIGE_MINDVISION_HPP

#include <windows.h>
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <queue>

#include "CameraApi.h"
class CameraLayer_GIGE_MindVision : public CameraLayer{

    std::mutex m;
    CameraHandle    m_hCamera;	//������豸���|the handle of the camera we use
    BYTE*           m_pFrameBuffer=NULL;
    BYTE*           extFrameBuffer=NULL;
    public:
    CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context);
    static void GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext);
    CameraLayer::status EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums);
    CameraLayer::status InitCamera();
    CameraLayer::status TriggerMode(int type);

    CameraLayer::status TriggerCount(int TYPE);
    CameraLayer::status RUN();
    CameraLayer::status SetCrop(int x,int y, int width,int height);
    CameraLayer::status SetResolution(int width,int height);
    CameraLayer::status SetAnalogGain(int min,int max);
    CameraLayer::status GetAnalogGain(int *ret_min,int *ret_max);
    CameraLayer::status SetExposureTime(double time_ms);
    CameraLayer::status GetExposureTime(double *ret_time_ms);
    acvImage* GetImg();
};


#endif