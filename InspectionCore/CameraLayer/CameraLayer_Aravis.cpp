
#include "CameraLayer_Aravis.hpp"
#include "logctrl.h"


CameraLayer_Aravis::CameraLayer_Aravis(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{
}

CameraLayer_Aravis::~CameraLayer_Aravis()
{
}



CameraLayer::status CameraLayer_Aravis::EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums)
{
    return CameraLayer::ACK;
}
void CameraLayer_Aravis::sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext)
{
}

void CameraLayer_Aravis::GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext)
{
}

CameraLayer::status CameraLayer_Aravis::InitCamera(tSdkCameraDevInfo *devInfo)
{

}
CameraLayer::status CameraLayer_Aravis::SetMirror(int Dir,int en)
{
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_Aravis::SetROIMirror(int Dir,int en)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::L_TriggerMode(int type)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::SetROI(float x, float y, float w, float h,int zw,int zh)
{

  return CameraLayer::NAK;
}
CameraLayer::status  CameraLayer_Aravis::GetROI(float *x, float *y, float *w, float *h,int*zw,int *zh)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::TriggerMode(int type)
{
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_Aravis::TriggerCount(int count)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::Trigger()
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::RUN()
{
  return CameraLayer::NAK;
}

void CameraLayer_Aravis::ContTriggerThread( )
{
}



void CameraLayer_Aravis::ContTriggerThreadTermination( )
{
}

CameraLayer::status CameraLayer_Aravis::SetResolution(int width,int height)
{
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::SetAnalogGain(int gain)
{
  return CameraLayer::ACK;
}



CameraLayer::status CameraLayer_Aravis::L_SetFrameRateMode(int mode)
{
  return CameraLayer::ACK;
}



CameraLayer::status CameraLayer_Aravis::SetOnceWB()
{
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_Aravis::SetFrameRateMode(int mode)
{
  return CameraLayer::ACK;
}




CameraLayer::status CameraLayer_Aravis::SnapFrame()
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::GetAnalogGain(int *ret_min,int *ret_max)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::SetExposureTime(double time_us)
{
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::GetExposureTime(double *ret_time_ms)
{
  return CameraLayer::ACK;
}

acvImage* CameraLayer_Aravis::GetFrame()
{
  return &img;
}