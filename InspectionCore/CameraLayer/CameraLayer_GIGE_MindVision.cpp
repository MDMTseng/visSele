
#include "CameraLayer_GIGE_MindVision.hpp"
#include "logctrl.h"

CameraLayer_GIGE_MindVision::CameraLayer_GIGE_MindVision(CameraLayer_Callback cb, void *context) : CameraLayer(cb, context)
{
  m.unlock();
  _cached_frame_info=NULL;
  
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

CameraLayer::status CameraLayer_GIGE_MindVision::EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums)
{
  if (CameraEnumerateDevice(pCameraList, piNums) != CAMERA_STATUS_SUCCESS || *piNums == 0)
  {
    LOGE("No camera was found!");
    return CameraLayer::NAK;
  }

  return CameraLayer::ACK;
}
void CameraLayer_GIGE_MindVision::sGIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead *frameInfo, PVOID pContext)
{
  CameraLayer_GIGE_MindVision *self = (CameraLayer_GIGE_MindVision *)pContext;
  self->GIGEMV_CB(hCamera, frameBuffer, frameInfo, pContext); //Hook back
}

void CameraLayer_GIGE_MindVision::GIGEMV_CB(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead *frameInfo, PVOID pContext)
{

  // LOGI("snapFlag:%d", snapFlag);
  m.lock();
  _cached_frame_info=frameInfo;
  int width = frameInfo->iWidth;
  int height = frameInfo->iHeight;

  // if (img.GetWidth() != width || img.GetHeight() != height)
  //   img.useExtBuffer(m_pFrameBuffer, maxWidth * maxHeight * 3, width, height);
  //img.ReSize(width,height);
  if (CameraImageProcess(hCamera, frameBuffer, m_pFrameBuffer, frameInfo) != CAMERA_STATUS_SUCCESS)
  {
    CameraLayer_GIGE_MindVision::frameInfo fi_ = {
      timeStamp_us : 0,
      width : 0,
      height : 0,
      offset_x : 0,
      offset_y : 0,
    };
    fi = fi_;
    callback(*this, CameraLayer::EV_ERROR, context);
  }
  else
  {

    LOGI("INCOMING IMGE....%d.................", m_hCamera);
    // for(int i=0;i<maxWidth*maxHeight*3;i+=3)
    // {
    //   int tmp=m_pFrameBuffer[i];
    //   m_pFrameBuffer[i]=m_pFrameBuffer[i+2];
    //   m_pFrameBuffer[i+2]=tmp;

    // }
    CameraLayer_GIGE_MindVision::frameInfo fi_ = {
      timeStamp_us : (uint64_t)frameInfo->uiTimeStamp*100,
      width : (uint32_t)frameInfo->iWidth,
      height : (uint32_t)frameInfo->iHeight,
      offset_x : ROI_x,
      offset_y : ROI_y,
    };
    fi = fi_;

    callback(*this, CameraLayer::EV_IMG, context);
  }
  _cached_frame_info=NULL;

  m.unlock();
}


