#ifndef FeatureManager_stage_light_report_HPP
#define FeatureManager_stage_light_report_HPP

#include "FeatureReport.h"
#include "FeatureManager.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include <string>


class FeatureManager_stage_light_report:public FeatureManager {

protected:
  acvImage *originalImage;
  acvImage cacheImage;
  acvImage cacheImage2;
  acvImage cacheImage3;
  int grid_size[2];
  float nonBG_thres;
  float nonBG_spread_thres;
public :
  FeatureManager_stage_light_report(const char *json_str);//:FeatureManager(json_str)}{};
  ~FeatureManager_stage_light_report();//:FeatureManager(json_str)}{};
  virtual int reload(const char *json_str);
  int parse_jobj();
  static const char* GetFeatureTypeName(){return "stage_light_report";};
  const FeatureReport* GetReport();
  int FeatureMatching(acvImage *img);
  void setOriginalImage(acvImage *oriImage){this->originalImage = oriImage;};
  void ClearReport();
};

#endif