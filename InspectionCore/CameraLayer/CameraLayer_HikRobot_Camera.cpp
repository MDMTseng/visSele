
#include "CameraLayer_HikRobot_Camera.hpp"

#include <string>

MV_CC_DEVICE_INFO_LIST s_dev_list;
CameraLayer::status CameraLayer_HikRobot_Camera::SetROI(int x, int y, int w, int h, int zw, int zh)
{

  StopAquisition();
  int max_w, max_h;

  MVCC_INTVALUE_EX WInfo = {0};
  GetIntValue("WidthMax", &WInfo);
  MVCC_INTVALUE_EX HInfo = {0};
  GetIntValue("HeightMax", &HInfo);
  max_w = WInfo.nCurValue;
  max_h = HInfo.nCurValue;
  if (x >= max_w || y >= max_h || w < 0 || h < 0)
  {
    return CameraLayer::NAK;
  }
  if (x < 0)
  {
    w += x;
    x = 0;
  }
  if (y < 0)
  {
    h += y;
    y = 0;
  }

  if (x + w > max_w)
  {
    w = max_w - x;
  }

  if (y + h > max_h)
  {
    h = max_h - y;
  }

  
  if (ROI_mirrorFlag[0])
  {
    x = max_w - (x + w);
  }

  if (ROI_mirrorFlag[1])
  {
    y = max_h - (y + h);
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  SetIntValue_w_Check("OffsetX", 0);
  SetIntValue_w_Check("OffsetY", 0);
  int wret=SetIntValue_w_Check("Width", (int)w);
  int hret=SetIntValue_w_Check("Height", (int)h);
  int xret=SetIntValue_w_Check("OffsetX", (int)x);
  int yret=SetIntValue_w_Check("OffsetY", (int)y);
  // LOGI("SET:%d,%d,%d,%d,  ret:%d,%d,%d,%d max.wh:%d,%d",x,y,w,h, xret,yret,wret,hret,max_w,max_h);
  GetROI(&x, &y, &w, &h, NULL,NULL);
  // LOGI("SET:%d,%d,%d,%d,  ret:%d,%d,%d,%d",x,y,w,h, xret,yret,wret,hret);
  // StartAquisition();
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh)
{
  MVCC_INTVALUE_EX WInfo = {0};
  GetIntValue("Width", &WInfo);
  MVCC_INTVALUE_EX HInfo = {0};
  GetIntValue("Height", &HInfo);
  MVCC_INTVALUE_EX OXInfo = {0};
  GetIntValue("OffsetX", &OXInfo);
  MVCC_INTVALUE_EX OYInfo = {0};
  GetIntValue("OffsetY", &OYInfo);

  if (x)
    *x = OXInfo.nCurValue;
  if (y)
    *y = OYInfo.nCurValue;
  if (w)
    *w = WInfo.nCurValue;
  if (h)
    *h = HInfo.nCurValue;
  return CameraLayer::ACK;
}

int CameraLayer_HikRobot_Camera::getXML(char *buffer, int bufferSize)
{
  unsigned int nXMLDataLen = 0;
  int nRet = MV_XML_GetGenICamXML(handle, (unsigned char *)buffer, bufferSize, &nXMLDataLen);
  if (buffer == NULL && bufferSize == 0)
  {
    return nXMLDataLen;
  }
  if (MV_OK != nRet)
  {
    return -1;
  }
  return nXMLDataLen;
}

void CameraLayer_HikRobot_Camera::sExceptionCallBack(unsigned int nMsgType, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;
  cl->ExceptionCallBack(nMsgType);
}

void CameraLayer_HikRobot_Camera::ExceptionCallBack(unsigned int nMsgType)
{
  inNoError=false;
  LOGI("ExceptionCallBack");
}


uint32_t CheckSumPush(uint32_t chSum,uint32_t val)
{
  const int shift=2;
  uint64_t v=(chSum<<shift)+val;

  uint64_t head=v>>32;
  while(head)
  {
    v&=((uint64_t)1<<32)-1;//head cut off
    v+=head;
    head=v>>32;
  }

  return v;
}

uint32_t pDataCheckSum( unsigned char *pData,size_t length)
{
  uint32_t chSum=0;
  int segs=20;
  for(int i=0;i<segs;i++)
  {
    int idx=i*(length-1)/(segs-1);
    chSum=CheckSumPush(chSum,pData[idx]);
  }
  return chSum;
}

void CameraLayer_HikRobot_Camera::sImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;
  // cl->ImageCallBack(pData, pFrameInfo);


  
  MvGvspPixelType pType =pFrameInfo->enPixelType;

  int chNum=1;
  if(pType == PixelType_Gvsp_Mono8)
  {
    chNum=1;
  }

  size_t datLength=pFrameInfo->nWidth*pFrameInfo->nHeight*chNum;
  hikFrameInfo info={
    .pData=pData,
    .pDataL=datLength,
    .sampleCheckSum= pDataCheckSum( pData,datLength),
    .frameInfo=*pFrameInfo,
    .context=context
  };
  
  LOGI("sampleCheckSum:%x",info.sampleCheckSum);


  try{
    cl->imgQueue.push_blocking(info);
  }
  catch(TS_Termination_Exception e)
  {
    LOGI(">>>");
  }
}


