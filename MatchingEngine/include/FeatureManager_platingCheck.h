#ifndef FeatureManager_PLATING_INSPECTION_HPP
#define FeatureManager_PLATING_INSPECTION_HPP

#include "FeatureManager.h"
#include "MorphEngine.h"

class FeatureManager_platingCheck:public FeatureManager {
  typedef struct stdMapData_{
    acvImage* rgb;
    acvImage* sobel;
  }stdMapData;
  vector<stdMapData> stdMap;
  MorphEngine moEng;
public :
  FeatureManager_platingCheck(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  static const char* GetFeatureTypeName(){return "plating_check";};
  cJSON *jobj;
protected:
  int creat_stdMapDat(FeatureManager_platingCheck::stdMapData *dat,char* f_path);
  int parse_jobj() override;
};


#endif
