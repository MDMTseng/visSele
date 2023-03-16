#pragma once
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
#include <StageInfo_Orientation.hpp>

#include "InspectionTarget.hpp"



#include "SBM_if.hpp"


using namespace cv;

using namespace std;

class InspectionTarget_SpriteDraw :public InspectionTarget
{
public:
  InspectionTarget_SpriteDraw(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path);

  static string TYPE(){ return "SpriteDraw"; }
  future<int> futureInputStagePool();

  int processInputPool();

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo();

  void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_SpriteDraw();

};



