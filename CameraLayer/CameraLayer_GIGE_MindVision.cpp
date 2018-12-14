
#include "CameraLayer_GIGE_MindVision.hpp"
#include "logctrl.h"


CameraLayer_GIGE_MindVision::CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{
}

CameraLayer_GIGE_MindVision::~CameraLayer_GIGE_MindVision()
{
    
	CameraPause(m_hCamera);
	if (m_pFrameBuffer)
	{
		CameraAlignFree(m_pFrameBuffer);
		m_pFrameBuffer = NULL;
	}

	CameraUnInit(m_hCamera);

}



CameraLayer::status CameraLayer_GIGE_MindVision::EnumerateDevice(tSdkCameraDevInfo * pCameraList,INT * piNums)
{
    if (CameraEnumerateDevice(pCameraList, piNums) != CAMERA_STATUS_SUCCESS || *piNums == 0)
	{
		LOGE("No camera was found!");
		return CameraLayer::NAK;
	}

    return CameraLayer::ACK;
}
void CameraLayer_GIGE_MindVision::sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext)
{
    CameraLayer_GIGE_MindVision *self = (CameraLayer_GIGE_MindVision*)pContext;
    self->GIGEMV_CB(hCamera,frameBuffer,frameInfo,pContext);//Hook back
}

void CameraLayer_GIGE_MindVision::GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext)
{
    LOGV("INCOMING IMGE....%d.................",m_hCamera);
    
    int width = frameInfo->iWidth;
    int height = frameInfo->iHeight;
    //img.ReSize(width,height);
    if(CameraImageProcess(hCamera, frameBuffer, m_pFrameBuffer, frameInfo)!=CAMERA_STATUS_SUCCESS)
    {
        
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
        callback(*this,CameraLayer::EV_IMG,context);
    }
    
}

CameraLayer::status CameraLayer_GIGE_MindVision::InitCamera(tSdkCameraDevInfo *devInfo)
{
    if(m_hCamera!=0)
    {
		LOGE("There is a camera hdl:&d..... INIT can only be done once", m_hCamera);
		return CameraLayer::NAK;
    }
	CameraSdkStatus status;
	tSdkCameraCapbility sCameraInfo;
    
	if ((status = CameraInit(devInfo, -1, -1, &m_hCamera)) != CAMERA_STATUS_SUCCESS)
	{
		LOGE("Failed to init the camera! Error code is %d(%s)", status, CameraGetErrorString(status));

		return CameraLayer::NAK;
	}

    CameraGetCapability(m_hCamera, &sCameraInfo);
    int width = sCameraInfo.sResolutionRange.iWidthMax;
    int height = sCameraInfo.sResolutionRange.iHeightMax;
    TriggerMode(1);
    TriggerCount(1);
    CameraSetCallbackFunction(m_hCamera,sGIGEMV_CB,(PVOID)this,NULL);
    
    int maxBufferSize = width*height * 3;
	m_pFrameBuffer = (BYTE *)CameraAlignMalloc(maxBufferSize, 16);
	LOGV("m_pFrameBuffer:%p m_hCamera:%d>>W:%d H:%d",m_pFrameBuffer,m_hCamera,width,height);
    img.useExtBuffer(m_pFrameBuffer,maxBufferSize,width,height);
	CameraPlay(m_hCamera);
    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::TriggerMode(int type)
{
	//0 for continuous, 1 for soft trigger, 2 for HW trigger
	if (CameraSetTriggerMode(m_hCamera,type)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_GIGE_MindVision::TriggerCount(int count)
{
	if (CameraSetTriggerCount(m_hCamera,count)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::Trigger()
{
    if (CameraSoftTrigger(m_hCamera)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
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
CameraLayer::status CameraLayer_GIGE_MindVision::SetAnalogGain(int gain)
{
    if (CameraSetAnalogGain(m_hCamera,gain)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetAnalogGain(int *ret_gain)
{
    if (CameraGetAnalogGain(m_hCamera,ret_gain)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetExposureTime(double time_ms)
{
    if (CameraSetExposureTime(m_hCamera,time_ms)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetExposureTime(double *ret_time_ms)
{
    if (CameraGetExposureTime(m_hCamera,ret_time_ms)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}

acvImage* CameraLayer_GIGE_MindVision::GetImg()
{
    return &img;
}