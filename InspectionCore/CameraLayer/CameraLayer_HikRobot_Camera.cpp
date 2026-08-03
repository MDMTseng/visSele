
#include "CameraLayer_HikRobot_Camera.hpp"

#include <string>

#include <logctrl.h>
LOG_MODULE("cam.hikrobot");

// Enumeration result, shared between listAddDevices() and every constructor:
// the constructor does not take a device handle, it re-finds its own device by
// serial number in here. That makes the call order load-bearing --
// listAddDevices() must have run, and no other thread may be enumerating,
// while a camera is being constructed. Both hold today because
// CameraLayerManager enumerates then constructs on one thread, which is why
// this is documented rather than restructured; the fix (hand the constructor
// the MV_CC_DEVICE_INFO it was matched from) is a cross-file change to the
// manager's protocol, not a local one.
MV_CC_DEVICE_INFO_LIST s_dev_list;
CameraLayer::status CameraLayer_HikRobot_Camera::SetROI(int x, int y, int w, int h, int zw, int zh)
{

  // `m` serialises the compound stop-modify-start sequences (this and
  // SetMirror). Deliberately NOT taken inside Start/StopAquisition themselves:
  // those are called from in here, and std::mutex is not recursive.
  std::lock_guard<std::mutex> guard(m);

  // Width/Height/Offset are only writable while grabbing is stopped, but the
  // camera has to come back on afterwards. SetROI is a live runtime call
  // (Core0_1/wiringPanel.cpp:1229 drives it from the UI), so leaving it
  // stopped meant one ROI change silently killed the camera until reconnect.
  // Restore whatever state we found -- the constructor calls SetROI before
  // StartAquisition, and that path must stay "not grabbing".
  const bool was_running = acquisition_started;
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
    if (was_running) StartAquisition();
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
  // Checked here, while x/y/w/h still hold what we asked for -- GetROI below
  // overwrites them with what the camera settled on.
  const bool roi_rejected =
      (wret != MV_OK || hret != MV_OK || xret != MV_OK || yret != MV_OK);
  if (roi_rejected)
    LOGE("SetROI partially rejected: w=%x h=%x x=%x y=%x (asked %d,%d %dx%d, max %dx%d)",
         wret, hret, xret, yret, x, y, w, h, max_w, max_h);

  GetROI(&x, &y, &w, &h, NULL,NULL);
  // LOGI("SET:%d,%d,%d,%d,  ret:%d,%d,%d,%d",x,y,w,h, xret,yret,wret,hret);
  if (was_running) StartAquisition();

  return roi_rejected ? CameraLayer::NAK : CameraLayer::ACK;
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
  error_code_list.push_back(nMsgType);
  callback(*this, CameraLayer::EV_ERROR, context);
}


// (CheckSumPush/pDataCheckSum lived here: a sampled checksum taken before and
// after handing a frame over, used to detect that the SDK had recycled the
// buffer underneath us -- and to drop the frame when it had. The frame-copy
// pool made that race impossible, which left these dead. They were also
// non-static, so they leaked into the global namespace.)

