#include "InspTar_ImgSrc.hpp"
#include <PlatUtil.hpp>
// #include "libuvc/libuvc.h"
#include "openpnp-capture.h"
using namespace cv;

using namespace std;

template <typename Base, typename T>
inline bool instanceof (const T)
{
  return is_base_of<Base, T>::value;
}

InspectionTarget_ImgSrc::InspectionTarget_ImgSrc(string id, cJSON *def, InspectionTargetManager *belongMan, std::string local_env_path)
    : InspectionTarget(id, def, belongMan, local_env_path)
{
  type = InspectionTarget_ImgSrc::TYPE();

  GrantCameraPermission();

  LOGE("InspectionTarget_ImgSrc INIT");
  setInspDef(def);
}

future<int> InspectionTarget_ImgSrc::futureInputStagePool()
{
  return async(launch::async, &InspectionTarget_ImgSrc::processInputStagePool, this);
}

int InspectionTarget_ImgSrc::processInputPool()
{
  int poolSize = input_pool.size();
  for (int i = 0; i < poolSize; i++)
  {
    shared_ptr<StageInfo> curInput = input_pool[i];
    singleProcess(curInput);

    input_pool[i] = NULL;
  }
  input_pool.clear();

  return poolSize; // run all
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


struct PIDSetting{
  int pidCode;
  std::string name;
  int value;
};


void InspectionTarget_ImgSrc::setInspDef(cJSON *def)
{
  LOGE("InspectionTarget_ImgSrc::setInspDef");
  InspectionTarget::setInspDef(def);


  return;
  // use libuvc to enum device list

  if (0)
  {

    cv::VideoCapture cap(0); // open the default camera

    if (!cap.isOpened())
    {
      std::cerr << "ERROR: Could not open camera" << std::endl;
      return;
    }

    // Try to set the white balance
    double blue = 0.5; // valid range might be 0-1 or 0-255, depends on camera
    double red = 0.5;  // valid range might be 0-1 or 0-255, depends on camera
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0);
    LOGE("set CAP_PROP_EXPOSURE: ret%d",cap.set(cv::CAP_PROP_EXPOSURE, 1));
    cap.set(cv::CAP_PROP_WHITE_BALANCE_BLUE_U, blue);
    cap.set(cv::CAP_PROP_WHITE_BALANCE_RED_V, red);

    cv::Mat frame;
    for (int i = 0; i < 10; ++i)
    {
      cap >> frame; // get a new frame from the camera

      if (frame.empty())
      {
        std::cerr << "ERROR: Could not grab frame" << std::endl;
        return;
      }
      LOGE("frame size:%d %d", frame.cols, frame.rows);
      cv::imwrite("data/test_"+to_string(i)+".jpg", frame);
    }

    cap.release();
  }


  if(1)
  {

    CapContext ctx = Cap_createContext();

    uint32_t deviceCount = Cap_getDeviceCount(ctx);
    printf("Number of devices: %d\n", deviceCount);
    for (uint32_t i = 0; i < deviceCount; i++)
    {
      printf("ID %d -> %s\n", i, Cap_getDeviceName(ctx, i));

      // show all supported frame buffer formats
      int32_t nFormats = Cap_getNumFormats(ctx, i);

      printf("  Number of formats: %d\n", nFormats);

      std::string fourccString;
      for (int32_t j = 0; j < nFormats; j++)
      {
        CapFormatInfo finfo;
        Cap_getFormatInfo(ctx, i, j, &finfo);
        fourccString = FourCCToString(finfo.fourcc);

        printf("  Format ID %d: %d x %d pixels  FOURCC=%s\n",
               j, finfo.width, finfo.height, fourccString.c_str());
      }
    }


    int deviceID=1;
    int deviceFormatID=11;



    int32_t streamID = Cap_openStream(ctx, deviceID, deviceFormatID);
    printf("Stream ID = %d\n", streamID);
    
    if (Cap_isOpenStream(ctx, streamID) == 1)
    {
        printf("Stream is open\n");
    }
    else
    {
        printf("Stream is closed (?)\n");
        return ;
    }








    // get current stream parameters 
    CapFormatInfo finfo;
    Cap_getFormatInfo(ctx, deviceID, deviceFormatID, &finfo);

    //disable auto exposure, focus and white balance
    Cap_setAutoProperty(ctx, streamID, CAPPROPID_EXPOSURE, 0);
    // Cap_setAutoProperty(ctx, streamID, CAPPROPID_FOCUS, 0);
    Cap_setAutoProperty(ctx, streamID, CAPPROPID_WHITEBALANCE, 0);
    Cap_setAutoProperty(ctx, streamID, CAPPROPID_GAIN, 0);



    {

      PIDSetting pidSettings[]={
        {CAPPROPID_EXPOSURE,"exposure",50},
        {CAPPROPID_WHITEBALANCE,"WB",3200},
        {CAPPROPID_GAIN,"gain",50},
        {CAPPROPID_BRIGHTNESS,"brightness",-1},


        {CAPPROPID_CONTRAST,"contrast",-1},
        {CAPPROPID_SATURATION,"saturation",-1},
        {CAPPROPID_GAMMA,"GAMMA",-1},
        {CAPPROPID_HUE,"HUE",-1},
        {CAPPROPID_SHARPNESS,"SHARPNESS",6},
        {CAPPROPID_BACKLIGHTCOMP,"BACKLIGHTCOMP",-1},
        {CAPPROPID_POWERLINEFREQ,"POWERLINEFREQ",-1},

        {-1,"",-1}
        };



      for(int i=0;;i++)
      {
        int PID=pidSettings[i].pidCode;
        if(PID<0 || PID>=CAPPROPID_LAST )break;
            // set focus in the middle of the range
        int val=pidSettings[i].value;
        int32_t vmax, vmin, vdefault;
        if (Cap_getPropertyLimits(ctx, streamID, PID, 
                &vmin, &vmax, &vdefault) == CAPRESULT_OK)
        { 

            int ret_val=-999;
            Cap_getProperty(ctx, streamID, PID, &ret_val);

            if(val<0)val=vdefault;
            else if(val<vmin)val=vmin;
            else if(val>vmax)val=vmax;
            Cap_setProperty(ctx, streamID, PID, val);
            printf("%s= %d (%d~%d)  d:%d\n",pidSettings[i].name.c_str(), val,vmax,vmin,vdefault);
        }
        else
        {
            printf("%s is not supported\n",pidSettings[i].name.c_str());
        }
      }

    }



    std::vector<uint8_t> m_buffer;

    printf("w:%d,h:%d\n",finfo.width,finfo.height);
    m_buffer.resize(finfo.width*finfo.height*3);


    int64 t0=cv::getTickCount();

    for(int i=0;i<100;i++)
    {


      while (Cap_hasNewFrame(ctx, streamID) != 1);//std::this_thread::sleep_for(std::chrono::milliseconds(10));


      int64 t1=cv::getTickCount();
      LOGE(">>>>>>>>process_time_us:%f", 1000000 * (t1 - t0) / cv::getTickFrequency());

      t0=t1;
      printf("get frame %d fps:%d bpp:%d\n",i,finfo.fps,finfo.bpp);

      {
          Cap_captureFrame(ctx, streamID, &m_buffer[0], m_buffer.size());
          cv::Mat img(finfo.height,finfo.width,CV_8UC3,&m_buffer[0]);
          // if(i==50)
          {
            // cv::imwrite("data/kkx"+to_string(i)+".jpg",img);
            // break;
          }
          // 



          // printf("Captured frames: %d\r", counter);
          // fflush(stdout);
      }




    }







    {

      // PIDSetting pidSettings[]={
      //   {CAPPROPID_EXPOSURE,"exposure",-1},
      //   {CAPPROPID_WHITEBALANCE,"WB",3200},
      //   {CAPPROPID_GAIN,"gain",0},
      //   {CAPPROPID_BRIGHTNESS,"brightness",-1},


      //   {CAPPROPID_CONTRAST,"contrast",-1},
      //   {CAPPROPID_SATURATION,"saturation",-1},
      //   {CAPPROPID_GAMMA,"GAMMA",-1},
      //   {CAPPROPID_HUE,"HUE",-1},
      //   {CAPPROPID_SHARPNESS,"SHARPNESS",6},
      //   {CAPPROPID_BACKLIGHTCOMP,"BACKLIGHTCOMP",-1},
      //   {CAPPROPID_POWERLINEFREQ,"POWERLINEFREQ",-1},

      //   {-1,"",-1}
      //   };
      PIDSetting pidSettings[]={
        {CAPPROPID_EXPOSURE,"exposure",-2},
        {CAPPROPID_WHITEBALANCE,"WB",-2},
        {CAPPROPID_GAIN,"gain",-2},
        {CAPPROPID_BRIGHTNESS,"brightness",-2},


        {CAPPROPID_CONTRAST,"contrast",-2},
        {CAPPROPID_SATURATION,"saturation",-2},
        {CAPPROPID_GAMMA,"GAMMA",-2},
        {CAPPROPID_HUE,"HUE",-2},
        {CAPPROPID_SHARPNESS,"SHARPNESS",-1},
        {CAPPROPID_BACKLIGHTCOMP,"BACKLIGHTCOMP",-2},
        {CAPPROPID_POWERLINEFREQ,"POWERLINEFREQ",-1},

        {-1,"",-1}
        };


      for(int i=0;;i++)
      {
        int PID=pidSettings[i].pidCode;
        if(PID<0 || PID>=CAPPROPID_LAST )break;
            // set focus in the middle of the range
        int val=pidSettings[i].value;
        int32_t vmax, vmin, vdefault;
        if (Cap_getPropertyLimits(ctx, streamID, PID, 
                &vmin, &vmax, &vdefault) == CAPRESULT_OK)
        { 

            int ret_val=-999;
            Cap_getProperty(ctx, streamID, PID, &ret_val);

            if(val==-2)
            {
              val=ret_val;
            }
            else
            {
              if(val<0)val=vdefault;
              else if(val<vmin)val=vmin;
              else if(val>vmax)val=vmax;
              Cap_setProperty(ctx, streamID, PID, val);
            }
            printf("%s= %d (%d~%d)  d:%d\n",pidSettings[i].name.c_str(), val,vmax,vmin,vdefault);
        }
        else
        {
            printf("%s is not supported\n",pidSettings[i].name.c_str());
        }
      }

    }




    Cap_closeStream(ctx, streamID);

    CapResult result = Cap_releaseContext(ctx);
  }
}

bool InspectionTarget_ImgSrc::exchangeCMD(cJSON *info, int id, exchangeCMD_ACT &act)
{
  // LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info, id, act);
  if (ret)
    return ret;
  string type = JFetch_STRING_ex(info, "type");

  return false;
}

cJSON *InspectionTarget_ImgSrc::genITIOInfo()
{
  cJSON *arr = cJSON_CreateArray();

  {
    cJSON *opt = cJSON_CreateObject();
    cJSON_AddItemToArray(arr, opt);

    {
      cJSON *sarr = cJSON_CreateArray();

      cJSON_AddItemToObject(opt, "i", sarr);
      cJSON_AddItemToArray(sarr, cJSON_CreateString(StageInfo_Image::stypeName().c_str()));
    }

    {
      cJSON *sarr = cJSON_CreateArray();

      cJSON_AddItemToObject(opt, "o", sarr);
      cJSON_AddItemToArray(sarr, cJSON_CreateString(StageInfo_Orientation::stypeName().c_str()));
    }
  }

  return arr;
}

void InspectionTarget_ImgSrc::singleProcess(shared_ptr<StageInfo> sinfo)
{
}

InspectionTarget_ImgSrc::~InspectionTarget_ImgSrc()
{
}