CameraLayer::status CameraLayer_GIGE_MindVision::ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount)
{

  if (_cached_frame_info == NULL)
  {
    return NAK;
  }
  //if(pType == PixelType_Gvsp_Mono8)
  {//for now it's only BGR
    
    int w=_cached_frame_info->iWidth;
    int h=_cached_frame_info->iHeight;

    int src_CH_COUNT=3;
    if(channelCount==src_CH_COUNT)
    {
      memcpy(imgBuffer,m_pFrameBuffer,w*h*src_CH_COUNT);
    }
    else
    {
      
      for(int i=0;i<h;i++)
      {
        uint8_t* src_Pix_Gray=m_pFrameBuffer+i*w*src_CH_COUNT;
        uint8_t* tar_Pix=imgBuffer+(i*w)*channelCount;
        for(int j=0;j<w;j++)
        {
          for(int k=0;k<channelCount;k++)
          {
            int src_pix_ch=k>=src_CH_COUNT?(src_CH_COUNT-1):k;
            *tar_Pix=src_Pix_Gray[src_pix_ch];
            tar_Pix++;
          }
          src_Pix_Gray+=src_CH_COUNT;
        }
      }
    }

    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  // LOGI("img.size:%d ", img_size);


  return ACK;
}



CameraLayer::status CameraLayer_GIGE_MindVision::InitCamera(tSdkCameraDevInfo *devInfo)
{
  if (m_hCamera != 0)
  {
    LOGE("There is a camera hdl:&d..... INIT can only be done once", m_hCamera);
    return CameraLayer::NAK;
  }

  {
    char buff[1000];
    snprintf(buff, sizeof(buff),
             "{\
      \"type\":\"CameraLayer_GIGE_MindVision\",\
      \"name\":\"%s\",\
      \"friendly_name\":\"%s\",\
      \"line_name\":\"%s\",\
      \"port_type\":\"%s\",\
      \"product_name\":\"%s\",\
      \"product_series\":\"%s\",\
      \"sensor_type\":\"%s\",\
      \"Sn\":\"%s\"\
    }",
             devInfo->acFriendlyName,
             devInfo->acFriendlyName,
             devInfo->acLinkName,
             devInfo->acPortType,
             devInfo->acProductName,
             devInfo->acProductSeries,
             devInfo->acSensorType,
             devInfo->acSn);
    cam_json_info.assign(buff);
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
  CameraSetCallbackFunction(m_hCamera, sGIGEMV_CB, (PVOID)this, NULL);
  CameraSetAeState(m_hCamera, FALSE);
  //CameraSetAutoConnect(m_hCamera,true);
  //

  int width = sCameraInfo.sResolutionRange.iWidthMax;
  int height = sCameraInfo.sResolutionRange.iHeightMax;
  maxWidth = width;
  maxHeight = height;
  int maxBufferSize = width * height * 3;
  m_pFrameBuffer = (BYTE *)CameraAlignMalloc(maxBufferSize, 16);
  LOGV("m_pFrameBuffer:%p m_hCamera:%d>>W:%d H:%d", m_pFrameBuffer, m_hCamera, width, height);

  //EXT_TRIG_EXP_GRR might cause the image brightness from manual trigger much brighter
  //TODO: do extensive EXT_TRIG_EXP_GRR test in the future..
  int retx = CameraSetExtTrigShutterType(m_hCamera, EXT_TRIG_EXP_STANDARD);
  LOGI("CameraSetExtTrigShutterType: ret:%d", retx);
  CameraSetIspOutFormat(m_hCamera, CAMERA_MEDIA_TYPE_BGR8);
  CameraPlay(m_hCamera);
  CameraLoadParameter(m_hCamera, PARAMETER_TEAM_A);
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetMirror(int Dir, int en)
{
  if (Dir < 0 || Dir > 1)
  {
    return CameraLayer::NAK;
  }
  m.lock();
  CameraSetMirror(m_hCamera, Dir, en);
  mirrorFlag[Dir] = en;
  m.unlock();
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetROIMirror(int Dir, int en)
{
  ROI_mirrorFlag[Dir] = en;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::L_TriggerMode(int type)
{
  m.lock();
  //0 for continuous, 1 for soft trigger, 2 for HW trigger
  if (CameraSetTriggerMode(m_hCamera, type) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    m.unlock();
    return CameraLayer::NAK;
  }
  L_triggerMode = type;
  m.unlock();
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetROI(int x, int y, int w, int h, int zw, int zh)
{

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (w < 0)
    w = 0;
  if (h < 0)
    h = 0;

  if (x <= 1)
  {
    ROI_x = x * maxWidth;
  }
  else
  {
    ROI_x = x > maxWidth ? maxWidth : x;
  }

  if (y <= 1)
  {
    ROI_y = y * maxHeight;
  }
  else
  {
    ROI_y = y > maxHeight ? maxHeight : y;
  }

  if (w <= 1)
  {
    ROI_w = w * maxWidth;
  }
  else
  {
    ROI_w = w > maxWidth ? maxWidth : w;
  }

  if (h <= 1)
  {
    ROI_h = h * maxHeight;
  }
  else
  {
    ROI_h = h > maxHeight ? maxHeight : h;
  }

  if (ROI_x >= maxWidth - 5)
    ROI_x = maxWidth - 5;
  if (ROI_w > maxWidth - ROI_x)
    ROI_w = maxWidth - ROI_x;

  if (ROI_y >= maxHeight - 5)
    ROI_y = maxHeight - 5;
  if (ROI_h > maxHeight - ROI_y)
    ROI_h = maxHeight - ROI_y;

  ROI_x = (int)ROI_x;
  ROI_y = (int)ROI_y;
  ROI_w = (int)ROI_w;
  ROI_h = (int)ROI_h;

  if (ROI_mirrorFlag[0])
  {
    ROI_x = maxWidth - (ROI_x + ROI_w);
  }

  if (ROI_mirrorFlag[1])
  {
    ROI_y = maxHeight - (ROI_y + ROI_h);
  }
  LOGI("MAX:%d %d", maxWidth, maxHeight);
  LOGI("ROI:%f %f %f %f", ROI_x, ROI_y, ROI_w, ROI_h);

  ROI_w=(int)ROI_w/16*16;
  ROI_h=(int)ROI_h/16*16;

  if(ROI_w<16)ROI_w=16;
  if(ROI_h<16)ROI_h=16;
  if(ROI_x>maxWidth-ROI_w)
  {
    ROI_x=maxWidth-ROI_w;
  }
  if(ROI_y>maxHeight-ROI_h)
  {
    ROI_y=maxHeight-ROI_h;
  }

  ROI_x=(int)ROI_x/16*16;//just to guess the inc value, usually 2 or 4 or 16
  ROI_y=(int)ROI_y/16*16;
  ROI_w=(int)ROI_w/16*16;
  ROI_h=(int)ROI_h/16*16;
  tSdkImageResolution resInfo = {
    iIndex : 0xFF,
    uBinSumMode : 0,
    uBinAverageMode : 0,
    uSkipMode : 0,
    uResampleMask : 0,
    iHOffsetFOV : (int)ROI_x,
    iVOffsetFOV : (int)ROI_y,
    iWidthFOV : (int)ROI_w,
    iHeightFOV : (int)ROI_h,
    iWidth : (int)ROI_w,
    iHeight : (int)ROI_h,
    iWidthZoomHd : 0,
    iHeightZoomHd : 0,
    iWidthZoomSw : 0,
    iHeightZoomSw : 0
  }; 

  CameraSdkStatus camst = CameraSetImageResolution(m_hCamera, &resInfo);
  
  // LOGI("ret>>ROI:%d %d, %d %d, %d %d", 
  //   resInfo.iHOffsetFOV,  resInfo.iVOffsetFOV  
  //   ,resInfo.iWidthFOV,resInfo.iWidth 
  //   ,resInfo.iHeightFOV,resInfo.iHeight);
  if (camst)
    return CameraLayer::NAK;
  camst = CameraGetImageResolution(m_hCamera, &resInfo);
  if (camst)
    return CameraLayer::NAK;
  //int maxBufferSize = (int)ROI_w*(int)ROI_h*3;
  ROI_x = resInfo.iHOffsetFOV;
  ROI_y = resInfo.iVOffsetFOV;

  ROI_w = resInfo.iWidth;
  ROI_h = resInfo.iHeight;

  if(0)
  {
    // LOGI("ROI:%f %f %f %f", ROI_x, ROI_y, ROI_w, ROI_h);
    tSdkImageResolution resInfo = {
      iIndex : 0xFF,
      uBinSumMode : 0,
      uBinAverageMode : 0,
      uSkipMode : 0,
      uResampleMask : 0,
      iHOffsetFOV : (int)ROI_x,
      iVOffsetFOV : (int)ROI_y,
      iWidthFOV : (int)ROI_w,
      iHeightFOV : (int)ROI_h,
      iWidth : (int)ROI_w,
      iHeight : (int)ROI_h,
      iWidthZoomHd : 0,
      iHeightZoomHd : 0,
      iWidthZoomSw : 0,
      iHeightZoomSw : 0
    };

    CameraSdkStatus camst = CameraSetImageResolution(m_hCamera, &resInfo);
  }

      LOGI(">>>>");
  if (ROI_mirrorFlag[0])
  {
    ROI_x = maxWidth - (ROI_x + ROI_w);
  }

      LOGI(">>>>");
  if (ROI_mirrorFlag[1])
  {
    ROI_y = maxHeight - (ROI_y + ROI_h);
  }
  LOGI("ret>>ROI:%f %f %f %f", ROI_x, ROI_y, ROI_w, ROI_h);
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh)
{
  if (x)
    *x = ROI_x;
  if (y)
    *y = ROI_y;
  if (w)
    *w = ROI_w;
  if (h)
    *h = ROI_h;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::TriggerMode(int type)
{

  return L_TriggerMode(type);
}

CameraLayer::status CameraLayer_GIGE_MindVision::TriggerCount(int count)
{
  takeCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_GIGE_MindVision::Trigger()
{
  if (CameraSoftTrigger(m_hCamera) != CAMERA_STATUS_SUCCESS)
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

CameraLayer::status CameraLayer_GIGE_MindVision::isInOperation()
{
  int min,max;

  return GetAnalogGain(&min, &max);
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetResolution(int width, int height)
{
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_GIGE_MindVision::SetAnalogGain(float gain)
{
  //CameraSetGain(m_hCamera,gain,gain,gain);
  if (CameraSetAnalogGain(m_hCamera, (int)gain) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::L_SetFrameRateMode(int mode)
{
  if (CameraSetFrameSpeed(m_hCamera, mode) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    return CameraLayer::NAK;
  }
  L_frameRateMode = mode;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetOnceWB()
{
  CameraSetClrTempMode(m_hCamera, CT_MODE_AUTO);
  CameraSetParameterMask(m_hCamera, ~0);
  CameraSaveParameter(m_hCamera, PARAMETER_TEAM_A);
  return CameraSetOnceWB(m_hCamera) == CAMERA_STATUS_SUCCESS ? CameraLayer::ACK : CameraLayer::NAK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetFrameRate(float frame_rate)
{


  int mode=2;
  if(frame_rate<20)
  {
    mode=1;
  }
  
  if(frame_rate<10)
  {
    mode=0;
  }
  
  return L_SetFrameRateMode(mode);
}


CameraLayer::status CameraLayer_GIGE_MindVision::GetAnalogGain(int *ret_min, int *ret_max)
{
  if (CameraGetAnalogGain(m_hCamera, ret_min) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    return CameraLayer::NAK;
  }
  *ret_max = *ret_min;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::SetExposureTime(float time_us)
{
  if (CameraSetExposureTime(m_hCamera, time_us) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_GIGE_MindVision::GetExposureTime(float *ret_time_ms)
{
  double ret_d_time_ms;
  if (CameraGetExposureTime(m_hCamera, &ret_d_time_ms) != CAMERA_STATUS_SUCCESS)
  {
    LOGE("Failed...");
    return CameraLayer::NAK;
  }
  if(ret_time_ms)*ret_time_ms=ret_d_time_ms;
  return CameraLayer::ACK;
}