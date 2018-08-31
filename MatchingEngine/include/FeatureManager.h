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
public :
  static bool check(cJSON *root){return false;};
  FeatureManager(const char *json_str){};
  virtual int reload(const char *json_str)=0;
  virtual int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)=0;
  virtual const FeatureReport* GetReport(){return NULL;};
  static const char* GetFeatureTypeName(){return NULL;};
  virtual ~FeatureManager(){};

};
#endif
