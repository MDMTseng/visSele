
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
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

class InspectionTarget_SurfaceCheckSimple :public InspectionTarget
{
  bool useExtParam=false;
  cJSON* extParam=NULL;
  bool show_display_overlay=true;
public:
  InspectionTarget_SurfaceCheckSimple(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path);

  static string TYPE(){ return "SurfaceCheckSimple"; }
  future<int> futureInputStagePool();

  int processInputPool();

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo();

  void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_SurfaceCheckSimple();

};