CameraLayer::status CameraLayer_HikRobot_Camera::isInOperation()
{
  return inNoError?ACK:NAK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount)
{

  if (_cached_pData == NULL || _cached_frame_info==NULL)
  {
    return NAK;
  }
  
  MvGvspPixelType pType = _cached_frame_info->enPixelType;
  LOGI("pType:%X PixelType_Gvsp_BayerGB8:%X", pType,PixelType_Gvsp_BayerGB8);
  if(pType == PixelType_Gvsp_Mono8)
  {
    
    int w=_cached_frame_info->nWidth;
    int h=_cached_frame_info->nHeight;
    for(int i=0;i<h;i++)
    {
      uint8_t* src_Pix_Gray=_cached_pData+i*w;
      uint8_t* tar_Pix=imgBuffer+(i*w)*channelCount;
      for(int j=0;j<w;j++)
      {
        tar_Pix[0]=
        tar_Pix[1]=
        tar_Pix[2]=*src_Pix_Gray;
        tar_Pix+=channelCount;
        // for(int k=0;k<channelCount;k++)//Somehow it's super slow
        // {
        //   *tar_Pix=*src_Pix_Gray;
        //   tar_Pix++;
        // }
        src_Pix_Gray++;
      }
    }
    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  else if(pType == PixelType_Gvsp_BGR8_Packed)
  {
    
    int w=_cached_frame_info->nWidth;
    int h=_cached_frame_info->nHeight;

    

    for(int i=0;i<h;i++)
    {
      uint8_t* src_Pix_Gray=_cached_pData+i*w*3;
      uint8_t* tar_Pix=imgBuffer+(i*w)*channelCount;
      memcpy(tar_Pix,src_Pix_Gray,w*3);
      // for(int j=0;j<w;j++)
      // {
      //   tar_Pix[0]=
      //   tar_Pix[1]=
      //   tar_Pix[2]=src_Pix_Gray[2];
      //   tar_Pix+=channelCount;
      //   // for(int k=0;k<channelCount;k++)//Somehow it's super slow
      //   // {
      //   //   *tar_Pix=*src_Pix_Gray;
      //   //   tar_Pix++;
      //   // }
      //   src_Pix_Gray+=3;
      // }
    }
    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  else
  {
    return NAK;
  }
  // LOGI("img.size:%d ", img_size);


  return ACK;
}

void CameraLayer_HikRobot_Camera::ImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo)
{


  // if (takeCount >= 0)
  //   LOGI(">>>>>takeCount:%d", takeCount);

  if (takeCount == 0)
  {
    
    // MV_CC_StopGrabbing(handle);
    // acquisition_started=false;
    return;
  }




  uint32_t tstH = pFrameInfo->nDevTimeStampHigh;
  uint32_t tstL = pFrameInfo->nDevTimeStampLow;
  uint64_t nDevTimeStamp = (((uint64_t)tstH) << 32) | tstL; //10ns
  uint64_t nHostTimeStamp = pFrameInfo->nHostTimeStamp;     //1ms

  uint64_t nDevTimeStamp_us = nDevTimeStamp / 100;
  uint64_t nHostTimeStamp_us = nHostTimeStamp * 1000;

  MvGvspPixelType pType = pFrameInfo->enPixelType;
  // LOGI("ImageCallBack:%d,%d %dx%d type:%0X len:%d  %d,%d,%d nDevTimeStamp:%lu host:%lu",
  //      pFrameInfo->nOffsetX, pFrameInfo->nOffsetY,
  //      pFrameInfo->nWidth, pFrameInfo->nHeight,
  //      pType, pFrameInfo->nFrameLen,
  //      pData[0], pData[1], pData[2], nDevTimeStamp_us, nHostTimeStamp_us);

  // LOGI("nUnparsedChunkNum:%d nLostPacket:%d nChunkWH:%d,%d",
  //      pFrameInfo->nUnparsedChunkNum,pFrameInfo->nLostPacket,pFrameInfo->nChunkWidth,pFrameInfo->nChunkHeight);
  _cached_pData=(uint8_t*)pData;
  _cached_frame_info=pFrameInfo;


  frameInfo _fi;
  _fi.timeStamp_us = nDevTimeStamp_us;
  _fi.offset_x =  pFrameInfo->nOffsetX;
  _fi.offset_y =  pFrameInfo->nOffsetY;
  _fi.width =  pFrameInfo->nWidth;
  _fi.height = pFrameInfo->nHeight;
  fi=_fi;
  callback(*this, CameraLayer::EV_IMG, context);

  if (takeCount > 0)
  {
    takeCount--;
  }

  if (takeCount == 0)
  {
    // MV_CC_StopGrabbing(handle);
    // acquisition_started=false;
  }


  if (snapFlag)
  {
    snapFlag = 0;

    conV.notify_one();
  }

  _cached_pData=NULL;
  _cached_frame_info=NULL;
}

int32_t listDevices(MV_CC_DEVICE_INFO_LIST *stDeviceList)
{
  if (stDeviceList == NULL)
    return MV_E_PARAMETER;
  memset(stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

  int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, stDeviceList);

  return nRet;
}

int CameraLayer_HikRobot_Camera::listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist)
{
  
  listDevices(&s_dev_list);
  
  for(int i=0;i<s_dev_list.nDeviceNum;i++)
  {
    CameraLayer::BasicCameraInfo info;
    switch(s_dev_list.pDeviceInfo[i]->nTLayerType)
    {
      case MV_GIGE_DEVICE:
      {
        auto mvcamInfo=s_dev_list.pDeviceInfo[i]->SpecialInfo.stGigEInfo;
        info.name=std::string((char*)mvcamInfo.chUserDefinedName);

        info.id=std::string((char*)mvcamInfo.chSerialNumber);
        info.model=std::string((char*)mvcamInfo.chModelName);


        info.serial_number=std::string((char*)mvcamInfo.chSerialNumber);

        info.vender=std::string((char*)mvcamInfo.chManufacturerName);



        // info.id=tmpList[i].id;
        // info.model=tmpList[i].model;
        // info.serial_number=tmpList[i].serial_nbr;
        // info.vender=tmpList[i].vendor;
        // info.ctx=NULL;
        // info.driver_name=getDriverName();


      break;
      }
      case MV_USB_DEVICE:
      {
        auto mvcamInfo=s_dev_list.pDeviceInfo[i]->SpecialInfo.stUsb3VInfo;
        info.name=std::string((char*)mvcamInfo.chUserDefinedName);
        info.id=std::string((char*)mvcamInfo.chSerialNumber);
        info.model=std::string((char*)mvcamInfo.chModelName);


        info.serial_number=std::string((char*)mvcamInfo.chSerialNumber);

        info.vender=std::string((char*)mvcamInfo.chManufacturerName);



        // info.id=tmpList[i].id;
        // info.model=tmpList[i].model;
        // info.serial_number=tmpList[i].serial_nbr;
        // info.vender=tmpList[i].vendor;
        // info.ctx=NULL;
        // info.driver_name=getDriverName();


      break;
      }
    }
    
    
    info.driver_name="HikRobot";//CameraLayer_HikRobot_Camera::getDriverName();

    LOGI(">>>name:%s  sn:%s  model:%s   ",info.name.c_str(),info.serial_number.c_str(),info.model.c_str());
    LOGI(">>>driver_name:%s   ",info.driver_name.c_str());

    devlist.push_back(info);
  }

  return devlist.size();


}



