#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;

#include "FeatureReport.h"
#include "acvImage_ComponentLabelingTool.hpp"

#include "cJSON.h"

class FeatureManager {
  protected:
  FeatureReport report;
  cJSON *root;
  virtual int parse_jobj()=0;
  acvRadialDistortionParam param;
  acvImage _buff;
public :
  //static bool check(cJSON *root);
  FeatureManager(const char *json_str){};
  void setRadialDistortionParam(acvRadialDistortionParam param){this->param=param;};
  virtual int reload(const char *json_str)=0;
  virtual int FeatureMatching(acvImage *img)=0;
  virtual const FeatureReport* GetReport(){return NULL;};
  virtual void ClearReport(){};
  static const char* GetFeatureTypeName(){return NULL;};
  virtual ~FeatureManager(){};

};
#endif