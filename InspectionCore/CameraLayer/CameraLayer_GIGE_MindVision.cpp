
#include "CameraLayer_GIGE_MindVision.hpp"
#include "logctrl.h"


CameraLayer_GIGE_MindVision::CameraLayer_GIGE_MindVision(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{
  m.unlock();
}

CameraLayer_GIGE_MindVision::~CameraLayer_GIGE_MindVision()
{
    LOGI("CameraPause");
	CameraPause(m_hCamera);
    LOGI("CameraStop");
    CameraStop(m_hCamera);
	if (m_pFrameBuffer)
	{
        LOGI("CameraAlignFree");
		CameraAlignFree(m_pFrameBuffer);
		m_pFrameBuffer = NULL;
	}

    LOGI("CameraUnInit");
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
  
  m.lock();
    LOGV("INCOMING IMGE....%d.................",m_hCamera);
    
    int width = frameInfo->iWidth;
    int height = frameInfo->iHeight;
    //img.ReSize(width,height);
    if(CameraImageProcess(hCamera, frameBuffer, m_pFrameBuffer, frameInfo)!=CAMERA_STATUS_SUCCESS)
    {
        CameraLayer_GIGE_MindVision::frameInfo fi_={
          timeStamp_100us:(uint64_t)frameInfo->uiTimeStamp,
          width:(uint32_t)frameInfo->iWidth,
          height:(uint32_t)frameInfo->iHeight,
          offset_x:ROI_x,
          offset_y:ROI_y,
        };
        fi = fi_;
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
        callback(*this,CameraLayer::EV_IMG,context);
    }
    
  m.unlock();
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
    TriggerMode(1);
    TriggerCount(1);
    CameraSetCallbackFunction(m_hCamera,sGIGEMV_CB,(PVOID)this,NULL);
    CameraSetAeState(m_hCamera,FALSE);
    //CameraSetAutoConnect(m_hCamera,true);
    //

    int width = sCameraInfo.sResolutionRange.iWidthMax;
    int height = sCameraInfo.sResolutionRange.iHeightMax;
    maxWidth = width;
    maxHeight= height;
    int maxBufferSize = width*height * 3;
    m_pFrameBuffer = (BYTE *)CameraAlignMalloc(maxBufferSize, 16);
    LOGV("m_pFrameBuffer:%p m_hCamera:%d>>W:%d H:%d",m_pFrameBuffer,m_hCamera,width,height);
    img.useExtBuffer(m_pFrameBuffer,maxBufferSize,width,height);
	  CameraPlay(m_hCamera);
    return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetMirror(int Dir,int en)
{
  m.lock();
  CameraSetMirror(m_hCamera,Dir,en);
  m.unlock();
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::L_TriggerMode(int type)
{
  m.lock();
	//0 for continuous, 1 for soft trigger, 2 for HW trigger
	if (CameraSetTriggerMode(m_hCamera,type)!= CAMERA_STATUS_SUCCESS)
  {
  LOGE("Failed...");
    m.unlock();
      return CameraLayer::NAK;
  }
  L_triggerMode=type;
  m.unlock();
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetROI(float x, float y, float w, float h,int zw,int zh)
{
  
  if(x<0)x=0;
  if(y<0)y=0;
  if(w<0)w=0;
  if(h<0)h=0;

  if(x<=1)
  {
    ROI_x=x*maxWidth;
  }
  else
  {
    ROI_x=x>maxWidth?maxWidth:x;
  }
  
  if(y<=1)
  {
    ROI_y=y*maxHeight;
  }
  else
  {
    ROI_y=y>maxHeight?maxHeight:y;
  }

  if(w<=1)
  {
    ROI_w=w*maxWidth;
  }
  else
  {
    ROI_w=w>maxWidth?maxWidth:w;
  }
  
  if(h<=1)
  {
    ROI_h=h*maxHeight;
  }
  else
  {
    ROI_h=h>maxHeight?maxHeight:h;
  }


  if(ROI_x>=maxWidth-5)ROI_x=maxWidth-5;
  if(ROI_w>maxWidth-ROI_x)ROI_w=maxWidth-ROI_x;

  
  if(ROI_y>=maxHeight-5)ROI_y=maxHeight-5;
  if(ROI_h>maxHeight-ROI_y)ROI_h=maxHeight-ROI_y;

  ROI_x=(int)ROI_x;
  ROI_y=(int)ROI_y;
  ROI_w=(int)ROI_w;
  ROI_h=(int)ROI_h;
  LOGI("MAX:%d %d",maxWidth,maxHeight);

  LOGI("ROI:%f %f %f %f",ROI_x,ROI_y,ROI_w,ROI_h);
  tSdkImageResolution resInfo={
    iIndex:0xFF,
    uBinSumMode:0,
    uBinAverageMode:0,
    uSkipMode:0,
    uResampleMask:0,
    iHOffsetFOV:(int)ROI_x,
    iVOffsetFOV:(int)ROI_y,
    iWidthFOV:(int)ROI_w,
    iHeightFOV:(int)ROI_h,
    iWidth:(int)ROI_w,
    iHeight:(int)ROI_h,
    iWidthZoomHd:0,
    iHeightZoomHd:0,
    iWidthZoomSw:0,
    iHeightZoomSw:0
  };


  CameraSdkStatus camst = CameraSetImageResolution (m_hCamera,&resInfo);
  if(camst)return CameraLayer::NAK;
  camst = CameraGetImageResolution (m_hCamera,&resInfo);
  if(camst)return CameraLayer::NAK;
  //int maxBufferSize = (int)ROI_w*(int)ROI_h*3;
  ROI_x=resInfo.iHOffsetFOV;
  ROI_y=resInfo.iVOffsetFOV;

  ROI_w=resInfo.iWidth;
  ROI_h=resInfo.iHeight;

  img.useExtBuffer(m_pFrameBuffer,maxWidth*maxHeight*3,(int)ROI_w,(int)ROI_h);
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_GIGE_MindVision::TriggerMode(int type)
{
    eff_triggerMode=type;
    
    if(eff_triggerMode==0 && L_frameRateMode==0)
    {
        if(L_triggerMode!=1)
            L_TriggerMode(1);
        if(cameraTriggerThread==NULL)
            cameraTriggerThread = new std::thread(&CameraLayer_GIGE_MindVision::ContTriggerThread, this);
        return CameraLayer::ACK;
    }
    else
    {
        ContTriggerThreadTermination();
    }
	return L_TriggerMode(type);
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

void CameraLayer_GIGE_MindVision::ContTriggerThread( )
{
    LOGI("ContTriggerThread: fr:%d eff_tm:%d",L_frameRateMode,eff_triggerMode);
    while( L_frameRateMode == 0 && eff_triggerMode==0)
    {
        Trigger();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}



void CameraLayer_GIGE_MindVision::ContTriggerThreadTermination( )
{
    if(cameraTriggerThread)
    {
        cameraTriggerThread->join();
        delete cameraTriggerThread;
        cameraTriggerThread = NULL;
    }
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetResolution(int width,int height)
{
    return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetAnalogGain(int gain)
{
    //CameraSetGain(m_hCamera,gain,gain,gain);
    if (CameraSetAnalogGain(m_hCamera,gain)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}



CameraLayer::status CameraLayer_GIGE_MindVision::L_SetFrameRateMode(int mode)
{
    if (CameraSetFrameSpeed(m_hCamera,mode)!= CAMERA_STATUS_SUCCESS)
    {
		LOGE("Failed...");
        return CameraLayer::NAK;
    }
    L_frameRateMode=mode;
    return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_GIGE_MindVision::SetFrameRateMode(int mode)
{
    L_SetFrameRateMode(mode);
    L_frameRateMode = mode;


    if(eff_triggerMode==0 && L_frameRateMode==0)
    {
        if(L_triggerMode!=1)
            L_TriggerMode(1);
        if(cameraTriggerThread==NULL)
            cameraTriggerThread = new std::thread(&CameraLayer_GIGE_MindVision::ContTriggerThread, this);
        return CameraLayer::ACK;
    }
    else
    {
        ContTriggerThreadTermination();
        L_TriggerMode(eff_triggerMode);
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

CameraLayer::status CameraLayer_GIGE_MindVision::SetExposureTime(double time_us)
{
    if (CameraSetExposureTime(m_hCamera,time_us)!= CAMERA_STATUS_SUCCESS)
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

acvImage* CameraLayer_GIGE_MindVision::GetFrame()
{
    return &img;
}