CameraLayer_HikRobot_Camera::CameraLayer_HikRobot_Camera(
  CameraLayer::BasicCameraInfo camInfo,
  std::string misc,
  CameraLayer_Callback cb, 
  void *context):CameraLayer(camInfo,misc,cb, context),imgQueue(10)
{
  bDevConnected = false;

  const MV_CC_DEVICE_INFO *devInfo=NULL;
  for(int i=0;i<s_dev_list.nDeviceNum;i++)
  {
    // if(camInfo.serial_number == std::string((char*)s_dev_list.  .chSerialNumber))





    switch(s_dev_list.pDeviceInfo[i]->nTLayerType)
    { 
      case MV_GIGE_DEVICE:
      {
        auto mvcamInfo=s_dev_list.pDeviceInfo[i]->SpecialInfo.stGigEInfo;
        if(camInfo.serial_number==std::string((char*)mvcamInfo.chSerialNumber))
        {
          devInfo=s_dev_list.pDeviceInfo[i];
        }
      break;
      }
      case MV_USB_DEVICE:
      {
        auto mvcamInfo=s_dev_list.pDeviceInfo[i]->SpecialInfo.stUsb3VInfo;
        if(camInfo.serial_number==std::string((char*)mvcamInfo.chSerialNumber))
        {
          devInfo=s_dev_list.pDeviceInfo[i];
        }
      break;
      }

    }

    if(devInfo)
      break;
  }



  if (devInfo == NULL)
  {
    throw std::invalid_argument("NULL devInfo");
  }

  HIKERR nRet = MV_CC_CreateHandle(&handle, devInfo);
  if (MV_OK != nRet)
  {
    string excpMsg = "target device handle creation failed";
    throw std::invalid_argument(excpMsg);
  }

  if (devInfo->nTLayerType == MV_GIGE_DEVICE)
  { //if it's GIGE device, try detect Optimal Packet Size
    //And configure it

    do
    {
      int32_t nRet = MV_CC_GetOptimalPacketSize(handle);
      if (nRet < MV_OK)
      {
        break;
      }

      int32_t pOptimalPacketSize = (int32_t)nRet;

      nRet = MV_CC_SetIntValueEx(handle, "GevSCPSPacketSize", pOptimalPacketSize);
      if (nRet != MV_OK)
      {
        break;
      }
    } while (0);
  }

  nRet = MV_CC_OpenDevice(handle);

  if (MV_OK != nRet)
  {
    CLOSE();
    string excpMsg = "target device open/connect failed";
    throw std::invalid_argument(excpMsg);
  }
  bDevConnected = true;

  nRet = MV_CC_RegisterImageCallBackEx(handle, sImageCallBack, this);
  if (MV_OK != nRet)
  {

    CLOSE();
    string excpMsg = "RegisterImageCallBack failed";
    throw std::invalid_argument(excpMsg);
  }

  nRet = MV_CC_RegisterExceptionCallBack(handle, sExceptionCallBack, this);
  if (MV_OK != nRet)
  {
  }

  if (0)
  {

    MVCC_INTVALUE_EX intValInfo = {0};
    GetIntValue("OffsetX", &intValInfo);
    LOGI("%d<[%d]<%d....%d", intValInfo.nMin, intValInfo.nCurValue, intValInfo.nMax, intValInfo.nInc);
  }

  {

    nRet = MV_CC_SetBoolValue(handle, "ChunkModeActive", true);
    nRet = MV_CC_SetEnumValueByString(handle, "ChunkSelector", "Exposure");
    nRet = MV_CC_SetBoolValue(handle, "ChunkEnable", true);
    nRet = MV_CC_SetEnumValueByString(handle, "ChunkSelector", "Timestamp");
    nRet = MV_CC_SetBoolValue(handle, "ChunkEnable", true);
    
    SetFrameRate(60);
    
  }

  // SetROI(1000,1000,200,200,0,0);

  MV_IMAGE_BASIC_INFO img_basic_info;
  MV_CC_GetImageInfo(handle, &img_basic_info);
  TriggerMode(1);
  threadRunningState = true;

  SetROI(0, 0, 999999, 999999, 0, 0);

  {
    MVCC_ENUMVALUE pixFormat={0};
    int ret = MV_CC_GetPixelFormat(handle,&pixFormat);
    //1080001 graylevel
    //108000A GB

    if(pixFormat.nCurValue==PixelType_Gvsp_BayerGB8)
    {
      int pixF=MV_CC_SetPixelFormat(handle,PixelType_Gvsp_BGR8_Packed);
    }
    ret = MV_CC_GetPixelFormat(handle,&pixFormat);

    // LOGI(">>nCurValue:%X",pixFormat.nCurValue);

  }






  StartAquisition();
  inNoError=true;
  imgQueueThread = std::thread(&CameraLayer_HikRobot_Camera::imgQThreadFunc,this);

  
  char buff[300];
  snprintf(buff, sizeof(buff),
  "{\
    \"type\":\"CameraLayer_HikRobot_Camera\",\
    \"name\":\"HikCam\"\
  }");



  cam_json_info.assign(buff);
  LOGI(">>>%s",cam_json_info.c_str());
}

