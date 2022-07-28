
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

class StageInfo_Orientation:public StageInfo
{
  public:
  static string stypeName(){return "Orientation";}
  string typeName(){return StageInfo_Orientation::stypeName();}

  struct orient{
    acv_XY center;
    float angle;
    bool flip;
  };


  
  vector<struct orient> orientation;

};


class InspectionTarget_Orientation_ColorRegionOval :public InspectionTarget
{
public:
  InspectionTarget_Orientation_ColorRegionOval(string id,cJSON* def,InspectionTargetManager* belongMan);

  static string TYPE(){ return "Orientation_ColorRegionOval"; }
  bool stageInfoFilter(shared_ptr<StageInfo> sinfo);
  future<int> futureInputStagePool();

  int processInputPool();

  cJSON* exchangeInfo(cJSON* info);

  virtual cJSON* genITIOInfo();

  void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_Orientation_ColorRegionOval();

};



