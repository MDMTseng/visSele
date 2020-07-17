#ifndef FeatureManager_stage_light_report_HPP
#define FeatureManager_stage_light_report_HPP

#include "FeatureReport.h"
#include "FeatureManager.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include <string>

int backLightBlockCalc(acvImage *img, int X, int Y, int W, int H, stage_light_grid_node_info *ret_info);

int backLightNonBackGroundExclusion(acvImage *img,acvImage *backGround,acvImage *buffer,
  int nonBG_thres=20,int nonBG_spread_thres=100);
class FeatureManager_stage_light_report:public FeatureManager {

protected:
  acvImage *originalImage;
  acvImage cacheImage;
  acvImage cacheImage2;
  acvImage cacheImage3;
  int grid_size[2];
  float nonBG_thres;
  float nonBG_spread_thres;
  int down_scale_factor;
public :
  FeatureManager_stage_light_report(const char *json_str);//:FeatureManager(json_str)}{};
  ~FeatureManager_stage_light_report();//:FeatureManager(json_str)}{};
  virtual int reload(const char *json_str);
  int parse_jobj();
  static const char* GetFeatureTypeName(){return "stage_light_report";};
  const FeatureReport* GetReport();
  int FeatureMatching(acvImage *img);
  void setOriginalImage(acvImage *oriImage){this->originalImage = oriImage;};
  virtual void ClearReport() override;
};

#endif