#ifndef FeatureManager_PLATING_INSPECTION_HPP
#define FeatureManager_PLATING_INSPECTION_HPP

#include "FeatureManager.h"

class FeatureManager_platingCheck:public FeatureManager {
public :
  FeatureManager_platingCheck(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
  cJSON *jobj;
protected:
  int parse_jobj() override;
};


#endif
