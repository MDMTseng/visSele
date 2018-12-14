
#include "CameraLayer_GIGE_MindVision.hpp"


CameraLayer_GIGE_MindVision::CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{
}



CameraLayer::status CameraLayer_GIGE_MindVision::EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums)
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::InitCamera()
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::TriggerMode(int type)
{
    return CameraLayer::NAK;
}


CameraLayer::status CameraLayer_GIGE_MindVision::TriggerCount(int TYPE)
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::RUN()
{
    return CameraLayer::NAK;
}


CameraLayer::status CameraLayer_GIGE_MindVision::SetCrop(int x,int y, int width,int height)
{
    return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetResolution(int width,int height)
{
    return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetAnalogGain(int min,int max)
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetAnalogGain(int *ret_min,int *ret_max)
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetExposureTime(double time_ms)
{
    return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetExposureTime(double *ret_time_ms)
{
    return CameraLayer::NAK;
}

acvImage* CameraLayer_GIGE_MindVision::GetImg()
{
    return &img;
}