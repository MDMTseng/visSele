#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "acvImage_BasicTool.hpp"
#include "FeatureManager.h"
#include "FeatureManager_group.h"



class MatchingEngine {
protected:
  vector<FeatureManager*> featureBundle;
  FeatureManager_BacPac *bacpac;
public:
  int ResetFeature();
  void setBacPac(FeatureManager_BacPac *bacpac){this->bacpac=bacpac;};
  int AddMatchingFeature(const char *json_str);
  int AddMatchingFeature(FeatureManager *featureSet);
  int FeatureMatching(acvImage *img);
  const FeatureReport *GetReport();
  cJSON*FeatureReport2Json(const FeatureReport *report);
  ~MatchingEngine();
};

#endif
