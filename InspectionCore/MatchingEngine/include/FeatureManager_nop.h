#ifndef FeatureManager_NOP_HPP
#define FeatureManager_NOP_HPP

#include "FeatureReport.h"
#include "FeatureManager.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include <string>
#include "FeatureManager_binary_processing.h"



class FeatureManager_nop:public FeatureManager {
public :
  FeatureManager_nop(const char *json_str): FeatureManager(json_str){};
  int reload(const char *json_str){return 0;};
  int FeatureMatching(acvImage *img){
    report.type=FeatureReport::nop;
    return 0;};
    
  const FeatureReport* GetReport(){return &report;};
  static const char* GetFeatureTypeName(){return "nop";};
  cJSON *jobj;
protected:
  int parse_jobj(){return 0;};
};


#endif