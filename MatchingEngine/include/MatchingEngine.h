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
  vector<FeatureManager*> featureBundle;
public:
  int ResetFeature();
  int AddMatchingFeature(const char *json_str);
  int AddMatchingFeature(FeatureManager *featureSet);
  int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg);
  const FeatureReport *GetReport();
  cJSON*FeatureReport2Json(const FeatureReport *report);
  ~MatchingEngine();
};

#endif