void CameraLayer_HikRobot_Camera::imgQThreadFunc()
{
  int nRet = MV_OK;
  while (bDevConnected==true)
  {
    hikFrameInfo info;
    try{
      imgQueue.pop_blocking(info);
      uint32_t cur_chSum=pDataCheckSum( info.pData,info.pDataL);
      if(info.sampleCheckSum!=cur_chSum)
      {//check sum error
        
        LOGE("ERROR:skip  0x%X!=0x%X",info.sampleCheckSum,cur_chSum);
        continue;
      }

    }
    catch(TS_Termination_Exception e)
    {
      break;
    }
    ImageCallBack(info.pData, &info.frameInfo);
  }
  LOGI("Thread ended....");
}

void CameraLayer_HikRobot_Camera::CLOSE()
{
  if (bDevConnected)
  {
    MV_CC_CloseDevice(handle);
    bDevConnected = false;
  }
  if (handle != NULL)
  {
    MV_CC_DestroyHandle(handle);
    handle = NULL;
  }
}
CameraLayer_HikRobot_Camera::~CameraLayer_HikRobot_Camera()
{
  imgQueue.termination_trigger();
  CLOSE();
  imgQueueThread.join();
}



CameraLayer::status CameraLayer_HikRobot_Camera::TriggerCount(int count)
{
  takeCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_HikRobot_Camera::TriggerMode(int type)
{
  
  CameraLayer::TriggerMode(type);
  // LOGI(">>>>type:%d",type);
  if (type == 0) //continuous
  {
    takeCount=-1;
    int nRet = SetEnumValue("TriggerMode", MV_TRIGGER_MODE_OFF);
    SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  int nRet = SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);

  // MV_CC_StopGrabbing(handle);
  if (type == 1) //software trigger
  {
    takeCount=0;
    int nRet = SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  if (type == 2) //hardware trigger
  {
    takeCount=-1;
    int nRet = SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::Trigger()
{
  if (takeCount >= 0)
    takeCount++;
  // arv_camera_software_trigger (camera,&err);

  int nRet = CommandExecute("TriggerSoftware");
  acquisition_started=true;
  
  return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
}


CameraLayer::status CameraLayer_HikRobot_Camera::SetAnalogGain(float gain)
{
  return (MV_OK == SetFloatValue("Gain",gain)) ? CameraLayer::ACK : CameraLayer::NAK;
}


CameraLayer::status CameraLayer_HikRobot_Camera::SetMirror(int Dir, int en)
{
  if (Dir < 0 || Dir > 1)
  {
    return CameraLayer::NAK;
  }
  m.lock();
  bool ben=(en!=0);
  if(Dir==0)
  {
    SetBoolValue("ReverseX", ben);
  }
  else if(Dir==1)
  {
    SetBoolValue("ReverseY", ben);
  }
  mirrorFlag[Dir] = en;
  m.unlock();
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::SetROIMirror(int Dir, int en)
{
  ROI_mirrorFlag[Dir] = en;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::SetFrameRate(float frame_rate)
{

  if(frame_rate>1000)//Max FPS
  {
    return (MV_OK == MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", false)) ? CameraLayer::ACK : CameraLayer::NAK;
  }
  MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", true);
  MVCC_FLOATVALUE resFPS;
  int ret2 = GetFloatValue("ResultingFrameRate",&resFPS);

  if(frame_rate>resFPS.fCurValue)frame_rate=resFPS.fMax;
  int ret = SetFloatValue("AcquisitionFrameRate",frame_rate);
  // LOGI(">%d> %f<%f<%f   ,%d",ret,resFPS.fMin,resFPS.fCurValue,resFPS.fMax,ret2);


  return (MV_OK == ret) ? CameraLayer::ACK : CameraLayer::NAK;
  
}
// CameraLayer::status CameraLayer_HikRobot_Camera::SetFrameRateMode(int mode)
// {
//   if(mode>=2)
//   {//as fast as possible
//     return SetBoolValue("AcquisitionFrameRateEnable", false)==0?CameraLayer::ACK:CameraLayer::NAK;
//   }
//   float tar_fr=30;
//   switch(mode)
//   {
//       case 0:tar_fr=1;break;
//       case 1:tar_fr=10;break;
//   }
  
//   SetBoolValue("AcquisitionFrameRateEnable", true);
//   return SetFrameRate(tar_fr);
// }

CameraLayer::status CameraLayer_HikRobot_Camera::SetExposureTime(float time_us)
{
  return (MV_OK == SetFloatValue("ExposureTime",time_us)) ? CameraLayer::ACK : CameraLayer::NAK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::GetExposureTime(float *ret_time_us)
{
  MVCC_FLOATVALUE retV;
  int ret = GetFloatValue("ExposureTime",&retV);
  if(ret_time_us)*ret_time_us=retV.fCurValue;
  return (MV_OK == ret) ? CameraLayer::ACK : CameraLayer::NAK;
}


CameraLayer::status CameraLayer_HikRobot_Camera::SetBalckLevel(float blvl)
{
  int ret = SetIntValue("BlackLevel",(int)blvl);
  // LOGI("BlackLevel set ret:%x,%d",ret,ret);
  return (MV_OK == ret) ? CameraLayer::ACK : CameraLayer::NAK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::SetGamma(float gamma)
{
  int ret;
  if(gamma!=gamma)
  {
    return (MV_OK == SetBoolValue("GammaEnable",false)) ? CameraLayer::ACK : CameraLayer::NAK;
  }
  ret=SetBoolValue("GammaEnable",true);
  // LOGI("GammaEnable set ret:%x,%d",ret,ret);
  if(MV_OK != ret)
  {
    return CameraLayer::NAK;
  }

  ret= SetFloatValue("Gamma",gamma);
  // LOGI("ExposureTime set ret:%x,%d",ret,ret);
  return (MV_OK ==ret) ? CameraLayer::ACK : CameraLayer::NAK;
}



CameraLayer::status CameraLayer_HikRobot_Camera::StartAquisition()
{
  return (MV_OK == MV_CC_StartGrabbing(handle)) ? CameraLayer::ACK : CameraLayer::NAK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::StopAquisition()
{
  return (MV_OK == MV_CC_StopGrabbing(handle)) ? CameraLayer::ACK : CameraLayer::NAK;
}