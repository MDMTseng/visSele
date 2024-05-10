
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

#include "CompScript.hpp"

using namespace cv;

using namespace std;
class InspectionTarget_SurfaceCheckSimple :public InspectionTarget
{
  bool useExtParam=false;
  cJSON* extParam=NULL;
  bool show_display_overlay=true;
  cv::Mat background_temp;

  map<string,CompScript*> scriptTable;


  CompScript *orientationAlter=NULL;//=new CompScript();
public:

  void INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path);


  static std::string sTYPE()
  {return "SurfaceCheckSimple";}
  virtual std::string TYPE()
  {return sTYPE();}




  
  virtual cJSON* genITIOInfo()
  {
    cJSON* info= genITInfo();
    
    {
      cJSON_AddItemToObject(info, "io",genIOInfo({StageInfo_Orientation::stypeName()},{StageInfo_SurfaceCheckSimple::stypeName()}) );
    }
    return info;
  }



  // future<int> futureInputStagePool();

  void setInspDef(cJSON* def);
  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);


  void run();
  void singleProcess(shared_ptr<StageInfo> sinfo);


  virtual ~InspectionTarget_SurfaceCheckSimple();

};

