#ifndef FeatureManager_stage_light_report_HPP
#define FeatureManager_stage_light_report_HPP

#include "FeatureReport.h"
#include "FeatureManager.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include <string>

int backLightBlockCalc(const cv::Mat &img, int X, int Y, int W, int H, stage_light_grid_node_info *ret_info);

int backLightNonBackGroundExclusion(const cv::Mat &img, cv::Mat &backGround, cv::Mat &buffer,
  int nonBG_thres=20, int nonBG_spread_thres=100);
class FeatureManager_stage_light_report:public FeatureManager {

protected:
  cv::Mat cacheImage;
  cv::Mat cacheImage2;
  cv::Mat cacheImage3;
  int grid_size[2];
  float nonBG_thres;
  float nonBG_spread_thres;
  int down_scale_factor;
public :
  FeatureManager_stage_light_report(const char *json_str);
  ~FeatureManager_stage_light_report();
  virtual int reload(const char *json_str);
  int parse_jobj();
  static const char* GetFeatureTypeName(){return "stage_light_report";};
  const FeatureReport* GetReport();
  int FeatureMatching(cv::Mat &img) override;
  virtual void ClearReport() override;
};

#endif