void CameraLayer_HikRobot_Camera::sImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;

  MvGvspPixelType pType = pFrameInfo->enPixelType;
  int chNum = (pType == PixelType_Gvsp_Mono8) ? 1 : 3;
  size_t datLength = (size_t)pFrameInfo->nWidth * pFrameInfo->nHeight * chNum;

  // Drop detection. MVS nFrameNum increments for every frame the sensor
  // exposes regardless of whether it reaches us; a gap means frames were
  // lost between the sensor and this callback (on-camera buffer, USB, or
  // SDK node pool). Diagnostic only -- nothing downstream depends on it.
  if (cl->_lastFrameNumValid && pFrameInfo->nFrameNum > cl->_lastFrameNum + 1)
  {
    unsigned lost = pFrameInfo->nFrameNum - cl->_lastFrameNum - 1;
    cl->_framesDroppedByGap += lost;
    LOGE("camera dropped %u frame(s) (nFrameNum %u->%u)",
         lost, cl->_lastFrameNum, pFrameInfo->nFrameNum);
  }
  cl->_lastFrameNum = pFrameInfo->nFrameNum;
  cl->_lastFrameNumValid = true;
  cl->_framesDelivered++;

  // Copy the volatile SDK buffer into a reusable pool slot NOW, while
  // pData is still valid. The queue then carries a pointer into stable
  // memory the SDK cannot overwrite -- this is what eliminates the
  // buffer-reuse race that the old double-checksum was (lossily) papering
  // over by dropping every mismatched frame.
  std::vector<uint8_t> &slot = cl->_frameBufPool[cl->_frameBufIdx];
  cl->_frameBufIdx = (cl->_frameBufIdx + 1) % FRAME_POOL_SIZE;
  if (slot.size() < datLength) slot.resize(datLength);
  memcpy(slot.data(), pData, datLength);

  hikFrameInfo info = {
    .pData = slot.data(),
    .pDataL = datLength,
    .frameInfo = *pFrameInfo,
    .context = context
  };

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

  if (_cached_pData == NULL || _cached_frame_info==NULL || imgBuffer == NULL)
  {
    return NAK;
  }

  MvGvspPixelType pType = _cached_frame_info->enPixelType;

  // The caller sizes imgBuffer from the frameInfo it was handed, but nothing
  // forced the two to agree: pixelCount was accepted and then ignored
  // completely, and every write below is sized from the FRAME instead. A
  // frame arriving larger than the buffer the caller prepared -- or a caller
  // asking for fewer channels than the frame carries -- was an unchecked heap
  // write. Both are cheap to rule out here.
  const size_t frame_w = (size_t)_cached_frame_info->nWidth;
  const size_t frame_h = (size_t)_cached_frame_info->nHeight;
  if (frame_w * frame_h > pixelCount)
  {
    LOGE("ExtractFrame: frame %zux%zu exceeds the caller's %zu-pixel buffer -- dropping",
         frame_w, frame_h, pixelCount);
    return NAK;
  }
  if (channelCount < 1)
  {
    return NAK;
  }

  // (no per-frame logging here -- this runs once per captured frame, so at
  // production rates it was tens of lines a second of constant text.)
  if(pType == PixelType_Gvsp_Mono8)
  {

    int w=_cached_frame_info->nWidth;
    int h=_cached_frame_info->nHeight;
    if(channelCount==1)
    {
      // Native gray: copy straight through, no replicate-to-BGR.
      memcpy(imgBuffer, _cached_pData, (size_t)w*h);
      return ACK;
    }
    // The replicate-to-BGR loop below writes tar_Pix[0..2] per pixel.
    if(channelCount < 3)
    {
      LOGE("ExtractFrame: Mono8 frame needs channelCount 1 or >=3, got %d", channelCount);
      return NAK;
    }
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

    // Each row copies w*3 source bytes into a row whose stride is
    // w*channelCount; anything under 3 channels overruns by w*(3-channelCount)
    // bytes per row. The caller wanting gray from a colour frame is a real
    // case (the pipeline is migrating to 1-channel), so reject it rather than
    // corrupt the heap -- the caller derives channelCount from frameInfo, so
    // this should be unreachable in practice.
    if(channelCount < 3)
    {
      LOGE("ExtractFrame: BGR8 frame needs channelCount >=3, got %d", channelCount);
      return NAK;
    }

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
  // Carried through so downstream can absorb transmission drops instead of
  // silently shifting every subsequent pairing by one.
  _fi.frameNum = pFrameInfo->nFrameNum;
  _fi.frameNumValid = true;
  _fi.timeStamp_us = nDevTimeStamp_us;
  _fi.offset_x =  pFrameInfo->nOffsetX;
  _fi.offset_y =  pFrameInfo->nOffsetY;
  _fi.width =  pFrameInfo->nWidth;
  _fi.height = pFrameInfo->nHeight;
  // Report the true channel count so the pipeline can keep a mono sensor as a
  // 1-channel frame (no replicate-to-BGR). Color/Bayer is delivered as BGR8.
  _fi.channelCount = (pType == PixelType_Gvsp_Mono8) ? 1 : 3;
  _fi.pixelBits=8*_fi.channelCount; // CameraLayer::frameInfo uses pixelBits
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
  void *context):CameraLayer(camInfo,misc,cb, context),imgQueue(IMG_QUEUE_DEPTH)
{
  bDevConnected = false;
  _frameBufPool.resize(FRAME_POOL_SIZE);

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

  {

    // Chunk mode used to be switched on here (Exposure + Timestamp), with all
    // five return codes assigned to the same `nRet` and never checked. Nothing
    // in this layer has ever parsed a chunk: the exposure and timestamp we
    // actually use come from MV_FRAME_OUT_INFO_EX (nDevTimeStampHigh/Low),
    // which is standard frame info, not chunk payload. So it was pure
    // per-frame overhead on a link we have measured to be the throughput
    // ceiling. Turned off explicitly rather than just not-enabled, so a
    // camera-side saved user set cannot bring it back silently.
    int chunkRet = MV_CC_SetBoolValue(handle, "ChunkModeActive", false);
    if (MV_OK != chunkRet)
      LOGI("ChunkModeActive off: ret=%x (harmless if the node is absent)", chunkRet);

    // NOTE: the frame-rate cap that used to be applied here (SetFrameRate(60))
    // now lives in TriggerMode(), because whether a cap is wanted depends
    // entirely on the mode -- see the comment there.
  }

  // SetROI(1000,1000,200,200,0,0);

  MV_IMAGE_BASIC_INFO img_basic_info;
  MV_CC_GetImageInfo(handle, &img_basic_info);

  registerLineEvents();
  logTriggerConfig("after-open");

  TriggerMode(1);
  logTriggerConfig("after-TriggerMode(1)");
  threadRunningState = true;

  SetROI(0, 0, 999999, 999999, 0, 0);

  {
    // ExtractFrame can only decode Mono8 and BGR8_Packed, so the camera has to
    // end up on one of those or every frame is silently rejected downstream.
    //
    // The old code queried the supported formats, threw the answer away, and
    // blind-set BGR8_Packed -- which the SDK documents as invalid unless the
    // value appears in nSupportValue. On a mono sensor it "worked" only
    // because the set failed and left Mono8 in place. Same outcome, chosen on
    // purpose this time.
    MVCC_ENUMVALUE pixFormat = {0};
    int ret = MV_CC_GetPixelFormat(handle, &pixFormat);

    bool has_mono8 = false, has_bgr8 = false;
    if (MV_OK == ret)
    {
      const unsigned kMax = sizeof(pixFormat.nSupportValue) / sizeof(pixFormat.nSupportValue[0]);
      unsigned n = (pixFormat.nSupportedNum > kMax) ? kMax : pixFormat.nSupportedNum;
      for (unsigned i = 0; i < n; i++)
      {
        if (pixFormat.nSupportValue[i] == PixelType_Gvsp_Mono8)       has_mono8 = true;
        if (pixFormat.nSupportValue[i] == PixelType_Gvsp_BGR8_Packed) has_bgr8  = true;
      }
    }
    else
    {
      LOGE("could not read PixelFormat (ret=%x) -- leaving the camera on its current format", ret);
    }

    // Preference deliberately unchanged: colour sensors are delivered as
    // BGR8_Packed (the SDK debayers), mono sensors stay Mono8.
    // NOT switching colour sensors to Mono8 here, tempting as it is -- it
    // would be a third of the payload on a link we have measured to be the
    // throughput ceiling, but it changes what the inspection pipeline sees,
    // so it wants a bench A/B rather than a quiet default change.
    if (has_bgr8)
    {
      int setRet = MV_CC_SetPixelFormat(handle, PixelType_Gvsp_BGR8_Packed);
      if (MV_OK != setRet)
        LOGE("PixelFormat BGR8_Packed advertised but rejected (ret=%x)", setRet);
    }
    else if (has_mono8 && pixFormat.nCurValue != PixelType_Gvsp_Mono8)
    {
      int setRet = MV_CC_SetPixelFormat(handle, PixelType_Gvsp_Mono8);
      if (MV_OK != setRet)
        LOGE("PixelFormat Mono8 advertised but rejected (ret=%x)", setRet);
    }

    MV_CC_GetPixelFormat(handle, &pixFormat);
    if (pixFormat.nCurValue != PixelType_Gvsp_Mono8 &&
        pixFormat.nCurValue != PixelType_Gvsp_BGR8_Packed)
    {
      // Loud, because the symptom otherwise is "camera opens fine, every
      // frame vanishes" -- ExtractFrame NAKs anything else.
      LOGE("camera settled on PixelFormat %X, which ExtractFrame cannot decode "
           "(only Mono8 %X / BGR8_Packed %X) -- frames will be rejected",
           pixFormat.nCurValue, PixelType_Gvsp_Mono8, PixelType_Gvsp_BGR8_Packed);
    }
    else
    {
      LOGI("PixelFormat: %X (mono8=%d bgr8=%d supported)",
           pixFormat.nCurValue, (int)has_mono8, (int)has_bgr8);
    }
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

void CameraLayer_HikRobot_Camera::sEventCallBack(MV_EVENT_OUT_INFO *pEventInfo, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;
  if (cl != NULL && pEventInfo != NULL)
    cl->EventCallBack(pEventInfo);
}

void CameraLayer_HikRobot_Camera::EventCallBack(MV_EVENT_OUT_INFO *pEventInfo)
{
  // EventName is an inline char[128], never NULL -- the old "== NULL" guard was
  // always false. What can actually happen is an empty/unterminated name, so
  // check that instead, and bound the compares to the array.
  if (pEventInfo->EventName[0] == '\0') return;
  const size_t kNameMax = sizeof(pEventInfo->EventName);

  // Camera-clock timestamp of the edge itself. Survives even when no frame
  // follows, which is exactly the case we cannot see any other way.
  uint64_t tick = (((uint64_t)pEventInfo->nTimestampHigh) << 32) |
                    (uint64_t)pEventInfo->nTimestampLow;

  if (strncmp(pEventInfo->EventName, "Line0RisingEdge", kNameMax) == 0)
  {
    _line0RisingEdges++;
    _line0LastEdgeDevTick = tick;
  }
  else if (strncmp(pEventInfo->EventName, "Line0FallingEdge", kNameMax) == 0)
  {
    _line0FallingEdges++;
  }
}

// Registration is unconditional and failure is not fatal: on firmwares (or
// TriggerSource settings) where these events never fire the counters simply
// stay at 0, which is itself the diagnostic. Nothing downstream depends on
// them yet -- they exist to be reconciled against the trigger source.
void CameraLayer_HikRobot_Camera::registerLineEvents()
{
  static const char *kEvents[] = { "Line0RisingEdge", "Line0FallingEdge" };

  for (int i = 0; i < 2; i++)
  {
    int rSel = MV_CC_SetEnumValueByString(handle, "EventSelector", kEvents[i]);
    int rEn  = (rSel == MV_OK) ? MV_CC_SetBoolValue(handle, "EventNotification", true) : -1;
    int rCb  = MV_CC_RegisterEventCallBackEx(handle, kEvents[i], sEventCallBack, this);
    LOGI("event %s: selector=%d notify=%d register=%d",
         kEvents[i], rSel, rEn, rCb);
  }
}

// Reads back what the firmware actually settled on. TriggerMode/Source/
// Activation all apply to whichever TriggerSelector is current, so the selector
// is the field that decides whether "trigger mode on" means one frame per edge
// or something else entirely -- and this code has never set it, meaning the
// firmware default has been in force all along. Log it rather than change it:
// which value is correct here is a question for the hardware, not the source.
void CameraLayer_HikRobot_Camera::logTriggerConfig(const char *when)
{
  MVCC_ENUMVALUE ev = {0};
  const char *keys[] = { "TriggerSelector", "TriggerMode", "TriggerSource", "TriggerActivation" };

  for (int i = 0; i < 4; i++)
  {
    memset(&ev, 0, sizeof(ev));
    int r = MV_CC_GetEnumValue(handle, keys[i], &ev);
    // Numeric value only -- enough to compare against the SDK constants, and
    // enum-as-string readback is not available on every node.
    LOGI("[trigger cfg %s] %-18s ret=%d cur=%u", when, keys[i], r, ev.nCurValue);
  }
}

void CameraLayer_HikRobot_Camera::imgQThreadFunc()
{
  int nRet = MV_OK;
  while (bDevConnected==true)
  {
    hikFrameInfo info;
    try{
      imgQueue.pop_blocking(info);
    }
    catch(TS_Termination_Exception e)
    {
      break;
    }
    // info.pData points into a _frameBufPool slot -- a private copy made
    // in sImageCallBack, so it is always valid here (no SDK reuse race).
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
  // Teardown order matters. imgQThreadFunc runs ImageCallBack, which touches
  // the SDK handle and calls out through `callback` into the owner; the old
  // order (CLOSE then join) let it keep running against a handle that
  // MV_CC_DestroyHandle had already freed, and against _frameBufPool while
  // this object was being destroyed.
  //
  // 1. stop the camera so sImageCallBack pushes no more frames,
  // 2. wake the consumer out of pop_blocking,
  // 3. join it -- after this nothing else touches handle or the frame pool,
  // 4. only then close and destroy the handle.
  if (bDevConnected)
    StopAquisition();
  imgQueue.termination_trigger();
  if (imgQueueThread.joinable())
    imgQueueThread.join();
  CLOSE();
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
    // Free-run: a cap is the only thing stopping the sensor from flooding the
    // link at max fps, so keep the 60 the constructor used to apply globally.
    SetFrameRate(60);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  // Triggered: AcquisitionFrameRateEnable is a HARD CEILING ON THE TRIGGER
  // RATE, not just on free-run output -- triggers arriving faster than the cap
  // are discarded by the camera, with no error and no frame. That failure looks
  // exactly like a dead trigger wire or a flaky opto, which is why it has to be
  // off here. (Confirmed on the bench: the camera-side preset for this rig also
  // keeps AcquisitionFrameRateEnable false for the same reason.)
  int fr_ret = MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", false);
  if (MV_OK != fr_ret)
    LOGE("could not clear AcquisitionFrameRateEnable (ret=%x) -- trigger rate may be capped", fr_ret);

  // ONE frame per trigger. Measured on the bench: these bodies expose only
  // TriggerSelector=FrameBurstStart (there is no FrameStart trigger), so
  // AcquisitionBurstFrameCount *is* the frames-per-trigger control -- and it
  // defaults to 5. Left alone, every trigger produces five frames: five times
  // the payload, on a link whose bandwidth is exactly what decides whether the
  // camera can accept the next trigger at all. The pipeline pairs one frame to
  // one part, so anything but 1 also desynchronises that pairing.
  // Acquisition-control nodes are locked while the camera is grabbing --
  // writing this one mid-stream is rejected by the device. Stop, write, restore.
  const bool burst_was_running = acquisition_started;
  if (burst_was_running) StopAquisition();

  int burst_ret = SetIntValue_w_Check("AcquisitionBurstFrameCount", 1);
  if (MV_OK != burst_ret)
    LOGI("AcquisitionBurstFrameCount=1: ret=%x (harmless if the node is absent)", burst_ret);

  if (burst_was_running) StartAquisition();

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
    // NOTE: "Anyway"(13) accepts edges on any input line, but it also silences
    // the per-edge trigger events -- which is why _line0RisingEdges can read 0
    // on a rig that is triggering perfectly well. Switching to Line0 restores
    // the event stream, but only if the trigger is physically wired to Line0;
    // if it is on Line1/Line2 the set succeeds and no trigger ever arrives.
    // Left on Anyway until the wiring is confirmed on the machine.
    int nRet = SetEnumValue("TriggerSource", 13);//anyway
    if(nRet!=MV_OK)
      nRet = SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);
    logTriggerConfig("after-TriggerMode(2)");
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::Trigger()
{
  if (takeCount >= 0)
    takeCount++;
  // arv_camera_software_trigger (camera,&err);

  // NOT acquisition_started=true here: issuing a software trigger does not
  // start grabbing, and claiming it does would make SetROI/SetMirror restart
  // a camera that was deliberately stopped.
  int nRet = CommandExecute("TriggerSoftware");

  return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
}


CameraLayer::status CameraLayer_HikRobot_Camera::SetAnalogGain(float gain)
{
  return (MV_OK == SetFloatValue("Gain",gain)) ? CameraLayer::ACK : CameraLayer::NAK;
}


CameraLayer::status CameraLayer_HikRobot_Camera::SetRGain(float gain)
{
  
  MV_CC_SetEnumValueByString(handle, "BalanceWhiteAuto", "Off");
  MV_CC_SetEnumValueByString(handle, "BalanceRatioSelector", "Red");
  
  SetIntValue("BalanceRatio",(int)gain);
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::SetGGain(float gain)
{
  MV_CC_SetEnumValueByString(handle, "BalanceWhiteAuto", "Off");
  MV_CC_SetEnumValueByString(handle, "BalanceRatioSelector", "Green");
  
  SetIntValue("BalanceRatio",(int)gain);
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::SetBGain(float gain)
{
  MV_CC_SetEnumValueByString(handle, "BalanceWhiteAuto", "Off");
  MV_CC_SetEnumValueByString(handle, "BalanceRatioSelector", "Blue");
  
  SetIntValue("BalanceRatio",(int)gain);
  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::SetMirror(int Dir, int en)
{
  if (Dir < 0 || Dir > 1)
  {
    return CameraLayer::NAK;
  }
  // Was a raw m.lock()/m.unlock() pair -- any throw in between (or the early
  // return that used to sit inside it) left the mutex held forever.
  std::lock_guard<std::mutex> guard(m);

  // Same restore-what-we-found rule as SetROI: this used to start grabbing
  // unconditionally, so toggling a mirror on a stopped camera silently
  // started it.
  const bool was_running = acquisition_started;
  StopAquisition();
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
  if (was_running) StartAquisition();
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
  int ret = GetFloatValue("ResultingFrameRate",&resFPS);

  if(frame_rate>resFPS.fMax)frame_rate=resFPS.fMax;
  int ret2 = SetFloatValue("AcquisitionFrameRate",frame_rate);
  LOGI(">ret%x,%x>  set:%.1f  m:%.1f<cur:%.1f<M:%.1f",
    ret,ret2,
    frame_rate,
    resFPS.fMin,  resFPS.fCurValue,  resFPS.fMax);


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



// acquisition_started tracks whether the camera is actually grabbing, so that
// the settings calls that must briefly stop it (ROI, mirror) can put it back
// the way they found it instead of guessing.
CameraLayer::status CameraLayer_HikRobot_Camera::StartAquisition()
{
  int ret = MV_CC_StartGrabbing(handle);
  if (MV_OK == ret)
    acquisition_started = true;
  return (MV_OK == ret) ? CameraLayer::ACK : CameraLayer::NAK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::StopAquisition()
{
  int ret = MV_CC_StopGrabbing(handle);
  if (MV_OK == ret)
    acquisition_started = false;
  return (MV_OK == ret) ? CameraLayer::ACK : CameraLayer::NAK;
}