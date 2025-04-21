
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include "CameraLayerManager.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>


using namespace cv;

using namespace std;
class InspectionTarget_CameraCalib :public InspectionTarget
{

public:

  void INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path);


  static std::string sTYPE()
  {return "CameraCalib";}
  virtual std::string TYPE()
  {return sTYPE();}


  Mat cameraMatrix;
  Mat distCoeffs;
  Mat perspectiveMatrix;
  Mat undistortMapX,undistortMapY;
  Mat distortMapX,distortMapY;
  float mmpp;//mm per pixel

  bool enable_calibration=false;
  string cache_latest_undistorted_image_path;
  Mat cache_latest_undistorted_image;

  int cache_latest_undistorted_image_accumulator_count=-1;
  cv::Mat cache_latest_undistorted_image_accumulator_32F;
  virtual cJSON* genITIOInfo()
  {
    cJSON* info= genITInfo();
    
    {
      cJSON_AddItemToObject(info, "io",genIOInfo({"sss"},{"aaa"}) );
    }
    return info;
  }



  // future<int> futureInputStagePool();

  void setInspDef(cJSON* def);
  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);


  void run();
  void singleProcess(shared_ptr<StageInfo> sinfo);


  virtual ~InspectionTarget_CameraCalib();

};

