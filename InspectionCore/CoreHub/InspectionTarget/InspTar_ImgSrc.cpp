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


/*
InspectionTarget_ImgSrc::InspectionTarget_ImgSrc(string id, cJSON *def, InspectionTargetManager *belongMan, std::string local_env_path)
    : InspectionTarget(id, def, belongMan, local_env_path)
{
  type = InspectionTarget_ImgSrc::TYPE();

  GrantCameraPermission();

  LOGE("InspectionTarget_ImgSrc INIT");
  setInspDef(def);
}

// future<int> InspectionTarget_ImgSrc::futureInputStagePool()
// {
//   return async(launch::async, &InspectionTarget_ImgSrc::processInputStagePool, this);
// }

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
    std::string v="";
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
  CapContext ctx = (CapContext)cam_ctx;
  
  // use libuvc to enum device list
  // return;
  if (0)//opencv method
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


  string def_CAM_UID=JFetch_STRING_ex(def,"CAM_UID");

  CAM_FMT_INFO new_fmt_info;
  {
    new_fmt_info.w=JFetch_NUMBER_ex(def,"format.w");
    new_fmt_info.h=JFetch_NUMBER_ex(def,"format.h");
    new_fmt_info.fourCC=JFetch_STRING_ex(def,"format.fourCC");
  }

  // LOGI("CAM_UID:%s  def_CAM_UID:%s",CAM_UID.c_str(),def_CAM_UID.c_str());
  // LOGI("cam wh :%dx%d  <>   %dx%d  ",new_fmt_info.w,new_fmt_info.h,cam_fmt_info.w,cam_fmt_info.h);




  if(CAM_UID!=def_CAM_UID || new_fmt_info.fourCC!=cam_fmt_info.fourCC || new_fmt_info.h!=cam_fmt_info.h || new_fmt_info.w!=cam_fmt_info.w)//need to switch camera
  {

    // LOGI("Changed");
    //stop the stream


    if(ctx)
    {

      if(cam_stream_id>=0)
      {
        CapResult res=Cap_closeStream(ctx, cam_stream_id);
        LOGE("close stream res:%d",res);
        //sleep
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        cam_stream_id=-1;
      }

      CapResult result = Cap_releaseContext(ctx);
      cam_ctx=NULL;
    }
    ctx = Cap_createContext();
    cam_ctx=ctx;


    int targetCamID=-1;
    int targetFmtID=-1;
    uint32_t deviceCount = Cap_getDeviceCount(ctx);
    printf("Number of devices: %d\n", deviceCount);
    for (uint32_t i = 0; i < deviceCount; i++)
    {
      printf("ID %d -> %s\n", i, Cap_getDeviceName(ctx, i));
      string cUID(Cap_getDeviceUniqueID(ctx, i));
      if(cUID!=def_CAM_UID)
      {
        continue;
      }
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
        if(new_fmt_info.fourCC==fourccString && new_fmt_info.h==finfo.height && new_fmt_info.w==finfo.width)//full match
        {
          targetCamID=i;
          targetFmtID=j;
          break;
        }
        
      }

      if(targetFmtID!=-1)break;
    }
    LOGI("targetCamID:%d targetFmtID:%d",targetCamID,targetFmtID);
    if(targetCamID!=-1 && targetFmtID!=-1)
    {
      cam_stream_id=Cap_openStream(ctx, targetCamID, targetFmtID);
      if(cam_stream_id>=0)
      {
        LOGE("open stream success cam_stream_id:%d",cam_stream_id);
        CAM_UID=def_CAM_UID;
        cam_fmt_info=new_fmt_info;
      }
      else
      {
        LOGE("open stream fail");
      }
    }
    else
    {
      LOGE("no camera found");
      CAM_UID="XXX";

    }

  }


  do
  {


    if(ctx){


      if(true)
      {

        int32_t streamID = cam_stream_id;
        if(streamID>=0){
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








          // // get current stream parameters 
          // CapFormatInfo finfo;
          // Cap_getFormatInfo(ctx, deviceID, deviceFormatID, &finfo);

          //disable auto exposure, focus and white balance
          CapResult res;
          res=Cap_setAutoProperty(ctx, streamID, CAPPROPID_EXPOSURE, 0);
          LOGI("setAutoProperty CAPPROPID_EXPOSURE:%d",res);
          // Cap_setAutoProperty(ctx, streamID, CAPPROPID_FOCUS, 0);
          res=Cap_setAutoProperty(ctx, streamID, CAPPROPID_WHITEBALANCE, 0);
          LOGI("setAutoProperty CAPPROPID_WHITEBALANCE:%d",res);
          res=Cap_setAutoProperty(ctx, streamID, CAPPROPID_GAIN, 0);
          LOGI("setAutoProperty CAPPROPID_GAIN:%d",res);



          {

            PIDSetting pidSettings[]={
              {CAPPROPID_EXPOSURE,"exposure",50},
              {CAPPROPID_WHITEBALANCE,"WB",6500},
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
              if ( Cap_getPropertyLimits(ctx, streamID, PID, 
                      &vmin, &vmax, &vdefault) == CAPRESULT_OK)
              { 

                  int ret_val=-999;
                  Cap_getProperty(ctx, streamID, PID, &ret_val);

                  if(val<0)val=vdefault;
                  else if(val<vmin)val=vmin;
                  else if(val>vmax)val=vmax;
                  CapResult setRes=Cap_setProperty(ctx, streamID, PID, val);
                  printf("%s= %d (%d~%d)  d:%d   setRes:%d\n",pidSettings[i].name.c_str(), val,vmax,vmin,vdefault,setRes);
              }
              else
              {
                  printf("%s is not supported\n",pidSettings[i].name.c_str());
              }
            }

          }



          std::vector<uint8_t> m_buffer;

          printf("w:%d,h:%d\n",cam_fmt_info.w,cam_fmt_info.h);
          m_buffer.resize(cam_fmt_info.w*cam_fmt_info.h*3);


          int64 t0=cv::getTickCount();

          printf("w:%d,h:%d\n",cam_fmt_info.w,cam_fmt_info.h);

          int testCapCount=100;
          for(int i=0;i<testCapCount;i++)
          {


            while (Cap_hasNewFrame(ctx, streamID) != 1)std::this_thread::sleep_for(std::chrono::milliseconds(10));


            int64 t1=cv::getTickCount();
            LOGE(">>>>>>>>process_time_us:%f", 1000000 * (t1 - t0) / cv::getTickFrequency());

            t0=t1;
            // printf("get frame %d fps:%d bpp:%d\n",i,finfo.fps,finfo.bpp);

            LOGE(">>>>>");
            if(1)
            {
                Cap_captureFrame(ctx, streamID, &m_buffer[0], m_buffer.size());
            LOGE(">>>>>");
                cv::Mat img(cam_fmt_info.h,cam_fmt_info.w,CV_8UC3,&m_buffer[0]);
            LOGE(">>>>>");
                cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
            LOGE(">>>>>");
                if(i==(testCapCount-1))
                  imwrite("data/kkx"+to_string(i)+"T_"+to_string(t1)+".jpg",img);
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
              {CAPPROPID_EXPOSURE,"exposure",-9992},
              {CAPPROPID_WHITEBALANCE,"WB",-9992},
              {CAPPROPID_GAIN,"gain",-9992},
              {CAPPROPID_BRIGHTNESS,"brightness",-9992},


              {CAPPROPID_CONTRAST,"contrast",-9992},
              {CAPPROPID_SATURATION,"saturation",-9992},
              {CAPPROPID_GAMMA,"GAMMA",-9992},
              {CAPPROPID_HUE,"HUE",-9992},
              {CAPPROPID_SHARPNESS,"SHARPNESS",-9992},
              {CAPPROPID_BACKLIGHTCOMP,"BACKLIGHTCOMP",-9992},
              {CAPPROPID_POWERLINEFREQ,"POWERLINEFREQ",-9992},

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

                  if(val==-9992)
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



        }
      
      }

      // {
      //   CapContext ctx2 = Cap_createContext();

      //   LOGE("ctxs:%p %p",ctx,ctx2);
      //   if(ctx2)
      //   {

      //     CapResult result = Cap_releaseContext(ctx2);
      //   }


      // }




    }
    
  }
  while(0);

}

bool InspectionTarget_ImgSrc::exchangeCMD(cJSON *info, int id, exchangeCMD_ACT &act)
{
  // LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info, id, act);
  if (ret)
    return ret;
  string type = JFetch_STRING_ex(info, "type");



  if(type=="GetCameraList")
  {
    
    CapContext ctx = Cap_createContext();
    if(ctx)
    {
      uint32_t deviceCount = Cap_getDeviceCount(ctx);

      cJSON* camList=cJSON_CreateArray();
        
      printf("  Number of deviceCount: %d\n", deviceCount);
      for(int i=0;i<deviceCount;i++)
      {

        cJSON* camInfo=cJSON_CreateObject();


        cJSON_AddItemToArray(camList,camInfo);

        cJSON_AddStringToObject(camInfo,"name",Cap_getDeviceName(ctx, i));
        cJSON_AddStringToObject(camInfo,"UID",Cap_getDeviceUniqueID(ctx, i));

        


        cJSON* jfmtList=cJSON_CreateArray();
        cJSON_AddItemToObject(camInfo,"formats",jfmtList);


        
        int32_t nFormats = Cap_getNumFormats(ctx, i);

        printf(" [%d] Number of formats: %d\n",i, nFormats);

        std::string fourccString;
        for (int32_t j = 0; j < nFormats; j++)
        {
          CapFormatInfo finfo;
          Cap_getFormatInfo(ctx, i, j, &finfo);
          fourccString = FourCCToString(finfo.fourcc);



          cJSON* jfmt=cJSON_CreateObject();
          cJSON_AddItemToArray(jfmtList,jfmt);
          cJSON_AddNumberToObject(jfmt,"w",finfo.width);
          cJSON_AddNumberToObject(jfmt,"h",finfo.height);
          cJSON_AddNumberToObject(jfmt,"bpp",finfo.bpp);
          cJSON_AddNumberToObject(jfmt,"fps",finfo.fps);
          cJSON_AddStringToObject(jfmt,"fourcc",fourccString.c_str());


        }












      }


      act.send("RP", id, camList);
      cJSON_Delete(camList);

      CapResult result = Cap_releaseContext(ctx);
    }



  }

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
  if(cam_stream_id>=0)
  {
    Cap_closeStream(cam_ctx, cam_stream_id);
    cam_stream_id=-1;
  }
  
  if(cam_ctx)
  {
    Cap_releaseContext(cam_ctx);
    cam_ctx=NULL; 
  }
}


*/