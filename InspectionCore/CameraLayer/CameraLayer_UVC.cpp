
#include "CameraLayer_UVC.hpp"

#include <string>

CameraLayer::status CameraLayer_UVC::SetROI(int x, int y, int w, int h, int zw, int zh)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh)
{
  return CameraLayer::NOT_SUPPORTED;
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


CameraLayer::status CameraLayer_UVC::isInOperation()
{
  return CameraLayer::NOT_SUPPORTED;
}

CameraLayer::status CameraLayer_UVC::ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount)
{

  return CameraLayer::NOT_SUPPORTED;
}
int CameraLayer_UVC::listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist)
{
  
  return 0;


}



std::string FourCCToString(uint32_t fourcc)
{
    std::string v;
    for(uint32_t i=0; i<4; i++)
    {
        v += static_cast<char>(fourcc & 0xFF);
        fourcc >>= 8;
    }
    return v;
}



CameraLayer_UVC::CameraLayer_UVC(
  CameraLayer::BasicCameraInfo camInfo,
  std::string misc,
  CameraLayer_Callback cb, 
  void *context):CameraLayer(camInfo,misc,cb, context)
  {

    ctx = Cap_createContext();
    
    int targetID=-1;
    int maxResFormatIdx=-1;


    uint32_t deviceCount = Cap_getDeviceCount(ctx);
    printf("Number of devices: %d\n", deviceCount);
    for (uint32_t i = 0; i < deviceCount; i++)
    {
      std::string devName=Cap_getDeviceName(ctx, i);
      std::string devID=Cap_getDeviceUniqueID(ctx, i);
      printf("ID %d -> %s\n", i, devName.c_str());

      camInfo.name=devName;

      // show all supported frame buffer formats
      int32_t nFormats = Cap_getNumFormats(ctx, i);

      printf("  Number of formats: %d\n", nFormats);

      maxResFormatIdx=-1;
      int maxRes=0;

      std::string fourccString;
      for (int32_t j = 0; j < nFormats; j++)
      {
        CapFormatInfo finfo;
        Cap_getFormatInfo(ctx, i, j, &finfo);
        fourccString = FourCCToString(finfo.fourcc);

        printf("  Format ID %d: %d x %d pixels  FOURCC=%s\n",
               j, finfo.width, finfo.height, fourccString.c_str());
        if(maxRes<finfo.width, finfo.height)
        {
            maxRes=finfo.width, finfo.height;
            maxResFormatIdx=j;
        }
      }



      if(
        (camInfo.name.length()>0 && camInfo.name==devName)&&
        (camInfo.serial_number.length()>0 && camInfo.serial_number==devID)
        )
      {
        targetID=i;
        break;
      }
    }

    if(targetID<0)
    {
        throw std::runtime_error("Camera not found");
    }

    int deviceFormatID=maxResFormatIdx;//TODO: select format


    streamID = Cap_openStream(ctx, targetID, deviceFormatID);
    printf("Stream ID = %d\n", streamID);
    
    if (Cap_isOpenStream(ctx, streamID) == 1)
    {
        printf("Stream is opened\n");
    }
    else
    {
        throw std::runtime_error("Stream open failed");
    }



    Cap_getFormatInfo(ctx, targetID, deviceFormatID, &streamfinfo);


    cameraThreadTerminationFlag=false;
    cameraThread = new std::thread(&CameraLayer_UVC::ContTriggerThread, this);



  }



void CameraLayer_UVC::ContTriggerThread( )
{




    std::vector<uint8_t> m_buffer;

    printf("w:%d,h:%d\n",streamfinfo.width,streamfinfo.height);


    for(;cameraThreadTerminationFlag==false;)
    {


      if (Cap_hasNewFrame(ctx, streamID) != 1)continue;//std::this_thread::sleep_for(std::chrono::milliseconds(10));
     
      m_buffer.resize(streamfinfo.width*streamfinfo.height*3);


      
      Cap_captureFrame(ctx, streamID, &m_buffer[0], m_buffer.size());

    //   int64 t1=cv::getTickCount();
    //   LOGE(">>>>>>>>process_time_us:%f", 1000000 * (t1 - t0) / cv::getTickFrequency());

    //   t0=t1;
    //   printf("get frame %d fps:%d bpp:%d\n",i,finfo.fps,finfo.bpp);

    //   {
    //       Cap_captureFrame(ctx, streamID, &m_buffer[0], m_buffer.size());
    //       cv::Mat img(finfo.height,finfo.width,CV_8UC3,&m_buffer[0]);
    //       // if(i==50)
    //       {
    //         // cv::imwrite("data/kkx"+to_string(i)+".jpg",img);
    //         // break;
    //       }
    //       // 



    //       // printf("Captured frames: %d\r", counter);
    //       // fflush(stdout);
    //   }




    }

}

CameraLayer_UVC::~CameraLayer_UVC()
{
    Cap_closeStream(ctx, streamID);

    CapResult result = Cap_releaseContext(ctx);
    cameraThread->join();
    delete cameraThread;
}


CameraLayer::status CameraLayer_UVC::SetFormatStr(std::string fstr){





    return NOT_SUPPORTED;
}

CameraLayer::status CameraLayer_UVC::TriggerCount(int count)
{
  takeCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_UVC::TriggerMode(int type){

  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::Trigger()
{
  return CameraLayer::NOT_SUPPORTED;
}


CameraLayer::status CameraLayer_UVC::SetAnalogGain(float gain)
{
  return CameraLayer::NOT_SUPPORTED;
}


CameraLayer::status CameraLayer_UVC::SetRGain(float gain)
{
  return CameraLayer::NOT_SUPPORTED;
}

CameraLayer::status CameraLayer_UVC::SetGGain(float gain)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::SetBGain(float gain)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::SetMirror(int Dir, int en)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::SetROIMirror(int Dir, int en)
{
  return CameraLayer::NOT_SUPPORTED;
}

CameraLayer::status CameraLayer_UVC::SetFrameRate(float frame_rate)
{
  return CameraLayer::NOT_SUPPORTED;
  
}
// CameraLayer::status CameraLayer_UVC::SetFrameRateMode(int mode)
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

CameraLayer::status CameraLayer_UVC::SetExposureTime(float time_us)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::GetExposureTime(float *ret_time_us)
{
  return CameraLayer::NOT_SUPPORTED;
}


CameraLayer::status CameraLayer_UVC::SetBalckLevel(float blvl)
{
  return CameraLayer::NOT_SUPPORTED;
}
CameraLayer::status CameraLayer_UVC::SetGamma(float gamma)
{
  return CameraLayer::NOT_SUPPORTED;
}

