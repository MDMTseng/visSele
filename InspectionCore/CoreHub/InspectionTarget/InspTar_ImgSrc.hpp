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




class InspectionTarget_ImgSrc :public InspectionTarget
{
  protected:
public:


  InspectionTarget_ImgSrc(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path);

  static string TYPE(){ return "ImgSrc"; }
  future<int> futureInputStagePool();

  virtual void setInspDef(cJSON* def);
  virtual int processInputPool();

  virtual bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo();

  virtual void singleProcess(shared_ptr<StageInfo> sinfo);
  virtual ~InspectionTarget_ImgSrc();

};



