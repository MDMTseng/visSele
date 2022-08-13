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




#include "SBM_if.hpp"


using namespace cv;

using namespace std;

class InspectionTarget_Orientation_ColorRegionOval :public InspectionTarget
{
public:
  InspectionTarget_Orientation_ColorRegionOval(string id,cJSON* def,InspectionTargetManager* belongMan);

  static string TYPE(){ return "Orientation_ColorRegionOval"; }
  bool stageInfoFilter(shared_ptr<StageInfo> sinfo);
  future<int> futureInputStagePool();

  int processInputPool();

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo();

  void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_Orientation_ColorRegionOval();

};







class InspectionTarget_Orientation_ShapeBasedMatching :public InspectionTarget
{
  protected:
  SBM_if *sbm;
  line2Dup::TemplatePyramid insp_tp;
  float matching_downScale=0.5;
  string template_name="tNAME";
public:
  InspectionTarget_Orientation_ShapeBasedMatching(string id,cJSON* def,InspectionTargetManager* belongMan);

  static string TYPE(){ return "Orientation_ShapeBasedMatching"; }
  bool stageInfoFilter(shared_ptr<StageInfo> sinfo);
  future<int> futureInputStagePool();

  void setInspDef(cJSON* def);
  int processInputPool();

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo();

  void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_Orientation_ShapeBasedMatching();

};



